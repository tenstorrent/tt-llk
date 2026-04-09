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
// Replay buffer layout (BH = 32 entries total):
//   [0..15]:  BFP tile replay (16 PACRs, Last=1 on final) — BFP per-tile path
//   [16..31]: Flat tile replay (16 PACRs, no Last) — Float16_b multi-tile MOP
//
// Two MOP variants:
//   Flat-output (Float16_b): outerloop=1, innerloop=unit_dim, single terminal Last
//   BFP-output (Bfp8_b/Bfp4_b): outerloop=unit_dim, innerloop=1, per-tile Last via replay

#pragma once

#include <cstdint>

#include "llk_pack.h"

// BFP tile replay at [0..15]: Last=1 on final PACR for per-tile BFP closure.
constexpr std::uint32_t REPLAY_BFP_TILE_OFFSET = 0;
constexpr std::uint32_t REPLAY_BFP_TILE_LEN    = 16;

// Flat tile replay at packer offset [16..31]: no Last, for multi-tile MOP.
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

// Record both replay variants using TTI_REPLAY (direct .ttinsn, avoids __builtin ICE).
__attribute__((noinline)) void _llk_pack_fast_tilize_load_replay_()
{
    // BFP tile replay at [0..15]: Last=1 on final PACR for per-tile BFP closure.
    // Exactly 16 PACRs = 64 BFP blocks = 1 complete tile. No extra ZeroWrite PACR.
    TTI_REPLAY(REPLAY_BFP_TILE_OFFSET, REPLAY_BFP_TILE_LEN, 0, 1);
    EMIT_FACE_PACRS(ADDR_MOD_3, 0); // face0
    EMIT_FACE_PACRS(ADDR_MOD_2, 0); // face1
    EMIT_FACE_PACRS(ADDR_MOD_3, 0); // face2
    EMIT_FACE_PACRS(ADDR_MOD_1, 1); // face3: Last=1 closes BFP tile

    // Flat tile replay at [16..31]: no Last, for multi-tile MOP batching.
    TTI_REPLAY(REPLAY_TILE_OFFSET, REPLAY_TILE_LEN, 0, 1);
    EMIT_FACE_PACRS(ADDR_MOD_3, 0); // face0
    EMIT_FACE_PACRS(ADDR_MOD_2, 0); // face1
    EMIT_FACE_PACRS(ADDR_MOD_3, 0); // face2
    EMIT_FACE_PACRS(ADDR_MOD_1, 0); // face3: no Last
}

#undef EMIT_FACE_PACRS

// Terminal PACR for flat-output path: ZeroWrite=1 + Last=1 flushes without reading DEST.
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

// Flat-output MOP (Float16_b): batches unit_dim tiles with single terminal Last.
inline void _llk_pack_fast_tilize_mop_config_flat_(const std::uint32_t mop_unit_dim)
{
    ckernel::ckernel_template tmp(
        1,            // outerloop: 1 unit
        mop_unit_dim, // innerloop: unit_dim tiles per unit
        lltt::replay_insn(REPLAY_TILE_OFFSET, REPLAY_TILE_LEN),
        TT_OP_ADDRCRZW(p_setadc::PAC, 0, 0, 1, 0, 0b0010 /* CH0_W */));

    tmp.set_start_op(TT_OP_SETADCZW(p_setadc::PAC, 0, 0, 0, 0, 0b0011 /* CH0_Z + CH0_W */));
    tmp.set_last_outer_loop_instr(PACR_FLUSH);
    tmp.set_end_ops(
        TT_OP_ADDDMAREG(0, p_gpr_pack::OUTPUT_ADDR, p_gpr_pack::OUTPUT_ADDR, p_gpr_pack::OUTPUT_ADDR_OFFSET),
        TT_OP_REG2FLOP(1, 0, 0, 0, THCON_SEC0_REG1_L1_Dest_addr_ADDR32 - THCON_CFGREG_BASE_ADDR32, p_gpr_pack::OUTPUT_ADDR));

    tmp.program();
}

// BFP-output MOP: outerloop=1 (one tile per MOP run). Tile closure is built
// into the BFP replay (Last=1 on 16th PACR). The block function runs the MOP
// unit_dim times with a STALLWAIT between runs so L1_Dest_addr is stable.
inline void _llk_pack_fast_tilize_mop_config_bfp_()
{
    ckernel::ckernel_template tmp(
        1, // outerloop: 1 tile per MOP run
        1, // innerloop: single tile body
        lltt::replay_insn(REPLAY_BFP_TILE_OFFSET, REPLAY_BFP_TILE_LEN),
        TT_OP_ADDRCRZW(p_setadc::PAC, 0, 0, 1, 0, 0b0010 /* CH0_W */));

    // start_op: restore Z/W from their CR shadows. W_Cr accumulates across
    // MOP runs (one per tile). Z_Cr stays 0 (initialized in block function).
    // Must NOT use SETADCZW here — that would reset W_Cr on every MOP run.
    tmp.set_start_op(TT_OP_ADDRCRZW(p_setadc::PAC, 0, 0, 0, 0, 0b0011 /* CH0_Z + CH0_W */));

    // last_outer (= B_instr since outerloop=1): same W_Cr advance
    tmp.set_last_outer_loop_instr(TT_OP_ADDRCRZW(p_setadc::PAC, 0, 0, 1, 0, 0b0010 /* CH0_W */));

    // end_ops: L1 address advance per tile
    tmp.set_end_ops(
        TT_OP_ADDDMAREG(0, p_gpr_pack::OUTPUT_ADDR, p_gpr_pack::OUTPUT_ADDR, p_gpr_pack::OUTPUT_ADDR_OFFSET),
        TT_OP_REG2FLOP(1, 0, 0, 0, THCON_SEC0_REG1_L1_Dest_addr_ADDR32 - THCON_CFGREG_BASE_ADDR32, p_gpr_pack::OUTPUT_ADDR));

    tmp.program();
}

// Global flag: set by init, used by reinit to select the correct MOP variant.
static bool fast_tilize_bfp_mode = false;

template <DstSync Dst, bool is_fp32_dest_acc_en = false>
__attribute__((noinline)) void _llk_pack_fast_tilize_init_(
    [[maybe_unused]] const std::uint32_t use_32bit_dest,
    const std::uint32_t pack_dst_format,
    [[maybe_unused]] const std::uint32_t unit_dim,
    [[maybe_unused]] const std::uint32_t num_faces = 4)
{
    fast_tilize_bfp_mode = IS_BFP_FORMAT(pack_dst_format);

    // Compat fp32-dest: fast-tilize uses a 16-bit DEST layout internally.
    // Clear Read_32b_data so the packer reads 16-bit DEST rows even when
    // the kernel was compiled with fp32 dest accumulation. Restored in uninit.
    if constexpr (is_fp32_dest_acc_en)
    {
        TTI_STALLWAIT(p_stall::STALL_CFG, p_stall::PACK);
        cfg_reg_rmw_tensix<PCK_DEST_RD_CTRL_Read_32b_data_RMW>(0);
    }

    // L1 address advancement.
    // BFP: per-tile (tile closure + L1 advance every outer iteration).
    // Flat: per-unit (single flush + L1 advance at end of MOP).
    const std::uint32_t tile_l1_size = GET_L1_HEADERLESS_TILE_SIZE(pack_dst_format);
    const std::uint32_t pacr_l1_size = fast_tilize_bfp_mode ? tile_l1_size : (unit_dim * tile_l1_size);
    TT_SETDMAREG(0, LOWER_HALFWORD(pacr_l1_size), 0, LO_16(p_gpr_pack::OUTPUT_ADDR_OFFSET));

    // DEST offset registers for SyncHalf
    TTI_SETDMAREG(0, 0x000, 0, LO_16(p_gpr_pack::DEST_OFFSET_LO + 0));
    TTI_SETDMAREG(0, DEST_REGISTER_HALF_SIZE, 0, LO_16(p_gpr_pack::DEST_OFFSET_HI + 0));

    select_packer_dest_registers<Dst>();

    // X counter: 16 datums per pack row
    TTI_SETADCXX(p_setadc::PAC, FACE_C_DIM - 1, 0x0);

    // DEST is always 16-bit in fast-tilize compat mode (fp32 cleared in init).
    // Stride = 2 bytes/datum regardless of output format.
    constexpr std::uint32_t x_stride = 2;

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

    if (fast_tilize_bfp_mode)
    {
        _llk_pack_fast_tilize_mop_config_bfp_();
    }
    else
    {
        _llk_pack_fast_tilize_mop_config_flat_(unit_dim);
    }
}

// Reprogram pack MOP and L1 advance for a different unit_dim.
// Replay buffer, addr_mods, strides all stay unchanged.
inline void _llk_pack_fast_tilize_reinit_unit_dim_(const std::uint32_t pack_dst_format, const std::uint32_t new_unit_dim)
{
    if (fast_tilize_bfp_mode)
    {
        // BFP: OUTPUT_ADDR_OFFSET is always per-tile — no change needed.
        // BFP MOP is outerloop=1 (not parameterized by unit_dim).
    }
    else
    {
        const std::uint32_t pacr_l1_size = new_unit_dim * GET_L1_HEADERLESS_TILE_SIZE(pack_dst_format);
        TT_SETDMAREG(0, LOWER_HALFWORD(pacr_l1_size), 0, LO_16(p_gpr_pack::OUTPUT_ADDR_OFFSET));
        _llk_pack_fast_tilize_mop_config_flat_(new_unit_dim);
    }
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

    if (fast_tilize_bfp_mode)
    {
        // BFP path: 1-tile MOP per run, with STALLWAIT between runs.
        // The BFP replay has Last=1 on its final PACR (exactly 16 PACRs = 1 tile).
        // The stall ensures L1_Dest_addr update (REG2FLOP) completes before
        // the next MOP's replay reads it.
        for (std::uint32_t u = 0; u < num_units; u++)
        {
            // Initialize Z/W/CRs to 0 at unit start. MOP start_op uses ADDRCRZW
            // (CR restore, not reset) so W_Cr accumulates across tiles within the unit.
            TTI_SETADCZW(p_setadc::PAC, 0, 0, 0, 0, 0b0011 /* CH0_Z + CH0_W */);

            for (std::uint32_t t = 0; t < unit_dim; t++)
            {
                ckernel::ckernel_template::run();
                // Wait for both PACK (tile flush) and THCON (REG2FLOP L1_Dest_addr write).
                // REG2FLOP is a THCON instruction; stalling on PACK alone lets the next
                // MOP start before the new L1 destination address is visible.
                TTI_STALLWAIT(p_stall::STALL_CFG, p_stall::PACK | p_stall::THCON);
            }
        }
    }
    else
    {
        // Flat path: multi-tile MOP with single terminal flush.
        for (std::uint32_t u = 0; u < num_units; u++)
        {
            ckernel::ckernel_template::run();
        }
    }
}

template <DstSync Dst, bool is_fp32_dest_acc_en>
inline void _llk_pack_fast_tilize_uninit_(
    const std::uint32_t pack_dst_format, [[maybe_unused]] const std::uint32_t face_r_dim, [[maybe_unused]] const std::uint32_t num_faces)
{
    // Wait for pack to finish before modifying config
    TTI_STALLWAIT(p_stall::STALL_CFG, p_stall::PACK);

    // Restore fp32 dest read mode if it was cleared in init
    if constexpr (is_fp32_dest_acc_en)
    {
        cfg_reg_rmw_tensix<PCK_DEST_RD_CTRL_Read_32b_data_RMW>(1);
    }

    // Restore standard pack strides
    set_packer_strides(pack_dst_format, TILE_C_DIM);

    // Restore standard pack X counter
    TTI_SETADCXX(p_setadc::PAC, FACE_C_DIM - 1, 0x0);
    // Restore standard addr_mod
    _llk_pack_configure_addrmod_<false, false>();
    // Restore standard pack MOP
    _llk_pack_mop_config_<false, false, false>(pack_dst_format, FACE_R_DIM, TILE_C_DIM, 4, false, false, 1);

    fast_tilize_bfp_mode = false;
}
