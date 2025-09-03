#!/usr/bin/env bash

cd /proj_sw/user_dev/lpremovic/qsr
source venv/bin/activate
export NNG_SOCKET_ADDR=tcp://yyzeon19:50999
export NNG_SOCKET_LOCAL_PORT=5555
tt-exalens --server --port 5556 -s tt-umd-simulators/build/emu-quasar-1x3
