# Architecture Research: Trigonometry Kernel for Quasar SFPU

## 1. SFPU Execution Model

### Overview
The SFPU (SIMD Floating Point Unit) on Quasar is a half-sized configuration with **8 slices** and **4 rows per slice**, giving **32 SFPU lanes** total. Each slice has 1 physical column (half-sized config). The SFPU executes instructions in a SIMD fashion -- all lanes execute the same instruction on different data.
(Source: Confluence page 1256423592, "Introduction" and "Hardware Overview")

### Pipeline Stages
- Instructions flow through stages S2-S5 of the Tensix pipeline
- S2: Instruction decode + LOADMACRO engine
- S3: Most Simple EXU operations, MAD multiplication phase, LUT coefficient unpacking
- S4: MAD addition + normalization, Store to SrcS completes
- S5: Store to Dest completes
(Source: Confluence page 1256423592, "Implementation Details")

### Key Timing
- Most SFPU instructions have 3-cycle latency from issue to result available
- MAD-type instructions (SFPMAD, SFPLUT, SFPLUTFP32): IPC=1, Latency=2 cycles
- SFPNONLINEAR (exp, tanh, recip, sqrt, relu): IPC=1, Latency=1 cycle
- SFPLOAD/SFPLOADI: IPC=1, Latency=1 cycle
- SFPSTORE: IPC=1, Latency=2 (SrcS) or 3 (Dest) cycles
- SFPCONFIG: IPC=0.5, Latency=1 (requires idle cycle after)
- Hardware auto-stalls for SFPMAD read-after-write hazards (no manual SFPNOP needed for MAD pipeline)
(Source: Confluence pages 1256423592 and 1170505767)

### Quasar-specific: SFP_ROWS = 2
On Quasar, SFPU processes **2 rows** per iteration (SFP_ROWS = 2, defined in cmath_common.h). The standard pattern is:
```
for (int d = 0; d < iterations; d++) {
    // load, compute, store for current 2 rows
    _incr_counters_<0x0, 0x0, SFP_ROWS, 0x0>();  // advance dest pointer by 2 rows
}
```
(Source: existing Quasar kernel implementations)

## 2. Register File Layouts

### LREGs (Local Registers) -- per lane
- **8 GPRs** (LREG0-LREG7) per lane, 32 bits each
- Divided into 2 banks: LREGS1 (LREG0-3) and LREGS2 (LREG4-7)
- No LREG-to-LREG data hazards (hardware guarantees)
- 1 additional Staging Register per lane (for LOADMACRO only)
(Source: Confluence page 1170505767, "Local Registers")

### Constant Registers -- per slice (shared across lanes)
- 6 programmable constants (updated via SFPCONFIG):
  - RL[0]: 0x00000000 (0.0) -- visible only to SFPLUT*
  - RL[1]: 0x00000000 (0.0) -- visible only to SFPLUT*
  - RL[2]: 0xBF800000 (-1.0)
  - RL[3]: 0x3B000000 (0.001953125)
  - RL[4]: 0xBF2CC4C7 (-0.6748776)
  - RL[5]: 0xBEB08FF9 (-0.34484843)
- 4 fixed constants:
  - FC[0]: 0x3F56594B (0.8373)
  - FC[1]: 0x00000000 (0.0)
  - FC[2]: 0x3F800000 (1.0)
  - FC[3]: {col[3:0], row[1:0]} (lane ID)
(Source: Confluence page 1170505767, "Constant Registers")

### Register Views
- **RG view**: LREGs 0-7 as indices 0-7; Programmable Constants 2-5 as indices 8-11; Fixed Constants 0-3 as indices 12-15
- **RL view**: All 6 Programmable Constants as indices 0-5; Fixed Constants as indices 6-9 (used by SFPLUT*)
- **RS view**: Special registers including PRNG
(Source: Confluence page 1170505767, "Register Views")

### SrcS Registers
- 2 banks, each with 3 slices of registers
- Each slice: 2 instances of tt_reg_bank, each 4 rows x 16 columns x 16 bits
- Unpacker writes to slices 0-1; SFPU reads from these and writes to slice 2
- Supports 16-bit and 32-bit access modes
- Double-buffered via bank toggling (data_valid synchronization)
(Source: Confluence page 141000706)

### Dest Registers
- Two modes: 16-bit (1024 rows) and 32-bit (512 rows, MSB/LSB split)
- Supported formats: FP16a, FP16b, FP32, Int8 (signed/unsigned), Int16 (signed/unsigned), Int32 (signed)
- Int32 unsigned and Tf19 NOT supported in Dest
(Source: Confluence page 80674824)

## 3. Instructions Required for Trigonometry Kernel

The trigonometry kernel (sine, cosine, and inverse hyperbolic functions) on Blackhole uses Maclaurin series polynomial approximation. The Quasar port needs these instructions:

### 3.1 SFPLOAD (opcode 0x70)
- Loads value from Dest or SrcS into an LREG
- Encoding: MR format
- InstrMod selects format (IMPLIED=0x0, FP16_A=0x1, FP16_B=0x2, FP32=0x3, etc.)
- IPC: 1, Latency: 1 cycle
- Done bit toggles bank ID
- Used to bring tile data into LREGs for computation
(Source: Confluence page 1170505767, "SFPLOAD")

### 3.2 SFPSTORE (opcode 0x72)
- Stores LREG value back to Dest or SrcS
- Encoding: MR format
- IPC: 1, Latency: 2 (SrcS) or 3 (Dest) cycles
- Does NOT perform rounding/saturation -- precede with SFP_STOCH_RND for faithful format conversion
(Source: Confluence page 1170505767, "SFPSTORE")

### 3.3 SFPLOADI (opcode 0x71)
- Loads a 16-bit immediate into an LREG
- InstrMod selects format: FP16_B=0x0, FP16_A=0x1, UINT16=0x2, INT16=0x4, HI16_ONLY=0x8, LO16_ONLY=0xA
- IPC: 1, Latency: 1 cycle
- Used for loading constants (e.g., 1/pi, pi, Maclaurin coefficients)
- To load a full 32-bit constant: use HI16_ONLY (0x8) for upper 16 bits, then LO16_ONLY (0xA) for lower 16 bits
(Source: Confluence page 1170505767, "SFPLOADI")

### 3.4 SFPMAD (opcode 0x84)
- Fused Multiply-Add: Result = (SrcA * SrcB) + SrcC
- Encoding: O4 format (VA, VB, VC, VD fields)
- IPC: 1, Latency: 2 cycles (fully pipelined)
- InstrMod[3]: Dest select (0=VD, 1=RG[RG[7]])
- InstrMod[2]: SrcA select (0=RG[VA], 1=RG[RG[7]])
- InstrMod[1]: Negate SrcC
- InstrMod[0]: Negate SrcA
- Subnormal inputs flushed to 0
- **Critical for polynomial evaluation** (Maclaurin series terms)
- FMA precision: 27 internal bits (may be unfaithful for true FMA; simple ADD/MUL are faithful RTNE)
(Source: Confluence pages 1170505767 and 1256423592, "SFPMAD" and "MAD EXU")

### 3.5 SFPMUL (opcode 0x86)
- Multiplication: Result = SrcA * SrcB (SrcC = 0.0 implicitly)
- Essentially SFPMAD with C = 0
- IPC: 1, Latency: 2 cycles
- Used for x^2, coefficient*power terms
(Source: Confluence page 1170505767)

### 3.6 SFPADD (opcode 0x80)
- Addition: Result = SrcA + SrcC (SrcB = 1.0 implicitly)
- Essentially SFPMAD with B = 1.0
- IPC: 1, Latency: 2 cycles
- Used for accumulating polynomial terms
(Source: Confluence page 1170505767)

### 3.7 SFPMOV (opcode 0x7C)
- Moves register VC to VD
- InstrMod=0: copy, InstrMod=1: copy with sign inversion (negate)
- IPC: 1, Latency: 1 cycle
- Used for negation (sign flip) in trig range reduction
(Source: Confluence page 1170505767, "SFPMOV")

### 3.8 SFPNONLINEAR / SFPARECIP (opcode 0x99)
- Hardware nonlinear function approximation
- InstrMod selects function: RECIP=0, RELU=2, SQRT=3, EXP=4, TANH=5
- IPC: 1, Latency: 1 cycle
- Accuracy: FP16_B precision (7 mantissa bits), max 1-3 ULP error depending on function
- **Relevant for trig**: tanh(mode=5) could be useful; exp(mode=4) for inverse hyperbolic functions
- NOTE: No dedicated sin/cos mode exists -- must use polynomial/Maclaurin approach
(Source: Confluence page 1170505767, "SFPNONLINEAR")

### 3.9 SFPCAST (opcode 0x90)
- Format conversion between FP32, INT32, SMAG32
- InstrMod=0: SMAG32->FP32 (RTNE), InstrMod=4: FP32->SMAG32 (RTNE)
- IPC: 1, Latency: 1 cycle
- **Critical for range reduction**: convert float to integer (to get number of pi radians)
(Source: Confluence page 1170505767, "SFPCAST")

### 3.10 SFPIADD (opcode 0x79)
- Integer addition/subtraction on 2's complement integers
- Can use immediate (12-bit) or register operand
- InstrMod[0]: 0=add, 1=subtract
- InstrMod[1]: 0=use Imm12, 1=use RG[VD]
- Sets CC Result (useful for conditional execution)
- **Used for range reduction**: extracting integer part, checking odd/even
(Source: Confluence page 1170505767, "SFPIADD")

### 3.11 SFPSETCC (opcode 0x7B)
- Sets condition code based on comparisons
- Used for conditional execution (v_if/v_else equivalent)
- InstrMod selects comparison mode
(Source: Confluence page 1170505767, "SFPSETCC")

### 3.12 SFPPUSHC / SFPPOPC / SFPCOMPC / SFPENCC
- Condition code stack manipulation for nested conditional execution
- SFPPUSHC: push current CC onto stack
- SFPPOPC: pop CC from stack
- SFPCOMPC: complement CC result
- SFPENCC: enable CC
- **Required for sign-flip conditional logic** (when integer part is odd, negate result)
(Source: Confluence page 1170505767)

### 3.13 SFPCONFIG (opcode 0x91)
- Writes to SFPU configuration/constant registers
- IPC: 0.5 (requires idle cycle after)
- Used to load LUT coefficients into programmable constant registers
- Source can be LREG[0] (InstrMod[0]=0) or fixed values (InstrMod[0]=1)
- Dest (VD) selects which register: 9-14 for programmable constants, 15 for SFPU Control, 8 for LOADMACRO Control
(Source: Confluence page 1170505767, "SFPCONFIG")

### 3.14 SFPLUT (opcode 0x73) and SFPLUTFP32 (opcode 0x95)
- Piecewise linear LUT-based approximation: Result = A * abs(RG[3]) + C
- SFPLUT: 8-bit packed coefficients in RL[0-2], 3 segments
- SFPLUTFP32: More flexible -- FP32 or FP16 coefficients, 3 or 6 segments
  - InstrMod=0x2: FP16 6-entry mode 1 (segments at 0.5, 1.0, 1.5, 2.0, 3.0)
  - InstrMod=0x6: FP16 6-entry mode 1 with sign preservation
- LUT always indexed by value of LREG3
- IPC: 1, Latency: 2 cycles
- Coefficients loaded via _sfpu_load_config32_ into RL[0-5] (programmable constants 9-14)
- **Could be used for piecewise approximation of sin/cos** as alternative to Maclaurin series
(Source: Confluence pages 1170505767 and 1256423592)

### 3.15 SFPNOP (opcode 0x8F)
- No operation; wastes 1 cycle
- Can optionally set dvalid flags (DestDone, SrcSWrDone, SrcSRdDone)
- Used for manual instruction padding when needed
(Source: Confluence page 1170505767, "SFPNOP")

### 3.16 SFPSETSGN
- Sets sign bit of result based on source register
- Used for sign manipulation in trig (e.g., preserving/restoring sign)
(Source: Confluence page 1170505767)

### 3.17 SFPABS
- Takes absolute value of source register
- Used in range reduction (working with |x| before applying polynomial)
(Source: Confluence page 1170505767)

## 4. Trigonometry-Specific Algorithm Analysis

### 4.1 Sine Implementation (Maclaurin Series)

The Blackhole reference implementation uses this algorithm:
1. **Range reduction**: x' = x / pi (scale to number of half-periods)
2. **Integer extraction**: n = int(x') (which half-period)
3. **Fractional extraction**: f = x' - n (fractional part in [-0.5, 0.5])
4. **Scale back**: f_rad = f * pi (back to radians in [-pi/2, pi/2]-ish range)
5. **Maclaurin series**: sin(f_rad) = f_rad - f_rad^3/3! + f_rad^5/5! - f_rad^7/7! + ...
6. **Sign correction**: if n is odd, negate the result

Maclaurin coefficients for sine:
- x^1: 1.0
- x^3: -1/6 = -0.166666666
- x^5: 1/120 = 0.0083333333
- x^7: -1/5040 = -0.0001984126
- x^9: 1/362880 = 0.0000027557 (non-approx only)
- x^11: -1/39916800 = -0.00000002505 (non-approx only)

### 4.2 Cosine Implementation (Maclaurin Series)

Similar range reduction, then:
- cos(f_rad) = 1 - f_rad^2/2! + f_rad^4/4! - f_rad^6/6! + ...

Maclaurin coefficients for cosine:
- x^0: 1.0
- x^2: -1/2 = -0.5
- x^4: 1/24 = 0.0416666666
- x^6: -1/720 = -0.0013888888
- x^8: 1/40320 = 0.0000248015 (non-approx only)
- x^10: -1/3628800 = -0.0000002755 (non-approx only)

### 4.3 Inverse Hyperbolic Functions

These use log, sqrt, and recip which are already available:
- acosh(x) = log(x + sqrt(x^2 - 1)) [for x >= 1]
- asinh(x) = log(x + sqrt(x^2 + 1))
- atanh(x) = 0.5 * ln((1+x)/(1-x)) [for |x| < 1]

### 4.4 Key Challenge: Float-to-Integer Conversion on Quasar

The Blackhole reference uses `sfpi::float_to_int16()` and `sfpi::int32_to_float()` from the SFPI library. On Quasar, these must be done using SFPCAST:
- FP32->SMAG32: SFPCAST with InstrMod=4 (RTNE rounding)
- SMAG32->FP32: SFPCAST with InstrMod=0

Additionally, SFPIADD can be used for integer arithmetic (masking with AND to check odd/even via bit 0).

### 4.5 Key Challenge: Conditional Execution on Quasar

The Blackhole reference uses `v_if`/`v_endif` (SFPI conditional constructs). On Quasar, this maps to:
- SFPSETCC to set condition code
- SFPPUSHC/SFPPOPC for nesting
- SFPCOMPC to complement (for else branch)
- SFPENCC to enable CC

For the trig kernel, conditional execution is needed when n is odd (flip sign).

## 5. Data Format Support and Conversion Rules

### 5.1 SFPU Internal Format
The SFPU operates internally in **FP32** (32-bit IEEE 754 floating point). All LREG values are 32 bits. Loads from Dest/SrcS convert to FP32; stores convert back from FP32.
(Source: Confluence pages 1170505767 and 1256423592)

### 5.2 Format Support Matrix for Trigonometry Kernel

Trigonometry functions (sin, cos, acosh, asinh, atanh) are **float-only transcendental operations**. Integer formats do NOT apply.

| Format | Enum Value | Applicable? | Constraints |
|--------|-----------|-------------|-------------|
| Float16 | 1 | YES | Primary test format; unpacked to FP32 for SFPU math |
| Float16_b | 5 | YES | Primary test format; unpacked to FP32 for SFPU math |
| Float32 | 0 | YES | Direct FP32 use; requires dest_acc=Yes for 32-bit Dest mode |
| Tf32 | 4 | YES | Stored as Float32 in Dest/SrcS; 19-bit in SrcAB |
| Int32 | 8 | NO | Integer -- transcendental trig is undefined for integer types |
| Int16 | 9 | NO | Integer |
| Int8 | 14 | NO | Integer |
| UInt8 | 17 | NO | Integer |
| MxFp8R | 18 | YES (with care) | L1 only format; unpacked to Float16_b for math; MX requires implied_math_format |
| MxFp8P | 20 | YES (with care) | L1 only format; unpacked to Float16_b for math; MX requires implied_math_format |

### 5.3 Unpacker Format Conversion Rules (relevant paths)
- Float32 -> Float32 (Dest/SrcS only), Tf32, Float16, Float16_b
- Float16/Float16_b -> Float16, Float16_b
- MxFp8R/MxFp8P -> Float16, Float16_b (not Dest/SrcS for Tf32)
- All unpacker conversions use mantissa truncation (Round to Zero)
- Subnormal inputs are clamped to 0 during unpacking
(Source: Confluence page 237174853, "Unpacker Format Conversions")

### 5.4 Packer Format Conversion Rules (relevant paths)
- Float32 -> Float32, Tf32, Float16, Float16_b, FP8R/P, MX formats
- Float16/Float16_b -> Float16, Float16_b, FP8R/P, MX formats
- Two rounding modes: RoundTiesToEven (default) or Stochastic
(Source: Confluence page 237174853, "Packer Format Conversions")

### 5.5 Dest Storage Formats
- FP16a: {sgn, man[9:0], exp[4:0]} in 16-bit container
- FP16b: {sgn, man[6:0], exp[7:0]} in 16-bit container
- FP32: MSBs {sgn, man[22:16], exp[7:0]} in row i; LSBs {man[15:0]} in row i+N/2
- Hidden bit not stored; subnormals clamped to 0
(Source: Confluence page 80674824)

### 5.6 SrcA/B Storage Formats (19-bit containers)
- FP16a: {sgn, man[9:0], 3'b0, exp[4:0]}
- FP16b: {sgn, man[6:0], 3'b0, exp[7:0]}
- Tf19: {sgn, man[9:0], exp[7:0]}
(Source: Confluence page 83230723)

## 6. Pipeline Constraints and Instruction Ordering

### 6.1 General Rules
- SFPMAD/SFPLUT/SFPLUTFP32 are fully pipelined at 2-cycle latency -- no bubbles needed between consecutive MAD ops (unless data dependency)
- Hardware auto-stalls for MAD read-after-write hazards on Quasar (no manual NOP required)
- SFPCONFIG requires 1 idle cycle after it (IPC=0.5)
- SFPSWAP requires 1 idle cycle after it
- No other explicit instruction scheduling requirements
(Source: Confluence pages 1170505767 and 1256423592)

### 6.2 LOADMACRO Rules
- LOADMACRO allows up to 4 additional instructions per cycle (5 total including the load)
- LOADMACRO Instruction Registers (4 templates) loaded via SFPCONFIG with backdoor loading
- Sequence Registers define timing/scheduling
- For simple trig kernels, LOADMACRO is likely not needed (sequential instructions suffice)
(Source: Confluence page 1256423592, "LOADMACRO Engine")

### 6.3 Load-Use Latency
- SFPLOAD: 1-cycle latency (result available next cycle)
- SFPMAD result available after 2 cycles
- SFPSTORE to Dest: 3-cycle latency
(Source: Confluence pages 1256423592 and 1170505767)

## 7. Blackhole vs. Quasar Differences

### 7.1 Programming Model
- **Blackhole**: Uses **SFPI** (SFPU Programming Interface) -- C++ abstraction layer with `vFloat`, `vInt`, `dst_reg[]`, `sfpi::abs()`, `sfpi::setsgn()`, `v_if`/`v_endif`, etc. SFPI compiles to SFPU instructions.
- **Quasar**: Uses **TTI macros** directly -- `TTI_SFPLOAD()`, `TTI_SFPMAD()`, `TTI_SFPSTORE()`, etc. No SFPI abstraction layer available.
- Key mapping: Blackhole `sfpi::float_to_int16()` -> Quasar SFPCAST; `sfpi::int32_to_float()` -> Quasar SFPCAST; `v_if(x != 0)` -> SFPSETCC/SFPPUSHC/SFPCOMPC/SFPPOPC.
(Source: existing Quasar implementations; DeepWiki comparison)

### 7.2 Instruction Set Differences
- Blackhole has `SFPARECIP` with the same modes; Quasar calls it `SFPNONLINEAR` (same opcode 0x99)
- Blackhole `SFPMAD` has negation modifiers; Quasar SFPMAD also has InstrMod[0] (negate A) and InstrMod[1] (negate C)
- Both support SFPLUT and SFPLUTFP32
- Blackhole auto-stalls for MAD RAW hazards; Quasar also auto-stalls (no NOP needed)
- Blackhole produces canonical NaNs (0x7fc00000); Quasar behavior to be verified
(Source: DeepWiki tenstorrent/tt-isa-documentation; Confluence pages)

### 7.3 Constant Availability
- Quasar p_sfpu::LCONST_0 = 0.0 (Fixed Constant 1, index 13 in RG view)
- Quasar p_sfpu::LCONST_1 = 1.0 (Fixed Constant 2, index 14 in RG view)
- Additional constants for Maclaurin coefficients must be loaded via SFPLOADI

### 7.4 Key Porting Considerations
1. Replace all SFPI constructs with TTI macros
2. Replace `dst_reg[0]` reads with `TTI_SFPLOAD` and writes with `TTI_SFPSTORE`
3. Replace `dst_reg++` with `_incr_counters_<0x0, 0x0, SFP_ROWS, 0x0>()`
4. Replace `vFloat` arithmetic (*, +, -) with `TTI_SFPMUL`, `TTI_SFPADD`, `TTI_SFPMAD`
5. Replace `float_to_int16` / `int32_to_float` with SFPCAST sequences
6. Replace `v_if`/`v_endif` with SFPSETCC/SFPPUSHC/SFPCOMPC/SFPPOPC/SFPENCC
7. Replace integer `&` (AND) with SFPAND or SFPIADD-based bit manipulation
8. Loading FP constants: use TTI_SFPLOADI with FP16_B encoding (InstrMod=0) or split 32-bit load

## 8. Quasar Instruction Verification

All instructions needed for the trigonometry kernel have been verified to exist in `tt_llk_quasar/instructions/assembly.yaml`:

| Instruction | Opcode | Present in assembly.yaml? |
|-------------|--------|--------------------------|
| SFPLOAD | 0x70 | YES (line 5558) |
| SFPLOADI | 0x71 | YES (line 5627) |
| SFPSTORE | 0x72 | YES (line 5658) |
| SFPLUT | 0x73 | YES (line 5668) |
| SFPLUTFP32 | 0x95 | YES (line 6351) |
| SFPMAD | 0x84 | YES (line 5907) |
| SFPMUL | 0x86 | YES (line 5967) |
| SFPADD | 0x80 | YES (line 5957) |
| SFPMOV | 0x7C | YES (line 5827) |
| SFPSETCC | 0x7B | YES (line 5817) |
| SFPPUSHC | 0x7B | YES (line 5977) |
| SFPPOPC | 0x7B | YES (line 5987) |
| SFPCOMPC | 0x7B | YES (line 6017) |
| SFPENCC | 0x7B | YES (line 6007) |
| SFPCONFIG | 0x91 | YES (line 6191) |
| SFPNOP | 0x8F | YES (line 6122) |
| SFPSETSGN | 0x7D | YES (line 5997) |
| SFPABS | 0x7E | YES (line 5837) |
| SFPNONLINEAR/SFPARECIP | 0x99 | YES (line 6409, as SFPARECIP) |
| SFPCAST | 0x90 | YES (line 6150) |
| SFPIADD | 0x79 | YES (line 5798) |
| SFPMULI | - | YES (line 5700) |
| SFPADDI | - | YES (line 5731) |

## 9. Existing Quasar Patterns for Reference

### 9.1 Simple SFPNONLINEAR Pattern (tanh, exp, recip, sqrt)
```cpp
TTI_SFPLOAD(p_sfpu::LREG0, p_sfpu::sfpmem::DEFAULT, ADDR_MOD_7, 0, 0);
TTI_SFPNONLINEAR(p_sfpu::LREG0, p_sfpu::LREG1, p_sfpnonlinear::MODE);
TTI_SFPSTORE(p_sfpu::LREG1, 0, ADDR_MOD_7, 0, 0);
```

### 9.2 LUT-Based Pattern (gelu)
```cpp
// Init: load LUT coefficients via _sfpu_load_config32_
_sfpu_load_config32_(0xB, upper16, lower16);  // RL[2] (programmable const 11)
// ...

// Compute:
TTI_SFPLOAD(p_sfpu::LREG3, p_sfpu::sfpmem::DEFAULT, ADDR_MOD_7, 0, 0);
TTI_SFPLUTFP32(p_sfpu::LREG4, 0x2);  // LUT on LREG3, result in LREG4
TTI_SFPMAD(VA, VB, VC, VD, 0);       // FMA combining LUT result
TTI_SFPSTORE(p_sfpu::LREG5, 0, ADDR_MOD_7, 0, 0);
```

### 9.3 Multi-Step Computation Pattern (sigmoid)
```cpp
TTI_SFPLOAD(p_sfpu::LREG0, p_sfpu::sfpmem::DEFAULT, ADDR_MOD_7, 0, 0);
TTI_SFPMOV(src_reg, work_reg, 1);           // Copy with sign inversion
TTI_SFPNONLINEAR(work_reg, dest_reg, EXP);  // exp(-x)
TTI_SFPADD(LCONST_1, dest_reg, LCONST_1, work_reg, 0); // 1 + exp(-x)
TTI_SFPNONLINEAR(work_reg, dest_reg, RECIP);            // 1/(1+exp(-x))
TTI_SFPSTORE(p_sfpu::LREG0, 0, ADDR_MOD_7, 0, 0);
```

### 9.4 Constant Loading Pattern
```cpp
// Load 32-bit constant into an LREG:
TTI_SFPLOADI(lreg, 0x8, upper16);  // HI16_ONLY: write upper 16 bits
TTI_SFPLOADI(lreg, 0xA, lower16);  // LO16_ONLY: write lower 16 bits

// Load FP16_B immediate:
TTI_SFPLOADI(lreg, 0x0, fp16b_value);  // FP16_B format, converts to FP32

// Load into programmable constant register (via LREG0):
TTI_SFPLOADI(p_sfpu::LREG0, 10, lower16);
TTI_SFPLOADI(p_sfpu::LREG0, 8, upper16);
TTI_SFPCONFIG(0, dest_reg_id, 0);
```

### 9.5 Iteration Loop Pattern
```cpp
inline void _calculate_op_(const int iterations) {
#pragma GCC unroll 8
    for (int d = 0; d < iterations; d++) {
        _calculate_op_sfp_rows_();
        ckernel::math::_incr_counters_<0x0, 0x0, ckernel::math::SFP_ROWS, 0x0>();
    }
}
```

## 10. Recommended Approach for Quasar Trigonometry

### Option A: Maclaurin Series (Matching Blackhole)
Port the Blackhole Maclaurin series approach to Quasar TTI macros:
1. Load input from dest
2. Multiply by 1/pi (constant loaded via SFPLOADI)
3. Convert to integer (SFPCAST FP32->SMAG32)
4. Convert back to float (SFPCAST SMAG32->FP32)
5. Subtract to get fractional part
6. Multiply by pi
7. Evaluate Maclaurin polynomial using SFPMAD chain
8. Conditional sign flip if integer part was odd (SFPSETCC + SFPMOV with sign inversion)
9. Store result

**Pros**: Direct port of proven algorithm; matches Blackhole behavior
**Cons**: Many instructions per element; SFPCAST + SFPIADD sequence for int conversion is complex

### Option B: SFPLUT-Based Piecewise Approximation
Use SFPLUTFP32 for piecewise linear approximation of sin/cos:
1. Range reduce to [0, 2*pi)
2. Use SFPLUTFP32 with precomputed slope/intercept coefficients
3. Multiple SFPLUT passes for higher accuracy

**Pros**: Fewer instructions per element; leverages hardware LUT
**Cons**: Lower accuracy than Maclaurin; complex coefficient precomputation; limited to 3 or 6 segments

### Recommendation
**Option A (Maclaurin series)** is recommended for initial implementation because:
- Direct port from working Blackhole code
- Higher accuracy (especially non-approximation mode)
- More straightforward testing (same algorithm, same expected results)
- SFPNONLINEAR already provides tanh which can accelerate the inverse hyperbolic functions
