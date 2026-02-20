# SPDX-FileCopyrightText: © 2026 Tenstorrent AI ULC
#
# SPDX-License-Identifier: Apache-2.0

from .fused_math import ComputeNode
from .fused_operation import FusedOperation
from .fuser_config import GlobalConfig


class FusedLoop:
    def unpack_loop(
        self,
        operation: FusedOperation,
        config: GlobalConfig,
        compute_unit: ComputeNode,
        block_x: int,
        block_y: int,
        block_size_x: int,
        block_size_y: int,
    ) -> str:
        return ""

    def math_loop(
        self,
        operation: FusedOperation,
        config: GlobalConfig,
        compute_unit: ComputeNode,
        block_x: int,
        block_y: int,
        block_size_x: int,
        block_size_y: int,
    ) -> str:
        return ""

    def pack_loop(
        self,
        operation: FusedOperation,
        config: GlobalConfig,
        compute_unit: ComputeNode,
        block_x: int,
        block_y: int,
        block_size_x: int,
        block_size_y: int,
    ) -> str:
        code = ""
        for tile_x in range(block_size_x):
            for tile_y in range(block_size_y):
                dest_tile_id = f"({tile_x} * {block_size_y} + {tile_y})"
                l1_tile_id = 0  # problem za marka iz buducnosti
                code += operation.packer.pack(
                    operation, config, compute_unit, dest_tile_id, l1_tile_id
                )
        return code


class LoopBlock(FusedLoop):
    def unpack_loop(
        self,
        operation: FusedOperation,
        config: GlobalConfig,
        compute_unit: ComputeNode,
        block_x: int,
        block_y: int,
        block_size_x: int,
        block_size_y: int,
    ) -> str:
        tile_id = 0  # problem za marka iz buducnosti
        return compute_unit.unpacker.unpack(operation, config, compute_unit, tile_id)

    def math_loop(
        self,
        operation: FusedOperation,
        config: GlobalConfig,
        compute_unit: ComputeNode,
        block_x: int,
        block_y: int,
        block_size_x: int,
        block_size_y: int,
    ) -> str:
        tile_id = 0
        return compute_unit.fpu.calculate(operation, config, compute_unit, tile_id)


class LoopTileByTile(FusedLoop):
    def unpack_loop(
        self,
        operation: FusedOperation,
        config: GlobalConfig,
        compute_unit: ComputeNode,
        block_x: int,
        block_y: int,
        block_size_x: int,
        block_size_y: int,
    ) -> str:
        code = ""
        for tile_x in range(block_size_x):
            for tile_y in range(block_size_y):
                tile_id = 0  # problem za marka iz buducnosti
                code += compute_unit.unpacker.unpack(
                    operation, config, compute_unit, tile_id
                )
        return code

    def math_loop(
        self,
        operation: FusedOperation,
        config: GlobalConfig,
        compute_unit: ComputeNode,
        block_x: int,
        block_y: int,
        block_size_x: int,
        block_size_y: int,
    ) -> str:
        code = ""
        for tile_x in range(block_size_x):
            for tile_y in range(block_size_y):
                tile_id = tile_x * operation.block_tiles_y + tile_y
                code += compute_unit.fpu.calculate(
                    operation, config, compute_unit, tile_id
                )

        return code
