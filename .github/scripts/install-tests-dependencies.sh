#!/usr/bin/env bash
# SPDX-FileCopyrightText: (c) 2025 Tenstorrent AI ULC
#
# SPDX-License-Identifier: Apache-2.0

set -e

pip install --upgrade --break-system-packages pip
pip install --no-cache-dir --break-system-packages -r /tmp/requirements_tests.txt
