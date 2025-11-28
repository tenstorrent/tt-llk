# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
#
# SPDX-License-Identifier: Apache-2.0

from typing import TYPE_CHECKING, List

if TYPE_CHECKING:
    from .fuse_operation import PipelineOperation


class Unpacker:
    def unpack(self, operation_config: "PipelineOperation") -> str:
        return ""

    def get_headers(self) -> List[str]:
        return []


class MatmulUnpacker(Unpacker):
    def get_headers(self) -> List[str]:
        return [
            "llk_unpack_A.h",
            "llk_unpack_AB.h",
            "llk_unpack_AB_matmul.h",
            "llk_unpack_common.h",
        ]

    def unpack(self, operation_config: "PipelineOperation") -> str:
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

        code = ""

        if stage > 0:
            code += f"""
    t6_semaphore_wait_on_zero<p_stall::STALL_SYNC>(semaphore::PACK_DONE);
    t6_semaphore_get<>(semaphore::PACK_DONE);
"""

        buffer_A_tile_size = operation_config.buffer_A_tile_size
        buffer_B_tile_size = operation_config.buffer_B_tile_size

        code += f"""
    constexpr Operand buffer_A{stage}({hex(buffer_A_address)}, {buffer_A_tile_size});
    constexpr Operand buffer_B{stage}({hex(buffer_B_address)}, {buffer_B_tile_size});

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
            {FACE_R_DIM},
            {FACE_R_DIM},
            false,
            false,
            {CT_DIM},
            {RT_DIM},
            {KT_DIM}
        );
    }}
"""
        return code
