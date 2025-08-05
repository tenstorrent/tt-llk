// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

/**
 * @file ckernel_defs.h
 * @brief Core Definitions and Constants for Wormhole B0 Tensix Kernel
 *
 * This header provides fundamental definitions, enumerations, and constants
 * used throughout the TT-LLK (Tenstorrent Low-Level Kernel) system. It serves
 * as the foundation for type-safe programming and consistent data organization
 * across all kernel operations.
 *
 * @author Tenstorrent AI ULC
 * @version 1.0
 * @date 2025
 *
 * # Key Components
 *
 * - **Core Enumerations**: Type-safe constants for sources, destinations, threads
 * - **Tile Organization**: Constants and layouts for tile-based processing
 * - **Data Format Support**: Format-aware size calculation and utilities
 * - **Hardware Abstraction**: Register space definitions and addressing modes
 * - **Operation Types**: Activation functions, binary operations, and processing modes
 *
 * # Tile Architecture
 *
 * Wormhole B0 processes data in tiles organized as follows:
 * - **Standard Tile**: 32x32 elements (1024 total)
 * - **Face Structure**: Each tile contains 4 faces of 16x16 elements
 * - **Flexible Shapes**: Support for 32x32, 32x16, and 16x16 tiles
 * - **Face Layout**: Row-major (default) or column-major organization
 *
 * # Data Format System
 *
 * Supports multiple precision formats with automatic size calculation:
 * - **Float32**: 32-bit floating point (4 bytes per element)
 * - **Float16**: 16-bit floating point (2 bytes per element)
 * - **Int32/Int8**: Integer formats
 * - **BFP2/4/8**: Block Floating Point with compression
 *
 * # Usage Example
 *
 * ```cpp
 * #include "ckernel_defs.h"
 *
 * // Configure operation sources
 * configure_operation(Srcs::SrcA, Srcs::SrcB);
 *
 * // Set destination parameters
 * setup_destination(DstTileShape::Tile32x32, DstTileFaceLayout::RowMajor);
 *
 * // Calculate tile size for format
 * uint32_t size = GET_L1_HEADERLESS_TILE_SIZE(
 *     static_cast<uint>(DataFormat::Float16)
 * );
 * ```
 *
 * @note This header contains fundamental types used throughout the kernel.
 *       Understanding these definitions is essential for effective LLK programming.
 *
 * @see llk_defs.h for data format definitions
 * @see tensix.h for hardware-specific constants
 * @see ckernel_ops.h for instruction generation using these definitions
 */

#pragma once

#include <cstdint>

#include "ckernel_ops.h"
#include "llk_defs.h"
#include "tensix.h"
#include "tensix_types.h"

/**
 * @namespace ckernel
 * @brief Core kernel functionality and hardware abstraction layer
 */
namespace ckernel
{

/**
 * @defgroup CoreEnumerations Core System Enumerations
 * @brief Fundamental type-safe constants for kernel operations
 * @{
 */

/**
 * @enum Srcs
 * @brief Source operand selection for mathematical operations
 *
 * Defines which source operand is being referenced in operations that
 * can have multiple inputs (A, B, C). This provides type safety and
 * clarity when configuring multi-operand operations.
 */
enum Srcs
{
    SrcA = 0, ///< Source operand A (primary input, often weights)
    SrcB = 1, ///< Source operand B (secondary input, often activations)
    SrcC = 2  ///< Source operand C (tertiary input, for ternary operations)
};

/**
 * @enum Unpackers
 * @brief Unpacker block selection for data input configuration
 *
 * Wormhole B0 has two independent unpackers that can be controlled
 * individually or together. This enum provides type-safe selection.
 */
enum Unpackers
{
    Unp0   = 0, ///< Unpacker 0 (typically for source A/weights)
    Unp1   = 1, ///< Unpacker 1 (typically for source B/activations)
    UnpAll = 2  ///< Both unpackers (for synchronized operations)
};

/**
 * @enum DstStart
 * @brief Destination accumulator starting offset
 *
 * Controls where accumulation begins in the destination register file.
 * This is critical for operations that need to preserve existing data
 * or implement specific accumulation patterns.
 */
enum DstStart
{
    StartZero = 0, ///< Start accumulation from beginning of destination
    StartHalf = 1  ///< Start accumulation from middle of destination
};

/**
 * @enum DstClear
 * @brief Destination register clearing modes
 *
 * Defines how much of the destination register space should be cleared
 * before or during operations. Different modes support various optimization
 * strategies and memory management approaches.
 */
enum DstClear
{
    ClearRow    = 0, ///< Clear single row only
    Clear16Rows = 1, ///< Clear 16 rows (one face)
    ClearHalf   = 2, ///< Clear half of destination space
    ClearFull   = 3  ///< Clear entire destination space
};

/**
 * @enum ThreadId
 * @brief Hardware thread identification for pipeline control
 *
 * Wormhole B0 has multiple hardware threads that handle different
 * stages of the processing pipeline. This enum provides type-safe
 * thread identification for synchronization and control.
 */
enum ThreadId
{
    UnpackThreadId = 0, ///< Unpack thread (data input stage)
    MathThreadId   = 1, ///< Math thread (computation stage)
    PackThreadId   = 2  ///< Pack thread (data output stage)
};

/**
 * @enum DstTileLayout
 * @brief Destination tile memory layout modes
 *
 * Controls how destination tiles are organized in memory. Different layouts
 * optimize for different access patterns and computation types.
 */
enum DstTileLayout
{
    Default,     ///< Standard tile layout (row-major within faces)
    Interleaved, ///< Interleaved layout for optimized access patterns
    // Future layouts (commented out):
    // TightDest,        ///< Compact destination layout
    // Conv3x3,          ///< Optimized for 3x3 convolution
    // Conv1x1,          ///< Optimized for 1x1 convolution
    // L1ReadSource,     ///< Optimized for L1 read operations
    // NLLLoss,          ///< Optimized for negative log likelihood loss
    // IndexAccumulate,  ///< Index accumulation with polling before pack
};

/**
 * @enum DstTileFaceLayout
 * @brief Face-level layout within destination tiles
 *
 * Each tile consists of 4 faces (16x16 elements each). This enum controls
 * how elements are organized within each face for optimal memory access.
 */
enum DstTileFaceLayout
{
    RowMajor, ///< Row-major layout (default, sequential row access)
    ColMajor, ///< Column-major layout (sequential column access)
};

/**
 * @enum DstTileShape
 * @brief Supported destination tile shapes
 *
 * Defines the physical dimensions of tiles that can be processed.
 * Different shapes optimize for different types of operations and
 * memory usage patterns.
 */
enum DstTileShape
{
    Tile32x32 = 0, ///< Standard 32x32 tile (1024 elements)
    Tile32x16 = 1, ///< Wide 32x16 tile (512 elements)
    Tile16x16 = 2  ///< Compact 16x16 tile (256 elements)
};

/**
 * @enum class ParallelPackerMode
 * @brief Parallel packer operation modes
 *
 * Controls how multiple packers work together for improved throughput.
 * Different modes optimize for different data patterns and performance
 * requirements.
 */
enum class ParallelPackerMode
{
    Disabled,      ///< Single packer operation (default)
    SingleFTEntry, ///< Single FIFO/Table entry shared across packers
    MultiFTEntry,  ///< Multiple FIFO/Table entries for parallel operation
    TileParallel   ///< Tile-level parallelism across packers
};

/**
 * @enum register_space_e
 * @brief Hardware register space identification
 *
 * Defines different register spaces available in the Tensix core.
 * Used for precise register addressing and hardware control.
 */
enum register_space_e
{
    TDMA_REGS     = 0x0, ///< Tensor DMA registers
    LOCAL_REGS    = 0x1, ///< Local processor registers
    ADDR_COUNTERS = 0x2  ///< Address counter registers
};

/**
 * @enum PackSelMask
 * @brief Pack unit selection bitmasks
 *
 * Wormhole B0 has 4 independent pack units. This enum provides
 * bitmask constants for selecting individual or groups of packers
 * for parallel operation.
 */
enum PackSelMask
{
    PACK_ALL = 0xF, ///< All 4 packers (default, bits 0-3)
    PACK_0   = 0x1, ///< Packer 0 only (bit 0)
    PACK_1   = 0x2, ///< Packer 1 only (bit 1)
    PACK_2   = 0x4, ///< Packer 2 only (bit 2)
    PACK_3   = 0x8, ///< Packer 3 only (bit 3)
    PACK_01  = 0x3, ///< Packers 0 and 1 (bits 0-1)
    PACK_23  = 0xC  ///< Packers 2 and 3 (bits 2-3)
};

/**
 * @enum SortDir
 * @brief Sort direction for argmax/argmin operations
 *
 * Used in sorting and selection operations to specify whether to
 * find maximum or minimum values. Implemented as a boolean enum
 * for efficiency.
 */
enum SortDir : bool
{
    ArgMax = false, ///< Find maximum values (ascending sort)
    ArgMin = true,  ///< Find minimum values (descending sort)
};

/** @} */ // end of CoreEnumerations group

/**
 * @defgroup TileConstants Tile and Face Dimension Constants
 * @brief Fundamental constants for tile-based data organization
 * @{
 */

constexpr std::uint32_t FACE_HEIGHT      = 16; ///< Height of a single face in elements
constexpr std::uint32_t FACE_WIDTH       = 16; ///< Width of a single face in elements
constexpr std::uint32_t TILE_HEIGHT      = 32; ///< Height of a standard tile in elements
constexpr std::uint32_t TILE_WIDTH       = 32; ///< Width of a standard tile in elements
constexpr std::uint32_t DATUMS_PER_ROW   = 16; ///< Number of data elements per row
constexpr std::uint32_t TILE_HEADER_SIZE = 1;  ///< Size of tile header in words

// Legacy aliases for compatibility
constexpr std::uint32_t FACE_R_DIM = FACE_HEIGHT; ///< Face row dimension (alias)
constexpr std::uint32_t FACE_C_DIM = FACE_WIDTH;  ///< Face column dimension (alias)

constexpr std::uint32_t TILE_R_DIM = TILE_HEIGHT; ///< Tile row dimension (alias)
constexpr std::uint32_t TILE_C_DIM = TILE_WIDTH;  ///< Tile column dimension (alias)

/**
 * @brief Number of faces per standard tile
 *
 * Each 32x32 tile contains exactly 4 faces arranged in a 2x2 pattern.
 * Each face is 16x16 elements.
 */
constexpr std::uint32_t TILE_NUM_FACES = ((TILE_R_DIM * TILE_C_DIM) / (FACE_R_DIM * FACE_C_DIM));

/**
 * @brief Number of FP16 tiles that fit in destination register space
 *
 * Calculated based on the full destination register size and tile dimensions.
 * This determines the maximum number of tiles that can be held simultaneously.
 */
constexpr uint32_t DEST_NUM_TILES_FP16 = (DEST_REGISTER_FULL_SIZE * DEST_FACE_WIDTH) / (TILE_HEIGHT * TILE_HEIGHT);

/**
 * @brief Half of the FP16 tile capacity in destination registers
 *
 * Used for operations that process data in two halves or need to
 * split processing across multiple passes.
 */
constexpr uint32_t DEST_NUM_TILES_FP16_HALF = DEST_NUM_TILES_FP16 / 2;

// Compile-time assertion to ensure tile count is power of 2 for efficient addressing
static_assert((DEST_NUM_TILES_FP16 & (DEST_NUM_TILES_FP16 - 1)) == 0);

/** @} */ // end of TileConstants group

/**
 * @defgroup UtilityMacros Utility Macros and Helper Functions
 * @brief Helper macros for register addressing and data manipulation
 * @{
 */

/**
 * @brief Access lower 16 bits of a register
 * @param REG Register number
 * @return Address for lower 16-bit half of the register
 *
 * Many Tensix instructions can address 16-bit halves of 32-bit registers
 * for fine-grained control and efficient data movement.
 */
#define LO_16(REG) (2 * (REG))

/**
 * @brief Access upper 16 bits of a register
 * @param REG Register number
 * @return Address for upper 16-bit half of the register
 *
 * Companion to LO_16 for accessing the upper half of 32-bit registers.
 */
#define HI_16(REG) (2 * (REG) + 1)

/**
 * @brief Calculate L1 tile size without header for given data format
 *
 * Computes the size in 16-byte units of a tile stored in L1 memory,
 * excluding the header. The size varies based on the data format due
 * to different element sizes and compression.
 *
 * @param format Data format value (see DataFormat enum)
 * @return Tile size in 16-byte units (shift right by 4 for byte size)
 *
 * # Format Size Breakdown:
 * - **Float32/Int32**: 4096 bytes → 256 units (4 bytes × 1024 elements)
 * - **Float16**: 2048 bytes → 128 units (2 bytes × 1024 elements)
 * - **BFP8**: 1088 bytes → 68 units (1024 + 64 bytes for metadata)
 * - **BFP4**: 576 bytes → 36 units (512 + 64 bytes for metadata)
 * - **BFP2**: 320 bytes → 20 units (256 + 64 bytes for metadata)
 * - **Int8**: 1024 bytes → 64 units (1 byte × 1024 elements)
 *
 * @note BFP (Block Floating Point) formats include metadata overhead
 * @note Result is in 16-byte units for alignment with hardware addressing
 */
constexpr static std::uint32_t GET_L1_HEADERLESS_TILE_SIZE(uint format)
{
    switch (format & 0xF)
    {
        case ((uint8_t)DataFormat::Int32):
        case ((uint8_t)DataFormat::Float32):
            return (4096 >> 4);
        case ((uint8_t)DataFormat::Float16):
        case ((uint8_t)DataFormat::Float16_b):
            return (2048 >> 4);
        case ((uint8_t)DataFormat::Bfp8):
        case ((uint8_t)DataFormat::Bfp8_b):
            return ((1024 >> 4) + (64 >> 4));
        case ((uint8_t)DataFormat::Bfp4):
        case ((uint8_t)DataFormat::Bfp4_b):
            return ((512 >> 4) + (64 >> 4));
        case ((uint8_t)DataFormat::Bfp2):
        case ((uint8_t)DataFormat::Bfp2_b):
            return ((256 >> 4) + (64 >> 4));
        case ((uint8_t)DataFormat::Int8):
        case ((uint8_t)DataFormat::Lf8):
            return (1024 >> 4);
        default:
            return ((1024 >> 4) + (64 >> 4));
    };
}

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
 * Converts a count of data elements to the corresponding byte size
 * based on the data format. Used for memory allocation and addressing
 * calculations.
 *
 * @param format Data format value (see DataFormat enum)
 * @param datum_count Number of data elements
 * @return Total byte size for the specified number of elements
 *
 * # Size Scaling:
 * - **Float32/Int32**: 4 bytes per element (shift left by 2)
 * - **Float16/UInt16**: 2 bytes per element (shift left by 1)
 * - **All others**: 1 byte per element (Int8, BFP formats)
 */
constexpr static std::uint32_t SCALE_DATUM_SIZE(uint format, uint datum_count)
{
    switch (static_cast<DataFormat>(format & 0xF))
    {
        case DataFormat::Int32:
        case DataFormat::Float32:
            return datum_count << 2; // 4 bytes per element

        case DataFormat::Float16:
        case DataFormat::Float16_b:
        case DataFormat::UInt16:
            return datum_count << 1; // 2 bytes per element

        default:
            return datum_count; // 1 byte per element
    };
}

/**
 * @brief Extract lower 16 bits from a 32-bit value
 * @param x 32-bit input value
 * @return Lower 16 bits as uint16_t equivalent
 */
#define LOWER_HALFWORD(x) ((x) & 0xFFFF)

/**
 * @brief Extract upper 16 bits from a 32-bit value
 * @param x 32-bit input value
 * @return Upper 16 bits shifted to lower position
 */
#define UPPER_HALFWORD(x) ((x) >> 16)

/** @} */ // end of UtilityMacros group

/**
 * @defgroup OperationTypes Operation Type Enumerations
 * @brief Type-safe constants for mathematical and activation operations
 * @{
 */

/**
 * @enum class ActivationType
 * @brief Supported activation function types
 *
 * Defines the activation functions available in the SFPU (Special Function
 * Processing Unit) for neural network operations. Each activation has
 * optimized hardware implementations.
 */
enum class ActivationType
{
    Celu        = 0, ///< Continuously Differentiable Exponential Linear Unit
    Elu         = 1, ///< Exponential Linear Unit
    Gelu        = 2, ///< Gaussian Error Linear Unit
    Hardtanh    = 3, ///< Hard Hyperbolic Tangent (clamped linear)
    Hardsigmoid = 4, ///< Hard Sigmoid (piecewise linear approximation)
};

/**
 * @enum class BinaryOp
 * @brief Binary mathematical operations
 *
 * Defines binary operations that can be performed between two operands.
 * These operations are supported by the math pipeline with optimized
 * hardware implementations.
 */
enum class BinaryOp : uint8_t
{
    ADD           = 0, ///< Addition (A + B)
    SUB           = 1, ///< Subtraction (A - B)
    MUL           = 2, ///< Multiplication (A × B)
    DIV           = 3, ///< Division (A ÷ B)
    RSUB          = 4, ///< Reverse subtraction (B - A)
    POW           = 5, ///< Power (A^B)
    XLOGY         = 6, ///< X × log(Y) operation
    RSHFT         = 7, ///< Right shift (arithmetic)
    LSHFT         = 8, ///< Left shift
    LOGICAL_RSHFT = 9  ///< Logical right shift (unsigned)
};

/** @} */ // end of OperationTypes group

} // namespace ckernel
