# Quasar LLK Generation Plan

Generated from `quasar_llk_gap_analysis.xlsx` + on-disk verification (2026-03-25, post-rebase to main d8e3e181).

---

## Current State (On Disk)

### Quasar SFPU kernels: **14 of 52** implemented
Existing: add, exp, gelu, lrelu, recip, relu, rsqrt, sigmoid, silu, sqrt, square, tanh, typecast_fp16b_uint16, typecast_int32_fp32

### Quasar LLK submodule: **21 files**
Includes math (8), pack (4), unpack (7), infrastructure (2).

### Quasar tests: **15 test files**

---

## Naming Differences (BH → Quasar)

These BH files have Quasar equivalents with different names — they are NOT missing:

| BH File | Quasar Equivalent | Status |
|---------|-------------------|--------|
| llk_unpack_A.h | llk_unpack_unary_operand.h | Exists |
| llk_unpack_AB.h | llk_unpack_binary_operands.h | Exists |
| llk_unpack_AB_matmul.h | llk_unpack_matmul.h | Exists |
| llk_unpack_AB_reduce.h | llk_unpack_reduce.h | Exists |

---

## Generation Targets

### Tier 1: Simple SFPU Kernels (10)

Direct computation, no LUT. Good for pipeline validation.

| # | Kernel | Codegen Prompt | Notes |
|---|--------|---------------|-------|
| 1 | ckernel_sfpu_abs.h | `Generate abs for Quasar` | |
| 2 | ckernel_sfpu_negative.h | `Generate negative for Quasar` | |
| 3 | ckernel_sfpu_sign.h | `Generate sign for Quasar` | |
| 4 | ckernel_sfpu_fill.h | `Generate fill for Quasar` | |
| 5 | ckernel_sfpu_hardtanh.h | `Generate hardtanh for Quasar` | |
| 6 | ckernel_sfpu_clamp.h | `Generate clamp for Quasar` | |
| 7 | ckernel_sfpu_threshold.h | `Generate threshold for Quasar` | |
| 8 | ckernel_sfpu_dropout.h | `Generate dropout for Quasar` | |
| 9 | ckernel_sfpu_is_fp16_zero.h | `Generate is_fp16_zero for Quasar` | |
| 10 | ckernel_sfpu_isinf_isnan.h | `Generate isinf_isnan for Quasar` | |

### Tier 2: Medium SFPU Kernels (14)

LUT-based, iterative, or multi-step computation.

| # | Kernel | Codegen Prompt | Notes |
|---|--------|---------------|-------|
| 11 | ckernel_sfpu_elu.h | `Generate elu for Quasar` | Uses exp |
| 12 | ckernel_sfpu_cdf.h | `Generate cdf for Quasar` | Uses exp |
| 13 | ckernel_sfpu_exp2.h | `Generate exp2 for Quasar` | |
| 14 | ckernel_sfpu_log.h | `Generate log for Quasar` | LUT-based |
| 15 | ckernel_sfpu_tanh_derivative.h | `Generate tanh_derivative for Quasar` | Uses tanh |
| 16 | ckernel_sfpu_rsqrt_compat.h | `Generate rsqrt_compat for Quasar` | |
| 17 | ckernel_sfpu_activations.h | `Generate activations for Quasar` | Multi-op dispatch |
| 18 | ckernel_sfpu_rounding_ops.h | `Generate rounding_ops for Quasar` | |
| 19 | ckernel_sfpu_polyval.h | `Generate polyval for Quasar` | Polynomial eval |
| 20 | ckernel_sfpu_trigonometry.h | `Generate trigonometry for Quasar` | sin/cos |
| 21 | ckernel_sfpu_load_config.h | `Generate load_config for Quasar` | Config loading |
| 22 | ckernel_sfpu_cast_fp32_to_fp16a.h | `Generate cast_fp32_to_fp16a for Quasar` | Format conversion |
| 23 | ckernel_sfpu_converter.h | `Generate converter for Quasar` | Format conversion |
| 24 | ckernel_sfpu_typecast.h | `Generate typecast for Quasar` | Multi-type cast |

### Tier 3: Complex SFPU Kernels (12)

Multi-operand, special hardware, or unusual patterns.

| # | Kernel | Codegen Prompt | Notes |
|---|--------|---------------|-------|
| 25 | ckernel_sfpu_binary.h | `Generate binary for Quasar` | 2-operand SFPU |
| 26 | ckernel_sfpu_binary_bitwise.h | `Generate binary_bitwise for Quasar` | Bitwise ops |
| 27 | ckernel_sfpu_comp.h | `Generate comp for Quasar` | Comparison ops |
| 28 | ckernel_sfpu_add_int.h | `Generate add_int for Quasar` | Integer arithmetic |
| 29 | ckernel_sfpu_sub_int.h | `Generate sub_int for Quasar` | Integer arithmetic |
| 30 | ckernel_sfpu_mul_int.h | `Generate mul_int for Quasar` | Integer arithmetic |
| 31 | ckernel_sfpu_shift.h | `Generate shift for Quasar` | Bit shifting |
| 32 | ckernel_sfpu_quant.h | `Generate quant for Quasar` | Quantization |
| 33 | ckernel_sfpu_where.h | `Generate where for Quasar` | Conditional select |
| 34 | ckernel_sfpu_topk.h | `Generate topk for Quasar` | Top-K selection |
| 35 | ckernel_sfpu_cumsum.h | `Generate cumsum for Quasar` | Cumulative sum |
| 36 | ckernel_sfpu_ema.h | `Generate ema for Quasar` | Exponential moving avg |

### Tier 4: Specialized SFPU Kernels (6)

Unusual data flow or multi-tile patterns.

| # | Kernel | Codegen Prompt | Notes |
|---|--------|---------------|-------|
| 37 | ckernel_sfpu_welfords.h | `Generate welfords for Quasar` | Online variance |
| 38 | ckernel_sfpu_reduce.h | `Generate reduce for Quasar` | SFPU reduction |
| 39 | ckernel_sfpu_reduce_custom.h | `Generate reduce_custom for Quasar` | Custom reduction |
| 40 | ckernel_sfpu_max_pool_indices.h | `Generate max_pool_indices for Quasar` | Pooling indices |
| 41 | ckernel_sfpu_add_top_row.h | `Generate add_top_row for Quasar` | Row manipulation |
| 42 | ckernel_sfpu_reshuffle_rows.h | `Generate reshuffle_rows for Quasar` | Row reordering |

### Tier 5: Missing LLK Submodule Files (11)

These are truly missing (not just renamed). 4 of the 15 BH-only files have Quasar equivalents with different names (see naming table above).

**Math SFPU wrappers** (depend on SFPU kernels from Tiers 1-4):

| # | BH File | Codegen Prompt | Dependencies |
|---|---------|---------------|--------------|
| 43 | llk_math_eltwise_unary_sfpu.h | `Generate math_eltwise_unary_sfpu for Quasar` | SFPU kernels |
| 44 | llk_math_eltwise_unary_sfpu_params.h | `Generate math_eltwise_unary_sfpu_params for Quasar` | #43 |
| 45 | llk_math_eltwise_binary_sfpu.h | `Generate math_eltwise_binary_sfpu for Quasar` | SFPU kernels |
| 46 | llk_math_eltwise_binary_sfpu_params.h | `Generate math_eltwise_binary_sfpu_params for Quasar` | #45 |
| 47 | llk_math_eltwise_ternary_sfpu.h | `Generate math_eltwise_ternary_sfpu for Quasar` | SFPU kernels |
| 48 | llk_math_eltwise_ternary_sfpu_params.h | `Generate math_eltwise_ternary_sfpu_params for Quasar` | #47 |
| 49 | llk_math_welfords_sfpu.h | `Generate math_welfords_sfpu for Quasar` | ckernel_sfpu_welfords.h |
| 50 | llk_math_welfords_sfpu_params.h | `Generate math_welfords_sfpu_params for Quasar` | #49 |

**Other missing LLK files**:

| # | BH File | Codegen Prompt | Notes |
|---|---------|---------------|-------|
| 51 | llk_math_transpose_dest.h | `Generate math_transpose_dest for Quasar` | Dest register transpose |
| 52 | llk_pack_rows.h | `Generate pack_rows for Quasar` | Row-based packing |
| 53 | llk_unpack_untilize.h | `Generate unpack_untilize for Quasar` | Untilize on unpack |

---

## Recommended Execution Order

### Phase A: Validate pipeline (pick 1-2 from Tier 1)
Start with the simplest kernel to prove codegen works end-to-end:
```
Generate abs for Quasar
Generate negative for Quasar
```
These are trivial SFPU ops. If codegen can't handle these, fix the pipeline before scaling.

### Phase B: Complete Tier 1 (remaining simple SFPU)
8 more simple kernels. Batch these — they're all similar patterns.

### Phase C: Tier 2 medium SFPU (14 kernels)
Some depend on existing SFPU (elu→exp, tanh_derivative→tanh). Those dependencies already exist on Quasar so no ordering constraint.

### Phase D: Tier 3 complex SFPU (12 kernels)
Integer ops, binary ops, conditional ops. More architectural discovery needed.

### Phase E: Tier 4 specialized SFPU (6 kernels)
Unusual patterns — welfords, reduce, pooling. Most complex SFPU work.

### Phase F: Tier 5 LLK submodule (11 files)
Math SFPU wrappers depend on SFPU kernels from earlier phases. Pack/unpack are independent.

---

## SFPU Dependency Chain

Some SFPU kernels depend on others already existing on Quasar:

| Kernel | Depends On | Status |
|--------|-----------|--------|
| ckernel_sfpu_elu.h | exp | exp exists on Quasar |
| ckernel_sfpu_cdf.h | exp | exp exists on Quasar |
| ckernel_sfpu_tanh_derivative.h | tanh | tanh exists on Quasar |
| ckernel_sfpu_activations.h | multiple | most deps exist |

All dependencies are satisfied by the 14 existing Quasar SFPU kernels. No ordering constraints within a tier.

---

## Notes

### 1. Existing implementations are now available as style examples
With 14 SFPU kernels already on Quasar, agents have on-target style references. This was the main concern before — now solved.

### 2. SFPU kernels are standalone
Each SFPU kernel is a single header file with no compilation dependencies on other SFPU kernels (they're dispatched at a higher level). Safe to generate in any order.

### 3. Math SFPU wrappers #include SFPU kernels
Tier 5 math wrappers (llk_math_eltwise_unary_sfpu.h etc.) will #include the SFPU kernels. Generate SFPU kernels before their math wrappers.

### 4. Params files are thin
The `_params.h` files are typically just parameter struct definitions with minimal logic. They're paired with their main file and can be generated together.

---

## Totals

| Category | Count |
|----------|-------|
| SFPU Tier 1 (simple) | 10 |
| SFPU Tier 2 (medium) | 14 |
| SFPU Tier 3 (complex) | 12 |
| SFPU Tier 4 (specialized) | 6 |
| LLK submodule (Tier 5) | 11 |
| **Total to generate** | **53** |
| Already on Quasar (SFPU) | 14 |
| Already on Quasar (LLK) | 21 |
