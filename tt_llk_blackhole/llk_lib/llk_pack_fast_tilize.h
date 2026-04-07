// SPDX-FileCopyrightText: © 2026 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

// BH Fast-Tilize Pack functions.
// Separated from llk_pack.h to avoid code-size regression in non-fast-tilize kernels.
// Include this header only from fast-tilize test/kernel sources.

#pragma once

#include <cstdint>

#include "llk_pack.h"

// ============================================================================
// BH Fast-Tilize Pack
//
// Reads 4 tiles from the interleaved DEST layout using DST_ACCESS_STRIDED_MODE.
// Strided mode reads 4 rows at stride 16 per PACR (matching the gap pattern).
// 4 PACRs per face, 4 faces per tile, 4 tiles per unit = 64 PACRs.
//
// DEST layout (from fast-tilize unpack+math):
//   Tile T, face F (F=0: cols 0-15, F=1: cols 16-31) top half:
//     DEST row = tensor_row * 16 + T * 2 + F
//     for tensor_row = 0..15 (top), 16..31 (bottom, add 256 offset)
// ============================================================================

__attribute__((noinline)) void _llk_pack_fast_tilize_configure_addrmod_()
{
    // MOP-based pack: Z handles left/right face (Z_stride=1 row), W handles top/bottom (W_stride=256 rows).
    // MOP outerloop=2 (face0+face1), innerloop=4 (4 PACRs per face).
    // Z toggles 0→1 at face boundary via last_inner, resets at last_outer.

    // ADDR_MOD_0: Y += 1 (advance to next 4-row group within a face)
    addr_mod_pack_t {
        .y_src = {.incr = 1},
    }
        .set(ADDR_MOD_0);

    // ADDR_MOD_2 (last_inner): Y clear, Z += 1 (face0→face1 within a half)
    addr_mod_pack_t {
        .y_src = {.incr = 0, .clr = 1},
        .z_src = {.incr = 1},
    }
        .set(ADDR_MOD_2);

    // ADDR_MOD_3 (last_outer): Y clear, Z clear (face-pair done, reset for next half or tile)
    addr_mod_pack_t {
        .y_src = {.incr = 0, .clr = 1},
        .z_src = {.incr = 0, .clr = 1},
    }
        .set(ADDR_MOD_3);
}

// L1 advance replay (only one needed, at packer replay offset)
constexpr std::uint32_t FAST_TILIZE_L1_OFFSET = ckernel::packer::replay_buf_offset;
constexpr std::uint32_t FAST_TILIZE_L1_LEN    = 4;

__attribute__((noinline)) void _llk_pack_fast_tilize_load_l1_advance_replay_()
{
    // L1 address advance after unit flush.
    load_replay_buf(
        FAST_TILIZE_L1_OFFSET,
        FAST_TILIZE_L1_LEN,
        []
        {
            TTI_ADDDMAREG(0, p_gpr_pack::OUTPUT_ADDR, p_gpr_pack::OUTPUT_ADDR, p_gpr_pack::OUTPUT_ADDR_OFFSET);
            TTI_STALLWAIT(p_stall::STALL_CFG, p_stall::THCON);
            TTI_WRCFG(p_gpr_pack::OUTPUT_ADDR, 0, THCON_SEC0_REG1_L1_Dest_addr_ADDR32);
            TTI_NOP;
        });
}

inline void _llk_pack_fast_tilize_mop_config_()
{
    // MOP-based pack: outerloop=2 (2 faces per run), innerloop=4 (4 PACRs per face).
    // Z handles left/right face (Z_stride=1 row), W handles tile+half selection.
    // Two run() calls per tile: one for top (face0+face1), one for bottom (face2+face3).
    // Per-tile structure required: tilized L1 format needs tiles contiguous.

    ckernel::ckernel_template tmp(
        2, // outerloop — 2 faces per run (face0+face1 or face2+face3)
        4, // innerloop — 4 PACRs per face (Y=0..3)
        TT_OP_PACR(
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
            0));

    // Last inner: Y clear + Z+=1 (face0→face1 transition)
    tmp.set_last_inner_loop_instr(TT_OP_PACR(
        p_pacr::CFG_CTXT_0,
        p_pacr::NO_ROW_PAD_ZERO,
        p_pacr::DST_ACCESS_STRIDED_MODE,
        ADDR_MOD_2,
        p_pacr::ADDR_CNT_CTXT_0,
        0,
        p_pacr::ALL_INTF_ACTIVE,
        0,
        0,
        p_pacr::NO_CTXT_CTRL,
        0,
        0));

    // Last outer: Y clear + Z clear (face-pair done, no Last — accumulate across tiles)
    tmp.set_last_outer_loop_instr(TT_OP_PACR(
        p_pacr::CFG_CTXT_0,
        p_pacr::NO_ROW_PAD_ZERO,
        p_pacr::DST_ACCESS_STRIDED_MODE,
        ADDR_MOD_3,
        p_pacr::ADDR_CNT_CTXT_0,
        0,
        p_pacr::ALL_INTF_ACTIVE,
        0,
        0,
        p_pacr::NO_CTXT_CTRL,
        0,
        0));

    tmp.program();

    // L1 advance replay (1 per 4-tile unit)
    _llk_pack_fast_tilize_load_l1_advance_replay_();
}

template <DstSync Dst>
__attribute__((noinline)) void _llk_pack_fast_tilize_init_(
    [[maybe_unused]] const std::uint32_t use_32bit_dest,
    const std::uint32_t pack_dst_format,
    [[maybe_unused]] const std::uint32_t unit_dim,
    [[maybe_unused]] const std::uint32_t num_faces = 4)
{
    // L1 address advancement per unit (4 tiles). Output buffer accumulates, flushes once.
    const std::uint32_t pacr_l1_size = 4 * (SCALE_DATUM_SIZE(pack_dst_format, TILE_C_DIM * TILE_R_DIM) >> 4);
    TT_SETDMAREG(0, LOWER_HALFWORD(pacr_l1_size), 0, LO_16(p_gpr_pack::OUTPUT_ADDR_OFFSET));

    // DEST offset registers for SyncHalf
    TTI_SETDMAREG(0, 0x000, 0, LO_16(p_gpr_pack::DEST_OFFSET_LO + 0));
    TTI_SETDMAREG(0, DEST_REGISTER_HALF_SIZE, 0, LO_16(p_gpr_pack::DEST_OFFSET_HI + 0));

    select_packer_dest_registers<Dst>();

    // X counter: 16 datums per pack row
    TTI_SETADCXX(p_setadc::PAC, FACE_C_DIM - 1, 0x0);

    // NOTE: remap_addrs + swizzle_32b enabled in block function after math done

    // Compute bytes-per-datum for the DEST source format
    std::uint32_t x_stride = (pack_dst_format & 0x3) == to_underlying(DataFormat::Float32)   ? 4
                             : (pack_dst_format & 0x3) == to_underlying(DataFormat::Float16) ? 2
                                                                                             : 1;

    // Strides for MOP-based gap DEST layout:
    // Y stride: 64 DEST rows per Y step (within-face group: 4 tensor rows × 16 rows/group)
    // Z stride: 1 DEST row (left/right face column: face0→face1 at Z+=1)
    // W stride: 2 DEST rows (tile selection: W=T for top, W=T+128 for bottom)
    //   Face0: W=T, Z=0 → T*2. Face1: W=T, Z=1 → T*2+1.
    //   Face2: W=T+128, Z=0 → (T+128)*2 = T*2+256. Face3: W=T+128, Z=1 → T*2+257.
    std::uint32_t y_stride = 64 * FACE_C_DIM * x_stride; // 64 rows
    std::uint32_t z_stride = FACE_C_DIM * x_stride;      // 1 row
    std::uint32_t w_stride = 2 * FACE_C_DIM * x_stride;  // 2 rows

    TT_SETDMAREG(0, LOWER_HALFWORD(y_stride << PCK0_ADDR_CTRL_XY_REG_0_Ystride_SHAMT), 0, LO_16(p_gpr_pack::TMP0));
    TT_SETDMAREG(0, UPPER_HALFWORD(y_stride << PCK0_ADDR_CTRL_XY_REG_0_Ystride_SHAMT), 0, HI_16(p_gpr_pack::TMP0));
    TT_SETDMAREG(0, LOWER_HALFWORD(z_stride << PCK0_ADDR_CTRL_ZW_REG_0_Zstride_SHAMT), 0, LO_16(p_gpr_pack::TMP1));
    TT_SETDMAREG(0, UPPER_HALFWORD(w_stride << PCK0_ADDR_CTRL_ZW_REG_0_Wstride_SHAMT), 0, HI_16(p_gpr_pack::TMP1));
    TTI_STALLWAIT(p_stall::STALL_CFG, p_stall::THCON);
    TTI_WRCFG(p_gpr_pack::TMP0, p_cfg::WRCFG_32b, PCK0_ADDR_CTRL_XY_REG_0_Xstride_ADDR32);
    TTI_WRCFG(p_gpr_pack::TMP1, p_cfg::WRCFG_32b, PCK0_ADDR_CTRL_ZW_REG_0_Zstride_ADDR32);
    TTI_NOP;
    TTI_NOP;

    _llk_pack_fast_tilize_configure_addrmod_();
    _llk_pack_fast_tilize_mop_config_();
}

inline void _llk_pack_fast_tilize_block_(
    [[maybe_unused]] const std::uint32_t tile_index,
    const std::uint32_t address,
    [[maybe_unused]] const std::uint32_t unit_dim,
    const std::uint32_t num_units,
    [[maybe_unused]] const std::uint32_t num_faces = 4)
{
    // remap enabled by math init (_llk_math_reconfig_remap_)
    program_packer_destination(address);

    // Pre-compute both MOP last_outer variants to avoid branching in inner loop
    constexpr std::uint32_t PACR_NO_LAST = TT_OP_PACR(
        p_pacr::CFG_CTXT_0,
        p_pacr::NO_ROW_PAD_ZERO,
        p_pacr::DST_ACCESS_STRIDED_MODE,
        ADDR_MOD_3,
        p_pacr::ADDR_CNT_CTXT_0,
        0,
        p_pacr::ALL_INTF_ACTIVE,
        0,
        0,
        p_pacr::NO_CTXT_CTRL,
        0,
        0);
    constexpr std::uint32_t PACR_WITH_LAST = TT_OP_PACR(
        p_pacr::CFG_CTXT_0,
        p_pacr::NO_ROW_PAD_ZERO,
        p_pacr::DST_ACCESS_STRIDED_MODE,
        ADDR_MOD_3,
        p_pacr::ADDR_CNT_CTXT_0,
        0,
        p_pacr::ALL_INTF_ACTIVE,
        0,
        0,
        p_pacr::NO_CTXT_CTRL,
        0,
        1);

    volatile std::uint32_t *mop_cfg = reinterpret_cast<volatile std::uint32_t *>(TENSIX_MOP_CFG_BASE);

    for (std::uint32_t u = 0; u < num_units; u++)
    {
        // Pack 4 tiles: 2 run() per tile (top + bottom face-pairs).
        // Per-tile structure required for tile-contiguous L1 output.
        for (std::uint32_t t = 0; t < 4; t++)
        {
            TT_SETADC(p_setadc::PAC, p_setadc::CH_0, p_setadc::SET_W, t);
            ckernel::ckernel_template::run();

            TT_SETADC(p_setadc::PAC, p_setadc::CH_0, p_setadc::SET_W, t + 128);
            if (t == 3)
            {
                mop_cfg[7] = PACR_WITH_LAST;
            }
            ckernel::ckernel_template::run();
        }

        mop_cfg[7] = PACR_NO_LAST;

        // L1 advance: ADDDMAREG + REG2FLOP (2 instructions, bypasses THCON).
        TTI_ADDDMAREG(0, p_gpr_pack::OUTPUT_ADDR, p_gpr_pack::OUTPUT_ADDR, p_gpr_pack::OUTPUT_ADDR_OFFSET);
        TTI_REG2FLOP(1 /*WRITE_4B*/, 0, 0, 0, THCON_SEC0_REG1_L1_Dest_addr_ADDR32 - THCON_CFGREG_BASE_ADDR32, p_gpr_pack::OUTPUT_ADDR);
    }
}

template <DstSync Dst, bool is_fp32_dest_acc_en>
inline void _llk_pack_fast_tilize_uninit_(
    const std::uint32_t pack_dst_format, [[maybe_unused]] const std::uint32_t face_r_dim, [[maybe_unused]] const std::uint32_t num_faces)
{
    // Wait for pack to finish before modifying config
    TTI_STALLWAIT(p_stall::STALL_CFG, p_stall::PACK);

    // remap disabled by math uninit (_llk_math_reconfig_remap_)

    // Restore standard pack strides
    set_packer_strides(pack_dst_format, TILE_C_DIM);

    // Restore standard pack X counter
    TTI_SETADCXX(p_setadc::PAC, FACE_C_DIM - 1, 0x0);
    // Restore standard addr_mod
    _llk_pack_configure_addrmod_<false, false>();
    // Restore standard pack MOP
    _llk_pack_mop_config_<false, false, false>(pack_dst_format, FACE_R_DIM, TILE_C_DIM, 4, false, false, 1);
}
