# Blackhole Pack Untilize Improvements — Design Spec

**Issue:** [tt-llk#61](https://github.com/tenstorrent/tt-llk/issues/61)
**Date:** 2026-04-01
**Status:** Draft

## Problem Statement

Blackhole's pack untilize kernel outputs only 2x16 datum rows per PACR instruction (using `TWO_INTFS_ACTIVE` with a single PACR context), while Wormhole B0 achieves 8x16. The bottleneck is that only `CFG_CTXT_0` is used, and the packer must iterate over top faces then bottom faces in separate passes.

## Solution Overview

Two optimization paths, selectable via template parameter:

1. **Fused path** (`standalone=false`, default): 2-context PACR with split interface selection. Compatible with fused operations (math/unpack running concurrently). Outputs 4x16 rows per tile iteration — a 2x improvement.

2. **Non-fused path** (`standalone=true`): Optimal face reordering for standalone untilize blocks. Maximally contiguous L1 output. Requires coordinated unpacker changes.

Both paths share common multi-context configuration infrastructure.

## Architecture

### Shared Infrastructure: Multi-Context Config Replication

All pack configuration registers must be programmed identically for contexts 0 and 1.

**Files modified:** `tt_llk_blackhole/common/inc/cpack_common.h`

#### `set_packer_config<>()`

Replicate config words to context 1:

```cpp
// Context 0 (existing)
cfg[THCON_SEC0_REG1_Row_start_section_size_ADDR32 + 0] = config.val[0];
cfg[THCON_SEC0_REG1_Row_start_section_size_ADDR32 + 2] = config.val[2];

// Context 1 (new)
cfg[THCON_SEC0_REG8_Row_start_section_size_ADDR32 + 0] = config.val[0];
cfg[THCON_SEC0_REG8_Row_start_section_size_ADDR32 + 2] = config.val[2];
```

#### `reconfigure_packer_l1_acc()`

```cpp
cfg_reg_rmw_tensix<THCON_SEC0_REG1_Disable_pack_zero_flags_RMW>(pack_l1_acc);
cfg_reg_rmw_tensix<THCON_SEC0_REG1_Pack_L1_Acc_RMW>(pack_l1_acc);
cfg_reg_rmw_tensix<THCON_SEC0_REG8_Disable_pack_zero_flags_RMW>(pack_l1_acc);  // new
cfg_reg_rmw_tensix<THCON_SEC0_REG8_Pack_L1_Acc_RMW>(pack_l1_acc);              // new
```

#### `reconfigure_exp_threshold<>()`

```cpp
cfg_reg_rmw_tensix<THCON_SEC0_REG1_Row_start_section_size_ADDR32 + 3, 0, MASK>(data);
cfg_reg_rmw_tensix<THCON_SEC0_REG8_Row_start_section_size_ADDR32 + 3, 0, MASK>(data);  // new
```

#### `reconfig_packer_data_format<>()`

Replicate format config write (`THCON_SEC0_REG8_Row_start_section_size_ADDR32 + 2`), exp section size, and Fp8 E4M3 mode to context 1.

#### `configure_pack<>()`

Replicate Fp8 E4M3 mode to context 1:
```cpp
cfg_reg_rmw_tensix<THCON_SEC0_REG8_Pac_LF8_4b_exp_RMW>(...);  // new
```

### GPR Register Usage

| GPR | Current Use | New Use |
|-----|-------------|---------|
| `OUTPUT_ADDR` (12) | Context 0 L1 address | Same |
| `OUTPUT_ADDR+1` (13) | Unused | Context 1 L1 address |
| `OUTPUT_ADDR_OFFSET` (50) | Row offset | Same (shared by both contexts) |

GPR 13 (`OUTPUT_ADDR+1`) is currently unused in `p_gpr_pack` and available for context 1's L1 address.

### Register Mapping

| PACR Context | Config Register Base | L1 Dest Addr Register |
|---|---|---|
| CFG_CTXT_0 | `THCON_SEC0_REG1_Row_start_section_size_ADDR32` | `THCON_SEC0_REG1_L1_Dest_addr_ADDR32` |
| CFG_CTXT_1 | `THCON_SEC0_REG8_Row_start_section_size_ADDR32` | `THCON_SEC0_REG8_L1_Dest_addr_ADDR32` |

---

## Fused Path (standalone=false)

### MOP Hardware Constraint

The MOP (Macro Operation Processor) supports exactly **2 inner loop instruction slots** (`LOOP_OP0`, `LOOP_OP1`). The 2-context approach needs 3 operations per tile (PACR_CNTX0, PACR_CNTX1, INCADCZW). Solution: restructure the MOP so tiles are the outer loop, with INCADCZW in END_OP, and use a software loop for rows.

### MOP Configuration

**File:** `tt_llk_blackhole/llk_lib/llk_pack_untilize.h`, `_llk_pack_untilize_mop_config_<>()`

When `num_faces == 4`, not dense, not narrow_row:

- **MOP_OUTER_LOOP** = `block_ct_dim` (tiles)
- **MOP_INNER_LOOP** = `1`
- **LOOP_OP0**: `PACR(CFG_CTXT_0, DST_STRIDED, ADDR_CNT_CTXT_0, intf_sel=0b0011, Last=0)` — top faces F0,F1
- **LOOP_OP1**: `PACR(CFG_CTXT_1, DST_STRIDED, ADDR_CNT_CTXT_0, intf_sel=0b1100, Last=0)` — bottom faces F2,F3
- **START_OP**: `NOP`
- **END_OP0**: `INCADCZW(PAC, w+=1)` — advance to next tile
- **END_OP1**: `NOP`
- **set_last_outer_loop_instr**: `PACR(CFG_CTXT_1, ..., Last=1)` — close the megarow on the last tile

Since `MOP_INNER_LOOP=1`, every iteration is the "last inner" iteration. The template's last-instruction replacement logic:
- Non-last outer iteration: `LOOP_OP1 = m_loop1_last_instr` = PACR(CNTX1, Last=0) (constructor default)
- Last outer iteration: `LOOP_OP1 = m_loop0_last_instr` = PACR(CNTX1, **Last=1**)

**MOP execution per tile:**
```
PACR(CNTX0, intf=0b0011, Last=0)  — pack top faces F0,F1 of tile[W]
PACR(CNTX1, intf=0b1100, Last=0)  — pack bottom faces F2,F3 of tile[W]
INCADCZW(w+=1)                     — advance W to next tile
```
On last tile: PACR(CNTX1) has Last=1 to close the megarow for all interfaces.

**Software loop for rows** (in `_llk_pack_untilize_`):
```cpp
for (uint32_t row = 0; row < face_r_dim; row++) {
    TT_SETADC(p_setadc::PAC, p_setadc::CH_0, p_setadc::SET_W, tile_dst_offset);
    ckernel_template::run();
    TTI_INCADCXY(p_setadc::PAC, 0, 0, 1, 0);  // Y += 1 (next row)
    lltt::replay(replay_buf_offset, replay_buf_len);  // update both L1 addrs
}
```

### Interface Selection

- `0b0011` = `TWO_INTFS_ACTIVE` — packer interfaces 0 and 1 active (dest rows 0 and 16 — F0, F1)
- `0b1100` — packer interfaces 2 and 3 active (dest rows 32 and 48 — F2, F3)

Note: `0b1100` is not currently named in `p_pacr`. Add a constant:
```cpp
constexpr static std::uint32_t _2nd_AND_3rd_INTF_ACTIVE = 0b1100;
```

### Replay Buffer (L1 Address Update)

Expanded from 4 to 7 instructions. Invoked per row in the software loop:

```cpp
load_replay_buf(replay_buf_offset, 7, [] {
    // Update both context L1 addresses
    TTI_ADDDMAREG(0, p_gpr_pack::OUTPUT_ADDR,   p_gpr_pack::OUTPUT_ADDR,   p_gpr_pack::OUTPUT_ADDR_OFFSET);
    TTI_ADDDMAREG(0, p_gpr_pack::OUTPUT_ADDR+1, p_gpr_pack::OUTPUT_ADDR+1, p_gpr_pack::OUTPUT_ADDR_OFFSET);
    TTI_STALLWAIT(p_stall::STALL_CFG, p_stall::THCON);
    TTI_WRCFG(p_gpr_pack::OUTPUT_ADDR,   0, THCON_SEC0_REG1_L1_Dest_addr_ADDR32);
    TTI_WRCFG(p_gpr_pack::OUTPUT_ADDR+1, 0, THCON_SEC0_REG8_L1_Dest_addr_ADDR32);
    TTI_NOP;
    TTI_NOP;
});
```

Replay buffer has 16 slots (offset 16-31 for pack). 7 instructions fits within the available space.

### Destination Programming

**`program_packer_untilized_destination()`** in `cpack_common.h` — activate the commented-out code:

```cpp
inline void program_packer_untilized_destination(
    const std::uint32_t addr,
    const std::uint32_t bottom_face_offset)
{
    // Context 0: top faces
    std::uint32_t addr0 = (1 << 31) | addr;
    TT_SETDMAREG(0, LOWER_HALFWORD(addr), 0, LO_16(p_gpr_pack::OUTPUT_ADDR));
    TT_SETDMAREG(0, UPPER_HALFWORD(addr0), 0, HI_16(p_gpr_pack::OUTPUT_ADDR));

    // Context 1: bottom faces
    std::uint32_t addr1_raw = addr + bottom_face_offset;
    std::uint32_t addr1 = (1 << 31) | addr1_raw;
    TT_SETDMAREG(0, LOWER_HALFWORD(addr1_raw), 0, LO_16(p_gpr_pack::OUTPUT_ADDR + 1));
    TT_SETDMAREG(0, UPPER_HALFWORD(addr1), 0, HI_16(p_gpr_pack::OUTPUT_ADDR + 1));

    TTI_STALLWAIT(p_stall::STALL_CFG, p_stall::THCON);
    TTI_WRCFG(p_gpr_pack::OUTPUT_ADDR, 0, THCON_SEC0_REG1_L1_Dest_addr_ADDR32);
    TTI_WRCFG(p_gpr_pack::OUTPUT_ADDR + 1, 0, THCON_SEC0_REG8_L1_Dest_addr_ADDR32);

    // Reset OUTPUT_ADDR high bit for replay buffer arithmetic
    TT_SETDMAREG(0, UPPER_HALFWORD(addr), 0, HI_16(p_gpr_pack::OUTPUT_ADDR));
    TT_SETDMAREG(0, UPPER_HALFWORD(addr1_raw), 0, HI_16(p_gpr_pack::OUTPUT_ADDR + 1));
    TTI_DMANOP;
}
```

### Init Changes

**`_llk_pack_untilize_init_<>()`:**

- `output_addr_offset` calculation stays the same: `SCALE_DATUM_SIZE(pack_dst_format, full_ct_dim * 2 * FACE_C_DIM)` — this is the row stride across all tiles for one face pair.
- Z-stride stays at `2 * face_r_dim * y_stride` for strided dest access.

### Execute Changes

**`_llk_pack_untilize_()`:**

When `num_faces == 4` and using 2-context mode:
- Compute `bottom_face_offset = SCALE_DATUM_SIZE(pack_dst_format, full_ct_dim * TILE_C_DIM * face_r_dim)` — skip all top-face rows to reach bottom faces.
- Call `program_packer_untilized_destination(address, bottom_face_offset)`
- Software loop over `face_r_dim` rows (see MOP Configuration section above)
- No face loop, no Z counter management

When `num_faces <= 2`: existing single-context behavior unchanged.

### Performance Analysis

**Current:** 2 face passes × 1 MOP run each = 2 MOP invocations, `2 × face_r_dim × block_ct_dim` PACR instructions total.

**New:** `face_r_dim` software loop iterations × 1 MOP run each = `face_r_dim` MOP invocations (typically 16), `face_r_dim × block_ct_dim × 2` PACR instructions total.

Total PACR count is identical (`32 × block_ct_dim` for default face_r_dim=16). The tradeoff is 16 MOP runs vs 2, adding ~80-160 cycles of MOP overhead. This is offset by:
1. Eliminating face loop overhead (Z counter setup/reset, Y counter reset between passes)
2. Better L1 write pattern (top and bottom faces written in the same pass)
3. Reduced instruction cache pressure

### Backward Compatibility

- `dense` mode: unchanged (already uses `ALL_INTF_ACTIVE` with context 0)
- `narrow_row`: unchanged (single interface)
- `num_faces == 1 or 2`: unchanged (single context)
- `num_faces == 4, non-dense`: **new 2-context path**

---

## Non-Fused Path (standalone=true)

### Concept

For standalone untilize (block of tiles, not fused with other ops), the L1 output is reordered for maximum contiguity: all top-face rows across tiles first, then all bottom-face rows.

Output order: T0F0_row0, T0F1_row0, T1F0_row0, T1F1_row0, ..., T0F2_row0, T0F3_row0, T1F2_row0, T1F3_row0, ...

### Packer-Side Changes

The packer MOP structure is identical to the fused path (2 PACR instructions per inner loop with split interfaces). The differences are:

1. **L1 offset calculation**: Context 1's base address is `base_addr + block_ct_dim * TILE_C_DIM * face_r_dim * datum_size` (all top-face data, then all bottom-face data).

2. **output_addr_offset**: Same per-row offset as fused path.

3. **Template parameter**: `standalone=true` flag triggers different offset calculations.

### Unpacker-Side Changes (Separate Work)

The unpacker (`llk_unpack_untilize.h`) needs a corresponding `standalone` mode that feeds tiles in the optimal order into dest. This is a separate but coordinated change — the packer design accommodates it via the template flag.

### Scope Decision

The packer changes for non-fused are minimal on top of the fused infrastructure (different offset calculation only). The unpacker changes are a follow-on task. This spec covers the packer side fully; the unpacker coordination is noted as a dependency.

---

## Files Modified

| File | Changes |
|---|---|
| `tt_llk_blackhole/common/inc/cpack_common.h` | Multi-context config replication in `set_packer_config`, `reconfigure_packer_l1_acc`, `reconfigure_exp_threshold`, `reconfig_packer_data_format`; new `program_packer_untilized_destination` |
| `tt_llk_blackhole/common/inc/ckernel_instr_params.h` | Add `_2nd_AND_3rd_INTF_ACTIVE = 0b1100` constant |
| `tt_llk_blackhole/llk_lib/llk_pack_untilize.h` | 2-context MOP config, expanded replay buffer, single-pass execution, `standalone` template parameter |
| `tt_llk_blackhole/llk_lib/llk_pack_common.h` | Fp8 E4M3 and other config replicated to context 1 in `configure_pack` |

## Test Plan

1. **Regression**: All existing pack_untilize tests must pass unchanged (fused path is default, backward compatible for num_faces <= 2)
2. **Fused 4-face validation**: `test_zzz_pack_untilize.py` with num_faces=4 exercises the new 2-context path
3. **Dense mode regression**: `test_dense_pack_untilize.py` unchanged
4. **Matmul fused**: `test_matmul_pack_untilize.py` validates fused operation compatibility
5. **Performance**: `perf_pack_untilize.py` to measure cycle count improvement
6. **Non-fused standalone**: New test parametrization with `standalone=true` (after unpacker changes)

## Risks

1. **MOP overhead from software row loop**: The 2-context MOP uses `face_r_dim` MOP invocations (typically 16) vs 2 in the current face-loop approach. Each MOP run adds ~5-10 cycles overhead for `mop_sync` and programming. Total overhead: ~80-160 extra cycles. This should be measured via `perf_pack_untilize.py` and compared against the benefit of eliminating the face loop.
2. **Last bit behavior with split interfaces**: The design relies on `Last=1` on PACR(CNTX1) closing the megarow for ALL interfaces (including CNTX0's interfaces 0,1). This matches packer hardware behavior where Last is a global row-close signal, but must be verified by testing. If incorrect, the fallback is to issue a separate close PACR after the MOP run.
3. **Replay buffer size**: 7 instructions (from 4) still fits in the 16-slot buffer. If other operations share the same replay buffer region, verify no overlap.
4. **Context 1 config staleness**: If `reconfig_packer_data_format` is called between pack operations, context 1 must also be updated. The design handles this by replicating all reconfig paths.
5. **GPR `OUTPUT_ADDR+1` availability**: GPR 13 is confirmed unused in `p_gpr_pack`. No conflict with existing register allocation. Wormhole B0 already uses `OUTPUT_ADDR+1` through `OUTPUT_ADDR+3` for its 4-packer model, validating this pattern.
6. **Non-fused unpacker dependency**: The standalone packer path is testable independently (correct L1 output layout), but full non-fused functionality requires unpacker changes.
