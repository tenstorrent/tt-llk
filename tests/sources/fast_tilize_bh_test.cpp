// SPDX-FileCopyrightText: © 2026 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

// BH full fast-tilize: unpack + math + pack → standard tilized output.
// Supports arbitrary ct_dim >= 2 via stride-preserving unit_dim={4,2,3}.
// Width 1 uses standard tilize fallback.
// Supports PERF_RUN_TYPE for performance measurements.

#include <cstdint>

#include "ckernel.h"
#include "llk_defs.h"
#include "params.h"
#include "profiler.h"

#ifndef PERF_RUN_TYPE
#define PERF_RUN_TYPE PerfRunType::L1_TO_L1
#endif
#include "perf.h"

std::uint32_t unp_cfg_context          = 0;
std::uint32_t pack_sync_tile_dst_ptr   = 0;
std::uint32_t math_sync_tile_dst_index = 0;

// Row decomposition: split ct_dim into {4, 2, 3} unit sequence.
// Each unit produces the same DEST layout (stale gaps for unused W positions).
constexpr std::uint32_t MAX_UNITS_PER_ROW = 16;

inline std::uint32_t decompose_row(const std::uint32_t ct_dim, std::uint32_t unit_dims[MAX_UNITS_PER_ROW])
{
    std::uint32_t n4  = ct_dim / 4;
    std::uint32_t rem = ct_dim % 4;
    std::uint32_t idx = 0;

    if (rem == 1 && n4 > 0)
    {
        n4--;
        rem = 5; // rewrite 4+1 as 2+3
    }

    for (std::uint32_t i = 0; i < n4; i++)
    {
        unit_dims[idx++] = 4;
    }

    if (rem == 2)
    {
        unit_dims[idx++] = 2;
    }
    else if (rem == 3)
    {
        unit_dims[idx++] = 3;
    }
    else if (rem == 5)
    {
        unit_dims[idx++] = 2;
        unit_dims[idx++] = 3;
    }
    return idx;
}

#ifdef LLK_TRISC_UNPACK

#include "llk_unpack_common.h"
#include "llk_unpack_tilize.h"

void run_kernel(RUNTIME_PARAMETERS params)
{
#ifndef SPEED_OF_LIGHT
    const std::uint32_t BLOCK_CT_DIM = params.BLOCK_CT_DIM;
    const std::uint32_t BLOCK_RT_DIM = params.BLOCK_RT_DIM;
    const std::uint32_t LOOP_FACTOR  = params.LOOP_FACTOR;
    const Operand& buffer_A          = params.buffer_A;
#endif

    std::uint32_t unit_dims[MAX_UNITS_PER_ROW];
    std::uint32_t units_per_row = decompose_row(BLOCK_CT_DIM, unit_dims);

    // Width 1 fallback
    if (BLOCK_CT_DIM == 1)
    {
        {
            ZONE_SCOPED("INIT")
            _llk_unpack_hw_configure_<is_fp32_dest_acc_en>(
                formats.unpack_A_src, formats.unpack_B_src, formats.unpack_A_dst, formats.unpack_B_dst, FACE_R_DIM, FACE_R_DIM, 4, 4);
            _llk_unpack_tilize_init_(formats.unpack_A_src, formats.unpack_A_dst, BLOCK_CT_DIM, FACE_R_DIM, false);
        }
        {
            ZONE_SCOPED("TILE_LOOP")
            if constexpr (PERF_RUN_TYPE != PerfRunType::PACK_ISOLATE)
            {
                for (std::uint32_t loop = 0; loop < LOOP_FACTOR; loop++)
                {
                    for (std::uint32_t row = 0; row < BLOCK_RT_DIM; row++)
                    {
                        _llk_unpack_tilize_(
                            L1_ADDRESS(buffer_A[0]), row * BLOCK_CT_DIM * TILE_R_DIM, formats.unpack_A_src, formats.unpack_A_dst, 0, FACE_R_DIM, 4, false);
                    }
                }
            }
        }
        {
            ZONE_SCOPED("UNINIT")
            _llk_unpack_tilize_uninit_(formats.unpack_A_dst, 4, FACE_R_DIM);
        }
        return;
    }

    // Fast-tilize forces compat 16-bit DEST. For dest_acc=Yes with non-Float16_b input,
    // override unpack_A_dst to Float16_b to get correct CH1_Z stride and SrcA format.
    const std::uint32_t fast_tilize_unpack_dst = is_fp32_dest_acc_en ? ckernel::to_underlying(DataFormat::Float16_b) : formats.unpack_A_dst;

    {
        ZONE_SCOPED("INIT")
        _llk_unpack_hw_configure_<is_fp32_dest_acc_en>(
            formats.unpack_A_src, formats.unpack_B_src, fast_tilize_unpack_dst, formats.unpack_B_dst, FACE_R_DIM, FACE_R_DIM, 4, 4);
        _llk_unpack_fast_tilize_init_(fast_tilize_unpack_dst, BLOCK_CT_DIM, unit_dims[0]);
        volatile std::uint32_t tt_reg_ptr* cfg   = get_cfg_pointer();
        cfg[THCON_SEC0_REG3_Base_address_ADDR32] = L1_ADDRESS(buffer_A[0]);
    }
    {
        ZONE_SCOPED("TILE_LOOP")
        if constexpr (PERF_RUN_TYPE == PerfRunType::PACK_ISOLATE)
        {
            return;
        }
        else if constexpr (PERF_RUN_TYPE == PerfRunType::MATH_ISOLATE)
        {
            // Each unit produces 4 dvalids regardless of unit_dim
            std::uint32_t total_units = BLOCK_RT_DIM * units_per_row;
            return _perf_unpack_loop_set_valid<true, false>(total_units * 4 * LOOP_FACTOR);
        }
        else
        {
            // Count 4-wide prefix units per row
            std::uint32_t n4_per_row = 0;
            for (std::uint32_t i = 0; i < units_per_row; i++)
            {
                if (unit_dims[i] == 4)
                {
                    n4_per_row++;
                }
            }

            std::uint32_t n4_total = BLOCK_RT_DIM * n4_per_row;
            bool has_tail          = (n4_per_row < units_per_row);

            for (std::uint32_t loop = 0; loop < LOOP_FACTOR; loop++)
            {
                // Phase 1: all 4-wide prefix units (uses existing block function)
                if (n4_total > 0)
                {
                    if (loop > 0 && has_tail)
                    {
                        // Restore 4-wide X counter after tail phase
                        _llk_unpack_fast_tilize_reinit_xdim_(4);
                    }
                    // Pass prefix_tiles as ct_dim so block's units_per_row = prefix_tiles/4
                    std::uint32_t prefix_ct = n4_per_row * 4;
                    _llk_unpack_fast_tilize_block_(L1_ADDRESS(buffer_A[0]), 0, formats.unpack_A_src, 4, n4_total, prefix_ct, 4);
                }

                // Phase 2: all tail units (2-wide or 3-wide)
                if (has_tail)
                {
                    // Reconfigure X counter for tail unit_dim
                    // Process tail units across all rows with direct MOP calls
                    for (std::uint32_t row = 0; row < BLOCK_RT_DIM; row++)
                    {
                        std::uint32_t col_offset = n4_per_row * 4; // tiles, in Y counter units

                        for (std::uint32_t u = n4_per_row; u < units_per_row; u++)
                        {
                            std::uint32_t udim = unit_dims[u];

                            if (u == n4_per_row && row == 0)
                            {
                                // First tail unit: reconfigure and set counters
                                _llk_unpack_fast_tilize_reinit_xdim_(udim);
                                TTI_SETADCXY(p_setadc::UNP_A, 0, 0, 0, 0, 0b0011);
                                TTI_SETADCZW(p_setadc::UNP_A, 0, 0, 0, 0, 0b1111);
                            }
                            else if (u == n4_per_row && row > 0)
                            {
                                // New row: advance W, reset Y/Z
                                TTI_ADDRCRXY(p_setadc::UNP_A, 0, 0, 0, 0, 0b0010);
                                TTI_ADDRCRZW(p_setadc::UNP_A, 0, 0, 2, 0, 0b0111);
                            }

                            if (u > n4_per_row)
                            {
                                // Switch unit_dim if needed
                                if (unit_dims[u] != unit_dims[u - 1])
                                {
                                    _llk_unpack_fast_tilize_reinit_xdim_(udim);
                                }
                                TTI_SETADCZW(p_setadc::UNP_A, 0, 0, 0, 0, 0b0101);
                                TT_INCADCXY(p_setadc::UNP_A, 0, 0, unit_dims[u - 1], 0); // Advance by prev unit's actual width
                            }

                            // Set Y to correct position (after prefix)
                            if (u == n4_per_row && row == 0)
                            {
                                if (col_offset > 0)
                                {
                                    TT_INCADCXY(p_setadc::UNP_A, 0, 0, col_offset, 0);
                                }
                            }

                            // Fire MOP: same 32 reads, same zmask as 4-wide
                            constexpr std::uint32_t ZMASK = 0x80808080;
                            TT_MOP_CFG(ZMASK >> 16);
                            TT_MOP(0, 32 - 1, ZMASK & 0xFFFF);

                            col_offset += udim;
                        }
                    }
                }
            }
        }
    }
    {
        ZONE_SCOPED("UNINIT")
        _llk_unpack_fast_tilize_uninit_<is_fp32_dest_acc_en>();
    }
}

#endif

#ifdef LLK_TRISC_MATH

#include "llk_math_common.h"
#include "llk_math_eltwise_unary_datacopy.h"

void run_kernel(RUNTIME_PARAMETERS params)
{
#ifndef SPEED_OF_LIGHT
    const std::uint32_t BLOCK_CT_DIM = params.BLOCK_CT_DIM;
    const std::uint32_t BLOCK_RT_DIM = params.BLOCK_RT_DIM;
    const std::uint32_t LOOP_FACTOR  = params.LOOP_FACTOR;
#endif
    constexpr std::uint32_t unit_dim = 4;

    std::uint32_t unit_dims[MAX_UNITS_PER_ROW];
    std::uint32_t units_per_row = decompose_row(BLOCK_CT_DIM, unit_dims);

    // Width 1 fallback
    if (BLOCK_CT_DIM == 1)
    {
        {
            ZONE_SCOPED("INIT")
            _llk_math_reconfig_remap_(true);
            _llk_math_pack_sync_init_<DstSync::SyncHalf, is_fp32_dest_acc_en>();
            _llk_math_hw_configure_<is_fp32_dest_acc_en>(formats.math, formats.math);
            _llk_math_eltwise_unary_datacopy_init_<A2D, is_fp32_dest_acc_en, BroadcastType::NONE, true>(4, formats.math);
        }
        {
            ZONE_SCOPED("TILE_LOOP")
            for (std::uint32_t loop = 0; loop < LOOP_FACTOR; loop++)
            {
                for (std::uint32_t row = 0; row < BLOCK_RT_DIM; row++)
                {
                    _llk_math_wait_for_dest_available_<DstSync::SyncHalf>();
                    _llk_math_eltwise_unary_datacopy_<A2D, DstSync::SyncHalf, is_fp32_dest_acc_en>(0, formats.math, formats.math, 4);
                    _llk_math_dest_section_done_<DstSync::SyncHalf, is_fp32_dest_acc_en>();
                }
            }
        }
        {
            ZONE_SCOPED("UNINIT")
        }
        return;
    }

    std::uint32_t num_units_total = BLOCK_RT_DIM * units_per_row;

    {
        ZONE_SCOPED("INIT")
        _llk_math_reconfig_remap_(true);
        _llk_math_pack_sync_init_<DstSync::SyncHalf, is_fp32_dest_acc_en>();
        _llk_math_hw_configure_<is_fp32_dest_acc_en>(formats.math, formats.math);
        _llk_math_fast_tilize_init_<is_fp32_dest_acc_en>(formats.math, unit_dim);
    }
    {
        ZONE_SCOPED("TILE_LOOP")
        if constexpr (PERF_RUN_TYPE == PerfRunType::PACK_ISOLATE)
        {
            // Release one section_done per unit so pack can run all units
            for (std::uint32_t i = 0; i < num_units_total; i++)
            {
                _llk_math_dest_section_done_<DstSync::SyncHalf, is_fp32_dest_acc_en>();
            }
            return;
        }
        else if constexpr (PERF_RUN_TYPE == PerfRunType::UNPACK_ISOLATE)
        {
            return _perf_math_loop_clear_valid<true, false>(num_units_total * 4 * LOOP_FACTOR);
        }
        else
        {
            // Math is completely unchanged — same MOP for all unit_dims.
            // Each unit consumes 4 dvalids and fills a full DEST half-bank.
            for (std::uint32_t loop = 0; loop < LOOP_FACTOR; loop++)
            {
                for (std::uint32_t u = 0; u < num_units_total; u++)
                {
                    _llk_math_wait_for_dest_available_<DstSync::SyncHalf>();
                    _llk_math_fast_tilize_block_<is_fp32_dest_acc_en>(0, formats.math, unit_dim, 1, 4);
                    _llk_math_dest_section_done_<DstSync::SyncHalf, is_fp32_dest_acc_en>();
                }
            }
        }
    }
    {
        ZONE_SCOPED("UNINIT")
        _llk_math_fast_tilize_uninit_<is_fp32_dest_acc_en>(formats.math);
    }
}

#endif

#ifdef LLK_TRISC_PACK

#include "llk_pack_common.h"
#include "llk_pack_fast_tilize.h"

void run_kernel(RUNTIME_PARAMETERS params)
{
#ifndef SPEED_OF_LIGHT
    const std::uint32_t BLOCK_CT_DIM = params.BLOCK_CT_DIM;
    const std::uint32_t BLOCK_RT_DIM = params.BLOCK_RT_DIM;
    const std::uint32_t LOOP_FACTOR  = params.LOOP_FACTOR;
    const Operand& buffer_Res        = params.buffer_Res;
#endif

    std::uint32_t unit_dims[MAX_UNITS_PER_ROW];
    std::uint32_t units_per_row = decompose_row(BLOCK_CT_DIM, unit_dims);

    // Width 1 fallback
    if (BLOCK_CT_DIM == 1)
    {
        {
            ZONE_SCOPED("INIT")
            _llk_pack_dest_init_<DstSync::SyncHalf, is_fp32_dest_acc_en>();
            _llk_pack_hw_configure_<is_fp32_dest_acc_en>(formats.pack_src, formats.pack_dst, SCALE_DATUM_SIZE(formats.pack_dst, TILE_C_DIM * TILE_R_DIM));
            _llk_pack_init_<false, false, true>(formats.pack_src, formats.pack_dst, FACE_R_DIM, TILE_C_DIM, 4, false, false, 1, false);
        }
        {
            ZONE_SCOPED("TILE_LOOP")
            for (std::uint32_t loop = 0; loop < LOOP_FACTOR; loop++)
            {
                for (std::uint32_t row = 0; row < BLOCK_RT_DIM; row++)
                {
                    _llk_packer_wait_for_math_done_();
                    _llk_pack_<DstSync::SyncHalf, is_fp32_dest_acc_en>(0, L1_ADDRESS(buffer_Res[row * BLOCK_CT_DIM]));
                    _llk_pack_dest_section_done_<DstSync::SyncHalf, is_fp32_dest_acc_en>();
                }
            }
        }
        {
            ZONE_SCOPED("UNINIT")
        }
        return;
    }

    std::uint32_t num_units_total = BLOCK_RT_DIM * units_per_row;

    {
        ZONE_SCOPED("INIT")
        _llk_pack_dest_init_<DstSync::SyncHalf, is_fp32_dest_acc_en>();
        // For dest_acc=Yes, override pack_src to Float16_b (compat 16-bit DEST).
        const std::uint32_t fast_tilize_pack_src = is_fp32_dest_acc_en ? ckernel::to_underlying(DataFormat::Float16_b) : formats.pack_src;
        _llk_pack_hw_configure_<is_fp32_dest_acc_en>(fast_tilize_pack_src, formats.pack_dst, SCALE_DATUM_SIZE(formats.pack_dst, TILE_C_DIM * TILE_R_DIM));
        _llk_pack_fast_tilize_init_<DstSync::SyncHalf, is_fp32_dest_acc_en>(0, formats.pack_dst, unit_dims[0], 4);
    }
    {
        ZONE_SCOPED("TILE_LOOP")
        if constexpr (PERF_RUN_TYPE == PerfRunType::UNPACK_ISOLATE)
        {
            return;
        }
        else if constexpr (PERF_RUN_TYPE == PerfRunType::MATH_ISOLATE)
        {
            for (std::uint32_t i = 0; i < num_units_total * LOOP_FACTOR; i++)
            {
                _llk_packer_wait_for_math_done_();
                _llk_pack_dest_section_done_<DstSync::SyncHalf, is_fp32_dest_acc_en>();
            }
            return;
        }
        else
        {
            std::uint32_t prev_udim = unit_dims[0];

            for (std::uint32_t loop = 0; loop < LOOP_FACTOR; loop++)
            {
                for (std::uint32_t row = 0; row < BLOCK_RT_DIM; row++)
                {
                    std::uint32_t col_offset = 0;

                    for (std::uint32_t u = 0; u < units_per_row; u++)
                    {
                        std::uint32_t udim = unit_dims[u];

                        if (udim != prev_udim)
                        {
                            _llk_pack_fast_tilize_reinit_unit_dim_(formats.pack_dst, udim);
                            prev_udim = udim;
                        }

                        _llk_packer_wait_for_math_done_();
                        std::uint32_t tile_offset = row * BLOCK_CT_DIM + col_offset;
                        _llk_pack_fast_tilize_block_(0, L1_ADDRESS(buffer_Res[tile_offset]), udim, 1, 4);
                        _llk_pack_dest_section_done_<DstSync::SyncHalf, is_fp32_dest_acc_en>();

                        col_offset += udim;
                    }
                }
            }
        }
    }
    {
        ZONE_SCOPED("UNINIT")
        _llk_pack_fast_tilize_uninit_<DstSync::SyncHalf, is_fp32_dest_acc_en>(formats.pack_dst, FACE_R_DIM, 4);
    }
}

#endif
