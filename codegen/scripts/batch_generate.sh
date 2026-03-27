#!/bin/bash
# Batch LLK kernel generation for Quasar
# Usage:
#   ./scripts/batch_generate.sh                        # list all kernels
#   ./scripts/batch_generate.sh --wave 1               # run all Wave 1 sequentially
#   ./scripts/batch_generate.sh --wave 1 --parallel    # run all Wave 1 in parallel
#   ./scripts/batch_generate.sh --kernel abs           # run a single kernel
#   ./scripts/batch_generate.sh --from 5               # resume from kernel #5
#   ./scripts/batch_generate.sh --wave 1 --dry-run     # show what would run
#   ./scripts/batch_generate.sh --parallel -j 4        # limit to 4 concurrent jobs
#   ./scripts/batch_generate.sh --model sonnet          # use a different model
#
# Waves are ordered to maximize test feedback early:
#   Wave 1: Testable simple SFPU (4)     — have golden generators
#   Wave 2: Testable medium SFPU (5)     — have golden generators
#   Wave 3: Remaining simple SFPU (6)    — compile-only
#   Wave 4: Remaining medium SFPU (9)    — compile-only
#   Wave 5: Complex SFPU w/ tests (4)    — have test potential
#   Wave 6: Remaining complex SFPU (8)   — compile-only
#   Wave 7: Specialized SFPU (6)         — compile-only
#   Wave 8: LLK Submodule core (10)      — math wrappers, pack, unpack
#   Wave 9: LLK Submodule experimental (13) — low priority
#
# Parallel safety:
#   - Each kernel writes to its own SFPU file — no conflicts
#   - Compilation uses per-PID build dirs — no conflicts
#   - runs.jsonl uses file locking for safe concurrent appends
#   - ckernel_sfpu.h is NOT edited during generation — update it after
#   - Wave 8-9 depends on Waves 1-7; do NOT run Wave 8-9 in parallel with earlier waves

set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
CODEGEN_DIR="$(dirname "$SCRIPT_DIR")"
LOG_DIR="/tmp/codegen_logs_$(date +%Y%m%d_%H%M%S)"

# CI environment — tells the orchestrator this is an automated batch run
export CODEGEN_BATCH_ID="${CODEGEN_BATCH_ID:-$(date +%Y-%m-%d)_batch}"
export CODEGEN_MODEL="${CODEGEN_MODEL:-opus}"

# --- Kernel definitions ---
# Format: "number|wave|kernel_name|type|notes"
# Ordered by wave to maximize testable kernels early.

KERNELS=(
  # Wave 1: Testable simple SFPU — have golden generators in test infra
  "1|1|abs|sfpu|has golden: _abs"
  "2|1|negative|sfpu|has golden: _neg"
  "3|1|fill|sfpu|has golden: _fill"
  "4|1|threshold|sfpu|has golden: _threshold"

  # Wave 2: Testable medium SFPU — have golden generators
  "5|2|elu|sfpu|has golden: _elu, uses exp"
  "6|2|exp2|sfpu|has golden: _exp2"
  "7|2|log|sfpu|has golden: _log, LUT-based"
  "8|2|trigonometry|sfpu|has golden: _cos/_sin"
  "9|2|activations|sfpu|has golden: multiple"

  # Wave 3: Remaining simple SFPU — compile-only
  "10|3|sign|sfpu|"
  "11|3|hardtanh|sfpu|"
  "12|3|clamp|sfpu|"
  "13|3|dropout|sfpu|"
  "14|3|is_fp16_zero|sfpu|"
  "15|3|isinf_isnan|sfpu|"

  # Wave 4: Remaining medium SFPU — compile-only
  "16|4|cdf|sfpu|uses exp"
  "17|4|tanh_derivative|sfpu|uses tanh"
  "18|4|rsqrt_compat|sfpu|"
  "19|4|rounding_ops|sfpu|"
  "20|4|polyval|sfpu|polynomial eval"
  "21|4|load_config|sfpu|config loading"
  "22|4|cast_fp32_to_fp16a|sfpu|format conversion"
  "23|4|converter|sfpu|format conversion"
  "24|4|typecast|sfpu|multi-type cast"

  # Wave 5: Complex SFPU with test potential
  "25|5|comp|sfpu|has cross-arch test"
  "26|5|topk|sfpu|has cross-arch test"
  "27|5|quant|sfpu|has cross-arch test"
  "28|5|binary|sfpu|test via eltwise_binary after math wrapper"

  # Wave 6: Remaining complex SFPU — compile-only
  "29|6|binary_bitwise|sfpu|bitwise ops"
  "30|6|add_int|sfpu|integer arithmetic"
  "31|6|sub_int|sfpu|integer arithmetic"
  "32|6|mul_int|sfpu|integer arithmetic"
  "33|6|shift|sfpu|bit shifting"
  "34|6|where|sfpu|conditional select"
  "35|6|cumsum|sfpu|cumulative sum"
  "36|6|ema|sfpu|exponential moving avg"

  # Wave 7: Specialized SFPU — compile-only
  "37|7|welfords|sfpu|online variance"
  "38|7|reduce|sfpu|SFPU reduction"
  "39|7|reduce_custom|sfpu|custom reduction"
  "40|7|max_pool_indices|sfpu|pooling indices"
  "41|7|add_top_row|sfpu|row manipulation"
  "42|7|reshuffle_rows|sfpu|row reordering"

  # Wave 8: LLK Submodule — core (math wrappers, pack, unpack)
  "43|8|math_eltwise_unary_sfpu_params|math|depends on sfpu_common (exists)"
  "44|8|math_eltwise_binary_sfpu|math|depends on SFPU kernels"
  "45|8|math_eltwise_binary_sfpu_params|math|depends on #44"
  "46|8|math_eltwise_ternary_sfpu|math|depends on SFPU kernels"
  "47|8|math_eltwise_ternary_sfpu_params|math|depends on #46"
  "48|8|math_welfords_sfpu|math|depends on sfpu_welfords"
  "49|8|math_welfords_sfpu_params|math|depends on #48"
  "50|8|math_transpose_dest|math|dest register transpose"
  "51|8|pack_rows|pack|row-based packing"
  "52|8|unpack_untilize|unpack|untilize on unpack"

  # Wave 9: LLK Submodule — experimental (low priority)
  "53|9|math_eltwise_binary_custom|math|experimental"
  "54|9|math_eltwise_unary_datacopy_custom|math|experimental"
  "55|9|math_matmul_custom_no_mop|math|experimental"
  "56|9|math_mul_reduce_scalar|math|experimental"
  "57|9|math_reduce_custom|math|experimental"
  "58|9|math_reduce_runtime_custom|math|experimental"
  "59|9|pack_custom|pack|experimental"
  "60|9|unpack_A_custom|unpack|experimental"
  "61|9|unpack_AB_matmul_custom|unpack|experimental"
  "62|9|unpack_AB_reduce_custom|unpack|experimental"
  "63|9|unpack_AB_reduce_custom_runtime|unpack|experimental"
  "64|9|unpack_AB_sub_bcast_col_custom|unpack|experimental"
  "65|9|unpack_mul_reduce_scalar|unpack|experimental"
)

# --- Parse args ---
WAVE=""
KERNEL=""
FROM=1
DRY_RUN=false
PARALLEL=false
MAX_JOBS=0  # 0 = unlimited
MODEL="${CODEGEN_MODEL}"

while [[ $# -gt 0 ]]; do
  case $1 in
    --wave)   WAVE="$2"; shift 2 ;;
    --kernel) KERNEL="$2"; shift 2 ;;
    --from)   FROM="$2"; shift 2 ;;
    --model)  MODEL="$2"; shift 2 ;;
    --dry-run) DRY_RUN=true; shift ;;
    --parallel) PARALLEL=true; shift ;;
    -j)       MAX_JOBS="$2"; PARALLEL=true; shift 2 ;;
    --help|-h)
      echo "Usage: $0 [--wave N] [--kernel NAME] [--from N] [--model MODEL] [--parallel] [-j N] [--dry-run]"
      echo ""
      echo "  --wave N      Run all kernels in wave N (1-9)"
      echo "  --kernel NAME Run a single kernel by name"
      echo "  --from N      Resume from kernel number N"
      echo "  --model MODEL Claude model to use (default: opus)"
      echo "  --parallel    Run kernels in parallel (within a wave)"
      echo "  -j N          Max concurrent jobs (default: unlimited)"
      echo "  --dry-run     Show prompts without running"
      echo ""
      echo "Waves (ordered for maximum test feedback):"
      echo "  1  Testable simple SFPU (4)      — have golden generators"
      echo "  2  Testable medium SFPU (5)      — have golden generators"
      echo "  3  Remaining simple SFPU (6)     — compile-only"
      echo "  4  Remaining medium SFPU (9)     — compile-only"
      echo "  5  Complex SFPU w/ tests (4)     — have test potential"
      echo "  6  Remaining complex SFPU (8)    — compile-only"
      echo "  7  Specialized SFPU (6)          — compile-only"
      echo "  8  LLK Submodule core (10)       — math wrappers, pack, unpack"
      echo "  9  LLK Submodule experimental (13) — low priority"
      echo ""
      echo "Parallel notes:"
      echo "  - Safe to run all kernels within Waves 1-7 in parallel"
      echo "  - Wave 8-9 depends on Waves 1-7 completing first"
      echo "  - Within Wave 8, #45 depends on #44, #47 on #46, #49 on #48"
      echo "  - After parallel runs, update ckernel_sfpu.h with new #includes"
      echo ""
      echo "With no args, lists all kernels."
      exit 0 ;;
    *) echo "Unknown arg: $1"; exit 1 ;;
  esac
done

# Update model env var for orchestrator
export CODEGEN_MODEL="$MODEL"

# --- List mode ---
if [[ -z "$WAVE" && -z "$KERNEL" ]]; then
  echo "=== Quasar LLK Generation Plan: 65 kernels (52 core + 13 experimental) ==="
  echo "=== Ordered by testability (testable kernels first) ==="
  echo "=== Model: $MODEL ==="
  echo ""
  current_wave=""
  for entry in "${KERNELS[@]}"; do
    IFS='|' read -r num wave name type notes <<< "$entry"
    if [[ "$wave" != "$current_wave" ]]; then
      current_wave="$wave"
      case $wave in
        1) echo "--- Wave 1: Testable Simple SFPU (4) — have golden generators ---" ;;
        2) echo "--- Wave 2: Testable Medium SFPU (5) — have golden generators ---" ;;
        3) echo "--- Wave 3: Remaining Simple SFPU (6) — compile-only ---" ;;
        4) echo "--- Wave 4: Remaining Medium SFPU (9) — compile-only ---" ;;
        5) echo "--- Wave 5: Complex SFPU w/ Tests (4) — have test potential ---" ;;
        6) echo "--- Wave 6: Remaining Complex SFPU (8) — compile-only ---" ;;
        7) echo "--- Wave 7: Specialized SFPU (6) — compile-only ---" ;;
        8) echo "--- Wave 8: LLK Submodule Core (10) — math wrappers, pack, unpack ---" ;;
        9) echo "--- Wave 9: LLK Submodule Experimental (13) — low priority ---" ;;
      esac
    fi
    printf "  %2s. %-40s [%s] %s\n" "$num" "$name" "$type" "$notes"
  done
  echo ""
  echo "Run with: $0 --wave 1                    # all wave 1 sequentially"
  echo "          $0 --wave 1 --parallel          # all wave 1 in parallel"
  echo "          $0 --wave 1 -j 4                # wave 1, max 4 concurrent"
  echo "          $0 --kernel abs                 # single kernel"
  echo "          $0 --from 5                     # resume from #5"
  echo "          $0 --model sonnet --wave 1      # use sonnet model"
  echo "          $0 --wave 1 --dry-run           # preview"
  exit 0
fi

# --- Build run list ---
run_list=()
for entry in "${KERNELS[@]}"; do
  IFS='|' read -r num wave name type notes <<< "$entry"

  # Filter by wave
  if [[ -n "$WAVE" && "$wave" != "$WAVE" ]]; then continue; fi

  # Filter by kernel name
  if [[ -n "$KERNEL" && "$name" != "$KERNEL" ]]; then continue; fi

  # Filter by --from
  if [[ "$num" -lt "$FROM" ]]; then continue; fi

  run_list+=("$entry")
done

if [[ ${#run_list[@]} -eq 0 ]]; then
  echo "No matching kernels found."
  exit 1
fi

mode="sequentially"
if $PARALLEL; then mode="in parallel"; fi
echo "=== Will generate ${#run_list[@]} kernel(s) ${mode} ==="
echo "=== Model: $MODEL ==="
if $PARALLEL && [[ $MAX_JOBS -gt 0 ]]; then
  echo "=== Max concurrent jobs: ${MAX_JOBS} ==="
fi
echo ""

# --- Run a single kernel ---
run_one_kernel() {
  local num="$1" name="$2" total="$3"
  local prompt="Generate ${name} for Quasar"
  local logfile="${LOG_DIR}/${name}.log"

  echo "[$num/$total] START: $prompt (model: $MODEL)"

  cd "$CODEGEN_DIR"
  claude -p "$prompt" --dangerously-skip-permissions --effort max --verbose --model "$MODEL" > "$logfile" 2>&1
  local exit_code=$?

  if [[ $exit_code -ne 0 ]]; then
    echo "[$num/$total] FAILED: $name (exit code $exit_code) — see $logfile"
    return 1
  else
    echo "[$num/$total] DONE: $name"
    return 0
  fi
}

# --- Sequential run ---
run_sequential() {
  for entry in "${run_list[@]}"; do
    IFS='|' read -r num wave name type notes <<< "$entry"

    prompt="Generate ${name} for Quasar"

    echo "[$num/${#KERNELS[@]}] $prompt (model: $MODEL)"

    if $DRY_RUN; then
      echo "  (dry run — skipping)"
      echo ""
      continue
    fi

    cd "$CODEGEN_DIR"
    claude -p "$prompt" --dangerously-skip-permissions --effort max --verbose --model "$MODEL" 2>&1 | tee "${LOG_DIR}/${name}.log"

    exit_code=${PIPESTATUS[0]}
    if [[ $exit_code -ne 0 ]]; then
      echo "  FAILED (exit code $exit_code) — stopping. Resume with: $0 --from $((num + 1))"
      exit 1
    fi

    echo "  Done."
    echo ""
  done
}

# --- Parallel run ---
run_parallel() {
  local pids=()
  local names=()
  local active=0

  for entry in "${run_list[@]}"; do
    IFS='|' read -r num wave name type notes <<< "$entry"

    if $DRY_RUN; then
      echo "[$num/${#KERNELS[@]}] Generate ${name} for Quasar (dry run — skipping)"
      continue
    fi

    # Throttle if max jobs reached
    if [[ $MAX_JOBS -gt 0 ]]; then
      while [[ $active -ge $MAX_JOBS ]]; do
        # Wait for any child to finish
        wait -n 2>/dev/null || true
        # Recount active
        active=0
        for pid in "${pids[@]}"; do
          if kill -0 "$pid" 2>/dev/null; then
            ((active++))
          fi
        done
      done
    fi

    run_one_kernel "$num" "$name" "${#KERNELS[@]}" &
    pids+=($!)
    names+=("$name")
    ((active++))
  done

  if $DRY_RUN; then return; fi

  # Wait for all and collect results
  echo ""
  echo "=== Waiting for ${#pids[@]} parallel job(s) to complete ==="
  echo ""

  local failed=0
  for i in "${!pids[@]}"; do
    if ! wait "${pids[$i]}"; then
      ((failed++))
    fi
  done

  echo ""
  if [[ $failed -gt 0 ]]; then
    echo "=== ${failed} kernel(s) FAILED — check logs in ${LOG_DIR}/ ==="
    return 1
  fi
}

# --- Main ---
mkdir -p "$LOG_DIR"

if $PARALLEL; then
  run_parallel
else
  run_sequential
fi

echo "=== All ${#run_list[@]} kernel(s) complete ==="
echo "=== Logs: ${LOG_DIR}/ ==="

# --- Post-run reminder ---
if ! $DRY_RUN; then
  echo ""
  echo "=== POST-RUN: Update ckernel_sfpu.h ==="
  echo "Add #include lines for newly generated kernels to:"
  echo "  tt_llk_quasar/common/inc/ckernel_sfpu.h"
  echo ""
  echo "Generated SFPU kernels:"
  for entry in "${run_list[@]}"; do
    IFS='|' read -r num wave name type notes <<< "$entry"
    if [[ "$type" == "sfpu" ]]; then
      echo "  #include \"sfpu/ckernel_sfpu_${name}.h\""
    fi
  done
fi
