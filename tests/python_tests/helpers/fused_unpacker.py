# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
#
# SPDX-License-Identifier: Apache-2.0

from typing import TYPE_CHECKING, List, Tuple

import torch

if TYPE_CHECKING:
    from .fused_operation import FusedOperation
    from .fuser_config import GlobalConfig
    from .fused_math import ComputeNode

from .chip_architecture import ChipArchitecture
from .fused_fpu import ReduceFpu
from .golden_generators import BroadcastGolden, TransposeGolden, get_golden_generator
from .llk_params import BroadcastType, PerfRunType, Transpose
from .tilize_untilize import tilize_block, untilize_block


class Unpacker:
    def init(
        self,
        operation: "FusedOperation",
        config: "GlobalConfig",
        compute_unit: "ComputeNode",
    ) -> str:
        return ""

    def unpack(
        self,
        operation: "FusedOperation",
        config: "GlobalConfig",
        compute_unit: "ComputeNode",
        tile_idx_expr: str,
    ) -> str:
        return ""

    def uninit(
        self,
        operation: "FusedOperation",
        config: "GlobalConfig",
        compute_unit: "ComputeNode",
    ) -> str:
        return ""

    def perf_set_valid(
        self, operation: "FusedOperation", config: "GlobalConfig"
    ) -> str:
        tile_cnt = operation.output.tile_count
        num_faces = operation.num_faces
        return f"_perf_unpack_loop_set_valid<true, true>({tile_cnt} * {num_faces});\n"

    def unpack_with_perf(
        self, operation: "FusedOperation", config: "GlobalConfig"
    ) -> str:
        if config.perf_run_type == PerfRunType.PACK_ISOLATE:
            return ""
        elif config.perf_run_type == PerfRunType.MATH_ISOLATE:
            return self.perf_set_valid(operation, config)
        else:
            return self.unpack(operation, config)

    def exec_perf(self, operation: "FusedOperation", config: "GlobalConfig") -> str:
        code = "{\n"
        code += '    ZONE_SCOPED("INIT")\n'
        code += self.hw_configure(operation, config)
        code += self.init(operation, config)
        code += "    PROFILER_SYNC();\n"
        code += "}\n"

        code += "{\n"
        code += '    ZONE_SCOPED("TILE_LOOP")\n'

        code += self.packer_sync(operation)
        code += f"    for(int loop = 0; loop < {config.loop_factor}; loop++)\n"
        code += "    {\n"
        code += self.unpack_with_perf(operation, config)
        code += "    }\n"
        code += "    PROFILER_SYNC();\n"
        code += "}\n"

        code += "{\n"
        code += '    ZONE_SCOPED("INIT")\n'
        code += self.uninit(operation, config)
        code += "    PROFILER_SYNC();\n"
        code += "}\n"

        return code

    def exec(self, operation: "FusedOperation", config: "GlobalConfig") -> str:
        stage = operation.stage_id
        buffer_A_address = operation.src_a.l1_address
        buffer_B_address = operation.src_b.l1_address
        buffer_A_tile_size = operation.buffer_A_tile_size
        buffer_B_tile_size = operation.buffer_B_tile_size
        unpack_a_src = operation.unpack_a_in
        unpack_a_dst = operation.unpack_a_out
        unpack_b_src = operation.unpack_a_in
        unpack_b_dst = operation.unpack_a_out

        code = (
            f"    // Operation {stage}: {self.__class__.__name__}\n"
            f"    UNUSED const Operand buffer_A{stage}({hex(buffer_A_address)}, {buffer_A_tile_size});\n"
            f"    UNUSED const Operand buffer_B{stage}({hex(buffer_B_address)}, {buffer_B_tile_size});\n"
            f"    UNUSED const std::uint32_t unpack_a_src_format{stage} = ckernel::to_underlying(DataFormat::{unpack_a_src.name});\n"
            f"    UNUSED const std::uint32_t unpack_a_dst_format{stage} = ckernel::to_underlying(DataFormat::{unpack_a_dst.name});\n"
            f"    UNUSED const std::uint32_t unpack_b_src_format{stage} = ckernel::to_underlying(DataFormat::{unpack_b_src.name});\n"
            f"    UNUSED const std::uint32_t unpack_b_dst_format{stage} = ckernel::to_underlying(DataFormat::{unpack_b_dst.name});\n"
        )

        if config.profiler_enabled:
            code += self.exec_perf(operation, config)
        else:
            code += self.hw_configure(operation, config)
            code += self.init(operation, config)
            code += self.packer_sync(operation)
            code += self.unpack_with_perf(operation, config)
            code += self.uninit(operation, config)

        return code

    def get_headers(self) -> List[str]:
        return ["perf.h"]

    def golden(
        self,
        tensor_a: torch.Tensor,
        tensor_b: torch.Tensor,
        operation: "FusedOperation",
        config: "GlobalConfig",
        compute_unit: "ComputeNode" = None,
    ) -> Tuple[torch.Tensor, torch.Tensor]:
        return tensor_a, tensor_b


class MatmulUnpacker(Unpacker):
    def get_headers(self) -> List[str]:
        return [
            "llk_unpack_AB_matmul.h",
            "llk_unpack_common.h",
        ]

    def perf_set_valid(
        self, operation: "FusedOperation", config: "GlobalConfig"
    ) -> str:
        rt_dim = operation.rt_dim
        kt_dim = operation.kt_dim
        ct_dim = operation.ct_dim
        return f"_perf_unpack_matmul_mock(1, {rt_dim}, {kt_dim}, {ct_dim});\n"

    def golden(
        self,
        tensor_a: torch.Tensor,
        tensor_b: torch.Tensor,
        operation: "FusedOperation",
        config: "GlobalConfig",
        compute_unit: "ComputeNode",
    ) -> Tuple[torch.Tensor, torch.Tensor]:
        t_matrix = get_golden_generator(TransposeGolden)

        if compute_unit.unpack_transpose_faces == Transpose.Yes:
            tensor_b = t_matrix.transpose_faces_multi_tile(
                tensor_b,
                operation.src_b.data_format,
                operation.src_b.tile_count,
                tilize=True,
                input_dimensions=operation.src_b.dimensions,
            )

        if compute_unit.unpack_transpose_within_face == Transpose.Yes:
            tensor_b = t_matrix.transpose_within_faces_multi_tile(
                tensor_b,
                operation.src_b.data_format,
                operation.src_b.tile_count,
                untilize=True,
                input_dimensions=operation.src_b.dimensions,
            )

        return tensor_a, tensor_b

    def init(
        self,
        operation: "FusedOperation",
        config: "GlobalConfig",
        compute_unit: "ComputeNode",
    ) -> str:
        face_r_dim = operation.face_r_dim
        ct_dim = operation.ct_dim
        rt_dim = operation.rt_dim
        kt_dim = operation.kt_dim
        batch_size = operation.batch_size

        transpose_faces = (
            "true" if compute_unit.unpack_transpose_faces.value else "false"
        )
        transpose_within_face = (
            "true" if compute_unit.unpack_transpose_within_face.value else "false"
        )

        if transpose_within_face != transpose_faces:
            raise ValueError(
                "MatmulUnpacker does not support different values for transpose_faces and transpose_within_face"
            )

        if batch_size == 1:
            return f"    _llk_unpack_AB_matmul_init_<>({transpose_faces}, 1, 1, {kt_dim}, {face_r_dim}, {face_r_dim});\n"
        else:
            return f"    _llk_unpack_AB_matmul_init_<>({transpose_faces}, {ct_dim}, {rt_dim}, {kt_dim}, {face_r_dim}, {face_r_dim});\n"

    def unpack(
        self,
        operation: "FusedOperation",
        config: "GlobalConfig",
        compute_unit: "ComputeNode",
        tile_idx_expr: str = None,
    ) -> str:
        stage = operation.stage_id
        ct_dim = operation.ct_dim
        rt_dim = operation.rt_dim
        kt_dim = operation.kt_dim
        batch_size = operation.batch_size
        unpack_tile_size_a = operation.tile_size_unpack_a
        unpack_tile_size_b = operation.tile_size_unpack_b

        if batch_size == 1:
            # Single tile per batch - tile_idx maps to (mt, nt) pair
            code = (
                f"    {{\n"
                f"        uint32_t mt = ({tile_idx_expr}) / {ct_dim};\n"
                f"        uint32_t nt = ({tile_idx_expr}) % {ct_dim};\n"
                f"        for (uint32_t kt = 0; kt < {kt_dim}; ++kt) {{\n"
                f"            _llk_unpack_AB_matmul_<>(\n"
                f"                L1_ADDRESS(buffer_A{stage}[0]), L1_ADDRESS(buffer_B{stage}[0]),\n"
                f"                mt * {kt_dim} + kt, kt * {ct_dim} + nt,\n"
                f"                {unpack_tile_size_a}, {unpack_tile_size_b}, false, false, 1, 1, {kt_dim}\n"
                f"            );\n"
                f"        }}\n"
                f"    }}\n"
            )
        else:
            # All tiles in one batch - no tile_idx needed, just kt loop
            code = (
                f"    for (std::uint32_t kt = 0; kt < {kt_dim}; ++kt) {{\n"
                f"        _llk_unpack_AB_matmul_<>(\n"
                f"            L1_ADDRESS(buffer_A{stage}[0]), L1_ADDRESS(buffer_B{stage}[0]),\n"
                f"            kt, kt * {ct_dim},\n"
                f"            {unpack_tile_size_a}, {unpack_tile_size_b}, false, false, {ct_dim}, {rt_dim}, {kt_dim}\n"
                f"        );\n"
                f"    }}\n"
            )

        return code


class UnpackerAB(Unpacker):
    def get_headers(self) -> List[str]:
        return [
            "llk_unpack_AB.h",
            "llk_unpack_common.h",
            "llk_unpack_tilize.h",
        ]

    def golden(
        self,
        tensor_a: torch.Tensor,
        tensor_b: torch.Tensor,
        operation: "FusedOperation",
        config: "GlobalConfig",
        compute_unit: "ComputeNode",
    ) -> Tuple[torch.Tensor, torch.Tensor]:
        t_matrix = get_golden_generator(TransposeGolden)
        if compute_unit.broadcast_type != BroadcastType.None_:
            tilized_b = tilize_block(
                tensor_b, operation.src_b.dimensions, operation.src_b.data_format
            )
            broadcast_golden = get_golden_generator(BroadcastGolden)
            broadcast_result = broadcast_golden(
                compute_unit.broadcast_type,
                tilized_b,
                operation.src_b.data_format,
                operation.num_faces,
                operation.src_b.tile_count,
                operation.face_r_dim,
            )
            tensor_b = untilize_block(
                broadcast_result,
                operation.src_b.data_format,
                operation.src_b.dimensions,
            )

        if compute_unit.unpack_transpose_faces == Transpose.Yes:
            tensor_a = t_matrix.transpose_faces_multi_tile(
                tensor_a,
                operation.src_a.data_format,
                operation.src_a.tile_count,
                tilize=True,
                untilize=True,
                input_dimensions=operation.src_a.dimensions,
            )

        if compute_unit.unpack_transpose_within_face == Transpose.Yes:
            tensor_a = t_matrix.transpose_within_faces_multi_tile(
                tensor_a,
                operation.src_a.data_format,
                operation.src_a.tile_count,
                tilize=True,
                untilize=True,
                input_dimensions=operation.src_a.dimensions,
            )

        return tensor_a.flatten(), tensor_b.flatten()

    def init(
        self,
        operation: "FusedOperation",
        config: "GlobalConfig",
        compute_unit: "ComputeNode",
    ) -> str:
        face_r_dim = operation.face_r_dim
        num_faces = operation.num_faces
        broadcast_type = f"BroadcastType::{compute_unit.broadcast_type.value}"

        if (
            compute_unit.broadcast_type == BroadcastType.Scalar
            and compute_unit.unpack_transpose_faces.value
        ):
            raise ValueError("SrcA transpose is not supported with scalar broadcast")

        transpose_faces = (
            "true" if compute_unit.unpack_transpose_faces.value else "false"
        )
        transpose_within_face = (
            "true" if compute_unit.unpack_transpose_within_face.value else "false"
        )

        if isinstance(compute_unit.fpu, ReduceFpu):
            if compute_unit.broadcast_type != BroadcastType.None_:
                raise ValueError("ReduceFpu does not support broadcasted inputs.")

            reduce_dim = compute_unit.fpu.reduce_dim()
            return (
                f"_llk_unpack_AB_reduce_init_<{reduce_dim}, {broadcast_type}>(\n"
                f"{face_r_dim}, {num_faces}, false, {transpose_faces}, {transpose_within_face});\n"
            )
        else:
            if transpose_within_face != transpose_faces:
                raise ValueError(
                    "UnpackerAB does not support different values for transpose_faces and transpose_within_face"
                )

            return f"_llk_unpack_AB_init_<{broadcast_type}>({face_r_dim}, {num_faces}, false, {transpose_faces});\n"

    def unpack(
        self,
        operation: "FusedOperation",
        config: "GlobalConfig",
        compute_unit: "ComputeNode",
        tile_idx_expr: str,
    ) -> str:
        stage = operation.stage_id
        return f"_llk_unpack_AB_<>(L1_ADDRESS(buffer_A{stage}[{tile_idx_expr}]), L1_ADDRESS(buffer_B{stage}[{tile_idx_expr}]));\n"


class UnpackerA(Unpacker):
    def get_headers(self) -> List[str]:
        return [
            "llk_unpack_A.h",
            "llk_unpack_common.h",
            "llk_unpack_tilize.h",
        ]

    def golden(
        self,
        tensor_a: torch.Tensor,
        tensor_b: torch.Tensor,
        operation: "FusedOperation",
        config: "GlobalConfig",
        compute_unit: "ComputeNode",
    ) -> Tuple[torch.Tensor, torch.Tensor]:
        t_matrix = get_golden_generator(TransposeGolden)

        if compute_unit.broadcast_type != BroadcastType.None_:
            tensor_b = tensor_a
            tensor_a = None
            tensor_b = tilize_block(
                tensor_b, operation.src_a.dimensions, operation.src_a.data_format
            )
            broadcast_golden = get_golden_generator(BroadcastGolden)
            tensor_b = broadcast_golden(
                compute_unit.broadcast_type,
                tensor_b,
                operation.src_a.data_format,
                operation.num_faces,
                operation.src_a.tile_count,
                operation.face_r_dim,
            )
            tensor_b = untilize_block(
                tensor_b,
                operation.src_a.data_format,
                operation.src_a.dimensions,
            )
        else:
            if compute_unit.unpack_transpose_faces == Transpose.Yes:
                tensor_a = t_matrix.transpose_faces_multi_tile(
                    tensor_a,
                    operation.src_a.data_format,
                    operation.src_a.tile_count,
                    tilize=True,
                    untilize=True,
                    input_dimensions=operation.src_a.dimensions,
                )

            if compute_unit.unpack_transpose_within_face == Transpose.Yes:
                tensor_a = t_matrix.transpose_within_faces_multi_tile(
                    tensor_a,
                    operation.src_a.data_format,
                    operation.src_a.tile_count,
                    tilize=True,
                    untilize=True,
                    input_dimensions=operation.src_a.dimensions,
                )
            tensor_b = None

        return tensor_a, tensor_b

    def init(
        self,
        operation: "FusedOperation",
        config: "GlobalConfig",
        compute_unit: "ComputeNode",
    ) -> str:
        stage = operation.stage_id
        unpack_to_dest = "true" if operation.unpack_to_dest else "false"
        broadcast_type = f"BroadcastType::{compute_unit.broadcast_type.value}"
        eltwise_reuse_type = "NONE"
        face_r_dim = operation.face_r_dim
        num_faces = operation.num_faces
        transpose_faces = (
            "true" if compute_unit.unpack_transpose_faces.value else "false"
        )
        transpose_within_face = (
            "true" if compute_unit.unpack_transpose_within_face.value else "false"
        )

        return (
            f"    _llk_unpack_A_init_<{broadcast_type}, false, EltwiseBinaryReuseDestType::{eltwise_reuse_type}, {unpack_to_dest}>(\n"
            f"        {transpose_faces}, {transpose_within_face}, {face_r_dim}, {num_faces}, unpack_a_src_format{stage}, unpack_a_dst_format{stage}\n"
            f"    );\n"
        )

    def unpack(
        self,
        operation: "FusedOperation",
        config: "GlobalConfig",
        compute_unit: "ComputeNode",
        tile_idx_expr: str,
    ) -> str:
        stage = operation.stage_id
        unpack_to_dest = "true" if operation.unpack_to_dest else "false"
        broadcast_type = f"BroadcastType::{compute_unit.broadcast_type.value}"

        return (
            f"_llk_unpack_A_<{broadcast_type}, false, EltwiseBinaryReuseDestType::NONE, {unpack_to_dest}>(\n"
            f"    L1_ADDRESS(buffer_A{stage}[{tile_idx_expr}]), unpack_a_src_format{stage}, unpack_a_dst_format{stage}\n"
            f");\n"
        )


class UnpackerTilizeA(Unpacker):
    def get_headers(self) -> List[str]:
        return [
            "llk_unpack_common.h",
            "llk_unpack_tilize.h",
        ]

    def perf_set_valid(
        self, operation: "FusedOperation", config: "GlobalConfig"
    ) -> str:
        tile_cnt = operation.output.tile_count
        return f"    _perf_unpack_loop_set_valid<true, true>({tile_cnt});\n"

    def golden(
        self,
        tensor_a: torch.Tensor,
        tensor_b: torch.Tensor,
        operation: "FusedOperation",
        config: "GlobalConfig",
        compute_unit: "ComputeNode",
    ) -> Tuple[torch.Tensor, torch.Tensor]:
        tilized_a = tilize_block(
            tensor_a,
            operation.src_a.dimensions,
            operation.src_a.data_format,
            operation.num_faces,
        )

        return tilized_a, None

    def init(
        self,
        operation: "FusedOperation",
        config: "GlobalConfig",
        compute_unit: "ComputeNode",
    ) -> str:
        stage = operation.stage_id
        face_r_dim = operation.face_r_dim
        block_ct_dim = operation.dest_tiles_w
        transpose_faces = compute_unit.unpack_transpose_faces.value
        transpose_within_face = compute_unit.unpack_transpose_within_face.value
        if compute_unit.broadcast_type != BroadcastType.None_:
            raise ValueError("UnpackerTilizeA does not support broadcast")

        if transpose_faces or transpose_within_face:
            raise ValueError("UnpackerTilizeA does not support transpose")

        return f"    _llk_unpack_tilize_init_(unpack_a_src_format{stage}, unpack_a_dst_format{stage}, {block_ct_dim}, {face_r_dim}, false);\n"

    def unpack(
        self,
        operation: "FusedOperation",
        config: "GlobalConfig",
        compute_unit: "ComputeNode",
        tile_idx_expr: str,
    ) -> str:
        stage = operation.stage_id
        face_r_dim = operation.face_r_dim
        num_faces = operation.num_faces
        block_ct_dim = operation.dest_tiles_w

        # For tilize, we need to compute row/col from tile_idx
        # Blackhole
        if config.architecture == ChipArchitecture.BLACKHOLE:
            return (
                f"{{\n"
                f"    uint32_t row = ({tile_idx_expr}) / {block_ct_dim};\n"
                f"    uint32_t col = ({tile_idx_expr}) % {block_ct_dim};\n"
                f"    _llk_unpack_tilize_(L1_ADDRESS(buffer_A{stage}[row * {block_ct_dim}]), col, unpack_a_src_format{stage}, unpack_a_dst_format{stage});\n"
                f"}}\n"
            )

        # Wormhole
        elif config.architecture == ChipArchitecture.WORMHOLE:
            return (
                f"{{\n"
                f"    uint32_t row = ({tile_idx_expr}) / {block_ct_dim};\n"
                f"    uint32_t col = ({tile_idx_expr}) % {block_ct_dim};\n"
                f"    _llk_unpack_tilize_(L1_ADDRESS(buffer_A{stage}[row * {block_ct_dim}]), col, unpack_a_src_format{stage}, unpack_a_dst_format{stage}, {block_ct_dim}, {face_r_dim}, {num_faces}, false);\n"
                f"}}\n"
            )

        else:
            raise ValueError("Architecture is not supported")

    def uninit(
        self,
        operation: "FusedOperation",
        config: "GlobalConfig",
        compute_unit: "ComputeNode",
    ) -> str:
        stage = operation.stage_id
        face_r_dim = operation.face_r_dim
        num_faces = operation.num_faces

        # Blackhole
        if config.architecture == ChipArchitecture.BLACKHOLE:
            code = f"    _llk_unpack_tilize_uninit_(unpack_a_dst_format{stage}, {num_faces}, {face_r_dim});\n"

        # Wormhole
        elif config.architecture == ChipArchitecture.WORMHOLE:
            code = f"    _llk_unpack_tilize_uninit_(unpack_a_dst_format{stage}, {face_r_dim});\n\n"

        else:
            raise ValueError("Architecture is not supported")

        return code
