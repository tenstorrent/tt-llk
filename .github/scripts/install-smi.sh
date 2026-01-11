#!/usr/bin/env bash
# SPDX-FileCopyrightText: (c) 2025 Tenstorrent AI ULC
#
# SPDX-License-Identifier: Apache-2.0

set -e

# Source centralized version configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=.github/scripts/versions.sh
source "${SCRIPT_DIR}/versions.sh"

wget -q -O "${TT_SMI_WHEEL}" "${TT_SMI_URL}" || exit 1
python3 -m pip install -q --break-system-packages --no-cache-dir "${TT_SMI_WHEEL}"
rm "${TT_SMI_WHEEL}"

# Ensure tt-smi is reachable on PATH even if pip chose a user scheme
USER_BIN_DIR="$(python3 - <<'PY'
import site, os
print(os.path.join(site.USER_BASE, "bin"))
PY
)"
if [ -d "${USER_BIN_DIR}" ] && [ ! -x "/usr/local/bin/tt-smi" ] && [ -x "${USER_BIN_DIR}/tt-smi" ]; then
  ln -s "${USER_BIN_DIR}/tt-smi" /usr/local/bin/tt-smi
fi
