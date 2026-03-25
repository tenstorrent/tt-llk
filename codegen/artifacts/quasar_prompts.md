# Quasar Kernel Generation Prompts

Run any prompt below from the `codegen/` directory:

```bash
cd codegen
claude -p "Generate abs for Quasar" --dangerously-skip-permissions --effort max --verbose
```

Ordered by testability — kernels with golden generators (functional test possible) come first.

**Parallel execution**: All SFPU kernels (Waves 1-7) can run in parallel — each writes to its own file. Wave 8 must wait for all SFPU kernels to exist.

```bash
# Run an entire wave in parallel
./scripts/batch_generate.sh --wave 1 --parallel

# Run waves 1-4 in parallel, max 8 concurrent
./scripts/batch_generate.sh --wave 1 --parallel -j 8
./scripts/batch_generate.sh --wave 2 --parallel -j 8
./scripts/batch_generate.sh --wave 3 --parallel -j 8
./scripts/batch_generate.sh --wave 4 --parallel -j 8
```

**After generating**: Add `#include` lines to `tt_llk_quasar/common/inc/ckernel_sfpu.h` for each new kernel.

---

## Wave 1: Testable Simple SFPU (4)

These have golden generators in `tests/python_tests/helpers/golden_generators.py`.
After generating each, add it to `sfpu_nonlinear_quasar_test.cpp` and run functional tests.
**All 4 can run in parallel.**

```
Generate abs for Quasar
```

```
Generate negative for Quasar
```

```
Generate fill for Quasar
```

```
Generate threshold for Quasar
```

---

## Wave 2: Testable Medium SFPU (5)

Also have golden generators. Functional test possible after wiring into the Quasar test.
**All 5 can run in parallel.**

```
Generate elu for Quasar
```

```
Generate exp2 for Quasar
```

```
Generate log for Quasar
```

```
Generate trigonometry for Quasar
```

```
Generate activations for Quasar
```

---

## Wave 3: Remaining Simple SFPU (6)

Compile-only validation — no golden generators exist for these.
**All 6 can run in parallel.**

```
Generate sign for Quasar
```

```
Generate hardtanh for Quasar
```

```
Generate clamp for Quasar
```

```
Generate dropout for Quasar
```

```
Generate is_fp16_zero for Quasar
```

```
Generate isinf_isnan for Quasar
```

---

## Wave 4: Remaining Medium SFPU (9)

Compile-only validation.
**All 9 can run in parallel.**

```
Generate cdf for Quasar
```

```
Generate tanh_derivative for Quasar
```

```
Generate rsqrt_compat for Quasar
```

```
Generate rounding_ops for Quasar
```

```
Generate polyval for Quasar
```

```
Generate load_config for Quasar
```

```
Generate cast_fp32_to_fp16a for Quasar
```

```
Generate converter for Quasar
```

```
Generate typecast for Quasar
```

---

## Wave 5: Complex SFPU with Test Potential (4)

These have cross-arch tests or dedicated test files that may work after wiring.
**All 4 can run in parallel.**

```
Generate comp for Quasar
```

```
Generate topk for Quasar
```

```
Generate quant for Quasar
```

```
Generate binary for Quasar
```

---

## Wave 6: Remaining Complex SFPU (8)

Compile-only validation.
**All 8 can run in parallel.**

```
Generate binary_bitwise for Quasar
```

```
Generate add_int for Quasar
```

```
Generate sub_int for Quasar
```

```
Generate mul_int for Quasar
```

```
Generate shift for Quasar
```

```
Generate where for Quasar
```

```
Generate cumsum for Quasar
```

```
Generate ema for Quasar
```

---

## Wave 7: Specialized SFPU (6)

Compile-only validation. Unusual data flow or multi-tile patterns.
**All 6 can run in parallel.**

```
Generate welfords for Quasar
```

```
Generate reduce for Quasar
```

```
Generate reduce_custom for Quasar
```

```
Generate max_pool_indices for Quasar
```

```
Generate add_top_row for Quasar
```

```
Generate reshuffle_rows for Quasar
```

---

## Wave 8: LLK Submodule (11)

Generate AFTER all SFPU kernels (Waves 1-7) — the math wrappers `#include` them.
After these exist, `test_zzz_eltwise_unary_sfpu.py` becomes a second validation path.

**Dependency constraints within Wave 8:**
- `#44` depends on `#43` (unary params depends on unary)
- `#46` depends on `#45` (binary params depends on binary)
- `#48` depends on `#47` (ternary params depends on ternary)
- `#50` depends on `#49` (welfords params depends on welfords)
- `#51`, `#52`, `#53` are independent of each other and of the above

**Safe parallel groups within Wave 8:**
- Group A (parallel): `#43`, `#45`, `#47`, `#49`, `#51`, `#52`, `#53`
- Group B (after A): `#44`, `#46`, `#48`, `#50`

```
Generate math_eltwise_unary_sfpu for Quasar
```

```
Generate math_eltwise_unary_sfpu_params for Quasar
```

```
Generate math_eltwise_binary_sfpu for Quasar
```

```
Generate math_eltwise_binary_sfpu_params for Quasar
```

```
Generate math_eltwise_ternary_sfpu for Quasar
```

```
Generate math_eltwise_ternary_sfpu_params for Quasar
```

```
Generate math_welfords_sfpu for Quasar
```

```
Generate math_welfords_sfpu_params for Quasar
```

```
Generate math_transpose_dest for Quasar
```

```
Generate pack_rows for Quasar
```

```
Generate unpack_untilize for Quasar
```
