# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
#
# SPDX-License-Identifier: Apache-2.0

from typing import TYPE_CHECKING, List

import torch

from .chip_architecture import ChipArchitecture

if TYPE_CHECKING:
    from .fused_operation import FusedOperation

from .golden_generators import UntilizeGolden, get_golden_generator


class Packer:
    def get_headers(self) -> List[str]:
        return [
            "llk_pack.h",
            "llk_pack_common.h",
        ]

    def golden(
        self,
        tensor: torch.Tensor,
        operation_config: "FusedOperation",
    ) -> torch.Tensor:
        return tensor

    def exec(self, operation_config: "FusedOperation") -> str:
        stage = operation_config.stage_id
        pack_src = operation_config.pack_in
        pack_dst = operation_config.pack_out
        result_buffer_address = operation_config.output.l1_address
        buffer_Res_tile_size = operation_config.buffer_Res_tile_size

        code = (
            f"    // Operation {stage}: Packer\n"
            f"    const Operand buffer_Res{stage}({hex(result_buffer_address)}, {buffer_Res_tile_size});\n"
            f"    const uint32_t pack_src_format{stage} = static_cast<std::underlying_type_t<DataFormat>>(DataFormat::{pack_src.name});\n"
            f"    const uint32_t pack_dst_format{stage} = static_cast<std::underlying_type_t<DataFormat>>(DataFormat::{pack_dst.name});\n"
        )

        code += self.hw_configure(operation_config)
        code += self.pack(operation_config)
        code += self.uninit(operation_config)
        code += self.sync(operation_config)

        return code

    def uninit(self, operation_config: "FusedOperation") -> str:
        return "\n"

    def hw_configure(self, operation_config: "FusedOperation") -> str:
        stage = operation_config.stage_id
        tilize = operation_config.bh_tilize.value
        dest_acc = operation_config.dest_acc.value
        pack_size = operation_config.tile_size_pack

        if stage == 0:
            if operation_config.architecture == ChipArchitecture.BLACKHOLE:
                code = (
                    f"    _llk_pack_hw_configure_<{dest_acc}, false, {tilize}>(\n"
                    f"        pack_src_format{stage}, pack_dst_format{stage}, {pack_size}\n"
                    f"    );\n"
                )
            elif operation_config.architecture == ChipArchitecture.WORMHOLE:
                code = (
                    f"    _llk_pack_hw_configure_<{dest_acc}, false>(\n"
                    f"        pack_src_format{stage}, pack_dst_format{stage}, {pack_size}\n"
                    f"    );\n"
                )
        else:
            code = (
                f"    _llk_pack_reconfig_data_format_<{dest_acc}, false>(\n"
                f"        pack_src_format{stage}, pack_dst_format{stage}, {pack_size}\n"
                f"    );\n"
            )

        return code

    def pack(self, operation_config: "FusedOperation") -> str:
        stage = operation_config.stage_id
        tile_cnt = operation_config.output.tile_count
        dest_acc = operation_config.dest_acc
        dest_acc_value = dest_acc.value
        bh_tilize = operation_config.bh_tilize.value
        face_r_dim = operation_config.face_r_dim
        num_faces = operation_config.num_faces

        if operation_config.architecture == ChipArchitecture.BLACKHOLE:
            code = (
                f"    _llk_pack_init_<false, false, {bh_tilize}>(\n"
                f"        pack_src_format{stage}, pack_dst_format{stage}, {face_r_dim}, TILE_C_DIM, {num_faces}, false, false\n"
                f"    );\n"
                f"    _llk_pack_dest_init_<DstSync::SyncHalf, {dest_acc_value}>();\n"
            )
        elif operation_config.architecture == ChipArchitecture.WORMHOLE:
            code = (
                f"    _llk_pack_init_<false, false>(\n"
                # pack_src_format is unused
                f"        pack_dst_format{stage}\n"  # , pack_src_format{stage}, {face_r_dim}, {num_faces}, false, false, false\n"
                f"    );\n"
                f"    _llk_pack_dest_init_<DstSync::SyncHalf, {dest_acc_value}, false>();\n"
            )
        else:
            raise ValueError("Unsupported architecture for packer")

        code += (
            f"    _llk_packer_wait_for_math_done_();\n"
            f"    for (int i = 0; i < {tile_cnt}; i++)\n"
            f"    {{\n"
            f"        _llk_pack_<DstSync::SyncHalf, {dest_acc_value}, false>(i, L1_ADDRESS(buffer_Res{stage}[i]));\n"
            f"    }}\n"
            f"    _llk_pack_dest_section_done_<DstSync::SyncHalf, {dest_acc_value}>();\n\n"
        )

        return code

    def sync(self, operation_config: "FusedOperation") -> str:
        stage = operation_config.stage_id
        num_stages = operation_config.num_stages

        if stage < num_stages - 1:
            return "    t6_semaphore_post<>(semaphore::PACK_DONE);\n\n"

        return ""


class PackerUntilize(Packer):
    def golden(
        self,
        tensor: torch.Tensor,
        operation_config: "FusedOperation",
    ) -> torch.Tensor:
        golden_packer = get_golden_generator(UntilizeGolden)

        tensor = golden_packer(
            tensor,
            operation_config.output.data_format,
            operation_config.output.dimensions,
        )

        return tensor

    def hw_configure(self, operation_config: "FusedOperation") -> str:
        stage = operation_config.stage_id
        tilize = operation_config.bh_tilize.value
        dest_acc = operation_config.dest_acc.value
        pack_size = operation_config.tile_size_pack

        if stage == 0:
            if operation_config.architecture == ChipArchitecture.BLACKHOLE:
                code = (
                    f"    _llk_pack_hw_configure_<{dest_acc}, true, {tilize}>(\n"
                    f"        pack_src_format{stage}, pack_dst_format{stage}, {pack_size}\n"
                    f"    );\n"
                )
            elif operation_config.architecture == ChipArchitecture.WORMHOLE:
                code = (
                    f"    _llk_pack_hw_configure_<{dest_acc}, true>(\n"
                    f"        pack_src_format{stage}, pack_dst_format{stage}, {pack_size}\n"
                    f"    );\n"
                )
        else:
            code = (
                f"    _llk_pack_reconfig_data_format_<{dest_acc}, false>(\n"
                f"        pack_src_format{stage}, pack_dst_format{stage}, {pack_size}\n"
                f"    );\n"
            )

        return code

    def uninit(self, operation_config: "FusedOperation") -> str:
        stage = operation_config.stage_id
        architecture = operation_config.architecture
        if architecture == ChipArchitecture.BLACKHOLE:
            return f"    _llk_pack_untilize_uninit_(pack_src_format{stage})\n\n"
        else:
            return "\n"

    def pack(self, operation_config: "FusedOperation") -> str:
        stage = operation_config.stage_id
        dest_acc = operation_config.dest_acc
        dest_acc_value = dest_acc.value
        face_r_dim = operation_config.face_r_dim
        num_faces = operation_config.num_faces
        block_ct_dim = operation_config.block_ct_dim
        full_ct_dim = operation_config.full_ct_dim
        full_rt_dim = operation_config.full_rt_dim

        if operation_config.architecture == ChipArchitecture.BLACKHOLE:
            code = (
                f"    _llk_pack_untilize_init_<{block_ct_dim}, {full_ct_dim}, false, false, TILE_C_DIM>(\n"
                f"        pack_src_format{stage}, pack_dst_format{stage}, {face_r_dim}, {num_faces}, false\n"
                f"    );\n"
                f"    _llk_pack_dest_init_<DstSync::SyncHalf, {dest_acc_value}>();\n"
            )
        elif operation_config.architecture == ChipArchitecture.WORMHOLE:
            code = (
                f"    _llk_pack_untilize_init_<{block_ct_dim}, {full_ct_dim}, false, false, TILE_C_DIM>(\n"
                f"        pack_dst_format{stage}, {face_r_dim}, {num_faces}, true\n"
                f"    );\n"
                f"    _llk_pack_dest_init_<DstSync::SyncHalf, {dest_acc_value}, true>();\n"
            )
        else:
            raise ValueError("Unsupported architecture for packer")

        code += (
            f"    _llk_packer_wait_for_math_done_();\n"
            f"    for (int rt = 0; rt < {full_rt_dim}; rt++)\n"
            f"    {{\n"
            f"        _llk_pack_untilize_<{block_ct_dim}, {full_ct_dim}, false, false, TILE_C_DIM, 0>(\n"
            f"            L1_ADDRESS(buffer_Res{stage}[rt * {full_ct_dim}]), pack_dst_format{stage}, {face_r_dim}, {num_faces}, rt * {full_ct_dim}\n"
            f"        );\n"
            f"    }}\n"
            f"    _llk_pack_dest_section_done_<DstSync::SyncHalf, {dest_acc_value}>();\n\n"
        )

        return code
