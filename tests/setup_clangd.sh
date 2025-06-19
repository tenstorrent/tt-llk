#!/usr/bin/env bash
# SPDX-FileCopyrightText: (c) 2025 Tenstorrent AI ULC
#
# SPDX-License-Identifier: Apache-2.0

set -e  # Exit immediately if a command exits with a non-zero status
set -o pipefail  # Fail if any command in a pipeline fails

ARCH=$1
if [[ "$ARCH" == "wormhole" ]]; then
    ARCH_LLK_ROOT="tt_llk_wormhole_b0"
    ARCH_DEFINE="ARCH_WORMHOLE"
    CHIP_ARCH="wormhole"
elif [[ "$ARCH" == "blackhole" ]]; then
    ARCH_LLK_ROOT="tt_llk_blackhole"
    ARCH_DEFINE="ARCH_BLACKHOLE"
    CHIP_ARCH="blackhole"
else
    echo "Usage: $0 [wormhole|blackhole]"
    exit 1
fi

echo -D$ARCH_DEFINE                         > compile_flags.txt
echo                                        >> compile_flags.txt
echo -DLLK_TRISC_UNPACK                     >> compile_flags.txt
echo -DLLK_TRISC_MATH                       >> compile_flags.txt
echo -DLLK_TRISC_PACK                       >> compile_flags.txt
echo                                        >> compile_flags.txt
echo -I../$ARCH_LLK_ROOT/llk_lib            >> compile_flags.txt
echo -I../$ARCH_LLK_ROOT/common/inc         >> compile_flags.txt
echo -I../$ARCH_LLK_ROOT/common/inc/sfpu    >> compile_flags.txt
echo -Ihw_specific/inc                      >> compile_flags.txt
echo -Ifirmware/riscv/common                >> compile_flags.txt
echo -Ifirmware/riscv/$CHIP_ARCH            >> compile_flags.txt
echo -Isfpi/include                         >> compile_flags.txt
echo -Ihelpers/include                      >> compile_flags.txt

(pkill clangd && clangd >/dev/null 2>&1 &) || true  # restart clang if it's running
