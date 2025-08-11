# Unpack Operations - Wormhole B0 Detailed Documentation

**File Location**: `tt_llk_wormhole_b0/llk_lib/llk_unpack*.h`
**Architecture**: Wormhole B0 Tensix
**Component**: Data Input and Preprocessing Pipeline

## Overview

Unpack operations form the input stage of the Tensix compute pipeline, responsible for reading data from L1 memory, performing format conversions, and feeding preprocessed data to the math pipeline. The Wormhole B0 unpack implementation provides sophisticated broadcast support, transpose operations, and multi-source coordination.

## Core Unpack Operations

### Single Source Unpack (`llk_unpack_A.h`)

The primary unpack operation handles single-source data loading with comprehensive broadcast and format conversion support:

```cpp
template <
    BroadcastType BType = BroadcastType::NONE,
    bool acc_to_dest = false,
    EltwiseBinaryReuseDestType binary_reuse_dest = EltwiseBinaryReuseDestType::NONE,
    bool unpack_to_dest = false
>
inline void _llk_unpack_A_mop_config_(
    const bool transpose_of_faces,
    const std::uint32_t num_faces,
    const std::uint32_t unpack_src_format,
    const std::uint32_t unpack_dst_format = 0
);
```

#### Static Instruction Definitions
```cpp
// Core unpack instructions for Source A
static constexpr uint unpack_srca =
    TT_OP_UNPACR(SrcA, 0b1 /*Z inc*/, 0, 0, 0, 1 /*Set OvrdThreadId*/, 1 /*Set Dvalid*/,
                 p_unpacr::RAREFYB_DISABLE, 0, 0, 0, 0, 1);

// Unpack to destination with dual-channel Z increment
static constexpr uint unpack_srca_to_dest =
    TT_OP_UNPACR(SrcA, 0b00010001 /*Z inc*/, 0, 0, 0, 1, 0,
                 p_unpacr::RAREFYB_DISABLE, 0, 0, 0, 0, 1); // ch0/ch1 z_inc

// Transpose-aware unpack for face reordering
static constexpr uint unpack_srca_to_dest_transpose_of_faces =
    TT_OP_UNPACR(SrcA, 0b00010010, 0, 0, 0, 1, 0,
                 p_unpacr::RAREFYB_DISABLE, 0, 0, 0, 0, 1); // inc srcA ch1_z+=1, ch0_z+=2
```

#### MOP Configuration Patterns

**32-bit Input with Transpose Support**:
```cpp
if (unpack_to_dest && is_32bit_input(unpack_src_format, unpack_dst_format)) {
    if (transpose_of_faces && num_faces == 4) {
        const uint32_t outerloop = 2;
        const uint32_t innerloop = 2;
        ckernel_template tmp(outerloop, innerloop, unpack_srca_to_dest_transpose_of_faces);
        tmp.set_end_op(TT_OP_SETADCZW(p_setadc::UNP_A, 0, 2, 0, 1, 0b0101));
        tmp.program(instrn_buffer);
    } else {
        const uint32_t outerloop = num_faces;
        constexpr uint32_t innerloop = 1;
        ckernel_template tmp(outerloop, innerloop, unpack_srca_to_dest);
        tmp.program(instrn_buffer);
    }
}
```

**Column Broadcast Mode**:
```cpp
else if constexpr (BType == BroadcastType::COL) {
    constexpr uint32_t innerloop = 1;
    const uint32_t outerloop = num_faces;

    ckernel_template tmp(outerloop, innerloop, unpack_srca);
    tmp.set_start_op(TT_OP_SETADCZW(p_setadc::UNP_A, 0, 0, 0, 1, 0b0001));  // Set srcA ch0_z = 1
    tmp.set_loop_op0(unpack_srca_zerosrc_set_dvalid);  // Zero source + set valid
    tmp.set_loop_op1(unpack_srcb_unpack_srcb);         // Dual source B unpack
    tmp.set_end_op(TT_OP_SETADCZW(p_setadc::UNP_A, 0, 0, 0, 0, 0b0001));   // Reset srcA ch0_z = 0
    tmp.program(instrn_buffer);
}
```

### Dual Source Unpack (`llk_unpack_AB.h`)

Handles coordinated unpacking of two data sources for binary operations:

```cpp
template <BroadcastType BType = BroadcastType::NONE>
inline void _llk_unpack_AB_mop_config_(
    const std::uint32_t num_faces,
    const std::uint32_t unpack_src_format_a,
    const std::uint32_t unpack_src_format_b = 0,
    const std::uint32_t unpack_dst_format = 0
) {
    // Coordinate dual-source unpacking with broadcast support
    constexpr uint unpack_srca = TT_OP_UNPACR(SrcA, 0b1, 0, 0, 0, 1, 1, p_unpacr::RAREFYB_DISABLE, 0, 0, 0, 0, 1);
    constexpr uint unpack_srcb = TT_OP_UNPACR(SrcB, 0b1, 0, 0, 0, 1, 1, p_unpacr::RAREFYB_DISABLE, 0, 0, 0, 0, 1);

    if constexpr (BType == BroadcastType::NONE) {
        // Standard dual-source operation
        const uint32_t outerloop = num_faces;
        constexpr uint32_t innerloop = 1;
        ckernel_template tmp(outerloop, innerloop, unpack_srca);
        tmp.set_loop_op0(unpack_srcb);
        tmp.program(instrn_buffer);
    }
    // Additional broadcast configurations...
}
```

### Matrix Multiplication Unpack (`llk_unpack_AB_matmul.h`)

Specialized unpack for matrix multiplication with optimized data access patterns:

```cpp
template <BroadcastType BType = BroadcastType::NONE, bool acc_to_dest = false>
inline void _llk_unpack_AB_matmul_mop_config_(
    const bool transpose_a_faces,
    const bool transpose_b_faces,
    const std::uint32_t num_faces,
    const std::uint32_t unpack_src_format_a,
    const std::uint32_t unpack_src_format_b,
    const std::uint32_t unpack_dst_format = 0
) {
    // Matrix-specific access patterns
    constexpr uint unpack_srca_matmul = TT_OP_UNPACR(SrcA, 0b0, 0, 0, 0, 1, 1, p_unpacr::RAREFYB_DISABLE, 0, 0, 0, 0, 1);
    constexpr uint unpack_srcb_matmul = TT_OP_UNPACR(SrcB, 0b0, 0, 0, 0, 1, 1, p_unpacr::RAREFYB_DISABLE, 0, 0, 0, 0, 1);

    // Optimized for matrix multiplication access patterns
    constexpr uint32_t innerloop = 1;
    const uint32_t outerloop = 1;
    ckernel_template tmp(outerloop, innerloop, unpack_srca_matmul);
    tmp.set_loop_op0(unpack_srcb_matmul);
    tmp.program(instrn_buffer);
}
```

## Tilize Operations (`llk_unpack_tilize.h`)

### Tilize Algorithm

Tilize operations convert row-major data to tiled format during unpack:

```cpp
template <
    bool neginf_srcA = false,
    std::uint32_t reload_srcB = false,
    bool zero_srcA = false,
    bool zero_srcA_reduce = false
>
inline void llk_unpack_tilizeA_B_mop_config(
    const bool narrow_tile = false,
    const std::uint32_t num_faces = 4
) {
    // Tilize mode configuration
    const uint32_t outerloop = narrow_tile ? 1 : num_faces;
    constexpr uint32_t innerloop = 1;

    // Special handling for narrow tiles
    constexpr uint unpack_srcA_tilize = TT_OP_UNPACR(
        SrcA, 0b1, 0, 0, 0, 1, 1,
        narrow_tile ? p_unpacr::RAREFYB_DISABLE : p_unpacr::RAREFYB_ENABLE,
        0, 0, 0, 0, 1
    );

    ckernel_template tmp(outerloop, innerloop, unpack_srcA_tilize);

    // Configure source B handling
    if constexpr (reload_srcB) {
        tmp.set_loop_op0(TT_OP_UNPACR(SrcB, 0b1, 0, 0, 0, 1, 1, p_unpacr::RAREFYB_DISABLE, 0, 0, 0, 0, 1));
    }

    tmp.program(instrn_buffer);
}
```

## Broadcast Support

### Broadcast Types and Implementation

#### Row Broadcast (`BroadcastType::ROW`)
```cpp
if constexpr (BType == BroadcastType::ROW) {
    constexpr uint32_t innerloop = 2;
    const uint32_t outerloop = num_faces >> 1;  // Divide by 2 for row pairs

    ckernel_template tmp(outerloop, innerloop, unpack_srca);
    tmp.set_loop_op0(unpack_srcb_zerosrc_set_dvalid);  // Zero + set valid
    tmp.set_loop_op1(unpack_srcb_set_dvalid);          // Set valid only
    tmp.program(instrn_buffer);
}
```

#### Scalar Broadcast (`BroadcastType::SCALAR`)
```cpp
if constexpr (BType == BroadcastType::SCALAR) {
    constexpr uint32_t innerloop = 1;
    const uint32_t outerloop = num_faces;

    ckernel_template tmp(outerloop, innerloop, unpack_srca);
    tmp.set_start_op(srcb_set_z_2);                    // Set srcB ch0_z = 2
    tmp.set_loop_op0(unpack_srcb_zerosrc_set_dvalid);  // Zero source + set valid
    tmp.set_end_op(srcb_clear_z);                      // Clear srcB ch0_z = 0
    tmp.program(instrn_buffer);
}
```

## Common Unpack Utilities (`llk_unpack_common.h`)

### Hardware Configuration

```cpp
template <bool is_fp32_dest_acc_en = false>
inline void _llk_unpack_hw_configure_(
    const std::uint32_t unpack_src_format,
    const std::uint32_t unpack_dst_format = 0,
    const std::uint32_t face_r_dim = FACE_R_DIM
) {
    // Format-specific hardware configuration
    configure_unpack_AB(
        unpack_src_format, unpack_src_format,  // A and B formats
        unpack_dst_format, unpack_dst_format,  // Destination formats
        face_r_dim, face_r_dim                 // Face dimensions
    );

    // FP32 accumulation mode setup
    if constexpr (is_fp32_dest_acc_en) {
        // Enable 32-bit destination accumulation
        math_dest_wait_and_clear<DstSync::SyncFull>();
    }
}
```

### Address Mode Configuration

```cpp
inline void configure_unpack_AB(
    const uint unpA_src_format, const uint unpB_src_format,
    const uint unpA_dst_format, const uint unpB_dst_format,
    const uint face_r_dim, const uint face_c_dim
) {
    // Calculate stride based on data format
    uint unpA_ch1_x_stride = (uint)(unpA_dst_format & 0x3) == (uint)DataFormat::Float32 ? 4
                           : (uint)(unpA_dst_format & 0x3) == (uint)DataFormat::Float16 ? 2 : 1;
    uint unpB_ch1_x_stride = (uint)(unpB_dst_format & 0x3) == (uint)DataFormat::Float32 ? 4
                           : (uint)(unpB_dst_format & 0x3) == (uint)DataFormat::Float16 ? 2 : 1;

    // Configure address generation
    volatile uint* cfg = get_cfg_pointer();
    cfg[UNP_CFG_REG_Offset_address_ADDR32] = (unpA_ch1_x_stride << UNP_CFG_CH1_X_stride_SHAMT) |
                                            (unpB_ch1_x_stride << UNP_CFG_CH1_X_stride_SHAMT);
}
```

## Reduce Operations (`llk_unpack_reduce.h`)

### Reduction-Specific Unpack

```cpp
template <ReduceDim reduce_dim = ReduceDim::REDUCE_ROW, PoolType pool_type = PoolType::SUM>
inline void _llk_unpack_reduce_mop_config_(
    const std::uint32_t num_faces,
    const std::uint32_t unpack_src_format,
    const std::uint32_t unpack_dst_format = 0
) {
    constexpr uint unpack_srca_reduce = TT_OP_UNPACR(SrcA, 0b1, 0, 0, 0, 1, 1, p_unpacr::RAREFYB_DISABLE, 0, 0, 0, 0, 1);

    if constexpr (reduce_dim == ReduceDim::REDUCE_ROW) {
        // Row reduction: process all faces sequentially
        const uint32_t outerloop = num_faces;
        constexpr uint32_t innerloop = 1;
        ckernel_template tmp(outerloop, innerloop, unpack_srca_reduce);
        tmp.program(instrn_buffer);
    } else if constexpr (reduce_dim == ReduceDim::REDUCE_COL) {
        // Column reduction: special access pattern
        constexpr uint32_t outerloop = 1;
        const uint32_t innerloop = num_faces;
        ckernel_template tmp(outerloop, innerloop, unpack_srca_reduce);
        tmp.program(instrn_buffer);
    }
}
```

## Performance Characteristics

### Cycle Costs by Operation Type

| Operation Mode | Cycles per Face | Memory Access | Notes |
|----------------|-----------------|---------------|-------|
| **Standard Unpack** | 1-2 | Sequential | Optimal for single source |
| **Broadcast Unpack** | 2-3 | Replicated | Additional broadcast overhead |
| **Tilize Unpack** | 2-4 | Strided | Format conversion cost |
| **Dual Source** | 2-3 | Coordinated | Synchronization overhead |

### Format-Specific Performance

| Data Format | Unpack Overhead | Bandwidth Efficiency | Notes |
|-------------|-----------------|---------------------|-------|
| **FP32** | Baseline | 100% | Full precision |
| **FP16** | 0.9x | 180% | Reduced memory traffic |
| **BF16** | 0.9x | 180% | Maintained precision range |
| **INT8** | 0.8x | 320% | Quantized efficiency |

## Advanced Features

### Instruction Replay (LLTT Integration)

```cpp
// Efficient instruction sequence reuse
lltt::record(0, 4);  // Record 4 instructions starting at index 0
TTI_UNPACR_NOP(SrcA, p_unpacr_nop::UNP_ZEROSRC);
TTI_UNPACR_NOP(SrcA, p_unpacr_nop::UNP_SET_DVALID);
TTI_UNPACR(SrcB, 0b1, 0, 0, 0, 1, 1, p_unpacr::RAREFYB_DISABLE, 0, 0, 0, 0, 1);
TTI_UNPACR(SrcB, 0b1, 0, 0, 0, 1, 1, p_unpacr::RAREFYB_DISABLE, 0, 0, 0, 0, 1);

// Replay recorded sequences
static constexpr uint unpack_srca_zerosrc_set_dvalid = lltt::replay_insn(0, 2);
static constexpr uint unpack_srcb_unpack_srcb = lltt::replay_insn(2, 2);
```

### Carriage Return Support

```cpp
// Address carriage return for efficient memory access patterns
// CR_INC is specified through address mode or instruction fields
// Limited to 3-4 bit increment values depending on instruction

// Example: ADDRCRXY usage
TT_OP_SETADCZW(p_setadc::UNP_A, 0, 0, 0, cr_increment, 0b0001);  // Set CR increment

// Note: CR bit with addrmod field has hardware bug - use specific instructions instead
```

## Synchronization and Dependencies

### Data Dependency Management

```cpp
// Ensure proper ordering between unpack stages
template <bool mail2math = true, bool mail2pack = true>
inline void _llk_unpack_get_tile_(std::uint32_t tile_index, std::uint32_t* p_tile) {
    constexpr uint32_t wait_sem = (mail2math && mail2pack) ? 2 : 1;

    // Wait for sufficient semaphore count
    while (semaphore_read(semaphore::UNPACK_OPERAND_SYNC) < wait_sem);

    if constexpr (mail2math) {
        *p_tile = mailbox_read(ThreadId::UnpackThreadId);
    }
}
```

### Format Validation

```cpp
// Runtime format compatibility checking
inline bool is_32bit_input(uint32_t src_format, uint32_t dst_format) {
    return ((src_format & 0x1F) == (uint32_t)DataFormat::Float32) ||
           ((dst_format & 0x1F) == (uint32_t)DataFormat::Float32);
}
```

## Error Conditions and Debugging

### Common Issues

#### MOP Pattern Misunderstanding
- **Issue**: Assuming 4 separate SKIP_A instructions in zero mask mode
- **Reality**: Single SKIP_A instruction handles all faces
- **Fix**: Use correct MOP expansion understanding

#### Format Mismatch
- **Symptom**: Data corruption or unexpected precision loss
- **Cause**: Incompatible src/dst format combinations
- **Solution**: Validate format compatibility at configuration time

#### Broadcast Configuration Errors
- **Symptom**: Incorrect data replication patterns
- **Cause**: Wrong broadcast type or address mode setup
- **Solution**: Verify broadcast type matches data access pattern

### Debug Features

```cpp
// Enable instruction tracing
#define ENABLE_LLTT_TRACE 1

// Register state inspection
volatile uint* unpack_cfg = get_cfg_pointer();
uint unpack_status = unpack_cfg[UNPACK_STATUS_OFFSET];

// Tile boundary verification
assert(tile_index < max_tiles_per_cb);
```

## Integration Patterns

### Standard Unpack Workflow
```cpp
// 1. Initialize unpack subsystem
llk_unpack_A_init<BroadcastType::NONE>(input_cb_id);

// 2. Configure hardware
_llk_unpack_hw_configure_<false>(unpack_src_format, unpack_dst_format);

// 3. Configure MOP
_llk_unpack_A_mop_config_<BroadcastType::NONE>(transpose_faces, num_faces, src_format);

// 4. Execute unpack
llk_unpack_A<BroadcastType::NONE>(tile_index, input_cb_id);

// 5. Signal completion
llk_unpack_A_dest_section_done<DstSync::SyncFull>();
```

### Broadcast Unpack Workflow
```cpp
// Column broadcast example
llk_unpack_AB_init<BroadcastType::COL>(cb_a, cb_b);
_llk_unpack_AB_mop_config_<BroadcastType::COL>(num_faces, format_a, format_b);
llk_unpack_AB<BroadcastType::COL>(tile_a, tile_b, cb_a, cb_b);
```

---

**Next**: [Math Operations Detailed Documentation](math_operations_detailed.md)
**Related**: [Pack Operations](pack_operations_detailed.md), [SFPU Operations](sfpu_operations_detailed.md)
**Implementation**: Source files in `tt_llk_wormhole_b0/llk_lib/llk_unpack*.h`
