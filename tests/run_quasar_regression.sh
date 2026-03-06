#!/usr/bin/env bash
# SPDX-FileCopyrightText: (c) 2025 Tenstorrent AI ULC
#
# SPDX-License-Identifier: Apache-2.0
#
# Automated regression runner for Quasar simulator tests.
# Starts tt-exalens server, runs pytest, then tears everything down.

set -euo pipefail

# ──────────────────────────────────────────────────────────────
# Configuration – override via environment or CLI flags
# ──────────────────────────────────────────────────────────────
SIMULATOR_BUILD_PATH="${SIMULATOR_BUILD_PATH:-/localdev/fvranic/tt-umd-simulators/build/emu-quasar-1x3}"
EXALENS_PORT="${EXALENS_PORT:-5556}"
export NNG_SOCKET_ADDR="${NNG_SOCKET_ADDR:-tcp://tensix-l-05:53018}"
export NNG_SOCKET_LOCAL_PORT="${NNG_SOCKET_LOCAL_PORT:-5555}"

EXALENS_READY_TIMEOUT=600
EXALENS_READY_PATTERN='\[4B MODE\]'

PYTEST_ARGS=()
TEST_PATTERN="test_*.py"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
QUASAR_TEST_DIR="${SCRIPT_DIR}/python_tests/quasar"

EXALENS_PID=""
EXALENS_LOG=""

# ──────────────────────────────────────────────────────────────
# Usage
# ──────────────────────────────────────────────────────────────
usage() {
    cat <<EOF
Usage: $(basename "$0") [OPTIONS] [-- PYTEST_EXTRA_ARGS...]

Starts tt-exalens against a Quasar simulator build, then runs the
Quasar pytest suite. Everything is cleaned up on exit.

Options:
  -s, --simulator-path PATH   Path to simulator build directory
                              (default: \$SIMULATOR_BUILD_PATH)
  -p, --port PORT             tt-exalens server port (default: \$EXALENS_PORT or 5556)
  -t, --test-pattern GLOB     Test file glob pattern (default: test_*.py)
  -h, --help                  Show this help message

Environment variables:
  SIMULATOR_BUILD_PATH        Same as --simulator-path
  EXALENS_PORT                Same as --port
  NNG_SOCKET_ADDR             NNG socket address (default: tcp://tensix-l-05:53018)
  NNG_SOCKET_LOCAL_PORT       NNG local port (default: 5555)

Examples:
  ./$(basename "$0")
  ./$(basename "$0") -s /path/to/build -p 5557
  ./$(basename "$0") -t "test_reduce_*.py" -- -x
  ./$(basename "$0") -- -v --tb=short
EOF
    exit 0
}

# ──────────────────────────────────────────────────────────────
# Parse CLI arguments
# ──────────────────────────────────────────────────────────────
while [[ $# -gt 0 ]]; do
    case "$1" in
        -s|--simulator-path) SIMULATOR_BUILD_PATH="$2"; shift 2 ;;
        -p|--port)           EXALENS_PORT="$2";         shift 2 ;;
        -t|--test-pattern)   TEST_PATTERN="$2";         shift 2 ;;
        -h|--help)           usage ;;
        --)                  shift; PYTEST_ARGS+=("$@"); break ;;
        *)                   PYTEST_ARGS+=("$1");        shift ;;
    esac
done

# ──────────────────────────────────────────────────────────────
# Validation
# ──────────────────────────────────────────────────────────────
if [[ ! -d "$SIMULATOR_BUILD_PATH" ]]; then
    echo "ERROR: Simulator build path does not exist: $SIMULATOR_BUILD_PATH" >&2
    exit 1
fi

if ! command -v tt-exalens &>/dev/null; then
    echo "ERROR: tt-exalens not found in PATH" >&2
    exit 1
fi

if ! command -v pytest &>/dev/null; then
    echo "ERROR: pytest not found in PATH" >&2
    exit 1
fi

if [[ ! -d "$QUASAR_TEST_DIR" ]]; then
    echo "ERROR: Quasar test directory not found: $QUASAR_TEST_DIR" >&2
    exit 1
fi

# ──────────────────────────────────────────────────────────────
# Cleanup – always kill the server on exit
# ──────────────────────────────────────────────────────────────
cleanup() {
    local exit_code=$?
    if [[ -n "$EXALENS_PID" ]] && kill -0 "$EXALENS_PID" 2>/dev/null; then
        echo ""
        echo ">>> Stopping tt-exalens (PID $EXALENS_PID)..."
        kill "$EXALENS_PID" 2>/dev/null || true
        wait "$EXALENS_PID" 2>/dev/null || true
    fi
    if [[ -n "$EXALENS_LOG" && -f "$EXALENS_LOG" ]]; then
        if [[ $exit_code -ne 0 ]]; then
            echo ">>> tt-exalens log (last 30 lines):"
            tail -n 30 "$EXALENS_LOG"
        fi
        rm -f "$EXALENS_LOG"
    fi
    exit "$exit_code"
}
trap cleanup EXIT INT TERM

# ──────────────────────────────────────────────────────────────
# Export environment needed by tt-exalens / simulator
# ──────────────────────────────────────────────────────────────
export CHIP_ARCH=quasar

echo "============================================================"
echo " Quasar Regression Runner"
echo "============================================================"
echo " Simulator build : $SIMULATOR_BUILD_PATH"
echo " NNG_SOCKET_ADDR : $NNG_SOCKET_ADDR"
echo " NNG_SOCKET_LOCAL_PORT : $NNG_SOCKET_LOCAL_PORT"
echo " tt-exalens port : $EXALENS_PORT"
echo " Test pattern     : $TEST_PATTERN"
echo " Extra pytest args: ${PYTEST_ARGS[*]:-<none>}"
echo "============================================================"
echo ""

# ──────────────────────────────────────────────────────────────
# Start tt-exalens server in the background
# ──────────────────────────────────────────────────────────────
EXALENS_LOG=$(mktemp /tmp/tt-exalens-XXXXXX.log)

echo ">>> Starting tt-exalens server..."
tt-exalens \
    --port="$EXALENS_PORT" \
    --server \
    -s "$SIMULATOR_BUILD_PATH" \
    >"$EXALENS_LOG" 2>&1 &
EXALENS_PID=$!

echo ">>> Waiting for tt-exalens to become ready (timeout: ${EXALENS_READY_TIMEOUT}s)..."
elapsed=0
ready=false
while [[ $elapsed -lt $EXALENS_READY_TIMEOUT ]]; do
    if ! kill -0 "$EXALENS_PID" 2>/dev/null; then
        echo "ERROR: tt-exalens exited prematurely. Log output:" >&2
        cat "$EXALENS_LOG" >&2
        exit 1
    fi

    if grep -q "$EXALENS_READY_PATTERN" "$EXALENS_LOG" 2>/dev/null; then
        ready=true
        break
    fi

    sleep 2
    elapsed=$((elapsed + 2))
    if (( elapsed % 10 == 0 )); then
        echo "    ... still waiting (${elapsed}s elapsed)"
    fi
done

if [[ "$ready" != true ]]; then
    echo "ERROR: tt-exalens did not become ready within ${EXALENS_READY_TIMEOUT}s" >&2
    echo "Log output:" >&2
    cat "$EXALENS_LOG" >&2
    exit 1
fi
echo ">>> tt-exalens ready (PID $EXALENS_PID, took ~${elapsed}s)"
echo ""

# ──────────────────────────────────────────────────────────────
# Run pytest
# ──────────────────────────────────────────────────────────────
echo ">>> Running Quasar tests..."
echo ""

pytest_exit=0
(
    cd "$QUASAR_TEST_DIR"
    pytest \
        --run-simulator \
        --port="$EXALENS_PORT" \
        -x \
        $TEST_PATTERN \
        "${PYTEST_ARGS[@]+"${PYTEST_ARGS[@]}"}"
) || pytest_exit=$?

echo ""
if [[ $pytest_exit -eq 0 ]]; then
    echo "============================================================"
    echo " ALL TESTS PASSED"
    echo "============================================================"
else
    echo "============================================================"
    echo " TESTS FAILED (exit code: $pytest_exit)"
    echo "============================================================"
fi

exit "$pytest_exit"
