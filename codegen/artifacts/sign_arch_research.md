# Architecture Research: sign Kernel for Quasar

## 1. SFPU Execution Model

### Overview
The SFPU (SIMD Floating Point Unit) on Quasar/Trinity is a functional unit within the Tensix compute engine that complements the main FPU. It handles complex functions (reciprocal, sigmoid, exponential, sign operations, etc.) and supports conditional execution.
*(Source: Page 1256423592 - Quasar/Trinity SFPU Micro-Architecture Spec, Introduction)*

### Lane/Slice/Row Layout
- **8 SFPU slices** (half-sized configuration)
- **4 rows per slice** = **32 SFPU lanes** total
- Each slice is directly connected to a corresponding SrcS Register slice and Dest Register slice in the FPU Gtile
- Each slice has a single instance of instruction decode logic shared among its 4 rows
- The SFPU executes the same instruction across all lanes simultaneously (SIMD)
*(Source: Page 1256423592, Introduction & Hardware Overview)*

### Pipeline Stages
- SFPU is tightly coupled with the main Tensix Compute pipeline
- Lives in Stages S2-S5 of the Tensix pipeline
- S2: Instruction Frontend (decode, LOADMACRO engine)
- S3: Most Simple EXU operations, MAD multiplication, Load format conversion
- S4: MAD addition/normalization, SWAP operations, Store to SrcS
- S5: Store to Dest
*(Source: Page 1256423592, Implementation Details)*

### Instruction Processing
- 1 new instruction per cycle from the Tensix pipeline
- LOADMACRO engine can produce up to 5 instructions (1 per engine) per cycle
- 5 execution units: Load EXU, Store EXU, Simple EXU, MAD EXU, Round EXU
*(Source: Page 1256423592, Instruction Frontend)*

## 2. Register File Layouts

### LREG GPRs (General Purpose Registers)
- **8 LREGs per lane** (LREG0-LREG7) divided into 2 banks of 4
  - LREGS1: LREGs 0-3
  - LREGS2: LREGs 4-7
- Each LREG is 32 bits wide
- Each bank has 8 standard read ports, 3 standard write ports, 1 bulk read, 1 bulk write
- Additionally, 1 Staging Register per lane for LOADMACRO sequences
*(Source: Page 1256423592, LREG Storage)*

### Constant Registers
- 6 constant registers per SFPU slice (not per lane)
- Constants 2-5 accessible via RG register view
- All 6 exposed to MAD EXU LUT coefficient unpacker via RL register view
*(Source: Page 1256423592, Constant Registers)*

### SrcS Registers
- 2 banks, each with 3 slices of registers
- Each slice: 2 instances of tt_reg_bank, each 4 rows x 16 columns x 16 bits
- Total: 3 x 8 x 16 x 16b = 768 bytes of storage across both banks
- Unpacker writes to slices 0-1; SFPU loads from slices 0-1, writes to slice 2; Packer reads from slice 2
- Bank switching via data_valid/pack_ready/sfpu_wr_ready synchronization flags
- 16-bit mode: direct row mapping; 32-bit mode: upper/lower 16 bits in different instances
*(Source: Page 141000706 - srcS registers)*

### Dest Registers
- Two modes: 16-bit (1024 rows) and 32-bit (512 effective rows)
- In 32-bit mode: physical rows i and i+N/2 combine for one 32-bit datum
- 16 columns per row
- Supports FP16a, FP16b, FP32, Int8 (signed/unsigned), Int16 (signed/unsigned), Int32 (signed)
*(Source: Page 195493892 - Dest; Page 80674824 - Dest storage formats)*

## 3. Instructions Required for Sign Kernel

The sign function computes: sign(x) = +1.0 if x > 0, -1.0 if x < 0, 0.0 if x == 0.

### Approach: Conditional execution with constant loading
The sign kernel on Quasar uses TTI (Tensix Tightly Issued) instructions with conditional execution (CC system). The approach:
1. Load input from Dest
2. Enable conditional execution
3. Set CC for negative values -> store -1.0
4. Complement CC -> store +1.0 for positive
5. Check for zero -> store 0.0
6. Reset CC and store result

### SFPLOAD (Opcode 0x70, Encoding MR)
- **Purpose**: Loads value from Dest or SrcS register file into an LREG
- **Formats**: Input: MOD_FP32/FP16_A/FP16_B/MOD_SMAG32/SMAG16/UINT16/SMAG8/UINT8; Output: FP32/SMAG32/UINT32
- **IPC**: 1; **Latency**: 1
- **InstrMod 0x0 (IMPLIED)**: Format inferred from register file format setting
- **Done bit**: Toggles read bank ID for SrcS or updates dvalid for Dest
- **Key**: ADDR_MOD_7 is used in existing Quasar kernels for standard load/store addressing
*(Source: Page 1170505767 - SFPU ISA, SFPLOAD section)*

### SFPSTORE (Opcode 0x72, Encoding MR)
- **Purpose**: Stores LREG value back to Dest or SrcS
- **Formats**: Input: FP32/SMAG32/UINT32; Output: MOD_FP32/FP16_A/FP16_B/etc.
- **IPC**: 1; **Latency**: 2 (SrcS), 3 (Dest)
- **InstrMod 0x0 (IMPLIED)**: Format inferred
- **Done bit**: Toggles write bank ID
- **Note**: SFPSTORE does not perform rounding or saturation; precede with SFP_STOCH_RND if needed
*(Source: Page 1170505767, SFPSTORE section)*

### SFPLOADI (Opcode 0x71, Encoding MI)
- **Purpose**: Loads immediate value into LREG
- **Formats**: Output: FP32/SMAG32/UINT32
- **IPC**: 1; **Latency**: 1
- **InstrMod 0x0 (FP16_B)**: Converts FP16_B immediate to FP32 in LREG
- **InstrMod 0x1 (FP16_A)**: Converts FP16_A immediate to FP32
- **InstrMod 0x2 (UINT16)**: Zero-extends to UINT32
- **InstrMod 0x8 (HI16_ONLY)**: Writes upper 16 bits, preserves lower
- **InstrMod 0xA (LO16_ONLY)**: Writes lower 16 bits, preserves upper
- **Used to preload constants** like +1.0, -1.0, 0.0 into LREGs
*(Source: Page 1170505767, SFPLOADI section)*

### SFPSETCC (Opcode 0x7B, Encoding O2)
- **Purpose**: Sets CC.Res based on value in RG[VC] or Imm12
- **Formats**: Input: FP32/INT32/SMAG32; No output (CC only)
- **IPC**: 1; **Latency**: 1
- **InstrMod modes**:
  - 0: Set CC.Res if RG[VC] is negative
  - 1: Set CC.Res based on Imm12[0]
  - 2: Set CC.Res if RG[VC] is not 0
  - 4: Set CC.Res if RG[VC] is positive
  - 6: Set CC.Res if RG[VC] is 0
  - 8: Invert current CC.Res
- **Imm12[11]**: Format select (0=INT32, 1=FP32/SMAG32)
- **Important**: For FP32 zero tests, uses SMAG32 comparison (true zero check, subnormals not treated as zero)
- **Predicated by current LaneEnabled state**
*(Source: Page 1170505767, SFPSETCC section)*

### SFPCOMPC (Opcode 0x8B, Encoding O2)
- **Purpose**: Conditionally complements or clears CC.Res
- **Executes on ALL lanes regardless of LaneEnabled**
- **IPC**: 1; **Latency**: 1
- **Algorithm**: If CC.En: if stack empty or stack[0].En && stack[0].Res: CC.Res = !CC.Res; else CC.Res = 0; else CC.Res = 0
- **Used for implementing "else" branches** in SIMT conditional execution
*(Source: Page 1170505767, SFPCOMPC section)*

### SFPENCC (Opcode 0x8A, Encoding O2)
- **Purpose**: Directly sets CC.En and CC.Res
- **Executes on ALL lanes regardless of LaneEnabled**
- **IPC**: 1; **Latency**: 1
- **InstrMod[3]**: CC Result Source Select (0=set to 1, 1=based on Imm12)
- **InstrMod[1:0]**: CC Enable Source Select (0=keep, 1=invert, 2=set from Imm12)
- **Key usage patterns (from existing Quasar kernels)**:
  - `TTI_SFPENCC(1, 2)` = enable CC (Imm12=1 -> CC.En=1; InstrMod=2 -> CC.En from Imm12[0])
  - `TTI_SFPENCC(0, 2)` = disable CC (Imm12=0 -> CC.En=0)
  - `TTI_SFPENCC(0, 0)` = reset CC.Res=1 for all lanes, keep CC.En
*(Source: Page 1170505767, SFPENCC section)*

### SFPMOV (Opcode 0x7C, Encoding O2)
- **Purpose**: Moves register value from RG[VC] to RG[VD]
- **Formats**: SMAG32/UINT32 -> SMAG32/UINT32
- **IPC**: 1; **Latency**: 1
- **InstrMod modes**:
  - 0: Copy without modification
  - 1: Copy with sign inversion (negate)
  - 2: Unconditional copy (ignores LaneEnabled)
  - 8: Copy from RS[VC] (read special registers)
- **Predicated by CC** (except InstrMod=2)
*(Source: Page 1170505767, SFPMOV section)*

### SFPABS (Opcode 0x7D, Encoding O2)
- **Purpose**: Returns absolute value of RG[VC] -> RG[VD]
- **Formats**: Input/Output: FP32 or INT32
- **IPC**: 1; **Latency**: 1
- **InstrMod[0]**: Format Select (0=INT32, 1=FP32)
- **For FP32**: NaN preserved as-is; Inf gets sign cleared; normal values get sign cleared
- **For INT32**: MAX_NEG saturates to MAX_POS
*(Source: Page 1170505767, SFPABS section)*

### SFPSETSGN (Opcode 0x89, Encoding O2)
- **Purpose**: Sets the sign of RG[VC] to either sign of RG[VD] or Imm12[0], stores in RG[VD]
- **Formats**: FP32 -> FP32
- **IPC**: 1; **Latency**: 1
- **InstrMod[0]**: Source Select (0=sign from RG[VD], 1=sign from Imm12[0])
- **Algorithm**: Copies exp and man from RG[VC], sets sign from selected source
*(Source: Page 1170505767, SFPSETSGN section)*

### SFPMUL (Opcode 0x86, Encoding O4)
- **Purpose**: Alias of SFPMAD with VC=0 (multiply only, no add)
- **Formats**: FP32 -> FP32
- **IPC**: 1; **Latency**: 2
- **Flushes subnormals**: Yes
*(Source: Page 1170505767, SFPMUL section)*

### SFPAND (Opcode 0x7E, Encoding O2)
- **Purpose**: Bitwise AND between RG[VC] and RG[VD] -> RG[VD]
- **Formats**: UINT32 -> UINT32
- **IPC**: 1; **Latency**: 1
*(Source: Page 1170505767, SFPAND section)*

### SFPOR (Opcode 0x7F, Encoding O2)
- **Purpose**: Bitwise OR between RG[VC] and RG[VD] -> RG[VD]
- **Formats**: UINT32 -> UINT32
- **IPC**: 1; **Latency**: 1
*(Source: Page 1170505767, SFPOR section)*

### SFPNOT (Opcode 0x80, Encoding O2)
- **Purpose**: Bitwise NOT of RG[VC] -> RG[VD]
- **Formats**: UINT32 -> UINT32
- **IPC**: 1; **Latency**: 1
*(Source: Page 1170505767, SFPNOT section)*

### SFPGT (Opcode 0x97, Encoding O2)
- **Purpose**: Tests if RG[VD] > RG[VC]; updates CC.Res and/or CCStack
- **Formats**: FP32 or INT32
- **IPC**: 1; **Latency**: 1
- **InstrMod[0]**: CC Result Update Enable
- **InstrMod[1]**: Stack Update Enable
- **InstrMod[3]**: LREG Update Enable (writes 0xFFFFFFFF or 0x00000000 to VD)
- **Imm12[0]**: Format Select (0=INT32, 1=FP32)
*(Source: Page 1170505767, SFPGT section)*

## 4. Instruction Cross-Check in assembly.yaml

All required instructions confirmed present in `tt_llk_quasar/instructions/assembly.yaml`:
- SFPLOAD: line 5558
- SFPLOADI: line 5627
- SFPSTORE: line 5658
- SFPSETCC: line 5817
- SFPMOV: line 5827
- SFPABS: line 5837
- SFPAND: line 5847
- SFPOR: line 5857
- SFPNOT: line 5867
- SFPMAD: line 5907
- SFPMUL: line 5967
- SFPSETSGN: line 5997
- SFPENCC: line 6007
- SFPCOMPC: line 6017
- SFPGT: (present as part of newer ISA)
- SFPNONLINEAR: (present, used for EXP/RELU etc.)

## 5. Data Format Support and Conversion Rules

### SFPU Internal Format
- SFPU internally operates on **FP32** (32-bit IEEE 754) or **SMAG32** (sign-magnitude 32-bit integer) or **UINT32**
- All loads convert input format to FP32/SMAG32/UINT32 in LREGs
- All stores convert from LREG format back to target register file format
*(Source: Page 1170505767, SFPLOAD/SFPSTORE sections)*

### Dest Register Storage Formats
| Format | Storage | Mode |
|--------|---------|------|
| FP16a | `{sgn, man[9:0], exp[4:0]}` in 16-bit row | 16-bit mode |
| FP16b | `{sgn, man[6:0], exp[7:0]}` in 16-bit row | 16-bit mode |
| FP32 | Split: row i = `{sgn, man[22:16], exp[7:0]}`, row i+N/2 = `{man[15:0]}` | 32-bit mode |
| Int8 (signed) | `{sgn, 3'b0, mag[6:0], 5'd16}` sign-magnitude | 16-bit mode |
| Int8 (unsigned) | `{3'b0, val[7:0], 5'd16}` | 16-bit mode |
| Int16 (signed) | `{sgn, mag[14:0]}` sign-magnitude | 16-bit mode |
| Int16 (unsigned) | `{val[15:0]}` | 16-bit mode |
| Int32 (signed) | Split: row i = `{sgn, mag[22:16], mag[30:23]}`, row i+N/2 = `{mag[15:0]}` | 32-bit mode |
*(Source: Page 80674824 - Dest storage formats)*

### Format Encodings (Quasar-supported)
```
FLOAT32   = 0    (E8M23)
TF32      = 4    (E8M10, stored in 32b)
FLOAT16   = 1    (E5M10)
FLOAT16_B = 5    (E8M7)
MXFP8R    = 18   (E5M2 with block exp)
MXFP8P    = 20   (E4M3 with block exp)
INT32     = 8
INT8      = 14
INT16     = 9
UINT8     = 17
```
*(Source: Page 237174853 - Tensix Formats)*

### Unpacker Conversions (relevant to sign kernel)
The unpacker converts data from L1 format to register file format:
- Float32 -> Float32, TF32, Float16, Float16_B
- Float16/Float16_B/FP8R/FP8P/MXFP8R/MXFP8P etc. -> Float16, Float16_B
- INT32 -> INT32 (Unpack-to-Dest only)
- INT8 -> INT8
- UINT8 -> UINT8
- INT16 -> INT16
- MX formats get unpacked to Float16 or Float16_B in src registers
*(Source: Page 237174853, Unpacker Format Conversions)*

### Packer Conversions (relevant to sign kernel)
- Float32 -> any supported format (Float32, TF32, Float16, Float16_B, FP8, MX, MXINT, etc.)
- Float16/Float16_B -> Float16, Float16_B, FP8, MX formats
- INT32 -> INT32, INT8, UINT8
- INT8 -> INT8
- UINT8 -> UINT8
- INT16 -> INT16
*(Source: Page 237174853, Packer Format Conversions)*

## 6. Format Support Matrix for Sign Kernel

The sign function is a **float-domain operation** that maps any floating-point value to {-1.0, 0.0, +1.0}. It operates on the sign and magnitude of the input.

| Format | Applicable | Constraints | Notes |
|--------|-----------|-------------|-------|
| **Float16** | YES | Primary test format | Standard FP16 (E5M10) |
| **Float16_b** | YES | Primary test format | BFloat16 (E8M7), most common Quasar format |
| **Float32** | YES | Requires dest_acc=Yes (32-bit dest mode) | Full precision FP32 |
| **Tf32** | YES | Requires dest_acc=Yes | TensorFloat-32, stored as FP32 in Dest/SrcS |
| **Int32** | NO | Sign is a float operation; input=int doesn't make semantic sense | sign() returns float |
| **Int16** | NO | Same as Int32 | Not applicable |
| **Int8** | NO | Same as Int32 | Not applicable |
| **UInt8** | NO | Same as Int32; unsigned values are all non-negative | Not applicable |
| **MxFp8R** (enum 18) | YES | L1-only format; unpacked to Float16_b for math; requires implied_math_format | MX block format |
| **MxFp8P** (enum 20) | YES | Same as MxFp8R | MX block format |

### Key Format Constraints
1. **MX formats** (MxFp8R, MxFp8P): Stored in L1 with block exponents. Unpacked to Float16_b for register storage. The sign kernel operates on the unpacked Float16_b representation, so MX formats work transparently.
2. **Float32/Tf32**: Require 32-bit Dest mode (dest_acc_en=true). The SFPU LOAD uses 32-bit addressing in this mode.
3. **Integer formats**: The sign function is mathematically defined for floating-point values. The output is always a float ({-1.0, 0.0, +1.0}), so integer inputs don't apply.
4. **Subnormal handling**: The SFPU flushes subnormals to zero in many operations. For sign, subnormals should be treated as zero (sign(subnormal) = 0). The SFPSETCC zero check uses SMAG32 comparison which is a true zero check (subnormals NOT treated as zero by SFPSETCC), but the SFPU load itself flushes subnormals depending on the format conversion.

*(Sources: Page 237174853 - Tensix Formats; Page 80674824 - Dest storage formats; Page 1170505767 - SFPU ISA)*

## 7. Pipeline Constraints and Instruction Ordering

### General SFPU Constraints
- Most Simple EXU operations complete in 1 cycle (S3), fully pipelined
- MAD EXU operations take 2 cycles (S3-S4), fully pipelined
- Store to SrcS: 2 cycle latency; Store to Dest: 3 cycle latency
- SFPCONFIG requires 1 idle cycle after it (writes affect S2 logic)
- SWAP requires 1 idle cycle between SWAPs (no pipelining registers S3->S4)
*(Source: Page 1256423592, Implementation Details)*

### Conditional Execution
- CC (Condition Code) system has two components: CC.En (enable) and CC.Res (result)
- `LaneEnabled = CC.En ? CC.Res : true` (when CC disabled, all lanes are enabled)
- CC.En and CC.Res are per-lane
- SFPSETCC, SFPCOMPC, SFPENCC manipulate CC state
- Most instructions are predicated by LaneEnabled; SFPENCC and SFPCOMPC execute unconditionally
- CC Stack provides nesting capability for if/else constructs
*(Source: Page 1170505767, Conditional Execution Instructions)*

### LOADMACRO Rules
- LOADMACRO engine can dispatch up to 5 instructions per cycle (1 per execution unit)
- Load EXU has no LOADMACRO slot (only accepts from main pipeline)
- Sequence registers define delay fields for when each sub-instruction fires
- LOADMACRO is primarily for optimized instruction scheduling; not needed for simple kernels like sign
*(Source: Page 1256423592, LOADMACRO Engine)*

### Row Increment Pattern (Quasar-specific)
- Quasar SFPU processes 2 rows at a time (`SFP_ROWS = 2`)
- After processing each set of rows: `_incr_counters_<0x0, 0x0, ckernel::math::SFP_ROWS, 0x0>()`
- This increments the Dest address counter by SFP_ROWS (2)
- The loop runs `TEST_FACE_R_DIM / SFP_ROWS` iterations per face
*(Source: Existing Quasar implementations - ckernel_sfpu_abs.h, ckernel_sfpu_elu.h, etc.)*

## 8. Blackhole Differences (Porting Notes)

### Blackhole sign implementation (reference)
The Blackhole sign kernel uses **SFPI** (Software-managed SFPU Programming Interface) with:
- `sfpi::vFloat v = sfpi::dst_reg[0]` (vector load)
- `sfpi::dst_reg[0] = sfpi::vConst1` (store +1.0)
- `v_if (v < 0.0F) { sfpi::dst_reg[0] = sfpi::vConstNeg1; } v_endif;` (conditional -1.0)
- `v_if (_sfpu_is_fp16_zero_(v, exponent_size_8)) { sfpi::dst_reg[0] = sfpi::vConst0; } v_endif;` (zero check)
- `sfpi::dst_reg++` (advance to next row)

### Key Differences for Quasar
1. **No SFPI**: Quasar does NOT use the SFPI C++ abstraction layer. Instead, it uses direct TTI (Tensix Tightly Issued) instruction macros like `TTI_SFPLOAD`, `TTI_SFPSTORE`, `TTI_SFPSETCC`, etc.
2. **No template parameters**: Quasar kernels do NOT use `template <bool APPROXIMATION_MODE, int ITERATIONS>`. They use plain `inline void` functions.
3. **No exponent_size_8 parameter**: Quasar handles zero detection differently. The Blackhole `_sfpu_is_fp16_zero_` helper deals with FP16a bias quirks that don't apply to Quasar's approach.
4. **Row processing**: Blackhole processes 1 row per iteration with `dst_reg++`; Quasar processes 2 rows per iteration with `_incr_counters_`.
5. **Includes**: Quasar uses `"ckernel_trisc_common.h"` and `"cmath_common.h"` instead of `"sfpi.h"`.
6. **Function signature**: Quasar pattern is `_calculate_{op}_sfp_rows_()` for inner loop + `_calculate_{op}_(const int iterations, ...)` for outer.
7. **Conditional execution**: Quasar uses explicit TTI_SFPENCC/TTI_SFPSETCC/TTI_SFPCOMPC for CC management rather than SFPI v_if/v_endif macros.

### DeepWiki Confirmation
DeepWiki for tenstorrent/tt-isa-documentation confirms:
- SFPSETSGN, SFPABS, SFPSETCC, SFPMOV are available for sign computation
- Conditional execution via SFPENCC/SFPSETCC/SFPCOMPC is the standard approach
- Sign function can be implemented via: check negative (SFPSETCC mod 0), conditionally store -1.0, complement CC (SFPCOMPC), store +1.0, check zero (SFPSETCC mod 6), store 0.0
*(Source: DeepWiki query on tenstorrent/tt-isa-documentation)*

## 9. Recommended Implementation Strategy

### Algorithm for sign(x)
```
// Pre-loop: enable CC, load constants
TTI_SFPLOADI(LREG1, FP16_B, 0x3F80)  // +1.0 in FP16_B
TTI_SFPLOADI(LREG2, FP16_B, 0xBF80)  // -1.0 in FP16_B
TTI_SFPENCC(1, 2)                      // enable conditional execution

// Per-row iteration:
TTI_SFPLOAD(LREG0, DEFAULT, ADDR_MOD_7, 0, 0)  // load x from Dest

TTI_SFPSETCC(0, LREG0, 0)              // CC.Res = (x < 0) [InstrMod=0: set if negative]

TTI_SFPMOV(LREG2, LREG3, 0)            // LREG3 = -1.0 (conditional: only for negative lanes)

TTI_SFPCOMPC(0, 0)                      // complement CC: now CC.Res = 1 for non-negative lanes

TTI_SFPMOV(LREG1, LREG3, 0)            // LREG3 = +1.0 (conditional: only for non-negative lanes)

TTI_SFPENCC(0, 0)                       // reset CC.Res = 1 for all lanes
TTI_SFPSETCC(0x800, LREG0, 6)          // CC.Res = (x == 0) [InstrMod=6, Imm12[11]=1 for FP mode]

TTI_SFPLOADI(LREG4, FP16_B, 0x0000)    // 0.0
TTI_SFPMOV(LREG4, LREG3, 0)            // LREG3 = 0.0 (conditional: only for zero lanes)

TTI_SFPENCC(0, 0)                       // clear CC for store

TTI_SFPSTORE(LREG3, DEFAULT, ADDR_MOD_7, 0, 0)  // store result

// Post-loop: disable CC
TTI_SFPENCC(0, 2)
```

**Note**: The exact register allocation and instruction sequence will be refined by the planner/writer agents. This is a high-level strategy showing the key instructions and their roles.

## 10. Existing Quasar Patterns (Code Style Reference)

From `ckernel_sfpu_elu.h` (closest pattern to sign - uses CC):
```cpp
inline void _calculate_elu_sfp_rows_()
{
    TTI_SFPLOAD(p_sfpu::LREG0, p_sfpu::sfpmem::DEFAULT, ADDR_MOD_7, 0, 0);
    TTI_SFPSETCC(0, p_sfpu::LREG0, 0);  // CC for negative
    // ... operations on negative lanes ...
    TTI_SFPENCC(0, 0);  // reset CC
    TTI_SFPSTORE(p_sfpu::LREG0, 0, ADDR_MOD_7, 0, 0);
}

inline void _calculate_elu_(const int iterations, const std::uint32_t slope)
{
    TTI_SFPLOADI(p_sfpu::LREG2, 0, (slope >> 16));
    TTI_SFPENCC(1, 2);  // enable CC
    for (int d = 0; d < iterations; d++)
    {
        _calculate_elu_sfp_rows_();
        ckernel::math::_incr_counters_<0x0, 0x0, ckernel::math::SFP_ROWS, 0x0>();
    }
    TTI_SFPENCC(0, 2);  // disable CC
}
```

From `ckernel_sfpu_threshold.h` (also uses CC + SFPCOMPC-style conditional):
```cpp
inline void _calculate_threshold_sfp_rows_()
{
    TTI_SFPLOAD(p_sfpu::LREG0, p_sfpu::sfpmem::DEFAULT, ADDR_MOD_7, 0, 0);
    TTI_SFPGT(0, p_sfpu::LREG0, p_sfpu::LREG2, 0x1);  // CC from comparison
    TTI_SFPMOV(p_sfpu::LREG3, p_sfpu::LREG0, 0);        // conditional move
    TTI_SFPENCC(0, 0);                                     // clear CC
    TTI_SFPSTORE(p_sfpu::LREG0, 0, ADDR_MOD_7, 0, 0);
}
```

## 11. p_sfpu Constants Reference

From existing Quasar code, the following `p_sfpu::` constants are used:
- `p_sfpu::LREG0` through `p_sfpu::LREG7` - LREG indices
- `p_sfpu::LCONST_0`, `p_sfpu::LCONST_1`, `p_sfpu::LCONST_neg1` - constant register indices
- `p_sfpu::sfpmem::DEFAULT` - implied format mode (InstrMod=0)
- `p_sfpu::sfpmem::INT32` - INT32 store mode (InstrMod=4 for SMAG32)
- `ADDR_MOD_7` - standard address mode for load/store operations
