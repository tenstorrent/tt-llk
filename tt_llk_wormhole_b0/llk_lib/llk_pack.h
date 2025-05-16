// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cstdint>

#include "ckernel.h"
#include "ckernel_globals.h"
#include "ckernel_ops.h"
#include "ckernel_template.h"
#include "llk_defs.h"
#include "llk_pack_common.h"

#include "debug/dprint.h"

using namespace ckernel;
using namespace ckernel::packer;

template <bool untilize = false>
inline void _llk_pack_configure_addrmod_()
{
    addr_mod_pack_t {
        .y_src = {.incr = 15}, // 4-bit value so max is 15. incadcxy will increment it by 1
        .y_dst = {.incr = 1},
    }
        .set(ADDR_MOD_0);

    if constexpr (untilize)
    {
        addr_mod_pack_t {
            .y_src = {.incr = 1, .clr = 0, .cr = 1},
            .y_dst = {.incr = 1, .clr = 0, .cr = 0},
        }
            .set(ADDR_MOD_1);
    }
    else
    {
        addr_mod_pack_t {
            .y_src = {.incr = 0, .clr = 1, .cr = 0},
            .y_dst = {.incr = 0, .clr = 1, .cr = 0},
            .z_src = {.incr = 0, .clr = 0},
            .z_dst = {.incr = 0, .clr = 0},
        }
            .set(ADDR_MOD_1);
    }

    addr_mod_pack_t {
        .y_src = {.incr = 0, .clr = 1, .cr = 0},
        .y_dst = {.incr = 0, .clr = 0, .cr = 0},
    }
        .set(ADDR_MOD_2);

        addr_mod_pack_t {
        .y_src = {.incr = 0, .clr = 0, .cr = 0},
        .y_dst = {.incr = 1, .clr = 0, .cr = 0},
    }
        .set(ADDR_MOD_3);
}

template <bool untilize = false, bool zero_output = false, DstTileFaceLayout FaceLayout = DstTileFaceLayout::RowMajor, bool write_tile_header = true, bool compact = false, std::uint32_t block_ct_dim = 1>
inline void _llk_pack_mop_config_(
    const std::uint32_t pack_dst_format,
    const std::uint32_t face_r_dim = FACE_R_DIM,
    const std::uint32_t num_faces  = 4,
    const bool partial_face        = false,
    const bool narrow_tile         = false)
{
    static_assert(FaceLayout == DstTileFaceLayout::RowMajor, "FaceLayout must be RowMajor");

    const uint PACKCNT              = (partial_face && IS_BFP_FORMAT(pack_dst_format)) ? 1 : num_faces;
    constexpr uint MEGAROW          = 1;
    constexpr uint ZERO_OUTPUT_FLAG = zero_output ? p_pacr::P_ZERO_OUTPUT_ENABLED : p_pacr::P_ZERO_OUTPUT_DISABLED;
    if constexpr (compact)
    {
        // Not used anymore
        // There are 8 tiles in DEST, so we'll pack 8 full-tile rows
        constexpr uint MOP_INNER_LOOP = block_ct_dim;
        constexpr uint MOP_OUTER_LOOP = 1;
        
        ckernel::ckernel_template tmp(
            MOP_OUTER_LOOP, 
            MOP_INNER_LOOP, 
            /* 
            Each PACR interface packs first row of a face in a tile:
            PACR0 - TileX_Face0_Row0 
            PACR1 - TileX_Face1_Row0 
            PACR2 - TileX_Face2_Row0 
            PACR3 - TileX_Face3_Row0 
            ADDR_MOD_3 increments Y_Ch1 by 1, to point to the next row in L1, 
            and clears Y_Ch0 to always use the first row of each face in DEST
            Edge masking makes sure that PACR2/3 pack out only zeroes, although this is redundant
            */
            TT_OP_PACR(ADDR_MOD_3, p_pacr::P_ZERO_OUTPUT_DISABLED, PACK_SEL(4), 0, 0 /*MEGAROW*/, 0, 0),
            /* Then we move W_Ch0 pointer to the next tile */
            TT_OP_INCADCZW(p_setadc::PAC, 0, 0, 1, 0));
            /* Before the inner loop we tell the PACRs to pack 16 datums (one row of the face) */
            tmp.set_start_op(TT_OP_SETADCXX(p_setadc::PAC, 15, 0x0));
            tmp.set_end_ops(
                /* At the end of the inner loop we tell the PACRs to pack zeroes to the rest of each face */
                TT_OP_SETADCXX(p_setadc::PAC, (16 - block_ct_dim) * 16 - 1, 0x0),
                /* In the end, we finish the tile packing 128 zeroes in bottom four halves of the faces */
                TT_OP_PACR(ADDR_MOD_3, p_pacr::P_ZERO_OUTPUT_ENABLED, PACK_SEL(4), 0, 0 /*MEGAROW*/, 0, 1)
            );
            tmp.program(instrn_buffer);
    }
    else
    {
        constexpr uint MOP_INNER_LOOP = 1;
        if constexpr (!untilize)
        {
            constexpr uint MOP_OUTER_LOOP = 1;
    
            ckernel::ckernel_template tmp(MOP_OUTER_LOOP, MOP_INNER_LOOP, TT_OP_PACR(ADDR_MOD_1, ZERO_OUTPUT_FLAG, PACK_SEL(PACKCNT), 0, MEGAROW, 0, 1));
    
            if (partial_face && IS_BFP_FORMAT(pack_dst_format))
            {
                tmp.set_start_op(TT_OP_PACR(ADDR_MOD_0, ZERO_OUTPUT_FLAG, PACK_SEL(PACKCNT), 0, MEGAROW, 0, 0)); // Don't close the tile, point to the next face
                tmp.set_loop_op0(TT_OP_INCADCXY(p_setadc::PAC, 0, 0, 1, 0));                                     // Inc ch0_y+=1 (addr_mod_0 will increment by 15)
                tmp.set_loop_op1(TT_OP_PACR(ADDR_MOD_1, ZERO_OUTPUT_FLAG, PACK_SEL(PACKCNT), 0, MEGAROW, 0, 1)); // Close the tile
            }
            // Write header to l1
            if constexpr (write_tile_header)
            {
                tmp.set_end_op(TT_OP_STOREIND(1, 0, p_ind::LD_16B, LO_16(0), p_ind::INC_NONE, p_gpr_pack::TILE_HEADER, p_gpr_pack::OUTPUT_ADDR));
            }
    
            tmp.program(instrn_buffer);
        }
        else
        {
            const uint MOP_OUTER_LOOP = ((face_r_dim == 1) || narrow_tile) ? 1 : (face_r_dim >> 1);
    
            if ((face_r_dim == 1) || narrow_tile)
            {
                ckernel::ckernel_template tmp(MOP_OUTER_LOOP, MOP_INNER_LOOP, TT_OP_PACR(ADDR_MOD_0, ZERO_OUTPUT_FLAG, PACK_SEL(PACKCNT), 0, MEGAROW, 0, 1));
                tmp.program(instrn_buffer);
            }
            else
            {
                // Inc ch0_y+=1 (addr_mod_0 will increment by 15)
                ckernel::ckernel_template tmp(MOP_OUTER_LOOP, MOP_INNER_LOOP, TT_OP_INCADCXY(p_setadc::PAC, 0, 0, 1, 0));
                tmp.set_start_op(TT_OP_PACR(ADDR_MOD_0, ZERO_OUTPUT_FLAG, PACK_SEL(PACKCNT), 0, MEGAROW, 0, 0));
                tmp.set_end_op(TT_OP_PACR(ADDR_MOD_1, ZERO_OUTPUT_FLAG, PACK_SEL(PACKCNT), 0, MEGAROW, 0, 0));
                tmp.program(instrn_buffer);
            }
        }
    }
}

template <
    bool is_fp32_dest_acc_en     = false,
    bool is_tile_dim_reconfig_en = false,
    DstTileFaceLayout FaceLayout = DstTileFaceLayout::RowMajor,
    bool write_tile_header       = true>
inline void _llk_pack_reconfig_data_format_(
    const std::uint32_t pack_src_format,
    const std::uint32_t pack_dst_format,
    const std::uint32_t tile_size,
    const std::uint32_t face_r_dim = FACE_R_DIM,
    const std::uint32_t num_faces  = 4,
    const bool partial_face        = false,
    const bool narrow_tile         = false)
{
    reconfig_packer_data_format<is_fp32_dest_acc_en>(pack_src_format, pack_dst_format, tile_size, face_r_dim);

    if constexpr (is_tile_dim_reconfig_en)
    {
        _llk_pack_mop_config_<false, false, FaceLayout, write_tile_header>(pack_dst_format, face_r_dim, num_faces, partial_face, narrow_tile);
    }
}

template <bool untilize = false, bool is_fp32_dest_acc_en = false>
inline void _llk_pack_hw_configure_(
    const std::uint32_t pack_src_format,
    const std::uint32_t pack_dst_format,
    const std::uint32_t tile_size,
    const std::uint32_t face_r_dim  = FACE_R_DIM,
    const std::uint32_t num_faces   = 4,
    const bool partial_face         = false,
    const bool narrow_tile          = false,
    const std::uint32_t relu_config = 0)
{
    configure_pack<is_fp32_dest_acc_en, untilize>(pack_src_format, pack_dst_format, tile_size, face_r_dim, num_faces, partial_face, narrow_tile, relu_config);
}

template <bool untilize = false, PoolType type, ReduceDim dim, bool is_fp32_dest_acc_en = false>
inline void _llk_pack_reduce_hw_configure_(
    const std::uint32_t pack_src_format,
    const std::uint32_t pack_dst_format,
    const std::uint32_t tile_size,
    const std::uint32_t face_r_dim  = FACE_R_DIM,
    const std::uint32_t num_faces   = 4,
    const bool partial_face         = false,
    const bool narrow_tile          = false,
    const std::uint32_t relu_config = 0)
{
    configure_pack<is_fp32_dest_acc_en, untilize>(pack_src_format, pack_dst_format, tile_size, face_r_dim, num_faces, partial_face, narrow_tile, relu_config);

    volatile uint tt_reg_ptr *cfg = get_cfg_pointer();

    ckernel::packer::pck_edge_offset_u pack_edge_offset = {.val = 0};
    pack_edge_offset.f.mask                             = 0x0;
    if constexpr (dim == ReduceDim::REDUCE_ROW)
    {
        cfg[PCK_EDGE_OFFSET_SEC0_mask_ADDR32 + 1] = 0x0001;
        if constexpr (untilize)
        {
            pack_edge_offset.f.tile_row_set_select_pack0 = 1;
            pack_edge_offset.f.tile_row_set_select_pack1 = 1;
            pack_edge_offset.f.tile_row_set_select_pack2 = 1;
            pack_edge_offset.f.tile_row_set_select_pack3 = 1;
            if (narrow_tile)
            {
                cfg[TILE_ROW_SET_MAPPING_1_row_set_mapping_0_ADDR32] = 0x55555555; // each packer packs 1x16 row
            }
            else
            {
                cfg[TILE_ROW_SET_MAPPING_1_row_set_mapping_0_ADDR32] = 0x11111111; // each packer packs 1x32 row
            }
        }
        else
        {
            pack_edge_offset.f.tile_row_set_select_pack0 = 1;
            if (narrow_tile)
            {
                pack_edge_offset.f.tile_row_set_select_pack1 = 1;
            }
            else
            {
                pack_edge_offset.f.tile_row_set_select_pack2 = 1;
            }
            cfg[TILE_ROW_SET_MAPPING_1_row_set_mapping_0_ADDR32] = 0x55555555; // each packer packs 1x16 row
        }
        cfg[PCK_EDGE_OFFSET_SEC0_mask_ADDR32 + 0] = pack_edge_offset.val;
    }
    else if constexpr (dim == ReduceDim::REDUCE_SCALAR)
    {
        pack_edge_offset.f.tile_row_set_select_pack0         = 1;
        cfg[PCK_EDGE_OFFSET_SEC0_mask_ADDR32 + 0]            = pack_edge_offset.val;
        cfg[PCK_EDGE_OFFSET_SEC0_mask_ADDR32 + 1]            = 0x0001;
        cfg[TILE_ROW_SET_MAPPING_1_row_set_mapping_0_ADDR32] = 0x00000001;
    }
    else
    {
        pack_edge_offset.f.tile_row_set_select_pack0 = 1;
        pack_edge_offset.f.tile_row_set_select_pack1 = 1;
        cfg[PCK_EDGE_OFFSET_SEC0_mask_ADDR32 + 0]    = pack_edge_offset.val;
        cfg[PCK_EDGE_OFFSET_SEC0_mask_ADDR32 + 1]    = 0xffff;

        if constexpr (untilize)
        {
            cfg[TILE_ROW_SET_MAPPING_1_row_set_mapping_0_ADDR32] = 0x00000005; // Each packer packs 1x32 row
        }
        else
        {
            cfg[TILE_ROW_SET_MAPPING_1_row_set_mapping_0_ADDR32] = 0x00000001;
        }
    }
}

template <bool untilize = false, bool zero_output = false, DstTileFaceLayout FaceLayout = DstTileFaceLayout::RowMajor, bool write_tile_header = true, bool compact = false, uint32_t block_ct_dim = 1>
inline void _llk_pack_init_(
    const std::uint32_t pack_dst_format,
    const std::uint32_t face_r_dim   = FACE_R_DIM,
    const std::uint32_t num_faces    = 4,
    const bool partial_face          = false,
    const bool narrow_tile           = false)
{
    _llk_pack_configure_addrmod_<untilize>();

    _llk_pack_mop_config_<untilize, zero_output, FaceLayout, write_tile_header, compact, block_ct_dim>(pack_dst_format, face_r_dim, num_faces, partial_face, narrow_tile);
}

inline void _llk_pack_init_compact_(const std::uint32_t tile_index, const std::uint32_t address) {
    program_packer_destination(address);
    TT_SETADC(p_setadc::PAC, p_setadc::CH_0, p_setadc::SET_W, tile_index);
    TTI_SETADCXX(p_setadc::PAC, 15, 0x0);
}

template <DstSync Dst, bool untilize = false, bool is_fp32_dest_acc_en = false>
inline void _llk_pack_(const std::uint32_t tile_index, const std::uint32_t address)
{
    TTI_PACR(ADDR_MOD_3, p_pacr::P_ZERO_OUTPUT_DISABLED, PACK_SEL(4), 0, 0 /*MEGAROW*/, 0, 0);
}

template <uint32_t block_ct_dim = 1>
inline void _llk_pack_last_()
{
    if (block_ct_dim != 16) {
        TTI_SETADCXX(p_setadc::PAC, (16 - block_ct_dim) * 16 - 1, 0x0);
        TTI_PACR(ADDR_MOD_3, p_pacr::P_ZERO_OUTPUT_ENABLED, PACK_SEL(4), 0, 0 /*MEGAROW*/, 0, 1);
    }

}

#include "llk_pack_untilize.h"
