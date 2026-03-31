# Threshold Kernel - Quasar Architecture Research

## 1. SFPU Execution Model

### Overview
The SFPU (SIMD Floating Point Unit) on Quasar/Trinity is a functional unit within the Tensix compute engine that complements the main FPU. It handles complex functions not suited for the main math engine, including conditional execution. (Source: Page 1256423592, "Introduction")

### Lane/Slice/Row Configuration
- **Quasar configuration**: Half-sized with **8 slices** and **4 rows per slice** = **32 SFPU lanes** total.
- Each slice is directly connected to a corresponding SrcS Register slice and Dest Register slice in the FPU Gtile.
- Within each slice, there is a single instruction decode block shared among 4 rows.
- For Quasar LLK: **SFP_ROWS = 2** (defined in `cmath_common.h`), meaning each SFPU iteration processes 2 rows of the Dest register.
- (Source: Page 1256423592, "Introduction" and "Hardware Overview")

### Instruction Execution
- Instructions are dispatched 1 per cycle from the Tensix pipeline.
- The SFPU supports **superscalar** operation via LOADMACRO: up to 5 instructions can execute in parallel.
- The SFPU occupies pipeline stages S2-S5.
- Fully pipelined execution for all instructions except SFPSHFT2 global variants and SFPSWAP.
- (Source: Page 1256423592, "Feature Summary")

### Conditional Execution Model
The threshold kernel relies heavily on conditional execution:
- **LaneFlags**: Per-lane boolean flag array (32 flags, one per lane).
- **UseLaneFlagsForLaneEnable**: When true, LaneFlags act as a predication mask.
- **SFPSETCC**: Sets LaneFlags based on LREG value conditions (negative, not-zero, positive, zero, or to immediate).
- **SFPENCC**: Enables/disables conditional execution by setting UseLaneFlagsForLaneEnable.
- **SFPGT**: New in Blackhole/Quasar - performs lanewise comparison (VD > VC) and writes result to VD, LaneFlags, or flag stack.
- (Source: Page 1612808695, Page 1612382847, DeepWiki query)

---

## 2. Register File Layouts

### LREG GPRs (General Purpose Registers)
- **8 GPRs per lane** (LREG0-LREG7), divided into 2 banks of 4.
  - Bank "LREGS1": LREG0-LREG3
  - Bank "LREGS2": LREG4-LREG7
- Each LREG holds a **32-bit** value (FP32, sign-magnitude integer, or unsigned integer).
- Each bank has 8 standard read ports, 3 write ports, 1 bulk read, 1 bulk write.
- (Source: Page 1256423592, "LREG Storage")

### Constant Registers (LREGs 9-14)
- Per-slice (not per-lane) constants.
- **LCONST_0** (LREG 9): Zero constant
- **LCONST_1** (LREG 10): One constant
- **LCONST_neg1** (LREG 11): Negative-one constant
- LREGs 11-14 can be loaded via SFPCONFIG with default values: -1.0, 1/65536, -0.67487759, -0.34484843
- (Source: `ckernel_instr_params.h` lines 310-320, Page 1256423592, Page 1613103724)

### Staging Register
- One per lane, used in LOADMACRO sequences.
- (Source: Page 1256423592, "LREG Storage")

### SrcS Registers
- **2 banks**, each with **3 slices** of registers.
- Each slice: 2 instances of tt_reg_bank, each 4 rows x 16 columns x 16 bits.
- Total: 768 bytes across both banks.
- Unpacker writes to slices 0-1; SFPU reads from these and writes to slice 2.
- Double-buffered: banks toggle for pipelining.
- Data format in SrcS is typically already in FP32-compatible format for SFPU access.
- (Source: Page 141000706)

### Dest Register
- Two modes: **16-bit mode** (1024 rows) and **32-bit mode** (512 effective rows).
- Supported storage formats: FP16a, FP16b, FP32, Int8 (signed/unsigned), Int16 (signed/unsigned), Int32 (signed).
- NOT supported: Int32 unsigned, Tf19, BFP formats, LF8.
- FP16a storage: `{sgn, man[9:0], exp[4:0]}` (shuffled).
- FP16b storage: `{sgn, man[6:0], exp[7:0]}` (shuffled).
- FP32 storage: row i = `{sgn, man[22:16], exp[7:0]}`, row i+N/2 = `{man[15:0]}` (shuffled).
- (Source: Page 195493892, Page 80674824)

---

## 3. Per-Instruction Details for Threshold Kernel

The threshold kernel on Blackhole uses SFPI (high-level C++ wrapper) with `dst_reg[0]`, `v_if`, comparison operators. On Quasar, the TTI (direct instruction) style is used. The required instructions are:

### SFPLOAD (opcode 0x70)
- **Purpose**: Move data from Dest or SrcS to an LREG.
- **Syntax**: `TT_SFPLOAD(lreg_ind, instr_mod0, sfpu_addr_mode, done, dest_addr_imm)`
- **Key modes (instr_mod0)**:
  - `MOD0_FMT_SRCB` (0): Format from SrcB config -> FP32
  - `MOD0_FMT_FP16` (1): FP16 -> FP32
  - `MOD0_FMT_BF16` (2): BF16 -> FP32
  - `MOD0_FMT_FP32` (3): FP32 -> FP32
  - `DEFAULT` (0): Auto-resolved based on config
- **Scheduling**: At least 3 unrelated instructions between FPU write and SFPLOAD from same Dest region.
- **Performance**: 1 cycle, fully pipelined.
- **Quasar constants**: `p_sfpu::sfpmem::DEFAULT` = 0
- (Source: Page 1612808665)

### SFPSTORE (opcode 0x72)
- **Purpose**: Move data from LREG to Dest or SrcS.
- **Syntax**: `TT_SFPSTORE(lreg_ind, instr_mod0, sfpu_addr_mode, done, dest_addr_imm)`
- **Key modes**: Same as SFPLOAD but in reverse direction.
- FP32->FP16: denormals flushed to zero, mantissa truncated toward zero, large magnitudes -> infinity. NaN -> infinity (avoid NaN in FP16 conversion).
- FP32->BF16: denormals flushed to zero, mantissa truncated.
- **Performance**: 1 cycle, fully pipelined. Store to SrcS completes in S4 (2-cycle latency), store to Dest completes in S5 (3-cycle latency).
- (Source: Page 1612382850)

### SFPLOADI (opcode 0x71)
- **Purpose**: Load a 16-bit immediate into all lanes of an LREG.
- **Syntax**: `TT_SFPLOADI(lreg_ind, instr_mod0, imm16)`
- **Key modes (instr_mod0)**:
  - `SFPLOADI_MOD0_FLOATB` (0): BF16 immediate -> FP32 (shift left 16)
  - `SFPLOADI_MOD0_FLOATA` (1): FP16 immediate -> FP32 (rebias exponent)
  - `SFPLOADI_MOD0_USHORT` (2): UINT16 -> zero-extended UINT32
  - `SFPLOADI_MOD0_SHORT` (4): INT16 -> sign-extended INT32
  - `SFPLOADI_MOD0_UPPER` (8): Write to high 16 bits, preserve low
  - `SFPLOADI_MOD0_LOWER` (10): Write to low 16 bits, preserve high
- **Usage in threshold**: Load threshold and value constants into LREGs. For Float16_b constant, use mode 0 with upper 16 bits of the float.
- (Source: Page 1612218782)

### SFPSETCC (opcode 0x7B)
- **Purpose**: Set CC (LaneFlags) based on LREG value.
- **Syntax**: `TT_SFPSETCC(imm12_math, lreg_c, instr_mod1)`
- **Modes (instr_mod1)**:
  - 0x0: Set CC if src_c is **negative**
  - 0x1: Set CC to imm12_math[0] (immediate value)
  - 0x2: Set CC if src_c is **not zero**
  - 0x4: Set CC if src_c is **positive**
  - 0x6: Set CC if src_c is **zero**
  - 0x8: **Invert** the current CC value
- **imm12_math bit 11**: If set, interpret src_c as INT32 instead of FP32/SMAG32.
- **Performance**: 1 cycle.
- (Source: Page 1612808695)

### SFPENCC (opcode 0x8A)
- **Purpose**: Enable/disable conditional execution.
- **Syntax**: `TT_SFPENCC(imm12_math, lreg_c, instr_mod1)`
- **Usage patterns** (from existing Quasar code):
  - `TTI_SFPENCC(1, 2)`: Enable CC execution (sets UseLaneFlagsForLaneEnable to true)
  - `TTI_SFPENCC(0, 2)`: Disable CC execution
  - `TTI_SFPENCC(0, 0)`: Clear/reset CC result (reset LaneFlags)
- **Critical**: CC must be enabled BEFORE the comparison instruction and disabled AFTER the loop.
- (Source: assembly.yaml line 6007, existing code in `ckernel_sfpu_lrelu.h`)

### SFPGT (opcode 0x97)
- **Purpose**: Lanewise comparison VD > VC on sign-magnitude integers or FP32.
- **Syntax**: `TT_SFPGT(imm12_math, lreg_c, lreg_dest, instr_mod1)`
- **Modifiers (instr_mod1)**:
  - `SFPGT_MOD1_SET_CC` (1): Write comparison result to LaneFlags
  - `SFPGT_MOD1_MUTATE_STACK` (2): Modify flag stack
  - `SFPGT_MOD1_MUTATE_OR` (4): OR into flag stack (else AND)
  - `SFPGT_MOD1_SET_VD` (8): Write -1/0 result to VD
- **Comparison**: Uses `SignMagIsSmaller(VC, VD)` -- tests if VC < VD (i.e., VD > VC).
- **FP32 total order**: -NaN < -Inf < ... < -0 < +0 < ... < +Inf < +NaN
- **NEW in Blackhole/Quasar**: This instruction did not exist in Wormhole.
- **Usage for threshold**: `SFPGT(0, threshold_lreg, input_lreg, 0x1)` sets CC where input > threshold.
- (Source: Page 1612382847)

### SFPMOV (opcode 0x7C)
- **Purpose**: Copy one LREG to another.
- **Syntax**: `TT_SFPMOV(lreg_c, lreg_dest, instr_mod1)`
- **Modes (instr_mod1)**:
  - 0x0: Copy lreg_c to lreg_dest (conditional on LaneEnabled)
  - 0x1: Copy with sign inversion
  - 0x2: Unconditional copy (ignores LaneEnabled)
- **Usage in threshold**: Conditionally move the replacement value where comparison is satisfied.
- (Source: Page 1612481158)

### SFPMAD (opcode 0x76)
- **Purpose**: Lanewise FP32 multiply-add: `VD = +/-(VA * VB) +/- VC`
- **Syntax**: `TT_SFPMAD(lreg_a, lreg_b, lreg_c, lreg_dest, instr_mod1)`
- **Modifiers**:
  - `SFPMAD_MOD1_NEGATE_VB` (1): Negate VB
  - `SFPMAD_MOD1_NEGATE_VC` (2): Negate VC
  - `SFPMAD_MOD1_INDIRECT_VA` (4): VA from LReg[7][3:0]
  - `SFPMAD_MOD1_INDIRECT_VD` (8): VD from LReg[7][3:0]
- **Scheduling**: Requires 2 cycles. Auto-stall on next cycle if reading from same dest. In LOADMACRO, manual gap needed.
- **IEEE754**: Denormal inputs -> zero. Result denormals flushed to signed zero. Partially fused multiply-add.
- **Usage in threshold**: Could be used to multiply input by 1.0 for copy or similar but SFPMOV is simpler.
- (Source: Page 1612448455)

### SFPSWAP (opcode 0x94)
- **Purpose**: Swap, min/max, or argmin/argmax on two vectors.
- **Syntax**: `TT_SFPSWAP(imm12_math, lreg_c, lreg_dest, instr_mod1)`
- **Mode 0 (SWAP)**: Unconditional swap of VD and VC.
- **Mode 1 (VEC_MIN_MAX)**: VD=min, VC=max per lane.
- **Scheduling**: Next cycle must be SFPNOP (auto-stalled or explicit).
- **Likely not needed** for basic threshold, but available for min/max variants.
- (Source: Page 1613398600)

### SFPCONFIG (opcode 0x87)
- **Purpose**: Manipulate SFPU configuration (LREGs 11-14, LaneConfig, LoadMacroConfig).
- Not directly needed for threshold, but used for CC stack/lane configuration if needed.
- (Source: Page 1613103724)

---

## 4. Data Format Support and Conversion Rules

### SFPU Internal Format
- All SFPU computations happen in **FP32** or **32-bit sign-magnitude integer** format.
- SFPLOAD converts from Dest format to FP32/SMAG32 in the LREG.
- SFPSTORE converts from FP32/SMAG32 in LREG back to Dest format.
- The SFPU itself is **format-agnostic** -- it operates on FP32 values regardless of the L1/Dest storage format. Format conversion happens at the load/store boundary.
- (Source: Page 1256423592, "Load EXU", "Store EXU")

### Unpacker Format Conversions (L1 -> Registers)
Key conversions supported:
- Float32 -> Float32, TF32, Float16, Float16_B
- Float16/Float16_B/FP8R/FP8P/MX formats -> Float16, Float16_B (and TF32 for SrcA/B)
- MxFp8R/MxFp8P -> Float16_B (unpacked with block exponent)
- Int32 -> Int32 (Dest/SrcS only)
- Int8 -> Int8
- UInt8 -> UInt8
- Int16 -> Int16
- (Source: Page 237174853, "Unpacker Format Conversions")

### Packer Format Conversions (Registers -> L1)
- Float32 -> all FP formats (Float32, TF32, Float16, Float16_B, FP8R/P, all MX)
- Float16/Float16_B -> Float16, Float16_B, FP8R/P, MX formats
- Int32 -> Int32, Int8, UInt8
- Int16 -> Int16
- Rounding: RoundTiesToEven or Stochastic (configurable).
- (Source: Page 237174853, "Packer Format Conversions")

### Subnormal Handling
- Unpacker: Subnormals clamped to 0 in most cases before writing to registers.
- Packer: Input subnormals clamped to 0 before conversion.
- SFPU: Denormal inputs treated as zero in arithmetic ops (SFPMAD). Denormal results flushed to signed zero.
- (Source: Page 237174853, "Subnormal Clamping Behavior")

---

## 5. Format Support Matrix for Threshold Kernel

Threshold is a **float comparison operation** (compare input against threshold, replace with value if condition met). It operates on floating-point values in the SFPU via FP32 LREGs. The comparison uses sign-magnitude ordering which is compatible with FP32 total ordering.

### Applicable Formats

| Format | Enum Value | Applicable | Constraints |
|--------|-----------|------------|-------------|
| **Float16** | 1 | YES | Standard FP format, loaded as FP16->FP32 in SFPU |
| **Float16_b** | 5 | YES | Standard BF16 format, loaded as BF16->FP32 in SFPU |
| **Float32** | 0 | YES | Native FP32 format, no conversion needed in SFPU |
| **Tf32** | 4 | YES | Stored as 32b in Dest/SrcS, loaded as FP32 |
| **MxFp8R** | 18 | YES | L1 only; unpacked to Float16_b for registers, then FP32 in SFPU |
| **MxFp8P** | 20 | YES | L1 only; unpacked to Float16_b for registers, then FP32 in SFPU |
| **Int32** | 8 | CONDITIONAL | SFPGT supports sign-magnitude comparison (imm12_math bit 11 can be used with SFPSETCC for INT mode). However, Int32 requires dest_acc=Yes (FP32 mode). Threshold comparison on integers is possible but needs INT-aware load/store modes. |
| **Int16** | 9 | NO | Limited SFPU support. Int16 is primarily an SFPU-operated format with restricted pipeline support. Not practical for threshold. |
| **Int8** | 14 | NO | Very limited range in sign-magnitude (+-255). Not meaningful for threshold comparison. |
| **UInt8** | 17 | NO | Unsigned 8-bit, very limited range, not meaningful for threshold. |

### Recommended Test Formats
For a standard float-domain threshold operation:
- **Primary**: Float16_b, Float16, Float32
- **Extended**: Tf32, MxFp8R, MxFp8P
- **NOT recommended**: Int32, Int16, Int8, UInt8 (threshold is a float comparison operation)

### Format-Specific Constraints
- **MX formats**: Require `implied_math_format=Yes` in unpacker config. Stored in L1 with block exponents; unpacked to Float16_b for register storage.
- **Int32**: Requires `dest_acc=Yes` (FP32 dest mode). Sign-magnitude comparison works differently from FP32 comparison.
- **Cross-exponent-family conversions** (e.g., Float16 -> Float16_b): May need `dest_acc=Yes`.
- (Source: Page 237174853, `format_config.py`, `data_format_inference.py`)

---

## 6. Pipeline Constraints and Instruction Ordering

### SFPMAD Scheduling
- 2-cycle computation. Hardware auto-stalls if next instruction reads from SFPMAD output register.
- In LOADMACRO: Software must ensure 1 cycle gap between SFPMAD and consumer.
- Known hazards: SFPAND, SFPOR, SFPIADD, SFPSHFT, SFPCONFIG, SFPSWAP, SFPSHFT2 -- auto-stall does NOT detect some read dependencies from these instructions.
- (Source: Page 1612448455, "Instruction scheduling")

### SFPSWAP Scheduling
- Requires SFPNOP on next cycle (auto-stalled if not provided).
- (Source: Page 1613398600)

### SFPLOAD Scheduling (from Dest)
- At least 3 unrelated Tensix instructions needed after FPU write to Dest before SFPLOAD can read same region.
- Alternatively, STALLWAIT (B8, C7) can be used.
- (Source: Page 1612808665, "Instruction scheduling")

### SFPCONFIG to LaneConfig
- If SFPCONFIG changes `DISABLE_BACKDOOR_LOAD`, the next instruction may see old or new value. Insert SFPNOP after to be safe.
- (Source: Page 1613103724)

### Quasar Row Processing
- `SFP_ROWS = 2` -- each SFPU iteration processes 2 rows.
- After each iteration, call `_incr_counters_<0x0, 0x0, SFP_ROWS, 0x0>()` to advance dest pointer.
- (Source: `cmath_common.h`, existing Quasar SFPU kernels)

### LOADMACRO Rules
- Can program up to 4 instruction templates.
- Supports up to 5 parallel instruction dispatches.
- Not required for threshold (simple kernel), but available for optimization.
- (Source: Page 1256423592, "LOADMACRO Engine")

---

## 7. Blackhole vs Quasar Differences for Threshold

### Blackhole (Reference) Implementation
The Blackhole `ckernel_sfpu_threshold.h` uses the **SFPI** C++ abstraction layer:
```cpp
sfpi::vFloat in = sfpi::dst_reg[0];
v_if (in <= v_threshold) {
    sfpi::dst_reg[0] = v_value;
}
v_endif;
sfpi::dst_reg++;
```
- Uses `sfpi::vFloat`, `sfpi::dst_reg`, `v_if/v_endif` macros.
- Template parameters: `T` (float or uint32_t), `APPROXIMATION_MODE`, `ITERATIONS`.
- Includes `<type_traits>`, `sfpi.h`, `ckernel_sfpu_converter.h`.

### Quasar Implementation Style
Quasar kernels use **TTI (direct instruction)** style, NOT SFPI:
- `TTI_SFPLOAD`, `TTI_SFPSTORE`, `TTI_SFPLOADI`, `TTI_SFPSETCC`, `TTI_SFPENCC`, `TTI_SFPGT`, `TTI_SFPMOV`
- Include `ckernel_trisc_common.h`, `cmath_common.h`
- Namespace: `ckernel::sfpu`
- Use `p_sfpu::LREG*`, `p_sfpu::sfpmem::DEFAULT`, `ADDR_MOD_7`
- Row increment: `ckernel::math::_incr_counters_<0x0, 0x0, ckernel::math::SFP_ROWS, 0x0>()`

### Key Differences
1. **No SFPI library on Quasar**: Cannot use `vFloat`, `dst_reg[]`, `v_if/v_endif`. Must use TTI instructions directly.
2. **Conditional execution on Quasar**: Use explicit `TTI_SFPENCC(1, 2)` to enable, `TTI_SFPSETCC` or `TTI_SFPGT` to set condition, then `TTI_SFPENCC(0, 0)` or `TTI_SFPENCC(0, 2)` to reset/disable.
3. **SFPGT available on Quasar**: The `SFPGT` instruction (opcode 0x97) exists in Quasar's assembly.yaml. This provides direct comparison without needing subtraction + sign check.
4. **SFP_ROWS = 2 on Quasar** vs potentially different on other architectures.
5. **Namespace**: `ckernel::sfpu` (not `ckernel::sfpu` with template specialization).
6. **No Converter class on Quasar**: `Converter::as_float` from Blackhole is not available. For uint32_t-to-float reinterpretation, a different approach is needed.

### Threshold Logic Mapping (Blackhole -> Quasar)
Blackhole `in <= threshold` maps to Quasar:
- Load threshold into LREG2 via `TTI_SFPLOADI`
- Enable CC: `TTI_SFPENCC(1, 2)`
- Per-iteration loop:
  1. Load data: `TTI_SFPLOAD(LREG0, DEFAULT, ADDR_MOD_7, 0, 0)`
  2. Compare: `TTI_SFPGT(0, LREG0, LREG2, 0x1)` -- sets CC where LREG2 > LREG0 (i.e., input <= threshold)
  3. Conditionally load value: `TTI_SFPLOADI(LREG0, 0, value_imm16)` -- only executes where CC is set
  4. Reset CC: `TTI_SFPENCC(0, 0)`
  5. Store: `TTI_SFPSTORE(LREG0, 0, ADDR_MOD_7, 0, 0)`
  6. Increment: `_incr_counters_<0x0, 0x0, SFP_ROWS, 0x0>()`
- Disable CC: `TTI_SFPENCC(0, 2)`

---

## 8. Existing Quasar Patterns for Reference

### Pattern: Conditional per-lane operation (from lrelu.h)
```cpp
TTI_SFPLOADI(p_sfpu::LREG2, 0 /*Float16_b*/, (slope >> 16));  // load constant
TTI_SFPENCC(1, 2);                                              // enable CC
for (int d = 0; d < iterations; d++) {
    TTI_SFPLOAD(LREG0, DEFAULT, ADDR_MOD_7, 0, 0);
    TTI_SFPSETCC(0, LREG0, 0);  // set CC if negative
    TTI_SFPMAD(LREG0, LREG2, LCONST_0, LREG0, 0);  // conditional multiply
    TTI_SFPENCC(0, 0);  // clear CC
    TTI_SFPSTORE(LREG0, 0, ADDR_MOD_7, 0, 0);
    _incr_counters_<0x0, 0x0, SFP_ROWS, 0x0>();
}
TTI_SFPENCC(0, 2);  // disable CC
```

### Pattern: Comparison with SFPGT (from lrelu.h relu_min)
```cpp
TTI_SFPLOADI(LREG2, 0 /*Float16_b*/, (threshold >> 16));
TTI_SFPENCC(1, 2);
for (...) {
    TTI_SFPLOAD(LREG0, DEFAULT, ADDR_MOD_7, 0, 0);
    TTI_SFPGT(0, LREG0, LREG2, 0x1);  // CC set where LREG2 > LREG0 (input <= threshold)
    TTI_SFPLOADI(LREG0, 0, 0);          // load 0 into LREG0 where CC is set
    TTI_SFPENCC(0, 0);                   // clear CC
    TTI_SFPSTORE(LREG0, 0, ADDR_MOD_7, 0, 0);
    _incr_counters_<0x0, 0x0, SFP_ROWS, 0x0>();
}
TTI_SFPENCC(0, 2);
```

This relu_min pattern is **very close** to what threshold needs. The main difference: threshold replaces with an arbitrary value (not just 0).

---

## 9. Quasar-Supported Data Formats (Complete Reference)

From `format_config.py` (QUASAR_DATA_FORMAT_ENUM_VALUES):
| DataFormat | Enum Value |
|-----------|-----------|
| Float32 | 0 |
| Tf32 | 4 |
| Float16 | 1 |
| Float16_b | 5 |
| MxFp8R | 18 |
| MxFp8P | 20 |
| Int32 | 8 |
| Int8 | 14 |
| UInt8 | 17 |
| UInt16 | 130 |
| Int16 | 9 |

Valid Quasar SrcReg formats: Float16_b, Float16, Float32, Tf32, Int32, Int8, UInt8, Int16
Valid Quasar Dest formats: Float16_b, Float16, Float32, Int32, Int8, UInt8, Int16

---

## 10. Summary: Key Facts for Threshold Implementation

1. **SFPU is format-agnostic**: Operates in FP32 internally. Format conversion at load/store boundary.
2. **Use TTI instruction style**: No SFPI library on Quasar.
3. **Key instructions**: SFPLOAD, SFPLOADI, SFPGT, SFPSETCC, SFPENCC, SFPMOV, SFPSTORE.
4. **CC flow**: Enable CC -> compare -> conditional op -> reset CC -> store -> increment.
5. **SFP_ROWS = 2**: Process 2 rows per iteration, increment counters accordingly.
6. **SFPGT for comparison**: `SFPGT(0, VC, VD, 0x1)` sets CC where VD > VC.
7. **Threshold logic**: Load input, compare with threshold, conditionally replace with value.
8. **Float formats for testing**: Float16_b, Float16, Float32 (primary); Tf32, MxFp8R, MxFp8P (extended).
9. **Parameters**: Threshold value and replacement value passed as uint32_t (bitcast of float).
10. **Closest existing pattern**: `_calculate_relu_min_` in `ckernel_sfpu_lrelu.h` -- nearly identical logic.
