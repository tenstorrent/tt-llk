// SPDX-FileCopyrightText: © 2026 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cstdint>

namespace ckernel
{
namespace sfpu
{

// Forward declaration of function defined later in ckernel_sfpu_reduce.h
inline void configure_addrmod_max_min(std::uint32_t num_cols);

// Non-LOADMACRO versions of init_reduce_max_min and calculate_reduce_max_min.
//
// The LOADMACRO version records 11 instructions in replay buffer 0.
// The SFPLOADMACRO calls (seq0, seq1) each do: SFPLOAD(VD) + scheduled SFPSWAP.
// Without LOADMACRO, each is replaced by 2 explicit instructions, expanding
// the buffer to 13 slots. The calculate function uses adjusted replay counts.

template <InstrModLoadStore INSTRUCTION_MODE, PoolType pool_type>
inline void init_reduce_max_min(std::uint32_t num_cols)
{
    _init_sfpu_config_reg();

    if constexpr (pool_type == PoolType::MIN)
    {
        TTI_SFPLOADI(ckernel::p_sfpu::LREG0, sfpi::SFPLOADI_MOD0_LOWER, 0x0100);
        TTI_SFPLOADI(ckernel::p_sfpu::LREG0, sfpi::SFPLOADI_MOD0_UPPER, 0x0000);
        TTI_SFPCONFIG(0, 0xF, 0);
    }

    configure_addrmod_max_min(num_cols);

    // Record 13-instruction replay buffer replacing SFPLOADMACRO with
    // explicit SFPLOAD + SFPSWAP pairs. BH NOPs between SWAPs are preserved.
    lltt::record<lltt::NoExec>(0, 13);
    TTI_INCRWC(0, 4, 0, 0);                                       // slot 0
    TTI_SFPLOAD(p_sfpu::LREG1, INSTRUCTION_MODE, ADDR_MOD_7, 2);  // slot 1: was SFPLOADMACRO(seq1, ..., 2)
    TTI_SFPSWAP(0, p_sfpu::LREG5, p_sfpu::LREG1, 1);              // slot 2: scheduled by seq1
    TTI_SFPLOAD(p_sfpu::LREG0, INSTRUCTION_MODE, ADDR_MOD_7, 16); // slot 3
    TTI_SFPLOAD(p_sfpu::LREG1, INSTRUCTION_MODE, ADDR_MOD_7, 18); // slot 4
    TTI_SFPSWAP(0, p_sfpu::LREG7, p_sfpu::LREG1, 1);              // slot 5
    TTI_SFPNOP;                                                   // slot 6 (preserved from original)
    TTI_SFPSWAP(0, p_sfpu::LREG6, p_sfpu::LREG0, 1);              // slot 7
    TTI_SFPNOP;                                                   // slot 8 (preserved from original)
    TTI_SFPLOAD(p_sfpu::LREG0, INSTRUCTION_MODE, ADDR_MOD_7, 0);  // slot 9: was SFPLOADMACRO(seq0, ..., 0)
    TTI_SFPSWAP(0, p_sfpu::LREG4, p_sfpu::LREG0, 1);              // slot 10: scheduled by seq0
    TTI_SFPLOAD(8, INSTRUCTION_MODE, ADDR_MOD_6, 0);              // slot 11: dummy increment
    TTI_SFPLOAD(8, INSTRUCTION_MODE, ADDR_MOD_5, 0);              // slot 12: dummy increment
}

template <PoolType pool_type, ReduceDim reduce_dim, InstrModLoadStore INSTRUCTION_MODE>
inline void calculate_reduce_max_min(const std::uint32_t block_height)
{
    static_assert(reduce_dim == REDUCE_COL, "Only column reduction (REDUCE_COL) is currently supported");
    static_assert(pool_type == PoolType::MAX || pool_type == PoolType::MIN, "Only MAX and MIN pool types are supported for this function");

    // Adjusted replay counts: 9→11, 10→12 (buffer expanded by 2 due to SFPLOADMACRO expansion)
    constexpr std::uint32_t replay_buffer_offset    = 11;
    constexpr std::uint32_t replay_buffer_next_face = 12;

    TTI_SFPLOAD(p_sfpu::LREG4, INSTRUCTION_MODE, ADDR_MOD_7, 0);
    TTI_SFPLOAD(p_sfpu::LREG5, INSTRUCTION_MODE, ADDR_MOD_7, 2);
    TTI_SFPLOAD(p_sfpu::LREG6, INSTRUCTION_MODE, ADDR_MOD_7, 16);
    TTI_SFPLOAD(p_sfpu::LREG7, INSTRUCTION_MODE, ADDR_MOD_7, 18);

    lltt::replay(0, replay_buffer_offset);
    lltt::replay(0, replay_buffer_offset);
    lltt::replay(0, replay_buffer_next_face);

    lltt::replay(0, replay_buffer_offset);
    lltt::replay(0, replay_buffer_offset);
    lltt::replay(0, replay_buffer_offset);
    lltt::replay(0, replay_buffer_next_face + 1);

    for (std::uint32_t i = 0; i < block_height - 1; i++)
    {
        lltt::replay(0, replay_buffer_offset);
        lltt::replay(0, replay_buffer_offset);
        lltt::replay(0, replay_buffer_offset);
        lltt::replay(0, replay_buffer_next_face);

        lltt::replay(0, replay_buffer_offset);
        lltt::replay(0, replay_buffer_offset);
        lltt::replay(0, replay_buffer_offset);
        lltt::replay(0, replay_buffer_next_face + 1);
    }

    TTI_SETRWC(p_setrwc::CLR_NONE, 0, 0, 0, 0, p_setrwc::SET_D);

    TTI_SFPTRANSP(0, 0, 0, 0);
    TTI_SFPSWAP(0, p_sfpu::LREG6, p_sfpu::LREG7, 1);
    TTI_SFPSWAP(0, p_sfpu::LREG5, p_sfpu::LREG6, 1);
    TTI_SFPSWAP(0, p_sfpu::LREG4, p_sfpu::LREG5, 1);
    TTI_SFPTRANSP(0, 0, 0, 0);

    TTI_SFPSTORE(p_sfpu::LREG4, INSTRUCTION_MODE, ADDR_MOD_7, 0);
    TTI_SFPSTORE(p_sfpu::LREG5, INSTRUCTION_MODE, ADDR_MOD_7, 2);
    TTI_SFPSTORE(p_sfpu::LREG6, INSTRUCTION_MODE, ADDR_MOD_7, 16);
    TTI_SFPSTORE(p_sfpu::LREG7, INSTRUCTION_MODE, ADDR_MOD_7, 18);
}

} // namespace sfpu
} // namespace ckernel
