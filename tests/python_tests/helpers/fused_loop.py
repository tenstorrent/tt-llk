# SPDX-FileCopyrightText: © 2026 Tenstorrent AI ULC
#
# SPDX-License-Identifier: Apache-2.0

from typing import TYPE_CHECKING

if TYPE_CHECKING:
    from .fused_math import ComputeNode
    from .fused_operation import FusedOperation
    from .fuser_config import GlobalConfig
    from .block_data import BlockData

from .llk_params import PerfRunType


class FusedLoop:
    def unpack_loop(
        self,
        operation: "FusedOperation",
        config: "GlobalConfig",
        compute_unit: "ComputeNode",
        block: "BlockData",
    ) -> str:
        return ""

    def math_loop(
        self,
        operation: "FusedOperation",
        config: "GlobalConfig",
        compute_unit: "ComputeNode",
        block: "BlockData",
    ) -> str:
        return ""

    def pack_loop(
        self,
        operation: "FusedOperation",
        config: "GlobalConfig",
        block: "BlockData",
    ) -> str:
        code = ""
        code += f"for (std::uint32_t tile_x = 0; tile_x < {block.block_tiles_x}; tile_x++) {{\n"
        code += f"for (std::uint32_t tile_y = 0; tile_y < {block.block_tiles_y}; tile_y++) {{\n"
        code += f"std::uint32_t l1_tile_id = {block.tile_count_x} * ({block.block_y} + tile_y) + ({block.block_x} + tile_x);\n"
        code += (
            f"std::uint32_t dest_tile_id = tile_x * {block.block_tiles_y} + tile_y;\n"
        )
        code += operation.math.packer().pack(
            operation, config, None, block, "dest_tile_id", "l1_tile_id"
        )
        code += "}\n"
        code += "}\n"
        return code


class LoopBlock(FusedLoop):
    def unpack_loop(
        self,
        operation: "FusedOperation",
        config: "GlobalConfig",
        compute_unit: "ComputeNode",
        block: "BlockData",
    ) -> str:
        code = ""
        if config.perf_run_type == PerfRunType.PACK_ISOLATE:
            return code
        if config.perf_run_type == PerfRunType.MATH_ISOLATE:
            return compute_unit.unpacker().perf_set_valid(
                operation, config, compute_unit, block
            )
        code += f"std::uint32_t tile_id = {block.tile_count_x} * {block.block_y} + {block.block_x};\n"
        code += compute_unit.unpacker().unpack(
            operation, config, compute_unit, block, "tile_id"
        )
        return code

    def math_loop(
        self,
        operation: "FusedOperation",
        config: "GlobalConfig",
        compute_unit: "ComputeNode",
        block: "BlockData",
    ) -> str:
        if config.perf_run_type == PerfRunType.PACK_ISOLATE:
            return ""
        if config.perf_run_type in (
            PerfRunType.UNPACK_ISOLATE,
            PerfRunType.L1_CONGESTION,
        ):
            return compute_unit.unpacker().perf_clear_valid(
                operation, config, compute_unit, block
            )
        tile_id = 0
        return compute_unit.fpu.calculate(
            operation, config, compute_unit, block, tile_id
        )


class LoopTileByTile(FusedLoop):
    def unpack_loop(
        self,
        operation: "FusedOperation",
        config: "GlobalConfig",
        compute_unit: "ComputeNode",
        block: "BlockData",
    ) -> str:
        code = ""
        if config.perf_run_type == PerfRunType.PACK_ISOLATE:
            return code
        code += f"for (std::uint32_t tile_x = 0; tile_x < {block.block_tiles_x}; tile_x++) {{\n"
        code += f"for (std::uint32_t tile_y = 0; tile_y < {block.block_tiles_y}; tile_y++) {{\n"
        if config.perf_run_type == PerfRunType.MATH_ISOLATE:
            code += compute_unit.unpacker().perf_set_valid(
                operation, config, compute_unit, block
            )
        else:
            code += f"std::uint32_t tile_id = {block.tile_count_x} * ({block.block_y} + tile_y) + ({block.block_x} + tile_x);\n"
            from .fused_unpacker import ReduceBlockMaxUnpacker

            if compute_unit.unpacker == ReduceBlockMaxUnpacker:
                code += "std::uint32_t gate_tile_id = tile_x;\n"
                code += compute_unit.unpacker().unpack(
                    operation,
                    config,
                    compute_unit,
                    block,
                    "gate_tile_id",
                    "tile_id",
                )
            else:
                code += compute_unit.unpacker().unpack(
                    operation, config, compute_unit, block, "tile_id"
                )
        code += "}\n"
        code += "}\n"
        return code

    def math_loop(
        self,
        operation: "FusedOperation",
        config: "GlobalConfig",
        compute_unit: "ComputeNode",
        block: "BlockData",
    ) -> str:
        code = ""
        if config.perf_run_type == PerfRunType.PACK_ISOLATE:
            return code
        code += f"for (std::uint32_t tile_x = 0; tile_x < {block.block_tiles_x}; tile_x++) {{\n"
        code += f"for (std::uint32_t tile_y = 0; tile_y < {block.block_tiles_y}; tile_y++) {{\n"
        if config.perf_run_type in (
            PerfRunType.UNPACK_ISOLATE,
            PerfRunType.L1_CONGESTION,
        ):
            code += compute_unit.unpacker().perf_clear_valid(
                operation, config, compute_unit, block
            )
        else:
            from .fused_fpu import ReduceBlockMaxFpu

            if isinstance(compute_unit.fpu, ReduceBlockMaxFpu):
                code += "std::uint32_t gate_tile_id = tile_x;\n"
                code += f"std::uint32_t dest_tile_id = tile_x * {block.block_tiles_y} + tile_y;\n"
                code += compute_unit.fpu.calculate(
                    operation,
                    config,
                    compute_unit,
                    block,
                    "gate_tile_id",
                    "dest_tile_id",
                )
            else:
                code += f"std::uint32_t tile_id = tile_x * {block.block_tiles_y} + tile_y;\n"
                code += compute_unit.fpu.calculate(
                    operation, config, compute_unit, block, "tile_id"
                )
        code += "}\n"
        code += "}\n"
        return code
