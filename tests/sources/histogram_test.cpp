// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0
#include <algorithm>
#include <cstdint>
#include <cstdio>

#include "build.h"
#include "ckernel.h"
#include "llk_defs.h"

#ifdef LLK_TRISC_UNPACK
static constexpr uint32_t core_idx = 0;
#endif

#ifdef LLK_TRISC_MATH
static constexpr uint32_t core_idx = 1;
#endif

#ifdef LLK_TRISC_PACK
static constexpr uint32_t core_idx = 2;
#endif

#ifdef LLK_BRISC
static constexpr uint32_t core_idx = 3;
#endif

static constexpr uint32_t timestamp_address  = 0x00019000;
static constexpr uint32_t start_address      = 0x00019FF0;
static constexpr uint32_t buffer_address     = 0x00020000;
static constexpr uint32_t bucket_address     = 0x00030000;
static constexpr uint32_t wall_clock_address = 0xFFB121F0;
static constexpr uint32_t regfile_address    = 0xFFE00000;
static constexpr uint32_t pc_buf_address     = 0xFFE80000;

static constexpr uint32_t buffer_size     = HISTOGRAM_BUFFER_SIZE;
static constexpr uint32_t num_cores       = HISTOGRAM_NUM_CORES;
static constexpr uint32_t pipeline_factor = HISTOGRAM_PIPELINE_FACTOR;
static constexpr uint32_t version         = HISTOGRAM_VERSION;
//  0 ERR
//  1 ERR SHIFTOPT
//  2 NERR
//  3 NERR SHIFTOPT
// 16 TSXATINC
// 17 TSXATINCPTR
// 18 TSXFULLATINC
// 19 TSXFULLATINCPTR
// 20 TSXFULLLOADSTORE

static constexpr uint32_t buffer_per_core  = buffer_size / num_cores;
static constexpr uint32_t outer_iterations = buffer_per_core / pipeline_factor;
static constexpr uint32_t timestamp_base   = timestamp_address + 4 * sizeof(uint32_t) * core_idx;
static constexpr uint32_t buffer_base      = buffer_address + buffer_per_core * sizeof(uint32_t) * core_idx;

inline void wait_start()
{
    asm("la a3, %0" : : "i"(start_address) : "a3");
start_loop:
    asm("lw  a5, 0(a3)");
    __asm__ goto("beq a5, zero, %l[start_loop]" : : : : start_loop);
}

inline void start_measuring()
{
    asm("fence");
    asm("la a3, %0" : : "i"(wall_clock_address) : "a3");
    asm("la a4, %0" : : "i"(timestamp_base) : "a4");
    asm("lw a5, 0(a3)");
    asm("sw a5, 0(a4)");
    asm("lw a5, 8(a3)");
    asm("sw a5, 4(a4)");
    asm("fence");
}

inline void stop_measuring()
{
    asm("fence");
    asm("la a3, %0" : : "i"(wall_clock_address) : "a3");
    asm("la a4, %0" : : "i"(timestamp_base) : "a4");
    asm("lw a5,  0(a3)");
    asm("sw a5,  8(a4)");
    asm("lw a5,  8(a3)");
    asm("sw a5, 12(a4)");
    asm("fence");
}

#if defined(LLK_TRISC_UNPACK) || HISTOGRAM_NUM_CORES > 1

void run_kernel()
{
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

    if constexpr (version < 16)
    {
        asm("la a0, %0" : : "i"(buffer_base) : "a0");
        asm("la a1, %0" : : "i"(bucket_address) : "a1");
    }
    else
    {
        asm("la a0, %0" : : "i"(buffer_base / 16) : "a0");
        asm("la a1, %0" : : "i"(bucket_address / 16) : "a1");
    }
    asm("la a6, %0" : : "i"(regfile_address) : "a6");
    asm("la a7, %0" : : "i"(pc_buf_address) : "a7");

    asm("la t2, 1");

    asm("sw a0, 240(a6)");
    asm("sw zero, 244(a6)");
    asm("sw a1, 248(a6)");

    wait_start();

    start_measuring();

    if constexpr (version == 0 || version == 1 || version == 2 || version == 3)
    { // ERR, ERR SHIFTOPT, NERR, NERR SHIFTOPT
        for (uint32_t i = 0; i < outer_iterations; i++)
        {
            switch (pipeline_factor)
            {
                case 16:
                    asm("lw t6,  60(a0)");
                    asm("lw t5,  56(a0)");
                    asm("lw t4,  52(a0)");
                    asm("lw t3,  48(a0)");
                    asm("lw s11, 44(a0)");
                    asm("lw s10, 40(a0)");
                    asm("lw s9,  36(a0)");
                    asm("lw s8,  32(a0)");
                case 8:
                    asm("lw s7,  28(a0)");
                    asm("lw s6,  24(a0)");
                    asm("lw s5,  20(a0)");
                    asm("lw s4,  16(a0)");
                case 4:
                    asm("lw s3,  12(a0)");
                    asm("lw s2,   8(a0)");
                case 2:
                    asm("lw s1,   4(a0)");
                case 1:
                    asm("lw s0,   0(a0)");
            }
            switch (pipeline_factor)
            {
                case 16:
                    asm("addi a0, a0, 64");
                    break;
                case 8:
                    asm("addi a0, a0, 32");
                    break;
                case 4:
                    asm("addi a0, a0, 16");
                    break;
                case 2:
                    asm("addi a0, a0, 8");
                    break;
                case 1:
                    asm("addi a0, a0, 4");
                    break;
            }
            if constexpr (version == 0 || version == 2)
            {
                switch (pipeline_factor)
                {
                    case 16:
                        asm("slli t6,  t6,  1");
                        asm("slli t5,  t5,  1");
                        asm("slli t4,  t4,  1");
                        asm("slli t3,  t3,  1");
                        asm("slli s11, s11, 1");
                        asm("slli s10, s10, 1");
                        asm("slli s9,  s9,  1");
                        asm("slli s8,  s8,  1");
                        asm("add  t6,  t6,  a1");
                        asm("add  t5,  t5,  a1");
                        asm("add  t4,  t4,  a1");
                        asm("add  t3,  t3,  a1");
                        asm("add  s11, s11, a1");
                        asm("add  s10, s10, a1");
                        asm("add  s9,  s9,  a1");
                        asm("add  s8,  s8,  a1");
                    case 8:
                        asm("slli s7,  s7,  1");
                        asm("slli s6,  s6,  1");
                        asm("slli s5,  s5,  1");
                        asm("slli s4,  s4,  1");
                        asm("add  s7,  s7,  a1");
                        asm("add  s6,  s6,  a1");
                        asm("add  s5,  s5,  a1");
                        asm("add  s4,  s4,  a1");
                    case 4:
                        asm("slli s3,  s3,  1");
                        asm("slli s2,  s2,  1");
                        asm("add  s3,  s3,  a1");
                        asm("add  s2,  s2,  a1");
                    case 2:
                        asm("slli s1,  s1,  1");
                        asm("add  s1,  s1,  a1");
                    case 1:
                        asm("slli s0,  s0,  1");
                        asm("add  s0,  s0,  a1");
                }
            }
            else
            {
                switch (pipeline_factor)
                {
                    case 16:
                        asm("slli t6,  t6,  1");
                        asm("slli t5,  t5,  1");
                        asm("slli t4,  t4,  1");
                        asm("slli t3,  t3,  1");
                        asm("slli s11, s11, 1");
                        asm("slli s10, s10, 1");
                        asm("slli s9,  s9,  1");
                        asm("slli s8,  s8,  1");
                    case 8:
                        asm("slli s7,  s7,  1");
                        asm("slli s6,  s6,  1");
                        asm("slli s5,  s5,  1");
                        asm("slli s4,  s4,  1");
                    case 4:
                        asm("slli s3,  s3,  1");
                        asm("slli s2,  s2,  1");
                    case 2:
                        asm("slli s1,  s1,  1");
                    case 1:
                        asm("slli s0,  s0,  1");
                }
                switch (pipeline_factor)
                {
                    case 16:
                        asm("add  t6,  t6,  a1");
                        asm("add  t5,  t5,  a1");
                        asm("add  t4,  t4,  a1");
                        asm("add  t3,  t3,  a1");
                        asm("add  s11, s11, a1");
                        asm("add  s10, s10, a1");
                        asm("add  s9,  s9,  a1");
                        asm("add  s8,  s8,  a1");
                    case 8:
                        asm("add  s7,  s7,  a1");
                        asm("add  s6,  s6,  a1");
                        asm("add  s5,  s5,  a1");
                        asm("add  s4,  s4,  a1");
                    case 4:
                        asm("add  s3,  s3,  a1");
                        asm("add  s2,  s2,  a1");
                    case 2:
                        asm("add  s1,  s1,  a1");
                    case 1:
                        asm("add  s0,  s0,  a1");
                }
            }
            if constexpr (version == 0 || version == 1)
            {
                switch (pipeline_factor)
                {
                    case 8:
                        asm("lh t6,  0(s7)");
                        asm("lh t5,  0(s6)");
                        asm("lh t4,  0(s5)");
                        asm("lh t3,  0(s4)");
                    case 4:
                        asm("lh s11, 0(s3)");
                        asm("lh s10, 0(s2)");
                    case 2:
                        asm("lh s9,  0(s1)");
                    case 1:
                        asm("lh s8,  0(s0)");
                }
                switch (pipeline_factor)
                {
                    case 8:
                        asm("addi t6,  t6,  1");
                        asm("addi t5,  t5,  1");
                        asm("addi t4,  t4,  1");
                        asm("addi t3,  t3,  1");
                    case 4:
                        asm("addi s11, s11, 1");
                        asm("addi s10, s10, 1");
                    case 2:
                        asm("addi s9,  s9,  1");
                    case 1:
                        asm("addi s8,  s8,  1");
                }
                switch (pipeline_factor)
                {
                    case 8:
                        asm("sh t6,  0(s7)");
                        asm("sh t5,  0(s6)");
                        asm("sh t4,  0(s5)");
                        asm("sh t3,  0(s4)");
                    case 4:
                        asm("sh s11, 0(s3)");
                        asm("sh s10, 0(s2)");
                    case 2:
                        asm("sh s9,  0(s1)");
                    case 1:
                        asm("sh s8,  0(s0)");
                }
            }
            else
            {
                switch (pipeline_factor)
                {
                    case 16:
                        asm("lh   t2, 0(t6)");
                        asm("addi t2, t2, 1");
                        asm("sh   t2, 0(t6)");
                        asm("lh   t2, 0(t5)");
                        asm("addi t2, t2, 1");
                        asm("sh   t2, 0(t5)");
                        asm("lh   t2, 0(t4)");
                        asm("addi t2, t2, 1");
                        asm("sh   t2, 0(t4)");
                        asm("lh   t2, 0(t3)");
                        asm("addi t2, t2, 1");
                        asm("sh   t2, 0(t3)");
                        asm("lh   t2, 0(s11)");
                        asm("addi t2, t2, 1");
                        asm("sh   t2, 0(s11)");
                        asm("lh   t2, 0(s10)");
                        asm("addi t2, t2, 1");
                        asm("sh   t2, 0(s10)");
                        asm("lh   t2, 0(s9)");
                        asm("addi t2, t2, 1");
                        asm("sh   t2, 0(s9)");
                        asm("lh   t2, 0(s8)");
                        asm("addi t2, t2, 1");
                        asm("sh   t2, 0(s8)");
                    case 8:
                        asm("lh   t2, 0(s7)");
                        asm("addi t2, t2, 1");
                        asm("sh   t2, 0(s7)");
                        asm("lh   t2, 0(s6)");
                        asm("addi t2, t2, 1");
                        asm("sh   t2, 0(s6)");
                        asm("lh   t2, 0(s5)");
                        asm("addi t2, t2, 1");
                        asm("sh   t2, 0(s5)");
                        asm("lh   t2, 0(s4)");
                        asm("addi t2, t2, 1");
                        asm("sh   t2, 0(s4)");
                    case 4:
                        asm("lh   t2, 0(s3)");
                        asm("addi t2, t2, 1");
                        asm("sh   t2, 0(s3)");
                        asm("lh   t2, 0(s2)");
                        asm("addi t2, t2, 1");
                        asm("sh   t2, 0(s2)");
                    case 2:
                        asm("lh   t2, 0(s1)");
                        asm("addi t2, t2, 1");
                        asm("sh   t2, 0(s1)");
                    case 1:
                        asm("lh   t2, 0(s0)");
                        asm("addi t2, t2, 1");
                        asm("sh   t2, 0(s0)");
                }
            }
        }
    }
    else if constexpr (version == 16 || version == 17)
    { // TSXATINC TSXATINCPTR
        for (uint32_t i = 0; i < outer_iterations / 16; i++)
        {
            switch (pipeline_factor)
            {
                case 4:
                    asm("lw t6,  252(a0)");
                    asm("lw t5,  248(a0)");
                    asm("lw t4,  244(a0)");
                    asm("lw t3,  240(a0)");
                    asm("lw s11, 236(a0)");
                    asm("lw s10, 232(a0)");
                    asm("lw s9,  228(a0)");
                    asm("lw s8,  224(a0)");
                    asm("lw s7,  220(a0)");
                    asm("lw s6,  216(a0)");
                    asm("lw s5,  212(a0)");
                    asm("lw s4,  208(a0)");
                    asm("lw s3,  204(a0)");
                    asm("lw s2,  200(a0)");
                    asm("lw s1,  196(a0)");
                    asm("lw s0,  192(a0)");

                    asm("add  t6,  t6,  a1");
                    asm("add  t5,  t5,  a1");
                    asm("add  t4,  t4,  a1");
                    asm("add  t3,  t3,  a1");
                    asm("add  s11, s11, a1");
                    asm("add  s10, s10, a1");
                    asm("add  s9,  s9,  a1");
                    asm("add  s8,  s8,  a1");
                    asm("add  s7,  s7,  a1");
                    asm("add  s6,  s6,  a1");
                    asm("add  s5,  s5,  a1");
                    asm("add  s4,  s4,  a1");
                    asm("add  s3,  s3,  a1");
                    asm("add  s2,  s2,  a1");
                    asm("add  s1,  s1,  a1");
                    asm("add  s0,  s0,  a1");

                    asm("sw t6,  252(a6)");
                    asm("sw t5,  248(a6)");
                    asm("sw t4,  244(a6)");
                    asm("sw t3,  240(a6)");
                    asm("sw s11, 236(a6)");
                    asm("sw s10, 232(a6)");
                    asm("sw s9,  228(a6)");
                    asm("sw s8,  224(a6)");
                    asm("sw s7,  220(a6)");
                    asm("sw s6,  216(a6)");
                    asm("sw s5,  212(a6)");
                    asm("sw s4,  208(a6)");
                    asm("sw s3,  204(a6)");
                    asm("sw s2,  200(a6)");
                    asm("sw s1,  196(a6)");
                    asm("sw s0,  192(a6)");

                    asm("lw t6,  188(a0)");
                    asm("lw t5,  184(a0)");
                    asm("lw t4,  180(a0)");
                    asm("lw t3,  176(a0)");
                    asm("lw s11, 172(a0)");
                    asm("lw s10, 168(a0)");
                    asm("lw s9,  164(a0)");
                    asm("lw s8,  160(a0)");
                    asm("lw s7,  156(a0)");
                    asm("lw s6,  152(a0)");
                    asm("lw s5,  148(a0)");
                    asm("lw s4,  144(a0)");
                    asm("lw s3,  140(a0)");
                    asm("lw s2,  136(a0)");
                    asm("lw s1,  132(a0)");
                    asm("lw s0,  128(a0)");

                    asm("add  t6,  t6,  a1");
                    asm("add  t5,  t5,  a1");
                    asm("add  t4,  t4,  a1");
                    asm("add  t3,  t3,  a1");
                    asm("add  s11, s11, a1");
                    asm("add  s10, s10, a1");
                    asm("add  s9,  s9,  a1");
                    asm("add  s8,  s8,  a1");
                    asm("add  s7,  s7,  a1");
                    asm("add  s6,  s6,  a1");
                    asm("add  s5,  s5,  a1");
                    asm("add  s4,  s4,  a1");
                    asm("add  s3,  s3,  a1");
                    asm("add  s2,  s2,  a1");
                    asm("add  s1,  s1,  a1");
                    asm("add  s0,  s0,  a1");

                    asm("sw t6,  188(a6)");
                    asm("sw t5,  184(a6)");
                    asm("sw t4,  180(a6)");
                    asm("sw t3,  176(a6)");
                    asm("sw s11, 172(a6)");
                    asm("sw s10, 168(a6)");
                    asm("sw s9,  164(a6)");
                    asm("sw s8,  160(a6)");
                    asm("sw s7,  156(a6)");
                    asm("sw s6,  152(a6)");
                    asm("sw s5,  148(a6)");
                    asm("sw s4,  144(a6)");
                    asm("sw s3,  140(a6)");
                    asm("sw s2,  136(a6)");
                    asm("sw s1,  132(a6)");
                    asm("sw s0,  128(a6)");
                case 2:
                    asm("lw t6,  124(a0)");
                    asm("lw t5,  120(a0)");
                    asm("lw t4,  116(a0)");
                    asm("lw t3,  112(a0)");
                    asm("lw s11, 108(a0)");
                    asm("lw s10, 104(a0)");
                    asm("lw s9,  100(a0)");
                    asm("lw s8,   96(a0)");
                    asm("lw s7,   92(a0)");
                    asm("lw s6,   88(a0)");
                    asm("lw s5,   84(a0)");
                    asm("lw s4,   80(a0)");
                    asm("lw s3,   76(a0)");
                    asm("lw s2,   72(a0)");
                    asm("lw s1,   68(a0)");
                    asm("lw s0,   64(a0)");

                    asm("add  t6,  t6,  a1");
                    asm("add  t5,  t5,  a1");
                    asm("add  t4,  t4,  a1");
                    asm("add  t3,  t3,  a1");
                    asm("add  s11, s11, a1");
                    asm("add  s10, s10, a1");
                    asm("add  s9,  s9,  a1");
                    asm("add  s8,  s8,  a1");
                    asm("add  s7,  s7,  a1");
                    asm("add  s6,  s6,  a1");
                    asm("add  s5,  s5,  a1");
                    asm("add  s4,  s4,  a1");
                    asm("add  s3,  s3,  a1");
                    asm("add  s2,  s2,  a1");
                    asm("add  s1,  s1,  a1");
                    asm("add  s0,  s0,  a1");

                    asm("sw t6,  124(a6)");
                    asm("sw t5,  120(a6)");
                    asm("sw t4,  116(a6)");
                    asm("sw t3,  112(a6)");
                    asm("sw s11, 108(a6)");
                    asm("sw s10, 104(a6)");
                    asm("sw s9,  100(a6)");
                    asm("sw s8,   96(a6)");
                    asm("sw s7,   92(a6)");
                    asm("sw s6,   88(a6)");
                    asm("sw s5,   84(a6)");
                    asm("sw s4,   80(a6)");
                    asm("sw s3,   76(a6)");
                    asm("sw s2,   72(a6)");
                    asm("sw s1,   68(a6)");
                    asm("sw s0,   64(a6)");
                case 1:
                    asm("lw t6,  60(a0)");
                    asm("lw t5,  56(a0)");
                    asm("lw t4,  52(a0)");
                    asm("lw t3,  48(a0)");
                    asm("lw s11, 44(a0)");
                    asm("lw s10, 40(a0)");
                    asm("lw s9,  36(a0)");
                    asm("lw s8,  32(a0)");
                    asm("lw s7,  28(a0)");
                    asm("lw s6,  24(a0)");
                    asm("lw s5,  20(a0)");
                    asm("lw s4,  16(a0)");
                    asm("lw s3,  12(a0)");
                    asm("lw s2,   8(a0)");
                    asm("lw s1,   4(a0)");
                    asm("lw s0,   0(a0)");

                    asm("add  t6,  t6,  a1");
                    asm("add  t5,  t5,  a1");
                    asm("add  t4,  t4,  a1");
                    asm("add  t3,  t3,  a1");
                    asm("add  s11, s11, a1");
                    asm("add  s10, s10, a1");
                    asm("add  s9,  s9,  a1");
                    asm("add  s8,  s8,  a1");
                    asm("add  s7,  s7,  a1");
                    asm("add  s6,  s6,  a1");
                    asm("add  s5,  s5,  a1");
                    asm("add  s4,  s4,  a1");
                    asm("add  s3,  s3,  a1");
                    asm("add  s2,  s2,  a1");
                    asm("add  s1,  s1,  a1");
                    asm("add  s0,  s0,  a1");

                    asm("sw t6,  60(a6)");
                    asm("sw t5,  56(a6)");
                    asm("sw t4,  52(a6)");
                    asm("sw t3,  48(a6)");
                    asm("sw s11, 44(a6)");
                    asm("sw s10, 40(a6)");
                    asm("sw s9,  36(a6)");
                    asm("sw s8,  32(a6)");
                    asm("sw s7,  28(a6)");
                    asm("sw s6,  24(a6)");
                    asm("sw s5,  20(a6)");
                    asm("sw s4,  16(a6)");
                    asm("sw s3,  12(a6)");
                    asm("sw s2,   8(a6)");
                    asm("sw s1,   4(a6)");
                    asm("sw s0,   0(a6)");
            }
            switch (pipeline_factor)
            {
                case 4:
                    asm("addi a0, a0, 256");
                    break;
                case 2:
                    asm("addi a0, a0, 128");
                    break;
                case 1:
                    asm("addi a0, a0, 64");
                    break;
            }
            if constexpr (version == 16)
            {
                switch (pipeline_factor)
                {
                    case 2:
                        asm("sw t2, 252(a6)");
                        asm("sw t2, 248(a6)");
                        asm("sw t2, 244(a6)");
                        asm("sw t2, 240(a6)");
                        asm("sw t2, 236(a6)");
                        asm("sw t2, 232(a6)");
                        asm("sw t2, 228(a6)");
                        asm("sw t2, 224(a6)");
                        asm("sw t2, 220(a6)");
                        asm("sw t2, 216(a6)");
                        asm("sw t2, 212(a6)");
                        asm("sw t2, 208(a6)");
                        asm("sw t2, 204(a6)");
                        asm("sw t2, 200(a6)");
                        asm("sw t2, 196(a6)");
                        asm("sw t2, 192(a6)");
                    case 1:
                        asm("sw t2, 188(a6)");
                        asm("sw t2, 184(a6)");
                        asm("sw t2, 180(a6)");
                        asm("sw t2, 176(a6)");
                        asm("sw t2, 172(a6)");
                        asm("sw t2, 168(a6)");
                        asm("sw t2, 164(a6)");
                        asm("sw t2, 160(a6)");
                        asm("sw t2, 156(a6)");
                        asm("sw t2, 152(a6)");
                        asm("sw t2, 148(a6)");
                        asm("sw t2, 144(a6)");
                        asm("sw t2, 140(a6)");
                        asm("sw t2, 136(a6)");
                        asm("sw t2, 132(a6)");
                        asm("sw t2, 128(a6)");
                }
            }
            if constexpr (version == 16)
            {
                switch (pipeline_factor)
                {
                    case 2:
                        TTI_ATINCGET(1, 31, 1, 63, 31);
                        TTI_ATINCGET(1, 31, 1, 62, 30);
                        TTI_ATINCGET(1, 31, 1, 61, 29);
                        TTI_ATINCGET(1, 31, 1, 60, 28);
                        TTI_ATINCGET(1, 31, 1, 59, 27);
                        TTI_ATINCGET(1, 31, 1, 58, 26);
                        TTI_ATINCGET(1, 31, 1, 57, 25);
                        TTI_ATINCGET(1, 31, 1, 56, 24);
                        TTI_ATINCGET(1, 31, 1, 55, 23);
                        TTI_ATINCGET(1, 31, 1, 54, 22);
                        TTI_ATINCGET(1, 31, 1, 53, 21);
                        TTI_ATINCGET(1, 31, 1, 52, 20);
                        TTI_ATINCGET(1, 31, 1, 51, 19);
                        TTI_ATINCGET(1, 31, 1, 50, 18);
                        TTI_ATINCGET(1, 31, 1, 49, 17);
                        TTI_ATINCGET(1, 31, 1, 48, 16);
                    case 1:
                        TTI_ATINCGET(1, 31, 1, 47, 15);
                        TTI_ATINCGET(1, 31, 1, 46, 14);
                        TTI_ATINCGET(1, 31, 1, 45, 13);
                        TTI_ATINCGET(1, 31, 1, 44, 12);
                        TTI_ATINCGET(1, 31, 1, 43, 11);
                        TTI_ATINCGET(1, 31, 1, 42, 10);
                        TTI_ATINCGET(1, 31, 1, 41, 9);
                        TTI_ATINCGET(1, 31, 1, 40, 8);
                        TTI_ATINCGET(1, 31, 1, 39, 7);
                        TTI_ATINCGET(1, 31, 1, 38, 6);
                        TTI_ATINCGET(1, 31, 1, 37, 5);
                        TTI_ATINCGET(1, 31, 1, 36, 4);
                        TTI_ATINCGET(1, 31, 1, 35, 3);
                        TTI_ATINCGET(1, 31, 1, 34, 2);
                        TTI_ATINCGET(1, 31, 1, 33, 1);
                        TTI_ATINCGET(1, 31, 1, 32, 0);
                }
            }
            else
            {
                switch (pipeline_factor)
                {
                    case 4:
                        TTI_ATINCGETPTR(1, 0, 0, 31, 1, 63, 63);
                        TTI_ATINCGETPTR(1, 0, 0, 31, 1, 62, 62);
                        TTI_ATINCGETPTR(1, 0, 0, 31, 1, 61, 61);
                        TTI_ATINCGETPTR(1, 0, 0, 31, 1, 60, 60);
                        TTI_ATINCGETPTR(1, 0, 0, 31, 1, 59, 59);
                        TTI_ATINCGETPTR(1, 0, 0, 31, 1, 58, 58);
                        TTI_ATINCGETPTR(1, 0, 0, 31, 1, 57, 57);
                        TTI_ATINCGETPTR(1, 0, 0, 31, 1, 56, 56);
                        TTI_ATINCGETPTR(1, 0, 0, 31, 1, 55, 55);
                        TTI_ATINCGETPTR(1, 0, 0, 31, 1, 54, 54);
                        TTI_ATINCGETPTR(1, 0, 0, 31, 1, 53, 53);
                        TTI_ATINCGETPTR(1, 0, 0, 31, 1, 52, 52);
                        TTI_ATINCGETPTR(1, 0, 0, 31, 1, 51, 51);
                        TTI_ATINCGETPTR(1, 0, 0, 31, 1, 50, 50);
                        TTI_ATINCGETPTR(1, 0, 0, 31, 1, 49, 49);
                        TTI_ATINCGETPTR(1, 0, 0, 31, 1, 48, 48);

                        TTI_ATINCGETPTR(1, 0, 0, 31, 1, 47, 47);
                        TTI_ATINCGETPTR(1, 0, 0, 31, 1, 46, 46);
                        TTI_ATINCGETPTR(1, 0, 0, 31, 1, 45, 45);
                        TTI_ATINCGETPTR(1, 0, 0, 31, 1, 44, 44);
                        TTI_ATINCGETPTR(1, 0, 0, 31, 1, 43, 43);
                        TTI_ATINCGETPTR(1, 0, 0, 31, 1, 42, 42);
                        TTI_ATINCGETPTR(1, 0, 0, 31, 1, 41, 41);
                        TTI_ATINCGETPTR(1, 0, 0, 31, 1, 40, 40);
                        TTI_ATINCGETPTR(1, 0, 0, 31, 1, 39, 39);
                        TTI_ATINCGETPTR(1, 0, 0, 31, 1, 38, 38);
                        TTI_ATINCGETPTR(1, 0, 0, 31, 1, 37, 37);
                        TTI_ATINCGETPTR(1, 0, 0, 31, 1, 36, 36);
                        TTI_ATINCGETPTR(1, 0, 0, 31, 1, 35, 35);
                        TTI_ATINCGETPTR(1, 0, 0, 31, 1, 34, 34);
                        TTI_ATINCGETPTR(1, 0, 0, 31, 1, 33, 33);
                        TTI_ATINCGETPTR(1, 0, 0, 31, 1, 32, 32);
                    case 2:
                        TTI_ATINCGETPTR(1, 0, 0, 31, 1, 31, 31);
                        TTI_ATINCGETPTR(1, 0, 0, 31, 1, 30, 30);
                        TTI_ATINCGETPTR(1, 0, 0, 31, 1, 29, 29);
                        TTI_ATINCGETPTR(1, 0, 0, 31, 1, 28, 28);
                        TTI_ATINCGETPTR(1, 0, 0, 31, 1, 27, 27);
                        TTI_ATINCGETPTR(1, 0, 0, 31, 1, 26, 26);
                        TTI_ATINCGETPTR(1, 0, 0, 31, 1, 25, 25);
                        TTI_ATINCGETPTR(1, 0, 0, 31, 1, 24, 24);
                        TTI_ATINCGETPTR(1, 0, 0, 31, 1, 23, 23);
                        TTI_ATINCGETPTR(1, 0, 0, 31, 1, 22, 22);
                        TTI_ATINCGETPTR(1, 0, 0, 31, 1, 21, 21);
                        TTI_ATINCGETPTR(1, 0, 0, 31, 1, 20, 20);
                        TTI_ATINCGETPTR(1, 0, 0, 31, 1, 19, 19);
                        TTI_ATINCGETPTR(1, 0, 0, 31, 1, 18, 18);
                        TTI_ATINCGETPTR(1, 0, 0, 31, 1, 17, 17);
                        TTI_ATINCGETPTR(1, 0, 0, 31, 1, 16, 16);
                    case 1:
                        TTI_ATINCGETPTR(1, 0, 0, 31, 1, 15, 15);
                        TTI_ATINCGETPTR(1, 0, 0, 31, 1, 14, 14);
                        TTI_ATINCGETPTR(1, 0, 0, 31, 1, 13, 13);
                        TTI_ATINCGETPTR(1, 0, 0, 31, 1, 12, 12);
                        TTI_ATINCGETPTR(1, 0, 0, 31, 1, 11, 11);
                        TTI_ATINCGETPTR(1, 0, 0, 31, 1, 10, 10);
                        TTI_ATINCGETPTR(1, 0, 0, 31, 1, 9, 9);
                        TTI_ATINCGETPTR(1, 0, 0, 31, 1, 8, 8);
                        TTI_ATINCGETPTR(1, 0, 0, 31, 1, 7, 7);
                        TTI_ATINCGETPTR(1, 0, 0, 31, 1, 6, 6);
                        TTI_ATINCGETPTR(1, 0, 0, 31, 1, 5, 5);
                        TTI_ATINCGETPTR(1, 0, 0, 31, 1, 4, 4);
                        TTI_ATINCGETPTR(1, 0, 0, 31, 1, 3, 3);
                        TTI_ATINCGETPTR(1, 0, 0, 31, 1, 2, 2);
                        TTI_ATINCGETPTR(1, 0, 0, 31, 1, 1, 1);
                        TTI_ATINCGETPTR(1, 0, 0, 31, 1, 0, 0);
                }
            }

            asm("lw t1, 4(a7)");
        }
    }
    else if constexpr (version == 18 || version == 19)
    { // TSXFULLATINC TSXFULLATINCPTR
        for (uint32_t i = 0; i < outer_iterations / 4; i++)
        {
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
            if constexpr (pipeline_factor < 4 && num_cores == 1)
            {
                TTI_STALLWAIT(ckernel::p_stall::STALL_THREAD, ckernel::p_stall::ALL_THREAD_RES);
            }
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

        asm("lw t1, 4(a7)");
        /*asm("slli a0, a0, 4");
        asm("lw t1, 0(a6)");
        asm("sw t1, 0(a0)");
        asm("lw t1, 4(a6)");
        asm("sw t1, 4(a0)");
        asm("lw t1, 8(a6)");
        asm("sw t1, 8(a0)");
        asm("lw t1, 12(a6)");
        asm("sw t1, 12(a0)");
        asm("lw t1, 16(a6)");
        asm("sw t1, 16(a0)");
        asm("lw t1, 20(a6)");
        asm("sw t1, 20(a0)");
        asm("lw t1, 24(a6)");
        asm("sw t1, 24(a0)");
        asm("lw t1, 28(a6)");
        asm("sw t1, 28(a0)");
        asm("lw t1, 32(a6)");
        asm("sw t1, 32(a0)");
        asm("lw t1, 36(a6)");
        asm("sw t1, 36(a0)");
        asm("lw t1, 40(a6)");
        asm("sw t1, 40(a0)");
        asm("lw t1, 44(a6)");
        asm("sw t1, 44(a0)");
        asm("lw t1, 48(a6)");
        asm("sw t1, 48(a0)");
        asm("lw t1, 52(a6)");
        asm("sw t1, 52(a0)");
        asm("lw t1, 56(a6)");
        asm("sw t1, 56(a0)");
        asm("lw t1, 60(a6)");
        asm("sw t1, 60(a0)");

        asm("lw t1, 64(a6)");
        asm("sw t1, 64(a0)");
        asm("lw t1, 68(a6)");
        asm("sw t1, 68(a0)");
        asm("lw t1, 72(a6)");
        asm("sw t1, 72(a0)");
        asm("lw t1, 76(a6)");
        asm("sw t1, 76(a0)");
        asm("lw t1, 80(a6)");
        asm("sw t1, 80(a0)");
        asm("lw t1, 84(a6)");
        asm("sw t1, 84(a0)");
        asm("lw t1, 88(a6)");
        asm("sw t1, 88(a0)");
        asm("lw t1, 92(a6)");
        asm("sw t1, 92(a0)");
        asm("lw t1, 96(a6)");
        asm("sw t1, 96(a0)");
        asm("lw t1, 100(a6)");
        asm("sw t1, 100(a0)");
        asm("lw t1, 104(a6)");
        asm("sw t1, 104(a0)");
        asm("lw t1, 108(a6)");
        asm("sw t1, 108(a0)");
        asm("lw t1, 112(a6)");
        asm("sw t1, 112(a0)");
        asm("lw t1, 116(a6)");
        asm("sw t1, 116(a0)");
        asm("lw t1, 120(a6)");
        asm("sw t1, 120(a0)");
        asm("lw t1, 124(a6)");
        asm("sw t1, 124(a0)");

        asm("lw t1, 240(a6)");
        asm("sw t1, 240(a0)");

        asm("lw t1, 244(a6)");
        asm("sw t1, 244(a0)");

        asm("lw t1, 248(a6)");
        asm("sw t1, 248(a0)");*/
    }

    stop_measuring();

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

#endif

#if defined(LLK_TRISC_MATH) && HISTOGRAM_NUM_CORES == 1
void run_kernel()
{
}
#endif

#if defined(LLK_TRISC_PACK) && HISTOGRAM_NUM_CORES == 1
void run_kernel()
{
}
#endif

#if defined(LLK_BRISC) && HISTOGRAM_NUM_CORES == 1
void run_kernel()
{
}
#endif
