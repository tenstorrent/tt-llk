# reduce_block_max_row Test Implementation

## Overview
This document describes the implementation of `reduce_block_max_row` tests in the tt-llk test infrastructure, based on the API traced from tt-metal.

## API Tracing Path

### From tt-metal compute kernel to tt-llk
1. **User Code**: `tt-metal/tests/tt_metal/tt_metal/test_kernels/misc/sdpa/reduce_block_max_row/compute.cpp`
   - Uses: `reduce_block_max_row_init<cols>()`, `reduce_block_max_row<cols>(...)`, `reduce_block_max_row_uninit()`

2. **API Header**: `tt-metal/tt_metal/include/compute_kernel_api/reduce_custom.h`
   - Defines high-level API functions that call into LLK

3. **LLK Implementation** (in tt-metal third_party):
   - Math: `tt-metal/tt_metal/third_party/tt_llk/tt_llk_blackhole/llk_lib/llk_math_reduce_custom.h`
   - Unpack: `tt-metal/tt_metal/third_party/tt_llk/tt_llk_blackhole/llk_lib/llk_unpack_AB_reduce_custom.h`

4. **LLK Source** (in tt-llk):
   - Math: `tt-llk/tt_llk_blackhole/llk_lib/llk_math_reduce_custom.h`
   - Unpack: `tt-llk/tt_llk_blackhole/llk_lib/llk_unpack_AB_reduce_custom.h`

## Parameter Mapping

### From tt-metal test_sdpa_reduce_c.cpp sweep parameters:

| tt-metal Parameter | Value Range | tt-llk Parameter | Notes |
|-------------------|-------------|------------------|-------|
| `k_chunk_size` (Sk_chunk_t) | {1, 2, 4, 8, 16} | `block_ct_dim` | Number of tiles in width dimension processed as a block |
| `q_chunk_size` (Sq_chunk_t) | {1, 2, 4, 8} | N/A | Not swept in LLK tests (focuses on single row processing) |
| `fp32_dest_acc_en` | {false, true} | `dest_acc` | 16-bit (No) or 32-bit (Yes) destination accumulation |
| `do_eltwise_max` | {false, true} | N/A | Test variant in tt-metal (max with previous max tile) |
| Data Format | Float16_b | Float16_b | API constraint: only bfloat16_b supported |

### Key API Constraints (from reduce_custom.h documentation):
- Scaler values must be 1.0 in F0 of scaler tile
- Data format must be bfloat16_b (Float16_b)
- Tile size is 32x32
- Only MAX pool on ROW dimension
- Can work in both 16-bit and 32-bit DEST modes

## Test Files Created

### Python Tests

1. **Performance Test**: `tests/python_tests/perf_reduce_block_max_row.py`
   - Sweeps: `block_ct_dim` = {1, 2, 4, 8, 16}
   - Sweeps: `dest_acc` = {No, Yes}
   - Format: Float16_b only
   - Run types: L1_TO_L1, UNPACK_ISOLATE, MATH_ISOLATE, PACK_ISOLATE, L1_CONGESTION

2. **Functional Test**: `tests/python_tests/test_reduce_block_max_row.py`
   - Same parameter sweeps as performance test
   - Includes golden reference verification
   - Uses `ReduceBlockMaxRowGolden` for expected output generation

### C++ Tests

1. **Performance Test**: `tests/sources/reduce_block_max_row_perf.cpp`
   - Direct LLK calls for performance measurement
   - Supports all performance isolation modes
   - Matches the structure of existing `reduce_perf.cpp`

2. **Functional Test**: `tests/sources/reduce_block_max_row_test.cpp`
   - Direct LLK calls for functional verification
   - Matches the structure of existing `reduce_test.cpp`

### Supporting Code

1. **Golden Generator**: `tests/python_tests/helpers/golden_generators.py`
   - Added `ReduceBlockMaxRowGolden` class
   - Implements block-based max row reduction
   - Takes `block_ct_dim` tiles horizontally, finds max per row, places in first column

2. **Template Parameter**: `tests/python_tests/helpers/test_variant_parameters.py`
   - Added `BLOCK_CT_DIM` class for template parameter generation

## Operation Description

The `reduce_block_max_row` operation:
1. Takes `block_ct_dim` input tiles arranged horizontally (32 rows × 32×block_ct_dim cols)
2. Processes all tiles as a single block using optimized MOP (Macro Operation)
3. Finds maximum value in each row across all `block_ct_dim` tiles
4. Returns a single output tile (32×32) with:
   - Max values placed in first column (one per row)
   - Remaining columns are typically zero (not used)

### Tile Layout
For a 32×32 tile stored as 4 faces of 16×16:
- **F0**: rows 0-15, cols 0-15
- **F1**: rows 0-15, cols 16-31
- **F2**: rows 16-31, cols 0-15
- **F3**: rows 16-31, cols 16-31

First column results:
- Rows 0-15: F0 column 0
- Rows 16-31: F2 column 0

## Important Implementation Notes

### Profiler Zone Synchronization
The performance test requires that profiler zones (ZONE_SCOPED) match exactly across unpack, math, and pack kernels. The `reduce_block_max_row` operation requires init and uninit calls, but these uninit calls are placed **outside** the profiled zones to avoid zone mismatch errors. Only two zones are used:
1. `INIT` - for initialization
2. `TILE_LOOP` - for the main operation

The uninit calls are made after the TILE_LOOP zone completes.

## Running the Tests

### Python Performance Test
```bash
cd tt-llk/tests/python_tests
pytest perf_reduce_block_max_row.py -v
```

### Python Functional Test
```bash
cd tt-llk/tests/python_tests
pytest test_reduce_block_max_row.py -v
```

### Specific Parameter Test
```bash
pytest test_reduce_block_max_row.py -v -k "block_ct_dim-8"
```

## Differences from Standard Reduce

| Aspect | Standard Reduce | reduce_block_max_row |
|--------|----------------|---------------------|
| Tile Processing | One tile at a time in loop | Block of tiles in single operation |
| Flexibility | Any reduce type/dimension | MAX on ROW only |
| Data Format | Multiple formats | bfloat16_b only |
| Use Case | General-purpose | Optimized for SDPA attention |
| Performance | Multiple LLK calls | Single block operation with MOP |

## References

- **tt-metal test**: `tt-metal/tests/tt_metal/tt_metal/test_sdpa_reduce_c.cpp`
- **tt-metal compute kernel**: `tt-metal/tests/tt_metal/tt_metal/test_kernels/misc/sdpa/reduce_block_max_row/compute.cpp`
- **API definition**: `tt-metal/tt_metal/include/compute_kernel_api/reduce_custom.h`
- **Existing reduce tests**:
  - `tt-llk/tests/python_tests/perf_reduce.py`
  - `tt-llk/tests/python_tests/test_reduce.py`
  - `tt-llk/tests/sources/reduce_perf.cpp`
  - `tt-llk/tests/sources/reduce_test.cpp`
