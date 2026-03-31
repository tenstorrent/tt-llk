# Analysis: where

## Kernel Type
sfpu

## Reference File
`tt_llk_blackhole/common/inc/sfpu/ckernel_sfpu_where.h`

## Purpose
The "where" kernel implements a ternary conditional select operation:
```
result[i] = (condition[i] == 0) ? false_value[i] : true_value[i]
```
It reads from three different Dest tile locations (condition, true_value, false_value) and writes the selected result to a fourth location (which may overlap with the condition input).

This is a pure data-selection operation -- no arithmetic is performed on the values. It selects between two values element-wise based on a condition being zero or non-zero.

## Algorithm Summary
1. Load condition from Dest tile `dst_index_in0` into an LReg
2. Load true_value from Dest tile `dst_index_in1` into another LReg
3. Set the per-lane condition code (CC) where condition == 0
4. Load false_value from Dest tile `dst_index_in2` -- this overwrites LREG1 ONLY in CC-set lanes (where condition was zero), leaving true_value in non-CC lanes
5. Reset CC (re-enable all lanes)
6. Store the result (which is now a mix of true/false values per lane) to Dest tile `dst_index_out`

The key insight is that steps 3-4 use conditional execution: after SFPSETCC marks lanes where condition==0, the subsequent SFPLOAD to LREG1 only writes to those marked lanes. So LREG1 ends up with `true_value` where condition!=0 and `false_value` where condition==0.

## Template Parameters
| Parameter | Purpose | Values |
|-----------|---------|--------|
| `APPROXIMATION_MODE` | Unused placeholder for API consistency | `true` / `false` (always `false` in test) |
| `data_format` | Controls SFPLOAD/SFPSTORE format mode (LO16 vs INT32) | `DataFormat::Float16_b`, `DataFormat::Float32`, `DataFormat::Int32`, `DataFormat::UInt32` (Blackhole) |
| `ITERATIONS` | Number of loop iterations over Dest rows | `32` in test (covers full 32x32 tile via internal loop) |

## Functions Identified
| Function | Purpose | Complexity |
|----------|---------|------------|
| `_calculate_where_<APPROXIMATION_MODE, data_format, ITERATIONS>` | Main compute: conditional select across 3 input tiles into 1 output tile | Medium |
| `_init_where_<APPROXIMATION_MODE>` | Configures SFPLOADMACRO macros for pipelined optimization (under `#ifndef DISABLE_SFPLOADMACRO`) | Medium |

## Key Constructs Used
- **TT_SFPLOAD (runtime address)**: Loads 32 datums from a specific Dest row into an LReg. Uses runtime-computed offsets (not `TTI_SFPLOAD`) because the tile offsets come from function parameters. Blackhole signature: `TT_SFPLOAD(lreg, mod0, addr_mode, dest_addr)` (4 params).
- **TT_SFPSTORE (runtime address)**: Stores 32 datums from an LReg to a specific Dest row. Blackhole signature: `TT_SFPSTORE(lreg, mod0, addr_mode, dest_addr)` (4 params).
- **TTI_SFPSETCC**: Sets per-lane CC based on LReg value. Used with `SFPSETCC_MOD1_LREG_EQ0` mode to mark lanes where condition==0. Blackhole signature: `TTI_SFPSETCC(imm12, lreg_c, lreg_dest, mod1)` (4 params).
- **TTI_SFPENCC**: Resets CC to re-enable all lanes. Blackhole uses `TTI_SFPENCC(0, 0, 0, sfpi::SFPENCC_MOD1_EU_R1)` (4 params).
- **InstrModLoadStore::LO16 / INT32**: Format mode for SFPLOAD/SFPSTORE. Float16_b uses LO16 (value 6), Float32/Int32/UInt32 use INT32 (value 4).
- **Offset computation**: `(dst_index * 32) << 1` -- each tile is 32 rows in Dest, left-shift by 1 for the addressing mode.
- **ADDR_MOD_7**: Address mode with zero increments (no auto-increment). Used for all loads.
- **ADDR_MOD_6**: Address mode with dest increment of 2 (used only in Blackhole SFPSTORE for auto-increment). Configured in `eltwise_ternary_sfpu_configure_addrmod<SfpuType::where>`.
- **lltt::record / lltt::replay**: Replay buffer for loop optimization. Records 6 instructions and replays them ITERATIONS times.
- **SFPLOADMACRO**: Pipelined load+compute+store for optimized path. Two variants: 3-cycle (dst_out==dst_in0) and 4-cycle (dst_out!=dst_in0). Blackhole signature: `TT_SFPLOADMACRO(macro_id_packed, mod0, addr_mode, dest_addr)` (4 params).
- **SFPCONFIG / SFPLOADI**: Configure LOADMACRO instruction templates and sequences in `_init_where_`.
- **`#ifdef DISABLE_SFPLOADMACRO`**: Conditional compilation separating simple vs optimized paths.
- **`#pragma GCC unroll 8`**: Loop unrolling hint.

## Dependencies
- `llk_defs.h` -- DataFormat enum, InstrModLoadStore enum
- `lltt.h` -- Replay buffer wrappers (lltt::record, lltt::replay)
- `sfpi.h` -- SFPI constants (SFPSETCC_MOD1_LREG_EQ0, SFPENCC_MOD1_EU_R1, SFPLOADI_MOD0_LOWER/UPPER)
- `ckernel_sfpu.h` -- Parent file that `#includes` this kernel
- `llk_math_eltwise_ternary_sfpu.h` -- Ternary SFPU framework (init, start, done, addr_mod config)
- `llk_math_eltwise_ternary_sfpu_params.h` -- Higher-level wrapper that calls `_calculate_where_` per face
- `ckernel_template.h` -- Not directly used but part of the ternary infrastructure

## Complexity Classification
**Medium**

The where kernel is conceptually simple (conditional select), but the implementation has two complexities:
1. It operates on **three input tiles + one output tile**, unlike typical unary SFPU kernels that operate on a single tile in-place. This requires explicit offset management.
2. The optimized path uses SFPLOADMACRO with pipelined scheduling, which requires careful macro configuration.

The basic (DISABLE_SFPLOADMACRO) path is straightforward to port. The SFPLOADMACRO optimization path requires understanding Quasar's 7-param SFPLOADMACRO API and SFPCONFIG programming model.

## Constructs Requiring Translation

### 1. SFPLOAD/SFPSTORE parameter count
- **Blackhole**: `TT_SFPLOAD(lreg, mod0, addr_mode, dest_addr)` -- 4 params
- **Quasar**: `TT_SFPLOAD(lreg, mod0, addr_mode, done, dest_addr)` -- 5 params, `done=0` for Dest access
- Same change for TT_SFPSTORE.

### 2. SFPSETCC parameter count
- **Blackhole**: `TTI_SFPSETCC(imm12, lreg_c, lreg_dest, mod1)` -- 4 params
- **Quasar**: `TTI_SFPSETCC(imm12, lreg_c, mod1)` -- 3 params (no lreg_dest)
- `sfpi::SFPSETCC_MOD1_LREG_EQ0` maps to `0x6` on Quasar

### 3. SFPENCC parameter count
- **Blackhole**: `TTI_SFPENCC(imm12, lreg_c, lreg_dest, mod1)` -- 4 params
- **Quasar**: `TTI_SFPENCC(imm12, mod1)` -- 2 params (no lreg_c, lreg_dest)
- `sfpi::SFPENCC_MOD1_EU_R1` (reset CC) maps to `TTI_SFPENCC(0, 0)` on Quasar

### 4. InstrModLoadStore enum
- **Blackhole**: Uses `InstrModLoadStore::LO16` (6) and `InstrModLoadStore::INT32` (4)
- **Quasar**: Uses `p_sfpu::sfpmem::FP16B` (2) for Float16_b, `p_sfpu::sfpmem::INT32` (4) for 32-bit formats, or `p_sfpu::sfpmem::DEFAULT` (0) for auto-resolve

### 5. Namespace and includes
- **Blackhole**: `namespace ckernel::sfpu`, includes `llk_defs.h`, `lltt.h`, `sfpi.h`
- **Quasar**: `namespace ckernel { namespace sfpu {`, includes `ckernel_trisc_common.h`, `cmath_common.h`

### 6. sfpi:: constants
- **Blackhole**: `sfpi::SFPSETCC_MOD1_LREG_EQ0`, `sfpi::SFPENCC_MOD1_EU_R1`, `sfpi::SFPLOADI_MOD0_LOWER`, `sfpi::SFPLOADI_MOD0_UPPER`
- **Quasar**: No `sfpi::` namespace; use raw numeric values (`0x6` for EQ0, `0` for SFPENCC reset, `2` for SFPLOADI UINT16 mode)

### 7. Replay buffer API
- **Blackhole**: `lltt::record(start, len)` + `lltt::replay(start, len)`
- **Quasar**: `load_replay_buf<start, len, exec_while_loading, set_mutex, last>(lambda)` + `TTI_REPLAY(start, len, 0, 0, 0, 0)` for playback

### 8. SFPLOADMACRO API
- **Blackhole**: `TT_SFPLOADMACRO((macro_id << 2), mod0, addr_mode, dest_addr)` -- 4 params, macro_id packed into lreg field
- **Quasar**: `TT_SFPLOADMACRO(seq_id, lreg_ind_lo, mod0, addr_mode, done, dest_addr, lreg_ind_hi)` -- 7 params, seq_id and lreg separate

### 9. ADDR_MOD_6 configuration
- **Blackhole**: `eltwise_ternary_sfpu_configure_addrmod<SfpuType::where>` configures ADDR_MOD_6 with `.dest = {.incr = 2}` for auto-increment store
- **Quasar**: The existing `_eltwise_ternary_sfpu_configure_addrmod_()` only configures ADDR_MOD_7 with zero increments. ADDR_MOD_6 would need to be configured separately if needed.

### 10. DataFormat::UInt32 (Blackhole-only)
- **Blackhole**: `static_assert` allows `DataFormat::UInt32`
- **Quasar**: No `DataFormat::UInt32` in the enum; has `Int16` instead. The static_assert needs to be updated.

### 11. Iteration model difference
- **Blackhole**: `ITERATIONS` is a template parameter; the internal loop replays a fixed instruction sequence. No `_incr_counters_` call -- offsets are explicit and fixed.
- **Quasar**: The "where" kernel is unique because it uses **explicit offsets** rather than `_incr_counters_`. Unlike typical Quasar SFPU kernels that load from current Dest row and auto-increment, "where" loads from 3 different tile offsets. The Quasar test harness calls `_calculate_where_<false, data_format, iterations>(0, 1, 2, 0)` with `iterations=32`, matching the Blackhole pattern exactly.

## Target Expected API

### From the Test Harness (`tests/sources/ttnn_where_test.cpp`)

The test harness has **no `#ifdef ARCH_QUASAR` branch** -- it uses the same code for both architectures with `#ifdef ARCH_BLACKHOLE` for specific differences. The math section calls:

```cpp
// Under #ifdef LLK_TRISC_MATH:
#include "ckernel_sfpu.h"
#include "ckernel_sfpu_where.h"
#include "llk_math_eltwise_ternary_sfpu.h"

_llk_math_eltwise_ternary_sfpu_init_<SfpuType::where>();
ckernel::sfpu::_init_where_<false>();
_llk_math_eltwise_ternary_sfpu_start_<DstSync::SyncHalf>(0);
ckernel::sfpu::_calculate_where_<false, static_cast<DataFormat>(UNPACK_A_IN), iterations>(0, 1, 2, 0);
_llk_math_eltwise_ternary_sfpu_done_();
```

Expected function signatures:
1. `_init_where_<false>()` -- no-arg init, template param `APPROXIMATION_MODE`
2. `_calculate_where_<false, DataFormat, 32>(uint32_t, uint32_t, uint32_t, uint32_t)` -- template params: APPROXIMATION_MODE, data_format, ITERATIONS; runtime params: 4x dst_index

The `#else` (non-Blackhole, i.e., Quasar) branches in the test differ from Blackhole only in:
- `_llk_math_eltwise_unary_datacopy_init_` has 4 template params (no 5th `false` param)
- `_llk_pack_hw_configure_` has 2 template params (no 3rd `false` param)
- `_llk_pack_dest_init_` has 3 template params (extra `false` param)

The SFPU kernel itself (`_init_where_` and `_calculate_where_`) is called **identically** across architectures.

### From the Parent File (`tt_llk_quasar/common/inc/ckernel_sfpu.h`)

The parent file already has `#include "sfpu/ckernel_sfpu_where.h"` -- so the include is already in place. The file does NOT exist yet (confirmed: NOT_FOUND).

### From the Ternary Framework (`tt_llk_quasar/llk_lib/llk_math_eltwise_ternary_sfpu.h`)

The Quasar ternary framework provides:
- `_llk_math_eltwise_ternary_sfpu_init_<SfpuType>()` -- configures ADDR_MOD_7 (zero increments) and resets counters
- `_llk_math_eltwise_ternary_sfpu_start_<DstSync>(dst_index)` -- sets dest write addr and issues STALLWAIT
- `_llk_math_eltwise_ternary_sfpu_done_()` -- resets D counters

Note: The Quasar ternary framework does NOT have:
- `_llk_math_eltwise_ternary_sfpu_uninit_()`
- ADDR_MOD_6 configuration for where-specific auto-increment store
- `_llk_math_eltwise_ternary_sfpu_inc_dst_face_addr_()`

### Template params to KEEP from reference
- `APPROXIMATION_MODE` (bool) -- used in template signature
- `data_format` (DataFormat) -- controls SFPLOAD/SFPSTORE format mode
- `ITERATIONS` (int) -- controls loop count

### Template params to DROP (reference-only)
- None

### Template params to ADD (target-only)
- None

### Reference features NOT in target (to DROP)
- `DataFormat::UInt32` in static_assert (Quasar has no UInt32)
- `InstrModLoadStore::LO16` / `InstrModLoadStore::INT32` enum (use `p_sfpu::sfpmem::` constants instead)
- `sfpi::SFPSETCC_MOD1_LREG_EQ0`, `sfpi::SFPENCC_MOD1_EU_R1` constants
- `lltt::record` / `lltt::replay` API (use `load_replay_buf` + `TTI_REPLAY` instead)
- `ADDR_MOD_6` with dest.incr=2 (not configured in Quasar ternary framework; may need separate setup)

## Format Support

### Format Domain
**Universal** -- The "where" operation is a conditional select (if condition==0 then false_value else true_value). No arithmetic is performed on data values. It is valid for ALL data types that the SFPU can load and store.

### Applicable Formats for Testing

Starting from ALL Quasar-supported formats (QUASAR_DATA_FORMAT_ENUM_VALUES):

| Format | Applicable | Rationale |
|--------|-----------|-----------|
| Float32 | Yes | SFPU can load/store FP32 (sfpmem::FP32). Requires dest_acc=Yes for 32-bit Dest mode. |
| Tf32 | Yes | Stored as FP32 in Dest; SFPU treats it identically to FP32. Requires dest_acc=Yes. |
| Float16 | Yes | SFPU can load/store FP16A (sfpmem::FP16A). 16-bit Dest mode. |
| Float16_b | Yes | SFPU can load/store FP16B (sfpmem::FP16B). 16-bit Dest mode. Primary test format. |
| Int32 | Yes | SFPU can load/store INT32 (sfpmem::INT32). Requires dest_acc=Yes for 32-bit Dest. Zero-check works on sign-magnitude representation. |
| Int16 | Yes | Quasar-specific format. SFPU arch research confirms SFPLOAD supports INT16 mode (value 8). The SFPSETCC EQ0 check works on any bit pattern (zero == all-bits-zero). |
| Int8 | Yes | SFPU can load/store INT8 (sfpmem::UINT8 value 5 per arch research). Zero-check works. |
| UInt8 | Yes | SFPU can load/store UINT8 (sfpmem::UINT8). Zero-check works. |
| UInt16 | Yes | SFPU can load/store UINT16 (sfpmem::UINT16). Zero-check works. |
| MxFp8R | Yes | L1-only format; unpacked to Float16_b by hardware. SFPU sees Float16_b in Dest. Packer converts back to MxFp8R for L1. |
| MxFp8P | Yes | L1-only format; unpacked to Float16_b by hardware. SFPU sees Float16_b in Dest. Packer converts back to MxFp8P for L1. |

**All 11 Quasar-supported formats are applicable.** The Blackhole reference's static_assert restricting to {Float32, Float16_b, Int32, UInt32} reflects Blackhole's limitations, not fundamental operation constraints.

### Format-Dependent Code Paths

The reference has one format-dependent path:

```cpp
constexpr std::uint32_t mod0 = data_format == DataFormat::Float16_b ? InstrModLoadStore::LO16 : InstrModLoadStore::INT32;
```

This selects the SFPLOAD/SFPSTORE instruction mode:
- Float16_b: Use LO16 mode (loads/stores 16-bit bfloat values)
- Everything else (Float32, Int32, UInt32): Use INT32 mode (loads/stores 32-bit values)

On Quasar, this translates to:
- Float16_b: Use `p_sfpu::sfpmem::FP16B` (value 2) -- or `p_sfpu::sfpmem::DEFAULT` (value 0) if SrcB format config is set correctly
- Float16: Use `p_sfpu::sfpmem::FP16A` (value 1)
- Float32/Tf32: Use `p_sfpu::sfpmem::FP32` (value 3) or `p_sfpu::sfpmem::INT32` (value 4)
- Int32: Use `p_sfpu::sfpmem::INT32` (value 4)
- Int8/UInt8: Use `p_sfpu::sfpmem::UINT8` (value 5) -- note: Quasar uses same constant for signed and unsigned 8-bit
- Int16/UInt16: Use `p_sfpu::sfpmem::UINT16` (value 6)
- MxFp8R/MxFp8P: Appear as Float16_b in Dest, use `p_sfpu::sfpmem::FP16B` or `DEFAULT`

**However**, the simplest approach is `p_sfpu::sfpmem::DEFAULT` (value 0) which auto-resolves based on the SrcB format configuration register. This is the pattern used by most existing Quasar SFPU kernels (sign, threshold, hardtanh, fill, lrelu). The auto-resolve approach eliminates the need for format-dependent code paths entirely.

### Format Constraints

Hardware constraints affecting format combinations for this kernel:
1. **Float32 and Int32**: Require `dest_acc=Yes` because they use 32-bit Dest mode
2. **Tf32**: Stored as FP32 in Dest, requires `dest_acc=Yes`
3. **MxFp8R and MxFp8P**: L1-only formats; unpacker converts to Float16_b. Require `implied_math_format=Yes` in test infrastructure. In Dest they appear as Float16_b.
4. **Cross-exponent-family conversions**: If input is Float16 (exponent A) and output is Float16_b (exponent B), `dest_acc=Yes` is needed. But for "where", input and output formats should typically match.
5. **Int16 with dest_acc=Yes**: Not supported per `data_format_inference.py` line 209 (raises ValueError). Int16 must use 16-bit Dest mode.
6. **32-bit output with dest_acc=No**: Quasar packer does not support this (raises ValueError in `infer_pack_in`).

### Reference Test Format Coverage

The existing test (`test_zzz_ttnn_where.py`) tests:
- Float16_b (with dest_acc=No)
- Float32 (with dest_acc=Yes)
- Int32 (with dest_acc=Yes)

These are the same 3 formats from the Blackhole static_assert (minus UInt32 which Quasar lacks). A comprehensive Quasar test should expand to all applicable formats.

## Existing Target Implementations

### Most Similar Existing Quasar Kernels

1. **`ckernel_sfpu_sign.h`** -- Uses conditional execution (SFPSETCC + SFPENCC pattern) with TTI_ (immediate) load/store. Pattern: load from current dest row, test condition, conditionally modify, store back. Uses `_incr_counters_` for iteration.

2. **`ckernel_sfpu_threshold.h`** -- Conditional replacement using SFPGT for comparison. Similar CC enable/disable pattern. Uses SFPMOV for conditional copy.

3. **`ckernel_sfpu_hardtanh.h`** -- Multiple conditional checks (upper/lower clamp) using SFPGT + SFPMOV. Good example of multi-condition logic.

4. **`ckernel_sfpu_fill.h`** -- Simplest pattern: just store a value to Dest. Shows integer format handling with explicit `store_mode` template param.

### Key Pattern Differences: "where" vs typical Quasar SFPU kernels

The "where" kernel is **atypical** compared to existing Quasar SFPU kernels because:

1. **Multi-tile access**: It reads from 3 different Dest tile locations (offsets 0, 1, 2) and writes to a 4th (offset 0). Typical Quasar kernels read/write the current row via ADDR_MOD_7 auto-increment.

2. **Runtime TT_SFPLOAD (not TTI_SFPLOAD)**: Because offsets are function parameters, the kernel must use `TT_SFPLOAD` (runtime address in instrn_buffer) not `TTI_SFPLOAD` (immediate/compile-time address).

3. **No `_incr_counters_`**: The Blackhole reference does NOT use `_incr_counters_`. Instead, it uses a replay buffer that replays the full load-setcc-load-encc-store sequence. The ADDR_MOD_7 (configured with zero increment) means the same offsets are reused each replay iteration. The `ITERATIONS` template parameter controls how many rows are processed, but the offsets are fixed per face.

4. **The test harness calls `_calculate_where_` per-face** via `_llk_math_eltwise_ternary_sfpu_params_` (Blackhole) or directly (the Quasar test calls it with `iterations=32`). The ITERATIONS=32 covers all 32 rows within a face (each iteration processes one row via replay).

### Quasar Ternary Framework

The existing `tt_llk_quasar/llk_lib/llk_math_eltwise_ternary_sfpu.h` provides the infrastructure:
- ADDR_MOD_7 configured with zero increments (no auto-advance) -- matches what "where" needs
- `_llk_math_eltwise_ternary_sfpu_init_<SfpuType::where>()` -- already handles SfpuType::where
- `_llk_math_eltwise_ternary_sfpu_start_<DstSync>()` -- sets dest write addr
- `_llk_math_eltwise_ternary_sfpu_done_()` -- resets counters

Note: Unlike Blackhole, Quasar's ternary framework does NOT configure ADDR_MOD_6. If the optimized SFPLOADMACRO path needs an auto-increment store address mode, that configuration must be added (either in the ternary framework or in `_init_where_`).

## Sub-Kernel Phases

The "where" kernel has a clear two-phase structure based on the `#ifdef DISABLE_SFPLOADMACRO` conditional compilation:

| Phase | Name | Functions | Dependencies |
|-------|------|-----------|--------------|
| 1 | Basic where (no SFPLOADMACRO) | `_calculate_where_<APPROXIMATION_MODE, data_format, ITERATIONS>`, `_init_where_<APPROXIMATION_MODE>` | none |
| 2 | SFPLOADMACRO optimization | Updated `_calculate_where_` (SFPLOADMACRO path), updated `_init_where_` (macro config) | Phase 1 |

**Phase 1** implements the DISABLE_SFPLOADMACRO path: straightforward load-setcc-load-encc-store sequence using a replay buffer. The `_init_where_` in phase 1 is a no-op (or minimal CC enable/disable setup) since SFPLOADMACRO is not used.

**Phase 2** adds the SFPLOADMACRO optimization: configures instruction templates and macro sequences in `_init_where_`, then uses `TT_SFPLOADMACRO` in `_calculate_where_` for pipelined execution.

**Ordering rationale**: Phase 1 must come first because:
- The basic path is simpler and provides a working baseline for testing
- Phase 2 modifies both functions, but the core algorithm tested in phase 1 remains the same
- If SFPLOADMACRO optimization fails, the phase 1 implementation is still correct

**Note on phase 1 structure**: Unlike typical Quasar SFPU kernels, "where" does NOT split into `_calculate_where_sfp_rows_` + outer `_calculate_where_` loop. The reason is that the entire load-compare-load-store sequence operates on explicit offsets within a replay buffer, not on the "current row" via auto-increment. The replay handles iteration. However, for consistency with Quasar patterns, the Quasar port COULD factor out the per-row logic, but this is an implementation choice for the planner, not a structural requirement.
