// SPDX-FileCopyrightText: © 2026 Tenstorrent AI ULC
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

template <BroadcastType BType = BroadcastType::NONE>
inline void _llk_unpack_AB_mop_config_(const std::uint32_t num_faces = 4, const bool narrow_tile = false)
{
    LLK_ASSERT(num_faces == 1 || num_faces == 2 || num_faces == 4, "num_faces must be 1, 2, or 4");

    // static constexpr std::uint32_t unpack_srca = TT_OP_UNPACR(SrcA, 0b1, 0, 0, 0, 1, 1, p_unpacr::RAREFYB_DISABLE, 0, 0, 0, 0, 1);
    // static constexpr std::uint32_t unpack_srcb = TT_OP_UNPACR(SrcB, 0b1, 0, 0, 0, 1, 1, p_unpacr::RAREFYB_DISABLE, 0, 0, 0, 0, 1);

    // if constexpr (BType == BroadcastType::COL)
    // {
    //     static constexpr std::uint32_t unpack_srcb_set_z = TT_OP_SETADCZW(0b010, 0, 0, 0, 2, 0b0001);
    //     const std::uint32_t outerloop                    = num_faces < 4 ? 1 : 2;
    //     const std::uint32_t innerloop                    = num_faces < 2 ? 1 : 2;
    //     ckernel_template tmp(outerloop, innerloop, unpack_srca);
    //     tmp.set_start_op(unpack_srcb);
    //     tmp.set_end_op(narrow_tile ? unpack_srcb : unpack_srcb_set_z);
    //     tmp.program();
    // }
    // else
    // {
    //     constexpr std::uint32_t outerloop = 1;
    //     const std::uint32_t innerloop     = num_faces;
    //     ckernel_template tmp(outerloop, innerloop, unpack_srca, unpack_srcb);
    //     tmp.program();
    // }
}

template <BroadcastType BType = BroadcastType::NONE, std::uint32_t ct_dim = 1>
inline void _llk_unpack_AB_init_(const std::uint32_t face_r_dim = FACE_R_DIM, const std::uint32_t num_faces = 4, const bool narrow_tile = false)
{
    LLK_ASSERT(num_faces == 1 || num_faces == 2 || num_faces == 4, "num_faces must be 1, 2, or 4");

    // Force both unpackers to unpack entire tile
    TTI_SETADCXX(p_setadc::UNP0, 1023, 0x0);
    TTI_SETADCXX(p_setadc::UNP1, 1023, 0x0);

    _llk_unpack_AB_mop_config_<BType>(num_faces, narrow_tile);
}

template <BroadcastType BType = BroadcastType::NONE, std::uint32_t ct_dim = 1>
inline void _llk_unpack_AB_(const std::uint32_t address_a, const std::uint32_t address_b)
{
    TTI_SETADCZW(0b011, 0, 0, 0, 0, 0b1111); // reset counters

    // Program srcA and srcB base addresses
    volatile std::uint32_t tt_reg_ptr *cfg = get_cfg_pointer(); // get pointer to registers for current state ID

    // Wait for free context
    wait_for_next_context(2);

    // Validate and configure addresses
    _llk_unpack_configure_addresses_(address_a, address_b, cfg);

    // Trisc::SEMPOST for context acquire
    semaphore_post(semaphore::UNPACK_SYNC);

    // Stall unpacker until pending CFG writes from Trisc have completed
    TTI_STALLWAIT(p_stall::STALL_UNPACK, p_stall::TRISC_CFG);

    // Run MOP
    // ckernel::ckernel_template::run();

    // unpack srcB with dvalid set, and no advancement of counters
    TTI_UNPACR(SrcB, 0b0, 0, 0, 0, 0, 1, p_unpacr::RAREFYB_DISABLE, 0, 0, 0, 0, 1);
    // unpack srcA with dvalid set, and no advancement of counters
    for (std::uint32_t i = 0; i < ct_dim; i++)
    {
        TTI_UNPACR(SrcA, 0b0, 0, 0, 0, 0, 1, p_unpacr::RAREFYB_DISABLE, 0, 0, 0, 0, 1);
    }

    // T6::SEMGET for context release
    t6_semaphore_get(semaphore::UNPACK_SYNC);

    // Switch unpacker config context
    switch_config_context(unp_cfg_context);
}
