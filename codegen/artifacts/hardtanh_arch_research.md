# Hardtanh Architecture Research for Quasar

## 1. Operation Definition

Hardtanh is a clamping activation function:
```
hardtanh(x, min_val, max_val) = max_val   if x > max_val
                                min_val   if x < min_val
                                x         otherwise
```

Default parameters: min_val = -1.0, max_val = 1.0. Equivalent to `clamp(x, min_val, max_val)`.

---

## 2. SFPU Execution Model

**Source: Confluence page 1256423592 (Quasar/Trinity SFPU Micro-Architecture Spec)**

### Lane Layout
- Quasar SFPU is a **half-sized configuration**: 8 slices, 4 rows per slice = **32 SFPU lanes total**
- Each slice is connected to a corresponding SrcS Register slice and Dest Register slice
- SFP_ROWS = 2 (the number of rows processed per SFPU iteration in the software model)
- Instructions execute on all 32 lanes simultaneously (SIMD)

### Pipeline Stages
- SFPU operates in pipeline stages S2-S5 of the Tensix compute pipeline
- S2: Instruction Frontend (decode, LOADMACRO engine)
- S3: Most Simple EXU operations complete (SFPSETCC, SFPMOV, SFPLOADI, SFPABS, SFPGT, etc.)
- S3-S4: MAD EXU operations (SFPMAD, SFPADD, SFPMUL, SFPADDI) -- 2-cycle latency
- S4-S5: Store EXU -- SrcS stores complete in S4 (2-cycle latency), Dest stores in S5 (3-cycle latency)

### Instruction Throughput
- Most instructions: IPC = 1 (one per cycle)
- MAD-class instructions: IPC = 1, latency = 2 (fully pipelined, but result available after 2 cycles)
- SFPSWAP: IPC = 0.5, latency = 2 (requires idle cycle after each SWAP)

### Key Constraint: MAD-to-Simple Hazard
- SFPMAD/SFPADD/SFPMUL results write to LREGs in S4
- Simple EXU reads LREGs in S3
- Therefore: a NOP is required between a MAD instruction and a subsequent Simple instruction that reads the same LREG
- Example: SFPMAD -> NOP -> SFPSETCC (if SFPSETCC reads the MAD output)

---

## 3. Register File Layouts

### LREG GPRs (Source: page 1256423592, "LREG Storage" section)
- 8 LREG GPRs per lane, divided into 2 banks:
  - LREGS1: LREGs 0-3
  - LREGS2: LREGs 4-7
- Each LREG is 32 bits wide
- Bank selection: bit 2 of register index (0 -> LREGS1, 1 -> LREGS2)
- Constant registers (per-slice, not per-lane): LREG indices with MSB set

### Constant Registers (Source: page 1256423592)
- 6 constant registers per SFPU Slice (shared across all lanes in the slice)
- Constants 2-5 accessible via RG view by most instructions
- Used by LUT coefficient unpacking (RL view)
- Pre-loaded constants available in the ckernel framework:
  - `LCONST_0` = 0.0
  - `LCONST_1` = 1.0
  - `LCONST_neg1` = -1.0

### SrcS Registers (Source: Confluence page 141000706)
- 2 banks, each with 3 slices of registers
- Each slice: 2 instances of tt_reg_bank (4 rows x 16 columns x 16 bits)
- Unpacker writes data to slices 0-1; SFPU reads from these and writes to slice 2
- SrcS address bit 10 = 1 indicates SrcS (vs Dest when bit 10 = 0)
- Format selection: implied from unpacker format or controlled via sfpu_srcs_slice0_format/sfpu_srcs_slice1_format registers
- 16-bit and 32-bit addressing modes supported

### Dest Register (Source: Confluence page 195493892 and page 80674824)
- Two modes: 16-bit (1024 rows) and 32-bit (512 rows, pairs of physical rows)
- In 32-bit mode: physical row `i` stores MSBs, physical row `i + N/2` stores LSBs
- Supported storage formats:
  - FP16a: `{sgn, man[9:0], exp[4:0]}`
  - FP16b: `{sgn, man[6:0], exp[7:0]}`
  - FP32: MSBs `{sgn, man[22:16], exp[7:0]}`, LSBs `{man[15:0]}`
  - Int8 (signed): sign+magnitude `{sgn, 3'b0, mag[6:0], 5'd16}`
  - Int8 (unsigned): `{3'b0, val[7:0], 5'd16}`
  - Int16 (signed): sign+magnitude `{sgn, mag[14:0]}`
  - Int16 (unsigned): `{val[15:0]}`
  - Int32 (signed): sign+magnitude, split across two physical rows
- Formats NOT supported in Dest: Int32 unsigned, Tf19/Tf32, BFP, LF8

---

## 4. Instructions Required for Hardtanh

Hardtanh requires a clamping pattern. There are two primary implementation strategies:

### Strategy A: Conditional Execution (SFPSETCC/SFPGT + SFPMOV/SFPLOADI)
Uses comparison instructions to set condition codes, then conditionally overwrites values.

### Strategy B: Arithmetic Clamping (Blackhole reference approach)
The Blackhole reference uses arithmetic offsets: add(-neg_threshold), clamp to 0, add(pos_threshold - neg_threshold), clamp to 0, add(pos_threshold).

### Strategy C: SFPGT-based Comparison (preferred for Quasar)
Uses SFPGT to compare values against bounds and conditionally replace them. This is the cleanest approach and matches patterns used in existing Quasar kernels (hardsigmoid, relu_max, threshold).

### Detailed Instruction Reference

#### SFPLOAD (Opcode 0x70) -- Source: page 1170505767
- Encoding: MR
- Loads value from Dest or SrcS into RG[VD]
- Format conversion applied based on InstrMod
- InstrMod 0x0 = IMPLIED (uses implied format from Dest/SrcS)
- Latency: 1 cycle
- IPC: 1
- `Done` flag: toggles read bank ID for SrcS, updates dvalid for Dest
- Quasar TTI: `TTI_SFPLOAD(vd, instr_mod, addr_mod, addr, done)`

#### SFPLOADI (Opcode 0x71) -- Source: page 1170505767
- Encoding: MI
- Loads 16-bit immediate into RG[VD] with format conversion
- InstrMod 0x0 = FP16_B: converts to FP32 in LREG
- InstrMod 0x1 = FP16_A: converts to FP32 in LREG
- InstrMod 0x2 = UINT16: zero-extends to UINT32
- Latency: 1 cycle
- IPC: 1
- Quasar TTI: `TTI_SFPLOADI(vd, instr_mod, imm16)`

#### SFPSTORE (Opcode 0x72) -- Source: page 1170505767
- Encoding: MR
- Stores RG[VD] to Dest or SrcS with format conversion
- InstrMod 0x0 = IMPLIED
- Latency: 2 cycles (SrcS), 3 cycles (Dest)
- IPC: 1
- `Done` flag: toggles write bank ID
- IMPORTANT: Does NOT perform rounding or saturation during format conversion; use SFPSTOCHRND before SFPSTORE for faithful conversion
- Quasar TTI: `TTI_SFPSTORE(vd, done, addr_mod, addr, instr_mod)`

#### SFPSETCC (Opcode 0x7B) -- Source: page 1170505767
- Encoding: O2
- Sets CC.Res based on RG[VC] or Imm12
- InstrMod modes:
  - 0: CC.Res = 1 if RG[VC] is negative (sign bit check)
  - 1: CC.Res = Imm12[0] (immediate value)
  - 2: CC.Res = 1 if RG[VC] != 0
  - 4: CC.Res = 1 if RG[VC] is positive
  - 6: CC.Res = 1 if RG[VC] == 0
  - 8: CC.Res = !CC.Res (invert)
- Imm12[11]: Format select (0 = INT32, 1 = FP32/SMAG32)
- Latency: 1 cycle, IPC: 1
- Execution predicated by current LaneEnabled state
- Quasar TTI: `TTI_SFPSETCC(imm12, vc, instr_mod)`

#### SFPGT (Opcode 0x97) -- Source: page 1170505767
- Encoding: O2
- Tests if RG[VD] > RG[VC]
- Imm12[0]: Format select (0 = INT32, 1 = FP32)
- InstrMod bits:
  - [0]: CC Result Update Enable (1 = update CC.Res)
  - [1]: Stack Update Enable
  - [2]: Stack Update Mode (AND vs OR)
  - [3]: LREG Update Enable (writes 0xFFFFFFFF or 0x00000000 to RG[VD])
- Latency: 1 cycle, IPC: 1
- LREG update is conditional on SFPU Control Register row enable, NOT current CC state
- Quasar TTI: `TTI_SFPGT(imm12, vc, vd, instr_mod)`
- **IMPORTANT**: When using SFPGT with InstrMod[0]=1 (CC update), it sets CC.Res = 1 for lanes where RG[VD] > RG[VC]

#### SFPENCC (Opcode 0x8A) -- Source: page 1170505767
- Encoding: O2
- Directly sets CC.En and CC.Res
- Executes on ALL lanes regardless of current LaneEnabled state
- InstrMod[1:0]: CC Enable Source Select
  - 0: Keep previous CC.En
  - 1: Invert CC.En
  - 2: Set CC.En based on Imm12[0]
- InstrMod[3]: CC Result Source Select
  - 0: Set CC.Res = 1
  - 1: Set CC.Res based on Imm12[1]
- Common usages:
  - `TTI_SFPENCC(1, 2)` -> Enable CC (CC.En = 1 from Imm12[0]=1, CC.Res = 1)
  - `TTI_SFPENCC(0, 2)` -> Disable CC (CC.En = 0 from Imm12[0]=0, CC.Res = 1)
  - `TTI_SFPENCC(0, 0)` -> Reset CC.Res = 1 for all lanes (CC.En unchanged)
- Quasar TTI: `TTI_SFPENCC(imm12, instr_mod)`

#### SFPCOMPC (Opcode 0x8B) -- Source: page 1170505767
- Encoding: O2
- Complements or clears CC.Res depending on CC state
- Executes on ALL lanes regardless of LaneEnabled
- Used for implementing "else" branches in conditional execution
- Latency: 1 cycle, IPC: 1
- Quasar TTI: `TTI_SFPCOMPC(imm12, instr_mod)`

#### SFPMOV (Opcode 0x7C) -- Source: page 1170505767
- Encoding: O2
- Copies RG[VC] to RG[VD]
- InstrMod:
  - 0: Simple copy (conditional on LaneEnabled)
  - 1: Copy with sign inversion
  - 2: Unconditional copy (ignores LaneEnabled)
  - 8: Copy from RS[VC] to RG[VD]
- Latency: 1 cycle, IPC: 1
- Quasar TTI: `TTI_SFPMOV(vc, vd, instr_mod)` -- NOTE: argument order is (src_vc, dst_vd, mod)

#### SFPMAD (Opcode 0x84) / SFPADD (0x85) / SFPMUL (0x86) -- Source: page 1170505767
- Encoding: O4
- FMA operation: (SrcA * SrcB) + SrcC
- SFPADD: alias where one of VA/VB = 1.0
- SFPMUL: alias where VC = 0.0
- InstrMod bits: [0] = negate SrcA, [1] = negate SrcC, [2] = SrcA from RG[7], [3] = Dest to RG[7]
- Latency: 2 cycles, IPC: 1 (fully pipelined)
- Flushes subnormal inputs to 0 of same sign
- Quasar TTI: `TTI_SFPMAD(va, vb, vc, vd, instr_mod)`, `TTI_SFPADD(va, vb, vc, vd, instr_mod)`, `TTI_SFPMUL(va, vb, vc, vd, instr_mod)`

#### SFPNOP (Opcode varies) -- Source: page 1170505767
- No operation, used for pipeline hazard avoidance
- Quasar TTI: `TTI_SFPNOP(imm12, instr_mod, done)`

---

## 5. Recommended Implementation Strategy

Based on analysis of existing Quasar kernels (hardsigmoid in ckernel_sfpu_activations.h, relu_max in ckernel_sfpu_lrelu.h, threshold in ckernel_sfpu_threshold.h), the recommended approach uses **SFPGT for upper bound clamping** and **SFPSETCC for lower bound clamping**:

### Algorithm
```
// Per-iteration (processes SFP_ROWS = 2 rows of Dest):
1. SFPLOAD LREG0 from Dest           // load input value
2. SFPGT: if LREG0 > LREG_max_val -> set CC
3. SFPMOV LREG_max_val -> LREG0      // clamp to max where CC set
4. SFPENCC(0, 0)                      // reset CC for all lanes
5. SFPSETCC on LREG0 with negative check  // CC = 1 for negative values...
   -- but this checks sign, not comparison to min_val
```

Actually, for arbitrary min_val/max_val, we need two SFPGT comparisons:

### Revised Algorithm (two SFPGT comparisons)
```
// Pre-loop: load min_val into LREG2, max_val into LREG3
// Enable CC

// Per-iteration:
1. SFPLOAD LREG0 from Dest            // load x
2. SFPGT(fp32, LREG2, LREG0, cc=1)   // CC.Res=1 where LREG0 > LREG2 (x > max_val is wrong direction)
```

Wait -- SFPGT tests `RG[VD] > RG[VC]`. So:
- `TTI_SFPGT(0x1, vc=LREG2_max, vd=LREG0, 0x1)` -> CC.Res=1 where LREG0 > LREG2_max (x > max_val)
- Then SFPMOV to overwrite LREG0 with max_val where CC is set

For lower bound:
- Reset CC first
- `TTI_SFPGT(0x1, vc=LREG0, vd=LREG2_min, 0x1)` -> CC.Res=1 where LREG2_min > LREG0 (min_val > x, i.e., x < min_val)
- Then SFPMOV to overwrite LREG0 with min_val where CC is set

### Final Algorithm
```
// Pre-loop:
SFPLOADI LREG2 <- min_val (FP16_B, upper 16 bits of FP32 param)
SFPLOADI LREG3 <- max_val (FP16_B, upper 16 bits of FP32 param)
SFPENCC(1, 2)   // enable conditional execution

// Per-iteration:
SFPLOAD  LREG0 <- Dest[addr]

// Upper clamp: if x > max_val, x = max_val
SFPGT(0, LREG3, LREG0, 0x1)      // CC = (LREG0 > LREG3), i.e., x > max_val (INT32 mode, works for FP32)
SFPMOV(LREG3, LREG0, 0)           // copy max_val where CC set
SFPENCC(0, 0)                      // reset CC for all lanes

// Lower clamp: if x < min_val, x = min_val
SFPGT(0, LREG0, LREG2, 0x1)      // CC = (LREG2 > LREG0), i.e., min_val > x (INT32 mode, works for FP32)
SFPMOV(LREG2, LREG0, 0)           // copy min_val where CC set
SFPENCC(0, 0)                      // reset CC for all lanes

SFPSTORE LREG0 -> Dest[addr]
```

Note: Uses `imm12=0` (INT32 comparison mode) following the convention of all existing Quasar SFPGT usage. This works correctly for IEEE 754 FP32 bit patterns due to the monotonic mapping between FP32 and 2's complement representations.

This pattern is validated by the existing hardsigmoid implementation which uses identical SFPGT + SFPMOV + SFPENCC sequences for its upper bound clamp.

---

## 6. Data Format Support and Conversion Rules

### SFPU Internal Format (Source: page 1170505767, SFPLOAD/SFPSTORE sections)
- SFPU internally works in 32-bit formats: FP32, SMAG32 (sign-magnitude 32-bit integer), UINT32
- SFPLOAD converts from register file format to LREG format (always 32-bit)
- SFPSTORE converts from LREG format back to register file format
- Format conversion is controlled by InstrMod field on LOAD/STORE

### Implied Format System (Source: page 141000706, "Format selection" section)
- When InstrMod=0 (IMPLIED), format is derived from the unpacker's output format
- The implied format is captured when unpacker writes to SrcS/Dest
- This is the standard mode used by SFPU kernels (`p_sfpu::sfpmem::DEFAULT`)

### Format Support Matrix for Hardtanh

Hardtanh is a floating-point comparison/clamping operation. It uses SFPGT (which supports FP32 comparison via Imm12[0]=1) and SFPMOV (format-agnostic bitwise copy).

| Format | Enum Value | Applicable? | Notes |
|--------|-----------|-------------|-------|
| Float16 (FP16a) | 1 | Yes | Unpacked to FP32 in LREG; comparison in FP32 |
| Float16_b (FP16b/BF16) | 5 | Yes | Primary format; unpacked to FP32 in LREG |
| Float32 | 0 | Yes | Native LREG format; requires dest_acc=32-bit mode |
| Tf32 | 4 | Yes | Unpacked to FP32 in registers; stored as 19-bit in SrcAB |
| Int32 | 8 | Possible but atypical | SFPGT supports INT32 mode (Imm12[0]=0); requires dest_acc mode; clamping integers is valid |
| Int16 | 9 | Not recommended | Limited SFPU support; would need SMAG16 handling |
| Int8 | 14 | Not recommended | Sign-magnitude encoding complexity; limited use case |
| UInt8 | 17 | Not recommended | Unsigned; clamping unsigned is unusual |
| MxFp8R | 18 | Yes (L1 only) | Unpacked to Float16_b for math; comparison works on unpacked FP32 |
| MxFp8P | 20 | Yes (L1 only) | Unpacked to Float16_b for math; comparison works on unpacked FP32 |

### Recommended Test Formats
For a float clamping operation, the primary test formats are:
- **Float16_b** (most common SFPU format)
- **Float16** (FP16a variant)
- **Float32** (requires is_fp32_dest_acc_en)

MX formats (MxFp8R, MxFp8P) can also work since they unpack to Float16_b before SFPU processing.

### Format-Specific Constraints
1. **Float32/Tf32**: Requires `is_fp32_dest_acc_en = true` (32-bit Dest mode)
2. **MX formats**: L1 storage only; unpacked to Float16_b before reaching SFPU; repacked on output
3. **Int32**: Would work with SFPGT in INT32 mode (Imm12[0]=0), but not typical for hardtanh
4. **Subnormal handling**: SFPU flushes subnormal inputs to 0 in MAD operations; SFPGT does not flush subnormals but checks true zero for SMAG32

---

## 7. Pipeline Constraints and Instruction Ordering

### Critical Timing Rules (Source: page 1256423592)

1. **MAD-to-use latency**: SFPMAD/SFPADD/SFPMUL have 2-cycle latency. If a subsequent Simple EXU instruction reads the MAD output LREG, a NOP must be inserted between them.
   - Example: `SFPMAD -> NOP -> SFPSETCC` (if SFPSETCC reads MAD result)
   - For hardtanh: NOT needed because we don't use MAD instructions

2. **SFPCONFIG follow-up**: No instruction may follow SFPCONFIG on the next cycle (architectural constraint)

3. **SFPSWAP idle cycle**: SFPSWAP requires an idle cycle after each execution (IPC = 0.5)

4. **SFPGT is a Simple EXU instruction**: 1-cycle latency, completes in S3. No special pipeline constraints beyond normal CC rules.

5. **SFPMOV conditional execution**: When CC is enabled, SFPMOV only writes to lanes where LaneEnabled (CC.En && CC.Res) is true. This is the mechanism for conditional clamping.

6. **SFPENCC resets**: SFPENCC executes on ALL lanes regardless of current LaneEnabled. Used to reset CC state between comparisons.

### Instruction Ordering for Hardtanh (no hazards)
The hardtanh sequence uses only Simple EXU instructions (SFPGT, SFPMOV, SFPSETCC, SFPENCC) and Load/Store, all of which are 1-cycle latency. No NOPs are needed between them.

---

## 8. LOADMACRO Rules

**Source: page 1256423592, "LOADMACRO Engine" section**

LOADMACRO provides superscalar execution by scheduling up to 5 instructions in parallel. However, for hardtanh:
- The operation is simple enough that LOADMACRO is not needed
- Existing Quasar SFPU kernels (lrelu, elu, activations, threshold) do NOT use LOADMACRO
- LOADMACRO is typically used for more complex multi-instruction sequences that benefit from parallel execution

---

## 9. Blackhole Differences

### Reference Implementation (Blackhole: ckernel_sfpu_hardtanh.h)
- Uses **SFPI** high-level API: `vFloat`, `dst_reg`, `v_if`/`v_endif`
- Algorithm uses arithmetic offsets:
  1. `val += p0` (add negative of neg_threshold)
  2. `v_if (val < 0.0f) val = 0.0f` (clamp to 0 if below lower bound)
  3. `val += p1` (add -(pos_threshold - neg_threshold))
  4. `v_if (val >= 0.0f) val = 0.0f` (clamp to 0 if above upper bound)
  5. `val += p2` (add pos_threshold to restore)
- Parameters: p0 = -neg_threshold, p1 = -(pos_threshold - neg_threshold), p2 = -pos_threshold
- This is a clever arithmetic approach that avoids storing separate min/max constants
- Template parameters: `<bool APPROXIMATION_MODE, int ITERATIONS>`

### Test Harness (Blackhole: tests/hw_specific/blackhole/metal_sfpu/)
- Different test pattern: uses `calculate_hardtanh<APPROXIMATE, ITERATIONS>` template
- Takes uint param0, param1 (FP32 encoded min/max values split into HI16/LO16)
- Uses `TT_SFPLOADI` with `SFPLOADI_MOD0_LOWER`/`SFPLOADI_MOD0_UPPER` for 32-bit constant loading
- Uses `SFPSWAP` for min/max operations

### Key Differences for Quasar
1. **No SFPI**: Quasar does not use the SFPI high-level API (vFloat, dst_reg, v_if/v_endif). Instead, it uses direct TTI instruction macros (TTI_SFPLOAD, TTI_SFPGT, etc.)
2. **Different instruction encodings**: Quasar TTI macros have different argument ordering than Blackhole's TT_ macros
3. **SFP_ROWS = 2**: Quasar processes 2 rows per SFPU iteration (vs 4 on some other archs)
4. **Counter increment**: Uses `ckernel::math::_incr_counters_<0x0, 0x0, ckernel::math::SFP_ROWS, 0x0>()`
5. **No SfpuType::hardtanh in Quasar**: The Quasar SfpuType enum does not yet include hardtanh (would need to be added)
6. **Preferred approach**: Use SFPGT + SFPMOV pattern (matching hardsigmoid, relu_max, threshold) rather than Blackhole's arithmetic offset approach or SFPSWAP approach

---

## 10. Existing Quasar Pattern Reference

### Hardsigmoid Upper Clamp Pattern (from ckernel_sfpu_activations.h)
This is the exact pattern to reuse for hardtanh:
```cpp
// Upper clamp: if LREG0 > 1.0, set LREG0 = 1.0
TTI_SFPGT(0, p_sfpu::LREG6, p_sfpu::LREG0, 0x1);    // CC = (LREG0 > LREG6)
TTI_SFPMOV(p_sfpu::LREG6, p_sfpu::LREG0, 0);          // copy 1.0 where CC set
TTI_SFPENCC(0, 0);                                      // reset CC
```

### Hardsigmoid Lower Clamp Pattern (sign-based)
```cpp
// Lower clamp: if LREG0 < 0, set LREG0 = 0 (ReLU)
TTI_SFPSETCC(0, p_sfpu::LREG0, 0);                     // CC = 1 where negative
TTI_SFPLOADI(p_sfpu::LREG0, 0, 0);                     // load 0 into negative lanes
TTI_SFPENCC(0, 0);                                      // reset CC
```

### For hardtanh, we need a generalized lower clamp (not just to 0):
```cpp
// Lower clamp: if x < min_val, set x = min_val
TTI_SFPGT(0, p_sfpu::LREG0, p_sfpu::LREG2, 0x1);     // CC = (LREG2 > LREG0), i.e., min > x
TTI_SFPMOV(p_sfpu::LREG2, p_sfpu::LREG0, 0);          // copy min_val where CC set
TTI_SFPENCC(0, 0);                                      // reset CC
```

Note on SFPGT argument ordering:
- `TTI_SFPGT(imm12, vc, vd, instr_mod)` tests `RG[VD] > RG[VC]`
- So `TTI_SFPGT(0x1, LREG_threshold, LREG_value, 0x1)` sets CC when value > threshold
- And `TTI_SFPGT(0x1, LREG_value, LREG_threshold, 0x1)` sets CC when threshold > value (i.e., value < threshold)

### Standard Quasar SFPU Kernel Structure
```cpp
// Init function: load constants
inline void _init_hardtanh_() { ... }

// Per-row calculation
inline void _calculate_hardtanh_sfp_rows_() { ... }

// Main function with iteration loop
inline void _calculate_hardtanh_(const int iterations, params...) {
    // Load constants
    TTI_SFPENCC(1, 2);  // enable CC
    #pragma GCC unroll 8
    for (int d = 0; d < iterations; d++) {
        _calculate_hardtanh_sfp_rows_();
        ckernel::math::_incr_counters_<0x0, 0x0, ckernel::math::SFP_ROWS, 0x0>();
    }
    TTI_SFPENCC(0, 2);  // disable CC
}
```

---

## 11. Quasar-Specific Constants and Macros

### p_sfpu Namespace Constants
- `p_sfpu::LREG0` through `p_sfpu::LREG7` -- LREG indices
- `p_sfpu::LCONST_0` -- constant 0.0
- `p_sfpu::LCONST_1` -- constant 1.0
- `p_sfpu::LCONST_neg1` -- constant -1.0
- `p_sfpu::sfpmem::DEFAULT` -- default memory access mode
- `ADDR_MOD_7` -- address modifier 7 (standard for SFPU load/store)

### SFPGT Imm12 Format Encoding
- Imm12[0] = 0: INT32 comparison (2's complement)
- Imm12[0] = 1: FP32 comparison (IEEE 754 with NaN/Inf handling)
- **IMPORTANT**: All existing Quasar SFPU kernels use `imm12=0` (INT32 mode) even for FP32 data. This works because IEEE 754 bit patterns are ordered identically to 2's complement integers for positive values, and the 2's complement comparison correctly handles negative FP32 values too. The only difference is NaN/Inf handling (INT32 mode does not report NaN/Inf flags). Follow existing convention: use `imm12=0` for SFPGT.

### TTI Macro Argument Orders (from existing Quasar code)
- `TTI_SFPLOAD(vd, instr_mod, addr_mod, addr, done)`
- `TTI_SFPLOADI(vd, instr_mod, imm16)`
- `TTI_SFPSTORE(vd, done, addr_mod, addr, instr_mod)`
- `TTI_SFPSETCC(imm12, vc, instr_mod)`
- `TTI_SFPGT(imm12, vc, vd, instr_mod)` -- tests RG[VD] > RG[VC]
- `TTI_SFPMOV(vc, vd, instr_mod)` -- copies RG[VC] to RG[VD]
- `TTI_SFPENCC(imm12, instr_mod)`
- `TTI_SFPCOMPC(imm12, instr_mod)`
- `TTI_SFPMAD(va, vb, vc, vd, instr_mod)`
- `TTI_SFPNOP(imm12, instr_mod, done)`

---

## 12. Required Infrastructure Changes

### SfpuType Enum
The Quasar `SfpuType` enum in `tt_llk_quasar/llk_lib/llk_defs.h` does NOT include `hardtanh`. It would need to be added, but this is only required if the test infrastructure uses `SfpuType::hardtanh`. For the kernel implementation itself, no enum change is needed.

### ckernel_sfpu.h Include
The parent file `tt_llk_quasar/common/inc/ckernel_sfpu.h` would need an `#include "sfpu/ckernel_sfpu_hardtanh.h"` line added.

### Test Sources
No Quasar test exists for hardtanh. A test file would need to be created following the pattern of `sfpu_elu_quasar_test.cpp`.
