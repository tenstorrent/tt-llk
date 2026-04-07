# SPDX-FileCopyrightText: © 2026 Tenstorrent AI ULC
#
# SPDX-License-Identifier: Apache-2.0

from typing import TYPE_CHECKING, List, Tuple

import torch

if TYPE_CHECKING:
    from .fused_operation import FusedOperation
    from .fuser_config import GlobalConfig
    from .fused_math import ComputeNode
    from .block_data import BlockData

from .fused_loop import FusedLoop


class Unpacker:
    loop: FusedLoop = FusedLoop()

    def init(
        self,
        operation: "FusedOperation",
        config: "GlobalConfig",
        compute_unit: "ComputeNode",
        block: "BlockData",
    ) -> str:
        return ""

    def unpack(
        self,
        operation: "FusedOperation",
        config: "GlobalConfig",
        compute_unit: "ComputeNode",
        block: "BlockData",
    ) -> str:
        return ""

    def uninit(
        self,
        operation: "FusedOperation",
        config: "GlobalConfig",
        compute_unit: "ComputeNode",
        block: "BlockData",
    ) -> str:
        return ""

    def perf_set_valid(
        self,
        operation: "FusedOperation",
        config: "GlobalConfig",
        compute_unit: "ComputeNode",
        block: "BlockData",
    ) -> str:
        num_faces = operation.num_faces
        return f"_perf_unpack_loop_set_valid<true, true>({num_faces});\n"

    def perf_clear_valid(
        self,
        operation: "FusedOperation",
        config: "GlobalConfig",
        compute_unit: "ComputeNode",
        block: "BlockData",
    ) -> str:
        num_faces = operation.num_faces
        return f"_perf_math_loop_clear_valid<true, true>({num_faces});\n"

    def get_headers(self) -> List[str]:
        return []

    def golden(
        self,
        tensor_a: torch.Tensor,
        tensor_b: torch.Tensor,
        operation: "FusedOperation",
        config: "GlobalConfig",
        compute_unit: "ComputeNode" = None,
    ) -> Tuple[torch.Tensor, torch.Tensor]:
        return tensor_a, tensor_b


# class UnpackerA(Unpacker):
#     loop: FusedLoop = LoopTileByTile()
#
#     def get_headers(self) -> List[str]:
#         return [
#             "llk_unpack_A.h",
#             "llk_unpack_common.h",
#             "llk_unpack_tilize.h",
#         ]
#
#     def golden(
#         self,
#         tensor_a: torch.Tensor,
#         tensor_b: torch.Tensor,
#         operation: "FusedOperation",
#         config: "GlobalConfig",
#         compute_unit: "ComputeNode",
#     ) -> Tuple[torch.Tensor, torch.Tensor]:
#         t_matrix = get_golden_generator(TransposeGolden)
#
#         if compute_unit.broadcast_type != BroadcastType.None_:
#             tensor_b = tensor_a
#             tensor_a = None
#             tensor_b = tilize_block(
#                 tensor_b, operation.src_a.dimensions, operation.src_a.data_format
#             )
#             broadcast_golden = get_golden_generator(BroadcastGolden)
#             tensor_b = broadcast_golden(
#                 compute_unit.broadcast_type,
#                 tensor_b,
#                 operation.src_a.data_format,
#                 operation.num_faces,
#                 operation.src_a.tile_count,
#                 operation.face_r_dim,
#             )
#             tensor_b = untilize_block(
#                 tensor_b,
#                 operation.src_a.data_format,
#                 operation.src_a.dimensions,
#             )
#         else:
#             if compute_unit.unpack_transpose_faces == Transpose.Yes:
#                 tensor_a = t_matrix.transpose_faces_multi_tile(
#                     tensor_a,
#                     operation.src_a.data_format,
#                     operation.src_a.tile_count,
#                     tilize=True,
#                     untilize=True,
#                     input_dimensions=operation.src_a.dimensions,
#                 )
#
#             if compute_unit.unpack_transpose_within_face == Transpose.Yes:
#                 tensor_a = t_matrix.transpose_within_faces_multi_tile(
#                     tensor_a,
#                     operation.src_a.data_format,
#                     operation.src_a.tile_count,
#                     tilize=True,
#                     untilize=True,
#                     input_dimensions=operation.src_a.dimensions,
#                 )
#             tensor_b = None
#
#         return tensor_a, tensor_b
#
#     def perf_set_valid(
#         self,
#         operation: "FusedOperation",
#         config: "GlobalConfig",
#         compute_unit: "ComputeNode",
#         block: "BlockData",
#     ) -> str:
#         if compute_unit.broadcast_type == BroadcastType.Scalar:
#             if config.architecture == ChipArchitecture.WORMHOLE:
#                 return "_perf_unpack_loop_set_valid<true, true>(1);\n"
#             else:
#                 return "_perf_unpack_loop_set_valid<false, true>(1);\n"
#         elif compute_unit.broadcast_type == BroadcastType.Column:
#             return (
#                 "_perf_unpack_loop_set_valid<false, true>(2);\n"
#                 "_perf_unpack_loop_set_valid<true, false>(1);\n"
#             )
#         elif compute_unit.broadcast_type == BroadcastType.Row:
#             return "_perf_unpack_loop_set_valid<false, true>(4);\n"
#         else:
#             num_faces = operation.num_faces
#             return f"_perf_unpack_loop_set_valid<true, true>({num_faces});\n"
#
#     def perf_clear_valid(
#         self,
#         operation: "FusedOperation",
#         config: "GlobalConfig",
#         compute_unit: "ComputeNode",
#         block: "BlockData",
#     ) -> str:
#         if compute_unit.broadcast_type == BroadcastType.Scalar:
#             if config.architecture == ChipArchitecture.WORMHOLE:
#                 return "_perf_math_loop_clear_valid<true, true>(1);\n"
#             else:
#                 return "_perf_math_loop_clear_valid<false, true>(1);\n"
#         elif compute_unit.broadcast_type == BroadcastType.Column:
#             return (
#                 "_perf_math_loop_clear_valid<false, true>(2);\n"
#                 "_perf_math_loop_clear_valid<true, false>(1);\n"
#             )
#         elif compute_unit.broadcast_type == BroadcastType.Row:
#             return "_perf_math_loop_clear_valid<false, true>(4);\n"
#         else:
#             num_faces = operation.num_faces
#             return f"_perf_math_loop_clear_valid<true, true>({num_faces});\n"
#
#     def init(
#         self,
#         operation: "FusedOperation",
#         config: "GlobalConfig",
#         compute_unit: "ComputeNode",
#         block: "BlockData",
#     ) -> str:
#         stage = operation.stage_id
#         unpack_to_dest = "true" if operation.unpack_to_dest else "false"
#         broadcast_type = compute_unit.broadcast_type.cpp_enum_value
#         reuse_dest = compute_unit.reuse_dest.cpp_enum_value
#         face_r_dim = operation.face_r_dim
#         num_faces = operation.num_faces
#         transpose_faces = compute_unit.unpack_transpose_faces.cpp_enum_value
#         transpose_within_face = compute_unit.unpack_transpose_within_face.cpp_enum_value
#
#         return (
#             f"    _llk_unpack_A_init_<{broadcast_type}, false, {reuse_dest}, {unpack_to_dest}>(\n"
#             f"        {transpose_faces}, {transpose_within_face}, {face_r_dim}, {num_faces}, unpack_a_src_format{stage}, unpack_a_dst_format{stage}\n"
#             f"    );\n"
#         )
#
#     def unpack(
#         self,
#         operation: "FusedOperation",
#         config: "GlobalConfig",
#         compute_unit: "ComputeNode",
#         block: "BlockData",
#     ) -> str:
#         stage = operation.stage_id
#         unpack_to_dest = "true" if operation.unpack_to_dest else "false"
#         broadcast_type = compute_unit.broadcast_type.cpp_enum_value
#         reuse_dest = compute_unit.reuse_dest.cpp_enum_value
#
#         return (
#             f"_llk_unpack_A_<{broadcast_type}, false, {reuse_dest}, {unpack_to_dest}>(\n"
#             f"    L1_ADDRESS(buffer_A{stage}[{block.tile_id_global}]), unpack_a_src_format{stage}, unpack_a_dst_format{stage}\n"
#             f");\n"
#         )


# class UnpackerTilizeA(Unpacker):
#     loop: FusedLoop = LoopTileByTile()
#
#     def get_headers(self) -> List[str]:
#         return [
#             "llk_unpack_common.h",
#             "llk_unpack_tilize.h",
#         ]
#
#     def perf_set_valid(
#         self,
#         operation: "FusedOperation",
#         config: "GlobalConfig",
#         compute_unit: "ComputeNode",
#         block: "BlockData",
#     ) -> str:
#         valid_cnt = 4 if config.architecture == ChipArchitecture.WORMHOLE else 1
#         return f"_perf_unpack_loop_set_valid<true, true>({valid_cnt});\n"
#
#     def perf_clear_valid(
#         self,
#         operation: "FusedOperation",
#         config: "GlobalConfig",
#         compute_unit: "ComputeNode",
#         block: "BlockData",
#     ) -> str:
#         valid_cnt = 4 if config.architecture == ChipArchitecture.WORMHOLE else 1
#         return f"_perf_math_loop_clear_valid<true, true>({valid_cnt});\n"
#
#     def golden(
#         self,
#         tensor_a: torch.Tensor,
#         tensor_b: torch.Tensor,
#         operation: "FusedOperation",
#         config: "GlobalConfig",
#         compute_unit: "ComputeNode",
#     ) -> Tuple[torch.Tensor, torch.Tensor]:
#         tilized_a = tilize_block(
#             tensor_a,
#             operation.src_a.dimensions,
#             operation.src_a.data_format,
#             operation.num_faces,
#         )
#
#         return tilized_a, None
#
#     def init(
#         self,
#         operation: "FusedOperation",
#         config: "GlobalConfig",
#         compute_unit: "ComputeNode",
#         block: "BlockData",
#     ) -> str:
#         stage = operation.stage_id
#         face_r_dim = operation.face_r_dim
#         block_ct_dim = operation.output.tile_count_x
#         transpose_faces = compute_unit.unpack_transpose_faces.value
#         transpose_within_face = compute_unit.unpack_transpose_within_face.value
#         if compute_unit.broadcast_type != BroadcastType.None_:
#             raise ValueError("UnpackerTilizeA does not support broadcast")
#
#         if transpose_faces or transpose_within_face:
#             raise ValueError("UnpackerTilizeA does not support transpose")
#
#         return f"    _llk_unpack_tilize_init_(unpack_a_src_format{stage}, unpack_a_dst_format{stage}, {block_ct_dim}, {face_r_dim}, false);\n"
#
#     def unpack(
#         self,
#         operation: "FusedOperation",
#         config: "GlobalConfig",
#         compute_unit: "ComputeNode",
#         block: "BlockData",
#     ) -> str:
#         stage = operation.stage_id
#         face_r_dim = operation.face_r_dim
#         num_faces = operation.num_faces
#         block_ct_dim = operation.output.tile_count_x
#
#         # For tilize, we need to compute row/col from tile_idx
#         # Blackhole
#         if config.architecture == ChipArchitecture.BLACKHOLE:
#             return (
#                 f"{{\n"
#                 f"    std::uint32_t row = ({block.tile_id_global}) / {block_ct_dim};\n"
#                 f"    std::uint32_t col = ({block.tile_id_global}) % {block_ct_dim};\n"
#                 f"    _llk_unpack_tilize_(L1_ADDRESS(buffer_A{stage}[row * {block_ct_dim}]), col, unpack_a_src_format{stage}, unpack_a_dst_format{stage});\n"
#                 f"}}\n"
#             )
#
#         # Wormhole
#         elif config.architecture == ChipArchitecture.WORMHOLE:
#             return (
#                 f"{{\n"
#                 f"    std::uint32_t row = ({block.tile_id_global}) / {block_ct_dim};\n"
#                 f"    std::uint32_t col = ({block.tile_id_global}) % {block_ct_dim};\n"
#                 f"    _llk_unpack_tilize_(L1_ADDRESS(buffer_A{stage}[row * {block_ct_dim}]), col, unpack_a_src_format{stage}, unpack_a_dst_format{stage}, {block_ct_dim}, {face_r_dim}, {num_faces}, false);\n"
#                 f"}}\n"
#             )
#
#         else:
#             raise ValueError("Architecture is not supported")
#
#     def uninit(
#         self,
#         operation: "FusedOperation",
#         config: "GlobalConfig",
#         compute_unit: "ComputeNode",
#         block: "BlockData",
#     ) -> str:
#         stage = operation.stage_id
#         face_r_dim = operation.face_r_dim
#         num_faces = operation.num_faces
#
#         # Blackhole
#         if config.architecture == ChipArchitecture.BLACKHOLE:
#             code = f"    _llk_unpack_tilize_uninit_(unpack_a_dst_format{stage}, {num_faces}, {face_r_dim});\n"
#
#         # Wormhole
#         elif config.architecture == ChipArchitecture.WORMHOLE:
#             code = f"    _llk_unpack_tilize_uninit_(unpack_a_dst_format{stage}, {face_r_dim});\n\n"
#
#         else:
#             raise ValueError("Architecture is not supported")
#
#         return code
