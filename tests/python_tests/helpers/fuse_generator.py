# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
#
# SPDX-License-Identifier: Apache-2.0

import os
from dataclasses import dataclass
from pathlib import Path
from typing import Dict, List

from .fuse_operation import PipelineOperation


@dataclass
class PipelineConfig:
    operations: List[PipelineOperation]

    def to_dict(self):
        return {
            "pipeline_name": self.pipeline_name,
            "operations": self.operations,
            "num_stages": len(self.operations),
        }


@dataclass
class UnpackKernelGenerator:
    def __init__(self, config: PipelineConfig):
        self.config = config

    def generate(self) -> str:
        num_stages = len(self.config.operations)

        code = ""

        for i in range(num_stages):
            code += f"constexpr uint32_t buffer_stage{i}_out = 0x{0x18000 + i*0x1000:05x};\n"

        code += f"""
#ifdef LLK_TRISC_UNPACK

#include "llk_unpack_A.h"
#include "llk_unpack_AB.h"
#include "llk_unpack_AB_matmul.h"
#include "llk_unpack_common.h"

void run_kernel()
{{"""
        for op in self.config.operations:
            code += op.unpack()

        code += f"""
}}

#endif
"""
        return code


@dataclass
class MathKernelGenerator:
    def __init__(self, config: PipelineConfig):
        self.config = config

    def generate(self) -> str:
        code = """
#ifdef LLK_TRISC_MATH
#include "llk_math_common.h"
#include "llk_math_matmul.h"

void run_kernel()
{"""
        for op in self.config.operations:
            code += op.do_math()

        code += f"""
}}
#endif
"""
        return code


@dataclass
class PackKernelGenerator:
    def __init__(self, config: PipelineConfig):
        self.config = config

    def generate(self) -> str:
        code = """
#ifdef LLK_TRISC_PACK
# include "llk_pack.h"
# include "llk_pack_common.h"
void run_kernel()
{"""
        for op in self.config.operations:
            code += op.pack()

        code += f"""
}}
#endif
"""
        return code


class KernelCompiler:

    def __init__(self, config: PipelineConfig):
        self.config = config
        self.unpack_gen = UnpackKernelGenerator(config)
        self.math_gen = MathKernelGenerator(config)
        self.pack_gen = PackKernelGenerator(config)

    def generate_all(self) -> Dict[str, str]:
        return {
            "unpack": self.unpack_gen.generate(),
            "math": self.math_gen.generate(),
            "pack": self.pack_gen.generate(),
        }

    def write_kernel(self):
        combined = f"""
#include <cstdint>

#include "ckernel.h"
#include "llk_defs.h"
#include "params.h"

using namespace ckernel;

uint32_t unp_cfg_context          = 0;
uint32_t pack_sync_tile_dst_ptr   = 0;
uint32_t math_sync_tile_dst_index = 0;
"""
        kernels = self.generate_all()
        combined += kernels["unpack"]
        combined += kernels["math"]
        combined += kernels["pack"]

        llk_home = Path(os.environ.get("LLK_HOME"))
        with open(llk_home / "tests/sources/fuse_test.cpp", "w") as f:
            f.write(combined)
