# LLK Library Operations - Wormhole B0

**File Location**: `tt_llk_wormhole_b0/llk_lib/`
**Architecture**: Wormhole B0 Tensix
**Component**: High-Level LLK API

## Overview

The LLK Library provides the primary API for neural network operations on Wormhole B0 Tensix cores. This layer abstracts hardware-specific details while providing optimal performance through template-based compile-time optimization.

## Core Definitions (`llk_defs.h`)

### Fundamental Enumerations

#### Vector Modes
```cpp
enum VectorMode {
    None      = 0,  // No vectorization
    R         = 1,  // Row-wise processing
    C         = 2,  // Column-wise processing
    RC        = 4,  // Row-Column processing
    RC_custom = 6,  // Custom row-column mode
    Invalid   = 0xFF
};
```

#### Broadcast Types
```cpp
enum BroadcastType {
    NONE   = 0x0,  // A - None || B - None
    COL    = 0x1,  // A - None || B - Col Broadcast
    ROW    = 0x2,  // A - None || B - Row Broadcast
    SCALAR = 0x3   // A - None || B - Scalar Broadcast
};
```

#### Data Format Handling
```cpp
// Stochastic rounding modes for different precision requirements
enum struct StochRndType {
    None = 0,      // No stochastic rounding (round to nearest even)
    Fpu  = 1,      // FPU accumulation rounding
    Pack = 2,      // Pack/gasket rounding
    All  = 0xf     // All stages rounding
};

// Load/Store instruction modifiers for Wormhole ISA
enum InstrModLoadStore {
    DEFAULT       = 0,   FP16A         = 1,   FP16B         = 2,
    FP32          = 3,   INT32         = 4,   INT8          = 5,
    LO16          = 6,   HI16          = 7,   INT32_2S_COMP = 12,
    INT8_2S_COMP  = 13,  LO16_ONLY     = 14, HI16_ONLY     = 15
};
```

### Operation Categories

#### Elementwise Operations
```cpp
enum EltwiseBinaryType {
    ELWMUL,   // Element-wise multiplication
    ELWDIV,   // Element-wise division
    ELWADD,   // Element-wise addition
    ELWSUB,   // Element-wise subtraction
    ELWLESS   // Element-wise less-than comparison
};

enum class EltwiseBinaryReuseDestType {
    NONE         = 0,  // No destination reuse
    DEST_TO_SRCA = 1,  // Reuse destination as source A
    DEST_TO_SRCB = 2   // Reuse destination as source B
};
```

#### Reduction Operations
```cpp
enum ReduceDim {
    REDUCE_ROW,     // Reduce along rows (column output)
    REDUCE_COL,     // Reduce along columns (row output)
    REDUCE_SCALAR   // Reduce to scalar value
};

enum PoolType {
    SUM,  // Summation pooling
    AVG,  // Average pooling
    MAX   // Max pooling
};
```

## Library Structure

### 1. Pack Operations (`llk_pack*.h`)

**Primary Functions**:
- `llk_pack.h` - Main pack operations and configuration
- `llk_pack_common.h` - Shared pack utilities and format handling
- `llk_pack_untilize.h` - Specialized untilize (transpose) operations

**Key Features**:
- **Template-based configuration** for compile-time optimization
- **Multiple data format support** (FP32, FP16, BF16, INT8)
- **Untilize mode** for efficient matrix transpose operations
- **Zero-output support** for sparsity optimizations

### 2. Unpack Operations (`llk_unpack*.h`)

**Operation Types**:
- `llk_unpack_A.h` - Source A unpack operations
- `llk_unpack_AB.h` - Dual-source unpack operations
- `llk_unpack_AB_matmul.h` - Matrix multiplication specific unpack
- `llk_unpack_tilize.h` - Tilize (format conversion) operations
- `llk_unpack_untilize.h` - Untilize operations
- `llk_unpack_reduce.h` - Reduction-specific unpack
- `llk_unpack_common.h` - Shared unpack utilities

**Configuration Options**:
- **Broadcast modes** (row, column, scalar)
- **Transpose support** for face-level operations
- **Destination targeting** (accumulate vs. overwrite)
- **Format conversion** during unpack

### 3. Math Operations (`llk_math*.h`)

**Computation Types**:
- `llk_math_matmul.h` - Matrix multiplication kernels
- `llk_math_reduce.h` - Reduction operations (sum, avg, max)
- `llk_math_eltwise_unary_sfpu.h` - Single-input SFPU operations
- `llk_math_eltwise_binary_sfpu.h` - Dual-input SFPU operations
- `llk_math_eltwise_ternary_sfpu.h` - Three-input SFPU operations
- `llk_math_eltwise_unary_datacopy.h` - Optimized data copy operations
- `llk_math_transpose_dest.h` - In-place transpose operations
- `llk_math_common.h` - Shared math utilities and synchronization

**SFPU Parameter Files**:
- `llk_math_eltwise_*_sfpu_params.h` - Template specializations and configurations

## Template System

### Compile-Time Optimization
The LLK library extensively uses C++ templates for compile-time optimization:

```cpp
// Example: Configurable pack operation
template <
    bool untilize = false,           // Enable untilize mode
    bool zero_output = false,        // Zero output optimization
    DstTileFaceLayout FaceLayout = DstTileFaceLayout::RowMajor,
    bool write_tile_header = true    // Include tile metadata
>
inline void _llk_pack_mop_config_(
    const std::uint32_t pack_dst_format,
    const std::uint32_t face_r_dim = FACE_R_DIM,
    const std::uint32_t num_faces  = 4,
    const bool partial_face        = false,
    const bool narrow_tile         = false
);
```

### Parameter Specialization
Templates allow for hardware-specific optimizations:

```cpp
// Broadcast-aware unpack configuration
template <
    BroadcastType BType = BroadcastType::NONE,
    bool acc_to_dest = false,
    EltwiseBinaryReuseDestType binary_reuse_dest = EltwiseBinaryReuseDestType::NONE,
    bool unpack_to_dest = false
>
inline void _llk_unpack_A_mop_config_(/*...*/);
```

## Operation Patterns

### Standard Workflow
```cpp
// 1. Initialize operation
llk_pack_init<DataFormat::Float16>(output_cb);

// 2. Configure hardware
_llk_pack_configure_addrmod_<false>(); // untilize = false

// 3. Execute operation
llk_pack<DstTileFaceLayout::RowMajor>(tile_index, output_cb);

// 4. Synchronize completion
llk_pack_dest_section_done<DstSync::SyncFull>();
```

### Advanced Configuration
```cpp
// Multi-source operation with broadcasting
llk_unpack_AB_matmul_init<BroadcastType::COL>(in_cb_a, in_cb_b);
_llk_unpack_AB_matmul_mop_config_<BroadcastType::COL>(
    transpose_a, transpose_b, num_faces,
    format_a, format_b, dest_format
);
llk_unpack_AB_matmul<BroadcastType::COL>(tile_a, tile_b, in_cb_a, in_cb_b);
```

## Performance Characteristics

### Cycle Costs (Typical)
| Operation Type | Cycles per Tile Face | Notes |
|----------------|---------------------|-------|
| Pack (basic) | 1-2 | Format dependent |
| Pack (untilize) | 2-4 | Additional transpose cost |
| Unpack (basic) | 1-2 | Single source |
| Unpack (broadcast) | 2-3 | Replication overhead |
| Math (basic) | 1-2 | Simple arithmetic |
| Math (matmul) | 8-16 | Complex operation |

### Memory Access Patterns
- **Sequential Access**: Optimal for basic pack/unpack
- **Strided Access**: Used in untilize operations
- **Broadcast Access**: Efficient replication for broadcast modes

## Error Handling

### Static Assertions
The library uses compile-time checks to prevent invalid configurations:

```cpp
static_assert(
    !((BType != BroadcastType::NONE) && acc_to_dest &&
      (binary_reuse_dest == EltwiseBinaryReuseDestType::DEST_TO_SRCB)),
    "Not supported configuration!"
);
```

### Debug Support
- **Instruction tracing** through LLTT (Low-Level Trace Tool)
- **Register state inspection** during operation
- **Cycle counting** for performance analysis

## Integration Guidelines

### Choosing Operations
1. **Analyze data flow**: Understand input/output requirements
2. **Select templates**: Choose appropriate compile-time parameters
3. **Configure hardware**: Set up address modes and formats
4. **Optimize pipeline**: Arrange operations for maximum throughput

### Common Pitfalls
- **Synchronization**: Ensure proper sync between pack/unpack stages
- **Format compatibility**: Verify data format consistency
- **Resource conflicts**: Avoid concurrent access to shared resources

---

**Next**: [Pack Operations Detailed Documentation](pack_operations_detailed.md)
**Related**: [Math Operations](math_operations_detailed.md), [SFPU Operations](sfpu_operations_detailed.md)
**Implementation**: Source files in `tt_llk_wormhole_b0/llk_lib/`
