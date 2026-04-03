# Blackhole P2 Issue Scripts

Scripts for fetching and running codegen against Blackhole P2 issues from `tenstorrent/tt-llk`.

## Prerequisites

- `gh` CLI authenticated (`gh auth status`)
- `claude` CLI available on PATH
- Python 3.10+

## Quick Start

```bash
cd codegen-bh

# List all open Blackhole P2 issues
bash scripts/batch_generate_bh.sh

# Dry-run a single issue (shows prompt, doesn't execute)
bash scripts/batch_generate_bh.sh --issue 1153 --dry-run

# Run codegen for a single issue
bash scripts/batch_generate_bh.sh --issue 1153
```

## fetch_bh_issues.py

Fetches issues from GitHub and saves to `artifacts/bh_p2_issues.json`.

```bash
# Fetch and display summary table
python scripts/fetch_bh_issues.py --summary

# Filter by additional label
python scripts/fetch_bh_issues.py --extra-label LLK --summary

# Include closed issues
python scripts/fetch_bh_issues.py --state all --summary

# Output JSON to stdout (for piping)
python scripts/fetch_bh_issues.py --stdout | jq '.issues[] | .number, .title'
```

## batch_generate_bh.sh

Batch runner that invokes `claude -p` for each issue. Issues are fetched from GitHub on first run and cached locally. Use `--refresh` to re-fetch.

### Branch isolation

Each issue runs on its own branch: `$(whoami)/issue-{number}-codegen`, branched from `origin/main`.

- **Sequential mode**: checks out each branch in turn, restores the original branch between issues
- **Parallel mode**: uses git worktrees under `/tmp/codegen_bh_worktree_{number}/` so each issue gets an isolated checkout (worktrees are cleaned up after completion)
- If a branch already exists (local or remote), it's reused rather than recreated

### Running specific issues

```bash
# Single issue by number
bash scripts/batch_generate_bh.sh --issue 960

# Preview the prompt without running
bash scripts/batch_generate_bh.sh --issue 960 --dry-run
```

### Filtering by label

```bash
# Only LLK-labeled issues
bash scripts/batch_generate_bh.sh --label LLK --run

# Only feature requests
bash scripts/batch_generate_bh.sh --label feature --run
```

### Running all issues

```bash
# Sequential (stops on first failure)
bash scripts/batch_generate_bh.sh --run

# Parallel (all at once)
bash scripts/batch_generate_bh.sh --run --parallel

# Parallel with concurrency limit
bash scripts/batch_generate_bh.sh --run -j 4
```

### Model selection

```bash
bash scripts/batch_generate_bh.sh --issue 1153 --model sonnet
bash scripts/batch_generate_bh.sh --run --model haiku --dry-run
```

### Refreshing the issue cache

```bash
# Re-fetch from GitHub before listing
bash scripts/batch_generate_bh.sh --refresh

# Re-fetch and run
bash scripts/batch_generate_bh.sh --refresh --run
```

## ci_weekly_bh.py

Weekly CI runner. Fetches issues live (no cache), runs each sequentially with a 4-hour timeout, writes a summary JSON to `artifacts/ci_weekly_bh_summary.json`.

### Running specific issues

```bash
# Run two specific issues
python scripts/ci_weekly_bh.py --issues 1153 960

# Dry-run to verify
python scripts/ci_weekly_bh.py --issues 1153 960 --dry-run
```

### Limiting scope

```bash
# First 5 issues only
python scripts/ci_weekly_bh.py --max-issues 5

# Only LLK-labeled issues
python scripts/ci_weekly_bh.py --label LLK
```

### Crontab setup

```bash
# Every Friday at 14:00
0 14 * * 5 cd /proj_sw/user_dev/$(whoami)/tt-llk/codegen-bh && python scripts/ci_weekly_bh.py >> /tmp/ci_weekly_bh.log 2>&1

# Weekly with sonnet model, limited to 5 issues
0 14 * * 5 cd /proj_sw/user_dev/$(whoami)/tt-llk/codegen-bh && python scripts/ci_weekly_bh.py --model sonnet --max-issues 5 >> /tmp/ci_weekly_bh.log 2>&1
```

## Outputs

| Path | Content |
|------|---------|
| `artifacts/bh_p2_issues.json` | Cached issue data from GitHub |
| `artifacts/ci_weekly_bh_summary.json` | CI run summary with per-issue results and token usage |
| `/tmp/codegen_bh_logs_*/` | Per-run logs (batch runner) |
| `../../llk_code_gen/blackhole_issue_solver/runs.jsonl` | Append-only run history |
