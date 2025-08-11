// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

/**
 * @file llk_static_asserts.h
 * @brief Comprehensive compile-time validation for LLK operations
 *
 * @details This header provides critical static assertions for Wormhole B0 hardware
 * constraints and common programming errors in LLK code. These compile-time checks
 * catch errors early and provide clear diagnostic messages.
 *
 * **Enhanced Features:**
 * - Hardware limit validations based on Wormhole B0 specifications
 * - Memory architecture constraint checking
 * - Performance parameter validation
 * - Data type and precision limit enforcement
 * - Template and instruction parameter verification
 *
 * **Design Principles:**
 * - Zero runtime overhead - all checks are compile-time only
 * - Minimal dependencies to avoid circular includes
 * - Focus on highest-impact validation areas
 * - Clear, actionable error messages with hardware context
 *
 * **Architecture Reference**: Wormhole B0 Tensix Programming Guide
 */

#pragma once

#include <cstdint>
#include <type_traits>

#include "llk_hardware_limits.h"

namespace llk_validation
{

/**
 * @brief Validates face count for hardware operations
 */
template <uint32_t face_count>
constexpr void validate_face_count()
{
    static_assert(
        face_count == 1 || face_count == 2 || face_count == 4,
        "CRITICAL: Face count must be 1, 2, or 4 for Wormhole B0 hardware. "
        "Other values will cause undefined behavior in tile operations.");
}

/**
 * @brief Validates tile dimensions for hardware compatibility
 */
template <uint32_t tile_r_dim, uint32_t tile_c_dim>
constexpr void validate_tile_dimensions()
{
    static_assert(tile_r_dim > 0 && tile_c_dim > 0, "CRITICAL: Tile dimensions must be positive");
    static_assert(tile_r_dim <= 32 && tile_c_dim <= 32, "CRITICAL: Tile dimensions must not exceed 32x32 for Wormhole B0");
    static_assert((tile_r_dim % 16) == 0 && (tile_c_dim % 16) == 0, "CRITICAL: Tile dimensions must be multiples of 16 (face dimension)");
}

/**
 * @brief Validates address mode index for hardware limits
 */
template <uint32_t addr_mod_index>
constexpr void validate_address_mode()
{
    static_assert(addr_mod_index < 8, "CRITICAL: Address mode index must be 0-7 for Wormhole B0 hardware");
}

/**
 * @brief Validates math fidelity descriptor range
 */
template <int math_fidelity>
constexpr void validate_math_fidelity()
{
    static_assert(math_fidelity >= 0 && math_fidelity <= 15, "CRITICAL: Math fidelity descriptor must be 0-15 for hardware compatibility");
}

/**
 * @brief Validates throttle level for Wormhole B0
 */
template <int throttle_level>
constexpr void validate_throttle_level()
{
    static_assert(throttle_level >= 0 && throttle_level <= 3, "CRITICAL: Throttle level must be 0-3 for Wormhole B0 hardware");
}

/**
 * @brief Validates matrix multiplication dimensions for compatibility
 */
template <uint32_t ct_dim, uint32_t rt_dim, uint32_t kt_dim>
constexpr void validate_matmul_dimensions()
{
    static_assert(ct_dim > 0 && rt_dim > 0 && kt_dim > 0, "CRITICAL: Matrix dimensions must be positive");
    static_assert(ct_dim <= 32 && rt_dim <= 32 && kt_dim <= 32, "CRITICAL: Matrix dimensions must not exceed 32 for tile operations");
}

// ============================================================================
// **MEMORY ARCHITECTURE VALIDATIONS**
// ============================================================================

/**
 * @brief Validates L1 memory access parameters
 */
template <uint32_t bank_id, uint32_t access_width_bits>
constexpr void validate_l1_access()
{
    using namespace llk_hardware_limits::memory;
    static_assert(bank_id < L1_BANK_COUNT, "CRITICAL: L1 bank ID must be 0-15 for Wormhole B0 (16 banks available)");
    static_assert(
        access_width_bits == L1_ACCESS_WIDTH_BITS || access_width_bits < L1_ACCESS_WIDTH_BITS, "CRITICAL: L1 access width must not exceed 128 bits per bank");
}

/**
 * @brief Validates instruction cache usage parameters
 */
template <uint32_t instruction_count, uint32_t target_riscv>
constexpr void validate_icache_usage()
{
    using namespace llk_hardware_limits::memory;
    constexpr uint32_t instruction_size_bytes = 4;
    constexpr uint32_t icache_sizes[]         = {
        ICACHE_RISCV_B_SIZE_BYTES,  // RISCV B
        ICACHE_RISCV_T0_SIZE_BYTES, // RISCV T0
        ICACHE_RISCV_T1_SIZE_BYTES, // RISCV T1
        ICACHE_RISCV_T2_SIZE_BYTES, // RISCV T2
        ICACHE_RISCV_NC_SIZE_BYTES  // RISCV NC
    };

    static_assert(target_riscv < 5, "CRITICAL: Invalid RISCV target (must be 0-4)");
    static_assert(
        instruction_count * instruction_size_bytes <= icache_sizes[target_riscv],
        "CRITICAL: Instruction count exceeds instruction cache capacity for target RISCV");
}

/**
 * @brief Validates destination register access patterns
 */
template <uint32_t row_index, uint32_t col_index, bool is_32bit_mode>
constexpr void validate_dst_access()
{
    using namespace llk_hardware_limits::registers;
    if constexpr (is_32bit_mode)
    {
        static_assert(row_index < DST_32B_ROWS, "CRITICAL: Dst row index exceeds 32-bit mode limit (512 rows)");
        static_assert(col_index < DST_32B_COLS, "CRITICAL: Dst column index exceeds limit (16 columns)");
    }
    else
    {
        static_assert(row_index < DST_16B_ROWS, "CRITICAL: Dst row index exceeds 16-bit mode limit (1024 rows)");
        static_assert(col_index < DST_16B_COLS, "CRITICAL: Dst column index exceeds limit (16 columns)");
    }
}

/**
 * @brief Validates source register bank and addressing
 */
template <uint32_t bank_id, uint32_t row_index, uint32_t col_index>
constexpr void validate_src_access()
{
    using namespace llk_hardware_limits::registers;
    static_assert(bank_id < SRC_BANK_COUNT, "CRITICAL: Source register bank must be 0-1 (2 banks available)");
    static_assert(row_index < SRC_BANK_ROWS, "CRITICAL: Source register row index exceeds limit (64 rows per bank)");
    static_assert(col_index < SRC_BANK_COLS, "CRITICAL: Source register column index exceeds limit (16 columns)");
}

// ============================================================================
// **PERFORMANCE CONSTRAINT VALIDATIONS**
// ============================================================================

/**
 * @brief Validates fidelity phase count for performance expectations
 */
template <uint32_t phase_count>
constexpr void validate_fidelity_phases()
{
    using namespace llk_hardware_limits::performance;
    static_assert(phase_count >= 1 && phase_count <= MATRIX_UNIT_MAX_FIDELITY_PHASES, "CRITICAL: Fidelity phase count must be 1-4 for Wormhole B0 Matrix Unit");
}

/**
 * @brief Validates instruction latency expectations
 */
template <uint32_t expected_latency_cycles>
constexpr void validate_instruction_latency()
{
    using namespace llk_hardware_limits::performance;
    static_assert(
        expected_latency_cycles <= MATRIX_UNIT_LATENCY_CYCLES * 2, "WARNING: Expected latency significantly exceeds typical Matrix Unit latency (5 cycles)");
}

/**
 * @brief Validates memory bandwidth requirements
 */
template <uint32_t required_bandwidth_bps, uint32_t memory_client_type>
constexpr void validate_memory_bandwidth()
{
    using namespace llk_hardware_limits::performance;
    // Memory client types: 0=RISCV, 1=Mover, 2=Unpacker, 3=Packer, 4=NoC
    constexpr uint32_t max_bandwidths[] = {RISCV_LOAD_BPC, MOVER_READ_BPC, UNPACKER_MAX_READ_BPC, PACKER_MAX_WRITE_BPC, NOC_READ_BPC};

    static_assert(memory_client_type < 5, "CRITICAL: Invalid memory client type");
    static_assert(
        required_bandwidth_bps <= max_bandwidths[memory_client_type] * CLOCK_FREQUENCY_HZ, "WARNING: Required bandwidth may exceed hardware capabilities");
}

// ============================================================================
// **DATA TYPE CONSTRAINT VALIDATIONS**
// ============================================================================

/**
 * @brief Validates integer value ranges for hardware compatibility
 */
template <int32_t value, uint32_t integer_type>
constexpr void validate_integer_range()
{
    using namespace llk_hardware_limits::data_types;
    if constexpr (integer_type == 8)
    {
        static_assert(value >= INT8_MIN_VALUE && value <= INT8_MAX_VALUE, "CRITICAL: Integer '8' value must be in range -1023 to +1023");
    }
    else if constexpr (integer_type == 16)
    {
        static_assert(value >= -INT16_MAX_MAGNITUDE && value <= INT16_MAX_MAGNITUDE, "CRITICAL: Integer '16' value must be in range -32767 to +32767");
    }
}

/**
 * @brief Validates floating-point precision parameters
 */
template <uint32_t exponent_bits, uint32_t mantissa_bits, uint32_t fp_type>
constexpr void validate_fp_precision()
{
    using namespace llk_hardware_limits::data_types;
    // FP types: 0=FP32, 1=BF16, 2=FP16, 3=TF32
    if constexpr (fp_type == 0)
    { // FP32
        static_assert(
            exponent_bits == FP32_EXPONENT_BITS && mantissa_bits == FP32_MANTISSA_BITS, "CRITICAL: FP32 must have 8 exponent bits and 23 mantissa bits");
    }
    else if constexpr (fp_type == 1)
    { // BF16
        static_assert(
            exponent_bits == BF16_EXPONENT_BITS && mantissa_bits == BF16_MANTISSA_BITS, "CRITICAL: BF16 must have 8 exponent bits and 7 mantissa bits");
    }
    else if constexpr (fp_type == 2)
    { // FP16
        static_assert(
            exponent_bits == FP16_EXPONENT_BITS && mantissa_bits == FP16_MANTISSA_BITS, "CRITICAL: FP16 must have 5 exponent bits and 10 mantissa bits");
    }
    else if constexpr (fp_type == 3)
    { // TF32
        static_assert(
            exponent_bits == TF32_EXPONENT_BITS && mantissa_bits == TF32_MANTISSA_BITS, "CRITICAL: TF32 must have 8 exponent bits and 10 mantissa bits");
    }
}

// ============================================================================
// **INSTRUCTION TEMPLATE VALIDATIONS**
// ============================================================================

/**
 * @brief Validates template loop parameters
 */
template <uint32_t outer_loop_count, uint32_t inner_loop_count>
constexpr void validate_template_loops()
{
    using namespace llk_hardware_limits::instructions;
    static_assert(outer_loop_count > 0 && outer_loop_count <= MAX_OUTER_LOOP_COUNT, "CRITICAL: Outer loop count must be 1-65535 for template system");
    static_assert(inner_loop_count > 0 && inner_loop_count <= MAX_INNER_LOOP_COUNT, "CRITICAL: Inner loop count must be 1-65535 for template system");

    // Warn about performance implications
    constexpr uint64_t total_iterations = static_cast<uint64_t>(outer_loop_count) * inner_loop_count;
    static_assert(total_iterations <= 1000000, "WARNING: Very large loop iteration count may impact performance");
}

/**
 * @brief Validates configuration state management
 */
template <uint32_t cfg_state_id, uint32_t dest_offset_id>
constexpr void validate_config_state()
{
    using namespace llk_hardware_limits::instructions;
    static_assert(cfg_state_id < MAX_CFG_STATES, "CRITICAL: Configuration state ID must be 0-15 for hardware compatibility");
    static_assert(dest_offset_id < MAX_DEST_OFFSETS, "CRITICAL: Destination offset ID must be 0-15 for hardware compatibility");
}

// ============================================================================
// **TILE AND ALIGNMENT VALIDATIONS**
// ============================================================================

/**
 * @brief Validates tile alignment and dimensions comprehensively
 */
template <uint32_t height, uint32_t width, uint32_t alignment_requirement = 16>
constexpr void validate_tile_alignment()
{
    using namespace llk_hardware_limits::tiles;
    static_assert(height > 0 && width > 0, "CRITICAL: Tile dimensions must be positive");
    static_assert(height <= MAX_TILE_DIM && width <= MAX_TILE_DIM, "CRITICAL: Tile dimensions must not exceed 32x32 for Wormhole B0");
    static_assert(
        (height % alignment_requirement) == 0 && (width % alignment_requirement) == 0,
        "CRITICAL: Tile dimensions must be aligned to face boundaries (multiples of 16)");
    static_assert(height >= FACE_HEIGHT && width >= FACE_WIDTH, "CRITICAL: Tile dimensions must be at least one face (16x16)");
}

/**
 * @brief Validates PCBuf configuration and usage
 */
template <uint32_t pcbuf_id, uint32_t fifo_usage>
constexpr void validate_pcbuf_usage()
{
    using namespace llk_hardware_limits::memory;
    static_assert(pcbuf_id < PCBUF_COUNT, "CRITICAL: PCBuf ID must be 0-2 (3 PCBufs available per tile)");
    static_assert(fifo_usage <= PCBUF_FIFO_DEPTH, "CRITICAL: PCBuf FIFO usage must not exceed 16 entries");
}

// ============================================================================
// **THROUGHPUT AND SCALING VALIDATIONS**
// ============================================================================

/**
 * @brief Validates expected throughput against hardware capabilities
 */
template <uint32_t expected_ops_per_second, uint32_t operation_type>
constexpr void validate_throughput_expectations()
{
    using namespace llk_hardware_limits::throughput;
    using namespace llk_hardware_limits::performance;

    // Operation types: 0=Matrix, 1=SFPU, 2=Memory
    constexpr float max_throughputs[] = {
        MATRIX_UNIT_1_PHASE_TFLOPS * 1e12f,                        // Matrix operations
        SFPU_FP32_TFLOPS * 1e12f,                                  // SFPU operations
        static_cast<float>(NOC_READ_BPC * CLOCK_FREQUENCY_HZ / 32) // Memory operations (approx)
    };

    static_assert(operation_type < 3, "CRITICAL: Invalid operation type for throughput validation");
    static_assert(expected_ops_per_second <= max_throughputs[operation_type], "WARNING: Expected throughput may exceed single-tile hardware capabilities");
}

} // namespace llk_validation

// **CONVENIENCE MACROS FOR COMMON VALIDATIONS**
// ============================================================================

/**
 * @brief Validate face count - use in tile operations
 */
#define LLK_STATIC_ASSERT_FACE_COUNT(count) llk_validation::validate_face_count<count>()

/**
 * @brief Validate tile dimensions - use in tile-based operations
 */
#define LLK_STATIC_ASSERT_TILE_DIMS(r_dim, c_dim) llk_validation::validate_tile_dimensions<r_dim, c_dim>()

/**
 * @brief Validate address mode - use in address mode configuration
 */
#define LLK_STATIC_ASSERT_ADDR_MODE(index) llk_validation::validate_address_mode<index>()

/**
 * @brief Validate math fidelity - use in math operations
 */
#define LLK_STATIC_ASSERT_MATH_FIDELITY(fidelity) llk_validation::validate_math_fidelity<fidelity>()

/**
 * @brief Validate throttle level - use in performance-critical operations
 */
#define LLK_STATIC_ASSERT_THROTTLE_LEVEL(level) llk_validation::validate_throttle_level<level>()

/**
 * @brief Validate matrix multiplication dimensions
 */
#define LLK_STATIC_ASSERT_MATMUL_DIMS(ct, rt, kt) llk_validation::validate_matmul_dimensions<ct, rt, kt>()

/**
 * @brief Simplified SFPU operation validation (avoids dependency issues)
 */
#define LLK_STATIC_ASSERT_SFPU_OPERATION(sfpu_op) static_assert(static_cast<int>(sfpu_op) >= 0, "SFPU operation must be valid")

// ============================================================================
// **ENHANCED CONVENIENCE MACROS FOR HARDWARE LIMITS**
// ============================================================================

// **MEMORY ARCHITECTURE MACROS**
/**
 * @brief Validate L1 memory bank access
 */
#define LLK_STATIC_ASSERT_L1_ACCESS(bank_id, access_width) llk_validation::validate_l1_access<bank_id, access_width>()

/**
 * @brief Validate instruction cache capacity
 */
#define LLK_STATIC_ASSERT_ICACHE_CAPACITY(instruction_count, riscv_target) llk_validation::validate_icache_usage<instruction_count, riscv_target>()

/**
 * @brief Validate destination register access
 */
#define LLK_STATIC_ASSERT_DST_ACCESS(row, col, is_32bit) llk_validation::validate_dst_access<row, col, is_32bit>()

/**
 * @brief Validate source register access
 */
#define LLK_STATIC_ASSERT_SRC_ACCESS(bank, row, col) llk_validation::validate_src_access<bank, row, col>()

// **PERFORMANCE CONSTRAINT MACROS**
/**
 * @brief Validate fidelity phase count
 */
#define LLK_STATIC_ASSERT_FIDELITY_PHASES(phase_count) llk_validation::validate_fidelity_phases<phase_count>()

/**
 * @brief Validate instruction latency expectations
 */
#define LLK_STATIC_ASSERT_LATENCY(expected_cycles) llk_validation::validate_instruction_latency<expected_cycles>()

/**
 * @brief Validate memory bandwidth requirements
 */
#define LLK_STATIC_ASSERT_BANDWIDTH(required_bps, client_type) llk_validation::validate_memory_bandwidth<required_bps, client_type>()

// **DATA TYPE CONSTRAINT MACROS**
/**
 * @brief Validate integer value range
 */
#define LLK_STATIC_ASSERT_INT_RANGE(value, int_type) llk_validation::validate_integer_range<value, int_type>()

/**
 * @brief Validate floating-point precision
 */
#define LLK_STATIC_ASSERT_FP_PRECISION(exp_bits, mant_bits, fp_type) llk_validation::validate_fp_precision<exp_bits, mant_bits, fp_type>()

// **INSTRUCTION TEMPLATE MACROS**
/**
 * @brief Validate template loop parameters
 */
#define LLK_STATIC_ASSERT_TEMPLATE_LOOPS(outer_count, inner_count) llk_validation::validate_template_loops<outer_count, inner_count>()

/**
 * @brief Validate configuration state
 */
#define LLK_STATIC_ASSERT_CONFIG_STATE(cfg_id, dest_offset_id) llk_validation::validate_config_state<cfg_id, dest_offset_id>()

// **TILE AND ALIGNMENT MACROS**
/**
 * @brief Validate tile alignment comprehensively
 */
#define LLK_STATIC_ASSERT_TILE_ALIGNMENT(height, width) llk_validation::validate_tile_alignment<height, width>()

/**
 * @brief Validate tile alignment with custom alignment requirement
 */
#define LLK_STATIC_ASSERT_TILE_ALIGNMENT_CUSTOM(height, width, alignment) llk_validation::validate_tile_alignment<height, width, alignment>()

/**
 * @brief Validate PCBuf usage
 */
#define LLK_STATIC_ASSERT_PCBUF_USAGE(pcbuf_id, fifo_entries) llk_validation::validate_pcbuf_usage<pcbuf_id, fifo_entries>()

// **THROUGHPUT AND PERFORMANCE MACROS**
/**
 * @brief Validate throughput expectations
 */
#define LLK_STATIC_ASSERT_THROUGHPUT(ops_per_sec, op_type) llk_validation::validate_throughput_expectations<ops_per_sec, op_type>()

// ============================================================================
// **COMPOSITE VALIDATION MACROS FOR COMMON OPERATIONS**
// ============================================================================

/**
 * @brief Complete validation for matrix multiplication setup
 */
#define LLK_STATIC_ASSERT_MATMUL_COMPLETE(ct, rt, kt, fidelity_phases) \
    do                                                                 \
    {                                                                  \
        LLK_STATIC_ASSERT_MATMUL_DIMS(ct, rt, kt);                     \
        LLK_STATIC_ASSERT_FIDELITY_PHASES(fidelity_phases);            \
    } while (0)

/**
 * @brief Complete validation for tile operation setup
 */
#define LLK_STATIC_ASSERT_TILE_OPERATION(height, width, face_count) \
    do                                                              \
    {                                                               \
        LLK_STATIC_ASSERT_TILE_ALIGNMENT(height, width);            \
        LLK_STATIC_ASSERT_FACE_COUNT(face_count);                   \
    } while (0)

/**
 * @brief Complete validation for register file access
 */
#define LLK_STATIC_ASSERT_REGISTER_ACCESS(dst_row, dst_col, src_bank, src_row, src_col, is_32bit) \
    do                                                                                            \
    {                                                                                             \
        LLK_STATIC_ASSERT_DST_ACCESS(dst_row, dst_col, is_32bit);                                 \
        LLK_STATIC_ASSERT_SRC_ACCESS(src_bank, src_row, src_col);                                 \
    } while (0)

/**
 * @brief Complete validation for template configuration
 */
#define LLK_STATIC_ASSERT_TEMPLATE_CONFIG(outer_loop, inner_loop, cfg_state, dest_offset, addr_mode) \
    do                                                                                               \
    {                                                                                                \
        LLK_STATIC_ASSERT_TEMPLATE_LOOPS(outer_loop, inner_loop);                                    \
        LLK_STATIC_ASSERT_CONFIG_STATE(cfg_state, dest_offset);                                      \
        LLK_STATIC_ASSERT_ADDR_MODE(addr_mode);                                                      \
    } while (0)

// ============================================================================
// **COMPILE-TIME FEATURE DETECTION MACROS**
// ============================================================================

/**
 * @brief Check if hardware supports requested configuration
 */
#define LLK_HARDWARE_SUPPORTS_CONFIG(feature_check) static_assert(feature_check, "CRITICAL: Hardware configuration not supported on Wormhole B0")

/**
 * @brief Validate that operation is supported in current mode
 */
#define LLK_STATIC_ASSERT_OPERATION_SUPPORTED(condition, message) static_assert(condition, "CRITICAL: Operation not supported - " message)

// ============================================================================
// **DEBUG AND DEVELOPMENT MACROS**
// ============================================================================

/**
 * @brief Enable comprehensive validation for debug builds
 */
#ifdef DEBUG
#define LLK_STATIC_ASSERT_DEBUG_COMPREHENSIVE(face_count, tile_h, tile_w, phases, addr_mode) \
    do                                                                                       \
    {                                                                                        \
        LLK_STATIC_ASSERT_FACE_COUNT(face_count);                                            \
        LLK_STATIC_ASSERT_TILE_ALIGNMENT(tile_h, tile_w);                                    \
        LLK_STATIC_ASSERT_FIDELITY_PHASES(phases);                                           \
        LLK_STATIC_ASSERT_ADDR_MODE(addr_mode);                                              \
    } while (0)
#else
#define LLK_STATIC_ASSERT_DEBUG_COMPREHENSIVE(face_count, tile_h, tile_w, phases, addr_mode) \
    do                                                                                       \
    { /* No-op in release builds */                                                          \
    } while (0)
#endif

/**
 * @brief Validate performance-critical path constraints
 */
#define LLK_STATIC_ASSERT_PERF_CRITICAL(latency_cycles, bandwidth_bps, client_type) \
    do                                                                              \
    {                                                                               \
        LLK_STATIC_ASSERT_LATENCY(latency_cycles);                                  \
        LLK_STATIC_ASSERT_BANDWIDTH(bandwidth_bps, client_type);                    \
    } while (0)
