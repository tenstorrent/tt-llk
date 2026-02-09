// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <array>

#include "cfg_defines.h"
#include "ckernel.h"

inline void device_setup()
{
#if defined(ARCH_WORMHOLE)
    // Use array-based initialization for consecutive TRISC addresses
    constexpr std::uint32_t TRISC_START_BASE    = 0x16DFF0;
    constexpr std::uint32_t TRISC_CONFIG_REGS[] = {TRISC_RESET_PC_SEC0_PC_ADDR32, TRISC_RESET_PC_SEC1_PC_ADDR32, TRISC_RESET_PC_SEC2_PC_ADDR32};

    volatile std::uint32_t* const trisc_start_addresses = reinterpret_cast<volatile std::uint32_t*>(TRISC_START_BASE);
    volatile uint tt_reg_ptr* cfg_regs                  = reinterpret_cast<volatile uint tt_reg_ptr*>(TENSIX_CFG_BASE);

    for (unsigned int i = 0; i < std::size(TRISC_CONFIG_REGS); ++i)
    {
        cfg_regs[TRISC_CONFIG_REGS[i]] = trisc_start_addresses[i];
    }
    cfg_regs[TRISC_RESET_PC_OVERRIDE_Reset_PC_Override_en_ADDR32] = 0b111;
#endif

#if defined(ARCH_BLACKHOLE)
    // Use array-based initialization for consecutive TRISC addresses, using BH debug registers
    constexpr std::uint32_t TRISC_START_BASE = 0x16DFF0;
    constexpr std::uint32_t TRISC_PC_REGS[]  = {RISCV_DEBUG_REG_TRISC0_RESET_PC, RISCV_DEBUG_REG_TRISC1_RESET_PC, RISCV_DEBUG_REG_TRISC2_RESET_PC};

    volatile std::uint32_t* const trisc_start_addresses = reinterpret_cast<volatile std::uint32_t*>(TRISC_START_BASE);

    for (unsigned int i = 0; i < std::size(TRISC_PC_REGS); ++i)
    {
        asm volatile("fence");
        ckernel::reg_write(TRISC_PC_REGS[i], trisc_start_addresses[i]);
        asm volatile("fence");
    }

    ckernel::reg_write(RISCV_DEBUG_REG_TRISC_RESET_PC_OVERRIDE, 0x7);
#endif

#if defined(ARCH_BLACKHOLE) && !defined(ARCH_QUASAR) // Ugly hack for now
    ckernel::reg_write(RISCV_DEBUG_REG_DEST_CG_CTRL, 0);
#endif
#if defined(ARCH_BLACKHOLE) || defined(ARCH_QUASAR)
    TTI_ZEROACC(ckernel::p_zeroacc::CLR_ALL, 0, 0, 1, 0);
#else
    TTI_ZEROACC(ckernel::p_zeroacc::CLR_ALL, 0, 0);
#endif

#if defined(ARCH_QUASAR)
    // Reset all dest dvalid bits for all clients
    TTI_CLEARDVALID(0, 0, 0xf, 0xf, 0, 0);
#endif

// Enable CC stack
#if defined(ARCH_QUASAR)
    TTI_SFPENCC(3, 10);
#else
    TTI_SFPENCC(3, 0, 0, 10);
#endif

    TTI_NOP;

    // Set default sfpu constant register state
    TTI_SFPCONFIG(0, 11, 1); // loading -1 to LREG11 where sfpi expects it

#ifndef ARCH_QUASAR
    // Initialize tensix semaphores
    ckernel::t6_semaphore_init(ckernel::semaphore::UNPACK_TO_DEST, 0, 1);
    ckernel::t6_semaphore_init(ckernel::semaphore::MATH_DONE, 0, 1);
    ckernel::t6_semaphore_init(ckernel::semaphore::PACK_DONE, 0, 1);
#endif
}

inline void clear_trisc_soft_reset()
{
#ifdef ARCH_QUASAR
    constexpr uint32_t TRISC_SOFT_RESET_MASK = 0x3000;
#else
    constexpr uint32_t TRISC_SOFT_RESET_MASK = 0x7000;
#endif

    volatile std::uint32_t* const REG_SOFT_RESET = reinterpret_cast<volatile std::uint32_t*>(RISCV_DEBUG_REG_SOFT_RESET_0);

    uint32_t soft_reset = ckernel::load_blocking(REG_SOFT_RESET);

    soft_reset &= ~TRISC_SOFT_RESET_MASK;

    ckernel::store_blocking(REG_SOFT_RESET, soft_reset);
}

// inline void set_triscs_soft_reset()
// {
// #ifdef ARCH_QUASAR
//     constexpr uint32_t TRISC_SOFT_RESET_MASK = 0x3000;
// #else
//     constexpr uint32_t TRISC_SOFT_RESET_MASK = 0x7000;
// #endif

// volatile std::uint32_t* const REG_SOFT_RESET = reinterpret_cast<volatile std::uint32_t*>(RISCV_DEBUG_REG_SOFT_RESET_0);

// uint32_t soft_reset = ckernel::load_blocking(REG_SOFT_RESET);

// soft_reset |= TRISC_SOFT_RESET_MASK;

// ckernel::store_blocking(REG_SOFT_RESET, soft_reset);

// }
