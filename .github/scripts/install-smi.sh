#!/usr/bin/env bash
# SPDX-FileCopyrightText: (c) 2025 Tenstorrent AI ULC
#
# SPDX-License-Identifier: Apache-2.0

set -e

SM_VERSION="3.0.27"
SM_WHEEL="tt_smi-${SM_VERSION}-py3-none-any.whl"

wget -O ${SM_WHEEL} \
    https://github.com/tenstorrent/tt-smi/releases/download/v${SM_VERSION}/${SM_WHEEL}
pip install --no-cache-dir ${SM_WHEEL}
rm ${SM_WHEEL}
