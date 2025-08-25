// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "ckernel.h"
#include "llk_defs.h"

#ifdef LLK_TRISC_UNPACK

// L1 Memory Addresses
static constexpr uint32_t histogram_timestamp_address = 0x00019000;
static constexpr uint32_t histogram_start_address     = 0x00019FF0;
static constexpr uint32_t histogram_buffer_address    = 0x00020000;
static constexpr uint32_t histogram_bucket_address    = 0x00030000;

// TRISC Memory Addresses
static constexpr uint32_t histogram_wall_clock_address = 0xFFB121F0;
static constexpr uint32_t histogram_regfile_address    = 0xFFE00000;
static constexpr uint32_t histogram_pc_buf_address     = 0xFFE80000;

// Histogram Configuration
static constexpr uint32_t histogram_buffer_size     = HISTOGRAM_BUFFER_SIZE;
static constexpr uint32_t histogram_num_cores       = HISTOGRAM_NUM_CORES;
static constexpr uint32_t histogram_pipeline_factor = HISTOGRAM_PIPELINE_FACTOR;
static constexpr uint32_t histogram_version         = HISTOGRAM_VERSION;

// Core-specific calculations
#ifdef LLK_TRISC_UNPACK
static constexpr uint32_t histogram_core_idx = 0;
#endif

static constexpr uint32_t histogram_buffer_per_core  = histogram_buffer_size / histogram_num_cores;
static constexpr uint32_t histogram_outer_iterations = histogram_buffer_per_core / histogram_pipeline_factor;
static constexpr uint32_t histogram_timestamp_base   = histogram_timestamp_address + 4 * sizeof(uint32_t) * histogram_core_idx;
static constexpr uint32_t histogram_buffer_base      = histogram_buffer_address + histogram_buffer_per_core * sizeof(uint32_t) * histogram_core_idx;

inline void llk_histogram_wait_start()
{
    asm("la a3, %0" : : "i"(histogram_start_address) : "a3");
start_loop:
    asm("lw  a5, 0(a3)");
    __asm__ goto("beq a5, zero, %l[start_loop]" : : : : start_loop);
}

inline void llk_histogram_start_measuring()
{
    asm("fence");
    asm("la a3, %0" : : "i"(histogram_wall_clock_address) : "a3");
    asm("la a4, %0" : : "i"(histogram_timestamp_base) : "a4");
    asm("lw a5, 0(a3)");
    asm("sw a5, 0(a4)");
    asm("lw a5, 8(a3)");
    asm("sw a5, 4(a4)");
    asm("fence");
}

inline void llk_histogram_stop_measuring()
{
    asm("fence");
    asm("la a3, %0" : : "i"(histogram_wall_clock_address) : "a3");
    asm("la a4, %0" : : "i"(histogram_timestamp_base) : "a4");
    asm("lw a5,  0(a3)");
    asm("sw a5,  8(a4)");
    asm("lw a5,  8(a3)");
    asm("sw a5, 12(a4)");
    asm("fence");
}

inline void llk_histogram_setup_registers()
{
    // Setup base addresses (16-byte aligned for version 18)
    asm("la a0, %0" : : "i"(histogram_buffer_base / 16) : "a0");
    asm("la a1, %0" : : "i"(histogram_bucket_address / 16) : "a1");

    // Setup regfile and pc_buf addresses
    asm("la a6, %0" : : "i"(histogram_regfile_address) : "a6");
    asm("la a7, %0" : : "i"(histogram_pc_buf_address) : "a7");

    // Store addresses in regfile
    asm("sw a0, 240(a6)");   // Store input data address in regfile
    asm("sw zero, 244(a6)"); // Store 0 in regfile
    asm("sw a1, 248(a6)");   // Store histogram bucket address in regfile
}

template <uint32_t pipeline_factor>
inline void llk_histogram_compute()
{
    static_assert(histogram_version == 18, "This LLK implementation only supports version 18");

    for (uint32_t i = 0; i < histogram_outer_iterations / 4; i++)
    {
        // Phase 1: Load input data using TTI_LOADIND
        switch (pipeline_factor)
        {
            case 4:
                TTI_LOADIND(0, 122, 3, 12, 60);
                TTI_LOADIND(0, 122, 3, 8, 60);
            case 2:
                TTI_LOADIND(0, 122, 3, 4, 60);
            case 1:
                TTI_LOADIND(0, 122, 3, 0, 60);
        }

        // Synchronization barrier for low parallelism scenarios
        if constexpr (pipeline_factor < 4 && histogram_num_cores == 1)
        {
            TTI_STALLWAIT(ckernel::p_stall::STALL_THREAD, ckernel::p_stall::ALL_THREAD_RES);
        }

        // Phase 2: Set increment values (1) for histogram buckets
        switch (pipeline_factor)
        {
            case 4:
                TTI_SETDMAREG(0, 1, 0, 62);
                TTI_SETDMAREG(0, 1, 0, 60);
                TTI_SETDMAREG(0, 1, 0, 58);
                TTI_SETDMAREG(0, 1, 0, 56);
                TTI_SETDMAREG(0, 1, 0, 54);
                TTI_SETDMAREG(0, 1, 0, 52);
                TTI_SETDMAREG(0, 1, 0, 50);
                TTI_SETDMAREG(0, 1, 0, 48);
            case 2:
                TTI_SETDMAREG(0, 1, 0, 46);
                TTI_SETDMAREG(0, 1, 0, 42);
                TTI_SETDMAREG(0, 1, 0, 44);
                TTI_SETDMAREG(0, 1, 0, 40);
            case 1:
                TTI_SETDMAREG(0, 1, 0, 38);
                TTI_SETDMAREG(0, 1, 0, 36);
                TTI_SETDMAREG(0, 1, 0, 34);
                TTI_SETDMAREG(0, 1, 0, 32);
        }

        // Phase 3: Add bucket base address to input values
        switch (pipeline_factor)
        {
            case 4:
                TTI_ADDDMAREG(0, 15, 15, 62);
                TTI_ADDDMAREG(0, 14, 14, 62);
                TTI_ADDDMAREG(0, 13, 13, 62);
                TTI_ADDDMAREG(0, 12, 12, 62);
                TTI_ADDDMAREG(0, 11, 11, 62);
                TTI_ADDDMAREG(0, 10, 10, 62);
                TTI_ADDDMAREG(0, 9, 9, 62);
                TTI_ADDDMAREG(0, 8, 8, 62);
            case 2:
                TTI_ADDDMAREG(0, 7, 7, 62);
                TTI_ADDDMAREG(0, 6, 6, 62);
                TTI_ADDDMAREG(0, 5, 5, 62);
                TTI_ADDDMAREG(0, 4, 4, 62);
            case 1:
                TTI_ADDDMAREG(0, 3, 3, 62);
                TTI_ADDDMAREG(0, 2, 2, 62);
                TTI_ADDDMAREG(0, 1, 1, 62);
                TTI_ADDDMAREG(0, 0, 0, 62);
        }

        // Phase 4: Perform atomic histogram increments
        switch (pipeline_factor)
        {
            case 4:
                TTI_ATINCGET(1, 31, 1, 31, 15);
                TTI_ATINCGET(1, 31, 1, 30, 14);
                TTI_ATINCGET(1, 31, 1, 29, 13);
                TTI_ATINCGET(1, 31, 1, 28, 12);
                TTI_ATINCGET(1, 31, 1, 27, 11);
                TTI_ATINCGET(1, 31, 1, 26, 10);
                TTI_ATINCGET(1, 31, 1, 25, 9);
                TTI_ATINCGET(1, 31, 1, 24, 8);
            case 2:
                TTI_ATINCGET(1, 31, 1, 23, 7);
                TTI_ATINCGET(1, 31, 1, 22, 6);
                TTI_ATINCGET(1, 31, 1, 21, 5);
                TTI_ATINCGET(1, 31, 1, 20, 4);
            case 1:
                TTI_ATINCGET(1, 31, 1, 19, 3);
                TTI_ATINCGET(1, 31, 1, 18, 2);
                TTI_ATINCGET(1, 31, 1, 17, 1);
                TTI_ATINCGET(1, 31, 1, 16, 0);
        }
    }

    // Synchronization read from PC buffer
    asm("lw t1, 4(a7)");
}

inline void llk_histogram_run()
{
    // Save registers on stack
    asm("addi sp, sp, -48");
    asm("sw   s0,   0(sp)");
    asm("sw   s1,   4(sp)");
    asm("sw   s2,   8(sp)");
    asm("sw   s3,  12(sp)");
    asm("sw   s4,  16(sp)");
    asm("sw   s5,  20(sp)");
    asm("sw   s6,  24(sp)");
    asm("sw   s7,  28(sp)");
    asm("sw   s8,  32(sp)");
    asm("sw   s9,  36(sp)");
    asm("sw   s10, 40(sp)");
    asm("sw   s11, 44(sp)");

    // Setup registers and addresses
    llk_histogram_setup_registers();

    // Wait for start signal
    llk_histogram_wait_start();

    // Begin timing measurement
    llk_histogram_start_measuring();

    // Execute histogram computation based on pipeline factor
    if constexpr (histogram_pipeline_factor == 1)
    {
        llk_histogram_compute<1>();
    }
    else if constexpr (histogram_pipeline_factor == 2)
    {
        llk_histogram_compute<2>();
    }
    else if constexpr (histogram_pipeline_factor == 4)
    {
        llk_histogram_compute<4>();
    }
    else
    {
        static_assert(histogram_pipeline_factor == 1 || histogram_pipeline_factor == 2 || histogram_pipeline_factor == 4, "Unsupported pipeline factor");
    }

    // End timing measurement
    llk_histogram_stop_measuring();

    // Restore registers from stack
    asm("lw   s0,   0(sp)");
    asm("lw   s1,   4(sp)");
    asm("lw   s2,   8(sp)");
    asm("lw   s3,  12(sp)");
    asm("lw   s4,  16(sp)");
    asm("lw   s5,  20(sp)");
    asm("lw   s6,  24(sp)");
    asm("lw   s7,  28(sp)");
    asm("lw   s8,  32(sp)");
    asm("lw   s9,  36(sp)");
    asm("lw   s10, 40(sp)");
    asm("lw   s11, 44(sp)");
    asm("addi sp, sp, 48");
}

#endif // LLK_TRISC_UNPACK
