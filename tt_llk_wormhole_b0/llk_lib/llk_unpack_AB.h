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
#include "lltt.h"

using namespace ckernel;
using namespace ckernel::unpacker;

template <BroadcastType BType = BroadcastType::NONE>
inline void _llk_unpack_AB_mop_config_(const bool transpose_of_faces = false, const std::uint32_t num_faces = 4, const bool narrow_tile = false)
{
    static constexpr uint unpack_srca = TT_OP_UNPACR(SrcA, 0b1, 0, 0, 0, 1, 1, p_unpacr::RAREFYB_DISABLE, 0, 0, 0, 0, 1);
    static constexpr uint unpack_srcb = TT_OP_UNPACR(SrcB, 0b1, 0, 0, 0, 1, 1, p_unpacr::RAREFYB_DISABLE, 0, 0, 0, 0, 1);

    if constexpr (BType == BroadcastType::COL)
    {
        static constexpr uint unpack_srcb_set_z = TT_OP_SETADCZW(0b010, 0, 0, 0, 2, 0b0001);
        const uint32_t outerloop                = num_faces < 4 ? 1 : 2;
        const uint32_t innerloop                = num_faces < 2 ? 1 : 2;
        ckernel_template tmp(outerloop, innerloop, unpack_srca);
        tmp.set_start_op(unpack_srcb);
        if (narrow_tile)
        {
            tmp.set_end_op(unpack_srcb); // Read face 1
        }
        else
        {
            tmp.set_end_op(unpack_srcb_set_z);
        }
        tmp.program(instrn_buffer);
    }
    else if constexpr (BType == BroadcastType::ROW)
    {
        static constexpr uint unpack_srcb_clear_z  = TT_OP_SETADCZW(0b010, 0, 0, 0, 0, 0b0001);
        static constexpr uint unpack_srcb_no_z_inc = TT_OP_UNPACR(SrcB, 0b0, 0, 0, 0, 1, 1, p_unpacr::RAREFYB_DISABLE, 0, 0, 0, 0, 1);
        const uint32_t outerloop                   = num_faces < 4 ? 1 : 2;
        const uint32_t innerloop                   = num_faces < 2 ? 1 : 2;
        ckernel_template tmp(outerloop, innerloop, narrow_tile ? unpack_srcb_no_z_inc : unpack_srcb, unpack_srca);
        tmp.set_end_op(unpack_srcb_clear_z);
        tmp.program(instrn_buffer);
    }
    else if constexpr (BType == BroadcastType::SCALAR)
    {
        const uint32_t outerloop = 1;
        const uint32_t innerloop = num_faces;
        ckernel_template tmp(outerloop, innerloop, unpack_srca);
        tmp.set_start_op(unpack_srcb);
        tmp.program(instrn_buffer);
    }
    else
    {
        if (transpose_of_faces)
        {
            static constexpr uint srca_set_z         = TT_OP_SETADCZW(0b001, 0, 0, 0, 1, 0b0001);                                         // set z to 1
            static constexpr uint unpack_srca_skip_z = TT_OP_UNPACR(SrcA, 0b10, 0, 0, 0, 1, 1, p_unpacr::RAREFYB_DISABLE, 0, 0, 0, 0, 1); // inc z by 2
            const uint32_t outerloop                 = num_faces < 4 ? 1 : 2;
            const uint32_t innerloop                 = num_faces < 2 ? 1 : 2;
            ckernel_template tmp(outerloop, innerloop, num_faces < 4 ? unpack_srca : unpack_srca_skip_z, unpack_srcb);
            tmp.set_end_op(srca_set_z);
            tmp.program(instrn_buffer);
        }
        else
        {
            constexpr uint32_t outerloop = 1;
            const uint32_t innerloop     = num_faces;
            ckernel_template tmp(outerloop, innerloop, unpack_srca, unpack_srcb);
            tmp.program(instrn_buffer);
        }
    }
}

template <bool is_fp32_dest_acc_en, StochRndType stoch_rnd_mode = StochRndType::None>
inline void _llk_unpack_AB_hw_configure_(
    const std::uint32_t unpA_src_format,
    const std::uint32_t unpB_src_format,
    const std::uint32_t unpA_dst_format,
    const std::uint32_t unpB_dst_format,
    const std::uint32_t face_r_dim                  = FACE_R_DIM,
    const std::uint32_t within_face_16x16_transpose = 0,
    const std::uint32_t num_faces                   = 4)
{
    constexpr bool is_row_pool  = false;
    constexpr bool stoch_rnd_en = (stoch_rnd_mode == StochRndType::All);
    constexpr bool fpu_srnd_en  = stoch_rnd_en || (stoch_rnd_mode == StochRndType::Fpu);
    constexpr bool pack_srnd_en = stoch_rnd_en || (stoch_rnd_mode == StochRndType::Pack);
    configure_unpack_AB<is_fp32_dest_acc_en, is_row_pool, fpu_srnd_en, pack_srnd_en>(
        unpA_src_format, unpB_src_format, unpA_dst_format, unpB_dst_format, face_r_dim, face_r_dim, within_face_16x16_transpose, num_faces, num_faces);
}

/**
 * @brief Initialize dual unpacker (A and B operands) for Wormhole B0 unpack thread
 * 
 * @details Configures both Unpacker 0 (operand A → SRCA) and Unpacker 1 (operand B → SRCB)
 * for efficient dual-operand data movement operations. This function supports various
 * broadcast patterns, transpose operations, and tile configurations for optimal data
 * loading from L1 SRAM to register files.
 * 
 * **Dual Unpacker Architecture:**
 * - Unpacker 0: Loads operand A data into SRCA register file (32×16 datums)
 * - Unpacker 1: Loads operand B data into SRCB register file (32×16 datums)
 * - Combined throughput: Up to 80B/clock for high-bandwidth data movement
 * 
 * **Broadcast Support:**
 * Template parameter enables efficient broadcast operations:
 * - BroadcastType::NONE: No broadcast, regular tile-to-tile movement
 * - BroadcastType::COL: Column broadcast for operand B across multiple A columns
 * - BroadcastType::ROW: Row broadcast for operand B across multiple A rows
 * - BroadcastType::SCALAR: Scalar broadcast for single value operations
 * 
 * **Performance Features:**
 * - Optimal L1 bank interleaving for both operands
 * - Hardware address generation with carriage return
 * - Support for narrow tiles and partial face operations
 * - Configurable transpose modes for data layout optimization
 * 
 * @tparam BType Broadcast type for operand B (default: NONE)
 * 
 * @param face_r_dim Row dimension of tile face (default: FACE_R_DIM)
 * @param num_faces Number of faces in tile (default: 4)
 * @param narrow_tile Enable narrow tile optimization for memory efficiency
 * @param transpose Transpose mode: 0=no transpose, 1=transpose within face
 * @param acc_to_dest Accumulation to destination mode (reserved parameter)
 * 
 * @note This function must be called once before any _llk_unpack_AB_() operations
 * @note Broadcast patterns affect address generation and memory access patterns
 * @note Both unpackers are configured simultaneously for coordinated operation
 * 
 * @warning face_r_dim must be power of 2 and ≤ 32
 * @warning num_faces must be 1, 2, or 4
 * @warning Broadcast template parameter affects all subsequent unpack operations
 * 
 * @see _llk_unpack_AB_() for per-tile execution
 * @see _llk_unpack_AB_hw_configure_() for hardware format configuration
 */
template <BroadcastType BType = BroadcastType::NONE>
inline void _llk_unpack_AB_init_(
    const std::uint32_t face_r_dim  = FACE_R_DIM,
    const std::uint32_t num_faces   = 4,
    const bool narrow_tile          = false,
    const std::uint32_t transpose   = 0,
    const std::uint32_t acc_to_dest = 0)
{
    // Validate template parameters
    LLK_VALIDATE_BROADCAST_TYPE(BType);
    
    // Validate function parameters
    LLK_VALIDATE_FACE_DIMENSION(face_r_dim);
    LLK_VALIDATE_NUM_FACES(num_faces);
    LLK_VALIDATE_PARAM_RANGE(transpose, 0, 1, "transpose must be 0 or 1");
    
    cfg_reg_rmw_tensix<THCON_SEC0_REG2_Haloize_mode_RMW>(transpose); // transpose within the face

    constexpr std::uint32_t UNP_SEL = p_setadc::UNP_AB;
    config_unpacker_x_end<UNP_SEL>(face_r_dim);

    _llk_unpack_AB_mop_config_<BType>(transpose > 0, num_faces, narrow_tile); // transpose of faces 0,2,1,3
}

/**
 * @brief Execute dual unpacker operation on specified L1 addresses
 * 
 * @details Performs the actual data movement from L1 SRAM to SRCA/SRCB register files
 * using the previously configured unpacker settings. This function handles the complete
 * unpacker pipeline including address setup, context management, and hardware synchronization.
 * 
 * **Operation Flow:**
 * 1. Reset address counters for fresh operation start
 * 2. Configure L1 source addresses for both operands
 * 3. Acquire hardware context for unpacker operation
 * 4. Execute configured macro-operation (MOP) sequence
 * 5. Release hardware context and switch to next context
 * 
 * **Context Management:**
 * Uses dual-context system for overlapped execution:
 * - Context 0/1 alternation enables pipeline overlap
 * - Semaphore synchronization ensures data consistency
 * - Automatic context switching for sustained throughput
 * 
 * @tparam BType Broadcast type (must match initialization template parameter)
 * @param address_a L1 SRAM address for operand A data
 * @param address_b L1 SRAM address for operand B data  
 * @param transpose_of_faces Reserved parameter (not used in current implementation)
 * 
 * @note Must be called after _llk_unpack_AB_init_() with matching template parameter
 * @note Addresses must be 16-byte aligned for optimal L1 access
 * @note Operates on tiles configured during initialization
 * 
 * @warning address_a and address_b must be valid L1 SRAM addresses
 * @warning Addresses must be aligned to tile boundaries
 * @warning BType template parameter must match initialization
 * 
 * @see _llk_unpack_AB_init_() for configuration
 * @see acquire_dst() for destination register management
 */
template <BroadcastType BType = BroadcastType::NONE>
inline void _llk_unpack_AB_(const std::uint32_t address_a, const std::uint32_t address_b, const bool transpose_of_faces = 0 /*not used*/)
{
    // Validate L1 addresses
    LLK_VALIDATE_L1_ALIGNMENT(address_a);
    LLK_VALIDATE_L1_ALIGNMENT(address_b);
    
    TTI_SETADCZW(0b011, 0, 0, 0, 0, 0b1111); // reset counters

    // Program srcA and srcB base addresses
    volatile uint tt_reg_ptr *cfg = get_cfg_pointer(); // get pointer to registers for current state ID

    // Wait for free context
    wait_for_next_context(2);

    // Get tile address
    if (0 == unp_cfg_context)
    {
        cfg[THCON_SEC0_REG3_Base_address_ADDR32] = address_a;
        cfg[THCON_SEC1_REG3_Base_address_ADDR32] = address_b;
    }
    else
    {
        cfg[THCON_SEC0_REG3_Base_cntx1_address_ADDR32] = address_a;
        cfg[THCON_SEC1_REG3_Base_cntx1_address_ADDR32] = address_b;
    }

    // Trisc::SEMPOST for context acquire
    semaphore_post(semaphore::UNPACK_SYNC);

    // Stall unpacker until pending CFG writes from Trisc have completed
    TTI_STALLWAIT(p_stall::STALL_UNPACK, p_stall::TRISC_CFG);

    // Run MOP
    ckernel::ckernel_template::run(instrn_buffer);

    // T6::SEMGET for context release
    t6_semaphore_get(semaphore::UNPACK_SYNC);

    // Switch unpacker config context
    switch_config_context(unp_cfg_context);
}
