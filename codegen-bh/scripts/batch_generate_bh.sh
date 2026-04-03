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
RUNS_JSONL="/proj_sw/user_dev/llk_code_gen/blackhole/runs.jsonl"
RUNS_BASE="/proj_sw/user_dev/llk_code_gen/blackhole"

# CI environment
export CODEGEN_BATCH_ID="${CODEGEN_BATCH_ID:-$(date +%Y-%m-%d)_bh_batch}"
export CODEGEN_MODEL="${CODEGEN_MODEL:-opus}"

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
      echo "  --model MODEL Claude model to use (default: opus)"
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

# --- Save CLI JSON output and patch token/cost data ---
save_cli_output() {
  local issue_num="$1" json_file="$2"
  python3 -c "
import json, os, sys, tempfile

RUNS_JSONL = '$RUNS_JSONL'
RUNS_BASE  = '$RUNS_BASE'

issue_num, json_file = sys.argv[1], sys.argv[2]

def resolve_log_dir(log_dir):
    if os.path.isabs(log_dir):
        return log_dir
    candidate = os.path.join(RUNS_BASE, log_dir)
    if os.path.isdir(candidate):
        return candidate
    if log_dir.startswith('logs/'):
        candidate = os.path.join(RUNS_BASE, log_dir[5:])
        if os.path.isdir(candidate):
            return candidate
    return log_dir

def extract_tokens(cli_json_path):
    try:
        with open(cli_json_path) as f:
            data = json.load(f)
        if not isinstance(data, list) or len(data) == 0:
            return None
        last = data[-1]
        if not isinstance(last, dict):
            return None
        model_usage = last.get('modelUsage', {})
        if model_usage:
            total_input = sum(v.get('inputTokens', 0) for v in model_usage.values())
            total_output = sum(v.get('outputTokens', 0) for v in model_usage.values())
            total_cache_read = sum(v.get('cacheReadInputTokens', 0) for v in model_usage.values())
            total_cache_creation = sum(v.get('cacheCreationInputTokens', 0) for v in model_usage.values())
            cost = last.get('total_cost_usd', 0)
            return {
                'input': total_input,
                'output': total_output,
                'cache_read': total_cache_read,
                'cache_creation': total_cache_creation,
                'total': total_input + total_output,
                'cost_usd': round(cost, 6) if cost else 0,
            }
        usage = last.get('usage', {})
        if usage:
            inp = usage.get('input_tokens', 0)
            out = usage.get('output_tokens', 0)
            return {
                'input': inp,
                'output': out,
                'cache_read': usage.get('cache_read_input_tokens', 0),
                'cache_creation': usage.get('cache_creation_input_tokens', 0),
                'total': inp + out,
                'cost_usd': round(last.get('total_cost_usd', 0), 6),
            }
        return None
    except Exception:
        return None

try:
    if not os.path.isfile(RUNS_JSONL):
        sys.exit(0)
    last_entry = None
    last_line_idx = None
    lines = []
    with open(RUNS_JSONL) as f:
        for i, line in enumerate(f):
            lines.append(line)
            try:
                entry = json.loads(line)
                if str(entry.get('issue_number', '')) == str(issue_num):
                    last_entry = entry
                    last_line_idx = i
            except:
                pass

    if not last_entry:
        print(f'  Warning: no runs.jsonl entry found for issue #{issue_num}', file=sys.stderr)
        sys.exit(0)

    log_dir = resolve_log_dir(last_entry.get('log_dir', ''))
    if os.path.isdir(log_dir):
        import shutil
        shutil.copy2(json_file, os.path.join(log_dir, 'cli_output.json'))
        print(f'  Saved CLI output to {log_dir}/cli_output.json')

    tokens = extract_tokens(json_file)
    if tokens:
        last_entry['tokens'] = tokens
        lines[last_line_idx] = json.dumps(last_entry, separators=(',', ':')) + '\n'
        tmp_fd, tmp_path = tempfile.mkstemp(dir=RUNS_BASE, suffix='.jsonl')
        try:
            with os.fdopen(tmp_fd, 'w') as tmp_f:
                tmp_f.writelines(lines)
            os.replace(tmp_path, RUNS_JSONL)
            print(f'  Patched runs.jsonl: input={tokens[\"input\"]} output={tokens[\"output\"]} cost=\${tokens[\"cost_usd\"]}')
        except Exception as e:
            try: os.unlink(tmp_path)
            except: pass
            print(f'  Warning: could not patch runs.jsonl: {e}', file=sys.stderr)
except Exception as e:
    print(f'  Warning: save_cli_output failed: {e}', file=sys.stderr)
" "$issue_num" "$json_file"
}

# --- Run a single issue ---
run_one_issue() {
  local num="$1" title="$2" total="$3"
  local prompt="Investigate and fix Blackhole issue #${num}: ${title}. CODEGEN_BATCH_ID=${CODEGEN_BATCH_ID} CODEGEN_MODEL=${MODEL}"
  local logfile="${LOG_DIR}/issue_${num}.log"
  local jsonfile="${LOG_DIR}/issue_${num}.json"

  echo "[#${num}/${total}] START: ${title} (model: $MODEL)"

  cd "$CODEGEN_DIR"
  claude -p "$prompt" --dangerously-skip-permissions --effort max --verbose --model "$MODEL" --output-format json > "$jsonfile" 2>"$logfile"
  local exit_code=$?

  save_cli_output "$num" "$jsonfile"

  if [[ $exit_code -ne 0 ]]; then
    echo "[#${num}/${total}] FAILED (exit code $exit_code) -- see $logfile"
    return 1
  else
    echo "[#${num}/${total}] DONE: #${num}"
    return 0
  fi
}

# --- Sequential run ---
run_sequential() {
  local total=${#ISSUE_LINES[@]}
  local idx=0
  for entry in "${ISSUE_LINES[@]}"; do
    IFS=$'\t' read -r num title labels assignees <<< "$entry"
    idx=$((idx + 1))

    prompt="Investigate and fix Blackhole issue #${num}: ${title}. CODEGEN_BATCH_ID=${CODEGEN_BATCH_ID} CODEGEN_MODEL=${MODEL}"

    echo "[${idx}/${total}] #${num}: ${title}"

    if $DRY_RUN; then
      echo "  Prompt: $prompt"
      echo "  (dry run -- skipping)"
      echo ""
      continue
    fi

    cd "$CODEGEN_DIR"
    claude -p "$prompt" --dangerously-skip-permissions --effort max --verbose --model "$MODEL" --output-format json 2>&1 1>"${LOG_DIR}/issue_${num}.json" | tee "${LOG_DIR}/issue_${num}.log"

    exit_code=${PIPESTATUS[0]}

    save_cli_output "$num" "${LOG_DIR}/issue_${num}.json"

    if [[ $exit_code -ne 0 ]]; then
      echo "  FAILED (exit code $exit_code) -- stopping."
      exit 1
    fi

    echo "  Done."
    echo ""
  done
}

# --- Parallel run ---
run_parallel() {
  local pids=()
  local nums=()
  local active=0
  local total=${#ISSUE_LINES[@]}

  for entry in "${ISSUE_LINES[@]}"; do
    IFS=$'\t' read -r num title labels assignees <<< "$entry"

    if $DRY_RUN; then
      echo "[#${num}/${total}] Blackhole issue #${num}: ${title} (dry run -- skipping)"
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

    run_one_issue "$num" "$title" "$total" &
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
