# Architecture Research: fill kernel for Quasar

## 1. SFPU Execution Model

### Overview
The SFPU (SIMD Floating Point Unit) is a functional unit within the Tensix compute engine that complements the main math engine (FPU). It handles complex functions not suited for the FPU, including conditional execution, reciprocal, sigmoid, exponential, and fill/store operations.
(Source: Confluence page 1256423592 - Introduction)

### Quasar SFPU Configuration
- **8 slices** (half-sized configuration vs. full 16-column)
- **4 rows per slice** = 32 SFPU lanes total
- Each slice connects to corresponding SrcS Register slice and Dest Register slice
- SFP_ROWS = 2 (confirmed in cmath_common.h line 16): each SFPU iteration processes 2 rows of data
- Single clock domain, Fmax = 1.5 GHz
(Source: Confluence page 1256423592 - Introduction, Hardware Overview)

### Execution Pipeline
- S2: Instruction Frontend (decode + LOADMACRO)
- S3: Main execution for Simple EXU, Round EXU, first stage of MAD EXU
- S4: Second stage of MAD EXU, Store to SrcS completion
- S5: Store to Dest completion
- All instructions are fully pipelined except global SFPSHFT2 variants and SFPSWAP
- Up to 5 instructions can execute in parallel via LOADMACRO templates (Load, Simple, MAD, Round, Store)
(Source: Confluence page 1256423592 - Implementation Details)

### Key Pattern for Quasar SFPU Kernels
Unlike Blackhole which uses the sfpi:: C++ abstraction layer (vFloat, dst_reg, etc.), Quasar kernels use raw TTI_* instruction macros directly. The pattern is:
```cpp
namespace ckernel { namespace sfpu {
    inline void _calculate_{op}_sfp_rows_() {
        // Per-row SFPU operations using TTI_* macros
    }
    inline void _calculate_{op}_(const int iterations, ...) {
        // Setup (load constants, etc.)
        for (int d = 0; d < iterations; d++) {
            _calculate_{op}_sfp_rows_();
            ckernel::math::_incr_counters_<0x0, 0x0, ckernel::math::SFP_ROWS, 0x0>();
        }
    }
}} // namespace ckernel::sfpu
```
(Source: Existing Quasar implementations - ckernel_sfpu_relu.h, ckernel_sfpu_abs.h, ckernel_sfpu_lrelu.h)

---

## 2. Register File Layouts

### LREG Storage (per SFPU lane)
- **8 GPR LREGs** divided into 2 banks of 4 registers:
  - LREGS1: LREG0-LREG3
  - LREGS2: LREG4-LREG7
- Each LREG is 32 bits wide
- Each bank has 8 standard read ports, 3 standard write ports, 1 bulk read port, 1 bulk write port
- Constants: `p_sfpu::LREG0` through `p_sfpu::LREG7`
(Source: Confluence page 1256423592 - LREG Storage)

### Constant Registers (per slice, not per lane)
- 6 constant registers (shared across all lanes in a slice)
- 4 programmable constants (registers 2-5) accessible via RG view
- 2 fixed constants:
  - LCONST_0 = 0.0
  - LCONST_1 = 1.0
- Updated only through SFPCONFIG instruction, written by column 0 row 0
- Constants: `p_sfpu::LCONST_0`, `p_sfpu::LCONST_1`
(Source: Confluence page 1170505767 - Register Description, page 1256423592 - Constant Registers)

### Staging Register
- 1 per lane, used as part of LOADMACRO sequences
- 1 read port + 1 write port
(Source: Confluence page 1256423592 - Staging Register)

### SrcS Registers
- 2 banks, each with 3 slices of registers
- Each slice: 2 instances of tt_reg_bank
- Each instance: 4 rows x 16 columns x 16 bits
- Unpacker writes to slices 0-1; SFPU loads/writes to slice 2; packer reads from slice 2
- Data is 16-bit or 32-bit (paired slices)
- Synchronization via data_valid / pack_ready / sfpu_wr_ready flags
(Source: Confluence page 141000706 - Overview, Detailed description)

### Dest Register
- 16-bit mode: 1024 rows
- 32-bit mode: 512 rows (pairs of 16-bit rows combined)
- In 32-bit mode: physical row i = MSBs, physical row i+N/2 = LSBs
- Two banks with bank selection via BankSelect bit in address
(Source: Confluence page 195493892, page 80674824)

---

## 3. Instructions Needed for Fill Kernel

The fill kernel needs these instructions:

### SFPLOADI (opcode 0x71) - Load Immediate
- **Encoding**: MI (immediate format)
- **Purpose**: Load a 16-bit immediate value into an LREG with format conversion
- **IPC**: 1, **Latency**: 1 cycle
- **Quasar macro**: `TTI_SFPLOADI(lreg_ind, instr_mod0, imm16)`
- **InstrMod values**:
  - 0x0 (FP16_B): Convert FP16_b immediate to FP32 in LREG (writes both MSBs and LSBs)
  - 0x1 (FP16_A): Convert FP16_a immediate to FP32 in LREG
  - 0x2 (UINT16): Zero-extend to UINT32
  - 0x4 (INT16): Sign-extend to INT32
  - 0x8 (HI16_ONLY): Write only upper 16 bits, preserve lower 16 bits
  - 0xA (LO16_ONLY): Write only lower 16 bits, preserve upper 16 bits
- **Algorithm**: `if (LaneEnabled) { RG[VD] = convert(Imm16, InstrMod); }`
- **Does NOT set CC, does NOT flush subnormals**
- For the fill kernel: used to load the fill value into an LREG. Float values use FP16_B mode (instr_mod0=0). Integer values use UINT16 (0x2) or HI16_ONLY+LO16_ONLY combo for 32-bit values.
(Source: Confluence page 1170505767 - SFPLOADI section, assembly.yaml line 5627)

### SFPSTORE (opcode 0x72) - Store to Register File
- **Encoding**: MR (memory register format)
- **Purpose**: Write LREG data back to Dest or SrcS register files
- **IPC**: 1, **Latency**: 2 cycles (SrcS), 3 cycles (Dest)
- **Quasar macro**: `TTI_SFPSTORE(lreg_ind, instr_mod0, sfpu_addr_mode, done, dest_reg_addr)`
- **InstrMod values (format select)**:
  - 0x0 (IMPLIED): Use implied format from register file
  - 0x1 (FP16_A): FP32 LREG -> FP16_A in RF, 16-bit addressing
  - 0x2 (FP16_B): FP32 LREG -> FP16_B in RF, 16-bit addressing
  - 0x3 (FP32): FP32 LREG -> MOD_FP32 in RF, 32-bit addressing
  - 0x4 (SMAG32): SMAG32 LREG -> MOD_SMAG32 in RF, 32-bit addressing
  - 0x6 (UINT16): UINT32 LREG -> UINT16 in RF, 16-bit addressing
  - 0x7 (HI16): UINT32 LREG -> UINT32 in RF, 32-bit addressing (swapped halves)
  - 0x9 (LO16): UINT32 LREG -> UINT32 in RF, 32-bit addressing
  - 0xB (UINT8): UINT32 LREG -> UINT8 in RF, 16-bit addressing
- **Write is predicated on LaneEnabled AND !STORE_DISABLE**
- **Done flag**: toggles write bank ID for the register file
- **IMPORTANT**: SFPSTORE does NOT perform rounding or saturation when converting to 8/16 bit formats; use SFP_STOCH_RND first for faithful conversion
(Source: Confluence page 1170505767 - SFPSTORE section, assembly.yaml line 5658)

### SFPNOP (opcode 0x8F) - No Operation
- **Purpose**: Explicit NOP for padding between instructions or toggling dvalid flags
- **IPC**: 1, **Latency**: 1 cycle
- **Quasar macro**: `TTI_SFPNOP(srcs_wr_done, srcs_rd_done, dest_done)`
- **Side effects**: Can set dvalid for Dest, SrcS write, SrcS read banks
- Used in init/done sequences: `TTI_SFPNOP(SRCS_WR_DONE, SRCS_RD_DONE, 0)`
(Source: Confluence page 1170505767 - SFPNOP section, assembly.yaml line 6122)

### SFPMOV (opcode 0x7C) - Move Register
- **Purpose**: Copy source register to destination register
- **IPC**: 1, **Latency**: 1 cycle
- **Quasar macro**: `TTI_SFPMOV(lreg_c, lreg_dest, instr_mod1)`
- **InstrMod values**:
  - 0: Copy RG[VC] to RG[VD]
  - 1: Copy with sign inversion
  - 2: Unconditional copy (ignores LaneEnabled)
  - 8: Copy from RS[VC] to RG[VD]
- Not directly needed for the basic fill, but may be useful if loading a 32-bit constant via two SFPLOADI operations
(Source: Confluence page 1170505767 - SFPMOV section, assembly.yaml line 5827)

### SFPSETCC (opcode 0x7B) - Set Condition Code
- **Purpose**: Set CC.Res based on register value or immediate
- **IPC**: 1, **Latency**: 1 cycle
- **Quasar macro**: `TTI_SFPSETCC(imm12, lreg_c, instr_mod1)`
- Not needed for the basic fill kernel, but may be needed if implementing conditional fill (fill_int with predicated stores)
(Source: Confluence page 1170505767 - SFPSETCC section)

### Helper Functions in cmath_common.h (Quasar)

**_sfpu_load_config32_(dest, upper16, lower16)**: Loads a 32-bit value into a programmable constant register via two SFPLOADI calls (LO16_ONLY + HI16_ONLY modes to LREG0, then SFPCONFIG).

**_incr_counters_<SRCA, SRCB, DEST, CR>()**: Increments the Dest RWC counter by `DEST` rows. For SFPU, typically `_incr_counters_<0x0, 0x0, SFP_ROWS, 0x0>()` to advance by 2 rows.
(Source: cmath_common.h lines 101-118)

---

## 4. Data Format Support and Conversion Rules

### Unpacker Format Conversions (L1 -> Register Files)
Key conversions supported:
- Float32 -> Float32 (Dest/SrcS only), TF32, Float16, Float16_B
- Float16/Float16_B/FP8R/FP8P/MX formats -> Float16, Float16_B, TF32 (not for Dest/SrcS)
- INT32 -> INT32 (Dest/SrcS only)
- INT8 -> INT8
- UINT8 -> UINT8
- INT16 -> INT16
(Source: Confluence page 237174853 - Unpacker Format Conversions)

### Packer Format Conversions (Register Files -> L1)
- Float32 -> Float32, TF32, Float16, Float16_B, FP8R/P, MX formats
- Float16/Float16_B -> Float16, Float16_B, FP8R/P, MX formats
- INT32 -> INT32, INT8, UINT8
- INT8 -> INT8
- UINT8 -> UINT8
- INT16 -> INT16
(Source: Confluence page 237174853 - Packer Format Conversions)

### Dest Storage Formats
- FP16a: `{sgn, man[9:0], exp[4:0]}`
- FP16b: `{sgn, man[6:0], exp[7:0]}`
- FP32: split across two physical rows (i and i+N/2)
- Int8 (signed): sign+magnitude `{sgn, 3'b0, mag[6:0], 5'd16}`
- Int8 (unsigned): `{3'b0, val[7:0], 5'd16}`
- Int16 (signed): sign+magnitude `{sgn, mag[14:0]}`
- Int16 (unsigned): `{val[15:0]}`
- Int32 (signed): split across two rows, sign+magnitude
- **NOT supported in Dest**: Int32 unsigned, TF19, any BFP format
(Source: Confluence page 80674824)

### SFPU Load/Store Format Conversions
- SFPLOADI: 16-bit immediate -> 32-bit LREG (FP16_A->FP32, FP16_B->FP32, INT16->INT32 sign-extend, UINT16->UINT32 zero-extend, or partial 16-bit writes)
- SFPSTORE: 32-bit LREG -> register file format (FP32->FP16_A/FP16_B/FP32, SMAG32->various, UINT32->UINT16/UINT8, etc.)
(Source: Confluence page 1170505767 - SFPLOADI, SFPSTORE sections)

---

## 5. Format Support Matrix for Fill Kernel

The fill kernel writes a constant value to all elements in destination register tiles. Because it is writing (not computing), it is applicable to ALL data formats that the register file and pack/unpack pipeline support.

### Applicable Formats

| Format | Enum Value | Applicable? | Fill Variant | Constraints |
|--------|-----------|-------------|--------------|-------------|
| Float16 | 1 | YES | _calculate_fill_ (float) | Value loaded as FP16_A via SFPLOADI(instr_mod0=1), stored via implied format |
| Float16_b | 5 | YES | _calculate_fill_ (float) | Value loaded as FP16_B via SFPLOADI(instr_mod0=0), stored via implied format |
| Float32 | 0 | YES | _calculate_fill_ (float) | Needs 32-bit store mode or bitcast variant |
| Tf32 | 4 | YES | _calculate_fill_ (float) | Stored as Float32 in Dest/SrcS |
| Int32 | 8 | YES | _calculate_fill_int_ (INT32 mode) | Needs _sfpu_load_imm32_ equivalent (two SFPLOADI + store) |
| Int16 | 9 | YES | _calculate_fill_int_ (LO16 mode) | Sign+magnitude in Dest |
| Int8 | 14 | YES | _calculate_fill_int_ | May need special handling for sign+magnitude storage |
| UInt8 | 17 | YES | _calculate_fill_int_ | May need special handling |
| UInt16 | 130 | YES | _calculate_fill_int_ | SFPU uses INT16 for UINT16 |
| MxFp8R | 18 | YES | _calculate_fill_ (float) | L1 format only; unpacked to Float16_b for math; fill writes to Dest which is then packed to MX |
| MxFp8P | 20 | YES | _calculate_fill_ (float) | Same as MxFp8R |

### Format-Specific Constraints
1. **Float formats** (Float16, Float16_b, Float32, Tf32): Use the float fill variant. SFPLOADI with FP16_B mode loads a FP16_b immediate and auto-converts to FP32 in the LREG. SFPSTORE with implied format (instr_mod0=0) writes in the format implied by the Dest configuration.
2. **Integer formats** (Int32, Int16, Int8, UInt8): Use the integer fill variant. Need explicit SFPSTORE InstrMod to select the correct output format. Int32 requires loading 32 bits via two SFPLOADI operations (HI16_ONLY + LO16_ONLY).
3. **MX formats** (MxFp8R, MxFp8P): These are L1-only formats. Data in Dest is in Float16_b or Float16 format. The packer handles conversion from Dest format to MX format in L1. Fill just needs to write the correct floating-point value to Dest.
4. **Subnormal handling**: SFPLOADI does NOT flush subnormals. SFPSTORE does NOT flush subnormals. The unpacker/packer handle subnormal clamping per format rules.
(Source: Confluence pages 237174853, 80674824, 1170505767)

---

## 6. Pipeline Constraints and Instruction Ordering

### General Rules
1. No WAR hazard on LREGs across consecutive instructions (architectural guarantee)
2. SFPLOADI followed by SFPSTORE using the same LREG: safe with 1-cycle latency (SFPLOADI latency = 1)
3. SFPSTORE latency: 2 cycles for SrcS, 3 cycles for Dest. But the pipeline is fully pipelined so new stores can be issued every cycle.
4. SFPCONFIG requires an idle cycle after it (no instructions on the following cycle)
5. ADDR_MOD_7 is configured to {.srca=0, .srcb=0, .dest=0} by _eltwise_unary_sfpu_configure_addrmod_()

### LOADMACRO Rules
Not needed for the fill kernel. Fill is simple enough to execute without LOADMACRO sequences.
(Source: Confluence page 1256423592 - LOADMACRO Engine)

### Counter Increment Pattern
After each iteration of SFPU rows, increment the Dest RWC by SFP_ROWS (=2):
```cpp
ckernel::math::_incr_counters_<0x0, 0x0, ckernel::math::SFP_ROWS, 0x0>();
```
This advances the Dest address pointer so the next store writes to the next set of rows.
(Source: Existing Quasar implementations, cmath_common.h)

---

## 7. Blackhole vs. Quasar Differences

### Critical Porting Differences
1. **No sfpi:: abstraction layer on Quasar**: Blackhole fill uses `sfpi::vFloat`, `sfpi::dst_reg[]`, `Converter::as_float()`. Quasar has NONE of these. Must use raw TTI_SFPLOADI, TTI_SFPSTORE, and _incr_counters_ macros.

2. **No `ckernel_sfpu_converter.h` on Quasar**: The Converter::as_float() utility for bitcasting uint32 to float does not exist on Quasar. The bitcast fill variant must be implemented differently.

3. **No `ckernel_sfpu_load_config.h` on Quasar**: Blackhole fill uses `_sfpu_load_imm32_()` and `_sfpu_load_imm16_()` helpers. These do not exist on Quasar. The equivalent must be done inline using TTI_SFPLOADI with HI16_ONLY/LO16_ONLY modes.

4. **No `InstrModLoadStore` enum on Quasar**: Blackhole's `_calculate_fill_int_` is templated on `InstrModLoadStore`. This enum is defined in Blackhole's llk_defs.h but NOT in Quasar's llk_defs.h.

5. **Namespace difference**: Blackhole uses `namespace ckernel::sfpu`. Quasar uses `namespace ckernel { namespace sfpu {`.

6. **Function signature difference**: Blackhole fill uses template parameters `<bool APPROXIMATION_MODE, int ITERATIONS>`. Quasar SFPU kernels use `inline void _calculate_{op}_(const int iterations, ...)` with iterations as a runtime parameter, not a template parameter.

7. **Row iteration**: Blackhole uses `sfpi::dst_reg++` to advance. Quasar uses `ckernel::math::_incr_counters_<0x0, 0x0, ckernel::math::SFP_ROWS, 0x0>()`.

8. **Include files**: Blackhole includes `ckernel_ops.h`, `ckernel_sfpu_converter.h`, `ckernel_sfpu_load_config.h`. Quasar includes `ckernel_trisc_common.h`, `cmath_common.h`.

### SFPU Lane Count
- Blackhole: 32 lanes (16 slices x 2? or full configuration)
- Quasar: 32 lanes (8 slices x 4 rows), SFP_ROWS = 2
(Source: Confluence page 1256423592, DeepWiki)

---

## 8. Assembly.yaml Cross-Check

All required instructions verified present in `tt_llk_quasar/instructions/assembly.yaml`:

| Instruction | Opcode | Line in assembly.yaml | Status |
|------------|--------|----------------------|--------|
| SFPLOADI | 0x71 | 5627 | CONFIRMED |
| SFPSTORE | 0x72 | 5658 | CONFIRMED |
| SFPMOV | 0x7C | 5827 | CONFIRMED |
| SFPSETCC | 0x7B | 5817 | CONFIRMED |
| SFPENCC | 0x8F... | 6007 | CONFIRMED |
| SFPCOMPC | 0x7A... | 6017 | CONFIRMED |
| SFPNOP | 0x8F | 6122 | CONFIRMED |

---

## 9. Existing Quasar Patterns for Fill-like Operations

### Loading a float constant and storing (from lrelu.h):
```cpp
TTI_SFPLOADI(p_sfpu::LREG2, 0 /*Float16_b*/, (slope >> 16)); // load FP16_b constant
```

### Loading a 32-bit value (from cmath_common.h _sfpu_load_config32_):
```cpp
TTI_SFPLOADI(p_sfpu::LREG0, 10, lower16); // insmod == 0xA: LO16_ONLY
TTI_SFPLOADI(p_sfpu::LREG0, 8, upper16);  // insmod == 0x8: HI16_ONLY
```

### Store with implied format (from relu.h, abs.h):
```cpp
TTI_SFPSTORE(p_sfpu::LREG1, 0, ADDR_MOD_7, 0, 0); // instr_mod0=0 = IMPLIED format
```

### Iteration pattern:
```cpp
for (int d = 0; d < iterations; d++) {
    _calculate_{op}_sfp_rows_();
    ckernel::math::_incr_counters_<0x0, 0x0, ckernel::math::SFP_ROWS, 0x0>();
}
```

---

## 10. Summary: What the Fill Kernel Needs

### Sub-kernels (3 variants from Blackhole reference):
1. **_calculate_fill_**: Float fill. Load float value via SFPLOADI (FP16_B mode), store to Dest via SFPSTORE (implied format) in a loop.
2. **_calculate_fill_int_**: Integer fill. Load integer value via SFPLOADI (UINT16 or two-step HI16+LO16 for 32-bit), store to Dest via SFPSTORE with explicit integer format mode.
3. **_calculate_fill_bitcast_**: Bitcast fill. Reinterpret a uint32 bit pattern as a float and fill. Since Converter::as_float doesn't exist on Quasar, this needs to load the bit pattern via two SFPLOADI operations (HI16_ONLY + LO16_ONLY) and store with implied format.

### Key Design Decisions for Quasar:
- Use raw TTI_* macros throughout (no sfpi::)
- Use `namespace ckernel { namespace sfpu {` pattern
- Use runtime `iterations` parameter (not template)
- Use `_incr_counters_<0,0,SFP_ROWS,0>()` for dest advancement
- Include `ckernel_trisc_common.h` and `cmath_common.h`
- For integer fill: since InstrModLoadStore enum doesn't exist on Quasar, use raw integer constants for SFPSTORE instr_mod0 parameter (e.g., 0x4 for SMAG32, 0x9 for LO16)
