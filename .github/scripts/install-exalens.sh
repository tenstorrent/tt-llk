#!/usr/bin/env bash
# SPDX-FileCopyrightText: (c) 2025 Tenstorrent AI ULC
#
# SPDX-License-Identifier: Apache-2.0

set -e

# Source centralized version configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=.github/scripts/versions.sh
source "${SCRIPT_DIR}/versions.sh"

# Install tt-umd first from test.pypi.org
python3 -m pip install -q --break-system-packages --no-cache-dir \
  --index-url https://test.pypi.org/simple/ --extra-index-url https://pypi.org/simple/ \
  tt-umd==${TT_UMD_VERSION}

wget -q -O "${EXALENS_WHEEL}" "${EXALENS_URL}" || exit 1
python3 -m pip install -q --break-system-packages --no-cache-dir "${EXALENS_WHEEL}"
rm "${EXALENS_WHEEL}"

# Ensure tt-exalens is reachable on PATH even if pip chose a user scheme
USER_BIN_DIR="$(python3 - <<'PY'
import site, os
print(os.path.join(site.USER_BASE, "bin"))
PY
)"
if [ -d "${USER_BIN_DIR}" ] && [ ! -x "/usr/local/bin/tt-exalens" ] && [ -x "${USER_BIN_DIR}/tt-exalens" ]; then
  ln -s "${USER_BIN_DIR}/tt-exalens" /usr/local/bin/tt-exalens
fi
