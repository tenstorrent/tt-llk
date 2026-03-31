# ELU Architecture Research Brief (Quasar SFPU)

## 1. SFPU Execution Model

**Source: Quasar/Trinity SFPU Micro-Architecture Spec (page 1256423592)**

### Lane/Slice/Row Organization
- Quasar SFPU is a **half-sized** configuration: **8 slices**, **4 rows per slice** = **32 SFPU lanes** total
- Each slice has 1 physical processing column (2 logical columns, odd column tied off)
- Each slice is directly connected to a corresponding SrcS Register slice and Dest Register slice in the FPU Gtile
- All lanes execute the **same instruction** (SIMD model) on their own local data
- Each slice has a single instruction decode instance shared among its 4 rows
- **SFP_ROWS = 2**: Quasar processes 2 rows per SFPU iteration (not 4 like some architectures)

### Pipeline Stages
- SFPU occupies Stages S2-S5 of the Tensix pipeline
- S2: Instruction Frontend (decode, LOADMACRO engine)
- S3: Most Simple EXU operations complete here; MAD stage 1 (multiply + align)
- S4: MAD stage 2 (add + normalize + round); SWAP spills to here; Store to SrcS completes
- S5: Store to Dest completes

### Execution Units
1. **Load EXU** (back-end only) - loads from Dest/SrcS, format conversion, immediate loading
2. **Store EXU** - stores to Dest/SrcS, format conversion
3. **Simple EXU** - integer ops, bitwise ops, comparisons, ABS, MOV, SETCC, COMPC, ENCC, NONLINEAR/ARECIP estimator, SWAP
4. **MAD EXU** - FP multiply-add, LUT operations (SFPMAD, SFPADD, SFPMUL, SFPLUT, SFPADDI, SFPMULI)
5. **Round EXU** - stochastic rounding, secondary bit shifter
6. **Transpose/Global Shift EXU** - TRANSP, SHFT2

### Key Execution Constraints
- **MAD instructions take 2 cycles** (IPC=1, latency=2). The hardware implicitly stalls if the next instruction depends on MAD results (Quasar auto-stall, unlike Blackhole where manual NOPs were sometimes needed)
- **SFPSTORE latency**: 2 cycles (SrcS), 3 cycles (Dest)
- **Simple EXU operations**: 1 cycle latency (except SWAP = 2 cycles)
- **SFPNONLINEAR/SFPARECIP**: 1 cycle latency, 1 IPC - executes on the Simple EXU Nonlinear Estimator
- All instructions fully pipelined (except SFPSWAP, SFPSHFT2 global variants)
- **SFPCONFIG** requires no instructions on the following cycle

## 2. Register File Layouts

### LREG GPRs (per lane)
**Source: SFPU MAS, LREG Storage section (page 1256423592)**

- 8 LREG GPRs per lane, divided into 2 banks:
  - **LREGS1**: LREG0-LREG3 (bank 0)
  - **LREGS2**: LREG4-LREG7 (bank 1)
- Each LREG is **32 bits** wide (can hold FP32, INT32, UINT32, SMAG32)
- 8 standard read ports, 3 standard write ports, 1 bulk read, 1 bulk write per bank
- Additionally, 1 **Staging Register** per lane for LOADMACRO sequences

### Constant Registers (per slice, not per lane)
- 6 constant registers per slice (shared across lanes in a slice)
- Accessible via RG view: Constants 2-5
- Used by LUT coefficient unpacking in MAD EXU
- **Software constants in ckernel_instr_params.h**:
  - `p_sfpu::LCONST_0 = 9` (represents 0.0)
  - `p_sfpu::LCONST_1 = 10` (represents 1.0)
  - `p_sfpu::LCONST_neg1 = 11` (represents -1.0)

### SrcS Registers
**Source: srcS registers (page 141000706)**

- 2 banks, each with 3 slices of registers
- Each slice: 2 instances of tt_reg_bank, each 4 rows x 16 columns x 16 bits
- Slices 0-1: written by unpacker; SFPU loads from here
- Slice 2: written by SFPU; packer reads from here
- **Double-buffered**: bank toggling via Done flags on SFPLOAD/SFPSTORE
- **Synchronization**: data_valid, pack_ready, sfpu_wr_ready bits control flow

### Dest Register
**Source: Dest (page 195493892)**

- **16-bit mode**: 1024 rows
- **32-bit mode**: 512 rows (pairs of physical rows combined)
- 16 columns per dest instance
- Double-banked with bank mux

### Dest Storage Formats
**Source: Dest storage formats (page 80674824)**

| Format | Storage Layout | Notes |
|--------|---------------|-------|
| FP16a | `{sgn, man[9:0], exp[4:0]}` in 16-bit row | Hidden bit not stored; subnormals clamped to 0 |
| FP16b | `{sgn, man[6:0], exp[7:0]}` in 16-bit row | Hidden bit not stored; subnormals clamped to 0 |
| FP32 | MSBs in row i: `{sgn, man[22:16], exp[7:0]}`; LSBs in row i+N/2: `{man[15:0]}` | 32-bit mode required |
| Int8 (signed) | `{sgn, 3'b0, mag[6:0], 5'd16}` in 16-bit row | Sign+magnitude; zero has lower 5 bits = 0 |
| Int8 (unsigned) | `{3'b0, val[7:0], 5'd16}` in 16-bit row | Zero has lower 5 bits = 0 |
| Int16 (signed) | `{sgn, mag[14:0]}` in 16-bit row | Sign+magnitude |
| Int16 (unsigned) | `{val[15:0]}` in 16-bit row | Direct storage |
| Int32 (signed) | MSBs: `{sgn, mag[22:16], mag[30:23]}`; LSBs: `{mag[15:0]}` | 32-bit mode; sign+magnitude |

**Unsupported in Dest**: Int32 unsigned, Tf19, BFP formats, LF8

### SrcA/B Storage Formats
**Source: SrcA/B storage formats (page 83230723)**

- 19-bit containers per datum, 2 banks of 64 rows x 16 columns
- Supported: FP16a, FP16b, Tf19/Tf32, Int8 signed/unsigned, Int8_2X, UInt8_2X, Int16 signed, MXFP4_2x_A/B
- **Unsupported**: Int32, FP32, Int16 unsigned, BFP, LF8

## 3. Instructions Required for ELU

ELU(x) = x if x >= 0, alpha * (exp(x) - 1) if x < 0

### SFPLOAD (Opcode 0x70)
**Source: SFPU ISA (page 1170505767), SFPLOAD section**

- Encoding: MR
- IPC: 1, Latency: 1
- Loads from Dest/SrcS into RG[VD]
- InstrMod selects format (IMPLIED=0, FP16_A=1, FP16_B=2, FP32=3, etc.)
- IMPLIED mode uses getImpliedFormat() which reads from register file state
- `Done` flag toggles read bank ID
- **Quasar usage**: `TTI_SFPLOAD(lreg, format, addr_mod, addr, done)`

### SFPLOADI (Opcode 0x71)
**Source: SFPU ISA, SFPLOADI section**

- Encoding: MI
- IPC: 1, Latency: 1
- Loads immediate 16-bit value into RG[VD]
- InstrMod: 0=FP16_B, 1=FP16_A, 2=UINT16, 4=INT16, 8=HI16_ONLY, 0xA=LO16_ONLY
- **For ELU**: Load slope/alpha parameter as BF16 immediate
- **Quasar usage**: `TTI_SFPLOADI(lreg, instrmod, imm16)`

### SFPSTORE (Opcode 0x72)
**Source: SFPU ISA, SFPSTORE section**

- Encoding: MR
- IPC: 1, Latency: 2 (SrcS) / 3 (Dest)
- Stores RG[VD] to Dest/SrcS with format conversion
- Does NOT perform rounding/saturation for 8/16-bit outputs
- `Done` flag toggles write bank ID
- **Quasar usage**: `TTI_SFPSTORE(lreg, done, addr_mod, addr, 0)`

### SFPSETCC (Opcode 0x7B)
**Source: SFPU ISA, SFPSETCC section**

- Encoding: O2
- IPC: 1, Latency: 1
- Sets CC.Res based on value in RG[VC]
- InstrMod modes:
  - 0: Set if RG[VC] is negative (sign bit check)
  - 1: Set based on Imm12[0]
  - 2: Set if RG[VC] is not 0
  - 4: Set if RG[VC] is positive
  - 6: Set if RG[VC] is 0
  - 8: Invert CC.Res
- Imm12[11]: 0=treat as INT32, 1=treat as FP32/SMAG32
- **For ELU**: Use mode 0 to check if value is negative
- **Quasar usage**: `TTI_SFPSETCC(imm12, lreg_vc, instrmod)`

### SFPENCC (Opcode 0x8A)
**Source: SFPU ISA, SFPENCC section**

- Encoding: O2
- IPC: 1, Latency: 1
- Directly sets CC.En and CC.Res
- Executes on ALL lanes regardless of LaneEnabled
- InstrMod[1:0]: 0=keep CC.En, 1=invert CC.En, 2=set CC.En from Imm12[0]
- InstrMod[3]: 0=set CC.Res to 1, 1=set CC.Res from Imm12[1]
- **For ELU**:
  - Enable CC: `TTI_SFPENCC(1, 2)` (sets CC.En=1 from Imm12[0]=1)
  - Clear/reset CC.Res: `TTI_SFPENCC(0, 0)` (keeps CC.En, sets CC.Res=1)
  - Disable CC: `TTI_SFPENCC(0, 2)` (sets CC.En=0 from Imm12[0]=0)
- **Quasar usage**: `TTI_SFPENCC(imm12, instrmod)`

### SFPCOMPC (Opcode 0x8B)
**Source: SFPU ISA, SFPCOMPC section**

- Encoding: O2
- IPC: 1, Latency: 1
- Conditionally complements or clears CC.Res
- Executes on ALL lanes regardless of LaneEnabled
- If CC.En and stack empty: CC.Res = !CC.Res
- If CC.En and stack[0].En and stack[0].Res: CC.Res = !CC.Res
- Otherwise: CC.Res = 0
- **For ELU**: Can be used to implement else-branch (complement CC after if-block)
- **Quasar usage**: `TTI_SFPCOMPC(imm12, instrmod)`

### SFPNONLINEAR/SFPARECIP (Opcode 0x99)
**Source: SFPU ISA, SFPNONLINEAR section**

- Named SFPARECIP in Quasar assembly.yaml, but TTI macro is `TTI_SFPNONLINEAR`
- Encoding: O2
- IPC: 1, Latency: 1
- Performs non-linear function approximation on RG[VC]
- InstrMod modes:
  - 0: RECIP_MODE - approximate reciprocal
  - 2: RELU_MODE - ReLU function
  - 3: SQRT_MODE - approximate square root
  - **4: EXP_MODE** - approximate e^x (base-e exponential)
  - 5: TANH_MODE - approximate hyperbolic tangent
- Flushes subnormal inputs to 0 of same sign
- **For ELU**: Use EXP_MODE to compute exp(x) for negative inputs
- **Quasar usage**: `TTI_SFPNONLINEAR(src_lreg, dst_lreg, instrmod)`
- **Constants**: `p_sfpnonlinear::EXP_MODE = 0x4`, `p_sfpnonlinear::RECIP_MODE = 0x0`

### SFPMAD (Opcode 0x84)
**Source: SFPU ISA, SFPMAD section**

- Encoding: O4 (VA, VB, VC, VD)
- IPC: 1, Latency: 2
- Computes (SrcA * SrcB) + SrcC where SrcA=RG[VA], SrcB=RG[VB], SrcC=RG[VC]
- Result written to RG[VD] (or RG[RG[7]] if InstrMod[3]=1)
- InstrMod[0]: negate SrcA; InstrMod[1]: negate SrcC; InstrMod[2]: use RG[7] for SrcA; InstrMod[3]: use RG[7] for dest
- Flushes subnormal inputs to 0; flushes subnormal results to 0
- **For ELU**: Multiply exp(x)-1 by alpha
- **Quasar usage**: `TTI_SFPMAD(va, vb, vc, vd, instrmod)`

### SFPADD (Opcode 0x85)
**Source: SFPU ISA, SFPADD section**

- Alias of SFPMAD with intent that VA or VB should be 1.0
- Same encoding, latency, behavior as SFPMAD
- **For ELU**: Compute exp(x) + (-1) = exp(x) - 1
- **Quasar usage**: `TTI_SFPADD(va, vb, vc, vd, instrmod)`

### SFPMUL (Opcode 0x86)
**Source: SFPU ISA, SFPMUL section**

- Alias of SFPMAD with intent that VC should be 0.0
- Same encoding, latency, behavior as SFPMAD
- **For ELU**: Multiply result by alpha
- **Quasar usage**: `TTI_SFPMUL(va, vb, vc, vd, instrmod)`

### SFPADDI (Opcode 0x75)
**Source: SFPU ISA, SFPADDI section**

- Encoding: O1
- IPC: 1, Latency: 2
- Adds RG[VD] + FP16_B(Imm16), stores back to RG[VD]
- InstrMod[2]: source from RG[RG[7]]; InstrMod[3]: dest to RG[RG[7]]
- **Quasar usage**: `TTI_SFPADDI(imm16, vd, instrmod)`

### SFPMOV
- Copies a register value; InstrMod[0]=1 inverts sign bit
- **For ELU**: Can negate input for exp(-x) computation
- **Quasar usage**: `TTI_SFPMOV(src_lreg, dst_lreg, instrmod)`

### SFPNOP (Opcode 0x8F)
**Source: SFPU ISA, SFPNOP section**

- Encoding: N
- IPC: 1, Latency: 1
- No-op, reliable way to add padding between instructions
- Can update dvalid flags (DestDone, SrcSWrDone, SrcSRdDone)
- **Quasar usage**: `TTI_SFPNOP`

## 4. Data Format Support and Conversion Rules

### SFPU Internal Format
**Source: SFPU ISA, Appendix A; SFPU MAS (page 1256423592)**

- SFPU internally works in **FP32** (32-bit IEEE 754) for floating-point ops
- Also supports **SMAG32** (sign-magnitude 32-bit integer) and **UINT32** for integer ops
- All LREGs hold 32-bit values regardless of Dest/SrcS format
- Format conversion happens on SFPLOAD (regfile->LREG) and SFPSTORE (LREG->regfile)

### Unpacker Format Conversions (L1 -> Register Files)
**Source: Tensix Formats (page 237174853)**

Key conversions supported:
- Float32 -> Float32 (Dest/SrcS), TF32, Float16, Float16_B
- Float16/Float16_B/FP8R/FP8P/MXFP8R/MXFP8P/MXFP6R/MXFP6P/MXINT8/MXINT4/MXINT2 -> Float16, TF32 (not Dest/SrcS), Float16_B
- INT32 -> INT32 (Dest/SrcS only)
- INT8 -> INT8
- UINT8 -> UINT8
- INT16 -> INT16

### Packer Format Conversions (Register Files -> L1)
**Source: Tensix Formats (page 237174853)**

Key conversions supported:
- Float32 -> Float32, TF32, Float16, Float16_B, FP8R, FP8P, MXFP8R/P, MXFP6R/P, MXFP4, MXINT8/4/2
- Float16/Float16_B -> Float16, Float16_B, FP8R, FP8P, all MX formats
- INT32 -> INT32, INT8, UINT8
- INT8 -> INT8
- UINT8 -> UINT8
- INT16 -> INT16

### SFPLOAD Format Modes
**Source: SFPU ISA, SFPLOAD section**

| InstrMod | Register Format | LREG Format | Addressing |
|----------|----------------|-------------|------------|
| 0 (IMPLIED) | Implied from regfile state | FP32 | 16/32-bit |
| 1 (FP16_A) | FP16_A | FP32 | 16-bit |
| 2 (FP16_B) | FP16_B | FP32 | 16-bit |
| 3 (FP32) | MOD_FP32 | FP32 | 32-bit |
| 4 (SMAG32) | MOD_SMAG32 | SMAG32 | 32-bit |

## 5. Format Support Matrix for ELU

**ELU is a floating-point operation** (exp, multiply, subtract). It does NOT apply to integer data types.

| Format | Applicable? | Constraints | Source |
|--------|------------|-------------|--------|
| **Float16** (FP16a, enum 1) | YES | Stored as FP16a in Dest; SFPU loads->FP32, computes, stores back | Tensix Formats (page 237174853) |
| **Float16_b** (BF16, enum 5) | YES | Primary format; stored as FP16b in Dest; SFPU loads->FP32, computes, stores back | Tensix Formats (page 237174853) |
| **Float32** (enum 0) | YES | Requires is_fp32_dest_acc_en=true; 32-bit Dest mode | Dest storage formats (page 80674824) |
| **Tf32** (enum 4) | YES | Treated as Float32 in Dest/SrcS; unpacked to 32b containers | Tensix Formats (page 237174853) |
| **Int32** (enum 8) | NO | ELU requires exp() which is a float operation | N/A |
| **Int16** (enum 9) | NO | ELU requires exp() which is a float operation | N/A |
| **Int8** (enum 14) | NO | ELU requires exp() which is a float operation | N/A |
| **UInt8** (enum 17) | NO | ELU requires exp() which is a float operation | N/A |
| **MxFp8R** (enum 18) | YES (L1 only) | Unpacked to Float16_b for register storage; no direct Dest format | Tensix Formats unpacker table |
| **MxFp8P** (enum 20) | YES (L1 only) | Unpacked to Float16_b for register storage; no direct Dest format | Tensix Formats unpacker table |

**Notes**:
- MX formats (MxFp8R, MxFp8P) are L1 storage formats. They are unpacked to Float16 or Float16_b by the unpacker before reaching Dest/SrcS. The SFPU sees Float16/Float16_b data regardless of L1 format.
- The SFPU always operates in FP32 internally. Load converts to FP32, compute in FP32, store converts back.
- SFPSTORE does NOT round for 16-bit outputs - use SFP_STOCH_RND before SFPSTORE for faithful conversion. However, for ELU, the existing Blackhole implementation uses `float_to_fp16b()` for rounding when not in fp32_dest_acc_en mode. On Quasar with TTI-only programming, this truncation is acceptable (matches other Quasar SFPU kernels).

## 6. Pipeline Constraints and Instruction Ordering

### Auto-Stalling (Quasar-specific)
**Source: SFPU MAS (page 1256423592); DeepWiki Blackhole comparison**

- Quasar implements **automatic stalling** for SFPMAD/SFPADD/SFPMUL read-after-write hazards
- If a subsequent instruction tries to read a register still being written by a MAD instruction, the hardware stalls automatically
- This is an improvement over Wormhole (which required manual NOPs)
- Blackhole also has auto-stalling but with some known hardware bugs requiring manual NOPs

### Instruction Ordering Rules
1. **MAD -> any consumer**: Auto-stalled by hardware (2-cycle MAD latency)
2. **SFPCONFIG -> any**: Require 1 empty cycle after SFPCONFIG
3. **SFPSWAP -> SFPSWAP**: Require 1 idle cycle between SWAPs
4. **Load -> use**: Effective 1-cycle load-to-use latency (address generation in S1)
5. **Store -> done**: 2 cycles (SrcS), 3 cycles (Dest)

### LOADMACRO Rules
**Source: SFPU MAS, LOADMACRO Engine section (page 1256423592)**

- LOADMACRO allows up to 5 instructions to execute in parallel (1 per engine)
- Sequencer has shift registers per non-Load execution unit
- Instructions are injected at delay positions and shift toward head
- Can replay instruction templates with different VD and address values
- Backdoor loading (VD in range 0xC-0xF) captures into LOADMACRO instruction registers
- **For ELU**: LOADMACRO is NOT needed for the basic implementation. The TTI instruction approach is simpler and sufficient.

### Conditional Execution Pattern (Critical for ELU)
**Source: SFPU ISA, Conditional Execution Instructions; existing Quasar lrelu.h**

The standard pattern for conditional execution on Quasar:
```
TTI_SFPENCC(1, 2);           // Enable CC globally (CC.En = 1)
// ... loop over tiles:
  TTI_SFPSETCC(0, lreg, 0);  // Set CC.Res = (lreg < 0) for each lane
  // ... instructions here execute only where CC.Res = 1
  TTI_SFPENCC(0, 0);         // Reset CC.Res = 1 for all lanes (keep CC.En)
// ... end loop
TTI_SFPENCC(0, 2);           // Disable CC globally (CC.En = 0)
```

For ELU specifically:
- Check if x < 0 using SFPSETCC mode 0 (set if negative)
- Instructions between SFPSETCC and SFPENCC(0,0) execute only for lanes where x < 0
- For lanes where x >= 0, the value in Dest is untouched (identity: y = x)

## 7. Blackhole vs Quasar Differences (Key for Porting)

### Programming Model
| Aspect | Blackhole | Quasar |
|--------|-----------|--------|
| **API** | sfpi C++ intrinsics (vFloat, dst_reg, v_if/v_endif) | TTI assembly-like macros (TTI_SFPLOAD, TTI_SFPMAD, etc.) |
| **Loop counter** | `dst_reg++` via sfpi abstraction | `_incr_counters_<0x0, 0x0, SFP_ROWS, 0x0>()` |
| **SFPU rows per iter** | 8 (ITERATIONS=8 default) | SFP_ROWS=2 (process 2 rows per iteration) |
| **Iteration count** | `ITERATIONS` template param (default 8) | `num_sfpu_iterations = FACE_R_DIM / SFP_ROWS` |
| **Conditional execution** | `v_if (v < 0.0f) { ... } v_endif;` | `TTI_SFPSETCC` / `TTI_SFPENCC` / `TTI_SFPCOMPC` |
| **Format conversion** | `sfpi::float_to_fp16b()`, `sfpi::reinterpret<>()` | Not available; SFPSTORE handles truncation |
| **Template params** | `<APPROXIMATION_MODE, is_fp32_dest_acc_en, ITERATIONS>` | Function takes `const int iterations` and params |
| **Namespace** | `ckernel::sfpu` | `ckernel::sfpu` (same) |
| **Include** | `sfpi.h`, `ckernel_sfpu_converter.h` | `ckernel_trisc_common.h`, `cmath_common.h` |

### Blackhole ELU Implementation Pattern
```cpp
// Blackhole uses sfpi abstraction:
sfpi::vFloat v = sfpi::dst_reg[0];
v_if (v < 0.0f) {
    sfpi::vFloat v_exp = _sfpu_exp_21f_bf16_<true>(v) - sfpi::vConst1;
    sfpi::vFloat result = s * v_exp;
    if constexpr (!is_fp32_dest_acc_en) {
        result = sfpi::reinterpret<sfpi::vFloat>(sfpi::float_to_fp16b(result, 0));
    }
    sfpi::dst_reg[0] = result;
}
v_endif;
dst_reg++;
```

### Quasar ELU Translation (Expected Pattern)
```cpp
// Quasar uses TTI macros:
TTI_SFPLOAD(LREG0, DEFAULT, ADDR_MOD_7, 0, 0);      // Load from Dest
TTI_SFPSETCC(0, LREG0, 0);                            // CC.Res = (LREG0 < 0)
TTI_SFPNONLINEAR(LREG0, LREG1, EXP_MODE);            // LREG1 = exp(LREG0)
TTI_SFPADD(LCONST_1, LREG1, LCONST_neg1, LREG1, 0); // LREG1 = 1*LREG1 + (-1) = exp(x)-1
TTI_SFPMUL(LREG2, LREG1, LCONST_0, LREG0, 0);       // LREG0 = alpha * (exp(x)-1)
TTI_SFPENCC(0, 0);                                     // Reset CC
TTI_SFPSTORE(LREG0, 0, ADDR_MOD_7, 0, 0);            // Store to Dest
_incr_counters_<0x0, 0x0, SFP_ROWS, 0x0>();
```

### Key Differences
1. **No sfpi abstraction on Quasar** - must use raw TTI instructions
2. **No _sfpu_exp_21f_bf16_ available on Quasar** - must use SFPNONLINEAR(EXP_MODE) hardware approximation instead of polynomial approach
3. **No float_to_fp16b() for rounding** - SFPSTORE truncates; use SFP_STOCH_RND if precise rounding needed
4. **SFP_ROWS=2** vs Blackhole ITERATIONS=8 - different iteration count
5. **SFPNONLINEAR is named SFPARECIP in assembly.yaml** but the TTI macro remains `TTI_SFPNONLINEAR`
6. **Quasar auto-stalls** MAD hazards (no manual NOPs needed after MAD instructions)

## 8. ELU-Specific Considerations

### Algorithm
ELU(x, alpha) = x if x >= 0, alpha * (exp(x) - 1) if x < 0

### Key Requirements
1. Need to load alpha parameter before the loop (via SFPLOADI)
2. Conditional execution: only apply exp/multiply for negative values
3. For x >= 0: identity (do not modify Dest value)
4. For x < 0: compute exp(x), subtract 1, multiply by alpha, store result

### Accuracy Considerations
- SFPNONLINEAR(EXP_MODE) provides an **approximation** of exp(x)
- For small negative x, exp(x) is close to 1, so exp(x)-1 may lose precision due to catastrophic cancellation
- The Blackhole implementation uses a high-precision polynomial `_sfpu_exp_21f_bf16_` for this reason; Quasar's hardware approximation may be less accurate
- SFPSTORE truncates to 16-bit without rounding; for BF16 output, this introduces additional error

### Register Allocation for ELU
- **LREG0**: Load input value from Dest, also final result storage
- **LREG1**: exp(x) intermediate, then exp(x)-1 intermediate
- **LREG2**: Alpha (slope) parameter, loaded once before loop
- **LCONST_0** (=9): Zero constant for SFPMUL addend
- **LCONST_1** (=10): 1.0 constant for SFPADD multiplier
- **LCONST_neg1** (=11): -1.0 constant for subtraction

## 9. Source References

| Fact | Source |
|------|--------|
| SFPU is half-sized 8 slices x 4 rows = 32 lanes | SFPU MAS (page 1256423592), Hardware Overview |
| SFP_ROWS = 2 on Quasar | cmath_common.h line 16 |
| LREG layout (8 GPRs, 2 banks of 4) | SFPU MAS (page 1256423592), LREG Storage |
| MAD 2-cycle latency with auto-stall | SFPU MAS (page 1256423592), MAD EXU; DeepWiki |
| SFPNONLINEAR EXP_MODE = 0x4 | ckernel_instr_params.h; SFPU ISA (page 1170505767) |
| Conditional execution pattern | Existing Quasar lrelu.h |
| Format conversion on LOAD/STORE | SFPU ISA Appendix A (page 1170505767) |
| Dest storage formats | Dest storage formats (page 80674824) |
| SrcA/B storage formats | SrcA/B storage formats (page 83230723) |
| SrcS register organization | srcS registers (page 141000706) |
| Tensix format encodings | Tensix Formats (page 237174853) |
| Unpacker/packer format conversions | Tensix Formats (page 237174853) |
| Blackhole vs Quasar differences | DeepWiki (tenstorrent/tt-isa-documentation) |
| Blackhole ELU reference | tt_llk_blackhole/common/inc/sfpu/ckernel_sfpu_elu.h |
| Quasar exp reference | tt_llk_quasar/common/inc/sfpu/ckernel_sfpu_exp.h |
| Quasar lrelu (CC pattern) | tt_llk_quasar/common/inc/sfpu/ckernel_sfpu_lrelu.h |
| Quasar sigmoid (exp+recip) | tt_llk_quasar/common/inc/sfpu/ckernel_sfpu_sigmoid.h |
| Constants (LCONST_0, LCONST_1) | tt_llk_quasar/common/inc/ckernel_instr_params.h |
