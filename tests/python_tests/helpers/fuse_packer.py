# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
#
# SPDX-License-Identifier: Apache-2.0

from typing import Dict

from .format_config import DataFormat
from .llk_params import format_tile_sizes


class Packer:
    def pack(self, config: Dict) -> str:
        return ""


class MatmulPacker(Packer):
    def pack(self, config: Dict) -> str:
        stage = config.get("stage", 0)
        num_stages = config.get("num_stages", 1)
        formats = config.get("formats")

        PACK_IN = f"static_cast<std::underlying_type_t<DataFormat>>(DataFormat::{formats.input_format})"
        PACK_OUT = f"static_cast<std::underlying_type_t<DataFormat>>(DataFormat::{formats.output_format})"

        result_buffer_address = config.get("result_buffer_address", 0x1C000)

        TILE_CNT = config.get("tile_cnt", 1)

        code = f"""
        constexpr Operand buffer_Res{stage}({hex(result_buffer_address)}, {format_tile_sizes[formats.output_format if formats is not None else DataFormat.Float16_b]});

#ifdef ARCH_BLACKHOLE
    const bool TILIZE = {str(stage < num_stages - 1).lower()};
    _llk_pack_hw_configure_<is_fp32_dest_acc_en, false, TILIZE>({PACK_IN}, {PACK_OUT}, 128);
    _llk_pack_init_<false, false, DstTileFaceLayout::RowMajor, false, TILIZE>({PACK_OUT});
    _llk_pack_dest_init_<DstSync::SyncHalf, is_fp32_dest_acc_en, DstTileFaceLayout::RowMajor>();
#else
//    _llk_pack_hw_configure_<is_fp32_dest_acc_en, false>({PACK_IN}, {PACK_OUT}, 128);
//    _llk_pack_init_<false, false, DstTileFaceLayout::RowMajor, false>({PACK_OUT});
//    _llk_pack_dest_init_<DstSync::SyncHalf, is_fp32_dest_acc_en, DstTileFaceLayout::RowMajor, false>();
#endif
    _llk_packer_wait_for_math_done_();
    for (int i = 0; i < {TILE_CNT}; i++)
    {{
        _llk_pack_<DstSync::SyncHalf, is_fp32_dest_acc_en, false>(i, L1_ADDRESS(buffer_Res{stage}[i]));
    }}
    _llk_pack_dest_section_done_<DstSync::SyncHalf, is_fp32_dest_acc_en>();
"""
        if stage < num_stages - 1:
            code += f"""
    t6_semaphore_post<>(semaphore::PACK_DONE);
"""
        return code
