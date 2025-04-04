// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[0] alias: debug_bus_thread_state note: THREAD0
//  name: i_ttsync_dbg_bits
//  name: ibuffer_stall
#define DEBUG_BUS_THREAD_STATE_0__I_TTSYNC_DBG_BITS_IBUFFER_STALL_WORD {13, 1, 0, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_0__I_TTSYNC_DBG_BITS_IBUFFER_STALL {DEBUG_BUS_THREAD_STATE_0__I_TTSYNC_DBG_BITS_IBUFFER_STALL_WORD, 7, 0x1}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[0] alias: debug_bus_thread_state note: THREAD0
//  name: i_ttsync_dbg_bits
//  name: risc_cfg_stall
#define DEBUG_BUS_THREAD_STATE_0__I_TTSYNC_DBG_BITS_RISC_CFG_STALL_WORD {13, 1, 0, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_0__I_TTSYNC_DBG_BITS_RISC_CFG_STALL {DEBUG_BUS_THREAD_STATE_0__I_TTSYNC_DBG_BITS_RISC_CFG_STALL_WORD, 6, 0x1}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[0] alias: debug_bus_thread_state note: THREAD0
//  name: i_ttsync_dbg_bits
//  name: risc_gpr_stall
#define DEBUG_BUS_THREAD_STATE_0__I_TTSYNC_DBG_BITS_RISC_GPR_STALL_WORD {13, 1, 0, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_0__I_TTSYNC_DBG_BITS_RISC_GPR_STALL {DEBUG_BUS_THREAD_STATE_0__I_TTSYNC_DBG_BITS_RISC_GPR_STALL_WORD, 5, 0x1}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[0] alias: debug_bus_thread_state note: THREAD0
//  name: i_ttsync_dbg_bits
//  name: risc_tdma_stall
#define DEBUG_BUS_THREAD_STATE_0__I_TTSYNC_DBG_BITS_RISC_TDMA_STALL_WORD {13, 1, 0, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_0__I_TTSYNC_DBG_BITS_RISC_TDMA_STALL {DEBUG_BUS_THREAD_STATE_0__I_TTSYNC_DBG_BITS_RISC_TDMA_STALL_WORD, 4, 0x1}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[0] alias: debug_bus_thread_state note: THREAD0
//  name: i_ttsync_dbg_bits
//  name: prev_gen_no
#define DEBUG_BUS_THREAD_STATE_0__I_TTSYNC_DBG_BITS_PREV_GEN_NO__3_0_WORD {12, 1, 6, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_0__I_TTSYNC_DBG_BITS_PREV_GEN_NO__3_0 {DEBUG_BUS_THREAD_STATE_0__I_TTSYNC_DBG_BITS_PREV_GEN_NO__3_0_WORD, 28, 0xF}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[0] alias: debug_bus_thread_state note: THREAD0
//  name: i_ttsync_dbg_bits
//  name: prev_gen_no
#define DEBUG_BUS_THREAD_STATE_0__I_TTSYNC_DBG_BITS_PREV_GEN_NO__7_4_WORD {13, 1, 0, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_0__I_TTSYNC_DBG_BITS_PREV_GEN_NO__7_4 {DEBUG_BUS_THREAD_STATE_0__I_TTSYNC_DBG_BITS_PREV_GEN_NO__7_4_WORD, 0, 0xF}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[0] alias: debug_bus_thread_state note: THREAD0
//  name: i_ttsync_dbg_bits
//  name: lsq_head_valid
#define DEBUG_BUS_THREAD_STATE_0__I_TTSYNC_DBG_BITS_LSQ_HEAD_VALID_WORD {12, 1, 6, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_0__I_TTSYNC_DBG_BITS_LSQ_HEAD_VALID {DEBUG_BUS_THREAD_STATE_0__I_TTSYNC_DBG_BITS_LSQ_HEAD_VALID_WORD, 27, 0x1}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[0] alias: debug_bus_thread_state note: THREAD0
//  name: i_ttsync_dbg_bits
//  name: lsq_head_rsrcs alias: ttsync_rsrc_t
//  name: wr_tdma
#define DEBUG_BUS_THREAD_STATE_0__I_TTSYNC_DBG_BITS_LSQ_HEAD_RSRCS_WR_TDMA_WORD {12, 1, 6, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_0__I_TTSYNC_DBG_BITS_LSQ_HEAD_RSRCS_WR_TDMA {DEBUG_BUS_THREAD_STATE_0__I_TTSYNC_DBG_BITS_LSQ_HEAD_RSRCS_WR_TDMA_WORD, 26, 0x1}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[0] alias: debug_bus_thread_state note: THREAD0
//  name: i_ttsync_dbg_bits
//  name: lsq_head_rsrcs alias: ttsync_rsrc_t
//  name: rd_tdma
#define DEBUG_BUS_THREAD_STATE_0__I_TTSYNC_DBG_BITS_LSQ_HEAD_RSRCS_RD_TDMA_WORD {12, 1, 6, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_0__I_TTSYNC_DBG_BITS_LSQ_HEAD_RSRCS_RD_TDMA {DEBUG_BUS_THREAD_STATE_0__I_TTSYNC_DBG_BITS_LSQ_HEAD_RSRCS_RD_TDMA_WORD, 25, 0x1}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[0] alias: debug_bus_thread_state note: THREAD0
//  name: i_ttsync_dbg_bits
//  name: lsq_head_rsrcs alias: ttsync_rsrc_t
//  name: wr_gpr
#define DEBUG_BUS_THREAD_STATE_0__I_TTSYNC_DBG_BITS_LSQ_HEAD_RSRCS_WR_GPR_WORD {12, 1, 6, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_0__I_TTSYNC_DBG_BITS_LSQ_HEAD_RSRCS_WR_GPR {DEBUG_BUS_THREAD_STATE_0__I_TTSYNC_DBG_BITS_LSQ_HEAD_RSRCS_WR_GPR_WORD, 24, 0x1}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[0] alias: debug_bus_thread_state note: THREAD0
//  name: i_ttsync_dbg_bits
//  name: lsq_head_rsrcs alias: ttsync_rsrc_t
//  name: rd_gpr
#define DEBUG_BUS_THREAD_STATE_0__I_TTSYNC_DBG_BITS_LSQ_HEAD_RSRCS_RD_GPR_WORD {12, 1, 6, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_0__I_TTSYNC_DBG_BITS_LSQ_HEAD_RSRCS_RD_GPR {DEBUG_BUS_THREAD_STATE_0__I_TTSYNC_DBG_BITS_LSQ_HEAD_RSRCS_RD_GPR_WORD, 23, 0x1}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[0] alias: debug_bus_thread_state note: THREAD0
//  name: i_ttsync_dbg_bits
//  name: lsq_head_rsrcs alias: ttsync_rsrc_t
//  name: target_cfg_space
#define DEBUG_BUS_THREAD_STATE_0__I_TTSYNC_DBG_BITS_LSQ_HEAD_RSRCS_TARGET_CFG_SPACE_WORD {12, 1, 6, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_0__I_TTSYNC_DBG_BITS_LSQ_HEAD_RSRCS_TARGET_CFG_SPACE {DEBUG_BUS_THREAD_STATE_0__I_TTSYNC_DBG_BITS_LSQ_HEAD_RSRCS_TARGET_CFG_SPACE_WORD, 20, 0x7}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[0] alias: debug_bus_thread_state note: THREAD0
//  name: i_ttsync_dbg_bits
//  name: lsq_head_rsrcs alias: ttsync_rsrc_t
//  name: cfg_state
#define DEBUG_BUS_THREAD_STATE_0__I_TTSYNC_DBG_BITS_LSQ_HEAD_RSRCS_CFG_STATE_WORD {12, 1, 6, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_0__I_TTSYNC_DBG_BITS_LSQ_HEAD_RSRCS_CFG_STATE {DEBUG_BUS_THREAD_STATE_0__I_TTSYNC_DBG_BITS_LSQ_HEAD_RSRCS_CFG_STATE_WORD, 18, 0x3}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[0] alias: debug_bus_thread_state note: THREAD0
//  name: i_ttsync_dbg_bits
//  name: lsq_head_rsrcs alias: ttsync_rsrc_t
//  name: wr_cfg
#define DEBUG_BUS_THREAD_STATE_0__I_TTSYNC_DBG_BITS_LSQ_HEAD_RSRCS_WR_CFG_WORD {12, 1, 6, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_0__I_TTSYNC_DBG_BITS_LSQ_HEAD_RSRCS_WR_CFG {DEBUG_BUS_THREAD_STATE_0__I_TTSYNC_DBG_BITS_LSQ_HEAD_RSRCS_WR_CFG_WORD, 17, 0x1}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[0] alias: debug_bus_thread_state note: THREAD0
//  name: i_ttsync_dbg_bits
//  name: lsq_head_rsrcs alias: ttsync_rsrc_t
//  name: rd_cfg
#define DEBUG_BUS_THREAD_STATE_0__I_TTSYNC_DBG_BITS_LSQ_HEAD_RSRCS_RD_CFG_WORD {12, 1, 6, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_0__I_TTSYNC_DBG_BITS_LSQ_HEAD_RSRCS_RD_CFG {DEBUG_BUS_THREAD_STATE_0__I_TTSYNC_DBG_BITS_LSQ_HEAD_RSRCS_RD_CFG_WORD, 16, 0x1}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[0] alias: debug_bus_thread_state note: THREAD0
//  name: i_ttsync_dbg_bits
//  name: lsq_head_gen_no
#define DEBUG_BUS_THREAD_STATE_0__I_TTSYNC_DBG_BITS_LSQ_HEAD_GEN_NO_WORD {12, 1, 6, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_0__I_TTSYNC_DBG_BITS_LSQ_HEAD_GEN_NO {DEBUG_BUS_THREAD_STATE_0__I_TTSYNC_DBG_BITS_LSQ_HEAD_GEN_NO_WORD, 8, 0xFF}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[0] alias: debug_bus_thread_state note: THREAD0
//  name: i_ttsync_dbg_bits
//  name: lsq_full
#define DEBUG_BUS_THREAD_STATE_0__I_TTSYNC_DBG_BITS_LSQ_FULL_WORD {12, 1, 6, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_0__I_TTSYNC_DBG_BITS_LSQ_FULL {DEBUG_BUS_THREAD_STATE_0__I_TTSYNC_DBG_BITS_LSQ_FULL_WORD, 7, 0x1}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[0] alias: debug_bus_thread_state note: THREAD0
//  name: i_ttsync_dbg_bits
//  name: rq_head_valid
#define DEBUG_BUS_THREAD_STATE_0__I_TTSYNC_DBG_BITS_RQ_HEAD_VALID_WORD {12, 1, 6, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_0__I_TTSYNC_DBG_BITS_RQ_HEAD_VALID {DEBUG_BUS_THREAD_STATE_0__I_TTSYNC_DBG_BITS_RQ_HEAD_VALID_WORD, 6, 0x1}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[0] alias: debug_bus_thread_state note: THREAD0
//  name: i_ttsync_dbg_bits
//  name: rq_head_rsrcs alias: ttsync_rsrc_t
//  name: wr_tdma
#define DEBUG_BUS_THREAD_STATE_0__I_TTSYNC_DBG_BITS_RQ_HEAD_RSRCS_WR_TDMA_WORD {12, 1, 6, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_0__I_TTSYNC_DBG_BITS_RQ_HEAD_RSRCS_WR_TDMA {DEBUG_BUS_THREAD_STATE_0__I_TTSYNC_DBG_BITS_RQ_HEAD_RSRCS_WR_TDMA_WORD, 5, 0x1}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[0] alias: debug_bus_thread_state note: THREAD0
//  name: i_ttsync_dbg_bits
//  name: rq_head_rsrcs alias: ttsync_rsrc_t
//  name: rd_tdma
#define DEBUG_BUS_THREAD_STATE_0__I_TTSYNC_DBG_BITS_RQ_HEAD_RSRCS_RD_TDMA_WORD {12, 1, 6, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_0__I_TTSYNC_DBG_BITS_RQ_HEAD_RSRCS_RD_TDMA {DEBUG_BUS_THREAD_STATE_0__I_TTSYNC_DBG_BITS_RQ_HEAD_RSRCS_RD_TDMA_WORD, 4, 0x1}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[0] alias: debug_bus_thread_state note: THREAD0
//  name: i_ttsync_dbg_bits
//  name: rq_head_rsrcs alias: ttsync_rsrc_t
//  name: wr_gpr
#define DEBUG_BUS_THREAD_STATE_0__I_TTSYNC_DBG_BITS_RQ_HEAD_RSRCS_WR_GPR_WORD {12, 1, 6, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_0__I_TTSYNC_DBG_BITS_RQ_HEAD_RSRCS_WR_GPR {DEBUG_BUS_THREAD_STATE_0__I_TTSYNC_DBG_BITS_RQ_HEAD_RSRCS_WR_GPR_WORD, 3, 0x1}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[0] alias: debug_bus_thread_state note: THREAD0
//  name: i_ttsync_dbg_bits
//  name: rq_head_rsrcs alias: ttsync_rsrc_t
//  name: rd_gpr
#define DEBUG_BUS_THREAD_STATE_0__I_TTSYNC_DBG_BITS_RQ_HEAD_RSRCS_RD_GPR_WORD {12, 1, 6, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_0__I_TTSYNC_DBG_BITS_RQ_HEAD_RSRCS_RD_GPR {DEBUG_BUS_THREAD_STATE_0__I_TTSYNC_DBG_BITS_RQ_HEAD_RSRCS_RD_GPR_WORD, 2, 0x1}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[0] alias: debug_bus_thread_state note: THREAD0
//  name: i_ttsync_dbg_bits
//  name: rq_head_rsrcs alias: ttsync_rsrc_t
//  name: target_cfg_space
#define DEBUG_BUS_THREAD_STATE_0__I_TTSYNC_DBG_BITS_RQ_HEAD_RSRCS_TARGET_CFG_SPACE__0_0_WORD {12, 1, 4, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_0__I_TTSYNC_DBG_BITS_RQ_HEAD_RSRCS_TARGET_CFG_SPACE__0_0 {DEBUG_BUS_THREAD_STATE_0__I_TTSYNC_DBG_BITS_RQ_HEAD_RSRCS_TARGET_CFG_SPACE__0_0_WORD, 31, 0x1}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[0] alias: debug_bus_thread_state note: THREAD0
//  name: i_ttsync_dbg_bits
//  name: rq_head_rsrcs alias: ttsync_rsrc_t
//  name: target_cfg_space
#define DEBUG_BUS_THREAD_STATE_0__I_TTSYNC_DBG_BITS_RQ_HEAD_RSRCS_TARGET_CFG_SPACE__2_1_WORD {12, 1, 6, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_0__I_TTSYNC_DBG_BITS_RQ_HEAD_RSRCS_TARGET_CFG_SPACE__2_1 {DEBUG_BUS_THREAD_STATE_0__I_TTSYNC_DBG_BITS_RQ_HEAD_RSRCS_TARGET_CFG_SPACE__2_1_WORD, 0, 0x3}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[0] alias: debug_bus_thread_state note: THREAD0
//  name: i_ttsync_dbg_bits
//  name: rq_head_rsrcs alias: ttsync_rsrc_t
//  name: cfg_state
#define DEBUG_BUS_THREAD_STATE_0__I_TTSYNC_DBG_BITS_RQ_HEAD_RSRCS_CFG_STATE_WORD {12, 1, 4, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_0__I_TTSYNC_DBG_BITS_RQ_HEAD_RSRCS_CFG_STATE {DEBUG_BUS_THREAD_STATE_0__I_TTSYNC_DBG_BITS_RQ_HEAD_RSRCS_CFG_STATE_WORD, 29, 0x3}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[0] alias: debug_bus_thread_state note: THREAD0
//  name: i_ttsync_dbg_bits
//  name: rq_head_rsrcs alias: ttsync_rsrc_t
//  name: wr_cfg
#define DEBUG_BUS_THREAD_STATE_0__I_TTSYNC_DBG_BITS_RQ_HEAD_RSRCS_WR_CFG_WORD {12, 1, 4, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_0__I_TTSYNC_DBG_BITS_RQ_HEAD_RSRCS_WR_CFG {DEBUG_BUS_THREAD_STATE_0__I_TTSYNC_DBG_BITS_RQ_HEAD_RSRCS_WR_CFG_WORD, 28, 0x1}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[0] alias: debug_bus_thread_state note: THREAD0
//  name: i_ttsync_dbg_bits
//  name: rq_head_rsrcs alias: ttsync_rsrc_t
//  name: rd_cfg
#define DEBUG_BUS_THREAD_STATE_0__I_TTSYNC_DBG_BITS_RQ_HEAD_RSRCS_RD_CFG_WORD {12, 1, 4, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_0__I_TTSYNC_DBG_BITS_RQ_HEAD_RSRCS_RD_CFG {DEBUG_BUS_THREAD_STATE_0__I_TTSYNC_DBG_BITS_RQ_HEAD_RSRCS_RD_CFG_WORD, 27, 0x1}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[0] alias: debug_bus_thread_state note: THREAD0
//  name: i_ttsync_dbg_bits
//  name: rq_head_gen_no
#define DEBUG_BUS_THREAD_STATE_0__I_TTSYNC_DBG_BITS_RQ_HEAD_GEN_NO_WORD {12, 1, 4, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_0__I_TTSYNC_DBG_BITS_RQ_HEAD_GEN_NO {DEBUG_BUS_THREAD_STATE_0__I_TTSYNC_DBG_BITS_RQ_HEAD_GEN_NO_WORD, 19, 0xFF}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[0] alias: debug_bus_thread_state note: THREAD0
//  name: i_ttsync_dbg_bits
//  name: rq_full
#define DEBUG_BUS_THREAD_STATE_0__I_TTSYNC_DBG_BITS_RQ_FULL_WORD {12, 1, 4, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_0__I_TTSYNC_DBG_BITS_RQ_FULL {DEBUG_BUS_THREAD_STATE_0__I_TTSYNC_DBG_BITS_RQ_FULL_WORD, 18, 0x1}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[0] alias: debug_bus_thread_state note: THREAD0
//  name: i_cg_trisc_busy
#define DEBUG_BUS_THREAD_STATE_0__I_CG_TRISC_BUSY_WORD {12, 1, 4, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_0__I_CG_TRISC_BUSY {DEBUG_BUS_THREAD_STATE_0__I_CG_TRISC_BUSY_WORD, 17, 0x1}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[0] alias: debug_bus_thread_state note: THREAD0
//  name: machine_busy
#define DEBUG_BUS_THREAD_STATE_0__MACHINE_BUSY_WORD {12, 1, 4, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_0__MACHINE_BUSY {DEBUG_BUS_THREAD_STATE_0__MACHINE_BUSY_WORD, 16, 0x1}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[0] alias: debug_bus_thread_state note: THREAD0
//  name: req_iramd_buffer_not_empty
#define DEBUG_BUS_THREAD_STATE_0__REQ_IRAMD_BUFFER_NOT_EMPTY_WORD {12, 1, 4, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_0__REQ_IRAMD_BUFFER_NOT_EMPTY {DEBUG_BUS_THREAD_STATE_0__REQ_IRAMD_BUFFER_NOT_EMPTY_WORD, 15, 0x1}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[0] alias: debug_bus_thread_state note: THREAD0
//  name: gpr_file_busy
#define DEBUG_BUS_THREAD_STATE_0__GPR_FILE_BUSY_WORD {12, 1, 4, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_0__GPR_FILE_BUSY {DEBUG_BUS_THREAD_STATE_0__GPR_FILE_BUSY_WORD, 14, 0x1}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[0] alias: debug_bus_thread_state note: THREAD0
//  name: cfg_exu_busy
#define DEBUG_BUS_THREAD_STATE_0__CFG_EXU_BUSY_WORD {12, 1, 4, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_0__CFG_EXU_BUSY {DEBUG_BUS_THREAD_STATE_0__CFG_EXU_BUSY_WORD, 13, 0x1}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[0] alias: debug_bus_thread_state note: THREAD0
//  name: req_iramd_buffer_empty
#define DEBUG_BUS_THREAD_STATE_0__REQ_IRAMD_BUFFER_EMPTY_WORD {12, 1, 4, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_0__REQ_IRAMD_BUFFER_EMPTY {DEBUG_BUS_THREAD_STATE_0__REQ_IRAMD_BUFFER_EMPTY_WORD, 12, 0x1}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[0] alias: debug_bus_thread_state note: THREAD0
//  name: req_iramd_buffer_full
#define DEBUG_BUS_THREAD_STATE_0__REQ_IRAMD_BUFFER_FULL_WORD {12, 1, 4, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_0__REQ_IRAMD_BUFFER_FULL {DEBUG_BUS_THREAD_STATE_0__REQ_IRAMD_BUFFER_FULL_WORD, 11, 0x1}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[0] alias: debug_bus_thread_state note: THREAD0
//  name: ~ibuffer_rtr
#define DEBUG_BUS_THREAD_STATE_0___IBUFFER_RTR_WORD {12, 1, 4, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_0___IBUFFER_RTR {DEBUG_BUS_THREAD_STATE_0___IBUFFER_RTR_WORD, 10, 0x1}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[0] alias: debug_bus_thread_state note: THREAD0
//  name: ibuffer_empty
#define DEBUG_BUS_THREAD_STATE_0__IBUFFER_EMPTY_WORD {12, 1, 4, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_0__IBUFFER_EMPTY {DEBUG_BUS_THREAD_STATE_0__IBUFFER_EMPTY_WORD, 9, 0x1}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[0] alias: debug_bus_thread_state note: THREAD0
//  name: ibuffer_empty_raw
#define DEBUG_BUS_THREAD_STATE_0__IBUFFER_EMPTY_RAW_WORD {12, 1, 4, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_0__IBUFFER_EMPTY_RAW {DEBUG_BUS_THREAD_STATE_0__IBUFFER_EMPTY_RAW_WORD, 8, 0x1}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[0] alias: debug_bus_thread_state note: THREAD0
//  name: thread_inst
#define DEBUG_BUS_THREAD_STATE_0__THREAD_INST__23_0_WORD {12, 1, 2, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_0__THREAD_INST__23_0 {DEBUG_BUS_THREAD_STATE_0__THREAD_INST__23_0_WORD, 8, 0xFFFFFF}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[0] alias: debug_bus_thread_state note: THREAD0
//  name: thread_inst
#define DEBUG_BUS_THREAD_STATE_0__THREAD_INST__31_24_WORD {12, 1, 4, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_0__THREAD_INST__31_24 {DEBUG_BUS_THREAD_STATE_0__THREAD_INST__31_24_WORD, 0, 0xFF}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[0] alias: debug_bus_thread_state note: THREAD0
//  name: math_inst
#define DEBUG_BUS_THREAD_STATE_0__MATH_INST_WORD {12, 1, 2, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_0__MATH_INST {DEBUG_BUS_THREAD_STATE_0__MATH_INST_WORD, 7, 0x1}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[0] alias: debug_bus_thread_state note: THREAD0
//  name: tdma_inst
#define DEBUG_BUS_THREAD_STATE_0__TDMA_INST_WORD {12, 1, 2, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_0__TDMA_INST {DEBUG_BUS_THREAD_STATE_0__TDMA_INST_WORD, 6, 0x1}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[0] alias: debug_bus_thread_state note: THREAD0
//  name: pack_inst
#define DEBUG_BUS_THREAD_STATE_0__PACK_INST_WORD {12, 1, 2, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_0__PACK_INST {DEBUG_BUS_THREAD_STATE_0__PACK_INST_WORD, 5, 0x1}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[0] alias: debug_bus_thread_state note: THREAD0
//  name: move_inst
#define DEBUG_BUS_THREAD_STATE_0__MOVE_INST_WORD {12, 1, 2, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_0__MOVE_INST {DEBUG_BUS_THREAD_STATE_0__MOVE_INST_WORD, 4, 0x1}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[0] alias: debug_bus_thread_state note: THREAD0
//  name: sfpu_inst
#define DEBUG_BUS_THREAD_STATE_0__SFPU_INST_WORD {12, 1, 2, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_0__SFPU_INST {DEBUG_BUS_THREAD_STATE_0__SFPU_INST_WORD, 3, 0x1}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[0] alias: debug_bus_thread_state note: THREAD0
//  name: unpack_inst
#define DEBUG_BUS_THREAD_STATE_0__UNPACK_INST_WORD {12, 1, 2, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_0__UNPACK_INST {DEBUG_BUS_THREAD_STATE_0__UNPACK_INST_WORD, 1, 0x3}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[0] alias: debug_bus_thread_state note: THREAD0
//  name: xsearch_inst
#define DEBUG_BUS_THREAD_STATE_0__XSEARCH_INST_WORD {12, 1, 2, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_0__XSEARCH_INST {DEBUG_BUS_THREAD_STATE_0__XSEARCH_INST_WORD, 0, 0x1}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[0] alias: debug_bus_thread_state note: THREAD0
//  name: thcon_inst
#define DEBUG_BUS_THREAD_STATE_0__THCON_INST_WORD {12, 1, 0, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_0__THCON_INST {DEBUG_BUS_THREAD_STATE_0__THCON_INST_WORD, 31, 0x1}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[0] alias: debug_bus_thread_state note: THREAD0
//  name: sync_inst
#define DEBUG_BUS_THREAD_STATE_0__SYNC_INST_WORD {12, 1, 0, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_0__SYNC_INST {DEBUG_BUS_THREAD_STATE_0__SYNC_INST_WORD, 30, 0x1}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[0] alias: debug_bus_thread_state note: THREAD0
//  name: cfg_inst
#define DEBUG_BUS_THREAD_STATE_0__CFG_INST_WORD {12, 1, 0, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_0__CFG_INST {DEBUG_BUS_THREAD_STATE_0__CFG_INST_WORD, 29, 0x1}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[0] alias: debug_bus_thread_state note: THREAD0
//  name: stalled_pack_inst
#define DEBUG_BUS_THREAD_STATE_0__STALLED_PACK_INST_WORD {12, 1, 0, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_0__STALLED_PACK_INST {DEBUG_BUS_THREAD_STATE_0__STALLED_PACK_INST_WORD, 28, 0x1}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[0] alias: debug_bus_thread_state note: THREAD0
//  name: stalled_unpack_inst
#define DEBUG_BUS_THREAD_STATE_0__STALLED_UNPACK_INST_WORD {12, 1, 0, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_0__STALLED_UNPACK_INST {DEBUG_BUS_THREAD_STATE_0__STALLED_UNPACK_INST_WORD, 26, 0x3}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[0] alias: debug_bus_thread_state note: THREAD0
//  name: stalled_math_inst
#define DEBUG_BUS_THREAD_STATE_0__STALLED_MATH_INST_WORD {12, 1, 0, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_0__STALLED_MATH_INST {DEBUG_BUS_THREAD_STATE_0__STALLED_MATH_INST_WORD, 25, 0x1}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[0] alias: debug_bus_thread_state note: THREAD0
//  name: stalled_tdma_inst
#define DEBUG_BUS_THREAD_STATE_0__STALLED_TDMA_INST_WORD {12, 1, 0, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_0__STALLED_TDMA_INST {DEBUG_BUS_THREAD_STATE_0__STALLED_TDMA_INST_WORD, 24, 0x1}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[0] alias: debug_bus_thread_state note: THREAD0
//  name: stalled_move_inst
#define DEBUG_BUS_THREAD_STATE_0__STALLED_MOVE_INST_WORD {12, 1, 0, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_0__STALLED_MOVE_INST {DEBUG_BUS_THREAD_STATE_0__STALLED_MOVE_INST_WORD, 23, 0x1}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[0] alias: debug_bus_thread_state note: THREAD0
//  name: stalled_xsearch_inst
#define DEBUG_BUS_THREAD_STATE_0__STALLED_XSEARCH_INST_WORD {12, 1, 0, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_0__STALLED_XSEARCH_INST {DEBUG_BUS_THREAD_STATE_0__STALLED_XSEARCH_INST_WORD, 22, 0x1}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[0] alias: debug_bus_thread_state note: THREAD0
//  name: stalled_thcon_inst
#define DEBUG_BUS_THREAD_STATE_0__STALLED_THCON_INST_WORD {12, 1, 0, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_0__STALLED_THCON_INST {DEBUG_BUS_THREAD_STATE_0__STALLED_THCON_INST_WORD, 21, 0x1}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[0] alias: debug_bus_thread_state note: THREAD0
//  name: stalled_sync_inst
#define DEBUG_BUS_THREAD_STATE_0__STALLED_SYNC_INST_WORD {12, 1, 0, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_0__STALLED_SYNC_INST {DEBUG_BUS_THREAD_STATE_0__STALLED_SYNC_INST_WORD, 20, 0x1}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[0] alias: debug_bus_thread_state note: THREAD0
//  name: stalled_cfg_inst
#define DEBUG_BUS_THREAD_STATE_0__STALLED_CFG_INST_WORD {12, 1, 0, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_0__STALLED_CFG_INST {DEBUG_BUS_THREAD_STATE_0__STALLED_CFG_INST_WORD, 19, 0x1}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[0] alias: debug_bus_thread_state note: THREAD0
//  name: stalled_sfpu_inst
#define DEBUG_BUS_THREAD_STATE_0__STALLED_SFPU_INST_WORD {12, 1, 0, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_0__STALLED_SFPU_INST {DEBUG_BUS_THREAD_STATE_0__STALLED_SFPU_INST_WORD, 18, 0x1}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[0] alias: debug_bus_thread_state note: THREAD0
//  name: tdma_kick_move
#define DEBUG_BUS_THREAD_STATE_0__TDMA_KICK_MOVE_WORD {12, 1, 0, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_0__TDMA_KICK_MOVE {DEBUG_BUS_THREAD_STATE_0__TDMA_KICK_MOVE_WORD, 17, 0x1}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[0] alias: debug_bus_thread_state note: THREAD0
//  name: tdma_kick_pack
#define DEBUG_BUS_THREAD_STATE_0__TDMA_KICK_PACK_WORD {12, 1, 0, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_0__TDMA_KICK_PACK {DEBUG_BUS_THREAD_STATE_0__TDMA_KICK_PACK_WORD, 16, 0x1}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[0] alias: debug_bus_thread_state note: THREAD0
//  name: tdma_kick_unpack
#define DEBUG_BUS_THREAD_STATE_0__TDMA_KICK_UNPACK_WORD {12, 1, 0, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_0__TDMA_KICK_UNPACK {DEBUG_BUS_THREAD_STATE_0__TDMA_KICK_UNPACK_WORD, 14, 0x3}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[0] alias: debug_bus_thread_state note: THREAD0
//  name: tdma_kick_xsearch
#define DEBUG_BUS_THREAD_STATE_0__TDMA_KICK_XSEARCH_WORD {12, 1, 0, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_0__TDMA_KICK_XSEARCH {DEBUG_BUS_THREAD_STATE_0__TDMA_KICK_XSEARCH_WORD, 13, 0x1}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[0] alias: debug_bus_thread_state note: THREAD0
//  name: tdma_kick_thcon
#define DEBUG_BUS_THREAD_STATE_0__TDMA_KICK_THCON_WORD {12, 1, 0, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_0__TDMA_KICK_THCON {DEBUG_BUS_THREAD_STATE_0__TDMA_KICK_THCON_WORD, 12, 0x1}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[0] alias: debug_bus_thread_state note: THREAD0
//  name: tdma_status_busy
#define DEBUG_BUS_THREAD_STATE_0__TDMA_STATUS_BUSY_WORD {12, 1, 0, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_0__TDMA_STATUS_BUSY {DEBUG_BUS_THREAD_STATE_0__TDMA_STATUS_BUSY_WORD, 7, 0x1F}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[0] alias: debug_bus_thread_state note: THREAD0
//  name: packer_busy
#define DEBUG_BUS_THREAD_STATE_0__PACKER_BUSY_WORD {12, 1, 0, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_0__PACKER_BUSY {DEBUG_BUS_THREAD_STATE_0__PACKER_BUSY_WORD, 6, 0x1}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[0] alias: debug_bus_thread_state note: THREAD0
//  name: unpacker_busy
#define DEBUG_BUS_THREAD_STATE_0__UNPACKER_BUSY_WORD {12, 1, 0, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_0__UNPACKER_BUSY {DEBUG_BUS_THREAD_STATE_0__UNPACKER_BUSY_WORD, 4, 0x3}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[0] alias: debug_bus_thread_state note: THREAD0
//  name: thcon_busy
#define DEBUG_BUS_THREAD_STATE_0__THCON_BUSY_WORD {12, 1, 0, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_0__THCON_BUSY {DEBUG_BUS_THREAD_STATE_0__THCON_BUSY_WORD, 3, 0x1}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[0] alias: debug_bus_thread_state note: THREAD0
//  name: move_busy
#define DEBUG_BUS_THREAD_STATE_0__MOVE_BUSY_WORD {12, 1, 0, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_0__MOVE_BUSY {DEBUG_BUS_THREAD_STATE_0__MOVE_BUSY_WORD, 2, 0x1}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[0] alias: debug_bus_thread_state note: THREAD0
//  name: xsearch_busy
#define DEBUG_BUS_THREAD_STATE_0__XSEARCH_BUSY_WORD {12, 1, 0, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_0__XSEARCH_BUSY {DEBUG_BUS_THREAD_STATE_0__XSEARCH_BUSY_WORD, 0, 0x3}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_stall_inst_cnt[0] alias: perf_cnt_instrn_thread_inst_cnts note: THREAD0
//  name: perf_cnt_instrn_thread_inst_pack alias: perf_cnt
//  name: grant_cnt
#define DEBUG_BUS_STALL_INST_CNT_0__PERF_CNT_INSTRN_THREAD_INST_PACK_GRANT_CNT_WORD {11, 1, 6, 0, 1, 0}
#define DEBUG_BUS_STALL_INST_CNT_0__PERF_CNT_INSTRN_THREAD_INST_PACK_GRANT_CNT {DEBUG_BUS_STALL_INST_CNT_0__PERF_CNT_INSTRN_THREAD_INST_PACK_GRANT_CNT_WORD, 0, 0xFFFFFFFF}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_stall_inst_cnt[0] alias: perf_cnt_instrn_thread_inst_cnts note: THREAD0
//  name: perf_cnt_instrn_thread_inst_unpack alias: perf_cnt
//  name: grant_cnt
#define DEBUG_BUS_STALL_INST_CNT_0__PERF_CNT_INSTRN_THREAD_INST_UNPACK_GRANT_CNT_WORD {11, 1, 4, 0, 1, 0}
#define DEBUG_BUS_STALL_INST_CNT_0__PERF_CNT_INSTRN_THREAD_INST_UNPACK_GRANT_CNT {DEBUG_BUS_STALL_INST_CNT_0__PERF_CNT_INSTRN_THREAD_INST_UNPACK_GRANT_CNT_WORD, 0, 0xFFFFFFFF}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_stall_inst_cnt[0] alias: perf_cnt_instrn_thread_inst_cnts note: THREAD0
//  name: perf_cnt_instrn_thread_inst_math alias: perf_cnt
//  name: grant_cnt
#define DEBUG_BUS_STALL_INST_CNT_0__PERF_CNT_INSTRN_THREAD_INST_MATH_GRANT_CNT_WORD {11, 1, 2, 0, 1, 0}
#define DEBUG_BUS_STALL_INST_CNT_0__PERF_CNT_INSTRN_THREAD_INST_MATH_GRANT_CNT {DEBUG_BUS_STALL_INST_CNT_0__PERF_CNT_INSTRN_THREAD_INST_MATH_GRANT_CNT_WORD, 0, 0xFFFFFFFF}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_stall_inst_cnt[0] alias: perf_cnt_instrn_thread_inst_cnts note: THREAD0
//  name: perf_cnt_instrn_thread_inst_move alias: perf_cnt
//  name: grant_cnt
#define DEBUG_BUS_STALL_INST_CNT_0__PERF_CNT_INSTRN_THREAD_INST_MOVE_GRANT_CNT_WORD {11, 1, 0, 0, 1, 0}
#define DEBUG_BUS_STALL_INST_CNT_0__PERF_CNT_INSTRN_THREAD_INST_MOVE_GRANT_CNT {DEBUG_BUS_STALL_INST_CNT_0__PERF_CNT_INSTRN_THREAD_INST_MOVE_GRANT_CNT_WORD, 0, 0xFFFFFFFF}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_stall_inst_cnt[0] alias: perf_cnt_instrn_thread_inst_cnts note: THREAD0
//  name: perf_cnt_instrn_thread_inst_xsearch alias: perf_cnt
//  name: grant_cnt
#define DEBUG_BUS_STALL_INST_CNT_0__PERF_CNT_INSTRN_THREAD_INST_XSEARCH_GRANT_CNT_WORD {10, 1, 6, 0, 1, 0}
#define DEBUG_BUS_STALL_INST_CNT_0__PERF_CNT_INSTRN_THREAD_INST_XSEARCH_GRANT_CNT {DEBUG_BUS_STALL_INST_CNT_0__PERF_CNT_INSTRN_THREAD_INST_XSEARCH_GRANT_CNT_WORD, 0, 0xFFFFFFFF}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_stall_inst_cnt[0] alias: perf_cnt_instrn_thread_inst_cnts note: THREAD0
//  name: perf_cnt_instrn_thread_inst_thcon alias: perf_cnt
//  name: grant_cnt
#define DEBUG_BUS_STALL_INST_CNT_0__PERF_CNT_INSTRN_THREAD_INST_THCON_GRANT_CNT_WORD {10, 1, 4, 0, 1, 0}
#define DEBUG_BUS_STALL_INST_CNT_0__PERF_CNT_INSTRN_THREAD_INST_THCON_GRANT_CNT {DEBUG_BUS_STALL_INST_CNT_0__PERF_CNT_INSTRN_THREAD_INST_THCON_GRANT_CNT_WORD, 0, 0xFFFFFFFF}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_stall_inst_cnt[0] alias: perf_cnt_instrn_thread_inst_cnts note: THREAD0
//  name: perf_cnt_instrn_thread_inst_sync alias: perf_cnt
//  name: grant_cnt
#define DEBUG_BUS_STALL_INST_CNT_0__PERF_CNT_INSTRN_THREAD_INST_SYNC_GRANT_CNT_WORD {10, 1, 2, 0, 1, 0}
#define DEBUG_BUS_STALL_INST_CNT_0__PERF_CNT_INSTRN_THREAD_INST_SYNC_GRANT_CNT {DEBUG_BUS_STALL_INST_CNT_0__PERF_CNT_INSTRN_THREAD_INST_SYNC_GRANT_CNT_WORD, 0, 0xFFFFFFFF}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_stall_inst_cnt[0] alias: perf_cnt_instrn_thread_inst_cnts note: THREAD0
//  name: perf_cnt_instrn_thread_inst_cfg alias: perf_cnt
//  name: grant_cnt
#define DEBUG_BUS_STALL_INST_CNT_0__PERF_CNT_INSTRN_THREAD_INST_CFG_GRANT_CNT_WORD {10, 1, 0, 0, 1, 0}
#define DEBUG_BUS_STALL_INST_CNT_0__PERF_CNT_INSTRN_THREAD_INST_CFG_GRANT_CNT {DEBUG_BUS_STALL_INST_CNT_0__PERF_CNT_INSTRN_THREAD_INST_CFG_GRANT_CNT_WORD, 0, 0xFFFFFFFF}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[1] alias: debug_bus_thread_state note: THREAD1
//  name: i_ttsync_dbg_bits
//  name: ibuffer_stall
#define DEBUG_BUS_THREAD_STATE_1__I_TTSYNC_DBG_BITS_IBUFFER_STALL_WORD {9, 1, 0, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_1__I_TTSYNC_DBG_BITS_IBUFFER_STALL {DEBUG_BUS_THREAD_STATE_1__I_TTSYNC_DBG_BITS_IBUFFER_STALL_WORD, 7, 0x1}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[1] alias: debug_bus_thread_state note: THREAD1
//  name: i_ttsync_dbg_bits
//  name: risc_cfg_stall
#define DEBUG_BUS_THREAD_STATE_1__I_TTSYNC_DBG_BITS_RISC_CFG_STALL_WORD {9, 1, 0, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_1__I_TTSYNC_DBG_BITS_RISC_CFG_STALL {DEBUG_BUS_THREAD_STATE_1__I_TTSYNC_DBG_BITS_RISC_CFG_STALL_WORD, 6, 0x1}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[1] alias: debug_bus_thread_state note: THREAD1
//  name: i_ttsync_dbg_bits
//  name: risc_gpr_stall
#define DEBUG_BUS_THREAD_STATE_1__I_TTSYNC_DBG_BITS_RISC_GPR_STALL_WORD {9, 1, 0, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_1__I_TTSYNC_DBG_BITS_RISC_GPR_STALL {DEBUG_BUS_THREAD_STATE_1__I_TTSYNC_DBG_BITS_RISC_GPR_STALL_WORD, 5, 0x1}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[1] alias: debug_bus_thread_state note: THREAD1
//  name: i_ttsync_dbg_bits
//  name: risc_tdma_stall
#define DEBUG_BUS_THREAD_STATE_1__I_TTSYNC_DBG_BITS_RISC_TDMA_STALL_WORD {9, 1, 0, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_1__I_TTSYNC_DBG_BITS_RISC_TDMA_STALL {DEBUG_BUS_THREAD_STATE_1__I_TTSYNC_DBG_BITS_RISC_TDMA_STALL_WORD, 4, 0x1}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[1] alias: debug_bus_thread_state note: THREAD1
//  name: i_ttsync_dbg_bits
//  name: prev_gen_no
#define DEBUG_BUS_THREAD_STATE_1__I_TTSYNC_DBG_BITS_PREV_GEN_NO__3_0_WORD {8, 1, 6, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_1__I_TTSYNC_DBG_BITS_PREV_GEN_NO__3_0 {DEBUG_BUS_THREAD_STATE_1__I_TTSYNC_DBG_BITS_PREV_GEN_NO__3_0_WORD, 28, 0xF}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[1] alias: debug_bus_thread_state note: THREAD1
//  name: i_ttsync_dbg_bits
//  name: prev_gen_no
#define DEBUG_BUS_THREAD_STATE_1__I_TTSYNC_DBG_BITS_PREV_GEN_NO__7_4_WORD {9, 1, 0, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_1__I_TTSYNC_DBG_BITS_PREV_GEN_NO__7_4 {DEBUG_BUS_THREAD_STATE_1__I_TTSYNC_DBG_BITS_PREV_GEN_NO__7_4_WORD, 0, 0xF}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[1] alias: debug_bus_thread_state note: THREAD1
//  name: i_ttsync_dbg_bits
//  name: lsq_head_valid
#define DEBUG_BUS_THREAD_STATE_1__I_TTSYNC_DBG_BITS_LSQ_HEAD_VALID_WORD {8, 1, 6, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_1__I_TTSYNC_DBG_BITS_LSQ_HEAD_VALID {DEBUG_BUS_THREAD_STATE_1__I_TTSYNC_DBG_BITS_LSQ_HEAD_VALID_WORD, 27, 0x1}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[1] alias: debug_bus_thread_state note: THREAD1
//  name: i_ttsync_dbg_bits
//  name: lsq_head_rsrcs alias: ttsync_rsrc_t
//  name: wr_tdma
#define DEBUG_BUS_THREAD_STATE_1__I_TTSYNC_DBG_BITS_LSQ_HEAD_RSRCS_WR_TDMA_WORD {8, 1, 6, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_1__I_TTSYNC_DBG_BITS_LSQ_HEAD_RSRCS_WR_TDMA {DEBUG_BUS_THREAD_STATE_1__I_TTSYNC_DBG_BITS_LSQ_HEAD_RSRCS_WR_TDMA_WORD, 26, 0x1}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[1] alias: debug_bus_thread_state note: THREAD1
//  name: i_ttsync_dbg_bits
//  name: lsq_head_rsrcs alias: ttsync_rsrc_t
//  name: rd_tdma
#define DEBUG_BUS_THREAD_STATE_1__I_TTSYNC_DBG_BITS_LSQ_HEAD_RSRCS_RD_TDMA_WORD {8, 1, 6, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_1__I_TTSYNC_DBG_BITS_LSQ_HEAD_RSRCS_RD_TDMA {DEBUG_BUS_THREAD_STATE_1__I_TTSYNC_DBG_BITS_LSQ_HEAD_RSRCS_RD_TDMA_WORD, 25, 0x1}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[1] alias: debug_bus_thread_state note: THREAD1
//  name: i_ttsync_dbg_bits
//  name: lsq_head_rsrcs alias: ttsync_rsrc_t
//  name: wr_gpr
#define DEBUG_BUS_THREAD_STATE_1__I_TTSYNC_DBG_BITS_LSQ_HEAD_RSRCS_WR_GPR_WORD {8, 1, 6, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_1__I_TTSYNC_DBG_BITS_LSQ_HEAD_RSRCS_WR_GPR {DEBUG_BUS_THREAD_STATE_1__I_TTSYNC_DBG_BITS_LSQ_HEAD_RSRCS_WR_GPR_WORD, 24, 0x1}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[1] alias: debug_bus_thread_state note: THREAD1
//  name: i_ttsync_dbg_bits
//  name: lsq_head_rsrcs alias: ttsync_rsrc_t
//  name: rd_gpr
#define DEBUG_BUS_THREAD_STATE_1__I_TTSYNC_DBG_BITS_LSQ_HEAD_RSRCS_RD_GPR_WORD {8, 1, 6, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_1__I_TTSYNC_DBG_BITS_LSQ_HEAD_RSRCS_RD_GPR {DEBUG_BUS_THREAD_STATE_1__I_TTSYNC_DBG_BITS_LSQ_HEAD_RSRCS_RD_GPR_WORD, 23, 0x1}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[1] alias: debug_bus_thread_state note: THREAD1
//  name: i_ttsync_dbg_bits
//  name: lsq_head_rsrcs alias: ttsync_rsrc_t
//  name: target_cfg_space
#define DEBUG_BUS_THREAD_STATE_1__I_TTSYNC_DBG_BITS_LSQ_HEAD_RSRCS_TARGET_CFG_SPACE_WORD {8, 1, 6, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_1__I_TTSYNC_DBG_BITS_LSQ_HEAD_RSRCS_TARGET_CFG_SPACE {DEBUG_BUS_THREAD_STATE_1__I_TTSYNC_DBG_BITS_LSQ_HEAD_RSRCS_TARGET_CFG_SPACE_WORD, 20, 0x7}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[1] alias: debug_bus_thread_state note: THREAD1
//  name: i_ttsync_dbg_bits
//  name: lsq_head_rsrcs alias: ttsync_rsrc_t
//  name: cfg_state
#define DEBUG_BUS_THREAD_STATE_1__I_TTSYNC_DBG_BITS_LSQ_HEAD_RSRCS_CFG_STATE_WORD {8, 1, 6, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_1__I_TTSYNC_DBG_BITS_LSQ_HEAD_RSRCS_CFG_STATE {DEBUG_BUS_THREAD_STATE_1__I_TTSYNC_DBG_BITS_LSQ_HEAD_RSRCS_CFG_STATE_WORD, 18, 0x3}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[1] alias: debug_bus_thread_state note: THREAD1
//  name: i_ttsync_dbg_bits
//  name: lsq_head_rsrcs alias: ttsync_rsrc_t
//  name: wr_cfg
#define DEBUG_BUS_THREAD_STATE_1__I_TTSYNC_DBG_BITS_LSQ_HEAD_RSRCS_WR_CFG_WORD {8, 1, 6, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_1__I_TTSYNC_DBG_BITS_LSQ_HEAD_RSRCS_WR_CFG {DEBUG_BUS_THREAD_STATE_1__I_TTSYNC_DBG_BITS_LSQ_HEAD_RSRCS_WR_CFG_WORD, 17, 0x1}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[1] alias: debug_bus_thread_state note: THREAD1
//  name: i_ttsync_dbg_bits
//  name: lsq_head_rsrcs alias: ttsync_rsrc_t
//  name: rd_cfg
#define DEBUG_BUS_THREAD_STATE_1__I_TTSYNC_DBG_BITS_LSQ_HEAD_RSRCS_RD_CFG_WORD {8, 1, 6, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_1__I_TTSYNC_DBG_BITS_LSQ_HEAD_RSRCS_RD_CFG {DEBUG_BUS_THREAD_STATE_1__I_TTSYNC_DBG_BITS_LSQ_HEAD_RSRCS_RD_CFG_WORD, 16, 0x1}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[1] alias: debug_bus_thread_state note: THREAD1
//  name: i_ttsync_dbg_bits
//  name: lsq_head_gen_no
#define DEBUG_BUS_THREAD_STATE_1__I_TTSYNC_DBG_BITS_LSQ_HEAD_GEN_NO_WORD {8, 1, 6, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_1__I_TTSYNC_DBG_BITS_LSQ_HEAD_GEN_NO {DEBUG_BUS_THREAD_STATE_1__I_TTSYNC_DBG_BITS_LSQ_HEAD_GEN_NO_WORD, 8, 0xFF}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[1] alias: debug_bus_thread_state note: THREAD1
//  name: i_ttsync_dbg_bits
//  name: lsq_full
#define DEBUG_BUS_THREAD_STATE_1__I_TTSYNC_DBG_BITS_LSQ_FULL_WORD {8, 1, 6, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_1__I_TTSYNC_DBG_BITS_LSQ_FULL {DEBUG_BUS_THREAD_STATE_1__I_TTSYNC_DBG_BITS_LSQ_FULL_WORD, 7, 0x1}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[1] alias: debug_bus_thread_state note: THREAD1
//  name: i_ttsync_dbg_bits
//  name: rq_head_valid
#define DEBUG_BUS_THREAD_STATE_1__I_TTSYNC_DBG_BITS_RQ_HEAD_VALID_WORD {8, 1, 6, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_1__I_TTSYNC_DBG_BITS_RQ_HEAD_VALID {DEBUG_BUS_THREAD_STATE_1__I_TTSYNC_DBG_BITS_RQ_HEAD_VALID_WORD, 6, 0x1}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[1] alias: debug_bus_thread_state note: THREAD1
//  name: i_ttsync_dbg_bits
//  name: rq_head_rsrcs alias: ttsync_rsrc_t
//  name: wr_tdma
#define DEBUG_BUS_THREAD_STATE_1__I_TTSYNC_DBG_BITS_RQ_HEAD_RSRCS_WR_TDMA_WORD {8, 1, 6, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_1__I_TTSYNC_DBG_BITS_RQ_HEAD_RSRCS_WR_TDMA {DEBUG_BUS_THREAD_STATE_1__I_TTSYNC_DBG_BITS_RQ_HEAD_RSRCS_WR_TDMA_WORD, 5, 0x1}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[1] alias: debug_bus_thread_state note: THREAD1
//  name: i_ttsync_dbg_bits
//  name: rq_head_rsrcs alias: ttsync_rsrc_t
//  name: rd_tdma
#define DEBUG_BUS_THREAD_STATE_1__I_TTSYNC_DBG_BITS_RQ_HEAD_RSRCS_RD_TDMA_WORD {8, 1, 6, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_1__I_TTSYNC_DBG_BITS_RQ_HEAD_RSRCS_RD_TDMA {DEBUG_BUS_THREAD_STATE_1__I_TTSYNC_DBG_BITS_RQ_HEAD_RSRCS_RD_TDMA_WORD, 4, 0x1}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[1] alias: debug_bus_thread_state note: THREAD1
//  name: i_ttsync_dbg_bits
//  name: rq_head_rsrcs alias: ttsync_rsrc_t
//  name: wr_gpr
#define DEBUG_BUS_THREAD_STATE_1__I_TTSYNC_DBG_BITS_RQ_HEAD_RSRCS_WR_GPR_WORD {8, 1, 6, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_1__I_TTSYNC_DBG_BITS_RQ_HEAD_RSRCS_WR_GPR {DEBUG_BUS_THREAD_STATE_1__I_TTSYNC_DBG_BITS_RQ_HEAD_RSRCS_WR_GPR_WORD, 3, 0x1}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[1] alias: debug_bus_thread_state note: THREAD1
//  name: i_ttsync_dbg_bits
//  name: rq_head_rsrcs alias: ttsync_rsrc_t
//  name: rd_gpr
#define DEBUG_BUS_THREAD_STATE_1__I_TTSYNC_DBG_BITS_RQ_HEAD_RSRCS_RD_GPR_WORD {8, 1, 6, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_1__I_TTSYNC_DBG_BITS_RQ_HEAD_RSRCS_RD_GPR {DEBUG_BUS_THREAD_STATE_1__I_TTSYNC_DBG_BITS_RQ_HEAD_RSRCS_RD_GPR_WORD, 2, 0x1}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[1] alias: debug_bus_thread_state note: THREAD1
//  name: i_ttsync_dbg_bits
//  name: rq_head_rsrcs alias: ttsync_rsrc_t
//  name: target_cfg_space
#define DEBUG_BUS_THREAD_STATE_1__I_TTSYNC_DBG_BITS_RQ_HEAD_RSRCS_TARGET_CFG_SPACE__0_0_WORD {8, 1, 4, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_1__I_TTSYNC_DBG_BITS_RQ_HEAD_RSRCS_TARGET_CFG_SPACE__0_0 {DEBUG_BUS_THREAD_STATE_1__I_TTSYNC_DBG_BITS_RQ_HEAD_RSRCS_TARGET_CFG_SPACE__0_0_WORD, 31, 0x1}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[1] alias: debug_bus_thread_state note: THREAD1
//  name: i_ttsync_dbg_bits
//  name: rq_head_rsrcs alias: ttsync_rsrc_t
//  name: target_cfg_space
#define DEBUG_BUS_THREAD_STATE_1__I_TTSYNC_DBG_BITS_RQ_HEAD_RSRCS_TARGET_CFG_SPACE__2_1_WORD {8, 1, 6, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_1__I_TTSYNC_DBG_BITS_RQ_HEAD_RSRCS_TARGET_CFG_SPACE__2_1 {DEBUG_BUS_THREAD_STATE_1__I_TTSYNC_DBG_BITS_RQ_HEAD_RSRCS_TARGET_CFG_SPACE__2_1_WORD, 0, 0x3}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[1] alias: debug_bus_thread_state note: THREAD1
//  name: i_ttsync_dbg_bits
//  name: rq_head_rsrcs alias: ttsync_rsrc_t
//  name: cfg_state
#define DEBUG_BUS_THREAD_STATE_1__I_TTSYNC_DBG_BITS_RQ_HEAD_RSRCS_CFG_STATE_WORD {8, 1, 4, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_1__I_TTSYNC_DBG_BITS_RQ_HEAD_RSRCS_CFG_STATE {DEBUG_BUS_THREAD_STATE_1__I_TTSYNC_DBG_BITS_RQ_HEAD_RSRCS_CFG_STATE_WORD, 29, 0x3}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[1] alias: debug_bus_thread_state note: THREAD1
//  name: i_ttsync_dbg_bits
//  name: rq_head_rsrcs alias: ttsync_rsrc_t
//  name: wr_cfg
#define DEBUG_BUS_THREAD_STATE_1__I_TTSYNC_DBG_BITS_RQ_HEAD_RSRCS_WR_CFG_WORD {8, 1, 4, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_1__I_TTSYNC_DBG_BITS_RQ_HEAD_RSRCS_WR_CFG {DEBUG_BUS_THREAD_STATE_1__I_TTSYNC_DBG_BITS_RQ_HEAD_RSRCS_WR_CFG_WORD, 28, 0x1}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[1] alias: debug_bus_thread_state note: THREAD1
//  name: i_ttsync_dbg_bits
//  name: rq_head_rsrcs alias: ttsync_rsrc_t
//  name: rd_cfg
#define DEBUG_BUS_THREAD_STATE_1__I_TTSYNC_DBG_BITS_RQ_HEAD_RSRCS_RD_CFG_WORD {8, 1, 4, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_1__I_TTSYNC_DBG_BITS_RQ_HEAD_RSRCS_RD_CFG {DEBUG_BUS_THREAD_STATE_1__I_TTSYNC_DBG_BITS_RQ_HEAD_RSRCS_RD_CFG_WORD, 27, 0x1}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[1] alias: debug_bus_thread_state note: THREAD1
//  name: i_ttsync_dbg_bits
//  name: rq_head_gen_no
#define DEBUG_BUS_THREAD_STATE_1__I_TTSYNC_DBG_BITS_RQ_HEAD_GEN_NO_WORD {8, 1, 4, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_1__I_TTSYNC_DBG_BITS_RQ_HEAD_GEN_NO {DEBUG_BUS_THREAD_STATE_1__I_TTSYNC_DBG_BITS_RQ_HEAD_GEN_NO_WORD, 19, 0xFF}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[1] alias: debug_bus_thread_state note: THREAD1
//  name: i_ttsync_dbg_bits
//  name: rq_full
#define DEBUG_BUS_THREAD_STATE_1__I_TTSYNC_DBG_BITS_RQ_FULL_WORD {8, 1, 4, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_1__I_TTSYNC_DBG_BITS_RQ_FULL {DEBUG_BUS_THREAD_STATE_1__I_TTSYNC_DBG_BITS_RQ_FULL_WORD, 18, 0x1}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[1] alias: debug_bus_thread_state note: THREAD1
//  name: i_cg_trisc_busy
#define DEBUG_BUS_THREAD_STATE_1__I_CG_TRISC_BUSY_WORD {8, 1, 4, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_1__I_CG_TRISC_BUSY {DEBUG_BUS_THREAD_STATE_1__I_CG_TRISC_BUSY_WORD, 17, 0x1}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[1] alias: debug_bus_thread_state note: THREAD1
//  name: machine_busy
#define DEBUG_BUS_THREAD_STATE_1__MACHINE_BUSY_WORD {8, 1, 4, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_1__MACHINE_BUSY {DEBUG_BUS_THREAD_STATE_1__MACHINE_BUSY_WORD, 16, 0x1}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[1] alias: debug_bus_thread_state note: THREAD1
//  name: req_iramd_buffer_not_empty
#define DEBUG_BUS_THREAD_STATE_1__REQ_IRAMD_BUFFER_NOT_EMPTY_WORD {8, 1, 4, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_1__REQ_IRAMD_BUFFER_NOT_EMPTY {DEBUG_BUS_THREAD_STATE_1__REQ_IRAMD_BUFFER_NOT_EMPTY_WORD, 15, 0x1}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[1] alias: debug_bus_thread_state note: THREAD1
//  name: gpr_file_busy
#define DEBUG_BUS_THREAD_STATE_1__GPR_FILE_BUSY_WORD {8, 1, 4, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_1__GPR_FILE_BUSY {DEBUG_BUS_THREAD_STATE_1__GPR_FILE_BUSY_WORD, 14, 0x1}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[1] alias: debug_bus_thread_state note: THREAD1
//  name: cfg_exu_busy
#define DEBUG_BUS_THREAD_STATE_1__CFG_EXU_BUSY_WORD {8, 1, 4, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_1__CFG_EXU_BUSY {DEBUG_BUS_THREAD_STATE_1__CFG_EXU_BUSY_WORD, 13, 0x1}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[1] alias: debug_bus_thread_state note: THREAD1
//  name: req_iramd_buffer_empty
#define DEBUG_BUS_THREAD_STATE_1__REQ_IRAMD_BUFFER_EMPTY_WORD {8, 1, 4, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_1__REQ_IRAMD_BUFFER_EMPTY {DEBUG_BUS_THREAD_STATE_1__REQ_IRAMD_BUFFER_EMPTY_WORD, 12, 0x1}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[1] alias: debug_bus_thread_state note: THREAD1
//  name: req_iramd_buffer_full
#define DEBUG_BUS_THREAD_STATE_1__REQ_IRAMD_BUFFER_FULL_WORD {8, 1, 4, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_1__REQ_IRAMD_BUFFER_FULL {DEBUG_BUS_THREAD_STATE_1__REQ_IRAMD_BUFFER_FULL_WORD, 11, 0x1}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[1] alias: debug_bus_thread_state note: THREAD1
//  name: ~ibuffer_rtr
#define DEBUG_BUS_THREAD_STATE_1___IBUFFER_RTR_WORD {8, 1, 4, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_1___IBUFFER_RTR {DEBUG_BUS_THREAD_STATE_1___IBUFFER_RTR_WORD, 10, 0x1}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[1] alias: debug_bus_thread_state note: THREAD1
//  name: ibuffer_empty
#define DEBUG_BUS_THREAD_STATE_1__IBUFFER_EMPTY_WORD {8, 1, 4, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_1__IBUFFER_EMPTY {DEBUG_BUS_THREAD_STATE_1__IBUFFER_EMPTY_WORD, 9, 0x1}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[1] alias: debug_bus_thread_state note: THREAD1
//  name: ibuffer_empty_raw
#define DEBUG_BUS_THREAD_STATE_1__IBUFFER_EMPTY_RAW_WORD {8, 1, 4, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_1__IBUFFER_EMPTY_RAW {DEBUG_BUS_THREAD_STATE_1__IBUFFER_EMPTY_RAW_WORD, 8, 0x1}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[1] alias: debug_bus_thread_state note: THREAD1
//  name: thread_inst
#define DEBUG_BUS_THREAD_STATE_1__THREAD_INST__23_0_WORD {8, 1, 2, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_1__THREAD_INST__23_0 {DEBUG_BUS_THREAD_STATE_1__THREAD_INST__23_0_WORD, 8, 0xFFFFFF}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[1] alias: debug_bus_thread_state note: THREAD1
//  name: thread_inst
#define DEBUG_BUS_THREAD_STATE_1__THREAD_INST__31_24_WORD {8, 1, 4, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_1__THREAD_INST__31_24 {DEBUG_BUS_THREAD_STATE_1__THREAD_INST__31_24_WORD, 0, 0xFF}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[1] alias: debug_bus_thread_state note: THREAD1
//  name: math_inst
#define DEBUG_BUS_THREAD_STATE_1__MATH_INST_WORD {8, 1, 2, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_1__MATH_INST {DEBUG_BUS_THREAD_STATE_1__MATH_INST_WORD, 7, 0x1}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[1] alias: debug_bus_thread_state note: THREAD1
//  name: tdma_inst
#define DEBUG_BUS_THREAD_STATE_1__TDMA_INST_WORD {8, 1, 2, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_1__TDMA_INST {DEBUG_BUS_THREAD_STATE_1__TDMA_INST_WORD, 6, 0x1}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[1] alias: debug_bus_thread_state note: THREAD1
//  name: pack_inst
#define DEBUG_BUS_THREAD_STATE_1__PACK_INST_WORD {8, 1, 2, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_1__PACK_INST {DEBUG_BUS_THREAD_STATE_1__PACK_INST_WORD, 5, 0x1}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[1] alias: debug_bus_thread_state note: THREAD1
//  name: move_inst
#define DEBUG_BUS_THREAD_STATE_1__MOVE_INST_WORD {8, 1, 2, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_1__MOVE_INST {DEBUG_BUS_THREAD_STATE_1__MOVE_INST_WORD, 4, 0x1}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[1] alias: debug_bus_thread_state note: THREAD1
//  name: sfpu_inst
#define DEBUG_BUS_THREAD_STATE_1__SFPU_INST_WORD {8, 1, 2, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_1__SFPU_INST {DEBUG_BUS_THREAD_STATE_1__SFPU_INST_WORD, 3, 0x1}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[1] alias: debug_bus_thread_state note: THREAD1
//  name: unpack_inst
#define DEBUG_BUS_THREAD_STATE_1__UNPACK_INST_WORD {8, 1, 2, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_1__UNPACK_INST {DEBUG_BUS_THREAD_STATE_1__UNPACK_INST_WORD, 1, 0x3}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[1] alias: debug_bus_thread_state note: THREAD1
//  name: xsearch_inst
#define DEBUG_BUS_THREAD_STATE_1__XSEARCH_INST_WORD {8, 1, 2, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_1__XSEARCH_INST {DEBUG_BUS_THREAD_STATE_1__XSEARCH_INST_WORD, 0, 0x1}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[1] alias: debug_bus_thread_state note: THREAD1
//  name: thcon_inst
#define DEBUG_BUS_THREAD_STATE_1__THCON_INST_WORD {8, 1, 0, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_1__THCON_INST {DEBUG_BUS_THREAD_STATE_1__THCON_INST_WORD, 31, 0x1}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[1] alias: debug_bus_thread_state note: THREAD1
//  name: sync_inst
#define DEBUG_BUS_THREAD_STATE_1__SYNC_INST_WORD {8, 1, 0, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_1__SYNC_INST {DEBUG_BUS_THREAD_STATE_1__SYNC_INST_WORD, 30, 0x1}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[1] alias: debug_bus_thread_state note: THREAD1
//  name: cfg_inst
#define DEBUG_BUS_THREAD_STATE_1__CFG_INST_WORD {8, 1, 0, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_1__CFG_INST {DEBUG_BUS_THREAD_STATE_1__CFG_INST_WORD, 29, 0x1}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[1] alias: debug_bus_thread_state note: THREAD1
//  name: stalled_pack_inst
#define DEBUG_BUS_THREAD_STATE_1__STALLED_PACK_INST_WORD {8, 1, 0, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_1__STALLED_PACK_INST {DEBUG_BUS_THREAD_STATE_1__STALLED_PACK_INST_WORD, 28, 0x1}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[1] alias: debug_bus_thread_state note: THREAD1
//  name: stalled_unpack_inst
#define DEBUG_BUS_THREAD_STATE_1__STALLED_UNPACK_INST_WORD {8, 1, 0, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_1__STALLED_UNPACK_INST {DEBUG_BUS_THREAD_STATE_1__STALLED_UNPACK_INST_WORD, 26, 0x3}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[1] alias: debug_bus_thread_state note: THREAD1
//  name: stalled_math_inst
#define DEBUG_BUS_THREAD_STATE_1__STALLED_MATH_INST_WORD {8, 1, 0, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_1__STALLED_MATH_INST {DEBUG_BUS_THREAD_STATE_1__STALLED_MATH_INST_WORD, 25, 0x1}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[1] alias: debug_bus_thread_state note: THREAD1
//  name: stalled_tdma_inst
#define DEBUG_BUS_THREAD_STATE_1__STALLED_TDMA_INST_WORD {8, 1, 0, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_1__STALLED_TDMA_INST {DEBUG_BUS_THREAD_STATE_1__STALLED_TDMA_INST_WORD, 24, 0x1}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[1] alias: debug_bus_thread_state note: THREAD1
//  name: stalled_move_inst
#define DEBUG_BUS_THREAD_STATE_1__STALLED_MOVE_INST_WORD {8, 1, 0, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_1__STALLED_MOVE_INST {DEBUG_BUS_THREAD_STATE_1__STALLED_MOVE_INST_WORD, 23, 0x1}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[1] alias: debug_bus_thread_state note: THREAD1
//  name: stalled_xsearch_inst
#define DEBUG_BUS_THREAD_STATE_1__STALLED_XSEARCH_INST_WORD {8, 1, 0, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_1__STALLED_XSEARCH_INST {DEBUG_BUS_THREAD_STATE_1__STALLED_XSEARCH_INST_WORD, 22, 0x1}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[1] alias: debug_bus_thread_state note: THREAD1
//  name: stalled_thcon_inst
#define DEBUG_BUS_THREAD_STATE_1__STALLED_THCON_INST_WORD {8, 1, 0, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_1__STALLED_THCON_INST {DEBUG_BUS_THREAD_STATE_1__STALLED_THCON_INST_WORD, 21, 0x1}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[1] alias: debug_bus_thread_state note: THREAD1
//  name: stalled_sync_inst
#define DEBUG_BUS_THREAD_STATE_1__STALLED_SYNC_INST_WORD {8, 1, 0, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_1__STALLED_SYNC_INST {DEBUG_BUS_THREAD_STATE_1__STALLED_SYNC_INST_WORD, 20, 0x1}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[1] alias: debug_bus_thread_state note: THREAD1
//  name: stalled_cfg_inst
#define DEBUG_BUS_THREAD_STATE_1__STALLED_CFG_INST_WORD {8, 1, 0, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_1__STALLED_CFG_INST {DEBUG_BUS_THREAD_STATE_1__STALLED_CFG_INST_WORD, 19, 0x1}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[1] alias: debug_bus_thread_state note: THREAD1
//  name: stalled_sfpu_inst
#define DEBUG_BUS_THREAD_STATE_1__STALLED_SFPU_INST_WORD {8, 1, 0, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_1__STALLED_SFPU_INST {DEBUG_BUS_THREAD_STATE_1__STALLED_SFPU_INST_WORD, 18, 0x1}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[1] alias: debug_bus_thread_state note: THREAD1
//  name: tdma_kick_move
#define DEBUG_BUS_THREAD_STATE_1__TDMA_KICK_MOVE_WORD {8, 1, 0, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_1__TDMA_KICK_MOVE {DEBUG_BUS_THREAD_STATE_1__TDMA_KICK_MOVE_WORD, 17, 0x1}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[1] alias: debug_bus_thread_state note: THREAD1
//  name: tdma_kick_pack
#define DEBUG_BUS_THREAD_STATE_1__TDMA_KICK_PACK_WORD {8, 1, 0, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_1__TDMA_KICK_PACK {DEBUG_BUS_THREAD_STATE_1__TDMA_KICK_PACK_WORD, 16, 0x1}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[1] alias: debug_bus_thread_state note: THREAD1
//  name: tdma_kick_unpack
#define DEBUG_BUS_THREAD_STATE_1__TDMA_KICK_UNPACK_WORD {8, 1, 0, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_1__TDMA_KICK_UNPACK {DEBUG_BUS_THREAD_STATE_1__TDMA_KICK_UNPACK_WORD, 14, 0x3}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[1] alias: debug_bus_thread_state note: THREAD1
//  name: tdma_kick_xsearch
#define DEBUG_BUS_THREAD_STATE_1__TDMA_KICK_XSEARCH_WORD {8, 1, 0, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_1__TDMA_KICK_XSEARCH {DEBUG_BUS_THREAD_STATE_1__TDMA_KICK_XSEARCH_WORD, 13, 0x1}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[1] alias: debug_bus_thread_state note: THREAD1
//  name: tdma_kick_thcon
#define DEBUG_BUS_THREAD_STATE_1__TDMA_KICK_THCON_WORD {8, 1, 0, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_1__TDMA_KICK_THCON {DEBUG_BUS_THREAD_STATE_1__TDMA_KICK_THCON_WORD, 12, 0x1}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[1] alias: debug_bus_thread_state note: THREAD1
//  name: tdma_status_busy
#define DEBUG_BUS_THREAD_STATE_1__TDMA_STATUS_BUSY_WORD {8, 1, 0, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_1__TDMA_STATUS_BUSY {DEBUG_BUS_THREAD_STATE_1__TDMA_STATUS_BUSY_WORD, 7, 0x1F}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[1] alias: debug_bus_thread_state note: THREAD1
//  name: packer_busy
#define DEBUG_BUS_THREAD_STATE_1__PACKER_BUSY_WORD {8, 1, 0, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_1__PACKER_BUSY {DEBUG_BUS_THREAD_STATE_1__PACKER_BUSY_WORD, 6, 0x1}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[1] alias: debug_bus_thread_state note: THREAD1
//  name: unpacker_busy
#define DEBUG_BUS_THREAD_STATE_1__UNPACKER_BUSY_WORD {8, 1, 0, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_1__UNPACKER_BUSY {DEBUG_BUS_THREAD_STATE_1__UNPACKER_BUSY_WORD, 4, 0x3}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[1] alias: debug_bus_thread_state note: THREAD1
//  name: thcon_busy
#define DEBUG_BUS_THREAD_STATE_1__THCON_BUSY_WORD {8, 1, 0, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_1__THCON_BUSY {DEBUG_BUS_THREAD_STATE_1__THCON_BUSY_WORD, 3, 0x1}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[1] alias: debug_bus_thread_state note: THREAD1
//  name: move_busy
#define DEBUG_BUS_THREAD_STATE_1__MOVE_BUSY_WORD {8, 1, 0, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_1__MOVE_BUSY {DEBUG_BUS_THREAD_STATE_1__MOVE_BUSY_WORD, 2, 0x1}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[1] alias: debug_bus_thread_state note: THREAD1
//  name: xsearch_busy
#define DEBUG_BUS_THREAD_STATE_1__XSEARCH_BUSY_WORD {8, 1, 0, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_1__XSEARCH_BUSY {DEBUG_BUS_THREAD_STATE_1__XSEARCH_BUSY_WORD, 0, 0x3}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_stall_inst_cnt[1] alias: perf_cnt_instrn_thread_inst_cnts note: THREAD1
//  name: perf_cnt_instrn_thread_inst_pack alias: perf_cnt
//  name: grant_cnt
#define DEBUG_BUS_STALL_INST_CNT_1__PERF_CNT_INSTRN_THREAD_INST_PACK_GRANT_CNT_WORD {7, 1, 6, 0, 1, 0}
#define DEBUG_BUS_STALL_INST_CNT_1__PERF_CNT_INSTRN_THREAD_INST_PACK_GRANT_CNT {DEBUG_BUS_STALL_INST_CNT_1__PERF_CNT_INSTRN_THREAD_INST_PACK_GRANT_CNT_WORD, 0, 0xFFFFFFFF}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_stall_inst_cnt[1] alias: perf_cnt_instrn_thread_inst_cnts note: THREAD1
//  name: perf_cnt_instrn_thread_inst_unpack alias: perf_cnt
//  name: grant_cnt
#define DEBUG_BUS_STALL_INST_CNT_1__PERF_CNT_INSTRN_THREAD_INST_UNPACK_GRANT_CNT_WORD {7, 1, 4, 0, 1, 0}
#define DEBUG_BUS_STALL_INST_CNT_1__PERF_CNT_INSTRN_THREAD_INST_UNPACK_GRANT_CNT {DEBUG_BUS_STALL_INST_CNT_1__PERF_CNT_INSTRN_THREAD_INST_UNPACK_GRANT_CNT_WORD, 0, 0xFFFFFFFF}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_stall_inst_cnt[1] alias: perf_cnt_instrn_thread_inst_cnts note: THREAD1
//  name: perf_cnt_instrn_thread_inst_math alias: perf_cnt
//  name: grant_cnt
#define DEBUG_BUS_STALL_INST_CNT_1__PERF_CNT_INSTRN_THREAD_INST_MATH_GRANT_CNT_WORD {7, 1, 2, 0, 1, 0}
#define DEBUG_BUS_STALL_INST_CNT_1__PERF_CNT_INSTRN_THREAD_INST_MATH_GRANT_CNT {DEBUG_BUS_STALL_INST_CNT_1__PERF_CNT_INSTRN_THREAD_INST_MATH_GRANT_CNT_WORD, 0, 0xFFFFFFFF}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_stall_inst_cnt[1] alias: perf_cnt_instrn_thread_inst_cnts note: THREAD1
//  name: perf_cnt_instrn_thread_inst_move alias: perf_cnt
//  name: grant_cnt
#define DEBUG_BUS_STALL_INST_CNT_1__PERF_CNT_INSTRN_THREAD_INST_MOVE_GRANT_CNT_WORD {7, 1, 0, 0, 1, 0}
#define DEBUG_BUS_STALL_INST_CNT_1__PERF_CNT_INSTRN_THREAD_INST_MOVE_GRANT_CNT {DEBUG_BUS_STALL_INST_CNT_1__PERF_CNT_INSTRN_THREAD_INST_MOVE_GRANT_CNT_WORD, 0, 0xFFFFFFFF}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_stall_inst_cnt[1] alias: perf_cnt_instrn_thread_inst_cnts note: THREAD1
//  name: perf_cnt_instrn_thread_inst_xsearch alias: perf_cnt
//  name: grant_cnt
#define DEBUG_BUS_STALL_INST_CNT_1__PERF_CNT_INSTRN_THREAD_INST_XSEARCH_GRANT_CNT_WORD {6, 1, 6, 0, 1, 0}
#define DEBUG_BUS_STALL_INST_CNT_1__PERF_CNT_INSTRN_THREAD_INST_XSEARCH_GRANT_CNT {DEBUG_BUS_STALL_INST_CNT_1__PERF_CNT_INSTRN_THREAD_INST_XSEARCH_GRANT_CNT_WORD, 0, 0xFFFFFFFF}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_stall_inst_cnt[1] alias: perf_cnt_instrn_thread_inst_cnts note: THREAD1
//  name: perf_cnt_instrn_thread_inst_thcon alias: perf_cnt
//  name: grant_cnt
#define DEBUG_BUS_STALL_INST_CNT_1__PERF_CNT_INSTRN_THREAD_INST_THCON_GRANT_CNT_WORD {6, 1, 4, 0, 1, 0}
#define DEBUG_BUS_STALL_INST_CNT_1__PERF_CNT_INSTRN_THREAD_INST_THCON_GRANT_CNT {DEBUG_BUS_STALL_INST_CNT_1__PERF_CNT_INSTRN_THREAD_INST_THCON_GRANT_CNT_WORD, 0, 0xFFFFFFFF}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_stall_inst_cnt[1] alias: perf_cnt_instrn_thread_inst_cnts note: THREAD1
//  name: perf_cnt_instrn_thread_inst_sync alias: perf_cnt
//  name: grant_cnt
#define DEBUG_BUS_STALL_INST_CNT_1__PERF_CNT_INSTRN_THREAD_INST_SYNC_GRANT_CNT_WORD {6, 1, 2, 0, 1, 0}
#define DEBUG_BUS_STALL_INST_CNT_1__PERF_CNT_INSTRN_THREAD_INST_SYNC_GRANT_CNT {DEBUG_BUS_STALL_INST_CNT_1__PERF_CNT_INSTRN_THREAD_INST_SYNC_GRANT_CNT_WORD, 0, 0xFFFFFFFF}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_stall_inst_cnt[1] alias: perf_cnt_instrn_thread_inst_cnts note: THREAD1
//  name: perf_cnt_instrn_thread_inst_cfg alias: perf_cnt
//  name: grant_cnt
#define DEBUG_BUS_STALL_INST_CNT_1__PERF_CNT_INSTRN_THREAD_INST_CFG_GRANT_CNT_WORD {6, 1, 0, 0, 1, 0}
#define DEBUG_BUS_STALL_INST_CNT_1__PERF_CNT_INSTRN_THREAD_INST_CFG_GRANT_CNT {DEBUG_BUS_STALL_INST_CNT_1__PERF_CNT_INSTRN_THREAD_INST_CFG_GRANT_CNT_WORD, 0, 0xFFFFFFFF}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[2] alias: debug_bus_thread_state note: THREAD2
//  name: i_ttsync_dbg_bits
//  name: ibuffer_stall
#define DEBUG_BUS_THREAD_STATE_2__I_TTSYNC_DBG_BITS_IBUFFER_STALL_WORD {5, 1, 0, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_2__I_TTSYNC_DBG_BITS_IBUFFER_STALL {DEBUG_BUS_THREAD_STATE_2__I_TTSYNC_DBG_BITS_IBUFFER_STALL_WORD, 7, 0x1}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[2] alias: debug_bus_thread_state note: THREAD2
//  name: i_ttsync_dbg_bits
//  name: risc_cfg_stall
#define DEBUG_BUS_THREAD_STATE_2__I_TTSYNC_DBG_BITS_RISC_CFG_STALL_WORD {5, 1, 0, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_2__I_TTSYNC_DBG_BITS_RISC_CFG_STALL {DEBUG_BUS_THREAD_STATE_2__I_TTSYNC_DBG_BITS_RISC_CFG_STALL_WORD, 6, 0x1}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[2] alias: debug_bus_thread_state note: THREAD2
//  name: i_ttsync_dbg_bits
//  name: risc_gpr_stall
#define DEBUG_BUS_THREAD_STATE_2__I_TTSYNC_DBG_BITS_RISC_GPR_STALL_WORD {5, 1, 0, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_2__I_TTSYNC_DBG_BITS_RISC_GPR_STALL {DEBUG_BUS_THREAD_STATE_2__I_TTSYNC_DBG_BITS_RISC_GPR_STALL_WORD, 5, 0x1}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[2] alias: debug_bus_thread_state note: THREAD2
//  name: i_ttsync_dbg_bits
//  name: risc_tdma_stall
#define DEBUG_BUS_THREAD_STATE_2__I_TTSYNC_DBG_BITS_RISC_TDMA_STALL_WORD {5, 1, 0, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_2__I_TTSYNC_DBG_BITS_RISC_TDMA_STALL {DEBUG_BUS_THREAD_STATE_2__I_TTSYNC_DBG_BITS_RISC_TDMA_STALL_WORD, 4, 0x1}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[2] alias: debug_bus_thread_state note: THREAD2
//  name: i_ttsync_dbg_bits
//  name: prev_gen_no
#define DEBUG_BUS_THREAD_STATE_2__I_TTSYNC_DBG_BITS_PREV_GEN_NO__3_0_WORD {4, 1, 6, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_2__I_TTSYNC_DBG_BITS_PREV_GEN_NO__3_0 {DEBUG_BUS_THREAD_STATE_2__I_TTSYNC_DBG_BITS_PREV_GEN_NO__3_0_WORD, 28, 0xF}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[2] alias: debug_bus_thread_state note: THREAD2
//  name: i_ttsync_dbg_bits
//  name: prev_gen_no
#define DEBUG_BUS_THREAD_STATE_2__I_TTSYNC_DBG_BITS_PREV_GEN_NO__7_4_WORD {5, 1, 0, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_2__I_TTSYNC_DBG_BITS_PREV_GEN_NO__7_4 {DEBUG_BUS_THREAD_STATE_2__I_TTSYNC_DBG_BITS_PREV_GEN_NO__7_4_WORD, 0, 0xF}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[2] alias: debug_bus_thread_state note: THREAD2
//  name: i_ttsync_dbg_bits
//  name: lsq_head_valid
#define DEBUG_BUS_THREAD_STATE_2__I_TTSYNC_DBG_BITS_LSQ_HEAD_VALID_WORD {4, 1, 6, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_2__I_TTSYNC_DBG_BITS_LSQ_HEAD_VALID {DEBUG_BUS_THREAD_STATE_2__I_TTSYNC_DBG_BITS_LSQ_HEAD_VALID_WORD, 27, 0x1}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[2] alias: debug_bus_thread_state note: THREAD2
//  name: i_ttsync_dbg_bits
//  name: lsq_head_rsrcs alias: ttsync_rsrc_t
//  name: wr_tdma
#define DEBUG_BUS_THREAD_STATE_2__I_TTSYNC_DBG_BITS_LSQ_HEAD_RSRCS_WR_TDMA_WORD {4, 1, 6, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_2__I_TTSYNC_DBG_BITS_LSQ_HEAD_RSRCS_WR_TDMA {DEBUG_BUS_THREAD_STATE_2__I_TTSYNC_DBG_BITS_LSQ_HEAD_RSRCS_WR_TDMA_WORD, 26, 0x1}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[2] alias: debug_bus_thread_state note: THREAD2
//  name: i_ttsync_dbg_bits
//  name: lsq_head_rsrcs alias: ttsync_rsrc_t
//  name: rd_tdma
#define DEBUG_BUS_THREAD_STATE_2__I_TTSYNC_DBG_BITS_LSQ_HEAD_RSRCS_RD_TDMA_WORD {4, 1, 6, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_2__I_TTSYNC_DBG_BITS_LSQ_HEAD_RSRCS_RD_TDMA {DEBUG_BUS_THREAD_STATE_2__I_TTSYNC_DBG_BITS_LSQ_HEAD_RSRCS_RD_TDMA_WORD, 25, 0x1}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[2] alias: debug_bus_thread_state note: THREAD2
//  name: i_ttsync_dbg_bits
//  name: lsq_head_rsrcs alias: ttsync_rsrc_t
//  name: wr_gpr
#define DEBUG_BUS_THREAD_STATE_2__I_TTSYNC_DBG_BITS_LSQ_HEAD_RSRCS_WR_GPR_WORD {4, 1, 6, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_2__I_TTSYNC_DBG_BITS_LSQ_HEAD_RSRCS_WR_GPR {DEBUG_BUS_THREAD_STATE_2__I_TTSYNC_DBG_BITS_LSQ_HEAD_RSRCS_WR_GPR_WORD, 24, 0x1}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[2] alias: debug_bus_thread_state note: THREAD2
//  name: i_ttsync_dbg_bits
//  name: lsq_head_rsrcs alias: ttsync_rsrc_t
//  name: rd_gpr
#define DEBUG_BUS_THREAD_STATE_2__I_TTSYNC_DBG_BITS_LSQ_HEAD_RSRCS_RD_GPR_WORD {4, 1, 6, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_2__I_TTSYNC_DBG_BITS_LSQ_HEAD_RSRCS_RD_GPR {DEBUG_BUS_THREAD_STATE_2__I_TTSYNC_DBG_BITS_LSQ_HEAD_RSRCS_RD_GPR_WORD, 23, 0x1}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[2] alias: debug_bus_thread_state note: THREAD2
//  name: i_ttsync_dbg_bits
//  name: lsq_head_rsrcs alias: ttsync_rsrc_t
//  name: target_cfg_space
#define DEBUG_BUS_THREAD_STATE_2__I_TTSYNC_DBG_BITS_LSQ_HEAD_RSRCS_TARGET_CFG_SPACE_WORD {4, 1, 6, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_2__I_TTSYNC_DBG_BITS_LSQ_HEAD_RSRCS_TARGET_CFG_SPACE {DEBUG_BUS_THREAD_STATE_2__I_TTSYNC_DBG_BITS_LSQ_HEAD_RSRCS_TARGET_CFG_SPACE_WORD, 20, 0x7}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[2] alias: debug_bus_thread_state note: THREAD2
//  name: i_ttsync_dbg_bits
//  name: lsq_head_rsrcs alias: ttsync_rsrc_t
//  name: cfg_state
#define DEBUG_BUS_THREAD_STATE_2__I_TTSYNC_DBG_BITS_LSQ_HEAD_RSRCS_CFG_STATE_WORD {4, 1, 6, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_2__I_TTSYNC_DBG_BITS_LSQ_HEAD_RSRCS_CFG_STATE {DEBUG_BUS_THREAD_STATE_2__I_TTSYNC_DBG_BITS_LSQ_HEAD_RSRCS_CFG_STATE_WORD, 18, 0x3}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[2] alias: debug_bus_thread_state note: THREAD2
//  name: i_ttsync_dbg_bits
//  name: lsq_head_rsrcs alias: ttsync_rsrc_t
//  name: wr_cfg
#define DEBUG_BUS_THREAD_STATE_2__I_TTSYNC_DBG_BITS_LSQ_HEAD_RSRCS_WR_CFG_WORD {4, 1, 6, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_2__I_TTSYNC_DBG_BITS_LSQ_HEAD_RSRCS_WR_CFG {DEBUG_BUS_THREAD_STATE_2__I_TTSYNC_DBG_BITS_LSQ_HEAD_RSRCS_WR_CFG_WORD, 17, 0x1}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[2] alias: debug_bus_thread_state note: THREAD2
//  name: i_ttsync_dbg_bits
//  name: lsq_head_rsrcs alias: ttsync_rsrc_t
//  name: rd_cfg
#define DEBUG_BUS_THREAD_STATE_2__I_TTSYNC_DBG_BITS_LSQ_HEAD_RSRCS_RD_CFG_WORD {4, 1, 6, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_2__I_TTSYNC_DBG_BITS_LSQ_HEAD_RSRCS_RD_CFG {DEBUG_BUS_THREAD_STATE_2__I_TTSYNC_DBG_BITS_LSQ_HEAD_RSRCS_RD_CFG_WORD, 16, 0x1}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[2] alias: debug_bus_thread_state note: THREAD2
//  name: i_ttsync_dbg_bits
//  name: lsq_head_gen_no
#define DEBUG_BUS_THREAD_STATE_2__I_TTSYNC_DBG_BITS_LSQ_HEAD_GEN_NO_WORD {4, 1, 6, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_2__I_TTSYNC_DBG_BITS_LSQ_HEAD_GEN_NO {DEBUG_BUS_THREAD_STATE_2__I_TTSYNC_DBG_BITS_LSQ_HEAD_GEN_NO_WORD, 8, 0xFF}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[2] alias: debug_bus_thread_state note: THREAD2
//  name: i_ttsync_dbg_bits
//  name: lsq_full
#define DEBUG_BUS_THREAD_STATE_2__I_TTSYNC_DBG_BITS_LSQ_FULL_WORD {4, 1, 6, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_2__I_TTSYNC_DBG_BITS_LSQ_FULL {DEBUG_BUS_THREAD_STATE_2__I_TTSYNC_DBG_BITS_LSQ_FULL_WORD, 7, 0x1}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[2] alias: debug_bus_thread_state note: THREAD2
//  name: i_ttsync_dbg_bits
//  name: rq_head_valid
#define DEBUG_BUS_THREAD_STATE_2__I_TTSYNC_DBG_BITS_RQ_HEAD_VALID_WORD {4, 1, 6, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_2__I_TTSYNC_DBG_BITS_RQ_HEAD_VALID {DEBUG_BUS_THREAD_STATE_2__I_TTSYNC_DBG_BITS_RQ_HEAD_VALID_WORD, 6, 0x1}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[2] alias: debug_bus_thread_state note: THREAD2
//  name: i_ttsync_dbg_bits
//  name: rq_head_rsrcs alias: ttsync_rsrc_t
//  name: wr_tdma
#define DEBUG_BUS_THREAD_STATE_2__I_TTSYNC_DBG_BITS_RQ_HEAD_RSRCS_WR_TDMA_WORD {4, 1, 6, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_2__I_TTSYNC_DBG_BITS_RQ_HEAD_RSRCS_WR_TDMA {DEBUG_BUS_THREAD_STATE_2__I_TTSYNC_DBG_BITS_RQ_HEAD_RSRCS_WR_TDMA_WORD, 5, 0x1}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[2] alias: debug_bus_thread_state note: THREAD2
//  name: i_ttsync_dbg_bits
//  name: rq_head_rsrcs alias: ttsync_rsrc_t
//  name: rd_tdma
#define DEBUG_BUS_THREAD_STATE_2__I_TTSYNC_DBG_BITS_RQ_HEAD_RSRCS_RD_TDMA_WORD {4, 1, 6, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_2__I_TTSYNC_DBG_BITS_RQ_HEAD_RSRCS_RD_TDMA {DEBUG_BUS_THREAD_STATE_2__I_TTSYNC_DBG_BITS_RQ_HEAD_RSRCS_RD_TDMA_WORD, 4, 0x1}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[2] alias: debug_bus_thread_state note: THREAD2
//  name: i_ttsync_dbg_bits
//  name: rq_head_rsrcs alias: ttsync_rsrc_t
//  name: wr_gpr
#define DEBUG_BUS_THREAD_STATE_2__I_TTSYNC_DBG_BITS_RQ_HEAD_RSRCS_WR_GPR_WORD {4, 1, 6, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_2__I_TTSYNC_DBG_BITS_RQ_HEAD_RSRCS_WR_GPR {DEBUG_BUS_THREAD_STATE_2__I_TTSYNC_DBG_BITS_RQ_HEAD_RSRCS_WR_GPR_WORD, 3, 0x1}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[2] alias: debug_bus_thread_state note: THREAD2
//  name: i_ttsync_dbg_bits
//  name: rq_head_rsrcs alias: ttsync_rsrc_t
//  name: rd_gpr
#define DEBUG_BUS_THREAD_STATE_2__I_TTSYNC_DBG_BITS_RQ_HEAD_RSRCS_RD_GPR_WORD {4, 1, 6, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_2__I_TTSYNC_DBG_BITS_RQ_HEAD_RSRCS_RD_GPR {DEBUG_BUS_THREAD_STATE_2__I_TTSYNC_DBG_BITS_RQ_HEAD_RSRCS_RD_GPR_WORD, 2, 0x1}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[2] alias: debug_bus_thread_state note: THREAD2
//  name: i_ttsync_dbg_bits
//  name: rq_head_rsrcs alias: ttsync_rsrc_t
//  name: target_cfg_space
#define DEBUG_BUS_THREAD_STATE_2__I_TTSYNC_DBG_BITS_RQ_HEAD_RSRCS_TARGET_CFG_SPACE__0_0_WORD {4, 1, 4, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_2__I_TTSYNC_DBG_BITS_RQ_HEAD_RSRCS_TARGET_CFG_SPACE__0_0 {DEBUG_BUS_THREAD_STATE_2__I_TTSYNC_DBG_BITS_RQ_HEAD_RSRCS_TARGET_CFG_SPACE__0_0_WORD, 31, 0x1}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[2] alias: debug_bus_thread_state note: THREAD2
//  name: i_ttsync_dbg_bits
//  name: rq_head_rsrcs alias: ttsync_rsrc_t
//  name: target_cfg_space
#define DEBUG_BUS_THREAD_STATE_2__I_TTSYNC_DBG_BITS_RQ_HEAD_RSRCS_TARGET_CFG_SPACE__2_1_WORD {4, 1, 6, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_2__I_TTSYNC_DBG_BITS_RQ_HEAD_RSRCS_TARGET_CFG_SPACE__2_1 {DEBUG_BUS_THREAD_STATE_2__I_TTSYNC_DBG_BITS_RQ_HEAD_RSRCS_TARGET_CFG_SPACE__2_1_WORD, 0, 0x3}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[2] alias: debug_bus_thread_state note: THREAD2
//  name: i_ttsync_dbg_bits
//  name: rq_head_rsrcs alias: ttsync_rsrc_t
//  name: cfg_state
#define DEBUG_BUS_THREAD_STATE_2__I_TTSYNC_DBG_BITS_RQ_HEAD_RSRCS_CFG_STATE_WORD {4, 1, 4, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_2__I_TTSYNC_DBG_BITS_RQ_HEAD_RSRCS_CFG_STATE {DEBUG_BUS_THREAD_STATE_2__I_TTSYNC_DBG_BITS_RQ_HEAD_RSRCS_CFG_STATE_WORD, 29, 0x3}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[2] alias: debug_bus_thread_state note: THREAD2
//  name: i_ttsync_dbg_bits
//  name: rq_head_rsrcs alias: ttsync_rsrc_t
//  name: wr_cfg
#define DEBUG_BUS_THREAD_STATE_2__I_TTSYNC_DBG_BITS_RQ_HEAD_RSRCS_WR_CFG_WORD {4, 1, 4, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_2__I_TTSYNC_DBG_BITS_RQ_HEAD_RSRCS_WR_CFG {DEBUG_BUS_THREAD_STATE_2__I_TTSYNC_DBG_BITS_RQ_HEAD_RSRCS_WR_CFG_WORD, 28, 0x1}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[2] alias: debug_bus_thread_state note: THREAD2
//  name: i_ttsync_dbg_bits
//  name: rq_head_rsrcs alias: ttsync_rsrc_t
//  name: rd_cfg
#define DEBUG_BUS_THREAD_STATE_2__I_TTSYNC_DBG_BITS_RQ_HEAD_RSRCS_RD_CFG_WORD {4, 1, 4, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_2__I_TTSYNC_DBG_BITS_RQ_HEAD_RSRCS_RD_CFG {DEBUG_BUS_THREAD_STATE_2__I_TTSYNC_DBG_BITS_RQ_HEAD_RSRCS_RD_CFG_WORD, 27, 0x1}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[2] alias: debug_bus_thread_state note: THREAD2
//  name: i_ttsync_dbg_bits
//  name: rq_head_gen_no
#define DEBUG_BUS_THREAD_STATE_2__I_TTSYNC_DBG_BITS_RQ_HEAD_GEN_NO_WORD {4, 1, 4, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_2__I_TTSYNC_DBG_BITS_RQ_HEAD_GEN_NO {DEBUG_BUS_THREAD_STATE_2__I_TTSYNC_DBG_BITS_RQ_HEAD_GEN_NO_WORD, 19, 0xFF}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[2] alias: debug_bus_thread_state note: THREAD2
//  name: i_ttsync_dbg_bits
//  name: rq_full
#define DEBUG_BUS_THREAD_STATE_2__I_TTSYNC_DBG_BITS_RQ_FULL_WORD {4, 1, 4, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_2__I_TTSYNC_DBG_BITS_RQ_FULL {DEBUG_BUS_THREAD_STATE_2__I_TTSYNC_DBG_BITS_RQ_FULL_WORD, 18, 0x1}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[2] alias: debug_bus_thread_state note: THREAD2
//  name: i_cg_trisc_busy
#define DEBUG_BUS_THREAD_STATE_2__I_CG_TRISC_BUSY_WORD {4, 1, 4, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_2__I_CG_TRISC_BUSY {DEBUG_BUS_THREAD_STATE_2__I_CG_TRISC_BUSY_WORD, 17, 0x1}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[2] alias: debug_bus_thread_state note: THREAD2
//  name: machine_busy
#define DEBUG_BUS_THREAD_STATE_2__MACHINE_BUSY_WORD {4, 1, 4, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_2__MACHINE_BUSY {DEBUG_BUS_THREAD_STATE_2__MACHINE_BUSY_WORD, 16, 0x1}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[2] alias: debug_bus_thread_state note: THREAD2
//  name: req_iramd_buffer_not_empty
#define DEBUG_BUS_THREAD_STATE_2__REQ_IRAMD_BUFFER_NOT_EMPTY_WORD {4, 1, 4, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_2__REQ_IRAMD_BUFFER_NOT_EMPTY {DEBUG_BUS_THREAD_STATE_2__REQ_IRAMD_BUFFER_NOT_EMPTY_WORD, 15, 0x1}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[2] alias: debug_bus_thread_state note: THREAD2
//  name: gpr_file_busy
#define DEBUG_BUS_THREAD_STATE_2__GPR_FILE_BUSY_WORD {4, 1, 4, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_2__GPR_FILE_BUSY {DEBUG_BUS_THREAD_STATE_2__GPR_FILE_BUSY_WORD, 14, 0x1}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[2] alias: debug_bus_thread_state note: THREAD2
//  name: cfg_exu_busy
#define DEBUG_BUS_THREAD_STATE_2__CFG_EXU_BUSY_WORD {4, 1, 4, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_2__CFG_EXU_BUSY {DEBUG_BUS_THREAD_STATE_2__CFG_EXU_BUSY_WORD, 13, 0x1}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[2] alias: debug_bus_thread_state note: THREAD2
//  name: req_iramd_buffer_empty
#define DEBUG_BUS_THREAD_STATE_2__REQ_IRAMD_BUFFER_EMPTY_WORD {4, 1, 4, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_2__REQ_IRAMD_BUFFER_EMPTY {DEBUG_BUS_THREAD_STATE_2__REQ_IRAMD_BUFFER_EMPTY_WORD, 12, 0x1}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[2] alias: debug_bus_thread_state note: THREAD2
//  name: req_iramd_buffer_full
#define DEBUG_BUS_THREAD_STATE_2__REQ_IRAMD_BUFFER_FULL_WORD {4, 1, 4, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_2__REQ_IRAMD_BUFFER_FULL {DEBUG_BUS_THREAD_STATE_2__REQ_IRAMD_BUFFER_FULL_WORD, 11, 0x1}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[2] alias: debug_bus_thread_state note: THREAD2
//  name: ~ibuffer_rtr
#define DEBUG_BUS_THREAD_STATE_2___IBUFFER_RTR_WORD {4, 1, 4, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_2___IBUFFER_RTR {DEBUG_BUS_THREAD_STATE_2___IBUFFER_RTR_WORD, 10, 0x1}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[2] alias: debug_bus_thread_state note: THREAD2
//  name: ibuffer_empty
#define DEBUG_BUS_THREAD_STATE_2__IBUFFER_EMPTY_WORD {4, 1, 4, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_2__IBUFFER_EMPTY {DEBUG_BUS_THREAD_STATE_2__IBUFFER_EMPTY_WORD, 9, 0x1}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[2] alias: debug_bus_thread_state note: THREAD2
//  name: ibuffer_empty_raw
#define DEBUG_BUS_THREAD_STATE_2__IBUFFER_EMPTY_RAW_WORD {4, 1, 4, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_2__IBUFFER_EMPTY_RAW {DEBUG_BUS_THREAD_STATE_2__IBUFFER_EMPTY_RAW_WORD, 8, 0x1}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[2] alias: debug_bus_thread_state note: THREAD2
//  name: thread_inst
#define DEBUG_BUS_THREAD_STATE_2__THREAD_INST__23_0_WORD {4, 1, 2, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_2__THREAD_INST__23_0 {DEBUG_BUS_THREAD_STATE_2__THREAD_INST__23_0_WORD, 8, 0xFFFFFF}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[2] alias: debug_bus_thread_state note: THREAD2
//  name: thread_inst
#define DEBUG_BUS_THREAD_STATE_2__THREAD_INST__31_24_WORD {4, 1, 4, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_2__THREAD_INST__31_24 {DEBUG_BUS_THREAD_STATE_2__THREAD_INST__31_24_WORD, 0, 0xFF}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[2] alias: debug_bus_thread_state note: THREAD2
//  name: math_inst
#define DEBUG_BUS_THREAD_STATE_2__MATH_INST_WORD {4, 1, 2, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_2__MATH_INST {DEBUG_BUS_THREAD_STATE_2__MATH_INST_WORD, 7, 0x1}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[2] alias: debug_bus_thread_state note: THREAD2
//  name: tdma_inst
#define DEBUG_BUS_THREAD_STATE_2__TDMA_INST_WORD {4, 1, 2, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_2__TDMA_INST {DEBUG_BUS_THREAD_STATE_2__TDMA_INST_WORD, 6, 0x1}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[2] alias: debug_bus_thread_state note: THREAD2
//  name: pack_inst
#define DEBUG_BUS_THREAD_STATE_2__PACK_INST_WORD {4, 1, 2, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_2__PACK_INST {DEBUG_BUS_THREAD_STATE_2__PACK_INST_WORD, 5, 0x1}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[2] alias: debug_bus_thread_state note: THREAD2
//  name: move_inst
#define DEBUG_BUS_THREAD_STATE_2__MOVE_INST_WORD {4, 1, 2, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_2__MOVE_INST {DEBUG_BUS_THREAD_STATE_2__MOVE_INST_WORD, 4, 0x1}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[2] alias: debug_bus_thread_state note: THREAD2
//  name: sfpu_inst
#define DEBUG_BUS_THREAD_STATE_2__SFPU_INST_WORD {4, 1, 2, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_2__SFPU_INST {DEBUG_BUS_THREAD_STATE_2__SFPU_INST_WORD, 3, 0x1}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[2] alias: debug_bus_thread_state note: THREAD2
//  name: unpack_inst
#define DEBUG_BUS_THREAD_STATE_2__UNPACK_INST_WORD {4, 1, 2, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_2__UNPACK_INST {DEBUG_BUS_THREAD_STATE_2__UNPACK_INST_WORD, 1, 0x3}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[2] alias: debug_bus_thread_state note: THREAD2
//  name: xsearch_inst
#define DEBUG_BUS_THREAD_STATE_2__XSEARCH_INST_WORD {4, 1, 2, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_2__XSEARCH_INST {DEBUG_BUS_THREAD_STATE_2__XSEARCH_INST_WORD, 0, 0x1}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[2] alias: debug_bus_thread_state note: THREAD2
//  name: thcon_inst
#define DEBUG_BUS_THREAD_STATE_2__THCON_INST_WORD {4, 1, 0, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_2__THCON_INST {DEBUG_BUS_THREAD_STATE_2__THCON_INST_WORD, 31, 0x1}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[2] alias: debug_bus_thread_state note: THREAD2
//  name: sync_inst
#define DEBUG_BUS_THREAD_STATE_2__SYNC_INST_WORD {4, 1, 0, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_2__SYNC_INST {DEBUG_BUS_THREAD_STATE_2__SYNC_INST_WORD, 30, 0x1}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[2] alias: debug_bus_thread_state note: THREAD2
//  name: cfg_inst
#define DEBUG_BUS_THREAD_STATE_2__CFG_INST_WORD {4, 1, 0, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_2__CFG_INST {DEBUG_BUS_THREAD_STATE_2__CFG_INST_WORD, 29, 0x1}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[2] alias: debug_bus_thread_state note: THREAD2
//  name: stalled_pack_inst
#define DEBUG_BUS_THREAD_STATE_2__STALLED_PACK_INST_WORD {4, 1, 0, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_2__STALLED_PACK_INST {DEBUG_BUS_THREAD_STATE_2__STALLED_PACK_INST_WORD, 28, 0x1}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[2] alias: debug_bus_thread_state note: THREAD2
//  name: stalled_unpack_inst
#define DEBUG_BUS_THREAD_STATE_2__STALLED_UNPACK_INST_WORD {4, 1, 0, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_2__STALLED_UNPACK_INST {DEBUG_BUS_THREAD_STATE_2__STALLED_UNPACK_INST_WORD, 26, 0x3}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[2] alias: debug_bus_thread_state note: THREAD2
//  name: stalled_math_inst
#define DEBUG_BUS_THREAD_STATE_2__STALLED_MATH_INST_WORD {4, 1, 0, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_2__STALLED_MATH_INST {DEBUG_BUS_THREAD_STATE_2__STALLED_MATH_INST_WORD, 25, 0x1}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[2] alias: debug_bus_thread_state note: THREAD2
//  name: stalled_tdma_inst
#define DEBUG_BUS_THREAD_STATE_2__STALLED_TDMA_INST_WORD {4, 1, 0, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_2__STALLED_TDMA_INST {DEBUG_BUS_THREAD_STATE_2__STALLED_TDMA_INST_WORD, 24, 0x1}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[2] alias: debug_bus_thread_state note: THREAD2
//  name: stalled_move_inst
#define DEBUG_BUS_THREAD_STATE_2__STALLED_MOVE_INST_WORD {4, 1, 0, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_2__STALLED_MOVE_INST {DEBUG_BUS_THREAD_STATE_2__STALLED_MOVE_INST_WORD, 23, 0x1}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[2] alias: debug_bus_thread_state note: THREAD2
//  name: stalled_xsearch_inst
#define DEBUG_BUS_THREAD_STATE_2__STALLED_XSEARCH_INST_WORD {4, 1, 0, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_2__STALLED_XSEARCH_INST {DEBUG_BUS_THREAD_STATE_2__STALLED_XSEARCH_INST_WORD, 22, 0x1}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[2] alias: debug_bus_thread_state note: THREAD2
//  name: stalled_thcon_inst
#define DEBUG_BUS_THREAD_STATE_2__STALLED_THCON_INST_WORD {4, 1, 0, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_2__STALLED_THCON_INST {DEBUG_BUS_THREAD_STATE_2__STALLED_THCON_INST_WORD, 21, 0x1}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[2] alias: debug_bus_thread_state note: THREAD2
//  name: stalled_sync_inst
#define DEBUG_BUS_THREAD_STATE_2__STALLED_SYNC_INST_WORD {4, 1, 0, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_2__STALLED_SYNC_INST {DEBUG_BUS_THREAD_STATE_2__STALLED_SYNC_INST_WORD, 20, 0x1}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[2] alias: debug_bus_thread_state note: THREAD2
//  name: stalled_cfg_inst
#define DEBUG_BUS_THREAD_STATE_2__STALLED_CFG_INST_WORD {4, 1, 0, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_2__STALLED_CFG_INST {DEBUG_BUS_THREAD_STATE_2__STALLED_CFG_INST_WORD, 19, 0x1}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[2] alias: debug_bus_thread_state note: THREAD2
//  name: stalled_sfpu_inst
#define DEBUG_BUS_THREAD_STATE_2__STALLED_SFPU_INST_WORD {4, 1, 0, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_2__STALLED_SFPU_INST {DEBUG_BUS_THREAD_STATE_2__STALLED_SFPU_INST_WORD, 18, 0x1}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[2] alias: debug_bus_thread_state note: THREAD2
//  name: tdma_kick_move
#define DEBUG_BUS_THREAD_STATE_2__TDMA_KICK_MOVE_WORD {4, 1, 0, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_2__TDMA_KICK_MOVE {DEBUG_BUS_THREAD_STATE_2__TDMA_KICK_MOVE_WORD, 17, 0x1}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[2] alias: debug_bus_thread_state note: THREAD2
//  name: tdma_kick_pack
#define DEBUG_BUS_THREAD_STATE_2__TDMA_KICK_PACK_WORD {4, 1, 0, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_2__TDMA_KICK_PACK {DEBUG_BUS_THREAD_STATE_2__TDMA_KICK_PACK_WORD, 16, 0x1}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[2] alias: debug_bus_thread_state note: THREAD2
//  name: tdma_kick_unpack
#define DEBUG_BUS_THREAD_STATE_2__TDMA_KICK_UNPACK_WORD {4, 1, 0, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_2__TDMA_KICK_UNPACK {DEBUG_BUS_THREAD_STATE_2__TDMA_KICK_UNPACK_WORD, 14, 0x3}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[2] alias: debug_bus_thread_state note: THREAD2
//  name: tdma_kick_xsearch
#define DEBUG_BUS_THREAD_STATE_2__TDMA_KICK_XSEARCH_WORD {4, 1, 0, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_2__TDMA_KICK_XSEARCH {DEBUG_BUS_THREAD_STATE_2__TDMA_KICK_XSEARCH_WORD, 13, 0x1}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[2] alias: debug_bus_thread_state note: THREAD2
//  name: tdma_kick_thcon
#define DEBUG_BUS_THREAD_STATE_2__TDMA_KICK_THCON_WORD {4, 1, 0, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_2__TDMA_KICK_THCON {DEBUG_BUS_THREAD_STATE_2__TDMA_KICK_THCON_WORD, 12, 0x1}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[2] alias: debug_bus_thread_state note: THREAD2
//  name: tdma_status_busy
#define DEBUG_BUS_THREAD_STATE_2__TDMA_STATUS_BUSY_WORD {4, 1, 0, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_2__TDMA_STATUS_BUSY {DEBUG_BUS_THREAD_STATE_2__TDMA_STATUS_BUSY_WORD, 7, 0x1F}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[2] alias: debug_bus_thread_state note: THREAD2
//  name: packer_busy
#define DEBUG_BUS_THREAD_STATE_2__PACKER_BUSY_WORD {4, 1, 0, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_2__PACKER_BUSY {DEBUG_BUS_THREAD_STATE_2__PACKER_BUSY_WORD, 6, 0x1}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[2] alias: debug_bus_thread_state note: THREAD2
//  name: unpacker_busy
#define DEBUG_BUS_THREAD_STATE_2__UNPACKER_BUSY_WORD {4, 1, 0, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_2__UNPACKER_BUSY {DEBUG_BUS_THREAD_STATE_2__UNPACKER_BUSY_WORD, 4, 0x3}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[2] alias: debug_bus_thread_state note: THREAD2
//  name: thcon_busy
#define DEBUG_BUS_THREAD_STATE_2__THCON_BUSY_WORD {4, 1, 0, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_2__THCON_BUSY {DEBUG_BUS_THREAD_STATE_2__THCON_BUSY_WORD, 3, 0x1}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[2] alias: debug_bus_thread_state note: THREAD2
//  name: move_busy
#define DEBUG_BUS_THREAD_STATE_2__MOVE_BUSY_WORD {4, 1, 0, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_2__MOVE_BUSY {DEBUG_BUS_THREAD_STATE_2__MOVE_BUSY_WORD, 2, 0x1}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_thread_state[2] alias: debug_bus_thread_state note: THREAD2
//  name: xsearch_busy
#define DEBUG_BUS_THREAD_STATE_2__XSEARCH_BUSY_WORD {4, 1, 0, 0, 1, 0}
#define DEBUG_BUS_THREAD_STATE_2__XSEARCH_BUSY {DEBUG_BUS_THREAD_STATE_2__XSEARCH_BUSY_WORD, 0, 0x3}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_stall_inst_cnt[2] alias: perf_cnt_instrn_thread_inst_cnts note: THREAD2
//  name: perf_cnt_instrn_thread_inst_pack alias: perf_cnt
//  name: grant_cnt
#define DEBUG_BUS_STALL_INST_CNT_2__PERF_CNT_INSTRN_THREAD_INST_PACK_GRANT_CNT_WORD {3, 1, 6, 0, 1, 0}
#define DEBUG_BUS_STALL_INST_CNT_2__PERF_CNT_INSTRN_THREAD_INST_PACK_GRANT_CNT {DEBUG_BUS_STALL_INST_CNT_2__PERF_CNT_INSTRN_THREAD_INST_PACK_GRANT_CNT_WORD, 0, 0xFFFFFFFF}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_stall_inst_cnt[2] alias: perf_cnt_instrn_thread_inst_cnts note: THREAD2
//  name: perf_cnt_instrn_thread_inst_unpack alias: perf_cnt
//  name: grant_cnt
#define DEBUG_BUS_STALL_INST_CNT_2__PERF_CNT_INSTRN_THREAD_INST_UNPACK_GRANT_CNT_WORD {3, 1, 4, 0, 1, 0}
#define DEBUG_BUS_STALL_INST_CNT_2__PERF_CNT_INSTRN_THREAD_INST_UNPACK_GRANT_CNT {DEBUG_BUS_STALL_INST_CNT_2__PERF_CNT_INSTRN_THREAD_INST_UNPACK_GRANT_CNT_WORD, 0, 0xFFFFFFFF}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_stall_inst_cnt[2] alias: perf_cnt_instrn_thread_inst_cnts note: THREAD2
//  name: perf_cnt_instrn_thread_inst_math alias: perf_cnt
//  name: grant_cnt
#define DEBUG_BUS_STALL_INST_CNT_2__PERF_CNT_INSTRN_THREAD_INST_MATH_GRANT_CNT_WORD {3, 1, 2, 0, 1, 0}
#define DEBUG_BUS_STALL_INST_CNT_2__PERF_CNT_INSTRN_THREAD_INST_MATH_GRANT_CNT {DEBUG_BUS_STALL_INST_CNT_2__PERF_CNT_INSTRN_THREAD_INST_MATH_GRANT_CNT_WORD, 0, 0xFFFFFFFF}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_stall_inst_cnt[2] alias: perf_cnt_instrn_thread_inst_cnts note: THREAD2
//  name: perf_cnt_instrn_thread_inst_move alias: perf_cnt
//  name: grant_cnt
#define DEBUG_BUS_STALL_INST_CNT_2__PERF_CNT_INSTRN_THREAD_INST_MOVE_GRANT_CNT_WORD {3, 1, 0, 0, 1, 0}
#define DEBUG_BUS_STALL_INST_CNT_2__PERF_CNT_INSTRN_THREAD_INST_MOVE_GRANT_CNT {DEBUG_BUS_STALL_INST_CNT_2__PERF_CNT_INSTRN_THREAD_INST_MOVE_GRANT_CNT_WORD, 0, 0xFFFFFFFF}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_stall_inst_cnt[2] alias: perf_cnt_instrn_thread_inst_cnts note: THREAD2
//  name: perf_cnt_instrn_thread_inst_xsearch alias: perf_cnt
//  name: grant_cnt
#define DEBUG_BUS_STALL_INST_CNT_2__PERF_CNT_INSTRN_THREAD_INST_XSEARCH_GRANT_CNT_WORD {2, 1, 6, 0, 1, 0}
#define DEBUG_BUS_STALL_INST_CNT_2__PERF_CNT_INSTRN_THREAD_INST_XSEARCH_GRANT_CNT {DEBUG_BUS_STALL_INST_CNT_2__PERF_CNT_INSTRN_THREAD_INST_XSEARCH_GRANT_CNT_WORD, 0, 0xFFFFFFFF}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_stall_inst_cnt[2] alias: perf_cnt_instrn_thread_inst_cnts note: THREAD2
//  name: perf_cnt_instrn_thread_inst_thcon alias: perf_cnt
//  name: grant_cnt
#define DEBUG_BUS_STALL_INST_CNT_2__PERF_CNT_INSTRN_THREAD_INST_THCON_GRANT_CNT_WORD {2, 1, 4, 0, 1, 0}
#define DEBUG_BUS_STALL_INST_CNT_2__PERF_CNT_INSTRN_THREAD_INST_THCON_GRANT_CNT {DEBUG_BUS_STALL_INST_CNT_2__PERF_CNT_INSTRN_THREAD_INST_THCON_GRANT_CNT_WORD, 0, 0xFFFFFFFF}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_stall_inst_cnt[2] alias: perf_cnt_instrn_thread_inst_cnts note: THREAD2
//  name: perf_cnt_instrn_thread_inst_sync alias: perf_cnt
//  name: grant_cnt
#define DEBUG_BUS_STALL_INST_CNT_2__PERF_CNT_INSTRN_THREAD_INST_SYNC_GRANT_CNT_WORD {2, 1, 2, 0, 1, 0}
#define DEBUG_BUS_STALL_INST_CNT_2__PERF_CNT_INSTRN_THREAD_INST_SYNC_GRANT_CNT {DEBUG_BUS_STALL_INST_CNT_2__PERF_CNT_INSTRN_THREAD_INST_SYNC_GRANT_CNT_WORD, 0, 0xFFFFFFFF}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: debug_bus_stall_inst_cnt[2] alias: perf_cnt_instrn_thread_inst_cnts note: THREAD2
//  name: perf_cnt_instrn_thread_inst_cfg alias: perf_cnt
//  name: grant_cnt
#define DEBUG_BUS_STALL_INST_CNT_2__PERF_CNT_INSTRN_THREAD_INST_CFG_GRANT_CNT_WORD {2, 1, 0, 0, 1, 0}
#define DEBUG_BUS_STALL_INST_CNT_2__PERF_CNT_INSTRN_THREAD_INST_CFG_GRANT_CNT {DEBUG_BUS_STALL_INST_CNT_2__PERF_CNT_INSTRN_THREAD_INST_CFG_GRANT_CNT_WORD, 0, 0xFFFFFFFF}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: perf_cnt_instrn_thread_stall_cnts[0] alias: perf_cnt note: THREAD0
//  name: req_cnt
#define DEBUG_BUS_DEBUG_DAISY_STOP_THREADS_WHEN_I_DBG_INSTRN_THREAD_PERF_CNT_MUX_SEL_0___0__PERF_CNT_INSTRN_THREAD_STALL_CNTS_0__REQ_CNT_WORD {1, 1, 6, 0, 1, 0}
#define DEBUG_BUS_DEBUG_DAISY_STOP_THREADS_WHEN_I_DBG_INSTRN_THREAD_PERF_CNT_MUX_SEL_0___0__PERF_CNT_INSTRN_THREAD_STALL_CNTS_0__REQ_CNT {DEBUG_BUS_DEBUG_DAISY_STOP_THREADS_WHEN_I_DBG_INSTRN_THREAD_PERF_CNT_MUX_SEL_0___0__PERF_CNT_INSTRN_THREAD_STALL_CNTS_0__REQ_CNT_WORD, 0, 0xFFFFFFFF}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: perf_cnt_instrn_thread_stall_cnts[1] alias: perf_cnt note: THREAD1
//  name: req_cnt
#define DEBUG_BUS_DEBUG_DAISY_STOP_THREADS_WHEN_I_DBG_INSTRN_THREAD_PERF_CNT_MUX_SEL_0___0__PERF_CNT_INSTRN_THREAD_STALL_CNTS_1__REQ_CNT_WORD {1, 1, 4, 0, 1, 0}
#define DEBUG_BUS_DEBUG_DAISY_STOP_THREADS_WHEN_I_DBG_INSTRN_THREAD_PERF_CNT_MUX_SEL_0___0__PERF_CNT_INSTRN_THREAD_STALL_CNTS_1__REQ_CNT {DEBUG_BUS_DEBUG_DAISY_STOP_THREADS_WHEN_I_DBG_INSTRN_THREAD_PERF_CNT_MUX_SEL_0___0__PERF_CNT_INSTRN_THREAD_STALL_CNTS_1__REQ_CNT_WORD, 0, 0xFFFFFFFF}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]==0)
//  name: perf_cnt_instrn_thread_stall_cnts[2] alias: perf_cnt note: THREAD2
//  name: req_cnt
#define DEBUG_BUS_DEBUG_DAISY_STOP_THREADS_WHEN_I_DBG_INSTRN_THREAD_PERF_CNT_MUX_SEL_0___0__PERF_CNT_INSTRN_THREAD_STALL_CNTS_2__REQ_CNT_WORD {1, 1, 2, 0, 1, 0}
#define DEBUG_BUS_DEBUG_DAISY_STOP_THREADS_WHEN_I_DBG_INSTRN_THREAD_PERF_CNT_MUX_SEL_0___0__PERF_CNT_INSTRN_THREAD_STALL_CNTS_2__REQ_CNT {DEBUG_BUS_DEBUG_DAISY_STOP_THREADS_WHEN_I_DBG_INSTRN_THREAD_PERF_CNT_MUX_SEL_0___0__PERF_CNT_INSTRN_THREAD_STALL_CNTS_2__REQ_CNT_WORD, 0, 0xFFFFFFFF}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]!=0)
//  name: debug_bus_stall_rsn_cnt0[0] alias: perf_cnt_instrn_thread_stall_rsn_cnts note: THREAD0
//  name: perf_cnt_instrn_thread_stall_rsn_sem_zero alias: perf_cnt
//  name: grant_cnt
#define DEBUG_BUS_STALL_RSN_CNT0_0__PERF_CNT_INSTRN_THREAD_STALL_RSN_SEM_ZERO_GRANT_CNT_WORD {13, 1, 6, 0, 1, 0}
#define DEBUG_BUS_STALL_RSN_CNT0_0__PERF_CNT_INSTRN_THREAD_STALL_RSN_SEM_ZERO_GRANT_CNT {DEBUG_BUS_STALL_RSN_CNT0_0__PERF_CNT_INSTRN_THREAD_STALL_RSN_SEM_ZERO_GRANT_CNT_WORD, 0, 0xFFFFFFFF}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]!=0)
//  name: debug_bus_stall_rsn_cnt0[0] alias: perf_cnt_instrn_thread_stall_rsn_cnts note: THREAD0
//  name: perf_cnt_instrn_thread_stall_rsn_sem_max alias: perf_cnt
//  name: grant_cnt
#define DEBUG_BUS_STALL_RSN_CNT0_0__PERF_CNT_INSTRN_THREAD_STALL_RSN_SEM_MAX_GRANT_CNT_WORD {13, 1, 4, 0, 1, 0}
#define DEBUG_BUS_STALL_RSN_CNT0_0__PERF_CNT_INSTRN_THREAD_STALL_RSN_SEM_MAX_GRANT_CNT {DEBUG_BUS_STALL_RSN_CNT0_0__PERF_CNT_INSTRN_THREAD_STALL_RSN_SEM_MAX_GRANT_CNT_WORD, 0, 0xFFFFFFFF}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]!=0)
//  name: debug_bus_stall_rsn_cnt0[0] alias: perf_cnt_instrn_thread_stall_rsn_cnts note: THREAD0
//  name: perf_cnt_instrn_thread_stall_rsn_srca_cleared alias: perf_cnt note: Only on thread 0
//  name: grant_cnt
#define DEBUG_BUS_STALL_RSN_CNT0_0__PERF_CNT_INSTRN_THREAD_STALL_RSN_SRCA_CLEARED_GRANT_CNT_WORD {13, 1, 2, 0, 1, 0}
#define DEBUG_BUS_STALL_RSN_CNT0_0__PERF_CNT_INSTRN_THREAD_STALL_RSN_SRCA_CLEARED_GRANT_CNT {DEBUG_BUS_STALL_RSN_CNT0_0__PERF_CNT_INSTRN_THREAD_STALL_RSN_SRCA_CLEARED_GRANT_CNT_WORD, 0, 0xFFFFFFFF}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]!=0)
//  name: debug_bus_stall_rsn_cnt0[0] alias: perf_cnt_instrn_thread_stall_rsn_cnts note: THREAD0
//  name: perf_cnt_instrn_thread_stall_rsn_srcb_cleared alias: perf_cnt note: Only on thread 0
//  name: grant_cnt
#define DEBUG_BUS_STALL_RSN_CNT0_0__PERF_CNT_INSTRN_THREAD_STALL_RSN_SRCB_CLEARED_GRANT_CNT_WORD {13, 1, 0, 0, 1, 0}
#define DEBUG_BUS_STALL_RSN_CNT0_0__PERF_CNT_INSTRN_THREAD_STALL_RSN_SRCB_CLEARED_GRANT_CNT {DEBUG_BUS_STALL_RSN_CNT0_0__PERF_CNT_INSTRN_THREAD_STALL_RSN_SRCB_CLEARED_GRANT_CNT_WORD, 0, 0xFFFFFFFF}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]!=0)
//  name: debug_bus_stall_rsn_cnt0[0] alias: perf_cnt_instrn_thread_stall_rsn_cnts note: THREAD0
//  name: perf_cnt_instrn_thread_stall_rsn_srca_valid alias: perf_cnt note: Only on thread 0
//  name: grant_cnt
#define DEBUG_BUS_STALL_RSN_CNT0_0__PERF_CNT_INSTRN_THREAD_STALL_RSN_SRCA_VALID_GRANT_CNT_WORD {12, 1, 6, 0, 1, 0}
#define DEBUG_BUS_STALL_RSN_CNT0_0__PERF_CNT_INSTRN_THREAD_STALL_RSN_SRCA_VALID_GRANT_CNT {DEBUG_BUS_STALL_RSN_CNT0_0__PERF_CNT_INSTRN_THREAD_STALL_RSN_SRCA_VALID_GRANT_CNT_WORD, 0, 0xFFFFFFFF}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]!=0)
//  name: debug_bus_stall_rsn_cnt0[0] alias: perf_cnt_instrn_thread_stall_rsn_cnts note: THREAD0
//  name: perf_cnt_instrn_thread_stall_rsn_srcb_valid alias: perf_cnt note: Only on thread 0
//  name: grant_cnt
#define DEBUG_BUS_STALL_RSN_CNT0_0__PERF_CNT_INSTRN_THREAD_STALL_RSN_SRCB_VALID_GRANT_CNT_WORD {12, 1, 4, 0, 1, 0}
#define DEBUG_BUS_STALL_RSN_CNT0_0__PERF_CNT_INSTRN_THREAD_STALL_RSN_SRCB_VALID_GRANT_CNT {DEBUG_BUS_STALL_RSN_CNT0_0__PERF_CNT_INSTRN_THREAD_STALL_RSN_SRCB_VALID_GRANT_CNT_WORD, 0, 0xFFFFFFFF}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]!=0)
//  name: debug_bus_stall_rsn_cnt0[0] alias: perf_cnt_instrn_thread_stall_rsn_cnts note: THREAD0
//  name: perf_cnt_instrn_thread_stall_rsn_move alias: perf_cnt
//  name: grant_cnt
#define DEBUG_BUS_STALL_RSN_CNT0_0__PERF_CNT_INSTRN_THREAD_STALL_RSN_MOVE_GRANT_CNT_WORD {12, 1, 2, 0, 1, 0}
#define DEBUG_BUS_STALL_RSN_CNT0_0__PERF_CNT_INSTRN_THREAD_STALL_RSN_MOVE_GRANT_CNT {DEBUG_BUS_STALL_RSN_CNT0_0__PERF_CNT_INSTRN_THREAD_STALL_RSN_MOVE_GRANT_CNT_WORD, 0, 0xFFFFFFFF}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]!=0)
//  name: debug_bus_stall_rsn_cnt0[0] alias: perf_cnt_instrn_thread_stall_rsn_cnts note: THREAD0
//  name: perf_cnt_instrn_thread_stall_rsn_trisc_reg_access alias: perf_cnt
//  name: grant_cnt
#define DEBUG_BUS_STALL_RSN_CNT0_0__PERF_CNT_INSTRN_THREAD_STALL_RSN_TRISC_REG_ACCESS_GRANT_CNT_WORD {12, 1, 0, 0, 1, 0}
#define DEBUG_BUS_STALL_RSN_CNT0_0__PERF_CNT_INSTRN_THREAD_STALL_RSN_TRISC_REG_ACCESS_GRANT_CNT {DEBUG_BUS_STALL_RSN_CNT0_0__PERF_CNT_INSTRN_THREAD_STALL_RSN_TRISC_REG_ACCESS_GRANT_CNT_WORD, 0, 0xFFFFFFFF}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]!=0)
//  name: debug_bus_stall_rsn_cnt1[0] alias: perf_cnt_instrn_thread_stall_rsn_cnts note: THREAD0
//  name: perf_cnt_instrn_thread_stall_rsn_thcon alias: perf_cnt
//  name: grant_cnt
#define DEBUG_BUS_STALL_RSN_CNT1_0__PERF_CNT_INSTRN_THREAD_STALL_RSN_THCON_GRANT_CNT_WORD {11, 1, 6, 0, 1, 0}
#define DEBUG_BUS_STALL_RSN_CNT1_0__PERF_CNT_INSTRN_THREAD_STALL_RSN_THCON_GRANT_CNT {DEBUG_BUS_STALL_RSN_CNT1_0__PERF_CNT_INSTRN_THREAD_STALL_RSN_THCON_GRANT_CNT_WORD, 0, 0xFFFFFFFF}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]!=0)
//  name: debug_bus_stall_rsn_cnt1[0] alias: perf_cnt_instrn_thread_stall_rsn_cnts note: THREAD0
//  name: perf_cnt_instrn_thread_stall_rsn_unpack0 alias: perf_cnt
//  name: grant_cnt
#define DEBUG_BUS_STALL_RSN_CNT1_0__PERF_CNT_INSTRN_THREAD_STALL_RSN_UNPACK0_GRANT_CNT_WORD {11, 1, 4, 0, 1, 0}
#define DEBUG_BUS_STALL_RSN_CNT1_0__PERF_CNT_INSTRN_THREAD_STALL_RSN_UNPACK0_GRANT_CNT {DEBUG_BUS_STALL_RSN_CNT1_0__PERF_CNT_INSTRN_THREAD_STALL_RSN_UNPACK0_GRANT_CNT_WORD, 0, 0xFFFFFFFF}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]!=0)
//  name: debug_bus_stall_rsn_cnt1[0] alias: perf_cnt_instrn_thread_stall_rsn_cnts note: THREAD0
//  name: perf_cnt_instrn_thread_stall_rsn_pack0 alias: perf_cnt
//  name: grant_cnt
#define DEBUG_BUS_STALL_RSN_CNT1_0__PERF_CNT_INSTRN_THREAD_STALL_RSN_PACK0_GRANT_CNT_WORD {11, 1, 0, 0, 1, 0}
#define DEBUG_BUS_STALL_RSN_CNT1_0__PERF_CNT_INSTRN_THREAD_STALL_RSN_PACK0_GRANT_CNT {DEBUG_BUS_STALL_RSN_CNT1_0__PERF_CNT_INSTRN_THREAD_STALL_RSN_PACK0_GRANT_CNT_WORD, 0, 0xFFFFFFFF}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]!=0)
//  name: debug_bus_stall_rsn_cnt1[0] alias: perf_cnt_instrn_thread_stall_rsn_cnts note: THREAD0
//  name: perf_cnt_instrn_thread_stall_rsn_sfpu alias: perf_cnt
//  name: grant_cnt
#define DEBUG_BUS_STALL_RSN_CNT1_0__PERF_CNT_INSTRN_THREAD_STALL_RSN_SFPU_GRANT_CNT_WORD {10, 1, 2, 0, 1, 0}
#define DEBUG_BUS_STALL_RSN_CNT1_0__PERF_CNT_INSTRN_THREAD_STALL_RSN_SFPU_GRANT_CNT {DEBUG_BUS_STALL_RSN_CNT1_0__PERF_CNT_INSTRN_THREAD_STALL_RSN_SFPU_GRANT_CNT_WORD, 0, 0xFFFFFFFF}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]!=0)
//  name: debug_bus_stall_rsn_cnt1[0] alias: perf_cnt_instrn_thread_stall_rsn_cnts note: THREAD0
//  name: perf_cnt_instrn_thread_stall_rsn_math alias: perf_cnt
//  name: grant_cnt
#define DEBUG_BUS_STALL_RSN_CNT1_0__PERF_CNT_INSTRN_THREAD_STALL_RSN_MATH_GRANT_CNT_WORD {10, 1, 0, 0, 1, 0}
#define DEBUG_BUS_STALL_RSN_CNT1_0__PERF_CNT_INSTRN_THREAD_STALL_RSN_MATH_GRANT_CNT {DEBUG_BUS_STALL_RSN_CNT1_0__PERF_CNT_INSTRN_THREAD_STALL_RSN_MATH_GRANT_CNT_WORD, 0, 0xFFFFFFFF}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]!=0)
//  name: debug_bus_stall_rsn_cnt0[1] alias: perf_cnt_instrn_thread_stall_rsn_cnts note: THREAD1
//  name: perf_cnt_instrn_thread_stall_rsn_sem_zero alias: perf_cnt
//  name: grant_cnt
#define DEBUG_BUS_STALL_RSN_CNT0_1__PERF_CNT_INSTRN_THREAD_STALL_RSN_SEM_ZERO_GRANT_CNT_WORD {9, 1, 6, 0, 1, 0}
#define DEBUG_BUS_STALL_RSN_CNT0_1__PERF_CNT_INSTRN_THREAD_STALL_RSN_SEM_ZERO_GRANT_CNT {DEBUG_BUS_STALL_RSN_CNT0_1__PERF_CNT_INSTRN_THREAD_STALL_RSN_SEM_ZERO_GRANT_CNT_WORD, 0, 0xFFFFFFFF}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]!=0)
//  name: debug_bus_stall_rsn_cnt0[1] alias: perf_cnt_instrn_thread_stall_rsn_cnts note: THREAD1
//  name: perf_cnt_instrn_thread_stall_rsn_sem_max alias: perf_cnt
//  name: grant_cnt
#define DEBUG_BUS_STALL_RSN_CNT0_1__PERF_CNT_INSTRN_THREAD_STALL_RSN_SEM_MAX_GRANT_CNT_WORD {9, 1, 4, 0, 1, 0}
#define DEBUG_BUS_STALL_RSN_CNT0_1__PERF_CNT_INSTRN_THREAD_STALL_RSN_SEM_MAX_GRANT_CNT {DEBUG_BUS_STALL_RSN_CNT0_1__PERF_CNT_INSTRN_THREAD_STALL_RSN_SEM_MAX_GRANT_CNT_WORD, 0, 0xFFFFFFFF}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]!=0)
//  name: debug_bus_stall_rsn_cnt0[1] alias: perf_cnt_instrn_thread_stall_rsn_cnts note: THREAD1
//  name: perf_cnt_instrn_thread_stall_rsn_srca_cleared alias: perf_cnt note: Only on thread 0
//  name: grant_cnt
#define DEBUG_BUS_STALL_RSN_CNT0_1__PERF_CNT_INSTRN_THREAD_STALL_RSN_SRCA_CLEARED_GRANT_CNT_WORD {9, 1, 2, 0, 1, 0}
#define DEBUG_BUS_STALL_RSN_CNT0_1__PERF_CNT_INSTRN_THREAD_STALL_RSN_SRCA_CLEARED_GRANT_CNT {DEBUG_BUS_STALL_RSN_CNT0_1__PERF_CNT_INSTRN_THREAD_STALL_RSN_SRCA_CLEARED_GRANT_CNT_WORD, 0, 0xFFFFFFFF}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]!=0)
//  name: debug_bus_stall_rsn_cnt0[1] alias: perf_cnt_instrn_thread_stall_rsn_cnts note: THREAD1
//  name: perf_cnt_instrn_thread_stall_rsn_srcb_cleared alias: perf_cnt note: Only on thread 0
//  name: grant_cnt
#define DEBUG_BUS_STALL_RSN_CNT0_1__PERF_CNT_INSTRN_THREAD_STALL_RSN_SRCB_CLEARED_GRANT_CNT_WORD {9, 1, 0, 0, 1, 0}
#define DEBUG_BUS_STALL_RSN_CNT0_1__PERF_CNT_INSTRN_THREAD_STALL_RSN_SRCB_CLEARED_GRANT_CNT {DEBUG_BUS_STALL_RSN_CNT0_1__PERF_CNT_INSTRN_THREAD_STALL_RSN_SRCB_CLEARED_GRANT_CNT_WORD, 0, 0xFFFFFFFF}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]!=0)
//  name: debug_bus_stall_rsn_cnt0[1] alias: perf_cnt_instrn_thread_stall_rsn_cnts note: THREAD1
//  name: perf_cnt_instrn_thread_stall_rsn_srca_valid alias: perf_cnt note: Only on thread 0
//  name: grant_cnt
#define DEBUG_BUS_STALL_RSN_CNT0_1__PERF_CNT_INSTRN_THREAD_STALL_RSN_SRCA_VALID_GRANT_CNT_WORD {8, 1, 6, 0, 1, 0}
#define DEBUG_BUS_STALL_RSN_CNT0_1__PERF_CNT_INSTRN_THREAD_STALL_RSN_SRCA_VALID_GRANT_CNT {DEBUG_BUS_STALL_RSN_CNT0_1__PERF_CNT_INSTRN_THREAD_STALL_RSN_SRCA_VALID_GRANT_CNT_WORD, 0, 0xFFFFFFFF}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]!=0)
//  name: debug_bus_stall_rsn_cnt0[1] alias: perf_cnt_instrn_thread_stall_rsn_cnts note: THREAD1
//  name: perf_cnt_instrn_thread_stall_rsn_srcb_valid alias: perf_cnt note: Only on thread 0
//  name: grant_cnt
#define DEBUG_BUS_STALL_RSN_CNT0_1__PERF_CNT_INSTRN_THREAD_STALL_RSN_SRCB_VALID_GRANT_CNT_WORD {8, 1, 4, 0, 1, 0}
#define DEBUG_BUS_STALL_RSN_CNT0_1__PERF_CNT_INSTRN_THREAD_STALL_RSN_SRCB_VALID_GRANT_CNT {DEBUG_BUS_STALL_RSN_CNT0_1__PERF_CNT_INSTRN_THREAD_STALL_RSN_SRCB_VALID_GRANT_CNT_WORD, 0, 0xFFFFFFFF}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]!=0)
//  name: debug_bus_stall_rsn_cnt0[1] alias: perf_cnt_instrn_thread_stall_rsn_cnts note: THREAD1
//  name: perf_cnt_instrn_thread_stall_rsn_move alias: perf_cnt
//  name: grant_cnt
#define DEBUG_BUS_STALL_RSN_CNT0_1__PERF_CNT_INSTRN_THREAD_STALL_RSN_MOVE_GRANT_CNT_WORD {8, 1, 2, 0, 1, 0}
#define DEBUG_BUS_STALL_RSN_CNT0_1__PERF_CNT_INSTRN_THREAD_STALL_RSN_MOVE_GRANT_CNT {DEBUG_BUS_STALL_RSN_CNT0_1__PERF_CNT_INSTRN_THREAD_STALL_RSN_MOVE_GRANT_CNT_WORD, 0, 0xFFFFFFFF}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]!=0)
//  name: debug_bus_stall_rsn_cnt0[1] alias: perf_cnt_instrn_thread_stall_rsn_cnts note: THREAD1
//  name: perf_cnt_instrn_thread_stall_rsn_trisc_reg_access alias: perf_cnt
//  name: grant_cnt
#define DEBUG_BUS_STALL_RSN_CNT0_1__PERF_CNT_INSTRN_THREAD_STALL_RSN_TRISC_REG_ACCESS_GRANT_CNT_WORD {8, 1, 0, 0, 1, 0}
#define DEBUG_BUS_STALL_RSN_CNT0_1__PERF_CNT_INSTRN_THREAD_STALL_RSN_TRISC_REG_ACCESS_GRANT_CNT {DEBUG_BUS_STALL_RSN_CNT0_1__PERF_CNT_INSTRN_THREAD_STALL_RSN_TRISC_REG_ACCESS_GRANT_CNT_WORD, 0, 0xFFFFFFFF}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]!=0)
//  name: debug_bus_stall_rsn_cnt1[1] alias: perf_cnt_instrn_thread_stall_rsn_cnts note: THREAD1
//  name: perf_cnt_instrn_thread_stall_rsn_thcon alias: perf_cnt
//  name: grant_cnt
#define DEBUG_BUS_STALL_RSN_CNT1_1__PERF_CNT_INSTRN_THREAD_STALL_RSN_THCON_GRANT_CNT_WORD {7, 1, 6, 0, 1, 0}
#define DEBUG_BUS_STALL_RSN_CNT1_1__PERF_CNT_INSTRN_THREAD_STALL_RSN_THCON_GRANT_CNT {DEBUG_BUS_STALL_RSN_CNT1_1__PERF_CNT_INSTRN_THREAD_STALL_RSN_THCON_GRANT_CNT_WORD, 0, 0xFFFFFFFF}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]!=0)
//  name: debug_bus_stall_rsn_cnt1[1] alias: perf_cnt_instrn_thread_stall_rsn_cnts note: THREAD1
//  name: perf_cnt_instrn_thread_stall_rsn_unpack0 alias: perf_cnt
//  name: grant_cnt
#define DEBUG_BUS_STALL_RSN_CNT1_1__PERF_CNT_INSTRN_THREAD_STALL_RSN_UNPACK0_GRANT_CNT_WORD {7, 1, 4, 0, 1, 0}
#define DEBUG_BUS_STALL_RSN_CNT1_1__PERF_CNT_INSTRN_THREAD_STALL_RSN_UNPACK0_GRANT_CNT {DEBUG_BUS_STALL_RSN_CNT1_1__PERF_CNT_INSTRN_THREAD_STALL_RSN_UNPACK0_GRANT_CNT_WORD, 0, 0xFFFFFFFF}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]!=0)
//  name: debug_bus_stall_rsn_cnt1[1] alias: perf_cnt_instrn_thread_stall_rsn_cnts note: THREAD1
//  name: perf_cnt_instrn_thread_stall_rsn_pack0 alias: perf_cnt
//  name: grant_cnt
#define DEBUG_BUS_STALL_RSN_CNT1_1__PERF_CNT_INSTRN_THREAD_STALL_RSN_PACK0_GRANT_CNT_WORD {7, 1, 0, 0, 1, 0}
#define DEBUG_BUS_STALL_RSN_CNT1_1__PERF_CNT_INSTRN_THREAD_STALL_RSN_PACK0_GRANT_CNT {DEBUG_BUS_STALL_RSN_CNT1_1__PERF_CNT_INSTRN_THREAD_STALL_RSN_PACK0_GRANT_CNT_WORD, 0, 0xFFFFFFFF}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]!=0)
//  name: debug_bus_stall_rsn_cnt1[1] alias: perf_cnt_instrn_thread_stall_rsn_cnts note: THREAD1
//  name: perf_cnt_instrn_thread_stall_rsn_sfpu alias: perf_cnt
//  name: grant_cnt
#define DEBUG_BUS_STALL_RSN_CNT1_1__PERF_CNT_INSTRN_THREAD_STALL_RSN_SFPU_GRANT_CNT_WORD {6, 1, 2, 0, 1, 0}
#define DEBUG_BUS_STALL_RSN_CNT1_1__PERF_CNT_INSTRN_THREAD_STALL_RSN_SFPU_GRANT_CNT {DEBUG_BUS_STALL_RSN_CNT1_1__PERF_CNT_INSTRN_THREAD_STALL_RSN_SFPU_GRANT_CNT_WORD, 0, 0xFFFFFFFF}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]!=0)
//  name: debug_bus_stall_rsn_cnt1[1] alias: perf_cnt_instrn_thread_stall_rsn_cnts note: THREAD1
//  name: perf_cnt_instrn_thread_stall_rsn_math alias: perf_cnt
//  name: grant_cnt
#define DEBUG_BUS_STALL_RSN_CNT1_1__PERF_CNT_INSTRN_THREAD_STALL_RSN_MATH_GRANT_CNT_WORD {6, 1, 0, 0, 1, 0}
#define DEBUG_BUS_STALL_RSN_CNT1_1__PERF_CNT_INSTRN_THREAD_STALL_RSN_MATH_GRANT_CNT {DEBUG_BUS_STALL_RSN_CNT1_1__PERF_CNT_INSTRN_THREAD_STALL_RSN_MATH_GRANT_CNT_WORD, 0, 0xFFFFFFFF}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]!=0)
//  name: debug_bus_stall_rsn_cnt0[2] alias: perf_cnt_instrn_thread_stall_rsn_cnts note: THREAD2
//  name: perf_cnt_instrn_thread_stall_rsn_sem_zero alias: perf_cnt
//  name: grant_cnt
#define DEBUG_BUS_STALL_RSN_CNT0_2__PERF_CNT_INSTRN_THREAD_STALL_RSN_SEM_ZERO_GRANT_CNT_WORD {5, 1, 6, 0, 1, 0}
#define DEBUG_BUS_STALL_RSN_CNT0_2__PERF_CNT_INSTRN_THREAD_STALL_RSN_SEM_ZERO_GRANT_CNT {DEBUG_BUS_STALL_RSN_CNT0_2__PERF_CNT_INSTRN_THREAD_STALL_RSN_SEM_ZERO_GRANT_CNT_WORD, 0, 0xFFFFFFFF}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]!=0)
//  name: debug_bus_stall_rsn_cnt0[2] alias: perf_cnt_instrn_thread_stall_rsn_cnts note: THREAD2
//  name: perf_cnt_instrn_thread_stall_rsn_sem_max alias: perf_cnt
//  name: grant_cnt
#define DEBUG_BUS_STALL_RSN_CNT0_2__PERF_CNT_INSTRN_THREAD_STALL_RSN_SEM_MAX_GRANT_CNT_WORD {5, 1, 4, 0, 1, 0}
#define DEBUG_BUS_STALL_RSN_CNT0_2__PERF_CNT_INSTRN_THREAD_STALL_RSN_SEM_MAX_GRANT_CNT {DEBUG_BUS_STALL_RSN_CNT0_2__PERF_CNT_INSTRN_THREAD_STALL_RSN_SEM_MAX_GRANT_CNT_WORD, 0, 0xFFFFFFFF}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]!=0)
//  name: debug_bus_stall_rsn_cnt0[2] alias: perf_cnt_instrn_thread_stall_rsn_cnts note: THREAD2
//  name: perf_cnt_instrn_thread_stall_rsn_srca_cleared alias: perf_cnt note: Only on thread 0
//  name: grant_cnt
#define DEBUG_BUS_STALL_RSN_CNT0_2__PERF_CNT_INSTRN_THREAD_STALL_RSN_SRCA_CLEARED_GRANT_CNT_WORD {5, 1, 2, 0, 1, 0}
#define DEBUG_BUS_STALL_RSN_CNT0_2__PERF_CNT_INSTRN_THREAD_STALL_RSN_SRCA_CLEARED_GRANT_CNT {DEBUG_BUS_STALL_RSN_CNT0_2__PERF_CNT_INSTRN_THREAD_STALL_RSN_SRCA_CLEARED_GRANT_CNT_WORD, 0, 0xFFFFFFFF}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]!=0)
//  name: debug_bus_stall_rsn_cnt0[2] alias: perf_cnt_instrn_thread_stall_rsn_cnts note: THREAD2
//  name: perf_cnt_instrn_thread_stall_rsn_srcb_cleared alias: perf_cnt note: Only on thread 0
//  name: grant_cnt
#define DEBUG_BUS_STALL_RSN_CNT0_2__PERF_CNT_INSTRN_THREAD_STALL_RSN_SRCB_CLEARED_GRANT_CNT_WORD {5, 1, 0, 0, 1, 0}
#define DEBUG_BUS_STALL_RSN_CNT0_2__PERF_CNT_INSTRN_THREAD_STALL_RSN_SRCB_CLEARED_GRANT_CNT {DEBUG_BUS_STALL_RSN_CNT0_2__PERF_CNT_INSTRN_THREAD_STALL_RSN_SRCB_CLEARED_GRANT_CNT_WORD, 0, 0xFFFFFFFF}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]!=0)
//  name: debug_bus_stall_rsn_cnt0[2] alias: perf_cnt_instrn_thread_stall_rsn_cnts note: THREAD2
//  name: perf_cnt_instrn_thread_stall_rsn_srca_valid alias: perf_cnt note: Only on thread 0
//  name: grant_cnt
#define DEBUG_BUS_STALL_RSN_CNT0_2__PERF_CNT_INSTRN_THREAD_STALL_RSN_SRCA_VALID_GRANT_CNT_WORD {4, 1, 6, 0, 1, 0}
#define DEBUG_BUS_STALL_RSN_CNT0_2__PERF_CNT_INSTRN_THREAD_STALL_RSN_SRCA_VALID_GRANT_CNT {DEBUG_BUS_STALL_RSN_CNT0_2__PERF_CNT_INSTRN_THREAD_STALL_RSN_SRCA_VALID_GRANT_CNT_WORD, 0, 0xFFFFFFFF}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]!=0)
//  name: debug_bus_stall_rsn_cnt0[2] alias: perf_cnt_instrn_thread_stall_rsn_cnts note: THREAD2
//  name: perf_cnt_instrn_thread_stall_rsn_srcb_valid alias: perf_cnt note: Only on thread 0
//  name: grant_cnt
#define DEBUG_BUS_STALL_RSN_CNT0_2__PERF_CNT_INSTRN_THREAD_STALL_RSN_SRCB_VALID_GRANT_CNT_WORD {4, 1, 4, 0, 1, 0}
#define DEBUG_BUS_STALL_RSN_CNT0_2__PERF_CNT_INSTRN_THREAD_STALL_RSN_SRCB_VALID_GRANT_CNT {DEBUG_BUS_STALL_RSN_CNT0_2__PERF_CNT_INSTRN_THREAD_STALL_RSN_SRCB_VALID_GRANT_CNT_WORD, 0, 0xFFFFFFFF}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]!=0)
//  name: debug_bus_stall_rsn_cnt0[2] alias: perf_cnt_instrn_thread_stall_rsn_cnts note: THREAD2
//  name: perf_cnt_instrn_thread_stall_rsn_move alias: perf_cnt
//  name: grant_cnt
#define DEBUG_BUS_STALL_RSN_CNT0_2__PERF_CNT_INSTRN_THREAD_STALL_RSN_MOVE_GRANT_CNT_WORD {4, 1, 2, 0, 1, 0}
#define DEBUG_BUS_STALL_RSN_CNT0_2__PERF_CNT_INSTRN_THREAD_STALL_RSN_MOVE_GRANT_CNT {DEBUG_BUS_STALL_RSN_CNT0_2__PERF_CNT_INSTRN_THREAD_STALL_RSN_MOVE_GRANT_CNT_WORD, 0, 0xFFFFFFFF}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]!=0)
//  name: debug_bus_stall_rsn_cnt0[2] alias: perf_cnt_instrn_thread_stall_rsn_cnts note: THREAD2
//  name: perf_cnt_instrn_thread_stall_rsn_trisc_reg_access alias: perf_cnt
//  name: grant_cnt
#define DEBUG_BUS_STALL_RSN_CNT0_2__PERF_CNT_INSTRN_THREAD_STALL_RSN_TRISC_REG_ACCESS_GRANT_CNT_WORD {4, 1, 0, 0, 1, 0}
#define DEBUG_BUS_STALL_RSN_CNT0_2__PERF_CNT_INSTRN_THREAD_STALL_RSN_TRISC_REG_ACCESS_GRANT_CNT {DEBUG_BUS_STALL_RSN_CNT0_2__PERF_CNT_INSTRN_THREAD_STALL_RSN_TRISC_REG_ACCESS_GRANT_CNT_WORD, 0, 0xFFFFFFFF}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]!=0)
//  name: debug_bus_stall_rsn_cnt1[2] alias: perf_cnt_instrn_thread_stall_rsn_cnts note: THREAD2
//  name: perf_cnt_instrn_thread_stall_rsn_thcon alias: perf_cnt
//  name: grant_cnt
#define DEBUG_BUS_STALL_RSN_CNT1_2__PERF_CNT_INSTRN_THREAD_STALL_RSN_THCON_GRANT_CNT_WORD {3, 1, 6, 0, 1, 0}
#define DEBUG_BUS_STALL_RSN_CNT1_2__PERF_CNT_INSTRN_THREAD_STALL_RSN_THCON_GRANT_CNT {DEBUG_BUS_STALL_RSN_CNT1_2__PERF_CNT_INSTRN_THREAD_STALL_RSN_THCON_GRANT_CNT_WORD, 0, 0xFFFFFFFF}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]!=0)
//  name: debug_bus_stall_rsn_cnt1[2] alias: perf_cnt_instrn_thread_stall_rsn_cnts note: THREAD2
//  name: perf_cnt_instrn_thread_stall_rsn_unpack0 alias: perf_cnt
//  name: grant_cnt
#define DEBUG_BUS_STALL_RSN_CNT1_2__PERF_CNT_INSTRN_THREAD_STALL_RSN_UNPACK0_GRANT_CNT_WORD {3, 1, 4, 0, 1, 0}
#define DEBUG_BUS_STALL_RSN_CNT1_2__PERF_CNT_INSTRN_THREAD_STALL_RSN_UNPACK0_GRANT_CNT {DEBUG_BUS_STALL_RSN_CNT1_2__PERF_CNT_INSTRN_THREAD_STALL_RSN_UNPACK0_GRANT_CNT_WORD, 0, 0xFFFFFFFF}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]!=0)
//  name: debug_bus_stall_rsn_cnt1[2] alias: perf_cnt_instrn_thread_stall_rsn_cnts note: THREAD2
//  name: perf_cnt_instrn_thread_stall_rsn_pack0 alias: perf_cnt
//  name: grant_cnt
#define DEBUG_BUS_STALL_RSN_CNT1_2__PERF_CNT_INSTRN_THREAD_STALL_RSN_PACK0_GRANT_CNT_WORD {3, 1, 0, 0, 1, 0}
#define DEBUG_BUS_STALL_RSN_CNT1_2__PERF_CNT_INSTRN_THREAD_STALL_RSN_PACK0_GRANT_CNT {DEBUG_BUS_STALL_RSN_CNT1_2__PERF_CNT_INSTRN_THREAD_STALL_RSN_PACK0_GRANT_CNT_WORD, 0, 0xFFFFFFFF}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]!=0)
//  name: debug_bus_stall_rsn_cnt1[2] alias: perf_cnt_instrn_thread_stall_rsn_cnts note: THREAD2
//  name: perf_cnt_instrn_thread_stall_rsn_sfpu alias: perf_cnt
//  name: grant_cnt
#define DEBUG_BUS_STALL_RSN_CNT1_2__PERF_CNT_INSTRN_THREAD_STALL_RSN_SFPU_GRANT_CNT_WORD {2, 1, 2, 0, 1, 0}
#define DEBUG_BUS_STALL_RSN_CNT1_2__PERF_CNT_INSTRN_THREAD_STALL_RSN_SFPU_GRANT_CNT {DEBUG_BUS_STALL_RSN_CNT1_2__PERF_CNT_INSTRN_THREAD_STALL_RSN_SFPU_GRANT_CNT_WORD, 0, 0xFFFFFFFF}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]!=0)
//  name: debug_bus_stall_rsn_cnt1[2] alias: perf_cnt_instrn_thread_stall_rsn_cnts note: THREAD2
//  name: perf_cnt_instrn_thread_stall_rsn_math alias: perf_cnt
//  name: grant_cnt
#define DEBUG_BUS_STALL_RSN_CNT1_2__PERF_CNT_INSTRN_THREAD_STALL_RSN_MATH_GRANT_CNT_WORD {2, 1, 0, 0, 1, 0}
#define DEBUG_BUS_STALL_RSN_CNT1_2__PERF_CNT_INSTRN_THREAD_STALL_RSN_MATH_GRANT_CNT {DEBUG_BUS_STALL_RSN_CNT1_2__PERF_CNT_INSTRN_THREAD_STALL_RSN_MATH_GRANT_CNT_WORD, 0, 0xFFFFFFFF}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]!=0)
//  name: perf_cnt_instrn_thread_stall_cnts[0] alias: perf_cnt note: THREAD0
//  name: req_cnt
#define DEBUG_BUS_DEBUG_DAISY_STOP_THREADS_WHEN_I_DBG_INSTRN_THREAD_PERF_CNT_MUX_SEL_0___0__PERF_CNT_INSTRN_THREAD_STALL_CNTS_0__REQ_CNT_WORD {1, 1, 6, 0, 1, 0}
#define DEBUG_BUS_DEBUG_DAISY_STOP_THREADS_WHEN_I_DBG_INSTRN_THREAD_PERF_CNT_MUX_SEL_0___0__PERF_CNT_INSTRN_THREAD_STALL_CNTS_0__REQ_CNT {DEBUG_BUS_DEBUG_DAISY_STOP_THREADS_WHEN_I_DBG_INSTRN_THREAD_PERF_CNT_MUX_SEL_0___0__PERF_CNT_INSTRN_THREAD_STALL_CNTS_0__REQ_CNT_WORD, 0, 0xFFFFFFFF}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]!=0)
//  name: perf_cnt_instrn_thread_stall_cnts[1] alias: perf_cnt note: THREAD1
//  name: req_cnt
#define DEBUG_BUS_DEBUG_DAISY_STOP_THREADS_WHEN_I_DBG_INSTRN_THREAD_PERF_CNT_MUX_SEL_0___0__PERF_CNT_INSTRN_THREAD_STALL_CNTS_1__REQ_CNT_WORD {1, 1, 4, 0, 1, 0}
#define DEBUG_BUS_DEBUG_DAISY_STOP_THREADS_WHEN_I_DBG_INSTRN_THREAD_PERF_CNT_MUX_SEL_0___0__PERF_CNT_INSTRN_THREAD_STALL_CNTS_1__REQ_CNT {DEBUG_BUS_DEBUG_DAISY_STOP_THREADS_WHEN_I_DBG_INSTRN_THREAD_PERF_CNT_MUX_SEL_0___0__PERF_CNT_INSTRN_THREAD_STALL_CNTS_1__REQ_CNT_WORD, 0, 0xFFFFFFFF}

//  location: tt_instruction_thread name: debug_daisy_stop_threads(when i_dbg_instrn_thread_perf_cnt_mux_sel[0]!=0)
//  name: perf_cnt_instrn_thread_stall_cnts[2] alias: perf_cnt note: THREAD2
//  name: req_cnt
#define DEBUG_BUS_DEBUG_DAISY_STOP_THREADS_WHEN_I_DBG_INSTRN_THREAD_PERF_CNT_MUX_SEL_0___0__PERF_CNT_INSTRN_THREAD_STALL_CNTS_2__REQ_CNT_WORD {1, 1, 2, 0, 1, 0}
#define DEBUG_BUS_DEBUG_DAISY_STOP_THREADS_WHEN_I_DBG_INSTRN_THREAD_PERF_CNT_MUX_SEL_0___0__PERF_CNT_INSTRN_THREAD_STALL_CNTS_2__REQ_CNT {DEBUG_BUS_DEBUG_DAISY_STOP_THREADS_WHEN_I_DBG_INSTRN_THREAD_PERF_CNT_MUX_SEL_0___0__PERF_CNT_INSTRN_THREAD_STALL_CNTS_2__REQ_CNT_WORD, 0, 0xFFFFFFFF}

//  location: tt_instruction_thread name: debug_daisy_stop_gprs
//  name: thcon_gpr_debug_bus
//  name: thcon_p0_tid
#define DEBUG_BUS_THCON_P0_TID_WORD {16, 2, 6, 0, 1, 0}
#define DEBUG_BUS_THCON_P0_TID {DEBUG_BUS_THCON_P0_TID_WORD, 16, 0x3}

//  location: tt_instruction_thread name: debug_daisy_stop_gprs
//  name: thcon_gpr_debug_bus
//  name: thcon_p0_wren
#define DEBUG_BUS_THCON_P0_WREN_WORD {16, 2, 6, 0, 1, 0}
#define DEBUG_BUS_THCON_P0_WREN {DEBUG_BUS_THCON_P0_WREN_WORD, 15, 0x1}

//  location: tt_instruction_thread name: debug_daisy_stop_gprs
//  name: thcon_gpr_debug_bus
//  name: thcon_p0_rden
#define DEBUG_BUS_THCON_P0_RDEN_WORD {16, 2, 6, 0, 1, 0}
#define DEBUG_BUS_THCON_P0_RDEN {DEBUG_BUS_THCON_P0_RDEN_WORD, 14, 0x1}

//  location: tt_instruction_thread name: debug_daisy_stop_gprs
//  name: thcon_gpr_debug_bus
//  name: thcon_p0_gpr_addr
#define DEBUG_BUS_THCON_P0_GPR_ADDR_WORD {16, 2, 6, 0, 1, 0}
#define DEBUG_BUS_THCON_P0_GPR_ADDR {DEBUG_BUS_THCON_P0_GPR_ADDR_WORD, 10, 0xF}

//  location: tt_instruction_thread name: debug_daisy_stop_gprs
//  name: thcon_gpr_debug_bus
//  name: thcon_p0_gpr_byten
#define DEBUG_BUS_THCON_P0_GPR_BYTEN__5_0_WORD {16, 2, 4, 0, 1, 0}
#define DEBUG_BUS_THCON_P0_GPR_BYTEN__5_0 {DEBUG_BUS_THCON_P0_GPR_BYTEN__5_0_WORD, 26, 0x3F}

//  location: tt_instruction_thread name: debug_daisy_stop_gprs
//  name: thcon_gpr_debug_bus
//  name: thcon_p0_gpr_byten
#define DEBUG_BUS_THCON_P0_GPR_BYTEN__15_6_WORD {16, 2, 6, 0, 1, 0}
#define DEBUG_BUS_THCON_P0_GPR_BYTEN__15_6 {DEBUG_BUS_THCON_P0_GPR_BYTEN__15_6_WORD, 0, 0x3FF}

//  location: tt_instruction_thread name: debug_daisy_stop_gprs
//  name: thcon_gpr_debug_bus
//  name: p0_gpr_accept
#define DEBUG_BUS_P0_GPR_ACCEPT_WORD {16, 2, 4, 0, 1, 0}
#define DEBUG_BUS_P0_GPR_ACCEPT {DEBUG_BUS_P0_GPR_ACCEPT_WORD, 25, 0x1}

//  location: tt_instruction_thread name: debug_daisy_stop_gprs
//  name: thcon_gpr_debug_bus
//  name: cfg_gpr_p0_req
#define DEBUG_BUS_CFG_GPR_P0_REQ_WORD {16, 2, 4, 0, 1, 0}
#define DEBUG_BUS_CFG_GPR_P0_REQ {DEBUG_BUS_CFG_GPR_P0_REQ_WORD, 24, 0x1}

//  location: tt_instruction_thread name: debug_daisy_stop_gprs
//  name: thcon_gpr_debug_bus
//  name: cfg_gpr_p0_tid
#define DEBUG_BUS_CFG_GPR_P0_TID_WORD {16, 2, 4, 0, 1, 0}
#define DEBUG_BUS_CFG_GPR_P0_TID {DEBUG_BUS_CFG_GPR_P0_TID_WORD, 22, 0x3}

//  location: tt_instruction_thread name: debug_daisy_stop_gprs
//  name: thcon_gpr_debug_bus
//  name: cfg_gpr_p0_addr
#define DEBUG_BUS_CFG_GPR_P0_ADDR_WORD {16, 2, 4, 0, 1, 0}
#define DEBUG_BUS_CFG_GPR_P0_ADDR {DEBUG_BUS_CFG_GPR_P0_ADDR_WORD, 18, 0xF}

//  location: tt_instruction_thread name: debug_daisy_stop_gprs
//  name: thcon_gpr_debug_bus
//  name: gpr_cfg_p0_accept
#define DEBUG_BUS_GPR_CFG_P0_ACCEPT_WORD {16, 2, 4, 0, 1, 0}
#define DEBUG_BUS_GPR_CFG_P0_ACCEPT {DEBUG_BUS_GPR_CFG_P0_ACCEPT_WORD, 17, 0x1}

//  location: tt_instruction_thread name: debug_daisy_stop_gprs
//  name: thcon_gpr_debug_bus
//  name: l1_ret_vld
#define DEBUG_BUS_L1_RET_VLD_WORD {16, 2, 4, 0, 1, 0}
#define DEBUG_BUS_L1_RET_VLD {DEBUG_BUS_L1_RET_VLD_WORD, 16, 0x1}

//  location: tt_instruction_thread name: debug_daisy_stop_gprs
//  name: thcon_gpr_debug_bus
//  name: l1_ret_tid
#define DEBUG_BUS_L1_RET_TID_WORD {16, 2, 4, 0, 1, 0}
#define DEBUG_BUS_L1_RET_TID {DEBUG_BUS_L1_RET_TID_WORD, 14, 0x3}

//  location: tt_instruction_thread name: debug_daisy_stop_gprs
//  name: thcon_gpr_debug_bus
//  name: l1_ret_gpr
#define DEBUG_BUS_L1_RET_GPR_WORD {16, 2, 4, 0, 1, 0}
#define DEBUG_BUS_L1_RET_GPR {DEBUG_BUS_L1_RET_GPR_WORD, 10, 0xF}

//  location: tt_instruction_thread name: debug_daisy_stop_gprs
//  name: thcon_gpr_debug_bus
//  name: l1_ret_byten
#define DEBUG_BUS_L1_RET_BYTEN__5_0_WORD {16, 2, 2, 0, 1, 0}
#define DEBUG_BUS_L1_RET_BYTEN__5_0 {DEBUG_BUS_L1_RET_BYTEN__5_0_WORD, 26, 0x3F}

//  location: tt_instruction_thread name: debug_daisy_stop_gprs
//  name: thcon_gpr_debug_bus
//  name: l1_ret_byten
#define DEBUG_BUS_L1_RET_BYTEN__15_6_WORD {16, 2, 4, 0, 1, 0}
#define DEBUG_BUS_L1_RET_BYTEN__15_6 {DEBUG_BUS_L1_RET_BYTEN__15_6_WORD, 0, 0x3FF}

//  location: tt_instruction_thread name: debug_daisy_stop_gprs
//  name: thcon_gpr_debug_bus
//  name: l1_return_accept
#define DEBUG_BUS_L1_RETURN_ACCEPT_WORD {16, 2, 2, 0, 1, 0}
#define DEBUG_BUS_L1_RETURN_ACCEPT {DEBUG_BUS_L1_RETURN_ACCEPT_WORD, 25, 0x1}

//  location: tt_instruction_thread name: debug_daisy_stop_gprs
//  name: thcon_gpr_debug_bus
//  name: thcon_p1_req_vld
#define DEBUG_BUS_THCON_P1_REQ_VLD_WORD {16, 2, 2, 0, 1, 0}
#define DEBUG_BUS_THCON_P1_REQ_VLD {DEBUG_BUS_THCON_P1_REQ_VLD_WORD, 24, 0x1}

//  location: tt_instruction_thread name: debug_daisy_stop_gprs
//  name: thcon_gpr_debug_bus
//  name: thcon_p1_tid
#define DEBUG_BUS_THCON_P1_TID_WORD {16, 2, 2, 0, 1, 0}
#define DEBUG_BUS_THCON_P1_TID {DEBUG_BUS_THCON_P1_TID_WORD, 22, 0x3}

//  location: tt_instruction_thread name: debug_daisy_stop_gprs
//  name: thcon_gpr_debug_bus
//  name: thcon_p1_gpr
#define DEBUG_BUS_THCON_P1_GPR_WORD {16, 2, 2, 0, 1, 0}
#define DEBUG_BUS_THCON_P1_GPR {DEBUG_BUS_THCON_P1_GPR_WORD, 18, 0xF}

//  location: tt_instruction_thread name: debug_daisy_stop_gprs
//  name: thcon_gpr_debug_bus
//  name: thcon_p1_req_accept
#define DEBUG_BUS_THCON_P1_REQ_ACCEPT_WORD {16, 2, 2, 0, 1, 0}
#define DEBUG_BUS_THCON_P1_REQ_ACCEPT {DEBUG_BUS_THCON_P1_REQ_ACCEPT_WORD, 17, 0x1}

//  location: tt_instruction_thread name: debug_daisy_stop_gprs
//  name: thcon_gpr_debug_bus
//  name: cfg_gpr_p1_req
#define DEBUG_BUS_CFG_GPR_P1_REQ_WORD {16, 2, 2, 0, 1, 0}
#define DEBUG_BUS_CFG_GPR_P1_REQ {DEBUG_BUS_CFG_GPR_P1_REQ_WORD, 16, 0x1}

//  location: tt_instruction_thread name: debug_daisy_stop_gprs
//  name: thcon_gpr_debug_bus
//  name: cfg_gpr_p1_tid
#define DEBUG_BUS_CFG_GPR_P1_TID_WORD {16, 2, 2, 0, 1, 0}
#define DEBUG_BUS_CFG_GPR_P1_TID {DEBUG_BUS_CFG_GPR_P1_TID_WORD, 14, 0x3}

//  location: tt_instruction_thread name: debug_daisy_stop_gprs
//  name: thcon_gpr_debug_bus
//  name: cfg_gpr_p1_addr
#define DEBUG_BUS_CFG_GPR_P1_ADDR_WORD {16, 2, 2, 0, 1, 0}
#define DEBUG_BUS_CFG_GPR_P1_ADDR {DEBUG_BUS_CFG_GPR_P1_ADDR_WORD, 10, 0xF}

//  location: tt_instruction_thread name: debug_daisy_stop_gprs
//  name: thcon_gpr_debug_bus
//  name: cfg_gpr_p1_byten
#define DEBUG_BUS_CFG_GPR_P1_BYTEN__5_0_WORD {16, 2, 0, 0, 1, 0}
#define DEBUG_BUS_CFG_GPR_P1_BYTEN__5_0 {DEBUG_BUS_CFG_GPR_P1_BYTEN__5_0_WORD, 26, 0x3F}

//  location: tt_instruction_thread name: debug_daisy_stop_gprs
//  name: thcon_gpr_debug_bus
//  name: cfg_gpr_p1_byten
#define DEBUG_BUS_CFG_GPR_P1_BYTEN__15_6_WORD {16, 2, 2, 0, 1, 0}
#define DEBUG_BUS_CFG_GPR_P1_BYTEN__15_6 {DEBUG_BUS_CFG_GPR_P1_BYTEN__15_6_WORD, 0, 0x3FF}

//  location: tt_instruction_thread name: debug_daisy_stop_gprs
//  name: thcon_gpr_debug_bus
//  name: gpr_cfg_p1_accept
#define DEBUG_BUS_GPR_CFG_P1_ACCEPT_WORD {16, 2, 0, 0, 1, 0}
#define DEBUG_BUS_GPR_CFG_P1_ACCEPT {DEBUG_BUS_GPR_CFG_P1_ACCEPT_WORD, 25, 0x1}

//  location: tt_instruction_thread name: debug_daisy_stop_gprs
//  name: thcon_gpr_debug_bus
//  name: i_risc_out_reg_rden
#define DEBUG_BUS_I_RISC_OUT_REG_RDEN_WORD {16, 2, 0, 0, 1, 0}
#define DEBUG_BUS_I_RISC_OUT_REG_RDEN {DEBUG_BUS_I_RISC_OUT_REG_RDEN_WORD, 24, 0x1}

//  location: tt_instruction_thread name: debug_daisy_stop_gprs
//  name: thcon_gpr_debug_bus
//  name: i_risc_out_reg_wren
#define DEBUG_BUS_I_RISC_OUT_REG_WREN_WORD {16, 2, 0, 0, 1, 0}
#define DEBUG_BUS_I_RISC_OUT_REG_WREN {DEBUG_BUS_I_RISC_OUT_REG_WREN_WORD, 23, 0x1}

//  location: tt_instruction_thread name: debug_daisy_stop_gprs
//  name: thcon_gpr_debug_bus
//  name: riscv_tid
#define DEBUG_BUS_RISCV_TID_WORD {16, 2, 0, 0, 1, 0}
#define DEBUG_BUS_RISCV_TID {DEBUG_BUS_RISCV_TID_WORD, 21, 0x3}

//  location: tt_instruction_thread name: debug_daisy_stop_gprs
//  name: thcon_gpr_debug_bus
//  name: i_risc_out_reg_index
#define DEBUG_BUS_I_RISC_OUT_REG_INDEX__5_2_WORD {16, 2, 0, 0, 1, 0}
#define DEBUG_BUS_I_RISC_OUT_REG_INDEX__5_2 {DEBUG_BUS_I_RISC_OUT_REG_INDEX__5_2_WORD, 17, 0xF}

//  location: tt_instruction_thread name: debug_daisy_stop_gprs
//  name: thcon_gpr_debug_bus
//  name: i_risc_out_reg_byten
#define DEBUG_BUS_I_RISC_OUT_REG_BYTEN_WORD {16, 2, 0, 0, 1, 0}
#define DEBUG_BUS_I_RISC_OUT_REG_BYTEN {DEBUG_BUS_I_RISC_OUT_REG_BYTEN_WORD, 1, 0xFFFF}

//  location: tt_instruction_thread name: debug_daisy_stop_gprs
//  name: thcon_gpr_debug_bus
//  name: o_risc_in_reg_req_ready
#define DEBUG_BUS_O_RISC_IN_REG_REQ_READY_WORD {16, 2, 0, 0, 1, 0}
#define DEBUG_BUS_O_RISC_IN_REG_REQ_READY {DEBUG_BUS_O_RISC_IN_REG_REQ_READY_WORD, 0, 0x1}

//  location: tt_instruction_thread name: debug_daisy_stop_gprs
//  name: gpr_data_debug_buses[0]
//  name: thcon_p0_gpr_wrdata
#define DEBUG_BUS_THCON_P0_GPR_WRDATA__31_0_WORD {14, 2, 0, 0, 1, 0}
#define DEBUG_BUS_THCON_P0_GPR_WRDATA__31_0 {DEBUG_BUS_THCON_P0_GPR_WRDATA__31_0_WORD, 0, 0xFFFFFFFF}

//  location: tt_instruction_thread name: debug_daisy_stop_gprs
//  name: gpr_data_debug_buses[0]
//  name: thcon_p0_gpr_wrdata
#define DEBUG_BUS_THCON_P0_GPR_WRDATA__63_32_WORD {14, 2, 2, 0, 1, 0}
#define DEBUG_BUS_THCON_P0_GPR_WRDATA__63_32 {DEBUG_BUS_THCON_P0_GPR_WRDATA__63_32_WORD, 0, 0xFFFFFFFF}

//  location: tt_instruction_thread name: debug_daisy_stop_gprs
//  name: gpr_data_debug_buses[0]
//  name: thcon_p0_gpr_wrdata
#define DEBUG_BUS_THCON_P0_GPR_WRDATA__95_64_WORD {14, 2, 4, 0, 1, 0}
#define DEBUG_BUS_THCON_P0_GPR_WRDATA__95_64 {DEBUG_BUS_THCON_P0_GPR_WRDATA__95_64_WORD, 0, 0xFFFFFFFF}

//  location: tt_instruction_thread name: debug_daisy_stop_gprs
//  name: gpr_data_debug_buses[0]
//  name: thcon_p0_gpr_wrdata
#define DEBUG_BUS_THCON_P0_GPR_WRDATA__127_96_WORD {14, 2, 6, 0, 1, 0}
#define DEBUG_BUS_THCON_P0_GPR_WRDATA__127_96 {DEBUG_BUS_THCON_P0_GPR_WRDATA__127_96_WORD, 0, 0xFFFFFFFF}

//  location: tt_instruction_thread name: debug_daisy_stop_gprs
//  name: gpr_data_debug_buses[1]
//  name: p0_gpr_ret
#define DEBUG_BUS_P0_GPR_RET__31_0_WORD {12, 2, 0, 0, 1, 0}
#define DEBUG_BUS_P0_GPR_RET__31_0 {DEBUG_BUS_P0_GPR_RET__31_0_WORD, 0, 0xFFFFFFFF}

//  location: tt_instruction_thread name: debug_daisy_stop_gprs
//  name: gpr_data_debug_buses[1]
//  name: p0_gpr_ret
#define DEBUG_BUS_P0_GPR_RET__63_32_WORD {12, 2, 2, 0, 1, 0}
#define DEBUG_BUS_P0_GPR_RET__63_32 {DEBUG_BUS_P0_GPR_RET__63_32_WORD, 0, 0xFFFFFFFF}

//  location: tt_instruction_thread name: debug_daisy_stop_gprs
//  name: gpr_data_debug_buses[1]
//  name: p0_gpr_ret
#define DEBUG_BUS_P0_GPR_RET__95_64_WORD {12, 2, 4, 0, 1, 0}
#define DEBUG_BUS_P0_GPR_RET__95_64 {DEBUG_BUS_P0_GPR_RET__95_64_WORD, 0, 0xFFFFFFFF}

//  location: tt_instruction_thread name: debug_daisy_stop_gprs
//  name: gpr_data_debug_buses[1]
//  name: p0_gpr_ret
#define DEBUG_BUS_P0_GPR_RET__127_96_WORD {12, 2, 6, 0, 1, 0}
#define DEBUG_BUS_P0_GPR_RET__127_96 {DEBUG_BUS_P0_GPR_RET__127_96_WORD, 0, 0xFFFFFFFF}

//  location: tt_instruction_thread name: debug_daisy_stop_gprs
//  name: gpr_data_debug_buses[2]
//  name: gpr_cfg_p0_data
#define DEBUG_BUS_GPR_CFG_P0_DATA__31_0_WORD {10, 2, 0, 0, 1, 0}
#define DEBUG_BUS_GPR_CFG_P0_DATA__31_0 {DEBUG_BUS_GPR_CFG_P0_DATA__31_0_WORD, 0, 0xFFFFFFFF}

//  location: tt_instruction_thread name: debug_daisy_stop_gprs
//  name: gpr_data_debug_buses[2]
//  name: gpr_cfg_p0_data
#define DEBUG_BUS_GPR_CFG_P0_DATA__63_32_WORD {10, 2, 2, 0, 1, 0}
#define DEBUG_BUS_GPR_CFG_P0_DATA__63_32 {DEBUG_BUS_GPR_CFG_P0_DATA__63_32_WORD, 0, 0xFFFFFFFF}

//  location: tt_instruction_thread name: debug_daisy_stop_gprs
//  name: gpr_data_debug_buses[2]
//  name: gpr_cfg_p0_data
#define DEBUG_BUS_GPR_CFG_P0_DATA__95_64_WORD {10, 2, 4, 0, 1, 0}
#define DEBUG_BUS_GPR_CFG_P0_DATA__95_64 {DEBUG_BUS_GPR_CFG_P0_DATA__95_64_WORD, 0, 0xFFFFFFFF}

//  location: tt_instruction_thread name: debug_daisy_stop_gprs
//  name: gpr_data_debug_buses[2]
//  name: gpr_cfg_p0_data
#define DEBUG_BUS_GPR_CFG_P0_DATA__127_96_WORD {10, 2, 6, 0, 1, 0}
#define DEBUG_BUS_GPR_CFG_P0_DATA__127_96 {DEBUG_BUS_GPR_CFG_P0_DATA__127_96_WORD, 0, 0xFFFFFFFF}

//  location: tt_instruction_thread name: debug_daisy_stop_gprs
//  name: gpr_data_debug_buses[3]
//  name: l1_ret_wrdata
#define DEBUG_BUS_L1_RET_WRDATA__31_0_WORD {8, 2, 0, 0, 1, 0}
#define DEBUG_BUS_L1_RET_WRDATA__31_0 {DEBUG_BUS_L1_RET_WRDATA__31_0_WORD, 0, 0xFFFFFFFF}

//  location: tt_instruction_thread name: debug_daisy_stop_gprs
//  name: gpr_data_debug_buses[3]
//  name: l1_ret_wrdata
#define DEBUG_BUS_L1_RET_WRDATA__63_32_WORD {8, 2, 2, 0, 1, 0}
#define DEBUG_BUS_L1_RET_WRDATA__63_32 {DEBUG_BUS_L1_RET_WRDATA__63_32_WORD, 0, 0xFFFFFFFF}

//  location: tt_instruction_thread name: debug_daisy_stop_gprs
//  name: gpr_data_debug_buses[3]
//  name: l1_ret_wrdata
#define DEBUG_BUS_L1_RET_WRDATA__95_64_WORD {8, 2, 4, 0, 1, 0}
#define DEBUG_BUS_L1_RET_WRDATA__95_64 {DEBUG_BUS_L1_RET_WRDATA__95_64_WORD, 0, 0xFFFFFFFF}

//  location: tt_instruction_thread name: debug_daisy_stop_gprs
//  name: gpr_data_debug_buses[3]
//  name: l1_ret_wrdata
#define DEBUG_BUS_L1_RET_WRDATA__127_96_WORD {8, 2, 6, 0, 1, 0}
#define DEBUG_BUS_L1_RET_WRDATA__127_96 {DEBUG_BUS_L1_RET_WRDATA__127_96_WORD, 0, 0xFFFFFFFF}

//  location: tt_instruction_thread name: debug_daisy_stop_gprs
//  name: gpr_data_debug_buses[4]
//  name: thcon_p1_ret
#define DEBUG_BUS_THCON_P1_RET__31_0_WORD {6, 2, 0, 0, 1, 0}
#define DEBUG_BUS_THCON_P1_RET__31_0 {DEBUG_BUS_THCON_P1_RET__31_0_WORD, 0, 0xFFFFFFFF}

//  location: tt_instruction_thread name: debug_daisy_stop_gprs
//  name: gpr_data_debug_buses[4]
//  name: thcon_p1_ret
#define DEBUG_BUS_THCON_P1_RET__63_32_WORD {6, 2, 2, 0, 1, 0}
#define DEBUG_BUS_THCON_P1_RET__63_32 {DEBUG_BUS_THCON_P1_RET__63_32_WORD, 0, 0xFFFFFFFF}

//  location: tt_instruction_thread name: debug_daisy_stop_gprs
//  name: gpr_data_debug_buses[4]
//  name: thcon_p1_ret
#define DEBUG_BUS_THCON_P1_RET__95_64_WORD {6, 2, 4, 0, 1, 0}
#define DEBUG_BUS_THCON_P1_RET__95_64 {DEBUG_BUS_THCON_P1_RET__95_64_WORD, 0, 0xFFFFFFFF}

//  location: tt_instruction_thread name: debug_daisy_stop_gprs
//  name: gpr_data_debug_buses[4]
//  name: thcon_p1_ret
#define DEBUG_BUS_THCON_P1_RET__127_96_WORD {6, 2, 6, 0, 1, 0}
#define DEBUG_BUS_THCON_P1_RET__127_96 {DEBUG_BUS_THCON_P1_RET__127_96_WORD, 0, 0xFFFFFFFF}

//  location: tt_instruction_thread name: debug_daisy_stop_gprs
//  name: gpr_data_debug_buses[5]
//  name: cfg_gpr_p1_data
#define DEBUG_BUS_CFG_GPR_P1_DATA__31_0_WORD {4, 2, 0, 0, 1, 0}
#define DEBUG_BUS_CFG_GPR_P1_DATA__31_0 {DEBUG_BUS_CFG_GPR_P1_DATA__31_0_WORD, 0, 0xFFFFFFFF}

//  location: tt_instruction_thread name: debug_daisy_stop_gprs
//  name: gpr_data_debug_buses[5]
//  name: cfg_gpr_p1_data
#define DEBUG_BUS_CFG_GPR_P1_DATA__63_32_WORD {4, 2, 2, 0, 1, 0}
#define DEBUG_BUS_CFG_GPR_P1_DATA__63_32 {DEBUG_BUS_CFG_GPR_P1_DATA__63_32_WORD, 0, 0xFFFFFFFF}

//  location: tt_instruction_thread name: debug_daisy_stop_gprs
//  name: gpr_data_debug_buses[5]
//  name: cfg_gpr_p1_data
#define DEBUG_BUS_CFG_GPR_P1_DATA__95_64_WORD {4, 2, 4, 0, 1, 0}
#define DEBUG_BUS_CFG_GPR_P1_DATA__95_64 {DEBUG_BUS_CFG_GPR_P1_DATA__95_64_WORD, 0, 0xFFFFFFFF}

//  location: tt_instruction_thread name: debug_daisy_stop_gprs
//  name: gpr_data_debug_buses[5]
//  name: cfg_gpr_p1_data
#define DEBUG_BUS_CFG_GPR_P1_DATA__127_96_WORD {4, 2, 6, 0, 1, 0}
#define DEBUG_BUS_CFG_GPR_P1_DATA__127_96 {DEBUG_BUS_CFG_GPR_P1_DATA__127_96_WORD, 0, 0xFFFFFFFF}

//  location: tt_instruction_thread name: debug_daisy_stop_gprs
//  name: gpr_data_debug_buses[6]
//  name: i_risc_out_reg_wrdata
#define DEBUG_BUS_I_RISC_OUT_REG_WRDATA__31_0_WORD {2, 2, 0, 0, 1, 0}
#define DEBUG_BUS_I_RISC_OUT_REG_WRDATA__31_0 {DEBUG_BUS_I_RISC_OUT_REG_WRDATA__31_0_WORD, 0, 0xFFFFFFFF}

//  location: tt_instruction_thread name: debug_daisy_stop_gprs
//  name: gpr_data_debug_buses[6]
//  name: i_risc_out_reg_wrdata
#define DEBUG_BUS_I_RISC_OUT_REG_WRDATA__63_32_WORD {2, 2, 2, 0, 1, 0}
#define DEBUG_BUS_I_RISC_OUT_REG_WRDATA__63_32 {DEBUG_BUS_I_RISC_OUT_REG_WRDATA__63_32_WORD, 0, 0xFFFFFFFF}

//  location: tt_instruction_thread name: debug_daisy_stop_gprs
//  name: gpr_data_debug_buses[6]
//  name: i_risc_out_reg_wrdata
#define DEBUG_BUS_I_RISC_OUT_REG_WRDATA__95_64_WORD {2, 2, 4, 0, 1, 0}
#define DEBUG_BUS_I_RISC_OUT_REG_WRDATA__95_64 {DEBUG_BUS_I_RISC_OUT_REG_WRDATA__95_64_WORD, 0, 0xFFFFFFFF}

//  location: tt_instruction_thread name: debug_daisy_stop_gprs
//  name: gpr_data_debug_buses[6]
//  name: i_risc_out_reg_wrdata
#define DEBUG_BUS_I_RISC_OUT_REG_WRDATA__127_96_WORD {2, 2, 6, 0, 1, 0}
#define DEBUG_BUS_I_RISC_OUT_REG_WRDATA__127_96 {DEBUG_BUS_I_RISC_OUT_REG_WRDATA__127_96_WORD, 0, 0xFFFFFFFF}

//  location: tt_instruction_thread name: debug_daisy_stop_gprs
//  name: gpr_data_debug_buses[7]
//  name: o_risc_in_reg_rddata
#define DEBUG_BUS_O_RISC_IN_REG_RDDATA__31_0_WORD {0, 2, 0, 0, 1, 0}
#define DEBUG_BUS_O_RISC_IN_REG_RDDATA__31_0 {DEBUG_BUS_O_RISC_IN_REG_RDDATA__31_0_WORD, 0, 0xFFFFFFFF}

//  location: tt_instruction_thread name: debug_daisy_stop_gprs
//  name: gpr_data_debug_buses[7]
//  name: o_risc_in_reg_rddata
#define DEBUG_BUS_O_RISC_IN_REG_RDDATA__63_32_WORD {0, 2, 2, 0, 1, 0}
#define DEBUG_BUS_O_RISC_IN_REG_RDDATA__63_32 {DEBUG_BUS_O_RISC_IN_REG_RDDATA__63_32_WORD, 0, 0xFFFFFFFF}

//  location: tt_instruction_thread name: debug_daisy_stop_gprs
//  name: gpr_data_debug_buses[7]
//  name: o_risc_in_reg_rddata
#define DEBUG_BUS_O_RISC_IN_REG_RDDATA__95_64_WORD {0, 2, 4, 0, 1, 0}
#define DEBUG_BUS_O_RISC_IN_REG_RDDATA__95_64 {DEBUG_BUS_O_RISC_IN_REG_RDDATA__95_64_WORD, 0, 0xFFFFFFFF}

//  location: tt_instruction_thread name: debug_daisy_stop_gprs
//  name: gpr_data_debug_buses[7]
//  name: o_risc_in_reg_rddata
#define DEBUG_BUS_O_RISC_IN_REG_RDDATA__127_96_WORD {0, 2, 6, 0, 1, 0}
#define DEBUG_BUS_O_RISC_IN_REG_RDDATA__127_96 {DEBUG_BUS_O_RISC_IN_REG_RDDATA__127_96_WORD, 0, 0xFFFFFFFF}

//  location: tt_instruction_issue name: debug_daisy_stop_issue0
//  name: debug_issue0_in[3]
//  name: math_instrn
#define DEBUG_BUS_MATH_INSTRN__20_0_WORD {7, 3, 2, 0, 1, 0}
#define DEBUG_BUS_MATH_INSTRN__20_0 {DEBUG_BUS_MATH_INSTRN__20_0_WORD, 11, 0x1FFFFF}

//  location: tt_instruction_issue name: debug_daisy_stop_issue0
//  name: debug_issue0_in[3]
//  name: math_instrn
#define DEBUG_BUS_MATH_INSTRN__31_21_WORD {7, 3, 4, 0, 1, 0}
#define DEBUG_BUS_MATH_INSTRN__31_21 {DEBUG_BUS_MATH_INSTRN__31_21_WORD, 0, 0x7FF}

//  location: tt_instruction_issue name: debug_daisy_stop_issue0
//  name: debug_issue0_in[3]
//  name: math_winner_thread
#define DEBUG_BUS_MATH_WINNER_THREAD_WORD {7, 3, 2, 0, 1, 0}
#define DEBUG_BUS_MATH_WINNER_THREAD {DEBUG_BUS_MATH_WINNER_THREAD_WORD, 9, 0x3}

//  location: tt_instruction_issue name: debug_daisy_stop_issue0
//  name: debug_issue0_in[3]
//  name: math_winner
#define DEBUG_BUS_MATH_WINNER_WORD {7, 3, 2, 0, 1, 0}
#define DEBUG_BUS_MATH_WINNER {DEBUG_BUS_MATH_WINNER_WORD, 6, 0x7}

//  location: tt_instruction_issue name: debug_daisy_stop_issue0
//  name: debug_issue0_in[3]
//  name: s0_fidelity_phase_d
#define DEBUG_BUS_S0_FIDELITY_PHASE_D_WORD {7, 3, 2, 0, 1, 0}
#define DEBUG_BUS_S0_FIDELITY_PHASE_D {DEBUG_BUS_S0_FIDELITY_PHASE_D_WORD, 4, 0x3}

//  location: tt_instruction_issue name: debug_daisy_stop_issue0
//  name: debug_issue0_in[3]
//  name: s0_srca_reg_addr_d
#define DEBUG_BUS_S0_SRCA_REG_ADDR_D__1_0_WORD {7, 3, 0, 0, 1, 0}
#define DEBUG_BUS_S0_SRCA_REG_ADDR_D__1_0 {DEBUG_BUS_S0_SRCA_REG_ADDR_D__1_0_WORD, 30, 0x3}

//  location: tt_instruction_issue name: debug_daisy_stop_issue0
//  name: debug_issue0_in[3]
//  name: s0_srca_reg_addr_d
#define DEBUG_BUS_S0_SRCA_REG_ADDR_D__5_2_WORD {7, 3, 2, 0, 1, 0}
#define DEBUG_BUS_S0_SRCA_REG_ADDR_D__5_2 {DEBUG_BUS_S0_SRCA_REG_ADDR_D__5_2_WORD, 0, 0xF}

//  location: tt_instruction_issue name: debug_daisy_stop_issue0
//  name: debug_issue0_in[3]
//  name: s0_srcb_reg_addr_d
#define DEBUG_BUS_S0_SRCB_REG_ADDR_D_WORD {7, 3, 0, 0, 1, 0}
#define DEBUG_BUS_S0_SRCB_REG_ADDR_D {DEBUG_BUS_S0_SRCB_REG_ADDR_D_WORD, 24, 0x3F}

//  location: tt_instruction_issue name: debug_daisy_stop_issue0
//  name: debug_issue0_in[3]
//  name: s0_dst_reg_addr_d
#define DEBUG_BUS_S0_DST_REG_ADDR_D_WORD {7, 3, 0, 0, 1, 0}
#define DEBUG_BUS_S0_DST_REG_ADDR_D {DEBUG_BUS_S0_DST_REG_ADDR_D_WORD, 14, 0x3FF}

//  location: tt_instruction_issue name: debug_daisy_stop_issue0
//  name: debug_issue0_in[3]
//  name: s0_mov_dst_reg_addr_d
#define DEBUG_BUS_S0_MOV_DST_REG_ADDR_D_WORD {7, 3, 0, 0, 1, 0}
#define DEBUG_BUS_S0_MOV_DST_REG_ADDR_D {DEBUG_BUS_S0_MOV_DST_REG_ADDR_D_WORD, 4, 0x3FF}

//  location: tt_instruction_issue name: debug_daisy_stop_issue0
//  name: debug_issue0_in[3]
//  name: s0_dec_instr_single_output_row_d
#define DEBUG_BUS_S0_DEC_INSTR_SINGLE_OUTPUT_ROW_D_WORD {7, 3, 0, 0, 1, 0}
#define DEBUG_BUS_S0_DEC_INSTR_SINGLE_OUTPUT_ROW_D {DEBUG_BUS_S0_DEC_INSTR_SINGLE_OUTPUT_ROW_D_WORD, 3, 0x1}

//  location: tt_instruction_issue name: debug_daisy_stop_issue0
//  name: debug_issue0_in[3]
//  name: fpu_rd_data_required_d
#define DEBUG_BUS_FPU_RD_DATA_REQUIRED_D_WORD {7, 3, 0, 0, 1, 0}
#define DEBUG_BUS_FPU_RD_DATA_REQUIRED_D {DEBUG_BUS_FPU_RD_DATA_REQUIRED_D_WORD, 0, 0x7}

//  location: tt_instruction_issue name: debug_daisy_stop_issue0
//  name: debug_issue0_in[3]
//  name: tdma_srca_regif_unpack_src_reg_set_upd
#define DEBUG_BUS_TDMA_SRCA_REGIF_UNPACK_SRC_REG_SET_UPD_WORD {6, 3, 6, 0, 1, 0}
#define DEBUG_BUS_TDMA_SRCA_REGIF_UNPACK_SRC_REG_SET_UPD {DEBUG_BUS_TDMA_SRCA_REGIF_UNPACK_SRC_REG_SET_UPD_WORD, 4, 0x1}

//  location: tt_instruction_issue name: debug_daisy_stop_issue0
//  name: debug_issue0_in[3]
//  name: dma_srca_wr_port_avail
#define DEBUG_BUS_DEBUG_ISSUE0_IN_3__DMA_SRCA_WR_PORT_AVAIL_WORD {6, 3, 6, 0, 1, 0}
#define DEBUG_BUS_DEBUG_ISSUE0_IN_3__DMA_SRCA_WR_PORT_AVAIL {DEBUG_BUS_DEBUG_ISSUE0_IN_3__DMA_SRCA_WR_PORT_AVAIL_WORD, 3, 0x1}

//  location: tt_instruction_issue name: debug_daisy_stop_issue0
//  name: debug_issue0_in[3]
//  name: srca_write_ready
#define DEBUG_BUS_DEBUG_ISSUE0_IN_3__SRCA_WRITE_READY_WORD {6, 3, 6, 0, 1, 0}
#define DEBUG_BUS_DEBUG_ISSUE0_IN_3__SRCA_WRITE_READY {DEBUG_BUS_DEBUG_ISSUE0_IN_3__SRCA_WRITE_READY_WORD, 2, 0x1}

//  location: tt_instruction_issue name: debug_daisy_stop_issue0
//  name: debug_issue0_in[3]
//  name: tdma_srca_regif_unpack_if_sel
#define DEBUG_BUS_TDMA_SRCA_REGIF_UNPACK_IF_SEL_WORD {6, 3, 6, 0, 1, 0}
#define DEBUG_BUS_TDMA_SRCA_REGIF_UNPACK_IF_SEL {DEBUG_BUS_TDMA_SRCA_REGIF_UNPACK_IF_SEL_WORD, 1, 0x1}

//  location: tt_instruction_issue name: debug_daisy_stop_issue0
//  name: debug_issue0_in[3]
//  name: tdma_srca_regif_state_id
#define DEBUG_BUS_TDMA_SRCA_REGIF_STATE_ID_WORD {6, 3, 6, 0, 1, 0}
#define DEBUG_BUS_TDMA_SRCA_REGIF_STATE_ID {DEBUG_BUS_TDMA_SRCA_REGIF_STATE_ID_WORD, 0, 0x1}

//  location: tt_instruction_issue name: debug_daisy_stop_issue0
//  name: debug_issue0_in[3]
//  name: tdma_srca_regif_addr
#define DEBUG_BUS_TDMA_SRCA_REGIF_ADDR_WORD {6, 3, 4, 0, 1, 0}
#define DEBUG_BUS_TDMA_SRCA_REGIF_ADDR {DEBUG_BUS_TDMA_SRCA_REGIF_ADDR_WORD, 18, 0x3FFF}

//  location: tt_instruction_issue name: debug_daisy_stop_issue0
//  name: debug_issue0_in[3]
//  name: tdma_srca_regif_wren
#define DEBUG_BUS_TDMA_SRCA_REGIF_WREN__3_0_WORD {6, 3, 4, 0, 1, 0}
#define DEBUG_BUS_TDMA_SRCA_REGIF_WREN__3_0 {DEBUG_BUS_TDMA_SRCA_REGIF_WREN__3_0_WORD, 14, 0xF}

//  location: tt_instruction_issue name: debug_daisy_stop_issue0
//  name: debug_issue0_in[3]
//  name: tdma_srca_regif_thread_id
#define DEBUG_BUS_TDMA_SRCA_REGIF_THREAD_ID_WORD {6, 3, 4, 0, 1, 0}
#define DEBUG_BUS_TDMA_SRCA_REGIF_THREAD_ID {DEBUG_BUS_TDMA_SRCA_REGIF_THREAD_ID_WORD, 12, 0x3}

//  location: tt_instruction_issue name: debug_daisy_stop_issue0
//  name: debug_issue0_in[3]
//  name: tdma_srca_regif_out_data_format
#define DEBUG_BUS_TDMA_SRCA_REGIF_OUT_DATA_FORMAT_WORD {6, 3, 4, 0, 1, 0}
#define DEBUG_BUS_TDMA_SRCA_REGIF_OUT_DATA_FORMAT {DEBUG_BUS_TDMA_SRCA_REGIF_OUT_DATA_FORMAT_WORD, 8, 0xF}

//  location: tt_instruction_issue name: debug_daisy_stop_issue0
//  name: debug_issue0_in[3]
//  name: tdma_srca_regif_data_format
#define DEBUG_BUS_TDMA_SRCA_REGIF_DATA_FORMAT_WORD {6, 3, 4, 0, 1, 0}
#define DEBUG_BUS_TDMA_SRCA_REGIF_DATA_FORMAT {DEBUG_BUS_TDMA_SRCA_REGIF_DATA_FORMAT_WORD, 4, 0xF}

//  location: tt_instruction_issue name: debug_daisy_stop_issue0
//  name: debug_issue0_in[3]
//  name: tdma_srca_regif_shift_amount
#define DEBUG_BUS_TDMA_SRCA_REGIF_SHIFT_AMOUNT_WORD {6, 3, 4, 0, 1, 0}
#define DEBUG_BUS_TDMA_SRCA_REGIF_SHIFT_AMOUNT {DEBUG_BUS_TDMA_SRCA_REGIF_SHIFT_AMOUNT_WORD, 0, 0xF}

//  location: tt_instruction_issue name: debug_daisy_stop_issue0
//  name: debug_issue0_in[3]
//  name: tdma_srcb_regif_unpack_src_reg_set_upd
#define DEBUG_BUS_TDMA_SRCB_REGIF_UNPACK_SRC_REG_SET_UPD_WORD {6, 3, 2, 0, 1, 0}
#define DEBUG_BUS_TDMA_SRCB_REGIF_UNPACK_SRC_REG_SET_UPD {DEBUG_BUS_TDMA_SRCB_REGIF_UNPACK_SRC_REG_SET_UPD_WORD, 31, 0x1}

//  location: tt_instruction_issue name: debug_daisy_stop_issue0
//  name: debug_issue0_in[3]
//  name: dma_srcb_wr_port_avail
#define DEBUG_BUS_DEBUG_ISSUE0_IN_3__DMA_SRCB_WR_PORT_AVAIL_WORD {6, 3, 2, 0, 1, 0}
#define DEBUG_BUS_DEBUG_ISSUE0_IN_3__DMA_SRCB_WR_PORT_AVAIL {DEBUG_BUS_DEBUG_ISSUE0_IN_3__DMA_SRCB_WR_PORT_AVAIL_WORD, 30, 0x1}

//  location: tt_instruction_issue name: debug_daisy_stop_issue0
//  name: debug_issue0_in[3]
//  name: srcb_write_ready
#define DEBUG_BUS_DEBUG_ISSUE0_IN_3__SRCB_WRITE_READY_WORD {6, 3, 2, 0, 1, 0}
#define DEBUG_BUS_DEBUG_ISSUE0_IN_3__SRCB_WRITE_READY {DEBUG_BUS_DEBUG_ISSUE0_IN_3__SRCB_WRITE_READY_WORD, 29, 0x1}

//  location: tt_instruction_issue name: debug_daisy_stop_issue0
//  name: debug_issue0_in[3]
//  name: tdma_srcb_regif_state_id
#define DEBUG_BUS_TDMA_SRCB_REGIF_STATE_ID_WORD {6, 3, 2, 0, 1, 0}
#define DEBUG_BUS_TDMA_SRCB_REGIF_STATE_ID {DEBUG_BUS_TDMA_SRCB_REGIF_STATE_ID_WORD, 28, 0x1}

//  location: tt_instruction_issue name: debug_daisy_stop_issue0
//  name: debug_issue0_in[3]
//  name: tdma_srcb_regif_addr
#define DEBUG_BUS_TDMA_SRCB_REGIF_ADDR_WORD {6, 3, 2, 0, 1, 0}
#define DEBUG_BUS_TDMA_SRCB_REGIF_ADDR {DEBUG_BUS_TDMA_SRCB_REGIF_ADDR_WORD, 14, 0x3FFF}

//  location: tt_instruction_issue name: debug_daisy_stop_issue0
//  name: debug_issue0_in[3]
//  name: tdma_srcb_regif_wren
#define DEBUG_BUS_TDMA_SRCB_REGIF_WREN__3_0_WORD {6, 3, 2, 0, 1, 0}
#define DEBUG_BUS_TDMA_SRCB_REGIF_WREN__3_0 {DEBUG_BUS_TDMA_SRCB_REGIF_WREN__3_0_WORD, 10, 0xF}

//  location: tt_instruction_issue name: debug_daisy_stop_issue0
//  name: debug_issue0_in[3]
//  name: tdma_srcb_regif_thread_id
#define DEBUG_BUS_TDMA_SRCB_REGIF_THREAD_ID_WORD {6, 3, 2, 0, 1, 0}
#define DEBUG_BUS_TDMA_SRCB_REGIF_THREAD_ID {DEBUG_BUS_TDMA_SRCB_REGIF_THREAD_ID_WORD, 8, 0x3}

//  location: tt_instruction_issue name: debug_daisy_stop_issue0
//  name: debug_issue0_in[3]
//  name: tdma_srcb_regif_out_data_format
#define DEBUG_BUS_TDMA_SRCB_REGIF_OUT_DATA_FORMAT_WORD {6, 3, 2, 0, 1, 0}
#define DEBUG_BUS_TDMA_SRCB_REGIF_OUT_DATA_FORMAT {DEBUG_BUS_TDMA_SRCB_REGIF_OUT_DATA_FORMAT_WORD, 4, 0xF}

//  location: tt_instruction_issue name: debug_daisy_stop_issue0
//  name: debug_issue0_in[3]
//  name: tdma_srcb_regif_data_format
#define DEBUG_BUS_TDMA_SRCB_REGIF_DATA_FORMAT_WORD {6, 3, 2, 0, 1, 0}
#define DEBUG_BUS_TDMA_SRCB_REGIF_DATA_FORMAT {DEBUG_BUS_TDMA_SRCB_REGIF_DATA_FORMAT_WORD, 0, 0xF}

//  location: tt_instruction_issue name: debug_daisy_stop_issue0
//  name: debug_issue0_in[3]
//  name: tdma_dstac_regif_rden_raw
#define DEBUG_BUS_TDMA_DSTAC_REGIF_RDEN_RAW_WORD {6, 3, 0, 0, 1, 0}
#define DEBUG_BUS_TDMA_DSTAC_REGIF_RDEN_RAW {DEBUG_BUS_TDMA_DSTAC_REGIF_RDEN_RAW_WORD, 7, 0x1}

//  location: tt_instruction_issue name: debug_daisy_stop_issue0
//  name: debug_issue0_in[3]
//  name: tdma_dstac_regif_thread_id
#define DEBUG_BUS_TDMA_DSTAC_REGIF_THREAD_ID_WORD {6, 3, 0, 0, 1, 0}
#define DEBUG_BUS_TDMA_DSTAC_REGIF_THREAD_ID {DEBUG_BUS_TDMA_DSTAC_REGIF_THREAD_ID_WORD, 5, 0x3}

//  location: tt_instruction_issue name: debug_daisy_stop_issue0
//  name: debug_issue0_in[3]
//  name: tdma_dstac_regif_data_format
#define DEBUG_BUS_TDMA_DSTAC_REGIF_DATA_FORMAT_WORD {6, 3, 0, 0, 1, 0}
#define DEBUG_BUS_TDMA_DSTAC_REGIF_DATA_FORMAT {DEBUG_BUS_TDMA_DSTAC_REGIF_DATA_FORMAT_WORD, 1, 0xF}

//  location: tt_instruction_issue name: debug_daisy_stop_issue0
//  name: debug_issue0_in[3]
//  name: dstac_regif_tdma_reqif_ready
#define DEBUG_BUS_DSTAC_REGIF_TDMA_REQIF_READY_WORD {6, 3, 0, 0, 1, 0}
#define DEBUG_BUS_DSTAC_REGIF_TDMA_REQIF_READY {DEBUG_BUS_DSTAC_REGIF_TDMA_REQIF_READY_WORD, 0, 0x1}

//  location: tt_instruction_issue name: debug_daisy_stop_issue0
//  name: debug_issue0_in[2]
//  name: tdma_pack_busy
#define DEBUG_BUS_TDMA_PACK_BUSY__0_0_WORD {5, 3, 6, 0, 1, 0}
#define DEBUG_BUS_TDMA_PACK_BUSY__0_0 {DEBUG_BUS_TDMA_PACK_BUSY__0_0_WORD, 12, 0x1}

//  location: tt_instruction_issue name: debug_daisy_stop_issue0
//  name: debug_issue0_in[2]
//  name: tdma_unpack_busy
#define DEBUG_BUS_TDMA_UNPACK_BUSY_WORD {5, 3, 6, 0, 1, 0}
#define DEBUG_BUS_TDMA_UNPACK_BUSY {DEBUG_BUS_TDMA_UNPACK_BUSY_WORD, 6, 0x3F}

//  location: tt_instruction_issue name: debug_daisy_stop_issue0
//  name: debug_issue0_in[2]
//  name: tdma_tc_busy
#define DEBUG_BUS_TDMA_TC_BUSY_WORD {5, 3, 6, 0, 1, 0}
#define DEBUG_BUS_TDMA_TC_BUSY {DEBUG_BUS_TDMA_TC_BUSY_WORD, 3, 0x7}

//  location: tt_instruction_issue name: debug_daisy_stop_issue0
//  name: debug_issue0_in[2]
//  name: tdma_move_busy
#define DEBUG_BUS_TDMA_MOVE_BUSY_WORD {5, 3, 6, 0, 1, 0}
#define DEBUG_BUS_TDMA_MOVE_BUSY {DEBUG_BUS_TDMA_MOVE_BUSY_WORD, 2, 0x1}

//  location: tt_instruction_issue name: debug_daisy_stop_issue0
//  name: debug_issue0_in[2]
//  name: tdma_xsearch_busy
#define DEBUG_BUS_TDMA_XSEARCH_BUSY__3_0_WORD {5, 3, 4, 0, 1, 0}
#define DEBUG_BUS_TDMA_XSEARCH_BUSY__3_0 {DEBUG_BUS_TDMA_XSEARCH_BUSY__3_0_WORD, 28, 0xF}

//  location: tt_instruction_issue name: debug_daisy_stop_issue0
//  name: debug_issue0_in[2]
//  name: tdma_xsearch_busy
#define DEBUG_BUS_TDMA_XSEARCH_BUSY__5_4_WORD {5, 3, 6, 0, 1, 0}
#define DEBUG_BUS_TDMA_XSEARCH_BUSY__5_4 {DEBUG_BUS_TDMA_XSEARCH_BUSY__5_4_WORD, 0, 0x3}

//  location: tt_instruction_issue name: debug_daisy_stop_issue0
//  name: debug_issue0_in[2]
//  name: i_cg_regblocks_en
#define DEBUG_BUS_I_CG_REGBLOCKS_EN_WORD {5, 3, 4, 0, 1, 0}
#define DEBUG_BUS_I_CG_REGBLOCKS_EN {DEBUG_BUS_I_CG_REGBLOCKS_EN_WORD, 25, 0x1}

//  location: tt_instruction_issue name: debug_daisy_stop_issue0
//  name: debug_issue0_in[2]
//  name: cg_regblocks_busy_d
#define DEBUG_BUS_CG_REGBLOCKS_BUSY_D_WORD {5, 3, 4, 0, 1, 0}
#define DEBUG_BUS_CG_REGBLOCKS_BUSY_D {DEBUG_BUS_CG_REGBLOCKS_BUSY_D_WORD, 24, 0x1}

//  location: tt_instruction_issue name: debug_daisy_stop_issue0
//  name: debug_issue0_in[2]
//  name: srcb_reg_addr
#define DEBUG_BUS_SRCB_REG_ADDR__4_0_WORD {5, 3, 4, 0, 1, 0}
#define DEBUG_BUS_SRCB_REG_ADDR__4_0 {DEBUG_BUS_SRCB_REG_ADDR__4_0_WORD, 19, 0x1F}

//  location: tt_instruction_issue name: debug_daisy_stop_issue0
//  name: debug_issue0_in[2]
//  name: srcb_reg_addr_d
#define DEBUG_BUS_SRCB_REG_ADDR_D__4_0_WORD {5, 3, 4, 0, 1, 0}
#define DEBUG_BUS_SRCB_REG_ADDR_D__4_0 {DEBUG_BUS_SRCB_REG_ADDR_D__4_0_WORD, 14, 0x1F}

//  location: tt_instruction_issue name: debug_daisy_stop_issue0
//  name: debug_issue0_in[2]
//  name: fpu_output_mode_d
#define DEBUG_BUS_FPU_OUTPUT_MODE_D_WORD {5, 3, 4, 0, 1, 0}
#define DEBUG_BUS_FPU_OUTPUT_MODE_D {DEBUG_BUS_FPU_OUTPUT_MODE_D_WORD, 11, 0x7}

//  location: tt_instruction_issue name: debug_daisy_stop_issue0
//  name: debug_issue0_in[2]
//  name: fpu_output_mode
#define DEBUG_BUS_FPU_OUTPUT_MODE_WORD {5, 3, 4, 0, 1, 0}
#define DEBUG_BUS_FPU_OUTPUT_MODE {DEBUG_BUS_FPU_OUTPUT_MODE_WORD, 8, 0x7}

//  location: tt_instruction_issue name: debug_daisy_stop_issue0
//  name: debug_issue0_in[2]
//  name: srcb_single_row_rd_mode_d
#define DEBUG_BUS_SRCB_SINGLE_ROW_RD_MODE_D_WORD {5, 3, 4, 0, 1, 0}
#define DEBUG_BUS_SRCB_SINGLE_ROW_RD_MODE_D {DEBUG_BUS_SRCB_SINGLE_ROW_RD_MODE_D_WORD, 7, 0x1}

//  location: tt_instruction_issue name: debug_daisy_stop_issue0
//  name: debug_issue0_in[2]
//  name: srcb_single_row_rd_mode
#define DEBUG_BUS_SRCB_SINGLE_ROW_RD_MODE_WORD {5, 3, 4, 0, 1, 0}
#define DEBUG_BUS_SRCB_SINGLE_ROW_RD_MODE {DEBUG_BUS_SRCB_SINGLE_ROW_RD_MODE_WORD, 6, 0x1}

//  location: tt_instruction_issue name: debug_daisy_stop_issue0
//  name: debug_issue0_in[2]
//  name: dest_apply_relu
#define DEBUG_BUS_DEST_APPLY_RELU_WORD {5, 3, 0, 0, 1, 0}
#define DEBUG_BUS_DEST_APPLY_RELU {DEBUG_BUS_DEST_APPLY_RELU_WORD, 5, 0x3}

//  location: tt_instruction_issue name: debug_daisy_stop_issue0
//  name: debug_issue0_in[2]
//  name: tdma_dstac_regif_state_id
#define DEBUG_BUS_TDMA_DSTAC_REGIF_STATE_ID_WORD {5, 3, 0, 0, 1, 0}
#define DEBUG_BUS_TDMA_DSTAC_REGIF_STATE_ID {DEBUG_BUS_TDMA_DSTAC_REGIF_STATE_ID_WORD, 4, 0x1}

//  location: tt_instruction_issue name: debug_daisy_stop_issue0
//  name: debug_issue0_in[2]
//  name: relu_thresh
#define DEBUG_BUS_RELU_THRESH__11_0_WORD {4, 3, 6, 0, 1, 0}
#define DEBUG_BUS_RELU_THRESH__11_0 {DEBUG_BUS_RELU_THRESH__11_0_WORD, 20, 0xFFF}

//  location: tt_instruction_issue name: debug_daisy_stop_issue0
//  name: debug_issue0_in[2]
//  name: relu_thresh
#define DEBUG_BUS_RELU_THRESH__15_12_WORD {5, 3, 0, 0, 1, 0}
#define DEBUG_BUS_RELU_THRESH__15_12 {DEBUG_BUS_RELU_THRESH__15_12_WORD, 0, 0xF}

//  location: tt_instruction_issue name: debug_daisy_stop_issue0
//  name: debug_issue0_in[2]
//  name: dest_offset_state_id
#define DEBUG_BUS_DEST_OFFSET_STATE_ID_WORD {4, 3, 6, 0, 1, 0}
#define DEBUG_BUS_DEST_OFFSET_STATE_ID {DEBUG_BUS_DEST_OFFSET_STATE_ID_WORD, 19, 0x1}

//  location: tt_instruction_issue name: debug_daisy_stop_issue0
//  name: debug_issue0_in[2]
//  name: dma_dest_offset_apply_en
#define DEBUG_BUS_DMA_DEST_OFFSET_APPLY_EN_WORD {4, 3, 6, 0, 1, 0}
#define DEBUG_BUS_DMA_DEST_OFFSET_APPLY_EN {DEBUG_BUS_DMA_DEST_OFFSET_APPLY_EN_WORD, 18, 0x1}

//  location: tt_instruction_issue name: debug_daisy_stop_issue0
//  name: debug_issue0_in[2]
//  name: srca_fpu_output_alu_format_s1
#define DEBUG_BUS_SRCA_FPU_OUTPUT_ALU_FORMAT_S1_WORD {4, 3, 6, 0, 1, 0}
#define DEBUG_BUS_SRCA_FPU_OUTPUT_ALU_FORMAT_S1 {DEBUG_BUS_SRCA_FPU_OUTPUT_ALU_FORMAT_S1_WORD, 14, 0xF}

//  location: tt_instruction_issue name: debug_daisy_stop_issue0
//  name: debug_issue0_in[2]
//  name: srcb_fpu_output_alu_format_s1
#define DEBUG_BUS_SRCB_FPU_OUTPUT_ALU_FORMAT_S1_WORD {4, 3, 6, 0, 1, 0}
#define DEBUG_BUS_SRCB_FPU_OUTPUT_ALU_FORMAT_S1 {DEBUG_BUS_SRCB_FPU_OUTPUT_ALU_FORMAT_S1_WORD, 10, 0xF}

//  location: tt_instruction_issue name: debug_daisy_stop_issue0
//  name: debug_issue0_in[2]
//  name: dest_fpu_output_alu_format_s1
#define DEBUG_BUS_DEST_FPU_OUTPUT_ALU_FORMAT_S1_WORD {4, 3, 6, 0, 1, 0}
#define DEBUG_BUS_DEST_FPU_OUTPUT_ALU_FORMAT_S1 {DEBUG_BUS_DEST_FPU_OUTPUT_ALU_FORMAT_S1_WORD, 6, 0xF}

//  location: tt_instruction_issue name: debug_daisy_stop_issue0
//  name: debug_issue0_in[2]
//  name: dest_dma_output_alu_format
#define DEBUG_BUS_DEST_DMA_OUTPUT_ALU_FORMAT__3_0_WORD {4, 3, 6, 0, 1, 0}
#define DEBUG_BUS_DEST_DMA_OUTPUT_ALU_FORMAT__3_0 {DEBUG_BUS_DEST_DMA_OUTPUT_ALU_FORMAT__3_0_WORD, 2, 0xF}

//  location: tt_instruction_issue name: debug_daisy_stop_issue0
//  name: debug_issue0_in[2]
//  name: srca_gate_src_pipeline
#define DEBUG_BUS_SRCA_GATE_SRC_PIPELINE__0_0_WORD {4, 3, 4, 0, 1, 0}
#define DEBUG_BUS_SRCA_GATE_SRC_PIPELINE__0_0 {DEBUG_BUS_SRCA_GATE_SRC_PIPELINE__0_0_WORD, 28, 0x1}

//  location: tt_instruction_issue name: debug_daisy_stop_issue0
//  name: debug_issue0_in[2]
//  name: srca_gate_src_pipeline
#define DEBUG_BUS_SRCA_GATE_SRC_PIPELINE__1_1_WORD {4, 3, 4, 0, 1, 0}
#define DEBUG_BUS_SRCA_GATE_SRC_PIPELINE__1_1 {DEBUG_BUS_SRCA_GATE_SRC_PIPELINE__1_1_WORD, 27, 0x1}

//  location: tt_instruction_issue name: debug_daisy_stop_issue0
//  name: debug_issue0_in[2]
//  name: srcb_gate_src_pipeline
#define DEBUG_BUS_SRCB_GATE_SRC_PIPELINE__0_0_WORD {4, 3, 4, 0, 1, 0}
#define DEBUG_BUS_SRCB_GATE_SRC_PIPELINE__0_0 {DEBUG_BUS_SRCB_GATE_SRC_PIPELINE__0_0_WORD, 26, 0x1}

//  location: tt_instruction_issue name: debug_daisy_stop_issue0
//  name: debug_issue0_in[2]
//  name: srcb_gate_src_pipeline
#define DEBUG_BUS_SRCB_GATE_SRC_PIPELINE__1_1_WORD {4, 3, 4, 0, 1, 0}
#define DEBUG_BUS_SRCB_GATE_SRC_PIPELINE__1_1 {DEBUG_BUS_SRCB_GATE_SRC_PIPELINE__1_1_WORD, 25, 0x1}

//  location: tt_instruction_issue name: debug_daisy_stop_issue0
//  name: debug_issue0_in[2]
//  name: squash_alu_instrn
#define DEBUG_BUS_SQUASH_ALU_INSTRN_WORD {4, 3, 4, 0, 1, 0}
#define DEBUG_BUS_SQUASH_ALU_INSTRN {DEBUG_BUS_SQUASH_ALU_INSTRN_WORD, 24, 0x1}

//  location: tt_instruction_issue name: debug_daisy_stop_issue0
//  name: debug_issue0_in[2]
//  name: alu_inst_issue_ready
#define DEBUG_BUS_ALU_INST_ISSUE_READY_WORD {4, 3, 4, 0, 1, 0}
#define DEBUG_BUS_ALU_INST_ISSUE_READY {DEBUG_BUS_ALU_INST_ISSUE_READY_WORD, 23, 0x1}

//  location: tt_instruction_issue name: debug_daisy_stop_issue0
//  name: debug_issue0_in[2]
//  name: alu_inst_issue_ready_src
#define DEBUG_BUS_ALU_INST_ISSUE_READY_SRC_WORD {4, 3, 4, 0, 1, 0}
#define DEBUG_BUS_ALU_INST_ISSUE_READY_SRC {DEBUG_BUS_ALU_INST_ISSUE_READY_SRC_WORD, 22, 0x1}

//  location: tt_instruction_issue name: debug_daisy_stop_issue0
//  name: debug_issue0_in[2]
//  name: sfpu_inst_issue_ready_s1
#define DEBUG_BUS_SFPU_INST_ISSUE_READY_S1_WORD {4, 3, 4, 0, 1, 0}
#define DEBUG_BUS_SFPU_INST_ISSUE_READY_S1 {DEBUG_BUS_SFPU_INST_ISSUE_READY_S1_WORD, 21, 0x1}

//  location: tt_instruction_issue name: debug_daisy_stop_issue0
//  name: debug_issue0_in[2]
//  name: sfpu_inst_store_ready_s1
#define DEBUG_BUS_SFPU_INST_STORE_READY_S1_WORD {4, 3, 4, 0, 1, 0}
#define DEBUG_BUS_SFPU_INST_STORE_READY_S1 {DEBUG_BUS_SFPU_INST_STORE_READY_S1_WORD, 20, 0x1}

//  location: tt_instruction_issue name: debug_daisy_stop_issue0
//  name: debug_issue0_in[2]
//  name: lddest_instr_valid
#define DEBUG_BUS_LDDEST_INSTR_VALID_WORD {4, 3, 4, 0, 1, 0}
#define DEBUG_BUS_LDDEST_INSTR_VALID {DEBUG_BUS_LDDEST_INSTR_VALID_WORD, 19, 0x1}

//  location: tt_instruction_issue name: debug_daisy_stop_issue0
//  name: debug_issue0_in[2]
//  name: rddest_instr_valid
#define DEBUG_BUS_RDDEST_INSTR_VALID_WORD {4, 3, 4, 0, 1, 0}
#define DEBUG_BUS_RDDEST_INSTR_VALID {DEBUG_BUS_RDDEST_INSTR_VALID_WORD, 18, 0x1}

//  location: tt_instruction_issue name: debug_daisy_stop_issue0
//  name: debug_issue0_in[2]
//  name: dest_reg_deps_scoreboard_bank_pending
#define DEBUG_BUS_DEST_REG_DEPS_SCOREBOARD_BANK_PENDING_WORD {4, 3, 4, 0, 1, 0}
#define DEBUG_BUS_DEST_REG_DEPS_SCOREBOARD_BANK_PENDING {DEBUG_BUS_DEST_REG_DEPS_SCOREBOARD_BANK_PENDING_WORD, 16, 0x3}

//  location: tt_instruction_issue name: debug_daisy_stop_issue0
//  name: debug_issue0_in[2]
//  name: dest_reg_deps_scoreboard_something_pending
#define DEBUG_BUS_DEST_REG_DEPS_SCOREBOARD_SOMETHING_PENDING_WORD {4, 3, 4, 0, 1, 0}
#define DEBUG_BUS_DEST_REG_DEPS_SCOREBOARD_SOMETHING_PENDING {DEBUG_BUS_DEST_REG_DEPS_SCOREBOARD_SOMETHING_PENDING_WORD, 15, 0x1}

//  location: tt_instruction_issue name: debug_daisy_stop_issue0
//  name: debug_issue0_in[2]
//  name: dest_reg_deps_scoreboard_pending_thread
#define DEBUG_BUS_DEST_REG_DEPS_SCOREBOARD_PENDING_THREAD_WORD {4, 3, 4, 0, 1, 0}
#define DEBUG_BUS_DEST_REG_DEPS_SCOREBOARD_PENDING_THREAD {DEBUG_BUS_DEST_REG_DEPS_SCOREBOARD_PENDING_THREAD_WORD, 12, 0x7}

//  location: tt_instruction_issue name: debug_daisy_stop_issue0
//  name: debug_issue0_in[2]
//  name: all_buffers_empty
#define DEBUG_BUS_ALL_BUFFERS_EMPTY_WORD {4, 3, 4, 0, 1, 0}
#define DEBUG_BUS_ALL_BUFFERS_EMPTY {DEBUG_BUS_ALL_BUFFERS_EMPTY_WORD, 9, 0x7}

//  location: tt_instruction_issue name: debug_daisy_stop_issue0
//  name: debug_issue0_in[2]
//  name: dest_reg_deps_scoreboard_stall
#define DEBUG_BUS_DEST_REG_DEPS_SCOREBOARD_STALL_WORD {4, 3, 4, 0, 1, 0}
#define DEBUG_BUS_DEST_REG_DEPS_SCOREBOARD_STALL {DEBUG_BUS_DEST_REG_DEPS_SCOREBOARD_STALL_WORD, 8, 0x1}

//  location: tt_instruction_issue name: debug_daisy_stop_issue0
//  name: debug_issue0_in[2]
//  name: dest_wr_port_stall
#define DEBUG_BUS_DEST_WR_PORT_STALL_WORD {4, 3, 4, 0, 1, 0}
#define DEBUG_BUS_DEST_WR_PORT_STALL {DEBUG_BUS_DEST_WR_PORT_STALL_WORD, 7, 0x1}

//  location: tt_instruction_issue name: debug_daisy_stop_issue0
//  name: debug_issue0_in[2]
//  name: dest_fpu_rd_port_stall
#define DEBUG_BUS_DEST_FPU_RD_PORT_STALL_WORD {4, 3, 4, 0, 1, 0}
#define DEBUG_BUS_DEST_FPU_RD_PORT_STALL {DEBUG_BUS_DEST_FPU_RD_PORT_STALL_WORD, 6, 0x1}

//  location: tt_instruction_issue name: debug_daisy_stop_issue0
//  name: debug_issue0_in[2]
//  name: dest2src_post_stall
#define DEBUG_BUS_DEST2SRC_POST_STALL_WORD {4, 3, 4, 0, 1, 0}
#define DEBUG_BUS_DEST2SRC_POST_STALL {DEBUG_BUS_DEST2SRC_POST_STALL_WORD, 5, 0x1}

//  location: tt_instruction_issue name: debug_daisy_stop_issue0
//  name: debug_issue0_in[2]
//  name: post_shiftxb_stall
#define DEBUG_BUS_POST_SHIFTXB_STALL_WORD {4, 3, 4, 0, 1, 0}
#define DEBUG_BUS_POST_SHIFTXB_STALL {DEBUG_BUS_POST_SHIFTXB_STALL_WORD, 4, 0x1}

//  location: tt_instruction_issue name: debug_daisy_stop_issue0
//  name: debug_issue0_in[2]
//  name: dest2src_dest_stall
#define DEBUG_BUS_DEST2SRC_DEST_STALL_WORD {4, 3, 4, 0, 1, 0}
#define DEBUG_BUS_DEST2SRC_DEST_STALL {DEBUG_BUS_DEST2SRC_DEST_STALL_WORD, 3, 0x1}

//  location: tt_instruction_issue name: debug_daisy_stop_issue0
//  name: debug_issue0_in[2]
//  name: post_alu_instr_stall
#define DEBUG_BUS_POST_ALU_INSTR_STALL_WORD {4, 3, 4, 0, 1, 0}
#define DEBUG_BUS_POST_ALU_INSTR_STALL {DEBUG_BUS_POST_ALU_INSTR_STALL_WORD, 2, 0x1}

//  location: tt_instruction_issue name: debug_daisy_stop_issue0
//  name: debug_issue0_in[2]
//  name: fidelity_phase_cnt
#define DEBUG_BUS_FIDELITY_PHASE_CNT__3_0_WORD {4, 3, 2, 0, 1, 0}
#define DEBUG_BUS_FIDELITY_PHASE_CNT__3_0 {DEBUG_BUS_FIDELITY_PHASE_CNT__3_0_WORD, 28, 0xF}

//  location: tt_instruction_issue name: debug_daisy_stop_issue0
//  name: debug_issue0_in[2]
//  name: fidelity_phase_cnt
#define DEBUG_BUS_FIDELITY_PHASE_CNT__5_4_WORD {4, 3, 4, 0, 1, 0}
#define DEBUG_BUS_FIDELITY_PHASE_CNT__5_4 {DEBUG_BUS_FIDELITY_PHASE_CNT__5_4_WORD, 0, 0x3}

//  location: tt_instruction_issue name: debug_daisy_stop_issue0
//  name: debug_issue0_in[2]
//  name: dbg_last_math_instrn
//  name: math_instrn
#define DEBUG_BUS_MATH_INSTRN__1_0_WORD {4, 3, 2, 0, 1, 0}
#define DEBUG_BUS_MATH_INSTRN__1_0 {DEBUG_BUS_MATH_INSTRN__1_0_WORD, 26, 0x3}

//  location: tt_instruction_issue name: debug_daisy_stop_issue0
//  name: debug_issue0_in[2]
//  name: dbg_last_math_instrn
//  name: math_winner_thread
#define DEBUG_BUS_MATH_WINNER_THREAD__5_0_WORD {4, 3, 0, 0, 1, 0}
#define DEBUG_BUS_MATH_WINNER_THREAD__5_0 {DEBUG_BUS_MATH_WINNER_THREAD__5_0_WORD, 26, 0x3F}

//  location: tt_instruction_issue name: debug_daisy_stop_issue0
//  name: debug_issue0_in[2]
//  name: dbg_last_math_instrn
//  name: math_winner_thread
#define DEBUG_BUS_MATH_WINNER_THREAD__31_6_WORD {4, 3, 2, 0, 1, 0}
#define DEBUG_BUS_MATH_WINNER_THREAD__31_6 {DEBUG_BUS_MATH_WINNER_THREAD__31_6_WORD, 0, 0x3FFFFFF}

//  location: tt_instruction_issue name: debug_daisy_stop_issue0
//  name: debug_issue0_in[2]
//  name: s0_srca_reg_addr
#define DEBUG_BUS_S0_SRCA_REG_ADDR_WORD {4, 3, 0, 0, 1, 0}
#define DEBUG_BUS_S0_SRCA_REG_ADDR {DEBUG_BUS_S0_SRCA_REG_ADDR_WORD, 19, 0x3F}

//  location: tt_instruction_issue name: debug_daisy_stop_issue0
//  name: debug_issue0_in[2]
//  name: s0_srcb_reg_addr
#define DEBUG_BUS_S0_SRCB_REG_ADDR_WORD {4, 3, 0, 0, 1, 0}
#define DEBUG_BUS_S0_SRCB_REG_ADDR {DEBUG_BUS_S0_SRCB_REG_ADDR_WORD, 13, 0x3F}

//  location: tt_instruction_issue name: debug_daisy_stop_issue0
//  name: debug_issue0_in[2]
//  name: s0_dst_reg_addr
#define DEBUG_BUS_S0_DST_REG_ADDR_WORD {4, 3, 0, 0, 1, 0}
#define DEBUG_BUS_S0_DST_REG_ADDR {DEBUG_BUS_S0_DST_REG_ADDR_WORD, 3, 0x3FF}

//  location: tt_instruction_issue name: debug_daisy_stop_issue0
//  name: debug_issue0_in[2]
//  name: s0_fidelity_phase
#define DEBUG_BUS_S0_FIDELITY_PHASE_WORD {4, 3, 0, 0, 1, 0}
#define DEBUG_BUS_S0_FIDELITY_PHASE {DEBUG_BUS_S0_FIDELITY_PHASE_WORD, 1, 0x3}

//  location: tt_instruction_issue name: debug_daisy_stop_issue0
//  name: debug_issue0_in[2]
//  name: s0_dec_instr_single_output_row
#define DEBUG_BUS_S0_DEC_INSTR_SINGLE_OUTPUT_ROW_WORD {4, 3, 0, 0, 1, 0}
#define DEBUG_BUS_S0_DEC_INSTR_SINGLE_OUTPUT_ROW {DEBUG_BUS_S0_DEC_INSTR_SINGLE_OUTPUT_ROW_WORD, 0, 0x1}

//  location: tt_instruction_issue name: debug_daisy_stop_issue0
//  name: debug_issue0_in[0]
//  name: (|math_winner_combo&math_instrn_pipe_ack)
#define DEBUG_BUS___MATH_WINNER_COMBO_MATH_INSTRN_PIPE_ACK__WORD {1, 3, 6, 0, 1, 0}
#define DEBUG_BUS___MATH_WINNER_COMBO_MATH_INSTRN_PIPE_ACK_ {DEBUG_BUS___MATH_WINNER_COMBO_MATH_INSTRN_PIPE_ACK__WORD, 28, 0x1}

//  location: tt_instruction_issue name: debug_daisy_stop_issue0
//  name: debug_issue0_in[0]
//  name: math_instrn_pipe_ack
#define DEBUG_BUS_DEBUG_DAISY_STOP_ISSUE0_DEBUG_ISSUE0_IN_0__MATH_INSTRN_PIPE_ACK__251_WORD {1, 3, 6, 0, 1, 0}
#define DEBUG_BUS_DEBUG_DAISY_STOP_ISSUE0_DEBUG_ISSUE0_IN_0__MATH_INSTRN_PIPE_ACK__251 {DEBUG_BUS_DEBUG_DAISY_STOP_ISSUE0_DEBUG_ISSUE0_IN_0__MATH_INSTRN_PIPE_ACK__251_WORD, 27, 0x1}

//  location: tt_instruction_issue name: debug_daisy_stop_issue0
//  name: debug_issue0_in[0]
//  name: o_math_instrnbuf_rden
#define DEBUG_BUS_O_MATH_INSTRNBUF_RDEN_WORD {1, 3, 6, 0, 1, 0}
#define DEBUG_BUS_O_MATH_INSTRNBUF_RDEN {DEBUG_BUS_O_MATH_INSTRNBUF_RDEN_WORD, 26, 0x1}

//  location: tt_instruction_issue name: debug_daisy_stop_issue0
//  name: debug_issue0_in[0]
//  name: math_instrn_valid
#define DEBUG_BUS_MATH_INSTRN_VALID_WORD {1, 3, 6, 0, 1, 0}
#define DEBUG_BUS_MATH_INSTRN_VALID {DEBUG_BUS_MATH_INSTRN_VALID_WORD, 25, 0x1}

//  location: tt_instruction_issue name: debug_daisy_stop_issue0
//  name: debug_issue0_in[0]
//  name: src_data_ready
#define DEBUG_BUS_SRC_DATA_READY_WORD {1, 3, 6, 0, 1, 0}
#define DEBUG_BUS_SRC_DATA_READY {DEBUG_BUS_SRC_DATA_READY_WORD, 24, 0x1}

//  location: tt_instruction_issue name: debug_daisy_stop_issue0
//  name: debug_issue0_in[0]
//  name: srcb_data_ready
#define DEBUG_BUS_SRCB_DATA_READY_WORD {1, 3, 6, 0, 1, 0}
#define DEBUG_BUS_SRCB_DATA_READY {DEBUG_BUS_SRCB_DATA_READY_WORD, 23, 0x1}

//  location: tt_instruction_issue name: debug_daisy_stop_issue0
//  name: debug_issue0_in[0]
//  name: srca_data_ready
#define DEBUG_BUS_SRCA_DATA_READY_WORD {1, 3, 6, 0, 1, 0}
#define DEBUG_BUS_SRCA_DATA_READY {DEBUG_BUS_SRCA_DATA_READY_WORD, 22, 0x1}

//  location: tt_instruction_issue name: debug_daisy_stop_issue0
//  name: debug_issue0_in[0]
//  name: srcb_write_ready
#define DEBUG_BUS_DEBUG_ISSUE0_IN_0__SRCB_WRITE_READY_WORD {1, 3, 6, 0, 1, 0}
#define DEBUG_BUS_DEBUG_ISSUE0_IN_0__SRCB_WRITE_READY {DEBUG_BUS_DEBUG_ISSUE0_IN_0__SRCB_WRITE_READY_WORD, 21, 0x1}

//  location: tt_instruction_issue name: debug_daisy_stop_issue0
//  name: debug_issue0_in[0]
//  name: srca_write_ready
#define DEBUG_BUS_DEBUG_ISSUE0_IN_0__SRCA_WRITE_READY_WORD {1, 3, 6, 0, 1, 0}
#define DEBUG_BUS_DEBUG_ISSUE0_IN_0__SRCA_WRITE_READY {DEBUG_BUS_DEBUG_ISSUE0_IN_0__SRCA_WRITE_READY_WORD, 20, 0x1}

//  location: tt_instruction_issue name: debug_daisy_stop_issue0
//  name: debug_issue0_in[0]
//  name: srca_update_inst
#define DEBUG_BUS_SRCA_UPDATE_INST_WORD {1, 3, 6, 0, 1, 0}
#define DEBUG_BUS_SRCA_UPDATE_INST {DEBUG_BUS_SRCA_UPDATE_INST_WORD, 19, 0x1}

//  location: tt_instruction_issue name: debug_daisy_stop_issue0
//  name: debug_issue0_in[0]
//  name: srcb_update_inst
#define DEBUG_BUS_SRCB_UPDATE_INST_WORD {1, 3, 6, 0, 1, 0}
#define DEBUG_BUS_SRCB_UPDATE_INST {DEBUG_BUS_SRCB_UPDATE_INST_WORD, 18, 0x1}

//  location: tt_instruction_issue name: debug_daisy_stop_issue0
//  name: debug_issue0_in[0]
//  name: allow_regfile_update
#define DEBUG_BUS_ALLOW_REGFILE_UPDATE_WORD {1, 3, 6, 0, 1, 0}
#define DEBUG_BUS_ALLOW_REGFILE_UPDATE {DEBUG_BUS_ALLOW_REGFILE_UPDATE_WORD, 17, 0x1}

//  location: tt_instruction_issue name: debug_daisy_stop_issue0
//  name: debug_issue0_in[0]
//  name: math_srca_wr_port_avail
#define DEBUG_BUS_MATH_SRCA_WR_PORT_AVAIL_WORD {1, 3, 6, 0, 1, 0}
#define DEBUG_BUS_MATH_SRCA_WR_PORT_AVAIL {DEBUG_BUS_MATH_SRCA_WR_PORT_AVAIL_WORD, 16, 0x1}

//  location: tt_instruction_issue name: debug_daisy_stop_issue0
//  name: debug_issue0_in[0]
//  name: dma_srca_wr_port_avail
#define DEBUG_BUS_DEBUG_ISSUE0_IN_0__DMA_SRCA_WR_PORT_AVAIL_WORD {1, 3, 6, 0, 1, 0}
#define DEBUG_BUS_DEBUG_ISSUE0_IN_0__DMA_SRCA_WR_PORT_AVAIL {DEBUG_BUS_DEBUG_ISSUE0_IN_0__DMA_SRCA_WR_PORT_AVAIL_WORD, 15, 0x1}

//  location: tt_instruction_issue name: debug_daisy_stop_issue0
//  name: debug_issue0_in[0]
//  name: math_srcb_wr_port_avail
#define DEBUG_BUS_MATH_SRCB_WR_PORT_AVAIL_WORD {1, 3, 6, 0, 1, 0}
#define DEBUG_BUS_MATH_SRCB_WR_PORT_AVAIL {DEBUG_BUS_MATH_SRCB_WR_PORT_AVAIL_WORD, 14, 0x1}

//  location: tt_instruction_issue name: debug_daisy_stop_issue0
//  name: debug_issue0_in[0]
//  name: dma_srcb_wr_port_avail
#define DEBUG_BUS_DEBUG_ISSUE0_IN_0__DMA_SRCB_WR_PORT_AVAIL_WORD {1, 3, 6, 0, 1, 0}
#define DEBUG_BUS_DEBUG_ISSUE0_IN_0__DMA_SRCB_WR_PORT_AVAIL {DEBUG_BUS_DEBUG_ISSUE0_IN_0__DMA_SRCB_WR_PORT_AVAIL_WORD, 13, 0x1}

//  location: tt_instruction_issue name: debug_daisy_stop_issue0
//  name: debug_issue0_in[0]
//  name: s0_alu_inst_decoded
#define DEBUG_BUS_S0_ALU_INST_DECODED_WORD {1, 3, 6, 0, 1, 0}
#define DEBUG_BUS_S0_ALU_INST_DECODED {DEBUG_BUS_S0_ALU_INST_DECODED_WORD, 10, 0x7}

//  location: tt_instruction_issue name: debug_daisy_stop_issue0
//  name: debug_issue0_in[0]
//  name: s0_sfpu_inst_decoded
#define DEBUG_BUS_S0_SFPU_INST_DECODED_WORD {1, 3, 6, 0, 1, 0}
#define DEBUG_BUS_S0_SFPU_INST_DECODED {DEBUG_BUS_S0_SFPU_INST_DECODED_WORD, 7, 0x7}

//  location: tt_instruction_issue name: debug_daisy_stop_issue0
//  name: debug_issue0_in[0]
//  name: regw_incr_inst_decoded
#define DEBUG_BUS_REGW_INCR_INST_DECODED_WORD {1, 3, 6, 0, 1, 0}
#define DEBUG_BUS_REGW_INCR_INST_DECODED {DEBUG_BUS_REGW_INCR_INST_DECODED_WORD, 4, 0x7}

//  location: tt_instruction_issue name: debug_daisy_stop_issue0
//  name: debug_issue0_in[0]
//  name: regmov_inst_decoded
#define DEBUG_BUS_REGMOV_INST_DECODED_WORD {1, 3, 6, 0, 1, 0}
#define DEBUG_BUS_REGMOV_INST_DECODED {DEBUG_BUS_REGMOV_INST_DECODED_WORD, 1, 0x7}

//  location: tt_instruction_issue name: debug_daisy_stop_issue0
//  name: debug_issue0_in[0]
//  name: math_instr_valid_th
#define DEBUG_BUS_MATH_INSTR_VALID_TH_WORD {1, 3, 4, 0, 1, 0}
#define DEBUG_BUS_MATH_INSTR_VALID_TH {DEBUG_BUS_MATH_INSTR_VALID_TH_WORD, 29, 0x7}

//  location: tt_instruction_issue name: debug_daisy_stop_issue0
//  name: debug_issue0_in[0]
//  name: math_winner_thread_combo
#define DEBUG_BUS_MATH_WINNER_THREAD_COMBO_WORD {1, 3, 4, 0, 1, 0}
#define DEBUG_BUS_MATH_WINNER_THREAD_COMBO {DEBUG_BUS_MATH_WINNER_THREAD_COMBO_WORD, 27, 0x3}

//  location: tt_instruction_issue name: debug_daisy_stop_issue0
//  name: debug_issue0_in[0]
//  name: math_instrn_pipe_ack
#define DEBUG_BUS_DEBUG_DAISY_STOP_ISSUE0_DEBUG_ISSUE0_IN_0__MATH_INSTRN_PIPE_ACK__215_WORD {1, 3, 4, 0, 1, 0}
#define DEBUG_BUS_DEBUG_DAISY_STOP_ISSUE0_DEBUG_ISSUE0_IN_0__MATH_INSTRN_PIPE_ACK__215 {DEBUG_BUS_DEBUG_DAISY_STOP_ISSUE0_DEBUG_ISSUE0_IN_0__MATH_INSTRN_PIPE_ACK__215_WORD, 23, 0x1}

//  location: tt_instruction_issue name: debug_daisy_stop_issue0
//  name: debug_issue0_in[0]
//  name: math_winner_wo_pipe_stall
#define DEBUG_BUS_MATH_WINNER_WO_PIPE_STALL_WORD {1, 3, 4, 0, 1, 0}
#define DEBUG_BUS_MATH_WINNER_WO_PIPE_STALL {DEBUG_BUS_MATH_WINNER_WO_PIPE_STALL_WORD, 19, 0x7}

//  location: tt_instruction_issue name: debug_daisy_stop_issue0
//  name: debug_issue0_in[0]
//  name: s0_srca_data_ready
#define DEBUG_BUS_S0_SRCA_DATA_READY_WORD {1, 3, 4, 0, 1, 0}
#define DEBUG_BUS_S0_SRCA_DATA_READY {DEBUG_BUS_S0_SRCA_DATA_READY_WORD, 16, 0x7}

//  location: tt_instruction_issue name: debug_daisy_stop_issue0
//  name: debug_issue0_in[0]
//  name: s0_srcb_data_ready
#define DEBUG_BUS_S0_SRCB_DATA_READY_WORD {1, 3, 4, 0, 1, 0}
#define DEBUG_BUS_S0_SRCB_DATA_READY {DEBUG_BUS_S0_SRCB_DATA_READY_WORD, 13, 0x7}

//  location: tt_instruction_issue name: debug_daisy_stop_issue0
//  name: debug_issue0_in[0]
//  name: math_thread_inst_data_valid
#define DEBUG_BUS_MATH_THREAD_INST_DATA_VALID_WORD {1, 3, 4, 0, 1, 0}
#define DEBUG_BUS_MATH_THREAD_INST_DATA_VALID {DEBUG_BUS_MATH_THREAD_INST_DATA_VALID_WORD, 9, 0x7}

//  location: tt_instruction_issue name: debug_daisy_stop_issue0
//  name: debug_issue0_in[0]
//  name: i_DEST_TARGET_REG_CFG_PACK_SEC0_Offset
#define DEBUG_BUS_I_DEST_TARGET_REG_CFG_PACK_SEC0_OFFSET__2_0_WORD {1, 3, 2, 0, 1, 0}
#define DEBUG_BUS_I_DEST_TARGET_REG_CFG_PACK_SEC0_OFFSET__2_0 {DEBUG_BUS_I_DEST_TARGET_REG_CFG_PACK_SEC0_OFFSET__2_0_WORD, 29, 0x7}

//  location: tt_instruction_issue name: debug_daisy_stop_issue0
//  name: debug_issue0_in[0]
//  name: i_DEST_TARGET_REG_CFG_PACK_SEC0_Offset
#define DEBUG_BUS_I_DEST_TARGET_REG_CFG_PACK_SEC0_OFFSET__11_3_WORD {1, 3, 4, 0, 1, 0}
#define DEBUG_BUS_I_DEST_TARGET_REG_CFG_PACK_SEC0_OFFSET__11_3 {DEBUG_BUS_I_DEST_TARGET_REG_CFG_PACK_SEC0_OFFSET__11_3_WORD, 0, 0x1FF}

//  location: tt_instruction_issue name: debug_daisy_stop_issue0
//  name: debug_issue0_in[0]
//  name: i_DEST_TARGET_REG_CFG_PACK_SEC1_Offset
#define DEBUG_BUS_I_DEST_TARGET_REG_CFG_PACK_SEC1_OFFSET_WORD {1, 3, 2, 0, 1, 0}
#define DEBUG_BUS_I_DEST_TARGET_REG_CFG_PACK_SEC1_OFFSET {DEBUG_BUS_I_DEST_TARGET_REG_CFG_PACK_SEC1_OFFSET_WORD, 17, 0xFFF}

//  location: tt_instruction_issue name: debug_daisy_stop_issue0
//  name: debug_issue0_in[0]
//  name: i_DEST_TARGET_REG_CFG_PACK_SEC2_Offset
#define DEBUG_BUS_I_DEST_TARGET_REG_CFG_PACK_SEC2_OFFSET_WORD {1, 3, 2, 0, 1, 0}
#define DEBUG_BUS_I_DEST_TARGET_REG_CFG_PACK_SEC2_OFFSET {DEBUG_BUS_I_DEST_TARGET_REG_CFG_PACK_SEC2_OFFSET_WORD, 5, 0xFFF}

//  location: tt_instruction_issue name: debug_daisy_stop_issue0
//  name: debug_issue0_in[0]
//  name: i_DEST_TARGET_REG_CFG_PACK_SEC3_Offset
#define DEBUG_BUS_I_DEST_TARGET_REG_CFG_PACK_SEC3_OFFSET__6_0_WORD {1, 3, 0, 0, 1, 0}
#define DEBUG_BUS_I_DEST_TARGET_REG_CFG_PACK_SEC3_OFFSET__6_0 {DEBUG_BUS_I_DEST_TARGET_REG_CFG_PACK_SEC3_OFFSET__6_0_WORD, 25, 0x7F}

//  location: tt_instruction_issue name: debug_daisy_stop_issue0
//  name: debug_issue0_in[0]
//  name: i_DEST_TARGET_REG_CFG_PACK_SEC3_Offset
#define DEBUG_BUS_I_DEST_TARGET_REG_CFG_PACK_SEC3_OFFSET__11_7_WORD {1, 3, 2, 0, 1, 0}
#define DEBUG_BUS_I_DEST_TARGET_REG_CFG_PACK_SEC3_OFFSET__11_7 {DEBUG_BUS_I_DEST_TARGET_REG_CFG_PACK_SEC3_OFFSET__11_7_WORD, 0, 0x1F}

//  location: tt_instruction_issue name: debug_daisy_stop_issue0
//  name: debug_issue0_in[0]
//  name: i_DEST_TARGET_REG_CFG_PACK_SEC0_ZOffset
#define DEBUG_BUS_I_DEST_TARGET_REG_CFG_PACK_SEC0_ZOFFSET_WORD {1, 3, 0, 0, 1, 0}
#define DEBUG_BUS_I_DEST_TARGET_REG_CFG_PACK_SEC0_ZOFFSET {DEBUG_BUS_I_DEST_TARGET_REG_CFG_PACK_SEC0_ZOFFSET_WORD, 19, 0x3F}

//  location: tt_instruction_issue name: debug_daisy_stop_issue0
//  name: debug_issue0_in[0]
//  name: i_DEST_TARGET_REG_CFG_PACK_SEC1_ZOffset
#define DEBUG_BUS_I_DEST_TARGET_REG_CFG_PACK_SEC1_ZOFFSET_WORD {1, 3, 0, 0, 1, 0}
#define DEBUG_BUS_I_DEST_TARGET_REG_CFG_PACK_SEC1_ZOFFSET {DEBUG_BUS_I_DEST_TARGET_REG_CFG_PACK_SEC1_ZOFFSET_WORD, 13, 0x3F}

//  location: tt_instruction_issue name: debug_daisy_stop_issue0
//  name: debug_issue0_in[0]
//  name: i_DEST_TARGET_REG_CFG_PACK_SEC2_ZOffset
#define DEBUG_BUS_I_DEST_TARGET_REG_CFG_PACK_SEC2_ZOFFSET_WORD {1, 3, 0, 0, 1, 0}
#define DEBUG_BUS_I_DEST_TARGET_REG_CFG_PACK_SEC2_ZOFFSET {DEBUG_BUS_I_DEST_TARGET_REG_CFG_PACK_SEC2_ZOFFSET_WORD, 7, 0x3F}

//  location: tt_instruction_issue name: debug_daisy_stop_issue0
//  name: debug_issue0_in[0]
//  name: i_DEST_TARGET_REG_CFG_PACK_SEC3_ZOffset
#define DEBUG_BUS_I_DEST_TARGET_REG_CFG_PACK_SEC3_ZOFFSET_WORD {1, 3, 0, 0, 1, 0}
#define DEBUG_BUS_I_DEST_TARGET_REG_CFG_PACK_SEC3_ZOFFSET {DEBUG_BUS_I_DEST_TARGET_REG_CFG_PACK_SEC3_ZOFFSET_WORD, 1, 0x3F}

//  location: tt_instruction_issue name: debug_daisy_stop_issue0
//  name: debug_issue0_in[0]
//  name: i_DEST_TARGET_REG_CFG_Math_offset
#define DEBUG_BUS_I_DEST_TARGET_REG_CFG_MATH_OFFSET__34_24_WORD {0, 3, 6, 0, 1, 0}
#define DEBUG_BUS_I_DEST_TARGET_REG_CFG_MATH_OFFSET__34_24 {DEBUG_BUS_I_DEST_TARGET_REG_CFG_MATH_OFFSET__34_24_WORD, 21, 0x7FF}

//  location: tt_instruction_issue name: debug_daisy_stop_issue0
//  name: debug_issue0_in[0]
//  name: i_DEST_TARGET_REG_CFG_Math_offset
#define DEBUG_BUS_I_DEST_TARGET_REG_CFG_MATH_OFFSET__35_35_WORD {1, 3, 0, 0, 1, 0}
#define DEBUG_BUS_I_DEST_TARGET_REG_CFG_MATH_OFFSET__35_35 {DEBUG_BUS_I_DEST_TARGET_REG_CFG_MATH_OFFSET__35_35_WORD, 0, 0x1}

//  location: tt_instruction_issue name: debug_daisy_stop_issue0
//  name: debug_issue0_in[0]
//  name: i_DEST_TARGET_REG_CFG_Math_offset
#define DEBUG_BUS_I_DEST_TARGET_REG_CFG_MATH_OFFSET__23_12_WORD {0, 3, 6, 0, 1, 0}
#define DEBUG_BUS_I_DEST_TARGET_REG_CFG_MATH_OFFSET__23_12 {DEBUG_BUS_I_DEST_TARGET_REG_CFG_MATH_OFFSET__23_12_WORD, 9, 0xFFF}

//  location: tt_instruction_issue name: debug_daisy_stop_issue0
//  name: debug_issue0_in[0]
//  name: i_DEST_TARGET_REG_CFG_Math_offset
#define DEBUG_BUS_I_DEST_TARGET_REG_CFG_MATH_OFFSET__2_0_WORD {0, 3, 4, 0, 1, 0}
#define DEBUG_BUS_I_DEST_TARGET_REG_CFG_MATH_OFFSET__2_0 {DEBUG_BUS_I_DEST_TARGET_REG_CFG_MATH_OFFSET__2_0_WORD, 29, 0x7}

//  location: tt_instruction_issue name: debug_daisy_stop_issue0
//  name: debug_issue0_in[0]
//  name: i_DEST_TARGET_REG_CFG_Math_offset
#define DEBUG_BUS_I_DEST_TARGET_REG_CFG_MATH_OFFSET__11_3_WORD {0, 3, 6, 0, 1, 0}
#define DEBUG_BUS_I_DEST_TARGET_REG_CFG_MATH_OFFSET__11_3 {DEBUG_BUS_I_DEST_TARGET_REG_CFG_MATH_OFFSET__11_3_WORD, 0, 0x1FF}

//  location: tt_instruction_issue name: debug_daisy_stop_issue0
//  name: debug_issue0_in[0]
//  name: i_thread_state_id
#define DEBUG_BUS_I_THREAD_STATE_ID_WORD {0, 3, 4, 0, 1, 0}
#define DEBUG_BUS_I_THREAD_STATE_ID {DEBUG_BUS_I_THREAD_STATE_ID_WORD, 25, 0x7}

//  location: tt_instruction_issue name: debug_daisy_stop_issue0
//  name: debug_issue0_in[0]
//  name: i_opcode
#define DEBUG_BUS_I_OPCODE__23_16_WORD {0, 3, 4, 0, 1, 0}
#define DEBUG_BUS_I_OPCODE__23_16 {DEBUG_BUS_I_OPCODE__23_16_WORD, 17, 0xFF}

//  location: tt_instruction_issue name: debug_daisy_stop_issue0
//  name: debug_issue0_in[0]
//  name: i_instrn_payload
#define DEBUG_BUS_I_INSTRN_PAYLOAD__54_48_WORD {0, 3, 2, 0, 1, 0}
#define DEBUG_BUS_I_INSTRN_PAYLOAD__54_48 {DEBUG_BUS_I_INSTRN_PAYLOAD__54_48_WORD, 25, 0x7F}

//  location: tt_instruction_issue name: debug_daisy_stop_issue0
//  name: debug_issue0_in[0]
//  name: i_instrn_payload
#define DEBUG_BUS_I_INSTRN_PAYLOAD__71_55_WORD {0, 3, 4, 0, 1, 0}
#define DEBUG_BUS_I_INSTRN_PAYLOAD__71_55 {DEBUG_BUS_I_INSTRN_PAYLOAD__71_55_WORD, 0, 0x1FFFF}

//  location: tt_instruction_issue name: debug_daisy_stop_issue0
//  name: debug_issue0_in[0]
//  name: i_opcode
#define DEBUG_BUS_I_OPCODE__15_8_WORD {0, 3, 2, 0, 1, 0}
#define DEBUG_BUS_I_OPCODE__15_8 {DEBUG_BUS_I_OPCODE__15_8_WORD, 17, 0xFF}

//  location: tt_instruction_issue name: debug_daisy_stop_issue0
//  name: debug_issue0_in[0]
//  name: i_instrn_payload
#define DEBUG_BUS_I_INSTRN_PAYLOAD__30_24_WORD {0, 3, 0, 0, 1, 0}
#define DEBUG_BUS_I_INSTRN_PAYLOAD__30_24 {DEBUG_BUS_I_INSTRN_PAYLOAD__30_24_WORD, 25, 0x7F}

//  location: tt_instruction_issue name: debug_daisy_stop_issue0
//  name: debug_issue0_in[0]
//  name: i_instrn_payload
#define DEBUG_BUS_I_INSTRN_PAYLOAD__47_31_WORD {0, 3, 2, 0, 1, 0}
#define DEBUG_BUS_I_INSTRN_PAYLOAD__47_31 {DEBUG_BUS_I_INSTRN_PAYLOAD__47_31_WORD, 0, 0x1FFFF}

//  location: tt_instruction_issue name: debug_daisy_stop_issue0
//  name: debug_issue0_in[0]
//  name: i_opcode
#define DEBUG_BUS_I_OPCODE__8_8_WORD {0, 3, 0, 0, 1, 0}
#define DEBUG_BUS_I_OPCODE__8_8 {DEBUG_BUS_I_OPCODE__8_8_WORD, 24, 0x1}

//  location: tt_instruction_issue name: debug_daisy_stop_issue0
//  name: debug_issue0_in[0]
//  name: i_instrn_payload
#define DEBUG_BUS_I_INSTRN_PAYLOAD__23_0_WORD {0, 3, 0, 0, 1, 0}
#define DEBUG_BUS_I_INSTRN_PAYLOAD__23_0 {DEBUG_BUS_I_INSTRN_PAYLOAD__23_0_WORD, 0, 0xFFFFFF}

//  location: tt_instruction_issue name: debug_daisy_stop_issue1
//  name: perf_cnt_instrn_issue_dbg[18] alias: perf_cnt note: 
//  name: stall_cnt
#define DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_18__STALL_CNT_WORD {22, 4, 6, 0, 1, 0}
#define DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_18__STALL_CNT {DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_18__STALL_CNT_WORD, 0, 0xFFFFFFFF}

//  location: tt_instruction_issue name: debug_daisy_stop_issue1
//  name: perf_cnt_instrn_issue_dbg[18] alias: perf_cnt note: 
//  name: grant_cnt
#define DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_18__GRANT_CNT_WORD {22, 4, 4, 0, 1, 0}
#define DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_18__GRANT_CNT {DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_18__GRANT_CNT_WORD, 0, 0xFFFFFFFF}

//  location: tt_instruction_issue name: debug_daisy_stop_issue1
//  name: perf_cnt_instrn_issue_dbg[18] alias: perf_cnt note: 
//  name: req_cnt
#define DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_18__REQ_CNT_WORD {22, 4, 2, 0, 1, 0}
#define DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_18__REQ_CNT {DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_18__REQ_CNT_WORD, 0, 0xFFFFFFFF}

//  location: tt_instruction_issue name: debug_daisy_stop_issue1
//  name: perf_cnt_instrn_issue_dbg[18] alias: perf_cnt note: 
//  name: ref_cnt
#define DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_18__REF_CNT_WORD {22, 4, 0, 0, 1, 0}
#define DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_18__REF_CNT {DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_18__REF_CNT_WORD, 0, 0xFFFFFFFF}

//  location: tt_instruction_issue name: debug_daisy_stop_issue1
//  name: perf_cnt_instrn_issue_dbg[17] alias: perf_cnt note: 
//  name: stall_cnt
#define DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_17__STALL_CNT_WORD {20, 4, 6, 0, 1, 0}
#define DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_17__STALL_CNT {DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_17__STALL_CNT_WORD, 0, 0xFFFFFFFF}

//  location: tt_instruction_issue name: debug_daisy_stop_issue1
//  name: perf_cnt_instrn_issue_dbg[17] alias: perf_cnt note: 
//  name: grant_cnt
#define DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_17__GRANT_CNT_WORD {20, 4, 4, 0, 1, 0}
#define DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_17__GRANT_CNT {DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_17__GRANT_CNT_WORD, 0, 0xFFFFFFFF}

//  location: tt_instruction_issue name: debug_daisy_stop_issue1
//  name: perf_cnt_instrn_issue_dbg[17] alias: perf_cnt note: 
//  name: req_cnt
#define DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_17__REQ_CNT_WORD {20, 4, 2, 0, 1, 0}
#define DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_17__REQ_CNT {DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_17__REQ_CNT_WORD, 0, 0xFFFFFFFF}

//  location: tt_instruction_issue name: debug_daisy_stop_issue1
//  name: perf_cnt_instrn_issue_dbg[17] alias: perf_cnt note: 
//  name: ref_cnt
#define DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_17__REF_CNT_WORD {20, 4, 0, 0, 1, 0}
#define DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_17__REF_CNT {DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_17__REF_CNT_WORD, 0, 0xFFFFFFFF}

//  location: tt_instruction_issue name: debug_daisy_stop_issue1
//  name: perf_cnt_instrn_issue_dbg[16] alias: perf_cnt note: 
//  name: stall_cnt
#define DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_16__STALL_CNT_WORD {18, 4, 6, 0, 1, 0}
#define DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_16__STALL_CNT {DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_16__STALL_CNT_WORD, 0, 0xFFFFFFFF}

//  location: tt_instruction_issue name: debug_daisy_stop_issue1
//  name: perf_cnt_instrn_issue_dbg[16] alias: perf_cnt note: 
//  name: grant_cnt
#define DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_16__GRANT_CNT_WORD {18, 4, 4, 0, 1, 0}
#define DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_16__GRANT_CNT {DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_16__GRANT_CNT_WORD, 0, 0xFFFFFFFF}

//  location: tt_instruction_issue name: debug_daisy_stop_issue1
//  name: perf_cnt_instrn_issue_dbg[16] alias: perf_cnt note: 
//  name: req_cnt
#define DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_16__REQ_CNT_WORD {18, 4, 2, 0, 1, 0}
#define DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_16__REQ_CNT {DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_16__REQ_CNT_WORD, 0, 0xFFFFFFFF}

//  location: tt_instruction_issue name: debug_daisy_stop_issue1
//  name: perf_cnt_instrn_issue_dbg[16] alias: perf_cnt note: 
//  name: ref_cnt
#define DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_16__REF_CNT_WORD {18, 4, 0, 0, 1, 0}
#define DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_16__REF_CNT {DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_16__REF_CNT_WORD, 0, 0xFFFFFFFF}

//  location: tt_instruction_issue name: debug_daisy_stop_issue1
//  name: perf_cnt_instrn_issue_dbg[15] alias: perf_cnt note: 
//  name: stall_cnt
#define DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_15__STALL_CNT_WORD {16, 4, 6, 0, 1, 0}
#define DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_15__STALL_CNT {DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_15__STALL_CNT_WORD, 0, 0xFFFFFFFF}

//  location: tt_instruction_issue name: debug_daisy_stop_issue1
//  name: perf_cnt_instrn_issue_dbg[15] alias: perf_cnt note: 
//  name: grant_cnt
#define DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_15__GRANT_CNT_WORD {16, 4, 4, 0, 1, 0}
#define DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_15__GRANT_CNT {DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_15__GRANT_CNT_WORD, 0, 0xFFFFFFFF}

//  location: tt_instruction_issue name: debug_daisy_stop_issue1
//  name: perf_cnt_instrn_issue_dbg[15] alias: perf_cnt note: 
//  name: req_cnt
#define DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_15__REQ_CNT_WORD {16, 4, 2, 0, 1, 0}
#define DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_15__REQ_CNT {DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_15__REQ_CNT_WORD, 0, 0xFFFFFFFF}

//  location: tt_instruction_issue name: debug_daisy_stop_issue1
//  name: perf_cnt_instrn_issue_dbg[15] alias: perf_cnt note: 
//  name: ref_cnt
#define DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_15__REF_CNT_WORD {16, 4, 0, 0, 1, 0}
#define DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_15__REF_CNT {DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_15__REF_CNT_WORD, 0, 0xFFFFFFFF}

//  location: tt_instruction_issue name: debug_daisy_stop_issue1
//  name: perf_cnt_instrn_issue_dbg[14] alias: perf_cnt note: 
//  name: stall_cnt
#define DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_14__STALL_CNT_WORD {14, 4, 6, 0, 1, 0}
#define DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_14__STALL_CNT {DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_14__STALL_CNT_WORD, 0, 0xFFFFFFFF}

//  location: tt_instruction_issue name: debug_daisy_stop_issue1
//  name: perf_cnt_instrn_issue_dbg[14] alias: perf_cnt note: 
//  name: grant_cnt
#define DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_14__GRANT_CNT_WORD {14, 4, 4, 0, 1, 0}
#define DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_14__GRANT_CNT {DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_14__GRANT_CNT_WORD, 0, 0xFFFFFFFF}

//  location: tt_instruction_issue name: debug_daisy_stop_issue1
//  name: perf_cnt_instrn_issue_dbg[14] alias: perf_cnt note: 
//  name: req_cnt
#define DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_14__REQ_CNT_WORD {14, 4, 2, 0, 1, 0}
#define DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_14__REQ_CNT {DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_14__REQ_CNT_WORD, 0, 0xFFFFFFFF}

//  location: tt_instruction_issue name: debug_daisy_stop_issue1
//  name: perf_cnt_instrn_issue_dbg[14] alias: perf_cnt note: 
//  name: ref_cnt
#define DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_14__REF_CNT_WORD {14, 4, 0, 0, 1, 0}
#define DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_14__REF_CNT {DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_14__REF_CNT_WORD, 0, 0xFFFFFFFF}

//  location: tt_instruction_issue name: debug_daisy_stop_issue1
//  name: perf_cnt_instrn_issue_dbg[13] alias: perf_cnt note: 
//  name: stall_cnt
#define DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_13__STALL_CNT_WORD {12, 4, 6, 0, 1, 0}
#define DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_13__STALL_CNT {DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_13__STALL_CNT_WORD, 0, 0xFFFFFFFF}

//  location: tt_instruction_issue name: debug_daisy_stop_issue1
//  name: perf_cnt_instrn_issue_dbg[13] alias: perf_cnt note: 
//  name: grant_cnt
#define DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_13__GRANT_CNT_WORD {12, 4, 4, 0, 1, 0}
#define DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_13__GRANT_CNT {DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_13__GRANT_CNT_WORD, 0, 0xFFFFFFFF}

//  location: tt_instruction_issue name: debug_daisy_stop_issue1
//  name: perf_cnt_instrn_issue_dbg[13] alias: perf_cnt note: 
//  name: req_cnt
#define DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_13__REQ_CNT_WORD {12, 4, 2, 0, 1, 0}
#define DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_13__REQ_CNT {DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_13__REQ_CNT_WORD, 0, 0xFFFFFFFF}

//  location: tt_instruction_issue name: debug_daisy_stop_issue1
//  name: perf_cnt_instrn_issue_dbg[13] alias: perf_cnt note: 
//  name: ref_cnt
#define DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_13__REF_CNT_WORD {12, 4, 0, 0, 1, 0}
#define DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_13__REF_CNT {DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_13__REF_CNT_WORD, 0, 0xFFFFFFFF}

//  location: tt_instruction_issue name: debug_daisy_stop_issue1
//  name: perf_cnt_instrn_issue_dbg[12] alias: perf_cnt note: 
//  name: stall_cnt
#define DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_12__STALL_CNT_WORD {10, 4, 6, 0, 1, 0}
#define DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_12__STALL_CNT {DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_12__STALL_CNT_WORD, 0, 0xFFFFFFFF}

//  location: tt_instruction_issue name: debug_daisy_stop_issue1
//  name: perf_cnt_instrn_issue_dbg[12] alias: perf_cnt note: 
//  name: grant_cnt
#define DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_12__GRANT_CNT_WORD {10, 4, 4, 0, 1, 0}
#define DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_12__GRANT_CNT {DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_12__GRANT_CNT_WORD, 0, 0xFFFFFFFF}

//  location: tt_instruction_issue name: debug_daisy_stop_issue1
//  name: perf_cnt_instrn_issue_dbg[12] alias: perf_cnt note: 
//  name: req_cnt
#define DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_12__REQ_CNT_WORD {10, 4, 2, 0, 1, 0}
#define DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_12__REQ_CNT {DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_12__REQ_CNT_WORD, 0, 0xFFFFFFFF}

//  location: tt_instruction_issue name: debug_daisy_stop_issue1
//  name: perf_cnt_instrn_issue_dbg[12] alias: perf_cnt note: 
//  name: ref_cnt
#define DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_12__REF_CNT_WORD {10, 4, 0, 0, 1, 0}
#define DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_12__REF_CNT {DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_12__REF_CNT_WORD, 0, 0xFFFFFFFF}

//  location: tt_instruction_issue name: debug_daisy_stop_issue1
//  name: perf_cnt_instrn_issue_dbg[11] alias: perf_cnt note: 
//  name: stall_cnt
#define DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_11__STALL_CNT_WORD {8, 4, 6, 0, 1, 0}
#define DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_11__STALL_CNT {DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_11__STALL_CNT_WORD, 0, 0xFFFFFFFF}

//  location: tt_instruction_issue name: debug_daisy_stop_issue1
//  name: perf_cnt_instrn_issue_dbg[11] alias: perf_cnt note: 
//  name: grant_cnt
#define DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_11__GRANT_CNT_WORD {8, 4, 4, 0, 1, 0}
#define DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_11__GRANT_CNT {DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_11__GRANT_CNT_WORD, 0, 0xFFFFFFFF}

//  location: tt_instruction_issue name: debug_daisy_stop_issue1
//  name: perf_cnt_instrn_issue_dbg[11] alias: perf_cnt note: 
//  name: req_cnt
#define DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_11__REQ_CNT_WORD {8, 4, 2, 0, 1, 0}
#define DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_11__REQ_CNT {DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_11__REQ_CNT_WORD, 0, 0xFFFFFFFF}

//  location: tt_instruction_issue name: debug_daisy_stop_issue1
//  name: perf_cnt_instrn_issue_dbg[11] alias: perf_cnt note: 
//  name: ref_cnt
#define DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_11__REF_CNT_WORD {8, 4, 0, 0, 1, 0}
#define DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_11__REF_CNT {DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_11__REF_CNT_WORD, 0, 0xFFFFFFFF}

//  location: tt_instruction_issue name: debug_daisy_stop_issue1
//  name: perf_cnt_instrn_issue_dbg[10] alias: perf_cnt note: 
//  name: stall_cnt
#define DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_10__STALL_CNT_WORD {6, 4, 6, 0, 1, 0}
#define DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_10__STALL_CNT {DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_10__STALL_CNT_WORD, 0, 0xFFFFFFFF}

//  location: tt_instruction_issue name: debug_daisy_stop_issue1
//  name: perf_cnt_instrn_issue_dbg[10] alias: perf_cnt note: 
//  name: grant_cnt
#define DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_10__GRANT_CNT_WORD {6, 4, 4, 0, 1, 0}
#define DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_10__GRANT_CNT {DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_10__GRANT_CNT_WORD, 0, 0xFFFFFFFF}

//  location: tt_instruction_issue name: debug_daisy_stop_issue1
//  name: perf_cnt_instrn_issue_dbg[10] alias: perf_cnt note: 
//  name: req_cnt
#define DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_10__REQ_CNT_WORD {6, 4, 2, 0, 1, 0}
#define DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_10__REQ_CNT {DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_10__REQ_CNT_WORD, 0, 0xFFFFFFFF}

//  location: tt_instruction_issue name: debug_daisy_stop_issue1
//  name: perf_cnt_instrn_issue_dbg[10] alias: perf_cnt note: 
//  name: ref_cnt
#define DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_10__REF_CNT_WORD {6, 4, 0, 0, 1, 0}
#define DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_10__REF_CNT {DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_10__REF_CNT_WORD, 0, 0xFFFFFFFF}

//  location: tt_instruction_issue name: debug_daisy_stop_issue1
//  name: perf_cnt_instrn_issue_dbg[9] alias: perf_cnt note: 
//  name: stall_cnt
#define DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_9__STALL_CNT_WORD {4, 4, 6, 0, 1, 0}
#define DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_9__STALL_CNT {DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_9__STALL_CNT_WORD, 0, 0xFFFFFFFF}

//  location: tt_instruction_issue name: debug_daisy_stop_issue1
//  name: perf_cnt_instrn_issue_dbg[9] alias: perf_cnt note: 
//  name: grant_cnt
#define DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_9__GRANT_CNT_WORD {4, 4, 4, 0, 1, 0}
#define DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_9__GRANT_CNT {DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_9__GRANT_CNT_WORD, 0, 0xFFFFFFFF}

//  location: tt_instruction_issue name: debug_daisy_stop_issue1
//  name: perf_cnt_instrn_issue_dbg[9] alias: perf_cnt note: 
//  name: req_cnt
#define DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_9__REQ_CNT_WORD {4, 4, 2, 0, 1, 0}
#define DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_9__REQ_CNT {DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_9__REQ_CNT_WORD, 0, 0xFFFFFFFF}

//  location: tt_instruction_issue name: debug_daisy_stop_issue1
//  name: perf_cnt_instrn_issue_dbg[9] alias: perf_cnt note: 
//  name: ref_cnt
#define DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_9__REF_CNT_WORD {4, 4, 0, 0, 1, 0}
#define DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_9__REF_CNT {DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_9__REF_CNT_WORD, 0, 0xFFFFFFFF}

//  location: tt_instruction_issue name: debug_daisy_stop_issue1
//  name: perf_cnt_instrn_issue_dbg[8] alias: perf_cnt note: 
//  name: stall_cnt
#define DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_8__STALL_CNT_WORD {2, 4, 6, 0, 1, 0}
#define DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_8__STALL_CNT {DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_8__STALL_CNT_WORD, 0, 0xFFFFFFFF}

//  location: tt_instruction_issue name: debug_daisy_stop_issue1
//  name: perf_cnt_instrn_issue_dbg[8] alias: perf_cnt note: 
//  name: grant_cnt
#define DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_8__GRANT_CNT_WORD {2, 4, 4, 0, 1, 0}
#define DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_8__GRANT_CNT {DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_8__GRANT_CNT_WORD, 0, 0xFFFFFFFF}

//  location: tt_instruction_issue name: debug_daisy_stop_issue1
//  name: perf_cnt_instrn_issue_dbg[8] alias: perf_cnt note: 
//  name: req_cnt
#define DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_8__REQ_CNT_WORD {2, 4, 2, 0, 1, 0}
#define DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_8__REQ_CNT {DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_8__REQ_CNT_WORD, 0, 0xFFFFFFFF}

//  location: tt_instruction_issue name: debug_daisy_stop_issue1
//  name: perf_cnt_instrn_issue_dbg[8] alias: perf_cnt note: 
//  name: ref_cnt
#define DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_8__REF_CNT_WORD {2, 4, 0, 0, 1, 0}
#define DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_8__REF_CNT {DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_8__REF_CNT_WORD, 0, 0xFFFFFFFF}

//  location: tt_instruction_issue name: debug_daisy_stop_issue2
//  name: perf_cnt_instrn_issue_dbg[7] alias: perf_cnt note: 
//  name: stall_cnt
#define DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_7__STALL_CNT_WORD {14, 5, 6, 0, 1, 0}
#define DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_7__STALL_CNT {DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_7__STALL_CNT_WORD, 0, 0xFFFFFFFF}

//  location: tt_instruction_issue name: debug_daisy_stop_issue2
//  name: perf_cnt_instrn_issue_dbg[7] alias: perf_cnt note: 
//  name: grant_cnt
#define DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_7__GRANT_CNT_WORD {14, 5, 4, 0, 1, 0}
#define DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_7__GRANT_CNT {DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_7__GRANT_CNT_WORD, 0, 0xFFFFFFFF}

//  location: tt_instruction_issue name: debug_daisy_stop_issue2
//  name: perf_cnt_instrn_issue_dbg[7] alias: perf_cnt note: 
//  name: req_cnt
#define DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_7__REQ_CNT_WORD {14, 5, 2, 0, 1, 0}
#define DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_7__REQ_CNT {DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_7__REQ_CNT_WORD, 0, 0xFFFFFFFF}

//  location: tt_instruction_issue name: debug_daisy_stop_issue2
//  name: perf_cnt_instrn_issue_dbg[7] alias: perf_cnt note: 
//  name: ref_cnt
#define DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_7__REF_CNT_WORD {14, 5, 0, 0, 1, 0}
#define DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_7__REF_CNT {DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_7__REF_CNT_WORD, 0, 0xFFFFFFFF}

//  location: tt_instruction_issue name: debug_daisy_stop_issue2
//  name: perf_cnt_instrn_issue_dbg[6] alias: perf_cnt note: 
//  name: stall_cnt
#define DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_6__STALL_CNT_WORD {12, 5, 6, 0, 1, 0}
#define DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_6__STALL_CNT {DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_6__STALL_CNT_WORD, 0, 0xFFFFFFFF}

//  location: tt_instruction_issue name: debug_daisy_stop_issue2
//  name: perf_cnt_instrn_issue_dbg[6] alias: perf_cnt note: 
//  name: grant_cnt
#define DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_6__GRANT_CNT_WORD {12, 5, 4, 0, 1, 0}
#define DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_6__GRANT_CNT {DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_6__GRANT_CNT_WORD, 0, 0xFFFFFFFF}

//  location: tt_instruction_issue name: debug_daisy_stop_issue2
//  name: perf_cnt_instrn_issue_dbg[6] alias: perf_cnt note: 
//  name: req_cnt
#define DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_6__REQ_CNT_WORD {12, 5, 2, 0, 1, 0}
#define DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_6__REQ_CNT {DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_6__REQ_CNT_WORD, 0, 0xFFFFFFFF}

//  location: tt_instruction_issue name: debug_daisy_stop_issue2
//  name: perf_cnt_instrn_issue_dbg[6] alias: perf_cnt note: 
//  name: ref_cnt
#define DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_6__REF_CNT_WORD {12, 5, 0, 0, 1, 0}
#define DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_6__REF_CNT {DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_6__REF_CNT_WORD, 0, 0xFFFFFFFF}

//  location: tt_instruction_issue name: debug_daisy_stop_issue2
//  name: perf_cnt_instrn_issue_dbg[5] alias: perf_cnt note: 
//  name: stall_cnt
#define DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_5__STALL_CNT_WORD {10, 5, 6, 0, 1, 0}
#define DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_5__STALL_CNT {DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_5__STALL_CNT_WORD, 0, 0xFFFFFFFF}

//  location: tt_instruction_issue name: debug_daisy_stop_issue2
//  name: perf_cnt_instrn_issue_dbg[5] alias: perf_cnt note: 
//  name: grant_cnt
#define DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_5__GRANT_CNT_WORD {10, 5, 4, 0, 1, 0}
#define DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_5__GRANT_CNT {DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_5__GRANT_CNT_WORD, 0, 0xFFFFFFFF}

//  location: tt_instruction_issue name: debug_daisy_stop_issue2
//  name: perf_cnt_instrn_issue_dbg[5] alias: perf_cnt note: 
//  name: req_cnt
#define DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_5__REQ_CNT_WORD {10, 5, 2, 0, 1, 0}
#define DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_5__REQ_CNT {DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_5__REQ_CNT_WORD, 0, 0xFFFFFFFF}

//  location: tt_instruction_issue name: debug_daisy_stop_issue2
//  name: perf_cnt_instrn_issue_dbg[5] alias: perf_cnt note: 
//  name: ref_cnt
#define DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_5__REF_CNT_WORD {10, 5, 0, 0, 1, 0}
#define DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_5__REF_CNT {DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_5__REF_CNT_WORD, 0, 0xFFFFFFFF}

//  location: tt_instruction_issue name: debug_daisy_stop_issue2
//  name: perf_cnt_instrn_issue_dbg[4] alias: perf_cnt note: 
//  name: stall_cnt
#define DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_4__STALL_CNT_WORD {8, 5, 6, 0, 1, 0}
#define DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_4__STALL_CNT {DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_4__STALL_CNT_WORD, 0, 0xFFFFFFFF}

//  location: tt_instruction_issue name: debug_daisy_stop_issue2
//  name: perf_cnt_instrn_issue_dbg[4] alias: perf_cnt note: 
//  name: grant_cnt
#define DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_4__GRANT_CNT_WORD {8, 5, 4, 0, 1, 0}
#define DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_4__GRANT_CNT {DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_4__GRANT_CNT_WORD, 0, 0xFFFFFFFF}

//  location: tt_instruction_issue name: debug_daisy_stop_issue2
//  name: perf_cnt_instrn_issue_dbg[4] alias: perf_cnt note: 
//  name: req_cnt
#define DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_4__REQ_CNT_WORD {8, 5, 2, 0, 1, 0}
#define DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_4__REQ_CNT {DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_4__REQ_CNT_WORD, 0, 0xFFFFFFFF}

//  location: tt_instruction_issue name: debug_daisy_stop_issue2
//  name: perf_cnt_instrn_issue_dbg[4] alias: perf_cnt note: 
//  name: ref_cnt
#define DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_4__REF_CNT_WORD {8, 5, 0, 0, 1, 0}
#define DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_4__REF_CNT {DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_4__REF_CNT_WORD, 0, 0xFFFFFFFF}

//  location: tt_instruction_issue name: debug_daisy_stop_issue2
//  name: perf_cnt_instrn_issue_dbg[3] alias: perf_cnt note: 
//  name: stall_cnt
#define DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_3__STALL_CNT_WORD {6, 5, 6, 0, 1, 0}
#define DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_3__STALL_CNT {DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_3__STALL_CNT_WORD, 0, 0xFFFFFFFF}

//  location: tt_instruction_issue name: debug_daisy_stop_issue2
//  name: perf_cnt_instrn_issue_dbg[3] alias: perf_cnt note: 
//  name: grant_cnt
#define DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_3__GRANT_CNT_WORD {6, 5, 4, 0, 1, 0}
#define DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_3__GRANT_CNT {DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_3__GRANT_CNT_WORD, 0, 0xFFFFFFFF}

//  location: tt_instruction_issue name: debug_daisy_stop_issue2
//  name: perf_cnt_instrn_issue_dbg[3] alias: perf_cnt note: 
//  name: req_cnt
#define DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_3__REQ_CNT_WORD {6, 5, 2, 0, 1, 0}
#define DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_3__REQ_CNT {DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_3__REQ_CNT_WORD, 0, 0xFFFFFFFF}

//  location: tt_instruction_issue name: debug_daisy_stop_issue2
//  name: perf_cnt_instrn_issue_dbg[3] alias: perf_cnt note: 
//  name: ref_cnt
#define DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_3__REF_CNT_WORD {6, 5, 0, 0, 1, 0}
#define DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_3__REF_CNT {DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_3__REF_CNT_WORD, 0, 0xFFFFFFFF}

//  location: tt_instruction_issue name: debug_daisy_stop_issue2
//  name: perf_cnt_instrn_issue_dbg[2] alias: perf_cnt note: 
//  name: stall_cnt
#define DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_2__STALL_CNT_WORD {4, 5, 6, 0, 1, 0}
#define DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_2__STALL_CNT {DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_2__STALL_CNT_WORD, 0, 0xFFFFFFFF}

//  location: tt_instruction_issue name: debug_daisy_stop_issue2
//  name: perf_cnt_instrn_issue_dbg[2] alias: perf_cnt note: 
//  name: grant_cnt
#define DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_2__GRANT_CNT_WORD {4, 5, 4, 0, 1, 0}
#define DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_2__GRANT_CNT {DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_2__GRANT_CNT_WORD, 0, 0xFFFFFFFF}

//  location: tt_instruction_issue name: debug_daisy_stop_issue2
//  name: perf_cnt_instrn_issue_dbg[2] alias: perf_cnt note: 
//  name: req_cnt
#define DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_2__REQ_CNT_WORD {4, 5, 2, 0, 1, 0}
#define DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_2__REQ_CNT {DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_2__REQ_CNT_WORD, 0, 0xFFFFFFFF}

//  location: tt_instruction_issue name: debug_daisy_stop_issue2
//  name: perf_cnt_instrn_issue_dbg[2] alias: perf_cnt note: 
//  name: ref_cnt
#define DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_2__REF_CNT_WORD {4, 5, 0, 0, 1, 0}
#define DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_2__REF_CNT {DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_2__REF_CNT_WORD, 0, 0xFFFFFFFF}

//  location: tt_instruction_issue name: debug_daisy_stop_issue2
//  name: perf_cnt_instrn_issue_dbg[1] alias: perf_cnt note: 
//  name: stall_cnt
#define DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_1__STALL_CNT_WORD {2, 5, 6, 0, 1, 0}
#define DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_1__STALL_CNT {DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_1__STALL_CNT_WORD, 0, 0xFFFFFFFF}

//  location: tt_instruction_issue name: debug_daisy_stop_issue2
//  name: perf_cnt_instrn_issue_dbg[1] alias: perf_cnt note: 
//  name: grant_cnt
#define DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_1__GRANT_CNT_WORD {2, 5, 4, 0, 1, 0}
#define DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_1__GRANT_CNT {DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_1__GRANT_CNT_WORD, 0, 0xFFFFFFFF}

//  location: tt_instruction_issue name: debug_daisy_stop_issue2
//  name: perf_cnt_instrn_issue_dbg[1] alias: perf_cnt note: 
//  name: req_cnt
#define DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_1__REQ_CNT_WORD {2, 5, 2, 0, 1, 0}
#define DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_1__REQ_CNT {DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_1__REQ_CNT_WORD, 0, 0xFFFFFFFF}

//  location: tt_instruction_issue name: debug_daisy_stop_issue2
//  name: perf_cnt_instrn_issue_dbg[1] alias: perf_cnt note: 
//  name: ref_cnt
#define DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_1__REF_CNT_WORD {2, 5, 0, 0, 1, 0}
#define DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_1__REF_CNT {DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_1__REF_CNT_WORD, 0, 0xFFFFFFFF}

//  location: tt_instruction_issue name: debug_daisy_stop_issue2
//  name: perf_cnt_instrn_issue_dbg[0] alias: perf_cnt note: 
//  name: stall_cnt
#define DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_0__STALL_CNT_WORD {0, 5, 6, 0, 1, 0}
#define DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_0__STALL_CNT {DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_0__STALL_CNT_WORD, 0, 0xFFFFFFFF}

//  location: tt_instruction_issue name: debug_daisy_stop_issue2
//  name: perf_cnt_instrn_issue_dbg[0] alias: perf_cnt note: 
//  name: grant_cnt
#define DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_0__GRANT_CNT_WORD {0, 5, 4, 0, 1, 0}
#define DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_0__GRANT_CNT {DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_0__GRANT_CNT_WORD, 0, 0xFFFFFFFF}

//  location: tt_instruction_issue name: debug_daisy_stop_issue2
//  name: perf_cnt_instrn_issue_dbg[0] alias: perf_cnt note: 
//  name: req_cnt
#define DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_0__REQ_CNT_WORD {0, 5, 2, 0, 1, 0}
#define DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_0__REQ_CNT {DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_0__REQ_CNT_WORD, 0, 0xFFFFFFFF}

//  location: tt_instruction_issue name: debug_daisy_stop_issue2
//  name: perf_cnt_instrn_issue_dbg[0] alias: perf_cnt note: 
//  name: ref_cnt
#define DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_0__REF_CNT_WORD {0, 5, 0, 0, 1, 0}
#define DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_0__REF_CNT {DEBUG_BUS_PERF_CNT_INSTRN_ISSUE_DBG_0__REF_CNT_WORD, 0, 0xFFFFFFFF}

//  location: tt_instruction_issue name: debug_daisy_stop_issue3
//  name: debug_issue2_in[5] alias: sfpu_debug_issue
//  name: dbg_dest_sfpu_zero_return
#define DEBUG_BUS_DBG_DEST_SFPU_ZERO_RETURN_WORD {11, 6, 0, 0, 1, 0}
#define DEBUG_BUS_DBG_DEST_SFPU_ZERO_RETURN {DEBUG_BUS_DBG_DEST_SFPU_ZERO_RETURN_WORD, 1, 0xFF}

//  location: tt_instruction_issue name: debug_daisy_stop_issue3
//  name: debug_issue2_in[5] alias: sfpu_debug_issue
//  name: dest_sfpu_wr_en
#define DEBUG_BUS_DEST_SFPU_WR_EN__6_0_WORD {10, 6, 6, 0, 1, 0}
#define DEBUG_BUS_DEST_SFPU_WR_EN__6_0 {DEBUG_BUS_DEST_SFPU_WR_EN__6_0_WORD, 25, 0x7F}

//  location: tt_instruction_issue name: debug_daisy_stop_issue3
//  name: debug_issue2_in[5] alias: sfpu_debug_issue
//  name: dest_sfpu_wr_en
#define DEBUG_BUS_DEST_SFPU_WR_EN__7_7_WORD {11, 6, 0, 0, 1, 0}
#define DEBUG_BUS_DEST_SFPU_WR_EN__7_7 {DEBUG_BUS_DEST_SFPU_WR_EN__7_7_WORD, 0, 0x1}

//  location: tt_instruction_issue name: debug_daisy_stop_issue3
//  name: debug_issue2_in[5] alias: sfpu_debug_issue
//  name: dest_sfpu_rd_en
#define DEBUG_BUS_DEST_SFPU_RD_EN_WORD {10, 6, 6, 0, 1, 0}
#define DEBUG_BUS_DEST_SFPU_RD_EN {DEBUG_BUS_DEST_SFPU_RD_EN_WORD, 17, 0xFF}

//  location: tt_instruction_issue name: debug_daisy_stop_issue3
//  name: debug_issue2_in[5] alias: sfpu_debug_issue
//  name: sfpu_store_32bits_s1
#define DEBUG_BUS_SFPU_STORE_32BITS_S1_WORD {10, 6, 6, 0, 1, 0}
#define DEBUG_BUS_SFPU_STORE_32BITS_S1 {DEBUG_BUS_SFPU_STORE_32BITS_S1_WORD, 16, 0x1}

//  location: tt_instruction_issue name: debug_daisy_stop_issue3
//  name: debug_issue2_in[5] alias: sfpu_debug_issue
//  name: sfpu_load_32bits_s1
#define DEBUG_BUS_SFPU_LOAD_32BITS_S1_WORD {10, 6, 6, 0, 1, 0}
#define DEBUG_BUS_SFPU_LOAD_32BITS_S1 {DEBUG_BUS_SFPU_LOAD_32BITS_S1_WORD, 15, 0x1}

//  location: tt_instruction_issue name: debug_daisy_stop_issue3
//  name: debug_issue2_in[5] alias: sfpu_debug_issue
//  name: sfpu_dst_reg_addr_s1_q
#define DEBUG_BUS_SFPU_DST_REG_ADDR_S1_Q_WORD {10, 6, 6, 0, 1, 0}
#define DEBUG_BUS_SFPU_DST_REG_ADDR_S1_Q {DEBUG_BUS_SFPU_DST_REG_ADDR_S1_Q_WORD, 3, 0x3FF}

//  location: tt_instruction_issue name: debug_daisy_stop_issue3
//  name: debug_issue2_in[5] alias: sfpu_debug_issue
//  name: sfpu_update_zero_flags_s1
#define DEBUG_BUS_SFPU_UPDATE_ZERO_FLAGS_S1_WORD {10, 6, 6, 0, 1, 0}
#define DEBUG_BUS_SFPU_UPDATE_ZERO_FLAGS_S1 {DEBUG_BUS_SFPU_UPDATE_ZERO_FLAGS_S1_WORD, 2, 0x1}

//  location: tt_instruction_issue name: debug_daisy_stop_issue3
//  name: debug_issue2_in[5] alias: sfpu_debug_issue
//  name: sfpu_instr_valid_th_s1s4
#define DEBUG_BUS_SFPU_INSTR_VALID_TH_S1S4__0_0_WORD {10, 6, 4, 0, 1, 0}
#define DEBUG_BUS_SFPU_INSTR_VALID_TH_S1S4__0_0 {DEBUG_BUS_SFPU_INSTR_VALID_TH_S1S4__0_0_WORD, 31, 0x1}

//  location: tt_instruction_issue name: debug_daisy_stop_issue3
//  name: debug_issue2_in[5] alias: sfpu_debug_issue
//  name: sfpu_instr_valid_th_s1s4
#define DEBUG_BUS_SFPU_INSTR_VALID_TH_S1S4__2_1_WORD {10, 6, 6, 0, 1, 0}
#define DEBUG_BUS_SFPU_INSTR_VALID_TH_S1S4__2_1 {DEBUG_BUS_SFPU_INSTR_VALID_TH_S1S4__2_1_WORD, 0, 0x3}

//  location: tt_instruction_issue name: debug_daisy_stop_issue3
//  name: debug_issue2_in[5] alias: sfpu_debug_issue
//  name: sfpu_empty
#define DEBUG_BUS_SFPU_EMPTY_WORD {10, 6, 4, 0, 1, 0}
#define DEBUG_BUS_SFPU_EMPTY {DEBUG_BUS_SFPU_EMPTY_WORD, 28, 0x7}

//  location: tt_instruction_issue name: debug_daisy_stop_issue3
//  name: debug_issue2_in[5] alias: sfpu_debug_issue
//  name: sfpu_active_q
#define DEBUG_BUS_SFPU_ACTIVE_Q_WORD {10, 6, 4, 0, 1, 0}
#define DEBUG_BUS_SFPU_ACTIVE_Q {DEBUG_BUS_SFPU_ACTIVE_Q_WORD, 25, 0x7}

//  location: tt_instruction_issue name: debug_daisy_stop_issue3
//  name: debug_issue2_in[5] alias: sfpu_debug_issue
//  name: sfpu_winner_combo_s0
#define DEBUG_BUS_SFPU_WINNER_COMBO_S0_WORD {10, 6, 4, 0, 1, 0}
#define DEBUG_BUS_SFPU_WINNER_COMBO_S0 {DEBUG_BUS_SFPU_WINNER_COMBO_S0_WORD, 22, 0x7}

//  location: tt_instruction_issue name: debug_daisy_stop_issue3
//  name: debug_issue2_in[5] alias: sfpu_debug_issue
//  name: i_sfpu_busy
#define DEBUG_BUS_I_SFPU_BUSY_WORD {10, 6, 4, 0, 1, 0}
#define DEBUG_BUS_I_SFPU_BUSY {DEBUG_BUS_I_SFPU_BUSY_WORD, 21, 0x1}

//  location: tt_instruction_issue name: debug_daisy_stop_issue3
//  name: debug_issue2_in[5] alias: sfpu_debug_issue
//  name: sfpu_instrn_pipe_ack_s0
#define DEBUG_BUS_SFPU_INSTRN_PIPE_ACK_S0_WORD {10, 6, 4, 0, 1, 0}
#define DEBUG_BUS_SFPU_INSTRN_PIPE_ACK_S0 {DEBUG_BUS_SFPU_INSTRN_PIPE_ACK_S0_WORD, 20, 0x1}

//  location: tt_instruction_issue name: debug_daisy_stop_issue3
//  name: debug_issue2_in[5] alias: sfpu_debug_issue
//  name: sfpu_instrnbuf_rden_s1
#define DEBUG_BUS_SFPU_INSTRNBUF_RDEN_S1_WORD {10, 6, 4, 0, 1, 0}
#define DEBUG_BUS_SFPU_INSTRNBUF_RDEN_S1 {DEBUG_BUS_SFPU_INSTRNBUF_RDEN_S1_WORD, 19, 0x1}

//  location: tt_instruction_issue name: debug_daisy_stop_issue3
//  name: debug_issue2_in[5] alias: sfpu_debug_issue
//  name: sfpu_instruction_issue_stall
#define DEBUG_BUS_SFPU_INSTRUCTION_ISSUE_STALL_WORD {10, 6, 4, 0, 1, 0}
#define DEBUG_BUS_SFPU_INSTRUCTION_ISSUE_STALL {DEBUG_BUS_SFPU_INSTRUCTION_ISSUE_STALL_WORD, 18, 0x1}

//  location: tt_instruction_issue name: debug_daisy_stop_issue3
//  name: debug_issue2_in[5] alias: sfpu_debug_issue
//  name: sfpu_instrn_valid_s1
#define DEBUG_BUS_SFPU_INSTRN_VALID_S1_WORD {10, 6, 4, 0, 1, 0}
#define DEBUG_BUS_SFPU_INSTRN_VALID_S1 {DEBUG_BUS_SFPU_INSTRN_VALID_S1_WORD, 17, 0x1}

//  location: tt_instruction_issue name: debug_daisy_stop_issue3
//  name: debug_issue2_in[4] alias: regblocks_debug_bus_0
//  name: math_srcb_done
#define DEBUG_BUS_MATH_SRCB_DONE_WORD {9, 6, 6, 0, 1, 0}
#define DEBUG_BUS_MATH_SRCB_DONE {DEBUG_BUS_MATH_SRCB_DONE_WORD, 28, 0x3}

//  location: tt_instruction_issue name: debug_daisy_stop_issue3
//  name: debug_issue2_in[4] alias: regblocks_debug_bus_0
//  name: srcb_write_done
#define DEBUG_BUS_SRCB_WRITE_DONE_WORD {9, 6, 6, 0, 1, 0}
#define DEBUG_BUS_SRCB_WRITE_DONE {DEBUG_BUS_SRCB_WRITE_DONE_WORD, 27, 0x1}

//  location: tt_instruction_issue name: debug_daisy_stop_issue3
//  name: debug_issue2_in[4] alias: regblocks_debug_bus_0
//  name: clr_src_b
#define DEBUG_BUS_CLR_SRC_B_WORD {9, 6, 6, 0, 1, 0}
#define DEBUG_BUS_CLR_SRC_B {DEBUG_BUS_CLR_SRC_B_WORD, 26, 0x1}

//  location: tt_instruction_issue name: debug_daisy_stop_issue3
//  name: debug_issue2_in[4] alias: regblocks_debug_bus_0
//  name: tdma_unpack_clr_src_b_ctrl
#define DEBUG_BUS_TDMA_UNPACK_CLR_SRC_B_CTRL_WORD {9, 6, 6, 0, 1, 0}
#define DEBUG_BUS_TDMA_UNPACK_CLR_SRC_B_CTRL {DEBUG_BUS_TDMA_UNPACK_CLR_SRC_B_CTRL_WORD, 21, 0x1F}

//  location: tt_instruction_issue name: debug_daisy_stop_issue3
//  name: debug_issue2_in[4] alias: regblocks_debug_bus_0
//  name: clr_all_banks
#define DEBUG_BUS_CLR_ALL_BANKS_WORD {9, 6, 6, 0, 1, 0}
#define DEBUG_BUS_CLR_ALL_BANKS {DEBUG_BUS_CLR_ALL_BANKS_WORD, 20, 0x1}

//  location: tt_instruction_issue name: debug_daisy_stop_issue3
//  name: debug_issue2_in[4] alias: regblocks_debug_bus_0
//  name: reset_datavalid
#define DEBUG_BUS_RESET_DATAVALID_WORD {9, 6, 6, 0, 1, 0}
#define DEBUG_BUS_RESET_DATAVALID {DEBUG_BUS_RESET_DATAVALID_WORD, 19, 0x1}

//  location: tt_instruction_issue name: debug_daisy_stop_issue3
//  name: debug_issue2_in[4] alias: regblocks_debug_bus_0
//  name: disable_srcb_dvalid_clear
#define DEBUG_BUS_DISABLE_SRCB_DVALID_CLEAR_WORD {9, 6, 6, 0, 1, 0}
#define DEBUG_BUS_DISABLE_SRCB_DVALID_CLEAR {DEBUG_BUS_DISABLE_SRCB_DVALID_CLEAR_WORD, 17, 0x3}

//  location: tt_instruction_issue name: debug_daisy_stop_issue3
//  name: debug_issue2_in[4] alias: regblocks_debug_bus_0
//  name: disable_srcb_bank_switch
#define DEBUG_BUS_DISABLE_SRCB_BANK_SWITCH_WORD {9, 6, 6, 0, 1, 0}
#define DEBUG_BUS_DISABLE_SRCB_BANK_SWITCH {DEBUG_BUS_DISABLE_SRCB_BANK_SWITCH_WORD, 15, 0x3}

//  location: tt_instruction_issue name: debug_daisy_stop_issue3
//  name: debug_issue2_in[4] alias: regblocks_debug_bus_0
//  name: fpu_op_valid
#define DEBUG_BUS_FPU_OP_VALID_WORD {9, 6, 6, 0, 1, 0}
#define DEBUG_BUS_FPU_OP_VALID {DEBUG_BUS_FPU_OP_VALID_WORD, 13, 0x3}

//  location: tt_instruction_issue name: debug_daisy_stop_issue3
//  name: debug_issue2_in[4] alias: regblocks_debug_bus_0
//  name: i_CG_SRC_PIPELINE_GateSrcBPipeEn
#define DEBUG_BUS_I_CG_SRC_PIPELINE_GATESRCBPIPEEN_WORD {9, 6, 6, 0, 1, 0}
#define DEBUG_BUS_I_CG_SRC_PIPELINE_GATESRCBPIPEEN {DEBUG_BUS_I_CG_SRC_PIPELINE_GATESRCBPIPEEN_WORD, 12, 0x1}

//  location: tt_instruction_issue name: debug_daisy_stop_issue3
//  name: debug_issue2_in[4] alias: regblocks_debug_bus_0
//  name: gate_srcb_src_pipeline_rst
#define DEBUG_BUS_GATE_SRCB_SRC_PIPELINE_RST_WORD {9, 6, 6, 0, 1, 0}
#define DEBUG_BUS_GATE_SRCB_SRC_PIPELINE_RST {DEBUG_BUS_GATE_SRCB_SRC_PIPELINE_RST_WORD, 11, 0x1}

//  location: tt_instruction_issue name: debug_daisy_stop_issue3
//  name: debug_issue2_in[4] alias: regblocks_debug_bus_0
//  name: srcb_data_valid
#define DEBUG_BUS_SRCB_DATA_VALID_WORD {9, 6, 6, 0, 1, 0}
#define DEBUG_BUS_SRCB_DATA_VALID {DEBUG_BUS_SRCB_DATA_VALID_WORD, 9, 0x3}

//  location: tt_instruction_issue name: debug_daisy_stop_issue3
//  name: debug_issue2_in[4] alias: regblocks_debug_bus_0
//  name: srcb_data_valid_exp
#define DEBUG_BUS_SRCB_DATA_VALID_EXP_WORD {9, 6, 6, 0, 1, 0}
#define DEBUG_BUS_SRCB_DATA_VALID_EXP {DEBUG_BUS_SRCB_DATA_VALID_EXP_WORD, 7, 0x3}

//  location: tt_instruction_issue name: debug_daisy_stop_issue3
//  name: debug_issue2_in[4] alias: regblocks_debug_bus_0
//  name: srcb_write_math_id
#define DEBUG_BUS_SRCB_WRITE_MATH_ID_WORD {9, 6, 6, 0, 1, 0}
#define DEBUG_BUS_SRCB_WRITE_MATH_ID {DEBUG_BUS_SRCB_WRITE_MATH_ID_WORD, 6, 0x1}

//  location: tt_instruction_issue name: debug_daisy_stop_issue3
//  name: debug_issue2_in[4] alias: regblocks_debug_bus_0
//  name: srcb_read_math_id
#define DEBUG_BUS_SRCB_READ_MATH_ID_WORD {9, 6, 6, 0, 1, 0}
#define DEBUG_BUS_SRCB_READ_MATH_ID {DEBUG_BUS_SRCB_READ_MATH_ID_WORD, 5, 0x1}

//  location: tt_instruction_issue name: debug_daisy_stop_issue3
//  name: debug_issue2_in[4] alias: regblocks_debug_bus_0
//  name: srcb_read_math_id_exp
#define DEBUG_BUS_SRCB_READ_MATH_ID_EXP_WORD {9, 6, 6, 0, 1, 0}
#define DEBUG_BUS_SRCB_READ_MATH_ID_EXP {DEBUG_BUS_SRCB_READ_MATH_ID_EXP_WORD, 4, 0x1}

//  location: tt_instruction_issue name: debug_daisy_stop_issue3
//  name: debug_issue2_in[4] alias: regblocks_debug_bus_0
//  name: srcb_addr_chg_track_state_exp
#define DEBUG_BUS_SRCB_ADDR_CHG_TRACK_STATE_EXP_WORD {9, 6, 6, 0, 1, 0}
#define DEBUG_BUS_SRCB_ADDR_CHG_TRACK_STATE_EXP {DEBUG_BUS_SRCB_ADDR_CHG_TRACK_STATE_EXP_WORD, 2, 0x3}

//  location: tt_instruction_issue name: debug_daisy_stop_issue3
//  name: debug_issue2_in[4] alias: regblocks_debug_bus_0
//  name: srcb_addr_chg_track_state_man
#define DEBUG_BUS_SRCB_ADDR_CHG_TRACK_STATE_MAN_WORD {9, 6, 6, 0, 1, 0}
#define DEBUG_BUS_SRCB_ADDR_CHG_TRACK_STATE_MAN {DEBUG_BUS_SRCB_ADDR_CHG_TRACK_STATE_MAN_WORD, 0, 0x3}

//  location: tt_instruction_issue name: debug_daisy_stop_issue3
//  name: debug_issue2_in[4] alias: regblocks_debug_bus_0
//  name: regblocks_debug_bus_0_pre
//  name: i_dest_fp32_read_en
#define DEBUG_BUS_I_DEST_FP32_READ_EN_WORD {9, 6, 4, 0, 1, 0}
#define DEBUG_BUS_I_DEST_FP32_READ_EN {DEBUG_BUS_I_DEST_FP32_READ_EN_WORD, 12, 0xF}

//  location: tt_instruction_issue name: debug_daisy_stop_issue3
//  name: debug_issue2_in[4] alias: regblocks_debug_bus_0
//  name: regblocks_debug_bus_0_pre
//  name: i_pack_unsigned
#define DEBUG_BUS_I_PACK_UNSIGNED_WORD {9, 6, 4, 0, 1, 0}
#define DEBUG_BUS_I_PACK_UNSIGNED {DEBUG_BUS_I_PACK_UNSIGNED_WORD, 8, 0xF}

//  location: tt_instruction_issue name: debug_daisy_stop_issue3
//  name: debug_issue2_in[4] alias: regblocks_debug_bus_0
//  name: regblocks_debug_bus_0_pre
//  name: i_dest_read_int8
#define DEBUG_BUS_I_DEST_READ_INT8_WORD {9, 6, 4, 0, 1, 0}
#define DEBUG_BUS_I_DEST_READ_INT8 {DEBUG_BUS_I_DEST_READ_INT8_WORD, 4, 0xF}

//  location: tt_instruction_issue name: debug_daisy_stop_issue3
//  name: debug_issue2_in[4] alias: regblocks_debug_bus_0
//  name: regblocks_debug_bus_0_pre
//  name: i_gasket_round_10b_mant
#define DEBUG_BUS_I_GASKET_ROUND_10B_MANT_WORD {9, 6, 4, 0, 1, 0}
#define DEBUG_BUS_I_GASKET_ROUND_10B_MANT {DEBUG_BUS_I_GASKET_ROUND_10B_MANT_WORD, 0, 0xF}

//  location: tt_instruction_issue name: debug_daisy_stop_issue3
//  name: debug_issue2_in[4] alias: regblocks_debug_bus_0
//  name: regblocks_debug_bus_0_pre
//  name: i_pack_req_dest_output_alu_format
#define DEBUG_BUS_I_PACK_REQ_DEST_OUTPUT_ALU_FORMAT_WORD {9, 6, 2, 0, 1, 0}
#define DEBUG_BUS_I_PACK_REQ_DEST_OUTPUT_ALU_FORMAT {DEBUG_BUS_I_PACK_REQ_DEST_OUTPUT_ALU_FORMAT_WORD, 16, 0xFFFF}

//  location: tt_instruction_issue name: debug_daisy_stop_issue3
//  name: debug_issue2_in[4] alias: regblocks_debug_bus_0
//  name: regblocks_debug_bus_0_pre
//  name: i_pack_req_dest_x_pos
#define DEBUG_BUS_I_PACK_REQ_DEST_X_POS_WORD {9, 6, 2, 0, 1, 0}
#define DEBUG_BUS_I_PACK_REQ_DEST_X_POS {DEBUG_BUS_I_PACK_REQ_DEST_X_POS_WORD, 8, 0xFF}

//  location: tt_instruction_issue name: debug_daisy_stop_issue3
//  name: debug_issue2_in[4] alias: regblocks_debug_bus_0
//  name: regblocks_debug_bus_0_pre
//  name: i_pack_req_dest_ds_rate
#define DEBUG_BUS_I_PACK_REQ_DEST_DS_RATE__3_0_WORD {9, 6, 0, 0, 1, 0}
#define DEBUG_BUS_I_PACK_REQ_DEST_DS_RATE__3_0 {DEBUG_BUS_I_PACK_REQ_DEST_DS_RATE__3_0_WORD, 28, 0xF}

//  location: tt_instruction_issue name: debug_daisy_stop_issue3
//  name: debug_issue2_in[4] alias: regblocks_debug_bus_0
//  name: regblocks_debug_bus_0_pre
//  name: i_pack_req_dest_ds_rate
#define DEBUG_BUS_I_PACK_REQ_DEST_DS_RATE__11_4_WORD {9, 6, 2, 0, 1, 0}
#define DEBUG_BUS_I_PACK_REQ_DEST_DS_RATE__11_4 {DEBUG_BUS_I_PACK_REQ_DEST_DS_RATE__11_4_WORD, 0, 0xFF}

//  location: tt_instruction_issue name: debug_daisy_stop_issue3
//  name: debug_issue2_in[4] alias: regblocks_debug_bus_0
//  name: regblocks_debug_bus_0_pre
//  name: i_pack_req_dest_ds_mask
#define DEBUG_BUS_I_PACK_REQ_DEST_DS_MASK__3_0_WORD {8, 6, 4, 0, 1, 0}
#define DEBUG_BUS_I_PACK_REQ_DEST_DS_MASK__3_0 {DEBUG_BUS_I_PACK_REQ_DEST_DS_MASK__3_0_WORD, 28, 0xF}

//  location: tt_instruction_issue name: debug_daisy_stop_issue3
//  name: debug_issue2_in[4] alias: regblocks_debug_bus_0
//  name: regblocks_debug_bus_0_pre
//  name: i_pack_req_dest_ds_mask
#define DEBUG_BUS_I_PACK_REQ_DEST_DS_MASK__35_4_WORD {8, 6, 6, 0, 1, 0}
#define DEBUG_BUS_I_PACK_REQ_DEST_DS_MASK__35_4 {DEBUG_BUS_I_PACK_REQ_DEST_DS_MASK__35_4_WORD, 0, 0xFFFFFFFF}

//  location: tt_instruction_issue name: debug_daisy_stop_issue3
//  name: debug_issue2_in[4] alias: regblocks_debug_bus_0
//  name: regblocks_debug_bus_0_pre
//  name: i_pack_req_dest_ds_mask
#define DEBUG_BUS_I_PACK_REQ_DEST_DS_MASK__63_36_WORD {9, 6, 0, 0, 1, 0}
#define DEBUG_BUS_I_PACK_REQ_DEST_DS_MASK__63_36 {DEBUG_BUS_I_PACK_REQ_DEST_DS_MASK__63_36_WORD, 0, 0xFFFFFFF}

//  location: tt_instruction_issue name: debug_daisy_stop_issue3
//  name: debug_issue2_in[4] alias: regblocks_debug_bus_0
//  name: regblocks_debug_bus_0_pre
//  name: i_packer_z_pos
#define DEBUG_BUS_I_PACKER_Z_POS_WORD {8, 6, 4, 0, 1, 0}
#define DEBUG_BUS_I_PACKER_Z_POS {DEBUG_BUS_I_PACKER_Z_POS_WORD, 4, 0xFFFFFF}

//  location: tt_instruction_issue name: debug_daisy_stop_issue3
//  name: debug_issue2_in[4] alias: regblocks_debug_bus_0
//  name: regblocks_debug_bus_0_pre
//  name: i_packer_edge_mask
#define DEBUG_BUS_I_PACKER_EDGE_MASK__27_0_WORD {8, 6, 0, 0, 1, 0}
#define DEBUG_BUS_I_PACKER_EDGE_MASK__27_0 {DEBUG_BUS_I_PACKER_EDGE_MASK__27_0_WORD, 4, 0xFFFFFFF}

//  location: tt_instruction_issue name: debug_daisy_stop_issue3
//  name: debug_issue2_in[4] alias: regblocks_debug_bus_0
//  name: regblocks_debug_bus_0_pre
//  name: i_packer_edge_mask
#define DEBUG_BUS_I_PACKER_EDGE_MASK__59_28_WORD {8, 6, 2, 0, 1, 0}
#define DEBUG_BUS_I_PACKER_EDGE_MASK__59_28 {DEBUG_BUS_I_PACKER_EDGE_MASK__59_28_WORD, 0, 0xFFFFFFFF}

//  location: tt_instruction_issue name: debug_daisy_stop_issue3
//  name: debug_issue2_in[4] alias: regblocks_debug_bus_0
//  name: regblocks_debug_bus_0_pre
//  name: i_packer_edge_mask
#define DEBUG_BUS_I_PACKER_EDGE_MASK__63_60_WORD {8, 6, 4, 0, 1, 0}
#define DEBUG_BUS_I_PACKER_EDGE_MASK__63_60 {DEBUG_BUS_I_PACKER_EDGE_MASK__63_60_WORD, 0, 0xF}

//  location: tt_instruction_issue name: debug_daisy_stop_issue3
//  name: debug_issue2_in[4] alias: regblocks_debug_bus_0
//  name: regblocks_debug_bus_0_pre
//  name: i_packer_edge_mask_mode
#define DEBUG_BUS_I_PACKER_EDGE_MASK_MODE_WORD {8, 6, 0, 0, 1, 0}
#define DEBUG_BUS_I_PACKER_EDGE_MASK_MODE {DEBUG_BUS_I_PACKER_EDGE_MASK_MODE_WORD, 0, 0xF}

//  location: tt_instruction_issue name: debug_daisy_stop_issue3
//  name: debug_issue2_in[3]
//  name: dec_instr_single_output_row
#define DEBUG_BUS_DEC_INSTR_SINGLE_OUTPUT_ROW_WORD {7, 6, 4, 0, 1, 0}
#define DEBUG_BUS_DEC_INSTR_SINGLE_OUTPUT_ROW {DEBUG_BUS_DEC_INSTR_SINGLE_OUTPUT_ROW_WORD, 4, 0x1}

//  location: tt_instruction_issue name: debug_daisy_stop_issue3
//  name: debug_issue2_in[3]
//  name: curr_issue_instr_dest_fpu_addr
#define DEBUG_BUS_CURR_ISSUE_INSTR_DEST_FPU_ADDR__5_0_WORD {7, 6, 2, 0, 1, 0}
#define DEBUG_BUS_CURR_ISSUE_INSTR_DEST_FPU_ADDR__5_0 {DEBUG_BUS_CURR_ISSUE_INSTR_DEST_FPU_ADDR__5_0_WORD, 26, 0x3F}

//  location: tt_instruction_issue name: debug_daisy_stop_issue3
//  name: debug_issue2_in[3]
//  name: curr_issue_instr_dest_fpu_addr
#define DEBUG_BUS_CURR_ISSUE_INSTR_DEST_FPU_ADDR__9_6_WORD {7, 6, 4, 0, 1, 0}
#define DEBUG_BUS_CURR_ISSUE_INSTR_DEST_FPU_ADDR__9_6 {DEBUG_BUS_CURR_ISSUE_INSTR_DEST_FPU_ADDR__9_6_WORD, 0, 0xF}

//  location: tt_instruction_issue name: debug_daisy_stop_issue3
//  name: debug_issue2_in[3]
//  name: dest_wrmask
#define DEBUG_BUS_DEST_WRMASK_WORD {6, 6, 6, 0, 1, 0}
#define DEBUG_BUS_DEST_WRMASK {DEBUG_BUS_DEST_WRMASK_WORD, 13, 0xF}

//  location: tt_instruction_issue name: debug_daisy_stop_issue3
//  name: debug_issue2_in[3]
//  name: dest_fpu_wr_en
#define DEBUG_BUS_DEST_FPU_WR_EN_WORD {6, 6, 6, 0, 1, 0}
#define DEBUG_BUS_DEST_FPU_WR_EN {DEBUG_BUS_DEST_FPU_WR_EN_WORD, 5, 0xFF}

//  location: tt_instruction_issue name: debug_daisy_stop_issue3
//  name: debug_issue2_in[3]
//  name: dest_fpu_rd_en
#define DEBUG_BUS_DEST_FPU_RD_EN__1_0_WORD {6, 6, 6, 0, 1, 0}
#define DEBUG_BUS_DEST_FPU_RD_EN__1_0 {DEBUG_BUS_DEST_FPU_RD_EN__1_0_WORD, 3, 0x3}

//  location: tt_instruction_issue name: debug_daisy_stop_issue3
//  name: debug_issue2_in[3]
//  name: pack_req_fifo_wren
#define DEBUG_BUS_PACK_REQ_FIFO_WREN_WORD {6, 6, 6, 0, 1, 0}
#define DEBUG_BUS_PACK_REQ_FIFO_WREN {DEBUG_BUS_PACK_REQ_FIFO_WREN_WORD, 2, 0x1}

//  location: tt_instruction_issue name: debug_daisy_stop_issue3
//  name: debug_issue2_in[3]
//  name: pack_req_fifo_rden
#define DEBUG_BUS_PACK_REQ_FIFO_RDEN_WORD {6, 6, 6, 0, 1, 0}
#define DEBUG_BUS_PACK_REQ_FIFO_RDEN {DEBUG_BUS_PACK_REQ_FIFO_RDEN_WORD, 1, 0x1}

//  location: tt_instruction_issue name: debug_daisy_stop_issue3
//  name: debug_issue2_in[3]
//  name: pack_req_fifo_empty
#define DEBUG_BUS_PACK_REQ_FIFO_EMPTY_WORD {6, 6, 6, 0, 1, 0}
#define DEBUG_BUS_PACK_REQ_FIFO_EMPTY {DEBUG_BUS_PACK_REQ_FIFO_EMPTY_WORD, 0, 0x1}

//  location: tt_instruction_issue name: debug_daisy_stop_issue3
//  name: debug_issue2_in[3]
//  name: pack_req_fifo_full
#define DEBUG_BUS_PACK_REQ_FIFO_FULL_WORD {6, 6, 4, 0, 1, 0}
#define DEBUG_BUS_PACK_REQ_FIFO_FULL {DEBUG_BUS_PACK_REQ_FIFO_FULL_WORD, 31, 0x1}

//  location: tt_instruction_issue name: debug_daisy_stop_issue3
//  name: debug_issue2_in[2] alias: dma_cnt_state note: TRISC2 view of PACK
//  name: dma_cnt_ch1_state alias: dma_cnt_ch_state note: ch1
//  name: w_cr
#define DEBUG_BUS_DEBUG_ISSUE2_IN_2__DMA_CNT_CH1_STATE_W_CR__7_0_WORD {5, 6, 6, 0, 1, 0}
#define DEBUG_BUS_DEBUG_ISSUE2_IN_2__DMA_CNT_CH1_STATE_W_CR__7_0 {DEBUG_BUS_DEBUG_ISSUE2_IN_2__DMA_CNT_CH1_STATE_W_CR__7_0_WORD, 24, 0xFF}

//  location: tt_instruction_issue name: debug_daisy_stop_issue3
//  name: debug_issue2_in[2] alias: dma_cnt_state note: TRISC2 view of PACK
//  name: dma_cnt_ch1_state alias: dma_cnt_ch_state note: ch1
//  name: w_counter
#define DEBUG_BUS_DEBUG_ISSUE2_IN_2__DMA_CNT_CH1_STATE_W_COUNTER__7_0_WORD {5, 6, 6, 0, 1, 0}
#define DEBUG_BUS_DEBUG_ISSUE2_IN_2__DMA_CNT_CH1_STATE_W_COUNTER__7_0 {DEBUG_BUS_DEBUG_ISSUE2_IN_2__DMA_CNT_CH1_STATE_W_COUNTER__7_0_WORD, 16, 0xFF}

//  location: tt_instruction_issue name: debug_daisy_stop_issue3
//  name: debug_issue2_in[2] alias: dma_cnt_state note: TRISC2 view of PACK
//  name: dma_cnt_ch1_state alias: dma_cnt_ch_state note: ch1
//  name: z_cr
#define DEBUG_BUS_DEBUG_ISSUE2_IN_2__DMA_CNT_CH1_STATE_Z_CR__7_0_WORD {5, 6, 6, 0, 1, 0}
#define DEBUG_BUS_DEBUG_ISSUE2_IN_2__DMA_CNT_CH1_STATE_Z_CR__7_0 {DEBUG_BUS_DEBUG_ISSUE2_IN_2__DMA_CNT_CH1_STATE_Z_CR__7_0_WORD, 8, 0xFF}

//  location: tt_instruction_issue name: debug_daisy_stop_issue3
//  name: debug_issue2_in[2] alias: dma_cnt_state note: TRISC2 view of PACK
//  name: dma_cnt_ch1_state alias: dma_cnt_ch_state note: ch1
//  name: z_counter
#define DEBUG_BUS_DEBUG_ISSUE2_IN_2__DMA_CNT_CH1_STATE_Z_COUNTER__7_0_WORD {5, 6, 6, 0, 1, 0}
#define DEBUG_BUS_DEBUG_ISSUE2_IN_2__DMA_CNT_CH1_STATE_Z_COUNTER__7_0 {DEBUG_BUS_DEBUG_ISSUE2_IN_2__DMA_CNT_CH1_STATE_Z_COUNTER__7_0_WORD, 0, 0xFF}

//  location: tt_instruction_issue name: debug_daisy_stop_issue3
//  name: debug_issue2_in[2] alias: dma_cnt_state note: TRISC2 view of PACK
//  name: dma_cnt_ch1_state alias: dma_cnt_ch_state note: ch1
//  name: y_cr
#define DEBUG_BUS_DEBUG_ISSUE2_IN_2__DMA_CNT_CH1_STATE_Y_CR_WORD {5, 6, 4, 0, 1, 0}
#define DEBUG_BUS_DEBUG_ISSUE2_IN_2__DMA_CNT_CH1_STATE_Y_CR {DEBUG_BUS_DEBUG_ISSUE2_IN_2__DMA_CNT_CH1_STATE_Y_CR_WORD, 16, 0x1FFF}

//  location: tt_instruction_issue name: debug_daisy_stop_issue3
//  name: debug_issue2_in[2] alias: dma_cnt_state note: TRISC2 view of PACK
//  name: dma_cnt_ch1_state alias: dma_cnt_ch_state note: ch1
//  name: y_counter
#define DEBUG_BUS_DEBUG_ISSUE2_IN_2__DMA_CNT_CH1_STATE_Y_COUNTER_WORD {5, 6, 4, 0, 1, 0}
#define DEBUG_BUS_DEBUG_ISSUE2_IN_2__DMA_CNT_CH1_STATE_Y_COUNTER {DEBUG_BUS_DEBUG_ISSUE2_IN_2__DMA_CNT_CH1_STATE_Y_COUNTER_WORD, 0, 0x1FFF}

//  location: tt_instruction_issue name: debug_daisy_stop_issue3
//  name: debug_issue2_in[2] alias: dma_cnt_state note: TRISC2 view of PACK
//  name: dma_cnt_ch1_state alias: dma_cnt_ch_state note: ch1
//  name: x_cr
#define DEBUG_BUS_DEBUG_ISSUE2_IN_2__DMA_CNT_CH1_STATE_X_CR__13_0_WORD {5, 6, 0, 0, 1, 0}
#define DEBUG_BUS_DEBUG_ISSUE2_IN_2__DMA_CNT_CH1_STATE_X_CR__13_0 {DEBUG_BUS_DEBUG_ISSUE2_IN_2__DMA_CNT_CH1_STATE_X_CR__13_0_WORD, 18, 0x3FFF}

//  location: tt_instruction_issue name: debug_daisy_stop_issue3
//  name: debug_issue2_in[2] alias: dma_cnt_state note: TRISC2 view of PACK
//  name: dma_cnt_ch1_state alias: dma_cnt_ch_state note: ch1
//  name: x_cr
#define DEBUG_BUS_DEBUG_ISSUE2_IN_2__DMA_CNT_CH1_STATE_X_CR__17_14_WORD {5, 6, 2, 0, 1, 0}
#define DEBUG_BUS_DEBUG_ISSUE2_IN_2__DMA_CNT_CH1_STATE_X_CR__17_14 {DEBUG_BUS_DEBUG_ISSUE2_IN_2__DMA_CNT_CH1_STATE_X_CR__17_14_WORD, 0, 0xF}

//  location: tt_instruction_issue name: debug_daisy_stop_issue3
//  name: debug_issue2_in[2] alias: dma_cnt_state note: TRISC2 view of PACK
//  name: dma_cnt_ch1_state alias: dma_cnt_ch_state note: ch1
//  name: x_counter
#define DEBUG_BUS_DEBUG_ISSUE2_IN_2__DMA_CNT_CH1_STATE_X_COUNTER_WORD {5, 6, 0, 0, 1, 0}
#define DEBUG_BUS_DEBUG_ISSUE2_IN_2__DMA_CNT_CH1_STATE_X_COUNTER {DEBUG_BUS_DEBUG_ISSUE2_IN_2__DMA_CNT_CH1_STATE_X_COUNTER_WORD, 0, 0x3FFFF}

//  location: tt_instruction_issue name: debug_daisy_stop_issue3
//  name: debug_issue2_in[2] alias: dma_cnt_state note: TRISC2 view of PACK
//  name: dma_cnt_ch0_state alias: dma_cnt_ch_state note: ch0
//  name: w_cr
#define DEBUG_BUS_DEBUG_ISSUE2_IN_2__DMA_CNT_CH0_STATE_W_CR__7_0_WORD {4, 6, 6, 0, 1, 0}
#define DEBUG_BUS_DEBUG_ISSUE2_IN_2__DMA_CNT_CH0_STATE_W_CR__7_0 {DEBUG_BUS_DEBUG_ISSUE2_IN_2__DMA_CNT_CH0_STATE_W_CR__7_0_WORD, 24, 0xFF}

//  location: tt_instruction_issue name: debug_daisy_stop_issue3
//  name: debug_issue2_in[2] alias: dma_cnt_state note: TRISC2 view of PACK
//  name: dma_cnt_ch0_state alias: dma_cnt_ch_state note: ch0
//  name: w_counter
#define DEBUG_BUS_DEBUG_ISSUE2_IN_2__DMA_CNT_CH0_STATE_W_COUNTER__7_0_WORD {4, 6, 6, 0, 1, 0}
#define DEBUG_BUS_DEBUG_ISSUE2_IN_2__DMA_CNT_CH0_STATE_W_COUNTER__7_0 {DEBUG_BUS_DEBUG_ISSUE2_IN_2__DMA_CNT_CH0_STATE_W_COUNTER__7_0_WORD, 16, 0xFF}

//  location: tt_instruction_issue name: debug_daisy_stop_issue3
//  name: debug_issue2_in[2] alias: dma_cnt_state note: TRISC2 view of PACK
//  name: dma_cnt_ch0_state alias: dma_cnt_ch_state note: ch0
//  name: z_cr
#define DEBUG_BUS_DEBUG_ISSUE2_IN_2__DMA_CNT_CH0_STATE_Z_CR__7_0_WORD {4, 6, 6, 0, 1, 0}
#define DEBUG_BUS_DEBUG_ISSUE2_IN_2__DMA_CNT_CH0_STATE_Z_CR__7_0 {DEBUG_BUS_DEBUG_ISSUE2_IN_2__DMA_CNT_CH0_STATE_Z_CR__7_0_WORD, 8, 0xFF}

//  location: tt_instruction_issue name: debug_daisy_stop_issue3
//  name: debug_issue2_in[2] alias: dma_cnt_state note: TRISC2 view of PACK
//  name: dma_cnt_ch0_state alias: dma_cnt_ch_state note: ch0
//  name: z_counter
#define DEBUG_BUS_DEBUG_ISSUE2_IN_2__DMA_CNT_CH0_STATE_Z_COUNTER__7_0_WORD {4, 6, 6, 0, 1, 0}
#define DEBUG_BUS_DEBUG_ISSUE2_IN_2__DMA_CNT_CH0_STATE_Z_COUNTER__7_0 {DEBUG_BUS_DEBUG_ISSUE2_IN_2__DMA_CNT_CH0_STATE_Z_COUNTER__7_0_WORD, 0, 0xFF}

//  location: tt_instruction_issue name: debug_daisy_stop_issue3
//  name: debug_issue2_in[2] alias: dma_cnt_state note: TRISC2 view of PACK
//  name: dma_cnt_ch0_state alias: dma_cnt_ch_state note: ch0
//  name: y_cr
#define DEBUG_BUS_DEBUG_ISSUE2_IN_2__DMA_CNT_CH0_STATE_Y_CR_WORD {4, 6, 4, 0, 1, 0}
#define DEBUG_BUS_DEBUG_ISSUE2_IN_2__DMA_CNT_CH0_STATE_Y_CR {DEBUG_BUS_DEBUG_ISSUE2_IN_2__DMA_CNT_CH0_STATE_Y_CR_WORD, 16, 0x1FFF}

//  location: tt_instruction_issue name: debug_daisy_stop_issue3
//  name: debug_issue2_in[2] alias: dma_cnt_state note: TRISC2 view of PACK
//  name: dma_cnt_ch0_state alias: dma_cnt_ch_state note: ch0
//  name: y_counter
#define DEBUG_BUS_DEBUG_ISSUE2_IN_2__DMA_CNT_CH0_STATE_Y_COUNTER_WORD {4, 6, 4, 0, 1, 0}
#define DEBUG_BUS_DEBUG_ISSUE2_IN_2__DMA_CNT_CH0_STATE_Y_COUNTER {DEBUG_BUS_DEBUG_ISSUE2_IN_2__DMA_CNT_CH0_STATE_Y_COUNTER_WORD, 0, 0x1FFF}

//  location: tt_instruction_issue name: debug_daisy_stop_issue3
//  name: debug_issue2_in[2] alias: dma_cnt_state note: TRISC2 view of PACK
//  name: dma_cnt_ch0_state alias: dma_cnt_ch_state note: ch0
//  name: x_cr
#define DEBUG_BUS_DEBUG_ISSUE2_IN_2__DMA_CNT_CH0_STATE_X_CR__13_0_WORD {4, 6, 0, 0, 1, 0}
#define DEBUG_BUS_DEBUG_ISSUE2_IN_2__DMA_CNT_CH0_STATE_X_CR__13_0 {DEBUG_BUS_DEBUG_ISSUE2_IN_2__DMA_CNT_CH0_STATE_X_CR__13_0_WORD, 18, 0x3FFF}

//  location: tt_instruction_issue name: debug_daisy_stop_issue3
//  name: debug_issue2_in[2] alias: dma_cnt_state note: TRISC2 view of PACK
//  name: dma_cnt_ch0_state alias: dma_cnt_ch_state note: ch0
//  name: x_cr
#define DEBUG_BUS_DEBUG_ISSUE2_IN_2__DMA_CNT_CH0_STATE_X_CR__17_14_WORD {4, 6, 2, 0, 1, 0}
#define DEBUG_BUS_DEBUG_ISSUE2_IN_2__DMA_CNT_CH0_STATE_X_CR__17_14 {DEBUG_BUS_DEBUG_ISSUE2_IN_2__DMA_CNT_CH0_STATE_X_CR__17_14_WORD, 0, 0xF}

//  location: tt_instruction_issue name: debug_daisy_stop_issue3
//  name: debug_issue2_in[2] alias: dma_cnt_state note: TRISC2 view of PACK
//  name: dma_cnt_ch0_state alias: dma_cnt_ch_state note: ch0
//  name: x_counter
#define DEBUG_BUS_DEBUG_ISSUE2_IN_2__DMA_CNT_CH0_STATE_X_COUNTER_WORD {4, 6, 0, 0, 1, 0}
#define DEBUG_BUS_DEBUG_ISSUE2_IN_2__DMA_CNT_CH0_STATE_X_COUNTER {DEBUG_BUS_DEBUG_ISSUE2_IN_2__DMA_CNT_CH0_STATE_X_COUNTER_WORD, 0, 0x3FFFF}

//  location: tt_instruction_issue name: debug_daisy_stop_issue3
//  name: debug_issue2_in[1] alias: dma_cnt_state note: TRISC0 view of UNP1
//  name: dma_cnt_ch1_state alias: dma_cnt_ch_state note: ch1
//  name: w_cr
#define DEBUG_BUS_DEBUG_ISSUE2_IN_1__DMA_CNT_CH1_STATE_W_CR__7_0_WORD {3, 6, 6, 0, 1, 0}
#define DEBUG_BUS_DEBUG_ISSUE2_IN_1__DMA_CNT_CH1_STATE_W_CR__7_0 {DEBUG_BUS_DEBUG_ISSUE2_IN_1__DMA_CNT_CH1_STATE_W_CR__7_0_WORD, 24, 0xFF}

//  location: tt_instruction_issue name: debug_daisy_stop_issue3
//  name: debug_issue2_in[1] alias: dma_cnt_state note: TRISC0 view of UNP1
//  name: dma_cnt_ch1_state alias: dma_cnt_ch_state note: ch1
//  name: w_counter
#define DEBUG_BUS_DEBUG_ISSUE2_IN_1__DMA_CNT_CH1_STATE_W_COUNTER__7_0_WORD {3, 6, 6, 0, 1, 0}
#define DEBUG_BUS_DEBUG_ISSUE2_IN_1__DMA_CNT_CH1_STATE_W_COUNTER__7_0 {DEBUG_BUS_DEBUG_ISSUE2_IN_1__DMA_CNT_CH1_STATE_W_COUNTER__7_0_WORD, 16, 0xFF}

//  location: tt_instruction_issue name: debug_daisy_stop_issue3
//  name: debug_issue2_in[1] alias: dma_cnt_state note: TRISC0 view of UNP1
//  name: dma_cnt_ch1_state alias: dma_cnt_ch_state note: ch1
//  name: z_cr
#define DEBUG_BUS_DEBUG_ISSUE2_IN_1__DMA_CNT_CH1_STATE_Z_CR__7_0_WORD {3, 6, 6, 0, 1, 0}
#define DEBUG_BUS_DEBUG_ISSUE2_IN_1__DMA_CNT_CH1_STATE_Z_CR__7_0 {DEBUG_BUS_DEBUG_ISSUE2_IN_1__DMA_CNT_CH1_STATE_Z_CR__7_0_WORD, 8, 0xFF}

//  location: tt_instruction_issue name: debug_daisy_stop_issue3
//  name: debug_issue2_in[1] alias: dma_cnt_state note: TRISC0 view of UNP1
//  name: dma_cnt_ch1_state alias: dma_cnt_ch_state note: ch1
//  name: z_counter
#define DEBUG_BUS_DEBUG_ISSUE2_IN_1__DMA_CNT_CH1_STATE_Z_COUNTER__7_0_WORD {3, 6, 6, 0, 1, 0}
#define DEBUG_BUS_DEBUG_ISSUE2_IN_1__DMA_CNT_CH1_STATE_Z_COUNTER__7_0 {DEBUG_BUS_DEBUG_ISSUE2_IN_1__DMA_CNT_CH1_STATE_Z_COUNTER__7_0_WORD, 0, 0xFF}

//  location: tt_instruction_issue name: debug_daisy_stop_issue3
//  name: debug_issue2_in[1] alias: dma_cnt_state note: TRISC0 view of UNP1
//  name: dma_cnt_ch1_state alias: dma_cnt_ch_state note: ch1
//  name: y_cr
#define DEBUG_BUS_DEBUG_ISSUE2_IN_1__DMA_CNT_CH1_STATE_Y_CR_WORD {3, 6, 4, 0, 1, 0}
#define DEBUG_BUS_DEBUG_ISSUE2_IN_1__DMA_CNT_CH1_STATE_Y_CR {DEBUG_BUS_DEBUG_ISSUE2_IN_1__DMA_CNT_CH1_STATE_Y_CR_WORD, 16, 0x1FFF}

//  location: tt_instruction_issue name: debug_daisy_stop_issue3
//  name: debug_issue2_in[1] alias: dma_cnt_state note: TRISC0 view of UNP1
//  name: dma_cnt_ch1_state alias: dma_cnt_ch_state note: ch1
//  name: y_counter
#define DEBUG_BUS_DEBUG_ISSUE2_IN_1__DMA_CNT_CH1_STATE_Y_COUNTER_WORD {3, 6, 4, 0, 1, 0}
#define DEBUG_BUS_DEBUG_ISSUE2_IN_1__DMA_CNT_CH1_STATE_Y_COUNTER {DEBUG_BUS_DEBUG_ISSUE2_IN_1__DMA_CNT_CH1_STATE_Y_COUNTER_WORD, 0, 0x1FFF}

//  location: tt_instruction_issue name: debug_daisy_stop_issue3
//  name: debug_issue2_in[1] alias: dma_cnt_state note: TRISC0 view of UNP1
//  name: dma_cnt_ch1_state alias: dma_cnt_ch_state note: ch1
//  name: x_cr
#define DEBUG_BUS_DEBUG_ISSUE2_IN_1__DMA_CNT_CH1_STATE_X_CR__13_0_WORD {3, 6, 0, 0, 1, 0}
#define DEBUG_BUS_DEBUG_ISSUE2_IN_1__DMA_CNT_CH1_STATE_X_CR__13_0 {DEBUG_BUS_DEBUG_ISSUE2_IN_1__DMA_CNT_CH1_STATE_X_CR__13_0_WORD, 18, 0x3FFF}

//  location: tt_instruction_issue name: debug_daisy_stop_issue3
//  name: debug_issue2_in[1] alias: dma_cnt_state note: TRISC0 view of UNP1
//  name: dma_cnt_ch1_state alias: dma_cnt_ch_state note: ch1
//  name: x_cr
#define DEBUG_BUS_DEBUG_ISSUE2_IN_1__DMA_CNT_CH1_STATE_X_CR__17_14_WORD {3, 6, 2, 0, 1, 0}
#define DEBUG_BUS_DEBUG_ISSUE2_IN_1__DMA_CNT_CH1_STATE_X_CR__17_14 {DEBUG_BUS_DEBUG_ISSUE2_IN_1__DMA_CNT_CH1_STATE_X_CR__17_14_WORD, 0, 0xF}

//  location: tt_instruction_issue name: debug_daisy_stop_issue3
//  name: debug_issue2_in[1] alias: dma_cnt_state note: TRISC0 view of UNP1
//  name: dma_cnt_ch1_state alias: dma_cnt_ch_state note: ch1
//  name: x_counter
#define DEBUG_BUS_DEBUG_ISSUE2_IN_1__DMA_CNT_CH1_STATE_X_COUNTER_WORD {3, 6, 0, 0, 1, 0}
#define DEBUG_BUS_DEBUG_ISSUE2_IN_1__DMA_CNT_CH1_STATE_X_COUNTER {DEBUG_BUS_DEBUG_ISSUE2_IN_1__DMA_CNT_CH1_STATE_X_COUNTER_WORD, 0, 0x3FFFF}

//  location: tt_instruction_issue name: debug_daisy_stop_issue3
//  name: debug_issue2_in[1] alias: dma_cnt_state note: TRISC0 view of UNP1
//  name: dma_cnt_ch0_state alias: dma_cnt_ch_state note: ch0
//  name: w_cr
#define DEBUG_BUS_DEBUG_ISSUE2_IN_1__DMA_CNT_CH0_STATE_W_CR__7_0_WORD {2, 6, 6, 0, 1, 0}
#define DEBUG_BUS_DEBUG_ISSUE2_IN_1__DMA_CNT_CH0_STATE_W_CR__7_0 {DEBUG_BUS_DEBUG_ISSUE2_IN_1__DMA_CNT_CH0_STATE_W_CR__7_0_WORD, 24, 0xFF}

//  location: tt_instruction_issue name: debug_daisy_stop_issue3
//  name: debug_issue2_in[1] alias: dma_cnt_state note: TRISC0 view of UNP1
//  name: dma_cnt_ch0_state alias: dma_cnt_ch_state note: ch0
//  name: w_counter
#define DEBUG_BUS_DEBUG_ISSUE2_IN_1__DMA_CNT_CH0_STATE_W_COUNTER__7_0_WORD {2, 6, 6, 0, 1, 0}
#define DEBUG_BUS_DEBUG_ISSUE2_IN_1__DMA_CNT_CH0_STATE_W_COUNTER__7_0 {DEBUG_BUS_DEBUG_ISSUE2_IN_1__DMA_CNT_CH0_STATE_W_COUNTER__7_0_WORD, 16, 0xFF}

//  location: tt_instruction_issue name: debug_daisy_stop_issue3
//  name: debug_issue2_in[1] alias: dma_cnt_state note: TRISC0 view of UNP1
//  name: dma_cnt_ch0_state alias: dma_cnt_ch_state note: ch0
//  name: z_cr
#define DEBUG_BUS_DEBUG_ISSUE2_IN_1__DMA_CNT_CH0_STATE_Z_CR__7_0_WORD {2, 6, 6, 0, 1, 0}
#define DEBUG_BUS_DEBUG_ISSUE2_IN_1__DMA_CNT_CH0_STATE_Z_CR__7_0 {DEBUG_BUS_DEBUG_ISSUE2_IN_1__DMA_CNT_CH0_STATE_Z_CR__7_0_WORD, 8, 0xFF}

//  location: tt_instruction_issue name: debug_daisy_stop_issue3
//  name: debug_issue2_in[1] alias: dma_cnt_state note: TRISC0 view of UNP1
//  name: dma_cnt_ch0_state alias: dma_cnt_ch_state note: ch0
//  name: z_counter
#define DEBUG_BUS_DEBUG_ISSUE2_IN_1__DMA_CNT_CH0_STATE_Z_COUNTER__7_0_WORD {2, 6, 6, 0, 1, 0}
#define DEBUG_BUS_DEBUG_ISSUE2_IN_1__DMA_CNT_CH0_STATE_Z_COUNTER__7_0 {DEBUG_BUS_DEBUG_ISSUE2_IN_1__DMA_CNT_CH0_STATE_Z_COUNTER__7_0_WORD, 0, 0xFF}

//  location: tt_instruction_issue name: debug_daisy_stop_issue3
//  name: debug_issue2_in[1] alias: dma_cnt_state note: TRISC0 view of UNP1
//  name: dma_cnt_ch0_state alias: dma_cnt_ch_state note: ch0
//  name: y_cr
#define DEBUG_BUS_DEBUG_ISSUE2_IN_1__DMA_CNT_CH0_STATE_Y_CR_WORD {2, 6, 4, 0, 1, 0}
#define DEBUG_BUS_DEBUG_ISSUE2_IN_1__DMA_CNT_CH0_STATE_Y_CR {DEBUG_BUS_DEBUG_ISSUE2_IN_1__DMA_CNT_CH0_STATE_Y_CR_WORD, 16, 0x1FFF}

//  location: tt_instruction_issue name: debug_daisy_stop_issue3
//  name: debug_issue2_in[1] alias: dma_cnt_state note: TRISC0 view of UNP1
//  name: dma_cnt_ch0_state alias: dma_cnt_ch_state note: ch0
//  name: y_counter
#define DEBUG_BUS_DEBUG_ISSUE2_IN_1__DMA_CNT_CH0_STATE_Y_COUNTER_WORD {2, 6, 4, 0, 1, 0}
#define DEBUG_BUS_DEBUG_ISSUE2_IN_1__DMA_CNT_CH0_STATE_Y_COUNTER {DEBUG_BUS_DEBUG_ISSUE2_IN_1__DMA_CNT_CH0_STATE_Y_COUNTER_WORD, 0, 0x1FFF}

//  location: tt_instruction_issue name: debug_daisy_stop_issue3
//  name: debug_issue2_in[1] alias: dma_cnt_state note: TRISC0 view of UNP1
//  name: dma_cnt_ch0_state alias: dma_cnt_ch_state note: ch0
//  name: x_cr
#define DEBUG_BUS_DEBUG_ISSUE2_IN_1__DMA_CNT_CH0_STATE_X_CR__13_0_WORD {2, 6, 0, 0, 1, 0}
#define DEBUG_BUS_DEBUG_ISSUE2_IN_1__DMA_CNT_CH0_STATE_X_CR__13_0 {DEBUG_BUS_DEBUG_ISSUE2_IN_1__DMA_CNT_CH0_STATE_X_CR__13_0_WORD, 18, 0x3FFF}

//  location: tt_instruction_issue name: debug_daisy_stop_issue3
//  name: debug_issue2_in[1] alias: dma_cnt_state note: TRISC0 view of UNP1
//  name: dma_cnt_ch0_state alias: dma_cnt_ch_state note: ch0
//  name: x_cr
#define DEBUG_BUS_DEBUG_ISSUE2_IN_1__DMA_CNT_CH0_STATE_X_CR__17_14_WORD {2, 6, 2, 0, 1, 0}
#define DEBUG_BUS_DEBUG_ISSUE2_IN_1__DMA_CNT_CH0_STATE_X_CR__17_14 {DEBUG_BUS_DEBUG_ISSUE2_IN_1__DMA_CNT_CH0_STATE_X_CR__17_14_WORD, 0, 0xF}

//  location: tt_instruction_issue name: debug_daisy_stop_issue3
//  name: debug_issue2_in[1] alias: dma_cnt_state note: TRISC0 view of UNP1
//  name: dma_cnt_ch0_state alias: dma_cnt_ch_state note: ch0
//  name: x_counter
#define DEBUG_BUS_DEBUG_ISSUE2_IN_1__DMA_CNT_CH0_STATE_X_COUNTER_WORD {2, 6, 0, 0, 1, 0}
#define DEBUG_BUS_DEBUG_ISSUE2_IN_1__DMA_CNT_CH0_STATE_X_COUNTER {DEBUG_BUS_DEBUG_ISSUE2_IN_1__DMA_CNT_CH0_STATE_X_COUNTER_WORD, 0, 0x3FFFF}

//  location: tt_instruction_issue name: debug_daisy_stop_issue3
//  name: debug_issue2_in[0] alias: dma_cnt_state note: TRISC0 view of UNP0
//  name: dma_cnt_ch1_state alias: dma_cnt_ch_state note: ch1
//  name: w_cr
#define DEBUG_BUS_DEBUG_ISSUE2_IN_0__DMA_CNT_CH1_STATE_W_CR__7_0_WORD {1, 6, 6, 0, 1, 0}
#define DEBUG_BUS_DEBUG_ISSUE2_IN_0__DMA_CNT_CH1_STATE_W_CR__7_0 {DEBUG_BUS_DEBUG_ISSUE2_IN_0__DMA_CNT_CH1_STATE_W_CR__7_0_WORD, 24, 0xFF}

//  location: tt_instruction_issue name: debug_daisy_stop_issue3
//  name: debug_issue2_in[0] alias: dma_cnt_state note: TRISC0 view of UNP0
//  name: dma_cnt_ch1_state alias: dma_cnt_ch_state note: ch1
//  name: w_counter
#define DEBUG_BUS_DEBUG_ISSUE2_IN_0__DMA_CNT_CH1_STATE_W_COUNTER__7_0_WORD {1, 6, 6, 0, 1, 0}
#define DEBUG_BUS_DEBUG_ISSUE2_IN_0__DMA_CNT_CH1_STATE_W_COUNTER__7_0 {DEBUG_BUS_DEBUG_ISSUE2_IN_0__DMA_CNT_CH1_STATE_W_COUNTER__7_0_WORD, 16, 0xFF}

//  location: tt_instruction_issue name: debug_daisy_stop_issue3
//  name: debug_issue2_in[0] alias: dma_cnt_state note: TRISC0 view of UNP0
//  name: dma_cnt_ch1_state alias: dma_cnt_ch_state note: ch1
//  name: z_cr
#define DEBUG_BUS_DEBUG_ISSUE2_IN_0__DMA_CNT_CH1_STATE_Z_CR__7_0_WORD {1, 6, 6, 0, 1, 0}
#define DEBUG_BUS_DEBUG_ISSUE2_IN_0__DMA_CNT_CH1_STATE_Z_CR__7_0 {DEBUG_BUS_DEBUG_ISSUE2_IN_0__DMA_CNT_CH1_STATE_Z_CR__7_0_WORD, 8, 0xFF}

//  location: tt_instruction_issue name: debug_daisy_stop_issue3
//  name: debug_issue2_in[0] alias: dma_cnt_state note: TRISC0 view of UNP0
//  name: dma_cnt_ch1_state alias: dma_cnt_ch_state note: ch1
//  name: z_counter
#define DEBUG_BUS_DEBUG_ISSUE2_IN_0__DMA_CNT_CH1_STATE_Z_COUNTER__7_0_WORD {1, 6, 6, 0, 1, 0}
#define DEBUG_BUS_DEBUG_ISSUE2_IN_0__DMA_CNT_CH1_STATE_Z_COUNTER__7_0 {DEBUG_BUS_DEBUG_ISSUE2_IN_0__DMA_CNT_CH1_STATE_Z_COUNTER__7_0_WORD, 0, 0xFF}

//  location: tt_instruction_issue name: debug_daisy_stop_issue3
//  name: debug_issue2_in[0] alias: dma_cnt_state note: TRISC0 view of UNP0
//  name: dma_cnt_ch1_state alias: dma_cnt_ch_state note: ch1
//  name: y_cr
#define DEBUG_BUS_DEBUG_ISSUE2_IN_0__DMA_CNT_CH1_STATE_Y_CR_WORD {1, 6, 4, 0, 1, 0}
#define DEBUG_BUS_DEBUG_ISSUE2_IN_0__DMA_CNT_CH1_STATE_Y_CR {DEBUG_BUS_DEBUG_ISSUE2_IN_0__DMA_CNT_CH1_STATE_Y_CR_WORD, 16, 0x1FFF}

//  location: tt_instruction_issue name: debug_daisy_stop_issue3
//  name: debug_issue2_in[0] alias: dma_cnt_state note: TRISC0 view of UNP0
//  name: dma_cnt_ch1_state alias: dma_cnt_ch_state note: ch1
//  name: y_counter
#define DEBUG_BUS_DEBUG_ISSUE2_IN_0__DMA_CNT_CH1_STATE_Y_COUNTER_WORD {1, 6, 4, 0, 1, 0}
#define DEBUG_BUS_DEBUG_ISSUE2_IN_0__DMA_CNT_CH1_STATE_Y_COUNTER {DEBUG_BUS_DEBUG_ISSUE2_IN_0__DMA_CNT_CH1_STATE_Y_COUNTER_WORD, 0, 0x1FFF}

//  location: tt_instruction_issue name: debug_daisy_stop_issue3
//  name: debug_issue2_in[0] alias: dma_cnt_state note: TRISC0 view of UNP0
//  name: dma_cnt_ch1_state alias: dma_cnt_ch_state note: ch1
//  name: x_cr
#define DEBUG_BUS_DEBUG_ISSUE2_IN_0__DMA_CNT_CH1_STATE_X_CR__13_0_WORD {1, 6, 0, 0, 1, 0}
#define DEBUG_BUS_DEBUG_ISSUE2_IN_0__DMA_CNT_CH1_STATE_X_CR__13_0 {DEBUG_BUS_DEBUG_ISSUE2_IN_0__DMA_CNT_CH1_STATE_X_CR__13_0_WORD, 18, 0x3FFF}

//  location: tt_instruction_issue name: debug_daisy_stop_issue3
//  name: debug_issue2_in[0] alias: dma_cnt_state note: TRISC0 view of UNP0
//  name: dma_cnt_ch1_state alias: dma_cnt_ch_state note: ch1
//  name: x_cr
#define DEBUG_BUS_DEBUG_ISSUE2_IN_0__DMA_CNT_CH1_STATE_X_CR__17_14_WORD {1, 6, 2, 0, 1, 0}
#define DEBUG_BUS_DEBUG_ISSUE2_IN_0__DMA_CNT_CH1_STATE_X_CR__17_14 {DEBUG_BUS_DEBUG_ISSUE2_IN_0__DMA_CNT_CH1_STATE_X_CR__17_14_WORD, 0, 0xF}

//  location: tt_instruction_issue name: debug_daisy_stop_issue3
//  name: debug_issue2_in[0] alias: dma_cnt_state note: TRISC0 view of UNP0
//  name: dma_cnt_ch1_state alias: dma_cnt_ch_state note: ch1
//  name: x_counter
#define DEBUG_BUS_DEBUG_ISSUE2_IN_0__DMA_CNT_CH1_STATE_X_COUNTER_WORD {1, 6, 0, 0, 1, 0}
#define DEBUG_BUS_DEBUG_ISSUE2_IN_0__DMA_CNT_CH1_STATE_X_COUNTER {DEBUG_BUS_DEBUG_ISSUE2_IN_0__DMA_CNT_CH1_STATE_X_COUNTER_WORD, 0, 0x3FFFF}

//  location: tt_instruction_issue name: debug_daisy_stop_issue3
//  name: debug_issue2_in[0] alias: dma_cnt_state note: TRISC0 view of UNP0
//  name: dma_cnt_ch0_state alias: dma_cnt_ch_state note: ch0
//  name: w_cr
#define DEBUG_BUS_DEBUG_ISSUE2_IN_0__DMA_CNT_CH0_STATE_W_CR__7_0_WORD {0, 6, 6, 0, 1, 0}
#define DEBUG_BUS_DEBUG_ISSUE2_IN_0__DMA_CNT_CH0_STATE_W_CR__7_0 {DEBUG_BUS_DEBUG_ISSUE2_IN_0__DMA_CNT_CH0_STATE_W_CR__7_0_WORD, 24, 0xFF}

//  location: tt_instruction_issue name: debug_daisy_stop_issue3
//  name: debug_issue2_in[0] alias: dma_cnt_state note: TRISC0 view of UNP0
//  name: dma_cnt_ch0_state alias: dma_cnt_ch_state note: ch0
//  name: w_counter
#define DEBUG_BUS_DEBUG_ISSUE2_IN_0__DMA_CNT_CH0_STATE_W_COUNTER__7_0_WORD {0, 6, 6, 0, 1, 0}
#define DEBUG_BUS_DEBUG_ISSUE2_IN_0__DMA_CNT_CH0_STATE_W_COUNTER__7_0 {DEBUG_BUS_DEBUG_ISSUE2_IN_0__DMA_CNT_CH0_STATE_W_COUNTER__7_0_WORD, 16, 0xFF}

//  location: tt_instruction_issue name: debug_daisy_stop_issue3
//  name: debug_issue2_in[0] alias: dma_cnt_state note: TRISC0 view of UNP0
//  name: dma_cnt_ch0_state alias: dma_cnt_ch_state note: ch0
//  name: z_cr
#define DEBUG_BUS_DEBUG_ISSUE2_IN_0__DMA_CNT_CH0_STATE_Z_CR__7_0_WORD {0, 6, 6, 0, 1, 0}
#define DEBUG_BUS_DEBUG_ISSUE2_IN_0__DMA_CNT_CH0_STATE_Z_CR__7_0 {DEBUG_BUS_DEBUG_ISSUE2_IN_0__DMA_CNT_CH0_STATE_Z_CR__7_0_WORD, 8, 0xFF}

//  location: tt_instruction_issue name: debug_daisy_stop_issue3
//  name: debug_issue2_in[0] alias: dma_cnt_state note: TRISC0 view of UNP0
//  name: dma_cnt_ch0_state alias: dma_cnt_ch_state note: ch0
//  name: z_counter
#define DEBUG_BUS_DEBUG_ISSUE2_IN_0__DMA_CNT_CH0_STATE_Z_COUNTER__7_0_WORD {0, 6, 6, 0, 1, 0}
#define DEBUG_BUS_DEBUG_ISSUE2_IN_0__DMA_CNT_CH0_STATE_Z_COUNTER__7_0 {DEBUG_BUS_DEBUG_ISSUE2_IN_0__DMA_CNT_CH0_STATE_Z_COUNTER__7_0_WORD, 0, 0xFF}

//  location: tt_instruction_issue name: debug_daisy_stop_issue3
//  name: debug_issue2_in[0] alias: dma_cnt_state note: TRISC0 view of UNP0
//  name: dma_cnt_ch0_state alias: dma_cnt_ch_state note: ch0
//  name: y_cr
#define DEBUG_BUS_DEBUG_ISSUE2_IN_0__DMA_CNT_CH0_STATE_Y_CR_WORD {0, 6, 4, 0, 1, 0}
#define DEBUG_BUS_DEBUG_ISSUE2_IN_0__DMA_CNT_CH0_STATE_Y_CR {DEBUG_BUS_DEBUG_ISSUE2_IN_0__DMA_CNT_CH0_STATE_Y_CR_WORD, 16, 0x1FFF}

//  location: tt_instruction_issue name: debug_daisy_stop_issue3
//  name: debug_issue2_in[0] alias: dma_cnt_state note: TRISC0 view of UNP0
//  name: dma_cnt_ch0_state alias: dma_cnt_ch_state note: ch0
//  name: y_counter
#define DEBUG_BUS_DEBUG_ISSUE2_IN_0__DMA_CNT_CH0_STATE_Y_COUNTER_WORD {0, 6, 4, 0, 1, 0}
#define DEBUG_BUS_DEBUG_ISSUE2_IN_0__DMA_CNT_CH0_STATE_Y_COUNTER {DEBUG_BUS_DEBUG_ISSUE2_IN_0__DMA_CNT_CH0_STATE_Y_COUNTER_WORD, 0, 0x1FFF}

//  location: tt_instruction_issue name: debug_daisy_stop_issue3
//  name: debug_issue2_in[0] alias: dma_cnt_state note: TRISC0 view of UNP0
//  name: dma_cnt_ch0_state alias: dma_cnt_ch_state note: ch0
//  name: x_cr
#define DEBUG_BUS_DEBUG_ISSUE2_IN_0__DMA_CNT_CH0_STATE_X_CR__13_0_WORD {0, 6, 0, 0, 1, 0}
#define DEBUG_BUS_DEBUG_ISSUE2_IN_0__DMA_CNT_CH0_STATE_X_CR__13_0 {DEBUG_BUS_DEBUG_ISSUE2_IN_0__DMA_CNT_CH0_STATE_X_CR__13_0_WORD, 18, 0x3FFF}

//  location: tt_instruction_issue name: debug_daisy_stop_issue3
//  name: debug_issue2_in[0] alias: dma_cnt_state note: TRISC0 view of UNP0
//  name: dma_cnt_ch0_state alias: dma_cnt_ch_state note: ch0
//  name: x_cr
#define DEBUG_BUS_DEBUG_ISSUE2_IN_0__DMA_CNT_CH0_STATE_X_CR__17_14_WORD {0, 6, 2, 0, 1, 0}
#define DEBUG_BUS_DEBUG_ISSUE2_IN_0__DMA_CNT_CH0_STATE_X_CR__17_14 {DEBUG_BUS_DEBUG_ISSUE2_IN_0__DMA_CNT_CH0_STATE_X_CR__17_14_WORD, 0, 0xF}

//  location: tt_instruction_issue name: debug_daisy_stop_issue3
//  name: debug_issue2_in[0] alias: dma_cnt_state note: TRISC0 view of UNP0
//  name: dma_cnt_ch0_state alias: dma_cnt_ch_state note: ch0
//  name: x_counter
#define DEBUG_BUS_DEBUG_ISSUE2_IN_0__DMA_CNT_CH0_STATE_X_COUNTER_WORD {0, 6, 0, 0, 1, 0}
#define DEBUG_BUS_DEBUG_ISSUE2_IN_0__DMA_CNT_CH0_STATE_X_COUNTER {DEBUG_BUS_DEBUG_ISSUE2_IN_0__DMA_CNT_CH0_STATE_X_COUNTER_WORD, 0, 0x3FFFF}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_14 alias: fpu_dbg_bus
//  name: srca_wren_resh_d[7] note: Only when h2_debug_en_q[3]
#define DEBUG_BUS_SRCA_WREN_RESH_D_7__WORD {28, 7, 4, 0, 1, 0}
#define DEBUG_BUS_SRCA_WREN_RESH_D_7_ {DEBUG_BUS_SRCA_WREN_RESH_D_7__WORD, 12, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_14 alias: fpu_dbg_bus
//  name: srca_wr_datum_en_resh_d[7] note: Only when h2_debug_en_q[3]
#define DEBUG_BUS_SRCA_WR_DATUM_EN_RESH_D_7___5_0_WORD {28, 7, 2, 0, 1, 0}
#define DEBUG_BUS_SRCA_WR_DATUM_EN_RESH_D_7___5_0 {DEBUG_BUS_SRCA_WR_DATUM_EN_RESH_D_7___5_0_WORD, 26, 0x3F}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_14 alias: fpu_dbg_bus
//  name: srca_wr_datum_en_resh_d[7] note: Only when h2_debug_en_q[3]
#define DEBUG_BUS_SRCA_WR_DATUM_EN_RESH_D_7___15_6_WORD {28, 7, 4, 0, 1, 0}
#define DEBUG_BUS_SRCA_WR_DATUM_EN_RESH_D_7___15_6 {DEBUG_BUS_SRCA_WR_DATUM_EN_RESH_D_7___15_6_WORD, 0, 0x3FF}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_14 alias: fpu_dbg_bus
//  name: srca_wraddr_resh_d[7] note: Only when h2_debug_en_q[3]
#define DEBUG_BUS_SRCA_WRADDR_RESH_D_7__WORD {28, 7, 2, 0, 1, 0}
#define DEBUG_BUS_SRCA_WRADDR_RESH_D_7_ {DEBUG_BUS_SRCA_WRADDR_RESH_D_7__WORD, 12, 0x3FFF}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_14 alias: fpu_dbg_bus
//  name: srca_wr_format_resh_d[7] note: Only when h2_debug_en_q[3]
#define DEBUG_BUS_SRCA_WR_FORMAT_RESH_D_7__WORD {28, 7, 2, 0, 1, 0}
#define DEBUG_BUS_SRCA_WR_FORMAT_RESH_D_7_ {DEBUG_BUS_SRCA_WR_FORMAT_RESH_D_7__WORD, 8, 0xF}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_14 alias: fpu_dbg_bus
//  name: h2_sfpu_dbg_bus[7] alias: cc_satisfied_s3[0] note: SFPU Instance 7, only when coresponding h2_debug_en_q
#define DEBUG_BUS_H2_SFPU_DBG_BUS_7__WORD {28, 7, 0, 0, 1, 0}
#define DEBUG_BUS_H2_SFPU_DBG_BUS_7_ {DEBUG_BUS_H2_SFPU_DBG_BUS_7__WORD, 28, 0xF}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_14 alias: fpu_dbg_bus
//  name: h2_sfpu_dbg_bus[6] alias: cc_satisfied_s3[0] note: SFPU Instance 6, only when coresponding h2_debug_en_q
#define DEBUG_BUS_H2_SFPU_DBG_BUS_6__WORD {28, 7, 0, 0, 1, 0}
#define DEBUG_BUS_H2_SFPU_DBG_BUS_6_ {DEBUG_BUS_H2_SFPU_DBG_BUS_6__WORD, 24, 0xF}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_14 alias: fpu_dbg_bus
//  name: h2_sfpu_dbg_bus[5] alias: cc_satisfied_s3[0] note: SFPU Instance 5, only when coresponding h2_debug_en_q
#define DEBUG_BUS_H2_SFPU_DBG_BUS_5__WORD {28, 7, 0, 0, 1, 0}
#define DEBUG_BUS_H2_SFPU_DBG_BUS_5_ {DEBUG_BUS_H2_SFPU_DBG_BUS_5__WORD, 20, 0xF}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_14 alias: fpu_dbg_bus
//  name: h2_sfpu_dbg_bus[4] alias: cc_satisfied_s3[0] note: SFPU Instance 4, only when coresponding h2_debug_en_q
#define DEBUG_BUS_H2_SFPU_DBG_BUS_4__WORD {28, 7, 0, 0, 1, 0}
#define DEBUG_BUS_H2_SFPU_DBG_BUS_4_ {DEBUG_BUS_H2_SFPU_DBG_BUS_4__WORD, 16, 0xF}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_14 alias: fpu_dbg_bus
//  name: h2_sfpu_dbg_bus[3] alias: cc_satisfied_s3[0] note: SFPU Instance 3, only when coresponding h2_debug_en_q
#define DEBUG_BUS_H2_SFPU_DBG_BUS_3__WORD {28, 7, 0, 0, 1, 0}
#define DEBUG_BUS_H2_SFPU_DBG_BUS_3_ {DEBUG_BUS_H2_SFPU_DBG_BUS_3__WORD, 12, 0xF}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_14 alias: fpu_dbg_bus
//  name: h2_sfpu_dbg_bus[2] alias: cc_satisfied_s3[0] note: SFPU Instance 2, only when coresponding h2_debug_en_q
#define DEBUG_BUS_H2_SFPU_DBG_BUS_2__WORD {28, 7, 0, 0, 1, 0}
#define DEBUG_BUS_H2_SFPU_DBG_BUS_2_ {DEBUG_BUS_H2_SFPU_DBG_BUS_2__WORD, 8, 0xF}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_14 alias: fpu_dbg_bus
//  name: h2_sfpu_dbg_bus[1] alias: cc_satisfied_s3[0] note: SFPU Instance 1, only when coresponding h2_debug_en_q
#define DEBUG_BUS_H2_SFPU_DBG_BUS_1__WORD {28, 7, 0, 0, 1, 0}
#define DEBUG_BUS_H2_SFPU_DBG_BUS_1_ {DEBUG_BUS_H2_SFPU_DBG_BUS_1__WORD, 4, 0xF}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_14 alias: fpu_dbg_bus
//  name: h2_sfpu_dbg_bus[0] alias: cc_satisfied_s3[0] note: SFPU Instance 0, only when coresponding h2_debug_en_q
#define DEBUG_BUS_H2_SFPU_DBG_BUS_0__WORD {28, 7, 0, 0, 1, 0}
#define DEBUG_BUS_H2_SFPU_DBG_BUS_0_ {DEBUG_BUS_H2_SFPU_DBG_BUS_0__WORD, 0, 0xF}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_13
//  name: risc_wrapper_noc_ctrl_debug_bus alias: risc_wrapper_debug_bus note: NCRISC
//  name: o_par_err_risc_localmem
#define DEBUG_BUS_RISC_WRAPPER_NOC_CTRL_DEBUG_BUS_O_PAR_ERR_RISC_LOCALMEM_WORD {27, 7, 6, 0, 1, 0}
#define DEBUG_BUS_RISC_WRAPPER_NOC_CTRL_DEBUG_BUS_O_PAR_ERR_RISC_LOCALMEM {DEBUG_BUS_RISC_WRAPPER_NOC_CTRL_DEBUG_BUS_O_PAR_ERR_RISC_LOCALMEM_WORD, 31, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_13
//  name: risc_wrapper_noc_ctrl_debug_bus alias: risc_wrapper_debug_bus note: NCRISC
//  name: i_mailbox_rden
#define DEBUG_BUS_RISC_WRAPPER_NOC_CTRL_DEBUG_BUS_I_MAILBOX_RDEN_WORD {27, 7, 6, 0, 1, 0}
#define DEBUG_BUS_RISC_WRAPPER_NOC_CTRL_DEBUG_BUS_I_MAILBOX_RDEN {DEBUG_BUS_RISC_WRAPPER_NOC_CTRL_DEBUG_BUS_I_MAILBOX_RDEN_WORD, 27, 0xF}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_13
//  name: risc_wrapper_noc_ctrl_debug_bus alias: risc_wrapper_debug_bus note: NCRISC
//  name: i_mailbox_rd_type
#define DEBUG_BUS_RISC_WRAPPER_NOC_CTRL_DEBUG_BUS_I_MAILBOX_RD_TYPE_WORD {27, 7, 6, 0, 1, 0}
#define DEBUG_BUS_RISC_WRAPPER_NOC_CTRL_DEBUG_BUS_I_MAILBOX_RD_TYPE {DEBUG_BUS_RISC_WRAPPER_NOC_CTRL_DEBUG_BUS_I_MAILBOX_RD_TYPE_WORD, 23, 0xF}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_13
//  name: risc_wrapper_noc_ctrl_debug_bus alias: risc_wrapper_debug_bus note: NCRISC
//  name: o_mailbox_rd_req_ready
#define DEBUG_BUS_RISC_WRAPPER_NOC_CTRL_DEBUG_BUS_O_MAILBOX_RD_REQ_READY_WORD {27, 7, 6, 0, 1, 0}
#define DEBUG_BUS_RISC_WRAPPER_NOC_CTRL_DEBUG_BUS_O_MAILBOX_RD_REQ_READY {DEBUG_BUS_RISC_WRAPPER_NOC_CTRL_DEBUG_BUS_O_MAILBOX_RD_REQ_READY_WORD, 19, 0xF}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_13
//  name: risc_wrapper_noc_ctrl_debug_bus alias: risc_wrapper_debug_bus note: NCRISC
//  name: o_mailbox_rdvalid
#define DEBUG_BUS_RISC_WRAPPER_NOC_CTRL_DEBUG_BUS_O_MAILBOX_RDVALID_WORD {27, 7, 6, 0, 1, 0}
#define DEBUG_BUS_RISC_WRAPPER_NOC_CTRL_DEBUG_BUS_O_MAILBOX_RDVALID {DEBUG_BUS_RISC_WRAPPER_NOC_CTRL_DEBUG_BUS_O_MAILBOX_RDVALID_WORD, 15, 0xF}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_13
//  name: risc_wrapper_noc_ctrl_debug_bus alias: risc_wrapper_debug_bus note: NCRISC
//  name: o_mailbox_rddata
#define DEBUG_BUS_RISC_WRAPPER_NOC_CTRL_DEBUG_BUS_O_MAILBOX_RDDATA__6_0_WORD {27, 7, 6, 0, 1, 0}
#define DEBUG_BUS_RISC_WRAPPER_NOC_CTRL_DEBUG_BUS_O_MAILBOX_RDDATA__6_0 {DEBUG_BUS_RISC_WRAPPER_NOC_CTRL_DEBUG_BUS_O_MAILBOX_RDDATA__6_0_WORD, 8, 0x7F}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_13
//  name: risc_wrapper_noc_ctrl_debug_bus alias: risc_wrapper_debug_bus note: NCRISC
//  name: intf_wrack_brisc
#define DEBUG_BUS_RISC_WRAPPER_NOC_CTRL_DEBUG_BUS_INTF_WRACK_BRISC__10_0_WORD {27, 7, 0, 0, 1, 0}
#define DEBUG_BUS_RISC_WRAPPER_NOC_CTRL_DEBUG_BUS_INTF_WRACK_BRISC__10_0 {DEBUG_BUS_RISC_WRAPPER_NOC_CTRL_DEBUG_BUS_INTF_WRACK_BRISC__10_0_WORD, 21, 0x7FF}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_13
//  name: risc_wrapper_noc_ctrl_debug_bus alias: risc_wrapper_debug_bus note: NCRISC
//  name: intf_wrack_brisc
#define DEBUG_BUS_RISC_WRAPPER_NOC_CTRL_DEBUG_BUS_INTF_WRACK_BRISC__16_11_WORD {27, 7, 2, 0, 1, 0}
#define DEBUG_BUS_RISC_WRAPPER_NOC_CTRL_DEBUG_BUS_INTF_WRACK_BRISC__16_11 {DEBUG_BUS_RISC_WRAPPER_NOC_CTRL_DEBUG_BUS_INTF_WRACK_BRISC__16_11_WORD, 0, 0x3F}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_13
//  name: risc_wrapper_noc_ctrl_debug_bus alias: risc_wrapper_debug_bus note: NCRISC
//  name: dmem_tensix_rden
#define DEBUG_BUS_RISC_WRAPPER_NOC_CTRL_DEBUG_BUS_DMEM_TENSIX_RDEN_WORD {27, 7, 0, 0, 1, 0}
#define DEBUG_BUS_RISC_WRAPPER_NOC_CTRL_DEBUG_BUS_DMEM_TENSIX_RDEN {DEBUG_BUS_RISC_WRAPPER_NOC_CTRL_DEBUG_BUS_DMEM_TENSIX_RDEN_WORD, 20, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_13
//  name: risc_wrapper_noc_ctrl_debug_bus alias: risc_wrapper_debug_bus note: NCRISC
//  name: dmem_tensix_wren
#define DEBUG_BUS_RISC_WRAPPER_NOC_CTRL_DEBUG_BUS_DMEM_TENSIX_WREN_WORD {27, 7, 0, 0, 1, 0}
#define DEBUG_BUS_RISC_WRAPPER_NOC_CTRL_DEBUG_BUS_DMEM_TENSIX_WREN {DEBUG_BUS_RISC_WRAPPER_NOC_CTRL_DEBUG_BUS_DMEM_TENSIX_WREN_WORD, 19, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_13
//  name: risc_wrapper_noc_ctrl_debug_bus alias: risc_wrapper_debug_bus note: NCRISC
//  name: icache_req_fifo_full
#define DEBUG_BUS_RISC_WRAPPER_NOC_CTRL_DEBUG_BUS_ICACHE_REQ_FIFO_FULL_WORD {27, 7, 0, 0, 1, 0}
#define DEBUG_BUS_RISC_WRAPPER_NOC_CTRL_DEBUG_BUS_ICACHE_REQ_FIFO_FULL {DEBUG_BUS_RISC_WRAPPER_NOC_CTRL_DEBUG_BUS_ICACHE_REQ_FIFO_FULL_WORD, 1, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_13
//  name: risc_wrapper_noc_ctrl_debug_bus alias: risc_wrapper_debug_bus note: NCRISC
//  name: icache_req_fifo_empty
#define DEBUG_BUS_RISC_WRAPPER_NOC_CTRL_DEBUG_BUS_ICACHE_REQ_FIFO_EMPTY_WORD {27, 7, 0, 0, 1, 0}
#define DEBUG_BUS_RISC_WRAPPER_NOC_CTRL_DEBUG_BUS_ICACHE_REQ_FIFO_EMPTY {DEBUG_BUS_RISC_WRAPPER_NOC_CTRL_DEBUG_BUS_ICACHE_REQ_FIFO_EMPTY_WORD, 0, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_13
//  name: icache_debug_bus_trisc[2] alias: icache_debug_bus note: TRISC2
//  name: o_busy
#define DEBUG_BUS_ICACHE_DEBUG_BUS_TRISC_2__O_BUSY_WORD {26, 7, 6, 0, 1, 0}
#define DEBUG_BUS_ICACHE_DEBUG_BUS_TRISC_2__O_BUSY {DEBUG_BUS_ICACHE_DEBUG_BUS_TRISC_2__O_BUSY_WORD, 21, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_13
//  name: icache_debug_bus_trisc[2] alias: icache_debug_bus note: TRISC2
//  name: req_fifo_empty
#define DEBUG_BUS_ICACHE_DEBUG_BUS_TRISC_2__REQ_FIFO_EMPTY_WORD {26, 7, 6, 0, 1, 0}
#define DEBUG_BUS_ICACHE_DEBUG_BUS_TRISC_2__REQ_FIFO_EMPTY {DEBUG_BUS_ICACHE_DEBUG_BUS_TRISC_2__REQ_FIFO_EMPTY_WORD, 20, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_13
//  name: icache_debug_bus_trisc[2] alias: icache_debug_bus note: TRISC2
//  name: req_fifo_full
#define DEBUG_BUS_ICACHE_DEBUG_BUS_TRISC_2__REQ_FIFO_FULL_WORD {26, 7, 6, 0, 1, 0}
#define DEBUG_BUS_ICACHE_DEBUG_BUS_TRISC_2__REQ_FIFO_FULL {DEBUG_BUS_ICACHE_DEBUG_BUS_TRISC_2__REQ_FIFO_FULL_WORD, 19, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_13
//  name: icache_debug_bus_trisc[2] alias: icache_debug_bus note: TRISC2
//  name: mshr_empty
#define DEBUG_BUS_ICACHE_DEBUG_BUS_TRISC_2__MSHR_EMPTY_WORD {26, 7, 6, 0, 1, 0}
#define DEBUG_BUS_ICACHE_DEBUG_BUS_TRISC_2__MSHR_EMPTY {DEBUG_BUS_ICACHE_DEBUG_BUS_TRISC_2__MSHR_EMPTY_WORD, 18, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_13
//  name: icache_debug_bus_trisc[2] alias: icache_debug_bus note: TRISC2
//  name: mshr_full
#define DEBUG_BUS_ICACHE_DEBUG_BUS_TRISC_2__MSHR_FULL_WORD {26, 7, 6, 0, 1, 0}
#define DEBUG_BUS_ICACHE_DEBUG_BUS_TRISC_2__MSHR_FULL {DEBUG_BUS_ICACHE_DEBUG_BUS_TRISC_2__MSHR_FULL_WORD, 17, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_13
//  name: icache_debug_bus_trisc[2] alias: icache_debug_bus note: TRISC2
//  name: way_hit
#define DEBUG_BUS_ICACHE_DEBUG_BUS_TRISC_2__WAY_HIT_WORD {26, 7, 6, 0, 1, 0}
#define DEBUG_BUS_ICACHE_DEBUG_BUS_TRISC_2__WAY_HIT {DEBUG_BUS_ICACHE_DEBUG_BUS_TRISC_2__WAY_HIT_WORD, 15, 0x3}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_13
//  name: icache_debug_bus_trisc[2] alias: icache_debug_bus note: TRISC2
//  name: mshr_pf_hit
#define DEBUG_BUS_ICACHE_DEBUG_BUS_TRISC_2__MSHR_PF_HIT_WORD {26, 7, 6, 0, 1, 0}
#define DEBUG_BUS_ICACHE_DEBUG_BUS_TRISC_2__MSHR_PF_HIT {DEBUG_BUS_ICACHE_DEBUG_BUS_TRISC_2__MSHR_PF_HIT_WORD, 14, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_13
//  name: icache_debug_bus_trisc[2] alias: icache_debug_bus note: TRISC2
//  name: mshr_cpu_hit
#define DEBUG_BUS_ICACHE_DEBUG_BUS_TRISC_2__MSHR_CPU_HIT_WORD {26, 7, 6, 0, 1, 0}
#define DEBUG_BUS_ICACHE_DEBUG_BUS_TRISC_2__MSHR_CPU_HIT {DEBUG_BUS_ICACHE_DEBUG_BUS_TRISC_2__MSHR_CPU_HIT_WORD, 13, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_13
//  name: icache_debug_bus_trisc[2] alias: icache_debug_bus note: TRISC2
//  name: some_mshr_allocated
#define DEBUG_BUS_ICACHE_DEBUG_BUS_TRISC_2__SOME_MSHR_ALLOCATED_WORD {26, 7, 6, 0, 1, 0}
#define DEBUG_BUS_ICACHE_DEBUG_BUS_TRISC_2__SOME_MSHR_ALLOCATED {DEBUG_BUS_ICACHE_DEBUG_BUS_TRISC_2__SOME_MSHR_ALLOCATED_WORD, 12, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_13
//  name: icache_debug_bus_trisc[2] alias: icache_debug_bus note: TRISC2
//  name: latched_req_cpu_vld
#define DEBUG_BUS_ICACHE_DEBUG_BUS_TRISC_2__LATCHED_REQ_CPU_VLD_WORD {26, 7, 6, 0, 1, 0}
#define DEBUG_BUS_ICACHE_DEBUG_BUS_TRISC_2__LATCHED_REQ_CPU_VLD {DEBUG_BUS_ICACHE_DEBUG_BUS_TRISC_2__LATCHED_REQ_CPU_VLD_WORD, 11, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_13
//  name: icache_debug_bus_trisc[2] alias: icache_debug_bus note: TRISC2
//  name: cpu_req_dispatched
#define DEBUG_BUS_ICACHE_DEBUG_BUS_TRISC_2__CPU_REQ_DISPATCHED_WORD {26, 7, 6, 0, 1, 0}
#define DEBUG_BUS_ICACHE_DEBUG_BUS_TRISC_2__CPU_REQ_DISPATCHED {DEBUG_BUS_ICACHE_DEBUG_BUS_TRISC_2__CPU_REQ_DISPATCHED_WORD, 10, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_13
//  name: icache_debug_bus_trisc[2] alias: icache_debug_bus note: TRISC2
//  name: latched_req_pf_vld
#define DEBUG_BUS_ICACHE_DEBUG_BUS_TRISC_2__LATCHED_REQ_PF_VLD_WORD {26, 7, 6, 0, 1, 0}
#define DEBUG_BUS_ICACHE_DEBUG_BUS_TRISC_2__LATCHED_REQ_PF_VLD {DEBUG_BUS_ICACHE_DEBUG_BUS_TRISC_2__LATCHED_REQ_PF_VLD_WORD, 9, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_13
//  name: icache_debug_bus_trisc[2] alias: icache_debug_bus note: TRISC2
//  name: pf_req_dispatched
#define DEBUG_BUS_ICACHE_DEBUG_BUS_TRISC_2__PF_REQ_DISPATCHED_WORD {26, 7, 6, 0, 1, 0}
#define DEBUG_BUS_ICACHE_DEBUG_BUS_TRISC_2__PF_REQ_DISPATCHED {DEBUG_BUS_ICACHE_DEBUG_BUS_TRISC_2__PF_REQ_DISPATCHED_WORD, 8, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_13
//  name: icache_debug_bus_trisc[2] alias: icache_debug_bus note: TRISC2
//  name: qual_rden
#define DEBUG_BUS_ICACHE_DEBUG_BUS_TRISC_2__QUAL_RDEN_WORD {26, 7, 6, 0, 1, 0}
#define DEBUG_BUS_ICACHE_DEBUG_BUS_TRISC_2__QUAL_RDEN {DEBUG_BUS_ICACHE_DEBUG_BUS_TRISC_2__QUAL_RDEN_WORD, 7, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_13
//  name: icache_debug_bus_trisc[2] alias: icache_debug_bus note: TRISC2
//  name: i_mispredict
#define DEBUG_BUS_ICACHE_DEBUG_BUS_TRISC_2__I_MISPREDICT_WORD {26, 7, 6, 0, 1, 0}
#define DEBUG_BUS_ICACHE_DEBUG_BUS_TRISC_2__I_MISPREDICT {DEBUG_BUS_ICACHE_DEBUG_BUS_TRISC_2__I_MISPREDICT_WORD, 6, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_13
//  name: icache_debug_bus_trisc[2] alias: icache_debug_bus note: TRISC2
//  name: o_req_ready
#define DEBUG_BUS_ICACHE_DEBUG_BUS_TRISC_2__O_REQ_READY_WORD {26, 7, 6, 0, 1, 0}
#define DEBUG_BUS_ICACHE_DEBUG_BUS_TRISC_2__O_REQ_READY {DEBUG_BUS_ICACHE_DEBUG_BUS_TRISC_2__O_REQ_READY_WORD, 5, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_13
//  name: icache_debug_bus_trisc[2] alias: icache_debug_bus note: TRISC2
//  name: o_instrn_vld
#define DEBUG_BUS_ICACHE_DEBUG_BUS_TRISC_2__O_INSTRN_VLD_WORD {26, 7, 6, 0, 1, 0}
#define DEBUG_BUS_ICACHE_DEBUG_BUS_TRISC_2__O_INSTRN_VLD {DEBUG_BUS_ICACHE_DEBUG_BUS_TRISC_2__O_INSTRN_VLD_WORD, 4, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_13
//  name: icache_debug_bus_trisc[1] alias: icache_debug_bus note: TRISC1
//  name: o_busy
#define DEBUG_BUS_ICACHE_DEBUG_BUS_TRISC_1__O_BUSY_WORD {26, 7, 4, 0, 1, 0}
#define DEBUG_BUS_ICACHE_DEBUG_BUS_TRISC_1__O_BUSY {DEBUG_BUS_ICACHE_DEBUG_BUS_TRISC_1__O_BUSY_WORD, 28, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_13
//  name: icache_debug_bus_trisc[1] alias: icache_debug_bus note: TRISC1
//  name: req_fifo_empty
#define DEBUG_BUS_ICACHE_DEBUG_BUS_TRISC_1__REQ_FIFO_EMPTY_WORD {26, 7, 4, 0, 1, 0}
#define DEBUG_BUS_ICACHE_DEBUG_BUS_TRISC_1__REQ_FIFO_EMPTY {DEBUG_BUS_ICACHE_DEBUG_BUS_TRISC_1__REQ_FIFO_EMPTY_WORD, 27, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_13
//  name: icache_debug_bus_trisc[1] alias: icache_debug_bus note: TRISC1
//  name: req_fifo_full
#define DEBUG_BUS_ICACHE_DEBUG_BUS_TRISC_1__REQ_FIFO_FULL_WORD {26, 7, 4, 0, 1, 0}
#define DEBUG_BUS_ICACHE_DEBUG_BUS_TRISC_1__REQ_FIFO_FULL {DEBUG_BUS_ICACHE_DEBUG_BUS_TRISC_1__REQ_FIFO_FULL_WORD, 26, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_13
//  name: icache_debug_bus_trisc[1] alias: icache_debug_bus note: TRISC1
//  name: mshr_empty
#define DEBUG_BUS_ICACHE_DEBUG_BUS_TRISC_1__MSHR_EMPTY_WORD {26, 7, 4, 0, 1, 0}
#define DEBUG_BUS_ICACHE_DEBUG_BUS_TRISC_1__MSHR_EMPTY {DEBUG_BUS_ICACHE_DEBUG_BUS_TRISC_1__MSHR_EMPTY_WORD, 25, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_13
//  name: icache_debug_bus_trisc[1] alias: icache_debug_bus note: TRISC1
//  name: mshr_full
#define DEBUG_BUS_ICACHE_DEBUG_BUS_TRISC_1__MSHR_FULL_WORD {26, 7, 4, 0, 1, 0}
#define DEBUG_BUS_ICACHE_DEBUG_BUS_TRISC_1__MSHR_FULL {DEBUG_BUS_ICACHE_DEBUG_BUS_TRISC_1__MSHR_FULL_WORD, 24, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_13
//  name: icache_debug_bus_trisc[1] alias: icache_debug_bus note: TRISC1
//  name: way_hit
#define DEBUG_BUS_ICACHE_DEBUG_BUS_TRISC_1__WAY_HIT_WORD {26, 7, 4, 0, 1, 0}
#define DEBUG_BUS_ICACHE_DEBUG_BUS_TRISC_1__WAY_HIT {DEBUG_BUS_ICACHE_DEBUG_BUS_TRISC_1__WAY_HIT_WORD, 22, 0x3}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_13
//  name: icache_debug_bus_trisc[1] alias: icache_debug_bus note: TRISC1
//  name: mshr_pf_hit
#define DEBUG_BUS_ICACHE_DEBUG_BUS_TRISC_1__MSHR_PF_HIT_WORD {26, 7, 4, 0, 1, 0}
#define DEBUG_BUS_ICACHE_DEBUG_BUS_TRISC_1__MSHR_PF_HIT {DEBUG_BUS_ICACHE_DEBUG_BUS_TRISC_1__MSHR_PF_HIT_WORD, 21, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_13
//  name: icache_debug_bus_trisc[1] alias: icache_debug_bus note: TRISC1
//  name: mshr_cpu_hit
#define DEBUG_BUS_ICACHE_DEBUG_BUS_TRISC_1__MSHR_CPU_HIT_WORD {26, 7, 4, 0, 1, 0}
#define DEBUG_BUS_ICACHE_DEBUG_BUS_TRISC_1__MSHR_CPU_HIT {DEBUG_BUS_ICACHE_DEBUG_BUS_TRISC_1__MSHR_CPU_HIT_WORD, 20, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_13
//  name: icache_debug_bus_trisc[1] alias: icache_debug_bus note: TRISC1
//  name: some_mshr_allocated
#define DEBUG_BUS_ICACHE_DEBUG_BUS_TRISC_1__SOME_MSHR_ALLOCATED_WORD {26, 7, 4, 0, 1, 0}
#define DEBUG_BUS_ICACHE_DEBUG_BUS_TRISC_1__SOME_MSHR_ALLOCATED {DEBUG_BUS_ICACHE_DEBUG_BUS_TRISC_1__SOME_MSHR_ALLOCATED_WORD, 19, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_13
//  name: icache_debug_bus_trisc[1] alias: icache_debug_bus note: TRISC1
//  name: latched_req_cpu_vld
#define DEBUG_BUS_ICACHE_DEBUG_BUS_TRISC_1__LATCHED_REQ_CPU_VLD_WORD {26, 7, 4, 0, 1, 0}
#define DEBUG_BUS_ICACHE_DEBUG_BUS_TRISC_1__LATCHED_REQ_CPU_VLD {DEBUG_BUS_ICACHE_DEBUG_BUS_TRISC_1__LATCHED_REQ_CPU_VLD_WORD, 18, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_13
//  name: icache_debug_bus_trisc[1] alias: icache_debug_bus note: TRISC1
//  name: cpu_req_dispatched
#define DEBUG_BUS_ICACHE_DEBUG_BUS_TRISC_1__CPU_REQ_DISPATCHED_WORD {26, 7, 4, 0, 1, 0}
#define DEBUG_BUS_ICACHE_DEBUG_BUS_TRISC_1__CPU_REQ_DISPATCHED {DEBUG_BUS_ICACHE_DEBUG_BUS_TRISC_1__CPU_REQ_DISPATCHED_WORD, 17, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_13
//  name: icache_debug_bus_trisc[1] alias: icache_debug_bus note: TRISC1
//  name: latched_req_pf_vld
#define DEBUG_BUS_ICACHE_DEBUG_BUS_TRISC_1__LATCHED_REQ_PF_VLD_WORD {26, 7, 4, 0, 1, 0}
#define DEBUG_BUS_ICACHE_DEBUG_BUS_TRISC_1__LATCHED_REQ_PF_VLD {DEBUG_BUS_ICACHE_DEBUG_BUS_TRISC_1__LATCHED_REQ_PF_VLD_WORD, 16, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_13
//  name: icache_debug_bus_trisc[1] alias: icache_debug_bus note: TRISC1
//  name: pf_req_dispatched
#define DEBUG_BUS_ICACHE_DEBUG_BUS_TRISC_1__PF_REQ_DISPATCHED_WORD {26, 7, 4, 0, 1, 0}
#define DEBUG_BUS_ICACHE_DEBUG_BUS_TRISC_1__PF_REQ_DISPATCHED {DEBUG_BUS_ICACHE_DEBUG_BUS_TRISC_1__PF_REQ_DISPATCHED_WORD, 15, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_13
//  name: icache_debug_bus_trisc[1] alias: icache_debug_bus note: TRISC1
//  name: qual_rden
#define DEBUG_BUS_ICACHE_DEBUG_BUS_TRISC_1__QUAL_RDEN_WORD {26, 7, 4, 0, 1, 0}
#define DEBUG_BUS_ICACHE_DEBUG_BUS_TRISC_1__QUAL_RDEN {DEBUG_BUS_ICACHE_DEBUG_BUS_TRISC_1__QUAL_RDEN_WORD, 14, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_13
//  name: icache_debug_bus_trisc[1] alias: icache_debug_bus note: TRISC1
//  name: i_mispredict
#define DEBUG_BUS_ICACHE_DEBUG_BUS_TRISC_1__I_MISPREDICT_WORD {26, 7, 4, 0, 1, 0}
#define DEBUG_BUS_ICACHE_DEBUG_BUS_TRISC_1__I_MISPREDICT {DEBUG_BUS_ICACHE_DEBUG_BUS_TRISC_1__I_MISPREDICT_WORD, 13, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_13
//  name: icache_debug_bus_trisc[1] alias: icache_debug_bus note: TRISC1
//  name: o_req_ready
#define DEBUG_BUS_ICACHE_DEBUG_BUS_TRISC_1__O_REQ_READY_WORD {26, 7, 4, 0, 1, 0}
#define DEBUG_BUS_ICACHE_DEBUG_BUS_TRISC_1__O_REQ_READY {DEBUG_BUS_ICACHE_DEBUG_BUS_TRISC_1__O_REQ_READY_WORD, 12, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_13
//  name: icache_debug_bus_trisc[1] alias: icache_debug_bus note: TRISC1
//  name: o_instrn_vld
#define DEBUG_BUS_ICACHE_DEBUG_BUS_TRISC_1__O_INSTRN_VLD_WORD {26, 7, 4, 0, 1, 0}
#define DEBUG_BUS_ICACHE_DEBUG_BUS_TRISC_1__O_INSTRN_VLD {DEBUG_BUS_ICACHE_DEBUG_BUS_TRISC_1__O_INSTRN_VLD_WORD, 11, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_13
//  name: icache_debug_bus_trisc[0] alias: icache_debug_bus note: TRISC0
//  name: o_busy
#define DEBUG_BUS_ICACHE_DEBUG_BUS_TRISC_0__O_BUSY_WORD {26, 7, 4, 0, 1, 0}
#define DEBUG_BUS_ICACHE_DEBUG_BUS_TRISC_0__O_BUSY {DEBUG_BUS_ICACHE_DEBUG_BUS_TRISC_0__O_BUSY_WORD, 3, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_13
//  name: icache_debug_bus_trisc[0] alias: icache_debug_bus note: TRISC0
//  name: req_fifo_empty
#define DEBUG_BUS_ICACHE_DEBUG_BUS_TRISC_0__REQ_FIFO_EMPTY_WORD {26, 7, 4, 0, 1, 0}
#define DEBUG_BUS_ICACHE_DEBUG_BUS_TRISC_0__REQ_FIFO_EMPTY {DEBUG_BUS_ICACHE_DEBUG_BUS_TRISC_0__REQ_FIFO_EMPTY_WORD, 2, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_13
//  name: icache_debug_bus_trisc[0] alias: icache_debug_bus note: TRISC0
//  name: req_fifo_full
#define DEBUG_BUS_ICACHE_DEBUG_BUS_TRISC_0__REQ_FIFO_FULL_WORD {26, 7, 4, 0, 1, 0}
#define DEBUG_BUS_ICACHE_DEBUG_BUS_TRISC_0__REQ_FIFO_FULL {DEBUG_BUS_ICACHE_DEBUG_BUS_TRISC_0__REQ_FIFO_FULL_WORD, 1, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_13
//  name: icache_debug_bus_trisc[0] alias: icache_debug_bus note: TRISC0
//  name: mshr_empty
#define DEBUG_BUS_ICACHE_DEBUG_BUS_TRISC_0__MSHR_EMPTY_WORD {26, 7, 4, 0, 1, 0}
#define DEBUG_BUS_ICACHE_DEBUG_BUS_TRISC_0__MSHR_EMPTY {DEBUG_BUS_ICACHE_DEBUG_BUS_TRISC_0__MSHR_EMPTY_WORD, 0, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_13
//  name: icache_debug_bus_trisc[0] alias: icache_debug_bus note: TRISC0
//  name: mshr_full
#define DEBUG_BUS_ICACHE_DEBUG_BUS_TRISC_0__MSHR_FULL_WORD {26, 7, 2, 0, 1, 0}
#define DEBUG_BUS_ICACHE_DEBUG_BUS_TRISC_0__MSHR_FULL {DEBUG_BUS_ICACHE_DEBUG_BUS_TRISC_0__MSHR_FULL_WORD, 31, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_13
//  name: icache_debug_bus_trisc[0] alias: icache_debug_bus note: TRISC0
//  name: way_hit
#define DEBUG_BUS_ICACHE_DEBUG_BUS_TRISC_0__WAY_HIT_WORD {26, 7, 2, 0, 1, 0}
#define DEBUG_BUS_ICACHE_DEBUG_BUS_TRISC_0__WAY_HIT {DEBUG_BUS_ICACHE_DEBUG_BUS_TRISC_0__WAY_HIT_WORD, 29, 0x3}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_13
//  name: icache_debug_bus_trisc[0] alias: icache_debug_bus note: TRISC0
//  name: mshr_pf_hit
#define DEBUG_BUS_ICACHE_DEBUG_BUS_TRISC_0__MSHR_PF_HIT_WORD {26, 7, 2, 0, 1, 0}
#define DEBUG_BUS_ICACHE_DEBUG_BUS_TRISC_0__MSHR_PF_HIT {DEBUG_BUS_ICACHE_DEBUG_BUS_TRISC_0__MSHR_PF_HIT_WORD, 28, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_13
//  name: icache_debug_bus_trisc[0] alias: icache_debug_bus note: TRISC0
//  name: mshr_cpu_hit
#define DEBUG_BUS_ICACHE_DEBUG_BUS_TRISC_0__MSHR_CPU_HIT_WORD {26, 7, 2, 0, 1, 0}
#define DEBUG_BUS_ICACHE_DEBUG_BUS_TRISC_0__MSHR_CPU_HIT {DEBUG_BUS_ICACHE_DEBUG_BUS_TRISC_0__MSHR_CPU_HIT_WORD, 27, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_13
//  name: icache_debug_bus_trisc[0] alias: icache_debug_bus note: TRISC0
//  name: some_mshr_allocated
#define DEBUG_BUS_ICACHE_DEBUG_BUS_TRISC_0__SOME_MSHR_ALLOCATED_WORD {26, 7, 2, 0, 1, 0}
#define DEBUG_BUS_ICACHE_DEBUG_BUS_TRISC_0__SOME_MSHR_ALLOCATED {DEBUG_BUS_ICACHE_DEBUG_BUS_TRISC_0__SOME_MSHR_ALLOCATED_WORD, 26, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_13
//  name: icache_debug_bus_trisc[0] alias: icache_debug_bus note: TRISC0
//  name: latched_req_cpu_vld
#define DEBUG_BUS_ICACHE_DEBUG_BUS_TRISC_0__LATCHED_REQ_CPU_VLD_WORD {26, 7, 2, 0, 1, 0}
#define DEBUG_BUS_ICACHE_DEBUG_BUS_TRISC_0__LATCHED_REQ_CPU_VLD {DEBUG_BUS_ICACHE_DEBUG_BUS_TRISC_0__LATCHED_REQ_CPU_VLD_WORD, 25, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_13
//  name: icache_debug_bus_trisc[0] alias: icache_debug_bus note: TRISC0
//  name: cpu_req_dispatched
#define DEBUG_BUS_ICACHE_DEBUG_BUS_TRISC_0__CPU_REQ_DISPATCHED_WORD {26, 7, 2, 0, 1, 0}
#define DEBUG_BUS_ICACHE_DEBUG_BUS_TRISC_0__CPU_REQ_DISPATCHED {DEBUG_BUS_ICACHE_DEBUG_BUS_TRISC_0__CPU_REQ_DISPATCHED_WORD, 24, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_13
//  name: icache_debug_bus_trisc[0] alias: icache_debug_bus note: TRISC0
//  name: latched_req_pf_vld
#define DEBUG_BUS_ICACHE_DEBUG_BUS_TRISC_0__LATCHED_REQ_PF_VLD_WORD {26, 7, 2, 0, 1, 0}
#define DEBUG_BUS_ICACHE_DEBUG_BUS_TRISC_0__LATCHED_REQ_PF_VLD {DEBUG_BUS_ICACHE_DEBUG_BUS_TRISC_0__LATCHED_REQ_PF_VLD_WORD, 23, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_13
//  name: icache_debug_bus_trisc[0] alias: icache_debug_bus note: TRISC0
//  name: pf_req_dispatched
#define DEBUG_BUS_ICACHE_DEBUG_BUS_TRISC_0__PF_REQ_DISPATCHED_WORD {26, 7, 2, 0, 1, 0}
#define DEBUG_BUS_ICACHE_DEBUG_BUS_TRISC_0__PF_REQ_DISPATCHED {DEBUG_BUS_ICACHE_DEBUG_BUS_TRISC_0__PF_REQ_DISPATCHED_WORD, 22, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_13
//  name: icache_debug_bus_trisc[0] alias: icache_debug_bus note: TRISC0
//  name: qual_rden
#define DEBUG_BUS_ICACHE_DEBUG_BUS_TRISC_0__QUAL_RDEN_WORD {26, 7, 2, 0, 1, 0}
#define DEBUG_BUS_ICACHE_DEBUG_BUS_TRISC_0__QUAL_RDEN {DEBUG_BUS_ICACHE_DEBUG_BUS_TRISC_0__QUAL_RDEN_WORD, 21, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_13
//  name: icache_debug_bus_trisc[0] alias: icache_debug_bus note: TRISC0
//  name: i_mispredict
#define DEBUG_BUS_ICACHE_DEBUG_BUS_TRISC_0__I_MISPREDICT_WORD {26, 7, 2, 0, 1, 0}
#define DEBUG_BUS_ICACHE_DEBUG_BUS_TRISC_0__I_MISPREDICT {DEBUG_BUS_ICACHE_DEBUG_BUS_TRISC_0__I_MISPREDICT_WORD, 20, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_13
//  name: icache_debug_bus_trisc[0] alias: icache_debug_bus note: TRISC0
//  name: o_req_ready
#define DEBUG_BUS_ICACHE_DEBUG_BUS_TRISC_0__O_REQ_READY_WORD {26, 7, 2, 0, 1, 0}
#define DEBUG_BUS_ICACHE_DEBUG_BUS_TRISC_0__O_REQ_READY {DEBUG_BUS_ICACHE_DEBUG_BUS_TRISC_0__O_REQ_READY_WORD, 19, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_13
//  name: icache_debug_bus_trisc[0] alias: icache_debug_bus note: TRISC0
//  name: o_instrn_vld
#define DEBUG_BUS_ICACHE_DEBUG_BUS_TRISC_0__O_INSTRN_VLD_WORD {26, 7, 2, 0, 1, 0}
#define DEBUG_BUS_ICACHE_DEBUG_BUS_TRISC_0__O_INSTRN_VLD {DEBUG_BUS_ICACHE_DEBUG_BUS_TRISC_0__O_INSTRN_VLD_WORD, 18, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_13
//  name: icache_debug_bus_briscv alias: icache_debug_bus note: BRISC
//  name: o_busy
#define DEBUG_BUS_ICACHE_DEBUG_BUS_BRISCV_O_BUSY_WORD {26, 7, 2, 0, 1, 0}
#define DEBUG_BUS_ICACHE_DEBUG_BUS_BRISCV_O_BUSY {DEBUG_BUS_ICACHE_DEBUG_BUS_BRISCV_O_BUSY_WORD, 10, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_13
//  name: icache_debug_bus_briscv alias: icache_debug_bus note: BRISC
//  name: req_fifo_empty
#define DEBUG_BUS_ICACHE_DEBUG_BUS_BRISCV_REQ_FIFO_EMPTY_WORD {26, 7, 2, 0, 1, 0}
#define DEBUG_BUS_ICACHE_DEBUG_BUS_BRISCV_REQ_FIFO_EMPTY {DEBUG_BUS_ICACHE_DEBUG_BUS_BRISCV_REQ_FIFO_EMPTY_WORD, 9, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_13
//  name: icache_debug_bus_briscv alias: icache_debug_bus note: BRISC
//  name: req_fifo_full
#define DEBUG_BUS_ICACHE_DEBUG_BUS_BRISCV_REQ_FIFO_FULL_WORD {26, 7, 2, 0, 1, 0}
#define DEBUG_BUS_ICACHE_DEBUG_BUS_BRISCV_REQ_FIFO_FULL {DEBUG_BUS_ICACHE_DEBUG_BUS_BRISCV_REQ_FIFO_FULL_WORD, 8, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_13
//  name: icache_debug_bus_briscv alias: icache_debug_bus note: BRISC
//  name: mshr_empty
#define DEBUG_BUS_ICACHE_DEBUG_BUS_BRISCV_MSHR_EMPTY_WORD {26, 7, 2, 0, 1, 0}
#define DEBUG_BUS_ICACHE_DEBUG_BUS_BRISCV_MSHR_EMPTY {DEBUG_BUS_ICACHE_DEBUG_BUS_BRISCV_MSHR_EMPTY_WORD, 7, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_13
//  name: icache_debug_bus_briscv alias: icache_debug_bus note: BRISC
//  name: mshr_full
#define DEBUG_BUS_ICACHE_DEBUG_BUS_BRISCV_MSHR_FULL_WORD {26, 7, 2, 0, 1, 0}
#define DEBUG_BUS_ICACHE_DEBUG_BUS_BRISCV_MSHR_FULL {DEBUG_BUS_ICACHE_DEBUG_BUS_BRISCV_MSHR_FULL_WORD, 6, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_13
//  name: icache_debug_bus_briscv alias: icache_debug_bus note: BRISC
//  name: way_hit
#define DEBUG_BUS_ICACHE_DEBUG_BUS_BRISCV_WAY_HIT_WORD {26, 7, 2, 0, 1, 0}
#define DEBUG_BUS_ICACHE_DEBUG_BUS_BRISCV_WAY_HIT {DEBUG_BUS_ICACHE_DEBUG_BUS_BRISCV_WAY_HIT_WORD, 4, 0x3}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_13
//  name: icache_debug_bus_briscv alias: icache_debug_bus note: BRISC
//  name: mshr_pf_hit
#define DEBUG_BUS_ICACHE_DEBUG_BUS_BRISCV_MSHR_PF_HIT_WORD {26, 7, 2, 0, 1, 0}
#define DEBUG_BUS_ICACHE_DEBUG_BUS_BRISCV_MSHR_PF_HIT {DEBUG_BUS_ICACHE_DEBUG_BUS_BRISCV_MSHR_PF_HIT_WORD, 3, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_13
//  name: icache_debug_bus_briscv alias: icache_debug_bus note: BRISC
//  name: mshr_cpu_hit
#define DEBUG_BUS_ICACHE_DEBUG_BUS_BRISCV_MSHR_CPU_HIT_WORD {26, 7, 2, 0, 1, 0}
#define DEBUG_BUS_ICACHE_DEBUG_BUS_BRISCV_MSHR_CPU_HIT {DEBUG_BUS_ICACHE_DEBUG_BUS_BRISCV_MSHR_CPU_HIT_WORD, 2, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_13
//  name: icache_debug_bus_briscv alias: icache_debug_bus note: BRISC
//  name: some_mshr_allocated
#define DEBUG_BUS_ICACHE_DEBUG_BUS_BRISCV_SOME_MSHR_ALLOCATED_WORD {26, 7, 2, 0, 1, 0}
#define DEBUG_BUS_ICACHE_DEBUG_BUS_BRISCV_SOME_MSHR_ALLOCATED {DEBUG_BUS_ICACHE_DEBUG_BUS_BRISCV_SOME_MSHR_ALLOCATED_WORD, 1, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_13
//  name: icache_debug_bus_briscv alias: icache_debug_bus note: BRISC
//  name: latched_req_cpu_vld
#define DEBUG_BUS_ICACHE_DEBUG_BUS_BRISCV_LATCHED_REQ_CPU_VLD_WORD {26, 7, 2, 0, 1, 0}
#define DEBUG_BUS_ICACHE_DEBUG_BUS_BRISCV_LATCHED_REQ_CPU_VLD {DEBUG_BUS_ICACHE_DEBUG_BUS_BRISCV_LATCHED_REQ_CPU_VLD_WORD, 0, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_13
//  name: icache_debug_bus_briscv alias: icache_debug_bus note: BRISC
//  name: cpu_req_dispatched
#define DEBUG_BUS_ICACHE_DEBUG_BUS_BRISCV_CPU_REQ_DISPATCHED_WORD {26, 7, 0, 0, 1, 0}
#define DEBUG_BUS_ICACHE_DEBUG_BUS_BRISCV_CPU_REQ_DISPATCHED {DEBUG_BUS_ICACHE_DEBUG_BUS_BRISCV_CPU_REQ_DISPATCHED_WORD, 31, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_13
//  name: icache_debug_bus_briscv alias: icache_debug_bus note: BRISC
//  name: latched_req_pf_vld
#define DEBUG_BUS_ICACHE_DEBUG_BUS_BRISCV_LATCHED_REQ_PF_VLD_WORD {26, 7, 0, 0, 1, 0}
#define DEBUG_BUS_ICACHE_DEBUG_BUS_BRISCV_LATCHED_REQ_PF_VLD {DEBUG_BUS_ICACHE_DEBUG_BUS_BRISCV_LATCHED_REQ_PF_VLD_WORD, 30, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_13
//  name: icache_debug_bus_briscv alias: icache_debug_bus note: BRISC
//  name: pf_req_dispatched
#define DEBUG_BUS_ICACHE_DEBUG_BUS_BRISCV_PF_REQ_DISPATCHED_WORD {26, 7, 0, 0, 1, 0}
#define DEBUG_BUS_ICACHE_DEBUG_BUS_BRISCV_PF_REQ_DISPATCHED {DEBUG_BUS_ICACHE_DEBUG_BUS_BRISCV_PF_REQ_DISPATCHED_WORD, 29, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_13
//  name: icache_debug_bus_briscv alias: icache_debug_bus note: BRISC
//  name: qual_rden
#define DEBUG_BUS_ICACHE_DEBUG_BUS_BRISCV_QUAL_RDEN_WORD {26, 7, 0, 0, 1, 0}
#define DEBUG_BUS_ICACHE_DEBUG_BUS_BRISCV_QUAL_RDEN {DEBUG_BUS_ICACHE_DEBUG_BUS_BRISCV_QUAL_RDEN_WORD, 28, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_13
//  name: icache_debug_bus_briscv alias: icache_debug_bus note: BRISC
//  name: i_mispredict
#define DEBUG_BUS_ICACHE_DEBUG_BUS_BRISCV_I_MISPREDICT_WORD {26, 7, 0, 0, 1, 0}
#define DEBUG_BUS_ICACHE_DEBUG_BUS_BRISCV_I_MISPREDICT {DEBUG_BUS_ICACHE_DEBUG_BUS_BRISCV_I_MISPREDICT_WORD, 27, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_13
//  name: icache_debug_bus_briscv alias: icache_debug_bus note: BRISC
//  name: o_req_ready
#define DEBUG_BUS_ICACHE_DEBUG_BUS_BRISCV_O_REQ_READY_WORD {26, 7, 0, 0, 1, 0}
#define DEBUG_BUS_ICACHE_DEBUG_BUS_BRISCV_O_REQ_READY {DEBUG_BUS_ICACHE_DEBUG_BUS_BRISCV_O_REQ_READY_WORD, 26, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_13
//  name: icache_debug_bus_briscv alias: icache_debug_bus note: BRISC
//  name: o_instrn_vld
#define DEBUG_BUS_ICACHE_DEBUG_BUS_BRISCV_O_INSTRN_VLD_WORD {26, 7, 0, 0, 1, 0}
#define DEBUG_BUS_ICACHE_DEBUG_BUS_BRISCV_O_INSTRN_VLD {DEBUG_BUS_ICACHE_DEBUG_BUS_BRISCV_O_INSTRN_VLD_WORD, 25, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_13
//  name: icache_debug_bus_briscv_noc_ctrl alias: icache_debug_bus note: NCRISC
//  name: o_busy
#define DEBUG_BUS_ICACHE_DEBUG_BUS_BRISCV_NOC_CTRL_O_BUSY_WORD {26, 7, 0, 0, 1, 0}
#define DEBUG_BUS_ICACHE_DEBUG_BUS_BRISCV_NOC_CTRL_O_BUSY {DEBUG_BUS_ICACHE_DEBUG_BUS_BRISCV_NOC_CTRL_O_BUSY_WORD, 17, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_13
//  name: icache_debug_bus_briscv_noc_ctrl alias: icache_debug_bus note: NCRISC
//  name: req_fifo_empty
#define DEBUG_BUS_ICACHE_DEBUG_BUS_BRISCV_NOC_CTRL_REQ_FIFO_EMPTY_WORD {26, 7, 0, 0, 1, 0}
#define DEBUG_BUS_ICACHE_DEBUG_BUS_BRISCV_NOC_CTRL_REQ_FIFO_EMPTY {DEBUG_BUS_ICACHE_DEBUG_BUS_BRISCV_NOC_CTRL_REQ_FIFO_EMPTY_WORD, 16, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_13
//  name: icache_debug_bus_briscv_noc_ctrl alias: icache_debug_bus note: NCRISC
//  name: req_fifo_full
#define DEBUG_BUS_ICACHE_DEBUG_BUS_BRISCV_NOC_CTRL_REQ_FIFO_FULL_WORD {26, 7, 0, 0, 1, 0}
#define DEBUG_BUS_ICACHE_DEBUG_BUS_BRISCV_NOC_CTRL_REQ_FIFO_FULL {DEBUG_BUS_ICACHE_DEBUG_BUS_BRISCV_NOC_CTRL_REQ_FIFO_FULL_WORD, 15, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_13
//  name: icache_debug_bus_briscv_noc_ctrl alias: icache_debug_bus note: NCRISC
//  name: mshr_empty
#define DEBUG_BUS_ICACHE_DEBUG_BUS_BRISCV_NOC_CTRL_MSHR_EMPTY_WORD {26, 7, 0, 0, 1, 0}
#define DEBUG_BUS_ICACHE_DEBUG_BUS_BRISCV_NOC_CTRL_MSHR_EMPTY {DEBUG_BUS_ICACHE_DEBUG_BUS_BRISCV_NOC_CTRL_MSHR_EMPTY_WORD, 14, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_13
//  name: icache_debug_bus_briscv_noc_ctrl alias: icache_debug_bus note: NCRISC
//  name: mshr_full
#define DEBUG_BUS_ICACHE_DEBUG_BUS_BRISCV_NOC_CTRL_MSHR_FULL_WORD {26, 7, 0, 0, 1, 0}
#define DEBUG_BUS_ICACHE_DEBUG_BUS_BRISCV_NOC_CTRL_MSHR_FULL {DEBUG_BUS_ICACHE_DEBUG_BUS_BRISCV_NOC_CTRL_MSHR_FULL_WORD, 13, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_13
//  name: icache_debug_bus_briscv_noc_ctrl alias: icache_debug_bus note: NCRISC
//  name: way_hit
#define DEBUG_BUS_ICACHE_DEBUG_BUS_BRISCV_NOC_CTRL_WAY_HIT_WORD {26, 7, 0, 0, 1, 0}
#define DEBUG_BUS_ICACHE_DEBUG_BUS_BRISCV_NOC_CTRL_WAY_HIT {DEBUG_BUS_ICACHE_DEBUG_BUS_BRISCV_NOC_CTRL_WAY_HIT_WORD, 11, 0x3}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_13
//  name: icache_debug_bus_briscv_noc_ctrl alias: icache_debug_bus note: NCRISC
//  name: mshr_pf_hit
#define DEBUG_BUS_ICACHE_DEBUG_BUS_BRISCV_NOC_CTRL_MSHR_PF_HIT_WORD {26, 7, 0, 0, 1, 0}
#define DEBUG_BUS_ICACHE_DEBUG_BUS_BRISCV_NOC_CTRL_MSHR_PF_HIT {DEBUG_BUS_ICACHE_DEBUG_BUS_BRISCV_NOC_CTRL_MSHR_PF_HIT_WORD, 10, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_13
//  name: icache_debug_bus_briscv_noc_ctrl alias: icache_debug_bus note: NCRISC
//  name: mshr_cpu_hit
#define DEBUG_BUS_ICACHE_DEBUG_BUS_BRISCV_NOC_CTRL_MSHR_CPU_HIT_WORD {26, 7, 0, 0, 1, 0}
#define DEBUG_BUS_ICACHE_DEBUG_BUS_BRISCV_NOC_CTRL_MSHR_CPU_HIT {DEBUG_BUS_ICACHE_DEBUG_BUS_BRISCV_NOC_CTRL_MSHR_CPU_HIT_WORD, 9, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_13
//  name: icache_debug_bus_briscv_noc_ctrl alias: icache_debug_bus note: NCRISC
//  name: some_mshr_allocated
#define DEBUG_BUS_ICACHE_DEBUG_BUS_BRISCV_NOC_CTRL_SOME_MSHR_ALLOCATED_WORD {26, 7, 0, 0, 1, 0}
#define DEBUG_BUS_ICACHE_DEBUG_BUS_BRISCV_NOC_CTRL_SOME_MSHR_ALLOCATED {DEBUG_BUS_ICACHE_DEBUG_BUS_BRISCV_NOC_CTRL_SOME_MSHR_ALLOCATED_WORD, 8, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_13
//  name: icache_debug_bus_briscv_noc_ctrl alias: icache_debug_bus note: NCRISC
//  name: latched_req_cpu_vld
#define DEBUG_BUS_ICACHE_DEBUG_BUS_BRISCV_NOC_CTRL_LATCHED_REQ_CPU_VLD_WORD {26, 7, 0, 0, 1, 0}
#define DEBUG_BUS_ICACHE_DEBUG_BUS_BRISCV_NOC_CTRL_LATCHED_REQ_CPU_VLD {DEBUG_BUS_ICACHE_DEBUG_BUS_BRISCV_NOC_CTRL_LATCHED_REQ_CPU_VLD_WORD, 7, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_13
//  name: icache_debug_bus_briscv_noc_ctrl alias: icache_debug_bus note: NCRISC
//  name: cpu_req_dispatched
#define DEBUG_BUS_ICACHE_DEBUG_BUS_BRISCV_NOC_CTRL_CPU_REQ_DISPATCHED_WORD {26, 7, 0, 0, 1, 0}
#define DEBUG_BUS_ICACHE_DEBUG_BUS_BRISCV_NOC_CTRL_CPU_REQ_DISPATCHED {DEBUG_BUS_ICACHE_DEBUG_BUS_BRISCV_NOC_CTRL_CPU_REQ_DISPATCHED_WORD, 6, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_13
//  name: icache_debug_bus_briscv_noc_ctrl alias: icache_debug_bus note: NCRISC
//  name: latched_req_pf_vld
#define DEBUG_BUS_ICACHE_DEBUG_BUS_BRISCV_NOC_CTRL_LATCHED_REQ_PF_VLD_WORD {26, 7, 0, 0, 1, 0}
#define DEBUG_BUS_ICACHE_DEBUG_BUS_BRISCV_NOC_CTRL_LATCHED_REQ_PF_VLD {DEBUG_BUS_ICACHE_DEBUG_BUS_BRISCV_NOC_CTRL_LATCHED_REQ_PF_VLD_WORD, 5, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_13
//  name: icache_debug_bus_briscv_noc_ctrl alias: icache_debug_bus note: NCRISC
//  name: pf_req_dispatched
#define DEBUG_BUS_ICACHE_DEBUG_BUS_BRISCV_NOC_CTRL_PF_REQ_DISPATCHED_WORD {26, 7, 0, 0, 1, 0}
#define DEBUG_BUS_ICACHE_DEBUG_BUS_BRISCV_NOC_CTRL_PF_REQ_DISPATCHED {DEBUG_BUS_ICACHE_DEBUG_BUS_BRISCV_NOC_CTRL_PF_REQ_DISPATCHED_WORD, 4, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_13
//  name: icache_debug_bus_briscv_noc_ctrl alias: icache_debug_bus note: NCRISC
//  name: qual_rden
#define DEBUG_BUS_ICACHE_DEBUG_BUS_BRISCV_NOC_CTRL_QUAL_RDEN_WORD {26, 7, 0, 0, 1, 0}
#define DEBUG_BUS_ICACHE_DEBUG_BUS_BRISCV_NOC_CTRL_QUAL_RDEN {DEBUG_BUS_ICACHE_DEBUG_BUS_BRISCV_NOC_CTRL_QUAL_RDEN_WORD, 3, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_13
//  name: icache_debug_bus_briscv_noc_ctrl alias: icache_debug_bus note: NCRISC
//  name: i_mispredict
#define DEBUG_BUS_ICACHE_DEBUG_BUS_BRISCV_NOC_CTRL_I_MISPREDICT_WORD {26, 7, 0, 0, 1, 0}
#define DEBUG_BUS_ICACHE_DEBUG_BUS_BRISCV_NOC_CTRL_I_MISPREDICT {DEBUG_BUS_ICACHE_DEBUG_BUS_BRISCV_NOC_CTRL_I_MISPREDICT_WORD, 2, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_13
//  name: icache_debug_bus_briscv_noc_ctrl alias: icache_debug_bus note: NCRISC
//  name: o_req_ready
#define DEBUG_BUS_ICACHE_DEBUG_BUS_BRISCV_NOC_CTRL_O_REQ_READY_WORD {26, 7, 0, 0, 1, 0}
#define DEBUG_BUS_ICACHE_DEBUG_BUS_BRISCV_NOC_CTRL_O_REQ_READY {DEBUG_BUS_ICACHE_DEBUG_BUS_BRISCV_NOC_CTRL_O_REQ_READY_WORD, 1, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_13
//  name: icache_debug_bus_briscv_noc_ctrl alias: icache_debug_bus note: NCRISC
//  name: o_instrn_vld
#define DEBUG_BUS_ICACHE_DEBUG_BUS_BRISCV_NOC_CTRL_O_INSTRN_VLD_WORD {26, 7, 0, 0, 1, 0}
#define DEBUG_BUS_ICACHE_DEBUG_BUS_BRISCV_NOC_CTRL_O_INSTRN_VLD {DEBUG_BUS_ICACHE_DEBUG_BUS_BRISCV_NOC_CTRL_O_INSTRN_VLD_WORD, 0, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_12 alias: debug_bus_trisc note: NCRISC
//  name: ex_id_rtr
#define DEBUG_BUS_DEBUG_DAISY_STOP_TENSIX_DEBUG_TENSIX_IN_12_EX_ID_RTR__3305_WORD {25, 7, 6, 0, 1, 0}
#define DEBUG_BUS_DEBUG_DAISY_STOP_TENSIX_DEBUG_TENSIX_IN_12_EX_ID_RTR__3305 {DEBUG_BUS_DEBUG_DAISY_STOP_TENSIX_DEBUG_TENSIX_IN_12_EX_ID_RTR__3305_WORD, 9, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_12 alias: debug_bus_trisc note: NCRISC
//  name: id_ex_rts
#define DEBUG_BUS_DEBUG_DAISY_STOP_TENSIX_DEBUG_TENSIX_IN_12_ID_EX_RTS__3304_WORD {25, 7, 6, 0, 1, 0}
#define DEBUG_BUS_DEBUG_DAISY_STOP_TENSIX_DEBUG_TENSIX_IN_12_ID_EX_RTS__3304 {DEBUG_BUS_DEBUG_DAISY_STOP_TENSIX_DEBUG_TENSIX_IN_12_ID_EX_RTS__3304_WORD, 8, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_12 alias: debug_bus_trisc note: NCRISC
//  name: if_rts
#define DEBUG_BUS_DEBUG_TENSIX_IN_12_IF_RTS_WORD {25, 7, 6, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_12_IF_RTS {DEBUG_BUS_DEBUG_TENSIX_IN_12_IF_RTS_WORD, 7, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_12 alias: debug_bus_trisc note: NCRISC
//  name: if_ex_predicted
#define DEBUG_BUS_DEBUG_TENSIX_IN_12_IF_EX_PREDICTED_WORD {25, 7, 6, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_12_IF_EX_PREDICTED {DEBUG_BUS_DEBUG_TENSIX_IN_12_IF_EX_PREDICTED_WORD, 5, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_12 alias: debug_bus_trisc note: NCRISC
//  name: if_ex_deco
#define DEBUG_BUS_DEBUG_TENSIX_IN_12_IF_EX_DECO__36_32_WORD {25, 7, 6, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_12_IF_EX_DECO__36_32 {DEBUG_BUS_DEBUG_TENSIX_IN_12_IF_EX_DECO__36_32_WORD, 0, 0x1F}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_12 alias: debug_bus_trisc note: NCRISC
//  name: if_ex_deco
#define DEBUG_BUS_DEBUG_TENSIX_IN_12_IF_EX_DECO__31_0_WORD {25, 7, 4, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_12_IF_EX_DECO__31_0 {DEBUG_BUS_DEBUG_TENSIX_IN_12_IF_EX_DECO__31_0_WORD, 0, 0xFFFFFFFF}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_12 alias: debug_bus_trisc note: NCRISC
//  name: id_ex_rts
#define DEBUG_BUS_DEBUG_DAISY_STOP_TENSIX_DEBUG_TENSIX_IN_12_ID_EX_RTS__3263_WORD {25, 7, 2, 0, 1, 0}
#define DEBUG_BUS_DEBUG_DAISY_STOP_TENSIX_DEBUG_TENSIX_IN_12_ID_EX_RTS__3263 {DEBUG_BUS_DEBUG_DAISY_STOP_TENSIX_DEBUG_TENSIX_IN_12_ID_EX_RTS__3263_WORD, 31, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_12 alias: debug_bus_trisc note: NCRISC
//  name: ex_id_rtr
#define DEBUG_BUS_DEBUG_DAISY_STOP_TENSIX_DEBUG_TENSIX_IN_12_EX_ID_RTR__3262_WORD {25, 7, 2, 0, 1, 0}
#define DEBUG_BUS_DEBUG_DAISY_STOP_TENSIX_DEBUG_TENSIX_IN_12_EX_ID_RTR__3262 {DEBUG_BUS_DEBUG_DAISY_STOP_TENSIX_DEBUG_TENSIX_IN_12_EX_ID_RTR__3262_WORD, 30, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_12 alias: debug_bus_trisc note: NCRISC
//  name: id_ex_pc
#define DEBUG_BUS_DEBUG_TENSIX_IN_12_ID_EX_PC__29_0_WORD {25, 7, 2, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_12_ID_EX_PC__29_0 {DEBUG_BUS_DEBUG_TENSIX_IN_12_ID_EX_PC__29_0_WORD, 0, 0x3FFFFFFF}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_12 alias: debug_bus_trisc note: NCRISC
//  name: id_rf_wr_flag
#define DEBUG_BUS_DEBUG_TENSIX_IN_12_ID_RF_WR_FLAG_WORD {25, 7, 0, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_12_ID_RF_WR_FLAG {DEBUG_BUS_DEBUG_TENSIX_IN_12_ID_RF_WR_FLAG_WORD, 28, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_12 alias: debug_bus_trisc note: NCRISC
//  name: id_rf_wraddr
#define DEBUG_BUS_DEBUG_TENSIX_IN_12_ID_RF_WRADDR_WORD {25, 7, 0, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_12_ID_RF_WRADDR {DEBUG_BUS_DEBUG_TENSIX_IN_12_ID_RF_WRADDR_WORD, 20, 0x1F}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_12 alias: debug_bus_trisc note: NCRISC
//  name: id_rf_p1_rden
#define DEBUG_BUS_DEBUG_TENSIX_IN_12_ID_RF_P1_RDEN_WORD {25, 7, 0, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_12_ID_RF_P1_RDEN {DEBUG_BUS_DEBUG_TENSIX_IN_12_ID_RF_P1_RDEN_WORD, 18, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_12 alias: debug_bus_trisc note: NCRISC
//  name: id_rf_p1_rdaddr
#define DEBUG_BUS_DEBUG_TENSIX_IN_12_ID_RF_P1_RDADDR_WORD {25, 7, 0, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_12_ID_RF_P1_RDADDR {DEBUG_BUS_DEBUG_TENSIX_IN_12_ID_RF_P1_RDADDR_WORD, 10, 0x1F}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_12 alias: debug_bus_trisc note: NCRISC
//  name: id_rf_p0_rden
#define DEBUG_BUS_DEBUG_TENSIX_IN_12_ID_RF_P0_RDEN_WORD {25, 7, 0, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_12_ID_RF_P0_RDEN {DEBUG_BUS_DEBUG_TENSIX_IN_12_ID_RF_P0_RDEN_WORD, 8, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_12 alias: debug_bus_trisc note: NCRISC
//  name: id_rf_p0_rdaddr
#define DEBUG_BUS_DEBUG_TENSIX_IN_12_ID_RF_P0_RDADDR_WORD {25, 7, 0, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_12_ID_RF_P0_RDADDR {DEBUG_BUS_DEBUG_TENSIX_IN_12_ID_RF_P0_RDADDR_WORD, 0, 0x1F}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_12 alias: debug_bus_trisc note: NCRISC
//  name: i_instrn_vld
#define DEBUG_BUS_DEBUG_TENSIX_IN_12_I_INSTRN_VLD_WORD {24, 7, 6, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_12_I_INSTRN_VLD {DEBUG_BUS_DEBUG_TENSIX_IN_12_I_INSTRN_VLD_WORD, 31, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_12 alias: debug_bus_trisc note: NCRISC
//  name: i_instrn
#define DEBUG_BUS_DEBUG_TENSIX_IN_12_I_INSTRN__30_0_WORD {24, 7, 6, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_12_I_INSTRN__30_0 {DEBUG_BUS_DEBUG_TENSIX_IN_12_I_INSTRN__30_0_WORD, 0, 0x7FFFFFFF}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_12 alias: debug_bus_trisc note: NCRISC
//  name: i_instrn_req_rtr
#define DEBUG_BUS_DEBUG_TENSIX_IN_12_I_INSTRN_REQ_RTR_WORD {24, 7, 4, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_12_I_INSTRN_REQ_RTR {DEBUG_BUS_DEBUG_TENSIX_IN_12_I_INSTRN_REQ_RTR_WORD, 31, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_12 alias: debug_bus_trisc note: NCRISC
//  name: (o_instrn_req_early&~o_instrn_req_cancel)
#define DEBUG_BUS_DEBUG_TENSIX_IN_12__O_INSTRN_REQ_EARLY__O_INSTRN_REQ_CANCEL__WORD {24, 7, 4, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_12__O_INSTRN_REQ_EARLY__O_INSTRN_REQ_CANCEL_ {DEBUG_BUS_DEBUG_TENSIX_IN_12__O_INSTRN_REQ_EARLY__O_INSTRN_REQ_CANCEL__WORD, 30, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_12 alias: debug_bus_trisc note: NCRISC
//  name: o_instrn_addr
#define DEBUG_BUS_DEBUG_TENSIX_IN_12_O_INSTRN_ADDR__29_0_WORD {24, 7, 4, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_12_O_INSTRN_ADDR__29_0 {DEBUG_BUS_DEBUG_TENSIX_IN_12_O_INSTRN_ADDR__29_0_WORD, 0, 0x3FFFFFFF}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_12 alias: debug_bus_trisc note: NCRISC
//  name: dbg_obs_mem_wren
#define DEBUG_BUS_DEBUG_TENSIX_IN_12_DBG_OBS_MEM_WREN_WORD {24, 7, 2, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_12_DBG_OBS_MEM_WREN {DEBUG_BUS_DEBUG_TENSIX_IN_12_DBG_OBS_MEM_WREN_WORD, 31, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_12 alias: debug_bus_trisc note: NCRISC
//  name: dbg_obs_mem_rden
#define DEBUG_BUS_DEBUG_TENSIX_IN_12_DBG_OBS_MEM_RDEN_WORD {24, 7, 2, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_12_DBG_OBS_MEM_RDEN {DEBUG_BUS_DEBUG_TENSIX_IN_12_DBG_OBS_MEM_RDEN_WORD, 30, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_12 alias: debug_bus_trisc note: NCRISC
//  name: dbg_obs_mem_addr
#define DEBUG_BUS_DEBUG_TENSIX_IN_12_DBG_OBS_MEM_ADDR__29_0_WORD {24, 7, 2, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_12_DBG_OBS_MEM_ADDR__29_0 {DEBUG_BUS_DEBUG_TENSIX_IN_12_DBG_OBS_MEM_ADDR__29_0_WORD, 0, 0x3FFFFFFF}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_12 alias: debug_bus_trisc note: NCRISC
//  name: dbg_obs_cmt_vld
#define DEBUG_BUS_DEBUG_TENSIX_IN_12_DBG_OBS_CMT_VLD_WORD {24, 7, 0, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_12_DBG_OBS_CMT_VLD {DEBUG_BUS_DEBUG_TENSIX_IN_12_DBG_OBS_CMT_VLD_WORD, 31, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_12 alias: debug_bus_trisc note: NCRISC
//  name: dbg_obs_cmt_pc
#define DEBUG_BUS_DEBUG_TENSIX_IN_12_DBG_OBS_CMT_PC__30_0_WORD {24, 7, 0, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_12_DBG_OBS_CMT_PC__30_0 {DEBUG_BUS_DEBUG_TENSIX_IN_12_DBG_OBS_CMT_PC__30_0_WORD, 0, 0x7FFFFFFF}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_11 alias: trisc_wrapper_debug_bus note: TRISC2
//  name: trisc_mop_buf_empty
#define DEBUG_BUS_DEBUG_TENSIX_IN_11_TRISC_MOP_BUF_EMPTY_WORD {23, 7, 6, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_11_TRISC_MOP_BUF_EMPTY {DEBUG_BUS_DEBUG_TENSIX_IN_11_TRISC_MOP_BUF_EMPTY_WORD, 30, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_11 alias: trisc_wrapper_debug_bus note: TRISC2
//  name: trisc_mop_buf_full
#define DEBUG_BUS_DEBUG_TENSIX_IN_11_TRISC_MOP_BUF_FULL_WORD {23, 7, 6, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_11_TRISC_MOP_BUF_FULL {DEBUG_BUS_DEBUG_TENSIX_IN_11_TRISC_MOP_BUF_FULL_WORD, 29, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_11 alias: trisc_wrapper_debug_bus note: TRISC2
//  name: mop_decode_debug_bus
//  name: debug_math_loop_state
#define DEBUG_BUS_DEBUG_TENSIX_IN_11_MOP_DECODE_DEBUG_BUS_DEBUG_MATH_LOOP_STATE_WORD {23, 7, 6, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_11_MOP_DECODE_DEBUG_BUS_DEBUG_MATH_LOOP_STATE {DEBUG_BUS_DEBUG_TENSIX_IN_11_MOP_DECODE_DEBUG_BUS_DEBUG_MATH_LOOP_STATE_WORD, 26, 0x7}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_11 alias: trisc_wrapper_debug_bus note: TRISC2
//  name: mop_decode_debug_bus
//  name: debug_unpack_loop_state
#define DEBUG_BUS_DEBUG_TENSIX_IN_11_MOP_DECODE_DEBUG_BUS_DEBUG_UNPACK_LOOP_STATE_WORD {23, 7, 6, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_11_MOP_DECODE_DEBUG_BUS_DEBUG_UNPACK_LOOP_STATE {DEBUG_BUS_DEBUG_TENSIX_IN_11_MOP_DECODE_DEBUG_BUS_DEBUG_UNPACK_LOOP_STATE_WORD, 23, 0x7}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_11 alias: trisc_wrapper_debug_bus note: TRISC2
//  name: mop_decode_debug_bus
//  name: mop_stage_valid
#define DEBUG_BUS_DEBUG_TENSIX_IN_11_MOP_DECODE_DEBUG_BUS_MOP_STAGE_VALID_WORD {23, 7, 6, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_11_MOP_DECODE_DEBUG_BUS_MOP_STAGE_VALID {DEBUG_BUS_DEBUG_TENSIX_IN_11_MOP_DECODE_DEBUG_BUS_MOP_STAGE_VALID_WORD, 22, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_11 alias: trisc_wrapper_debug_bus note: TRISC2
//  name: mop_decode_debug_bus
//  name: mop_stage_opcode
#define DEBUG_BUS_DEBUG_TENSIX_IN_11_MOP_DECODE_DEBUG_BUS_MOP_STAGE_OPCODE__9_0_WORD {23, 7, 4, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_11_MOP_DECODE_DEBUG_BUS_MOP_STAGE_OPCODE__9_0 {DEBUG_BUS_DEBUG_TENSIX_IN_11_MOP_DECODE_DEBUG_BUS_MOP_STAGE_OPCODE__9_0_WORD, 22, 0x3FF}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_11 alias: trisc_wrapper_debug_bus note: TRISC2
//  name: mop_decode_debug_bus
//  name: mop_stage_opcode
#define DEBUG_BUS_DEBUG_TENSIX_IN_11_MOP_DECODE_DEBUG_BUS_MOP_STAGE_OPCODE__31_10_WORD {23, 7, 6, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_11_MOP_DECODE_DEBUG_BUS_MOP_STAGE_OPCODE__31_10 {DEBUG_BUS_DEBUG_TENSIX_IN_11_MOP_DECODE_DEBUG_BUS_MOP_STAGE_OPCODE__31_10_WORD, 0, 0x3FFFFF}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_11 alias: trisc_wrapper_debug_bus note: TRISC2
//  name: mop_decode_debug_bus
//  name: math_loop_active
#define DEBUG_BUS_DEBUG_TENSIX_IN_11_MOP_DECODE_DEBUG_BUS_MATH_LOOP_ACTIVE_WORD {23, 7, 4, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_11_MOP_DECODE_DEBUG_BUS_MATH_LOOP_ACTIVE {DEBUG_BUS_DEBUG_TENSIX_IN_11_MOP_DECODE_DEBUG_BUS_MATH_LOOP_ACTIVE_WORD, 21, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_11 alias: trisc_wrapper_debug_bus note: TRISC2
//  name: mop_decode_debug_bus
//  name: unpack_loop_active
#define DEBUG_BUS_DEBUG_TENSIX_IN_11_MOP_DECODE_DEBUG_BUS_UNPACK_LOOP_ACTIVE_WORD {23, 7, 4, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_11_MOP_DECODE_DEBUG_BUS_UNPACK_LOOP_ACTIVE {DEBUG_BUS_DEBUG_TENSIX_IN_11_MOP_DECODE_DEBUG_BUS_UNPACK_LOOP_ACTIVE_WORD, 20, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_11 alias: trisc_wrapper_debug_bus note: TRISC2
//  name: mop_decode_debug_bus
//  name: o_instrn_valid
#define DEBUG_BUS_DEBUG_TENSIX_IN_11_MOP_DECODE_DEBUG_BUS_O_INSTRN_VALID_WORD {23, 7, 4, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_11_MOP_DECODE_DEBUG_BUS_O_INSTRN_VALID {DEBUG_BUS_DEBUG_TENSIX_IN_11_MOP_DECODE_DEBUG_BUS_O_INSTRN_VALID_WORD, 19, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_11 alias: trisc_wrapper_debug_bus note: TRISC2
//  name: mop_decode_debug_bus
//  name: o_instrn_opcode
#define DEBUG_BUS_DEBUG_TENSIX_IN_11_MOP_DECODE_DEBUG_BUS_O_INSTRN_OPCODE__12_0_WORD {23, 7, 2, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_11_MOP_DECODE_DEBUG_BUS_O_INSTRN_OPCODE__12_0 {DEBUG_BUS_DEBUG_TENSIX_IN_11_MOP_DECODE_DEBUG_BUS_O_INSTRN_OPCODE__12_0_WORD, 19, 0x1FFF}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_11 alias: trisc_wrapper_debug_bus note: TRISC2
//  name: mop_decode_debug_bus
//  name: o_instrn_opcode
#define DEBUG_BUS_DEBUG_TENSIX_IN_11_MOP_DECODE_DEBUG_BUS_O_INSTRN_OPCODE__31_13_WORD {23, 7, 4, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_11_MOP_DECODE_DEBUG_BUS_O_INSTRN_OPCODE__31_13 {DEBUG_BUS_DEBUG_TENSIX_IN_11_MOP_DECODE_DEBUG_BUS_O_INSTRN_OPCODE__31_13_WORD, 0, 0x7FFFF}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_11 alias: trisc_wrapper_debug_bus note: TRISC2
//  name: pc_buffer_debug_bus
//  name: sempost_pending
#define DEBUG_BUS_DEBUG_TENSIX_IN_11_PC_BUFFER_DEBUG_BUS_SEMPOST_PENDING_WORD {23, 7, 2, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_11_PC_BUFFER_DEBUG_BUS_SEMPOST_PENDING {DEBUG_BUS_DEBUG_TENSIX_IN_11_PC_BUFFER_DEBUG_BUS_SEMPOST_PENDING_WORD, 8, 0xFF}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_11 alias: trisc_wrapper_debug_bus note: TRISC2
//  name: pc_buffer_debug_bus
//  name: semget_pending
#define DEBUG_BUS_DEBUG_TENSIX_IN_11_PC_BUFFER_DEBUG_BUS_SEMGET_PENDING_WORD {23, 7, 2, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_11_PC_BUFFER_DEBUG_BUS_SEMGET_PENDING {DEBUG_BUS_DEBUG_TENSIX_IN_11_PC_BUFFER_DEBUG_BUS_SEMGET_PENDING_WORD, 0, 0xFF}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_11 alias: trisc_wrapper_debug_bus note: TRISC2
//  name: pc_buffer_debug_bus
//  name: trisc_read_request_pending
#define DEBUG_BUS_DEBUG_TENSIX_IN_11_PC_BUFFER_DEBUG_BUS_TRISC_READ_REQUEST_PENDING_WORD {23, 7, 0, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_11_PC_BUFFER_DEBUG_BUS_TRISC_READ_REQUEST_PENDING {DEBUG_BUS_DEBUG_TENSIX_IN_11_PC_BUFFER_DEBUG_BUS_TRISC_READ_REQUEST_PENDING_WORD, 31, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_11 alias: trisc_wrapper_debug_bus note: TRISC2
//  name: pc_buffer_debug_bus
//  name: trisc_sync_activated
#define DEBUG_BUS_DEBUG_TENSIX_IN_11_PC_BUFFER_DEBUG_BUS_TRISC_SYNC_ACTIVATED_WORD {23, 7, 0, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_11_PC_BUFFER_DEBUG_BUS_TRISC_SYNC_ACTIVATED {DEBUG_BUS_DEBUG_TENSIX_IN_11_PC_BUFFER_DEBUG_BUS_TRISC_SYNC_ACTIVATED_WORD, 30, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_11 alias: trisc_wrapper_debug_bus note: TRISC2
//  name: pc_buffer_debug_bus
//  name: trisc_sync_type
#define DEBUG_BUS_DEBUG_TENSIX_IN_11_PC_BUFFER_DEBUG_BUS_TRISC_SYNC_TYPE_WORD {23, 7, 0, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_11_PC_BUFFER_DEBUG_BUS_TRISC_SYNC_TYPE {DEBUG_BUS_DEBUG_TENSIX_IN_11_PC_BUFFER_DEBUG_BUS_TRISC_SYNC_TYPE_WORD, 29, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_11 alias: trisc_wrapper_debug_bus note: TRISC2
//  name: pc_buffer_debug_bus
//  name: riscv_sync_activated
#define DEBUG_BUS_DEBUG_TENSIX_IN_11_PC_BUFFER_DEBUG_BUS_RISCV_SYNC_ACTIVATED_WORD {23, 7, 0, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_11_PC_BUFFER_DEBUG_BUS_RISCV_SYNC_ACTIVATED {DEBUG_BUS_DEBUG_TENSIX_IN_11_PC_BUFFER_DEBUG_BUS_RISCV_SYNC_ACTIVATED_WORD, 28, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_11 alias: trisc_wrapper_debug_bus note: TRISC2
//  name: pc_buffer_debug_bus
//  name: pc_buffer_idle
#define DEBUG_BUS_DEBUG_TENSIX_IN_11_PC_BUFFER_DEBUG_BUS_PC_BUFFER_IDLE_WORD {23, 7, 0, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_11_PC_BUFFER_DEBUG_BUS_PC_BUFFER_IDLE {DEBUG_BUS_DEBUG_TENSIX_IN_11_PC_BUFFER_DEBUG_BUS_PC_BUFFER_IDLE_WORD, 27, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_11 alias: trisc_wrapper_debug_bus note: TRISC2
//  name: pc_buffer_debug_bus
//  name: i_busy
#define DEBUG_BUS_DEBUG_TENSIX_IN_11_PC_BUFFER_DEBUG_BUS_I_BUSY_WORD {23, 7, 0, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_11_PC_BUFFER_DEBUG_BUS_I_BUSY {DEBUG_BUS_DEBUG_TENSIX_IN_11_PC_BUFFER_DEBUG_BUS_I_BUSY_WORD, 26, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_11 alias: trisc_wrapper_debug_bus note: TRISC2
//  name: pc_buffer_debug_bus
//  name: i_mops_outstanding
#define DEBUG_BUS_DEBUG_TENSIX_IN_11_PC_BUFFER_DEBUG_BUS_I_MOPS_OUTSTANDING_WORD {23, 7, 0, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_11_PC_BUFFER_DEBUG_BUS_I_MOPS_OUTSTANDING {DEBUG_BUS_DEBUG_TENSIX_IN_11_PC_BUFFER_DEBUG_BUS_I_MOPS_OUTSTANDING_WORD, 25, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_11 alias: trisc_wrapper_debug_bus note: TRISC2
//  name: pc_buffer_debug_bus
//  name: cmd_fifo_full
#define DEBUG_BUS_DEBUG_TENSIX_IN_11_PC_BUFFER_DEBUG_BUS_CMD_FIFO_FULL_WORD {23, 7, 0, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_11_PC_BUFFER_DEBUG_BUS_CMD_FIFO_FULL {DEBUG_BUS_DEBUG_TENSIX_IN_11_PC_BUFFER_DEBUG_BUS_CMD_FIFO_FULL_WORD, 24, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_11 alias: trisc_wrapper_debug_bus note: TRISC2
//  name: pc_buffer_debug_bus
//  name: cmd_fifo_empty
#define DEBUG_BUS_DEBUG_TENSIX_IN_11_PC_BUFFER_DEBUG_BUS_CMD_FIFO_EMPTY_WORD {23, 7, 0, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_11_PC_BUFFER_DEBUG_BUS_CMD_FIFO_EMPTY {DEBUG_BUS_DEBUG_TENSIX_IN_11_PC_BUFFER_DEBUG_BUS_CMD_FIFO_EMPTY_WORD, 23, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_11 alias: trisc_wrapper_debug_bus note: TRISC2
//  name: pc_buffer_debug_bus
//  name: next_cmd_fifo_data
#define DEBUG_BUS_DEBUG_TENSIX_IN_11_PC_BUFFER_DEBUG_BUS_NEXT_CMD_FIFO_DATA__8_0_WORD {22, 7, 6, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_11_PC_BUFFER_DEBUG_BUS_NEXT_CMD_FIFO_DATA__8_0 {DEBUG_BUS_DEBUG_TENSIX_IN_11_PC_BUFFER_DEBUG_BUS_NEXT_CMD_FIFO_DATA__8_0_WORD, 23, 0x1FF}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_11 alias: trisc_wrapper_debug_bus note: TRISC2
//  name: pc_buffer_debug_bus
//  name: next_cmd_fifo_data
#define DEBUG_BUS_DEBUG_TENSIX_IN_11_PC_BUFFER_DEBUG_BUS_NEXT_CMD_FIFO_DATA__31_9_WORD {23, 7, 0, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_11_PC_BUFFER_DEBUG_BUS_NEXT_CMD_FIFO_DATA__31_9 {DEBUG_BUS_DEBUG_TENSIX_IN_11_PC_BUFFER_DEBUG_BUS_NEXT_CMD_FIFO_DATA__31_9_WORD, 0, 0x7FFFFF}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_11 alias: trisc_wrapper_debug_bus note: TRISC2
//  name: risc_wrapper_debug_bus_trisc
//  name: o_par_err_risc_localmem
#define DEBUG_BUS_DEBUG_TENSIX_IN_11_RISC_WRAPPER_DEBUG_BUS_TRISC_O_PAR_ERR_RISC_LOCALMEM_WORD {22, 7, 6, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_11_RISC_WRAPPER_DEBUG_BUS_TRISC_O_PAR_ERR_RISC_LOCALMEM {DEBUG_BUS_DEBUG_TENSIX_IN_11_RISC_WRAPPER_DEBUG_BUS_TRISC_O_PAR_ERR_RISC_LOCALMEM_WORD, 22, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_11 alias: trisc_wrapper_debug_bus note: TRISC2
//  name: risc_wrapper_debug_bus_trisc
//  name: i_mailbox_rden
#define DEBUG_BUS_DEBUG_TENSIX_IN_11_RISC_WRAPPER_DEBUG_BUS_TRISC_I_MAILBOX_RDEN_WORD {22, 7, 6, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_11_RISC_WRAPPER_DEBUG_BUS_TRISC_I_MAILBOX_RDEN {DEBUG_BUS_DEBUG_TENSIX_IN_11_RISC_WRAPPER_DEBUG_BUS_TRISC_I_MAILBOX_RDEN_WORD, 18, 0xF}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_11 alias: trisc_wrapper_debug_bus note: TRISC2
//  name: risc_wrapper_debug_bus_trisc
//  name: i_mailbox_rd_type
#define DEBUG_BUS_DEBUG_TENSIX_IN_11_RISC_WRAPPER_DEBUG_BUS_TRISC_I_MAILBOX_RD_TYPE_WORD {22, 7, 6, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_11_RISC_WRAPPER_DEBUG_BUS_TRISC_I_MAILBOX_RD_TYPE {DEBUG_BUS_DEBUG_TENSIX_IN_11_RISC_WRAPPER_DEBUG_BUS_TRISC_I_MAILBOX_RD_TYPE_WORD, 14, 0xF}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_11 alias: trisc_wrapper_debug_bus note: TRISC2
//  name: risc_wrapper_debug_bus_trisc
//  name: o_mailbox_rd_req_ready
#define DEBUG_BUS_DEBUG_TENSIX_IN_11_RISC_WRAPPER_DEBUG_BUS_TRISC_O_MAILBOX_RD_REQ_READY_WORD {22, 7, 6, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_11_RISC_WRAPPER_DEBUG_BUS_TRISC_O_MAILBOX_RD_REQ_READY {DEBUG_BUS_DEBUG_TENSIX_IN_11_RISC_WRAPPER_DEBUG_BUS_TRISC_O_MAILBOX_RD_REQ_READY_WORD, 10, 0xF}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_11 alias: trisc_wrapper_debug_bus note: TRISC2
//  name: risc_wrapper_debug_bus_trisc
//  name: o_mailbox_rdvalid
#define DEBUG_BUS_DEBUG_TENSIX_IN_11_RISC_WRAPPER_DEBUG_BUS_TRISC_O_MAILBOX_RDVALID_WORD {22, 7, 6, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_11_RISC_WRAPPER_DEBUG_BUS_TRISC_O_MAILBOX_RDVALID {DEBUG_BUS_DEBUG_TENSIX_IN_11_RISC_WRAPPER_DEBUG_BUS_TRISC_O_MAILBOX_RDVALID_WORD, 6, 0xF}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_11 alias: trisc_wrapper_debug_bus note: TRISC2
//  name: risc_wrapper_debug_bus_trisc
//  name: o_mailbox_rddata
#define DEBUG_BUS_DEBUG_TENSIX_IN_11_RISC_WRAPPER_DEBUG_BUS_TRISC_O_MAILBOX_RDDATA__15_0_WORD {22, 7, 4, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_11_RISC_WRAPPER_DEBUG_BUS_TRISC_O_MAILBOX_RDDATA__15_0 {DEBUG_BUS_DEBUG_TENSIX_IN_11_RISC_WRAPPER_DEBUG_BUS_TRISC_O_MAILBOX_RDDATA__15_0_WORD, 16, 0xFFFF}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_11 alias: trisc_wrapper_debug_bus note: TRISC2
//  name: risc_wrapper_debug_bus_trisc
//  name: o_mailbox_rddata
#define DEBUG_BUS_DEBUG_TENSIX_IN_11_RISC_WRAPPER_DEBUG_BUS_TRISC_O_MAILBOX_RDDATA__21_16_WORD {22, 7, 6, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_11_RISC_WRAPPER_DEBUG_BUS_TRISC_O_MAILBOX_RDDATA__21_16 {DEBUG_BUS_DEBUG_TENSIX_IN_11_RISC_WRAPPER_DEBUG_BUS_TRISC_O_MAILBOX_RDDATA__21_16_WORD, 0, 0x3F}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_11 alias: trisc_wrapper_debug_bus note: TRISC2
//  name: risc_wrapper_debug_bus_trisc
//  name: intf_wrack_trisc
#define DEBUG_BUS_DEBUG_TENSIX_IN_11_RISC_WRAPPER_DEBUG_BUS_TRISC_INTF_WRACK_TRISC__12_0_WORD {22, 7, 0, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_11_RISC_WRAPPER_DEBUG_BUS_TRISC_INTF_WRACK_TRISC__12_0 {DEBUG_BUS_DEBUG_TENSIX_IN_11_RISC_WRAPPER_DEBUG_BUS_TRISC_INTF_WRACK_TRISC__12_0_WORD, 17, 0x1FFF}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_11 alias: trisc_wrapper_debug_bus note: TRISC2
//  name: risc_wrapper_debug_bus_trisc
//  name: dmem_tensix_rden
#define DEBUG_BUS_DEBUG_TENSIX_IN_11_RISC_WRAPPER_DEBUG_BUS_TRISC_DMEM_TENSIX_RDEN_WORD {22, 7, 0, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_11_RISC_WRAPPER_DEBUG_BUS_TRISC_DMEM_TENSIX_RDEN {DEBUG_BUS_DEBUG_TENSIX_IN_11_RISC_WRAPPER_DEBUG_BUS_TRISC_DMEM_TENSIX_RDEN_WORD, 16, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_11 alias: trisc_wrapper_debug_bus note: TRISC2
//  name: risc_wrapper_debug_bus_trisc
//  name: dmem_tensix_wren
#define DEBUG_BUS_DEBUG_TENSIX_IN_11_RISC_WRAPPER_DEBUG_BUS_TRISC_DMEM_TENSIX_WREN_WORD {22, 7, 0, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_11_RISC_WRAPPER_DEBUG_BUS_TRISC_DMEM_TENSIX_WREN {DEBUG_BUS_DEBUG_TENSIX_IN_11_RISC_WRAPPER_DEBUG_BUS_TRISC_DMEM_TENSIX_WREN_WORD, 15, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_11 alias: trisc_wrapper_debug_bus note: TRISC2
//  name: risc_wrapper_debug_bus_trisc
//  name: icache_req_fifo_full
#define DEBUG_BUS_DEBUG_TENSIX_IN_11_RISC_WRAPPER_DEBUG_BUS_TRISC_ICACHE_REQ_FIFO_FULL_WORD {22, 7, 0, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_11_RISC_WRAPPER_DEBUG_BUS_TRISC_ICACHE_REQ_FIFO_FULL {DEBUG_BUS_DEBUG_TENSIX_IN_11_RISC_WRAPPER_DEBUG_BUS_TRISC_ICACHE_REQ_FIFO_FULL_WORD, 1, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_11 alias: trisc_wrapper_debug_bus note: TRISC2
//  name: risc_wrapper_debug_bus_trisc
//  name: icache_req_fifo_empty
#define DEBUG_BUS_DEBUG_TENSIX_IN_11_RISC_WRAPPER_DEBUG_BUS_TRISC_ICACHE_REQ_FIFO_EMPTY_WORD {22, 7, 0, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_11_RISC_WRAPPER_DEBUG_BUS_TRISC_ICACHE_REQ_FIFO_EMPTY {DEBUG_BUS_DEBUG_TENSIX_IN_11_RISC_WRAPPER_DEBUG_BUS_TRISC_ICACHE_REQ_FIFO_EMPTY_WORD, 0, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_10 alias: trisc_wrapper_debug_bus note: TRISC1
//  name: trisc_mop_buf_empty
#define DEBUG_BUS_DEBUG_TENSIX_IN_10_TRISC_MOP_BUF_EMPTY_WORD {21, 7, 6, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_10_TRISC_MOP_BUF_EMPTY {DEBUG_BUS_DEBUG_TENSIX_IN_10_TRISC_MOP_BUF_EMPTY_WORD, 30, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_10 alias: trisc_wrapper_debug_bus note: TRISC1
//  name: trisc_mop_buf_full
#define DEBUG_BUS_DEBUG_TENSIX_IN_10_TRISC_MOP_BUF_FULL_WORD {21, 7, 6, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_10_TRISC_MOP_BUF_FULL {DEBUG_BUS_DEBUG_TENSIX_IN_10_TRISC_MOP_BUF_FULL_WORD, 29, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_10 alias: trisc_wrapper_debug_bus note: TRISC1
//  name: mop_decode_debug_bus
//  name: debug_math_loop_state
#define DEBUG_BUS_DEBUG_TENSIX_IN_10_MOP_DECODE_DEBUG_BUS_DEBUG_MATH_LOOP_STATE_WORD {21, 7, 6, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_10_MOP_DECODE_DEBUG_BUS_DEBUG_MATH_LOOP_STATE {DEBUG_BUS_DEBUG_TENSIX_IN_10_MOP_DECODE_DEBUG_BUS_DEBUG_MATH_LOOP_STATE_WORD, 26, 0x7}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_10 alias: trisc_wrapper_debug_bus note: TRISC1
//  name: mop_decode_debug_bus
//  name: debug_unpack_loop_state
#define DEBUG_BUS_DEBUG_TENSIX_IN_10_MOP_DECODE_DEBUG_BUS_DEBUG_UNPACK_LOOP_STATE_WORD {21, 7, 6, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_10_MOP_DECODE_DEBUG_BUS_DEBUG_UNPACK_LOOP_STATE {DEBUG_BUS_DEBUG_TENSIX_IN_10_MOP_DECODE_DEBUG_BUS_DEBUG_UNPACK_LOOP_STATE_WORD, 23, 0x7}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_10 alias: trisc_wrapper_debug_bus note: TRISC1
//  name: mop_decode_debug_bus
//  name: mop_stage_valid
#define DEBUG_BUS_DEBUG_TENSIX_IN_10_MOP_DECODE_DEBUG_BUS_MOP_STAGE_VALID_WORD {21, 7, 6, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_10_MOP_DECODE_DEBUG_BUS_MOP_STAGE_VALID {DEBUG_BUS_DEBUG_TENSIX_IN_10_MOP_DECODE_DEBUG_BUS_MOP_STAGE_VALID_WORD, 22, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_10 alias: trisc_wrapper_debug_bus note: TRISC1
//  name: mop_decode_debug_bus
//  name: mop_stage_opcode
#define DEBUG_BUS_DEBUG_TENSIX_IN_10_MOP_DECODE_DEBUG_BUS_MOP_STAGE_OPCODE__9_0_WORD {21, 7, 4, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_10_MOP_DECODE_DEBUG_BUS_MOP_STAGE_OPCODE__9_0 {DEBUG_BUS_DEBUG_TENSIX_IN_10_MOP_DECODE_DEBUG_BUS_MOP_STAGE_OPCODE__9_0_WORD, 22, 0x3FF}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_10 alias: trisc_wrapper_debug_bus note: TRISC1
//  name: mop_decode_debug_bus
//  name: mop_stage_opcode
#define DEBUG_BUS_DEBUG_TENSIX_IN_10_MOP_DECODE_DEBUG_BUS_MOP_STAGE_OPCODE__31_10_WORD {21, 7, 6, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_10_MOP_DECODE_DEBUG_BUS_MOP_STAGE_OPCODE__31_10 {DEBUG_BUS_DEBUG_TENSIX_IN_10_MOP_DECODE_DEBUG_BUS_MOP_STAGE_OPCODE__31_10_WORD, 0, 0x3FFFFF}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_10 alias: trisc_wrapper_debug_bus note: TRISC1
//  name: mop_decode_debug_bus
//  name: math_loop_active
#define DEBUG_BUS_DEBUG_TENSIX_IN_10_MOP_DECODE_DEBUG_BUS_MATH_LOOP_ACTIVE_WORD {21, 7, 4, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_10_MOP_DECODE_DEBUG_BUS_MATH_LOOP_ACTIVE {DEBUG_BUS_DEBUG_TENSIX_IN_10_MOP_DECODE_DEBUG_BUS_MATH_LOOP_ACTIVE_WORD, 21, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_10 alias: trisc_wrapper_debug_bus note: TRISC1
//  name: mop_decode_debug_bus
//  name: unpack_loop_active
#define DEBUG_BUS_DEBUG_TENSIX_IN_10_MOP_DECODE_DEBUG_BUS_UNPACK_LOOP_ACTIVE_WORD {21, 7, 4, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_10_MOP_DECODE_DEBUG_BUS_UNPACK_LOOP_ACTIVE {DEBUG_BUS_DEBUG_TENSIX_IN_10_MOP_DECODE_DEBUG_BUS_UNPACK_LOOP_ACTIVE_WORD, 20, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_10 alias: trisc_wrapper_debug_bus note: TRISC1
//  name: mop_decode_debug_bus
//  name: o_instrn_valid
#define DEBUG_BUS_DEBUG_TENSIX_IN_10_MOP_DECODE_DEBUG_BUS_O_INSTRN_VALID_WORD {21, 7, 4, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_10_MOP_DECODE_DEBUG_BUS_O_INSTRN_VALID {DEBUG_BUS_DEBUG_TENSIX_IN_10_MOP_DECODE_DEBUG_BUS_O_INSTRN_VALID_WORD, 19, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_10 alias: trisc_wrapper_debug_bus note: TRISC1
//  name: mop_decode_debug_bus
//  name: o_instrn_opcode
#define DEBUG_BUS_DEBUG_TENSIX_IN_10_MOP_DECODE_DEBUG_BUS_O_INSTRN_OPCODE__12_0_WORD {21, 7, 2, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_10_MOP_DECODE_DEBUG_BUS_O_INSTRN_OPCODE__12_0 {DEBUG_BUS_DEBUG_TENSIX_IN_10_MOP_DECODE_DEBUG_BUS_O_INSTRN_OPCODE__12_0_WORD, 19, 0x1FFF}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_10 alias: trisc_wrapper_debug_bus note: TRISC1
//  name: mop_decode_debug_bus
//  name: o_instrn_opcode
#define DEBUG_BUS_DEBUG_TENSIX_IN_10_MOP_DECODE_DEBUG_BUS_O_INSTRN_OPCODE__31_13_WORD {21, 7, 4, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_10_MOP_DECODE_DEBUG_BUS_O_INSTRN_OPCODE__31_13 {DEBUG_BUS_DEBUG_TENSIX_IN_10_MOP_DECODE_DEBUG_BUS_O_INSTRN_OPCODE__31_13_WORD, 0, 0x7FFFF}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_10 alias: trisc_wrapper_debug_bus note: TRISC1
//  name: pc_buffer_debug_bus
//  name: sempost_pending
#define DEBUG_BUS_DEBUG_TENSIX_IN_10_PC_BUFFER_DEBUG_BUS_SEMPOST_PENDING_WORD {21, 7, 2, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_10_PC_BUFFER_DEBUG_BUS_SEMPOST_PENDING {DEBUG_BUS_DEBUG_TENSIX_IN_10_PC_BUFFER_DEBUG_BUS_SEMPOST_PENDING_WORD, 8, 0xFF}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_10 alias: trisc_wrapper_debug_bus note: TRISC1
//  name: pc_buffer_debug_bus
//  name: semget_pending
#define DEBUG_BUS_DEBUG_TENSIX_IN_10_PC_BUFFER_DEBUG_BUS_SEMGET_PENDING_WORD {21, 7, 2, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_10_PC_BUFFER_DEBUG_BUS_SEMGET_PENDING {DEBUG_BUS_DEBUG_TENSIX_IN_10_PC_BUFFER_DEBUG_BUS_SEMGET_PENDING_WORD, 0, 0xFF}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_10 alias: trisc_wrapper_debug_bus note: TRISC1
//  name: pc_buffer_debug_bus
//  name: trisc_read_request_pending
#define DEBUG_BUS_DEBUG_TENSIX_IN_10_PC_BUFFER_DEBUG_BUS_TRISC_READ_REQUEST_PENDING_WORD {21, 7, 0, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_10_PC_BUFFER_DEBUG_BUS_TRISC_READ_REQUEST_PENDING {DEBUG_BUS_DEBUG_TENSIX_IN_10_PC_BUFFER_DEBUG_BUS_TRISC_READ_REQUEST_PENDING_WORD, 31, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_10 alias: trisc_wrapper_debug_bus note: TRISC1
//  name: pc_buffer_debug_bus
//  name: trisc_sync_activated
#define DEBUG_BUS_DEBUG_TENSIX_IN_10_PC_BUFFER_DEBUG_BUS_TRISC_SYNC_ACTIVATED_WORD {21, 7, 0, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_10_PC_BUFFER_DEBUG_BUS_TRISC_SYNC_ACTIVATED {DEBUG_BUS_DEBUG_TENSIX_IN_10_PC_BUFFER_DEBUG_BUS_TRISC_SYNC_ACTIVATED_WORD, 30, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_10 alias: trisc_wrapper_debug_bus note: TRISC1
//  name: pc_buffer_debug_bus
//  name: trisc_sync_type
#define DEBUG_BUS_DEBUG_TENSIX_IN_10_PC_BUFFER_DEBUG_BUS_TRISC_SYNC_TYPE_WORD {21, 7, 0, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_10_PC_BUFFER_DEBUG_BUS_TRISC_SYNC_TYPE {DEBUG_BUS_DEBUG_TENSIX_IN_10_PC_BUFFER_DEBUG_BUS_TRISC_SYNC_TYPE_WORD, 29, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_10 alias: trisc_wrapper_debug_bus note: TRISC1
//  name: pc_buffer_debug_bus
//  name: riscv_sync_activated
#define DEBUG_BUS_DEBUG_TENSIX_IN_10_PC_BUFFER_DEBUG_BUS_RISCV_SYNC_ACTIVATED_WORD {21, 7, 0, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_10_PC_BUFFER_DEBUG_BUS_RISCV_SYNC_ACTIVATED {DEBUG_BUS_DEBUG_TENSIX_IN_10_PC_BUFFER_DEBUG_BUS_RISCV_SYNC_ACTIVATED_WORD, 28, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_10 alias: trisc_wrapper_debug_bus note: TRISC1
//  name: pc_buffer_debug_bus
//  name: pc_buffer_idle
#define DEBUG_BUS_DEBUG_TENSIX_IN_10_PC_BUFFER_DEBUG_BUS_PC_BUFFER_IDLE_WORD {21, 7, 0, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_10_PC_BUFFER_DEBUG_BUS_PC_BUFFER_IDLE {DEBUG_BUS_DEBUG_TENSIX_IN_10_PC_BUFFER_DEBUG_BUS_PC_BUFFER_IDLE_WORD, 27, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_10 alias: trisc_wrapper_debug_bus note: TRISC1
//  name: pc_buffer_debug_bus
//  name: i_busy
#define DEBUG_BUS_DEBUG_TENSIX_IN_10_PC_BUFFER_DEBUG_BUS_I_BUSY_WORD {21, 7, 0, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_10_PC_BUFFER_DEBUG_BUS_I_BUSY {DEBUG_BUS_DEBUG_TENSIX_IN_10_PC_BUFFER_DEBUG_BUS_I_BUSY_WORD, 26, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_10 alias: trisc_wrapper_debug_bus note: TRISC1
//  name: pc_buffer_debug_bus
//  name: i_mops_outstanding
#define DEBUG_BUS_DEBUG_TENSIX_IN_10_PC_BUFFER_DEBUG_BUS_I_MOPS_OUTSTANDING_WORD {21, 7, 0, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_10_PC_BUFFER_DEBUG_BUS_I_MOPS_OUTSTANDING {DEBUG_BUS_DEBUG_TENSIX_IN_10_PC_BUFFER_DEBUG_BUS_I_MOPS_OUTSTANDING_WORD, 25, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_10 alias: trisc_wrapper_debug_bus note: TRISC1
//  name: pc_buffer_debug_bus
//  name: cmd_fifo_full
#define DEBUG_BUS_DEBUG_TENSIX_IN_10_PC_BUFFER_DEBUG_BUS_CMD_FIFO_FULL_WORD {21, 7, 0, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_10_PC_BUFFER_DEBUG_BUS_CMD_FIFO_FULL {DEBUG_BUS_DEBUG_TENSIX_IN_10_PC_BUFFER_DEBUG_BUS_CMD_FIFO_FULL_WORD, 24, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_10 alias: trisc_wrapper_debug_bus note: TRISC1
//  name: pc_buffer_debug_bus
//  name: cmd_fifo_empty
#define DEBUG_BUS_DEBUG_TENSIX_IN_10_PC_BUFFER_DEBUG_BUS_CMD_FIFO_EMPTY_WORD {21, 7, 0, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_10_PC_BUFFER_DEBUG_BUS_CMD_FIFO_EMPTY {DEBUG_BUS_DEBUG_TENSIX_IN_10_PC_BUFFER_DEBUG_BUS_CMD_FIFO_EMPTY_WORD, 23, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_10 alias: trisc_wrapper_debug_bus note: TRISC1
//  name: pc_buffer_debug_bus
//  name: next_cmd_fifo_data
#define DEBUG_BUS_DEBUG_TENSIX_IN_10_PC_BUFFER_DEBUG_BUS_NEXT_CMD_FIFO_DATA__8_0_WORD {20, 7, 6, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_10_PC_BUFFER_DEBUG_BUS_NEXT_CMD_FIFO_DATA__8_0 {DEBUG_BUS_DEBUG_TENSIX_IN_10_PC_BUFFER_DEBUG_BUS_NEXT_CMD_FIFO_DATA__8_0_WORD, 23, 0x1FF}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_10 alias: trisc_wrapper_debug_bus note: TRISC1
//  name: pc_buffer_debug_bus
//  name: next_cmd_fifo_data
#define DEBUG_BUS_DEBUG_TENSIX_IN_10_PC_BUFFER_DEBUG_BUS_NEXT_CMD_FIFO_DATA__31_9_WORD {21, 7, 0, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_10_PC_BUFFER_DEBUG_BUS_NEXT_CMD_FIFO_DATA__31_9 {DEBUG_BUS_DEBUG_TENSIX_IN_10_PC_BUFFER_DEBUG_BUS_NEXT_CMD_FIFO_DATA__31_9_WORD, 0, 0x7FFFFF}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_10 alias: trisc_wrapper_debug_bus note: TRISC1
//  name: risc_wrapper_debug_bus_trisc
//  name: o_par_err_risc_localmem
#define DEBUG_BUS_DEBUG_TENSIX_IN_10_RISC_WRAPPER_DEBUG_BUS_TRISC_O_PAR_ERR_RISC_LOCALMEM_WORD {20, 7, 6, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_10_RISC_WRAPPER_DEBUG_BUS_TRISC_O_PAR_ERR_RISC_LOCALMEM {DEBUG_BUS_DEBUG_TENSIX_IN_10_RISC_WRAPPER_DEBUG_BUS_TRISC_O_PAR_ERR_RISC_LOCALMEM_WORD, 22, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_10 alias: trisc_wrapper_debug_bus note: TRISC1
//  name: risc_wrapper_debug_bus_trisc
//  name: i_mailbox_rden
#define DEBUG_BUS_DEBUG_TENSIX_IN_10_RISC_WRAPPER_DEBUG_BUS_TRISC_I_MAILBOX_RDEN_WORD {20, 7, 6, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_10_RISC_WRAPPER_DEBUG_BUS_TRISC_I_MAILBOX_RDEN {DEBUG_BUS_DEBUG_TENSIX_IN_10_RISC_WRAPPER_DEBUG_BUS_TRISC_I_MAILBOX_RDEN_WORD, 18, 0xF}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_10 alias: trisc_wrapper_debug_bus note: TRISC1
//  name: risc_wrapper_debug_bus_trisc
//  name: i_mailbox_rd_type
#define DEBUG_BUS_DEBUG_TENSIX_IN_10_RISC_WRAPPER_DEBUG_BUS_TRISC_I_MAILBOX_RD_TYPE_WORD {20, 7, 6, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_10_RISC_WRAPPER_DEBUG_BUS_TRISC_I_MAILBOX_RD_TYPE {DEBUG_BUS_DEBUG_TENSIX_IN_10_RISC_WRAPPER_DEBUG_BUS_TRISC_I_MAILBOX_RD_TYPE_WORD, 14, 0xF}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_10 alias: trisc_wrapper_debug_bus note: TRISC1
//  name: risc_wrapper_debug_bus_trisc
//  name: o_mailbox_rd_req_ready
#define DEBUG_BUS_DEBUG_TENSIX_IN_10_RISC_WRAPPER_DEBUG_BUS_TRISC_O_MAILBOX_RD_REQ_READY_WORD {20, 7, 6, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_10_RISC_WRAPPER_DEBUG_BUS_TRISC_O_MAILBOX_RD_REQ_READY {DEBUG_BUS_DEBUG_TENSIX_IN_10_RISC_WRAPPER_DEBUG_BUS_TRISC_O_MAILBOX_RD_REQ_READY_WORD, 10, 0xF}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_10 alias: trisc_wrapper_debug_bus note: TRISC1
//  name: risc_wrapper_debug_bus_trisc
//  name: o_mailbox_rdvalid
#define DEBUG_BUS_DEBUG_TENSIX_IN_10_RISC_WRAPPER_DEBUG_BUS_TRISC_O_MAILBOX_RDVALID_WORD {20, 7, 6, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_10_RISC_WRAPPER_DEBUG_BUS_TRISC_O_MAILBOX_RDVALID {DEBUG_BUS_DEBUG_TENSIX_IN_10_RISC_WRAPPER_DEBUG_BUS_TRISC_O_MAILBOX_RDVALID_WORD, 6, 0xF}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_10 alias: trisc_wrapper_debug_bus note: TRISC1
//  name: risc_wrapper_debug_bus_trisc
//  name: o_mailbox_rddata
#define DEBUG_BUS_DEBUG_TENSIX_IN_10_RISC_WRAPPER_DEBUG_BUS_TRISC_O_MAILBOX_RDDATA__15_0_WORD {20, 7, 4, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_10_RISC_WRAPPER_DEBUG_BUS_TRISC_O_MAILBOX_RDDATA__15_0 {DEBUG_BUS_DEBUG_TENSIX_IN_10_RISC_WRAPPER_DEBUG_BUS_TRISC_O_MAILBOX_RDDATA__15_0_WORD, 16, 0xFFFF}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_10 alias: trisc_wrapper_debug_bus note: TRISC1
//  name: risc_wrapper_debug_bus_trisc
//  name: o_mailbox_rddata
#define DEBUG_BUS_DEBUG_TENSIX_IN_10_RISC_WRAPPER_DEBUG_BUS_TRISC_O_MAILBOX_RDDATA__21_16_WORD {20, 7, 6, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_10_RISC_WRAPPER_DEBUG_BUS_TRISC_O_MAILBOX_RDDATA__21_16 {DEBUG_BUS_DEBUG_TENSIX_IN_10_RISC_WRAPPER_DEBUG_BUS_TRISC_O_MAILBOX_RDDATA__21_16_WORD, 0, 0x3F}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_10 alias: trisc_wrapper_debug_bus note: TRISC1
//  name: risc_wrapper_debug_bus_trisc
//  name: intf_wrack_trisc
#define DEBUG_BUS_DEBUG_TENSIX_IN_10_RISC_WRAPPER_DEBUG_BUS_TRISC_INTF_WRACK_TRISC__12_0_WORD {20, 7, 0, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_10_RISC_WRAPPER_DEBUG_BUS_TRISC_INTF_WRACK_TRISC__12_0 {DEBUG_BUS_DEBUG_TENSIX_IN_10_RISC_WRAPPER_DEBUG_BUS_TRISC_INTF_WRACK_TRISC__12_0_WORD, 17, 0x1FFF}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_10 alias: trisc_wrapper_debug_bus note: TRISC1
//  name: risc_wrapper_debug_bus_trisc
//  name: dmem_tensix_rden
#define DEBUG_BUS_DEBUG_TENSIX_IN_10_RISC_WRAPPER_DEBUG_BUS_TRISC_DMEM_TENSIX_RDEN_WORD {20, 7, 0, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_10_RISC_WRAPPER_DEBUG_BUS_TRISC_DMEM_TENSIX_RDEN {DEBUG_BUS_DEBUG_TENSIX_IN_10_RISC_WRAPPER_DEBUG_BUS_TRISC_DMEM_TENSIX_RDEN_WORD, 16, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_10 alias: trisc_wrapper_debug_bus note: TRISC1
//  name: risc_wrapper_debug_bus_trisc
//  name: dmem_tensix_wren
#define DEBUG_BUS_DEBUG_TENSIX_IN_10_RISC_WRAPPER_DEBUG_BUS_TRISC_DMEM_TENSIX_WREN_WORD {20, 7, 0, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_10_RISC_WRAPPER_DEBUG_BUS_TRISC_DMEM_TENSIX_WREN {DEBUG_BUS_DEBUG_TENSIX_IN_10_RISC_WRAPPER_DEBUG_BUS_TRISC_DMEM_TENSIX_WREN_WORD, 15, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_10 alias: trisc_wrapper_debug_bus note: TRISC1
//  name: risc_wrapper_debug_bus_trisc
//  name: icache_req_fifo_full
#define DEBUG_BUS_DEBUG_TENSIX_IN_10_RISC_WRAPPER_DEBUG_BUS_TRISC_ICACHE_REQ_FIFO_FULL_WORD {20, 7, 0, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_10_RISC_WRAPPER_DEBUG_BUS_TRISC_ICACHE_REQ_FIFO_FULL {DEBUG_BUS_DEBUG_TENSIX_IN_10_RISC_WRAPPER_DEBUG_BUS_TRISC_ICACHE_REQ_FIFO_FULL_WORD, 1, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_10 alias: trisc_wrapper_debug_bus note: TRISC1
//  name: risc_wrapper_debug_bus_trisc
//  name: icache_req_fifo_empty
#define DEBUG_BUS_DEBUG_TENSIX_IN_10_RISC_WRAPPER_DEBUG_BUS_TRISC_ICACHE_REQ_FIFO_EMPTY_WORD {20, 7, 0, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_10_RISC_WRAPPER_DEBUG_BUS_TRISC_ICACHE_REQ_FIFO_EMPTY {DEBUG_BUS_DEBUG_TENSIX_IN_10_RISC_WRAPPER_DEBUG_BUS_TRISC_ICACHE_REQ_FIFO_EMPTY_WORD, 0, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_9 alias: trisc_wrapper_debug_bus note: TRISC0
//  name: trisc_mop_buf_empty
#define DEBUG_BUS_DEBUG_TENSIX_IN_9_TRISC_MOP_BUF_EMPTY_WORD {19, 7, 6, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_9_TRISC_MOP_BUF_EMPTY {DEBUG_BUS_DEBUG_TENSIX_IN_9_TRISC_MOP_BUF_EMPTY_WORD, 30, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_9 alias: trisc_wrapper_debug_bus note: TRISC0
//  name: trisc_mop_buf_full
#define DEBUG_BUS_DEBUG_TENSIX_IN_9_TRISC_MOP_BUF_FULL_WORD {19, 7, 6, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_9_TRISC_MOP_BUF_FULL {DEBUG_BUS_DEBUG_TENSIX_IN_9_TRISC_MOP_BUF_FULL_WORD, 29, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_9 alias: trisc_wrapper_debug_bus note: TRISC0
//  name: mop_decode_debug_bus
//  name: debug_math_loop_state
#define DEBUG_BUS_DEBUG_TENSIX_IN_9_MOP_DECODE_DEBUG_BUS_DEBUG_MATH_LOOP_STATE_WORD {19, 7, 6, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_9_MOP_DECODE_DEBUG_BUS_DEBUG_MATH_LOOP_STATE {DEBUG_BUS_DEBUG_TENSIX_IN_9_MOP_DECODE_DEBUG_BUS_DEBUG_MATH_LOOP_STATE_WORD, 26, 0x7}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_9 alias: trisc_wrapper_debug_bus note: TRISC0
//  name: mop_decode_debug_bus
//  name: debug_unpack_loop_state
#define DEBUG_BUS_DEBUG_TENSIX_IN_9_MOP_DECODE_DEBUG_BUS_DEBUG_UNPACK_LOOP_STATE_WORD {19, 7, 6, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_9_MOP_DECODE_DEBUG_BUS_DEBUG_UNPACK_LOOP_STATE {DEBUG_BUS_DEBUG_TENSIX_IN_9_MOP_DECODE_DEBUG_BUS_DEBUG_UNPACK_LOOP_STATE_WORD, 23, 0x7}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_9 alias: trisc_wrapper_debug_bus note: TRISC0
//  name: mop_decode_debug_bus
//  name: mop_stage_valid
#define DEBUG_BUS_DEBUG_TENSIX_IN_9_MOP_DECODE_DEBUG_BUS_MOP_STAGE_VALID_WORD {19, 7, 6, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_9_MOP_DECODE_DEBUG_BUS_MOP_STAGE_VALID {DEBUG_BUS_DEBUG_TENSIX_IN_9_MOP_DECODE_DEBUG_BUS_MOP_STAGE_VALID_WORD, 22, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_9 alias: trisc_wrapper_debug_bus note: TRISC0
//  name: mop_decode_debug_bus
//  name: mop_stage_opcode
#define DEBUG_BUS_DEBUG_TENSIX_IN_9_MOP_DECODE_DEBUG_BUS_MOP_STAGE_OPCODE__9_0_WORD {19, 7, 4, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_9_MOP_DECODE_DEBUG_BUS_MOP_STAGE_OPCODE__9_0 {DEBUG_BUS_DEBUG_TENSIX_IN_9_MOP_DECODE_DEBUG_BUS_MOP_STAGE_OPCODE__9_0_WORD, 22, 0x3FF}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_9 alias: trisc_wrapper_debug_bus note: TRISC0
//  name: mop_decode_debug_bus
//  name: mop_stage_opcode
#define DEBUG_BUS_DEBUG_TENSIX_IN_9_MOP_DECODE_DEBUG_BUS_MOP_STAGE_OPCODE__31_10_WORD {19, 7, 6, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_9_MOP_DECODE_DEBUG_BUS_MOP_STAGE_OPCODE__31_10 {DEBUG_BUS_DEBUG_TENSIX_IN_9_MOP_DECODE_DEBUG_BUS_MOP_STAGE_OPCODE__31_10_WORD, 0, 0x3FFFFF}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_9 alias: trisc_wrapper_debug_bus note: TRISC0
//  name: mop_decode_debug_bus
//  name: math_loop_active
#define DEBUG_BUS_DEBUG_TENSIX_IN_9_MOP_DECODE_DEBUG_BUS_MATH_LOOP_ACTIVE_WORD {19, 7, 4, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_9_MOP_DECODE_DEBUG_BUS_MATH_LOOP_ACTIVE {DEBUG_BUS_DEBUG_TENSIX_IN_9_MOP_DECODE_DEBUG_BUS_MATH_LOOP_ACTIVE_WORD, 21, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_9 alias: trisc_wrapper_debug_bus note: TRISC0
//  name: mop_decode_debug_bus
//  name: unpack_loop_active
#define DEBUG_BUS_DEBUG_TENSIX_IN_9_MOP_DECODE_DEBUG_BUS_UNPACK_LOOP_ACTIVE_WORD {19, 7, 4, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_9_MOP_DECODE_DEBUG_BUS_UNPACK_LOOP_ACTIVE {DEBUG_BUS_DEBUG_TENSIX_IN_9_MOP_DECODE_DEBUG_BUS_UNPACK_LOOP_ACTIVE_WORD, 20, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_9 alias: trisc_wrapper_debug_bus note: TRISC0
//  name: mop_decode_debug_bus
//  name: o_instrn_valid
#define DEBUG_BUS_DEBUG_TENSIX_IN_9_MOP_DECODE_DEBUG_BUS_O_INSTRN_VALID_WORD {19, 7, 4, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_9_MOP_DECODE_DEBUG_BUS_O_INSTRN_VALID {DEBUG_BUS_DEBUG_TENSIX_IN_9_MOP_DECODE_DEBUG_BUS_O_INSTRN_VALID_WORD, 19, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_9 alias: trisc_wrapper_debug_bus note: TRISC0
//  name: mop_decode_debug_bus
//  name: o_instrn_opcode
#define DEBUG_BUS_DEBUG_TENSIX_IN_9_MOP_DECODE_DEBUG_BUS_O_INSTRN_OPCODE__12_0_WORD {19, 7, 2, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_9_MOP_DECODE_DEBUG_BUS_O_INSTRN_OPCODE__12_0 {DEBUG_BUS_DEBUG_TENSIX_IN_9_MOP_DECODE_DEBUG_BUS_O_INSTRN_OPCODE__12_0_WORD, 19, 0x1FFF}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_9 alias: trisc_wrapper_debug_bus note: TRISC0
//  name: mop_decode_debug_bus
//  name: o_instrn_opcode
#define DEBUG_BUS_DEBUG_TENSIX_IN_9_MOP_DECODE_DEBUG_BUS_O_INSTRN_OPCODE__31_13_WORD {19, 7, 4, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_9_MOP_DECODE_DEBUG_BUS_O_INSTRN_OPCODE__31_13 {DEBUG_BUS_DEBUG_TENSIX_IN_9_MOP_DECODE_DEBUG_BUS_O_INSTRN_OPCODE__31_13_WORD, 0, 0x7FFFF}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_9 alias: trisc_wrapper_debug_bus note: TRISC0
//  name: pc_buffer_debug_bus
//  name: sempost_pending
#define DEBUG_BUS_DEBUG_TENSIX_IN_9_PC_BUFFER_DEBUG_BUS_SEMPOST_PENDING_WORD {19, 7, 2, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_9_PC_BUFFER_DEBUG_BUS_SEMPOST_PENDING {DEBUG_BUS_DEBUG_TENSIX_IN_9_PC_BUFFER_DEBUG_BUS_SEMPOST_PENDING_WORD, 8, 0xFF}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_9 alias: trisc_wrapper_debug_bus note: TRISC0
//  name: pc_buffer_debug_bus
//  name: semget_pending
#define DEBUG_BUS_DEBUG_TENSIX_IN_9_PC_BUFFER_DEBUG_BUS_SEMGET_PENDING_WORD {19, 7, 2, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_9_PC_BUFFER_DEBUG_BUS_SEMGET_PENDING {DEBUG_BUS_DEBUG_TENSIX_IN_9_PC_BUFFER_DEBUG_BUS_SEMGET_PENDING_WORD, 0, 0xFF}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_9 alias: trisc_wrapper_debug_bus note: TRISC0
//  name: pc_buffer_debug_bus
//  name: trisc_read_request_pending
#define DEBUG_BUS_DEBUG_TENSIX_IN_9_PC_BUFFER_DEBUG_BUS_TRISC_READ_REQUEST_PENDING_WORD {19, 7, 0, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_9_PC_BUFFER_DEBUG_BUS_TRISC_READ_REQUEST_PENDING {DEBUG_BUS_DEBUG_TENSIX_IN_9_PC_BUFFER_DEBUG_BUS_TRISC_READ_REQUEST_PENDING_WORD, 31, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_9 alias: trisc_wrapper_debug_bus note: TRISC0
//  name: pc_buffer_debug_bus
//  name: trisc_sync_activated
#define DEBUG_BUS_DEBUG_TENSIX_IN_9_PC_BUFFER_DEBUG_BUS_TRISC_SYNC_ACTIVATED_WORD {19, 7, 0, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_9_PC_BUFFER_DEBUG_BUS_TRISC_SYNC_ACTIVATED {DEBUG_BUS_DEBUG_TENSIX_IN_9_PC_BUFFER_DEBUG_BUS_TRISC_SYNC_ACTIVATED_WORD, 30, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_9 alias: trisc_wrapper_debug_bus note: TRISC0
//  name: pc_buffer_debug_bus
//  name: trisc_sync_type
#define DEBUG_BUS_DEBUG_TENSIX_IN_9_PC_BUFFER_DEBUG_BUS_TRISC_SYNC_TYPE_WORD {19, 7, 0, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_9_PC_BUFFER_DEBUG_BUS_TRISC_SYNC_TYPE {DEBUG_BUS_DEBUG_TENSIX_IN_9_PC_BUFFER_DEBUG_BUS_TRISC_SYNC_TYPE_WORD, 29, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_9 alias: trisc_wrapper_debug_bus note: TRISC0
//  name: pc_buffer_debug_bus
//  name: riscv_sync_activated
#define DEBUG_BUS_DEBUG_TENSIX_IN_9_PC_BUFFER_DEBUG_BUS_RISCV_SYNC_ACTIVATED_WORD {19, 7, 0, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_9_PC_BUFFER_DEBUG_BUS_RISCV_SYNC_ACTIVATED {DEBUG_BUS_DEBUG_TENSIX_IN_9_PC_BUFFER_DEBUG_BUS_RISCV_SYNC_ACTIVATED_WORD, 28, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_9 alias: trisc_wrapper_debug_bus note: TRISC0
//  name: pc_buffer_debug_bus
//  name: pc_buffer_idle
#define DEBUG_BUS_DEBUG_TENSIX_IN_9_PC_BUFFER_DEBUG_BUS_PC_BUFFER_IDLE_WORD {19, 7, 0, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_9_PC_BUFFER_DEBUG_BUS_PC_BUFFER_IDLE {DEBUG_BUS_DEBUG_TENSIX_IN_9_PC_BUFFER_DEBUG_BUS_PC_BUFFER_IDLE_WORD, 27, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_9 alias: trisc_wrapper_debug_bus note: TRISC0
//  name: pc_buffer_debug_bus
//  name: i_busy
#define DEBUG_BUS_DEBUG_TENSIX_IN_9_PC_BUFFER_DEBUG_BUS_I_BUSY_WORD {19, 7, 0, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_9_PC_BUFFER_DEBUG_BUS_I_BUSY {DEBUG_BUS_DEBUG_TENSIX_IN_9_PC_BUFFER_DEBUG_BUS_I_BUSY_WORD, 26, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_9 alias: trisc_wrapper_debug_bus note: TRISC0
//  name: pc_buffer_debug_bus
//  name: i_mops_outstanding
#define DEBUG_BUS_DEBUG_TENSIX_IN_9_PC_BUFFER_DEBUG_BUS_I_MOPS_OUTSTANDING_WORD {19, 7, 0, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_9_PC_BUFFER_DEBUG_BUS_I_MOPS_OUTSTANDING {DEBUG_BUS_DEBUG_TENSIX_IN_9_PC_BUFFER_DEBUG_BUS_I_MOPS_OUTSTANDING_WORD, 25, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_9 alias: trisc_wrapper_debug_bus note: TRISC0
//  name: pc_buffer_debug_bus
//  name: cmd_fifo_full
#define DEBUG_BUS_DEBUG_TENSIX_IN_9_PC_BUFFER_DEBUG_BUS_CMD_FIFO_FULL_WORD {19, 7, 0, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_9_PC_BUFFER_DEBUG_BUS_CMD_FIFO_FULL {DEBUG_BUS_DEBUG_TENSIX_IN_9_PC_BUFFER_DEBUG_BUS_CMD_FIFO_FULL_WORD, 24, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_9 alias: trisc_wrapper_debug_bus note: TRISC0
//  name: pc_buffer_debug_bus
//  name: cmd_fifo_empty
#define DEBUG_BUS_DEBUG_TENSIX_IN_9_PC_BUFFER_DEBUG_BUS_CMD_FIFO_EMPTY_WORD {19, 7, 0, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_9_PC_BUFFER_DEBUG_BUS_CMD_FIFO_EMPTY {DEBUG_BUS_DEBUG_TENSIX_IN_9_PC_BUFFER_DEBUG_BUS_CMD_FIFO_EMPTY_WORD, 23, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_9 alias: trisc_wrapper_debug_bus note: TRISC0
//  name: pc_buffer_debug_bus
//  name: next_cmd_fifo_data
#define DEBUG_BUS_DEBUG_TENSIX_IN_9_PC_BUFFER_DEBUG_BUS_NEXT_CMD_FIFO_DATA__8_0_WORD {18, 7, 6, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_9_PC_BUFFER_DEBUG_BUS_NEXT_CMD_FIFO_DATA__8_0 {DEBUG_BUS_DEBUG_TENSIX_IN_9_PC_BUFFER_DEBUG_BUS_NEXT_CMD_FIFO_DATA__8_0_WORD, 23, 0x1FF}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_9 alias: trisc_wrapper_debug_bus note: TRISC0
//  name: pc_buffer_debug_bus
//  name: next_cmd_fifo_data
#define DEBUG_BUS_DEBUG_TENSIX_IN_9_PC_BUFFER_DEBUG_BUS_NEXT_CMD_FIFO_DATA__31_9_WORD {19, 7, 0, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_9_PC_BUFFER_DEBUG_BUS_NEXT_CMD_FIFO_DATA__31_9 {DEBUG_BUS_DEBUG_TENSIX_IN_9_PC_BUFFER_DEBUG_BUS_NEXT_CMD_FIFO_DATA__31_9_WORD, 0, 0x7FFFFF}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_9 alias: trisc_wrapper_debug_bus note: TRISC0
//  name: risc_wrapper_debug_bus_trisc
//  name: o_par_err_risc_localmem
#define DEBUG_BUS_DEBUG_TENSIX_IN_9_RISC_WRAPPER_DEBUG_BUS_TRISC_O_PAR_ERR_RISC_LOCALMEM_WORD {18, 7, 6, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_9_RISC_WRAPPER_DEBUG_BUS_TRISC_O_PAR_ERR_RISC_LOCALMEM {DEBUG_BUS_DEBUG_TENSIX_IN_9_RISC_WRAPPER_DEBUG_BUS_TRISC_O_PAR_ERR_RISC_LOCALMEM_WORD, 22, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_9 alias: trisc_wrapper_debug_bus note: TRISC0
//  name: risc_wrapper_debug_bus_trisc
//  name: i_mailbox_rden
#define DEBUG_BUS_DEBUG_TENSIX_IN_9_RISC_WRAPPER_DEBUG_BUS_TRISC_I_MAILBOX_RDEN_WORD {18, 7, 6, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_9_RISC_WRAPPER_DEBUG_BUS_TRISC_I_MAILBOX_RDEN {DEBUG_BUS_DEBUG_TENSIX_IN_9_RISC_WRAPPER_DEBUG_BUS_TRISC_I_MAILBOX_RDEN_WORD, 18, 0xF}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_9 alias: trisc_wrapper_debug_bus note: TRISC0
//  name: risc_wrapper_debug_bus_trisc
//  name: i_mailbox_rd_type
#define DEBUG_BUS_DEBUG_TENSIX_IN_9_RISC_WRAPPER_DEBUG_BUS_TRISC_I_MAILBOX_RD_TYPE_WORD {18, 7, 6, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_9_RISC_WRAPPER_DEBUG_BUS_TRISC_I_MAILBOX_RD_TYPE {DEBUG_BUS_DEBUG_TENSIX_IN_9_RISC_WRAPPER_DEBUG_BUS_TRISC_I_MAILBOX_RD_TYPE_WORD, 14, 0xF}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_9 alias: trisc_wrapper_debug_bus note: TRISC0
//  name: risc_wrapper_debug_bus_trisc
//  name: o_mailbox_rd_req_ready
#define DEBUG_BUS_DEBUG_TENSIX_IN_9_RISC_WRAPPER_DEBUG_BUS_TRISC_O_MAILBOX_RD_REQ_READY_WORD {18, 7, 6, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_9_RISC_WRAPPER_DEBUG_BUS_TRISC_O_MAILBOX_RD_REQ_READY {DEBUG_BUS_DEBUG_TENSIX_IN_9_RISC_WRAPPER_DEBUG_BUS_TRISC_O_MAILBOX_RD_REQ_READY_WORD, 10, 0xF}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_9 alias: trisc_wrapper_debug_bus note: TRISC0
//  name: risc_wrapper_debug_bus_trisc
//  name: o_mailbox_rdvalid
#define DEBUG_BUS_DEBUG_TENSIX_IN_9_RISC_WRAPPER_DEBUG_BUS_TRISC_O_MAILBOX_RDVALID_WORD {18, 7, 6, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_9_RISC_WRAPPER_DEBUG_BUS_TRISC_O_MAILBOX_RDVALID {DEBUG_BUS_DEBUG_TENSIX_IN_9_RISC_WRAPPER_DEBUG_BUS_TRISC_O_MAILBOX_RDVALID_WORD, 6, 0xF}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_9 alias: trisc_wrapper_debug_bus note: TRISC0
//  name: risc_wrapper_debug_bus_trisc
//  name: o_mailbox_rddata
#define DEBUG_BUS_DEBUG_TENSIX_IN_9_RISC_WRAPPER_DEBUG_BUS_TRISC_O_MAILBOX_RDDATA__15_0_WORD {18, 7, 4, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_9_RISC_WRAPPER_DEBUG_BUS_TRISC_O_MAILBOX_RDDATA__15_0 {DEBUG_BUS_DEBUG_TENSIX_IN_9_RISC_WRAPPER_DEBUG_BUS_TRISC_O_MAILBOX_RDDATA__15_0_WORD, 16, 0xFFFF}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_9 alias: trisc_wrapper_debug_bus note: TRISC0
//  name: risc_wrapper_debug_bus_trisc
//  name: o_mailbox_rddata
#define DEBUG_BUS_DEBUG_TENSIX_IN_9_RISC_WRAPPER_DEBUG_BUS_TRISC_O_MAILBOX_RDDATA__21_16_WORD {18, 7, 6, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_9_RISC_WRAPPER_DEBUG_BUS_TRISC_O_MAILBOX_RDDATA__21_16 {DEBUG_BUS_DEBUG_TENSIX_IN_9_RISC_WRAPPER_DEBUG_BUS_TRISC_O_MAILBOX_RDDATA__21_16_WORD, 0, 0x3F}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_9 alias: trisc_wrapper_debug_bus note: TRISC0
//  name: risc_wrapper_debug_bus_trisc
//  name: intf_wrack_trisc
#define DEBUG_BUS_DEBUG_TENSIX_IN_9_RISC_WRAPPER_DEBUG_BUS_TRISC_INTF_WRACK_TRISC__12_0_WORD {18, 7, 0, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_9_RISC_WRAPPER_DEBUG_BUS_TRISC_INTF_WRACK_TRISC__12_0 {DEBUG_BUS_DEBUG_TENSIX_IN_9_RISC_WRAPPER_DEBUG_BUS_TRISC_INTF_WRACK_TRISC__12_0_WORD, 17, 0x1FFF}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_9 alias: trisc_wrapper_debug_bus note: TRISC0
//  name: risc_wrapper_debug_bus_trisc
//  name: dmem_tensix_rden
#define DEBUG_BUS_DEBUG_TENSIX_IN_9_RISC_WRAPPER_DEBUG_BUS_TRISC_DMEM_TENSIX_RDEN_WORD {18, 7, 0, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_9_RISC_WRAPPER_DEBUG_BUS_TRISC_DMEM_TENSIX_RDEN {DEBUG_BUS_DEBUG_TENSIX_IN_9_RISC_WRAPPER_DEBUG_BUS_TRISC_DMEM_TENSIX_RDEN_WORD, 16, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_9 alias: trisc_wrapper_debug_bus note: TRISC0
//  name: risc_wrapper_debug_bus_trisc
//  name: dmem_tensix_wren
#define DEBUG_BUS_DEBUG_TENSIX_IN_9_RISC_WRAPPER_DEBUG_BUS_TRISC_DMEM_TENSIX_WREN_WORD {18, 7, 0, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_9_RISC_WRAPPER_DEBUG_BUS_TRISC_DMEM_TENSIX_WREN {DEBUG_BUS_DEBUG_TENSIX_IN_9_RISC_WRAPPER_DEBUG_BUS_TRISC_DMEM_TENSIX_WREN_WORD, 15, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_9 alias: trisc_wrapper_debug_bus note: TRISC0
//  name: risc_wrapper_debug_bus_trisc
//  name: icache_req_fifo_full
#define DEBUG_BUS_DEBUG_TENSIX_IN_9_RISC_WRAPPER_DEBUG_BUS_TRISC_ICACHE_REQ_FIFO_FULL_WORD {18, 7, 0, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_9_RISC_WRAPPER_DEBUG_BUS_TRISC_ICACHE_REQ_FIFO_FULL {DEBUG_BUS_DEBUG_TENSIX_IN_9_RISC_WRAPPER_DEBUG_BUS_TRISC_ICACHE_REQ_FIFO_FULL_WORD, 1, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_9 alias: trisc_wrapper_debug_bus note: TRISC0
//  name: risc_wrapper_debug_bus_trisc
//  name: icache_req_fifo_empty
#define DEBUG_BUS_DEBUG_TENSIX_IN_9_RISC_WRAPPER_DEBUG_BUS_TRISC_ICACHE_REQ_FIFO_EMPTY_WORD {18, 7, 0, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_9_RISC_WRAPPER_DEBUG_BUS_TRISC_ICACHE_REQ_FIFO_EMPTY {DEBUG_BUS_DEBUG_TENSIX_IN_9_RISC_WRAPPER_DEBUG_BUS_TRISC_ICACHE_REQ_FIFO_EMPTY_WORD, 0, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_8 alias: debug_bus_trisc note: TRISC2
//  name: ex_id_rtr
#define DEBUG_BUS_DEBUG_DAISY_STOP_TENSIX_DEBUG_TENSIX_IN_8_EX_ID_RTR__2281_WORD {17, 7, 6, 0, 1, 0}
#define DEBUG_BUS_DEBUG_DAISY_STOP_TENSIX_DEBUG_TENSIX_IN_8_EX_ID_RTR__2281 {DEBUG_BUS_DEBUG_DAISY_STOP_TENSIX_DEBUG_TENSIX_IN_8_EX_ID_RTR__2281_WORD, 9, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_8 alias: debug_bus_trisc note: TRISC2
//  name: id_ex_rts
#define DEBUG_BUS_DEBUG_DAISY_STOP_TENSIX_DEBUG_TENSIX_IN_8_ID_EX_RTS__2280_WORD {17, 7, 6, 0, 1, 0}
#define DEBUG_BUS_DEBUG_DAISY_STOP_TENSIX_DEBUG_TENSIX_IN_8_ID_EX_RTS__2280 {DEBUG_BUS_DEBUG_DAISY_STOP_TENSIX_DEBUG_TENSIX_IN_8_ID_EX_RTS__2280_WORD, 8, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_8 alias: debug_bus_trisc note: TRISC2
//  name: if_rts
#define DEBUG_BUS_DEBUG_TENSIX_IN_8_IF_RTS_WORD {17, 7, 6, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_8_IF_RTS {DEBUG_BUS_DEBUG_TENSIX_IN_8_IF_RTS_WORD, 7, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_8 alias: debug_bus_trisc note: TRISC2
//  name: if_ex_predicted
#define DEBUG_BUS_DEBUG_TENSIX_IN_8_IF_EX_PREDICTED_WORD {17, 7, 6, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_8_IF_EX_PREDICTED {DEBUG_BUS_DEBUG_TENSIX_IN_8_IF_EX_PREDICTED_WORD, 5, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_8 alias: debug_bus_trisc note: TRISC2
//  name: if_ex_deco
#define DEBUG_BUS_DEBUG_TENSIX_IN_8_IF_EX_DECO__36_32_WORD {17, 7, 6, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_8_IF_EX_DECO__36_32 {DEBUG_BUS_DEBUG_TENSIX_IN_8_IF_EX_DECO__36_32_WORD, 0, 0x1F}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_8 alias: debug_bus_trisc note: TRISC2
//  name: if_ex_deco
#define DEBUG_BUS_DEBUG_TENSIX_IN_8_IF_EX_DECO__31_0_WORD {17, 7, 4, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_8_IF_EX_DECO__31_0 {DEBUG_BUS_DEBUG_TENSIX_IN_8_IF_EX_DECO__31_0_WORD, 0, 0xFFFFFFFF}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_8 alias: debug_bus_trisc note: TRISC2
//  name: id_ex_rts
#define DEBUG_BUS_DEBUG_DAISY_STOP_TENSIX_DEBUG_TENSIX_IN_8_ID_EX_RTS__2239_WORD {17, 7, 2, 0, 1, 0}
#define DEBUG_BUS_DEBUG_DAISY_STOP_TENSIX_DEBUG_TENSIX_IN_8_ID_EX_RTS__2239 {DEBUG_BUS_DEBUG_DAISY_STOP_TENSIX_DEBUG_TENSIX_IN_8_ID_EX_RTS__2239_WORD, 31, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_8 alias: debug_bus_trisc note: TRISC2
//  name: ex_id_rtr
#define DEBUG_BUS_DEBUG_DAISY_STOP_TENSIX_DEBUG_TENSIX_IN_8_EX_ID_RTR__2238_WORD {17, 7, 2, 0, 1, 0}
#define DEBUG_BUS_DEBUG_DAISY_STOP_TENSIX_DEBUG_TENSIX_IN_8_EX_ID_RTR__2238 {DEBUG_BUS_DEBUG_DAISY_STOP_TENSIX_DEBUG_TENSIX_IN_8_EX_ID_RTR__2238_WORD, 30, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_8 alias: debug_bus_trisc note: TRISC2
//  name: id_ex_pc
#define DEBUG_BUS_DEBUG_TENSIX_IN_8_ID_EX_PC__29_0_WORD {17, 7, 2, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_8_ID_EX_PC__29_0 {DEBUG_BUS_DEBUG_TENSIX_IN_8_ID_EX_PC__29_0_WORD, 0, 0x3FFFFFFF}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_8 alias: debug_bus_trisc note: TRISC2
//  name: id_rf_wr_flag
#define DEBUG_BUS_DEBUG_TENSIX_IN_8_ID_RF_WR_FLAG_WORD {17, 7, 0, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_8_ID_RF_WR_FLAG {DEBUG_BUS_DEBUG_TENSIX_IN_8_ID_RF_WR_FLAG_WORD, 28, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_8 alias: debug_bus_trisc note: TRISC2
//  name: id_rf_wraddr
#define DEBUG_BUS_DEBUG_TENSIX_IN_8_ID_RF_WRADDR_WORD {17, 7, 0, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_8_ID_RF_WRADDR {DEBUG_BUS_DEBUG_TENSIX_IN_8_ID_RF_WRADDR_WORD, 20, 0x1F}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_8 alias: debug_bus_trisc note: TRISC2
//  name: id_rf_p1_rden
#define DEBUG_BUS_DEBUG_TENSIX_IN_8_ID_RF_P1_RDEN_WORD {17, 7, 0, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_8_ID_RF_P1_RDEN {DEBUG_BUS_DEBUG_TENSIX_IN_8_ID_RF_P1_RDEN_WORD, 18, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_8 alias: debug_bus_trisc note: TRISC2
//  name: id_rf_p1_rdaddr
#define DEBUG_BUS_DEBUG_TENSIX_IN_8_ID_RF_P1_RDADDR_WORD {17, 7, 0, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_8_ID_RF_P1_RDADDR {DEBUG_BUS_DEBUG_TENSIX_IN_8_ID_RF_P1_RDADDR_WORD, 10, 0x1F}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_8 alias: debug_bus_trisc note: TRISC2
//  name: id_rf_p0_rden
#define DEBUG_BUS_DEBUG_TENSIX_IN_8_ID_RF_P0_RDEN_WORD {17, 7, 0, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_8_ID_RF_P0_RDEN {DEBUG_BUS_DEBUG_TENSIX_IN_8_ID_RF_P0_RDEN_WORD, 8, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_8 alias: debug_bus_trisc note: TRISC2
//  name: id_rf_p0_rdaddr
#define DEBUG_BUS_DEBUG_TENSIX_IN_8_ID_RF_P0_RDADDR_WORD {17, 7, 0, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_8_ID_RF_P0_RDADDR {DEBUG_BUS_DEBUG_TENSIX_IN_8_ID_RF_P0_RDADDR_WORD, 0, 0x1F}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_8 alias: debug_bus_trisc note: TRISC2
//  name: i_instrn_vld
#define DEBUG_BUS_DEBUG_TENSIX_IN_8_I_INSTRN_VLD_WORD {16, 7, 6, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_8_I_INSTRN_VLD {DEBUG_BUS_DEBUG_TENSIX_IN_8_I_INSTRN_VLD_WORD, 31, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_8 alias: debug_bus_trisc note: TRISC2
//  name: i_instrn
#define DEBUG_BUS_DEBUG_TENSIX_IN_8_I_INSTRN__30_0_WORD {16, 7, 6, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_8_I_INSTRN__30_0 {DEBUG_BUS_DEBUG_TENSIX_IN_8_I_INSTRN__30_0_WORD, 0, 0x7FFFFFFF}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_8 alias: debug_bus_trisc note: TRISC2
//  name: i_instrn_req_rtr
#define DEBUG_BUS_DEBUG_TENSIX_IN_8_I_INSTRN_REQ_RTR_WORD {16, 7, 4, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_8_I_INSTRN_REQ_RTR {DEBUG_BUS_DEBUG_TENSIX_IN_8_I_INSTRN_REQ_RTR_WORD, 31, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_8 alias: debug_bus_trisc note: TRISC2
//  name: (o_instrn_req_early&~o_instrn_req_cancel)
#define DEBUG_BUS_DEBUG_TENSIX_IN_8__O_INSTRN_REQ_EARLY__O_INSTRN_REQ_CANCEL__WORD {16, 7, 4, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_8__O_INSTRN_REQ_EARLY__O_INSTRN_REQ_CANCEL_ {DEBUG_BUS_DEBUG_TENSIX_IN_8__O_INSTRN_REQ_EARLY__O_INSTRN_REQ_CANCEL__WORD, 30, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_8 alias: debug_bus_trisc note: TRISC2
//  name: o_instrn_addr
#define DEBUG_BUS_DEBUG_TENSIX_IN_8_O_INSTRN_ADDR__29_0_WORD {16, 7, 4, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_8_O_INSTRN_ADDR__29_0 {DEBUG_BUS_DEBUG_TENSIX_IN_8_O_INSTRN_ADDR__29_0_WORD, 0, 0x3FFFFFFF}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_8 alias: debug_bus_trisc note: TRISC2
//  name: dbg_obs_mem_wren
#define DEBUG_BUS_DEBUG_TENSIX_IN_8_DBG_OBS_MEM_WREN_WORD {16, 7, 2, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_8_DBG_OBS_MEM_WREN {DEBUG_BUS_DEBUG_TENSIX_IN_8_DBG_OBS_MEM_WREN_WORD, 31, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_8 alias: debug_bus_trisc note: TRISC2
//  name: dbg_obs_mem_rden
#define DEBUG_BUS_DEBUG_TENSIX_IN_8_DBG_OBS_MEM_RDEN_WORD {16, 7, 2, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_8_DBG_OBS_MEM_RDEN {DEBUG_BUS_DEBUG_TENSIX_IN_8_DBG_OBS_MEM_RDEN_WORD, 30, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_8 alias: debug_bus_trisc note: TRISC2
//  name: dbg_obs_mem_addr
#define DEBUG_BUS_DEBUG_TENSIX_IN_8_DBG_OBS_MEM_ADDR__29_0_WORD {16, 7, 2, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_8_DBG_OBS_MEM_ADDR__29_0 {DEBUG_BUS_DEBUG_TENSIX_IN_8_DBG_OBS_MEM_ADDR__29_0_WORD, 0, 0x3FFFFFFF}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_8 alias: debug_bus_trisc note: TRISC2
//  name: dbg_obs_cmt_vld
#define DEBUG_BUS_DEBUG_TENSIX_IN_8_DBG_OBS_CMT_VLD_WORD {16, 7, 0, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_8_DBG_OBS_CMT_VLD {DEBUG_BUS_DEBUG_TENSIX_IN_8_DBG_OBS_CMT_VLD_WORD, 31, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_8 alias: debug_bus_trisc note: TRISC2
//  name: dbg_obs_cmt_pc
#define DEBUG_BUS_DEBUG_TENSIX_IN_8_DBG_OBS_CMT_PC__30_0_WORD {16, 7, 0, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_8_DBG_OBS_CMT_PC__30_0 {DEBUG_BUS_DEBUG_TENSIX_IN_8_DBG_OBS_CMT_PC__30_0_WORD, 0, 0x7FFFFFFF}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_7 alias: debug_bus_trisc note: TRISC1
//  name: ex_id_rtr
#define DEBUG_BUS_DEBUG_DAISY_STOP_TENSIX_DEBUG_TENSIX_IN_7_EX_ID_RTR__2025_WORD {15, 7, 6, 0, 1, 0}
#define DEBUG_BUS_DEBUG_DAISY_STOP_TENSIX_DEBUG_TENSIX_IN_7_EX_ID_RTR__2025 {DEBUG_BUS_DEBUG_DAISY_STOP_TENSIX_DEBUG_TENSIX_IN_7_EX_ID_RTR__2025_WORD, 9, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_7 alias: debug_bus_trisc note: TRISC1
//  name: id_ex_rts
#define DEBUG_BUS_DEBUG_DAISY_STOP_TENSIX_DEBUG_TENSIX_IN_7_ID_EX_RTS__2024_WORD {15, 7, 6, 0, 1, 0}
#define DEBUG_BUS_DEBUG_DAISY_STOP_TENSIX_DEBUG_TENSIX_IN_7_ID_EX_RTS__2024 {DEBUG_BUS_DEBUG_DAISY_STOP_TENSIX_DEBUG_TENSIX_IN_7_ID_EX_RTS__2024_WORD, 8, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_7 alias: debug_bus_trisc note: TRISC1
//  name: if_rts
#define DEBUG_BUS_DEBUG_TENSIX_IN_7_IF_RTS_WORD {15, 7, 6, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_7_IF_RTS {DEBUG_BUS_DEBUG_TENSIX_IN_7_IF_RTS_WORD, 7, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_7 alias: debug_bus_trisc note: TRISC1
//  name: if_ex_predicted
#define DEBUG_BUS_DEBUG_TENSIX_IN_7_IF_EX_PREDICTED_WORD {15, 7, 6, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_7_IF_EX_PREDICTED {DEBUG_BUS_DEBUG_TENSIX_IN_7_IF_EX_PREDICTED_WORD, 5, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_7 alias: debug_bus_trisc note: TRISC1
//  name: if_ex_deco
#define DEBUG_BUS_DEBUG_TENSIX_IN_7_IF_EX_DECO__36_32_WORD {15, 7, 6, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_7_IF_EX_DECO__36_32 {DEBUG_BUS_DEBUG_TENSIX_IN_7_IF_EX_DECO__36_32_WORD, 0, 0x1F}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_7 alias: debug_bus_trisc note: TRISC1
//  name: if_ex_deco
#define DEBUG_BUS_DEBUG_TENSIX_IN_7_IF_EX_DECO__31_0_WORD {15, 7, 4, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_7_IF_EX_DECO__31_0 {DEBUG_BUS_DEBUG_TENSIX_IN_7_IF_EX_DECO__31_0_WORD, 0, 0xFFFFFFFF}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_7 alias: debug_bus_trisc note: TRISC1
//  name: id_ex_rts
#define DEBUG_BUS_DEBUG_DAISY_STOP_TENSIX_DEBUG_TENSIX_IN_7_ID_EX_RTS__1983_WORD {15, 7, 2, 0, 1, 0}
#define DEBUG_BUS_DEBUG_DAISY_STOP_TENSIX_DEBUG_TENSIX_IN_7_ID_EX_RTS__1983 {DEBUG_BUS_DEBUG_DAISY_STOP_TENSIX_DEBUG_TENSIX_IN_7_ID_EX_RTS__1983_WORD, 31, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_7 alias: debug_bus_trisc note: TRISC1
//  name: ex_id_rtr
#define DEBUG_BUS_DEBUG_DAISY_STOP_TENSIX_DEBUG_TENSIX_IN_7_EX_ID_RTR__1982_WORD {15, 7, 2, 0, 1, 0}
#define DEBUG_BUS_DEBUG_DAISY_STOP_TENSIX_DEBUG_TENSIX_IN_7_EX_ID_RTR__1982 {DEBUG_BUS_DEBUG_DAISY_STOP_TENSIX_DEBUG_TENSIX_IN_7_EX_ID_RTR__1982_WORD, 30, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_7 alias: debug_bus_trisc note: TRISC1
//  name: id_ex_pc
#define DEBUG_BUS_DEBUG_TENSIX_IN_7_ID_EX_PC__29_0_WORD {15, 7, 2, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_7_ID_EX_PC__29_0 {DEBUG_BUS_DEBUG_TENSIX_IN_7_ID_EX_PC__29_0_WORD, 0, 0x3FFFFFFF}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_7 alias: debug_bus_trisc note: TRISC1
//  name: id_rf_wr_flag
#define DEBUG_BUS_DEBUG_TENSIX_IN_7_ID_RF_WR_FLAG_WORD {15, 7, 0, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_7_ID_RF_WR_FLAG {DEBUG_BUS_DEBUG_TENSIX_IN_7_ID_RF_WR_FLAG_WORD, 28, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_7 alias: debug_bus_trisc note: TRISC1
//  name: id_rf_wraddr
#define DEBUG_BUS_DEBUG_TENSIX_IN_7_ID_RF_WRADDR_WORD {15, 7, 0, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_7_ID_RF_WRADDR {DEBUG_BUS_DEBUG_TENSIX_IN_7_ID_RF_WRADDR_WORD, 20, 0x1F}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_7 alias: debug_bus_trisc note: TRISC1
//  name: id_rf_p1_rden
#define DEBUG_BUS_DEBUG_TENSIX_IN_7_ID_RF_P1_RDEN_WORD {15, 7, 0, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_7_ID_RF_P1_RDEN {DEBUG_BUS_DEBUG_TENSIX_IN_7_ID_RF_P1_RDEN_WORD, 18, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_7 alias: debug_bus_trisc note: TRISC1
//  name: id_rf_p1_rdaddr
#define DEBUG_BUS_DEBUG_TENSIX_IN_7_ID_RF_P1_RDADDR_WORD {15, 7, 0, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_7_ID_RF_P1_RDADDR {DEBUG_BUS_DEBUG_TENSIX_IN_7_ID_RF_P1_RDADDR_WORD, 10, 0x1F}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_7 alias: debug_bus_trisc note: TRISC1
//  name: id_rf_p0_rden
#define DEBUG_BUS_DEBUG_TENSIX_IN_7_ID_RF_P0_RDEN_WORD {15, 7, 0, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_7_ID_RF_P0_RDEN {DEBUG_BUS_DEBUG_TENSIX_IN_7_ID_RF_P0_RDEN_WORD, 8, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_7 alias: debug_bus_trisc note: TRISC1
//  name: id_rf_p0_rdaddr
#define DEBUG_BUS_DEBUG_TENSIX_IN_7_ID_RF_P0_RDADDR_WORD {15, 7, 0, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_7_ID_RF_P0_RDADDR {DEBUG_BUS_DEBUG_TENSIX_IN_7_ID_RF_P0_RDADDR_WORD, 0, 0x1F}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_7 alias: debug_bus_trisc note: TRISC1
//  name: i_instrn_vld
#define DEBUG_BUS_DEBUG_TENSIX_IN_7_I_INSTRN_VLD_WORD {14, 7, 6, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_7_I_INSTRN_VLD {DEBUG_BUS_DEBUG_TENSIX_IN_7_I_INSTRN_VLD_WORD, 31, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_7 alias: debug_bus_trisc note: TRISC1
//  name: i_instrn
#define DEBUG_BUS_DEBUG_TENSIX_IN_7_I_INSTRN__30_0_WORD {14, 7, 6, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_7_I_INSTRN__30_0 {DEBUG_BUS_DEBUG_TENSIX_IN_7_I_INSTRN__30_0_WORD, 0, 0x7FFFFFFF}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_7 alias: debug_bus_trisc note: TRISC1
//  name: i_instrn_req_rtr
#define DEBUG_BUS_DEBUG_TENSIX_IN_7_I_INSTRN_REQ_RTR_WORD {14, 7, 4, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_7_I_INSTRN_REQ_RTR {DEBUG_BUS_DEBUG_TENSIX_IN_7_I_INSTRN_REQ_RTR_WORD, 31, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_7 alias: debug_bus_trisc note: TRISC1
//  name: (o_instrn_req_early&~o_instrn_req_cancel)
#define DEBUG_BUS_DEBUG_TENSIX_IN_7__O_INSTRN_REQ_EARLY__O_INSTRN_REQ_CANCEL__WORD {14, 7, 4, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_7__O_INSTRN_REQ_EARLY__O_INSTRN_REQ_CANCEL_ {DEBUG_BUS_DEBUG_TENSIX_IN_7__O_INSTRN_REQ_EARLY__O_INSTRN_REQ_CANCEL__WORD, 30, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_7 alias: debug_bus_trisc note: TRISC1
//  name: o_instrn_addr
#define DEBUG_BUS_DEBUG_TENSIX_IN_7_O_INSTRN_ADDR__29_0_WORD {14, 7, 4, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_7_O_INSTRN_ADDR__29_0 {DEBUG_BUS_DEBUG_TENSIX_IN_7_O_INSTRN_ADDR__29_0_WORD, 0, 0x3FFFFFFF}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_7 alias: debug_bus_trisc note: TRISC1
//  name: dbg_obs_mem_wren
#define DEBUG_BUS_DEBUG_TENSIX_IN_7_DBG_OBS_MEM_WREN_WORD {14, 7, 2, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_7_DBG_OBS_MEM_WREN {DEBUG_BUS_DEBUG_TENSIX_IN_7_DBG_OBS_MEM_WREN_WORD, 31, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_7 alias: debug_bus_trisc note: TRISC1
//  name: dbg_obs_mem_rden
#define DEBUG_BUS_DEBUG_TENSIX_IN_7_DBG_OBS_MEM_RDEN_WORD {14, 7, 2, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_7_DBG_OBS_MEM_RDEN {DEBUG_BUS_DEBUG_TENSIX_IN_7_DBG_OBS_MEM_RDEN_WORD, 30, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_7 alias: debug_bus_trisc note: TRISC1
//  name: dbg_obs_mem_addr
#define DEBUG_BUS_DEBUG_TENSIX_IN_7_DBG_OBS_MEM_ADDR__29_0_WORD {14, 7, 2, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_7_DBG_OBS_MEM_ADDR__29_0 {DEBUG_BUS_DEBUG_TENSIX_IN_7_DBG_OBS_MEM_ADDR__29_0_WORD, 0, 0x3FFFFFFF}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_7 alias: debug_bus_trisc note: TRISC1
//  name: dbg_obs_cmt_vld
#define DEBUG_BUS_DEBUG_TENSIX_IN_7_DBG_OBS_CMT_VLD_WORD {14, 7, 0, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_7_DBG_OBS_CMT_VLD {DEBUG_BUS_DEBUG_TENSIX_IN_7_DBG_OBS_CMT_VLD_WORD, 31, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_7 alias: debug_bus_trisc note: TRISC1
//  name: dbg_obs_cmt_pc
#define DEBUG_BUS_DEBUG_TENSIX_IN_7_DBG_OBS_CMT_PC__30_0_WORD {14, 7, 0, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_7_DBG_OBS_CMT_PC__30_0 {DEBUG_BUS_DEBUG_TENSIX_IN_7_DBG_OBS_CMT_PC__30_0_WORD, 0, 0x7FFFFFFF}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_6 alias: debug_bus_trisc note: TRISC0
//  name: ex_id_rtr
#define DEBUG_BUS_DEBUG_DAISY_STOP_TENSIX_DEBUG_TENSIX_IN_6_EX_ID_RTR__1769_WORD {13, 7, 6, 0, 1, 0}
#define DEBUG_BUS_DEBUG_DAISY_STOP_TENSIX_DEBUG_TENSIX_IN_6_EX_ID_RTR__1769 {DEBUG_BUS_DEBUG_DAISY_STOP_TENSIX_DEBUG_TENSIX_IN_6_EX_ID_RTR__1769_WORD, 9, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_6 alias: debug_bus_trisc note: TRISC0
//  name: id_ex_rts
#define DEBUG_BUS_DEBUG_DAISY_STOP_TENSIX_DEBUG_TENSIX_IN_6_ID_EX_RTS__1768_WORD {13, 7, 6, 0, 1, 0}
#define DEBUG_BUS_DEBUG_DAISY_STOP_TENSIX_DEBUG_TENSIX_IN_6_ID_EX_RTS__1768 {DEBUG_BUS_DEBUG_DAISY_STOP_TENSIX_DEBUG_TENSIX_IN_6_ID_EX_RTS__1768_WORD, 8, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_6 alias: debug_bus_trisc note: TRISC0
//  name: if_rts
#define DEBUG_BUS_DEBUG_TENSIX_IN_6_IF_RTS_WORD {13, 7, 6, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_6_IF_RTS {DEBUG_BUS_DEBUG_TENSIX_IN_6_IF_RTS_WORD, 7, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_6 alias: debug_bus_trisc note: TRISC0
//  name: if_ex_predicted
#define DEBUG_BUS_DEBUG_TENSIX_IN_6_IF_EX_PREDICTED_WORD {13, 7, 6, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_6_IF_EX_PREDICTED {DEBUG_BUS_DEBUG_TENSIX_IN_6_IF_EX_PREDICTED_WORD, 5, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_6 alias: debug_bus_trisc note: TRISC0
//  name: if_ex_deco
#define DEBUG_BUS_DEBUG_TENSIX_IN_6_IF_EX_DECO__36_32_WORD {13, 7, 6, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_6_IF_EX_DECO__36_32 {DEBUG_BUS_DEBUG_TENSIX_IN_6_IF_EX_DECO__36_32_WORD, 0, 0x1F}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_6 alias: debug_bus_trisc note: TRISC0
//  name: if_ex_deco
#define DEBUG_BUS_DEBUG_TENSIX_IN_6_IF_EX_DECO__31_0_WORD {13, 7, 4, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_6_IF_EX_DECO__31_0 {DEBUG_BUS_DEBUG_TENSIX_IN_6_IF_EX_DECO__31_0_WORD, 0, 0xFFFFFFFF}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_6 alias: debug_bus_trisc note: TRISC0
//  name: id_ex_rts
#define DEBUG_BUS_DEBUG_DAISY_STOP_TENSIX_DEBUG_TENSIX_IN_6_ID_EX_RTS__1727_WORD {13, 7, 2, 0, 1, 0}
#define DEBUG_BUS_DEBUG_DAISY_STOP_TENSIX_DEBUG_TENSIX_IN_6_ID_EX_RTS__1727 {DEBUG_BUS_DEBUG_DAISY_STOP_TENSIX_DEBUG_TENSIX_IN_6_ID_EX_RTS__1727_WORD, 31, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_6 alias: debug_bus_trisc note: TRISC0
//  name: ex_id_rtr
#define DEBUG_BUS_DEBUG_DAISY_STOP_TENSIX_DEBUG_TENSIX_IN_6_EX_ID_RTR__1726_WORD {13, 7, 2, 0, 1, 0}
#define DEBUG_BUS_DEBUG_DAISY_STOP_TENSIX_DEBUG_TENSIX_IN_6_EX_ID_RTR__1726 {DEBUG_BUS_DEBUG_DAISY_STOP_TENSIX_DEBUG_TENSIX_IN_6_EX_ID_RTR__1726_WORD, 30, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_6 alias: debug_bus_trisc note: TRISC0
//  name: id_ex_pc
#define DEBUG_BUS_DEBUG_TENSIX_IN_6_ID_EX_PC__29_0_WORD {13, 7, 2, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_6_ID_EX_PC__29_0 {DEBUG_BUS_DEBUG_TENSIX_IN_6_ID_EX_PC__29_0_WORD, 0, 0x3FFFFFFF}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_6 alias: debug_bus_trisc note: TRISC0
//  name: id_rf_wr_flag
#define DEBUG_BUS_DEBUG_TENSIX_IN_6_ID_RF_WR_FLAG_WORD {13, 7, 0, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_6_ID_RF_WR_FLAG {DEBUG_BUS_DEBUG_TENSIX_IN_6_ID_RF_WR_FLAG_WORD, 28, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_6 alias: debug_bus_trisc note: TRISC0
//  name: id_rf_wraddr
#define DEBUG_BUS_DEBUG_TENSIX_IN_6_ID_RF_WRADDR_WORD {13, 7, 0, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_6_ID_RF_WRADDR {DEBUG_BUS_DEBUG_TENSIX_IN_6_ID_RF_WRADDR_WORD, 20, 0x1F}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_6 alias: debug_bus_trisc note: TRISC0
//  name: id_rf_p1_rden
#define DEBUG_BUS_DEBUG_TENSIX_IN_6_ID_RF_P1_RDEN_WORD {13, 7, 0, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_6_ID_RF_P1_RDEN {DEBUG_BUS_DEBUG_TENSIX_IN_6_ID_RF_P1_RDEN_WORD, 18, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_6 alias: debug_bus_trisc note: TRISC0
//  name: id_rf_p1_rdaddr
#define DEBUG_BUS_DEBUG_TENSIX_IN_6_ID_RF_P1_RDADDR_WORD {13, 7, 0, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_6_ID_RF_P1_RDADDR {DEBUG_BUS_DEBUG_TENSIX_IN_6_ID_RF_P1_RDADDR_WORD, 10, 0x1F}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_6 alias: debug_bus_trisc note: TRISC0
//  name: id_rf_p0_rden
#define DEBUG_BUS_DEBUG_TENSIX_IN_6_ID_RF_P0_RDEN_WORD {13, 7, 0, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_6_ID_RF_P0_RDEN {DEBUG_BUS_DEBUG_TENSIX_IN_6_ID_RF_P0_RDEN_WORD, 8, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_6 alias: debug_bus_trisc note: TRISC0
//  name: id_rf_p0_rdaddr
#define DEBUG_BUS_DEBUG_TENSIX_IN_6_ID_RF_P0_RDADDR_WORD {13, 7, 0, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_6_ID_RF_P0_RDADDR {DEBUG_BUS_DEBUG_TENSIX_IN_6_ID_RF_P0_RDADDR_WORD, 0, 0x1F}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_6 alias: debug_bus_trisc note: TRISC0
//  name: i_instrn_vld
#define DEBUG_BUS_DEBUG_TENSIX_IN_6_I_INSTRN_VLD_WORD {12, 7, 6, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_6_I_INSTRN_VLD {DEBUG_BUS_DEBUG_TENSIX_IN_6_I_INSTRN_VLD_WORD, 31, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_6 alias: debug_bus_trisc note: TRISC0
//  name: i_instrn
#define DEBUG_BUS_DEBUG_TENSIX_IN_6_I_INSTRN__30_0_WORD {12, 7, 6, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_6_I_INSTRN__30_0 {DEBUG_BUS_DEBUG_TENSIX_IN_6_I_INSTRN__30_0_WORD, 0, 0x7FFFFFFF}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_6 alias: debug_bus_trisc note: TRISC0
//  name: i_instrn_req_rtr
#define DEBUG_BUS_DEBUG_TENSIX_IN_6_I_INSTRN_REQ_RTR_WORD {12, 7, 4, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_6_I_INSTRN_REQ_RTR {DEBUG_BUS_DEBUG_TENSIX_IN_6_I_INSTRN_REQ_RTR_WORD, 31, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_6 alias: debug_bus_trisc note: TRISC0
//  name: (o_instrn_req_early&~o_instrn_req_cancel)
#define DEBUG_BUS_DEBUG_TENSIX_IN_6__O_INSTRN_REQ_EARLY__O_INSTRN_REQ_CANCEL__WORD {12, 7, 4, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_6__O_INSTRN_REQ_EARLY__O_INSTRN_REQ_CANCEL_ {DEBUG_BUS_DEBUG_TENSIX_IN_6__O_INSTRN_REQ_EARLY__O_INSTRN_REQ_CANCEL__WORD, 30, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_6 alias: debug_bus_trisc note: TRISC0
//  name: o_instrn_addr
#define DEBUG_BUS_DEBUG_TENSIX_IN_6_O_INSTRN_ADDR__29_0_WORD {12, 7, 4, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_6_O_INSTRN_ADDR__29_0 {DEBUG_BUS_DEBUG_TENSIX_IN_6_O_INSTRN_ADDR__29_0_WORD, 0, 0x3FFFFFFF}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_6 alias: debug_bus_trisc note: TRISC0
//  name: dbg_obs_mem_wren
#define DEBUG_BUS_DEBUG_TENSIX_IN_6_DBG_OBS_MEM_WREN_WORD {12, 7, 2, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_6_DBG_OBS_MEM_WREN {DEBUG_BUS_DEBUG_TENSIX_IN_6_DBG_OBS_MEM_WREN_WORD, 31, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_6 alias: debug_bus_trisc note: TRISC0
//  name: dbg_obs_mem_rden
#define DEBUG_BUS_DEBUG_TENSIX_IN_6_DBG_OBS_MEM_RDEN_WORD {12, 7, 2, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_6_DBG_OBS_MEM_RDEN {DEBUG_BUS_DEBUG_TENSIX_IN_6_DBG_OBS_MEM_RDEN_WORD, 30, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_6 alias: debug_bus_trisc note: TRISC0
//  name: dbg_obs_mem_addr
#define DEBUG_BUS_DEBUG_TENSIX_IN_6_DBG_OBS_MEM_ADDR__29_0_WORD {12, 7, 2, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_6_DBG_OBS_MEM_ADDR__29_0 {DEBUG_BUS_DEBUG_TENSIX_IN_6_DBG_OBS_MEM_ADDR__29_0_WORD, 0, 0x3FFFFFFF}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_6 alias: debug_bus_trisc note: TRISC0
//  name: dbg_obs_cmt_vld
#define DEBUG_BUS_DEBUG_TENSIX_IN_6_DBG_OBS_CMT_VLD_WORD {12, 7, 0, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_6_DBG_OBS_CMT_VLD {DEBUG_BUS_DEBUG_TENSIX_IN_6_DBG_OBS_CMT_VLD_WORD, 31, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_6 alias: debug_bus_trisc note: TRISC0
//  name: dbg_obs_cmt_pc
#define DEBUG_BUS_DEBUG_TENSIX_IN_6_DBG_OBS_CMT_PC__30_0_WORD {12, 7, 0, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_6_DBG_OBS_CMT_PC__30_0 {DEBUG_BUS_DEBUG_TENSIX_IN_6_DBG_OBS_CMT_PC__30_0_WORD, 0, 0x7FFFFFFF}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_5 alias: debug_bus_trisc note: BRISC
//  name: ex_id_rtr
#define DEBUG_BUS_DEBUG_DAISY_STOP_TENSIX_DEBUG_TENSIX_IN_5_EX_ID_RTR__1513_WORD {11, 7, 6, 0, 1, 0}
#define DEBUG_BUS_DEBUG_DAISY_STOP_TENSIX_DEBUG_TENSIX_IN_5_EX_ID_RTR__1513 {DEBUG_BUS_DEBUG_DAISY_STOP_TENSIX_DEBUG_TENSIX_IN_5_EX_ID_RTR__1513_WORD, 9, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_5 alias: debug_bus_trisc note: BRISC
//  name: id_ex_rts
#define DEBUG_BUS_DEBUG_DAISY_STOP_TENSIX_DEBUG_TENSIX_IN_5_ID_EX_RTS__1512_WORD {11, 7, 6, 0, 1, 0}
#define DEBUG_BUS_DEBUG_DAISY_STOP_TENSIX_DEBUG_TENSIX_IN_5_ID_EX_RTS__1512 {DEBUG_BUS_DEBUG_DAISY_STOP_TENSIX_DEBUG_TENSIX_IN_5_ID_EX_RTS__1512_WORD, 8, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_5 alias: debug_bus_trisc note: BRISC
//  name: if_rts
#define DEBUG_BUS_DEBUG_TENSIX_IN_5_IF_RTS_WORD {11, 7, 6, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_5_IF_RTS {DEBUG_BUS_DEBUG_TENSIX_IN_5_IF_RTS_WORD, 7, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_5 alias: debug_bus_trisc note: BRISC
//  name: if_ex_predicted
#define DEBUG_BUS_DEBUG_TENSIX_IN_5_IF_EX_PREDICTED_WORD {11, 7, 6, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_5_IF_EX_PREDICTED {DEBUG_BUS_DEBUG_TENSIX_IN_5_IF_EX_PREDICTED_WORD, 5, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_5 alias: debug_bus_trisc note: BRISC
//  name: if_ex_deco
#define DEBUG_BUS_DEBUG_TENSIX_IN_5_IF_EX_DECO__36_32_WORD {11, 7, 6, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_5_IF_EX_DECO__36_32 {DEBUG_BUS_DEBUG_TENSIX_IN_5_IF_EX_DECO__36_32_WORD, 0, 0x1F}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_5 alias: debug_bus_trisc note: BRISC
//  name: if_ex_deco
#define DEBUG_BUS_DEBUG_TENSIX_IN_5_IF_EX_DECO__31_0_WORD {11, 7, 4, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_5_IF_EX_DECO__31_0 {DEBUG_BUS_DEBUG_TENSIX_IN_5_IF_EX_DECO__31_0_WORD, 0, 0xFFFFFFFF}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_5 alias: debug_bus_trisc note: BRISC
//  name: id_ex_rts
#define DEBUG_BUS_DEBUG_DAISY_STOP_TENSIX_DEBUG_TENSIX_IN_5_ID_EX_RTS__1471_WORD {11, 7, 2, 0, 1, 0}
#define DEBUG_BUS_DEBUG_DAISY_STOP_TENSIX_DEBUG_TENSIX_IN_5_ID_EX_RTS__1471 {DEBUG_BUS_DEBUG_DAISY_STOP_TENSIX_DEBUG_TENSIX_IN_5_ID_EX_RTS__1471_WORD, 31, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_5 alias: debug_bus_trisc note: BRISC
//  name: ex_id_rtr
#define DEBUG_BUS_DEBUG_DAISY_STOP_TENSIX_DEBUG_TENSIX_IN_5_EX_ID_RTR__1470_WORD {11, 7, 2, 0, 1, 0}
#define DEBUG_BUS_DEBUG_DAISY_STOP_TENSIX_DEBUG_TENSIX_IN_5_EX_ID_RTR__1470 {DEBUG_BUS_DEBUG_DAISY_STOP_TENSIX_DEBUG_TENSIX_IN_5_EX_ID_RTR__1470_WORD, 30, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_5 alias: debug_bus_trisc note: BRISC
//  name: id_ex_pc
#define DEBUG_BUS_DEBUG_TENSIX_IN_5_ID_EX_PC__29_0_WORD {11, 7, 2, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_5_ID_EX_PC__29_0 {DEBUG_BUS_DEBUG_TENSIX_IN_5_ID_EX_PC__29_0_WORD, 0, 0x3FFFFFFF}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_5 alias: debug_bus_trisc note: BRISC
//  name: id_rf_wr_flag
#define DEBUG_BUS_DEBUG_TENSIX_IN_5_ID_RF_WR_FLAG_WORD {11, 7, 0, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_5_ID_RF_WR_FLAG {DEBUG_BUS_DEBUG_TENSIX_IN_5_ID_RF_WR_FLAG_WORD, 28, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_5 alias: debug_bus_trisc note: BRISC
//  name: id_rf_wraddr
#define DEBUG_BUS_DEBUG_TENSIX_IN_5_ID_RF_WRADDR_WORD {11, 7, 0, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_5_ID_RF_WRADDR {DEBUG_BUS_DEBUG_TENSIX_IN_5_ID_RF_WRADDR_WORD, 20, 0x1F}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_5 alias: debug_bus_trisc note: BRISC
//  name: id_rf_p1_rden
#define DEBUG_BUS_DEBUG_TENSIX_IN_5_ID_RF_P1_RDEN_WORD {11, 7, 0, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_5_ID_RF_P1_RDEN {DEBUG_BUS_DEBUG_TENSIX_IN_5_ID_RF_P1_RDEN_WORD, 18, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_5 alias: debug_bus_trisc note: BRISC
//  name: id_rf_p1_rdaddr
#define DEBUG_BUS_DEBUG_TENSIX_IN_5_ID_RF_P1_RDADDR_WORD {11, 7, 0, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_5_ID_RF_P1_RDADDR {DEBUG_BUS_DEBUG_TENSIX_IN_5_ID_RF_P1_RDADDR_WORD, 10, 0x1F}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_5 alias: debug_bus_trisc note: BRISC
//  name: id_rf_p0_rden
#define DEBUG_BUS_DEBUG_TENSIX_IN_5_ID_RF_P0_RDEN_WORD {11, 7, 0, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_5_ID_RF_P0_RDEN {DEBUG_BUS_DEBUG_TENSIX_IN_5_ID_RF_P0_RDEN_WORD, 8, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_5 alias: debug_bus_trisc note: BRISC
//  name: id_rf_p0_rdaddr
#define DEBUG_BUS_DEBUG_TENSIX_IN_5_ID_RF_P0_RDADDR_WORD {11, 7, 0, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_5_ID_RF_P0_RDADDR {DEBUG_BUS_DEBUG_TENSIX_IN_5_ID_RF_P0_RDADDR_WORD, 0, 0x1F}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_5 alias: debug_bus_trisc note: BRISC
//  name: i_instrn_vld
#define DEBUG_BUS_DEBUG_TENSIX_IN_5_I_INSTRN_VLD_WORD {10, 7, 6, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_5_I_INSTRN_VLD {DEBUG_BUS_DEBUG_TENSIX_IN_5_I_INSTRN_VLD_WORD, 31, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_5 alias: debug_bus_trisc note: BRISC
//  name: i_instrn
#define DEBUG_BUS_DEBUG_TENSIX_IN_5_I_INSTRN__30_0_WORD {10, 7, 6, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_5_I_INSTRN__30_0 {DEBUG_BUS_DEBUG_TENSIX_IN_5_I_INSTRN__30_0_WORD, 0, 0x7FFFFFFF}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_5 alias: debug_bus_trisc note: BRISC
//  name: i_instrn_req_rtr
#define DEBUG_BUS_DEBUG_TENSIX_IN_5_I_INSTRN_REQ_RTR_WORD {10, 7, 4, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_5_I_INSTRN_REQ_RTR {DEBUG_BUS_DEBUG_TENSIX_IN_5_I_INSTRN_REQ_RTR_WORD, 31, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_5 alias: debug_bus_trisc note: BRISC
//  name: (o_instrn_req_early&~o_instrn_req_cancel)
#define DEBUG_BUS_DEBUG_TENSIX_IN_5__O_INSTRN_REQ_EARLY__O_INSTRN_REQ_CANCEL__WORD {10, 7, 4, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_5__O_INSTRN_REQ_EARLY__O_INSTRN_REQ_CANCEL_ {DEBUG_BUS_DEBUG_TENSIX_IN_5__O_INSTRN_REQ_EARLY__O_INSTRN_REQ_CANCEL__WORD, 30, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_5 alias: debug_bus_trisc note: BRISC
//  name: o_instrn_addr
#define DEBUG_BUS_DEBUG_TENSIX_IN_5_O_INSTRN_ADDR__29_0_WORD {10, 7, 4, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_5_O_INSTRN_ADDR__29_0 {DEBUG_BUS_DEBUG_TENSIX_IN_5_O_INSTRN_ADDR__29_0_WORD, 0, 0x3FFFFFFF}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_5 alias: debug_bus_trisc note: BRISC
//  name: dbg_obs_mem_wren
#define DEBUG_BUS_DEBUG_TENSIX_IN_5_DBG_OBS_MEM_WREN_WORD {10, 7, 2, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_5_DBG_OBS_MEM_WREN {DEBUG_BUS_DEBUG_TENSIX_IN_5_DBG_OBS_MEM_WREN_WORD, 31, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_5 alias: debug_bus_trisc note: BRISC
//  name: dbg_obs_mem_rden
#define DEBUG_BUS_DEBUG_TENSIX_IN_5_DBG_OBS_MEM_RDEN_WORD {10, 7, 2, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_5_DBG_OBS_MEM_RDEN {DEBUG_BUS_DEBUG_TENSIX_IN_5_DBG_OBS_MEM_RDEN_WORD, 30, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_5 alias: debug_bus_trisc note: BRISC
//  name: dbg_obs_mem_addr
#define DEBUG_BUS_DEBUG_TENSIX_IN_5_DBG_OBS_MEM_ADDR__29_0_WORD {10, 7, 2, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_5_DBG_OBS_MEM_ADDR__29_0 {DEBUG_BUS_DEBUG_TENSIX_IN_5_DBG_OBS_MEM_ADDR__29_0_WORD, 0, 0x3FFFFFFF}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_5 alias: debug_bus_trisc note: BRISC
//  name: dbg_obs_cmt_vld
#define DEBUG_BUS_DEBUG_TENSIX_IN_5_DBG_OBS_CMT_VLD_WORD {10, 7, 0, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_5_DBG_OBS_CMT_VLD {DEBUG_BUS_DEBUG_TENSIX_IN_5_DBG_OBS_CMT_VLD_WORD, 31, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_5 alias: debug_bus_trisc note: BRISC
//  name: dbg_obs_cmt_pc
#define DEBUG_BUS_DEBUG_TENSIX_IN_5_DBG_OBS_CMT_PC__30_0_WORD {10, 7, 0, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_IN_5_DBG_OBS_CMT_PC__30_0 {DEBUG_BUS_DEBUG_TENSIX_IN_5_DBG_OBS_CMT_PC__30_0_WORD, 0, 0x7FFFFFFF}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_4 alias: perf_cnt_l1_dbg_dual note: 7 and 6
//  name: perf_cnt_l1_dbg alias: perf_cnt note: High counter
//  name: stall_cnt
#define DEBUG_BUS_DEBUG_DAISY_STOP_TENSIX_DEBUG_TENSIX_IN_4_PERF_CNT_L1_DBG_STALL_CNT__1248_WORD {9, 7, 6, 0, 1, 0}
#define DEBUG_BUS_DEBUG_DAISY_STOP_TENSIX_DEBUG_TENSIX_IN_4_PERF_CNT_L1_DBG_STALL_CNT__1248 {DEBUG_BUS_DEBUG_DAISY_STOP_TENSIX_DEBUG_TENSIX_IN_4_PERF_CNT_L1_DBG_STALL_CNT__1248_WORD, 0, 0xFFFFFFFF}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_4 alias: perf_cnt_l1_dbg_dual note: 7 and 6
//  name: perf_cnt_l1_dbg alias: perf_cnt note: High counter
//  name: grant_cnt
#define DEBUG_BUS_DEBUG_DAISY_STOP_TENSIX_DEBUG_TENSIX_IN_4_PERF_CNT_L1_DBG_GRANT_CNT__1216_WORD {9, 7, 4, 0, 1, 0}
#define DEBUG_BUS_DEBUG_DAISY_STOP_TENSIX_DEBUG_TENSIX_IN_4_PERF_CNT_L1_DBG_GRANT_CNT__1216 {DEBUG_BUS_DEBUG_DAISY_STOP_TENSIX_DEBUG_TENSIX_IN_4_PERF_CNT_L1_DBG_GRANT_CNT__1216_WORD, 0, 0xFFFFFFFF}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_4 alias: perf_cnt_l1_dbg_dual note: 7 and 6
//  name: perf_cnt_l1_dbg alias: perf_cnt note: High counter
//  name: req_cnt
#define DEBUG_BUS_DEBUG_DAISY_STOP_TENSIX_DEBUG_TENSIX_IN_4_PERF_CNT_L1_DBG_REQ_CNT__1184_WORD {9, 7, 2, 0, 1, 0}
#define DEBUG_BUS_DEBUG_DAISY_STOP_TENSIX_DEBUG_TENSIX_IN_4_PERF_CNT_L1_DBG_REQ_CNT__1184 {DEBUG_BUS_DEBUG_DAISY_STOP_TENSIX_DEBUG_TENSIX_IN_4_PERF_CNT_L1_DBG_REQ_CNT__1184_WORD, 0, 0xFFFFFFFF}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_4 alias: perf_cnt_l1_dbg_dual note: 7 and 6
//  name: perf_cnt_l1_dbg alias: perf_cnt note: High counter
//  name: ref_cnt
#define DEBUG_BUS_DEBUG_DAISY_STOP_TENSIX_DEBUG_TENSIX_IN_4_PERF_CNT_L1_DBG_REF_CNT__1152_WORD {9, 7, 0, 0, 1, 0}
#define DEBUG_BUS_DEBUG_DAISY_STOP_TENSIX_DEBUG_TENSIX_IN_4_PERF_CNT_L1_DBG_REF_CNT__1152 {DEBUG_BUS_DEBUG_DAISY_STOP_TENSIX_DEBUG_TENSIX_IN_4_PERF_CNT_L1_DBG_REF_CNT__1152_WORD, 0, 0xFFFFFFFF}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_4 alias: perf_cnt_l1_dbg_dual note: 7 and 6
//  name: perf_cnt_l1_dbg alias: perf_cnt note: Low counter
//  name: stall_cnt
#define DEBUG_BUS_DEBUG_DAISY_STOP_TENSIX_DEBUG_TENSIX_IN_4_PERF_CNT_L1_DBG_STALL_CNT__1120_WORD {8, 7, 6, 0, 1, 0}
#define DEBUG_BUS_DEBUG_DAISY_STOP_TENSIX_DEBUG_TENSIX_IN_4_PERF_CNT_L1_DBG_STALL_CNT__1120 {DEBUG_BUS_DEBUG_DAISY_STOP_TENSIX_DEBUG_TENSIX_IN_4_PERF_CNT_L1_DBG_STALL_CNT__1120_WORD, 0, 0xFFFFFFFF}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_4 alias: perf_cnt_l1_dbg_dual note: 7 and 6
//  name: perf_cnt_l1_dbg alias: perf_cnt note: Low counter
//  name: grant_cnt
#define DEBUG_BUS_DEBUG_DAISY_STOP_TENSIX_DEBUG_TENSIX_IN_4_PERF_CNT_L1_DBG_GRANT_CNT__1088_WORD {8, 7, 4, 0, 1, 0}
#define DEBUG_BUS_DEBUG_DAISY_STOP_TENSIX_DEBUG_TENSIX_IN_4_PERF_CNT_L1_DBG_GRANT_CNT__1088 {DEBUG_BUS_DEBUG_DAISY_STOP_TENSIX_DEBUG_TENSIX_IN_4_PERF_CNT_L1_DBG_GRANT_CNT__1088_WORD, 0, 0xFFFFFFFF}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_4 alias: perf_cnt_l1_dbg_dual note: 7 and 6
//  name: perf_cnt_l1_dbg alias: perf_cnt note: Low counter
//  name: req_cnt
#define DEBUG_BUS_DEBUG_DAISY_STOP_TENSIX_DEBUG_TENSIX_IN_4_PERF_CNT_L1_DBG_REQ_CNT__1056_WORD {8, 7, 2, 0, 1, 0}
#define DEBUG_BUS_DEBUG_DAISY_STOP_TENSIX_DEBUG_TENSIX_IN_4_PERF_CNT_L1_DBG_REQ_CNT__1056 {DEBUG_BUS_DEBUG_DAISY_STOP_TENSIX_DEBUG_TENSIX_IN_4_PERF_CNT_L1_DBG_REQ_CNT__1056_WORD, 0, 0xFFFFFFFF}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_4 alias: perf_cnt_l1_dbg_dual note: 7 and 6
//  name: perf_cnt_l1_dbg alias: perf_cnt note: Low counter
//  name: ref_cnt
#define DEBUG_BUS_DEBUG_DAISY_STOP_TENSIX_DEBUG_TENSIX_IN_4_PERF_CNT_L1_DBG_REF_CNT__1024_WORD {8, 7, 0, 0, 1, 0}
#define DEBUG_BUS_DEBUG_DAISY_STOP_TENSIX_DEBUG_TENSIX_IN_4_PERF_CNT_L1_DBG_REF_CNT__1024 {DEBUG_BUS_DEBUG_DAISY_STOP_TENSIX_DEBUG_TENSIX_IN_4_PERF_CNT_L1_DBG_REF_CNT__1024_WORD, 0, 0xFFFFFFFF}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_3 alias: perf_cnt_l1_dbg_dual note: 5 and 4
//  name: perf_cnt_l1_dbg alias: perf_cnt note: High counter
//  name: stall_cnt
#define DEBUG_BUS_DEBUG_DAISY_STOP_TENSIX_DEBUG_TENSIX_IN_3_PERF_CNT_L1_DBG_STALL_CNT__992_WORD {7, 7, 6, 0, 1, 0}
#define DEBUG_BUS_DEBUG_DAISY_STOP_TENSIX_DEBUG_TENSIX_IN_3_PERF_CNT_L1_DBG_STALL_CNT__992 {DEBUG_BUS_DEBUG_DAISY_STOP_TENSIX_DEBUG_TENSIX_IN_3_PERF_CNT_L1_DBG_STALL_CNT__992_WORD, 0, 0xFFFFFFFF}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_3 alias: perf_cnt_l1_dbg_dual note: 5 and 4
//  name: perf_cnt_l1_dbg alias: perf_cnt note: High counter
//  name: grant_cnt
#define DEBUG_BUS_DEBUG_DAISY_STOP_TENSIX_DEBUG_TENSIX_IN_3_PERF_CNT_L1_DBG_GRANT_CNT__960_WORD {7, 7, 4, 0, 1, 0}
#define DEBUG_BUS_DEBUG_DAISY_STOP_TENSIX_DEBUG_TENSIX_IN_3_PERF_CNT_L1_DBG_GRANT_CNT__960 {DEBUG_BUS_DEBUG_DAISY_STOP_TENSIX_DEBUG_TENSIX_IN_3_PERF_CNT_L1_DBG_GRANT_CNT__960_WORD, 0, 0xFFFFFFFF}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_3 alias: perf_cnt_l1_dbg_dual note: 5 and 4
//  name: perf_cnt_l1_dbg alias: perf_cnt note: High counter
//  name: req_cnt
#define DEBUG_BUS_DEBUG_DAISY_STOP_TENSIX_DEBUG_TENSIX_IN_3_PERF_CNT_L1_DBG_REQ_CNT__928_WORD {7, 7, 2, 0, 1, 0}
#define DEBUG_BUS_DEBUG_DAISY_STOP_TENSIX_DEBUG_TENSIX_IN_3_PERF_CNT_L1_DBG_REQ_CNT__928 {DEBUG_BUS_DEBUG_DAISY_STOP_TENSIX_DEBUG_TENSIX_IN_3_PERF_CNT_L1_DBG_REQ_CNT__928_WORD, 0, 0xFFFFFFFF}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_3 alias: perf_cnt_l1_dbg_dual note: 5 and 4
//  name: perf_cnt_l1_dbg alias: perf_cnt note: High counter
//  name: ref_cnt
#define DEBUG_BUS_DEBUG_DAISY_STOP_TENSIX_DEBUG_TENSIX_IN_3_PERF_CNT_L1_DBG_REF_CNT__896_WORD {7, 7, 0, 0, 1, 0}
#define DEBUG_BUS_DEBUG_DAISY_STOP_TENSIX_DEBUG_TENSIX_IN_3_PERF_CNT_L1_DBG_REF_CNT__896 {DEBUG_BUS_DEBUG_DAISY_STOP_TENSIX_DEBUG_TENSIX_IN_3_PERF_CNT_L1_DBG_REF_CNT__896_WORD, 0, 0xFFFFFFFF}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_3 alias: perf_cnt_l1_dbg_dual note: 5 and 4
//  name: perf_cnt_l1_dbg alias: perf_cnt note: Low counter
//  name: stall_cnt
#define DEBUG_BUS_DEBUG_DAISY_STOP_TENSIX_DEBUG_TENSIX_IN_3_PERF_CNT_L1_DBG_STALL_CNT__864_WORD {6, 7, 6, 0, 1, 0}
#define DEBUG_BUS_DEBUG_DAISY_STOP_TENSIX_DEBUG_TENSIX_IN_3_PERF_CNT_L1_DBG_STALL_CNT__864 {DEBUG_BUS_DEBUG_DAISY_STOP_TENSIX_DEBUG_TENSIX_IN_3_PERF_CNT_L1_DBG_STALL_CNT__864_WORD, 0, 0xFFFFFFFF}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_3 alias: perf_cnt_l1_dbg_dual note: 5 and 4
//  name: perf_cnt_l1_dbg alias: perf_cnt note: Low counter
//  name: grant_cnt
#define DEBUG_BUS_DEBUG_DAISY_STOP_TENSIX_DEBUG_TENSIX_IN_3_PERF_CNT_L1_DBG_GRANT_CNT__832_WORD {6, 7, 4, 0, 1, 0}
#define DEBUG_BUS_DEBUG_DAISY_STOP_TENSIX_DEBUG_TENSIX_IN_3_PERF_CNT_L1_DBG_GRANT_CNT__832 {DEBUG_BUS_DEBUG_DAISY_STOP_TENSIX_DEBUG_TENSIX_IN_3_PERF_CNT_L1_DBG_GRANT_CNT__832_WORD, 0, 0xFFFFFFFF}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_3 alias: perf_cnt_l1_dbg_dual note: 5 and 4
//  name: perf_cnt_l1_dbg alias: perf_cnt note: Low counter
//  name: req_cnt
#define DEBUG_BUS_DEBUG_DAISY_STOP_TENSIX_DEBUG_TENSIX_IN_3_PERF_CNT_L1_DBG_REQ_CNT__800_WORD {6, 7, 2, 0, 1, 0}
#define DEBUG_BUS_DEBUG_DAISY_STOP_TENSIX_DEBUG_TENSIX_IN_3_PERF_CNT_L1_DBG_REQ_CNT__800 {DEBUG_BUS_DEBUG_DAISY_STOP_TENSIX_DEBUG_TENSIX_IN_3_PERF_CNT_L1_DBG_REQ_CNT__800_WORD, 0, 0xFFFFFFFF}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_3 alias: perf_cnt_l1_dbg_dual note: 5 and 4
//  name: perf_cnt_l1_dbg alias: perf_cnt note: Low counter
//  name: ref_cnt
#define DEBUG_BUS_DEBUG_DAISY_STOP_TENSIX_DEBUG_TENSIX_IN_3_PERF_CNT_L1_DBG_REF_CNT__768_WORD {6, 7, 0, 0, 1, 0}
#define DEBUG_BUS_DEBUG_DAISY_STOP_TENSIX_DEBUG_TENSIX_IN_3_PERF_CNT_L1_DBG_REF_CNT__768 {DEBUG_BUS_DEBUG_DAISY_STOP_TENSIX_DEBUG_TENSIX_IN_3_PERF_CNT_L1_DBG_REF_CNT__768_WORD, 0, 0xFFFFFFFF}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_2 alias: perf_cnt_l1_dbg_dual note: 3 and 2
//  name: perf_cnt_l1_dbg alias: perf_cnt note: High counter
//  name: stall_cnt
#define DEBUG_BUS_DEBUG_DAISY_STOP_TENSIX_DEBUG_TENSIX_IN_2_PERF_CNT_L1_DBG_STALL_CNT__736_WORD {5, 7, 6, 0, 1, 0}
#define DEBUG_BUS_DEBUG_DAISY_STOP_TENSIX_DEBUG_TENSIX_IN_2_PERF_CNT_L1_DBG_STALL_CNT__736 {DEBUG_BUS_DEBUG_DAISY_STOP_TENSIX_DEBUG_TENSIX_IN_2_PERF_CNT_L1_DBG_STALL_CNT__736_WORD, 0, 0xFFFFFFFF}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_2 alias: perf_cnt_l1_dbg_dual note: 3 and 2
//  name: perf_cnt_l1_dbg alias: perf_cnt note: High counter
//  name: grant_cnt
#define DEBUG_BUS_DEBUG_DAISY_STOP_TENSIX_DEBUG_TENSIX_IN_2_PERF_CNT_L1_DBG_GRANT_CNT__704_WORD {5, 7, 4, 0, 1, 0}
#define DEBUG_BUS_DEBUG_DAISY_STOP_TENSIX_DEBUG_TENSIX_IN_2_PERF_CNT_L1_DBG_GRANT_CNT__704 {DEBUG_BUS_DEBUG_DAISY_STOP_TENSIX_DEBUG_TENSIX_IN_2_PERF_CNT_L1_DBG_GRANT_CNT__704_WORD, 0, 0xFFFFFFFF}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_2 alias: perf_cnt_l1_dbg_dual note: 3 and 2
//  name: perf_cnt_l1_dbg alias: perf_cnt note: High counter
//  name: req_cnt
#define DEBUG_BUS_DEBUG_DAISY_STOP_TENSIX_DEBUG_TENSIX_IN_2_PERF_CNT_L1_DBG_REQ_CNT__672_WORD {5, 7, 2, 0, 1, 0}
#define DEBUG_BUS_DEBUG_DAISY_STOP_TENSIX_DEBUG_TENSIX_IN_2_PERF_CNT_L1_DBG_REQ_CNT__672 {DEBUG_BUS_DEBUG_DAISY_STOP_TENSIX_DEBUG_TENSIX_IN_2_PERF_CNT_L1_DBG_REQ_CNT__672_WORD, 0, 0xFFFFFFFF}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_2 alias: perf_cnt_l1_dbg_dual note: 3 and 2
//  name: perf_cnt_l1_dbg alias: perf_cnt note: High counter
//  name: ref_cnt
#define DEBUG_BUS_DEBUG_DAISY_STOP_TENSIX_DEBUG_TENSIX_IN_2_PERF_CNT_L1_DBG_REF_CNT__640_WORD {5, 7, 0, 0, 1, 0}
#define DEBUG_BUS_DEBUG_DAISY_STOP_TENSIX_DEBUG_TENSIX_IN_2_PERF_CNT_L1_DBG_REF_CNT__640 {DEBUG_BUS_DEBUG_DAISY_STOP_TENSIX_DEBUG_TENSIX_IN_2_PERF_CNT_L1_DBG_REF_CNT__640_WORD, 0, 0xFFFFFFFF}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_2 alias: perf_cnt_l1_dbg_dual note: 3 and 2
//  name: perf_cnt_l1_dbg alias: perf_cnt note: Low counter
//  name: stall_cnt
#define DEBUG_BUS_DEBUG_DAISY_STOP_TENSIX_DEBUG_TENSIX_IN_2_PERF_CNT_L1_DBG_STALL_CNT__608_WORD {4, 7, 6, 0, 1, 0}
#define DEBUG_BUS_DEBUG_DAISY_STOP_TENSIX_DEBUG_TENSIX_IN_2_PERF_CNT_L1_DBG_STALL_CNT__608 {DEBUG_BUS_DEBUG_DAISY_STOP_TENSIX_DEBUG_TENSIX_IN_2_PERF_CNT_L1_DBG_STALL_CNT__608_WORD, 0, 0xFFFFFFFF}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_2 alias: perf_cnt_l1_dbg_dual note: 3 and 2
//  name: perf_cnt_l1_dbg alias: perf_cnt note: Low counter
//  name: grant_cnt
#define DEBUG_BUS_DEBUG_DAISY_STOP_TENSIX_DEBUG_TENSIX_IN_2_PERF_CNT_L1_DBG_GRANT_CNT__576_WORD {4, 7, 4, 0, 1, 0}
#define DEBUG_BUS_DEBUG_DAISY_STOP_TENSIX_DEBUG_TENSIX_IN_2_PERF_CNT_L1_DBG_GRANT_CNT__576 {DEBUG_BUS_DEBUG_DAISY_STOP_TENSIX_DEBUG_TENSIX_IN_2_PERF_CNT_L1_DBG_GRANT_CNT__576_WORD, 0, 0xFFFFFFFF}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_2 alias: perf_cnt_l1_dbg_dual note: 3 and 2
//  name: perf_cnt_l1_dbg alias: perf_cnt note: Low counter
//  name: req_cnt
#define DEBUG_BUS_DEBUG_DAISY_STOP_TENSIX_DEBUG_TENSIX_IN_2_PERF_CNT_L1_DBG_REQ_CNT__544_WORD {4, 7, 2, 0, 1, 0}
#define DEBUG_BUS_DEBUG_DAISY_STOP_TENSIX_DEBUG_TENSIX_IN_2_PERF_CNT_L1_DBG_REQ_CNT__544 {DEBUG_BUS_DEBUG_DAISY_STOP_TENSIX_DEBUG_TENSIX_IN_2_PERF_CNT_L1_DBG_REQ_CNT__544_WORD, 0, 0xFFFFFFFF}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_2 alias: perf_cnt_l1_dbg_dual note: 3 and 2
//  name: perf_cnt_l1_dbg alias: perf_cnt note: Low counter
//  name: ref_cnt
#define DEBUG_BUS_DEBUG_DAISY_STOP_TENSIX_DEBUG_TENSIX_IN_2_PERF_CNT_L1_DBG_REF_CNT__512_WORD {4, 7, 0, 0, 1, 0}
#define DEBUG_BUS_DEBUG_DAISY_STOP_TENSIX_DEBUG_TENSIX_IN_2_PERF_CNT_L1_DBG_REF_CNT__512 {DEBUG_BUS_DEBUG_DAISY_STOP_TENSIX_DEBUG_TENSIX_IN_2_PERF_CNT_L1_DBG_REF_CNT__512_WORD, 0, 0xFFFFFFFF}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_1 alias: perf_cnt_l1_dbg_dual note: 1 and 0
//  name: perf_cnt_l1_dbg alias: perf_cnt note: High counter
//  name: stall_cnt
#define DEBUG_BUS_DEBUG_DAISY_STOP_TENSIX_DEBUG_TENSIX_IN_1_PERF_CNT_L1_DBG_STALL_CNT__480_WORD {3, 7, 6, 0, 1, 0}
#define DEBUG_BUS_DEBUG_DAISY_STOP_TENSIX_DEBUG_TENSIX_IN_1_PERF_CNT_L1_DBG_STALL_CNT__480 {DEBUG_BUS_DEBUG_DAISY_STOP_TENSIX_DEBUG_TENSIX_IN_1_PERF_CNT_L1_DBG_STALL_CNT__480_WORD, 0, 0xFFFFFFFF}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_1 alias: perf_cnt_l1_dbg_dual note: 1 and 0
//  name: perf_cnt_l1_dbg alias: perf_cnt note: High counter
//  name: grant_cnt
#define DEBUG_BUS_DEBUG_DAISY_STOP_TENSIX_DEBUG_TENSIX_IN_1_PERF_CNT_L1_DBG_GRANT_CNT__448_WORD {3, 7, 4, 0, 1, 0}
#define DEBUG_BUS_DEBUG_DAISY_STOP_TENSIX_DEBUG_TENSIX_IN_1_PERF_CNT_L1_DBG_GRANT_CNT__448 {DEBUG_BUS_DEBUG_DAISY_STOP_TENSIX_DEBUG_TENSIX_IN_1_PERF_CNT_L1_DBG_GRANT_CNT__448_WORD, 0, 0xFFFFFFFF}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_1 alias: perf_cnt_l1_dbg_dual note: 1 and 0
//  name: perf_cnt_l1_dbg alias: perf_cnt note: High counter
//  name: req_cnt
#define DEBUG_BUS_DEBUG_DAISY_STOP_TENSIX_DEBUG_TENSIX_IN_1_PERF_CNT_L1_DBG_REQ_CNT__416_WORD {3, 7, 2, 0, 1, 0}
#define DEBUG_BUS_DEBUG_DAISY_STOP_TENSIX_DEBUG_TENSIX_IN_1_PERF_CNT_L1_DBG_REQ_CNT__416 {DEBUG_BUS_DEBUG_DAISY_STOP_TENSIX_DEBUG_TENSIX_IN_1_PERF_CNT_L1_DBG_REQ_CNT__416_WORD, 0, 0xFFFFFFFF}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_1 alias: perf_cnt_l1_dbg_dual note: 1 and 0
//  name: perf_cnt_l1_dbg alias: perf_cnt note: High counter
//  name: ref_cnt
#define DEBUG_BUS_DEBUG_DAISY_STOP_TENSIX_DEBUG_TENSIX_IN_1_PERF_CNT_L1_DBG_REF_CNT__384_WORD {3, 7, 0, 0, 1, 0}
#define DEBUG_BUS_DEBUG_DAISY_STOP_TENSIX_DEBUG_TENSIX_IN_1_PERF_CNT_L1_DBG_REF_CNT__384 {DEBUG_BUS_DEBUG_DAISY_STOP_TENSIX_DEBUG_TENSIX_IN_1_PERF_CNT_L1_DBG_REF_CNT__384_WORD, 0, 0xFFFFFFFF}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_1 alias: perf_cnt_l1_dbg_dual note: 1 and 0
//  name: perf_cnt_l1_dbg alias: perf_cnt note: Low counter
//  name: stall_cnt
#define DEBUG_BUS_DEBUG_DAISY_STOP_TENSIX_DEBUG_TENSIX_IN_1_PERF_CNT_L1_DBG_STALL_CNT__352_WORD {2, 7, 6, 0, 1, 0}
#define DEBUG_BUS_DEBUG_DAISY_STOP_TENSIX_DEBUG_TENSIX_IN_1_PERF_CNT_L1_DBG_STALL_CNT__352 {DEBUG_BUS_DEBUG_DAISY_STOP_TENSIX_DEBUG_TENSIX_IN_1_PERF_CNT_L1_DBG_STALL_CNT__352_WORD, 0, 0xFFFFFFFF}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_1 alias: perf_cnt_l1_dbg_dual note: 1 and 0
//  name: perf_cnt_l1_dbg alias: perf_cnt note: Low counter
//  name: grant_cnt
#define DEBUG_BUS_DEBUG_DAISY_STOP_TENSIX_DEBUG_TENSIX_IN_1_PERF_CNT_L1_DBG_GRANT_CNT__320_WORD {2, 7, 4, 0, 1, 0}
#define DEBUG_BUS_DEBUG_DAISY_STOP_TENSIX_DEBUG_TENSIX_IN_1_PERF_CNT_L1_DBG_GRANT_CNT__320 {DEBUG_BUS_DEBUG_DAISY_STOP_TENSIX_DEBUG_TENSIX_IN_1_PERF_CNT_L1_DBG_GRANT_CNT__320_WORD, 0, 0xFFFFFFFF}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_1 alias: perf_cnt_l1_dbg_dual note: 1 and 0
//  name: perf_cnt_l1_dbg alias: perf_cnt note: Low counter
//  name: req_cnt
#define DEBUG_BUS_DEBUG_DAISY_STOP_TENSIX_DEBUG_TENSIX_IN_1_PERF_CNT_L1_DBG_REQ_CNT__288_WORD {2, 7, 2, 0, 1, 0}
#define DEBUG_BUS_DEBUG_DAISY_STOP_TENSIX_DEBUG_TENSIX_IN_1_PERF_CNT_L1_DBG_REQ_CNT__288 {DEBUG_BUS_DEBUG_DAISY_STOP_TENSIX_DEBUG_TENSIX_IN_1_PERF_CNT_L1_DBG_REQ_CNT__288_WORD, 0, 0xFFFFFFFF}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_1 alias: perf_cnt_l1_dbg_dual note: 1 and 0
//  name: perf_cnt_l1_dbg alias: perf_cnt note: Low counter
//  name: ref_cnt
#define DEBUG_BUS_DEBUG_DAISY_STOP_TENSIX_DEBUG_TENSIX_IN_1_PERF_CNT_L1_DBG_REF_CNT__256_WORD {2, 7, 0, 0, 1, 0}
#define DEBUG_BUS_DEBUG_DAISY_STOP_TENSIX_DEBUG_TENSIX_IN_1_PERF_CNT_L1_DBG_REF_CNT__256 {DEBUG_BUS_DEBUG_DAISY_STOP_TENSIX_DEBUG_TENSIX_IN_1_PERF_CNT_L1_DBG_REF_CNT__256_WORD, 0, 0xFFFFFFFF}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_0
//  name: risc_wrapper_debug_bus alias: risc_wrapper_debug_bus note: BRISC
//  name: o_par_err_risc_localmem
#define DEBUG_BUS_RISC_WRAPPER_DEBUG_BUS_O_PAR_ERR_RISC_LOCALMEM_WORD {1, 7, 6, 0, 1, 0}
#define DEBUG_BUS_RISC_WRAPPER_DEBUG_BUS_O_PAR_ERR_RISC_LOCALMEM {DEBUG_BUS_RISC_WRAPPER_DEBUG_BUS_O_PAR_ERR_RISC_LOCALMEM_WORD, 31, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_0
//  name: risc_wrapper_debug_bus alias: risc_wrapper_debug_bus note: BRISC
//  name: i_mailbox_rden
#define DEBUG_BUS_RISC_WRAPPER_DEBUG_BUS_I_MAILBOX_RDEN_WORD {1, 7, 6, 0, 1, 0}
#define DEBUG_BUS_RISC_WRAPPER_DEBUG_BUS_I_MAILBOX_RDEN {DEBUG_BUS_RISC_WRAPPER_DEBUG_BUS_I_MAILBOX_RDEN_WORD, 27, 0xF}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_0
//  name: risc_wrapper_debug_bus alias: risc_wrapper_debug_bus note: BRISC
//  name: i_mailbox_rd_type
#define DEBUG_BUS_RISC_WRAPPER_DEBUG_BUS_I_MAILBOX_RD_TYPE_WORD {1, 7, 6, 0, 1, 0}
#define DEBUG_BUS_RISC_WRAPPER_DEBUG_BUS_I_MAILBOX_RD_TYPE {DEBUG_BUS_RISC_WRAPPER_DEBUG_BUS_I_MAILBOX_RD_TYPE_WORD, 23, 0xF}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_0
//  name: risc_wrapper_debug_bus alias: risc_wrapper_debug_bus note: BRISC
//  name: o_mailbox_rd_req_ready
#define DEBUG_BUS_RISC_WRAPPER_DEBUG_BUS_O_MAILBOX_RD_REQ_READY_WORD {1, 7, 6, 0, 1, 0}
#define DEBUG_BUS_RISC_WRAPPER_DEBUG_BUS_O_MAILBOX_RD_REQ_READY {DEBUG_BUS_RISC_WRAPPER_DEBUG_BUS_O_MAILBOX_RD_REQ_READY_WORD, 19, 0xF}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_0
//  name: risc_wrapper_debug_bus alias: risc_wrapper_debug_bus note: BRISC
//  name: o_mailbox_rdvalid
#define DEBUG_BUS_RISC_WRAPPER_DEBUG_BUS_O_MAILBOX_RDVALID_WORD {1, 7, 6, 0, 1, 0}
#define DEBUG_BUS_RISC_WRAPPER_DEBUG_BUS_O_MAILBOX_RDVALID {DEBUG_BUS_RISC_WRAPPER_DEBUG_BUS_O_MAILBOX_RDVALID_WORD, 15, 0xF}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_0
//  name: risc_wrapper_debug_bus alias: risc_wrapper_debug_bus note: BRISC
//  name: o_mailbox_rddata
#define DEBUG_BUS_RISC_WRAPPER_DEBUG_BUS_O_MAILBOX_RDDATA__6_0_WORD {1, 7, 6, 0, 1, 0}
#define DEBUG_BUS_RISC_WRAPPER_DEBUG_BUS_O_MAILBOX_RDDATA__6_0 {DEBUG_BUS_RISC_WRAPPER_DEBUG_BUS_O_MAILBOX_RDDATA__6_0_WORD, 8, 0x7F}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_0
//  name: risc_wrapper_debug_bus alias: risc_wrapper_debug_bus note: BRISC
//  name: intf_wrack_brisc
#define DEBUG_BUS_RISC_WRAPPER_DEBUG_BUS_INTF_WRACK_BRISC__10_0_WORD {1, 7, 0, 0, 1, 0}
#define DEBUG_BUS_RISC_WRAPPER_DEBUG_BUS_INTF_WRACK_BRISC__10_0 {DEBUG_BUS_RISC_WRAPPER_DEBUG_BUS_INTF_WRACK_BRISC__10_0_WORD, 21, 0x7FF}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_0
//  name: risc_wrapper_debug_bus alias: risc_wrapper_debug_bus note: BRISC
//  name: intf_wrack_brisc
#define DEBUG_BUS_RISC_WRAPPER_DEBUG_BUS_INTF_WRACK_BRISC__16_11_WORD {1, 7, 2, 0, 1, 0}
#define DEBUG_BUS_RISC_WRAPPER_DEBUG_BUS_INTF_WRACK_BRISC__16_11 {DEBUG_BUS_RISC_WRAPPER_DEBUG_BUS_INTF_WRACK_BRISC__16_11_WORD, 0, 0x3F}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_0
//  name: risc_wrapper_debug_bus alias: risc_wrapper_debug_bus note: BRISC
//  name: dmem_tensix_rden
#define DEBUG_BUS_RISC_WRAPPER_DEBUG_BUS_DMEM_TENSIX_RDEN_WORD {1, 7, 0, 0, 1, 0}
#define DEBUG_BUS_RISC_WRAPPER_DEBUG_BUS_DMEM_TENSIX_RDEN {DEBUG_BUS_RISC_WRAPPER_DEBUG_BUS_DMEM_TENSIX_RDEN_WORD, 20, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_0
//  name: risc_wrapper_debug_bus alias: risc_wrapper_debug_bus note: BRISC
//  name: dmem_tensix_wren
#define DEBUG_BUS_RISC_WRAPPER_DEBUG_BUS_DMEM_TENSIX_WREN_WORD {1, 7, 0, 0, 1, 0}
#define DEBUG_BUS_RISC_WRAPPER_DEBUG_BUS_DMEM_TENSIX_WREN {DEBUG_BUS_RISC_WRAPPER_DEBUG_BUS_DMEM_TENSIX_WREN_WORD, 19, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_0
//  name: risc_wrapper_debug_bus alias: risc_wrapper_debug_bus note: BRISC
//  name: icache_req_fifo_full
#define DEBUG_BUS_RISC_WRAPPER_DEBUG_BUS_ICACHE_REQ_FIFO_FULL_WORD {1, 7, 0, 0, 1, 0}
#define DEBUG_BUS_RISC_WRAPPER_DEBUG_BUS_ICACHE_REQ_FIFO_FULL {DEBUG_BUS_RISC_WRAPPER_DEBUG_BUS_ICACHE_REQ_FIFO_FULL_WORD, 1, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_0
//  name: risc_wrapper_debug_bus alias: risc_wrapper_debug_bus note: BRISC
//  name: icache_req_fifo_empty
#define DEBUG_BUS_RISC_WRAPPER_DEBUG_BUS_ICACHE_REQ_FIFO_EMPTY_WORD {1, 7, 0, 0, 1, 0}
#define DEBUG_BUS_RISC_WRAPPER_DEBUG_BUS_ICACHE_REQ_FIFO_EMPTY {DEBUG_BUS_RISC_WRAPPER_DEBUG_BUS_ICACHE_REQ_FIFO_EMPTY_WORD, 0, 0x1}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_0
//  name: perf_cnt_fpu_dbg_0 alias: perf_cnt
//  name: stall_cnt
#define DEBUG_BUS_PERF_CNT_FPU_DBG_0_STALL_CNT_WORD {0, 7, 6, 0, 1, 0}
#define DEBUG_BUS_PERF_CNT_FPU_DBG_0_STALL_CNT {DEBUG_BUS_PERF_CNT_FPU_DBG_0_STALL_CNT_WORD, 0, 0xFFFFFFFF}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_0
//  name: perf_cnt_fpu_dbg_0 alias: perf_cnt
//  name: grant_cnt
#define DEBUG_BUS_PERF_CNT_FPU_DBG_0_GRANT_CNT_WORD {0, 7, 4, 0, 1, 0}
#define DEBUG_BUS_PERF_CNT_FPU_DBG_0_GRANT_CNT {DEBUG_BUS_PERF_CNT_FPU_DBG_0_GRANT_CNT_WORD, 0, 0xFFFFFFFF}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_0
//  name: perf_cnt_fpu_dbg_0 alias: perf_cnt
//  name: req_cnt
#define DEBUG_BUS_PERF_CNT_FPU_DBG_0_REQ_CNT_WORD {0, 7, 2, 0, 1, 0}
#define DEBUG_BUS_PERF_CNT_FPU_DBG_0_REQ_CNT {DEBUG_BUS_PERF_CNT_FPU_DBG_0_REQ_CNT_WORD, 0, 0xFFFFFFFF}

//  location: tt_tensix name: debug_daisy_stop_tensix
//  name: debug_tensix_in_0
//  name: perf_cnt_fpu_dbg_0 alias: perf_cnt
//  name: ref_cnt
#define DEBUG_BUS_PERF_CNT_FPU_DBG_0_REF_CNT_WORD {0, 7, 0, 0, 1, 0}
#define DEBUG_BUS_PERF_CNT_FPU_DBG_0_REF_CNT {DEBUG_BUS_PERF_CNT_FPU_DBG_0_REF_CNT_WORD, 0, 0xFFFFFFFF}

//  location: tt_tensix_with_l1 name: debug_daisy_stop_l1
//  name: debug_tensix_w_l1_in_6
//  name: t_l1_addr
//  name: l1_addr_p41 alias: l1_addr note: Port41
#define DEBUG_BUS_L1_ADDR_P41__14_0_WORD {13, 8, 2, 0, 1, 0}
#define DEBUG_BUS_L1_ADDR_P41__14_0 {DEBUG_BUS_L1_ADDR_P41__14_0_WORD, 17, 0x7FFF}

//  location: tt_tensix_with_l1 name: debug_daisy_stop_l1
//  name: debug_tensix_w_l1_in_6
//  name: t_l1_addr
//  name: l1_addr_p41 alias: l1_addr note: Port41
#define DEBUG_BUS_L1_ADDR_P41__16_15_WORD {13, 8, 4, 0, 1, 0}
#define DEBUG_BUS_L1_ADDR_P41__16_15 {DEBUG_BUS_L1_ADDR_P41__16_15_WORD, 0, 0x3}

//  location: tt_tensix_with_l1 name: debug_daisy_stop_l1
//  name: debug_tensix_w_l1_in_6
//  name: t_l1_addr
//  name: l1_addr_p40 alias: l1_addr note: Port40
#define DEBUG_BUS_L1_ADDR_P40_WORD {13, 8, 2, 0, 1, 0}
#define DEBUG_BUS_L1_ADDR_P40 {DEBUG_BUS_L1_ADDR_P40_WORD, 0, 0x1FFFF}

//  location: tt_tensix_with_l1 name: debug_daisy_stop_l1
//  name: debug_tensix_w_l1_in_6
//  name: t_l1_addr
//  name: l1_addr_p39 alias: l1_addr note: Port39
#define DEBUG_BUS_L1_ADDR_P39_WORD {13, 8, 0, 0, 1, 0}
#define DEBUG_BUS_L1_ADDR_P39 {DEBUG_BUS_L1_ADDR_P39_WORD, 15, 0x1FFFF}

//  location: tt_tensix_with_l1 name: debug_daisy_stop_l1
//  name: debug_tensix_w_l1_in_6
//  name: t_l1_addr
//  name: l1_addr_p38 alias: l1_addr note: Port38
#define DEBUG_BUS_L1_ADDR_P38__16_2_WORD {13, 8, 0, 0, 1, 0}
#define DEBUG_BUS_L1_ADDR_P38__16_2 {DEBUG_BUS_L1_ADDR_P38__16_2_WORD, 0, 0x7FFF}

//  location: tt_tensix_with_l1 name: debug_daisy_stop_l1
//  name: debug_tensix_w_l1_in_5 alias: t_l1_addr
//  name: l1_addr_p38 alias: l1_addr note: Port38
#define DEBUG_BUS_L1_ADDR_P38__1_0_WORD {11, 8, 6, 0, 1, 0}
#define DEBUG_BUS_L1_ADDR_P38__1_0 {DEBUG_BUS_L1_ADDR_P38__1_0_WORD, 30, 0x3}

//  location: tt_tensix_with_l1 name: debug_daisy_stop_l1
//  name: debug_tensix_w_l1_in_5 alias: t_l1_addr
//  name: l1_addr_p37 alias: l1_addr note: Port37
#define DEBUG_BUS_L1_ADDR_P37_WORD {11, 8, 6, 0, 1, 0}
#define DEBUG_BUS_L1_ADDR_P37 {DEBUG_BUS_L1_ADDR_P37_WORD, 13, 0x1FFFF}

//  location: tt_tensix_with_l1 name: debug_daisy_stop_l1
//  name: debug_tensix_w_l1_in_5 alias: t_l1_addr
//  name: l1_addr_p36 alias: l1_addr note: Port36
#define DEBUG_BUS_L1_ADDR_P36__3_0_WORD {11, 8, 4, 0, 1, 0}
#define DEBUG_BUS_L1_ADDR_P36__3_0 {DEBUG_BUS_L1_ADDR_P36__3_0_WORD, 28, 0xF}

//  location: tt_tensix_with_l1 name: debug_daisy_stop_l1
//  name: debug_tensix_w_l1_in_5 alias: t_l1_addr
//  name: l1_addr_p36 alias: l1_addr note: Port36
#define DEBUG_BUS_L1_ADDR_P36__16_4_WORD {11, 8, 6, 0, 1, 0}
#define DEBUG_BUS_L1_ADDR_P36__16_4 {DEBUG_BUS_L1_ADDR_P36__16_4_WORD, 0, 0x1FFF}

//  location: tt_tensix_with_l1 name: debug_daisy_stop_l1
//  name: debug_tensix_w_l1_in_5 alias: t_l1_addr
//  name: l1_addr_p35 alias: l1_addr note: Port35
#define DEBUG_BUS_L1_ADDR_P35_WORD {11, 8, 4, 0, 1, 0}
#define DEBUG_BUS_L1_ADDR_P35 {DEBUG_BUS_L1_ADDR_P35_WORD, 11, 0x1FFFF}

//  location: tt_tensix_with_l1 name: debug_daisy_stop_l1
//  name: debug_tensix_w_l1_in_5 alias: t_l1_addr
//  name: l1_addr_p34 alias: l1_addr note: Port34
#define DEBUG_BUS_L1_ADDR_P34__5_0_WORD {11, 8, 2, 0, 1, 0}
#define DEBUG_BUS_L1_ADDR_P34__5_0 {DEBUG_BUS_L1_ADDR_P34__5_0_WORD, 26, 0x3F}

//  location: tt_tensix_with_l1 name: debug_daisy_stop_l1
//  name: debug_tensix_w_l1_in_5 alias: t_l1_addr
//  name: l1_addr_p34 alias: l1_addr note: Port34
#define DEBUG_BUS_L1_ADDR_P34__16_6_WORD {11, 8, 4, 0, 1, 0}
#define DEBUG_BUS_L1_ADDR_P34__16_6 {DEBUG_BUS_L1_ADDR_P34__16_6_WORD, 0, 0x7FF}

//  location: tt_tensix_with_l1 name: debug_daisy_stop_l1
//  name: debug_tensix_w_l1_in_5 alias: t_l1_addr
//  name: l1_addr_p33 alias: l1_addr note: Port33
#define DEBUG_BUS_L1_ADDR_P33_WORD {11, 8, 2, 0, 1, 0}
#define DEBUG_BUS_L1_ADDR_P33 {DEBUG_BUS_L1_ADDR_P33_WORD, 9, 0x1FFFF}

//  location: tt_tensix_with_l1 name: debug_daisy_stop_l1
//  name: debug_tensix_w_l1_in_5 alias: t_l1_addr
//  name: l1_addr_p32 alias: l1_addr note: Port32
#define DEBUG_BUS_L1_ADDR_P32__7_0_WORD {11, 8, 0, 0, 1, 0}
#define DEBUG_BUS_L1_ADDR_P32__7_0 {DEBUG_BUS_L1_ADDR_P32__7_0_WORD, 24, 0xFF}

//  location: tt_tensix_with_l1 name: debug_daisy_stop_l1
//  name: debug_tensix_w_l1_in_5 alias: t_l1_addr
//  name: l1_addr_p32 alias: l1_addr note: Port32
#define DEBUG_BUS_L1_ADDR_P32__16_8_WORD {11, 8, 2, 0, 1, 0}
#define DEBUG_BUS_L1_ADDR_P32__16_8 {DEBUG_BUS_L1_ADDR_P32__16_8_WORD, 0, 0x1FF}

//  location: tt_tensix_with_l1 name: debug_daisy_stop_l1
//  name: debug_tensix_w_l1_in_5 alias: t_l1_addr
//  name: l1_addr_p31 alias: l1_addr note: Port31
#define DEBUG_BUS_L1_ADDR_P31_WORD {11, 8, 0, 0, 1, 0}
#define DEBUG_BUS_L1_ADDR_P31 {DEBUG_BUS_L1_ADDR_P31_WORD, 7, 0x1FFFF}

//  location: tt_tensix_with_l1 name: debug_daisy_stop_l1
//  name: debug_tensix_w_l1_in_5 alias: t_l1_addr
//  name: l1_addr_p30 alias: l1_addr note: Port30
#define DEBUG_BUS_L1_ADDR_P30__9_0_WORD {10, 8, 6, 0, 1, 0}
#define DEBUG_BUS_L1_ADDR_P30__9_0 {DEBUG_BUS_L1_ADDR_P30__9_0_WORD, 22, 0x3FF}

//  location: tt_tensix_with_l1 name: debug_daisy_stop_l1
//  name: debug_tensix_w_l1_in_5 alias: t_l1_addr
//  name: l1_addr_p30 alias: l1_addr note: Port30
#define DEBUG_BUS_L1_ADDR_P30__16_10_WORD {11, 8, 0, 0, 1, 0}
#define DEBUG_BUS_L1_ADDR_P30__16_10 {DEBUG_BUS_L1_ADDR_P30__16_10_WORD, 0, 0x7F}

//  location: tt_tensix_with_l1 name: debug_daisy_stop_l1
//  name: debug_tensix_w_l1_in_5 alias: t_l1_addr
//  name: l1_addr_p29 alias: l1_addr note: Port29
#define DEBUG_BUS_L1_ADDR_P29_WORD {10, 8, 6, 0, 1, 0}
#define DEBUG_BUS_L1_ADDR_P29 {DEBUG_BUS_L1_ADDR_P29_WORD, 5, 0x1FFFF}

//  location: tt_tensix_with_l1 name: debug_daisy_stop_l1
//  name: debug_tensix_w_l1_in_5 alias: t_l1_addr
//  name: l1_addr_p28 alias: l1_addr note: Port28
#define DEBUG_BUS_L1_ADDR_P28__11_0_WORD {10, 8, 4, 0, 1, 0}
#define DEBUG_BUS_L1_ADDR_P28__11_0 {DEBUG_BUS_L1_ADDR_P28__11_0_WORD, 20, 0xFFF}

//  location: tt_tensix_with_l1 name: debug_daisy_stop_l1
//  name: debug_tensix_w_l1_in_5 alias: t_l1_addr
//  name: l1_addr_p28 alias: l1_addr note: Port28
#define DEBUG_BUS_L1_ADDR_P28__16_12_WORD {10, 8, 6, 0, 1, 0}
#define DEBUG_BUS_L1_ADDR_P28__16_12 {DEBUG_BUS_L1_ADDR_P28__16_12_WORD, 0, 0x1F}

//  location: tt_tensix_with_l1 name: debug_daisy_stop_l1
//  name: debug_tensix_w_l1_in_5 alias: t_l1_addr
//  name: l1_addr_p27 alias: l1_addr note: Port27
#define DEBUG_BUS_L1_ADDR_P27_WORD {10, 8, 4, 0, 1, 0}
#define DEBUG_BUS_L1_ADDR_P27 {DEBUG_BUS_L1_ADDR_P27_WORD, 3, 0x1FFFF}

//  location: tt_tensix_with_l1 name: debug_daisy_stop_l1
//  name: debug_tensix_w_l1_in_5 alias: t_l1_addr
//  name: l1_addr_p26 alias: l1_addr note: Port26
#define DEBUG_BUS_L1_ADDR_P26__13_0_WORD {10, 8, 2, 0, 1, 0}
#define DEBUG_BUS_L1_ADDR_P26__13_0 {DEBUG_BUS_L1_ADDR_P26__13_0_WORD, 18, 0x3FFF}

//  location: tt_tensix_with_l1 name: debug_daisy_stop_l1
//  name: debug_tensix_w_l1_in_5 alias: t_l1_addr
//  name: l1_addr_p26 alias: l1_addr note: Port26
#define DEBUG_BUS_L1_ADDR_P26__16_14_WORD {10, 8, 4, 0, 1, 0}
#define DEBUG_BUS_L1_ADDR_P26__16_14 {DEBUG_BUS_L1_ADDR_P26__16_14_WORD, 0, 0x7}

//  location: tt_tensix_with_l1 name: debug_daisy_stop_l1
//  name: debug_tensix_w_l1_in_5 alias: t_l1_addr
//  name: l1_addr_p25 alias: l1_addr note: Port25
#define DEBUG_BUS_L1_ADDR_P25_WORD {10, 8, 2, 0, 1, 0}
#define DEBUG_BUS_L1_ADDR_P25 {DEBUG_BUS_L1_ADDR_P25_WORD, 1, 0x1FFFF}

//  location: tt_tensix_with_l1 name: debug_daisy_stop_l1
//  name: debug_tensix_w_l1_in_5 alias: t_l1_addr
//  name: l1_addr_p24 alias: l1_addr note: Port24
#define DEBUG_BUS_L1_ADDR_P24__15_0_WORD {10, 8, 0, 0, 1, 0}
#define DEBUG_BUS_L1_ADDR_P24__15_0 {DEBUG_BUS_L1_ADDR_P24__15_0_WORD, 16, 0xFFFF}

//  location: tt_tensix_with_l1 name: debug_daisy_stop_l1
//  name: debug_tensix_w_l1_in_5 alias: t_l1_addr
//  name: l1_addr_p24 alias: l1_addr note: Port24
#define DEBUG_BUS_L1_ADDR_P24__16_16_WORD {10, 8, 2, 0, 1, 0}
#define DEBUG_BUS_L1_ADDR_P24__16_16 {DEBUG_BUS_L1_ADDR_P24__16_16_WORD, 0, 0x1}

//  location: tt_tensix_with_l1 name: debug_daisy_stop_l1
//  name: debug_tensix_w_l1_in_5 alias: t_l1_addr
//  name: l1_addr_p23 alias: l1_addr note: Port23
#define DEBUG_BUS_L1_ADDR_P23__16_1_WORD {10, 8, 0, 0, 1, 0}
#define DEBUG_BUS_L1_ADDR_P23__16_1 {DEBUG_BUS_L1_ADDR_P23__16_1_WORD, 0, 0xFFFF}

//  location: tt_tensix_with_l1 name: debug_daisy_stop_l1
//  name: debug_tensix_w_l1_in_4 alias: t_l1_addr
//  name: l1_addr_p23 alias: l1_addr note: Port23
#define DEBUG_BUS_L1_ADDR_P23__0_0_WORD {9, 8, 6, 0, 1, 0}
#define DEBUG_BUS_L1_ADDR_P23__0_0 {DEBUG_BUS_L1_ADDR_P23__0_0_WORD, 31, 0x1}

//  location: tt_tensix_with_l1 name: debug_daisy_stop_l1
//  name: debug_tensix_w_l1_in_4 alias: t_l1_addr
//  name: l1_addr_p22 alias: l1_addr note: Port22
#define DEBUG_BUS_L1_ADDR_P22_WORD {9, 8, 6, 0, 1, 0}
#define DEBUG_BUS_L1_ADDR_P22 {DEBUG_BUS_L1_ADDR_P22_WORD, 14, 0x1FFFF}

//  location: tt_tensix_with_l1 name: debug_daisy_stop_l1
//  name: debug_tensix_w_l1_in_4 alias: t_l1_addr
//  name: l1_addr_p21 alias: l1_addr note: Port21
#define DEBUG_BUS_L1_ADDR_P21__2_0_WORD {9, 8, 4, 0, 1, 0}
#define DEBUG_BUS_L1_ADDR_P21__2_0 {DEBUG_BUS_L1_ADDR_P21__2_0_WORD, 29, 0x7}

//  location: tt_tensix_with_l1 name: debug_daisy_stop_l1
//  name: debug_tensix_w_l1_in_4 alias: t_l1_addr
//  name: l1_addr_p21 alias: l1_addr note: Port21
#define DEBUG_BUS_L1_ADDR_P21__16_3_WORD {9, 8, 6, 0, 1, 0}
#define DEBUG_BUS_L1_ADDR_P21__16_3 {DEBUG_BUS_L1_ADDR_P21__16_3_WORD, 0, 0x3FFF}

//  location: tt_tensix_with_l1 name: debug_daisy_stop_l1
//  name: debug_tensix_w_l1_in_4 alias: t_l1_addr
//  name: l1_addr_p20 alias: l1_addr note: Port20
#define DEBUG_BUS_L1_ADDR_P20_WORD {9, 8, 4, 0, 1, 0}
#define DEBUG_BUS_L1_ADDR_P20 {DEBUG_BUS_L1_ADDR_P20_WORD, 12, 0x1FFFF}

//  location: tt_tensix_with_l1 name: debug_daisy_stop_l1
//  name: debug_tensix_w_l1_in_4 alias: t_l1_addr
//  name: l1_addr_p19 alias: l1_addr note: Port19
#define DEBUG_BUS_L1_ADDR_P19__4_0_WORD {9, 8, 2, 0, 1, 0}
#define DEBUG_BUS_L1_ADDR_P19__4_0 {DEBUG_BUS_L1_ADDR_P19__4_0_WORD, 27, 0x1F}

//  location: tt_tensix_with_l1 name: debug_daisy_stop_l1
//  name: debug_tensix_w_l1_in_4 alias: t_l1_addr
//  name: l1_addr_p19 alias: l1_addr note: Port19
#define DEBUG_BUS_L1_ADDR_P19__16_5_WORD {9, 8, 4, 0, 1, 0}
#define DEBUG_BUS_L1_ADDR_P19__16_5 {DEBUG_BUS_L1_ADDR_P19__16_5_WORD, 0, 0xFFF}

//  location: tt_tensix_with_l1 name: debug_daisy_stop_l1
//  name: debug_tensix_w_l1_in_4 alias: t_l1_addr
//  name: l1_addr_p18 alias: l1_addr note: Port18
#define DEBUG_BUS_L1_ADDR_P18_WORD {9, 8, 2, 0, 1, 0}
#define DEBUG_BUS_L1_ADDR_P18 {DEBUG_BUS_L1_ADDR_P18_WORD, 10, 0x1FFFF}

//  location: tt_tensix_with_l1 name: debug_daisy_stop_l1
//  name: debug_tensix_w_l1_in_4 alias: t_l1_addr
//  name: l1_addr_p17 alias: l1_addr note: Port17
#define DEBUG_BUS_L1_ADDR_P17__6_0_WORD {9, 8, 0, 0, 1, 0}
#define DEBUG_BUS_L1_ADDR_P17__6_0 {DEBUG_BUS_L1_ADDR_P17__6_0_WORD, 25, 0x7F}

//  location: tt_tensix_with_l1 name: debug_daisy_stop_l1
//  name: debug_tensix_w_l1_in_4 alias: t_l1_addr
//  name: l1_addr_p17 alias: l1_addr note: Port17
#define DEBUG_BUS_L1_ADDR_P17__16_7_WORD {9, 8, 2, 0, 1, 0}
#define DEBUG_BUS_L1_ADDR_P17__16_7 {DEBUG_BUS_L1_ADDR_P17__16_7_WORD, 0, 0x3FF}

//  location: tt_tensix_with_l1 name: debug_daisy_stop_l1
//  name: debug_tensix_w_l1_in_4 alias: t_l1_addr
//  name: l1_addr_p16 alias: l1_addr note: Port16
#define DEBUG_BUS_L1_ADDR_P16_WORD {9, 8, 0, 0, 1, 0}
#define DEBUG_BUS_L1_ADDR_P16 {DEBUG_BUS_L1_ADDR_P16_WORD, 8, 0x1FFFF}

//  location: tt_tensix_with_l1 name: debug_daisy_stop_l1
//  name: debug_tensix_w_l1_in_4 alias: t_l1_addr
//  name: l1_addr_p15 alias: l1_addr note: Port15
#define DEBUG_BUS_L1_ADDR_P15__8_0_WORD {8, 8, 6, 0, 1, 0}
#define DEBUG_BUS_L1_ADDR_P15__8_0 {DEBUG_BUS_L1_ADDR_P15__8_0_WORD, 23, 0x1FF}

//  location: tt_tensix_with_l1 name: debug_daisy_stop_l1
//  name: debug_tensix_w_l1_in_4 alias: t_l1_addr
//  name: l1_addr_p15 alias: l1_addr note: Port15
#define DEBUG_BUS_L1_ADDR_P15__16_9_WORD {9, 8, 0, 0, 1, 0}
#define DEBUG_BUS_L1_ADDR_P15__16_9 {DEBUG_BUS_L1_ADDR_P15__16_9_WORD, 0, 0xFF}

//  location: tt_tensix_with_l1 name: debug_daisy_stop_l1
//  name: debug_tensix_w_l1_in_4 alias: t_l1_addr
//  name: l1_addr_p14 alias: l1_addr note: Port14
#define DEBUG_BUS_L1_ADDR_P14_WORD {8, 8, 6, 0, 1, 0}
#define DEBUG_BUS_L1_ADDR_P14 {DEBUG_BUS_L1_ADDR_P14_WORD, 6, 0x1FFFF}

//  location: tt_tensix_with_l1 name: debug_daisy_stop_l1
//  name: debug_tensix_w_l1_in_4 alias: t_l1_addr
//  name: l1_addr_p13 alias: l1_addr note: Port13
#define DEBUG_BUS_L1_ADDR_P13__10_0_WORD {8, 8, 4, 0, 1, 0}
#define DEBUG_BUS_L1_ADDR_P13__10_0 {DEBUG_BUS_L1_ADDR_P13__10_0_WORD, 21, 0x7FF}

//  location: tt_tensix_with_l1 name: debug_daisy_stop_l1
//  name: debug_tensix_w_l1_in_4 alias: t_l1_addr
//  name: l1_addr_p13 alias: l1_addr note: Port13
#define DEBUG_BUS_L1_ADDR_P13__16_11_WORD {8, 8, 6, 0, 1, 0}
#define DEBUG_BUS_L1_ADDR_P13__16_11 {DEBUG_BUS_L1_ADDR_P13__16_11_WORD, 0, 0x3F}

//  location: tt_tensix_with_l1 name: debug_daisy_stop_l1
//  name: debug_tensix_w_l1_in_4 alias: t_l1_addr
//  name: l1_addr_p12 alias: l1_addr note: Port12
#define DEBUG_BUS_L1_ADDR_P12_WORD {8, 8, 4, 0, 1, 0}
#define DEBUG_BUS_L1_ADDR_P12 {DEBUG_BUS_L1_ADDR_P12_WORD, 4, 0x1FFFF}

//  location: tt_tensix_with_l1 name: debug_daisy_stop_l1
//  name: debug_tensix_w_l1_in_4 alias: t_l1_addr
//  name: l1_addr_p11 alias: l1_addr note: Port11
#define DEBUG_BUS_L1_ADDR_P11__12_0_WORD {8, 8, 2, 0, 1, 0}
#define DEBUG_BUS_L1_ADDR_P11__12_0 {DEBUG_BUS_L1_ADDR_P11__12_0_WORD, 19, 0x1FFF}

//  location: tt_tensix_with_l1 name: debug_daisy_stop_l1
//  name: debug_tensix_w_l1_in_4 alias: t_l1_addr
//  name: l1_addr_p11 alias: l1_addr note: Port11
#define DEBUG_BUS_L1_ADDR_P11__16_13_WORD {8, 8, 4, 0, 1, 0}
#define DEBUG_BUS_L1_ADDR_P11__16_13 {DEBUG_BUS_L1_ADDR_P11__16_13_WORD, 0, 0xF}

//  location: tt_tensix_with_l1 name: debug_daisy_stop_l1
//  name: debug_tensix_w_l1_in_4 alias: t_l1_addr
//  name: l1_addr_p10 alias: l1_addr note: Port10
#define DEBUG_BUS_L1_ADDR_P10_WORD {8, 8, 2, 0, 1, 0}
#define DEBUG_BUS_L1_ADDR_P10 {DEBUG_BUS_L1_ADDR_P10_WORD, 2, 0x1FFFF}

//  location: tt_tensix_with_l1 name: debug_daisy_stop_l1
//  name: debug_tensix_w_l1_in_4 alias: t_l1_addr
//  name: l1_addr_p9 alias: l1_addr note: Port9
#define DEBUG_BUS_L1_ADDR_P9__14_0_WORD {8, 8, 0, 0, 1, 0}
#define DEBUG_BUS_L1_ADDR_P9__14_0 {DEBUG_BUS_L1_ADDR_P9__14_0_WORD, 17, 0x7FFF}

//  location: tt_tensix_with_l1 name: debug_daisy_stop_l1
//  name: debug_tensix_w_l1_in_4 alias: t_l1_addr
//  name: l1_addr_p9 alias: l1_addr note: Port9
#define DEBUG_BUS_L1_ADDR_P9__16_15_WORD {8, 8, 2, 0, 1, 0}
#define DEBUG_BUS_L1_ADDR_P9__16_15 {DEBUG_BUS_L1_ADDR_P9__16_15_WORD, 0, 0x3}

//  location: tt_tensix_with_l1 name: debug_daisy_stop_l1
//  name: debug_tensix_w_l1_in_4 alias: t_l1_addr
//  name: l1_addr_p8 alias: l1_addr note: Port8
#define DEBUG_BUS_L1_ADDR_P8_WORD {8, 8, 0, 0, 1, 0}
#define DEBUG_BUS_L1_ADDR_P8 {DEBUG_BUS_L1_ADDR_P8_WORD, 0, 0x1FFFF}

//  location: tt_tensix_with_l1 name: debug_daisy_stop_l1
//  name: debug_tensix_w_l1_in_3
//  name: t_l1_addr
//  name: l1_addr_p7 alias: l1_addr note: Port7
#define DEBUG_BUS_L1_ADDR_P7__10_0_WORD {7, 8, 6, 0, 1, 0}
#define DEBUG_BUS_L1_ADDR_P7__10_0 {DEBUG_BUS_L1_ADDR_P7__10_0_WORD, 21, 0x7FF}

//  location: tt_tensix_with_l1 name: debug_daisy_stop_l1
//  name: debug_tensix_w_l1_in_3
//  name: t_l1_addr
//  name: l1_addr_p6 alias: l1_addr note: Port6
#define DEBUG_BUS_L1_ADDR_P6_WORD {7, 8, 6, 0, 1, 0}
#define DEBUG_BUS_L1_ADDR_P6 {DEBUG_BUS_L1_ADDR_P6_WORD, 4, 0x1FFFF}

//  location: tt_tensix_with_l1 name: debug_daisy_stop_l1
//  name: debug_tensix_w_l1_in_3
//  name: t_l1_addr
//  name: l1_addr_p5 alias: l1_addr note: Port5
#define DEBUG_BUS_L1_ADDR_P5__12_0_WORD {7, 8, 4, 0, 1, 0}
#define DEBUG_BUS_L1_ADDR_P5__12_0 {DEBUG_BUS_L1_ADDR_P5__12_0_WORD, 19, 0x1FFF}

//  location: tt_tensix_with_l1 name: debug_daisy_stop_l1
//  name: debug_tensix_w_l1_in_3
//  name: t_l1_addr
//  name: l1_addr_p5 alias: l1_addr note: Port5
#define DEBUG_BUS_L1_ADDR_P5__16_13_WORD {7, 8, 6, 0, 1, 0}
#define DEBUG_BUS_L1_ADDR_P5__16_13 {DEBUG_BUS_L1_ADDR_P5__16_13_WORD, 0, 0xF}

//  location: tt_tensix_with_l1 name: debug_daisy_stop_l1
//  name: debug_tensix_w_l1_in_3
//  name: t_l1_addr
//  name: l1_addr_p4 alias: l1_addr note: Port4
#define DEBUG_BUS_L1_ADDR_P4_WORD {7, 8, 4, 0, 1, 0}
#define DEBUG_BUS_L1_ADDR_P4 {DEBUG_BUS_L1_ADDR_P4_WORD, 2, 0x1FFFF}

//  location: tt_tensix_with_l1 name: debug_daisy_stop_l1
//  name: debug_tensix_w_l1_in_3
//  name: t_l1_addr
//  name: l1_addr_p3 alias: l1_addr note: Port3
#define DEBUG_BUS_L1_ADDR_P3__14_0_WORD {7, 8, 2, 0, 1, 0}
#define DEBUG_BUS_L1_ADDR_P3__14_0 {DEBUG_BUS_L1_ADDR_P3__14_0_WORD, 17, 0x7FFF}

//  location: tt_tensix_with_l1 name: debug_daisy_stop_l1
//  name: debug_tensix_w_l1_in_3
//  name: t_l1_addr
//  name: l1_addr_p3 alias: l1_addr note: Port3
#define DEBUG_BUS_L1_ADDR_P3__16_15_WORD {7, 8, 4, 0, 1, 0}
#define DEBUG_BUS_L1_ADDR_P3__16_15 {DEBUG_BUS_L1_ADDR_P3__16_15_WORD, 0, 0x3}

//  location: tt_tensix_with_l1 name: debug_daisy_stop_l1
//  name: debug_tensix_w_l1_in_3
//  name: t_l1_addr
//  name: l1_addr_p2 alias: l1_addr note: Port2
#define DEBUG_BUS_L1_ADDR_P2_WORD {7, 8, 2, 0, 1, 0}
#define DEBUG_BUS_L1_ADDR_P2 {DEBUG_BUS_L1_ADDR_P2_WORD, 0, 0x1FFFF}

//  location: tt_tensix_with_l1 name: debug_daisy_stop_l1
//  name: debug_tensix_w_l1_in_3
//  name: t_l1_addr
//  name: l1_addr_p1 alias: l1_addr note: Port1
#define DEBUG_BUS_L1_ADDR_P1_WORD {7, 8, 0, 0, 1, 0}
#define DEBUG_BUS_L1_ADDR_P1 {DEBUG_BUS_L1_ADDR_P1_WORD, 15, 0x1FFFF}

//  location: tt_tensix_with_l1 name: debug_daisy_stop_l1
//  name: debug_tensix_w_l1_in_3
//  name: t_l1_addr
//  name: l1_addr_p0 alias: l1_addr note: Port0
#define DEBUG_BUS_L1_ADDR_P0__1_0_WORD {6, 8, 6, 0, 1, 0}
#define DEBUG_BUS_L1_ADDR_P0__1_0 {DEBUG_BUS_L1_ADDR_P0__1_0_WORD, 30, 0x3}

//  location: tt_tensix_with_l1 name: debug_daisy_stop_l1
//  name: debug_tensix_w_l1_in_3
//  name: t_l1_addr
//  name: l1_addr_p0 alias: l1_addr note: Port0
#define DEBUG_BUS_L1_ADDR_P0__16_2_WORD {7, 8, 0, 0, 1, 0}
#define DEBUG_BUS_L1_ADDR_P0__16_2 {DEBUG_BUS_L1_ADDR_P0__16_2_WORD, 0, 0x7FFF}

//  location: tt_tensix_with_l1 name: debug_daisy_stop_l1
//  name: debug_tensix_w_l1_in_3
//  name: t_l1_reqif_ready
#define DEBUG_BUS_T_L1_REQIF_READY__11_0_WORD {6, 8, 4, 0, 1, 0}
#define DEBUG_BUS_T_L1_REQIF_READY__11_0 {DEBUG_BUS_T_L1_REQIF_READY__11_0_WORD, 20, 0xFFF}

//  location: tt_tensix_with_l1 name: debug_daisy_stop_l1
//  name: debug_tensix_w_l1_in_3
//  name: t_l1_reqif_ready
#define DEBUG_BUS_T_L1_REQIF_READY__41_12_WORD {6, 8, 6, 0, 1, 0}
#define DEBUG_BUS_T_L1_REQIF_READY__41_12 {DEBUG_BUS_T_L1_REQIF_READY__41_12_WORD, 0, 0x3FFFFFFF}

//  location: tt_tensix_with_l1 name: debug_daisy_stop_l1
//  name: debug_tensix_w_l1_in_3
//  name: t_l1_rden
#define DEBUG_BUS_T_L1_RDEN__21_0_WORD {6, 8, 2, 0, 1, 0}
#define DEBUG_BUS_T_L1_RDEN__21_0 {DEBUG_BUS_T_L1_RDEN__21_0_WORD, 10, 0x3FFFFF}

//  location: tt_tensix_with_l1 name: debug_daisy_stop_l1
//  name: debug_tensix_w_l1_in_3
//  name: t_l1_rden
#define DEBUG_BUS_T_L1_RDEN__41_22_WORD {6, 8, 4, 0, 1, 0}
#define DEBUG_BUS_T_L1_RDEN__41_22 {DEBUG_BUS_T_L1_RDEN__41_22_WORD, 0, 0xFFFFF}

//  location: tt_tensix_with_l1 name: debug_daisy_stop_l1
//  name: debug_tensix_w_l1_in_3
//  name: t_l1_wren
#define DEBUG_BUS_T_L1_WREN__31_0_WORD {6, 8, 0, 0, 1, 0}
#define DEBUG_BUS_T_L1_WREN__31_0 {DEBUG_BUS_T_L1_WREN__31_0_WORD, 0, 0xFFFFFFFF}

//  location: tt_tensix_with_l1 name: debug_daisy_stop_l1
//  name: debug_tensix_w_l1_in_3
//  name: t_l1_wren
#define DEBUG_BUS_T_L1_WREN__41_32_WORD {6, 8, 2, 0, 1, 0}
#define DEBUG_BUS_T_L1_WREN__41_32 {DEBUG_BUS_T_L1_WREN__41_32_WORD, 0, 0x3FF}

//  location: tt_tensix_with_l1 name: debug_daisy_stop_l1
//  name: debug_tensix_w_l1_in_2
//  name: t_l1_at_instrn
//  name: l1_at_instrn_p9 alias: l1_at_instrn note: Port9
#define DEBUG_BUS_T_L1_AT_INSTRN_L1_AT_INSTRN_P9_WORD {5, 8, 0, 0, 1, 0}
#define DEBUG_BUS_T_L1_AT_INSTRN_L1_AT_INSTRN_P9 {DEBUG_BUS_T_L1_AT_INSTRN_L1_AT_INSTRN_P9_WORD, 16, 0xFFFF}

//  location: tt_tensix_with_l1 name: debug_daisy_stop_l1
//  name: debug_tensix_w_l1_in_2
//  name: t_l1_at_instrn
//  name: l1_at_instrn_p8 alias: l1_at_instrn note: Port8
#define DEBUG_BUS_T_L1_AT_INSTRN_L1_AT_INSTRN_P8_WORD {5, 8, 0, 0, 1, 0}
#define DEBUG_BUS_T_L1_AT_INSTRN_L1_AT_INSTRN_P8 {DEBUG_BUS_T_L1_AT_INSTRN_L1_AT_INSTRN_P8_WORD, 0, 0xFFFF}

//  location: tt_tensix_with_l1 name: debug_daisy_stop_l1
//  name: debug_tensix_w_l1_in_2
//  name: t_l1_at_instrn
//  name: l1_at_instrn_p7 alias: l1_at_instrn note: Port7
#define DEBUG_BUS_T_L1_AT_INSTRN_L1_AT_INSTRN_P7_WORD {4, 8, 6, 0, 1, 0}
#define DEBUG_BUS_T_L1_AT_INSTRN_L1_AT_INSTRN_P7 {DEBUG_BUS_T_L1_AT_INSTRN_L1_AT_INSTRN_P7_WORD, 16, 0xFFFF}

//  location: tt_tensix_with_l1 name: debug_daisy_stop_l1
//  name: debug_tensix_w_l1_in_2
//  name: t_l1_at_instrn
//  name: l1_at_instrn_p6 alias: l1_at_instrn note: Port6
#define DEBUG_BUS_T_L1_AT_INSTRN_L1_AT_INSTRN_P6_WORD {4, 8, 6, 0, 1, 0}
#define DEBUG_BUS_T_L1_AT_INSTRN_L1_AT_INSTRN_P6 {DEBUG_BUS_T_L1_AT_INSTRN_L1_AT_INSTRN_P6_WORD, 0, 0xFFFF}

//  location: tt_tensix_with_l1 name: debug_daisy_stop_l1
//  name: debug_tensix_w_l1_in_2
//  name: t_l1_at_instrn
//  name: l1_at_instrn_p5 alias: l1_at_instrn note: Port5
#define DEBUG_BUS_T_L1_AT_INSTRN_L1_AT_INSTRN_P5_WORD {4, 8, 4, 0, 1, 0}
#define DEBUG_BUS_T_L1_AT_INSTRN_L1_AT_INSTRN_P5 {DEBUG_BUS_T_L1_AT_INSTRN_L1_AT_INSTRN_P5_WORD, 16, 0xFFFF}

//  location: tt_tensix_with_l1 name: debug_daisy_stop_l1
//  name: debug_tensix_w_l1_in_2
//  name: t_l1_at_instrn
//  name: l1_at_instrn_p4 alias: l1_at_instrn note: Port4
#define DEBUG_BUS_T_L1_AT_INSTRN_L1_AT_INSTRN_P4_WORD {4, 8, 4, 0, 1, 0}
#define DEBUG_BUS_T_L1_AT_INSTRN_L1_AT_INSTRN_P4 {DEBUG_BUS_T_L1_AT_INSTRN_L1_AT_INSTRN_P4_WORD, 0, 0xFFFF}

//  location: tt_tensix_with_l1 name: debug_daisy_stop_l1
//  name: debug_tensix_w_l1_in_2
//  name: t_l1_at_instrn
//  name: l1_at_instrn_p3 alias: l1_at_instrn note: Port3
#define DEBUG_BUS_T_L1_AT_INSTRN_L1_AT_INSTRN_P3_WORD {4, 8, 2, 0, 1, 0}
#define DEBUG_BUS_T_L1_AT_INSTRN_L1_AT_INSTRN_P3 {DEBUG_BUS_T_L1_AT_INSTRN_L1_AT_INSTRN_P3_WORD, 16, 0xFFFF}

//  location: tt_tensix_with_l1 name: debug_daisy_stop_l1
//  name: debug_tensix_w_l1_in_2
//  name: t_l1_at_instrn
//  name: l1_at_instrn_p2 alias: l1_at_instrn note: Port2
#define DEBUG_BUS_T_L1_AT_INSTRN_L1_AT_INSTRN_P2_WORD {4, 8, 2, 0, 1, 0}
#define DEBUG_BUS_T_L1_AT_INSTRN_L1_AT_INSTRN_P2 {DEBUG_BUS_T_L1_AT_INSTRN_L1_AT_INSTRN_P2_WORD, 0, 0xFFFF}

//  location: tt_tensix_with_l1 name: debug_daisy_stop_l1
//  name: debug_tensix_w_l1_in_2
//  name: t_l1_at_instrn
//  name: l1_at_instrn_p1 alias: l1_at_instrn note: Port1
#define DEBUG_BUS_T_L1_AT_INSTRN_L1_AT_INSTRN_P1_WORD {4, 8, 0, 0, 1, 0}
#define DEBUG_BUS_T_L1_AT_INSTRN_L1_AT_INSTRN_P1 {DEBUG_BUS_T_L1_AT_INSTRN_L1_AT_INSTRN_P1_WORD, 16, 0xFFFF}

//  location: tt_tensix_with_l1 name: debug_daisy_stop_l1
//  name: debug_tensix_w_l1_in_2
//  name: t_l1_at_instrn
//  name: l1_at_instrn_p0 alias: l1_at_instrn note: Port0
#define DEBUG_BUS_T_L1_AT_INSTRN_L1_AT_INSTRN_P0_WORD {4, 8, 0, 0, 1, 0}
#define DEBUG_BUS_T_L1_AT_INSTRN_L1_AT_INSTRN_P0 {DEBUG_BUS_T_L1_AT_INSTRN_L1_AT_INSTRN_P0_WORD, 0, 0xFFFF}

//  location: tt_tensix_with_l1 name: debug_daisy_stop_l1
//  name: debug_tensix_w_l1_in_1 alias: t_l1_at_instrn
//  name: l1_at_instrn_p15 alias: l1_at_instrn note: Port15
#define DEBUG_BUS_DEBUG_TENSIX_W_L1_IN_1_L1_AT_INSTRN_P15_WORD {3, 8, 6, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_W_L1_IN_1_L1_AT_INSTRN_P15 {DEBUG_BUS_DEBUG_TENSIX_W_L1_IN_1_L1_AT_INSTRN_P15_WORD, 16, 0xFFFF}

//  location: tt_tensix_with_l1 name: debug_daisy_stop_l1
//  name: debug_tensix_w_l1_in_1 alias: t_l1_at_instrn
//  name: l1_at_instrn_p14 alias: l1_at_instrn note: Port14
#define DEBUG_BUS_DEBUG_TENSIX_W_L1_IN_1_L1_AT_INSTRN_P14_WORD {3, 8, 6, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_W_L1_IN_1_L1_AT_INSTRN_P14 {DEBUG_BUS_DEBUG_TENSIX_W_L1_IN_1_L1_AT_INSTRN_P14_WORD, 0, 0xFFFF}

//  location: tt_tensix_with_l1 name: debug_daisy_stop_l1
//  name: debug_tensix_w_l1_in_1 alias: t_l1_at_instrn
//  name: l1_at_instrn_p13 alias: l1_at_instrn note: Port13
#define DEBUG_BUS_DEBUG_TENSIX_W_L1_IN_1_L1_AT_INSTRN_P13_WORD {3, 8, 4, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_W_L1_IN_1_L1_AT_INSTRN_P13 {DEBUG_BUS_DEBUG_TENSIX_W_L1_IN_1_L1_AT_INSTRN_P13_WORD, 16, 0xFFFF}

//  location: tt_tensix_with_l1 name: debug_daisy_stop_l1
//  name: debug_tensix_w_l1_in_1 alias: t_l1_at_instrn
//  name: l1_at_instrn_p12 alias: l1_at_instrn note: Port12
#define DEBUG_BUS_DEBUG_TENSIX_W_L1_IN_1_L1_AT_INSTRN_P12_WORD {3, 8, 4, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_W_L1_IN_1_L1_AT_INSTRN_P12 {DEBUG_BUS_DEBUG_TENSIX_W_L1_IN_1_L1_AT_INSTRN_P12_WORD, 0, 0xFFFF}

//  location: tt_tensix_with_l1 name: debug_daisy_stop_l1
//  name: debug_tensix_w_l1_in_1 alias: t_l1_at_instrn
//  name: l1_at_instrn_p11 alias: l1_at_instrn note: Port11
#define DEBUG_BUS_DEBUG_TENSIX_W_L1_IN_1_L1_AT_INSTRN_P11_WORD {3, 8, 2, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_W_L1_IN_1_L1_AT_INSTRN_P11 {DEBUG_BUS_DEBUG_TENSIX_W_L1_IN_1_L1_AT_INSTRN_P11_WORD, 16, 0xFFFF}

//  location: tt_tensix_with_l1 name: debug_daisy_stop_l1
//  name: debug_tensix_w_l1_in_1 alias: t_l1_at_instrn
//  name: l1_at_instrn_p10 alias: l1_at_instrn note: Port10
#define DEBUG_BUS_DEBUG_TENSIX_W_L1_IN_1_L1_AT_INSTRN_P10_WORD {3, 8, 2, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_W_L1_IN_1_L1_AT_INSTRN_P10 {DEBUG_BUS_DEBUG_TENSIX_W_L1_IN_1_L1_AT_INSTRN_P10_WORD, 0, 0xFFFF}

//  location: tt_tensix_with_l1 name: debug_daisy_stop_l1
//  name: debug_tensix_w_l1_in_1 alias: t_l1_at_instrn
//  name: l1_at_instrn_p9 alias: l1_at_instrn note: Port9
#define DEBUG_BUS_DEBUG_TENSIX_W_L1_IN_1_L1_AT_INSTRN_P9_WORD {3, 8, 0, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_W_L1_IN_1_L1_AT_INSTRN_P9 {DEBUG_BUS_DEBUG_TENSIX_W_L1_IN_1_L1_AT_INSTRN_P9_WORD, 16, 0xFFFF}

//  location: tt_tensix_with_l1 name: debug_daisy_stop_l1
//  name: debug_tensix_w_l1_in_1 alias: t_l1_at_instrn
//  name: l1_at_instrn_p8 alias: l1_at_instrn note: Port8
#define DEBUG_BUS_DEBUG_TENSIX_W_L1_IN_1_L1_AT_INSTRN_P8_WORD {3, 8, 0, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_W_L1_IN_1_L1_AT_INSTRN_P8 {DEBUG_BUS_DEBUG_TENSIX_W_L1_IN_1_L1_AT_INSTRN_P8_WORD, 0, 0xFFFF}

//  location: tt_tensix_with_l1 name: debug_daisy_stop_l1
//  name: debug_tensix_w_l1_in_1 alias: t_l1_at_instrn
//  name: l1_at_instrn_p7 alias: l1_at_instrn note: Port7
#define DEBUG_BUS_DEBUG_TENSIX_W_L1_IN_1_L1_AT_INSTRN_P7_WORD {2, 8, 6, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_W_L1_IN_1_L1_AT_INSTRN_P7 {DEBUG_BUS_DEBUG_TENSIX_W_L1_IN_1_L1_AT_INSTRN_P7_WORD, 16, 0xFFFF}

//  location: tt_tensix_with_l1 name: debug_daisy_stop_l1
//  name: debug_tensix_w_l1_in_1 alias: t_l1_at_instrn
//  name: l1_at_instrn_p6 alias: l1_at_instrn note: Port6
#define DEBUG_BUS_DEBUG_TENSIX_W_L1_IN_1_L1_AT_INSTRN_P6_WORD {2, 8, 6, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_W_L1_IN_1_L1_AT_INSTRN_P6 {DEBUG_BUS_DEBUG_TENSIX_W_L1_IN_1_L1_AT_INSTRN_P6_WORD, 0, 0xFFFF}

//  location: tt_tensix_with_l1 name: debug_daisy_stop_l1
//  name: debug_tensix_w_l1_in_1 alias: t_l1_at_instrn
//  name: l1_at_instrn_p5 alias: l1_at_instrn note: Port5
#define DEBUG_BUS_DEBUG_TENSIX_W_L1_IN_1_L1_AT_INSTRN_P5_WORD {2, 8, 4, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_W_L1_IN_1_L1_AT_INSTRN_P5 {DEBUG_BUS_DEBUG_TENSIX_W_L1_IN_1_L1_AT_INSTRN_P5_WORD, 16, 0xFFFF}

//  location: tt_tensix_with_l1 name: debug_daisy_stop_l1
//  name: debug_tensix_w_l1_in_1 alias: t_l1_at_instrn
//  name: l1_at_instrn_p4 alias: l1_at_instrn note: Port4
#define DEBUG_BUS_DEBUG_TENSIX_W_L1_IN_1_L1_AT_INSTRN_P4_WORD {2, 8, 4, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_W_L1_IN_1_L1_AT_INSTRN_P4 {DEBUG_BUS_DEBUG_TENSIX_W_L1_IN_1_L1_AT_INSTRN_P4_WORD, 0, 0xFFFF}

//  location: tt_tensix_with_l1 name: debug_daisy_stop_l1
//  name: debug_tensix_w_l1_in_1 alias: t_l1_at_instrn
//  name: l1_at_instrn_p3 alias: l1_at_instrn note: Port3
#define DEBUG_BUS_DEBUG_TENSIX_W_L1_IN_1_L1_AT_INSTRN_P3_WORD {2, 8, 2, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_W_L1_IN_1_L1_AT_INSTRN_P3 {DEBUG_BUS_DEBUG_TENSIX_W_L1_IN_1_L1_AT_INSTRN_P3_WORD, 16, 0xFFFF}

//  location: tt_tensix_with_l1 name: debug_daisy_stop_l1
//  name: debug_tensix_w_l1_in_1 alias: t_l1_at_instrn
//  name: l1_at_instrn_p2 alias: l1_at_instrn note: Port2
#define DEBUG_BUS_DEBUG_TENSIX_W_L1_IN_1_L1_AT_INSTRN_P2_WORD {2, 8, 2, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_W_L1_IN_1_L1_AT_INSTRN_P2 {DEBUG_BUS_DEBUG_TENSIX_W_L1_IN_1_L1_AT_INSTRN_P2_WORD, 0, 0xFFFF}

//  location: tt_tensix_with_l1 name: debug_daisy_stop_l1
//  name: debug_tensix_w_l1_in_1 alias: t_l1_at_instrn
//  name: l1_at_instrn_p1 alias: l1_at_instrn note: Port1
#define DEBUG_BUS_DEBUG_TENSIX_W_L1_IN_1_L1_AT_INSTRN_P1_WORD {2, 8, 0, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_W_L1_IN_1_L1_AT_INSTRN_P1 {DEBUG_BUS_DEBUG_TENSIX_W_L1_IN_1_L1_AT_INSTRN_P1_WORD, 16, 0xFFFF}

//  location: tt_tensix_with_l1 name: debug_daisy_stop_l1
//  name: debug_tensix_w_l1_in_1 alias: t_l1_at_instrn
//  name: l1_at_instrn_p0 alias: l1_at_instrn note: Port0
#define DEBUG_BUS_DEBUG_TENSIX_W_L1_IN_1_L1_AT_INSTRN_P0_WORD {2, 8, 0, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_W_L1_IN_1_L1_AT_INSTRN_P0 {DEBUG_BUS_DEBUG_TENSIX_W_L1_IN_1_L1_AT_INSTRN_P0_WORD, 0, 0xFFFF}

//  location: tt_tensix_with_l1 name: debug_daisy_stop_l1
//  name: debug_tensix_w_l1_in_0 alias: t_l1_at_instrn
//  name: l1_at_instrn_p15 alias: l1_at_instrn note: Port15
#define DEBUG_BUS_DEBUG_TENSIX_W_L1_IN_0_L1_AT_INSTRN_P15_WORD {1, 8, 6, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_W_L1_IN_0_L1_AT_INSTRN_P15 {DEBUG_BUS_DEBUG_TENSIX_W_L1_IN_0_L1_AT_INSTRN_P15_WORD, 16, 0xFFFF}

//  location: tt_tensix_with_l1 name: debug_daisy_stop_l1
//  name: debug_tensix_w_l1_in_0 alias: t_l1_at_instrn
//  name: l1_at_instrn_p14 alias: l1_at_instrn note: Port14
#define DEBUG_BUS_DEBUG_TENSIX_W_L1_IN_0_L1_AT_INSTRN_P14_WORD {1, 8, 6, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_W_L1_IN_0_L1_AT_INSTRN_P14 {DEBUG_BUS_DEBUG_TENSIX_W_L1_IN_0_L1_AT_INSTRN_P14_WORD, 0, 0xFFFF}

//  location: tt_tensix_with_l1 name: debug_daisy_stop_l1
//  name: debug_tensix_w_l1_in_0 alias: t_l1_at_instrn
//  name: l1_at_instrn_p13 alias: l1_at_instrn note: Port13
#define DEBUG_BUS_DEBUG_TENSIX_W_L1_IN_0_L1_AT_INSTRN_P13_WORD {1, 8, 4, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_W_L1_IN_0_L1_AT_INSTRN_P13 {DEBUG_BUS_DEBUG_TENSIX_W_L1_IN_0_L1_AT_INSTRN_P13_WORD, 16, 0xFFFF}

//  location: tt_tensix_with_l1 name: debug_daisy_stop_l1
//  name: debug_tensix_w_l1_in_0 alias: t_l1_at_instrn
//  name: l1_at_instrn_p12 alias: l1_at_instrn note: Port12
#define DEBUG_BUS_DEBUG_TENSIX_W_L1_IN_0_L1_AT_INSTRN_P12_WORD {1, 8, 4, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_W_L1_IN_0_L1_AT_INSTRN_P12 {DEBUG_BUS_DEBUG_TENSIX_W_L1_IN_0_L1_AT_INSTRN_P12_WORD, 0, 0xFFFF}

//  location: tt_tensix_with_l1 name: debug_daisy_stop_l1
//  name: debug_tensix_w_l1_in_0 alias: t_l1_at_instrn
//  name: l1_at_instrn_p11 alias: l1_at_instrn note: Port11
#define DEBUG_BUS_DEBUG_TENSIX_W_L1_IN_0_L1_AT_INSTRN_P11_WORD {1, 8, 2, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_W_L1_IN_0_L1_AT_INSTRN_P11 {DEBUG_BUS_DEBUG_TENSIX_W_L1_IN_0_L1_AT_INSTRN_P11_WORD, 16, 0xFFFF}

//  location: tt_tensix_with_l1 name: debug_daisy_stop_l1
//  name: debug_tensix_w_l1_in_0 alias: t_l1_at_instrn
//  name: l1_at_instrn_p10 alias: l1_at_instrn note: Port10
#define DEBUG_BUS_DEBUG_TENSIX_W_L1_IN_0_L1_AT_INSTRN_P10_WORD {1, 8, 2, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_W_L1_IN_0_L1_AT_INSTRN_P10 {DEBUG_BUS_DEBUG_TENSIX_W_L1_IN_0_L1_AT_INSTRN_P10_WORD, 0, 0xFFFF}

//  location: tt_tensix_with_l1 name: debug_daisy_stop_l1
//  name: debug_tensix_w_l1_in_0 alias: t_l1_at_instrn
//  name: l1_at_instrn_p9 alias: l1_at_instrn note: Port9
#define DEBUG_BUS_DEBUG_TENSIX_W_L1_IN_0_L1_AT_INSTRN_P9_WORD {1, 8, 0, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_W_L1_IN_0_L1_AT_INSTRN_P9 {DEBUG_BUS_DEBUG_TENSIX_W_L1_IN_0_L1_AT_INSTRN_P9_WORD, 16, 0xFFFF}

//  location: tt_tensix_with_l1 name: debug_daisy_stop_l1
//  name: debug_tensix_w_l1_in_0 alias: t_l1_at_instrn
//  name: l1_at_instrn_p8 alias: l1_at_instrn note: Port8
#define DEBUG_BUS_DEBUG_TENSIX_W_L1_IN_0_L1_AT_INSTRN_P8_WORD {1, 8, 0, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_W_L1_IN_0_L1_AT_INSTRN_P8 {DEBUG_BUS_DEBUG_TENSIX_W_L1_IN_0_L1_AT_INSTRN_P8_WORD, 0, 0xFFFF}

//  location: tt_tensix_with_l1 name: debug_daisy_stop_l1
//  name: debug_tensix_w_l1_in_0 alias: t_l1_at_instrn
//  name: l1_at_instrn_p7 alias: l1_at_instrn note: Port7
#define DEBUG_BUS_DEBUG_TENSIX_W_L1_IN_0_L1_AT_INSTRN_P7_WORD {0, 8, 6, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_W_L1_IN_0_L1_AT_INSTRN_P7 {DEBUG_BUS_DEBUG_TENSIX_W_L1_IN_0_L1_AT_INSTRN_P7_WORD, 16, 0xFFFF}

//  location: tt_tensix_with_l1 name: debug_daisy_stop_l1
//  name: debug_tensix_w_l1_in_0 alias: t_l1_at_instrn
//  name: l1_at_instrn_p6 alias: l1_at_instrn note: Port6
#define DEBUG_BUS_DEBUG_TENSIX_W_L1_IN_0_L1_AT_INSTRN_P6_WORD {0, 8, 6, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_W_L1_IN_0_L1_AT_INSTRN_P6 {DEBUG_BUS_DEBUG_TENSIX_W_L1_IN_0_L1_AT_INSTRN_P6_WORD, 0, 0xFFFF}

//  location: tt_tensix_with_l1 name: debug_daisy_stop_l1
//  name: debug_tensix_w_l1_in_0 alias: t_l1_at_instrn
//  name: l1_at_instrn_p5 alias: l1_at_instrn note: Port5
#define DEBUG_BUS_DEBUG_TENSIX_W_L1_IN_0_L1_AT_INSTRN_P5_WORD {0, 8, 4, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_W_L1_IN_0_L1_AT_INSTRN_P5 {DEBUG_BUS_DEBUG_TENSIX_W_L1_IN_0_L1_AT_INSTRN_P5_WORD, 16, 0xFFFF}

//  location: tt_tensix_with_l1 name: debug_daisy_stop_l1
//  name: debug_tensix_w_l1_in_0 alias: t_l1_at_instrn
//  name: l1_at_instrn_p4 alias: l1_at_instrn note: Port4
#define DEBUG_BUS_DEBUG_TENSIX_W_L1_IN_0_L1_AT_INSTRN_P4_WORD {0, 8, 4, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_W_L1_IN_0_L1_AT_INSTRN_P4 {DEBUG_BUS_DEBUG_TENSIX_W_L1_IN_0_L1_AT_INSTRN_P4_WORD, 0, 0xFFFF}

//  location: tt_tensix_with_l1 name: debug_daisy_stop_l1
//  name: debug_tensix_w_l1_in_0 alias: t_l1_at_instrn
//  name: l1_at_instrn_p3 alias: l1_at_instrn note: Port3
#define DEBUG_BUS_DEBUG_TENSIX_W_L1_IN_0_L1_AT_INSTRN_P3_WORD {0, 8, 2, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_W_L1_IN_0_L1_AT_INSTRN_P3 {DEBUG_BUS_DEBUG_TENSIX_W_L1_IN_0_L1_AT_INSTRN_P3_WORD, 16, 0xFFFF}

//  location: tt_tensix_with_l1 name: debug_daisy_stop_l1
//  name: debug_tensix_w_l1_in_0 alias: t_l1_at_instrn
//  name: l1_at_instrn_p2 alias: l1_at_instrn note: Port2
#define DEBUG_BUS_DEBUG_TENSIX_W_L1_IN_0_L1_AT_INSTRN_P2_WORD {0, 8, 2, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_W_L1_IN_0_L1_AT_INSTRN_P2 {DEBUG_BUS_DEBUG_TENSIX_W_L1_IN_0_L1_AT_INSTRN_P2_WORD, 0, 0xFFFF}

//  location: tt_tensix_with_l1 name: debug_daisy_stop_l1
//  name: debug_tensix_w_l1_in_0 alias: t_l1_at_instrn
//  name: l1_at_instrn_p1 alias: l1_at_instrn note: Port1
#define DEBUG_BUS_DEBUG_TENSIX_W_L1_IN_0_L1_AT_INSTRN_P1_WORD {0, 8, 0, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_W_L1_IN_0_L1_AT_INSTRN_P1 {DEBUG_BUS_DEBUG_TENSIX_W_L1_IN_0_L1_AT_INSTRN_P1_WORD, 16, 0xFFFF}

//  location: tt_tensix_with_l1 name: debug_daisy_stop_l1
//  name: debug_tensix_w_l1_in_0 alias: t_l1_at_instrn
//  name: l1_at_instrn_p0 alias: l1_at_instrn note: Port0
#define DEBUG_BUS_DEBUG_TENSIX_W_L1_IN_0_L1_AT_INSTRN_P0_WORD {0, 8, 0, 0, 1, 0}
#define DEBUG_BUS_DEBUG_TENSIX_W_L1_IN_0_L1_AT_INSTRN_P0 {DEBUG_BUS_DEBUG_TENSIX_W_L1_IN_0_L1_AT_INSTRN_P0_WORD, 0, 0xFFFF}

//  location: tt_thread_controller name: debug_daisy_stop
//  name: debug_in_thcon_1 alias: debug_in_thcon_tdma_params_1
//  name: o_exp_section_size
#define DEBUG_BUS_O_EXP_SECTION_SIZE__11_0_WORD {3, 10, 4, 0, 1, 0}
#define DEBUG_BUS_O_EXP_SECTION_SIZE__11_0 {DEBUG_BUS_O_EXP_SECTION_SIZE__11_0_WORD, 20, 0xFFF}

//  location: tt_thread_controller name: debug_daisy_stop
//  name: debug_in_thcon_1 alias: debug_in_thcon_tdma_params_1
//  name: o_exp_section_size
#define DEBUG_BUS_O_EXP_SECTION_SIZE__31_12_WORD {3, 10, 6, 0, 1, 0}
#define DEBUG_BUS_O_EXP_SECTION_SIZE__31_12 {DEBUG_BUS_O_EXP_SECTION_SIZE__31_12_WORD, 0, 0xFFFFF}

//  location: tt_thread_controller name: debug_daisy_stop
//  name: debug_in_thcon_1 alias: debug_in_thcon_tdma_params_1
//  name: o_rowstart_section_size
#define DEBUG_BUS_O_ROWSTART_SECTION_SIZE__11_0_WORD {3, 10, 2, 0, 1, 0}
#define DEBUG_BUS_O_ROWSTART_SECTION_SIZE__11_0 {DEBUG_BUS_O_ROWSTART_SECTION_SIZE__11_0_WORD, 20, 0xFFF}

//  location: tt_thread_controller name: debug_daisy_stop
//  name: debug_in_thcon_1 alias: debug_in_thcon_tdma_params_1
//  name: o_rowstart_section_size
#define DEBUG_BUS_O_ROWSTART_SECTION_SIZE__31_12_WORD {3, 10, 4, 0, 1, 0}
#define DEBUG_BUS_O_ROWSTART_SECTION_SIZE__31_12 {DEBUG_BUS_O_ROWSTART_SECTION_SIZE__31_12_WORD, 0, 0xFFFFF}

//  location: tt_thread_controller name: debug_daisy_stop
//  name: debug_in_thcon_1 alias: debug_in_thcon_tdma_params_1
//  name: o_add_l1_destination_addr_offset
#define DEBUG_BUS_O_ADD_L1_DESTINATION_ADDR_OFFSET__3_0_WORD {2, 10, 0, 0, 1, 0}
#define DEBUG_BUS_O_ADD_L1_DESTINATION_ADDR_OFFSET__3_0 {DEBUG_BUS_O_ADD_L1_DESTINATION_ADDR_OFFSET__3_0_WORD, 0, 0xF}

//  location: tt_thread_controller name: debug_daisy_stop
//  name: debug_in_thcon_0
//  name: debug_in_thcon_instrn
#define DEBUG_BUS_DEBUG_IN_THCON_INSTRN_WORD {0, 10, 2, 0, 1, 0}
#define DEBUG_BUS_DEBUG_IN_THCON_INSTRN {DEBUG_BUS_DEBUG_IN_THCON_INSTRN_WORD, 0, 0xFFFFFFFF}

//  location: tt_search_startx name: debug_daisy_stop
//  name: debug_in_xsearch_1 alias: debug_in_xsearch_output
//  name: o_first_datum_prefix_zeros
#define DEBUG_BUS_O_FIRST_DATUM_PREFIX_ZEROS_WORD {2, 11, 4, 0, 1, 0}
#define DEBUG_BUS_O_FIRST_DATUM_PREFIX_ZEROS {DEBUG_BUS_O_FIRST_DATUM_PREFIX_ZEROS_WORD, 1, 0xFFFF}

//  location: tt_search_startx name: debug_daisy_stop
//  name: debug_in_xsearch_1 alias: debug_in_xsearch_output
//  name: o_start_datum_index
#define DEBUG_BUS_O_START_DATUM_INDEX__14_0_WORD {2, 11, 2, 0, 1, 0}
#define DEBUG_BUS_O_START_DATUM_INDEX__14_0 {DEBUG_BUS_O_START_DATUM_INDEX__14_0_WORD, 17, 0x7FFF}

//  location: tt_search_startx name: debug_daisy_stop
//  name: debug_in_xsearch_1 alias: debug_in_xsearch_output
//  name: o_start_datum_index
#define DEBUG_BUS_O_START_DATUM_INDEX__15_15_WORD {2, 11, 4, 0, 1, 0}
#define DEBUG_BUS_O_START_DATUM_INDEX__15_15 {DEBUG_BUS_O_START_DATUM_INDEX__15_15_WORD, 0, 0x1}

//  location: tt_search_startx name: debug_daisy_stop
//  name: debug_in_xsearch_1 alias: debug_in_xsearch_output
//  name: o_end_datum_index
#define DEBUG_BUS_O_END_DATUM_INDEX_WORD {2, 11, 2, 0, 1, 0}
#define DEBUG_BUS_O_END_DATUM_INDEX {DEBUG_BUS_O_END_DATUM_INDEX_WORD, 1, 0xFFFF}

//  location: tt_search_startx name: debug_daisy_stop
//  name: debug_in_xsearch_1 alias: debug_in_xsearch_output
//  name: o_first_data_skip_one_phase
#define DEBUG_BUS_O_FIRST_DATA_SKIP_ONE_PHASE__2_0_WORD {2, 11, 0, 0, 1, 0}
#define DEBUG_BUS_O_FIRST_DATA_SKIP_ONE_PHASE__2_0 {DEBUG_BUS_O_FIRST_DATA_SKIP_ONE_PHASE__2_0_WORD, 29, 0x7}

//  location: tt_search_startx name: debug_daisy_stop
//  name: debug_in_xsearch_1 alias: debug_in_xsearch_output
//  name: o_first_data_skip_one_phase
#define DEBUG_BUS_O_FIRST_DATA_SKIP_ONE_PHASE__3_3_WORD {2, 11, 2, 0, 1, 0}
#define DEBUG_BUS_O_FIRST_DATA_SKIP_ONE_PHASE__3_3 {DEBUG_BUS_O_FIRST_DATA_SKIP_ONE_PHASE__3_3_WORD, 0, 0x1}

//  location: tt_search_startx name: debug_daisy_stop
//  name: debug_in_xsearch_1 alias: debug_in_xsearch_output
//  name: x_start_d
#define DEBUG_BUS_X_START_D_WORD {2, 11, 0, 0, 1, 0}
#define DEBUG_BUS_X_START_D {DEBUG_BUS_X_START_D_WORD, 16, 0x1FFF}

//  location: tt_search_startx name: debug_daisy_stop
//  name: debug_in_xsearch_1 alias: debug_in_xsearch_output
//  name: x_end_d
#define DEBUG_BUS_X_END_D_WORD {2, 11, 0, 0, 1, 0}
#define DEBUG_BUS_X_END_D {DEBUG_BUS_X_END_D_WORD, 3, 0x1FFF}

//  location: tt_search_startx name: debug_daisy_stop
//  name: debug_in_xsearch_1 alias: debug_in_xsearch_output
//  name: unpack_sel_d
#define DEBUG_BUS_UNPACK_SEL_D_WORD {2, 11, 0, 0, 1, 0}
#define DEBUG_BUS_UNPACK_SEL_D {DEBUG_BUS_UNPACK_SEL_D_WORD, 2, 0x1}

//  location: tt_search_startx name: debug_daisy_stop
//  name: debug_in_xsearch_1 alias: debug_in_xsearch_output
//  name: thread_id_d
#define DEBUG_BUS_THREAD_ID_D_WORD {2, 11, 0, 0, 1, 0}
#define DEBUG_BUS_THREAD_ID_D {DEBUG_BUS_THREAD_ID_D_WORD, 0, 0x3}

//  location: tt_unpack_row name: UNP0_debug_daisy_stop
//  name: debug_in_unpack_0
//  name: demux_fifo_empty
#define DEBUG_BUS_UNP0_DEBUG_DAISY_STOP_DEBUG_IN_UNPACK_0_DEMUX_FIFO_EMPTY__2_0_WORD {1, 12, 2, 0, 1, 0}
#define DEBUG_BUS_UNP0_DEBUG_DAISY_STOP_DEBUG_IN_UNPACK_0_DEMUX_FIFO_EMPTY__2_0 {DEBUG_BUS_UNP0_DEBUG_DAISY_STOP_DEBUG_IN_UNPACK_0_DEMUX_FIFO_EMPTY__2_0_WORD, 29, 0x7}

//  location: tt_unpack_row name: UNP0_debug_daisy_stop
//  name: debug_in_unpack_0
//  name: demux_fifo_full
#define DEBUG_BUS_UNP0_DEBUG_DAISY_STOP_DEBUG_IN_UNPACK_0_DEMUX_FIFO_FULL__2_0_WORD {1, 12, 2, 0, 1, 0}
#define DEBUG_BUS_UNP0_DEBUG_DAISY_STOP_DEBUG_IN_UNPACK_0_DEMUX_FIFO_FULL__2_0 {DEBUG_BUS_UNP0_DEBUG_DAISY_STOP_DEBUG_IN_UNPACK_0_DEMUX_FIFO_FULL__2_0_WORD, 26, 0x7}

//  location: tt_unpack_row name: UNP0_debug_daisy_stop
//  name: debug_in_unpack_0
//  name: &demux_fifo_empty
#define DEBUG_BUS_UNP0_DEBUG_DAISY_STOP_DEBUG_IN_UNPACK_0__DEMUX_FIFO_EMPTY_WORD {1, 12, 2, 0, 1, 0}
#define DEBUG_BUS_UNP0_DEBUG_DAISY_STOP_DEBUG_IN_UNPACK_0__DEMUX_FIFO_EMPTY {DEBUG_BUS_UNP0_DEBUG_DAISY_STOP_DEBUG_IN_UNPACK_0__DEMUX_FIFO_EMPTY_WORD, 25, 0x1}

//  location: tt_unpack_row name: UNP0_debug_daisy_stop
//  name: debug_in_unpack_0
//  name: |demux_fifo_full
#define DEBUG_BUS_UNP0_DEBUG_DAISY_STOP_DEBUG_IN_UNPACK_0__DEMUX_FIFO_FULL_WORD {1, 12, 0, 0, 1, 0}
#define DEBUG_BUS_UNP0_DEBUG_DAISY_STOP_DEBUG_IN_UNPACK_0__DEMUX_FIFO_FULL {DEBUG_BUS_UNP0_DEBUG_DAISY_STOP_DEBUG_IN_UNPACK_0__DEMUX_FIFO_FULL_WORD, 28, 0x1}

//  location: tt_unpack_row name: UNP0_debug_daisy_stop
//  name: debug_in_unpack_0
//  name: req_param_fifo_empty
#define DEBUG_BUS_UNP0_DEBUG_DAISY_STOP_DEBUG_IN_UNPACK_0_REQ_PARAM_FIFO_EMPTY_WORD {0, 12, 2, 0, 1, 0}
#define DEBUG_BUS_UNP0_DEBUG_DAISY_STOP_DEBUG_IN_UNPACK_0_REQ_PARAM_FIFO_EMPTY {DEBUG_BUS_UNP0_DEBUG_DAISY_STOP_DEBUG_IN_UNPACK_0_REQ_PARAM_FIFO_EMPTY_WORD, 31, 0x1}

//  location: tt_unpack_row name: UNP0_debug_daisy_stop
//  name: debug_in_unpack_0
//  name: req_param_fifo_full
#define DEBUG_BUS_UNP0_DEBUG_DAISY_STOP_DEBUG_IN_UNPACK_0_REQ_PARAM_FIFO_FULL_WORD {0, 12, 2, 0, 1, 0}
#define DEBUG_BUS_UNP0_DEBUG_DAISY_STOP_DEBUG_IN_UNPACK_0_REQ_PARAM_FIFO_FULL {DEBUG_BUS_UNP0_DEBUG_DAISY_STOP_DEBUG_IN_UNPACK_0_REQ_PARAM_FIFO_FULL_WORD, 30, 0x1}

//  location: tt_unpack_row name: UNP0_debug_daisy_stop
//  name: debug_in_unpack_0
//  name: param_fifo_empty
#define DEBUG_BUS_UNP0_DEBUG_DAISY_STOP_DEBUG_IN_UNPACK_0_PARAM_FIFO_EMPTY_WORD {0, 12, 2, 0, 1, 0}
#define DEBUG_BUS_UNP0_DEBUG_DAISY_STOP_DEBUG_IN_UNPACK_0_PARAM_FIFO_EMPTY {DEBUG_BUS_UNP0_DEBUG_DAISY_STOP_DEBUG_IN_UNPACK_0_PARAM_FIFO_EMPTY_WORD, 29, 0x1}

//  location: tt_unpack_row name: UNP0_debug_daisy_stop
//  name: debug_in_unpack_0
//  name: param_fifo_full
#define DEBUG_BUS_UNP0_DEBUG_DAISY_STOP_DEBUG_IN_UNPACK_0_PARAM_FIFO_FULL_WORD {0, 12, 2, 0, 1, 0}
#define DEBUG_BUS_UNP0_DEBUG_DAISY_STOP_DEBUG_IN_UNPACK_0_PARAM_FIFO_FULL {DEBUG_BUS_UNP0_DEBUG_DAISY_STOP_DEBUG_IN_UNPACK_0_PARAM_FIFO_FULL_WORD, 28, 0x1}

//  location: tt_unpack_row name: UNP1_debug_daisy_stop
//  name: debug_in_unpack_0
//  name: demux_fifo_empty
#define DEBUG_BUS_UNP1_DEBUG_DAISY_STOP_DEBUG_IN_UNPACK_0_DEMUX_FIFO_EMPTY__2_0_WORD {1, 13, 2, 0, 1, 0}
#define DEBUG_BUS_UNP1_DEBUG_DAISY_STOP_DEBUG_IN_UNPACK_0_DEMUX_FIFO_EMPTY__2_0 {DEBUG_BUS_UNP1_DEBUG_DAISY_STOP_DEBUG_IN_UNPACK_0_DEMUX_FIFO_EMPTY__2_0_WORD, 29, 0x7}

//  location: tt_unpack_row name: UNP1_debug_daisy_stop
//  name: debug_in_unpack_0
//  name: demux_fifo_full
#define DEBUG_BUS_UNP1_DEBUG_DAISY_STOP_DEBUG_IN_UNPACK_0_DEMUX_FIFO_FULL__2_0_WORD {1, 13, 2, 0, 1, 0}
#define DEBUG_BUS_UNP1_DEBUG_DAISY_STOP_DEBUG_IN_UNPACK_0_DEMUX_FIFO_FULL__2_0 {DEBUG_BUS_UNP1_DEBUG_DAISY_STOP_DEBUG_IN_UNPACK_0_DEMUX_FIFO_FULL__2_0_WORD, 26, 0x7}

//  location: tt_unpack_row name: UNP1_debug_daisy_stop
//  name: debug_in_unpack_0
//  name: &demux_fifo_empty
#define DEBUG_BUS_UNP1_DEBUG_DAISY_STOP_DEBUG_IN_UNPACK_0__DEMUX_FIFO_EMPTY_WORD {1, 13, 2, 0, 1, 0}
#define DEBUG_BUS_UNP1_DEBUG_DAISY_STOP_DEBUG_IN_UNPACK_0__DEMUX_FIFO_EMPTY {DEBUG_BUS_UNP1_DEBUG_DAISY_STOP_DEBUG_IN_UNPACK_0__DEMUX_FIFO_EMPTY_WORD, 25, 0x1}

//  location: tt_unpack_row name: UNP1_debug_daisy_stop
//  name: debug_in_unpack_0
//  name: |demux_fifo_full
#define DEBUG_BUS_UNP1_DEBUG_DAISY_STOP_DEBUG_IN_UNPACK_0__DEMUX_FIFO_FULL_WORD {1, 13, 0, 0, 1, 0}
#define DEBUG_BUS_UNP1_DEBUG_DAISY_STOP_DEBUG_IN_UNPACK_0__DEMUX_FIFO_FULL {DEBUG_BUS_UNP1_DEBUG_DAISY_STOP_DEBUG_IN_UNPACK_0__DEMUX_FIFO_FULL_WORD, 28, 0x1}

//  location: tt_unpack_row name: UNP1_debug_daisy_stop
//  name: debug_in_unpack_0
//  name: req_param_fifo_empty
#define DEBUG_BUS_UNP1_DEBUG_DAISY_STOP_DEBUG_IN_UNPACK_0_REQ_PARAM_FIFO_EMPTY_WORD {0, 13, 2, 0, 1, 0}
#define DEBUG_BUS_UNP1_DEBUG_DAISY_STOP_DEBUG_IN_UNPACK_0_REQ_PARAM_FIFO_EMPTY {DEBUG_BUS_UNP1_DEBUG_DAISY_STOP_DEBUG_IN_UNPACK_0_REQ_PARAM_FIFO_EMPTY_WORD, 31, 0x1}

//  location: tt_unpack_row name: UNP1_debug_daisy_stop
//  name: debug_in_unpack_0
//  name: req_param_fifo_full
#define DEBUG_BUS_UNP1_DEBUG_DAISY_STOP_DEBUG_IN_UNPACK_0_REQ_PARAM_FIFO_FULL_WORD {0, 13, 2, 0, 1, 0}
#define DEBUG_BUS_UNP1_DEBUG_DAISY_STOP_DEBUG_IN_UNPACK_0_REQ_PARAM_FIFO_FULL {DEBUG_BUS_UNP1_DEBUG_DAISY_STOP_DEBUG_IN_UNPACK_0_REQ_PARAM_FIFO_FULL_WORD, 30, 0x1}

//  location: tt_unpack_row name: UNP1_debug_daisy_stop
//  name: debug_in_unpack_0
//  name: param_fifo_empty
#define DEBUG_BUS_UNP1_DEBUG_DAISY_STOP_DEBUG_IN_UNPACK_0_PARAM_FIFO_EMPTY_WORD {0, 13, 2, 0, 1, 0}
#define DEBUG_BUS_UNP1_DEBUG_DAISY_STOP_DEBUG_IN_UNPACK_0_PARAM_FIFO_EMPTY {DEBUG_BUS_UNP1_DEBUG_DAISY_STOP_DEBUG_IN_UNPACK_0_PARAM_FIFO_EMPTY_WORD, 29, 0x1}

//  location: tt_unpack_row name: UNP1_debug_daisy_stop
//  name: debug_in_unpack_0
//  name: param_fifo_full
#define DEBUG_BUS_UNP1_DEBUG_DAISY_STOP_DEBUG_IN_UNPACK_0_PARAM_FIFO_FULL_WORD {0, 13, 2, 0, 1, 0}
#define DEBUG_BUS_UNP1_DEBUG_DAISY_STOP_DEBUG_IN_UNPACK_0_PARAM_FIFO_FULL {DEBUG_BUS_UNP1_DEBUG_DAISY_STOP_DEBUG_IN_UNPACK_0_PARAM_FIFO_FULL_WORD, 28, 0x1}

//  location: tt_pack_row name: debug_daisy_stop_issue
//  name: i_debug_tile_pos_generator
//  name: o_tdma_packer_z_pos
#define DEBUG_BUS_O_TDMA_PACKER_Z_POS__15_0_WORD {6, 14, 0, 0, 1, 0}
#define DEBUG_BUS_O_TDMA_PACKER_Z_POS__15_0 {DEBUG_BUS_O_TDMA_PACKER_Z_POS__15_0_WORD, 16, 0xFFFF}

//  location: tt_pack_row name: debug_daisy_stop_issue
//  name: i_debug_tile_pos_generator
//  name: o_tdma_packer_z_pos
#define DEBUG_BUS_O_TDMA_PACKER_Z_POS__23_16_WORD {6, 14, 2, 0, 1, 0}
#define DEBUG_BUS_O_TDMA_PACKER_Z_POS__23_16 {DEBUG_BUS_O_TDMA_PACKER_Z_POS__23_16_WORD, 0, 0xFF}

//  location: tt_pack_row name: debug_daisy_stop_issue
//  name: i_debug_tile_pos_generator
//  name: o_tdma_packer_y_pos
#define DEBUG_BUS_O_TDMA_PACKER_Y_POS_WORD {6, 14, 0, 0, 1, 0}
#define DEBUG_BUS_O_TDMA_PACKER_Y_POS {DEBUG_BUS_O_TDMA_PACKER_Y_POS_WORD, 0, 0xFFFF}

//  location: tt_pack_row name: debug_daisy_stop_issue
//  name: debug_in_pack_2
//  name: packed_exps_p5
#define DEBUG_BUS_PACKED_EXPS_P5__29_0_WORD {5, 14, 2, 0, 1, 0}
#define DEBUG_BUS_PACKED_EXPS_P5__29_0 {DEBUG_BUS_PACKED_EXPS_P5__29_0_WORD, 2, 0x3FFFFFFF}

//  location: tt_pack_row name: debug_daisy_stop_issue
//  name: debug_in_pack_2
//  name: packed_exps_p5
#define DEBUG_BUS_PACKED_EXPS_P5__61_30_WORD {5, 14, 4, 0, 1, 0}
#define DEBUG_BUS_PACKED_EXPS_P5__61_30 {DEBUG_BUS_PACKED_EXPS_P5__61_30_WORD, 0, 0xFFFFFFFF}

//  location: tt_pack_row name: debug_daisy_stop_issue
//  name: debug_in_pack_2
//  name: packed_data_p5
#define DEBUG_BUS_PACKED_DATA_P5__27_0_WORD {4, 14, 6, 0, 1, 0}
#define DEBUG_BUS_PACKED_DATA_P5__27_0 {DEBUG_BUS_PACKED_DATA_P5__27_0_WORD, 4, 0xFFFFFFF}

//  location: tt_pack_row name: debug_daisy_stop_issue
//  name: debug_in_pack_2
//  name: packed_data_p5
#define DEBUG_BUS_PACKED_DATA_P5__59_28_WORD {5, 14, 0, 0, 1, 0}
#define DEBUG_BUS_PACKED_DATA_P5__59_28 {DEBUG_BUS_PACKED_DATA_P5__59_28_WORD, 0, 0xFFFFFFFF}

//  location: tt_pack_row name: debug_daisy_stop_issue
//  name: debug_in_pack_2
//  name: packed_data_p5
#define DEBUG_BUS_PACKED_DATA_P5__61_60_WORD {5, 14, 2, 0, 1, 0}
#define DEBUG_BUS_PACKED_DATA_P5__61_60 {DEBUG_BUS_PACKED_DATA_P5__61_60_WORD, 0, 0x3}

//  location: tt_pack_row name: debug_daisy_stop_issue
//  name: debug_in_pack_2
//  name: dram_data_fifo_rden
#define DEBUG_BUS_DRAM_DATA_FIFO_RDEN_WORD {4, 14, 6, 0, 1, 0}
#define DEBUG_BUS_DRAM_DATA_FIFO_RDEN {DEBUG_BUS_DRAM_DATA_FIFO_RDEN_WORD, 3, 0x1}

//  location: tt_pack_row name: debug_daisy_stop_issue
//  name: debug_in_pack_2
//  name: dram_rden
#define DEBUG_BUS_DRAM_RDEN_WORD {4, 14, 6, 0, 1, 0}
#define DEBUG_BUS_DRAM_RDEN {DEBUG_BUS_DRAM_RDEN_WORD, 2, 0x1}

//  location: tt_pack_row name: debug_daisy_stop_issue
//  name: debug_in_pack_2
//  name: dram_data_fifo_rden_p2
#define DEBUG_BUS_DRAM_DATA_FIFO_RDEN_P2_WORD {4, 14, 6, 0, 1, 0}
#define DEBUG_BUS_DRAM_DATA_FIFO_RDEN_P2 {DEBUG_BUS_DRAM_DATA_FIFO_RDEN_P2_WORD, 1, 0x1}

//  location: tt_pack_row name: debug_daisy_stop_issue
//  name: debug_in_pack_2
//  name: dram_rddata_phase_adj_asmbld_any_valid_p2
#define DEBUG_BUS_DRAM_RDDATA_PHASE_ADJ_ASMBLD_ANY_VALID_P2_WORD {4, 14, 6, 0, 1, 0}
#define DEBUG_BUS_DRAM_RDDATA_PHASE_ADJ_ASMBLD_ANY_VALID_P2 {DEBUG_BUS_DRAM_RDDATA_PHASE_ADJ_ASMBLD_ANY_VALID_P2_WORD, 0, 0x1}

//  location: tt_pack_row name: debug_daisy_stop_issue
//  name: debug_in_pack_2
//  name: pipe_data_clken_p2
#define DEBUG_BUS_PIPE_DATA_CLKEN_P2_WORD {4, 14, 4, 0, 1, 0}
#define DEBUG_BUS_PIPE_DATA_CLKEN_P2 {DEBUG_BUS_PIPE_DATA_CLKEN_P2_WORD, 31, 0x1}

//  location: tt_pack_row name: debug_daisy_stop_issue
//  name: debug_in_pack_2
//  name: pipe_clken_p2
#define DEBUG_BUS_PIPE_CLKEN_P2_WORD {4, 14, 4, 0, 1, 0}
#define DEBUG_BUS_PIPE_CLKEN_P2 {DEBUG_BUS_PIPE_CLKEN_P2_WORD, 30, 0x1}

//  location: tt_pack_row name: debug_daisy_stop_issue
//  name: debug_in_pack_2
//  name: pipe_clken_p3
#define DEBUG_BUS_PIPE_CLKEN_P3_WORD {4, 14, 4, 0, 1, 0}
#define DEBUG_BUS_PIPE_CLKEN_P3 {DEBUG_BUS_PIPE_CLKEN_P3_WORD, 29, 0x1}

//  location: tt_pack_row name: debug_daisy_stop_issue
//  name: debug_in_pack_2
//  name: pipe_busy_p3
#define DEBUG_BUS_PIPE_BUSY_P3_WORD {4, 14, 4, 0, 1, 0}
#define DEBUG_BUS_PIPE_BUSY_P3 {DEBUG_BUS_PIPE_BUSY_P3_WORD, 28, 0x1}

//  location: tt_pack_row name: debug_daisy_stop_issue
//  name: debug_in_pack_2
//  name: pipe_clken_p4
#define DEBUG_BUS_PIPE_CLKEN_P4_WORD {4, 14, 4, 0, 1, 0}
#define DEBUG_BUS_PIPE_CLKEN_P4 {DEBUG_BUS_PIPE_CLKEN_P4_WORD, 27, 0x1}

//  location: tt_pack_row name: debug_daisy_stop_issue
//  name: debug_in_pack_2
//  name: pipe_busy_p4
#define DEBUG_BUS_PIPE_BUSY_P4_WORD {4, 14, 4, 0, 1, 0}
#define DEBUG_BUS_PIPE_BUSY_P4 {DEBUG_BUS_PIPE_BUSY_P4_WORD, 26, 0x1}

//  location: tt_pack_row name: debug_daisy_stop_issue
//  name: debug_in_pack_2
//  name: pipe_busy_p5
#define DEBUG_BUS_PIPE_BUSY_P5_WORD {4, 14, 4, 0, 1, 0}
#define DEBUG_BUS_PIPE_BUSY_P5 {DEBUG_BUS_PIPE_BUSY_P5_WORD, 25, 0x1}

//  location: tt_pack_row name: debug_daisy_stop_issue
//  name: debug_in_pack_2
//  name: pipe_busy_p6
#define DEBUG_BUS_PIPE_BUSY_P6_WORD {4, 14, 4, 0, 1, 0}
#define DEBUG_BUS_PIPE_BUSY_P6 {DEBUG_BUS_PIPE_BUSY_P6_WORD, 24, 0x1}

//  location: tt_pack_row name: debug_daisy_stop_issue
//  name: debug_in_pack_2
//  name: pipe_busy_p6p7
#define DEBUG_BUS_PIPE_BUSY_P6P7_WORD {4, 14, 4, 0, 1, 0}
#define DEBUG_BUS_PIPE_BUSY_P6P7 {DEBUG_BUS_PIPE_BUSY_P6P7_WORD, 23, 0x1}

//  location: tt_pack_row name: debug_daisy_stop_issue
//  name: debug_in_pack_2
//  name: pipe_busy_p8
#define DEBUG_BUS_PIPE_BUSY_P8_WORD {4, 14, 4, 0, 1, 0}
#define DEBUG_BUS_PIPE_BUSY_P8 {DEBUG_BUS_PIPE_BUSY_P8_WORD, 22, 0x1}

//  location: tt_pack_row name: debug_daisy_stop_issue
//  name: debug_in_pack_2
//  name: in_param_fifo_empty
#define DEBUG_BUS_IN_PARAM_FIFO_EMPTY_WORD {4, 14, 4, 0, 1, 0}
#define DEBUG_BUS_IN_PARAM_FIFO_EMPTY {DEBUG_BUS_IN_PARAM_FIFO_EMPTY_WORD, 21, 0x1}

//  location: tt_pack_row name: debug_daisy_stop_issue
//  name: debug_in_pack_2
//  name: fmt_bw_expand_in_param_fifo_empty_p1
#define DEBUG_BUS_FMT_BW_EXPAND_IN_PARAM_FIFO_EMPTY_P1_WORD {4, 14, 4, 0, 1, 0}
#define DEBUG_BUS_FMT_BW_EXPAND_IN_PARAM_FIFO_EMPTY_P1 {DEBUG_BUS_FMT_BW_EXPAND_IN_PARAM_FIFO_EMPTY_P1_WORD, 20, 0x1}

//  location: tt_pack_row name: debug_daisy_stop_issue
//  name: debug_in_pack_2
//  name: &l1_req_fifo_empty
#define DEBUG_BUS__L1_REQ_FIFO_EMPTY_WORD {4, 14, 4, 0, 1, 0}
#define DEBUG_BUS__L1_REQ_FIFO_EMPTY {DEBUG_BUS__L1_REQ_FIFO_EMPTY_WORD, 19, 0x1}

//  location: tt_pack_row name: debug_daisy_stop_issue
//  name: debug_in_pack_2
//  name: dram_req_fifo_empty
#define DEBUG_BUS_DRAM_REQ_FIFO_EMPTY_WORD {4, 14, 4, 0, 1, 0}
#define DEBUG_BUS_DRAM_REQ_FIFO_EMPTY {DEBUG_BUS_DRAM_REQ_FIFO_EMPTY_WORD, 18, 0x1}

//  location: tt_pack_row name: debug_daisy_stop_issue
//  name: debug_in_pack_2
//  name: &l1_to_l1_pack_resp_fifo_empty
#define DEBUG_BUS__L1_TO_L1_PACK_RESP_FIFO_EMPTY_WORD {4, 14, 4, 0, 1, 0}
#define DEBUG_BUS__L1_TO_L1_PACK_RESP_FIFO_EMPTY {DEBUG_BUS__L1_TO_L1_PACK_RESP_FIFO_EMPTY_WORD, 17, 0x1}

//  location: tt_pack_row name: debug_daisy_stop_issue
//  name: debug_in_pack_2
//  name: &l1_to_l1_pack_resp_demux_fifo_empty
#define DEBUG_BUS__L1_TO_L1_PACK_RESP_DEMUX_FIFO_EMPTY_WORD {4, 14, 4, 0, 1, 0}
#define DEBUG_BUS__L1_TO_L1_PACK_RESP_DEMUX_FIFO_EMPTY {DEBUG_BUS__L1_TO_L1_PACK_RESP_DEMUX_FIFO_EMPTY_WORD, 16, 0x1}

//  location: tt_pack_row name: debug_daisy_stop_issue
//  name: debug_in_pack_2
//  name: |requester_busy
#define DEBUG_BUS__REQUESTER_BUSY_WORD {4, 14, 4, 0, 1, 0}
#define DEBUG_BUS__REQUESTER_BUSY {DEBUG_BUS__REQUESTER_BUSY_WORD, 15, 0x1}

//  location: tt_pack_row name: debug_daisy_stop_issue
//  name: debug_in_pack_2
//  name: stall_on_tile_end_drain_q
#define DEBUG_BUS_STALL_ON_TILE_END_DRAIN_Q_WORD {4, 14, 4, 0, 1, 0}
#define DEBUG_BUS_STALL_ON_TILE_END_DRAIN_Q {DEBUG_BUS_STALL_ON_TILE_END_DRAIN_Q_WORD, 14, 0x1}

//  location: tt_pack_row name: debug_daisy_stop_issue
//  name: debug_in_pack_2
//  name: stall_on_tile_end_drain_nxt
#define DEBUG_BUS_STALL_ON_TILE_END_DRAIN_NXT_WORD {4, 14, 4, 0, 1, 0}
#define DEBUG_BUS_STALL_ON_TILE_END_DRAIN_NXT {DEBUG_BUS_STALL_ON_TILE_END_DRAIN_NXT_WORD, 13, 0x1}

//  location: tt_pack_row name: debug_daisy_stop_issue
//  name: debug_in_pack_2
//  name: last_row_end_valid
#define DEBUG_BUS_LAST_ROW_END_VALID_WORD {4, 14, 4, 0, 1, 0}
#define DEBUG_BUS_LAST_ROW_END_VALID {DEBUG_BUS_LAST_ROW_END_VALID_WORD, 12, 0x1}

//  location: tt_pack_row name: debug_daisy_stop_issue
//  name: debug_in_pack_2
//  name: set_last_row_end_valid
#define DEBUG_BUS_SET_LAST_ROW_END_VALID_WORD {4, 14, 4, 0, 1, 0}
#define DEBUG_BUS_SET_LAST_ROW_END_VALID {DEBUG_BUS_SET_LAST_ROW_END_VALID_WORD, 11, 0x1}

//  location: tt_pack_row name: debug_daisy_stop_issue
//  name: debug_in_pack_2
//  name: data_conv_busy_c0
#define DEBUG_BUS_DATA_CONV_BUSY_C0_WORD {4, 14, 4, 0, 1, 0}
#define DEBUG_BUS_DATA_CONV_BUSY_C0 {DEBUG_BUS_DATA_CONV_BUSY_C0_WORD, 10, 0x1}

//  location: tt_pack_row name: debug_daisy_stop_issue
//  name: debug_in_pack_2
//  name: data_conv_busy_c1
#define DEBUG_BUS_DATA_CONV_BUSY_C1_WORD {4, 14, 4, 0, 1, 0}
#define DEBUG_BUS_DATA_CONV_BUSY_C1 {DEBUG_BUS_DATA_CONV_BUSY_C1_WORD, 9, 0x1}

//  location: tt_pack_row name: debug_daisy_stop_issue
//  name: debug_in_pack_2
//  name: in_param_fifo_full
#define DEBUG_BUS_IN_PARAM_FIFO_FULL_WORD {4, 14, 4, 0, 1, 0}
#define DEBUG_BUS_IN_PARAM_FIFO_FULL {DEBUG_BUS_IN_PARAM_FIFO_FULL_WORD, 8, 0x1}

//  location: tt_pack_row name: debug_daisy_stop_issue
//  name: debug_in_pack_2
//  name: fmt_bw_expand_in_param_fifo_full_p1
#define DEBUG_BUS_FMT_BW_EXPAND_IN_PARAM_FIFO_FULL_P1_WORD {4, 14, 4, 0, 1, 0}
#define DEBUG_BUS_FMT_BW_EXPAND_IN_PARAM_FIFO_FULL_P1 {DEBUG_BUS_FMT_BW_EXPAND_IN_PARAM_FIFO_FULL_P1_WORD, 7, 0x1}

//  location: tt_pack_row name: debug_daisy_stop_issue
//  name: debug_in_pack_2
//  name: |l1_req_fifo_full
#define DEBUG_BUS__L1_REQ_FIFO_FULL_WORD {4, 14, 4, 0, 1, 0}
#define DEBUG_BUS__L1_REQ_FIFO_FULL {DEBUG_BUS__L1_REQ_FIFO_FULL_WORD, 6, 0x1}

//  location: tt_pack_row name: debug_daisy_stop_issue
//  name: debug_in_pack_2
//  name: dram_req_fifo_full
#define DEBUG_BUS_DRAM_REQ_FIFO_FULL_WORD {4, 14, 4, 0, 1, 0}
#define DEBUG_BUS_DRAM_REQ_FIFO_FULL {DEBUG_BUS_DRAM_REQ_FIFO_FULL_WORD, 5, 0x1}

//  location: tt_pack_row name: debug_daisy_stop_issue
//  name: debug_in_pack_2
//  name: |l1_to_l1_pack_resp_fifo_full
#define DEBUG_BUS__L1_TO_L1_PACK_RESP_FIFO_FULL_WORD {4, 14, 4, 0, 1, 0}
#define DEBUG_BUS__L1_TO_L1_PACK_RESP_FIFO_FULL {DEBUG_BUS__L1_TO_L1_PACK_RESP_FIFO_FULL_WORD, 4, 0x1}

//  location: tt_pack_row name: debug_daisy_stop_issue
//  name: debug_in_pack_2
//  name: |l1_to_l1_pack_resp_demux_fifo_full
#define DEBUG_BUS__L1_TO_L1_PACK_RESP_DEMUX_FIFO_FULL_WORD {4, 14, 4, 0, 1, 0}
#define DEBUG_BUS__L1_TO_L1_PACK_RESP_DEMUX_FIFO_FULL {DEBUG_BUS__L1_TO_L1_PACK_RESP_DEMUX_FIFO_FULL_WORD, 3, 0x1}

//  location: tt_pack_row name: debug_daisy_stop_issue
//  name: debug_in_pack_2
//  name: reg_fifo_empty
#define DEBUG_BUS_REG_FIFO_EMPTY_WORD {4, 14, 4, 0, 1, 0}
#define DEBUG_BUS_REG_FIFO_EMPTY {DEBUG_BUS_REG_FIFO_EMPTY_WORD, 2, 0x1}

//  location: tt_pack_row name: debug_daisy_stop_issue
//  name: debug_in_pack_2
//  name: reg_fifo_full
#define DEBUG_BUS_REG_FIFO_FULL_WORD {4, 14, 4, 0, 1, 0}
#define DEBUG_BUS_REG_FIFO_FULL {DEBUG_BUS_REG_FIFO_FULL_WORD, 1, 0x1}

//  location: tt_pack_row name: debug_daisy_stop_issue
//  name: debug_in_pack_2
//  name: param_fifo_flush_wa_buffers_vld_p2
#define DEBUG_BUS_PARAM_FIFO_FLUSH_WA_BUFFERS_VLD_P2_WORD {4, 14, 4, 0, 1, 0}
#define DEBUG_BUS_PARAM_FIFO_FLUSH_WA_BUFFERS_VLD_P2 {DEBUG_BUS_PARAM_FIFO_FLUSH_WA_BUFFERS_VLD_P2_WORD, 0, 0x1}
