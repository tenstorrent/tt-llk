// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cstdint>

#include "ckernel_ops.h"
#include "llk_defs.h"
#include "tensix.h"
#include "tensix_types.h"

/**
 * @file ckernel_defs.h
 * @brief Core definitions, enums, and constants for the Tensix compute kernel framework
 *
 * @details This file contains fundamental type definitions, enumeration constants, and utility
 * functions that form the foundation of the compute kernel framework. These definitions are
 * used throughout the kernel system for data types, threading models, memory layouts, and
 * mathematical operations.
 *
 * **Key Categories:**
 * - **Data Sources**: Input operand identification and routing
 * - **Threading Model**: Thread identification and coordination  
 * - **Memory Layout**: Tile organization and destination management
 * - **Data Formats**: Size calculations and format detection utilities
 * - **Mathematical Operations**: Activation functions and binary operations
 * - **Hardware Configuration**: Register spaces and packing modes
 *
 * **Architecture Integration:**
 * All definitions are optimized for the Blackhole Tensix architecture, providing
 * efficient mappings to hardware resources and optimal performance characteristics.
 */

namespace ckernel
{

/**
 * @brief Source operand identifiers for kernel operations
 *
 * @details Defines the standard source operand labels used throughout the kernel
 * framework. These identifiers map to specific data input streams and are used
 * for operand routing, unpacker configuration, and mathematical operations.
 *
 * **Operand Usage:**
 * - **SrcA**: Primary operand for mathematical operations (left operand)
 * - **SrcB**: Secondary operand for binary operations (right operand) 
 * - **SrcC**: Tertiary operand for ternary operations (bias, accumulator)
 *
 * **Integration with Hardware:**
 * - Maps directly to unpacker unit source selection
 * - Used in instruction encoding for operand specification
 * - Critical for data flow routing in compute pipeline
 * - Determines memory access patterns and timing
 */
enum Srcs
{
    SrcA = 0,    ///< Primary source operand (first input)
    SrcB = 1,    ///< Secondary source operand (second input)
    SrcC = 2     ///< Tertiary source operand (bias/accumulator)
};

/**
 * @brief Unpacker unit identifiers for data input processing
 *
 * @details Identifies specific unpacker units responsible for data input processing
 * and format conversion. The Tensix architecture provides multiple unpacker units
 * for parallel data processing and format-specific optimizations.
 *
 * **Unpacker Roles:**
 * - **Unp0**: Primary unpacker for SrcA data formatting
 * - **Unp1**: Secondary unpacker for SrcB data formatting
 * - **UnpAll**: Broadcast operation affecting all unpacker units
 *
 * **Data Processing:**
 * - Format conversion from memory representation to compute format
 * - Data layout transformation for optimal compute access
 * - Parallel processing for improved throughput
 * - Specialized handling for different data types
 */
enum Unpackers
{
    Unp0   = 0,  ///< Primary unpacker unit (typically for SrcA)
    Unp1   = 1,  ///< Secondary unpacker unit (typically for SrcB)
    UnpAll = 2   ///< All unpacker units (broadcast operations)
};

/**
 * @brief Destination register starting positions for tiled operations
 *
 * @details Specifies where tile operations should begin writing in the destination
 * register file. This enables efficient memory usage and supports various tiling
 * strategies for optimal performance.
 *
 * **Starting Positions:**
 * - **StartZero**: Begin at the start of destination register space
 * - **StartHalf**: Begin at the midpoint of destination register space
 *
 * **Use Cases:**
 * - Double-buffering for continuous operation
 * - Memory bank optimization
 * - Pipeline stage coordination
 * - Intermediate result storage
 */
enum DstStart
{
    StartZero = 0,  ///< Start writing at beginning of destination registers
    StartHalf = 1   ///< Start writing at halfway point of destination registers
};

/**
 * @brief Destination register clearing modes for memory management
 *
 * @details Defines different strategies for clearing destination register contents.
 * Clearing is essential for accurate accumulation, preventing data corruption, and
 * ensuring deterministic results across kernel executions.
 *
 * **Clearing Strategies:**
 * - **ClearRow**: Clear single row (16 datums)
 * - **Clear16Rows**: Clear 16 rows (one face worth of data)
 * - **ClearHalf**: Clear half of destination register space
 * - **ClearFull**: Clear entire destination register space
 *
 * **Performance Considerations:**
 * - Smaller clear operations reduce overhead
 * - Full clears ensure complete data consistency
 * - Strategic clearing optimizes memory bandwidth
 * - Clear timing affects pipeline efficiency
 */
enum DstClear
{
    ClearRow    = 0,  ///< Clear single row of destination data
    Clear16Rows = 1,  ///< Clear 16 rows (one face) of destination data
    ClearHalf   = 2,  ///< Clear half of destination register space
    ClearFull   = 3   ///< Clear entire destination register space
};

/**
 * @brief Thread identifiers for kernel execution pipeline
 *
 * @details Identifies the three main execution threads in the kernel pipeline.
 * Each thread has specialized responsibilities and operates in coordination
 * with the others to achieve high-throughput data processing.
 *
 * **Thread Responsibilities:**
 * - **UnpackThreadId**: Data input, format conversion, memory staging
 * - **MathThreadId**: Mathematical computations, SFPU operations
 * - **PackThreadId**: Data output, format conversion, memory writeback
 *
 * **Pipeline Coordination:**
 * - Threads operate in parallel for maximum throughput
 * - Synchronization via semaphores and hardware signals
 * - Data flows sequentially: Unpack → Math → Pack
 * - Each thread optimized for specific hardware units
 */
enum ThreadId
{
    UnpackThreadId = 0,  ///< Unpack thread (data input processing)
    MathThreadId   = 1,  ///< Math thread (computational operations)
    PackThreadId   = 2   ///< Pack thread (data output processing)
};

/**
 * @brief Destination tile layout strategies for memory organization
 *
 * @details Defines different approaches to organizing tiles in destination memory.
 * Layout choice affects memory access patterns, cache efficiency, and overall
 * system performance depending on the specific computation being performed.
 *
 * **Layout Types:**
 * - **Default**: Standard tile organization for general operations
 * - **Interleaved**: Alternating tile arrangement for specific access patterns
 *
 * **Performance Impact:**
 * - Layout affects memory bandwidth utilization
 * - Optimal layout depends on subsequent operations
 * - Some operations benefit from specific arrangements
 * - Layout choice impacts cache locality
 */
enum DstTileLayout
{
    Default,      ///< Standard tile layout for general operations
    Interleaved,  ///< Interleaved tile layout for optimized access patterns
    // TightDest,
    // Conv3x3,
    // Conv1x1,
    // L1ReadSource,
    // NLLLoss,
    // IndexAccumulate, //Add polling before packing to L1
};

/**
 * @brief Face layout organization within tiles
 *
 * @details Specifies how data within individual tile faces should be organized.
 * Face layout affects how data is accessed within tiles and can be optimized
 * for different mathematical operations and memory access patterns.
 *
 * **Layout Options:**
 * - **RowMajor**: Data organized row-by-row (default, cache-friendly)
 * - **ColMajor**: Data organized column-by-column (matrix transpose friendly)
 *
 * **Operation Optimization:**
 * - RowMajor optimal for most mathematical operations
 * - ColMajor beneficial for transpose and certain matrix operations
 * - Layout choice affects memory access efficiency
 * - Some algorithms require specific face layouts
 */
enum DstTileFaceLayout
{
    RowMajor, ///< Row-major face layout (default, cache-friendly)
    ColMajor, ///< Column-major face layout (transpose-friendly)
};

/**
 * @brief Supported tile shapes for different computational patterns
 *
 * @details Defines the standard tile dimensions supported by the hardware.
 * Different tile shapes are optimized for different types of operations,
 * memory usage patterns, and performance characteristics.
 *
 * **Tile Dimensions:**
 * - **Tile32x32**: Standard full tile (1024 elements)
 * - **Tile32x16**: Half-width tile (512 elements)
 * - **Tile16x16**: Quarter tile (256 elements)
 *
 * **Usage Considerations:**
 * - Larger tiles provide better compute efficiency
 * - Smaller tiles enable more flexible memory usage
 * - Tile shape affects register file utilization
 * - Some operations require specific tile dimensions
 */
enum DstTileShape
{
    Tile32x32 = 0,  ///< Standard 32x32 tile shape (1024 elements)
    Tile32x16 = 1,  ///< Half-width 32x16 tile shape (512 elements)
    Tile16x16 = 2   ///< Quarter 16x16 tile shape (256 elements)
};

/**
 * @brief Parallel packer operation modes for output optimization
 *
 * @details Configures how the pack units operate in parallel to optimize data
 * output throughput. Different modes provide trade-offs between throughput,
 * latency, and resource utilization.
 *
 * **Parallel Modes:**
 * - **Disabled**: Single-threaded packing (simple, lower throughput)
 * - **SingleFTEntry**: Single FIFO table entry parallelism
 * - **MultiFTEntry**: Multiple FIFO table entry parallelism
 * - **TileParallel**: Tile-level parallel processing (highest throughput)
 *
 * **Performance Trade-offs:**
 * - Higher parallelism increases throughput but consumes more resources
 * - Tile-parallel mode optimal for large-scale operations
 * - Simple modes better for small or irregular operations
 * - Mode selection affects memory bandwidth requirements
 */
enum class ParallelPackerMode
{
    Disabled,      ///< No parallel packing (sequential operation)
    SingleFTEntry, ///< Single FIFO table entry parallel mode
    MultiFTEntry,  ///< Multiple FIFO table entry parallel mode
    TileParallel   ///< Tile-level parallel packing (maximum throughput)
};

/**
 * @brief Hardware register space identifiers for memory-mapped operations
 *
 * @details Identifies different hardware register spaces for memory-mapped
 * operations. Each space has specific purposes and access characteristics
 * for optimal hardware interaction.
 *
 * **Register Spaces:**
 * - **TDMA_REGS**: Tensix DMA registers for data movement control
 * - **LOCAL_REGS**: Local processor registers for immediate operations
 * - **ADDR_COUNTERS**: Address counter registers for memory management
 */
enum register_space_e
{
    TDMA_REGS     = 0x0,  ///< Tensix DMA register space
    LOCAL_REGS    = 0x1,  ///< Local processor register space
    ADDR_COUNTERS = 0x2   ///< Address counter register space
};

/**
 * @brief Pack unit selection masks for output routing
 *
 * @details Bitmask values for selecting which pack units should participate
 * in output operations. Enables fine-grained control over data output
 * routing and parallel processing optimization.
 *
 * **Individual Pack Units:**
 * - **PACK_0/1/2/3**: Individual pack unit selection
 * - **PACK_01/23**: Paired pack unit selection
 * - **PACK_ALL**: All pack units (default, maximum throughput)
 *
 * **Usage Patterns:**
 * - Fine-grained output control for specific operations
 * - Load balancing across pack units
 * - Partial output for debugging and testing
 * - Resource allocation optimization
 */
enum PackSelMask
{
    PACK_ALL = 0xF, ///< All pack units (default configuration)
    PACK_0   = 0x1, ///< Pack unit 0 only
    PACK_1   = 0x2, ///< Pack unit 1 only
    PACK_2   = 0x4, ///< Pack unit 2 only
    PACK_3   = 0x8, ///< Pack unit 3 only
    PACK_01  = 0x3, ///< Pack units 0 and 1
    PACK_23  = 0xC  ///< Pack units 2 and 3
};

/**
 * @brief Sorting direction for argmax/argmin operations
 *
 * @details Specifies the direction for sorting operations in argmax and argmin
 * computations. Used by specialized mathematical operations that require
 * finding extreme values and their indices.
 *
 * **Direction Types:**
 * - **ArgMax**: Find maximum values and indices (false = ascending search)
 * - **ArgMin**: Find minimum values and indices (true = descending search)
 */
enum SortDir : bool
{
    ArgMax = false,  ///< Find maximum values (ascending sort order)
    ArgMin = true,   ///< Find minimum values (descending sort order)
};

//
// **Tile and Face Dimension Constants**
//

/**
 * @brief Standard face dimensions and tile organization constants
 * @details Fundamental constants defining the basic geometric organization of data
 */
constexpr std::uint32_t FACE_HEIGHT      = 16;  ///< Height of a single face in elements
constexpr std::uint32_t FACE_WIDTH       = 16;  ///< Width of a single face in elements
constexpr std::uint32_t TILE_HEIGHT      = 32;  ///< Height of a standard tile in elements
constexpr std::uint32_t TILE_WIDTH       = 32;  ///< Width of a standard tile in elements
constexpr std::uint32_t DATUMS_PER_ROW   = 16;  ///< Number of data elements per row
constexpr std::uint32_t TILE_HEADER_SIZE = 1;   ///< Size of tile header in memory units

/**
 * @brief Alias constants for dimensional consistency
 * @details Alternative names for face and tile dimensions to support different coding styles
 */
constexpr std::uint32_t FACE_R_DIM = FACE_HEIGHT;  ///< Face row dimension (height)
constexpr std::uint32_t FACE_C_DIM = FACE_WIDTH;   ///< Face column dimension (width)
constexpr std::uint32_t TILE_R_DIM = TILE_HEIGHT;  ///< Tile row dimension (height)
constexpr std::uint32_t TILE_C_DIM = TILE_WIDTH;   ///< Tile column dimension (width)

/**
 * @brief Calculated tile organization constants
 * @details Derived constants for tile face organization and memory calculations
 */
constexpr std::uint32_t TILE_NUM_FACES = ((TILE_R_DIM * TILE_C_DIM) / (FACE_R_DIM * FACE_C_DIM));  ///< Number of faces per tile (4)

/**
 * @brief Destination register capacity constants for FP16 operations
 * @details Hardware-specific constants for destination register organization
 */
constexpr uint32_t DEST_NUM_TILES_FP16      = (DEST_REGISTER_FULL_SIZE * DEST_FACE_WIDTH) / (TILE_HEIGHT * TILE_HEIGHT);  ///< Full destination capacity in FP16 tiles
constexpr uint32_t DEST_NUM_TILES_FP16_HALF = DEST_NUM_TILES_FP16 / 2;  ///< Half destination capacity for double-buffering
static_assert((DEST_NUM_TILES_FP16 & (DEST_NUM_TILES_FP16 - 1)) == 0);  ///< Ensure power-of-2 for efficient addressing

//
// **Register Address Macros**
//

/**
 * @brief Macros for accessing lower and upper 16-bit halves of registers
 * @details Utility macros for operations that need to address register halves separately
 */
#define LO_16(REG) (2 * (REG))      ///< Calculate address for lower 16 bits of register
#define HI_16(REG) (2 * (REG) + 1)  ///< Calculate address for upper 16 bits of register

//
// **Data Format Utility Functions**
//

/**
 * @brief Calculate L1 memory tile size for different data formats
 *
 * @details Computes the size of a tile in L1 memory (excluding header) based on the
 * data format. Different formats have different storage requirements, affecting
 * memory allocation and transfer sizing.
 *
 * **Format-Specific Sizes:**
 * - **32-bit formats** (Int32, Float32): 4096 bytes (4 bytes × 1024 elements)
 * - **16-bit formats** (Float16, Float16_b): 2048 bytes (2 bytes × 1024 elements)
 * - **BFP formats** (Bfp8, Bfp4, Bfp2): Variable size with exponent overhead
 * - **8-bit formats** (Int8, Lf8, Fp8_e4m3): 1024 bytes (1 byte × 1024 elements)
 *
 * **Size Calculation:**
 * Sizes are returned in 16-byte units for hardware alignment requirements.
 * BFP formats include additional storage for shared exponents.
 *
 * @param format Data format identifier from DataFormat enum
 * @return Tile size in 16-byte units (headerless)
 *
 * @note Sizes exclude tile headers which are handled separately
 * @note BFP formats include exponent storage overhead (64 bytes)
 * @note All sizes aligned to 16-byte boundaries for hardware efficiency
 */
constexpr static std::uint32_t GET_L1_HEADERLESS_TILE_SIZE(uint format)
{
    switch (format & 0xF)
    {
        case ((uint8_t)DataFormat::Int32):
        case ((uint8_t)DataFormat::Float32):
            return (4096 >> 4);  // 256 units of 16 bytes
        case ((uint8_t)DataFormat::Float16):
        case ((uint8_t)DataFormat::Float16_b):
            return (2048 >> 4);  // 128 units of 16 bytes
        case ((uint8_t)DataFormat::Bfp8):
        case ((uint8_t)DataFormat::Bfp8_b):
            return ((1024 >> 4) + (64 >> 4));  // 64 + 4 units (data + exponents)
        case ((uint8_t)DataFormat::Bfp4):
        case ((uint8_t)DataFormat::Bfp4_b):
            return ((512 >> 4) + (64 >> 4));   // 32 + 4 units (data + exponents)
        case ((uint8_t)DataFormat::Bfp2):
        case ((uint8_t)DataFormat::Bfp2_b):
            return ((256 >> 4) + (64 >> 4));   // 16 + 4 units (data + exponents)
        case ((uint8_t)DataFormat::Int8):
        case ((uint8_t)DataFormat::Lf8):
        case ((uint8_t)DataFormat::Fp8_e4m3):
            return (1024 >> 4);  // 64 units of 16 bytes
        default:
            return ((1024 >> 4) + (64 >> 4));  // Default to BFP8 size
    };
}

/**
 * @brief Check if data format is a Block Floating Point (BFP) format
 *
 * @details Determines whether the specified format uses Block Floating Point
 * representation, which stores mantissas and shared exponents separately.
 * BFP formats require special handling for memory allocation and processing.
 *
 * @param format Data format identifier from DataFormat enum
 * @return true if format is BFP (Bfp8, Bfp4, Bfp2 variants), false otherwise
 */
constexpr static bool IS_BFP_FORMAT(uint format)
{
    switch (format & 0xF)
    {
        case ((uint8_t)DataFormat::Bfp8):
        case ((uint8_t)DataFormat::Bfp8_b):
        case ((uint8_t)DataFormat::Bfp4):
        case ((uint8_t)DataFormat::Bfp4_b):
        case ((uint8_t)DataFormat::Bfp2):
        case ((uint8_t)DataFormat::Bfp2_b):
            return true;
        default:
            return false;
    };
}

/**
 * @brief Check if data format is a BFP A-variant format
 *
 * @details Identifies BFP A-variant formats which have specific processing
 * requirements and hardware optimizations different from B-variants.
 *
 * @param format Data format identifier from DataFormat enum
 * @return true if format is BFP A-variant (Bfp8, Bfp4, Bfp2), false otherwise
 */
constexpr static bool IS_BFP_A_FORMAT(uint format)
{
    switch (format & 0xF)
    {
        case ((uint8_t)DataFormat::Bfp8):
        case ((uint8_t)DataFormat::Bfp4):
        case ((uint8_t)DataFormat::Bfp2):
            return true;
        default:
            return false;
    };
}

/**
 * @brief Check if data format is an A-variant format
 *
 * @details Identifies all A-variant formats including BFP and floating-point
 * types. A-variants typically have specific hardware optimizations and
 * processing characteristics.
 *
 * @param format Data format identifier from DataFormat enum
 * @return true if format is A-variant (Lf8, Float16, Bfp8, Bfp4, Bfp2), false otherwise
 */
constexpr static bool IS_A_FORMAT(uint format)
{
    switch (format & 0xF)
    {
        case ((uint8_t)DataFormat::Lf8):
        case ((uint8_t)DataFormat::Float16):
        case ((uint8_t)DataFormat::Bfp8):
        case ((uint8_t)DataFormat::Bfp4):
        case ((uint8_t)DataFormat::Bfp2):
            return true;
        default:
            return false;
    };
}

/**
 * @brief Scale datum count to byte size based on data format
 *
 * @details Converts a count of data elements to the corresponding byte size
 * based on the data format. Essential for memory allocation and transfer
 * size calculations.
 *
 * **Format Sizes:**
 * - **32-bit formats**: 4 bytes per datum (left shift by 2)
 * - **16-bit formats**: 2 bytes per datum (left shift by 1)  
 * - **8-bit formats**: 1 byte per datum (no scaling)
 *
 * @param format Data format identifier from DataFormat enum
 * @param datum_count Number of data elements
 * @return Total byte size for the specified number of datums
 */
constexpr static std::uint32_t SCALE_DATUM_SIZE(uint format, uint datum_count)
{
    switch (static_cast<DataFormat>(format & 0xF))
    {
        case DataFormat::Int32:
        case DataFormat::Float32:
            return datum_count << 2;  // 4 bytes per datum

        case DataFormat::Float16:
        case DataFormat::Float16_b:
        case DataFormat::UInt16:
            return datum_count << 1;  // 2 bytes per datum

        default:
            return datum_count;       // 1 byte per datum
    };
}

//
// **Bit Manipulation Macros**
//

/**
 * @brief Extract lower and upper halfwords from 32-bit values
 * @details Utility macros for bit manipulation and register operations
 */
#define LOWER_HALFWORD(x) ((x) & 0xFFFF)    ///< Extract lower 16 bits
#define UPPER_HALFWORD(x) ((x) >> 16)       ///< Extract upper 16 bits

//
// **Operation Type Enumerations**
//

/**
 * @brief Activation function types for neural network operations
 *
 * @details Identifies different activation functions supported by the SFPU.
 * Each activation function has specific mathematical properties and use cases
 * in neural network architectures.
 */
enum class ActivationType
{
    Celu        = 0,  ///< Continuously Differentiable Exponential Linear Unit
    Elu         = 1,  ///< Exponential Linear Unit
    Gelu        = 2,  ///< Gaussian Error Linear Unit
    Hardtanh    = 3,  ///< Hard Hyperbolic Tangent (piecewise linear)
    Hardsigmoid = 4,  ///< Hard Sigmoid (piecewise linear)
};

/**
 * @brief Binary mathematical operation types
 *
 * @details Defines the set of binary mathematical operations supported by
 * the compute units. These operations form the foundation of most mathematical
 * computations in neural networks and scientific computing.
 *
 * **Arithmetic Operations:**
 * - ADD, SUB, MUL, DIV: Basic arithmetic
 * - RSUB: Reverse subtraction (B - A instead of A - B)
 * - POW: Exponentiation (A^B)
 *
 * **Special Functions:**
 * - XLOGY: X * log(Y) with special handling for edge cases
 *
 * **Bit Operations:**
 * - RSHFT, LSHFT: Arithmetic right/left bit shifts
 * - LOGICAL_RSHFT: Logical right shift (zero-fill)
 */
enum class BinaryOp : uint8_t
{
    ADD           = 0,  ///< Addition (A + B)
    SUB           = 1,  ///< Subtraction (A - B)
    MUL           = 2,  ///< Multiplication (A * B)
    DIV           = 3,  ///< Division (A / B)
    RSUB          = 4,  ///< Reverse subtraction (B - A)
    POW           = 5,  ///< Power/exponentiation (A^B)
    XLOGY         = 6,  ///< X * log(Y) function
    RSHFT         = 7,  ///< Arithmetic right shift
    LSHFT         = 8,  ///< Arithmetic left shift
    LOGICAL_RSHFT = 9   ///< Logical right shift (zero-fill)
};

} // namespace ckernel
