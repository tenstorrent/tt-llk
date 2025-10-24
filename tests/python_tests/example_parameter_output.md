# Example Parameter Coverage Output

This document shows examples of what the parameter coverage analysis tools generate.

## Example 1: JSON Output Structure

```json
{
  "total_tests": 547,
  "total_parametrized": 423,
  "tests": [
    {
      "test_id": "test_math_matmul.py::test_math_matmul[math_matmul_test-LoFi-config0-1]",
      "file": "tests/python_tests/test_math_matmul.py",
      "test_name": "test_math_matmul[math_matmul_test-LoFi-config0-1]",
      "function": "test_math_matmul",
      "class": null,
      "module": "test_math_matmul",
      "parameters": {
        "test_name": "math_matmul_test",
        "math_fidelity": "MathFidelity.LoFi",
        "matmul_config": "MatmulConfig(...)",
        "throttle": "1"
      }
    },
    // ... more tests
  ],
  "unique_parameter_values": {
    "formats": [
      "InputOutputFormat(Float16_b, Float16_b)",
      "InputOutputFormat(Float16_b, Float16)",
      "InputOutputFormat(Float16_b, Float32)",
      "InputOutputFormat(Float16, Float16_b)",
      // ... more format combinations
    ],
    "dest_acc": ["DestAccumulation.No", "DestAccumulation.Yes"],
    "math_fidelity": ["MathFidelity.LoFi", "MathFidelity.HiFi2", "MathFidelity.HiFi3", "MathFidelity.HiFi4"],
    "reduce_dim": ["ReduceDimension.Row", "ReduceDimension.Column", "ReduceDimension.Scalar"],
    "pool_type": ["ReducePool.Max", "ReducePool.Average", "ReducePool.Sum"],
    // ... more parameters
  },
  "parameter_usage_count": {
    "formats": 156,
    "dest_acc": 142,
    "math_fidelity": 89,
    "reduce_dim": 45,
    "pool_type": 45,
    "test_name": 423,
    "throttle": 89,
    "num_faces": 36,
    "tilize_en": 24
  }
}
```

## Example 2: CSV Output (Flattened View)

| test_id | file | test_name | class | param_formats | param_dest_acc | param_math_fidelity | param_reduce_dim |
|---------|------|-----------|-------|---------------|----------------|---------------------|------------------|
| test_math_matmul.py::test_math_matmul[...] | tests/python_tests/test_math_matmul.py | test_math_matmul[...] | None | InputOutputFormat(Float16_b, Float16_b) | DestAccumulation.No | MathFidelity.LoFi | |
| test_reduce.py::test_reduce[...] | tests/python_tests/test_reduce.py | test_reduce[...] | None | InputOutputFormat(Float16_b, Float16_b) | DestAccumulation.No | | ReduceDimension.Row |
| test_eltwise_unary_datacopy.py::test_unary_datacopy[...] | tests/python_tests/test_eltwise_unary_datacopy.py | test_unary_datacopy[...] | None | InputOutputFormat(Float32, Float32) | DestAccumulation.Yes | | |

## Example 3: Excel Workbook Sheets

### Sheet 1: Tests
Complete list of all tests with their parameter values (similar to CSV above)

### Sheet 2: Parameter Summary
| parameter | unique_values | usage_count | sample_values |
|-----------|---------------|-------------|---------------|
| formats | 8 | 156 | InputOutputFormat(Float16_b, Float16_b), InputOutputFormat(Float16_b, Float16), ... |
| dest_acc | 2 | 142 | DestAccumulation.No, DestAccumulation.Yes |
| math_fidelity | 4 | 89 | MathFidelity.LoFi, MathFidelity.HiFi2, MathFidelity.HiFi3, MathFidelity.HiFi4 |
| reduce_dim | 3 | 45 | ReduceDimension.Row, ReduceDimension.Column, ReduceDimension.Scalar |
| pool_type | 3 | 45 | ReducePool.Max, ReducePool.Average, ReducePool.Sum |

### Sheet 3: Coverage Matrix
| test | formats | dest_acc | math_fidelity | reduce_dim | pool_type | throttle | num_faces |
|------|---------|----------|---------------|------------|-----------|----------|-----------|
| test_math_matmul | ✓ | ✓ | ✓ | | | ✓ | |
| test_reduce | ✓ | ✓ | | ✓ | ✓ | | |
| test_unary_datacopy | ✓ | ✓ | | | | | ✓ |
| test_eltwise_unary_sfpu | ✓ | ✓ | | | | | |

## Example 4: Summary Report (Text)

```
TEST PARAMETER SUMMARY
==================================================

Total tests collected: 547
Parametrized tests: 423
Unique parameters: 12

Parameter Usage:
------------------------------
  formats: 156 tests (8 unique values)
  dest_acc: 142 tests (2 unique values)
  math_fidelity: 89 tests (4 unique values)
  reduce_dim: 45 tests (3 unique values)
  pool_type: 45 tests (3 unique values)
  test_name: 423 tests (27 unique values)
  throttle: 89 tests (5 unique values)
  num_faces: 36 tests (3 unique values)
  tilize_en: 24 tests (2 unique values)
  matmul_config: 89 tests (45 unique values)
  dest_sync: 65 tests (2 unique values)
  stochastic_rnd: 89 tests (1 unique values)


Parameter Value Samples:
------------------------------

dest_acc:
  - DestAccumulation.No
  - DestAccumulation.Yes

formats:
  - InputOutputFormat(Bfp8_b, Bfp8_b)
  - InputOutputFormat(Bfp8_b, Float16)
  - InputOutputFormat(Bfp8_b, Float16_b)
  - InputOutputFormat(Bfp8_b, Float32)
  - InputOutputFormat(Float16, Bfp8_b)
  - InputOutputFormat(Float16, Float16)
  - InputOutputFormat(Float16, Float16_b)
  - InputOutputFormat(Float16, Float32)

math_fidelity:
  - MathFidelity.HiFi2
  - MathFidelity.HiFi3
  - MathFidelity.HiFi4
  - MathFidelity.LoFi

num_faces:
  - 1
  - 2
  - 4

pool_type:
  - ReducePool.Average
  - ReducePool.Max
  - ReducePool.Sum

reduce_dim:
  - ReduceDimension.Column
  - ReduceDimension.Row
  - ReduceDimension.Scalar

... and more parameters
```

## Key Benefits

1. **Complete Coverage View**: See all parameter combinations being tested
2. **Gap Analysis**: Identify missing parameter combinations
3. **Test Planning**: Use the coverage matrix to plan new tests
4. **Documentation**: Auto-generated documentation of test parameters
5. **CI Integration**: Can be automated in CI pipelines to track coverage over time
