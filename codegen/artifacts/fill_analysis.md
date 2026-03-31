# Analysis: fill

## Kernel Type
sfpu

## Reference File
`tt_llk_blackhole/common/inc/sfpu/ckernel_sfpu_fill.h`

## Purpose
The fill kernel writes a constant value to every element in destination register tiles. It is the SFPU equivalent of `memset` -- given a value, it broadcasts that value to all elements across all iterations (rows/faces). This is used for initializing tiles with a known constant (zero, one, arbitrary value).

## Algorithm Summary
1. **Float fill (`_calculate_fill_`)**: Load a float constant into all SFPU lanes, store to every destination row, advancing the destination pointer after each iteration.
2. **Integer fill (`_calculate_fill_int_`)**: Load an integer constant (either 32-bit or 16-bit) into an LREG, then store to every destination row with an explicit integer format mode.
3. **Bitcast fill (`_calculate_fill_bitcast_`)**: Reinterpret a 32-bit unsigned integer bit pattern as a float, then fill destination rows as with the float variant. Useful for writing specific bit patterns (e.g., NaN, special encodings) to float tiles.

## Template Parameters (Reference / Blackhole)

| Parameter | Purpose | Values |
|-----------|---------|--------|
| `APPROXIMATION_MODE` (bool) | Unused in fill -- present for API consistency | true/false |
| `ITERATIONS` (int) | Number of dest rows to fill (compile-time constant) | Typically 8 for a 32x32 tile (4 faces x 2 rows) |
| `INSTRUCTION_MODE` (InstrModLoadStore) | Integer store format mode (`_calculate_fill_int_` only) | `INT32` (4) or `LO16` (6) |

## Functions Identified

| Function | Purpose | Complexity |
|----------|---------|------------|
| `_calculate_fill_` | Fill destination with a float constant | Low |
| `_calculate_fill_int_` | Fill destination with an integer constant (32-bit or 16-bit) | Medium |
| `_calculate_fill_bitcast_` | Fill destination with a bitcast float from uint32 bit pattern | Low-Medium |

## Key Constructs Used

- **`sfpi::vFloat fill_val = value`**: Loads a float constant into all SFPU lanes via the sfpi abstraction layer. On Quasar this must be done with `TTI_SFPLOADI` using FP16_B mode (instr_mod0=0), which auto-converts FP16_b to FP32 in the LREG.
- **`sfpi::dst_reg[0] = fill_val`**: Stores the LREG value to the current destination row. On Quasar this is `TTI_SFPSTORE` with implied format (instr_mod0 = `p_sfpu::sfpmem::DEFAULT` = 0).
- **`sfpi::dst_reg++`**: Advances the destination pointer by one row pair. On Quasar this is `ckernel::math::_incr_counters_<0x0, 0x0, ckernel::math::SFP_ROWS, 0x0>()`.
- **`Converter::as_float(value_bit_mask)`**: Union-based reinterpretation of uint32 bits as a float. Does not exist on Quasar; must load the raw 32 bits via two `TTI_SFPLOADI` calls (HI16_ONLY + LO16_ONLY modes).
- **`_sfpu_load_imm32_(dest, value)`**: Loads a 32-bit immediate via two SFPLOADI calls (LO16_ONLY then HI16_ONLY). Does NOT exist on Quasar as a named helper; must be inlined using `TTI_SFPLOADI(lreg, 10, lower16)` and `TTI_SFPLOADI(lreg, 8, upper16)`.
- **`_sfpu_load_imm16_(dest, value)`**: Loads a 16-bit immediate as unsigned integer via `TT_SFPLOADI(dest, 2, val)`. Does NOT exist on Quasar; must be inlined as `TTI_SFPLOADI(lreg, 2, val)`.
- **`InstrModLoadStore` enum**: Blackhole-specific enum defining SFPLOAD/SFPSTORE format modes. Does NOT exist on Quasar. The equivalent are the `p_sfpu::sfpmem::*` constants (e.g., `p_sfpu::sfpmem::INT32 = 0b0100`, `p_sfpu::sfpmem::UINT16 = 0b0110`).
- **`TTI_SFPSTORE(lreg, mode, addr_mod, dest_addr)`**: Blackhole has 4-arg version. Quasar has 5-arg version: `TTI_SFPSTORE(lreg, mode, addr_mod, done, dest_addr)` with an extra `done` parameter (0 for normal operation).

## Dependencies

### Reference (Blackhole)
- `ckernel_ops.h` -- TTI macros
- `ckernel_sfpu_converter.h` -- `Converter::as_float()` (bitcast)
- `ckernel_sfpu_load_config.h` -- `_sfpu_load_imm32_()`, `_sfpu_load_imm16_()`
- `p_sfpu::LREG1` -- register constants
- `InstrModLoadStore` enum from `llk_defs.h`
- `ADDR_MOD_3` -- address modifier used by Blackhole fill_int

### Target (Quasar) -- what will be needed
- `ckernel_trisc_common.h` -- standard Quasar SFPU include
- `cmath_common.h` -- provides `_incr_counters_`, `_sfpu_load_config32_`, `SFP_ROWS`, `p_sfpu` constants
- `<cstdint>` -- for `std::uint32_t` parameter type
- `p_sfpu::sfpmem::*` constants from `ckernel_instr_params.h` (included transitively)
- `ADDR_MOD_7` -- the standard addr mod for Quasar SFPU store operations

## Complexity Classification
**Simple**

The fill kernel is algorithmically trivial: load a constant, store it repeatedly. The only complexity is:
1. Adapting the Blackhole sfpi:: abstraction to Quasar TTI_ macros (well-established pattern)
2. Handling the integer fill variant without the `InstrModLoadStore` enum
3. Handling the bitcast variant without `Converter::as_float`

All required instructions (`SFPLOADI`, `SFPSTORE`, `SFPNOP`) are confirmed available on Quasar. Multiple existing Quasar kernels demonstrate the exact patterns needed.

## Constructs Requiring Translation

| Reference Construct | What it does | Quasar Equivalent |
|---------------------|-------------|-------------------|
| `sfpi::vFloat fill_val = value` | Load float constant to all lanes | `TTI_SFPLOADI(p_sfpu::LREG1, 0, (upper16_of_fp16b_value))` |
| `sfpi::dst_reg[0] = fill_val` | Store LREG to dest row | `TTI_SFPSTORE(p_sfpu::LREG1, p_sfpu::sfpmem::DEFAULT, ADDR_MOD_7, 0, 0)` |
| `sfpi::dst_reg++` | Advance dest pointer | `ckernel::math::_incr_counters_<0x0, 0x0, ckernel::math::SFP_ROWS, 0x0>()` |
| `Converter::as_float(val)` | Bitcast uint32 to float | Two `TTI_SFPLOADI` calls: `(lreg, 10, val & 0xFFFF)` then `(lreg, 8, (val >> 16) & 0xFFFF)` |
| `_sfpu_load_imm32_(dest, val)` | Load 32-bit immediate | Two `TTI_SFPLOADI` calls (same as above) |
| `_sfpu_load_imm16_(dest, val)` | Load 16-bit unsigned immediate | `TTI_SFPLOADI(lreg, 2, val)` (instr_mod0=2 = UINT16 mode) |
| `InstrModLoadStore::INT32` | SFPSTORE format = 4 | `p_sfpu::sfpmem::INT32` (= 0b0100 = 4) |
| `InstrModLoadStore::LO16` | SFPSTORE format = 6 | `p_sfpu::sfpmem::UINT16` (= 0b0110 = 6) |
| `ADDR_MOD_3` in SFPSTORE | Address modifier | `ADDR_MOD_7` (standard Quasar SFPU addr mod, configured to {.srca=0,.srcb=0,.dest=0}) |
| `TTI_SFPSTORE(lreg, mod, addr, dest)` (4 args) | Blackhole SFPSTORE | `TTI_SFPSTORE(lreg, mod, addr, done, dest)` (5 args, done=0) |
| `namespace ckernel::sfpu` | C++17 nested namespace | `namespace ckernel { namespace sfpu {` (C++11 style) |
| Template params `<APPROXIMATION_MODE, ITERATIONS>` | Compile-time iteration count | Runtime parameter `const int iterations` |

## Target Expected API

### From Step 1.5a: Test Harness
No existing test file found for fill on any architecture. Tests will need to be created by the test-writer agent.

### From Step 1.5b: Parent/Caller File
The Quasar `ckernel_sfpu.h` does NOT currently include `sfpu/ckernel_sfpu_fill.h`. It will need to be added. The parent file is at `tt_llk_quasar/common/inc/ckernel_sfpu.h`.

The `llk_math_eltwise_unary_sfpu_common.h` provides the wrapper `_llk_math_eltwise_unary_sfpu_params_` which calls SFPU functions with signature `void func(const int iterations, ...)`.

### From Step 1.5c: Closest Existing Target Kernels

**ckernel_sfpu_abs.h** (simplest read-modify-write):
- Includes: `ckernel_trisc_common.h`, `cmath_common.h`
- Namespace: `namespace ckernel { namespace sfpu {`
- Inner function: `inline void _calculate_abs_sfp_rows_()` (no template)
- Outer function: `inline void _calculate_abs_(const int iterations)` with `#pragma GCC unroll 8` and `_incr_counters_` pattern
- Uses `ADDR_MOD_7` for SFPSTORE and SFPLOAD
- Uses `p_sfpu::sfpmem::DEFAULT` for implied format

**ckernel_sfpu_lrelu.h** (has constant loading):
- Includes: `<cstdint>`, `ckernel_trisc_common.h`, `cmath_common.h`
- Shows how to load a float constant: `TTI_SFPLOADI(p_sfpu::LREG2, 0 /*Float16_b*/, (slope >> 16))`
- The value parameter is `const std::uint32_t slope` (upper 16 bits used as FP16_b)

**ckernel_sfpu_typecast_int32_fp32.h** (integer SFPLOAD/SFPSTORE):
- Uses `p_sfpu::sfpmem::INT32` for SFPLOAD and `p_sfpu::sfpmem::FP32` for SFPSTORE

**ckernel_sfpu_typecast_fp16b_uint16.h** (integer SFPSTORE):
- Uses `p_sfpu::sfpmem::UINT16` for SFPSTORE

### Function Signatures the Target Expects

Based on the established Quasar pattern:
1. `inline void _calculate_fill_(const int iterations, const std::uint32_t value)` -- float fill
2. `inline void _calculate_fill_int_(const int iterations, const std::uint32_t value, const std::uint32_t store_mode)` -- integer fill (store_mode replaces the template `InstrModLoadStore`)
3. `inline void _calculate_fill_bitcast_(const int iterations, const std::uint32_t value_bit_mask)` -- bitcast fill

### Template Parameters
- **KEEP from reference**: None (Quasar SFPU kernels use runtime parameters)
- **DROP (reference-only)**:
  - `APPROXIMATION_MODE` -- not used by fill, and Quasar SFPU kernels don't template on this
  - `ITERATIONS` -- becomes runtime `const int iterations`
  - `InstrModLoadStore INSTRUCTION_MODE` -- becomes runtime parameter `const std::uint32_t store_mode`
- **ADD (target-only)**: None expected

### Reference Features NOT Present on Target (DROP)
- `sfpi::` abstraction layer (vFloat, dst_reg, etc.)
- `Converter::as_float()` helper
- `_sfpu_load_imm32_()` / `_sfpu_load_imm16_()` helpers
- `InstrModLoadStore` enum
- `ADDR_MOD_3` usage (Quasar uses `ADDR_MOD_7`)

## Format Support

### Format Domain
**Universal** -- Fill writes a constant value to all elements; it is not a mathematical operation. It is applicable to both float and integer data formats since it just loads an immediate and stores it. The appropriate fill variant (_calculate_fill_ vs _calculate_fill_int_) is selected based on the data format.

### Applicable Formats for Testing

Based on the operation type, Quasar architecture support (`QUASAR_DATA_FORMAT_ENUM_VALUES`), and existing test patterns:

| Format | Applicable | Rationale |
|--------|-----------|-----------|
| Float16 | Yes | Standard float format; fill stores with implied format. Enum value 1. |
| Float16_b | Yes | Standard float format; fill stores with implied format. Enum value 5. |
| Float32 | Yes | 32-bit float; needs 32-bit store mode or implied format with dest_acc. Enum value 0. |
| Int32 | Yes | 32-bit integer; uses _calculate_fill_int_ with sfpmem::INT32 store mode. Enum value 8. |
| Int16 | Yes | 16-bit integer (Quasar-specific, enum value 9). Uses _calculate_fill_int_ with sfpmem::UINT16 mode. |
| Int8 | Yes | 8-bit signed integer. Uses _calculate_fill_int_ with sfpmem::UINT8 mode. Enum value 14. |
| UInt8 | Yes | 8-bit unsigned integer. Uses _calculate_fill_int_ with sfpmem::UINT8 mode. Enum value 17. |
| MxFp8R | Yes | L1-only MX format; unpacked to Float16_b in Dest. Fill writes Float16_b value, packer converts to MX. Enum value 18. |
| MxFp8P | Yes | Same as MxFp8R but different MX variant. Enum value 20. |

### Format-Dependent Code Paths
The reference has **three separate functions** based on format category:
1. `_calculate_fill_` -- for float formats (Float16, Float16_b, Float32, MxFp8R/P). Uses `sfpi::vFloat` and implied-format SFPSTORE.
2. `_calculate_fill_int_` -- for integer formats (Int32, Int16, Int8, UInt8). Uses explicit integer SFPSTORE modes. Templated on `InstrModLoadStore` to select `INT32` vs `LO16` mode.
3. `_calculate_fill_bitcast_` -- for writing raw bit patterns as float. Uses `Converter::as_float()` to reinterpret bits.

The caller is responsible for routing to the correct function based on data format.

### Format Constraints
- **MX formats** (MxFp8R, MxFp8P): Require `implied_math_format=Yes` in the test. Data flows through Dest as Float16_b.
- **Int32**: Requires `dest_acc=Yes` (32-bit Dest). Loading requires two `SFPLOADI` calls (HI16_ONLY + LO16_ONLY). Store uses `sfpmem::INT32`.
- **Int16**: Uses `sfpmem::UINT16` store mode. Stored as sign+magnitude in Dest.
- **Int8/UInt8**: Uses `sfpmem::UINT8` store mode. May need special handling for sign+magnitude.
- **Float32**: Requires `dest_acc=Yes` when input is non-Float32.
- **Cross-exponent-family** conversions (e.g., Float16_b input -> Float16 output) require `dest_acc=Yes`.
- **Integer and float formats cannot be mixed** in input->output (integer in, integer out; float in, float out).

## Existing Target Implementations
No existing `ckernel_sfpu_fill.h` on Quasar. The file will be created from scratch.

Closest existing patterns (all on Quasar):
- `ckernel_sfpu_abs.h` -- simplest SFPU kernel, demonstrates standard structure
- `ckernel_sfpu_relu.h` -- similar simple structure
- `ckernel_sfpu_lrelu.h` -- demonstrates constant loading via `TTI_SFPLOADI`
- `ckernel_sfpu_typecast_int32_fp32.h` -- demonstrates `sfpmem::INT32` / `sfpmem::FP32` usage
- `ckernel_sfpu_typecast_fp16b_uint16.h` -- demonstrates `sfpmem::UINT16` usage

## Sub-Kernel Phases

| Phase | Name | Functions | Dependencies |
|-------|------|-----------|--------------|
| 1 | float_fill | `_calculate_fill_` | none |
| 2 | int_fill | `_calculate_fill_int_` | none (independent, but Phase 1 verifies basic infrastructure) |
| 3 | bitcast_fill | `_calculate_fill_bitcast_` | none (independent, but Phase 1 verifies the file structure) |

**Ordering rationale**:
- **Phase 1 (float_fill)** is simplest: just SFPLOADI + SFPSTORE with implied format. Establishes the file with includes, namespace, and the basic iteration pattern.
- **Phase 2 (int_fill)** is medium complexity: needs explicit store modes and the two-SFPLOADI pattern for 32-bit values. Builds on the file created in Phase 1.
- **Phase 3 (bitcast_fill)** is similar to Phase 1 but loads raw bits instead of a float. Uses the two-SFPLOADI pattern (like Phase 2's INT32 path) but stores with implied format.

For simple testing, Phase 1 can be tested with float formats (Float16, Float16_b, Float32), Phase 2 with integer formats (Int32, Int16), and Phase 3 with float formats using specific bit patterns.

Note: All three phases are independent in functionality but should be done sequentially because they share a file and each phase appends to the existing file content.

## Notes for Planner

1. **SfpuType enum**: Quasar's `llk_defs.h` does NOT have a `fill` entry in the `SfpuType` enum. The test infrastructure may need this to be added if the test dispatch relies on it. However, since fill is typically called directly (not through a switch on SfpuType), this may not be needed.

2. **No `fill` in `ckernel_sfpu.h` includes**: The Quasar parent file `tt_llk_quasar/common/inc/ckernel_sfpu.h` does not include `sfpu/ckernel_sfpu_fill.h`. The writer must add this include.

3. **SFPSTORE `done` parameter**: All existing Quasar SFPSTORE calls use `done=0`. The `done` flag toggles the write bank ID -- setting it to 0 is correct for normal store operations within a tile.

4. **SFPLOADI for float constants**: The pattern from lrelu.h is `TTI_SFPLOADI(lreg, 0, (value >> 16))` where `value` is a uint32 with the FP16_b representation in the upper 16 bits. The `>> 16` extracts the FP16_b half, and `instr_mod0=0` means FP16_B mode which auto-converts to FP32 in the LREG.
