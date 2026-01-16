#!/usr/bin/env bash
# SPDX-FileCopyrightText: (c) 2025 Tenstorrent AI ULC
#
# SPDX-License-Identifier: Apache-2.0

set -e

wget -qO- https://astral.sh/uv/install.sh | sh
uv pip install -q --system --index-strategy unsafe-best-match --no-cache-dir -r /tmp/requirements_tests.txt
