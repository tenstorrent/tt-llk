# Blackhole Pack Untilize Improvements — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Optimize Blackhole's pack untilize kernel from 2x16 to 4x16 datum rows per tile by enabling 2-context PACR with split interface selection.

**Architecture:** Add multi-context config replication (context 0 + context 1) to all packer configuration functions. Restructure the MOP in `llk_pack_untilize.h` to use tiles as the outer loop with 2 PACR instructions (one per context) in the inner loop, and a software loop for rows. Add `standalone` template parameter for the non-fused path (packer side only; unpacker changes are a follow-on).

**Tech Stack:** C++ header-only library targeting Blackhole Tensix coprocessor. Hardware register programming via `cfg_reg_rmw_tensix`, `TTI_WRCFG`, `TT_SETDMAREG` macros. MOP template via `ckernel::ckernel_template`. Tests via pytest + hardware simulator.

**Spec:** `docs/superpowers/specs/2026-04-01-pack-untilize-improvements-design.md`

---

### Task 1: Add `_2nd_AND_3rd_INTF_ACTIVE` constant to `p_pacr`

**Files:**
- Modify: `tt_llk_blackhole/common/inc/ckernel_instr_params.h:168`

- [ ] **Step 1: Add the new interface selection constant**

After the existing `_1st_AND_3rd_INTF_ACTIVE` line (line 168), add:

```cpp
    constexpr static std::uint32_t _2nd_AND_3rd_INTF_ACTIVE = 0b1100;
```

This enables packer interfaces 2 and 3 (bottom faces F2, F3 in strided mode).

- [ ] **Step 2: Verify compilation**

Run: `cd /proj_sw/user_dev/nstamatovic/tt-llk && grep -n '_2nd_AND_3rd_INTF_ACTIVE' tt_llk_blackhole/common/inc/ckernel_instr_params.h`

Expected: One line showing the new constant definition.

- [ ] **Step 3: Commit**

```bash
git add tt_llk_blackhole/common/inc/ckernel_instr_params.h
git commit -m "feat(blackhole): add _2nd_AND_3rd_INTF_ACTIVE packer interface constant

Add 0b1100 constant for selecting packer interfaces 2 and 3,
needed for 2-context pack untilize optimization (issue #61)."
```

---

### Task 2: Replicate packer config to context 1 in `set_packer_config`

**Files:**
- Modify: `tt_llk_blackhole/common/inc/cpack_common.h:311-312`

- [ ] **Step 1: Add context 1 config writes in `set_packer_config`**

In `set_packer_config<>()` (line 311-312), after the existing context 0 writes, add context 1 replication:

```cpp
    cfg[THCON_SEC0_REG1_Row_start_section_size_ADDR32 + 0] = config.val[0];
    cfg[THCON_SEC0_REG1_Row_start_section_size_ADDR32 + 2] = config.val[2];

    // Context 1: replicate identical config for multi-context pack untilize
    cfg[THCON_SEC0_REG8_Row_start_section_size_ADDR32 + 0] = config.val[0];
    cfg[THCON_SEC0_REG8_Row_start_section_size_ADDR32 + 2] = config.val[2];
```

- [ ] **Step 2: Run existing regression tests to verify no breakage**

Delegate to `/llk-test-runner` subagent:
```
TEST_PATH="tests/python_tests/test_zzz_pack_untilize.py"
```

Expected: All tests PASS (the extra config write is a no-op for single-context operations).

- [ ] **Step 3: Commit**

```bash
git add tt_llk_blackhole/common/inc/cpack_common.h
git commit -m "feat(blackhole): replicate packer config to context 1 in set_packer_config

Program THCON_SEC0_REG8 (context 1) with the same data format, exp
section size, and compression settings as THCON_SEC0_REG1 (context 0).
Required for 2-context pack untilize optimization (issue #61)."
```

---

### Task 3: Replicate context 1 config in remaining packer functions

**Files:**
- Modify: `tt_llk_blackhole/common/inc/cpack_common.h:201-210,226-256,346-431,433-501`

- [ ] **Step 1: Update `reconfigure_packer_l1_acc()` (line 201-210)**

After the existing context 0 RMW writes, add context 1:

```cpp
inline void reconfigure_packer_l1_acc(const std::uint32_t pack_l1_acc)
{
    // Stall to avoid clobbering current packer configuration
    TTI_STALLWAIT(p_stall::STALL_CFG, p_stall::PACK);

    // While packing, if all datums of a face are 0s, the packer will automatically set the zflags. For L1 accumulation mode, even if we pack out an entire face
    // of 0s, because the data we are accumulating with is unknown, we don't want to set the zflags.
    cfg_reg_rmw_tensix<THCON_SEC0_REG1_Disable_pack_zero_flags_RMW>(pack_l1_acc);
    cfg_reg_rmw_tensix<THCON_SEC0_REG1_Pack_L1_Acc_RMW>(pack_l1_acc);

    // Context 1: replicate for multi-context pack untilize
    cfg_reg_rmw_tensix<THCON_SEC0_REG8_Disable_pack_zero_flags_RMW>(pack_l1_acc);
    cfg_reg_rmw_tensix<THCON_SEC0_REG8_Pack_L1_Acc_RMW>(pack_l1_acc);
}
```

- [ ] **Step 2: Update `reconfigure_exp_threshold<>()` (line 255)**

After the existing context 0 threshold write, add context 1:

```cpp
    cfg_reg_rmw_tensix<THCON_SEC0_REG1_Row_start_section_size_ADDR32 + 3, 0, THRESHOLD_RMW_MASK>(threshold_rmw_data);

    // Context 1: replicate threshold for multi-context pack untilize
    cfg_reg_rmw_tensix<THCON_SEC0_REG8_Row_start_section_size_ADDR32 + 3, 0, THRESHOLD_RMW_MASK>(threshold_rmw_data);
```

- [ ] **Step 3: Update `reconfig_packer_data_format<>()` (lines 372, 414, 418, 421)**

After each context 0 write, add context 1 equivalent:

At line 372 (format config write):
```cpp
    TTI_WRCFG(p_gpr_pack::TMP_LO, p_cfg::WRCFG_32b, THCON_SEC0_REG1_Row_start_section_size_ADDR32 + 2);
    // Context 1: replicate format config
    TTI_WRCFG(p_gpr_pack::TMP_LO, p_cfg::WRCFG_32b, THCON_SEC0_REG8_Row_start_section_size_ADDR32 + 2);
```

At line 414 (BFP exp section size):
```cpp
        cfg_reg_rmw_tensix<THCON_SEC0_REG1_Exp_section_size_RMW>((partial_face ? 1 : num_faces));
        // Context 1
        cfg_reg_rmw_tensix<THCON_SEC0_REG8_Exp_section_size_RMW>((partial_face ? 1 : num_faces));
```

At line 418 (Lf8/Int8 zero):
```cpp
        TTI_WRCFG(p_gpr::ZERO, p_cfg::WRCFG_32b, THCON_SEC0_REG1_Row_start_section_size_ADDR32);
        // Context 1
        TTI_WRCFG(p_gpr::ZERO, p_cfg::WRCFG_32b, THCON_SEC0_REG8_Row_start_section_size_ADDR32);
```

At line 421 (Fp8 E4M3 mode):
```cpp
    cfg_reg_rmw_tensix<THCON_SEC0_REG1_Pac_LF8_4b_exp_RMW>((pack_dst_format & 0x1F) == static_cast<DataFormatType>(DataFormat::Fp8_e4m3) ? 1 : 0);
    // Context 1
    cfg_reg_rmw_tensix<THCON_SEC0_REG8_Pac_LF8_4b_exp_RMW>((pack_dst_format & 0x1F) == static_cast<DataFormatType>(DataFormat::Fp8_e4m3) ? 1 : 0);
```

- [ ] **Step 4: Update `configure_pack<>()` (line 457)**

After the Fp8 E4M3 mode write:
```cpp
    // Set Fp8 E4M3 mode for packer
    cfg_reg_rmw_tensix<THCON_SEC0_REG1_Pac_LF8_4b_exp_RMW>(((pack_dst_format & 0x1F) == (std::uint32_t)DataFormat::Fp8_e4m3) ? 1 : 0);
    // Context 1: replicate Fp8 E4M3 mode
    cfg_reg_rmw_tensix<THCON_SEC0_REG8_Pac_LF8_4b_exp_RMW>(((pack_dst_format & 0x1F) == (std::uint32_t)DataFormat::Fp8_e4m3) ? 1 : 0);
```

- [ ] **Step 5: Run regression tests**

Delegate to `/llk-test-runner`:
```
TEST_PATH="tests/python_tests/test_zzz_pack_untilize.py tests/python_tests/test_dense_pack_untilize.py"
```

Expected: All tests PASS.

- [ ] **Step 6: Commit**

```bash
git add tt_llk_blackhole/common/inc/cpack_common.h
git commit -m "feat(blackhole): replicate all packer config to context 1

Extend reconfigure_packer_l1_acc, reconfigure_exp_threshold,
reconfig_packer_data_format, and configure_pack to also program
THCON_SEC0_REG8 (context 1) registers. Keeps context 1 in sync
with context 0 for all data format and threshold settings.
Part of 2-context pack untilize optimization (issue #61)."
```

---

### Task 4: Add `program_packer_untilized_destination()` for 2-context L1 addressing

**Files:**
- Modify: `tt_llk_blackhole/common/inc/cpack_common.h:556-582`

- [ ] **Step 1: Replace commented-out function with working 2-context implementation**

Replace the existing commented-out `program_packer_untilized_destination` function (lines 556-582) with:

```cpp
// Program packer L1 destination for 2-context pack untilize.
// Context 0 (THCON_SEC0_REG1) writes top faces, Context 1 (THCON_SEC0_REG8) writes bottom faces.
inline void program_packer_untilized_destination(const std::uint32_t addr, const std::uint32_t bottom_face_offset)
{
    LLK_ASSERT(is_valid_L1_address(addr), "L1 address must be in valid L1 memory region");
    LLK_ASSERT(is_valid_L1_address(addr + bottom_face_offset), "Bottom face L1 address must be in valid L1 memory region");

    // Context 0: top faces
    std::uint32_t new_l1_addr0 = (1 << 31) | addr;
    TT_SETDMAREG(0, LOWER_HALFWORD(addr), 0, LO_16(p_gpr_pack::OUTPUT_ADDR));
    TT_SETDMAREG(0, UPPER_HALFWORD(new_l1_addr0), 0, HI_16(p_gpr_pack::OUTPUT_ADDR));

    // Context 1: bottom faces
    std::uint32_t addr1 = addr + bottom_face_offset;
    std::uint32_t new_l1_addr1 = (1 << 31) | addr1;
    TT_SETDMAREG(0, LOWER_HALFWORD(addr1), 0, LO_16(p_gpr_pack::OUTPUT_ADDR + 1));
    TT_SETDMAREG(0, UPPER_HALFWORD(new_l1_addr1), 0, HI_16(p_gpr_pack::OUTPUT_ADDR + 1));

    TTI_STALLWAIT(p_stall::STALL_CFG, p_stall::THCON);
    TTI_WRCFG(p_gpr_pack::OUTPUT_ADDR, 0, THCON_SEC0_REG1_L1_Dest_addr_ADDR32);
    TTI_WRCFG(p_gpr_pack::OUTPUT_ADDR + 1, 0, THCON_SEC0_REG8_L1_Dest_addr_ADDR32);

    // Reset high bit in GPRs for subsequent replay buffer arithmetic (ADDDMAREG)
    TT_SETDMAREG(0, UPPER_HALFWORD(addr), 0, HI_16(p_gpr_pack::OUTPUT_ADDR));
    TT_SETDMAREG(0, UPPER_HALFWORD(addr1), 0, HI_16(p_gpr_pack::OUTPUT_ADDR + 1));
    TTI_DMANOP;
}
```

- [ ] **Step 2: Verify compilation**

Run: `cd /proj_sw/user_dev/nstamatovic/tt-llk && grep -n 'program_packer_untilized_destination' tt_llk_blackhole/common/inc/cpack_common.h`

Expected: Shows the new function definition (no commented-out code).

- [ ] **Step 3: Commit**

```bash
git add tt_llk_blackhole/common/inc/cpack_common.h
git commit -m "feat(blackhole): implement program_packer_untilized_destination for 2-context

Replace commented-out stub with working implementation that programs
both THCON_SEC0_REG1 (context 0, top faces) and THCON_SEC0_REG8
(context 1, bottom faces) L1 destination addresses.
Part of 2-context pack untilize optimization (issue #61)."
```

---

### Task 5: Rewrite `_llk_pack_untilize_mop_config_` for 2-context PACR

**Files:**
- Modify: `tt_llk_blackhole/llk_lib/llk_pack_untilize.h:33-134`

This is the core MOP restructuring. When `num_faces == 4` and not dense/narrow, the MOP switches to:
- outer=block_ct_dim (tiles), inner=1
- LOOP_OP0=PACR(CNTX0, intf 0,1), LOOP_OP1=PACR(CNTX1, intf 2,3)
- END_OP0=INCADCZW(w+=1)
- Last outer: PACR(CNTX1, Last=1)

- [ ] **Step 1: Add the 2-context MOP path**

In `_llk_pack_untilize_mop_config_<>()`, add a new branch for the `num_faces == 4 && !dense && !narrow_row` case. The full rewritten function body:

```cpp
template <std::uint32_t block_ct_dim, bool narrow_row = false, bool dense = false>
inline void _llk_pack_untilize_mop_config_(const std::uint32_t face_r_dim = FACE_R_DIM, const std::uint32_t num_faces = 4)
{
    static_assert(!dense || (block_ct_dim % 2 == 0), "block_ct_dim must be even when dense");
    static_assert(!dense || (!narrow_row), "narrow_row must be false when dense");
    LLK_ASSERT(num_faces == 1 || num_faces == 2 || num_faces == 4, "num_faces must be 1, 2, or 4");
    LLK_ASSERT(!dense || (num_faces == 2), "num_faces must be 2 when dense");

    constexpr bool use_2_contexts = !dense && !narrow_row;

    if constexpr (use_2_contexts)
    {
        // 2-context path: tiles as outer loop, 2 PACRs per tile, software row loop.
        // MOP outer=block_ct_dim (tiles), inner=1.
        // LOOP_OP0: PACR(CNTX0, intf 0,1) — top faces F0,F1
        // LOOP_OP1: PACR(CNTX1, intf 2,3) — bottom faces F2,F3
        // END_OP0: INCADCZW(w+=1) — advance to next tile
        if (num_faces == 4)
        {
            ckernel::ckernel_template tmp(
                block_ct_dim,
                1,
                TT_OP_PACR(
                    p_pacr::CFG_CTXT_0,
                    p_pacr::NO_ROW_PAD_ZERO,
                    p_pacr::DST_ACCESS_STRIDED_MODE,
                    ADDR_MOD_0,
                    p_pacr::ADDR_CNT_CTXT_0,
                    0,
                    p_pacr::TWO_INTFS_ACTIVE,
                    0,
                    0,
                    p_pacr::NO_CTXT_CTRL,
                    0,
                    0),
                TT_OP_PACR(
                    p_pacr::CFG_CTXT_1,
                    p_pacr::NO_ROW_PAD_ZERO,
                    p_pacr::DST_ACCESS_STRIDED_MODE,
                    ADDR_MOD_0,
                    p_pacr::ADDR_CNT_CTXT_0,
                    0,
                    p_pacr::_2nd_AND_3rd_INTF_ACTIVE,
                    0,
                    0,
                    p_pacr::NO_CTXT_CTRL,
                    0,
                    0));

            tmp.set_end_op(TT_OP_INCADCZW(p_setadc::PAC, 0, 0, 1, 0)); // w += 1

            // On last tile: PACR(CNTX1) with Last=1 to close the megarow
            tmp.set_last_outer_loop_instr(TT_OP_PACR(
                p_pacr::CFG_CTXT_1,
                p_pacr::NO_ROW_PAD_ZERO,
                p_pacr::DST_ACCESS_STRIDED_MODE,
                ADDR_MOD_0,
                p_pacr::ADDR_CNT_CTXT_0,
                0,
                p_pacr::_2nd_AND_3rd_INTF_ACTIVE,
                0,
                0,
                p_pacr::NO_CTXT_CTRL,
                0,
                1)); // Last=1

            // Replay buffer: update both context L1 addresses per row
            const std::uint32_t replay_buf_len = 7;
            load_replay_buf(
                ckernel::packer::replay_buf_offset,
                replay_buf_len,
                []
                {
                    TTI_ADDDMAREG(0, p_gpr_pack::OUTPUT_ADDR, p_gpr_pack::OUTPUT_ADDR, p_gpr_pack::OUTPUT_ADDR_OFFSET);
                    TTI_ADDDMAREG(0, p_gpr_pack::OUTPUT_ADDR + 1, p_gpr_pack::OUTPUT_ADDR + 1, p_gpr_pack::OUTPUT_ADDR_OFFSET);
                    TTI_STALLWAIT(p_stall::STALL_CFG, p_stall::THCON);
                    TTI_WRCFG(p_gpr_pack::OUTPUT_ADDR, 0, THCON_SEC0_REG1_L1_Dest_addr_ADDR32);
                    TTI_WRCFG(p_gpr_pack::OUTPUT_ADDR + 1, 0, THCON_SEC0_REG8_L1_Dest_addr_ADDR32);
                    TTI_NOP;
                    TTI_NOP;
                });

            tmp.program();
        }
        else
        {
            // num_faces == 1 or 2: single-context path (existing behavior)
            constexpr std::uint32_t MOP_INNER_LOOP = block_ct_dim;
            const std::uint32_t MOP_OUTER_LOOP     = face_r_dim;

            const std::uint32_t PACK_INTF_SEL = (num_faces == 1) ? p_pacr::SINGLE_INTF_ACTIVE : p_pacr::TWO_INTFS_ACTIVE;

            ckernel::ckernel_template tmp(
                MOP_OUTER_LOOP,
                MOP_INNER_LOOP,
                TT_OP_INCADCZW(p_setadc::PAC, 0, 0, 1, 0),
                TT_OP_PACR(
                    p_pacr::CFG_CTXT_0,
                    p_pacr::NO_ROW_PAD_ZERO,
                    p_pacr::DST_ACCESS_STRIDED_MODE,
                    ADDR_MOD_0,
                    p_pacr::ADDR_CNT_CTXT_0,
                    0,
                    PACK_INTF_SEL,
                    0,
                    0,
                    p_pacr::NO_CTXT_CTRL,
                    0,
                    0));

            tmp.set_start_op(TT_OP_ADDRCRZW(p_setadc::PAC, 0, 0, 0, 0, 0b0010));

            const std::uint32_t replay_buf_len = 4;
            load_replay_buf(
                ckernel::packer::replay_buf_offset,
                replay_buf_len,
                []
                {
                    TTI_ADDDMAREG(0, p_gpr_pack::OUTPUT_ADDR, p_gpr_pack::OUTPUT_ADDR, p_gpr_pack::OUTPUT_ADDR_OFFSET);
                    TTI_STALLWAIT(p_stall::STALL_CFG, p_stall::THCON);
                    TTI_WRCFG(p_gpr_pack::OUTPUT_ADDR, 0, THCON_SEC0_REG1_L1_Dest_addr_ADDR32);
                    TTI_NOP;
                });

            tmp.set_end_ops(TT_OP_INCADCXY(p_setadc::PAC, 0, 0, 1, 0), lltt::replay_insn(ckernel::packer::replay_buf_offset, replay_buf_len));

            std::uint32_t last_loop_op = TT_OP_PACR(
                p_pacr::CFG_CTXT_0,
                p_pacr::NO_ROW_PAD_ZERO,
                p_pacr::DST_ACCESS_STRIDED_MODE,
                ADDR_MOD_0,
                p_pacr::ADDR_CNT_CTXT_0,
                0,
                PACK_INTF_SEL,
                0,
                0,
                p_pacr::NO_CTXT_CTRL,
                0,
                1);

            tmp.set_last_inner_loop_instr(last_loop_op);
            tmp.set_last_outer_loop_instr(last_loop_op);

            tmp.program();
        }
    }
    else
    {
        // Dense or narrow_row: existing paths unchanged
        constexpr std::uint32_t MOP_INNER_LOOP = dense ? block_ct_dim / 2 : block_ct_dim;
        const std::uint32_t MOP_OUTER_LOOP     = face_r_dim;

        const std::uint32_t PACK_INTF_SEL = (dense)                          ? p_pacr::ALL_INTF_ACTIVE
                                            : (narrow_row || num_faces == 1) ? p_pacr::SINGLE_INTF_ACTIVE
                                                                             : p_pacr::TWO_INTFS_ACTIVE;

        ckernel::ckernel_template tmp(
            MOP_OUTER_LOOP,
            MOP_INNER_LOOP,
            TT_OP_INCADCZW(p_setadc::PAC, 0, 0, 1, 0),
            TT_OP_PACR(
                p_pacr::CFG_CTXT_0,
                p_pacr::NO_ROW_PAD_ZERO,
                p_pacr::DST_ACCESS_STRIDED_MODE,
                ADDR_MOD_0,
                p_pacr::ADDR_CNT_CTXT_0,
                0,
                PACK_INTF_SEL,
                0,
                0,
                p_pacr::NO_CTXT_CTRL,
                0,
                0));

        tmp.set_start_op(TT_OP_ADDRCRZW(p_setadc::PAC, 0, 0, 0, 0, 0b0010));

        const std::uint32_t replay_buf_len = 4;
        load_replay_buf(
            ckernel::packer::replay_buf_offset,
            replay_buf_len,
            []
            {
                TTI_ADDDMAREG(0, p_gpr_pack::OUTPUT_ADDR, p_gpr_pack::OUTPUT_ADDR, p_gpr_pack::OUTPUT_ADDR_OFFSET);
                TTI_STALLWAIT(p_stall::STALL_CFG, p_stall::THCON);
                TTI_WRCFG(p_gpr_pack::OUTPUT_ADDR, 0, THCON_SEC0_REG1_L1_Dest_addr_ADDR32);
                TTI_NOP;
            });

        tmp.set_end_ops(TT_OP_INCADCXY(p_setadc::PAC, 0, 0, 1, 0), lltt::replay_insn(ckernel::packer::replay_buf_offset, replay_buf_len));

        std::uint32_t last_loop_op = TT_OP_PACR(
            p_pacr::CFG_CTXT_0,
            p_pacr::NO_ROW_PAD_ZERO,
            p_pacr::DST_ACCESS_STRIDED_MODE,
            ADDR_MOD_0,
            p_pacr::ADDR_CNT_CTXT_0,
            0,
            PACK_INTF_SEL,
            0,
            0,
            p_pacr::NO_CTXT_CTRL,
            0,
            1);

        tmp.set_last_inner_loop_instr(last_loop_op);
        tmp.set_last_outer_loop_instr(last_loop_op);

        tmp.program();
    }
}
```

- [ ] **Step 2: Commit**

```bash
git add tt_llk_blackhole/llk_lib/llk_pack_untilize.h
git commit -m "feat(blackhole): rewrite pack untilize MOP for 2-context PACR

When num_faces==4 (non-dense, non-narrow): use tiles as MOP outer loop
with 2 PACR instructions per tile (CNTX0 for top faces F0/F1, CNTX1
for bottom faces F2/F3). INCADCZW in END_OP advances W between tiles.
Replay buffer expanded to 7 instructions for dual-context L1 updates.
Existing paths for num_faces<=2, dense, and narrow_row unchanged.
Part of issue #61."
```

---

### Task 6: Rewrite `_llk_pack_untilize_` execution for 2-context software row loop

**Files:**
- Modify: `tt_llk_blackhole/llk_lib/llk_pack_untilize.h:195-243`

- [ ] **Step 1: Rewrite the execution function**

Replace `_llk_pack_untilize_()` with the 2-context execution path:

```cpp
template <
    std::uint32_t block_ct_dim,
    std::uint32_t full_ct_dim        = block_ct_dim,
    bool narrow_row                  = false,
    std::uint32_t tile_dst_ct_offset = 0,
    bool dense                       = false>
inline void _llk_pack_untilize_(
    const std::uint32_t address,
    const std::uint32_t pack_dst_format,
    const std::uint32_t face_r_dim            = FACE_R_DIM,
    const std::uint32_t num_faces             = 4,
    const std::uint32_t tile_dst_rt_offset    = 0)
{
    static_assert(block_ct_dim <= (dense ? 16 : 8), "block_ct_dim must be <= 8 when not dense, <= 16 when dense");
    static_assert(!dense || (block_ct_dim % 2 == 0), "block_ct_dim must be even when dense");
    static_assert(!dense || (!narrow_row), "narrow_row must be false when dense");
    LLK_ASSERT(num_faces == 1 || num_faces == 2 || num_faces == 4, "num_faces must be 1, 2, or 4");
    LLK_ASSERT(!dense || (num_faces == 2), "num_faces must be 2 when dense");

    constexpr bool use_2_contexts = !dense && !narrow_row;
    const std::uint32_t tile_dst_offset = tile_dst_ct_offset + tile_dst_rt_offset;

    if constexpr (use_2_contexts)
    {
        if (num_faces == 4)
        {
            // 2-context path: program both context L1 addresses, then software row loop.
            const std::uint32_t bottom_face_offset = SCALE_DATUM_SIZE(pack_dst_format, full_ct_dim * TILE_C_DIM * face_r_dim);
            program_packer_untilized_destination(address, bottom_face_offset);

            constexpr std::uint32_t replay_buf_len = 7;

            for (std::uint32_t row = 0; row < face_r_dim; row++)
            {
                TT_SETADC(p_setadc::PAC, p_setadc::CH_0, p_setadc::SET_W, tile_dst_offset);
                ckernel::ckernel_template::run();
                TTI_INCADCXY(p_setadc::PAC, 0, 0, 1, 0); // Y += 1
                lltt::replay(ckernel::packer::replay_buf_offset, replay_buf_len);
            }
        }
        else
        {
            // num_faces == 1 or 2: single-context path (existing behavior)
            program_packer_destination(address);
            const std::uint32_t num_faces_per_rdim_tile = (num_faces > 2) ? 2 : 1;

            TTI_SETADCZW(p_setadc::PAC, 0, 0, 0, 0, 0b0001);
            TT_SETADC(p_setadc::PAC, p_setadc::CH_0, p_setadc::SET_W, (15 + tile_dst_offset) & 0xF);
            TTI_SETADCXY(p_setadc::PAC, 0, 0, 0, 0, 0b0011);

            for (std::uint32_t face = 0; face < num_faces_per_rdim_tile; face++)
            {
                ckernel::ckernel_template::run();
                TTI_INCADCZW(p_setadc::PAC, 0, 0, 0, 1);
                TTI_SETADCXY(p_setadc::PAC, 0, 0, 0, 0, 0b0010);
            }

            TTI_SETADCZW(p_setadc::PAC, 0, 0, 0, 0, 0b0101);
            set_dst_write_addr(tile_dst_offset);
        }
    }
    else
    {
        // Dense or narrow_row: existing path unchanged
        program_packer_destination(address);
        const std::uint32_t num_faces_per_rdim_tile = (num_faces > 2) ? 2 : 1;

        TTI_SETADCZW(p_setadc::PAC, 0, 0, 0, 0, 0b0001);
        TT_SETADC(p_setadc::PAC, p_setadc::CH_0, p_setadc::SET_W, (15 + tile_dst_offset) & 0xF);
        TTI_SETADCXY(p_setadc::PAC, 0, 0, 0, 0, 0b0011);

        for (std::uint32_t face = 0; face < num_faces_per_rdim_tile; face++)
        {
            ckernel::ckernel_template::run();
            TTI_INCADCZW(p_setadc::PAC, 0, 0, 0, 1);
            TTI_SETADCXY(p_setadc::PAC, 0, 0, 0, 0, 0b0010);
        }

        TTI_SETADCZW(p_setadc::PAC, 0, 0, 0, 0, 0b0101);
        set_dst_write_addr(tile_dst_offset);
    }
}
```

- [ ] **Step 2: Commit**

```bash
git add tt_llk_blackhole/llk_lib/llk_pack_untilize.h
git commit -m "feat(blackhole): rewrite pack untilize execution for 2-context row loop

When num_faces==4 (non-dense, non-narrow): program both context L1
addresses via program_packer_untilized_destination, then iterate over
rows with a software loop calling MOP run per row. Eliminates the
face loop for 4-face tiles. Existing paths preserved for other cases.
Part of issue #61."
```

---

### Task 7: Run full regression test suite

**Files:**
- Test: `tests/python_tests/test_zzz_pack_untilize.py`
- Test: `tests/python_tests/test_dense_pack_untilize.py`
- Test: `tests/python_tests/test_matmul_pack_untilize.py`

- [ ] **Step 1: Run main pack untilize tests**

Delegate to `/llk-test-runner`:
```
TEST_PATH="tests/python_tests/test_zzz_pack_untilize.py"
```

Expected: All tests PASS. This exercises the new 2-context path for num_faces=4 cases and the preserved single-context path for num_faces<=2.

- [ ] **Step 2: Run dense pack untilize tests**

Delegate to `/llk-test-runner`:
```
TEST_PATH="tests/python_tests/test_dense_pack_untilize.py"
```

Expected: All tests PASS (dense mode path unchanged).

- [ ] **Step 3: Run matmul fused pack untilize tests**

Delegate to `/llk-test-runner`:
```
TEST_PATH="tests/python_tests/test_matmul_pack_untilize.py"
```

Expected: All tests PASS (fused operation compatibility).

- [ ] **Step 4: If any test fails, debug and fix**

Read test output carefully. Common issues to check:
- L1 address offset calculation (`bottom_face_offset`) — verify it matches `face_r_dim * full_ct_dim * TILE_C_DIM * datum_size`
- Last bit behavior — if rows aren't closed properly, add explicit close PACR after the MOP run
- Replay buffer conflicts — if 7 instructions overflow, check that `replay_buf_offset + 7 <= 32`
- W counter initial value — verify `tile_dst_offset` is correct (not `(15 + tile_dst_offset) & 0xF` which was for the old ADDRCRZW scheme)

---

### Task 8: Add `standalone` template parameter (non-fused packer side)

**Files:**
- Modify: `tt_llk_blackhole/llk_lib/llk_pack_untilize.h`

- [ ] **Step 1: Add `standalone` template parameter to init, mop_config, and execute**

Add `bool standalone = false` to all three function templates. When `standalone=true` and `num_faces==4`, the only difference is the `bottom_face_offset` calculation:

In `_llk_pack_untilize_()`, change the offset calculation:
```cpp
            // Fused: bottom faces start after face_r_dim rows of full-width output
            // Standalone: same calculation (packer layout is the same; unpacker changes are separate)
            const std::uint32_t bottom_face_offset = SCALE_DATUM_SIZE(pack_dst_format, full_ct_dim * TILE_C_DIM * face_r_dim);
```

Note: For the packer side, the standalone offset is the same as fused. The L1 layout difference is handled by the unpacker feeding order. The template parameter is added now for forward compatibility.

In `_llk_pack_untilize_mop_config_<>()` and `_llk_pack_untilize_init_<>()`, add `bool standalone = false` to the template parameter list and forward it. No behavioral change yet.

- [ ] **Step 2: Verify compilation and run tests**

Delegate to `/llk-test-runner`:
```
TEST_PATH="tests/python_tests/test_zzz_pack_untilize.py"
```

Expected: All tests PASS (default `standalone=false` unchanged behavior).

- [ ] **Step 3: Commit**

```bash
git add tt_llk_blackhole/llk_lib/llk_pack_untilize.h
git commit -m "feat(blackhole): add standalone template parameter for non-fused untilize

Add bool standalone=false to pack untilize init, mop_config, and execute
functions. Currently a no-op placeholder for the non-fused path's packer
side. The unpacker-side standalone changes are a follow-on task.
Part of issue #61."
```

---

### Task 9: Run performance benchmark

**Files:**
- Test: `tests/python_tests/perf_pack_untilize.py`

- [ ] **Step 1: Run performance test**

Delegate to `/llk-test-runner` (perf tests must run alone):
```
TEST_PATH="tests/python_tests/perf_pack_untilize.py"
```

Expected: Performance data showing cycle counts. Compare against baseline to verify the 2-context path doesn't regress.

- [ ] **Step 2: Document results**

Note the cycle count difference in the commit message or PR description. The 2-context path should show improvement for num_faces=4 workloads due to eliminated face loop, though the software row loop adds some overhead.

- [ ] **Step 3: Final commit if any performance-driven adjustments were made**

```bash
git add -u
git commit -m "perf(blackhole): tune pack untilize 2-context path based on benchmark

[Include specific findings from perf test]"
```
