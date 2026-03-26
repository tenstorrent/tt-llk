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
        "tokens": None,
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
            timeout=3600,  # 1 hour max per kernel
        )
        result["exit_code"] = proc.returncode
        result["duration_seconds"] = int((datetime.now() - start).total_seconds())

        # Try to parse token usage from JSON output
        if proc.stdout.strip():
            try:
                output = json.loads(proc.stdout.strip())
                usage = output.get("usage", {})
                result["tokens"] = {
                    "input": usage.get("input_tokens", 0),
                    "output": usage.get("output_tokens", 0),
                    "cache_read": usage.get("cache_read_input_tokens", 0),
                }
            except json.JSONDecodeError:
                pass

        result["status"] = "success" if proc.returncode == 0 else "failed"

        if proc.returncode != 0:
            # Log last 20 lines of stderr for debugging
            stderr_tail = "\n".join(proc.stderr.strip().splitlines()[-20:])
            print(f"  FAILED (exit {proc.returncode}): {stderr_tail[:200]}")

    except subprocess.TimeoutExpired:
        result["status"] = "timeout"
        result["duration_seconds"] = 3600
        print(f"  TIMEOUT after 1 hour")

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
            "success": "OK",
            "failed": "FAIL",
            "timeout": "TIMEOUT",
            "skipped": "SKIP",
        }
        duration = (
            f" ({result['duration_seconds']}s)" if result["duration_seconds"] else ""
        )
        print(f"  Result: {status_icon.get(result['status'], '?')}{duration}")
        print()

    # Summary
    success = sum(1 for r in results if r["status"] == "success")
    failed = sum(1 for r in results if r["status"] == "failed")
    timeout = sum(1 for r in results if r["status"] == "timeout")

    print("=" * 50)
    print("  Summary")
    print("=" * 50)
    print(f"  Success:  {success}/{len(results)}")
    if failed:
        print(f"  Failed:   {failed}")
        for r in results:
            if r["status"] == "failed":
                print(f"            - {r['kernel']}")
    if timeout:
        print(f"  Timeout:  {timeout}")
    print(f"  Finished: {datetime.now().isoformat()}")
    print("=" * 50)

    # Exit with failure if any kernel failed
    sys.exit(1 if (failed + timeout) > 0 else 0)


if __name__ == "__main__":
    main()
