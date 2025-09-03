#!/usr/bin/env bash

cd /proj_sw/user_dev/lpremovic/qsr
source venv/bin/activate
tt-exalens --server --port 5556 -s tt-umd-simulators/build/vcs-quasar
