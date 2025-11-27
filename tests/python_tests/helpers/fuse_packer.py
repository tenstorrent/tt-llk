# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
#
# SPDX-License-Identifier: Apache-2.0

from typing import Dict, List


class Packer:
    def pack(self, config: Dict) -> str:
        return ""

    def get_headers(self) -> List[str]:
        return []


class MatmulPacker(Packer):
    def get_headers(self) -> List[str]:
        return [
            "llk_pack.h",
            "llk_pack_common.h",
        ]

    def pack(self, config: Dict) -> str:
        stage = config["stage_id"]
        num_stages = config["num_stages"]

        pack_src = config["pack_in"]
        pack_dst = config["pack_out"]

        PACK_IN = f"static_cast<std::underlying_type_t<DataFormat>>(DataFormat::{pack_src.name})"
        PACK_OUT = f"static_cast<std::underlying_type_t<DataFormat>>(DataFormat::{pack_dst.name})"

        result_buffer_address = config["result_buffer_address"]

        pack_size = config["tile_size_pack"]

        TILE_CNT = config["tile_cnt"]
        TILIZE = "false"

        dest_acc = config["dest_acc"]
        dest_acc_value = dest_acc.value

        buffer_Res_tile_size = config["buffer_Res_tile_size"]

        code = f"""
    constexpr Operand buffer_Res{stage}({hex(result_buffer_address)}, {buffer_Res_tile_size});

#ifdef ARCH_BLACKHOLE
    _llk_pack_hw_configure_<{dest_acc_value}, false, {TILIZE}>(
        {PACK_IN},
        {PACK_OUT},
        {pack_size}
    );
    _llk_pack_init_<false, false, DstTileFaceLayout::RowMajor, false, {TILIZE}>(
        {PACK_OUT}
    );
    _llk_pack_dest_init_<DstSync::SyncHalf, {dest_acc_value}, DstTileFaceLayout::RowMajor>();
#else
    _llk_pack_hw_configure_<{dest_acc_value}, false>(
        {PACK_IN},
        {PACK_OUT},
        {pack_size}
    );
    _llk_pack_init_<false, false, DstTileFaceLayout::RowMajor, false>(
        {PACK_OUT}
    );
    _llk_pack_dest_init_<DstSync::SyncHalf, {dest_acc_value}, DstTileFaceLayout::RowMajor, false>();
#endif
    _llk_packer_wait_for_math_done_();
    for (int i = 0; i < {TILE_CNT}; i++)
    {{
        _llk_pack_<DstSync::SyncHalf, {dest_acc_value}, false>(i, L1_ADDRESS(buffer_Res{stage}[i]));
    }}
    _llk_pack_dest_section_done_<DstSync::SyncHalf, {dest_acc_value}>();
"""
        if stage < num_stages - 1:
            code += f"""
    t6_semaphore_post<>(semaphore::PACK_DONE);
"""
        return code
