# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
#
# SPDX-License-Identifier: Apache-2.0

from typing import TYPE_CHECKING, List

from .llk_params import Tilize

if TYPE_CHECKING:
    from .fused_operation import FusedOperation


class Unpacker:
    def unpack(self, operation_config: "FusedOperation") -> str:
        return ""

    def get_headers(self) -> List[str]:
        return []


class MatmulUnpacker(Unpacker):
    def get_headers(self) -> List[str]:
        return [
            "llk_unpack_AB_matmul.h",
            "llk_unpack_common.h",
        ]

    def unpack(self, operation_config: "FusedOperation") -> str:
        stage = operation_config.stage_id
        face_r_dim = operation_config.face_r_dim
        ct_dim = operation_config.ct_dim
        rt_dim = operation_config.rt_dim
        kt_dim = operation_config.kt_dim

        buffer_A_address = operation_config.src_a.l1_address
        buffer_B_address = operation_config.src_b.l1_address
        unpack_a_src = operation_config.unpack_a_in
        unpack_a_dst = operation_config.unpack_a_out
        unpack_b_src = operation_config.unpack_a_in
        unpack_b_dst = operation_config.unpack_a_out
        unpack_size_a = operation_config.tile_size_unpack_a
        unpack_size_b = operation_config.tile_size_unpack_b
        dest_acc_value = operation_config.dest_acc.value
        buffer_A_tile_size = operation_config.buffer_A_tile_size
        buffer_B_tile_size = operation_config.buffer_B_tile_size

        code = (
            f"    // Operation {stage}: Matmul Unpacker\n"
            f"    const Operand buffer_A{stage}({hex(buffer_A_address)}, {buffer_A_tile_size});\n"
            f"    const Operand buffer_B{stage}({hex(buffer_B_address)}, {buffer_B_tile_size});\n"
            f"    const uint32_t unpack_a_src_format{stage} = static_cast<std::underlying_type_t<DataFormat>>(DataFormat::{unpack_a_src.name});\n"
            f"    const uint32_t unpack_a_dst_format{stage} = static_cast<std::underlying_type_t<DataFormat>>(DataFormat::{unpack_a_dst.name});\n"
            f"    const uint32_t unpack_b_src_format{stage} = static_cast<std::underlying_type_t<DataFormat>>(DataFormat::{unpack_b_src.name});\n"
            f"    const uint32_t unpack_b_dst_format{stage} = static_cast<std::underlying_type_t<DataFormat>>(DataFormat::{unpack_b_dst.name});\n\n"
        )

        if stage > 0:
            code += (
                f"    t6_semaphore_wait_on_zero<p_stall::STALL_SYNC>(semaphore::PACK_DONE);\n"
                f"    t6_semaphore_get<>(semaphore::PACK_DONE);\n\n"
            )

        code += (
            f"    _llk_unpack_AB_matmul_hw_configure_<{dest_acc_value}, StochRndType::None>(\n"
            f"        unpack_a_src_format{stage}, unpack_b_src_format{stage}, unpack_a_dst_format{stage}, unpack_b_dst_format{stage},\n"
            f"        {face_r_dim}, {face_r_dim}, 0, 4, 4, {unpack_size_a}, {unpack_size_b}\n"
            f"    );\n"
            f"    _llk_unpack_AB_matmul_init_<>(0, {ct_dim}, {rt_dim}, {kt_dim}, {face_r_dim}, {face_r_dim});\n"
            f"    for (uint32_t j = 0; j < {kt_dim}; j++)\n"
            f"    {{\n"
            f"        _llk_unpack_AB_matmul_<>(\n"
            f"            L1_ADDRESS(buffer_A{stage}[0]), L1_ADDRESS(buffer_B{stage}[0]),\n"
            f"            j, j * {ct_dim}, {unpack_size_a}, {unpack_size_b}, false, false, {ct_dim}, {rt_dim}, {kt_dim}\n"
            f"        );\n"
            f"    }}\n"
            f"\n"
        )

        return code


class UnpackerAB(Unpacker):
    def get_headers(self) -> List[str]:
        return [
            "llk_unpack_AB.h",
            "llk_unpack_common.h",
            "llk_unpack_tilize.h",
        ]

    def unpack(self, operation_config: "FusedOperation") -> str:
        stage = operation_config.stage_id
        buffer_A_address = operation_config.src_a.l1_address
        buffer_B_address = operation_config.src_b.l1_address
        unpack_a_src = operation_config.unpack_a_in
        unpack_a_dst = operation_config.unpack_a_out
        unpack_b_src = operation_config.unpack_a_in
        unpack_b_dst = operation_config.unpack_a_out
        tile_cnt = operation_config.output.tile_count
        dest_acc_value = operation_config.dest_acc.value
        buffer_A_tile_size = operation_config.buffer_A_tile_size
        buffer_B_tile_size = operation_config.buffer_B_tile_size

        code = (
            f"    // Operation {stage}: Unpacker AB\n"
            f"    const Operand buffer_A{stage}({hex(buffer_A_address)}, {buffer_A_tile_size});\n"
            f"    const Operand buffer_B{stage}({hex(buffer_B_address)}, {buffer_B_tile_size});\n"
            f"    const uint32_t unpack_a_src_format{stage} = static_cast<std::underlying_type_t<DataFormat>>(DataFormat::{unpack_a_src.name});\n"
            f"    const uint32_t unpack_a_dst_format{stage} = static_cast<std::underlying_type_t<DataFormat>>(DataFormat::{unpack_a_dst.name});\n"
            f"    const uint32_t unpack_b_src_format{stage} = static_cast<std::underlying_type_t<DataFormat>>(DataFormat::{unpack_b_src.name});\n"
            f"    const uint32_t unpack_b_dst_format{stage} = static_cast<std::underlying_type_t<DataFormat>>(DataFormat::{unpack_b_dst.name});\n\n"
        )

        if stage > 0:
            code += (
                f"    t6_semaphore_wait_on_zero<p_stall::STALL_SYNC>(semaphore::PACK_DONE);\n"
                f"    t6_semaphore_get<>(semaphore::PACK_DONE);\n\n"
            )

        code += (
            f"    _llk_unpack_AB_hw_configure_<{dest_acc_value}, StochRndType::None>(\n"
            f"        unpack_a_src_format{stage}, unpack_b_src_format{stage}, unpack_a_dst_format{stage}, unpack_b_dst_format{stage}\n"
            f"    );\n"
            f"    _llk_unpack_AB_init_<>();\n"
            f"    for (int i = 0; i < {tile_cnt}; i++)\n"
            f"    {{\n"
            f"        _llk_unpack_AB_<>(L1_ADDRESS(buffer_A{stage}[i]), L1_ADDRESS(buffer_B{stage}[i]));\n"
            f"    }}\n"
            f"\n"
        )

        return code


class UnpackerA(Unpacker):
    def get_headers(self) -> List[str]:
        return [
            "llk_unpack_A.h",
            "llk_unpack_common.h",
            "llk_unpack_tilize.h",
        ]

    def unpack(self, operation_config: "FusedOperation") -> str:
        stage = operation_config.stage_id
        buffer_A_address = operation_config.src_a.l1_address
        unpack_a_src = operation_config.unpack_a_in
        unpack_a_dst = operation_config.unpack_a_out
        tile_cnt = operation_config.output.tile_count
        dest_acc = operation_config.dest_acc.value
        buffer_A_tile_size = operation_config.buffer_A_tile_size
        tilize_en = operation_config.tilize
        unpack_to_dest = "true" if operation_config.unpack_to_dest else "false"
        brodcast_type = "NONE"  # TODO: make dynamic based on operation_config
        eltwise_reuse_type = "NONE"  # TODO: make dynamic based on operation_config
        face_r_dim = operation_config.face_r_dim
        num_faces = operation_config.num_faces
        block_rt_dim = operation_config.block_rt_dim
        block_ct_dim = operation_config.block_ct_dim

        code = (
            f"    // Operation {stage}: Unpacker A\n"
            f"    const Operand buffer_A{stage}({hex(buffer_A_address)}, {buffer_A_tile_size});\n"
            f"    const uint32_t unpack_src_format{stage} = static_cast<std::underlying_type_t<DataFormat>>(DataFormat::{unpack_a_src.name});\n"
            f"    const uint32_t unpack_dst_format{stage} = static_cast<std::underlying_type_t<DataFormat>>(DataFormat::{unpack_a_dst.name});\n\n"
        )

        if stage > 0:
            code += (
                f"    t6_semaphore_wait_on_zero<p_stall::STALL_SYNC>(semaphore::PACK_DONE);\n"
                f"    t6_semaphore_get<>(semaphore::PACK_DONE);\n"
            )

        if tilize_en == Tilize.No:
            code += (
                f"    _llk_unpack_A_hw_configure_<{dest_acc}, StochRndType::None>(\n"
                f"        unpack_src_format{stage}, unpack_dst_format{stage}, {face_r_dim}, 0, {num_faces}\n"
                f"    );\n"
                f"    _llk_unpack_A_init_<BroadcastType::{brodcast_type}, false, EltwiseBinaryReuseDestType::{eltwise_reuse_type}, {unpack_to_dest}>(\n"
                f"        0, 0, {face_r_dim}, {num_faces}, unpack_src_format{stage}, unpack_dst_format{stage}\n"
                f"    );\n"
                f"    for (int i = 0; i < {tile_cnt}; ++i)\n"
                f"    {{\n"
                f"        _llk_unpack_A_<BroadcastType::{brodcast_type}, false, EltwiseBinaryReuseDestType::NONE, {unpack_to_dest}>(\n"
                f"            L1_ADDRESS(buffer_A{stage}[i]), unpack_src_format{stage}, unpack_dst_format{stage}\n"
                f"        );\n"
                f"    }}\n"
            )
        else:
            code += (
                f"    _llk_unpack_tilize_hw_configure_<{dest_acc}, StochRndType::None>(\n"
                f"        unpack_src_format{stage}, unpack_dst_format{stage}, {face_r_dim}, 0, {num_faces}\n"
                f"    );\n"
                f"    _llk_unpack_tilize_init_(unpack_src_format{stage}, unpack_dst_format{stage}, {block_ct_dim}, {face_r_dim}, false);\n"
                f"    for (uint32_t i = 0; i < {block_rt_dim}; i++)\n"
                f"    {{\n"
                f"        for (uint32_t j = 0; j < {block_ct_dim}; j++)\n"
                f"        {{\n"
                f"            _llk_unpack_tilize_(L1_ADDRESS(buffer_A{stage}[i * {block_rt_dim}]), j, unpack_src_format{stage});\n"
                f"        }}\n"
                f"    }}\n"
            )
        code += "\n"

        return code
