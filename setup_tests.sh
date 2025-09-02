#!/usr/bin/env bash

source ../venv/bin/activate
cd tests
export CHIP_ARCH=quasar
./setup_testing_env.sh
cd python_tests
