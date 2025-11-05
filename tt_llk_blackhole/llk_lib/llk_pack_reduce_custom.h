// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cstdint>

#include "ckernel.h"
#include "ckernel_defs.h"
#include "ckernel_ops.h"
#include "cpack_common.h"
#include "llk_defs.h"

using namespace ckernel;
using namespace ckernel::packer;

/**
 * Configures pack masking for specialized reduce_max_row operations.
 *
 * This function works with the following assumptions:
 * - Scaler values are 1.0 and are contained inside F0 of the scaler tile
 * - The scaler doesn't change for the duration of the whole tile operation
 * - Operand and scaler data format is bfloat16_b
 * - Operand tile size is 32x32
 * - Can work on both 16-bit or 32-bit DEST register modes based on is_fp32_dest_acc_en flag
 * - Does only MAX pool on ROW dimension
 *
 * This function should NOT be used as a substitute for native reduce pack configuration.
 * Use the standard _llk_pack_reduce_mask_config_ for general-purpose reduction operations.
 */
inline void _llk_pack_reduce_max_row_mask_config_()
{
    ckernel::packer::pck_edge_offset_u pack_edge_offset = {.val = 0};

    // We initialize PCK_EDGE_OFFSET_SEC0 mask to clear out all the datums in the row
    pack_edge_offset.f.mask        = 0x0;
    uint32_t row_set_mapping_1     = 0;
    uint32_t edge_offset_sec1_mask = 0;

    // PCK_EDGE_OFFSET_SEC1 mask will clear out all the datums in the row except the first one
    edge_offset_sec1_mask = 0x0001;

    // Packer 0 and 2 will use TILE_ROW_SET_MAPPING_1, while packer 1 and 3 will keep using
    // TILE_ROW_SET_MAPPING_0 configuration which is the default one
    pack_edge_offset.f.tile_row_set_select_pack0 = 1;
    pack_edge_offset.f.tile_row_set_select_pack2 = 1;

    // TILE_ROW_SET_MAPPING_1 configuration sets all rows to use PCK_EDGE_OFFSET_SEC1 mask
    row_set_mapping_1 = 0x55555555; // each packer packs 1x16 row

    // Initialize TMP registers with values we need to write in CFG registers
    TTI_SETDMAREG(0, LOWER_HALFWORD(pack_edge_offset.val), 0, LO_16(p_gpr_pack::TMP0));
    TTI_SETDMAREG(0, UPPER_HALFWORD(pack_edge_offset.val), 0, HI_16(p_gpr_pack::TMP0));
    TTI_SETDMAREG(0, LOWER_HALFWORD(edge_offset_sec1_mask), 0, LO_16(p_gpr_pack::TMP_LO));
    TTI_SETDMAREG(0, LOWER_HALFWORD(row_set_mapping_1), 0, LO_16(p_gpr_pack::TMP1));
    TTI_SETDMAREG(0, UPPER_HALFWORD(row_set_mapping_1), 0, HI_16(p_gpr_pack::TMP1));

    // Wait for packer to finish to avoid breaking its current configuration
    TTI_STALLWAIT(p_stall::STALL_CFG, p_stall::PACK);

    // Configure packer
    TTI_WRCFG(p_gpr_pack::TMP0, p_cfg::WRCFG_32b, PCK_EDGE_OFFSET_SEC0_mask_ADDR32);
    TTI_WRCFG(p_gpr_pack::TMP_LO, p_cfg::WRCFG_32b, PCK_EDGE_OFFSET_SEC1_mask_ADDR32);
    TTI_WRCFG(p_gpr_pack::TMP1, p_cfg::WRCFG_32b, TILE_ROW_SET_MAPPING_1_row_set_mapping_0_ADDR32);

    TTI_NOP;
    TTI_NOP;
}
