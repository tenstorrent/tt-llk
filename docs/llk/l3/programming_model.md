# LLK Programming Model - Developer Guide

## Overview

This document provides a comprehensive guide to programming with Low-Level Kernels (LLKs) on Tenstorrent hardware. LLKs provide direct access to the Tensix engine's capabilities while abstracting hardware complexity through a well-designed API layer.

## Programming Model Fundamentals

### Thread-Based Architecture

The Tensix engine operates with a 3-thread model that maps directly to the hardware pipeline:

```cpp
#ifdef LLK_TRISC_UNPACK
// Thread 0: Data loading from L1 to register files
#include "llk_unpack_AB_matmul_api.h"
void unpack_main() {
    // Unpack operations
}
#endif

#ifdef LLK_TRISC_MATH  
// Thread 1: Mathematical operations on register data
#include "llk_math_matmul_api.h"
void math_main() {
    // Math operations
}
#endif

#ifdef LLK_TRISC_PACK
// Thread 2: Data output from register files to L1
#include "llk_pack_api.h" 
void pack_main() {
    // Pack operations
}
#endif
```

### Core Programming Pattern

Every LLK operation follows this fundamental pattern:

```cpp
// 1. Initialize operation
operation_init(input_cb, output_cb, params);

// 2. Execute operation on tiles
for (int i = 0; i < num_tiles; i++) {
    operation_tiles(cb_id, tile_index);
}
```

## API Design Principles

### Configuration and Execution Separation

LLK APIs separate configuration from execution for optimal performance:

```cpp
// Configuration (done once)
_llk_math_matmul_init_(transpose, ct_dim, rt_dim, kt_dim);

// Execution (done per tile)
_llk_math_matmul_(dst_index, /*...*/);
```

### Template-Based Optimization

Most LLK functions use templates for compile-time optimization:

```cpp
template <bool is_fp32_dest_acc_en, StochRndType stoch_rnd_mode>
inline void _llk_math_matmul_init_(/*...*/);
```

**Key Template Parameters:**
- `is_fp32_dest_acc_en`: Enables FP32 accumulation mode
- `stoch_rnd_mode`: Controls stochastic rounding behavior
- `DstTileFaceLayout`: Specifies memory layout (RowMajor/ColMajor)

## Resource Management

### Circular Buffer Coordination

LLK operations work with circular buffers for data flow:

```cpp
// Wait for input data availability
cb_wait_front(input_cb, num_tiles);

// Reserve output space
cb_reserve_back(output_cb, num_tiles);

// Perform operation
llk_operation(/*...*/);

// Release resources
cb_push_back(output_cb, num_tiles);
cb_pop_front(input_cb, num_tiles);
```

### Register File Management

Access to shared register files requires explicit resource management:

```cpp
// Acquire destination register access
acquire_dst(dst_tiles);

// Perform computation
llk_math_operation(/*...*/);

// Release destination registers
release_dst(dst_tiles);
```

## Common Operations Guide

### Matrix Multiplication

```cpp
// Initialization (once per kernel)
_llk_unpack_AB_matmul_init_(ct_dim, rt_dim, kt_dim);
_llk_math_matmul_init_(ct_dim, rt_dim, kt_dim);
_llk_pack_init_(pack_dst_format);

// Per-tile execution
for (int kt = 0; kt < kt_dim; kt++) {
    // Unpack input matrices
    _llk_unpack_AB_matmul_(
        src_a_addr, src_b_addr, 
        kt, kt * ct_dim);
    
    // Perform matrix multiplication
    _llk_math_matmul_(dst_index);
    
    // Pack result
    _llk_pack_(dst_addr, dst_index);
}
```

### Element-wise Operations

```cpp
// SFPU-based element-wise operations
_llk_math_eltwise_unary_sfpu_init_<SfpuType::relu>();

for (int i = 0; i < num_tiles; i++) {
    _llk_math_eltwise_unary_sfpu_<SfpuType::relu>(dst_index);
}
```

### Data Movement

```cpp
// Unpacking from L1 to registers
_llk_unpack_A_init_(BroadcastType::NONE, /*...*/);
_llk_unpack_A_(l1_addr, tile_index, src_format, dst_format);

// Packing from registers to L1  
_llk_pack_init_(dst_format, /*...*/);
_llk_pack_(l1_addr, tile_index);
```

## Data Format Handling

### Supported Formats

LLKs support comprehensive data format conversion:

```cpp
// Floating-point formats
DataFormat::Float32     // 32-bit IEEE 754
DataFormat::Float16     // 16-bit IEEE 754  
DataFormat::Float16_b   // 16-bit Brain Float
DataFormat::Tf32        // TensorFlow 32-bit

// Block floating-point formats
DataFormat::Bfp8        // 8-bit block floating-point
DataFormat::Bfp4        // 4-bit block floating-point
DataFormat::Bfp2        // 2-bit block floating-point

// Integer formats
DataFormat::Int32       // 32-bit signed integer
DataFormat::Int16       // 16-bit signed integer
DataFormat::Int8        // 8-bit signed integer
```

### Format Conversion

```cpp
// Automatic format conversion during unpack/pack
_llk_unpack_A_hw_configure_(
    src_format,    // Input format in L1
    dst_format,    // Format in register files  
    /*...*/);

_llk_pack_hw_configure_(
    src_format,    // Format in register files
    dst_format,    // Output format in L1
    /*...*/);
```

## Performance Optimization

### Fidelity Control

Control precision vs. performance trade-offs:

```cpp
// High performance, lower precision
constexpr int MATH_FIDELITY_DESC = 0x0; // 1 fidelity phase

// Balanced performance and precision  
constexpr int MATH_FIDELITY_DESC = 0x1; // 2 fidelity phases

// High precision, lower performance
constexpr int MATH_FIDELITY_DESC = 0x3; // 4 fidelity phases
```

### Memory Layout Optimization

```cpp
// Row-major layout (default, cache-friendly)
template <DstTileFaceLayout::RowMajor>
void operation(/*...*/);

// Column-major layout (for specific algorithms)
template <DstTileFaceLayout::ColMajor>  
void operation(/*...*/);
```

### Broadcast Optimization

```cpp
// Optimize for broadcast patterns
_llk_unpack_AB_init_<
    BroadcastType::COL,    // Column broadcast for operand B
    /*...*/>();
```

## Error Handling and Debugging

### Compile-Time Validation

```cpp
// LLKs use extensive compile-time validation
template <bool is_fp32_dest_acc_en>
inline void operation() {
    static_assert(is_fp32_dest_acc_en || condition, 
                  "FP32 mode required for this operation");
}
```

### Debug Features

```cpp
// Debug builds provide additional validation
#ifdef LLK_DEBUG
    // Additional parameter checking
    // Performance counters
    // Register state dumps
#endif
```

### Common Pitfalls

1. **Resource Management**: Always pair acquire/release calls
2. **Format Compatibility**: Ensure compatible src/dst formats
3. **Tile Alignment**: Operations require proper tile alignment
4. **Synchronization**: Maintain proper thread synchronization

## Advanced Features

### Macro Operations (MOPs)

For performance-critical loops, use hardware macro operations:

```cpp
// Configure MOP for repeated operations
ckernel::ckernel_template mop(
    outer_loop_count, 
    inner_loop_count,
    core_instruction);
    
mop.set_end_ops(cleanup_instruction);
mop.program(instrn_buffer);
```

### Address Generation

Advanced addressing patterns for complex data layouts:

```cpp
// Configure sophisticated address patterns
addr_mod_t addr_config {
    .srca = {.incr = stride_a, .clr = 0, .cr = 1},
    .srcb = {.incr = stride_b, .clr = 0, .cr = 1},
    .dest = {.incr = stride_d, .clr = 1, .cr = 0},
};
addr_config.set(ADDR_MOD_0);
```

### Custom SFPU Operations

Implement custom special functions:

```cpp
// Custom SFPU sequence
TTI_SFPLOAD(p_sfpu::LREG0, 0, 0);
TTI_SFPMUL(p_sfpu::LREG0, p_sfpu::LREG0, 
           p_sfpu::LCONST_0, p_sfpu::LREG1, 0);
TTI_SFPSTORE(p_sfpu::LREG1, 0, 0);
```

## Best Practices

### Performance

1. **Minimize Configuration Changes**: Batch operations with same config
2. **Use Templates**: Leverage compile-time optimization  
3. **Optimize Memory Access**: Consider L1 bank conflicts
4. **Pipeline Utilization**: Keep all 3 threads busy

### Maintainability

1. **Clear Naming**: Use descriptive variable names
2. **Comment Intent**: Document algorithm-specific choices
3. **Error Checking**: Validate parameters in debug builds
4. **Resource Cleanup**: Always release acquired resources

### Portability

1. **Abstract Hardware**: Use LLK APIs instead of direct instructions
2. **Template Parameters**: Make hardware features configurable
3. **Format Agnostic**: Design for multiple data formats

## Integration with TT-Metal

LLKs integrate seamlessly with the broader TT-Metal ecosystem:

```cpp
// TT-Metal compute kernel using LLK
namespace NAMESPACE {
void MAIN {
    // TT-Metal circular buffer setup
    uint32_t src0_cb_index = 0;
    uint32_t src1_cb_index = 1; 
    uint32_t output_cb_index = 16;
    
    // LLK operation implementation
    llk_operation_init(src0_cb_index, src1_cb_index, output_cb_index);
    
    for (uint32_t block = 0; block < num_blocks; block++) {
        llk_operation_tiles(/*...*/);
    }
}
}
```

## Conclusion

The LLK programming model provides a powerful yet accessible interface to Tenstorrent's specialized AI acceleration hardware. By following the patterns and best practices outlined in this guide, developers can achieve optimal performance while maintaining code clarity and maintainability.

For specific API details, refer to the function-specific documentation in the `llk_lib/` headers. For hardware architecture details, see the L2 documentation.
