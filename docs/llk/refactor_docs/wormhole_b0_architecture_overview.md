# TT-LLK Wormhole B0 Architecture Documentation

**Architecture**: Wormhole B0 Tensix
**Version**: TT-LLK Main Branch
**Last Updated**: January 2025

## Overview

The TT-LLK (Tenstorrent Low-Level Kernel) library for Wormhole B0 provides high-performance compute kernels for neural network operations on Tenstorrent's Tensix cores. This documentation covers the complete Wormhole B0 implementation including pack/unpack operations, mathematical computations, and SFPU (Special Function Processing Unit) operations.

## Architecture Components

### 1. **LLK Library** (`llk_lib/`)
High-level kernel operations providing the main API for neural network computations:

- **Pack Operations** (`llk_pack*.h`) - Data output and formatting
- **Unpack Operations** (`llk_unpack*.h`) - Data input and preprocessing
- **Math Operations** (`llk_math*.h`) - Core mathematical computations
- **Common Utilities** (`llk_*_common.h`) - Shared functionality

### 2. **Core Kernel Infrastructure** (`common/inc/`)
Low-level kernel support providing hardware abstraction:

- **Main Kernel** (`ckernel.h`) - Core infrastructure and sync mechanisms
- **Operations** (`ckernel_ops.h`) - Hardware instruction definitions
- **SFPI Interface** (`ckernel_sfpi.h`) - Special Function Processing Interface
- **Templates** (`ckernel_template.h`) - Instruction sequence generation

### 3. **SFPU Operations** (`common/inc/sfpu/`)
Specialized neural network function implementations:

- **Activation Functions** - GELU, ReLU, Sigmoid, Tanh, etc.
- **Mathematical Functions** - Exp, Log, Sqrt, Trigonometry
- **Utility Operations** - Type conversion, comparison, rounding

## Key Features

### Hardware-Optimized Design
- **Tensix-Specific**: Tailored for Wormhole B0 Tensix architecture
- **Pipeline Efficient**: Optimized instruction scheduling and data flow
- **Memory Aware**: Efficient L1 cache and register usage patterns

### Flexible Configuration
- **Template-Based**: Compile-time optimization through templates
- **Broadcast Support**: Row, column, and scalar broadcasting modes
- **Data Format Agnostic**: Support for FP32, FP16, BF16, INT8, etc.

### Performance Features
- **Dual-Mode Operations**: Speed vs. accuracy tradeoffs (e.g., GELU approximation)
- **Vectorized Processing**: SIMD operations across tile faces
- **Instruction Pipelining**: Overlapped execution stages

## Documentation Structure

This documentation is organized into the following sections:

1. **[LLK Library Operations](llk_library_operations.md)** - High-level API reference
2. **[Pack Operations](pack_operations_detailed.md)** - Data output and serialization
3. **[Unpack Operations](unpack_operations_detailed.md)** - Data input and preprocessing
4. **[Math Operations](math_operations_detailed.md)** - Core computational kernels
5. **[SFPU Operations](sfpu_operations_detailed.md)** - Special function implementations
6. **[Core Infrastructure](core_infrastructure_detailed.md)** - Low-level kernel support
7. **[Performance Guide](performance_optimization_guide.md)** - Optimization strategies

## Quick Reference

### Common Operation Patterns
```cpp
// Basic Pack Operation
llk_pack_init<DataFormat::Float16>(out_cb);
llk_pack<DstTileFaceLayout::RowMajor>(tile_index, out_cb);
llk_pack_dest_section_done<DstSync::SyncFull>();

// Basic Unpack Operation
llk_unpack_A_init<BroadcastType::NONE>(in_cb);
llk_unpack_A<BroadcastType::NONE>(tile_index, in_cb);

// SFPU Operation
llk_math_eltwise_unary_sfpu_init<SfpuType::gelu_derivative>();
llk_math_eltwise_unary_sfpu<SfpuType::gelu_derivative, 8>();
```

### Data Flow Pipeline
```
L1 Memory → Unpack → Math/SFPU → Pack → L1 Memory
    ↓         ↓         ↓           ↓        ↓
  Input    Preproc   Compute    Format   Output
  Tiles     Data    Results     Data     Tiles
```

## Performance Characteristics

### Throughput Targets
- **Math Operations**: 1-2 cycles per operation per tile face
- **SFPU Operations**: 2-12 cycles depending on complexity mode
- **Pack/Unpack**: 1-4 cycles per tile face depending on format

### Memory Bandwidth
- **L1 Access**: 8-cycle minimum latency for cache misses
- **Register File**: 1-cycle access for optimal scheduling
- **DEST Buffer**: Efficient double-buffering support

## Integration Guidelines

### Kernel Development
1. **Choose Operations**: Select appropriate LLK functions for your use case
2. **Configure Templates**: Set compile-time parameters for optimization
3. **Schedule Instructions**: Arrange operations for pipeline efficiency
4. **Handle Synchronization**: Manage data dependencies and timing

### Debugging Support
- **Instruction Tracing**: Built-in logging and replay capabilities
- **Register Inspection**: Debug access to internal state
- **Performance Counters**: Cycle counting and bottleneck analysis

---

**Related Documentation**: [Pack/Unpack Implementation Details](discussions%20on%20Pack%20and%20Unpack%20operation%20details%20vs.%20actual%20implementation.md)
**Architecture Reference**: Wormhole B0 Tensix Programming Guide
**API Reference**: Individual operation documentation files
