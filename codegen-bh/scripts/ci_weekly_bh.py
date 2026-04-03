#!/usr/bin/env python3
# SPDX-FileCopyrightText: (c) 2026 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0

"""
Weekly CI runner for Blackhole LLK CodeGen.

Fetches Blackhole P2 issues from GitHub and runs codegen for each one,
setting environment variables so the orchestrator logs them as run_type="ci".

Usage:
    # Run all open Blackhole P2 issues
    python scripts/ci_weekly_bh.py

    # Run specific issues by number
    python scripts/ci_weekly_bh.py --issues 1153 1148 960

    # Filter by additional label
    python scripts/ci_weekly_bh.py --label LLK

    # Use a different model
    python scripts/ci_weekly_bh.py --model sonnet

    # Dry run (print commands without executing)
    python scripts/ci_weekly_bh.py --dry-run

    # Limit to N issues (useful for partial runs)
    python scripts/ci_weekly_bh.py --max-issues 5

Crontab (every Friday at 14:00):
    0 14 * * 5 cd /proj_sw/user_dev/nstamatovic/tt-llk/codegen-bh && /path/to/python scripts/ci_weekly_bh.py >> /tmp/ci_weekly_bh.log 2>&1
"""

import argparse
import json
import os
import subprocess
import sys
from datetime import datetime
from pathlib import Path

REPO = "tenstorrent/tt-llk"
CODEGEN_DIR = Path(__file__).resolve().parent.parent
RUNS_JSONL = Path("/proj_sw/user_dev/llk_code_gen/blackhole/runs.jsonl")
RUNS_BASE = Path("/proj_sw/user_dev/llk_code_gen/blackhole")


def fetch_issues(
    state: str = "open",
    extra_label: str | None = None,
    limit: int = 200,
) -> list[dict]:
    """Fetch Blackhole P2 issues via gh CLI."""
    fields = "number,title,body,labels,assignees,createdAt,updatedAt,url,state"

    cmd = [
        "gh",
        "issue",
        "list",
        "-R",
        REPO,
        "--label",
        "blackhole",
        "--label",
        "P2",
        "--state",
        state,
        "--limit",
        str(limit),
        "--json",
        fields,
    ]
    if extra_label:
        cmd.extend(["--label", extra_label])

    result = subprocess.run(cmd, capture_output=True, text=True, timeout=60)
    if result.returncode != 0:
        print(f"Error fetching issues: {result.stderr.strip()}", file=sys.stderr)
        sys.exit(1)

    issues = json.loads(result.stdout)

    normalized = []
    for issue in issues:
        normalized.append(
            {
                "number": issue["number"],
                "title": issue["title"],
                "body": issue.get("body", ""),
                "url": issue.get(
                    "url", f"https://github.com/{REPO}/issues/{issue['number']}"
                ),
                "labels": [l["name"] for l in issue.get("labels", [])],
                "assignees": [
                    a.get("login", a.get("name", "unknown"))
                    for a in issue.get("assignees", [])
                ],
                "created_at": issue.get("createdAt", ""),
                "updated_at": issue.get("updatedAt", ""),
            }
        )

    normalized.sort(key=lambda x: x["number"])
    return normalized


def _resolve_log_dir(log_dir: str) -> Path:
    """Resolve relative log_dir paths against the runs base directory."""
    p = Path(log_dir)
    if p.is_absolute():
        return p
    candidate = RUNS_BASE / log_dir
    if candidate.is_dir():
        return candidate
    if log_dir.startswith("logs/"):
        candidate = RUNS_BASE / log_dir[5:]
        if candidate.is_dir():
            return candidate
    return p


def _extract_tokens_from_cli_output(cli_stdout: str) -> dict | None:
    """Extract cumulative token counts and cost from CLI JSON output."""
    try:
        data = json.loads(cli_stdout)
        if not isinstance(data, list) or len(data) == 0:
            return None
        last = data[-1]
        if not isinstance(last, dict):
            return None
        model_usage = last.get("modelUsage", {})
        if model_usage:
            total_input = sum(v.get("inputTokens", 0) for v in model_usage.values())
            total_output = sum(v.get("outputTokens", 0) for v in model_usage.values())
            total_cache_read = sum(
                v.get("cacheReadInputTokens", 0) for v in model_usage.values()
            )
            total_cache_creation = sum(
                v.get("cacheCreationInputTokens", 0) for v in model_usage.values()
            )
            cost = last.get("total_cost_usd", 0)
            return {
                "input": total_input,
                "output": total_output,
                "cache_read": total_cache_read,
                "cache_creation": total_cache_creation,
                "total": total_input + total_output,
                "cost_usd": round(cost, 6) if cost else 0,
            }
        usage = last.get("usage", {})
        if usage:
            inp = usage.get("input_tokens", 0)
            out = usage.get("output_tokens", 0)
            return {
                "input": inp,
                "output": out,
                "cache_read": usage.get("cache_read_input_tokens", 0),
                "cache_creation": usage.get("cache_creation_input_tokens", 0),
                "total": inp + out,
                "cost_usd": round(last.get("total_cost_usd", 0), 6),
            }
    except (json.JSONDecodeError, Exception):
        pass
    return None


def _save_cli_output_and_patch_tokens(
    cli_stdout: str, issue_number: int, batch_id: str, result: dict
) -> None:
    """Save CLI output to log_dir and patch token data into runs.jsonl + run.json."""
    tokens = _extract_tokens_from_cli_output(cli_stdout)
    if tokens:
        result["tokens"] = tokens

    try:
        if not RUNS_JSONL.exists():
            return
        lines = RUNS_JSONL.read_text().splitlines(keepends=True)
        last_entry = None
        last_idx = None
        for i, line in enumerate(lines):
            try:
                entry = json.loads(line)
                if entry.get("issue_number") == issue_number:
                    last_entry = entry
                    last_idx = i
            except json.JSONDecodeError:
                pass

        if not last_entry:
            return

        log_dir = _resolve_log_dir(last_entry.get("log_dir", ""))
        if log_dir.is_dir():
            (log_dir / "cli_output.json").write_text(cli_stdout)

        if tokens and last_idx is not None:
            last_entry["tokens"] = tokens
            lines[last_idx] = json.dumps(last_entry, separators=(",", ":")) + "\n"
            import tempfile

            tmp_fd, tmp_path = tempfile.mkstemp(dir=str(RUNS_BASE), suffix=".jsonl")
            try:
                with os.fdopen(tmp_fd, "w") as tmp_f:
                    tmp_f.writelines(lines)
                os.replace(tmp_path, str(RUNS_JSONL))
            except Exception:
                try:
                    os.unlink(tmp_path)
                except OSError:
                    pass

            if log_dir.is_dir():
                run_json = log_dir / "run.json"
                if run_json.is_file():
                    try:
                        run_data = json.loads(run_json.read_text())
                        run_data["tokens"] = tokens
                        run_json.write_text(json.dumps(run_data, indent=2) + "\n")
                    except Exception:
                        pass
    except Exception:
        pass


def run_issue(
    issue: dict, batch_id: str, model: str, dry_run: bool, timeout: int
) -> dict:
    """Run codegen for a single Blackhole P2 issue via claude CLI."""
    num = issue["number"]
    title = issue["title"]
    prompt = f"Investigate and fix Blackhole issue #{num}: {title}"
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
        "issue_number": num,
        "title": title,
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

        if proc.stdout.strip():
            _save_cli_output_and_patch_tokens(proc.stdout, num, batch_id, result)

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
    parser = argparse.ArgumentParser(
        description="Weekly CI runner for Blackhole LLK CodeGen"
    )
    parser.add_argument(
        "--issues",
        nargs="+",
        type=int,
        default=None,
        help="Specific issue numbers to run (default: all open BH P2 issues)",
    )
    parser.add_argument(
        "--label",
        default=None,
        help="Additional label filter (e.g., LLK, feature)",
    )
    parser.add_argument(
        "--max-issues",
        type=int,
        default=None,
        help="Limit to first N issues",
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
        help="Timeout per issue in seconds (default: 14400 / 4 hours)",
    )
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="Print commands without executing",
    )
    args = parser.parse_args()

    batch_id = args.batch_id or f"{datetime.now().strftime('%Y-%m-%d')}_bh_weekly"

    # Fetch issues from GitHub
    print("Fetching Blackhole P2 issues from GitHub...")
    issues = fetch_issues(extra_label=args.label)

    # Filter by specific issue numbers if provided
    if args.issues:
        issue_set = set(args.issues)
        issues = [i for i in issues if i["number"] in issue_set]
        missing = issue_set - {i["number"] for i in issues}
        if missing:
            print(f"  Warning: issues not found with blackhole+P2 labels: {missing}")

    # Apply max-issues limit
    if args.max_issues:
        issues = issues[: args.max_issues]

    if not issues:
        print("No matching Blackhole P2 issues found.")
        sys.exit(1)

    print()
    print("=" * 60)
    print("  LLK CodeGen -- Blackhole Weekly CI Run")
    print("=" * 60)
    print(f"  Batch ID:  {batch_id}")
    print(f"  Model:     {args.model}")
    print(f"  Issues:    {len(issues)}")
    for issue in issues:
        assignee = issue["assignees"][0] if issue["assignees"] else "(none)"
        print(f"             #{issue['number']}: {issue['title'][:50]}  [{assignee}]")
    print(f"  Timeout:   {args.timeout}s per issue")
    print(f"  Run type:  ci")
    print(f"  Started:   {datetime.now().isoformat()}")
    print("=" * 60)
    print()

    results = []
    for i, issue in enumerate(issues, 1):
        print(f"[{i}/{len(issues)}] #{issue['number']}: {issue['title']}")
        result = run_issue(issue, batch_id, args.model, args.dry_run, args.timeout)
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

    print("=" * 60)
    print("  Summary")
    print("=" * 60)
    print(f"  Completed: {completed}/{len(results)}")
    if crashed:
        print(f"  Crashed:   {crashed}")
        for r in results:
            if r["status"] == "crashed":
                print(f"             - #{r['issue_number']}: {r['title']}")
    if total_tokens["total"] > 0:
        print(
            f"  Tokens:    {total_tokens['total']:,} total"
            f" ({total_tokens['input']:,} in,"
            f" {total_tokens['output']:,} out,"
            f" {total_tokens['cache_read']:,} cached)"
        )
    print(f"  Finished:  {datetime.now().isoformat()}")
    print("=" * 60)

    # Write summary JSON
    summary_path = CODEGEN_DIR / "artifacts" / "ci_weekly_bh_summary.json"
    summary_path.parent.mkdir(parents=True, exist_ok=True)
    summary = {
        "batch_id": batch_id,
        "model": args.model,
        "started": datetime.now().isoformat(),
        "total": len(results),
        "completed": completed,
        "crashed": crashed,
        "tokens": total_tokens,
        "results": results,
    }
    summary_path.write_text(json.dumps(summary, indent=2) + "\n")
    print(f"  Summary:   {summary_path}")

    sys.exit(1 if crashed > 0 else 0)


if __name__ == "__main__":
    main()
