#!/bin/bash

# <run-config>
#   <scenario name="first-run" description="First time running or environment changed">
#     <var name="ENV_SETUP" value="1"/>
#     <var name="COMPILED" value="1"/>
#     <var name="RUN_TEST" value="1"/>
#   </scenario>
#   <scenario name="code-changed" description="Code changed, need recompile but env is ready">
#     <var name="ENV_SETUP" value="0"/>
#     <var name="COMPILED" value="1"/>
#     <var name="RUN_TEST" value="1"/>
#   </scenario>
#   <scenario name="rerun-only" description="Nothing changed, just rerun tests">
#     <var name="ENV_SETUP" value="0"/>
#     <var name="COMPILED" value="0"/>
#     <var name="RUN_TEST" value="1"/>
#   </scenario>
#   <option name="QUIET" default="0" description="Set to 1 to disable terminal output (logs still saved to /tmp/llk_test/)"/>
# </run-config>

# Override these based on the scenario above (uses env vars if provided)
ENV_SETUP=${ENV_SETUP:-1}
COMPILED=${COMPILED:-1}
RUN_TEST=${RUN_TEST:-1}
FILE_NAME=${FILE_NAME:-""}
QUIET=${QUIET:-1}
COVERAGE_FLAG=""

if [ "${COVERAGE:-0}" -eq 1 ]; then
    COVERAGE_FLAG="--coverage"
fi

if [ $ENV_SETUP -eq 1 ]; then
    echo "Setting up environment..."
    ./setup_testing_env.sh
fi

cd ./python_tests

if [ $COMPILED -eq 1 ]; then
    echo "Compiling..."
    mkdir -p /tmp/llk_test
    if [ $QUIET -eq 1 ]; then
        pytest $COVERAGE_FLAG --compile-producer -n 10 -x $FILE_NAME > /tmp/llk_test/compile.log 2>&1
        SUCCESS=$?
    else
        pytest $COVERAGE_FLAG --compile-producer -n 10 -x $FILE_NAME 2>&1 | tee /tmp/llk_test/compile.log
        SUCCESS=${PIPESTATUS[0]}
    fi

    if [ $SUCCESS -ne 0 ]; then
        echo "Compilation failed"
        exit 1
    fi

    rm -f /tmp/llk_test/compile.log
fi

if [ $RUN_TEST -eq 1 ]; then
    echo "Running tests..."
    mkdir -p /tmp/llk_test
    if [ $QUIET -eq 1 ]; then
        pytest $COVERAGE_FLAG --compile-consumer -x $FILE_NAME > /tmp/llk_test/run.and 2>&1 && tail -10 /tmp/llk_test/run.and
    else
        script -q -c "pytest $COVERAGE_FLAG --compile-consumer -x $FILE_NAME" /tmp/llk_test/run.and
    fi
fi
