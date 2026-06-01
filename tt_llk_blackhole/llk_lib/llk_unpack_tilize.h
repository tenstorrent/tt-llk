// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cstdint>

#include "ckernel.h"
#include "ckernel_defs.h"
#include "ckernel_globals.h"
#include "ckernel_ops.h"
#include "ckernel_template.h"
#include "cunpack_common.h"
#include "llk_assert.h"
#include "llk_unpack_common.h"

using namespace ckernel;
using namespace ckernel::unpacker;

inline void _llk_unpack_tilize_mop_config_(
    [[maybe_unused]] const bool narrow_tile = false, const bool unpack_to_dest = false, const bool skip_bh_workaround = false)
{
    LLK_ASSERT(!narrow_tile, "narrow_tile: this parameter is unused");

    static constexpr std::uint32_t unpack_srca =
        TT_OP_UNPACR(SrcA, 0b1 /*Z inc*/, 0, 0, 0, 1 /* Set OvrdThreadId*/, 1 /*Set Dvalid*/, p_unpacr::RAREFYB_DISABLE, 0, 0, 0, 0, 1);
    static constexpr std::uint32_t unpack_srca_to_dest =
        TT_OP_UNPACR(0, 0b00010001 /*Z inc*/, 0, 0, 0, 1 /* Set OvrdThreadId*/, 0, p_unpacr::RAREFYB_DISABLE, 0, 0, 0, 0, 1);
    static constexpr std::uint32_t unpack_srcb_set_dvalid = TT_OP_UNPACR_NOP(SrcB, 0, 0, p_unpacr_nop::SET_DVALID, 0, 0, 0, 0, p_unpacr_nop::UNP_ZEROSRC);

    const std::uint32_t outerloop = (!skip_bh_workaround || (skip_bh_workaround && narrow_tile)) ? 1 : 2;
    const std::uint32_t innerloop = 1;

    if (skip_bh_workaround)
    {
        static constexpr std::uint32_t unpack_srcb_zerosrc = TT_OP_UNPACR_NOP(SrcB, 0, 0, p_unpacr_nop::UNP_NOP, 0, 0, 0, 0, p_unpacr_nop::UNP_ZEROSRC);
        ckernel_template tmp(outerloop, innerloop, unpack_srcb_zerosrc, unpack_srcb_set_dvalid);
        if (!unpack_to_dest)
        {
            tmp.set_start_op(unpack_srca);
        }
        tmp.program();
        return;
    }

    ckernel_template tmp(outerloop, innerloop, unpack_to_dest ? unpack_srca_to_dest : unpack_srcb_set_dvalid);

    if (!unpack_to_dest)
    {
        tmp.set_start_op(unpack_srca);
    }

    tmp.program();
}

inline void _llk_unpack_tilize_init_(
    const std::uint32_t unpack_src_format = 0,
    const std::uint32_t unpack_dst_format = 0,
    const std::uint32_t ct_dim            = 0,
    const std::uint32_t face_r_dim        = FACE_R_DIM,
    const bool narrow_tile                = false,
    const std::uint32_t num_faces         = 4)
{
    LLK_ASSERT(face_r_dim == 2 || face_r_dim == 4 || face_r_dim == 8 || face_r_dim == 16, "face_r_dim must be 2, 4, 8, or 16 for tilize");
    LLK_ASSERT(num_faces == 2 || num_faces == 4, "num_faces must be 2 or 4 for tilize");
    cfg_reg_rmw_tensix<THCON_SEC0_REG2_Haloize_mode_RMW>(0);

    const std::uint32_t block_c_dim = ct_dim * (narrow_tile ? FACE_C_DIM : TILE_C_DIM);

    // In case of 32-bit numbers, we have to unpack into dest register
    // For integers, always unpack to dest. For Float32, only if unpack_dst_format is Float32 (lossless tilize mode)
    auto unpack_source_format = static_cast<DataFormat>(unpack_src_format);
    auto unpack_dest_format   = static_cast<DataFormat>(unpack_dst_format);

    const bool unpack_to_dest =
        (unpack_source_format == DataFormat::UInt32) || (unpack_source_format == DataFormat::Int32) || (unpack_dest_format == DataFormat::Float32);

    LLK_ASSERT(
        is_unpacker_format_conversion_supported_dest(static_cast<DataFormat>(unpack_src_format), static_cast<DataFormat>(unpack_dst_format), unpack_to_dest),
        "Unsupported unpacker format conversion.");

    // Set face dim
    TT_SETADCXX(p_setadc::UNP_A, face_r_dim * FACE_C_DIM - 1, 0x0);

    // Override default settings to enable tilize mode
    unpack_config_u config   = {0};
    config.f.out_data_format = unpack_dst_format;
    config.f.throttle_mode   = 2;
    config.f.tileize_mode    = 1;
    config.f.shift_amount    = (SCALE_DATUM_SIZE(unpack_src_format, block_c_dim)) >> 4;

    TT_SETDMAREG(0, LOWER_HALFWORD(config.val[0]), 0, LO_16(p_gpr_unpack::TMP0));
    TT_SETDMAREG(0, UPPER_HALFWORD(config.val[0]), 0, HI_16(p_gpr_unpack::TMP0));
    TTI_STALLWAIT(p_stall::STALL_CFG, p_stall::THCON);
    TTI_WRCFG(p_gpr_unpack::TMP0, p_cfg::WRCFG_32b, THCON_SEC0_REG2_Out_data_format_ADDR32);

    const bool is_8bit_format = IS_8BIT_FORMAT(unpack_src_format);
    // 8bit datums do not need the blackhole workaround therefore we fallback to regular tilize operation like for wormhole.
    if (is_8bit_format)
    {
        TTI_WRCFG(p_gpr_unpack::FACE_DIM_1x16, 0, THCON_SEC0_REG5_Tile_x_dim_cntx0_ADDR32);
    }
    else
    {
        // below is the configuration for unpack for srca
        const std::uint32_t Tile_x_dim = face_r_dim * num_faces * FACE_C_DIM;
        const std::uint32_t Tile_z_dim = 1;
        cfg_reg_rmw_tensix<THCON_SEC0_REG5_Tile_x_dim_cntx0_ADDR32, 0, 0xffffffff>(Tile_x_dim | (Tile_x_dim << 16));
        // Set x-dim to cover entire tile (face_r_dim * num_faces * FACE_C_DIM)
        cfg_reg_rmw_tensix<THCON_SEC0_REG0_TileDescriptor_ADDR32, 0, 0xffff0000>(0 | (Tile_x_dim << 16));
        // Set z-dim to 1 as X dim is set to cover the entire tile, so no need to iterate over faces.
        cfg_reg_rmw_tensix<THCON_SEC0_REG0_TileDescriptor_ADDR32 + 1, 0, 0xffff0000>(0 | (Tile_z_dim << 16));

        // Set x-end for Unpackers to (face_r_dim * num_faces * FACE_C_DIM - 1)
        TT_SETADCXX(p_setadc::UNP0, Tile_x_dim - 1, 0x0);
    }

    _llk_unpack_tilize_mop_config_(narrow_tile, unpack_to_dest, is_8bit_format);
}

inline void _llk_unpack_tilize_(
    const std::uint32_t base_address,
    const std::uint32_t tile_index,
    std::uint32_t unpack_src_format                 = 0,
    std::uint32_t unpack_dst_format                 = 0,
    [[maybe_unused]] std::uint32_t block_ct_dim     = 0,
    [[maybe_unused]] const std::uint32_t face_r_dim = FACE_R_DIM,
    [[maybe_unused]] const std::uint32_t num_faces  = 4,
    const bool narrow_tile                          = false)
{
    LLK_ASSERT(block_ct_dim == 0, "block_ct_dim: this parameter is unused");
    LLK_ASSERT(num_faces == 2 || num_faces == 4, "num_faces must be 2 or 4 for tilize");

    volatile std::uint32_t tt_reg_ptr* cfg = get_cfg_pointer(); // get pointer to registers for current state ID

    // In case of 32-bit numbers, we have to unpack into dest register
    // For integers, always unpack to dest. For Float32, only if unpack_dst_format is Float32 (lossless tilize mode)
    auto unpack_source_format = static_cast<DataFormat>(unpack_src_format);
    auto unpack_dest_format   = static_cast<DataFormat>(unpack_dst_format);

    const bool unpack_to_dest =
        (unpack_source_format == DataFormat::UInt32) || (unpack_source_format == DataFormat::Int32) || (unpack_dest_format == DataFormat::Float32);

    std::uint32_t top_face_offset_address = SCALE_DATUM_SIZE(unpack_src_format, tile_index) << (narrow_tile ? 0 : 1);

    // 8bit datums do not need the blackhole workaround therefore we fallback to regular tilize operation like for wormhole.
    if (IS_8BIT_FORMAT(unpack_src_format))
    {
        // Each iteration unpacks 2 face_r_dimx16 faces (1st 0,1 2nd 2,3 unless tile is <=16x32)
        // For narrow tile we unpack 1 face in each iteration
        // Offset address is in 16B words
        // Datum count = tile_index*face_r_dim (/16 to get word count)

        const auto config_vec                 = read_unpack_config();
        const std::uint32_t shift_amount      = config_vec[0].shift_amount;
        std::uint32_t bot_face_offset_address = shift_amount * face_r_dim; // bytes for bottom faces

        // Program srcA and srcB base addresses
        std::uint32_t num_loops = narrow_tile ? 2 : num_faces / 2;

        volatile std::uint32_t tt_reg_ptr* cfg = get_cfg_pointer(); // get pointer to registers for current state ID

        for (std::uint32_t n = 0; n < num_loops; n++)
        {
            std::uint32_t address = base_address + top_face_offset_address + ((n == 1) ? bot_face_offset_address : 0);

            // Clear z/w start counters
            TTI_SETADCZW(0b001, 0, 0, 0, 0, 0b1111);

            // Wait for free context
            wait_for_next_context(2);

            // Validate and configure address
            _llk_unpack_configure_single_address_(address, cfg);

            // Trisc::SEMPOST for context acquire
            semaphore_post(semaphore::UNPACK_SYNC);

            // Stall unpacker until pending CFG writes from Trisc have completed
            TTI_STALLWAIT(p_stall::STALL_UNPACK, p_stall::TRISC_CFG);

            // Run MOP
            ckernel::ckernel_template::run();

            // T6::SEMGET for context release
            t6_semaphore_get(semaphore::UNPACK_SYNC);

            // Switch unpacker config context
            switch_config_context(unp_cfg_context);
        }
    }
    else
    {
        // Program srcA and srcB base addresses
        // FIXME MT: This should be revisited for narrow tiles
        // std::uint32_t num_loops = narrow_tile ? 2 : num_faces/2;

        std::uint32_t address = base_address + top_face_offset_address;
        LLK_ASSERT(is_valid_L1_address(address), "L1 base_address must be in valid L1 memory region");

        // Clear z/w start counters
        TTI_SETADCZW(0b001, 0, 0, 0, 0, 0b1111);

        // Wait for free context
        wait_for_next_context(2);

        // Get tile address
        if (0 == unp_cfg_context)
        {
            cfg[THCON_SEC0_REG3_Base_address_ADDR32] = address;
        }
        else
        {
            cfg[THCON_SEC0_REG3_Base_cntx1_address_ADDR32] = address;
        }

        // Trisc::SEMPOST for context acquire
        semaphore_post(semaphore::UNPACK_SYNC);

        if (unpack_to_dest)
        {
            // Unpack to dest
            set_dst_write_addr(unp_cfg_context, unpack_src_format);
            wait_for_dest_available();
        }

        // Stall unpacker until pending CFG writes from Trisc have completed
        TTI_STALLWAIT(p_stall::STALL_UNPACK, p_stall::TRISC_CFG);

        // Run MOP
        ckernel::ckernel_template::run();

        // T6::SEMGET for context release
        t6_semaphore_get(semaphore::UNPACK_SYNC);

        if (unpack_to_dest)
        {
            unpack_to_dest_tile_done(unp_cfg_context);
        }

        // Switch unpacker config context
        switch_config_context(unp_cfg_context);
    }
}

inline void _llk_unpack_tilize_uninit_(const std::uint32_t unpack_dst_format, const std::uint32_t num_faces, const std::uint32_t face_r_dim)
{
    TTI_STALLWAIT(p_stall::STALL_THCON, p_stall::UNPACK);
    LLK_ASSERT(num_faces == 1 || num_faces == 2 || num_faces == 4, "num_faces must be 1, 2, or 4");
    // Revert X dim value to default.
    // TODO NC: Issue tt-llk#1036 will make this transient
    TT_SETADCXX(p_setadc::UNP_A, face_r_dim * FACE_C_DIM - 1, 0x0);
    TT_SETADCXX(p_setadc::UNP_B, face_r_dim * FACE_C_DIM - 1, 0x0);

    // Revert Z dim value back to default.
