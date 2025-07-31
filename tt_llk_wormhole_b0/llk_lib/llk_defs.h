// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

/**
 * @file llk_defs.h
 * @brief Core enumeration definitions and type constants for Low-Level Kernel operations
 *
 * @details This header defines the fundamental enumeration types and constants used throughout
 * the LLK ecosystem on Tenstorrent Tensix cores. These definitions provide type-safe abstractions
 * for hardware operation modes, data layouts, processing patterns, and algorithmic configurations.
 *
 * **Critical Importance:**
 * This file contains the foundational type definitions that control hardware behavior across
 * all LLK operations. Changes to these enumerations can affect performance, correctness, and
 * compatibility across the entire compute stack.
 *
 * **Key Categories:**
 * - **Vector Processing Modes**: Control SFPU execution patterns and data layout
 * - **Reduction Operations**: Define mathematical reduction algorithms and dimensions
 * - **Data Movement Types**: Specify source-to-destination transfer patterns
 * - **Element-wise Operations**: Binary and ternary mathematical operation types
 * - **Tile Geometry**: Define tile dimensions and face organization
 * - **Pooling Algorithms**: Mathematical pooling and aggregation strategies
 *
 * **Hardware Mapping:**
 * These enumerations directly map to hardware control signals and execution modes in the
 * Tensix processing pipeline. Understanding these definitions is essential for optimal
 * performance and correct operation configuration.
 *
 * **Performance Impact:**
 * Different enumeration values can significantly impact:
 * - Memory bandwidth utilization (vector modes)
 * - SFPU lane efficiency (processing patterns)
 * - Pipeline coordination (operation types)
 * - Cache behavior (data movement patterns)
 */

#pragma once

namespace ckernel
{

/**
 * @brief Vector processing modes for SFPU operations and data layout control
 * 
 * @details Defines how SFPU operations process tile data across the 2x2 face layout.
 * These modes control memory access patterns, computational efficiency, and data routing
 * for optimal performance across different algorithmic patterns.
 * 
 * **Face Layout Reference:**
 * ```
 * +-------+-------+  32x32 Tile Layout
 * | Face0 | Face1 |  (2x2 arrangement of 16x16 faces)
 * +-------+-------+
 * | Face2 | Face3 |
 * +-------+-------+
 * ```
 * 
 * **Performance Characteristics:**
 * - **RC modes**: Maximum SFPU utilization (all 32 lanes active)
 * - **Row/Column modes**: Optimized for specific data access patterns
 * - **None mode**: Minimal processing for special cases
 */
enum VectorMode
{
    None      = 0,  /**< No vector processing - minimal execution mode */
    R         = 1,  /**< Row vector mode - process faces 0+1 (top row), optimized for row-wise operations */
    C         = 2,  /**< Column vector mode - process faces 0+2 (left column), optimized for column-wise operations */
    RC        = 4,  /**< Full tile mode - process all 4 faces, maximum throughput and SFPU utilization */
    RC_custom = 6,  /**< Custom tile processing - specialized face iteration patterns for advanced algorithms */
    Invalid   = 0xFF, /**< Invalid mode marker - used for error detection and validation */
};

/**
 * @brief Reduction operation dimensions for mathematical aggregation functions
 * 
 * @details Specifies the dimension along which reduction operations (sum, mean, max, etc.)
 * are performed. Controls data flow patterns and memory access for optimal reduction performance.
 * 
 * **Reduction Patterns:**
 * ```
 * REDUCE_ROW:    [A B C D]     →  [sum]     (horizontal reduction)
 *                [E F G H]     →  [sum]
 * 
 * REDUCE_COL:    [A B]         →  [A+E]     (vertical reduction)
 *                [E F]         →  [B+F]
 * 
 * REDUCE_SCALAR: [A B]         →  [A+B+E+F] (complete reduction)
 *                [E F]
 * ```
 * 
 * **Performance Impact:**
 * - **ROW reduction**: Optimized memory coalescing for horizontal data access
 * - **COL reduction**: Stride-optimized access patterns for vertical aggregation
 * - **SCALAR reduction**: Maximum computational efficiency for complete aggregation
 */
enum ReduceDim
{
    REDUCE_ROW,    /**< Reduce across row dimension - horizontal aggregation with optimal memory coalescing */
    REDUCE_COL,    /**< Reduce across column dimension - vertical aggregation with stride-optimized access */
    REDUCE_SCALAR, /**< Reduce to scalar value - complete aggregation across all dimensions */
};

/**
 * @brief Tile dimension indices for addressing and geometry operations
 * 
 * @details Provides standardized indices for accessing tile dimension information
 * in arrays, coordinate systems, and geometric calculations. Ensures consistent
 * dimension ordering across the LLK ecosystem.
 * 
 * **Usage Examples:**
 * ```cpp
 * uint32_t tile_dims[2];
 * tile_dims[TileDim::R_IDX] = 32;  // Row dimension
 * tile_dims[TileDim::C_IDX] = 32;  // Column dimension
 * 
 * // Coordinate access patterns
 * auto row_size = dimensions[TileDim::R_IDX];
 * auto col_size = dimensions[TileDim::C_IDX];
 * ```
 */
enum TileDim
{
    R_IDX = 0, /**< Row dimension index - first coordinate in [row, column] ordering */
    C_IDX = 1, /**< Column dimension index - second coordinate in [row, column] ordering */
};

/**
 * @brief Pooling operation types for mathematical aggregation and downsampling
 * 
 * @details Defines the mathematical aggregation functions used in pooling operations.
 * These operations are fundamental for neural network layers, signal processing,
 * and statistical computations requiring spatial or temporal downsampling.
 * 
 * **Mathematical Definitions:**
 * - **SUM**: Σ(xi) - Arithmetic sum of all elements in pool window
 * - **AVG**: (1/n)Σ(xi) - Arithmetic mean, computed as sum divided by count
 * - **MAX**: max(xi) - Maximum value selection across pool window
 * 
 * **Performance Characteristics:**
 * - **SUM**: Fastest operation, single accumulation per element
 * - **AVG**: Moderate cost, requires division by pool size
 * - **MAX**: Comparison-intensive, may require specialized hardware paths
 * 
 * **Common Use Cases:**
 * - **SUM pooling**: Feature aggregation, attention mechanisms
 * - **AVG pooling**: Downsampling with smoothing, traditional CNNs
 * - **MAX pooling**: Feature extraction, translation invariance in vision models
 */
enum PoolType
{
    SUM, /**< Sum pooling - arithmetic summation across pool window (fastest) */
    AVG, /**< Average pooling - mean calculation with automatic normalization */
    MAX, /**< Max pooling - maximum value selection for feature extraction */
};

/**
 * @brief Data copy operation types for source-to-destination transfers
 * 
 * @details Specifies the source register type for data movement operations.
 * These operations are critical for data routing in the mathematical pipeline,
 * enabling flexible data flow patterns and register file management.
 * 
 * **Register File Architecture:**
 * ```
 * SrcA Register File  →  DEST Register File  (A2D operations)
 * SrcB Register File  →  DEST Register File  (B2D operations)
 * ```
 * 
 * **Operational Context:**
 * - **A2D**: Primary data path, typically used for main operand transfers
 * - **B2D**: Secondary data path, often used for broadcast operands or constants
 * 
 * **Performance Considerations:**
 * - Both paths have equivalent bandwidth and latency characteristics
 * - Choice depends on register allocation strategy and data dependency patterns
 * - Can be used simultaneously for dual-stream data movement operations
 */
enum DataCopyType
{
    A2D, /**< Source A to Destination copy - primary data path transfer */
    B2D, /**< Source B to Destination copy - secondary data path transfer */
};

/**
 * @brief Element-wise binary operation types for mathematical computations
 * 
 * @details Defines the fundamental binary mathematical operations performed element-wise
 * across tiles. These operations form the building blocks for complex mathematical
 * expressions and are heavily optimized for parallel execution on SFPU hardware.
 * 
 * **Mathematical Definitions:**
 * - **ELWMUL**: Element-wise multiplication (C[i] = A[i] * B[i])
 * - **ELWDIV**: Element-wise division (C[i] = A[i] / B[i])
 * - **ELWADD**: Element-wise addition (C[i] = A[i] + B[i])
 * - **ELWSUB**: Element-wise subtraction (C[i] = A[i] - B[i])
 * - **ELWLESS**: Element-wise less-than comparison (C[i] = A[i] < B[i] ? 1.0 : 0.0)
 * 
 * **Performance Characteristics:**
 * - **ELWMUL/ELWADD**: Single-cycle execution on SFPU hardware (fastest)
 * - **ELWSUB**: Single-cycle execution, equivalent performance to addition
 * - **ELWDIV**: Multi-cycle execution, may require specialized division units
 * - **ELWLESS**: Comparison operation, single-cycle with conditional output
 * 
 * **Common Usage Patterns:**
 * - **ELWMUL**: Scaling, Hadamard products, attention weights
 * - **ELWADD**: Bias addition, residual connections, accumulation
 * - **ELWSUB**: Difference computation, residual calculation
 * - **ELWDIV**: Normalization, ratio computation (use with caution for division by zero)
 * - **ELWLESS**: Masking, conditional selection, boolean logic
 */
enum EltwiseBinaryType
{
    ELWMUL,  /**< Element-wise multiplication - parallel multiply across all tile elements */
    ELWDIV,  /**< Element-wise division - parallel divide with potential for zero-division handling */
    ELWADD,  /**< Element-wise addition - parallel addition across all tile elements */
    ELWSUB,  /**< Element-wise subtraction - parallel subtraction across all tile elements */
    ELWLESS, /**< Element-wise less-than comparison - parallel conditional comparison */
};

/**
 * @brief Destination register reuse patterns for element-wise binary operations
 * 
 * @details Controls how the destination register is reused as a source operand in
 * element-wise binary operations. This optimization reduces memory movement and
 * enables efficient in-place operations and accumulation patterns.
 * 
 * **Reuse Patterns:**
 * ```
 * NONE:         SrcA ⊕ SrcB → Dest        (standard two-operand operation)
 * DEST_TO_SRCA: Dest ⊕ SrcB → Dest       (reuse destination as first operand)
 * DEST_TO_SRCB: SrcA ⊕ Dest → Dest       (reuse destination as second operand)
 * ```
 * 
 * **Performance Benefits:**
 * - **Reduces register pressure**: Fewer registers needed for intermediate results
 * - **Minimizes data movement**: Eliminates unnecessary copy operations
 * - **Enables accumulation**: Perfect for sum/product accumulation patterns
 * - **In-place operations**: Supports destructive updates for memory efficiency
 * 
 * **Common Usage Examples:**
 * ```cpp
 * // Accumulation: result += new_value
 * DEST_TO_SRCA + ELWADD  // dest = dest + src
 * 
 * // Scaling: value *= scale_factor  
 * DEST_TO_SRCA + ELWMUL  // dest = dest * src
 * 
 * // Residual: output = input + residual
 * DEST_TO_SRCB + ELWADD  // dest = src + dest
 * ```
 */
enum class EltwiseBinaryReuseDestType
{
    NONE         = 0, /**< No reuse - standard two-operand binary operation */
    DEST_TO_SRCA = 1, /**< Reuse destination as first operand (dest ⊕ srcB → dest) */
    DEST_TO_SRCB = 2, /**< Reuse destination as second operand (srcA ⊕ dest → dest) */
};

/**
 * @brief Destination synchronization modes for pipeline coordination
 * 
 * @details Controls the synchronization behavior between mathematical operations
 * and subsequent pipeline stages. These modes balance throughput and data consistency
 * requirements for optimal performance across different algorithmic patterns.
 * 
 * **Synchronization Behavior:**
 * - **SyncHalf**: Half-buffer synchronization for maximum throughput
 * - **SyncFull**: Complete synchronization for guaranteed data consistency
 * 
 * **Performance Trade-offs:**
 * ```
 * SyncHalf: Higher throughput | Lower latency | Risk of data hazards
 * SyncFull: Lower throughput  | Higher latency | Guaranteed consistency
 * ```
 * 
 * **Usage Guidelines:**
 * - **SyncHalf**: Use for independent operations where data hazards are impossible
 * - **SyncFull**: Use when subsequent operations depend on complete data availability
 * - **Pipeline depth**: SyncFull required for operations with complex dependencies
 */
enum DstSync
{
    SyncHalf = 0, /**< Half-buffer sync - optimized for throughput, minimal latency */
    SyncFull = 1, /**< Full synchronization - guaranteed data consistency, higher latency */
};

/**
 * @brief Broadcasting patterns for tensor operations and data distribution
 * 
 * @details Defines how operands are broadcast across tile dimensions for element-wise
 * operations. Broadcasting enables efficient computation between tensors of different
 * shapes by automatically replicating data along specified dimensions.
 * 
 * **Broadcasting Patterns:**
 * ```
 * NONE:   [A B] ⊕ [C D] → [A⊕C B⊕D]  (no broadcasting, element-wise)
 *         [E F]   [G H]   [E⊕G F⊕H]
 * 
 * COL:    [A B] ⊕ [C] → [A⊕C B⊕C]     (column broadcast)
 *         [E F]   [G]   [E⊕G F⊕G]
 * 
 * ROW:    [A B] ⊕ [C D] → [A⊕C B⊕D]   (row broadcast)
 *         [E F]           [E⊕C F⊕D]
 * 
 * SCALAR: [A B] ⊕ [S] → [A⊕S B⊕S]     (scalar broadcast)
 *         [E F]         [E⊕S F⊕S]
 * ```
 * 
 * **Performance Characteristics:**
 * - **NONE**: Maximum bandwidth utilization, no replication overhead
 * - **SCALAR**: Most efficient broadcast, single value replicated
 * - **ROW/COL**: Moderate overhead, optimized memory access patterns
 * 
 * **Memory Access Optimization:**
 * - **COL broadcast**: Stride-1 access, cache-friendly for vertical operations
 * - **ROW broadcast**: Optimized for horizontal data replication
 * - **SCALAR broadcast**: Minimal memory bandwidth, maximum compute efficiency
 */
enum BroadcastType
{
    NONE   = 0x0, /**< No broadcasting - element-wise operation between tensors of same shape */
    COL    = 0x1, /**< Column broadcast - replicate operand across columns (vertical expansion) */
    ROW    = 0x2, /**< Row broadcast - replicate operand across rows (horizontal expansion) */
    SCALAR = 0x3, /**< Scalar broadcast - replicate single value across entire tile */
};

enum src_op_id_e
{
    OP_SRC0 = 0,
    OP_SRC1 = 1,
    OP_SRC2 = 2,
    OP_SRC3 = 3,
    OP_SRC4 = 4,
};

enum local_op_id_e
{
    OP_LOCAL0 = 0,
    OP_LOCAL1 = 1,
    OP_LOCAL2 = 2,
    OP_LOCAL3 = 3,
    OP_LOCAL4 = 4,
};

enum out_op_id_e
{
    OUT_ID0 = 0,
    OUT_ID1 = 1,
    OUT_ID2 = 2,
    OUT_ID3 = 3,
    OUT_ID4 = 4,
};

/**
 * @brief ReLU (Rectified Linear Unit) activation function variants
 * 
 * @details Defines different types of ReLU activation functions commonly used in
 * neural networks and machine learning. These functions provide non-linearity
 * while maintaining computational efficiency for training and inference.
 * 
 * **Mathematical Definitions:**
 * - **NO_RELU**: f(x) = x (identity function, no activation)
 * - **ZERO_RELU**: f(x) = max(0, x) (standard ReLU, clamp negative values to zero)
 * - **MIN_THRESHOLD_RELU**: f(x) = max(threshold, x) (clamp below minimum threshold)
 * - **MAX_THRESHOLD_RELU**: f(x) = min(threshold, x) (clamp above maximum threshold)
 * 
 * **Performance Characteristics:**
 * - **NO_RELU**: Zero computational overhead (pass-through)
 * - **ZERO_RELU**: Single comparison per element, minimal overhead
 * - **Threshold variants**: Slight additional overhead for threshold comparison
 * 
 * **Common Usage Patterns:**
 * - **NO_RELU**: Linear layers, final outputs, debug/bypass mode
 * - **ZERO_RELU**: Standard hidden layers, most common activation
 * - **Thresholded ReLU**: Specialized networks, preventing dead neurons, gradient clipping
 */
enum ReluType
{
    NO_RELU,             /**< No activation - identity function (f(x) = x) */
    ZERO_RELU,           /**< Standard ReLU - clamp negative values (f(x) = max(0, x)) */
    MIN_THRESHOLD_RELU,  /**< Minimum threshold ReLU - clamp below threshold (f(x) = max(threshold, x)) */
    MAX_THRESHOLD_RELU,  /**< Maximum threshold ReLU - clamp above threshold (f(x) = min(threshold, x)) */
};

constexpr bool UnpackToDestEn  = true;
constexpr bool UnpackToDestDis = false;

/*
Stochastic rounding modes:
    None: No stochastic rounding enabled, default rounding is round to nearest even.
    Fpu: Enables stochastic rounding for every accumulation in the fpu
    Pack: Enables stochastic rounding in both gasket and packer. Gasket rounding is in
    data format conversion stage from dest format to pack_src_format. Packer rounding
    is in data format conversion stage from pack_src_format to pack_dst_format.
    All: Enables fpu, pack and gasket rounding.
*/
enum struct StochRndType
{
    None = 0,
    Fpu  = 1,
    Pack = 2,
    All  = 0xf,
};

// This is populated per Wormhole ISA for SFPLOAD/SFPSTORE instructions.
enum InstrModLoadStore
{
    DEFAULT       = 0,
    FP16A         = 1,
    FP16B         = 2,
    FP32          = 3,
    INT32         = 4,
    INT8          = 5,
    LO16          = 6,
    HI16          = 7,
    INT32_2S_COMP = 12,
    INT8_2S_COMP  = 13,
    LO16_ONLY     = 14,
    HI16_ONLY     = 15
};

} // namespace ckernel
