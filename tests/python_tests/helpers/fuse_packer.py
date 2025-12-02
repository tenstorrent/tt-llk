# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
#
# SPDX-License-Identifier: Apache-2.0

from typing import TYPE_CHECKING, List

from .chip_architecture import ChipArchitecture

if TYPE_CHECKING:
    from .fuse_operation import PipelineOperation


class Packer:
    def get_headers(self) -> List[str]:
        return [
            "llk_pack.h",
            "llk_pack_common.h",
        ]

    def pack(self, operation_config: "PipelineOperation") -> str:
        stage = operation_config.stage_id
        num_stages = operation_config.num_stages
        pack_src = operation_config.pack_in
        pack_dst = operation_config.pack_out

        PACK_IN = f"static_cast<std::underlying_type_t<DataFormat>>(DataFormat::{pack_src.name})"
        PACK_OUT = f"static_cast<std::underlying_type_t<DataFormat>>(DataFormat::{pack_dst.name})"

        result_buffer_address = operation_config.output.l1_address

        pack_size = operation_config.tile_size_pack

        TILE_CNT = operation_config.output.tile_count
        TILIZE = operation_config.tilize.value

        dest_acc = operation_config.dest_acc
        dest_acc_value = dest_acc.value

        buffer_Res_tile_size = operation_config.buffer_Res_tile_size

        code = f"\n\t// Operation {stage}: Packer\n"

        code += f"""
    Operand buffer_Res{stage}({hex(result_buffer_address)}, {buffer_Res_tile_size});
"""
        if operation_config.architecture == ChipArchitecture.BLACKHOLE:
            code += f"""
    _llk_pack_hw_configure_<{dest_acc_value}, false, {TILIZE}>(
        {PACK_IN},
        {PACK_OUT},
        {pack_size}
    );
    _llk_pack_init_<false, false, DstTileFaceLayout::RowMajor, false, {TILIZE}>(
        {PACK_OUT}
    );
    _llk_pack_dest_init_<DstSync::SyncHalf, {dest_acc_value}, DstTileFaceLayout::RowMajor>();
"""
        elif operation_config.architecture == ChipArchitecture.WORMHOLE:
            code += f"""
    _llk_pack_hw_configure_<{dest_acc_value}, false>(
        {PACK_IN},
        {PACK_OUT},
        {pack_size}
    );
    _llk_pack_init_<false, false, DstTileFaceLayout::RowMajor, false>(
        {PACK_OUT}
    );
    _llk_pack_dest_init_<DstSync::SyncHalf, {dest_acc_value}, DstTileFaceLayout::RowMajor, false>();
"""
        else:
            raise ValueError("Unsupported architecture for packer")

        code += f"""
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
<<<<<<< HEAD
=======


class EltwisePacker(Packer):
    def get_headers(self) -> List[str]:
        return [
            "llk_pack.h",
            "llk_pack_common.h",
        ]

    def pack(self, operation_config: "PipelineOperation") -> str:
        stage = operation_config.stage_id
        num_stages = operation_config.num_stages
        pack_src = operation_config.pack_in
        pack_dst = operation_config.pack_out

        PACK_IN = f"static_cast<std::underlying_type_t<DataFormat>>(DataFormat::{pack_src.name})"
        PACK_OUT = f"static_cast<std::underlying_type_t<DataFormat>>(DataFormat::{pack_dst.name})"

        result_buffer_address = operation_config.output.l1_address

        pack_size = operation_config.tile_size_pack

        TILE_CNT = operation_config.output.tile_count
        TILIZE = "false"

        dest_acc = operation_config.dest_acc.value

        buffer_Res_tile_size = operation_config.buffer_Res_tile_size

        code = f"""
    constexpr Operand buffer_Res{stage}({hex(result_buffer_address)}, {buffer_Res_tile_size});

#ifdef ARCH_BLACKHOLE
    _llk_pack_hw_configure_<{dest_acc}, false, {TILIZE}>(
        {PACK_IN},
        {PACK_OUT},
        {pack_size}
    );
    _llk_pack_init_<false, false, DstTileFaceLayout::RowMajor, false, {TILIZE}>(
        {PACK_OUT}
    );
    _llk_pack_dest_init_<DstSync::SyncHalf, {dest_acc}, DstTileFaceLayout::RowMajor>();
#else
    _llk_pack_hw_configure_<{dest_acc}, false>(
        {PACK_IN},
        {PACK_OUT},
        {pack_size}
    );
    _llk_pack_init_<false, false, DstTileFaceLayout::RowMajor, false>(
        {PACK_OUT}
    );
    _llk_pack_dest_init_<DstSync::SyncHalf, {dest_acc}, DstTileFaceLayout::RowMajor, false>();
#endif
    _llk_packer_wait_for_math_done_();
    for (int i = 0; i < {TILE_CNT}; i++)
    {{
        _llk_pack_<DstSync::SyncHalf, {dest_acc}, false>(i, L1_ADDRESS(buffer_Res{stage}[i]));
    }}
    _llk_pack_dest_section_done_<DstSync::SyncHalf, {dest_acc}>();
"""

        if stage < num_stages - 1:
            code += f"""
    t6_semaphore_post<>(semaphore::PACK_DONE);
"""
        return code
>>>>>>> 3e3631e3 (fix: synchronization and golden for eltwise fpu)
