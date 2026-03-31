# Architecture Research: "where" Kernel for Quasar

## 1. SFPU Execution Model

The SFPU (SIMD Floating Point Unit) on Quasar is a SIMD engine complementing the main FPU. Key characteristics:

- **8 SFPU instances** (lanes 0-7), each processing 4 rows of data simultaneously
- **32 lanes total**: 8 instances x 4 rows = 32 datums processed per instruction
- **SFP_ROWS = 2**: Quasar processes 2 rows of output per SFPU iteration (each SFPU instance handles 4 rows of Dest, but logical iteration increments by 2 rows via `_incr_counters_`)
- **ITERATIONS parameter**: For a standard 32x32 tile (1024 elements), `iterations = 32` in test harness. Each iteration processes 2 rows via SFP_ROWS, covering all 64 rows.
- Instructions execute in pipeline: Load sub-unit, Simple/Math sub-unit, Store sub-unit
- Conditional execution via CC (Condition Code) register per lane

**Source**: Confluence page 1256423592 (Quasar/Trinity SFPU MAS)

## 2. Register File Layouts

### 2.1 LRegs (Local Registers)
- **16 LRegs** (LREG0-LREG15), each holding 32 lanes of 32-bit values
- LREG0-LREG7: General purpose, directly addressable
- LREG8-LREG15: Indirect addressing / special purpose
- LREG11-LREG14: Configurable via SFPCONFIG (hold constants: -1.0, 1/65536, -0.67487759, -0.34484843 when `MOD1_IMM16_IS_VALUE` is used)
- LREG12-LREG15: Also accessible as `SFPSTORE` targets with VD >= 12 (writes instruction bits to LoadMacroConfig when `DISABLE_BACKDOOR_LOAD` is false)
- p_sfpu::LREG0 = 0, LREG1 = 1, ..., LREG7 = 7
- p_sfpu::LCONST_neg1 = 11 (holds -1.0), LCONST_1 = 12, etc.

**Source**: Confluence page 1612808665 (SFPLOAD ISA), page 1256423592 (SFPU MAS)

### 2.2 Dest Register
- 16 columns x 1024 rows (16-bit mode) or 512 rows (32-bit mode)
- Supports: FP16a, FP16b, FP32, Int8 (signed/unsigned), Int16 (signed/unsigned), Int32 (signed)
- **Does NOT support**: Int32 unsigned, Tf19/Tf32 (treated as FP32), any BFP format
- Double-banked for concurrent read/write
- SFPU reads from Dest via SFPLOAD, writes back via SFPSTORE

**Source**: Confluence page 195493892 (Dest), page 80674824 (Dest storage formats)

### 2.3 SrcS Register
- 2 banks x 3 slices x 2 instances x 4 rows x 16 columns x 16 bits
- Unpacker writes slices 0-1, SFPU reads from these and writes to slice 2
- Packer reads from slice 2
- Double-banked for pipeline overlap
- Data format determined by unpacker output format or by slice format registers
- 16-bit and 32-bit modes supported

**Source**: Confluence page 141000706 (srcS registers)

## 3. Instructions Required by the "where" Kernel

The "where" kernel implements: `result = (condition == 0) ? value_if_false : value_if_true`

It needs: SFPLOAD, SFPSTORE, SFPSETCC, SFPENCC, SFPMOV, and optionally SFPLOADMACRO/SFPCONFIG/REPLAY for optimization.

### 3.1 SFPLOAD (Move from Dest/SrcS to LReg)

**Quasar signature**: `TT_SFPLOAD(lreg_ind, instr_mod0, sfpu_addr_mode, done, dest_reg_addr)` (5 params)
**Blackhole signature**: `TT_SFPLOAD(lreg_ind, instr_mod0, sfpu_addr_mode, dest_reg_addr)` (4 params - no `done`)

- Loads up to 32 datums from Dest or SrcS into an LReg
- `dest_reg_addr`: bit 10 selects Dest (0) vs SrcS (1); bits 2-9 are row address; bit 1 selects even (0) vs odd (1) columns
- `done`: When targeting SrcS, toggles bank ID and dvalid
- `instr_mod0`: Data format conversion mode:
  - `DEFAULT (0)`: Based on SrcB format config
  - `FP16A (1)`: FP16 from Dest -> FP32 in LReg
  - `FP16B (2)`: BF16 from Dest -> FP32 in LReg
  - `FP32 (3)`: FP32 from Dest -> FP32 in LReg
  - `INT32 (4)`: Int32 from Dest -> sign-magnitude in LReg
  - `INT8 (5)`: Int8 from Dest -> sign-magnitude in LReg
  - `UINT16 (6)`: UInt16 from Dest -> zero-extended in LReg
  - `INT16 (8)`: Int16 from Dest -> sign-magnitude in LReg
  - `UINT8 (11)`: UInt8 from Dest -> unsigned in LReg
  - `LO16_ONLY (14)`: Write low 16 bits only, preserve high 16
  - `HI16_ONLY (15)`: Write high 16 bits only, preserve low 16
- `sfpu_addr_mode`: 3-bit ADDR_MOD_REG index (only applied when targeting Dest)
- Fully pipelined: one SFPLOAD per cycle

**Quasar `p_sfpu::sfpmem` constants**:
```
DEFAULT = 0b0000
FP16A   = 0b0001
FP16B   = 0b0010
FP32    = 0b0011
INT32   = 0b0100
UINT8   = 0b0101
UINT16  = 0b0110
```

**Source**: Confluence page 1612808665 (SFPLOAD ISA)

### 3.2 SFPSTORE (Move from LReg to Dest/SrcS)

**Quasar signature**: `TT_SFPSTORE(lreg_ind, instr_mod0, sfpu_addr_mode, done, dest_reg_addr)` (5 params)
**Blackhole signature**: `TT_SFPSTORE(lreg_ind, instr_mod0, sfpu_addr_mode, dest_reg_addr)` (4 params - no `done`)

- Stores up to 32 datums from an LReg to Dest or SrcS
- Same addressing as SFPLOAD
- `done`: When targeting SrcS/Dest, toggles bank IDs
- `instr_mod0` format conversion modes mirror SFPLOAD but in reverse direction
- **LaneEnabled**: Only writes to lanes where CC is set (unless STACK_MODE)
- Denormals flushed to zero in FP conversions

**Source**: Confluence page 1612382850 (SFPSTORE ISA)

### 3.3 SFPSETCC (Set CC Result)

**Quasar signature**: `TTI_SFPSETCC(imm12_math, lreg_c, instr_mod1)` (3 params)
**Blackhole signature**: `TTI_SFPSETCC(imm12_math, lreg_c, lreg_dest, instr_mod1)` (4 params)

- Sets the per-lane CC Result register based on LREG state
- `lreg_c`: Source LReg index for comparison
- `instr_mod1` modes:
  - **0x0**: Set CC Result if src_c is **negative** (x < 0)
  - **0x1**: Set CC Result to `imm12_math[0]` (constant)
  - **0x2**: Set CC Result if src_c is **not zero** (x != 0)
  - **0x4**: Set CC Result if src_c is **positive** (x > 0)
  - **0x6**: Set CC Result if src_c is **zero** (x == 0)
  - **0x8**: Invert current CC Result
- `imm12_math` bit 11: If set, interpret src_c as INT32 instead of FP32/SMAG32

**CRITICAL Quasar difference**: Quasar's `TTI_SFPSETCC` takes 3 params (no `lreg_dest`). The `lreg_dest` field (bits 4-7 in SFPU_MATHI12 encoding) is part of the instruction word but not exposed as a separate macro parameter. In existing Quasar kernels, it appears the lreg_c is at bits 8-11 and instr_mod1 at bits 0-3, with lreg_dest unused/zero in the TT_OP_SFPSETCC macro.

**For the "where" kernel**: `TTI_SFPSETCC(0, p_sfpu::LREG0, 0x6)` sets CC where LREG0 == 0 (instr_mod1=0x6).

**Source**: Confluence page 1612808695 (SFPSETCC ISA), Quasar ckernel_ops.h

### 3.4 SFPENCC (Enable/Disable CC)

**Quasar signature**: `TTI_SFPENCC(imm12_math, instr_mod1)` (2 params)
**Blackhole signature**: `TTI_SFPENCC(imm12_math, lreg_c, lreg_dest, instr_mod1)` (4 params)

- Manipulates the CC enable/disable state
- Used to:
  - **Reset CC** (all lanes active): `TTI_SFPENCC(0, 0)` -- clears CC, all lanes enabled
  - **Enable conditional execution**: `TTI_SFPENCC(1, 2)` -- enables CC-based lane masking
  - **Disable conditional execution**: `TTI_SFPENCC(0, 2)` -- disables CC, all lanes active again

**CRITICAL Quasar vs Blackhole difference**: Quasar SFPENCC has only 2 parameters. The Blackhole version has 4 parameters and uses different encoding for enabling CC-based conditional mode.

From existing Quasar kernels (sign.h, threshold.h, hardtanh.h, lrelu.h), the pattern is:
```cpp
TTI_SFPENCC(1, 2);  // Enable conditional execution globally
// ... loop body with SFPSETCC + conditional ops ...
TTI_SFPENCC(0, 2);  // Disable conditional execution
```

Within the loop body:
```cpp
TTI_SFPENCC(0, 0);  // Reset CC -- all lanes active (clear CC result)
```

**Blackhole `where` kernel uses**: `TTI_SFPENCC(0, 0, 0, sfpi::SFPENCC_MOD1_EU_R1)` which becomes `TTI_SFPENCC(0, 0)` on Quasar.

**Source**: Quasar ckernel_ops.h, existing Quasar kernels

### 3.5 SFPMOV (Conditional Move)

Present in Quasar assembly.yaml as opcode 0x7C.
- Copies value from one LReg to another, subject to LaneEnabled
- Used in existing Quasar kernels: `TTI_SFPMOV(src_lreg, dst_lreg, 0)`
- When CC is set on a lane, the value from src_lreg is written to dst_lreg for that lane

**Source**: assembly.yaml (SFPMOV, op 0x7C)

### 3.6 SFPCOMPC (Complement CC)

**Quasar signature**: `TTI_SFPCOMPC` (no params)
- Complements or clears the CC Result register
- Inverts which lanes are active

**Source**: Confluence page 1612186107 (SFPCOMPC ISA)

### 3.7 SFPLOADMACRO (Load and Run Macro) -- for optimization

**Quasar signature**: `TT_SFPLOADMACRO(seq_id, lreg_ind_lo, instr_mod0, sfpu_addr_mode, done, dest_reg_addr, lreg_ind_hi)` (7 params)
**Blackhole signature**: `TT_SFPLOADMACRO(macro_id_packed, instr_mod0, sfpu_addr_mode, dest_reg_addr)` (4 params)

- Performs SFPLOAD AND triggers a pre-programmed macro sequence
- `seq_id`: 2-bit macro sequence ID (0-3) to run after load
- `lreg_ind` = `(lreg_ind_hi << 2) | lreg_ind_lo`: Target LReg for the load
- Other params same as SFPLOAD

**CRITICAL Quasar vs Blackhole difference**: Quasar has 7 explicit parameters vs Blackhole's 4. The Blackhole version packs `macro_id` into the lreg_ind field. On Quasar, `seq_id` and `lreg_ind` are separate.

**Source**: Confluence page 1613988268 (SFPLOADMACRO ISA), Quasar ckernel_ops.h

### 3.8 SFPCONFIG (Configure Macros) -- for optimization

**Quasar signature**: `TTI_SFPCONFIG(imm16_math, config_dest, instr_mod1)` (3 params)

- Configures LoadMacroConfig for SFPLOADMACRO sequences
- `config_dest`:
  - 0-3: Write to InstructionTemplate[0-3]
  - 4-7: Write to Sequence[0-3]
  - 8: Write/manipulate Misc
  - 11-14: Write to LReg[11-14]
  - 15: Write/manipulate LaneConfig
- `instr_mod1`:
  - bit 0 (MOD1_IMM16_IS_VALUE): Use imm16 as value (vs LReg[0])
  - bits 1-2: Bitwise operation (OR, AND, XOR)

**Source**: Confluence page 1613103724 (SFPCONFIG ISA)

### 3.9 SFPLOADI (Load Immediate)

**Quasar signature**: `TTI_SFPLOADI(lreg_ind, instr_mod0, imm16)`

- Loads a 16-bit immediate to all lanes of an LReg
- `instr_mod0`:
  - 0: Convert BF16 imm16 to FP32 (sign-extend exponent, zero-extend mantissa)
  - 2: UINT16 -- zero-extend 16 bits to 32 bits
  - 8: HI16_ONLY -- write to upper 16 bits, preserve lower 16
  - 10: LO16_ONLY -- write to lower 16 bits, preserve upper 16
- Used for loading constants (e.g., `TTI_SFPLOADI(p_sfpu::LREG1, 0, 0x3F80)` loads 1.0f)

**Source**: Confluence page 1612218782 (SFPLOADI ISA)

### 3.10 REPLAY (Record/Replay Instructions) -- for optimization

**Quasar signature**: `TTI_REPLAY(start_idx, len, last, set_mutex, execute_while_loading, load_mode)` (6 params)
**Blackhole uses**: `lltt::record(start, len)` / `lltt::replay(start, len)` wrappers

**Quasar wrapper**: `load_replay_buf<start, len, exec_while_loading, set_mutex, last>(lambda)` -- template version for compile-time known params, or runtime version with explicit params.

- `load_mode=1`: Record next `len` instructions into replay buffer
- `load_mode=0`: Replay `len` instructions from buffer
- `execute_while_loading`: When recording, also execute the instructions
- Replay buffer is 32 entries deep per thread, with double banking

**Source**: Confluence page 1612808713 (REPLAY ISA), Quasar ckernel.h

## 4. How the "where" Kernel Works (Blackhole Reference)

The Blackhole `_calculate_where_` implements:
```
result[i] = (condition[i] == 0) ? false_value[i] : true_value[i]
```

### 4.1 Algorithm (DISABLE_SFPLOADMACRO path)
1. Load condition from Dest[offset0] into LREG0
2. Load true_value from Dest[offset1] into LREG1 (initially, LREG1 = true_value)
3. Set CC where LREG0 == 0: `SFPSETCC(0, LREG0, 0, SFPSETCC_MOD1_LREG_EQ0)` -- CC set for lanes where condition is zero
4. Load false_value from Dest[offset2] into LREG1 (overwrites true_value ONLY in CC-set lanes, i.e., where condition == 0)
5. Reset CC: `SFPENCC(0, 0, 0, SFPENCC_MOD1_EU_R1)` -- re-enable all lanes
6. Store LREG1 to Dest[offset3]

After step 4, LREG1 contains:
- false_value where condition == 0
- true_value where condition != 0

### 4.2 Key Observations for Quasar Port
- The kernel reads from 3 different Dest tile locations and writes to a 4th (or overlapping one)
- Offsets are computed as `(dst_index_in * 32) << 1` -- each tile is 32 rows, shift left by 1 for 16-bit addressing
- The `data_format` template parameter selects the SFPLOAD/SFPSTORE format mode:
  - Float16_b -> `InstrModLoadStore::LO16` (Blackhole) / `p_sfpu::sfpmem::FP16B` (Quasar, value 2)
  - Float32/Int32/UInt32 -> `InstrModLoadStore::INT32` (Blackhole) / `p_sfpu::sfpmem::INT32` (Quasar, value 4)

  **But on Quasar**, the default (`p_sfpu::sfpmem::DEFAULT = 0`) resolves based on SrcB format config and should work for all formats.

### 4.3 SFPLOADMACRO Optimization (Blackhole)
The Blackhole reference has an optimized path using SFPLOADMACRO:
- **Case 1** (dst_out == dst_in0): Uses macros 0 and 2, 3 cycles per row
- **Case 2** (dst_out != dst_in0): Uses macros 1 and 2, 4 cycles per row

This optimization uses `_init_where_` to configure:
- InstructionTemplate[0]: `SFPSETCC` with LREG_EQ0 mode
- InstructionTemplate[1]: `SFPENCC` with EU_R1 mode
- Macros 0, 1, 2: Scheduling patterns for load+compute+store pipelining

## 5. Quasar-Specific Considerations for Port

### 5.1 Critical API Differences

| Instruction | Blackhole Params | Quasar Params | Key Change |
|---|---|---|---|
| SFPLOAD | `(lreg, mod0, addr_mode, dest_addr)` | `(lreg, mod0, addr_mode, done, dest_addr)` | Added `done` param |
| SFPSTORE | `(lreg, mod0, addr_mode, dest_addr)` | `(lreg, mod0, addr_mode, done, dest_addr)` | Added `done` param |
| SFPSETCC | `(imm12, lreg_c, lreg_dest, mod1)` | `(imm12, lreg_c, mod1)` | Removed `lreg_dest` |
| SFPENCC | `(imm12, lreg_c, lreg_dest, mod1)` | `(imm12, mod1)` | Removed `lreg_c`, `lreg_dest` |
| SFPLOADMACRO | `(macro_packed, mod0, addr_mode, addr)` | `(seq_id, lreg_lo, mod0, addr_mode, done, addr, lreg_hi)` | 4->7 params, explicit fields |
| REPLAY | `lltt::record(start, len)` / `lltt::replay(start, len)` | `load_replay_buf<start, len>(lambda)` / `TTI_REPLAY(start, len, 0, 0, 0, 0)` | Different API wrappers |

### 5.2 Namespace and Include Patterns

Quasar SFPU kernels use:
```cpp
namespace ckernel { namespace sfpu { ... } }
```
(Two separate namespace declarations, NOT `ckernel::sfpu`)

Includes:
```cpp
#include "ckernel_trisc_common.h"
#include "cmath_common.h"
```
(NOT `#include "llk_defs.h"`, `#include "lltt.h"`, `#include "sfpi.h"` which are Blackhole-specific)

### 5.3 Iteration Pattern

Quasar SFPU kernels use:
```cpp
TTI_SFPENCC(1, 2);  // enable conditional execution
#pragma GCC unroll 8
for (int d = 0; d < iterations; d++) {
    _calculate_op_sfp_rows_();
    ckernel::math::_incr_counters_<0x0, 0x0, ckernel::math::SFP_ROWS, 0x0>();
}
TTI_SFPENCC(0, 2);  // disable conditional execution
```

The `_incr_counters_` call increments the Dest RWC by `SFP_ROWS=2`, moving to the next pair of rows.

**IMPORTANT**: The "where" kernel is different from typical unary SFPU kernels because:
1. It reads from **multiple Dest tile locations** (3 inputs + 1 output)
2. It uses **explicit offsets** rather than `_incr_counters_` with `ADDR_MOD_7`
3. The Blackhole reference uses runtime-computed `TT_SFPLOAD`/`TT_SFPSTORE` (not `TTI_` immediate variants) because offsets are function parameters

### 5.4 Addr Mode Configuration

From `llk_math_eltwise_ternary_sfpu.h`:
```cpp
addr_mod_t {
    .srca = {.incr = 0},
    .srcb = {.incr = 0},
    .dest = {.incr = 0},
}
    .set(ADDR_MOD_7, csr_read<CSR::TRISC_ID>());
```

ADDR_MOD_7 is configured with zero increments. ADDR_MOD_6 is used in Blackhole for the store with a different increment -- need to check if Quasar needs similar configuration.

## 6. Data Format Support and Conversion Rules

### 6.1 Format Support Matrix (Comprehensive)

The "where" kernel is a **conditional select** operation -- it does NOT perform any mathematical computation on the data. It simply selects between two values based on a condition. Therefore, ALL formats that the SFPU can load and store are applicable.

| Format | Enum Value | Can SFPU Load? | Can SFPU Store? | Semantically Valid? | Applicable? | Notes |
|---|---|---|---|---|---|---|
| **Float32** | 0 | Yes (MOD0_FMT_FP32) | Yes | Yes | **YES** | 32-bit mode in Dest, requires dest_acc_en |
| **Tf32** | 4 | Yes (treated as FP32 in Dest) | Yes | Yes | **YES** | Stored as FP32 in Dest; Tf32 is just truncated FP32 |
| **Float16** | 1 | Yes (MOD0_FMT_FP16) | Yes | Yes | **YES** | 16-bit mode in Dest |
| **Float16_b** | 5 | Yes (MOD0_FMT_BF16) | Yes | Yes | **YES** | 16-bit mode in Dest |
| **MxFp8R** | 18 | Via unpack to Float16_b | Yes (packer converts) | Yes | **YES** | L1 only format; unpacked to Float16_b for SFPU; packer converts back |
| **MxFp8P** | 20 | Via unpack to Float16_b | Yes (packer converts) | Yes | **YES** | L1 only format; unpacked to Float16_b for SFPU; packer converts back |
| **Int32** | 8 | Yes (MOD0_FMT_INT32) | Yes | Yes | **YES** | 32-bit mode, sign-magnitude in Dest, requires dest_acc_en |
| **Int8** | 14 | Yes (MOD0_FMT_INT8) | Yes | Yes | **YES** | Stored as sign+magnitude with fixed exponent in Dest |
| **UInt8** | 17 | Yes (MOD0_FMT_UINT8) | Yes | Yes | **YES** | 16-bit container in Dest, zero-extended |
| **UInt16** | 130 | Yes (MOD0_FMT_UINT16) | Yes | Yes | **YES** | 16-bit opaque in Dest |
| **Int16** | 9 | Yes (MOD0_FMT_INT16) | Yes | Yes | **YES** | Sign-magnitude in 16-bit Dest container |

**ALL 11 Quasar-supported formats are applicable** for the "where" kernel since it performs only conditional selection with no arithmetic.

### 6.2 Format-Specific Constraints

1. **Float32 and Int32**: Require `dest_acc_en=true` because they use 32-bit Dest mode
2. **Tf32**: Stored as FP32 in Dest (32-bit container), also requires `dest_acc_en=true`
3. **MxFp8R and MxFp8P**: These are L1-only formats. The unpacker converts them to Float16_b when loading into registers. The packer converts Float16_b back to MX format when writing to L1. For SFPU, they appear as Float16_b.
4. **Integer formats (Int32, Int8, UInt8, Int16, UInt16)**: The SFPSETCC comparison needs bit 11 of imm12_math set to interpret as INT32 (but for EQ0 check, this may not matter since checking for zero works the same for FP and INT bit patterns in the sign-magnitude representation)
5. **Cross-exponent-family conversions**: If input is Float16 (exponent A) and output is Float16_b (exponent B), `dest_acc_en` is needed. But for "where", input and output formats are typically the same.

### 6.3 Dest Register Format Storage

From the Dest storage formats page:
- **FP16a**: `{sgn, man[9:0], exp[4:0]}` -- 16-bit
- **FP16b**: `{sgn, man[6:0], exp[7:0]}` -- 16-bit
- **FP32**: `{sgn, man[22:16], exp[7:0]}` MSBs + `{man[15:0]}` LSBs -- 32-bit (two physical rows)
- **Int8 signed**: `{sgn, 3'b0, mag[6:0], 5'd16}` -- sign-magnitude
- **Int8 unsigned**: `{3'b0, val[7:0], 5'd16}` -- unsigned
- **Int16 signed**: `{sgn, mag[14:0]}` -- sign-magnitude
- **Int16 unsigned**: `{val[15:0]}` -- direct
- **Int32 signed**: `{sgn, mag[22:16], mag[30:23]}` + `{mag[15:0]}` -- sign-magnitude, two rows

**Source**: Confluence page 80674824 (Dest storage formats)

### 6.4 SrcA/B Register Format Storage

19-bit containers. Supports: FP16a, FP16b, Tf19 (called Tf32), Int8, Int16, MXFP4_2x, UInt8.
Does NOT support: Int32, FP32, Int16 unsigned, BFP formats.

**Source**: Confluence page 83230723 (SrcA/B storage formats)

### 6.5 Unpacker Format Conversions (Relevant to "where")

For "where", data is unpacked to Dest (via `unpack_to_dest` path):
- Float32 -> Float32 (Dest only)
- TF32 -> TF32 (as Float32 container in Dest)
- Float16/Float16_b/FP8R/FP8P/MXFP8R/MXFP8P/MXFP6R/MXFP6P/MXINT8/MXINT4/MXINT2 -> Float16 or Float16_B
- INT32 -> INT32 (Dest only)
- INT8 -> INT8
- UINT8 -> UINT8
- INT16 -> INT16

**Source**: Confluence page 237174853 (Tensix Formats)

### 6.6 Packer Format Conversions

- Float32 -> all float and MX formats
- Float16/Float16_B -> Float16, Float16_B, FP8R, FP8P, MX formats
- INT32 -> INT32, INT8, UINT8
- INT8 -> INT8
- UINT8 -> UINT8
- INT16 -> INT16

**Source**: Confluence page 237174853 (Tensix Formats)

## 7. Pipeline Constraints and Instruction Ordering

### 7.1 SFPLOAD -> FPU Write Hazard
At least 3 unrelated Tensix instructions must execute between an FPU write to Dest and an SFPLOAD reading from the same Dest region. Use STALLWAIT or NOPs.

### 7.2 Conditional Execution Ordering
The CC enable/disable pattern must wrap the entire computation:
```
SFPENCC(1, 2)    // enable CC globally
loop:
  SFPLOAD          // load data
  SFPSETCC         // set CC based on condition
  ...conditional ops...  // only active on CC-set lanes
  SFPENCC(0, 0)    // reset CC within loop (all lanes active for next iter's load)
  SFPSTORE         // store result
  _incr_counters_  // advance to next rows
SFPENCC(0, 2)    // disable CC globally
```

### 7.3 SFPLOADMACRO Constraints
- Macros must be configured via SFPCONFIG before first SFPLOADMACRO
- InstructionTemplate[0-3] store the instructions to be triggered
- Sequence[0-3] configure the scheduling (which template to trigger at which pipeline stage)
- Misc register configures global LOADMACRO behavior

## 8. Blackhole vs Quasar Key Differences Summary

| Aspect | Blackhole | Quasar |
|---|---|---|
| Namespace | `ckernel::sfpu` (single declaration) | `ckernel { sfpu {` (nested) |
| Includes | `llk_defs.h`, `lltt.h`, `sfpi.h` | `ckernel_trisc_common.h`, `cmath_common.h` |
| SFPLOAD params | 4 | 5 (added `done`) |
| SFPSTORE params | 4 | 5 (added `done`) |
| SFPSETCC params | 4 | 3 (removed `lreg_dest`) |
| SFPENCC params | 4 | 2 (removed `lreg_c`, `lreg_dest`) |
| SFPLOADMACRO params | 4 (packed) | 7 (explicit) |
| SFPENCC constants | `sfpi::SFPENCC_MOD1_EU_R1` | `0` (for reset), `2` (for enable/disable) |
| SFPSETCC constants | `sfpi::SFPSETCC_MOD1_LREG_EQ0` | `0x6` (for EQ0) |
| Replay API | `lltt::record(start, len)` / `lltt::replay(start, len)` | `load_replay_buf<start, len>(lambda)` / `TTI_REPLAY(...)` |
| DataFormat enum | Uses `DataFormat::UInt32` | No `UInt32`; has `Int16` instead |
| `_calculate_where_` template | `<bool APPROX, DataFormat fmt, int ITER>` | Should follow Quasar test expectation |
| Static assert formats | Float32, Float16_b, Int32, UInt32 | Should expand to all Quasar formats |
| ITERATIONS model | `ITERATIONS` template param, internal loop | `iterations` function param, `_incr_counters_` |

## 9. Recommended Approach for Port

### Phase 1: Basic `_calculate_where_` (no SFPLOADMACRO)

Implement the DISABLE_SFPLOADMACRO path first, adapted for Quasar:

```cpp
// In _calculate_where_sfp_rows_, load condition, true, false from 3 different Dest locations
// Use TT_SFPLOAD (runtime offsets) not TTI_SFPLOAD (compile-time)
// Use TTI_SFPSETCC(0, p_sfpu::LREG0, 0x6) for "CC where LREG0 == 0"
// Use TTI_SFPENCC(0, 0) to reset CC
// Use TT_SFPSTORE with runtime offset
```

Key implementation decisions:
1. The Quasar test harness calls `_calculate_where_<false, static_cast<DataFormat>(UNPACK_A_IN), iterations>(0, 1, 2, 0)` -- same signature as Blackhole
2. Use `p_sfpu::sfpmem::DEFAULT` for format mode (let hardware resolve based on SrcB config), OR map the DataFormat template param to the correct sfpmem value
3. The `done=0` parameter for SFPLOAD/SFPSTORE since we're targeting Dest (not SrcS)

### Phase 2: `_init_where_` + SFPLOADMACRO optimization

Port the SFPLOADMACRO-based optimization with Quasar's 7-param SFPLOADMACRO API.

## 10. Verification Checklist

- [ ] SFPLOAD/SFPSTORE have 5 params on Quasar (with `done`)
- [ ] SFPSETCC has 3 params on Quasar (no `lreg_dest`)
- [ ] SFPENCC has 2 params on Quasar (no `lreg_c`, `lreg_dest`)
- [ ] Use `ckernel { namespace sfpu {` not `ckernel::sfpu`
- [ ] Include `ckernel_trisc_common.h` and `cmath_common.h`
- [ ] All 11 Quasar data formats are applicable (universal select operation)
- [ ] 32-bit formats (Float32, Int32, Tf32) require `dest_acc_en`
- [ ] MX formats (MxFp8R, MxFp8P) are unpacked to Float16_b before reaching SFPU
