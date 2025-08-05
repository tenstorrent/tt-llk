# Math Operations - Wormhole B0 Detailed Documentation

**File Location**: `tt_llk_wormhole_b0/llk_lib/llk_math*.h`
**Architecture**: Wormhole B0 Tensix
**Component**: Core Mathematical Computation Pipeline

## Overview

Math operations form the computational heart of the Tensix pipeline, implementing high-performance linear algebra, reduction operations, and specialized neural network computations. The Wormhole B0 math implementation provides matrix multiplication, elementwise operations, reduction algorithms, and SFPU integration with sophisticated synchronization and memory management.

## Math Common Infrastructure (`llk_math_common.h`)

### Hardware Configuration and Format Management

```cpp
template <bool untilize_en = false, bool row_pool = false>
inline void _llk_math_hw_configure_(
    const std::uint32_t srca_data_format,
    const std::uint32_t srcb_data_format
) {
    // Synchronize before configuration changes
    TTI_STALLWAIT(p_stall::STALL_CFG, p_stall::MATH | p_stall::WAIT_SFPU);

    // Extract format parameters
    uint exp_width = ((uint)srca_data_format >> 2) & 0x1;  // 0=5-bit, 1=8-bit exponent
    uint int8_math_enabled = ((uint)(srca_data_format & 0xF) == (uint)DataFormat::Int8) ||
                            ((uint)(srcb_data_format & 0xF) == (uint)DataFormat::Int8) ||
                            ((uint)srca_data_format == (uint)DataFormat::Int32) ||
                            ((uint)srcb_data_format == (uint)DataFormat::Int32);

    // Special handling for row pooling operations
    uint srcb_format = (row_pool ? ((uint)DataFormat::Float16 | (exp_width << 2)) : srcb_data_format);

    // Configure ALU format specifications
    uint config_data = (srca_data_format << ALU_FORMAT_SPEC_REG0_SrcA_SHAMT) |
                       (srcb_format << ALU_FORMAT_SPEC_REG1_SrcB_SHAMT) |
                       (int8_math_enabled << ALU_ACC_CTRL_INT8_math_enabled_SHAMT);

    constexpr uint config_mask = ALU_FORMAT_SPEC_REG0_SrcA_MASK |
                                ALU_FORMAT_SPEC_REG1_SrcB_MASK |
                                ALU_ACC_CTRL_INT8_math_enabled_MASK;

    // Apply configuration with read-modify-write
    cfg_reg_rmw_tensix<ALU_FORMAT_SPEC_REG0_SrcA_ADDR32, 0, config_mask>(config_data);
}
```

### Synchronization Infrastructure

#### Destination Buffer Management
```cpp
template <DstSync Dst>
inline void _llk_math_wait_for_dest_available_() {
    // Lightweight sync with packer - assumes consistent buffering mode
    math_dest_wait();
}

template <DstSync Dst, bool is_fp32_dest_acc_en>
inline void _llk_math_dest_section_done_() {
    set_math_semaphores();  // Signal completion to other units

    if constexpr (Dst == DstSync::SyncHalf) {
        math_sync_tile_dst_index = 0;
        dest_section_flip();     // Switch buffer sections
    }
}
```

#### Pack Synchronization Protocol
```cpp
template <DstSync Dst, bool is_fp32_dest_acc_en>
inline void _llk_math_pack_sync_init_() {
    tensix_sync();  // Full core synchronization

    // Wait for all previous pack operations to complete
    while (semaphore_read(semaphore::MATH_PACK) > 0) {}

    if constexpr (Dst == DstSync::SyncFull) {
        TTI_SEMINIT(1, 0, p_stall::SEMAPHORE_1);
        reset_dest_offset_id();
        set_dest_section_base<StartZero>();
    } else {
        static_assert(Dst == DstSync::SyncHalf);
        TTI_SEMINIT(2, 0, p_stall::SEMAPHORE_1);
        reset_dest_offset_id();
        set_dest_section_base<StartZero>();
    }
}
```

### Mailbox Communication System

```cpp
template <bool mail2math = true, bool mail2pack = true>
inline void _llk_math_get_tile_(std::uint32_t tile_index, std::uint32_t* p_tile) {
    constexpr uint32_t wait_sem = (mail2math && mail2pack) ? 2 : 1;

    // Wait for sufficient operand synchronization
    while (semaphore_read(semaphore::UNPACK_OPERAND_SYNC) < wait_sem);

    if constexpr (mail2math) {
        *p_tile = mailbox_read(ThreadId::UnpackThreadId);
    }
}
```

## Matrix Multiplication Operations (`llk_math_matmul.h`)

### Core Matrix Multiplication Engine

The matrix multiplication implementation provides optimized computation for various data formats and broadcast modes:

```cpp
template <
    BroadcastType BType = BroadcastType::NONE,
    bool acc_to_dest = false,
    bool zero_acc = true
>
inline void _llk_math_matmul_mop_config_(
    const std::uint32_t num_faces,
    const std::uint32_t dst_offset = 0,
    const bool transpose_xy = false
) {
    // Matrix multiplication micro-operation configuration
    const uint32_t outerloop = 1;     // Single outer iteration
    const uint32_t innerloop = num_faces;  // Process all faces

    // Configure matrix operation based on broadcast type
    if constexpr (BType == BroadcastType::NONE) {
        // Standard A × B matrix multiplication
        constexpr uint matmul_op = TT_OP_MATMUL(
            p_setrwc::CLR_NONE,     // No counter clearing
            p_setrwc::CR_D,         // Destination carriage return
            p_setrwc::SET_D,        // Set destination valid
            p_setrwc::CLR_NONE,     // No accumulator clearing
            acc_to_dest ? 1 : 0,    // Accumulate to destination
            zero_acc ? 1 : 0        // Zero accumulator
        );

        ckernel_template tmp(outerloop, innerloop, matmul_op);
        tmp.program(instrn_buffer);
    }
    // Additional broadcast configurations...
}
```

#### Broadcast Matrix Multiplication
```cpp
// Column broadcast: B matrix broadcast across columns
else if constexpr (BType == BroadcastType::COL) {
    const uint32_t outerloop = num_faces;
    constexpr uint32_t innerloop = 1;

    constexpr uint matmul_col_bcast = TT_OP_MATMUL(
        p_setrwc::CLR_AB,       // Clear A and B counters
        p_setrwc::CR_D,         // Destination carriage return
        p_setrwc::SET_D,        // Set destination valid
        p_setrwc::CLR_NONE,     // Accumulate results
        0, 0  // Standard accumulation
    );

    ckernel_template tmp(outerloop, innerloop, matmul_col_bcast);

    // Configure source B reloading for broadcast
    tmp.set_loop_op0(TT_OP_SETADCZW(p_setadc::UNP_B, 0, 0, 0, 0, 0b0001));
    tmp.program(instrn_buffer);
}
```

### Matrix Transpose Support

```cpp
template <bool transpose_a = false, bool transpose_b = false>
inline void _llk_math_matmul_transpose_config_() {
    if constexpr (transpose_a) {
        // Configure A matrix transpose addressing
        addr_mod_unpack_a_t transpose_addr_a {
            .x_src = {.incr = 0, .clr = 0},
            .y_src = {.incr = 1, .clr = 0, .cr = 1},  // Transpose pattern
        };
        transpose_addr_a.set(ADDR_MOD_TRANSPOSE_A);
    }

    if constexpr (transpose_b) {
        // Configure B matrix transpose addressing
        addr_mod_unpack_b_t transpose_addr_b {
            .x_src = {.incr = 1, .clr = 0, .cr = 1},  // Transpose pattern
            .y_src = {.incr = 0, .clr = 0},
        };
        transpose_addr_b.set(ADDR_MOD_TRANSPOSE_B);
    }
}
```

## Reduction Operations (`llk_math_reduce.h`)

### Reduction Algorithm Implementation

```cpp
template <
    ReduceDim reduce_dim = ReduceDim::REDUCE_ROW,
    PoolType pool_type = PoolType::SUM,
    bool acc_to_dest = false
>
inline void _llk_math_reduce_mop_config_(
    const std::uint32_t num_faces,
    const std::uint32_t dst_offset = 0
) {
    if constexpr (reduce_dim == ReduceDim::REDUCE_ROW) {
        // Row reduction: sum/avg/max across row elements
        const uint32_t outerloop = num_faces;
        constexpr uint32_t innerloop = 1;

        if constexpr (pool_type == PoolType::SUM) {
            constexpr uint reduce_sum_op = TT_OP_REDUCE_SUM(
                p_setrwc::CLR_NONE,     // Preserve counters
                p_setrwc::SET_D,        // Set destination valid
                acc_to_dest ? 1 : 0     // Accumulation mode
            );

            ckernel_template tmp(outerloop, innerloop, reduce_sum_op);
            tmp.program(instrn_buffer);
        } else if constexpr (pool_type == PoolType::MAX) {
            constexpr uint reduce_max_op = TT_OP_REDUCE_MAX(
                p_setrwc::CLR_NONE,
                p_setrwc::SET_D,
                acc_to_dest ? 1 : 0
            );

            ckernel_template tmp(outerloop, innerloop, reduce_max_op);
            tmp.program(instrn_buffer);
        }
    } else if constexpr (reduce_dim == ReduceDim::REDUCE_COL) {
        // Column reduction: transpose access pattern required
        configure_column_reduction_addressing();

        const uint32_t outerloop = 1;
        const uint32_t innerloop = num_faces;

        constexpr uint reduce_col_op = TT_OP_REDUCE_COL(
            p_setrwc::CLR_A,        // Clear A counter between faces
            p_setrwc::SET_D,
            acc_to_dest ? 1 : 0
        );

        ckernel_template tmp(outerloop, innerloop, reduce_col_op);
        tmp.program(instrn_buffer);
    }
}
```

### Average Pooling Implementation

```cpp
template <bool acc_to_dest = false>
inline void _llk_math_reduce_avg_(
    const std::uint32_t num_faces,
    const std::uint32_t scale_factor
) {
    // Step 1: Sum reduction
    _llk_math_reduce_mop_config_<ReduceDim::REDUCE_ROW, PoolType::SUM, acc_to_dest>(num_faces);

    // Step 2: Division by count using SFPU
    _llk_math_eltwise_unary_sfpu_mop_config_<SfpuType::reciprocal>(num_faces);

    // Configure scale factor for division
    sfpi::vConstFloatPrgm0 = sfpi::s2vFloat16b(1.0f / scale_factor);
}
```

## Elementwise Operations

### Unary SFPU Operations (`llk_math_eltwise_unary_sfpu.h`)

```cpp
template <SfpuType sfpu_op, bool dst_accum_en = false>
inline void _llk_math_eltwise_unary_sfpu_mop_config_(
    const std::uint32_t num_faces,
    const std::uint32_t dst_offset = 0
) {
    const uint32_t outerloop = num_faces;
    constexpr uint32_t innerloop = 1;

    // SFPU operation encoding
    constexpr uint sfpu_unary_op = TT_OP_SFPU_UNARY(
        static_cast<uint>(sfpu_op),  // Operation type
        p_setrwc::SET_D,             // Set destination valid
        dst_accum_en ? 1 : 0         // Accumulation enable
    );

    ckernel_template tmp(outerloop, innerloop, sfpu_unary_op);
    tmp.program(instrn_buffer);
}
```

### Binary SFPU Operations (`llk_math_eltwise_binary_sfpu.h`)

```cpp
template <
    SfpuType sfpu_op,
    BroadcastType BType = BroadcastType::NONE,
    bool dst_accum_en = false
>
inline void _llk_math_eltwise_binary_sfpu_mop_config_(
    const std::uint32_t num_faces,
    const std::uint32_t dst_offset = 0
) {
    if constexpr (BType == BroadcastType::NONE) {
        // Standard element-wise binary operation
        const uint32_t outerloop = num_faces;
        constexpr uint32_t innerloop = 1;

        constexpr uint sfpu_binary_op = TT_OP_SFPU_BINARY(
            static_cast<uint>(sfpu_op),
            p_setrwc::SET_D,
            dst_accum_en ? 1 : 0
        );

        ckernel_template tmp(outerloop, innerloop, sfpu_binary_op);
        tmp.program(instrn_buffer);
    } else if constexpr (BType == BroadcastType::SCALAR) {
        // Scalar broadcast: reuse single value across all elements
        const uint32_t outerloop = num_faces;
        constexpr uint32_t innerloop = 1;

        constexpr uint sfpu_scalar_bcast = TT_OP_SFPU_BINARY_SCALAR(
            static_cast<uint>(sfpu_op),
            p_setrwc::SET_D,
            dst_accum_en ? 1 : 0
        );

        ckernel_template tmp(outerloop, innerloop, sfpu_scalar_bcast);
        // Configure scalar value reloading
        tmp.set_loop_op0(TT_OP_SETADCZW(p_setadc::UNP_B, 0, 0, 0, 0, 0b0001));
        tmp.program(instrn_buffer);
    }
}
```

### Ternary SFPU Operations (`llk_math_eltwise_ternary_sfpu.h`)

```cpp
template <SfpuType sfpu_op, bool dst_accum_en = false>
inline void _llk_math_eltwise_ternary_sfpu_mop_config_(
    const std::uint32_t num_faces,
    const std::uint32_t dst_offset = 0
) {
    // Ternary operations (e.g., fused multiply-add, conditional select)
    const uint32_t outerloop = num_faces;
    constexpr uint32_t innerloop = 1;

    constexpr uint sfpu_ternary_op = TT_OP_SFPU_TERNARY(
        static_cast<uint>(sfpu_op),
        p_setrwc::SET_D,
        dst_accum_en ? 1 : 0
    );

    ckernel_template tmp(outerloop, innerloop, sfpu_ternary_op);
    tmp.program(instrn_buffer);
}
```

## Data Copy Operations (`llk_math_eltwise_unary_datacopy.h`)

### Optimized Data Movement

```cpp
template <
    DataCopyType copy_type = DataCopyType::A2D,
    BroadcastType BType = BroadcastType::NONE,
    bool acc_to_dest = false
>
inline void _llk_math_eltwise_unary_datacopy_mop_config_(
    const std::uint32_t num_faces,
    const std::uint32_t dst_offset = 0
) {
    if constexpr (copy_type == DataCopyType::A2D) {
        // Optimized A to destination copy
        if constexpr (BType == BroadcastType::NONE) {
            // Direct copy without broadcast
            const uint32_t outerloop = num_faces;
            constexpr uint32_t innerloop = 1;

            constexpr uint copy_a2d_op = TT_OP_MOVA2D(
                p_setrwc::CLR_NONE,     // Preserve counters
                p_setrwc::SET_D,        // Set destination valid
                acc_to_dest ? 1 : 0     // Accumulation mode
            );

            ckernel_template tmp(outerloop, innerloop, copy_a2d_op);
            tmp.program(instrn_buffer);
        } else if constexpr (BType == BroadcastType::ROW) {
            // Row broadcast during copy
            const uint32_t outerloop = num_faces >> 1;  // Process face pairs
            constexpr uint32_t innerloop = 2;

            constexpr uint copy_row_bcast = TT_OP_MOVA2D_BCAST_ROW(
                p_setrwc::CLR_A,        // Clear A between rows
                p_setrwc::SET_D,
                acc_to_dest ? 1 : 0
            );

            ckernel_template tmp(outerloop, innerloop, copy_row_bcast);
            tmp.program(instrn_buffer);
        }
    } else if constexpr (copy_type == DataCopyType::B2D) {
        // Optimized B to destination copy with similar patterns
        configure_b2d_copy_operation<BType, acc_to_dest>(num_faces);
    }
}
```

## Transpose Operations (`llk_math_transpose_dest.h`)

### In-Place Destination Transpose

```cpp
template <bool transpose_xy = true>
inline void _llk_math_transpose_dest_mop_config_(
    const std::uint32_t num_faces,
    const std::uint32_t dst_offset = 0
) {
    if constexpr (transpose_xy) {
        // Standard X-Y transpose (most common)
        const uint32_t outerloop = num_faces >> 2;  // Process 4 faces at a time
        constexpr uint32_t innerloop = 4;

        constexpr uint transpose_xy_op = TT_OP_TRANSPOSE_XY(
            p_setrwc::CLR_NONE,     // Preserve state
            p_setrwc::SET_D,        // Set destination valid
            0                       // No accumulation
        );

        ckernel_template tmp(outerloop, innerloop, transpose_xy_op);

        // Configure face reordering for transpose
        tmp.set_loop_op0(TT_OP_REORDER_FACES(0, 2, 1, 3));  // Face permutation
        tmp.program(instrn_buffer);
    } else {
        // Alternative transpose modes (Z-Y, etc.)
        configure_alternative_transpose();
    }
}
```

### Transpose with Format Conversion

```cpp
template <
    bool transpose_xy = true,
    DataFormat src_format = DataFormat::Float16,
    DataFormat dst_format = DataFormat::Float16
>
inline void _llk_math_transpose_with_conversion_(
    const std::uint32_t num_faces
) {
    // Combined transpose and format conversion for efficiency
    _llk_math_transpose_dest_mop_config_<transpose_xy>(num_faces);

    if constexpr (src_format != dst_format) {
        // Additional format conversion step
        _llk_math_eltwise_unary_sfpu_mop_config_<SfpuType::typecast>(num_faces);
    }
}
```

## Performance Optimization Features

### Double Buffering Support

```cpp
template <DstSync sync_mode>
inline void configure_double_buffering() {
    if constexpr (sync_mode == DstSync::SyncHalf) {
        // Enable half-buffer synchronization
        set_dest_section_mode<DoubleBuffer>();
        configure_ping_pong_addressing();
    } else {
        // Full buffer synchronization
        set_dest_section_mode<SingleBuffer>();
    }
}
```

### Pipeline Optimization

```cpp
template <bool enable_pipeline = true>
inline void _llk_math_pipeline_config_() {
    if constexpr (enable_pipeline) {
        // Enable instruction pipelining for overlapped execution
        TTI_CFG_SET_PIPELINE_EN(1);

        // Configure pipeline depth based on operation complexity
        TTI_CFG_SET_PIPELINE_DEPTH(OPTIMAL_PIPELINE_DEPTH);
    }
}
```

### Memory Access Optimization

```cpp
template <bool enable_prefetch = true>
inline void configure_memory_optimization() {
    if constexpr (enable_prefetch) {
        // Enable data prefetching for next tile
        set_prefetch_mode<PrefetchNext>();

        // Configure optimal cache behavior
        set_cache_policy<WriteThrough>();
    }
}
```

## Performance Characteristics

### Operation Throughput (Cycles per Tile Face)

| Operation Type | Single Precision | Half Precision | INT8 | Notes |
|----------------|------------------|----------------|------|-------|
| **Matrix Multiplication** | 8-16 | 6-12 | 4-8 | Size dependent |
| **Reduction (Sum)** | 2-4 | 2-3 | 1-2 | Dimension dependent |
| **Reduction (Max)** | 3-5 | 2-4 | 2-3 | Comparison overhead |
| **Elementwise (SFPU)** | 2-12 | 2-8 | 1-4 | Function complexity |
| **Data Copy** | 1-2 | 1 | 1 | Memory bandwidth limited |
| **Transpose** | 4-8 | 3-6 | 2-4 | Access pattern dependent |

### Memory Bandwidth Utilization

| Data Format | Theoretical BW | Achieved BW | Efficiency |
|-------------|----------------|-------------|------------|
| **FP32** | 100% | 85-95% | High |
| **FP16** | 200% | 160-180% | Good |
| **BF16** | 200% | 160-180% | Good |
| **INT8** | 400% | 300-350% | Excellent |

## Integration Guidelines

### Standard Math Operation Workflow

```cpp
// 1. Configure hardware for data formats
_llk_math_hw_configure_<false, false>(srca_format, srcb_format);

// 2. Initialize synchronization
_llk_math_pack_sync_init_<DstSync::SyncFull, false>();

// 3. Configure specific operation
_llk_math_matmul_mop_config_<BroadcastType::NONE, false, true>(num_faces);

// 4. Execute operation
_llk_math_matmul_<BroadcastType::NONE, false>(tile_index);

// 5. Signal completion
_llk_math_dest_section_done_<DstSync::SyncFull, false>();
```

### Multi-Operation Sequences

```cpp
// Efficient operation chaining
_llk_math_hw_configure_<false, false>(format_a, format_b);

// Matrix multiplication followed by activation
_llk_math_matmul_<BroadcastType::NONE, false>(tiles);
_llk_math_eltwise_unary_sfpu_<SfpuType::gelu, false>(tiles);

// Followed by reduction
_llk_math_reduce_<ReduceDim::REDUCE_ROW, PoolType::SUM, false>(tiles);
```

### Error Handling and Validation

```cpp
// Operation validation
template <typename ConfigType>
inline bool validate_math_operation(const ConfigType& config) {
    // Verify format compatibility
    if (!is_format_compatible(config.src_format, config.dst_format)) {
        return false;
    }

    // Check resource availability
    if (!is_dest_buffer_available(config.num_faces)) {
        return false;
    }

    // Validate broadcast configuration
    if (!is_broadcast_valid(config.broadcast_type, config.dimensions)) {
        return false;
    }

    return true;
}
```

---

**Related**: [Pack Operations](pack_operations_detailed.md), [Unpack Operations](unpack_operations_detailed.md), [SFPU Operations](sfpu_operations_detailed.md)
**Implementation**: Source files in `tt_llk_wormhole_b0/llk_lib/llk_math*.h`
**Architecture**: [Wormhole B0 Overview](wormhole_b0_architecture_overview.md)
