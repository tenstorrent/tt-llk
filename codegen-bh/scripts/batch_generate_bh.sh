#!/bin/bash
# SPDX-FileCopyrightText: (c) 2026 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0

# Batch LLK kernel generation for Blackhole, driven by GitHub P2 issues.
#
# Fetches open Blackhole P2 issues from tenstorrent/tt-llk, then runs codegen
# for each issue's kernel. Issues are categorized by type (SFPU, math, pack,
# unpack) and can be filtered by label or issue number.
#
# Usage:
#   ./scripts/batch_generate_bh.sh                          # list all BH P2 issues
#   ./scripts/batch_generate_bh.sh --run                    # run all sequentially
#   ./scripts/batch_generate_bh.sh --run --parallel          # run all in parallel
#   ./scripts/batch_generate_bh.sh --run -j 4               # max 4 concurrent
#   ./scripts/batch_generate_bh.sh --issue 1153              # run single issue
#   ./scripts/batch_generate_bh.sh --label LLK --run        # only LLK-labeled issues
#   ./scripts/batch_generate_bh.sh --refresh                 # re-fetch issues from GitHub
#   ./scripts/batch_generate_bh.sh --run --dry-run           # show prompts without running
#   ./scripts/batch_generate_bh.sh --run --model sonnet      # use a different model

set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
CODEGEN_DIR="$(dirname "$SCRIPT_DIR")"
ISSUES_JSON="${CODEGEN_DIR}/artifacts/bh_p2_issues.json"
LOG_DIR="/tmp/codegen_bh_logs_$(date +%Y%m%d_%H%M%S)"
GIT_USER="$(whoami)"
REPO_ROOT="$(cd "$CODEGEN_DIR/.." && pwd)"
RUNS_BASE="$(cd "$REPO_ROOT/../../llk_code_gen/blackhole_issue_solver" 2>/dev/null && pwd || echo "$REPO_ROOT/../../llk_code_gen/blackhole_issue_solver")"
RUNS_JSONL="${RUNS_BASE}/runs.jsonl"

# CI environment
export CODEGEN_BATCH_ID="${CODEGEN_BATCH_ID:-$(date +%Y-%m-%d)_bh_batch}"
export CODEGEN_MODEL="${CODEGEN_MODEL:-claude-opus-4-6}"

# --- Parse args ---
RUN=false
ISSUE_NUM=""
EXTRA_LABEL=""
REFRESH=false
DRY_RUN=false
PARALLEL=false
MAX_JOBS=0
MODEL="${CODEGEN_MODEL}"

while [[ $# -gt 0 ]]; do
  case $1 in
    --run)      RUN=true; shift ;;
    --issue)    ISSUE_NUM="$2"; RUN=true; shift 2 ;;
    --label)    EXTRA_LABEL="$2"; shift 2 ;;
    --refresh)  REFRESH=true; shift ;;
    --dry-run)  DRY_RUN=true; shift ;;
    --parallel) PARALLEL=true; shift ;;
    -j)         MAX_JOBS="$2"; PARALLEL=true; shift 2 ;;
    --model)    MODEL="$2"; shift 2 ;;
    --help|-h)
      echo "Usage: $0 [--run] [--issue NUM] [--label LABEL] [--refresh] [--parallel] [-j N] [--model MODEL] [--dry-run]"
      echo ""
      echo "  --run         Run codegen for fetched issues (without this, just lists)"
      echo "  --issue NUM   Run codegen for a single issue number"
      echo "  --label LABEL Filter by additional GitHub label (e.g., LLK, feature)"
      echo "  --refresh     Re-fetch issues from GitHub before listing/running"
      echo "  --parallel    Run issues in parallel"
      echo "  -j N          Max concurrent jobs (default: unlimited)"
      echo "  --model MODEL Claude model to use (default: claude-opus-4-6)"
      echo "  --dry-run     Show prompts without running"
      echo ""
      echo "First run will auto-fetch issues from GitHub. Use --refresh to update."
      exit 0 ;;
    *) echo "Unknown arg: $1"; exit 1 ;;
  esac
done

export CODEGEN_MODEL="$MODEL"

# --- Fetch issues if needed ---
fetch_issues() {
  local extra_args=()
  if [[ -n "$EXTRA_LABEL" ]]; then
    extra_args+=(--extra-label "$EXTRA_LABEL")
  fi
  echo "Fetching Blackhole P2 issues from GitHub..."
  python3 "${SCRIPT_DIR}/fetch_bh_issues.py" -o "$ISSUES_JSON" --summary "${extra_args[@]}"
}

if [[ "$REFRESH" == true ]] || [[ ! -f "$ISSUES_JSON" ]]; then
  fetch_issues
fi

# --- Parse issues from JSON ---
# Extract issue list as tab-separated: "number\ttitle\tlabels\tassignees"
parse_issues() {
  python3 -c "
import json, sys

with open('$ISSUES_JSON') as f:
    data = json.load(f)

issue_num_filter = '$ISSUE_NUM'
extra_label = '$EXTRA_LABEL'

for issue in data['issues']:
    num = issue['number']
    if issue_num_filter and str(num) != issue_num_filter:
        continue
    if extra_label and extra_label not in issue['labels']:
        continue
    title = issue['title'].replace('\t', ' ')
    labels = ','.join(l for l in issue['labels'] if l not in ('blackhole', 'P2'))
    assignees = ','.join(issue['assignees']) if issue['assignees'] else '(none)'
    print(f'{num}\t{title}\t{labels}\t{assignees}')
"
}

mapfile -t ISSUE_LINES < <(parse_issues)

if [[ ${#ISSUE_LINES[@]} -eq 0 ]]; then
  echo "No matching Blackhole P2 issues found."
  if [[ -n "$ISSUE_NUM" ]]; then
    echo "Issue #${ISSUE_NUM} may not have 'blackhole' + 'P2' labels, or may be closed."
  fi
  exit 1
fi

# --- List mode (no --run) ---
if [[ "$RUN" == false ]]; then
  echo ""
  echo "=== Blackhole P2 Issues: ${#ISSUE_LINES[@]} issue(s) ==="
  echo "=== Model: $MODEL ==="
  echo ""
  printf "  %-6s %-55s %-15s %s\n" "#" "Title" "Assignee" "Labels"
  printf "  %-6s %-55s %-15s %s\n" "------" "-------" "--------" "------"
  for entry in "${ISSUE_LINES[@]}"; do
    IFS=$'\t' read -r num title labels assignees <<< "$entry"
    title_short="${title:0:53}"
    [[ ${#title} -gt 55 ]] && title_short="${title_short}.."
    assignee_short="${assignees:0:13}"
    [[ ${#assignees} -gt 13 ]] && assignee_short="${assignee_short}.."
    printf "  #%-5s %-55s %-15s %s\n" "$num" "$title_short" "$assignee_short" "$labels"
  done
  echo ""
  echo "Run with: $0 --run                    # all issues sequentially"
  echo "          $0 --run --parallel          # all in parallel"
  echo "          $0 --run -j 4               # max 4 concurrent"
  echo "          $0 --issue 1153              # single issue"
  echo "          $0 --label LLK --run         # filter by label"
  echo "          $0 --refresh                 # re-fetch from GitHub"
  echo "          $0 --run --dry-run           # preview prompts"
  exit 0
fi

# --- Branch management ---
create_issue_branch() {
  local num="$1"
  local branch="${GIT_USER}/issue-${num}-codegen"

  cd "$REPO_ROOT"

  # Check if branch already exists (local or remote)
  if git rev-parse --verify "$branch" &>/dev/null; then
    echo "  Branch $branch already exists, checking out"
    git checkout "$branch"
  elif git rev-parse --verify "origin/$branch" &>/dev/null; then
    echo "  Branch $branch exists on remote, checking out"
    git checkout -b "$branch" "origin/$branch"
  else
    echo "  Creating branch $branch from main"
    git checkout -b "$branch" origin/main
  fi
}

restore_branch() {
  local original="$1"
  cd "$REPO_ROOT"
  git checkout "$original" 2>/dev/null || true
}

# --- Prompt template ---
make_prompt() {
  local num="$1" title="$2"
  echo "Investigate and fix Blackhole issue #${num}: ${title}. Work autonomously -- use superpowers skills, do not ask questions. Test your changes thoroughly before committing -- compile, run existing tests, and add new tests if none exist. Commit your changes when done. CODEGEN_BATCH_ID=${CODEGEN_BATCH_ID} CODEGEN_MODEL=${MODEL}"
}

# --- Log run result to runs.jsonl and create run directory ---
log_run() {
  local issue_num="$1" title="$2" branch="$3" status="$4" start_time="$5" end_time="$6" tmp_log_dir="$7"
  python3 -c "
import json, sys, os, fcntl, shutil, re, glob
from datetime import datetime

RUNS_JSONL = '$RUNS_JSONL'
RUNS_BASE  = '$RUNS_BASE'
REPO_ROOT  = '$REPO_ROOT'

issue_num = int(sys.argv[1])
title = sys.argv[2]
branch = sys.argv[3]
status = sys.argv[4]
start_time = sys.argv[5]
end_time = sys.argv[6]
tmp_log_dir = sys.argv[7]
model = '$MODEL'
batch_id = '$CODEGEN_BATCH_ID'

# Build a clean dir name from the title
safe_title = re.sub(r'[^a-z0-9]+', '_', title.lower().strip())[:60].strip('_')
run_id = f'{datetime.now().strftime(\"%Y-%m-%d\")}_issue{issue_num}_{safe_title}'
run_dir = os.path.join(RUNS_BASE, run_id)
os.makedirs(run_dir, exist_ok=True)

# Copy logs from tmp dir into run dir
for pattern in [f'issue_{issue_num}.log', f'issue_{issue_num}.json']:
    src = os.path.join(tmp_log_dir, pattern)
    if os.path.isfile(src):
        shutil.copy2(src, run_dir)

# Snapshot changed files from the branch
try:
    import subprocess
    result = subprocess.run(
        ['git', 'diff', '--name-only', 'origin/main...HEAD'],
        capture_output=True, text=True, cwd=REPO_ROOT, timeout=10
    )
    changed = [f.strip() for f in result.stdout.strip().splitlines() if f.strip()]
    for fpath in changed:
        full = os.path.join(REPO_ROOT, fpath)
        if os.path.isfile(full):
            # Flatten path: tt_llk_blackhole/llk_lib/foo.h -> tt_llk_blackhole_llk_lib_foo.h
            flat_name = fpath.replace('/', '_')
            shutil.copy2(full, os.path.join(run_dir, flat_name))
except Exception:
    pass

# Fetch issue metadata from cached JSON if available
issue_meta = {'number': issue_num, 'title': title, 'url': f'https://github.com/tenstorrent/tt-llk/issues/{issue_num}', 'labels': []}
try:
    with open('$ISSUES_JSON') as f:
        data = json.load(f)
    for iss in data.get('issues', []):
        if iss['number'] == issue_num:
            issue_meta['labels'] = iss.get('labels', [])
            issue_meta['url'] = iss.get('url', issue_meta['url'])
            break
except Exception:
    pass

entry = {
    'run_id': run_id,
    'arch': 'blackhole',
    'start_time': start_time,
    'end_time': end_time,
    'status': status,
    'model': model,
    'run_type': 'ci' if batch_id else 'manual',
    'issue': issue_meta,
    'git_branch': branch,
    'batch_id': batch_id or None,
    'log_dir': run_id,
}

# Write run.json into the run directory
with open(os.path.join(run_dir, 'run.json'), 'w') as f:
    json.dump(entry, f, indent=2)
    f.write('\n')

# Append to runs.jsonl
os.makedirs(os.path.dirname(RUNS_JSONL), exist_ok=True)
line = json.dumps(entry, separators=(',', ':')) + '\n'
with open(RUNS_JSONL, 'a') as f:
    fcntl.flock(f, fcntl.LOCK_EX)
    f.write(line)
    fcntl.flock(f, fcntl.LOCK_UN)

print(f'  Logged to {run_dir}/')
print(f'  Appended to {RUNS_JSONL}')
" "$issue_num" "$title" "$branch" "$status" "$start_time" "$end_time" "$tmp_log_dir"
}

# --- Run a single issue (used by parallel mode) ---
run_one_issue() {
  local num="$1" title="$2" total="$3"
  local prompt
  prompt="$(make_prompt "$num" "$title")"
  local logfile="${LOG_DIR}/issue_${num}.log"
  local jsonfile="${LOG_DIR}/issue_${num}.json"
  local branch="${GIT_USER}/issue-${num}-codegen"
  local start_time
  start_time="$(date -u +%Y-%m-%dT%H:%M:%SZ)"

  echo "[#${num}/${total}] START: ${title} (model: $MODEL)"
  echo "  Branch: $branch"

  create_issue_branch "$num"

  cd "$CODEGEN_DIR"
  claude -p "$prompt" --dangerously-skip-permissions --effort max --verbose --model "$MODEL" --output-format json > "$jsonfile" 2>"$logfile"
  local exit_code=$?
  local end_time
  end_time="$(date -u +%Y-%m-%dT%H:%M:%SZ)"

  local status="completed"
  if [[ $exit_code -ne 0 ]]; then
    status="crashed"
  fi

  log_run "$num" "$title" "$branch" "$status" "$start_time" "$end_time" "$LOG_DIR"

  if [[ $exit_code -ne 0 ]]; then
    echo "[#${num}/${total}] FAILED (exit code $exit_code) -- see $logfile"
    return 1
  else
    echo "[#${num}/${total}] DONE: #${num} (branch: $branch)"
    return 0
  fi
}

# --- Sequential run ---
run_sequential() {
  local total=${#ISSUE_LINES[@]}
  local idx=0
  local original_branch
  original_branch="$(cd "$REPO_ROOT" && git rev-parse --abbrev-ref HEAD)"

  for entry in "${ISSUE_LINES[@]}"; do
    IFS=$'\t' read -r num title labels assignees <<< "$entry"
    idx=$((idx + 1))

    local branch="${GIT_USER}/issue-${num}-codegen"
    local prompt
    prompt="$(make_prompt "$num" "$title")"

    echo "[${idx}/${total}] #${num}: ${title}"
    echo "  Branch: $branch"

    if $DRY_RUN; then
      echo "  Prompt: $prompt"
      echo "  (dry run -- skipping)"
      echo ""
      continue
    fi

    local start_time
    start_time="$(date -u +%Y-%m-%dT%H:%M:%SZ)"

    create_issue_branch "$num"

    cd "$CODEGEN_DIR"
    claude -p "$prompt" --dangerously-skip-permissions --effort max --verbose --model "$MODEL" --output-format json 2>&1 1>"${LOG_DIR}/issue_${num}.json" | tee "${LOG_DIR}/issue_${num}.log"

    exit_code=${PIPESTATUS[0]}
    local end_time
    end_time="$(date -u +%Y-%m-%dT%H:%M:%SZ)"

    local status="completed"
    if [[ $exit_code -ne 0 ]]; then
      status="crashed"
    fi

    log_run "$num" "$title" "$branch" "$status" "$start_time" "$end_time" "$LOG_DIR"

    # Return to original branch before continuing to the next issue
    restore_branch "$original_branch"

    if [[ $exit_code -ne 0 ]]; then
      echo "  FAILED (exit code $exit_code) -- stopping."
      exit 1
    fi

    echo "  Done."
    echo ""
  done
}

# --- Parallel run ---
# NOTE: Parallel mode uses git worktrees so each issue gets its own checkout.
# Each worktree is created under /tmp/codegen_bh_worktree_<issue>/ and cleaned
# up after the run completes (successful or not).
run_parallel() {
  local pids=()
  local nums=()
  local worktrees=()
  local active=0
  local total=${#ISSUE_LINES[@]}

  for entry in "${ISSUE_LINES[@]}"; do
    IFS=$'\t' read -r num title labels assignees <<< "$entry"
    local branch="${GIT_USER}/issue-${num}-codegen"

    if $DRY_RUN; then
      echo "[#${num}/${total}] Blackhole issue #${num}: ${title}"
      echo "  Branch: $branch"
      echo "  (dry run -- skipping)"
      continue
    fi

    # Throttle if max jobs reached
    if [[ $MAX_JOBS -gt 0 ]]; then
      while [[ $active -ge $MAX_JOBS ]]; do
        wait -n 2>/dev/null || true
        active=0
        for pid in "${pids[@]}"; do
          if kill -0 "$pid" 2>/dev/null; then
            active=$((active + 1))
          fi
        done
      done
    fi

    # Create worktree for this issue
    local wt_dir="/tmp/codegen_bh_worktree_${num}"
    cd "$REPO_ROOT"

    # Create the branch if it doesn't exist
    if ! git rev-parse --verify "$branch" &>/dev/null; then
      if git rev-parse --verify "origin/$branch" &>/dev/null; then
        git branch "$branch" "origin/$branch"
      else
        git branch "$branch" origin/main
      fi
    fi

    # Clean up stale worktree if it exists
    if [[ -d "$wt_dir" ]]; then
      git worktree remove --force "$wt_dir" 2>/dev/null || rm -rf "$wt_dir"
    fi

    git worktree add "$wt_dir" "$branch"
    worktrees+=("$wt_dir")

    # Run in the worktree
    (
      local prompt
      prompt="$(make_prompt "$num" "$title")"
      local logfile="${LOG_DIR}/issue_${num}.log"
      local jsonfile="${LOG_DIR}/issue_${num}.json"
      local start_time
      start_time="$(date -u +%Y-%m-%dT%H:%M:%SZ)"

      echo "[#${num}/${total}] START: ${title} (model: $MODEL)"
      echo "  Branch: $branch"
      echo "  Worktree: $wt_dir"

      cd "${wt_dir}/codegen-bh"
      claude -p "$prompt" --dangerously-skip-permissions --effort max --verbose --model "$MODEL" --output-format json > "$jsonfile" 2>"$logfile"
      local exit_code=$?
      local end_time
      end_time="$(date -u +%Y-%m-%dT%H:%M:%SZ)"

      local status="completed"
      [[ $exit_code -ne 0 ]] && status="crashed"

      log_run "$num" "$title" "$branch" "$status" "$start_time" "$end_time" "$LOG_DIR"

      if [[ $exit_code -ne 0 ]]; then
        echo "[#${num}/${total}] FAILED (exit code $exit_code) -- see $logfile"
        exit 1
      else
        echo "[#${num}/${total}] DONE: #${num} (branch: $branch)"
        exit 0
      fi
    ) &
    pids+=($!)
    nums+=("$num")
    active=$((active + 1))
  done

  if $DRY_RUN; then return; fi

  echo ""
  echo "=== Waiting for ${#pids[@]} parallel job(s) to complete ==="
  echo ""

  local failed=0
  for i in "${!pids[@]}"; do
    if ! wait "${pids[$i]}"; then
      failed=$((failed + 1))
    fi
  done

  # Clean up worktrees
  cd "$REPO_ROOT"
  for wt in "${worktrees[@]}"; do
    git worktree remove --force "$wt" 2>/dev/null || true
  done

  echo ""
  if [[ $failed -gt 0 ]]; then
    echo "=== ${failed} issue(s) FAILED -- check logs in ${LOG_DIR}/ ==="
    return 1
  fi
}

# --- Main ---
mkdir -p "$LOG_DIR"
mkdir -p "$RUNS_BASE"

mode="sequentially"
if $PARALLEL; then mode="in parallel"; fi
echo "=== Will process ${#ISSUE_LINES[@]} Blackhole P2 issue(s) ${mode} ==="
echo "=== Model: $MODEL ==="
if $PARALLEL && [[ $MAX_JOBS -gt 0 ]]; then
  echo "=== Max concurrent jobs: ${MAX_JOBS} ==="
fi
echo ""

if $PARALLEL; then
  run_parallel
else
  run_sequential
fi

echo "=== All ${#ISSUE_LINES[@]} issue(s) complete ==="
echo "=== Logs: ${LOG_DIR}/ ==="
