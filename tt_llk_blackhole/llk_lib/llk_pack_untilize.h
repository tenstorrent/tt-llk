// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cstdint>

#include "ckernel.h"
#include "ckernel_globals.h"
#include "ckernel_ops.h"
#include "ckernel_template.h"
#include "llk_assert.h"
#include "llk_defs.h"
#include "llk_memory_checks.h"
#include "llk_pack_common.h"
#define USE_CFGSHIFTMASK

using namespace ckernel;
using namespace ckernel::packer;

inline void _llk_pack_untilize_configure_addrmod_()
{
    addr_mod_pack_t {
        .y_src = {.incr = 0, .clr = 0},
    }
        .set(ADDR_MOD_0);
}

/*
block_ct_dim represents the number of input tiles in a block.
dense is used with num_faces == 2 and even block_ct_dim, where two 16x32 (or smaller) tiles are packed in a single 32x32 tile region in dest.
*/
template <std::uint32_t block_ct_dim, bool narrow_row = false, bool dense = false>
inline void _llk_pack_untilize_mop_config_(const std::uint32_t face_r_dim = FACE_R_DIM, const std::uint32_t num_faces = 4)
{
    static_assert(!dense || (block_ct_dim % 2 == 0), "block_ct_dim must be even when dense");
    static_assert(!dense || (!narrow_row), "narrow_row must be false when dense");
    LLK_ASSERT(num_faces == 1 || num_faces == 2 || num_faces == 4, "num_faces must be 1, 2, or 4");
    LLK_ASSERT(!dense || (num_faces == 2), "num_faces must be 2 when dense");
    /*
    Outer loop iterates over the rows in the block, while the inner loop iterates
    over each tile in the block.
    When dense, we use all 4 interfaces to pack out a row each from 4 faces (2 tiles) that end up contiguous in L1
    because offsets align well and it improves perf, thus we halve the number of mop inner loops.
    */
    constexpr std::uint32_t MOP_INNER_LOOP = dense ? block_ct_dim / 2 : block_ct_dim;
    const std::uint32_t MOP_OUTER_LOOP     = face_r_dim;

    // For narrow row, the faces are stored in the first column of the tile, therefore requiring only one packer interface.
    const std::uint32_t PACK_INTF_SEL = (dense)                          ? p_pacr::ALL_INTF_ACTIVE
                                        : (narrow_row || num_faces == 1) ? p_pacr::SINGLE_INTF_ACTIVE
                                                                         : p_pacr::TWO_INTFS_ACTIVE;
    /*
    When using DST_STRIDED_MODE, each packer interface has a stride of 16*block_size,
    where block_size is set to be the size of a row within face.
    Each PACR instruction packs 2x16 datums if (num_faces>1), meaning that it would
    pack out one row for each tile in the block.
    In the inner loop, for each tile, the rows that get packed from dest register
    in the first outer loop iteration are:
    tile 0: row 0, row 16
    tile 1: row 64, row 80
    tile block_ct_dim-1: row 64*(block_ct_dim-1), row 64*(block_ct_dim-1)+16
    This processes is repeated for each row of the block in dest.
    */
    ckernel::ckernel_template tmp(
        MOP_OUTER_LOOP,
        MOP_INNER_LOOP,
        TT_OP_INCADCZW(p_setadc::PAC, 0, 0, 1, 0), // w cnt points to the next tile
        TT_OP_PACR(
            p_pacr::CFG_CTXT_0,
            p_pacr::NO_ROW_PAD_ZERO,
            p_pacr::DST_ACCESS_STRIDED_MODE,
            ADDR_MOD_0,
            p_pacr::ADDR_CNT_CTXT_0,
            0,
            PACK_INTF_SEL,
            0,
            0,
            p_pacr::NO_CTXT_CTRL,
            0,
            0));

    /*
    Since there are two inner loop operations, the instruction set by set_last_inner_loop_instr
    will replace the second inner loop operation (in the last iteration, call the PACR instruction
    with the Last bit set to 1 instead of 0 to close the row).
    The W counter CR shadow (W_Cr) is established by TT_SETADC(...SET_W...) in _llk_pack_untilize_
    before run() is called; the SETADCZW there only initializes Z.
    ADDRCRZW with increment 0 resets W to the stored W_Cr value at the start of each outer loop
    iteration (row), without needing tile_dst_offset baked into this MOP template.
    */
    tmp.set_start_op(TT_OP_ADDRCRZW(p_setadc::PAC, 0, 0, 0, 0, 0b0010 /*CH0_W*/)); // W = W_Cr (restore W to start of block)

#if defined(USE_CFGSHIFTMASK)
    tmp.set_end_ops(TT_OP_INCADCXY(p_setadc::PAC, 0, 0, 1, 0), TT_OP_CFGSHIFTMASK(1, 0b011, 32 - 1, 0, 0, THCON_SEC0_REG1_L1_Dest_addr_ADDR32));
#else
    const std::uint32_t replay_buf_len = 4;
    load_replay_buf(
        ckernel::packer::replay_buf_offset,
        replay_buf_len,
        []
        {
            // Update L1 address
            TTI_ADDDMAREG(0, p_gpr_pack::OUTPUT_ADDR, p_gpr_pack::OUTPUT_ADDR, p_gpr_pack::OUTPUT_ADDR_OFFSET);
            TTI_STALLWAIT(p_stall::STALL_CFG, p_stall::THCON);
            TTI_WRCFG(p_gpr_pack::OUTPUT_ADDR, 0, THCON_SEC0_REG1_L1_Dest_addr_ADDR32);
            TTI_NOP;
        });

    // After the inner loop finishes, move to the next row in the block, and update L1 address.
    tmp.set_end_ops(TT_OP_INCADCXY(p_setadc::PAC, 0, 0, 1, 0), lltt::replay_insn(ckernel::packer::replay_buf_offset, replay_buf_len));
#endif

    /*
    Close the row in the block by setting the Last bit to 1 in the last inner loop instruction.
    This will allow the L1 address to be updated for the next row.
    Revisit after #22820 to convert last_loop_op to constexpr.
    */
    std::uint32_t last_loop_op = TT_OP_PACR(
        p_pacr::CFG_CTXT_0,
        p_pacr::NO_ROW_PAD_ZERO,
        p_pacr::DST_ACCESS_STRIDED_MODE,
        ADDR_MOD_0,
        p_pacr::ADDR_CNT_CTXT_0,
        0,
        PACK_INTF_SEL,
        0,
        0,
        p_pacr::NO_CTXT_CTRL,
        0,
        1);

    tmp.set_last_inner_loop_instr(last_loop_op);

    tmp.set_last_outer_loop_instr(last_loop_op);

    tmp.program();
}

template <
    std::uint32_t block_ct_dim,
    std::uint32_t full_ct_dim    = block_ct_dim,
    bool narrow_row              = false,
    std::uint32_t row_num_datums = TILE_C_DIM,
    bool dense                   = false>
inline void _llk_pack_untilize_init_(
    const std::uint32_t pack_src_format, const std::uint32_t pack_dst_format, const std::uint32_t face_r_dim = FACE_R_DIM, const std::uint32_t num_faces = 4)
{
    static_assert(block_ct_dim <= (dense ? 16 : 8), "block_ct_dim must be <= 8 when not dense, <= 16 when dense");
    static_assert(!dense || (block_ct_dim % 2 == 0), "block_ct_dim must be even when dense");
    static_assert(!dense || (!narrow_row), "narrow_row must be false when dense");
    static_assert(full_ct_dim % block_ct_dim == 0, "full_ct_dim must be divisible by block_ct_dim");
    LLK_ASSERT(num_faces == 1 || num_faces == 2 || num_faces == 4, "num_faces must be 1, 2, or 4");
    LLK_ASSERT(!dense || (num_faces == 2), "num_faces must be 2 when dense");

    if constexpr (narrow_row)
    {
        // Changed to check against TILE_C_DIM instead of FACE_C_DIM until tt-metal#24095 is investigated.
        static_assert(row_num_datums < TILE_C_DIM, "row_num_datums must be set to less than TILE_C_DIM for narrow_row packing");
    }

    _llk_pack_untilize_configure_addrmod_();

    _llk_pack_untilize_mop_config_<block_ct_dim, narrow_row, dense>(face_r_dim, num_faces);

    // Set CH0 Zstride = 2x16x16 faces, .z_src = {.incr = 1} jumps 2 faces
    std::uint32_t x_stride       = (pack_src_format & 0x3) == to_underlying(DataFormat::Float32)   ? 4
                                   : (pack_src_format & 0x3) == to_underlying(DataFormat::Float16) ? 2
                                                                                                   : 1;
    std::uint32_t y_stride       = FACE_C_DIM * x_stride;
    const std::uint32_t z_stride = 2 * face_r_dim * y_stride;
    cfg_reg_rmw_tensix<PCK0_ADDR_CTRL_ZW_REG_0_Zstride_RMW>(z_stride);

    std::uint32_t output_addr_offset;
    if constexpr (narrow_row)
    {
        output_addr_offset = SCALE_DATUM_SIZE(pack_dst_format, full_ct_dim * row_num_datums);
    }
    else
    {
        output_addr_offset = SCALE_DATUM_SIZE(pack_dst_format, full_ct_dim * ((num_faces == 1) ? 1 : 2) * FACE_C_DIM);
    }

    // Store 16B aligned row offset address
    TT_SETDMAREG(0, LOWER_HALFWORD(output_addr_offset / 16), 0, LO_16(p_gpr_pack::OUTPUT_ADDR_OFFSET));

#if defined(USE_CFGSHIFTMASK)
    // Prepare value to be used by CFGSHIFTMASK
    TTI_STALLWAIT(p_stall::STALL_CFG, p_stall::THCON); // TODO SK check if this can be replaced with two SETDMAREG calls ?
    TTI_WRCFG(p_gpr_pack::OUTPUT_ADDR_OFFSET, 0, SCRATCH_SEC0_val_ADDR32);
    // TTI_NOP; TODO SK check if required
#endif

    // Always include setup calls for safety (as recommended by maintainer)
    // Program packer to pack out the correct number of datums per row
    if constexpr (narrow_row)
    {
        TTI_SETADCXX(p_setadc::PAC, row_num_datums - 1, 0x0);
    }
    else
    {
        TTI_SETADCXX(p_setadc::PAC, FACE_C_DIM - 1, 0x0);
    }
}

template <
    std::uint32_t block_ct_dim,
    std::uint32_t full_ct_dim        = block_ct_dim,
    bool narrow_row                  = false,
    std::uint32_t tile_dst_ct_offset = 0,
    bool dense                       = false>
inline void _llk_pack_untilize_(
    const std::uint32_t address,
    [[maybe_unused]] const std::uint32_t pack_dst_format,
    [[maybe_unused]] const std::uint32_t face_r_dim = FACE_R_DIM,
    const std::uint32_t num_faces                   = 4,
    const std::uint32_t tile_dst_rt_offset          = 0)
{
    static_assert(block_ct_dim <= (dense ? 16 : 8), "block_ct_dim must be <= 8 when not dense, <= 16 when dense");
    static_assert(!dense || (block_ct_dim % 2 == 0), "block_ct_dim must be even when dense");
    static_assert(!dense || (!narrow_row), "narrow_row must be false when dense");
    LLK_ASSERT(num_faces == 1 || num_faces == 2 || num_faces == 4, "num_faces must be 1, 2, or 4");
    LLK_ASSERT(!dense || (num_faces == 2), "num_faces must be 2 when dense");

    /*
    full_ct_dim represents the number of input tiles.
    For input widths greater than 8 tiles, input is split into blocks of equal sizes,
    each block the size of block_ct_dim. This function is called for each block.
    */
    // program_packer_untilized_destination<block_ct_dim, full_ct_dim, diagonal>(address, pack_dst_format);
    program_packer_destination(address);
    const std::uint32_t num_faces_per_rdim_tile = (num_faces > 2) ? 2 : 1;

    const std::uint32_t tile_dst_offset = tile_dst_ct_offset + tile_dst_rt_offset;
    // Set W = (15 + tile_dst_offset) & 0xF, establishing the W_Cr shadow so that ADDRCRZW in the
    // MOP START_OP resets W to this value at the start of each outer loop iteration (row).
    // The first INCADCZW in the inner loop then advances W to tile_dst_offset for the first tile.
    // SETADCZW's Ch0_W field is only 3 bits (0-7), so SETADC is used to carry the full 4-bit W value.
    TTI_SETADCZW(p_setadc::PAC, 0, 0, 0, 0, 0b0001);                                         // reset ch0 z counter
    TT_SETADC(p_setadc::PAC, p_setadc::CH_0, p_setadc::SET_W, (15 + tile_dst_offset) & 0xF); // set ch0 w counter, establishing W_Cr
    TTI_SETADCXY(p_setadc::PAC, 0, 0, 0, 0, 0b0011);                                         // reset ch0 xy counters

    // Iterate over top, then over bottom faces in the block (if num_faces > 2)
    for (std::uint32_t face = 0; face < num_faces_per_rdim_tile; face++)
    {
        ckernel::ckernel_template::run();

        TTI_INCADCZW(p_setadc::PAC, 0, 0, 0, 1);         // z cnt increments by 2xface_r_dimxFACE_C_DIM
        TTI_SETADCXY(p_setadc::PAC, 0, 0, 0, 0, 0b0010); // reset ch0_y counters
    }

    TTI_SETADCZW(p_setadc::PAC, 0, 0, 0, 0, 0b0101); // reset z counters
    set_dst_write_addr(tile_dst_offset);             // reset w counter
}

inline void _llk_pack_untilize_uninit_(const std::uint32_t pack_src_format)
{
    TTI_STALLWAIT(p_stall::STALL_CFG, p_stall::PACK);
    const std::uint32_t z_stride = SCALE_DATUM_SIZE(pack_src_format, FACE_R_DIM * FACE_C_DIM);
    cfg_reg_rmw_tensix<PCK0_ADDR_CTRL_ZW_REG_0_Zstride_RMW>(z_stride);
}

// ============================================================================
// 4-INTERFACE OPTIMIZED UNTILIZE - APPROACH 1
// ============================================================================
/*
 * Optimized untilize that uses all 4 packer interfaces for tile pairs.
 *
 * Key optimization: Pack 2 tiles simultaneously using 4 interfaces (64 datums/PACR vs 32).
 *
 * Requirements:
 * - Dest layout must be interleaved: T0F0, T0F1, T1F0, T1F1, T0F2, T0F3, T1F2, T1F3, ...
 * - block_ct_dim can be any value; odd tiles handled explicitly without MOP
 *
 * Performance:
 * - 2x bandwidth for tile pairs (all 4 interfaces active)
 * - Minimal overhead for odd remainder tile (explicit PACR loop)
 * - Zero MOP reprogramming cost
 *
 * See UNTILIZE_4INTF_OPTIMIZATION.md for detailed explanation
 */

template <std::uint32_t block_ct_dim>
inline void _llk_pack_untilize_4intf_mop_config_(const std::uint32_t face_r_dim = FACE_R_DIM, const std::uint32_t num_faces = 4)
{
    LLK_ASSERT(num_faces == 4, "4-interface optimization currently only supports num_faces == 4");

    // Configure MOP for tile pairs only (4 interfaces)
    // Remainder tile (if odd) is handled explicitly outside MOP
    constexpr std::uint32_t TILE_PAIRS = block_ct_dim / 2;

    if constexpr (TILE_PAIRS == 0)
    {
        // Single tile case: fall back to standard 2-interface MOP
        // Currently this is handled without MOP, check what would be preferable solution
        return;
    }

    /*
     * MOP configuration for tile pairs with 4 interfaces
     *
     * With tile-level interleaved dest layout:
     * For block_ct_dim=2:
     *   - W=0: 32x32 tile with F0=T0F0, F1=T0F1, F2=T1F0, F3=T1F1 (top faces)
     *   - W=1: 32x32 tile with F0=T0F2, F1=T0F3, F2=T1F2, F3=T1F3 (bottom faces)
     * For block_ct_dim=4:
     *   - W=0: T0F0, T0F1, T1F0, T1F1 (pair 0 top)
     *   - W=1: T0F2, T0F3, T1F2, T1F3 (pair 0 bot)
     *   - W=2: T2F0, T2F1, T3F0, T3F1 (pair 1 top)
     *   - W=3: T2F2, T2F3, T3F2, T3F3 (pair 1 bot)
     *
     * Packer stride is 256 datums (1 face), so 4 interfaces read 4 consecutive faces from one tile.
     * MOP processes even W positions (top faces) or odd W positions (bottom faces).
     * W counter increments by 2 to skip to next tile pair.
     */
    const std::uint32_t MOP_OUTER_LOOP = face_r_dim; // Rows per face (16 for 32x32 tiles)
    const std::uint32_t MOP_INNER_LOOP = TILE_PAIRS; // Number of tile pairs

    ckernel::ckernel_template tmp(
        MOP_OUTER_LOOP,
        MOP_INNER_LOOP,
        TT_OP_INCADCZW(p_setadc::PAC, 0, 0, 2, 0), // w cnt increments by 2 (skip to next tile pair)
        TT_OP_PACR(
            p_pacr::CFG_CTXT_0,
            p_pacr::NO_ROW_PAD_ZERO,
            p_pacr::DST_ACCESS_STRIDED_MODE,
            ADDR_MOD_0,
            p_pacr::ADDR_CNT_CTXT_0,
            0,
            p_pacr::ALL_INTF_ACTIVE, // All 4 interfaces active
            0,
            0,
            p_pacr::NO_CTXT_CTRL,
            0,
            0));

    // START_OP: Restore W counter at the beginning of each row
    tmp.set_start_op(TT_OP_ADDRCRZW(p_setadc::PAC, 0, 0, 0, 0, 0b0010 /*CH0_W*/));

#if defined(USE_CFGSHIFTMASK)
    tmp.set_end_ops(TT_OP_INCADCXY(p_setadc::PAC, 0, 0, 1, 0), TT_OP_CFGSHIFTMASK(1, 0b011, 32 - 1, 0, 0, THCON_SEC0_REG1_L1_Dest_addr_ADDR32));
#else
    // Replay buffer for L1 address updates
    const std::uint32_t replay_buf_len = 4;
    load_replay_buf(
        ckernel::packer::replay_buf_offset,
        replay_buf_len,
        []
        {
            TTI_ADDDMAREG(0, p_gpr_pack::OUTPUT_ADDR, p_gpr_pack::OUTPUT_ADDR, p_gpr_pack::OUTPUT_ADDR_OFFSET);
            TTI_STALLWAIT(p_stall::STALL_CFG, p_stall::THCON);
            TTI_WRCFG(p_gpr_pack::OUTPUT_ADDR, 0, THCON_SEC0_REG1_L1_Dest_addr_ADDR32);
            TTI_NOP;
        });

    // END_OPS: After completing all tile pairs in a row, move to next row and update L1 address
    tmp.set_end_ops(TT_OP_INCADCXY(p_setadc::PAC, 0, 0, 1, 0), lltt::replay_insn(ckernel::packer::replay_buf_offset, replay_buf_len));
#endif

    // Last iteration: close the row with Last bit = 1
    std::uint32_t last_loop_op = TT_OP_PACR(
        p_pacr::CFG_CTXT_0,
        p_pacr::NO_ROW_PAD_ZERO,
        p_pacr::DST_ACCESS_STRIDED_MODE,
        ADDR_MOD_0,
        p_pacr::ADDR_CNT_CTXT_0,
        0,
        p_pacr::ALL_INTF_ACTIVE,
        0,
        0,
        p_pacr::NO_CTXT_CTRL,
        0,
        1);

    tmp.set_last_inner_loop_instr(last_loop_op);
    tmp.set_last_outer_loop_instr(last_loop_op);

    tmp.program();
}

template <std::uint32_t block_ct_dim, std::uint32_t full_ct_dim = block_ct_dim, std::uint32_t row_num_datums = TILE_C_DIM>
inline void _llk_pack_untilize_4intf_init_(
    const std::uint32_t pack_src_format, const std::uint32_t pack_dst_format, const std::uint32_t face_r_dim = FACE_R_DIM, const std::uint32_t num_faces = 4)
{
    static_assert(block_ct_dim <= 16, "block_ct_dim must be <= 16 for 4-interface mode");
    static_assert(full_ct_dim % block_ct_dim == 0, "full_ct_dim must be divisible by block_ct_dim");
    LLK_ASSERT(num_faces == 4, "4-interface optimization currently only supports num_faces == 4");

    _llk_pack_untilize_configure_addrmod_();

    _llk_pack_untilize_4intf_mop_config_<block_ct_dim>(face_r_dim, num_faces);

    // Z-stride configuration for face transitions
    std::uint32_t x_stride       = (pack_src_format & 0x3) == to_underlying(DataFormat::Float32)   ? 4
                                   : (pack_src_format & 0x3) == to_underlying(DataFormat::Float16) ? 2
                                                                                                   : 1;
    std::uint32_t y_stride       = FACE_C_DIM * x_stride;
    const std::uint32_t z_stride = 2 * face_r_dim * y_stride; // Jump 2 faces at a time
    cfg_reg_rmw_tensix<PCK0_ADDR_CTRL_ZW_REG_0_Zstride_RMW>(z_stride);

    // OUTPUT_ADDR_OFFSET: advance by full row width in L1
    const std::uint32_t output_addr_offset = SCALE_DATUM_SIZE(pack_dst_format, full_ct_dim * 2 * FACE_C_DIM);
    TT_SETDMAREG(0, LOWER_HALFWORD(output_addr_offset / 16), 0, LO_16(p_gpr_pack::OUTPUT_ADDR_OFFSET));

#if defined(USE_CFGSHIFTMASK)
    // Prepare value to be used by CFGSHIFTMASK
    TTI_STALLWAIT(p_stall::STALL_CFG, p_stall::THCON); // TODO SK check if this can be replaced with two SETDMAREG calls ?
    TTI_WRCFG(p_gpr_pack::OUTPUT_ADDR_OFFSET, 0, SCRATCH_SEC0_val_ADDR32);
    // TTI_NOP; TODO SK check if required
#endif

    // Program packer to pack out correct number of datums per row
    TTI_SETADCXX(p_setadc::PAC, FACE_C_DIM - 1, 0x0);
}

template <std::uint32_t block_ct_dim, std::uint32_t full_ct_dim = block_ct_dim, std::uint32_t tile_dst_ct_offset = 0>
inline void _llk_pack_untilize_4intf_(
    const std::uint32_t address,
    [[maybe_unused]] const std::uint32_t pack_dst_format,
    const std::uint32_t face_r_dim         = FACE_R_DIM,
    const std::uint32_t num_faces          = 4,
    const std::uint32_t tile_dst_rt_offset = 0)
{
    static_assert(block_ct_dim <= 16, "block_ct_dim must be <= 16 for 4-interface mode");
    LLK_ASSERT(num_faces == 4, "4-interface optimization currently only supports num_faces == 4");

    constexpr std::uint32_t TILE_PAIRS          = block_ct_dim / 2;
    constexpr bool HAS_REMAINDER                = (block_ct_dim % 2) == 1;
    const std::uint32_t num_faces_per_rdim_tile = 2; // Top faces (F0/F1), then bottom faces (F2/F3)

    program_packer_destination(address);

    const std::uint32_t tile_dst_offset = tile_dst_ct_offset + tile_dst_rt_offset;

    // ========== PHASE 1: Process tile pairs using MOP (4 interfaces) ==========
    if constexpr (TILE_PAIRS > 0)
    {
        /*
         * With tile-level interleaved layout:
         * - face_pair=0 (top): Process W=0,2,4,... (even positions - tiles with top faces)
         * - face_pair=1 (bot): Process W=1,3,5,... (odd positions - tiles with bottom faces)
         *
         * Each tile at W position contains 4 faces spatially arranged:
         * - W=even: F0=T(2n)F0, F1=T(2n)F1, F2=T(2n+1)F0, F3=T(2n+1)F1
         * - W=odd:  F0=T(2n)F2, F1=T(2n)F3, F2=T(2n+1)F2, F3=T(2n+1)F3
         *
         * MOP increments W by 2, so W_Cr must be set for first INCADCZW to land on target:
         * - face_pair=0: W_Cr = (tile_dst_offset + 0 + 14) & 0xF → W starts at even
         * - face_pair=1: W_Cr = (tile_dst_offset + 1 + 14) & 0xF → W starts at odd
         */

        // Execute MOP for top faces (F0/F1), then bottom faces (F2/F3)
        for (std::uint32_t face_pair = 0; face_pair < num_faces_per_rdim_tile; face_pair++)
        {
            // Calculate starting W for this face_pair (0 for even, 1 for odd)
            const std::uint32_t w_start = tile_dst_offset + face_pair;
            const std::uint32_t w_cr    = (w_start + 14) & 0xF; // -2 mod 16 (since first INCADCZW adds 2)

            TTI_SETADCZW(p_setadc::PAC, 0, 0, 0, 0, 0b0001);                 // Reset Z
            TT_SETADC(p_setadc::PAC, p_setadc::CH_0, p_setadc::SET_W, w_cr); // Set W_Cr for this face_pair
            TTI_SETADCXY(p_setadc::PAC, 0, 0, 0, 0, 0b0011);                 // Reset X, Y

            ckernel::ckernel_template::run();

            TTI_SETADCXY(p_setadc::PAC, 0, 0, 0, 0, 0b0010); // Reset Y for next iteration
        }
    }

    // ========== PHASE 2: Process remainder tile explicitly (2 interfaces, no MOP) ==========
    if constexpr (HAS_REMAINDER)
    {
        /*
         * Remainder tile L1 address calculation with hardware address transformation:
         *
         * Physical offset: TILE_PAIRS × 4 faces × FACE_C_DIM datums × 2 bytes = TILE_PAIRS × 128 bytes
         * Hardware offset: physical_offset / 16 (no -1, since this is added to already-transformed base)
         *
         * Example for block_ct_dim=3 (TILE_PAIRS=1):
         *   - Physical offset: 1 × 128 = 0x80
         *   - Hardware offset: 0x80 / 16 = 8
         *   - If base address = 0x23FF (L1_ADDRESS(0x24000))
         *   - Remainder address = 0x23FF + 8 = 0x2407 = L1_ADDRESS(0x24080) ✓
         */
        constexpr std::uint32_t remainder_l1_offset_bytes = TILE_PAIRS * 4 * FACE_C_DIM * 2;
        constexpr std::uint32_t remainder_l1_offset_hw    = remainder_l1_offset_bytes / 16;

        const std::uint32_t remainder_base_address = address + remainder_l1_offset_hw;
        program_packer_destination(remainder_base_address);

        // Dest register W position for remainder tile (after all tile pairs)
        const std::uint32_t remainder_w_pos = tile_dst_offset + TILE_PAIRS * 2;

        // Set dest register counters remider tile
        TTI_SETADCZW(p_setadc::PAC, 0, 0, 0, 0, 0b0001);                                  // Reset Z
        TT_SETADC(p_setadc::PAC, p_setadc::CH_0, p_setadc::SET_W, remainder_w_pos & 0xF); // Set W to remainder tile position
        TTI_SETADCXY(p_setadc::PAC, 0, 0, 0, 0, 0b0011);                                  // Reset X and Y

        // Process both face pairs (top and bottom faces) of the remainder tile
        for (std::uint32_t face_pair = 0; face_pair < num_faces_per_rdim_tile; face_pair++)
        {
            // Pack all 16 rows for this face pair
            // Follow MOP pattern: PACR → INCADCXY (Y increment) → Address update
            for (std::uint32_t row = 0; row < face_r_dim; row++)
            {
                // Pack one row (32 datums from 2 faces)
                TTI_PACR(
                    p_pacr::CFG_CTXT_0,
                    p_pacr::NO_ROW_PAD_ZERO,
                    p_pacr::DST_ACCESS_STRIDED_MODE,
                    ADDR_MOD_0,
                    p_pacr::ADDR_CNT_CTXT_0,
                    0,
                    p_pacr::TWO_INTFS_ACTIVE, // 2 interfaces for single tile
                    0,
                    0,
                    p_pacr::NO_CTXT_CTRL,
                    0,
                    1); // row end, set Last bit to 1 to trigger L1 address update in replay buffer

                // After packing, move to next row (same as MOP END_OPS pattern)
                TTI_INCADCXY(p_setadc::PAC, 0, 0, 1, 0); // Increment Y counter
#if defined(USE_CFGSHIFTMASK)
                TTI_CFGSHIFTMASK(1, 0b011, 32 - 1, 0, 0, THCON_SEC0_REG1_L1_Dest_addr_ADDR32);
                TTI_NOP;
#else
                TTI_ADDDMAREG(0, p_gpr_pack::OUTPUT_ADDR, p_gpr_pack::OUTPUT_ADDR, p_gpr_pack::OUTPUT_ADDR_OFFSET);
                TTI_STALLWAIT(p_stall::STALL_CFG, p_stall::THCON);
                TTI_WRCFG(p_gpr_pack::OUTPUT_ADDR, 0, THCON_SEC0_REG1_L1_Dest_addr_ADDR32);
                TTI_NOP;
#endif
            }

            TTI_INCADCZW(p_setadc::PAC, 0, 0, 0, 1); // z cnt increments by 2xface_r_dimxFACE_C_DIM
            // Reset Y counter for next face pair
            TTI_SETADCXY(p_setadc::PAC, 0, 0, 0, 0, 0b0010);
        }
    }

    // Reset counters
    TTI_SETADCZW(p_setadc::PAC, 0, 0, 0, 0, 0b0101);
    set_dst_write_addr(tile_dst_offset);
}
