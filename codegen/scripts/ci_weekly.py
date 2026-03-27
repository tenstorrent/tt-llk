#!/usr/bin/env python3
# SPDX-FileCopyrightText: © 2026 Tenstorrent AI ULC
#
# SPDX-License-Identifier: Apache-2.0

"""
Weekly CI runner for LLK CodeGen.

Runs a set of kernel generation prompts as a CI batch, setting environment
variables so the orchestrator logs them with run_type="ci".

Usage:
    # Run all 5 default kernels
    python scripts/ci_weekly.py

    # Run specific kernels
    python scripts/ci_weekly.py --kernels abs negative elu

    # Use a different model
    python scripts/ci_weekly.py --model sonnet

    # Dry run (print commands without executing)
    python scripts/ci_weekly.py --dry-run

Crontab (every Friday at 12:00):
    0 12 * * 5 cd /proj_sw/user_dev/vvukomanovic/tt-llk/codegen && /path/to/python scripts/ci_weekly.py >> /tmp/ci_weekly.log 2>&1
"""

import argparse
import os
import subprocess
import sys
from datetime import datetime
from pathlib import Path

# Wave 1 kernels — simple SFPU ops with golden generators (testable)
DEFAULT_KERNELS = ["abs", "negative", "fill", "threshold", "elu"]

CODEGEN_DIR = Path(__file__).resolve().parent.parent


def run_kernel(kernel: str, batch_id: str, model: str, dry_run: bool) -> dict:
    """Run a single kernel generation via claude CLI."""
    prompt = f"Generate {kernel} for Quasar"
    env = {
        **os.environ,
        "CODEGEN_BATCH_ID": batch_id,
        "CODEGEN_MODEL": model,
    }

    cmd = [
        "claude",
        "-p",
        prompt,
        "--model",
        model,
        "--dangerously-skip-permissions",
        "--effort",
        "max",
        "--verbose",
    ]

    result = {
        "kernel": kernel,
        "prompt": prompt,
        "status": "pending",
        "exit_code": None,
        "duration_seconds": None,
    }

    if dry_run:
        print(f"  [DRY RUN] {' '.join(cmd)}")
        result["status"] = "skipped"
        return result

    print(f"  Starting: {prompt}")
    start = datetime.now()

    try:
        proc = subprocess.run(
            cmd,
            cwd=str(CODEGEN_DIR),
            env=env,
            capture_output=True,
            text=True,
        )
        result["exit_code"] = proc.returncode
        result["duration_seconds"] = int((datetime.now() - start).total_seconds())

        result["status"] = "completed" if proc.returncode == 0 else "crashed"

        if proc.returncode != 0:
            stderr_tail = "\n".join(proc.stderr.strip().splitlines()[-20:])
            print(f"  CRASHED (exit {proc.returncode}): {stderr_tail[:200]}")
    except Exception as e:
        result["status"] = "crashed"
        result["duration_seconds"] = int((datetime.now() - start).total_seconds())
        print(f"  EXCEPTION: {e}")

    return result


def main():
    parser = argparse.ArgumentParser(description="Weekly CI runner for LLK CodeGen")
    parser.add_argument(
        "--kernels",
        nargs="+",
        default=DEFAULT_KERNELS,
        help=f"Kernels to generate (default: {' '.join(DEFAULT_KERNELS)})",
    )
    parser.add_argument(
        "--model",
        default="opus",
        help="Claude model to use (default: opus)",
    )
    parser.add_argument(
        "--batch-id",
        default=None,
        help="Batch ID (default: auto-generated from date)",
    )
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="Print commands without executing",
    )
    args = parser.parse_args()

    batch_id = args.batch_id or f"{datetime.now().strftime('%Y-%m-%d')}_weekly"

    print("=" * 50)
    print("  LLK CodeGen — Weekly CI Run")
    print("=" * 50)
    print(f"  Batch ID:  {batch_id}")
    print(f"  Model:     {args.model}")
    print(f"  Kernels:   {', '.join(args.kernels)}")
    print(f"  Run type:  ci")
    print(f"  Started:   {datetime.now().isoformat()}")
    print("=" * 50)
    print()

    results = []
    for i, kernel in enumerate(args.kernels, 1):
        print(f"[{i}/{len(args.kernels)}] {kernel}")
        result = run_kernel(kernel, batch_id, args.model, args.dry_run)
        results.append(result)

        status_icon = {
            "completed": "OK",
            "crashed": "CRASH",
            "skipped": "SKIP",
        }
        duration = (
            f" ({result['duration_seconds']}s)" if result["duration_seconds"] else ""
        )
        print(f"  Result: {status_icon.get(result['status'], '?')}{duration}")
        print()

    # Summary
    completed = sum(1 for r in results if r["status"] == "completed")
    crashed = sum(1 for r in results if r["status"] == "crashed")

    print("=" * 50)
    print("  Summary")
    print("=" * 50)
    print(f"  Completed: {completed}/{len(results)}")
    if crashed:
        print(f"  Crashed:   {crashed}")
        for r in results:
            if r["status"] == "crashed":
                print(f"             - {r['kernel']}")
    print(f"  Finished:  {datetime.now().isoformat()}")
    print(f"  Results:   check dashboard for kernel success/compiled/failed status")
    print("=" * 50)

    sys.exit(1 if crashed > 0 else 0)


if __name__ == "__main__":
    main()
