# Quasar Architecture Static Context

This document provides architectural context for the Quasar configuration of Tensix NEO, used for LLK code generation.

## Overview

Quasar is Tenstorrent's datacenter-focused configuration of the Tensix NEO architecture. It features:
- 4 compute cores per NEO cluster
- 4MB L1 memory
- Advanced matrix and vector engines
- Support for MXFP4 format with 2x throughput

## Compute Core Architecture

Each compute core contains:
- **Matrix Engine (FPU)**: Executes matrix multiplication, element-wise operations
- **Vector Engine (SFPU)**: 32 SFPU lanes for transcendental/vector operations
- **4 TRISC cores**: RISC-V cores for kernel execution (Threads 0-3)
- **Source Register Files**: SrcA, SrcB (32x32 tiles), SrcS (for SFPU)
- **Destination Register File**: 1024 rows x 16 datums (512 rows for 32-bit data)
- **Unpackers**: Unpack0 (SrcA/Dest), Unpack1 (SrcB/metadata), Unpack2 (SrcS)
- **Packers**: Pack0 (Dest), Pack1 (SrcS)

## Matrix Engine (Quasar Configuration)

### Throughput
| Format | Throughput (per core) |
|--------|----------------------|
| MXFP8/MXFP6/MXINT8 | 2048 MAC ops/clk |
| MXFP4 | 4096 MAC ops/clk (2x mode) |
| INT8 | 2048 MAC ops/clk |
| BF16 | 1024-2048 MAC ops/clk |
| FP16 | 512 MAC ops/clk |
| TF32 | 512 MAC ops/clk |

### Operation
- Reads 8 rows from SrcB register file per instruction
- Each row has 16 datums (19-bit)
- SrcB rows broadcast horizontally across dot product engines
- SrcA provides 16 datums broadcast vertically
- Each MVMUL produces 8x16 output datums
- Results accumulated in destination register file

### Supported Instructions
- `MVMUL` / `MVMULDI`: Matrix multiplication with/without immediate
- `ELWMUL`: Element-wise multiply
- `ELWADD`: Element-wise add
- `ELWSUB`: Element-wise subtract
- `GMPOOL`: Global max pooling
- `GAPOOL`: Global average pooling

### High-Precision Throttling
Quasar implements automatic throttling for high-precision formats (BF16+):
- Limits MVMUL throughput to 50% for formats with mantissa >= 7 bits
- Implemented via data-valid phase stalling
- MX formats are NOT throttled

## Vector Engine (SFPU)

### Architecture
- 32 SFPU lanes organized in 4x8 grid
- Operates on FP32 precision internally
- Uses Local Registers (LREGs) for computation
- Supports conditional execution per lane

### Key Capabilities
- Single-cycle reciprocal and exponential
- Multiply-add operations
- Transcendental functions via series expansion
- Load/store from Dest or SrcS register files

### Data Movement
- Loads from Dest/SrcS fetch 16 datums (odd or even columns based on address)
- Stores update 16 datums at a time

## Register Files

### Source Registers (SrcA, SrcB)
- Size: 32x32 datum tiles (2 tiles per register file)
- Datum width: 19 bits
- Double-buffered for overlap of unpack and compute
- Supports implied formats (auto-track last written format)
- Strided write modes for 64B L1 access optimization

### SrcS Register File
- Size: 2x8x16 datums (32 bits each)
- Used for SFPU operations
- Double-buffered
- Read by Pack1, written by Unpack2

### Destination Register File
- Size: 1024 rows x 16 datums (16-bit mode)
- Size: 512 rows x 16 datums (32-bit mode)
- Accumulation formats: FP32, BF16, FP16, INT32
- Double-buffered for packer overlap
- Supports implied formats

## Data Formats

### Input Formats (SrcA/SrcB)
- BF16 (bfloat16)
- FP16 (IEEE float16)
- TF32 (TensorFloat-32)
- INT8
- MXFP4, MXFP6, MXFP8 (Microscaling formats)
- MXINT2, MXINT4, MXINT8
- FP8 (E4M3, E5M2 variants)

### Accumulation Formats (Dest)
- FP32
- BF16
- FP16
- INT32

### Rounding Modes
- Round-to-Nearest with ties-to-even
- Stochastic rounding

## Unpacker Instructions

| Instruction | Granularity |
|------------|-------------|
| UnpackRow | 16x1 datums |
| UnpackFace | 16x16x1, 16x8x1, 16x4x1, 16x2x1 |
| UnpackTile | 16x16x4, 16x16x1, 16x8x1, 16x4x1, 16x2x1, 16x1x1 |
| UnpackTilize | Flat data to tiled (1024 datums) |
| UnpackStrided | 16x8 with stride patterns |
| RVUnpack | Various sizes, no config needed |

- `*INC` variants use counters for auto-increment
- Inline transpose supported for SrcA/SrcB
- Format conversion on-the-fly

## Packer Instructions

| Instruction | Granularity |
|------------|-------------|
| PackRow | 16x1 datums |
| PackFace | 16x16x1, 16x8x1, 16x4x1, 16x2x1 |
| PackTile | 16x16x4, 16x16x1, 16x8x1, 16x4x1, 16x2x1, 16x1x1 |
| PackUntilize | Tiled to flat (1024 datums) |
| PackStrided | 16x8 with stride patterns |
| RVPack | Various sizes, no config needed |

- Format conversion and rounding
- ReLU variants on-the-fly
- Edge masking support
- L1 accumulation via floating-point atomics

## Synchronization

### Semaphores
- 16 local semaphores per compute core
- 64 cluster-level semaphores (8-bit)
- 8 mutex registers
- TTI instructions: `SEM_INIT`, `SEM_POST`, `SEM_GET`, `STALLWAIT`

### Data Valid / Write Ready
- Hardware sync between unpackers and compute
- `STALLWAIT` on dest read for explicit control
- Software can disable implicit double-buffering

### Tile Counters
- 32 tile counters per compute core
- Used for NoC/compute synchronization
- Dual copies for low-latency access

## L1 Memory

### Configuration
- 4MB total (Quasar)
- 64 logical banks, 64B wide each
- Physically: 4x 16B sub-banks per logical bank

### Access Ports per Compute Core
- TRISC0-3: 1 RW port (shared via L0)
- Unpack0,1,2: 5 Read ports (2+2+1)
- Pack0,1: 3 Write ports (2+1)

### Features
- Partitioned crossbar with address hashing
- Out-of-order request scheduling (configurable)
- Floating-point atomics (FP32, FP16, BF16, INT32)
- ECC at 16B granularity

## TRISC Programming Model

### Thread Assignment (typical)
- Thread 0: Unpacker operations
- Thread 1-2: Math/FPU operations
- Thread 3: SFPU operations (independent from FPU)

### Key Instructions
- TTI (Tensix-TRISC Interface) instructions for hardware control
- MOP (Macro-Operation) for instruction batching
- Replay buffers for loop execution
- WRCFG/RDCFG for configuration register access

### Hardware Sync (TTSync)
- Automatic ordering between TTI and MMIO
- Prevents RAW hazards with config registers
- Can be disabled for explicit software control

## Special Number Handling

### Infinity/NaN Propagation
- All operands checked at FPU input
- `Inf * 0 = NaN`
- `+Inf + (-Inf) = NaN`
- `NaN` propagates through all operations
- Applied to: MVMUL, ELWMUL, ELWADD, ELWSUB, GAPOOL

## Key Differences from Blackhole/Wormhole

1. **4 TRISC threads** (vs 3 in BH/WH)
2. **Unpack2 + Pack1** for independent SFPU data path
3. **SrcS register file** for SFPU-specific operations
4. **Buffer Descriptors** replace mega-config approach
5. **Per-execution-unit config state ID**
6. **8x8 multipliers** in FP lanes
7. **MXFP4 2x throughput** mode
8. **Structured sparsity** support
9. **Cluster-level semaphores** for multi-core sync
10. **High-precision throttling** (automatic)

## LLK Development Guidelines

### Data Flow Patterns
1. **Matrix Path**: L1 → Unpack0/1 → SrcA/SrcB → FPU → Dest → Pack0 → L1
2. **Vector Path**: L1 → Unpack2 → SrcS → SFPU → SrcS/Dest → Pack1/Pack0 → L1
3. **Combined**: Unpack → SrcA/B → FPU → Dest → SFPU → Dest → Pack

### Configuration Setup
- Use Buffer Descriptor Table (BDT) for buffer management
- Set formats via WRCFG instructions
- Configure tile sizes and strides in config registers
- Use implied formats for automatic format tracking

### Synchronization Best Practices
- Use semaphores for producer-consumer patterns
- Use `STALLWAIT` for explicit synchronization
- Leverage tile counters for NoC/compute coordination
- Consider cluster semaphores for multi-core operations
