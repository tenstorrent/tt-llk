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
        FACE_R_DIM = operation_config.face_r_dim
        CT_DIM = operation_config.ct_dim
        RT_DIM = operation_config.rt_dim
        KT_DIM = operation_config.kt_dim

        buffer_A_address = operation_config.src_a.l1_address
        buffer_B_address = operation_config.src_b.l1_address

        unpack_src = operation_config.unpack_a_in
        unpack_dst = operation_config.unpack_a_out

        UNPACK_A_IN = f"static_cast<std::underlying_type_t<DataFormat>>(DataFormat::{unpack_src.name})"
        UNPACK_A_OUT = f"static_cast<std::underlying_type_t<DataFormat>>(DataFormat::{unpack_dst.name})"

        unpack_size_a = operation_config.tile_size_unpack_a
        unpack_size_b = operation_config.tile_size_unpack_b

        dest_acc = operation_config.dest_acc
        dest_acc_value = dest_acc.value

        UNPACK_A_IN = f"static_cast<std::underlying_type_t<DataFormat>>(DataFormat::{unpack_src.name})"
        UNPACK_A_OUT = f"static_cast<std::underlying_type_t<DataFormat>>(DataFormat::{unpack_dst.name})"

        code = ""

        if stage > 0:
            code += f"""
    t6_semaphore_wait_on_zero<p_stall::STALL_SYNC>(semaphore::PACK_DONE);
    t6_semaphore_get<>(semaphore::PACK_DONE);
"""

        buffer_A_tile_size = operation_config.buffer_A_tile_size
        buffer_B_tile_size = operation_config.buffer_B_tile_size

        code += f"""
    Operand buffer_A{stage}({hex(buffer_A_address)}, {buffer_A_tile_size});
    Operand buffer_B{stage}({hex(buffer_B_address)}, {buffer_B_tile_size});
    _llk_unpack_AB_matmul_hw_configure_<{dest_acc_value}, StochRndType::None>(
        {UNPACK_A_IN},
        {UNPACK_A_IN},
        {UNPACK_A_OUT},
        {UNPACK_A_OUT},
        {FACE_R_DIM},
        {FACE_R_DIM},
        0,
        4,
        4,
        {unpack_size_a},
        {unpack_size_b}
    );
    _llk_unpack_AB_matmul_init_<>(0, {CT_DIM}, {RT_DIM}, {KT_DIM}, {FACE_R_DIM}, {FACE_R_DIM});
    for (uint32_t j = 0; j < {KT_DIM}; j++)
    {{
        _llk_unpack_AB_matmul_<>(
            L1_ADDRESS(buffer_A{stage}[0]),
            L1_ADDRESS(buffer_B{stage}[0]),
            j,
            j * {CT_DIM},
            {unpack_size_a},
            {unpack_size_b},
            false,
            false,
            {CT_DIM},
            {RT_DIM},
            {KT_DIM}
        );
    }}
"""
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

        UNPACK_A_IN = f"static_cast<std::underlying_type_t<DataFormat>>(DataFormat::{unpack_a_src.name})"
        UNPACK_A_OUT = f"static_cast<std::underlying_type_t<DataFormat>>(DataFormat::{unpack_a_dst.name})"
        UNPACK_B_IN = f"static_cast<std::underlying_type_t<DataFormat>>(DataFormat::{unpack_b_src.name})"
        UNPACK_B_OUT = f"static_cast<std::underlying_type_t<DataFormat>>(DataFormat::{unpack_b_dst.name})"

        tile_cnt = operation_config.output.tile_count

        dest_acc = operation_config.dest_acc
        dest_acc_value = dest_acc.value

        code = f"\n\t// Operation {stage}: Unpacker AB\n"

        if stage > 0:
            code += f"""
    t6_semaphore_wait_on_zero<p_stall::STALL_SYNC>(semaphore::PACK_DONE);
    t6_semaphore_get<>(semaphore::PACK_DONE);
"""

        buffer_A_tile_size = operation_config.buffer_A_tile_size
        buffer_B_tile_size = operation_config.buffer_B_tile_size

        code += f"""
    Operand buffer_A{stage}({hex(buffer_A_address)}, {buffer_A_tile_size});
    Operand buffer_B{stage}({hex(buffer_B_address)}, {buffer_B_tile_size});
"""

        code += f"""
    _llk_unpack_AB_hw_configure_<{dest_acc_value}, StochRndType::None>(
        {UNPACK_A_IN},
        {UNPACK_B_IN},
        {UNPACK_A_OUT},
        {UNPACK_B_OUT}
    );
    _llk_unpack_AB_init_<>();
    for (int i = 0; i < {tile_cnt}; i++)
    {{
        _llk_unpack_AB_<>(L1_ADDRESS(buffer_A{stage}[i]), L1_ADDRESS(buffer_B{stage}[i]));
    }}
"""
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
        buffer_B_address = operation_config.src_b.l1_address

        unpack_a_src = operation_config.unpack_a_in
        unpack_a_dst = operation_config.unpack_a_out

        unpack_src_format = f"static_cast<std::underlying_type_t<DataFormat>>(DataFormat::{unpack_a_src.name})"
        unpack_dst_format = f"static_cast<std::underlying_type_t<DataFormat>>(DataFormat::{unpack_a_dst.name})"

        tile_cnt = operation_config.output.tile_count

        dest_acc = operation_config.dest_acc.value

        code = f"\n\t// Operation {stage}: Unpacker A\n"

        if stage > 0:
            code += f"""
    t6_semaphore_wait_on_zero<p_stall::STALL_SYNC>(semaphore::PACK_DONE);
    t6_semaphore_get<>(semaphore::PACK_DONE);
"""

        buffer_A_tile_size = operation_config.buffer_A_tile_size

        code += f"""
    Operand buffer_A{stage}({hex(buffer_A_address)}, {buffer_A_tile_size});
"""
        tilize_en = operation_config.tilize
        unpack_to_dest = "true" if operation_config.unpack_to_dest else "false"

        brodcast_type = "NONE"  # TODO: make dynamic based on operation_config
        eltwise_reuse_type = "NONE"  # TODO: make dynamic based on operation_config

        face_r_dim = operation_config.face_r_dim
        num_faces = operation_config.num_faces

        block_rt_dim = operation_config.block_rt_dim
        block_ct_dim = operation_config.block_ct_dim

        if tilize_en == Tilize.No:
            code += f"""
    _llk_unpack_A_init_<BroadcastType::{brodcast_type}, false, EltwiseBinaryReuseDestType::{eltwise_reuse_type}, {unpack_to_dest}>(
        0, 0, {face_r_dim}, {num_faces}, {unpack_src_format}, {unpack_dst_format});
    _llk_unpack_A_hw_configure_<{dest_acc}, StochRndType::None>({unpack_src_format}, {unpack_dst_format}, {face_r_dim}, 0, {num_faces});

    for (int i = 0; i < {tile_cnt}; ++i)
    {{
        _llk_unpack_A_<BroadcastType::{brodcast_type}, false, EltwiseBinaryReuseDestType::NONE, {unpack_to_dest}>(
            L1_ADDRESS(buffer_A{stage}[i]), {unpack_src_format}, {unpack_dst_format});
    }}
"""
        else:
            code += f"""
    _llk_unpack_tilize_hw_configure_<{dest_acc}, StochRndType::None>({unpack_src_format}, {unpack_dst_format}, {face_r_dim}, 0, {num_faces});
    _llk_unpack_tilize_init_({unpack_src_format}, {unpack_dst_format}, {block_ct_dim}, {face_r_dim}, false);

    uint32_t read_offset = 0;

    for (uint32_t i = 0; i < {block_rt_dim}; i++)
    {{
        for (uint32_t j = 0; j < {block_ct_dim}; j++)
        {{
            _llk_unpack_tilize_(L1_ADDRESS(buffer_A{stage}[read_offset]), j, {unpack_src_format}, {block_ct_dim}, {face_r_dim}, {num_faces}, false);
        }}
        read_offset += {block_rt_dim};
    }}
"""
        return code
