/*
 * SPDX-FileCopyrightText: (c) 2025 Tenstorrent AI ULC
 *
 * SPDX-License-Identifier: Apache-2.0
 */

MEMORY
{
    BRISC_LOCAL_DATA_MEM : ORIGIN = 0xFFB00000, LENGTH = (4 * 1024) - 1024
    TRISC0_LOCAL_DATA_MEM : ORIGIN = 0xFFB00000, LENGTH = (2 * 1024) - 256
    TRISC1_LOCAL_DATA_MEM : ORIGIN = 0xFFB00000, LENGTH = (2 * 1024) - 256
    TRISC2_LOCAL_DATA_MEM : ORIGIN = 0xFFB00000, LENGTH = (2 * 1024) - 768
    NCRISC_LOCAL_DATA_MEM : ORIGIN = 0xFFB00000, LENGTH = (4 * 1024) - 1024
    BRISC_STACK_MEM : ORIGIN = (0xFFB00000 + (4 * 1024) - 1024), LENGTH = 1024
    TRISC0_STACK_MEM : ORIGIN = (0xFFB00000 + (2 * 1024) - 256), LENGTH = 256
    TRISC1_STACK_MEM : ORIGIN = (0xFFB00000 + (2 * 1024) - 256), LENGTH = 256
    TRISC2_STACK_MEM : ORIGIN = (0xFFB00000 + (2 * 1024) - 768), LENGTH = 768
    NCRISC_STACK_MEM : ORIGIN = (0xFFB00000 + (4 * 1024) - 1024), LENGTH = 1024
    BRISC_CODE : ORIGIN = (2048 + 512), LENGTH = (10 * 1024)
    TRISC0_CODE : ORIGIN = ((2048 + 512) + (10 * 1024)), LENGTH = (16 * 1024)
    TRISC1_CODE : ORIGIN = (((2048 + 512) + (10 * 1024)) + (16 * 1024)), LENGTH = (16 * 1024)
    TRISC2_CODE : ORIGIN = ((((2048 + 512) + (10 * 1024)) + (16 * 1024)) + (16 * 1024)), LENGTH = (16 * 1024)
    NCRISC_CODE : ORIGIN = 0xFFC00000, LENGTH = (16 * 1024)
    BRISC_LOADER_INIT_MEM : ORIGIN = 61952, LENGTH = 4 * 1024
    NCRISC_LOADER_INIT_MEM : ORIGIN = (61952 + (4 * 1024)), LENGTH = (4 * 1024)
    TRISC0_LOADER_INIT_MEM : ORIGIN = (61952 + (4 * 1024) + (4 * 1024)), LENGTH = (2 * 1024)
    TRISC1_LOADER_INIT_MEM : ORIGIN = (61952 + (4 * 1024) + (4 * 1024) + (2 * 1024)), LENGTH = (2 * 1024)
    TRISC2_LOADER_INIT_MEM : ORIGIN = (61952 + (4 * 1024) + (4 * 1024) + (2 * 1024) + (2 * 1024)), LENGTH = (2 * 1024)
    BRISC_LOADER_CODE_MEM : ORIGIN = 76288, LENGTH = 0 * 1024
    NCRISC_LOADER_CODE_MEM : ORIGIN = (76288 + (0 * 1024)), LENGTH = (16 * 1024)
    TRISC0_LOADER_CODE_MEM : ORIGIN = (76288 + (0 * 1024) + (16 * 1024)), LENGTH = (0 * 1024)
    TRISC1_LOADER_CODE_MEM : ORIGIN = (76288 + (0 * 1024) + (16 * 1024) + (0 * 1024)), LENGTH = (0 * 1024)
    TRISC2_LOADER_CODE_MEM : ORIGIN = (76288 + (0 * 1024) + (16 * 1024) + (0 * 1024) + (0 * 1024)), LENGTH = (0 * 1024)
}
PROVIDE(__instrn_buffer = 0xFFE40000);
