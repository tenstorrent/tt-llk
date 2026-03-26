# Testing Guide for 4-Interface Pack Untilize Optimization

This document explains how to test and compare the 4-interface optimized pack_untilize implementation against the standard 2-interface version.

## Overview

The test suite includes:
1. **Functional tests**: Verify correctness of the 4-interface implementation
2. **Performance tests**: Measure bandwidth improvement

## Test Structure

### C++ Kernel Files

| File | Purpose | Uses |
|------|---------|------|
| `tests/sources/pack_untilize_test.cpp` | Functional test (baseline 2-intf) | `_llk_pack_untilize_*` |
| `tests/sources/pack_untilize_4intf_test.cpp` | Functional test (optimized 4-intf) | `_llk_pack_untilize_4intf_*` |
| `tests/sources/pack_untilize_perf.cpp` | Performance test (baseline 2-intf) | `_llk_pack_untilize_*` |
| `tests/sources/pack_untilize_4intf_perf.cpp` | Performance test (optimized 4-intf) | `_llk_pack_untilize_4intf_*` |

### Python Test Files

| File | Tests |
|------|-------|
| `tests/python_tests/test_zzz_pack_untilize.py` | `test_pack_untilize` (baseline 2-intf)<br>`test_pack_untilize_4intf` (optimized 4-intf) |
| `tests/python_tests/perf_pack_untilize.py` | `test_perf_pack_untilize` (baseline 2-intf)<br>`test_perf_pack_untilize_4intf` (optimized 4-intf) |

### Helper Files

| File | Purpose |
|------|---------|
| `tests/python_tests/helpers/interleaved_layout.py` | Converts tile data to/from interleaved layout |

## Running Tests

### Prerequisites

```bash
cd /localdev/skrsmanovic/gitrepos/tt-llk/tests
source setup_testing_env.sh  # Or setup_external_testing_env.sh
```

###1. Functional Tests

#### Run Baseline (2-interface)

```bash
pytest python_tests/test_zzz_pack_untilize.py::test_pack_untilize -v
```

#### Run Optimized (4-interface)

```bash
pytest python_tests/test_zzz_pack_untilize.py::test_pack_untilize_4intf -v
```

#### Run Both with Specific Parameters

```bash
# Test with Float16, 64x64 input
pytest python_tests/test_zzz_pack_untilize.py \
    -k "pack_untilize" \
    --formats[input_format]=Float16 \
    --formats[output_format]=Float16 \
    --input_dimensions="[64,64]" \
    -v
```

#### Expected Outcome

✅ Both tests should pass with identical output correctness
✅ 4-interface test only runs on Blackhole architecture
✅ Both tests should produce the same row-major L1 layout

### 2. Performance Tests

#### Run Baseline Performance (2-interface)

```bash
pytest python_tests/perf_pack_untilize.py::test_perf_pack_untilize \
    --full_rt_dim=4 \
    --full_ct_dim=4 \
    --formats[input_format]=Float16 \
    --formats[output_format]=Float16 \
    -v --perf-report
```

#### Run Optimized Performance (4-interface)

```bash
pytest python_tests/perf_pack_untilize.py::test_perf_pack_untilize_4intf \
    --full_rt_dim=4 \
    --full_ct_dim=4 \
    --formats[input_format]=Float16 \
    --formats[output_format]=Float16 \
    -v --perf-report
```

#### Compare Both Versions

```bash
# Run both performance tests with same parameters
pytest python_tests/perf_pack_untilize.py \
    -k "perf_pack_untilize" \
    --full_rt_dim=4 \
    --full_ct_dim=4 \
    --formats[input_format]=Float16_b \
    --formats[output_format]=Float16 \
    -v --perf-report
```

#### Expected Performance Results

| block_ct_dim | Baseline (2-intf) | Optimized (4-intf) | Expected Speedup |
|--------------|-------------------|---------------------|------------------|
| 2 (even) | X cycles/tile | ~X/2 cycles/tile | ~2.0x |
| 4 (even) | X cycles/tile | ~X/2 cycles/tile | ~2.0x |
| 6 (even) | X cycles/tile | ~X/2 cycles/tile | ~2.0x |
| 8 (even) | X cycles/tile | ~X/2 cycles/tile | ~2.0x |
| 3 (odd) | X cycles/tile | ~X/1.8 cycles/tile | ~1.8x |
| 5 (odd) | X cycles/tile | ~X/1.8 cycles/tile | ~1.8x |
| 7 (odd) | X cycles/tile | ~X/1.8 cycles/tile | ~1.8x |

**Note**: Odd block_ct_dim has slightly lower speedup due to remainder tile handling overhead.

### 3. Run All Tests

```bash
# Run all functional tests
pytest python_tests/test_zzz_pack_untilize.py -v

# Run all performance tests
pytest python_tests/perf_pack_untilize.py -v --perf-report
```

## Understanding the Interleaved Layout

The 4-interface optimization requires input data to be in an interleaved layout:

### Standard Layout (2-interface baseline)
```
Memory: T0F0 T0F1 T0F2 T0F3 | T1F0 T1F1 T1F2 T1F3 | T2F0 T2F1 T2F2 T2F3 | ...
        ^----- Tile 0 -----^   ^----- Tile 1 -----^   ^----- Tile 2 -----^
```

### Interleaved Layout (4-interface optimized)
```
Memory: T0F0 T0F1 T1F0 T1F1 | T0F2 T0F3 T1F2 T1F3 | T2F0 T2F1 T3F0 T3F1 | ...
        ^--- Pair 0 top ---^   ^--- Pair 0 bot ---^   ^--- Pair 1 top ---^
```

The Python test handles this conversion automatically using `helpers/interleaved_layout.py`.

## Debugging Test Failures

### Functional Test Fails

#### Check Layout Conversion

```python
from helpers.interleaved_layout import validate_interleaved_conversion
import torch

# Create sample tiles
tiles = [torch.randn(32, 32) for _ in range(4)]

# Verify conversion is reversible
assert validate_interleaved_conversion(tiles)
```

#### Compare Outputs

```python
# In test_zzz_pack_untilize.py, add debug prints:
print(f"Golden tensor shape: {golden_tensor.shape}")
print(f"Result tensor shape: {res_tensor.shape}")
print(f"Golden first 10 values: {golden_tensor[:10]}")
print(f"Result first 10 values: {res_tensor[:10]}")
```

#### Check Architecture

```python
from helpers.chip_architecture import get_chip_architecture, ChipArchitecture

print(f"Running on: {get_chip_architecture()}")
# Should print: ChipArchitecture.BLACKHOLE for 4-intf tests
```

### Performance Test Shows No Improvement

#### Verify Block Size

```python
# Check what block_ct_dim is being used
print(f"block_ct_dim: {block_ct_dim}")
print(f"full_ct_dim: {full_ct_dim}")
print(f"Is even: {block_ct_dim % 2 == 0}")
```

#### Check Performance Metrics

Look for metrics in the performance report:
- **PACK_ISOLATE**: Pure packing performance (should show ~2x improvement)
- **L1_TO_L1**: End-to-end performance (includes unpack/math overhead)
- **L1_CONGESTION**: Shows L1 bandwidth utilization

#### Expected Bottlenecks

- If L1_TO_L1 shows < 2x improvement, packing might not be the bottleneck
- If PACK_ISOLATE shows ~2x but L1_TO_L1 doesn't, unpack/math dominate
- For odd block_ct_dim, remainder handling adds overhead

## Test Variations

### Test Different Block Sizes

```bash
# Test with various block sizes
for block_ct_dim in 2 4 6 8; do
    pytest python_tests/perf_pack_untilize.py \
        -k "perf_pack_untilize" \
        --full_ct_dim=$block_ct_dim \
        --full_rt_dim=1 \
        -v --perf-report
done
```

### Test Different Data Formats

```bash
# Compare Float16 vs Float32
pytest python_tests/perf_pack_untilize.py \
    -k "perf_pack_untilize_4intf" \
    --formats[input_format]=Float16 \
    --full_ct_dim=4 \
    -v --perf-report

pytest python_tests/perf_pack_untilize.py \
    -k "perf_pack_untilize_4intf" \
    --formats[input_format]=Float32 \
    --full_ct_dim=4 \
    -v --perf-report
```

### Test Odd vs Even Block Sizes

```bash
# Even block (should show ~2x speedup)
pytest python_tests/perf_pack_untilize.py::test_perf_pack_untilize_4intf \
    --full_ct_dim=4 \
    --full_rt_dim=1 \
    -v --perf-report

# Odd block (should show ~1.8x speedup)
pytest python_tests/perf_pack_untilize.py::test_perf_pack_untilize_4intf \
    --full_ct_dim=3 \
    --full_rt_dim=1 \
    -v --perf-report
```

## Analyzing Results

### Extracting Performance Data

Performance reports are typically saved in a JSON or CSV format. Look for:

```json
{
  "test_name": "test_perf_pack_untilize_4intf",
  "run_type": "PACK_ISOLATE",
  "cycles_per_tile": 123,
  "bandwidth_GB_s": 45.6,
  ...
}
```

### Calculating Speedup

```python
cycles_baseline = 250  # From test_perf_pack_untilize
cycles_optimized = 125  # From test_perf_pack_untilize_4intf

speedup = cycles_baseline / cycles_optimized
print(f"Speedup: {speedup:.2f}x")
# Expected: ~2.0x for even block_ct_dim, ~1.8x for odd
```

### Comparison Script

```python
#!/usr/bin/env python3
import json

def compare_perf_results(baseline_file, optimized_file):
    with open(baseline_file) as f:
        baseline = json.load(f)
    with open(optimized_file) as f:
        optimized = json.load(f)

    for run_type in ["PACK_ISOLATE", "L1_TO_L1", "L1_CONGESTION"]:
        base_cycles = baseline[run_type]["cycles_per_tile"]
        opt_cycles = optimized[run_type]["cycles_per_tile"]
        speedup = base_cycles / opt_cycles

        print(f"{run_type}:")
        print(f"  Baseline: {base_cycles} cycles/tile")
        print(f"  Optimized: {opt_cycles} cycles/tile")
        print(f"  Speedup: {speedup:.2f}x")
        print()

# Usage
compare_perf_results("baseline_results.json", "optimized_results.json")
```

## Known Limitations

1. **Architecture**: Currently only implemented for Blackhole (ARCH_BLACKHOLE)
2. **Tile Size**: Only supports full 32x32 tiles (num_faces = 4)
3. **Block Size**: block_ct_dim must be ≤ 16 (hardware constraint)
4. **Layout**: Requires upstream changes to produce interleaved dest layout in production

## Next Steps After Testing

### If Tests Pass

1. ✅ Verify ~2x performance improvement in PACK_ISOLATE mode
2. ✅ Compare with baseline to ensure identical functional correctness
3. ✅ Test with various data formats and block sizes
4. 📊 Document performance results in a report
5. 🚀 Consider upstream integration for dest layout generation

### If Tests Fail

1. ❌ Check error messages and architecture compatibility
2. 🔍 Verify interleaved layout conversion is correct
3. 🐛 Use debugging tips from "Debugging Test Failures" section
4. 📝 Report issues with detailed reproduction steps

## CI/CD Integration

To add these tests to continuous integration:

```yaml
# .github/workflows/llk_tests.yml
jobs:
  test_pack_untilize:
    runs-on: blackhole-runner  # Architecture-specific runner
    steps:
      - name: Run pack_untilize tests
        run: |
          pytest tests/python_tests/test_zzz_pack_untilize.py -v

      - name: Run pack_untilize performance comparison
        run: |
          pytest tests/python_tests/perf_pack_untilize.py -v --perf-report
          python scripts/compare_perf_results.py
```

## References

- [UNTILIZE_4INTF_OPTIMIZATION.md](../docs/UNTILIZE_4INTF_OPTIMIZATION.md) - Detailed explanation of all optimization approaches
- [llk_pack_untilize.h](../tt_llk_blackhole/llk_lib/llk_pack_untilize.h) - Implementation of 4-interface API
- [pack_untilize_4intf_test.cpp](sources/pack_untilize_4intf_test.cpp) - Functional test kernel
- [pack_untilize_4intf_perf.cpp](sources/pack_untilize_4intf_perf.cpp) - Performance test kernel

---

**Last Updated**: 2026-03-20
**Maintainer**: AI Assistant
**Status**: Ready for testing
