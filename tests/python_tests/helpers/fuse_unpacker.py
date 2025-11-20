# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
#
# SPDX-License-Identifier: Apache-2.0

from typing import Dict

from .format_config import DataFormat
from .llk_params import format_tile_sizes


class Unpacker:
    def unpack(self, config: Dict) -> str:
        return ""


class MatmulUnpacker(Unpacker):
    def unpack(self, config: Dict) -> str:
        stage = config["stage_id"]
        FACE_R_DIM = config.get("face_r_dim", 16)
        CT_DIM = config["ct_dim"]
        RT_DIM = config["rt_dim"]
        KT_DIM = config["kt_dim"]
        # TILE_SIZE_UNPACK_A = FACE_R_DIM * CT_DIM
        # TILE_SIZE_UNPACK_B = FACE_R_DIM * KT_DIM

        buffer_A_address = config.get("buffer_A_address", 0x1A000)
        buffer_B_address = config.get("buffer_B_address", 0x1B000)

        formats = config.get("formats")
        # print("formats in unpacker:", formats)
        UNPACK_A_IN = f"static_cast<std::underlying_type_t<DataFormat>>(DataFormat::{formats.input_format})"
        UNPACK_A_OUT = f"static_cast<std::underlying_type_t<DataFormat>>(DataFormat::{formats.input_format})"

        TILE_SIZES = {
            DataFormat.Bfp8_b: 68,
            DataFormat.Float32: 256,
        }

        # pack_size = TILE_SIZES.get(formats.output_format, 128)
        unpack_size_a = TILE_SIZES.get(formats.input_format, 128)
        unpack_size_b = TILE_SIZES.get(formats.input_format, 128)

        num_faces_A = config.get("num_faces_A", config.get("num_faces", 4))

        in0_tile_r_dim = config.get("in0_tile_r_dim", 32)
        face_r_dim = config.get("face_r_dim", 16)
        tiny_tiles = config.get("tiny_tiles", False)

        if tiny_tiles:
            unpack_size_a = (unpack_size_a // num_faces_A) * (
                in0_tile_r_dim // face_r_dim
            )

        code = ""

        if stage > 0:
            code += f"""
    t6_semaphore_wait_on_zero<p_stall::STALL_SYNC>(semaphore::PACK_DONE);
    t6_semaphore_get<>(semaphore::PACK_DONE);
"""

        code += f"""
    constexpr Operand buffer_A{stage}({hex(buffer_A_address)}, {format_tile_sizes[formats.input_format if formats is not None else DataFormat.Float16_b]});
    constexpr Operand buffer_B{stage}({hex(buffer_B_address)}, {format_tile_sizes[formats.input_format if formats is not None else DataFormat.Float16_b]});

    _llk_unpack_AB_matmul_hw_configure_<is_fp32_dest_acc_en, StochRndType::None>(
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
