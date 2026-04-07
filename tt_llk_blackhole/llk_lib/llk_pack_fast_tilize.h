// SPDX-FileCopyrightText: © 2026 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

// BH Fast-Tilize Pack — Replay Buffer Implementation
//
// DEST addressing: row = Y*64 + Z*1 + W*2
//   Y: face-row group (0-3 top, 4-7 bottom via Y_Cr)
//   Z: left/right face (0/1)
//   W: tile index (0-3), advanced via W_Cr + ADDRCRZW
//
// Self-resetting tile sequence (16 PACRs, 4 addr_mods):
//   face0: AM0,AM0,AM0,AM3  (Y++, then Y=Y_Cr+Z++)
//   face1: AM0,AM0,AM0,AM2  (Y++, then Y_Cr+=4,Y=Y_Cr,Z_clr)
//   face2: AM0,AM0,AM0,AM3  (Y++, then Y=Y_Cr+Z++)
//   face3: AM0,AM0,AM0,AM1  (Y++, then Y_clr+Y_Cr_clr+Z_clr)
//
// After face3: Y=0, Y_Cr=0, Z=0 — tile boundary needs only W_Cr advance.
// Single 16-entry replay at [16..31]. Body has no Last; tail has Last=1 on PACR 16.

#pragma once

#include <cstdint>

#include "llk_pack.h"

// Single replay at packer offset [16..31] (BH replay buf = 32 entries total).
constexpr std::uint32_t REPLAY_TILE_OFFSET = ckernel::packer::replay_buf_offset;
constexpr std::uint32_t REPLAY_TILE_LEN    = 16;

__attribute__((noinline)) void _llk_pack_fast_tilize_configure_addrmod_()
{
    // AM0: Y += 1 (advance within face)
    addr_mod_pack_t {
        .y_src = {.incr = 1},
    }
        .set(ADDR_MOD_0);

    // AM1: clear Y + Y_Cr + Z (self-resetting end-of-tile)
    // cr=1, clr=1: "reset counter and carriage return to zero"
    addr_mod_pack_t {
        .y_src = {.clr = 1, .cr = 1},
        .z_src = {.clr = 1},
    }
        .set(ADDR_MOD_1);

    // AM2: Y_Cr += 4, Y = Y_Cr+4, Z clear (top→bottom half jump)
    addr_mod_pack_t {
        .y_src = {.incr = 4, .cr = 1},
        .z_src = {.clr = 1},
    }
        .set(ADDR_MOD_2);

    // AM3: Y = Y_Cr (snap to base), Z += 1 (face0→face1, face2→face3)
    addr_mod_pack_t {
        .y_src = {.cr = 1},
        .z_src = {.incr = 1},
    }
        .set(ADDR_MOD_3);
}

// 4 PACRs for one face (macro — must stay inline during replay recording).
#define EMIT_FACE_PACRS(boundary_am, last) \
    TTI_PACR(                              \
        p_pacr::CFG_CTXT_0,                \
        p_pacr::NO_ROW_PAD_ZERO,           \
        p_pacr::DST_ACCESS_STRIDED_MODE,   \
        ADDR_MOD_0,                        \
        p_pacr::ADDR_CNT_CTXT_0,           \
        0,                                 \
        p_pacr::ALL_INTF_ACTIVE,           \
        0,                                 \
        0,                                 \
        p_pacr::NO_CTXT_CTRL,              \
        0,                                 \
        0);                                \
    TTI_PACR(                              \
        p_pacr::CFG_CTXT_0,                \
        p_pacr::NO_ROW_PAD_ZERO,           \
        p_pacr::DST_ACCESS_STRIDED_MODE,   \
        ADDR_MOD_0,                        \
        p_pacr::ADDR_CNT_CTXT_0,           \
        0,                                 \
        p_pacr::ALL_INTF_ACTIVE,           \
        0,                                 \
        0,                                 \
        p_pacr::NO_CTXT_CTRL,              \
        0,                                 \
        0);                                \
    TTI_PACR(                              \
        p_pacr::CFG_CTXT_0,                \
        p_pacr::NO_ROW_PAD_ZERO,           \
        p_pacr::DST_ACCESS_STRIDED_MODE,   \
        ADDR_MOD_0,                        \
        p_pacr::ADDR_CNT_CTXT_0,           \
        0,                                 \
        p_pacr::ALL_INTF_ACTIVE,           \
        0,                                 \
        0,                                 \
        p_pacr::NO_CTXT_CTRL,              \
        0,                                 \
        0);                                \
    TTI_PACR(                              \
        p_pacr::CFG_CTXT_0,                \
        p_pacr::NO_ROW_PAD_ZERO,           \
        p_pacr::DST_ACCESS_STRIDED_MODE,   \
        boundary_am,                       \
        p_pacr::ADDR_CNT_CTXT_0,           \
        0,                                 \
        p_pacr::ALL_INTF_ACTIVE,           \
        0,                                 \
        0,                                 \
        p_pacr::NO_CTXT_CTRL,              \
        0,                                 \
        last)

// Record the tile replay using TTI_REPLAY (direct .ttinsn, avoids __builtin ICE).
__attribute__((noinline)) void _llk_pack_fast_tilize_load_replay_()
{
    TTI_REPLAY(REPLAY_TILE_OFFSET, REPLAY_TILE_LEN, 0, 1); // record mode
    EMIT_FACE_PACRS(ADDR_MOD_3, 0);                        // face0: Y=Y_Cr(=0), Z++
    EMIT_FACE_PACRS(ADDR_MOD_2, 0);                        // face1: Y_Cr+=4, Y=4, Z_clr
    EMIT_FACE_PACRS(ADDR_MOD_3, 0);                        // face2: Y=Y_Cr(=4), Z++
    EMIT_FACE_PACRS(ADDR_MOD_1, 0);                        // face3: Y=0,Y_Cr=0,Z=0 (self-reset)
}

#undef EMIT_FACE_PACRS

// Terminal PACR after tile 3: Last=1 commits accumulated data to L1.
// ZeroWrite=1 prevents reading an extra row from DEST; Last=1 triggers the flush.
// NOTE: ttsim rejects ZeroWrite on PACR (tensix.cpp:1946). This path is
// silicon-only until a proper Flush-only drain is validated on BH hardware.
constexpr std::uint32_t PACR_FLUSH = TT_OP_PACR(
    p_pacr::CFG_CTXT_0,
    p_pacr::NO_ROW_PAD_ZERO,
    p_pacr::DST_ACCESS_STRIDED_MODE,
    ADDR_MOD_1,
    p_pacr::ADDR_CNT_CTXT_0,
    1 /* ZeroWrite */,
    p_pacr::ALL_INTF_ACTIVE,
    0,
    0,
    p_pacr::NO_CTXT_CTRL,
    0 /* Flush */,
    1 /* Last */);

inline void _llk_pack_fast_tilize_mop_config_()
{
    // Full-unit MOP: outerloop=1, innerloop=4.
    // start_op resets Z/W, inner loop replays 4 tiles with W_Cr advance,
    // end_ops handle L1 address advance. One MOP = one complete unit.
    ckernel::ckernel_template tmp(
        1, // outerloop: 1 unit
        4, // innerloop: 4 tiles per unit
        lltt::replay_insn(REPLAY_TILE_OFFSET, REPLAY_TILE_LEN),
        TT_OP_ADDRCRZW(p_setadc::PAC, 0, 0, 1, 0, 0b0010 /* CH0_W */));

    // start_op: reset Z=0, W=0 at unit start (W_Cr established by TT_SETADC before run)
    tmp.set_start_op(TT_OP_SETADCZW(p_setadc::PAC, 0, 0, 0, 0, 0b0011 /* CH0_Z + CH0_W */));

    // Last inner (= last outer since outerloop=1): terminal drain instead of W advance
    tmp.set_last_outer_loop_instr(PACR_FLUSH);

    // end_ops: L1 address advance after unit flush
    tmp.set_end_ops(
        TT_OP_ADDDMAREG(0, p_gpr_pack::OUTPUT_ADDR, p_gpr_pack::OUTPUT_ADDR, p_gpr_pack::OUTPUT_ADDR_OFFSET),
        TT_OP_REG2FLOP(1, 0, 0, 0, THCON_SEC0_REG1_L1_Dest_addr_ADDR32 - THCON_CFGREG_BASE_ADDR32, p_gpr_pack::OUTPUT_ADDR));

    tmp.program();
}

template <DstSync Dst>
__attribute__((noinline)) void _llk_pack_fast_tilize_init_(
    [[maybe_unused]] const std::uint32_t use_32bit_dest,
    const std::uint32_t pack_dst_format,
    [[maybe_unused]] const std::uint32_t unit_dim,
    [[maybe_unused]] const std::uint32_t num_faces = 4)
{
    // L1 address advancement per unit (4 tiles)
    const std::uint32_t pacr_l1_size = 4 * (SCALE_DATUM_SIZE(pack_dst_format, TILE_C_DIM * TILE_R_DIM) >> 4);
    TT_SETDMAREG(0, LOWER_HALFWORD(pacr_l1_size), 0, LO_16(p_gpr_pack::OUTPUT_ADDR_OFFSET));

    // DEST offset registers for SyncHalf
    TTI_SETDMAREG(0, 0x000, 0, LO_16(p_gpr_pack::DEST_OFFSET_LO + 0));
    TTI_SETDMAREG(0, DEST_REGISTER_HALF_SIZE, 0, LO_16(p_gpr_pack::DEST_OFFSET_HI + 0));

    select_packer_dest_registers<Dst>();

    // X counter: 16 datums per pack row
    TTI_SETADCXX(p_setadc::PAC, FACE_C_DIM - 1, 0x0);

    // Compute bytes-per-datum for the DEST source format
    std::uint32_t x_stride = (pack_dst_format & 0x3) == to_underlying(DataFormat::Float32)   ? 4
                             : (pack_dst_format & 0x3) == to_underlying(DataFormat::Float16) ? 2
                                                                                             : 1;

    // Strides: Y=64 rows, Z=1 row, W=2 rows
    std::uint32_t y_stride = 64 * FACE_C_DIM * x_stride;
    std::uint32_t z_stride = FACE_C_DIM * x_stride;
    std::uint32_t w_stride = 2 * FACE_C_DIM * x_stride;

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
    _llk_pack_fast_tilize_load_replay_();
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

    // Reset XY counters once (Y=0, Y_Cr=0)
    TTI_SETADCXY(p_setadc::PAC, 0, 0, 0, 0, 0b0011);

    for (std::uint32_t u = 0; u < num_units; u++)
    {
        // 1 MOP = full unit: start_op resets Z/W/W_Cr, 4 tile replays, flush, L1 advance
        ckernel::ckernel_template::run();
    }
}

template <DstSync Dst, bool is_fp32_dest_acc_en>
inline void _llk_pack_fast_tilize_uninit_(
    const std::uint32_t pack_dst_format, [[maybe_unused]] const std::uint32_t face_r_dim, [[maybe_unused]] const std::uint32_t num_faces)
{
    // Wait for pack to finish before modifying config
    TTI_STALLWAIT(p_stall::STALL_CFG, p_stall::PACK);

    // Restore standard pack strides
    set_packer_strides(pack_dst_format, TILE_C_DIM);

    // Restore standard pack X counter
    TTI_SETADCXX(p_setadc::PAC, FACE_C_DIM - 1, 0x0);
    // Restore standard addr_mod
    _llk_pack_configure_addrmod_<false, false>();
    // Restore standard pack MOP
    _llk_pack_mop_config_<false, false, false>(pack_dst_format, FACE_R_DIM, TILE_C_DIM, 4, false, false, 1);
}
