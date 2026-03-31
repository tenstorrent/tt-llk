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
import json
import os
import subprocess
import sys
from datetime import datetime
from pathlib import Path

# Wave 1 kernels — simple SFPU ops with golden generators (testable)
DEFAULT_KERNELS = ["abs", "negative", "fill", "threshold", "elu"]

CODEGEN_DIR = Path(__file__).resolve().parent.parent


def run_kernel(
    kernel: str, batch_id: str, model: str, dry_run: bool, timeout: int
) -> dict:
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
        "--output-format",
        "json",
    ]

    result = {
        "kernel": kernel,
        "prompt": prompt,
        "status": "pending",
        "exit_code": None,
        "duration_seconds": None,
        "tokens": {"input": 0, "output": 0, "cache_read": 0, "total": 0},
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
            timeout=timeout,
        )
        result["exit_code"] = proc.returncode
        result["duration_seconds"] = int((datetime.now() - start).total_seconds())

        result["status"] = "completed" if proc.returncode == 0 else "crashed"

        # Parse JSON output for token usage
        if proc.stdout.strip():
            try:
                output = json.loads(proc.stdout)
                usage = output.get("usage", {})
                result["tokens"] = {
                    "input": usage.get("input_tokens", 0),
                    "output": usage.get("output_tokens", 0),
                    "cache_read": usage.get("cache_read_input_tokens", 0),
                    "total": usage.get("input_tokens", 0)
                    + usage.get("output_tokens", 0),
                }
            except json.JSONDecodeError:
                pass  # Non-JSON output, tokens stay at 0

            # Save CLI output to run's log_dir for dashboard token tracking
            try:
                runs_file = Path("/proj_sw/user_dev/llk_code_gen/quasar/runs.jsonl")
                if runs_file.exists():
                    log_dir = None
                    for line in runs_file.read_text().splitlines():
                        try:
                            entry = json.loads(line)
                            if (
                                entry.get("kernel") == kernel
                                and entry.get("batch_id") == batch_id
                            ):
                                log_dir = entry.get("log_dir")
                        except json.JSONDecodeError:
                            pass
                    if log_dir and Path(log_dir).is_dir():
                        (Path(log_dir) / "cli_output.json").write_text(proc.stdout)
            except Exception:
                pass  # Best-effort — don't fail the run for this

        if proc.returncode != 0:
            stderr_tail = "\n".join(proc.stderr.strip().splitlines()[-20:])
            print(f"  CRASHED (exit {proc.returncode}): {stderr_tail[:200]}")
    except subprocess.TimeoutExpired:
        result["status"] = "crashed"
        result["duration_seconds"] = timeout
        print(f"  TIMEOUT after {timeout}s")
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
        "--timeout",
        type=int,
        default=14400,
        help="Timeout per kernel in seconds (default: 14400 / 4 hours)",
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
    print(f"  Timeout:   {args.timeout}s per kernel")
    print(f"  Run type:  ci")
    print(f"  Started:   {datetime.now().isoformat()}")
    print("=" * 50)
    print()

    results = []
    for i, kernel in enumerate(args.kernels, 1):
        print(f"[{i}/{len(args.kernels)}] {kernel}")
        result = run_kernel(kernel, batch_id, args.model, args.dry_run, args.timeout)
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
    total_tokens = {
        "input": sum(r["tokens"]["input"] for r in results),
        "output": sum(r["tokens"]["output"] for r in results),
        "cache_read": sum(r["tokens"]["cache_read"] for r in results),
        "total": sum(r["tokens"]["total"] for r in results),
    }

    print("=" * 50)
    print("  Summary")
    print("=" * 50)
    print(f"  Completed: {completed}/{len(results)}")
    if crashed:
        print(f"  Crashed:   {crashed}")
        for r in results:
            if r["status"] == "crashed":
                print(f"             - {r['kernel']}")
    if total_tokens["total"] > 0:
        print(
            f"  Tokens:    {total_tokens['total']:,} total"
            f" ({total_tokens['input']:,} in,"
            f" {total_tokens['output']:,} out,"
            f" {total_tokens['cache_read']:,} cached)"
        )
    print(f"  Finished:  {datetime.now().isoformat()}")
    print(f"  Results:   check dashboard for kernel success/compiled/failed status")
    print("=" * 50)

    sys.exit(1 if crashed > 0 else 0)


if __name__ == "__main__":
    main()
