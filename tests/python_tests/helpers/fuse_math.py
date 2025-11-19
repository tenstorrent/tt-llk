# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
#
# SPDX-License-Identifier: Apache-2.0

from typing import Dict

from .data_format_inference import infer_unpack_out


class Math:
    def math(self, config: Dict) -> str:
        return ""


class MatmulMath(Math):
    def math(self, config: Dict) -> str:
        stage = config.get("stage", 0)
        CT_DIM = config["ct_dim"]
        RT_DIM = config["rt_dim"]
        KT_DIM = config["kt_dim"]

        formats = config["formats"]

        math_format = infer_unpack_out(
            formats.input_format,
            formats.output_format,
            config["dest_acc"],
            config["math_fidelity"],
        )

        MATH_FORMAT = f"static_cast<std::underlying_type_t<DataFormat>>(DataFormat::{math_format})"

        code = f"""
    _llk_math_matmul_init_<MATH_FIDELITY, DstTileFaceLayout::RowMajor>(TILE_R_DIM, TILE_C_DIM, TILE_R_DIM, TILE_C_DIM, false, 0, {CT_DIM}, {RT_DIM}, {KT_DIM});
    _llk_math_pack_sync_init_<DstSync::SyncHalf, is_fp32_dest_acc_en>();
    _llk_math_hw_configure_<false, false>({MATH_FORMAT}, {MATH_FORMAT});
    _llk_math_wait_for_dest_available_<DstSync::SyncHalf>();
    for (uint32_t j = 0; j < {KT_DIM}; j++)
    {{
        _llk_math_matmul_<MATH_FIDELITY, DstTileFaceLayout::RowMajor>(0, 0, {CT_DIM}, {RT_DIM}, {KT_DIM});
    }}
    _llk_math_dest_section_done_<DstSync::SyncHalf, is_fp32_dest_acc_en>();
"""
        return code
