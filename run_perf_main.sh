#!/bin/bash
set -e

# Run 1: actual main branch (no counter code at all) -> /proj_sw/user_dev/mvlahovic/main_perf_data/
DEST="/proj_sw/user_dev/mvlahovic/main_perf_data"
BRANCH="mvlahovic/perf_counters_integration"
BASELINE="origin/main"

echo "============================================"
echo "  RUN 1: actual main branch (no counter code)"
echo "  Commit: $BASELINE"
echo "  Output: $DEST"
echo "============================================"

# Nuke build cache to ensure clean compile with --speed-of-light
echo "Cleaning build cache..."
rm -rf /tmp/tt-llk-build/

# Fetch latest main and checkout
echo "Fetching and checking out $BASELINE..."
git fetch origin main --quiet
git checkout "$BASELINE" --quiet

TESTS=(
    "perf_eltwise_binary_fpu"
    "perf_unpack_tilize"
    "perf_pack_untilize"
    "perf_unpack_untilize"
    "perf_matmul"
)

for test in "${TESTS[@]}"; do
    echo ""
    echo ">>> $test (compile-producer)..."
    if [ "$test" = "perf_matmul" ]; then
        python -m pytest "tests/python_tests/${test}.py" --speed-of-light -k "Float16_b and (LoFi or HiFi2)" --compile-producer -n 10 -q
    else
        python -m pytest "tests/python_tests/${test}.py" --speed-of-light --compile-producer -n 10 -q
    fi

    echo ">>> $test (compile-consumer)..."
    if [ "$test" = "perf_matmul" ]; then
        python -m pytest "tests/python_tests/${test}.py" --speed-of-light -k "Float16_b and (LoFi or HiFi2)" --compile-consumer -q
    else
        python -m pytest "tests/python_tests/${test}.py" --speed-of-light --compile-consumer -q
    fi

    echo ">>> Copying $test results to $DEST..."
    cp "perf_data/${test}/${test}.csv" "$DEST/${test}.csv"
    cp "perf_data/${test}/${test}.post.csv" "$DEST/${test}.post.csv"
    echo "  Done: $test"
done

# Return to branch
echo ""
echo "Returning to branch $BRANCH..."
git checkout "$BRANCH" --quiet

echo ""
echo "============================================"
echo "  RUN 1 COMPLETE"
echo "  Results in: $DEST"
echo "============================================"
