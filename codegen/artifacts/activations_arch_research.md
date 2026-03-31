# Quasar SFPU Architecture Research: Activations Kernel

## 1. SFPU Execution Model

### Overview
The SFPU (SIMD Floating Point Unit) on Quasar is a half-sized configuration complementing the main FPU math engine. It handles complex nonlinear functions (reciprocal, sigmoid, exponential, tanh, etc.) and supports both floating-point and integer data types.
**Source: Confluence page 1256423592 (Quasar/Trinity SFPU MAS), Introduction**

### SIMD Organization
- **8 SFPU slices** (half-sized configuration, only even physical columns present)
- **4 rows per slice** for a total of **32 SFPU lanes**
- Each slice is paired with 1 Dest register file slice, 2 FPU columns, and its own SrcS register file slice
- Within each slice, a single instruction decode unit is shared among the 4 rows
- All lanes in a slice execute the same instruction sequence on different data
**Source: Page 1256423592, Introduction + Hardware Overview**

### Pipeline Stages
- SFPU occupies pipeline stages S2-S5 of the Tensix pipeline
- S2: Instruction Frontend (decode, LOADMACRO engine)
- S3: Main execution (Simple EXU, MAD stage 1, Load backend, Config, Round)
- S4: MAD stage 2, SWAP completion, Store to SrcS
- S5: Store to Dest
**Source: Page 1256423592, Implementation Details**

### Instruction Throughput
- Most instructions: **IPC = 1** (one instruction per cycle)
- SFPSWAP: **IPC = 0.5** (requires idle cycle after)
- SFPCONFIG: **IPC = 0.5** (no instructions on following cycle)
- All MAD-type instructions: fully pipelined despite 2-cycle latency
**Source: Page 1170505767 (SFPU ISA), per-instruction tables**

### Instruction Latencies
- **1-cycle latency**: SFPLOAD, SFPLOADI, SFPMOV, SFPSETCC, SFPIADD, SFPSHFT, SFPABS, SFPNOP, SFPENCC, SFPCOMPC, SFPPUSHC, SFPPOPC, SFPCONFIG, SFPNONLINEAR/SFPARECIP
- **2-cycle latency**: SFPMAD, SFPMUL, SFPMULI, SFPADDI, SFPLUT, SFPLUTFP32, SFPSWAP, SFPSTORE (to SrcS)
- **3-cycle latency**: SFPSTORE (to Dest)
- **No LREG-to-LREG data hazards**: Hardware guarantees no explicit bubbles needed for RAW hazards between LREG operations
**Source: Page 1170505767, Feature Summary + per-instruction tables**

### SFP_ROWS
On Quasar, `SFP_ROWS = 2`. Each SFPU kernel iteration processes 2 rows of the dest register. The `_incr_counters_` template increments the dest address by `SFP_ROWS` after each iteration.
**Source: tt_llk_quasar/common/inc/cmath_common.h line 16**

---

## 2. Register File Layouts

### LREGs (Local Registers) - Per Lane
- **8 GPRs per lane** (LREG0-LREG7), each 32 bits wide
- Divided into 2 banks: LREGS1 (LREG0-3) and LREGS2 (LREG4-7)
- LREGS1: 8 standard read ports, 3 standard write ports, 1 bulk read, 1 bulk write
- LREGS2: same port configuration
- Undefined reset values
- **No data hazards** between LREGs (hardware guarantee)
- 1 additional **Staging Register** per lane (only usable in LOADMACRO sequences)
**Source: Page 1256423592, LREG Storage; Page 1170505767, LREGs**

### Constant Registers - Per Slice (shared across lanes)
- **6 programmable constants** (indices 0-5 in RL view, 9-14 in RG view)
- **4 fixed constants** per slice
- Only writable by SFPCONFIG from Row 0 of each slice
- Key reset values:
  - Programmable 0,1: 0.0 (LUT-only visibility via RL view)
  - Programmable 2 (RG[11]): -1.0
  - Programmable 3 (RG[12]): 0.001953125
  - Programmable 4 (RG[13]): -0.6748776 (fastlog2 coeff)
  - Programmable 5 (RG[14]): -0.34484842 (fastlog2 coeff)
  - Fixed 0 (RG[8]): 0.8373
  - Fixed 1 (RG[9]): 0.0
  - Fixed 2 (RG[10]): 1.0 (LCONST_1)
  - Fixed 3 (RG[11]): {col[3:0], row[1:0]} (lane ID)
**Source: Page 1170505767, Constant Registers**

### Quasar Code Constants
- `p_sfpu::LREG0` through `p_sfpu::LREG7` = 0-7
- `p_sfpu::LCONST_0` = 9 (Fixed Constant 1 = 0.0)
- `p_sfpu::LCONST_1` = 10 (Fixed Constant 2 = 1.0)
**Source: tt_llk_quasar/common/inc/ckernel_instr_params.h**

### SrcS Registers
- 2 banks, each with 3 slices of registers
- Each slice: 2 instances of tt_reg_bank, each 4 rows x 16 columns x 16 bits
- Total: 3 x 8 x 16 x 16b = 768 bytes of storage per bank pair
- Unpacker writes to slices 0-1, SFPU reads from those and writes to slice 2
- Packer reads from slice 2
- Double-buffered with bank switching via done signals
- Supports both 16-bit and 32-bit access modes
- Address bit 10 selects SrcS (1) vs Dest (0) for SFPLOAD/SFPSTORE
**Source: Page 141000706 (srcS registers)**

### Dest Register
- Two modes: 16-bit (1024 rows) and 32-bit (512 rows)
- 16 columns per row
- In 32-bit mode, rows i and i+N/2 combine to form one 32-bit datum
- Supported storage formats: FP16a, FP16b, FP32, Int8 (signed/unsigned), Int16 (signed/unsigned), Int32 (signed only)
- Unsupported in Dest: Int32 unsigned, Tf19, any BFP format, LF8/LF8+
**Source: Page 195493892 (Dest); Page 80674824 (Dest storage formats)**

### SrcA/B Registers
- Arrays of 19-bit containers
- Two banks of 64 rows x 16 columns x 19 bits
- Supported formats: FP16a, FP16b, Tf19 (confusingly called Tf32), Int8 signed/unsigned, Int16 signed, MXFP4_2x_A/B (Quasar only)
- Unsupported: Int32, FP32, Int16 unsigned, BFP, LF8
**Source: Page 83230723 (SrcA/B storage formats)**

---

## 3. Per-Instruction Details for Activation Kernels

The activations kernel on Blackhole uses these activation types: Celu, Elu, Gelu, Hardtanh, Hardsigmoid. The implementation relies on SFPI (high-level C++ intrinsics) which is NOT available on Quasar. Quasar implementations use TTI macros (direct instruction emission).

### SFPLOAD (Opcode 0x70)
- **Encoding**: MR
- **Latency**: 1 cycle
- **Function**: Loads a value from Dest or SrcS register file into RG[VD]
- **Format conversion**: Inline based on InstrMod (FP16_A/B -> FP32, SMAG formats, UINT/INT formats)
- **Key InstrMod values**:
  - 0x0 (IMPLIED): Uses implied format from unpacker
  - 0x1 (FP16_A): FP16 -> FP32 conversion
  - 0x2 (FP16_B): BFloat16 -> FP32 conversion
  - 0x3 (FP32): 32-bit access
- **Done bit**: Toggles read bank ID (SrcS) or updates dvalid (Dest)
- **Quasar pattern**: `TTI_SFPLOAD(lreg, instr_mod0, addr_mod, done, dest_reg_addr)`
- **Common usage**: `TTI_SFPLOAD(p_sfpu::LREG0, p_sfpu::sfpmem::DEFAULT, ADDR_MOD_7, 0, 0)`
**Source: Page 1170505767, SFPLOAD section**

### SFPSTORE (Opcode 0x72)
- **Encoding**: MR
- **Latency**: 2 cycles (SrcS), 3 cycles (Dest)
- **Function**: Stores RG[VD] to Dest or SrcS with format conversion
- **Same InstrMod format table as SFPLOAD but reversed direction**
- **Quasar pattern**: `TTI_SFPSTORE(lreg, instr_mod0, addr_mod, done, dest_reg_addr)`
- **Common usage**: `TTI_SFPSTORE(p_sfpu::LREG1, 0, ADDR_MOD_7, 0, 0)`
**Source: Page 1170505767, SFPSTORE section**

### SFPLOADI (Opcode 0x71)
- **Encoding**: MI
- **Latency**: 1 cycle
- **Function**: Loads 16-bit immediate into RG[VD] with format conversion
- **Key InstrMod values**:
  - 0x0 (FP16_B): fp16b -> FP32
  - 0x1 (FP16_A): fp16a -> FP32
  - 0x2 (UINT16): uint16 -> UINT32
  - 0x4 (INT16): int16 -> INT32 (sign-extended)
  - 0x8 (HI16_ONLY): Write upper 16 bits only, preserve lower
  - 0xA (LO16_ONLY): Write lower 16 bits only, preserve upper
- **Quasar pattern**: `TTI_SFPLOADI(lreg, instr_mod0, imm16)`
- **Common usage**: Loading FP16_B constants, building 32-bit values with HI16_ONLY+LO16_ONLY
**Source: Page 1170505767, SFPLOADI section**

### SFPMAD (Opcode 0x84) - Fused Multiply-Add
- **Encoding**: O4
- **Input/Output**: FP32
- **Latency**: 2 cycles (fully pipelined)
- **Function**: `RG[VD] = (RG[VA] * RG[VB]) + RG[VC]`
- **InstrMod bits**:
  - [3]: Destination override (use RG[RG[7]])
  - [2]: Source A override (use RG[RG[7]])
  - [1]: Negate addend (RG[VC])
  - [0]: Negate multiplier (RG[VA])
- **Subnormal handling**: Flushes subnormal inputs to 0
- **Quasar pattern**: `TTI_SFPMAD(lreg_a, lreg_b, lreg_c, lreg_dest, instr_mod1)`
**Source: Page 1170505767, SFPMAD section**

### SFPMUL (Opcode 0x86) - Multiplication
- **Alias of SFPMAD** with VC expected to be 0.0
- **Quasar pattern**: `TTI_SFPMUL(lreg_a, lreg_b, lreg_c, lreg_dest, instr_mod1)`
- **Common usage**: `TTI_SFPMUL(p_sfpu::LREG0, p_sfpu::LREG2, p_sfpu::LCONST_0, p_sfpu::LREG1, 0)` (LCONST_0 = 0.0)
**Source: Page 1170505767, SFPMUL section**

### SFPADDI (Opcode 0x75) - Add Immediate
- **Encoding**: O1
- **Input/Output**: FP32
- **Latency**: 2 cycles
- **Function**: `RG[VD] = RG[VD] + fp16bToFp32(Imm16)`
- **InstrMod**: [3] dest override, [2] source override via RG[7]
- **Note**: The TTI macro is `TTI_SFPADDI(imm16, lreg_dest, instr_mod1)`, but in ckernel_ops.h, there is also `TTI_SFPADD(lreg_a, lreg_b, lreg_c, lreg_dest, instr_mod1)` which maps to the same opcode with register sources
**Source: Page 1170505767, SFPADDI section**

### SFPADD (same opcode as SFPMAD with VC = addend)
- **Note**: TTI_SFPADD is actually SFPMAD with SrcA=1.0: `(1.0 * RG[VB]) + RG[VC]` = `RG[VB] + RG[VC]`
- **Quasar pattern**: `TTI_SFPADD(lreg_a, lreg_b, lreg_c, lreg_dest, instr_mod1)` where lreg_a = LCONST_1

### SFPNONLINEAR / SFPARECIP (Opcode 0x99)
- **IMPORTANT**: In Quasar's assembly.yaml, this instruction is named **SFPARECIP** (opcode 0x99), but the TTI macro is `TTI_SFPNONLINEAR`
- **Encoding**: O2
- **Input/Output**: FP32
- **Latency**: 1 cycle
- **IPC**: 1
- **Function modes** (via InstrMod):
  - `0x0` (RECIP_MODE): Approximate reciprocal of RG[VC]
  - `0x2` (RELU_MODE): ReLU (max(0, x))
  - `0x3` (SQRT_MODE): Approximate square root
  - `0x4` (EXP_MODE): Approximate e^x
  - `0x5` (TANH_MODE): Approximate tanh(x)
- **All approximations** use hardware LUT-based estimators (mantissa-indexed LUTs, ~7-bit mantissa precision for reciprocal)
- **Special cases**: 1/0 -> infinity, 1/inf -> 0, 1/NaN -> NaN, ReLU(NaN) unchanged
- **Subnormal inputs** flushed to signed zero
- **Quasar pattern**: `TTI_SFPNONLINEAR(lreg_c, lreg_dest, instr_mod1)`
- **p_sfpnonlinear modes**: RECIP_MODE=0, RELU_MODE=2, SQRT_MODE=3, EXP_MODE=4, TANH_MODE=5
**Source: Page 1170505767, SFPNONLINEAR section; ckernel_instr_params.h**

### SFPLUTFP32 (Opcode 0x95) - Quasar-specific LUT
- **Quasar-only instruction** (not in Blackhole)
- **Encoding**: SFPU_LUTFP32 argument format
- **Latency**: 2 cycles
- **Function**: Table look-up with FP32 precision, piecewise linear interpolation
- **Reads from LREG3** as input, uses constant registers (RL view) for LUT coefficients
- **InstrMod**:
  - 0x0: Force LREG3 sign to zero on input to MAD
  - 0x4: Use LREG3 sign as sign of result
- **Key for GELU**: Used with `_sfpu_load_config32_` to load piecewise coefficients
- **Quasar pattern**: `TTI_SFPLUTFP32(lreg_dest, instr_mod1)`
**Source: tt_llk_quasar/instructions/assembly.yaml line 6351**

### SFPLUT (Opcode 0x73)
- **Encoding**: MI
- **Latency**: 2 cycles
- **Function**: LUT-based piecewise linear interpolation: `RG[VD] = A * abs(RG[3]) + B`
- **LUT indexed by abs(LREG3)** value magnitude:
  - X < 1.0: RL[0]
  - 1.0 <= X < 2.0: RL[1]
  - X >= 2.0: RL[2]
- **Coefficients A and B** extracted from 8-bit packed values in constant registers
- **InstrMod[2]**: 0=keep result sign, 1=override with LREG3 sign
- **Used with `_sfpu_load_config32_`** to set up LUT coefficients
**Source: Page 1170505767, SFPLUT section**

### SFPMOV (Opcode 0x7C)
- **Encoding**: O2
- **Latency**: 1 cycle
- **Function**: Copy register with optional transformations
- **InstrMod modes**:
  - 0: Copy RG[VC] -> RG[VD]
  - 1: Copy with sign inversion (negate SMAG)
  - 2: Unconditional copy (ignores LaneEnable)
  - 8: Copy from RS view (special registers)
- **Key for sigmoid**: Mode 1 used to negate input (`TTI_SFPMOV(src_reg, work_reg, 1)`)
**Source: Page 1170505767, SFPMOV section**

### SFPSETCC (Opcode 0x7B)
- **Encoding**: O2
- **Latency**: 1 cycle
- **Function**: Sets CC.Res based on value or immediate
- **InstrMod modes**:
  - 0: Set if RG[VC] is negative
  - 1: Set from Imm12[0]
  - 2: Set if RG[VC] is not zero
  - 4: Set if RG[VC] is positive
  - 6: Set if RG[VC] is zero
  - 8: Invert current CC.Res
- **Imm12[11]**: 0=treat as INT32, 1=treat as FP32/SMAG32
**Source: Page 1170505767, SFPSETCC section**

### SFPIADD (Opcode 0x79)
- **Encoding**: O2
- **Input/Output**: INT32
- **Latency**: 1 cycle
- **Function**: Integer add/subtract with CC update
- **InstrMod**:
  - [1:0]=0: Reg+Reg, [1:0]=1: Reg+Imm, [1:0]=2: Reg-Reg
  - [2]: CC update control (0=update, 1=don't)
  - [3]: CC inversion
**Source: Page 1170505767, SFPIADD section**

### SFPABS (Opcode 0x7D)
- **Encoding**: O2
- **Latency**: 1 cycle
- **Function**: Absolute value of RG[VC] -> RG[VD]
- **InstrMod[0]**: 0=INT32 mode, 1=FP32 mode
- **FP32 mode**: Clears sign bit, preserves NaN, clears Inf sign
- **INT32 mode**: 2's complement abs, saturates INT32::MAX_NEG to INT32::MAX_POS
**Source: Page 1170505767, SFPABS section**

### SFPCONFIG (Opcode 0x91)
- **Encoding**: O1
- **Latency**: 1 cycle (IPC 0.5 - no instructions on following cycle)
- **Function**: Write to configuration/constant registers
- **Only executes on Row 0** of each SFPU slice
- **Used via helper**: `_sfpu_load_config32_(dest, upper16, lower16)` which uses SFPLOADI to LREG0 then SFPCONFIG to copy
**Source: Page 1170505767, SFPCONFIG section; cmath_common.h**

### Condition Code Instructions
- **SFPENCC** (0x8A): Sets CC.En and CC.Res directly
- **SFPCOMPC** (0x8B): Complement/clear CC.Res (for else-branch in if/else)
- **SFPPUSHC** (0x87): Push CC state onto stack (for nested conditionals)
- **SFPPOPC** (0x88): Pop/peek CC stack
- All execute regardless of LaneEnabled state
- CC Stack depth: implementation-defined but must be at least 2 deep
**Source: Page 1170505767, CC instruction sections**

### SFPSWAP (Opcode 0x92)
- **Encoding**: O2
- **Latency**: 2 cycles (IPC 0.5)
- **Function**: Exchange RG[VC] and RG[VD] conditionally or unconditionally
- **Used in sorting algorithms**; not typically used in activation kernels
**Source: Page 1170505767, SFPSWAP section**

---

## 4. Data Format Support and Conversion Rules

### Quasar Supported Data Formats (from QUASAR_DATA_FORMAT_ENUM_VALUES)
| Format | Enum Value | L1 Container | Src Reg Format | Dest Format |
|--------|-----------|-------------|---------------|-------------|
| Float32 | 0 | 32-bit | Not in SrcA/B | FP32 (32-bit mode) |
| Tf32 | 4 | 32-bit | Tf19 (19-bit) | FP32 container |
| Float16 (FP16_A) | 1 | 16-bit | FP16a (19-bit) | FP16a (16-bit) |
| Float16_b (FP16_B) | 5 | 16-bit | FP16b (19-bit) | FP16b (16-bit) |
| MxFp8R | 18 | 8-bit+block exp | Float16_b after unpack | Float16_b |
| MxFp8P | 20 | 8-bit+block exp | Float16_b after unpack | Float16_b |
| Int32 | 8 | 32-bit | Not in SrcA/B | Int32 (32-bit mode) |
| Int8 | 14 | 8-bit | Int8 (19-bit) | Int8 (16-bit) |
| UInt8 | 17 | 8-bit | UInt8 (19-bit) | UInt8 (16-bit) |
| Int16 | 9 | 16-bit | Int16 (19-bit) | Int16 (16-bit) |
| UInt16 | 130 | 16-bit | N/A | UInt16 (16-bit) |

**Source: tests/python_tests/helpers/format_config.py; Page 237174853 (Tensix Formats)**

### SFPU Internal Format
- The SFPU operates internally in **FP32** (32-bit IEEE 754) for floating-point operations
- Integer operations use **SMAG32** (sign-magnitude 32-bit) or **UINT32**/**INT32** (2's complement)
- All data loaded from Dest/SrcS is converted to FP32/SMAG32/UINT32 by the Load EXU
- All data stored back is converted from FP32/SMAG32/UINT32 by the Store EXU
- Format conversions are transparent to the SFPU compute logic
**Source: Page 1256423592, Load EXU Format Converter; Page 1170505767, SFPLOAD/SFPSTORE**

### Unpacker Format Conversions (for SFPU path via Dest/SrcS)
Key conversions relevant to SFPU:
- Float32 -> Float32 (direct to Dest/SrcS)
- Float16/Float16_B/FP8R/FP8P/MXFP8R/MXFP8P -> Float16 or Float16_B (in SrcS/Dest)
- Int32 -> Int32 (direct to Dest/SrcS only)
- Int8 -> Int8, UInt8 -> UInt8, Int16 -> Int16
- MX formats: unpacked to Float16_b for math operations
- All unpacker conversions use mantissa truncation (Round to Zero)
**Source: Page 237174853, Unpacker Format Conversions**

### Packer Format Conversions (from SFPU results)
- Float32 -> virtually any output format
- Float16/Float16_B -> Float16, Float16_B, FP8, MX formats
- Int32 -> Int32, Int8, UInt8
- Two rounding modes: RoundTiesToEven (default) or Stochastic Rounding
**Source: Page 237174853, Packer Format Conversions**

### Subnormal Handling
- **SFPU**: All subnormal inputs are flushed to signed zero
- **SFPU results**: Subnormal outputs are flushed to signed zero
- **Unpacker**: Generally clamps subnormals to 0 (with some exceptions for same-format cases)
- **Packer**: Input subnormals clamped to 0; output subnormals may be kept for MX/FP8P/FP8R formats
**Source: Page 1170505767, Limitations; Page 237174853, Subnormal Clamping**

---

## 5. Format Support Matrix for Activation Kernels

Activation functions (sigmoid, relu, gelu, exp, tanh, celu, hardsigmoid, etc.) are **floating-point operations**. They operate on FP32 data inside the SFPU. The format of data in L1/Dest affects only the load/store conversion, not the math itself.

### Applicable Formats for Activation Kernels

| Format | Applicable? | Constraints | Notes |
|--------|------------|-------------|-------|
| **Float16_b** | YES | Primary format | Most common for SFPU kernels; unpacks to 16-bit in Dest, loads as FP32 in SFPU |
| **Float16** | YES | Fully supported | FP16a format; slightly less common but fully functional |
| **Float32** | YES | Requires 32-bit Dest mode | dest_acc_en needed; higher precision but doubles Dest usage |
| **Tf32** | YES | Treated as Float32 in Dest/SrcS | Unpacked into 32-bit containers in Dest/SrcS; 19-bit in SrcA/B |
| **MxFp8R** | YES | L1 only; unpacked to Float16_b | MX block format; transparent to SFPU math |
| **MxFp8P** | YES | L1 only; unpacked to Float16_b | MX block format; transparent to SFPU math |
| **Int32** | NO | Integer format | Activation functions are floating-point ops; meaningless on integers |
| **Int16** | NO | Integer format | Not applicable to float activations |
| **Int8** | NO | Integer format | Not applicable to float activations |
| **UInt8** | NO | Integer format | Not applicable to float activations |
| **UInt16** | NO | Integer format | Not applicable to float activations |

### Format-Specific Constraints
1. **Float32/Tf32**: Requires `dest_acc_en` (32-bit Dest mode) since these use 32-bit containers
2. **MxFp8R/MxFp8P**: These are L1 storage formats only. The unpacker converts them to Float16_b before they reach Dest/SrcS. The SFPU sees Float16_b data. Packer can convert results back to MX format.
3. **Float16_b**: The default and most tested format. SFPLOAD with IMPLIED mode will use this.
4. **Float16**: Full support but less commonly used in practice.

### Recommended Test Format Priority
1. Float16_b (primary, most common)
2. Float16 (secondary float format)
3. Float32 (with dest_acc)
4. MxFp8R (if MX testing desired)
5. MxFp8P (if MX testing desired)

**Source: Page 237174853 (Tensix Formats); tests/python_tests/helpers/format_config.py; tests/python_tests/helpers/data_format_inference.py**

---

## 6. Pipeline Constraints, Instruction Ordering, and LOADMACRO Rules

### General Instruction Ordering
- **No explicit NOPs needed** between most instructions due to hardware hazard protection on LREGs
- **Exception**: SFPCONFIG requires no instruction on the following cycle
- **Exception**: SFPSWAP requires idle cycle after
- **MAD instructions** (SFPMAD, SFPMUL, SFPADDI, SFPMULI, SFPLUT, SFPLUTFP32) are 2-cycle latency but fully pipelined
**Source: Page 1170505767, Feature Summary; Page 1256423592, Simple EXU**

### LOADMACRO Rules
- LOADMACRO allows up to 5 simultaneous instructions: 1 Load + 1 Simple + 1 MAD + 1 Round + 1 Store
- Each unit in LOADMACRO has a programmable delay (0-7 cycles)
- LOADMACRO Instruction Registers (8 total) hold instruction templates
- LOADMACRO Sequence Registers (4 total) define the dispatch schedule
- Store in LOADMACRO can optionally use the Staging Register
- **No hazard protection** on Staging Register; programmer must manage timing
- **Activations kernels on Quasar typically do NOT use LOADMACRO** - they use sequential TTI instructions instead
**Source: Page 1256423592, LOADMACRO Engine; Page 1170505767, LOADMACRO Sequences**

### Condition Code Rules for Activation Kernels
- CC state: 2 bits per lane (CC.En and CC.Res)
- `LaneEnabled = CC.En ? CC.Res : 1` (when CC.En=0, lane is always enabled)
- SFPSETCC, SFPENCC, SFPPUSHC, SFPPOPC, SFPCOMPC used for conditional execution
- **Typical pattern for if/else**:
  1. `SFPSETCC(condition)` - set CC based on value
  2. Execute "if-true" instructions
  3. `SFPCOMPC` - flip CC for "else" branch
  4. Execute "else" instructions
  5. `SFPENCC` to disable CC
- **Nested conditionals**: Use SFPPUSHC/SFPPOPC to save/restore CC state
- CC manipulation instructions execute regardless of current LaneEnabled
**Source: Page 1170505767, Condition Code sections**

---

## 7. Blackhole vs Quasar Differences

### Key Architectural Differences
| Aspect | Blackhole | Quasar |
|--------|-----------|--------|
| **SFPI support** | Full SFPI C++ intrinsics (vFloat, dst_reg, v_if/v_endif) | **NO SFPI** - uses TTI macros only |
| **SFPU lanes** | 32 lanes (full-sized, 16 columns x 2 rows OR half-sized) | 32 lanes (half-sized, 8 slices x 4 rows) |
| **SFP_ROWS** | 8 (or architecture-dependent) | 2 |
| **SFPNONLINEAR naming** | SFPNONLINEAR (or SFPARECIP) | SFPARECIP in assembly.yaml, TTI_SFPNONLINEAR in C++ |
| **SFPLUTFP32** | Not available | Available (opcode 0x95) - FP32-precision LUT |
| **SFPLE/SFPGT** | Available on Blackhole | Available on Quasar (opcodes 0x96/0x97) |
| **Activations pattern** | Uses SFPI: `sfpi::vFloat v = sfpi::dst_reg[0]; v_if(v < 0.0f) {...}` | Uses TTI: `TTI_SFPLOAD; TTI_SFPNONLINEAR; TTI_SFPSTORE` |
| **Code style** | C++-like with operator overloading | Assembly-like with explicit register management |
| **ActivationType enum** | Present in ckernel_defs.h (Celu, Elu, Gelu, Hardtanh, Hardsigmoid) | **NOT present** in Quasar |
| **Converter helper** | `Converter::as_float(param)` for FP16_B -> vFloat | Use `TTI_SFPLOADI` with FP16_B mode |
| **Loop pattern** | `for(d) { vFloat v = dst_reg[0]; ...; dst_reg[0] = v; dst_reg++; }` | `for(d) { TTI_SFPLOAD; ...; TTI_SFPSTORE; _incr_counters_; }` |

### Porting Strategy
The Blackhole activations kernel uses SFPI intrinsics which are NOT available on Quasar. The Quasar version must be written using TTI macros following these existing Quasar patterns:

1. **Simple activations** (relu, exp, tanh, sqrt, recip): Use SFPNONLINEAR hardware instruction directly
2. **Sigmoid**: Chain of SFPMOV(negate) -> SFPNONLINEAR(EXP) -> SFPADD(+1) -> SFPNONLINEAR(RECIP)
3. **GELU**: Use SFPLUTFP32 with piecewise linear coefficients loaded via _sfpu_load_config32_
4. **SiLU**: sigmoid(x) * x, reusing the sigmoid helper
5. **Complex activations** (Celu, Hardsigmoid, etc.): Must be decomposed into primitive TTI operations

### What Quasar Already Has
These activation-related kernels already exist on Quasar and can serve as building blocks:
- `ckernel_sfpu_relu.h` - ReLU via SFPNONLINEAR RELU_MODE
- `ckernel_sfpu_exp.h` - exp via SFPNONLINEAR EXP_MODE
- `ckernel_sfpu_tanh.h` - tanh via SFPNONLINEAR TANH_MODE
- `ckernel_sfpu_sigmoid.h` - sigmoid chain (negate -> exp -> +1 -> recip)
- `ckernel_sfpu_gelu.h` - GELU via SFPLUTFP32 with coefficients
- `ckernel_sfpu_silu.h` - SiLU using sigmoid helper
- `ckernel_sfpu_recip.h` - reciprocal via SFPNONLINEAR
- `ckernel_sfpu_sqrt.h` - sqrt via SFPNONLINEAR
- `ckernel_sfpu_elu.h` - ELU
- `ckernel_sfpu_lrelu.h` - Leaky ReLU
- `ckernel_sfpu_abs.h` - abs via SFPABS

### What Quasar Does NOT Have
- `ckernel_sfpu_activations.h` - The unified activations kernel with ActivationType dispatch
- `ActivationType` enum in ckernel_defs.h
- SFPI-based implementation patterns (vFloat, v_if, dst_reg, etc.)

---

## 8. Quasar SFPU Kernel Patterns (from existing implementations)

### Standard Kernel Structure
```cpp
// Header includes
#pragma once
#include "ckernel_trisc_common.h"
#include "cmath_common.h"
// Optional: #include "ckernel_ops.h" (for TTI macros not in trisc_common)

namespace ckernel
{
namespace sfpu
{

// Optional init function (for LUT-based ops like GELU)
inline void _init_<op>_() { /* load constants */ }

// Per-row computation function
inline void _calculate_<op>_sfp_rows_()
{
    TTI_SFPLOAD(p_sfpu::LREG0, p_sfpu::sfpmem::DEFAULT, ADDR_MOD_7, 0, 0);
    // ... compute ...
    TTI_SFPSTORE(p_sfpu::LREG<result>, 0, ADDR_MOD_7, 0, 0);
}

// Main iteration loop
inline void _calculate_<op>_(const int iterations)
{
#pragma GCC unroll 8
    for (int d = 0; d < iterations; d++)
    {
        _calculate_<op>_sfp_rows_();
        ckernel::math::_incr_counters_<0x0, 0x0, ckernel::math::SFP_ROWS, 0x0>();
    }
}

} // namespace sfpu
} // namespace ckernel
```

### Key Patterns
1. **ADDR_MOD_7** is always used (set to all zeroes for default addressing)
2. **SFP_ROWS = 2** on Quasar (each iteration processes 2 rows)
3. **_incr_counters_** template increments dest address by SFP_ROWS
4. **Unroll pragma**: `#pragma GCC unroll 8` on the main loop
5. **Template parameter**: `APPROXIMATION_MODE` for ops with approx/precise variants
6. **Namespace**: `ckernel::sfpu`
7. **LREG allocation**: LREG0 typically for input, LREG1 for intermediate/output

---

## 9. Source References Summary

| Page ID | Title | Key Facts Used |
|---------|-------|---------------|
| 1256423592 | Quasar/Trinity SFPU MAS | Execution model, 8 slices x 4 rows, pipeline stages, EXU details, LOADMACRO engine |
| 1170505767 | Tensix SFPU ISA | Per-instruction details, encoding, operands, latencies, CC model |
| 141000706 | srcS registers | SrcS layout, bank switching, synchronization, format selection |
| 195493892 | Dest | Dest block diagram, bank structure, cell counts |
| 80674824 | Dest storage formats | FP16a/b, FP32, Int8/16/32 storage layouts in Dest |
| 83230723 | SrcA/B storage formats | 19-bit containers, supported format list |
| 237174853 | Tensix Formats | Format encodings, unpacker/packer conversions, special number handling, throughput |
| DeepWiki | tt-isa-documentation | Blackhole SFPU overview, instruction list, Blackhole vs Wormhole differences |
| assembly.yaml | Quasar ISA | Instruction existence verification, SFPARECIP naming, SFPLUTFP32 availability |
| Existing Quasar code | tt_llk_quasar/common/inc/sfpu/*.h | TTI macro patterns, SFP_ROWS=2, kernel structure |
