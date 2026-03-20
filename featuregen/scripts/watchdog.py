#!/usr/bin/env python3
# SPDX-FileCopyrightText: (c) 2026 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0

"""
Featuregen Watchdog — detects when the main orchestrator is stuck.

Runs periodically (e.g. via cron every 5 minutes). Reads the progress log,
checks how long each step has been running, and prints an alert + writes a
WATCHDOG_NUDGE.md file that the orchestrator reads at the top of each loop.

Usage:
    python featuregen/scripts/watchdog.py
    python featuregen/scripts/watchdog.py --artifacts featuregen/artifacts
    python featuregen/scripts/watchdog.py --verbose
"""

import argparse
import json
import sys
from datetime import datetime
from pathlib import Path

# Time budgets per step (minutes). If a step runs longer, it's considered stuck.
STEP_BUDGETS = {
    "Step 2: Analyze": 8,
    "Step 3: Plan": 10,
    "Step 4: Implement": 8,
    "Step 5: Compile": 4,  # per iteration
    "Step 5.5: Sync": 1,
    "Step 6: Test": 15,  # per iteration (pytest can be slow)
    "Step 6 Debug": 10,  # per iteration
}

# If no heartbeat at all for this long, definitely stuck
GLOBAL_STALL_MINUTES = 20


def find_progress_files(artifacts_dir: Path) -> list[Path]:
    return sorted(artifacts_dir.glob("*_progress.json"))


def parse_progress(path: Path) -> dict:
    try:
        return json.loads(path.read_text())
    except Exception as e:
        return {"error": str(e), "path": str(path)}


def check_stall(progress: dict, verbose: bool = False) -> tuple[bool, str]:
    """
    Returns (is_stuck, reason).
    """
    kernel = progress.get("kernel", "unknown")
    step = progress.get("current_step", "unknown")
    step_started_str = progress.get("step_started_at")
    heartbeat_str = progress.get("last_heartbeat_at")
    compile_attempts = progress.get("compile_attempts", 0)
    test_attempts = progress.get("test_attempts", 0)
    last_action = progress.get("last_action", "")

    now = datetime.utcnow()

    if verbose:
        print(f"Kernel:           {kernel}")
        print(f"Current step:     {step}")
        print(f"Step started:     {step_started_str}")
        print(f"Last heartbeat:   {heartbeat_str}")
        print(f"Compile attempts: {compile_attempts}")
        print(f"Test attempts:    {test_attempts}")
        print(f"Last action:      {last_action}")
        print()

    # Check heartbeat staleness
    if heartbeat_str:
        try:
            heartbeat = datetime.fromisoformat(heartbeat_str)
            age_minutes = (now - heartbeat).total_seconds() / 60
            if age_minutes > GLOBAL_STALL_MINUTES:
                return True, (
                    f"No heartbeat for {age_minutes:.0f} minutes "
                    f"(last: {heartbeat_str}). "
                    f"Step '{step}' may be hanging."
                )
        except ValueError:
            pass

    # Check per-step budget
    if step_started_str:
        try:
            step_started = datetime.fromisoformat(step_started_str)
            step_minutes = (now - step_started).total_seconds() / 60

            for budget_key, budget_minutes in STEP_BUDGETS.items():
                if budget_key.lower() in step.lower():
                    if step_minutes > budget_minutes * 2:  # 2x budget = stuck
                        return True, (
                            f"Step '{step}' has been running for {step_minutes:.0f} minutes "
                            f"(budget: {budget_minutes} min, 2x limit: {budget_minutes * 2} min). "
                            f"Last action: {last_action}"
                        )
                    elif step_minutes > budget_minutes and verbose:
                        print(
                            f"WARNING: Step '{step}' is over budget "
                            f"({step_minutes:.0f}/{budget_minutes} min)"
                        )
                    break
        except ValueError:
            pass

    # Check for obvious loops (same error repeated)
    error_log = progress.get("last_compile_error", "")
    prev_error_log = progress.get("prev_compile_error", "")
    if compile_attempts >= 3 and error_log and error_log == prev_error_log:
        return True, (
            f"Compile debugger has seen the same error {compile_attempts} times "
            f"without making progress. Error: {error_log[:200]}"
        )

    return False, ""


def write_nudge(artifacts_dir: Path, kernel: str, reason: str, step: str):
    nudge_path = artifacts_dir / "WATCHDOG_NUDGE.md"
    timestamp = datetime.utcnow().isoformat()
    content = f"""# Watchdog Nudge — {timestamp}

## Detected Stall
**Kernel:** {kernel}
**Step:** {step}
**Reason:** {reason}

## Recommended Actions
- If stuck in compile loop with same error: read featuregen/references/common-errors.md,
  try a completely different approach, or consider if the feature plan has a fundamental flaw.
- If stuck in test loop: re-read the full test failure output from scratch. Check if the
  logic error is in the NEW code path or if it's an interaction with EXISTING code.
- If analysis/planning is taking too long: you have enough context. Stop reading more files
  and write what you have. The compile/test loops will surface gaps.
- If sync or compile check is hanging: check if the venv is activated and the compiler path
  is valid.

## Action Required
Read this file, acknowledge the stall, take corrective action, then delete this file:
`featuregen/artifacts/WATCHDOG_NUDGE.md`
"""
    nudge_path.write_text(content)
    print(f"Nudge written to: {nudge_path}")


def main():
    parser = argparse.ArgumentParser(
        description="Featuregen watchdog — detect orchestrator stalls"
    )
    parser.add_argument(
        "--artifacts",
        type=Path,
        default=Path(__file__).parent.parent / "artifacts",
        help="Path to featuregen/artifacts directory",
    )
    parser.add_argument("--verbose", "-v", action="store_true")
    args = parser.parse_args()

    if not args.artifacts.exists():
        print(f"Artifacts directory not found: {args.artifacts}")
        print("No active featuregen session detected.")
        sys.exit(0)

    progress_files = find_progress_files(args.artifacts)
    if not progress_files:
        if args.verbose:
            print("No active featuregen sessions found (no *_progress.json files).")
        sys.exit(0)

    any_stuck = False
    for pf in progress_files:
        progress = parse_progress(pf)
        if "error" in progress:
            print(f"Could not parse {pf}: {progress['error']}")
            continue

        kernel = progress.get("kernel", pf.stem.replace("_progress", ""))
        step = progress.get("current_step", "unknown")

        if args.verbose:
            print(f"=== Session: {kernel} ===")

        is_stuck, reason = check_stall(progress, verbose=args.verbose)

        if is_stuck:
            any_stuck = True
            print(f"STUCK: {kernel} — {reason}")
            write_nudge(args.artifacts, kernel, reason, step)
        else:
            if args.verbose:
                print(f"OK: {kernel} — step '{step}' running normally")

    if any_stuck:
        print()
        print(
            "Watchdog has written WATCHDOG_NUDGE.md. The orchestrator will read it at the next loop checkpoint."
        )
        sys.exit(1)
    else:
        if args.verbose:
            print("All sessions healthy.")
        sys.exit(0)


if __name__ == "__main__":
    main()
