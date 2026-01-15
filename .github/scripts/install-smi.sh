#!/usr/bin/env bash
# SPDX-FileCopyrightText: (c) 2025 Tenstorrent AI ULC
#
# SPDX-License-Identifier: Apache-2.0

set -e

# tt-smi configuration
TT_SMI_VERSION="3.0.38"
TT_SMI_WHEEL="tt_smi-${TT_SMI_VERSION}-py3-none-any.whl"
TT_SMI_URL="https://github.com/tenstorrent/tt-smi/releases/download/v${TT_SMI_VERSION}/${TT_SMI_WHEEL}"

wget -q -O "${TT_SMI_WHEEL}" "${TT_SMI_URL}" || exit 1
uv pip install -q --system --no-cache-dir "${TT_SMI_WHEEL}"
rm "${TT_SMI_WHEEL}"
