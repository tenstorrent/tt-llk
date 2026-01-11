#!/usr/bin/env bash
# SPDX-FileCopyrightText: (c) 2025 Tenstorrent AI ULC
#
# SPDX-License-Identifier: Apache-2.0

set -e

python3 -m pip install -q --break-system-packages --upgrade pip
python3 -m pip install -q --break-system-packages --no-cache-dir -r /tmp/requirements_tests.txt
