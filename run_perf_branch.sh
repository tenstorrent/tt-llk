#!/bin/bash
set -e

# Run 2: our branch (no counters) -> /proj_sw/user_dev/mvlahovic/perf_data/
DEST="/proj_sw/user_dev/mvlahovic/perf_data"

echo "============================================"
echo "  RUN 2: branch (no counters)"
echo "  Output: $DEST"
echo "============================================"

# Nuke build cache to ensure clean compile with --speed-of-light
echo "Cleaning build cache..."
rm -rf /tmp/tt-llk-build/

TESTS=(
    "perf_eltwise_binary_fpu"
    "perf_unpack_tilize"
    "perf_pack_untilize"
    "perf_unpack_untilize"
    "perf_matmul"
)

for test in "${TESTS[@]}"; do
    echo ""
    echo "Running $test (compile-producer)..."
    if [ "$test" = "perf_matmul" ]; then
        python -m pytest "tests/python_tests/${test}.py" --speed-of-light -k "Float16_b and (LoFi or HiFi2)" --compile-producer -n 10 -q
    else
        python -m pytest "tests/python_tests/${test}.py" --speed-of-light --compile-producer -n 10 -q
    fi

    echo "Running $test (compile-consumer)..."
    if [ "$test" = "perf_matmul" ]; then
        python -m pytest "tests/python_tests/${test}.py" --speed-of-light -k "Float16_b and (LoFi or HiFi2)" --compile-consumer -q
    else
        python -m pytest "tests/python_tests/${test}.py" --speed-of-light --compile-consumer -q
    fi

    echo "Copying $test results to $DEST..."
    cp "perf_data/${test}/${test}.csv" "$DEST/${test}.csv"
    cp "perf_data/${test}/${test}.post.csv" "$DEST/${test}.post.csv"
    echo "  Done: $test"
done

echo ""
echo "============================================"
echo "  RUN 2 COMPLETE: branch (no counters)"
echo "  Results in: $DEST"
echo "============================================"
