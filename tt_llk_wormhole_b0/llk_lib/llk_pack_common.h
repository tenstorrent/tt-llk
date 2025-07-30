// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

/**
 * @file llk_pack_common.h
 * @brief Core packer unit synchronization and data handling functions for Tensix
 *
 * This header provides fundamental packer unit coordination, synchronization, and data
 * output functions used across all LLK packer operations. These functions manage the
 * interface between mathematical processing and L1 memory output, including tile header
 * generation and destination buffer management.
 *
 * @note Based on PR analysis: Race conditions in packer threads (PR #96/97) require
 *       careful synchronization patterns and proper STALLWAIT usage.
 */

#pragma once

#include <cstdint>

#include "ckernel.h"
#include "ckernel_defs.h"
#include "ckernel_instr_params.h"
#include "cpack_common.h"
#include "llk_defs.h"

using namespace ckernel;
using namespace ckernel::packer;

/**
 * @brief Wait for mathematical unit to complete operations before packing
 *
 * Synchronizes the packer with the mathematical processing unit by waiting for
 * math operations to complete. This function ensures that mathematical results
 * are ready before the packer attempts to process them.
 *
 * @note Uses semaphore-based synchronization to coordinate between math and pack units.
 *       Critical for preventing data races and ensuring result validity.
 * 
 * @warning Must be called before any packer operations that depend on math results.
 *          Missing synchronization can cause data corruption or invalid outputs.
 * 
 * @see _llk_packer_set_math_semaphore_ for releasing the semaphore after packing
 */
// wait until math is done and has produced something to pack
inline void _llk_packer_wait_for_math_done_()
{
    TTI_SEMWAIT(p_stall::STALL_TDMA, semaphore::t6_sem(semaphore::MATH_PACK), p_stall::STALL_ON_ZERO);
}

/**
 * @brief Release mathematical unit synchronization after packing completion
 *
 * Signals to the mathematical processing unit that packer operations are complete
 * and the math unit can proceed with new operations. This function manages the
 * semaphore coordination between pack and math units.
 *
 * @tparam WaitRes Wait resolution flags for synchronization timing control
 *
 * @note Semaphore release timing affects overall pipeline throughput. Should be
 *       called immediately after packer writes are committed to L1 memory.
 * 
 * @note WaitRes parameter allows fine-tuning of synchronization behavior for
 *       different performance requirements.
 *
 * @warning Improper semaphore release can cause pipeline stalls or deadlocks.
 *          Must be paired with corresponding _llk_packer_wait_for_math_done_ call.
 */
// Tell math that it can write again
template <uint WaitRes = p_stall::NONE>
inline void _llk_packer_set_math_semaphore_()
{
    t6_semaphore_get<WaitRes>(semaphore::MATH_PACK); // Indicate that packer is done and header is written into L1
}

/**
 * @brief Complete destination section packing and clear destination buffers
 *
 * Finalizes packer operations for a destination section, ensures all writes are
 * committed to L1 memory, and clears destination accumulator buffers for the next
 * operation. Handles both FP32 and standard accumulation modes with appropriate
 * clearing strategies.
 *
 * @tparam Dst Destination synchronization mode (SyncFull, SyncHalf)
 * @tparam is_fp32_dest_acc_en Enable FP32 destination accumulation mode
 *
 * @note Clearing behavior varies by sync mode:
 *       - SyncFull: Clears all destination registers
 *       - SyncHalf: Clears half of destination registers based on offset
 * 
 * @note FP32 accumulation mode uses different clearing commands (32-bit vs 16-bit)
 *       to handle the expanded data width properly.
 *
 * @warning STALLWAIT is critical to ensure pack completion before clearing.
 *          Premature clearing can cause data loss or corruption.
 * 
 * @warning Sync mode must match the mathematical operation's buffer usage.
 *          Mismatch can cause incomplete clears or buffer corruption.
 */
// Wait for all writes to complete in L1 (header + data)
// Tell math it can write again
// Clear dest
template <DstSync Dst, bool is_fp32_dest_acc_en>
inline void _llk_pack_dest_section_done_()
{
    TTI_STALLWAIT(p_stall::STALL_MATH, p_stall::PACK); // wait for pack to finish

    if constexpr (Dst == DstSync::SyncFull)
    {
        constexpr uint32_t CLEAR_MODE = is_fp32_dest_acc_en ? p_zeroacc::CLR_ALL_32B : p_zeroacc::CLR_ALL;
        TT_ZEROACC(CLEAR_MODE, ADDR_MOD_1, 0);
    }
    else
    {
        static_assert(Dst == DstSync::SyncHalf);
        constexpr uint32_t CLEAR_MODE = is_fp32_dest_acc_en ? p_zeroacc::CLR_HALF_32B : p_zeroacc::CLR_HALF;
        TT_ZEROACC(CLEAR_MODE, ADDR_MOD_1, (dest_offset_id) % 2);
    }

    // Tell math that it can write again
    _llk_packer_set_math_semaphore_<p_stall::NONE>();

    if constexpr (Dst == DstSync::SyncHalf)
    {
        flip_packer_dest_offset_id();
        select_packer_dest_registers<Dst>();
    }
}

template <DstSync Dst, DstTileFaceLayout FaceLayout, bool untilize = false, bool diagonal = false>
inline void _llk_init_packer_dest_offset_registers_(const std::uint32_t face_r_dim = FACE_R_DIM, const bool narrow_tile = false)
{
    TTI_STALLWAIT(p_stall::STALL_TDMA | p_stall::STALL_THCON, p_stall::PACK); // wait for pack to finish
    if constexpr (untilize)
    {
        const uint face_r_offset = ((face_r_dim == 1) || narrow_tile || diagonal) ? FACE_R_DIM : (face_r_dim >> 1);
        if constexpr (FaceLayout == ColMajor)
        {
            // Packer0 :  0,32,  1,33 ...  7, 39
            // Packer1 :  8,40,  9,41 ... 15, 47
            // Packer2 : 16,48, 17,49 ... 23, 55
            // Packer3 : 23,56, 24,57 ... 31, 63
            TT_SETDMAREG(0, 0x000 + 0x00, 0, LO_16(p_gpr_pack::DEST_OFFSET_LO + 0));
            TT_SETDMAREG(0, 0x000 + 0x08, 0, LO_16(p_gpr_pack::DEST_OFFSET_LO + 1));
            TT_SETDMAREG(0, 0x000 + 0x10, 0, LO_16(p_gpr_pack::DEST_OFFSET_LO + 2));
            TT_SETDMAREG(0, 0x000 + 0x18, 0, LO_16(p_gpr_pack::DEST_OFFSET_LO + 3));
            TT_SETDMAREG(0, DEST_REGISTER_HALF_SIZE + 0x00, 0, LO_16(p_gpr_pack::DEST_OFFSET_HI + 0));
            TT_SETDMAREG(0, DEST_REGISTER_HALF_SIZE + 0x08, 0, LO_16(p_gpr_pack::DEST_OFFSET_HI + 1));
            TT_SETDMAREG(0, DEST_REGISTER_HALF_SIZE + 0x10, 0, LO_16(p_gpr_pack::DEST_OFFSET_HI + 2));
            TT_SETDMAREG(0, DEST_REGISTER_HALF_SIZE + 0x18, 0, LO_16(p_gpr_pack::DEST_OFFSET_HI + 3));
        }
        else
        {
            if constexpr (diagonal)
            {
                // For example if face_offset = 8:
                //  Packer0 :  0,16,  1,17 ...  7, 23
                //  Packer1 :  8,24,  9,25 ... 15, 31
                //  Packer2 : 32,48, 33,49 ... 39, 55
                //  Packer3 : 40,56, 41,57 ... 47, 63
                TT_SETDMAREG(0, 0x000 + 0x00, 0, LO_16(p_gpr_pack::DEST_OFFSET_LO + 0));
                TT_SETDMAREG(0, 0x000 + 0x20 + face_r_offset, 0, LO_16(p_gpr_pack::DEST_OFFSET_LO + 1));
                // TT_SETDMAREG(0, 0x000 + 0x20, 0, LO_16(p_gpr_pack::DEST_OFFSET_LO + 2));
                // TT_SETDMAREG(0, 0x000 + 0x20 + face_r_offset, 0, LO_16(p_gpr_pack::DEST_OFFSET_LO + 3));
                TT_SETDMAREG(0, DEST_REGISTER_HALF_SIZE + 0x00, 0, LO_16(p_gpr_pack::DEST_OFFSET_HI + 0));
                TT_SETDMAREG(0, DEST_REGISTER_HALF_SIZE + 0x20 + face_r_offset, 0, LO_16(p_gpr_pack::DEST_OFFSET_HI + 1));
                // TT_SETDMAREG(0, DEST_REGISTER_HALF_SIZE + 0x20, 0, LO_16(p_gpr_pack::DEST_OFFSET_HI + 2));
                // TT_SETDMAREG(0, DEST_REGISTER_HALF_SIZE + 0x20 + face_r_offset, 0, LO_16(p_gpr_pack::DEST_OFFSET_HI + 3));
            }
            else
            {
                // For example if face_offset = 8:
                //  Packer0 :  0,16,  1,17 ...  7, 23
                //  Packer1 :  8,24,  9,25 ... 15, 31
                //  Packer2 : 32,48, 33,49 ... 39, 55
                //  Packer3 : 40,56, 41,57 ... 47, 63
                TT_SETDMAREG(0, 0x000 + 0x00, 0, LO_16(p_gpr_pack::DEST_OFFSET_LO + 0));
                TT_SETDMAREG(0, 0x000 + face_r_offset, 0, LO_16(p_gpr_pack::DEST_OFFSET_LO + 1));
                TT_SETDMAREG(0, 0x000 + 0x20, 0, LO_16(p_gpr_pack::DEST_OFFSET_LO + 2));
                TT_SETDMAREG(0, 0x000 + 0x20 + face_r_offset, 0, LO_16(p_gpr_pack::DEST_OFFSET_LO + 3));
                TT_SETDMAREG(0, DEST_REGISTER_HALF_SIZE + 0x00, 0, LO_16(p_gpr_pack::DEST_OFFSET_HI + 0));
                TT_SETDMAREG(0, DEST_REGISTER_HALF_SIZE + face_r_offset, 0, LO_16(p_gpr_pack::DEST_OFFSET_HI + 1));
                TT_SETDMAREG(0, DEST_REGISTER_HALF_SIZE + 0x20, 0, LO_16(p_gpr_pack::DEST_OFFSET_HI + 2));
                TT_SETDMAREG(0, DEST_REGISTER_HALF_SIZE + 0x20 + face_r_offset, 0, LO_16(p_gpr_pack::DEST_OFFSET_HI + 3));
            }
        }
    }
    else
    {
        if constexpr (FaceLayout == ColMajor)
        {
            TTI_SETDMAREG(0, 0x00, 0, LO_16(p_gpr_pack::DEST_OFFSET_LO + 0));
            TTI_SETDMAREG(0, 0x20, 0, LO_16(p_gpr_pack::DEST_OFFSET_LO + 1));
            TTI_SETDMAREG(0, 0x10, 0, LO_16(p_gpr_pack::DEST_OFFSET_LO + 2));
            TTI_SETDMAREG(0, 0x30, 0, LO_16(p_gpr_pack::DEST_OFFSET_LO + 3));
            TTI_SETDMAREG(0, DEST_REGISTER_HALF_SIZE + 0x00, 0, LO_16(p_gpr_pack::DEST_OFFSET_HI + 0));
            TTI_SETDMAREG(0, DEST_REGISTER_HALF_SIZE + 0x20, 0, LO_16(p_gpr_pack::DEST_OFFSET_HI + 1));
            TTI_SETDMAREG(0, DEST_REGISTER_HALF_SIZE + 0x10, 0, LO_16(p_gpr_pack::DEST_OFFSET_HI + 2));
            TTI_SETDMAREG(0, DEST_REGISTER_HALF_SIZE + 0x30, 0, LO_16(p_gpr_pack::DEST_OFFSET_HI + 3));
        }
        else
        { // Default to row major layout
            TTI_SETDMAREG(0, 0x00, 0, LO_16(p_gpr_pack::DEST_OFFSET_LO + 0));
            TTI_SETDMAREG(0, 0x10, 0, LO_16(p_gpr_pack::DEST_OFFSET_LO + 1));
            TTI_SETDMAREG(0, 0x20, 0, LO_16(p_gpr_pack::DEST_OFFSET_LO + 2));
            TTI_SETDMAREG(0, 0x30, 0, LO_16(p_gpr_pack::DEST_OFFSET_LO + 3));
            TTI_SETDMAREG(0, DEST_REGISTER_HALF_SIZE + 0x00, 0, LO_16(p_gpr_pack::DEST_OFFSET_HI + 0));
            TTI_SETDMAREG(0, DEST_REGISTER_HALF_SIZE + 0x10, 0, LO_16(p_gpr_pack::DEST_OFFSET_HI + 1));
            TTI_SETDMAREG(0, DEST_REGISTER_HALF_SIZE + 0x20, 0, LO_16(p_gpr_pack::DEST_OFFSET_HI + 2));
            TTI_SETDMAREG(0, DEST_REGISTER_HALF_SIZE + 0x30, 0, LO_16(p_gpr_pack::DEST_OFFSET_HI + 3));
        }
    }
    select_packer_dest_registers<Dst>();
}

template <DstSync Dst, bool is_fp32_dest_acc_en, DstTileFaceLayout FaceLayout = RowMajor, bool untilize = false>
inline void _llk_pack_dest_init_(const std::uint32_t face_r_dim = FACE_R_DIM, const bool narrow_tile = false)
{
    tensix_sync();
    reset_dest_offset_id();
    _llk_init_packer_dest_offset_registers_<Dst, FaceLayout, untilize>(face_r_dim, narrow_tile);
    packer_addr_counter_init();
    pack_sync_tile_dst_ptr = 0;
}

template <bool mail2math = true, bool mail2pack = true>
inline void _llk_pack_get_tile_(std::uint32_t tile_index, std::uint32_t *p_tile)
{
    constexpr uint32_t wait_sem = (mail2math && mail2pack) ? (2) : (1);
    while (semaphore_read(semaphore::UNPACK_OPERAND_SYNC) < wait_sem)
        ;
    if constexpr (mail2pack)
    {
        *p_tile = mailbox_read(ThreadId::UnpackThreadId);
    }
    else
    {
        *p_tile = 0x0;
    }
}

template <bool mail2math = true, bool mail2pack = true>
inline void _llk_pack_release_tile_()
{
    if constexpr (mail2pack)
    {
        semaphore_get(semaphore::UNPACK_OPERAND_SYNC);
    }
}

inline void _llk_pack_debug_dump_(std::uint8_t *data, std::uint32_t byte_size)
{
    debug_dump(data, byte_size);
}

inline void _llk_pack_debug_dump_seek_(std::uint8_t offset)
{
    debug_dump_seek(offset);
}

TT_ALWAYS_INLINE void _llk_pack_relu_config_(const std::uint32_t config)
{
    ReluType mode = (config & 0xf) == 0 ? ReluType::NO_RELU : ((config & 0xf) == 3 ? ReluType::MAX_THRESHOLD_RELU : ReluType::MIN_THRESHOLD_RELU);
    uint32_t val  = ((config >> 16) << STACC_RELU_ReluThreshold_SHAMT) | (((uint32_t)mode) << STACC_RELU_ApplyRelu_SHAMT);
    TTI_SETDMAREG(0, val & 0xffff, 0, LO_16(p_gpr_pack::TMP0));
    TTI_SETDMAREG(0, val >> 16, 0, HI_16(p_gpr_pack::TMP0));
    TTI_STALLWAIT(p_stall::STALL_CFG, p_stall::PACK);
    TTI_WRCFG(p_gpr_pack::TMP0, p_cfg::WRCFG_32b, STACC_RELU_ApplyRelu_ADDR32);
    TTI_NOP;
    TTI_NOP;
}

inline void _llk_pack_reconfig_l1_acc_(const std::uint32_t enable)
{
    reconfigure_packer_l1_acc(enable);
}

template <bool untilize = false, ReduceDim dim>
inline void _llk_pack_reduce_mask_config_()
{
    ckernel::packer::pck_edge_offset_u pack_edge_offset = {.val = 0};

    // We initialize PCK_EDGE_OFFSET_SEC0 mask to clear out all the datums in the row
    pack_edge_offset.f.mask        = 0x0;
    uint32_t row_set_mapping_1     = 0;
    uint32_t edge_offset_sec1_mask = 0;

    if constexpr (dim == ReduceDim::REDUCE_ROW)
    {
        // PCK_EDGE_OFFSET_SEC1 mask will clear out all the datums in the row except the first one
        edge_offset_sec1_mask = 0x0001;
        if constexpr (untilize)
        {
            pack_edge_offset.f.tile_row_set_select_pack0 = 1;
            pack_edge_offset.f.tile_row_set_select_pack1 = 1;
            pack_edge_offset.f.tile_row_set_select_pack2 = 1;
            pack_edge_offset.f.tile_row_set_select_pack3 = 1;
            row_set_mapping_1                            = 0x11111111; // each packer packs 1x32 row
        }
        else
        {
            // Packer 0 and 2 will use TILE_ROW_SET_MAPPING_1, while packer 1 and 3 will keep using
            // TILE_ROW_SET_MAPPING_0 configuration which is the default one
            pack_edge_offset.f.tile_row_set_select_pack0 = 1;
            pack_edge_offset.f.tile_row_set_select_pack2 = 1;

            // TILE_ROW_SET_MAPPING_1 configuration sets all rows to use PCK_EDGE_OFFSET_SEC1 mask
            row_set_mapping_1 = 0x55555555; // each packer packs 1x16 row
        }
    }
    else if constexpr (dim == ReduceDim::REDUCE_COL)
    {
        // PCK_EDGE_OFFSET_SEC1 mask will pass through all the datums in the row as they are
        edge_offset_sec1_mask = 0xffff;

        // Packer 0 and 1 will use TILE_ROW_SET_MAPPING_1, while packer 2 and 3 will keep using
        // TILE_ROW_SET_MAPPING_0 configuration which is the default one
        pack_edge_offset.f.tile_row_set_select_pack0 = 1;
        pack_edge_offset.f.tile_row_set_select_pack1 = 1;

        if constexpr (untilize)
        {
            row_set_mapping_1 = 0x00000005; // each packer packs 1x32 row
        }
        else
        {
            // TILE_ROW_SET_MAPPING_1 configuration sets only first row to use PCK_EDGE_OFFSET_SEC1 mask
            row_set_mapping_1 = 0x00000001; // each packer packs 1x16 row
        }
    }
    else if constexpr (dim == ReduceDim::REDUCE_SCALAR)
    {
        // PCK_EDGE_OFFSET_SEC1 mask will clear out all the datums in the row except the first one
        edge_offset_sec1_mask = 0x0001;
        // Packer 0  will use TILE_ROW_SET_MAPPING_1, while packers 1,2 and 3 will keep using
        // TILE_ROW_SET_MAPPING_0 configuration which is the default one
        pack_edge_offset.f.tile_row_set_select_pack0 = 1;

        // TILE_ROW_SET_MAPPING_1 configuration sets only first row to use PCK_EDGE_OFFSET_SEC1 mask
        row_set_mapping_1 = 0x00000001;
    }

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

inline void _llk_pack_reduce_mask_clear_()
{
    // By default, all packers are set to use TILE_ROW_SET_MAPPING_0 and
    // mask is configured to pass through all the datums
    pck_edge_offset_u pack_edge_offset = {.val = 0};
    pack_edge_offset.f.mask            = 0xffff;

    // Initialize TMP registers with values we need to write in CFG registers
    TTI_SETDMAREG(0, LOWER_HALFWORD(pack_edge_offset.val), 0, LO_16(p_gpr_pack::TMP0));
    TTI_SETDMAREG(0, UPPER_HALFWORD(pack_edge_offset.val), 0, HI_16(p_gpr_pack::TMP0));

    // Wait for packer to finish to avoid breaking its current configuration
    TTI_STALLWAIT(p_stall::STALL_CFG, p_stall::PACK);

    // Clear out packer configuration for reduce
    TTI_WRCFG(p_gpr_pack::TMP0, p_cfg::WRCFG_32b, PCK_EDGE_OFFSET_SEC0_mask_ADDR32);
    TTI_WRCFG(p_gpr_pack::TMP0, p_cfg::WRCFG_32b, PCK_EDGE_OFFSET_SEC1_mask_ADDR32);

    // All mappings point to PCK_EDGE_OFFSET_SEC0_mask_ADDR32
    TTI_WRCFG(p_gpr::ZERO, p_cfg::WRCFG_32b, TILE_ROW_SET_MAPPING_0_row_set_mapping_0_ADDR32);
    TTI_WRCFG(p_gpr::ZERO, p_cfg::WRCFG_32b, TILE_ROW_SET_MAPPING_1_row_set_mapping_0_ADDR32);

    TTI_NOP;
    TTI_NOP;
}
