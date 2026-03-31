# Architecture Research: exp2 (Exponential Base 2) for Quasar

## 1. SFPU Execution Model

**Source: Confluence page 1256423592 (Quasar/Trinity SFPU Micro-Architecture Spec)**

### Overview
The SFPU (SIMD Floating Point Unit) is a functional unit within the Tensix compute engine, complementing the main FPU. It handles complex functions including reciprocal, sigmoid, and exponential calculations. Despite the "floating-point" name, it also supports integer data types.

### Architecture Dimensions
- **Quasar SFPU**: Half-sized configuration with **8 slices** and **4 rows per slice** = **32 SFPU lanes** total
- **SFP_ROWS = 2** (defined in `cmath_common.h`) -- each SFPLOAD/SFPSTORE operation processes 2 rows
- Each slice is directly connected to a corresponding SrcS Register slice and Dest Register slice in the FPU Gtile
- Only 1 physical processing column per instance (SFPU_COLS=1), though 2 logical columns exist

### Pipeline Stages
- SFPU lives in Stages 2-5 (S2-S5) of the Tensix pipeline
- S2: Instruction decode (Frontend)
- S3: Simple EXU operations, MAD multiply, Load format conversion
- S4: MAD addition/normalization, SWAP completion, Store to SrcS
- S5: Store to Dest

### Execution Units (per slice)
1. **Instruction Frontend** -- decode + LOADMACRO engine
2. **LREG Storage** -- GPRs and constants
3. **Load EXU** -- reads from SrcS/Dest register files
4. **Store EXU** -- writes to SrcS/Dest register files
5. **Simple EXU** -- bitwise ops, shifts, CC management, **Nonlinear Estimator**
6. **MAD EXU** -- FP multiply-add, LUT operations (2 cycles)
7. **Round EXU** -- stochastic rounding, PRNG, secondary bit shifter
8. **Transpose/Global Shift EXU** -- TRANSP, SHFT2 global modes

### Instruction Throughput
- Most instructions: IPC=1 (1 instruction per cycle)
- MAD-class instructions (SFPMAD, SFPMUL, SFPADD, SFPMULI, SFPADDI): 2-cycle latency, fully pipelined (IPC=1)
- SFPNONLINEAR: IPC=1, Latency=1 (single-cycle operation in Simple EXU)
- Fully pipelined except SFPSWAP and global SFPSHFT2 variants

**Source: Confluence page 1256423592, sections "Introduction", "Hardware Overview", "Implementation Details"**

## 2. Register File Layouts

### LREG GPRs (General Purpose Registers)
**Source: Confluence page 1256423592, section "LREG Storage"**

- **8 LREG GPRs per lane**, divided into 2 banks of 4:
  - LREGS1: LREGs 0-3
  - LREGS2: LREGs 4-7
- Each LREG is 32 bits wide
- Each bank has 8 standard read ports, 3 write ports, 1 bulk read port, 1 bulk write port
- **Staging Register**: 1 additional register per lane for LOADMACRO sequences

### Constant Registers
- **6 constant registers per SFPU Slice** (not per lane)
- Constants 2-5 accessible by most instructions via RG view
- Also exposed to LUT coefficient unpacker in MAD EXU via RL view
- Written by SFPCONFIG instruction via Config EXU

### Named Constants (from `ckernel_instr_params.h`)
- `p_sfpu::LREG0` through `p_sfpu::LREG7` -- GPR indices
- `p_sfpu::LCONST_0` -- constant 0.0
- `p_sfpu::LCONST_1` -- constant 1.0
- `p_sfpu::LCONST_neg1` -- constant -1.0

### SrcS Registers
**Source: Confluence page 141000706**

- 2 banks, each with 3 slices of registers
- Each slice: 2 instances of tt_reg_bank, each 4 rows x 16 columns x 16 bits
- Slices 0-1: written by unpacker, read by SFPU
- Slice 2: written by SFPU, read by packer
- Double-buffered with bank-switching synchronization
- Supports 16-bit and 32-bit access modes

### Dest Register
**Source: Confluence pages 195493892, 80674824**

- Two modes: 16-bit (1024 rows) and 32-bit (512 rows, pairs of physical rows)
- Supported storage formats: FP16a, FP16b, FP32, Int8 (signed/unsigned), Int16 (signed/unsigned), Int32 (signed)
- Int32 unsigned is NOT supported in Dest
- In 32-bit mode, MSBs in physical row i, LSBs in physical row i+N/2

## 3. Per-Instruction Details for exp2 Kernel

### Critical Finding: SFPNONLINEAR (Quasar-Only)

**SFPNONLINEAR is a new instruction in Quasar that does NOT exist in Blackhole.**

**Source: Confluence page 1612907006 (ISA child page), SFPU ISA page 1170505767**

| Property | Value |
|----------|-------|
| Opcode | 0x99 |
| Encoding | O2 |
| Input Format | FP32 |
| Output Format | FP32 |
| IPC | 1 |
| Latency | 1 |
| Sets CC Result? | No |
| Sets Exception Flags? | Yes |
| Flushes Subnormals? | Yes |

**InstrMod Function Select:**
- 0x0: Approximate reciprocal
- 0x1: Approximate reciprocal (negative only, else passthrough)
- 0x2: ReLU
- 0x3: Approximate square root
- **0x4: Approximate exp(x) -- e raised to the power of input**
- 0x5: Approximate tanh
- 6-15: Reserved

**Algorithmic Implementation (from ISA spec):**
```
if (LaneEnabled) {
  FP32 Source = RG[VC];
  if (isDenormal(Source)) {
    Source.Man = 0;
  }
  FP32 Result;
  switch (InstrMod) {
    ...
    4: Result = exp(Source); break;  // exp mode
    ...
  }
  if (isNaN(Result)) { Flags.NaN = 1; }
  else if (isDenorm(Result)) { Result.Man = 0; Flags.Denorm = 1; }
  RG[VD] = Result;
}
```

**Implementation**: Located in Simple EXU Nonlinear Estimator. Uses custom LUT with pre/post-conditioning logic. (Source: SFPU MAS page 1256423592, section "Nonlinear Estimator")

**Numerical Accuracy:**
| Function | Max FP16_B ULP Error | Range | Notes |
|----------|---------------------|-------|-------|
| exp | 2 | [-87, Inf) | ULP error spikes below -87 as result should be subnormal, but returns 0 |

**Constants for modes (from `ckernel_instr_params.h`):**
```cpp
struct p_sfpnonlinear {
    constexpr static std::uint32_t RECIP_MODE = 0x0;
    constexpr static std::uint32_t RELU_MODE  = 0x2;
    constexpr static std::uint32_t SQRT_MODE  = 0x3;
    constexpr static std::uint32_t EXP_MODE   = 0x4;
    constexpr static std::uint32_t TANH_MODE  = 0x5;
};
```

### SFPLOAD
**Source: SFPU ISA page 1170505767**

| Property | Value |
|----------|-------|
| Opcode | 0x70 |
| IPC | 1, Latency | 1 |
| Input Formats | FP32, FP16_A, FP16_B, SMAG32, SMAG16, UINT16, SMAG8, UINT8 |
| Output Formats | FP32, SMAG32, UINT32 |

Loads a value from SrcS or Dest register file into an LREG. Format conversion happens automatically based on InstrMod. For Quasar, the DEFAULT mode (implied format) is the standard usage pattern.

**Quasar-specific**: TTI_SFPLOAD takes 5 arguments: `(lreg_dest, mode, addr_mod, done, dest_reg_addr)` -- note the extra `done` parameter compared to Blackhole's 4-argument version.

### SFPSTORE
**Source: SFPU ISA page 1170505767**

| Property | Value |
|----------|-------|
| Opcode | 0x72 |
| IPC | 1 |

Stores a value from an LREG to SrcS or Dest register file. Like SFPLOAD, Quasar's TTI_SFPSTORE takes 5 arguments: `(lreg_src, mode, addr_mod, done, dest_reg_addr)`.

### SFPMUL (multiply)
**Source: SFPU ISA page 1170505767, assembly.yaml**

| Property | Value |
|----------|-------|
| Opcode | 0x86 |
| IPC | 1, Latency | 2 |
| Formats | FP32 -> FP32 |

Alias of SFPMAD with VC=0. Computes `RG[VA] * RG[VB] + 0.0`. 2-cycle latency, fully pipelined.

### SFPMULI (multiply with 16-bit immediate)
**Source: SFPU ISA page 1170505767**

| Property | Value |
|----------|-------|
| Opcode | 0x74 |
| IPC | 1, Latency | 2 |
| Formats | FP32 -> FP32 |

Multiplies `RG[VD]` by an FP16_B immediate and stores back to `RG[VD]`. The immediate is a 16-bit value in FP16_B format. This is useful for multiplying by constants like ln(2).

### SFPMOV
**Source: assembly.yaml**

| Property | Value |
|----------|-------|
| Opcode | 0x7C |
| IPC | 1, Latency | 1 |

Move register with optional sign inversion (instr_mod1 bit 0: 1 = invert sign).

### SFPNOP
**Source: assembly.yaml**

| Property | Value |
|----------|-------|
| Opcode | 0x8F |

No-operation. Used for pipeline timing and synchronization. Has `store_done` and `toggle_state` fields for bank synchronization.

### SFPCONFIG
**Source: assembly.yaml**

| Property | Value |
|----------|-------|
| Opcode | 0x91 |

Programs configuration registers. Used to load constants into LREGs and configure LOADMACRO sequences.

## 4. exp2(x) Algorithm: Conversion from e^x to 2^x

### Blackhole Reference Approach
The Blackhole `_calculate_exp2_()` computes 2^x by converting it to e^(x * ln(2)):
```
2^x = e^(x * ln(2))
```
Where ln(2) = 0.6931471805. It then calls `_calculate_exponential_piecewise_()` which implements e^x via polynomial approximation or LUT-based approaches.

### Quasar Approach
On Quasar, SFPNONLINEAR with EXP_MODE (0x4) computes e^x in a single instruction. Therefore exp2(x) can be computed as:
1. Load x from Dest
2. Multiply x by ln(2) = 0.6931471805
3. Apply SFPNONLINEAR EXP_MODE to compute e^(x*ln(2)) = 2^x
4. Store result back to Dest

This is far simpler than Blackhole's approach because:
- Blackhole lacks SFPNONLINEAR and must implement exp via polynomial approximation, LUTs, or LOADMACRO sequences
- Quasar's SFPNONLINEAR provides hardware-accelerated exp in a single cycle

### Existing Quasar exp Pattern
The existing Quasar `ckernel_sfpu_exp.h` shows the pattern:
```cpp
template <bool APPROXIMATION_MODE>
inline void _calculate_exp_sfp_rows_()
{
    TTI_SFPLOAD(p_sfpu::LREG0, p_sfpu::sfpmem::DEFAULT, ADDR_MOD_7, 0, 0);
    if constexpr (APPROXIMATION_MODE) {
        TTI_SFPNONLINEAR(p_sfpu::LREG0, p_sfpu::LREG1, p_sfpnonlinear::EXP_MODE);
    }
    TTI_SFPSTORE(p_sfpu::LREG1, 0, ADDR_MOD_7, 0, 0);
}
```

For exp2, we need an additional multiply step between SFPLOAD and SFPNONLINEAR.

## 5. Data Format Support and Conversion Rules

### SFPU Internal Format
**Source: SFPU ISA page 1170505767, SFPU MAS page 1256423592**

The SFPU operates internally on **FP32** data. All data loaded from register files is converted to FP32 on load, and all data stored is converted from FP32 on store.

### IEEE 754 Deviations (in SFPU)
- Subnormal inputs are flushed to signed zero
- Subnormal results are flushed to signed zero
- FMA rounding may be unfaithful (though simple ADD and MUL round faithfully per IEEE 754 RNTE)
- SFPNONLINEAR flushes subnormal results to zero

### Format Support Matrix for exp2 (SFPU transcendental operation)

**Source: Confluence page 237174853 (Tensix Formats), page 80674824 (Dest storage formats), tests/python_tests/helpers/format_config.py, tests/python_tests/helpers/data_format_inference.py**

exp2 is a floating-point transcendental operation. The SFPU loads from Dest, processes in FP32, and stores back to Dest. The applicable formats are determined by what can be unpacked into Dest and what SFPU can process.

| Format | Enum Value | Applicable? | Constraints |
|--------|-----------|-------------|-------------|
| **Float16** | 1 | YES | Standard FP format, unpacks to FP32 in SFPU |
| **Float16_b** | 5 | YES | Standard FP format, unpacks to FP32 in SFPU |
| **Float32** | 0 | YES | Native SFPU format, requires dest_acc=Yes (32-bit Dest mode) |
| **Tf32** | 4 | YES (with caveats) | Unpacks to FP32 container in Dest/SrcS; requires dest_acc=Yes |
| **MxFp8R** | 18 | YES | L1 format only; unpacked to Float16_b for math |
| **MxFp8P** | 20 | YES | L1 format only; unpacked to Float16_b for math |
| **Int32** | 8 | NO | exp2 is a transcendental FP operation, meaningless on integers |
| **Int16** | 9 | NO | exp2 is a transcendental FP operation, meaningless on integers |
| **Int8** | 14 | NO | exp2 is a transcendental FP operation, meaningless on integers |
| **UInt8** | 17 | NO | exp2 is a transcendental FP operation, meaningless on integers |

**Key constraints:**
- Float32 and Tf32 require `is_fp32_dest_acc_en = true` (32-bit Dest accumulation mode)
- MX formats (MxFp8R, MxFp8P) are L1-only formats; unpacker converts them to Float16_b before they reach register files
- Integer formats do not apply to transcendental floating-point operations

### Unpacker Format Conversions (relevant to exp2)
**Source: Confluence page 237174853**

| Input Format (L1) | Output Format (Register) |
|-------------------|------------------------|
| Float32 | Float32, Tf32, Float16, Float16_B |
| Tf32 | Tf32, Float16, Float16_B |
| Float16, Float16_B | Float16, Float16_B |
| MxFp8R, MxFp8P | Float16, Float16_B |

### Packer Format Conversions (relevant to exp2)
| Input Format (Register) | Output Format (L1) |
|------------------------|-------------------|
| Float32 | Float32, Tf32, Float16, Float16_B, FP8R, FP8P, MxFp8R, MxFp8P, ... |
| Float16, Float16_B | Float16, Float16_B, FP8R, FP8P, MxFp8R, MxFp8P, ... |

### Valid Quasar Register Formats
**Source: tests/python_tests/helpers/data_format_inference.py**

- Src Register: Float16_b, Float16, Float32, Tf32, Int32, Int8, UInt8, Int16
- Dest Register: Float16_b, Float16, Float32, Int32, Int8, UInt8, Int16

## 6. Pipeline Constraints and Instruction Ordering

### General Rules
**Source: SFPU MAS page 1256423592**

1. **SFPCONFIG aftermath**: No instruction may follow SFPCONFIG on the next cycle (config register state can influence S2 execution)
2. **MAD 2-cycle latency**: SFPMAD/SFPMUL/SFPADD/SFPMULI/SFPADDI take 2 cycles but are fully pipelined. Hardware auto-stalls for read-after-write hazards on dependent data.
3. **SFPNONLINEAR latency**: 1-cycle latency, IPC=1 -- result available on next cycle
4. **SFPLOAD latency**: 1-cycle latency, result available on next cycle
5. **SFPSTORE latency**: 2-3 cycles (SrcS: S4, Dest: S5), but does not block subsequent instructions

### Quasar-Specific Hardware Stalling
- Hardware implicitly stalls if the next instruction depends on results of a multi-cycle instruction
- This is noted explicitly in the sigmoid implementation: "Although SFPMUL/SFPADD are 2 cycle instructions, we don't need a TTI_NOP because the hardware implicitly stalls if the next instruction depends on results"

### Iteration Pattern for SFPU Operations
- Each SFPLOAD/SFPSTORE + SFPU operation iteration processes **SFP_ROWS = 2** rows of Dest
- Counter increments via `_incr_counters_<0x0, 0x0, SFP_ROWS, 0x0>()`
- Total iterations = face_rows / SFP_ROWS (typically 16/2 = 8 per face)

## 7. LOADMACRO Rules

**Source: SFPU MAS page 1256423592, section "LOADMACRO Engine"**

LOADMACRO allows up to 5 instructions to execute in parallel (1 per engine). For simple kernels like exp2, LOADMACRO is not needed -- the basic SFPLOAD + SFPMULI + SFPNONLINEAR + SFPSTORE sequence is sufficient and straightforward.

Key LOADMACRO rules (for reference):
- Load unit has NO LOADMACRO slot -- SFPLOAD always comes from main pipeline
- Programmable instruction registers 0-7, sequence registers select and time them
- Backdoor loading uses illegal VD ranges (0xC-0xF) to program instruction registers
- `TTI_SFPCONFIG(0, N, 0)` writes LREG[0] contents to config destination N

## 8. Blackhole Differences (Porting Considerations)

### Key Differences

| Aspect | Blackhole | Quasar |
|--------|-----------|--------|
| SFPNONLINEAR instruction | **Does NOT exist** | Available (opcode 0x99) |
| exp implementation | Polynomial approximation or LUT-based via SFPLUT + SFPMAD + many instructions | Single SFPNONLINEAR instruction |
| SFPLOAD/SFPSTORE args | 4 arguments | **5 arguments** (extra `done` parameter) |
| Namespace | `ckernel::sfpu` | `ckernel` + `ckernel::sfpu` |
| SFPI library | Uses `sfpi::vFloat`, `sfpi::dst_reg[]` (C++ vector abstraction) | Uses TTI_* intrinsics directly |
| SFP_ROWS | Variable (arch-dependent) | Fixed at 2 |
| Counter increment | `sfpi::dst_reg++` | `_incr_counters_<0x0, 0x0, SFP_ROWS, 0x0>()` |

### Blackhole exp2 Implementation
Blackhole `_calculate_exp2_()`:
1. Load value from `sfpi::dst_reg[0]` (C++ abstraction)
2. Multiply by `0.6931471805f` (ln(2))
3. Call `_calculate_exponential_piecewise_()` which is a complex multi-instruction sequence
4. Store result via `sfpi::dst_reg[0] = exp`

### Quasar exp2 Implementation Strategy
Since Quasar has SFPNONLINEAR with EXP_MODE, exp2 becomes:
1. SFPLOAD from Dest
2. SFPMULI by ln(2) in FP16_B immediate format (or SFPMUL with constant in LREG)
3. SFPNONLINEAR with EXP_MODE to compute e^(x*ln(2)) = 2^x
4. SFPSTORE back to Dest

This is architecturally much simpler than Blackhole.

### Init Function
- Blackhole: `_init_exp2_()` calls `_init_exponential_()` which sets up LOADMACRO sequences, LUT constants, etc.
- Quasar: No special init needed for SFPNONLINEAR (it's hardware-internal). But if using SFPMULI with an FP16_B immediate for the ln(2) multiply, no LREG setup is needed either. If using SFPMUL with a pre-loaded constant, SFPCONFIG can load the constant once in init.

### SfpuType Enum
- Blackhole `SfpuType` includes `exp2`
- **Quasar `SfpuType` does NOT include `exp2`** -- it needs to be added

## 9. Summary of Required Instructions for exp2

| Instruction | Purpose | Assembly.yaml Confirmed |
|-------------|---------|----------------------|
| SFPLOAD | Load value from Dest into LREG | Yes (opcode 0x70) |
| SFPMULI or SFPMUL | Multiply by ln(2) constant | Yes (0x74 / 0x86) |
| SFPNONLINEAR | Compute exp(x*ln(2)) = 2^x | Yes (opcode 0x99 in assembly.yaml at line 6065) |
| SFPSTORE | Store result back to Dest | Yes (opcode 0x72) |
| SFPCONFIG | (init only, if using SFPMUL with LREG constant) | Yes (opcode 0x91) |
| SFPLOADI | (init only, to load constant into LREG[0]) | Yes (opcode 0x71) |

### SFPNONLINEAR / SFPARECIP in assembly.yaml

**Important**: In `assembly.yaml`, the instruction is named **SFPARECIP** (opcode 0x99). In `ckernel_ops.h`, it is defined as **SFPNONLINEAR** (same opcode 0x99). Both names refer to the same hardware instruction. The TTI macro is `TTI_SFPNONLINEAR(lreg_c, lreg_dest, instr_mod1)`.

```yaml
SFPARECIP:
    op_binary: 0x99
    ex_resource: SFPU
    ttsync_resource: OTHERS
    instrn_type: SFPU
```

From `ckernel_ops.h`:
```cpp
#define TT_OP_SFPNONLINEAR(lreg_c, lreg_dest, instr_mod1) TT_OP(0x99, (((lreg_c) << 8) + ((lreg_dest) << 4) + ((instr_mod1) << 0)))
#define TTI_SFPNONLINEAR(lreg_c, lreg_dest, instr_mod1) INSTRUCTION_WORD(TRISC_OP_SWIZZLE(TT_OP_SFPNONLINEAR(lreg_c, lreg_dest, instr_mod1)))
```

**Source: assembly.yaml cross-check**
```
SFPLOAD:      opcode 0x70  -- confirmed
SFPLOADI:     opcode 0x71  -- confirmed
SFPSTORE:     opcode 0x72  -- confirmed
SFPMULI:      opcode 0x74  -- confirmed
SFPMAD:       opcode 0x84  -- confirmed
SFPADD:       opcode 0x85  -- confirmed
SFPMUL:       opcode 0x86  -- confirmed
SFPNOP:       opcode 0x8F  -- confirmed
SFPCONFIG:    opcode 0x91  -- confirmed
SFPLOADMACRO: opcode 0x93  -- confirmed
SFPARECIP/SFPNONLINEAR: opcode 0x99  -- confirmed (SFPARECIP in yaml, SFPNONLINEAR in ckernel_ops.h)
```

All required instructions exist in Quasar's assembly.yaml.
