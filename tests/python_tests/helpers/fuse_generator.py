# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
#
# SPDX-License-Identifier: Apache-2.0

import os
from dataclasses import dataclass
from pathlib import Path
from typing import Dict, List

from .fuse_operation import PipelineOperation


@dataclass
class UnpackKernelGenerator:
    def __init__(self, operations: List[PipelineOperation]):
        self.operations = operations

    def generate(self) -> str:
        num_stages = len(self.operations)

        # Collect all unique headers from all operations
        all_headers = set()
        for op in self.operations:
            unpacker_instance = op.unpacker()
            all_headers.update(unpacker_instance.get_headers())

        # Generate include statements
        includes = "\n".join([f'#include "{header}"' for header in sorted(all_headers)])

        code = f"""
#ifdef LLK_TRISC_UNPACK

{includes}

void run_kernel()
{{"""
        for op in self.operations:
            code += op.unpack()

        code += f"""
}}

#endif
"""
        return code


@dataclass
class MathKernelGenerator:
    def __init__(self, operations: List[PipelineOperation]):
        self.operations = operations

    def generate(self) -> str:
        # Collect all unique headers from all operations
        all_headers = set()
        for op in self.operations:
            all_headers.update(op.math.get_headers())

        # Generate include statements
        includes = "\n".join([f'#include "{header}"' for header in sorted(all_headers)])

        code = f"""
#ifdef LLK_TRISC_MATH
{includes}

using namespace ckernel::sfpu;

void run_kernel()
{{"""
        for op in self.operations:
            code += op.do_math()

        code += f"""
}}
#endif
"""
        return code


@dataclass
class PackKernelGenerator:
    def __init__(self, operations: List[PipelineOperation]):
        self.operations = operations

    def generate(self) -> str:
        # Collect all unique headers from all operations
        all_headers = set()
        for op in self.operations:
            packer_instance = op.packer()
            all_headers.update(packer_instance.get_headers())

        # Generate include statements
        includes = "\n".join([f'#include "{header}"' for header in sorted(all_headers)])

        code = f"""
#ifdef LLK_TRISC_PACK
{includes}
void run_kernel()
{{"""
        for op in self.operations:
            code += op.pack()

        code += f"""
}}
#endif
"""
        return code


class KernelCompiler:

    def __init__(self, operations: List[PipelineOperation]):
        self.operations = operations
        num_stages = len(self.operations)

        for i, op in enumerate(self.operations):
            cfg = op.generated_config
            cfg["stage_id"] = i
            cfg["num_stages"] = num_stages

        self.unpack_gen = UnpackKernelGenerator(self.operations)
        self.math_gen = MathKernelGenerator(self.operations)
        self.pack_gen = PackKernelGenerator(self.operations)

    def generate_all(self) -> Dict[str, str]:
        return {
            "unpack": self.unpack_gen.generate(),
            "math": self.math_gen.generate(),
            "pack": self.pack_gen.generate(),
        }

    def write_kernel(self):
        combined = f"""
#define FUSE_TEST
#include <cstdint>

#include <array>
#include <type_traits>

#include <cstddef>
#include <utility>

#include "ckernel.h"
#include "llk_defs.h"

#include "ckernel_debug.h"
#include "ckernel_defs.h"
#include "ckernel_sfpu.h"
#include "tensix_types.h"
#include "operand.h"

#include "llk_sfpu_types.h"

using namespace ckernel;

uint32_t unp_cfg_context          = 0;
uint32_t pack_sync_tile_dst_ptr   = 0;
uint32_t math_sync_tile_dst_index = 0;

inline uint32_t L1_ADDRESS(uint32_t buffer_address)
{{
#ifdef ARCH_QUASAR
    return buffer_address / 16;
#else
    return (buffer_address / 16) - 1;
#endif
}}

constexpr bool UNPACKING_TO_DEST    = false;
constexpr bool APPROX_MODE          = false;
constexpr bool is_fp32_dest_acc_en  = false;

#include "data_format_inference.h"

"""
        kernels = self.generate_all()
        combined += kernels["unpack"]
        combined += kernels["math"]
        combined += kernels["pack"]

        llk_home = Path(os.environ.get("LLK_HOME"))
        with open(llk_home / "tests/sources/fuse_test.cpp", "w") as f:
            f.write(combined)
