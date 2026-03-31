# Quasar Architecture Research: LOG (Natural Logarithm) SFPU Kernel

## 1. SFPU Execution Model

### 1.1 Overview
The SFPU (SIMD Floating Point Unit) is a functional unit within the Tensix compute engine that complements the main FPU. It handles complex functions like reciprocal, sigmoid, exponential, and logarithm. The SFPU executes as a SIMD engine with 8 instances operating in parallel, each processing its own local data.

**Source**: Confluence page 1256423592 (Quasar/Trinity SFPU Micro-Architecture Spec)

### 1.2 Lane Layout
- **32 lanes total** (8 SFPU instances x 4 rows)
- Each SFPU instance processes data from 4 consecutive rows of the Dest/SrcS register file
- Even/odd column selection determines which half of the 16 columns each SFPU instance processes
- SFPLOAD reads 32 datums (8 instances x 4 rows), SFPSTORE writes 32 datums back

### 1.3 Quasar-Specific: SFP_ROWS = 2
- On Quasar, SFPU processes **2 rows** per iteration (not 4 as on some other architectures)
- The loop pattern is: `_calculate_*_sfp_rows_()` processes 2 rows, then `_incr_counters_<0x0, 0x0, ckernel::math::SFP_ROWS, 0x0>()` advances by 2 rows
- This is a critical difference from Blackhole, which uses `dst_reg++` (advancing by 32 rows in SFPI mode)

### 1.4 Execution Sub-Units
The SFPU has multiple sub-units that can execute in parallel:
- **Load sub-unit**: SFPLOAD (1 cycle, fully pipelined)
- **Store sub-unit**: SFPSTORE (1 cycle, fully pipelined)
- **MAD sub-unit**: SFPMAD, SFPMUL, SFPADD (2 cycles, with automatic stalling for data hazards)
- **Simple sub-unit**: SFPSETEXP, SFPSETMAN, SFPSETSGN, SFPEXEXP, SFPEXMAN, SFPMOV, SFPABS, etc. (1 cycle)
- **Nonlinear sub-unit**: SFPNONLINEAR/SFPARECIP (handles EXP, RECIP, SQRT, TANH, RELU approximations)
- **CC sub-unit**: SFPSETCC, SFPENCC, SFPPUSHC, SFPPOPC, SFPCOMPC

**Source**: Confluence pages 1256423592, 1170505767

## 2. Register File Layouts

### 2.1 LREGs (Local Registers)
- **8 general-purpose LREGs**: LREG0-LREG7 (each 32 bits per lane, 32 lanes)
- **Special constant registers**:
  - LCONST_0 (LREG9) = 0.0f
  - LCONST_1 (LREG10) = 1.0f
  - LCONST_neg1 (LREG11) = -1.0f
- **LUT registers**: LREG9-LREG14 (used for LUT-based approximations, loaded via `_sfpu_load_config32_`)
- All LREGs store FP32 values (or 32-bit integers reinterpreted as FP32)

**Source**: Confluence page 1256423592, ckernel_instr_params.h

### 2.2 Dest Register
- Two modes: 16-bit (1024 rows x 16 columns) and 32-bit (512 rows x 16 columns)
- **Supported formats in Dest**: FP16a, FP16b, FP32, Int8 (signed/unsigned), Int16 (signed/unsigned), Int32 (signed)
- **NOT supported in Dest**: Int32 unsigned, Tf19, BFP formats, LF8
- In 32-bit mode, physical row i stores MSBs and row i+N/2 stores LSBs

**Source**: Confluence page 195493892 (Dest), page 80674824 (Dest storage formats)

### 2.3 SrcS Registers
- 2 banks, each with 3 slices of registers
- Each slice: 2 instances of tt_reg_bank, each 4 rows x 16 columns x 16 bits
- SFPU reads from slices 0-1 (unpacker writes), writes to slice 2 (packer reads)
- Double-buffered: bank toggling via `done` bit in SFPLOAD/SFPSTORE
- SrcS data is typically in FP32 format when accessed by SFPU

**Source**: Confluence page 141000706 (srcS registers)

### 2.4 SrcA/B Storage Formats
- 19-bit containers (two banks of 64 rows x 16 columns)
- Formats: FP16a, FP16b, Tf19 (called Tf32), Int8, UInt8, Int16
- **NOT supported**: Int32, FP32, Int16 unsigned, BFP, LF8

**Source**: Confluence page 83230723 (SrcA/B storage formats)

## 3. Instructions Needed for LOG Kernel

### 3.1 SFPLOAD — Move from Dest/SrcS to LREG
- **Syntax**: `TTI_SFPLOAD(lreg_ind, instr_mod0, sfpu_addr_mode, done, dest_addr_imm)`
- **Key modes** (instr_mod0):
  - `DEFAULT` (0): Format determined by SrcB format config and ACC_CTRL_SFPU_Fp32
  - `FP16B` (2): BF16 from Dest -> FP32 in LREG
  - `FP32` (3): FP32 from Dest -> FP32 in LREG
- **Scheduling**: At least 3 unrelated instructions needed after FPU writes to Dest before SFPLOAD can read the same region
- **Performance**: 1 cycle, fully pipelined

**Source**: Confluence page 1612808665 (SFPLOAD ISA page)

### 3.2 SFPSTORE — Move from LREG to Dest/SrcS
- **Syntax**: `TTI_SFPSTORE(lreg_ind, instr_mod0, sfpu_addr_mode, done, dest_addr_imm)`
- **Key modes** (instr_mod0): Same as SFPLOAD but in reverse direction
  - FP16: FP32 -> FP16 (NaN -> infinity, denormals -> zero, mantissa truncated)
  - BF16: FP32 -> BF16 (denormals -> zero, mantissa truncated)
  - FP32: FP32 -> FP32 (denormals -> zero)
- **Performance**: 1 cycle, fully pipelined

**Source**: Confluence page 1612382850 (SFPSTORE ISA page)

### 3.3 SFPLOADI — Load Immediate to LREG
- **Syntax**: `TTI_SFPLOADI(lreg_ind, instr_mod0, imm16)`
- **Key modes** (instr_mod0):
  - 0 = FP16_B: Load 16-bit value as BF16 -> FP32 in LREG
  - 8 = HI16_ONLY: Write to high 16 bits of LREG, preserve low 16
  - 10 (0xA) = LO16_ONLY: Write to low 16 bits of LREG, preserve high 16
  - 2 = UINT16: Zero-extend 16-bit value to UINT32
- Used to load constants like ln(2), polynomial coefficients, and -infinity

**Source**: ckernel_ops.h line 651-653

### 3.4 SFPEXEXP — Extract Exponent
- **Syntax**: `TTI_SFPEXEXP(lreg_c, lreg_dest, instr_mod1)`
- **Behavior**: Extracts the exponent field of an FP32 value in lreg_c and places it (as a biased integer or unbiased integer depending on mode) into lreg_dest
- **Opcode**: 0x77
- **Critical for LOG**: log(x) = log(mantissa * 2^exp) = log(mantissa) + exp * log(2)
  - This instruction extracts the exponent part for the exp * ln(2) term

**Source**: assembly.yaml line 5778, ckernel_ops.h line 631

### 3.5 SFPSETEXP — Set Exponent
- **Syntax**: `TTI_SFPSETEXP(imm12_math, lreg_c, lreg_dest, instr_mod1)`
- **Functional model**: Combines FP32 sign+mantissa from lreg_c with new exponent from immediate or another LREG
- **Modes**:
  - `SFPSETEXP_MOD1_ARG_IMM` (1): Exponent from immediate (8-bit)
  - `SFPSETEXP_MOD1_ARG_EXPONENT` (2): Exponent from another LREG's exponent field
  - Default (0): Exponent from another LREG's low bits
- **Critical for LOG**: Used to normalize the input to [1, 2) range by setting exponent to 127 (the FP32 bias)
  - `setexp(in, 127)` in Blackhole's SFPI becomes `TTI_SFPSETEXP(127, lreg_src, lreg_dest, 1)` in Quasar TTI

**Source**: Confluence page 1613070904 (SFPSETEXP ISA page)

### 3.6 SFPSETMAN — Set Mantissa
- **Syntax**: `TTI_SFPSETMAN(imm12_math, lreg_c, lreg_dest, instr_mod1)`
- **Functional model**: Combines FP32 sign+exponent from lreg_c with new mantissa from immediate or another LREG
- **Modes**:
  - `SFPSETMAN_MOD1_ARG_IMM` (1): Mantissa from imm12 << 11 (left-shifted to fill 23-bit mantissa)
  - Default (0): Mantissa from another LREG's low 23 bits

**Source**: Confluence page 1612808726 (SFPSETMAN ISA page)

### 3.7 SFPSETSGN — Set Sign
- **Syntax**: `TTI_SFPSETSGN(imm12_math, lreg_c, lreg_dest, instr_mod1)`
- **Functional model**: Combines FP32 exp+mantissa from lreg_c with new sign from immediate or another LREG
- **Modes**:
  - `SFPSETSGN_MOD1_ARG_IMM` (1): Sign from imm1 (bit 0 of imm12)
  - Default (0): Sign from another LREG's bit 31
- **Used for LOG**: When handling negative exponent, the Blackhole code does `setsgn(~exp + 1, 1)` to negate

**Source**: Confluence page 1612513663 (SFPSETSGN ISA page)

### 3.8 SFPMAD — Multiply-Add
- **Syntax**: `TTI_SFPMAD(lreg_a, lreg_b, lreg_c, lreg_dest, instr_mod1)`
- **Computation**: `VD = +/-(VA * VB) +/- VC` (FP32 lanewise)
- **Modifiers**:
  - `SFPMAD_MOD1_NEGATE_VB` (1): Negate VB
  - `SFPMAD_MOD1_NEGATE_VC` (2): Negate VC
  - `SFPMAD_MOD1_INDIRECT_VA` (4): VA index from LREG7
  - `SFPMAD_MOD1_INDIRECT_VD` (8): VD index from LREG7
- **Scheduling**: 2-cycle latency. Hardware auto-stalls if next instruction reads from SFPMAD's destination. Inside SFPLOADMACRO, software must manually insert SFPNOP.
- **IEEE754**: Partially fused (product kept in higher precision than FP32). Denormal inputs -> zero. Output denormals -> zero.
- **Critical for LOG**: Used for Horner form polynomial evaluation: `x * (x * (A*x + B) + C) + D`

**Source**: Confluence page 1612448455 (SFPMAD ISA page)

### 3.9 SFPMUL — Multiply
- **Syntax**: `TTI_SFPMUL(lreg_a, lreg_b, lreg_c, lreg_dest, instr_mod1)`
- Identical to SFPMAD but preferred when VC == LCONST_0 (the zero constant), computing `VD = VA * VB + 0`
- **Scheduling**: Same as SFPMAD (2 cycles, auto-stall)

**Source**: Confluence page 1613431311 (SFPMUL ISA page)

### 3.10 SFPADD — Add
- **Syntax**: `TTI_SFPADD(lreg_a, lreg_b, lreg_c, lreg_dest, instr_mod1)`
- Performs FP32 addition. When using LCONST_1 as multiplier, effectively computes `VD = 1.0 * VA + VC = VA + VC`
- **Scheduling**: 2 cycles, same as SFPMAD

**Source**: assembly.yaml line 5957, ckernel_ops.h line 605

### 3.11 SFPIADD — Integer Add
- **Syntax**: `TTI_SFPIADD(imm12_math, lreg_c, lreg_dest, instr_mod1)`
- Performs integer addition on LREG values (or immediate + LREG)
- Can also set per-lane flags based on result
- **Potential use for LOG**: Manipulating extracted exponent as integer (e.g., subtracting bias 127)

**Source**: assembly.yaml line 5798, ckernel_ops.h line 638-641

### 3.12 SFPCAST — Type Cast
- **Syntax**: `TTI_SFPCAST(lreg_c, lreg_dest, instr_mod1)`
- Converts between sign-magnitude integers and FP32
- Mode 0: Sign-mag int32 -> FP32 (round nearest even)
- **Potential use for LOG**: Converting extracted integer exponent to FP32 for the exp*ln(2) computation

**Source**: assembly.yaml line 6150, ckernel_ops.h line 613-615

### 3.13 SFPNONLINEAR (SFPARECIP) — Approximate Nonlinear Functions
- **Syntax**: `TTI_SFPNONLINEAR(lreg_c, lreg_dest, instr_mod1)`
- **Opcode**: 0x99 (called SFPARECIP in assembly.yaml)
- **Available modes** (from p_sfpnonlinear):
  - RECIP_MODE (0x0): Approximate reciprocal
  - RELU_MODE (0x2): ReLU
  - SQRT_MODE (0x3): Approximate square root
  - EXP_MODE (0x4): Approximate exponential
  - TANH_MODE (0x5): Approximate tanh
- **NOTE**: There is NO dedicated LOG_MODE in SFPNONLINEAR. The log kernel must be implemented using polynomial approximation with SFPMAD/SFPMUL/SFPADD and float manipulation with SFPSETEXP/SFPEXEXP.

**Source**: ckernel_instr_params.h line 460-466, ckernel_ops.h line 691-693, assembly.yaml line 6409

### 3.14 Conditional Execution Instructions

#### SFPSETCC — Set CC Result
- **Syntax**: `TTI_SFPSETCC(imm12_math, lreg_c, instr_mod1)`
- **Modes**:
  - 0x0: CC Result = 1 if lreg_c is negative (sign bit check)
  - 0x1: CC Result = imm12_math[0] (set to constant)
  - 0x2: CC Result = 1 if lreg_c is not zero
  - 0x4: CC Result = 1 if lreg_c is positive
  - 0x6: CC Result = 1 if lreg_c is zero
  - 0x8: Invert current CC Result

**Source**: Confluence page 1612808695 (SFPSETCC ISA page)

#### SFPENCC — Enable/Disable Conditional Execution
- **Syntax**: `TTI_SFPENCC(cc_en, cc_result_mode)`
- **Key patterns from existing Quasar kernels**:
  - `TTI_SFPENCC(0, 0)`: Clear CC result (reset for all lanes)
  - `TTI_SFPENCC(0, 1)`: Set CC_en=1, CC_res=1 for all lanes
  - `TTI_SFPENCC(1, 2)`: Enable conditional execution
  - `TTI_SFPENCC(0, 2)`: Disable conditional execution

**Source**: Confluence page 1613201963, existing Quasar kernels (elu, lrelu, threshold)

#### SFPPUSHC — Push CC to Stack
- **Syntax**: `TTI_SFPPUSHC(instr_mod1)`
- **Modes**: 0x0 (normal push), 0x1-0xF (various replace/combine operations)
- Used for nested conditional blocks

**Source**: Confluence page 1613955892 (SFPPUSHC ISA page)

#### SFPPOPC — Pop CC from Stack
- **Syntax**: `TTI_SFPPOPC(instr_mod1)`
- **Modes**: 0x0 (normal pop), 0x1 (peek), etc.

**Source**: Confluence page 1612710342

#### SFPCOMPC — Complement CC Result
- **Syntax**: `TTI_SFPCOMPC`
- No arguments. Complements or clears the CC result register.
- Used for SIMT-style if/else construct

**Source**: Confluence page 1612186107

### 3.15 SFPNOP — No Operation
- **Syntax**: `TTI_SFPNOP(srcs_wr_done, srcs_rd_done, dest_done)`
- Used to insert pipeline bubbles when manual scheduling is needed (e.g., after SFPMUL before SFPNONLINEAR reads result)

**Source**: assembly.yaml line 6122

### 3.16 SFPMOV — Move/Copy LREG
- **Syntax**: `TTI_SFPMOV(lreg_c, lreg_dest, instr_mod1)`
- Copies lreg_c to lreg_dest. instr_mod1=1 inverts sign bit.

**Source**: ckernel_ops.h line 677-679

## 4. LOG Kernel Algorithm (from Blackhole Reference)

The Blackhole reference implementation uses the following algorithm for natural logarithm:

### 4.1 Mathematical Approach
For any positive FP32 number x = mantissa * 2^exponent:
```
ln(x) = ln(mantissa * 2^exp)
      = ln(mantissa) + exp * ln(2)
```

Where mantissa is normalized to [1, 2) by setting exponent to 127 (FP32 bias).

### 4.2 Steps
1. **Load input** from Dest into LREG
2. **Normalize**: Set exponent to 127 to put mantissa in range [1, 2)
3. **Polynomial approximation**: 3rd-order Chebyshev approximation in Horner form for ln(x) over [1, 2):
   - `series_result = x * (x * (x * A + B) + C) + D`
   - Coefficients: A=0.1058, B=-0.7116, C=2.0871, D=-1.4753
   - OR higher precision: A=0x2.44734p-4, B=-0xd.e712ap-4, C=0x2.4f5388p+0, D=-0x1.952992p+0
4. **Extract exponent**: Get the original biased exponent as integer
5. **Handle negative exponent**: If exp < 0, negate it (two's complement to sign-magnitude)
6. **Convert exponent to float**: int32 -> FP32
7. **Combine**: result = expf * ln(2) + series_result
8. **Optional base scaling**: result *= log_base_scale_factor (for log2, log10, etc.)
9. **Handle special case**: If input == 0, result = -infinity
10. **Store result** back to Dest

### 4.3 Two Variants in Blackhole
1. **`_calculate_log_body_<HAS_BASE_SCALING>`**: Uses programmable constants (vConstFloatPrgm0/1/2) loaded by `_init_log_()`. Has base scaling template parameter.
2. **`_calculate_log_body_no_init_`**: Self-contained, uses inline constants. No init function needed.

### 4.4 Blackhole Uses SFPI (High-Level API)
The Blackhole reference uses SFPI constructs:
- `sfpi::dst_reg[idx]` -> SFPLOAD/SFPSTORE
- `setexp(in, 127)` -> SFPSETEXP
- `sfpi::exexp(in)` -> SFPEXEXP
- `sfpi::setsgn(val, 1)` -> SFPSETSGN
- `int32_to_float(exp, 0)` -> SFPCAST
- `v_if (cond) { ... } v_endif` -> SFPSETCC/SFPENCC/SFPPUSHC/SFPPOPC
- `x * (x * (a*x + b) + c) + d` -> SFPMAD chain
- `sfpi::vConstFloatPrgm0` -> LREG loaded via config path

### 4.5 Quasar Translation Requirements
Quasar does NOT use SFPI. Instead, all operations must be expressed as direct TTI_ instruction calls. The key translations:
- `dst_reg[idx]` -> `TTI_SFPLOAD(...)` / `TTI_SFPSTORE(...)`
- `setexp(in, 127)` -> `TTI_SFPSETEXP(127, lreg_src, lreg_dest, 1)` (imm mode)
- `exexp(in)` -> `TTI_SFPEXEXP(lreg_src, lreg_dest, instr_mod1)`
- `setsgn(val, 1)` -> `TTI_SFPSETSGN(1, lreg_src, lreg_dest, 1)` (imm mode, sign=1)
- `int32_to_float(exp, 0)` -> `TTI_SFPCAST(lreg_src, lreg_dest, 0)` (int32 sign-mag to FP32)
- `x * a + b` -> `TTI_SFPMAD(lreg_x, lreg_a, lreg_b, lreg_dest, 0)`
- `v_if (in == 0.0F)` -> `TTI_SFPSETCC(0, lreg_in, 0x6)` (mode 0x6 = set CC if zero)
- `v_if (exp < 0)` -> `TTI_SFPSETCC(0, lreg_exp, 0x0)` (mode 0x0 = set CC if negative)
- `~exp + 1` -> Bitwise NOT + add 1 (integer negate in two's complement)
- `sfpi::vConstFloatPrgm0` -> Load constant via `TTI_SFPLOADI`

## 5. Data Format Support and Conversion Rules

### 5.1 Format Support Matrix for LOG Kernel

LOG is a **transcendental floating-point operation**. It applies to float formats only -- integer formats are NOT mathematically meaningful for logarithm (log of an integer stored as Int32/Int8/etc. would require conversion to float first).

| Format | Enum Value | Applicable | Constraints |
|--------|-----------|------------|-------------|
| Float16 | 1 | YES | Primary float format. Unpacked to FP32 in LREG for SFPU ops. |
| Float16_b | 5 | YES | Primary float format. Unpacked to FP32 in LREG for SFPU ops. |
| Float32 | 0 | YES | Native SFPU format (LREGs are FP32). Requires 32-bit Dest mode. |
| Tf32 | 4 | YES | Stored as FP32 in Dest/SrcS. Unpacked to FP32. |
| MxFp8R | 18 | YES (L1 only) | Unpacked to Float16 or Float16_b for math. SFPU operates on unpacked format. |
| MxFp8P | 20 | YES (L1 only) | Unpacked to Float16 or Float16_b for math. SFPU operates on unpacked format. |
| Int32 | 8 | NO | Integer type -- log of raw integer bits is not meaningful |
| Int16 | 9 | NO | Integer type |
| Int8 | 14 | NO | Integer type |
| UInt8 | 17 | NO | Integer type |
| UInt16 | 130 | NO | Integer type |

### 5.2 Unpacker Format Conversions (relevant to LOG)
- Float32 -> Float32 (Dest/SrcS), Tf32, Float16, Float16_b
- Tf32 -> Tf32, Float16, Float16_b
- Float16/Float16_b -> Float16, Float16_b
- MxFp8R/MxFp8P -> Float16, Float16_b (NOT directly to Dest/SrcS as Tf32)
- All conversions truncate mantissa (round to zero)
- Subnormals clamped to 0 in most conversions

### 5.3 Packer Format Conversions (relevant to LOG output)
- Float32 -> Float32, Tf32, Float16, Float16_b, FP8R/P, MxFP8R/P, etc.
- Float16/Float16_b -> Float16, Float16_b, FP8R/P, MxFP8R/P, etc.
- Rounding: RoundTiesToEven or Stochastic (configurable)

### 5.4 SFPLOAD/SFPSTORE Format Modes
- SFPLOAD `DEFAULT` (0): Auto-selects based on SrcB format config
- SFPLOAD `FP16B` (2): Reads BF16 from Dest, converts to FP32 in LREG
- SFPLOAD `FP32` (3): Reads FP32 from 32-bit Dest into LREG
- SFPSTORE reverses these conversions (FP32 in LREG -> Dest format)
- SFPSTORE to FP16: denormals flushed, mantissa truncated, NaN -> infinity

**Source**: Confluence page 237174853 (Tensix Formats), pages 80674824, 83230723

## 6. Pipeline Constraints and Instruction Ordering

### 6.1 SFPMAD/SFPMUL/SFPADD Scheduling
- **2-cycle latency**: Result not available until 2 cycles after issue
- **Auto-stall**: Hardware automatically stalls the next instruction if it reads from SFPMAD's destination
- **EXCEPTION in SFPLOADMACRO**: Inside LOADMACRO sequences, auto-stall does NOT apply. Software must insert SFPNOP manually.
- **EXCEPTION for specific instructions**: SFPIADD, SFPSHFT, SFPAND (with USE_VB), SFPOR (with USE_VB), SFPCONFIG, SFPSWAP, SFPSHFT2 -- the stalling logic has bugs that may miss these hazards. Manual SFPNOP insertion required.

### 6.2 SFPLOAD after FPU Write
- At least 3 unrelated Tensix instructions must execute between an FPU write to Dest and an SFPLOAD reading the same Dest region
- Alternatively, STALLWAIT with block bit B8 and condition C7 can be used

### 6.3 SFPMUL before SFPNONLINEAR
- SFPMUL is 2-cycle; SFPNONLINEAR runs in a different sub-unit (Simple EXU)
- An SFPNOP must be inserted between SFPMUL and SFPNONLINEAR if the SFPNONLINEAR reads the SFPMUL result
- Example from exp2 kernel: `TTI_SFPMUL(...)` then `TTI_SFPNOP(0,0,0)` then `TTI_SFPNONLINEAR(...)`

### 6.4 Conditional Execution Pattern
Typical pattern from existing Quasar kernels:
```
TTI_SFPSETCC(0, lreg, mode);    // Set CC result per-lane
// Operations here only execute on lanes where CC is set
TTI_SFPENCC(0, 0);              // Clear CC (or other reset)
```

For if/else with CC stack:
```
TTI_SFPENCC(1, 2);              // Enable CC
// ... conditional ops ...
TTI_SFPENCC(0, 2);              // Disable CC
```

**Source**: Confluence pages 1612448455 (SFPMAD), 1613431311 (SFPMUL), existing Quasar kernel patterns

## 7. LOADMACRO Rules

LOADMACRO is an optimization that allows SFPLOAD and computation instructions to execute simultaneously. Key rules:
- When using LOADMACRO, auto-stalling for SFPMAD data hazards is disabled
- Software must manually schedule instruction gaps (SFPNOP)
- LOADMACRO is NOT typically used in simple SFPU kernels on Quasar (current implementations use direct TTI_ sequences)

**Source**: Confluence page 1256423592, page 2022408406

## 8. Blackhole vs Quasar Differences (Relevant for LOG Porting)

### 8.1 Programming Model
- **Blackhole**: Uses SFPI (C++ SFPU Intrinsics) -- high-level API with operator overloading, `dst_reg`, `v_if`/`v_endif`, `setexp()`, `exexp()`, etc.
- **Quasar**: Uses direct TTI_ instruction macros -- must manually sequence instructions, manage registers, handle scheduling

### 8.2 SFPU Processing Width
- **Blackhole**: `dst_reg++` advances by 32 rows (full tile face). SFPI processes a full 32-datum face at once.
- **Quasar**: `_incr_counters_<0x0, 0x0, SFP_ROWS, 0x0>()` advances by 2 rows. SFPU processes 2 rows at a time. The iteration count adapts to process the full tile.

### 8.3 Iteration Count
- **Blackhole**: `iterations` in `_calculate_log_` is typically tile_size/32 (e.g., 8 for a 256-element tile)
- **Quasar**: `iterations` is typically tile_size/(2*16) = tile_size/32, but the row increment is only 2 (not 32), and SFPLOAD/SFPSTORE process data from 4 rows at a time with column selection

### 8.4 Includes
- **Blackhole**: `#include "sfpi.h"` (SFPI library)
- **Quasar**: `#include "ckernel_trisc_common.h"` and `#include "cmath_common.h"` (for TTI_ macros, `_sfpu_load_config32_`, `_incr_counters_`)

### 8.5 Namespace and Init Functions
- Both use `namespace ckernel { namespace sfpu { ... } }`
- Blackhole `_init_log_` loads programmable constants via `sfpi::vConstFloatPrgm0/1/2`
- Quasar equivalent: Use `TTI_SFPLOADI` to load constants into LREGs, or `_sfpu_load_config32_` for LUT registers

### 8.6 No sfpi.h on Quasar
The following SFPI constructs are NOT available on Quasar and must be translated:
- `sfpi::vFloat`, `sfpi::vInt` -> Manual LREG management
- `sfpi::dst_reg[idx]` -> `TTI_SFPLOAD`/`TTI_SFPSTORE`
- `sfpi::setexp()`, `sfpi::exexp()`, `sfpi::setsgn()` -> `TTI_SFPSETEXP`, `TTI_SFPEXEXP`, `TTI_SFPSETSGN`
- `sfpi::int32_to_float()` -> `TTI_SFPCAST`
- `sfpi::vConstFloatPrgm0/1/2` -> `TTI_SFPLOADI` to load constants
- `sfpi::s2vFloat16a()` -> `TTI_SFPLOADI` with appropriate format mode
- `v_if`/`v_endif` -> `TTI_SFPSETCC`/`TTI_SFPENCC` sequences
- FP32 arithmetic operators (+, *, etc.) -> `TTI_SFPMAD`/`TTI_SFPMUL`/`TTI_SFPADD`

## 9. Quasar Coding Patterns (from Existing Implementations)

### 9.1 Standard SFPU Kernel Structure
```cpp
#pragma once
#include "ckernel_trisc_common.h"
#include "cmath_common.h"

namespace ckernel {
namespace sfpu {

// Init function (load constants)
inline void _init_op_() {
    TTI_SFPLOADI(p_sfpu::LREG2, 0 /*FP16_B*/, constant_hex);
}

// Per-row calculation
template <bool APPROXIMATION_MODE>
inline void _calculate_op_sfp_rows_() {
    TTI_SFPLOAD(p_sfpu::LREG0, p_sfpu::sfpmem::DEFAULT, ADDR_MOD_7, 0, 0);
    // ... compute ...
    TTI_SFPSTORE(p_sfpu::LREG1, 0, ADDR_MOD_7, 0, 0);
}

// Main loop
template <bool APPROXIMATION_MODE>
inline void _calculate_op_(const int iterations) {
    #pragma GCC unroll 8
    for (int d = 0; d < iterations; d++) {
        _calculate_op_sfp_rows_<APPROXIMATION_MODE>();
        ckernel::math::_incr_counters_<0x0, 0x0, ckernel::math::SFP_ROWS, 0x0>();
    }
}

} // namespace sfpu
} // namespace ckernel
```

### 9.2 Conditional Execution Pattern (from ELU kernel)
```cpp
TTI_SFPSETCC(0, p_sfpu::LREG0, 0);  // CC.Res = 1 where LREG0 is negative
// ... ops only on negative lanes ...
TTI_SFPENCC(0, 0);                   // Clear CC result
```

### 9.3 Loading FP32 Constants
For BF16 constants (most common):
```cpp
TTI_SFPLOADI(p_sfpu::LREG2, 0 /*FP16_B*/, 0x3F31); // loads 0.6914 (approx ln2)
```

For full 32-bit constants:
```cpp
TTI_SFPLOADI(p_sfpu::LREG1, 10, (value & 0xFFFF));        // LO16_ONLY
TTI_SFPLOADI(p_sfpu::LREG1, 8, ((value >> 16) & 0xFFFF)); // HI16_ONLY
```

### 9.4 SFPMAD for Polynomial Evaluation (Horner Form)
For `result = x * (x * (A*x + B) + C) + D`:
```cpp
// Step 1: A*x + B -> LREG2
TTI_SFPMAD(p_sfpu::LREG0, p_sfpu::LREG_A, p_sfpu::LREG_B, p_sfpu::LREG2, 0);
// Step 2: x * LREG2 + C -> LREG2
TTI_SFPMAD(p_sfpu::LREG0, p_sfpu::LREG2, p_sfpu::LREG_C, p_sfpu::LREG2, 0);
// Step 3: x * LREG2 + D -> LREG2
TTI_SFPMAD(p_sfpu::LREG0, p_sfpu::LREG2, p_sfpu::LREG_D, p_sfpu::LREG2, 0);
```
Note: SFPMAD has 2-cycle latency but auto-stalls, so no manual NOP needed between chained SFPMADs.

## 10. Key Constants for LOG Kernel

| Constant | Value | FP32 Hex | BF16 Approx | Purpose |
|----------|-------|----------|-------------|---------|
| ln(2) | 0.693147... | 0x3F317218 | 0x3F31 | exp * ln(2) correction |
| Coeff A | 0.1058 | 0x3DD8A4C4 | 0x3DD8 | 3rd order polynomial |
| Coeff B | -0.7116 | 0xBF362B02 | 0xBF36 | 3rd order polynomial |
| Coeff C | 2.0871 | 0x400593CE | 0x4005 | 3rd order polynomial |
| Coeff D | -1.4753 | 0xBFBCF67C | 0xBFBC | 3rd order polynomial |
| -infinity | -inf | 0xFF800000 | 0xFF80 | ln(0) result |
| 127 | N/A | N/A | N/A | FP32 exponent bias (for setexp normalization) |

## 11. SfpuType Enum Status

The current Quasar `llk_defs.h` SfpuType enum does NOT include `log`. It will need to be added:
```cpp
enum class SfpuType : std::uint32_t {
    // ... existing entries ...
    log  // NEW
};
```

The parent include file `ckernel_sfpu.h` also does NOT currently include a log header. It will need:
```cpp
#include "sfpu/ckernel_sfpu_log.h"
```

**Source**: tt_llk_quasar/llk_lib/llk_defs.h, tt_llk_quasar/common/inc/ckernel_sfpu.h
