# Architecture Research Brief: abs kernel for Quasar

## 1. SFPU Execution Model

### Overview
The SFPU (SIMD Floating Point Unit) on Quasar/Trinity is a functional unit within the Tensix compute engine that complements the main FPU. It handles complex functions (reciprocal, sigmoid, exponential, absolute value, etc.) and supports conditional execution. Despite its name, it also handles integer data types.

**Source:** Confluence page 1256423592 (Quasar/Trinity SFPU MAS), Introduction section

### Configuration
- **Quasar/Trinity is a half-sized configuration**: 8 slices, 4 rows per slice = **32 SFPU lanes total**
- Each slice is connected to a corresponding SrcS Register slice and Dest Register slice
- All slices execute the **same instruction sequence** (SIMD) on their own local data
- Within each slice, a single instance of instruction decode logic is shared among the 4 rows
- SFP_ROWS = 2 (the number of rows processed per SFPU iteration step in software)

**Source:** Confluence page 1256423592, Introduction section

### Pipeline Stages
- The SFPU lives in Stages 2-5 (S2-S5) of the Tensix pipeline
- S2: Instruction Frontend (decode, LOADMACRO engine)
- S3: Most execution (Simple EXU, MAD EXU, etc.)
- S4: Some stores (SrcS stores complete in S4, latency=2)
- S5: Dest stores complete (latency=3)

**Source:** Confluence page 1256423592, Implementation Details section

### Execution Model per Iteration
For each iteration in a kernel loop:
1. **SFPLOAD**: Load data from Dest register into an LREG (1 cycle latency)
2. **Compute instruction(s)**: Process data in LREGs (e.g., SFPABS takes 1 cycle)
3. **SFPSTORE**: Write result from LREG back to Dest register (2-3 cycle latency)
4. **_incr_counters_**: Advance the Dest read/write counters by SFP_ROWS (=2)

**Source:** Existing Quasar implementations (ckernel_sfpu_relu.h, ckernel_sfpu_square.h, etc.)

## 2. Register File Layouts

### LREG GPRs (General Purpose Registers)
- **8 LREGs per lane** divided into 2 banks of 4:
  - LREGS1: LREGs 0-3
  - LREGS2: LREGs 4-7
- Each LREG is 32 bits wide
- Addressable via constants: `p_sfpu::LREG0` (0), `p_sfpu::LREG1` (1), `p_sfpu::LREG2` (2), `p_sfpu::LREG3` (3)
- LREGs 4-7 exist but are less commonly used
- 8 standard read ports, 3 standard write ports, 1 bulk read port, 1 bulk write port per bank

**Source:** Confluence page 1256423592, LREG Storage section

### Constant Registers
- 6 constant registers per SFPU Slice (not per lane)
- Constants 2-5 accessible via RG register view
- All 6 exposed to LUT coefficient unpacker via RL register view
- `p_sfpu::LCONST_0` etc. for accessing fixed constants

**Source:** Confluence page 1256423592, Constant Registers section

### SrcS Registers
- 2 banks, each with 3 slices of registers
- Each slice: 2 instances of tt_reg_bank, each 4 rows x 16 columns x 16 bits
- Total: 768B of storage across both banks (3 x 8x16x16b slices)
- Slices 0-1: unpacker writes data; Slice 2: SFPU writes results; Packer reads from slice 2
- Bank switching controlled by synchronization signals (data_valid, pack_ready, sfpu_wr_ready)
- Format: supports 16-bit and 32-bit modes; in 32-bit mode, upper 16 bits in one physical row, lower 16 in another

**Source:** Confluence page 141000706 (srcS registers)

### Dest Register
- Two modes: 16-bit (1024 rows) and 32-bit (512 rows, pairs combined)
- SFPU reads from Dest via SFPLOAD, writes back via SFPSTORE
- In 32-bit mode: physical row `i` has MSBs, physical row `i+N/2` has LSBs
- Supported storage formats: FP16a, FP16b, FP32, Int8 (signed/unsigned), Int16 (signed/unsigned), Int32 (signed)
- **NOT supported in Dest**: Int32 unsigned, Tf19, any BFP format, LF8

**Source:** Confluence pages 195493892 (Dest), 80674824 (Dest storage formats)

### SrcA/B Registers
- Arrays of 19-bit containers
- 2 banks of 64 rows x 16 columns x 19-bit datums = 38912 bits each
- Storage formats: FP16a, FP16b, Tf19 (confusingly called Tf32), Int8 (signed/unsigned), Int16 (signed), MXFP4_2x_A/B
- **NOT supported in SrcA/B**: Int32, FP32, Int16 unsigned, any BFP, LF8

**Source:** Confluence page 83230723 (SrcA/B storage formats)

## 3. Instructions Needed for abs Kernel

### SFPLOAD (Opcode 0x70)
**Purpose:** Load data from Dest or SrcS into an LREG

**Quasar Macro:**
```c
TTI_SFPLOAD(lreg_dest, instr_mod, addr_mod, done, addr)
```

**Key Parameters:**
- `lreg_dest`: Target LREG (e.g., `p_sfpu::LREG0`)
- `instr_mod`: Format select. For abs with floating point, use `p_sfpu::sfpmem::DEFAULT` (implied format, 0x0) to auto-detect format from Dest register. For 32-bit int data, use `p_sfpu::sfpmem::INT32` (0x4, SMAG32 mode).
- `addr_mod`: Address modifier (typically `ADDR_MOD_7` = all zeroes)
- `done`: Toggle bank ID (0 = don't toggle)
- `addr`: Base address (typically 0)

**Formats:**
| InstrMod | Register File Format | LREG Format |
|----------|---------------------|-------------|
| 0x0 (IMPLIED) | Implied (FP16a, FP16b, or FP32) | FP32 |
| 0x3 (FP32) | MOD_FP32 | FP32 |
| 0x4 (SMAG32) | MOD_SMAG32 | SMAG32 |

**Performance:** IPC=1, Latency=1 cycle

**Source:** Confluence page 1170505767 (SFPU ISA), SFPLOAD section

### SFPABS (Opcode 0x7D)
**Purpose:** Compute absolute value

**Quasar Macro:**
```c
TTI_SFPABS(lreg_c, lreg_dest, instr_mod1)
```

**IMPORTANT: Quasar API difference from Blackhole/Wormhole:**
- Quasar: `TTI_SFPABS(lreg_c, lreg_dest, instr_mod1)` — 3 parameters (no imm12_math)
- Blackhole: `TTI_SFPABS(imm12_math, lreg_c, lreg_dest, instr_mod1)` — 4 parameters
- The `imm12_math` parameter was removed from the Quasar encoding

**Encoding:** O2 format — `{lreg_c[11:8], lreg_dest[7:4], instr_mod1[3:0]}`

**Key Parameters:**
- `lreg_c`: Source LREG containing value to take absolute value of
- `lreg_dest`: Destination LREG for result
- `instr_mod1`: Format select:
  - `0`: INT32 (2's complement) absolute value
  - `1`: FP32 (sign-magnitude/floating-point) absolute value

**Algorithmic Implementation:**
```c
if (LaneEnabled) {
  if (InstrMod[0]) {  // FP32 mode
    if (isNaN(RG[VC].FP32)) {
      RG[VD].FP32 = RG[VC].FP32;  // NaN passes through unchanged
      Flags.NaN = 1;
    } else if (isInf(RG[VC].FP32)) {
      RG[VD].FP32 = RG[VC].FP32;
      RG[VD].FP32.Sgn = 0;  // -Inf becomes +Inf
      Flags.Inf = 1;
    } else {
      RG[VD].FP32 = RG[VC].FP32;
      RG[VD].FP32.Sgn = 0;  // Clear sign bit
    }
  } else {  // INT32 mode
    if (RG[VC].INT32 == INT32::MAX_NEG) {
      RG[VD].INT32 = INT32::MAX_POS;  // Saturate -2^31 to +2^31-1
      Flags.Overflow = 1;
    } else if (RG[VC].INT32 < 0) {
      RG[VD].INT32 = -RG[VC].INT32;
    } else {
      RG[VD] = RG[VC];
    }
  }
}
```

**Performance:** IPC=1, Latency=1 cycle
**Execution Unit:** Simple EXU (Integer Adder pathway)
**Sets CC Result?** No
**Sets Exception Flags?** Yes (NaN, Inf, Overflow)
**Flushes Subnormals?** No

**Hardware Implementation:** In the Simple EXU, ABS maps to `z = (x < 0) ? (0 - x) : (0 + x)` via the integer adder. The carry-in to the adder LSB is controlled through the sign bit of the input (for ABS). For FP32 mode, the sign bit is simply cleared.

**Source:** Confluence page 1612186129 (SFPABS ISA page), page 1170505767 (SFPU ISA), page 1256423592 (SFPU MAS Simple EXU section)

### SFPSTORE (Opcode 0x72)
**Purpose:** Write data from LREG back to Dest or SrcS

**Quasar Macro:**
```c
TTI_SFPSTORE(lreg_src, instr_mod, addr_mod, done, addr)
```

**Key Parameters:**
- `lreg_src`: Source LREG to store from
- `instr_mod`: Format select (0 = IMPLIED, matches load format)
- `addr_mod`: Address modifier (typically `ADDR_MOD_7`)
- `done`: Toggle bank ID (0 = don't toggle)
- `addr`: Base address (typically 0)

**Performance:** IPC=1, Latency=2 cycles (SrcS) or 3 cycles (Dest)

**Source:** Confluence page 1170505767, SFPSTORE section

## 4. Data Format Support and Conversion Rules

### SFPU Internal Format
- SFPU always works internally in **32-bit representation** (FP32 or SMAG32/INT32)
- On load, data from Dest (which may be FP16a, FP16b, or FP32) is converted to FP32 in the LREG
- On store, LREG FP32 data is converted back to the target register format

### Format Conversion on Load (Dest -> LREG)
- FP16a in Dest -> FP32 in LREG (expand mantissa, rebias exponent)
- FP16b in Dest -> FP32 in LREG (expand mantissa)
- FP32 in Dest -> FP32 in LREG (de-swizzle)
- SMAG32 in Dest -> SMAG32 in LREG (for integer operations)

### Format Conversion on Store (LREG -> Dest)
- FP32 in LREG -> FP16a in Dest (truncate mantissa, re-bias exponent)
- FP32 in LREG -> FP16b in Dest (truncate mantissa)
- FP32 in LREG -> FP32 in Dest (swizzle)

### Implied Format
When using `p_sfpu::sfpmem::DEFAULT` (IMPLIED mode), the SFPLOAD/SFPSTORE instructions auto-detect the format from the register file state:
- If Dest was written with FP16_b format data, SFPLOAD reads as FP16_b and converts to FP32
- If Dest was written with Float16 format data, SFPLOAD reads as Float16 and converts to FP32
- If Dest was written with FP32 format data, SFPLOAD reads as FP32

**Source:** Confluence page 1170505767 (SFPU ISA, Appendix A, SFPLOAD section), page 237174853 (Tensix Formats)

### Unpacker Format Conversions (L1 -> Registers)
For the abs kernel test pipeline: L1 data -> Unpacker -> Dest -> SFPU -> Dest -> Packer -> L1

Key unpacker conversions:
| Input (L1) | Output (Dest/SrcS) |
|---|---|
| Float32 | Float32, TF32, Float16, Float16_B |
| Float16, Float16_B | Float16, Float16_B |
| MxFp8R, MxFp8P | Float16, Float16_B |
| Int32 | Int32 (Dest only) |
| Int8 | Int8 |
| UInt8 | UInt8 |
| Int16 | Int16 |

**Source:** Confluence page 237174853 (Tensix Formats, Unpacker Format Conversions)

### Packer Format Conversions (Registers -> L1)
| Input (Dest) | Output (L1) |
|---|---|
| Float32 | Float32, TF32, Float16, Float16_B, FP8R, FP8P, MxFp8R, MxFp8P, ... |
| Float16, Float16_B | Float16, Float16_B, FP8R, FP8P, MxFp8R, MxFp8P, ... |
| Int32 | Int32, Int8, UInt8 |
| Int8 | Int8 |
| UInt8 | UInt8 |
| Int16 | Int16 |

**Source:** Confluence page 237174853 (Tensix Formats, Packer Format Conversions)

## 5. Format Support Matrix for abs Kernel

The `abs` kernel performs absolute value, which is a universal operation applicable to both floating-point and integer data. SFPABS supports both FP32 mode (instr_mod1=1) and INT32 mode (instr_mod1=0).

### Applicable Formats

| Format | Enum Value | Applicable? | Constraints | Notes |
|--------|-----------|-------------|-------------|-------|
| **Float16** | 1 | YES | None | Unpacked to FP32 in LREG, abs clears sign bit, stored back |
| **Float16_b** | 5 | YES | None | Most common format; unpacked to FP32 in LREG |
| **Float32** | 0 | YES | Requires 32-bit Dest mode | Full precision abs |
| **Tf32** | 4 | YES | Stored as FP32 in Dest/SrcS | Unpacked to FP32 containers |
| **MxFp8R** | 18 | YES | MX format, L1 only; unpacked to Float16_b for math | Needs implied_math_format handling |
| **MxFp8P** | 20 | YES | MX format, L1 only; unpacked to Float16_b for math | Needs implied_math_format handling |
| **Int32** | 8 | YES* | Requires SMAG32 load mode, dest_acc=Yes for 32-bit | Integer abs via SFPABS with instr_mod1=0 |
| **Int16** | 9 | CONDITIONAL | Limited SFPU support | Int16 uses sign+magnitude in Dest; may need special handling |
| **Int8** | 14 | CONDITIONAL | Limited range | Sign+magnitude in registers; abs would clear sign |
| **UInt8** | 17 | NO | Already unsigned | abs(unsigned) = identity; pointless |
| **UInt16** | 130 | NO | Already unsigned | abs(unsigned) = identity; pointless |

### Format Domain
- **Primary domain**: Float (Float16, Float16_b, Float32, Tf32) using `instr_mod1=1`
- **Secondary domain**: Integer (Int32) using `instr_mod1=0`
- **MX formats**: Supported via unpacking to Float16_b

### Format-Dependent Code Paths
The reference Blackhole implementation uses `sfpi::abs(v)` which operates on `vFloat` — this corresponds to FP32 mode only (clearing the sign bit). The Quasar implementation should primarily target FP32 mode (instr_mod1=1) for floating-point formats since:
1. Data is always loaded as FP32 in the LREG for float formats
2. SFPABS with instr_mod1=1 simply clears the sign bit of FP32 data
3. The Blackhole reference only implements the float path

For Int32 support, a separate code path would be needed with SFPLOAD in SMAG32 mode and SFPABS with instr_mod1=0. However, the reference implementation does NOT include an Int32 path, and the standard test infrastructure for abs tests against float formats.

### Recommended Test Formats
For the abs kernel, the following formats should be tested:
- **Must test**: Float16_b, Float16, Float32
- **Should test**: Tf32, MxFp8R, MxFp8P (if test infrastructure supports)
- **Optional**: Int32 (only if int32 abs variant is implemented)
- **Skip**: UInt8, UInt16 (unsigned types, abs is identity), Int8/Int16 (limited SFPU support for these)

**Source:** Confluence pages 237174853 (Tensix Formats), 80674824 (Dest storage formats), 1170505767 (SFPU ISA, SFPABS section), tests/python_tests/helpers/format_config.py (QUASAR_DATA_FORMAT_ENUM_VALUES), tests/python_tests/helpers/data_format_inference.py (VALID_QUASAR_SRC_REG_FORMATS, VALID_QUASAR_DEST_REG_FORMATS)

## 6. Pipeline Constraints and Instruction Ordering

### General SFPU Pipeline Rules
1. **SFPLOAD latency = 1 cycle**: Data available in LREG one cycle after load issues
2. **SFPABS latency = 1 cycle**: Result available in destination LREG one cycle after issue
3. **SFPSTORE latency = 2-3 cycles**: 2 cycles for SrcS, 3 cycles for Dest
4. SFPU is **fully pipelined** for all instructions except global SFPSHFT2 variants and SFPSWAP

### Instruction Ordering for abs Kernel
The abs kernel is extremely simple — just 3 instructions per iteration:
```
SFPLOAD  -> SFPABS -> SFPSTORE
```
No special ordering constraints beyond the natural data dependency chain.

### LOADMACRO Rules
- LOADMACRO allows up to 5 instructions in parallel (1 per execution unit)
- For the abs kernel, LOADMACRO is NOT needed — the operation is too simple to benefit from it
- If used: Load unit has no LOADMACRO slot (always from main pipeline), other units can be scheduled via LOADMACRO

### Iteration Pattern
Each iteration processes SFP_ROWS=2 rows of data:
```c
for (int d = 0; d < iterations; d++) {
    _calculate_abs_sfp_rows_();  // SFPLOAD + SFPABS + SFPSTORE
    ckernel::math::_incr_counters_<0x0, 0x0, ckernel::math::SFP_ROWS, 0x0>();
}
```

**Source:** Confluence page 1256423592 (SFPU MAS), existing Quasar implementations

## 7. Blackhole Differences (Porting Notes)

### Key Differences

| Aspect | Blackhole | Quasar |
|--------|-----------|--------|
| **API** | Uses SFPI intrinsics (`sfpi::abs()`, `vFloat`, `dst_reg[]`) | Uses TTI macros (`TTI_SFPLOAD`, `TTI_SFPABS`, `TTI_SFPSTORE`) |
| **TTI_SFPABS params** | 4 params: `(imm12_math, lreg_c, lreg_dest, instr_mod1)` | 3 params: `(lreg_c, lreg_dest, instr_mod1)` — no imm12_math |
| **SFPU lanes** | 16 slices x 4 rows = 64 lanes (full-sized) | 8 slices x 4 rows = 32 lanes (half-sized) |
| **Includes** | `#include "sfpi.h"` | `#include "ckernel_trisc_common.h"` + `#include "cmath_common.h"` |
| **Iteration model** | `sfpi::dst_reg[0]` / `sfpi::dst_reg++` | `TTI_SFPLOAD`/`TTI_SFPSTORE` + `_incr_counters_` |
| **Function signature** | `template <bool APPROXIMATION_MODE, int ITERATIONS>` | `inline void _calculate_abs_(const int iterations)` (no templates for simple ops) |
| **Namespace** | `ckernel::sfpu` | `ckernel::sfpu` (same) |

### Blackhole Reference Implementation
```cpp
template <bool APPROXIMATION_MODE, int ITERATIONS>
inline void _calculate_abs_(const int iterations) {
    for (int d = 0; d < iterations; d++) {
        sfpi::vFloat v   = sfpi::dst_reg[0];
        sfpi::dst_reg[0] = sfpi::abs(v);
        sfpi::dst_reg++;
    }
}
```

### What `sfpi::abs(v)` does internally
On Blackhole, `sfpi::abs()` on a `vFloat` clears the sign bit (equivalent to SFPABS with instr_mod1=1 for FP32 data). The Quasar port must translate this to explicit TTI instruction calls.

### Quasar Equivalent Pattern
Based on existing Quasar kernels (relu, square, recip, etc.):
```cpp
inline void _calculate_abs_sfp_rows_() {
    TTI_SFPLOAD(p_sfpu::LREG0, p_sfpu::sfpmem::DEFAULT, ADDR_MOD_7, 0, 0);
    TTI_SFPABS(p_sfpu::LREG0, p_sfpu::LREG0, 1);  // FP32 abs, instr_mod1=1
    TTI_SFPSTORE(p_sfpu::LREG0, 0, ADDR_MOD_7, 0, 0);
}

inline void _calculate_abs_(const int iterations) {
#pragma GCC unroll 8
    for (int d = 0; d < iterations; d++) {
        _calculate_abs_sfp_rows_();
        ckernel::math::_incr_counters_<0x0, 0x0, ckernel::math::SFP_ROWS, 0x0>();
    }
}
```

**Source:** Blackhole reference at tt_llk_blackhole/common/inc/sfpu/ckernel_sfpu_abs.h, Quasar ckernel_ops.h (line 599-601), all existing Quasar SFPU kernels for pattern reference

## 8. Assembly.yaml Cross-Check

Verified SFPABS exists in Quasar ISA:
```yaml
SFPABS:
    op_binary: 0x7D
    ex_resource: SFPU
    ttsync_resource: OTHERS
    instrn_type: SFPU
    src_mask: 0x0
    arguments: *SFPU_MATHI12  # Uses the standard 12-bit math instruction format
    description: "sFPU math instruction absolute value"
```

The SFPU_MATHI12 argument template:
- `instr_mod1` (bits [0:3]): Instruction modifier
- `lreg_dest` (bits [4:7]): LREG destination index
- `lreg_c` (bits [8:11]): LREG source index

This matches the Quasar `TTI_SFPABS(lreg_c, lreg_dest, instr_mod1)` macro.

**Source:** tt_llk_quasar/instructions/assembly.yaml (line 5837)

## 9. Summary for Downstream Agents

### Implementation Complexity: VERY LOW
The abs kernel is one of the simplest possible SFPU kernels:
- Single instruction core operation (SFPABS)
- No constants or LUT lookups needed
- No conditional execution needed
- No multi-step computation
- Standard SFPLOAD/SFPABS/SFPSTORE pattern identical to other simple Quasar SFPU kernels

### Key Implementation Details
1. Use `TTI_SFPABS(src_lreg, dest_lreg, 1)` for FP32 abs (instr_mod1=1 = floating-point mode)
2. Can use same LREG for src and dest (e.g., `TTI_SFPABS(p_sfpu::LREG0, p_sfpu::LREG0, 1)`)
3. Standard SFPLOAD/SFPSTORE wrapper with `ADDR_MOD_7` and `_incr_counters_`
4. Include `"ckernel_trisc_common.h"` and `"cmath_common.h"`
5. The function should be `_calculate_abs_(const int iterations)` — match existing Quasar kernel patterns
6. NaN values pass through unchanged (NaN flag set), -Inf becomes +Inf (Inf flag set)
7. Subnormals are NOT flushed by SFPABS
