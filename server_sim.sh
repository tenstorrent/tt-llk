#!/usr/bin/env bash

cd /proj_sw/user_dev/lpremovic/qsr
(
    sleep 1
    latest_log=$(ls -t vcs* | head -n1)
    code "$latest_log"
) &
tt-exalens --server --port 5556 -s tt-umd-simulators/build/vcs-quasar
