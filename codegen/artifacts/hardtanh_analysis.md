# Analysis: hardtanh

## Kernel Type
sfpu

## Reference File
`tt_llk_blackhole/common/inc/sfpu/ckernel_sfpu_hardtanh.h`

## Purpose
Hardtanh is a clamping activation function that clips input values to a specified range:
```
hardtanh(x, min_val, max_val) = max_val   if x > max_val
                                min_val   if x < min_val
                                x         otherwise
```
Equivalent to `clamp(x, min_val, max_val)`. Default PyTorch parameters are min_val=-1.0, max_val=1.0.

## Algorithm Summary
1. Load two threshold parameters (min_val, max_val)
2. For each element in the tile:
   a. Load input value x from Dest register
   b. If x > max_val, replace x with max_val (upper clamp)
   c. If x < min_val, replace x with min_val (lower clamp)
   d. Store result back to Dest register

The Blackhole reference uses an indirect arithmetic approach with three pre-computed parameters:
- p0 = -(neg_threshold), p1 = -(pos_threshold - neg_threshold), p2 = -(pos_threshold)
- Shifts values so clamping becomes comparison to zero, then restores offset

The Quasar implementation should use direct comparison (SFPGT + SFPMOV) which is cleaner and matches existing Quasar patterns (hardsigmoid, relu_max, threshold).

## Template Parameters
| Parameter | Purpose | Values |
|-----------|---------|--------|
| APPROXIMATION_MODE | Controls accuracy vs performance tradeoff | true/false (not used in actual computation for hardtanh) |
| ITERATIONS | Number of unrolled iterations | Default 8 (Blackhole); not used in Quasar pattern |

**Note on Quasar**: Quasar SFPU kernels do NOT use template parameters for the calculate function. They pass `iterations` as a runtime `const int` parameter. The `APPROXIMATION_MODE` and `ITERATIONS` template parameters are Blackhole-specific and should be dropped.

## Functions Identified
| Function | Purpose | Complexity |
|----------|---------|------------|
| `_calculate_hardtanh_` (Blackhole) | Main entry point: loads params, loops over iterations, calls inner function | Low |
| Inner loop body (Blackhole) | Per-iteration: load value, arithmetic clamp, store | Low |

**Quasar target functions** (derived from existing patterns):
| Function | Purpose | Complexity |
|----------|---------|------------|
| `_calculate_hardtanh_sfp_rows_()` | Per-row calculation: load, upper clamp, lower clamp, store | Low |
| `_calculate_hardtanh_(iterations, min_val, max_val)` | Main entry: load constants, enable CC, loop with counter increment | Low |

## Key Constructs Used

### Blackhole Reference Constructs
- `sfpi::vFloat`: SFPI high-level vector float type -- **NOT available on Quasar**
- `sfpi::s2vFloat16(param)`: Converts scalar uint to vFloat16 -- **NOT available on Quasar**
- `sfpi::dst_reg[0]`: SFPI destination register access -- **NOT available on Quasar**
- `v_if (val < 0.0f) ... v_endif`: SFPI conditional execution -- **NOT available on Quasar**
- `sfpi::dst_reg++`: SFPI destination register increment -- Quasar uses `_incr_counters_`
- `#pragma GCC unroll 0`: Blackhole disables unrolling; Quasar uses `#pragma GCC unroll 8`

### Parameter Encoding (Blackhole)
- params are pre-computed: p0 = -min_val, p1 = -(max_val - min_val), p2 = -max_val
- Encoded as FP16_B in the upper 16 bits of a uint32

### Quasar Target Constructs (from existing kernels)
- `TTI_SFPLOAD(vd, instr_mod, addr_mod, addr, done)`: Load from Dest to LREG
- `TTI_SFPLOADI(vd, instr_mod, imm16)`: Load 16-bit immediate to LREG (FP16_B with instr_mod=0)
- `TTI_SFPSTORE(vd, done, addr_mod, addr, instr_mod)`: Store LREG to Dest
- `TTI_SFPGT(imm12, vc, vd, instr_mod)`: Greater-than test (VD > VC), sets CC
- `TTI_SFPMOV(vc, vd, instr_mod)`: Copy VC to VD (conditional on CC when enabled)
- `TTI_SFPENCC(imm12, instr_mod)`: Enable/disable/reset condition codes
- `TTI_SFPSETCC(imm12, vc, instr_mod)`: Set CC based on register value
- `ckernel::math::_incr_counters_<0x0, 0x0, ckernel::math::SFP_ROWS, 0x0>()`: Dest row increment
- `p_sfpu::LREG0` through `p_sfpu::LREG7`: LREG indices
- `p_sfpu::sfpmem::DEFAULT`: Default memory access mode
- `ADDR_MOD_7`: Standard SFPU address modifier

## Dependencies

### Blackhole Reference Dependencies
- `<cstdint>`: Standard integer types
- `sfpi.h`: SFPI high-level API (vFloat, dst_reg, v_if, s2vFloat16)

### Quasar Target Dependencies (from existing patterns)
- `<cstdint>`: Standard integer types
- `ckernel_trisc_common.h`: Provides TTI macros, p_sfpu namespace, ADDR_MOD_7
- `cmath_common.h`: Provides `ckernel::math::_incr_counters_`, `ckernel::math::SFP_ROWS`

## Complexity Classification
**Simple**

Rationale:
- Quasar has direct hardware support for comparison (SFPGT) and conditional copy (SFPMOV)
- The pattern is nearly identical to existing Quasar kernels (hardsigmoid upper clamp, relu_max)
- Only Simple EXU instructions needed (no MAD, no NOP hazards)
- No LUT access, no LOADMACRO, no complex pipeline management
- Single function pair (_sfp_rows_ + main wrapper)

## Constructs Requiring Translation

| Blackhole Construct | What It Does | Quasar Equivalent |
|---------------------|-------------|-------------------|
| `sfpi::vFloat p0 = sfpi::s2vFloat16(param0)` | Load FP16_B param into vector register | `TTI_SFPLOADI(LREG2, 0, (param >> 16))` |
| `sfpi::dst_reg[0]` read | Load value from Dest | `TTI_SFPLOAD(LREG0, DEFAULT, ADDR_MOD_7, 0, 0)` |
| `sfpi::dst_reg[0]` write | Store value to Dest | `TTI_SFPSTORE(LREG0, 0, ADDR_MOD_7, 0, 0)` |
| `v_if (val < 0.0f) val = 0.0f; v_endif` | Conditional clamp to zero | `TTI_SFPGT` + `TTI_SFPMOV` + `TTI_SFPENCC` |
| `val += p0` | FP32 addition | Not needed: Quasar uses direct comparison |
| `sfpi::dst_reg++` | Advance Dest pointer | `ckernel::math::_incr_counters_<0x0, 0x0, SFP_ROWS, 0x0>()` |
| `#pragma GCC unroll 0` | Disable unrolling | `#pragma GCC unroll 8` (Quasar convention) |

**Algorithmic change**: The Blackhole arithmetic offset approach (add/clamp-to-zero/add) is replaced by direct SFPGT comparison on Quasar. This is simpler, more readable, and consistent with existing Quasar patterns.

## Target Expected API

### From Test Infrastructure
No Quasar test harness exists for hardtanh. However, based on the pattern established by similar kernels (threshold, elu, lrelu):

**Expected C++ test call pattern** (from `sfpu_threshold_quasar_test.cpp`, `sfpu_elu_quasar_test.cpp`):
```cpp
#include "sfpu/ckernel_sfpu_hardtanh.h"
using namespace ckernel::sfpu;

// Call pattern:
_llk_math_eltwise_unary_sfpu_params_<false>(
    ckernel::sfpu::_calculate_hardtanh_, i, num_sfpu_iterations, min_val_param, max_val_param);
```

**Function signature expected by test harness**:
```cpp
inline void _calculate_hardtanh_(const int iterations, const std::uint32_t min_val, const std::uint32_t max_val)
```
- `iterations`: Number of row-pairs to process (= TEST_FACE_R_DIM / SFP_ROWS)
- `min_val`: FP32-encoded min threshold (upper 16 bits = BF16 value for SFPLOADI)
- `max_val`: FP32-encoded max threshold (upper 16 bits = BF16 value for SFPLOADI)

### From Parent File (`tt_llk_quasar/common/inc/ckernel_sfpu.h`)
- Must be `#include "sfpu/ckernel_sfpu_hardtanh.h"`
- Must be within `namespace ckernel { namespace sfpu { ... } }`

### From Closest Existing Target Kernel (`ckernel_sfpu_threshold.h`, `ckernel_sfpu_lrelu.h`)
**Pattern to follow**:
- File includes: `<cstdint>`, `"ckernel_trisc_common.h"`, `"cmath_common.h"`
- Namespace: `ckernel::sfpu`
- Two functions: `_calculate_hardtanh_sfp_rows_()` (inner) and `_calculate_hardtanh_(iterations, params...)` (outer)
- Inner function is non-templated `inline void`
- Outer function loads constants via `TTI_SFPLOADI`, enables CC, loops with `#pragma GCC unroll 8`, increments counters, disables CC
- No init function needed (constants loaded inside `_calculate_hardtanh_` before the loop)
- No uninit function needed

### Template Parameters
- **KEEP from reference**: None (Quasar SFPU kernels don't use templates for calculate functions)
- **DROP (reference-only)**: `APPROXIMATION_MODE`, `ITERATIONS` -- Blackhole template params not used on Quasar
- **ADD (target-only)**: None

### Reference Features to DROP
- SFPI high-level API (vFloat, v_if/v_endif, dst_reg, s2vFloat16)
- Arithmetic offset algorithm (replaced with direct comparison)
- Three-parameter encoding (p0, p1, p2 pre-computed offsets); use two direct params (min_val, max_val)

### Target Features NOT in Reference
- `_calculate_hardtanh_sfp_rows_()` inner function (Quasar pattern: separate per-row function)
- `ckernel::math::_incr_counters_` call
- CC enable/disable wrapper (`SFPENCC(1,2)` / `SFPENCC(0,2)`)

## Format Support

### Format Domain
**Float-only** (with potential universal applicability)

Hardtanh is a clamping operation that is mathematically valid for any ordered type (float or integer). However:
- The SFPGT instruction supports both FP32 comparison (imm12[0]=1) and INT32 comparison (imm12[0]=0)
- Existing Quasar SFPGT usage uses imm12=0 (INT32 mode) even for FP32 data, which works due to IEEE 754 bit-pattern ordering
- Parameters are loaded as FP16_B via SFPLOADI (instr_mod=0), which assumes floating-point encoding
- Integer clamping would require INT16 or INT32 parameter encoding, which is a different code path

For initial implementation, treat as **float-only** (matching threshold, elu, hardsigmoid patterns).

### Applicable Formats for Testing
Based on the operation type, target architecture support, and existing test patterns:

| Format | Applicable | Rationale |
|--------|-----------|-----------|
| Float16 | Yes | Standard FP16a; unpacked to FP32 in LREG for comparison |
| Float16_b | Yes | Primary SFPU format (BF16); native SFPLOADI target format |
| Float32 | Yes | Native LREG format; requires is_fp32_dest_acc_en=true |
| Int32 | No | Integer clamping valid mathematically but parameter encoding assumes FP; exclude for initial impl |
| Int16 | No | Limited SFPU support; not used by similar kernels |
| Int8 | No | Sign-magnitude encoding complexity; not used by similar kernels |
| UInt8 | No | Unsigned; not used by similar float SFPU kernels |
| MxFp8R | No | Could work (unpacks to Float16_b) but not used by threshold/elu tests; add later if needed |
| MxFp8P | No | Could work (unpacks to Float16_b) but not used by threshold/elu tests; add later if needed |

### Format-Dependent Code Paths
The kernel is **format-agnostic** at the SFPU level. All values are converted to FP32 in LREGs by SFPLOAD/SFPSTORE (implied format conversion). The SFPGT comparison and SFPMOV copy operate on the 32-bit LREG values regardless of original format.

The only format dependency is in parameter loading: `TTI_SFPLOADI(reg, 0 /*Float16_b*/, (param >> 16))` always loads the upper 16 bits of the FP32-encoded parameter as BF16. This is the standard pattern used by all existing Quasar SFPU kernels (threshold, elu, lrelu, hardsigmoid).

### Format Constraints
- **Float32**: Requires `is_fp32_dest_acc_en = true` (32-bit Dest accumulation mode)
- **MX formats (MxFp8R, MxFp8P)**: L1-only storage; unpacked to Float16_b before SFPU; would work but excluded from initial tests
- **Cross-exponent-family**: Float32 input -> Float16 output requires dest_acc=Yes on Quasar
- **Non-Float32 -> Float32**: Requires dest_acc=Yes on Quasar packer
- **Integer formats**: Not applicable for float-encoded parameter loading; would need separate code path

## Existing Target Implementations

### Most Similar: `ckernel_sfpu_threshold.h` (Quasar)
- Uses SFPGT for comparison + SFPMOV for conditional replacement
- Two parameters loaded via SFPLOADI (threshold and replacement value)
- CC enable/disable pattern matches
- Identical structure to what hardtanh needs

### Also Similar: `ckernel_sfpu_lrelu.h` (Quasar) -- relu_max function
- `_calculate_relu_max_sfp_rows_()`: Uses SFPGT for upper bound check + SFPMOV for conditional clamp
- Pattern: `SFPGT(0, threshold_reg, value_reg, 0x1)` -> `SFPMOV(threshold_reg, value_reg, 0)` -> `SFPENCC(0,0)`
- Hardtanh is essentially TWO relu_max-style clamps in sequence (upper + lower)

### Also Similar: `ckernel_sfpu_activations.h` (Quasar) -- hardsigmoid
- Uses both SFPGT (upper clamp) and SFPSETCC (lower clamp to zero)
- CC reset between clamps via `SFPENCC(0, 0)`
- Most complete two-sided clamp pattern available

### Key Pattern from Existing Implementations
The standard Quasar SFPU two-sided clamp sequence:
```
// Upper clamp: if x > max_val, x = max_val
TTI_SFPGT(0, max_reg, value_reg, 0x1);     // CC = (value > max)
TTI_SFPMOV(max_reg, value_reg, 0);          // copy max where CC set
TTI_SFPENCC(0, 0);                          // reset CC

// Lower clamp: if x < min_val, x = min_val
TTI_SFPGT(0, value_reg, min_reg, 0x1);     // CC = (min > value), i.e., value < min
TTI_SFPMOV(min_reg, value_reg, 0);          // copy min where CC set
TTI_SFPENCC(0, 0);                          // reset CC
```

## Sub-Kernel Phases

Hardtanh is a simple kernel with a single function pair. There is only one phase.

| Phase | Name | Functions | Dependencies |
|-------|------|-----------|--------------|
| 1 | hardtanh | `_calculate_hardtanh_sfp_rows_()`, `_calculate_hardtanh_(iterations, min_val, max_val)` | none |

**Ordering rationale**: Single phase because:
- Only one logical operation (clamp)
- Only two functions that are tightly coupled (inner row calculation + outer loop wrapper)
- No variants, no separate init/uninit needed
- Matches the pattern of similarly simple kernels (relu, threshold)

## Infrastructure Changes Required

1. **SfpuType enum**: Add `hardtanh` to `ckernel::SfpuType` in `tt_llk_quasar/llk_lib/llk_defs.h` (needed only if tests reference `SfpuType::hardtanh`)
2. **ckernel_sfpu.h include**: Add `#include "sfpu/ckernel_sfpu_hardtanh.h"` to `tt_llk_quasar/common/inc/ckernel_sfpu.h`
3. **MathOperation enum**: Add `Hardtanh` to `tests/python_tests/helpers/llk_params.py` MathOperation enum (needed for test infrastructure)
4. **Golden generator**: Add `_hardtanh` method to UnarySFPUGolden in `tests/python_tests/helpers/golden_generators.py`
5. **Test files**: Create `tests/sources/quasar/sfpu_hardtanh_quasar_test.cpp` and `tests/python_tests/quasar/test_sfpu_hardtanh_quasar.py`
