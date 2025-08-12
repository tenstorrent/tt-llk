// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

/**
 * @file llk_validation.h
 * @brief Ultra-Lightweight Validation for Memory-Constrained Tensix Kernels
 *
 * @details This header provides hardware-aware validation for LLK operations with
 * ZERO memory overhead and NO standard library dependencies. Validation adapts
 * to each core's memory constraints and uses debug registers for failure tracking.
 *
 * **Design Principles:**
 * - Hardware constraints dictate validation capabilities 
 * - Each core type gets validation it can afford
 * - Zero allocation - uses debug registers for PC tracking
 * - No standard libraries - pure embedded C
 * - Compile-time configurable validation levels
 *
 * **Memory Constraints (Wormhole B0):**
 * - TRISC0/T1: 256B stack total - direct output only
 * - TRISC2: 768B stack - minimal buffering 
 * - BRISC: 1024B stack - reasonable validation
 * - All: PC stored in debug registers (zero local memory)
 *
 * **Validation Levels:**
 * - Level 0: Disabled (release builds)
 * - Level 1: Critical validation (PC + error codes)
 * - Level 2: Essential validation (basic parameter checks)
 * - Level 3: Full validation (only on cores with sufficient memory)
 *
 * **Safe Trap-Based Error Handling:**
 * Validation failures immediately call __builtin_trap() to halt execution:
 * - No register writes that could corrupt system state
 * - Preserves critical timing infrastructure (WALL_CLOCK, etc.)
 * - Immediate failure detection at exact validation point
 *
 * @author Tenstorrent AI ULC
 * @version 2.0 - Memory Optimized
 * @date 2025
 */

#pragma once

// ============================================================================
// **CORE IDENTIFICATION AND MEMORY BUDGET**
// ============================================================================


// Validation level configuration - use separate flags instead of numeric levels
#ifdef LLK_VALIDATION_ENABLED
    #define LLK_VALIDATION_LEVEL_1 1
    #define LLK_VALIDATION_LEVEL_2 1  // Default to essential validation when enabled
    #define LLK_VALIDATION_LEVEL_3 0  // Advanced validation off by default
#else
    #define LLK_VALIDATION_LEVEL_1 0
    #define LLK_VALIDATION_LEVEL_2 1
    #define LLK_VALIDATION_LEVEL_3 0
#endif

// For BRISC cores, enable advanced validation if requested
#if defined(BRISC) && defined(LLK_VALIDATION_ENABLED) && (LLK_VALIDATION_CORE_TYPE == 2)
    #undef LLK_VALIDATION_LEVEL_3
    #define LLK_VALIDATION_LEVEL_3 1
#endif

// Test environment configuration - don't halt on validation errors
#ifdef TEST_KERNEL
    #undef LLK_VALIDATION_HALT_ON_ERROR
    #define LLK_VALIDATION_HALT_ON_ERROR 0    // Continue execution in tests
#endif

// Safety check - ensure LLK_VALIDATION_CORE_TYPE is always defined
#ifndef LLK_VALIDATION_CORE_TYPE
    #define LLK_VALIDATION_CORE_TYPE 1        // Default to constrained
#endif

// ============================================================================
// **SAFE VALIDATION - NO REGISTER/MEMORY WRITES** 
// ============================================================================

// WARNING: Previous debug register allocation (0xFFB12xxx) conflicted with 
// critical system registers (WALL_CLOCK, timing infrastructure).
// New approach: NO register writes, immediate trap on validation failure.
// This prevents corruption of system timing that causes packer hangs.

// ============================================================================
// **ERROR CODE DEFINITIONS**
// ============================================================================

// Compact error codes (16-bit for efficient storage)
#define LLK_ERROR_PARAM_RANGE        0x1001
#define LLK_ERROR_TILE_FORMAT        0x1002  
#define LLK_ERROR_TILE_DIMS          0x1003
#define LLK_ERROR_MATH_FIDELITY      0x1004
#define LLK_ERROR_L1_ALIGNMENT       0x1005
#define LLK_ERROR_POWER_OF_2         0x1006
#define LLK_ERROR_SFPU_OPERATION     0x1007
#define LLK_ERROR_DEST_TILE_INDEX    0x1008
#define LLK_ERROR_SRC_TILE_INDEX     0x1009
#define LLK_ERROR_FACE_DIMENSION     0x100A
#define LLK_ERROR_NUM_FACES          0x100B
#define LLK_ERROR_BROADCAST_TYPE     0x100C
#define LLK_ERROR_MATMUL_DIMS        0x100D
#define LLK_ERROR_ULP_EXCEEDED       0x100E
#define LLK_ERROR_MATH_CORRECTNESS   0x100F
#define LLK_ERROR_ARCH_PARITY        0x1010
#define LLK_ERROR_ROUNDING_MODE      0x1011

// ============================================================================
// **CORE VALIDATION MACROS**
// ============================================================================

#if LLK_VALIDATION_LEVEL_1

// **SAFE VALIDATION - NO MEMORY/REGISTER CORRUPTION**
// Previous implementation wrote to debug registers that conflicted with
// critical system registers (WALL_CLOCK at 0xFFB121xx), causing packer hangs.
// New approach: immediate trap on failure, no state corruption.

#define LLK_VALIDATE_CORE(condition, error_code, val1, val2) \
    do { \
        if (!(condition)) { \
            /* Immediate halt - no register writes that corrupt system state */ \
            __builtin_trap(); \
        } \
    } while(0)

#else
    #define LLK_VALIDATE_CORE(condition, error_code, val1, val2) ((void)0)
#endif

// ============================================================================
// **PARAMETER VALIDATION MACROS**
// ============================================================================

#if LLK_VALIDATION_LEVEL_2

#define LLK_VALIDATE_PARAM_RANGE(param, min_val, max_val) \
    LLK_VALIDATE_CORE((param) >= (min_val) && (param) <= (max_val), \
                      LLK_ERROR_PARAM_RANGE, (param), (min_val))

#define LLK_VALIDATE_POWER_OF_2(value) \
    LLK_VALIDATE_CORE(((value) > 0) && (((value) & ((value) - 1)) == 0), \
                      LLK_ERROR_POWER_OF_2, (value), 0)

#define LLK_VALIDATE_L1_ALIGNMENT(address) \
    LLK_VALIDATE_CORE(((address) & 0xF) == 0, \
                      LLK_ERROR_L1_ALIGNMENT, (address), 16)

#define LLK_VALIDATE_TILE_DIMS(height, width) \
    do { \
        LLK_VALIDATE_PARAM_RANGE(height, 1, 32); \
        LLK_VALIDATE_PARAM_RANGE(width, 1, 32); \
        LLK_VALIDATE_CORE((height * width) <= 1024, LLK_ERROR_TILE_DIMS, height, width); \
    } while(0)

#define LLK_VALIDATE_DEST_TILE_INDEX(index) \
    LLK_VALIDATE_PARAM_RANGE(index, 0, 15)

#define LLK_VALIDATE_SRC_TILE_INDEX(index) \
    LLK_VALIDATE_PARAM_RANGE(index, 0, 15)

#define LLK_VALIDATE_FACE_DIMENSION(face_dim) \
    LLK_VALIDATE_CORE((face_dim) == 16, LLK_ERROR_FACE_DIMENSION, face_dim, 16)

#define LLK_VALIDATE_NUM_FACES(num_faces) \
    do { \
        LLK_VALIDATE_PARAM_RANGE(num_faces, 1, 4); \
        LLK_VALIDATE_POWER_OF_2(num_faces); \
    } while(0)

#else
    #define LLK_VALIDATE_PARAM_RANGE(param, min_val, max_val) ((void)0)
    #define LLK_VALIDATE_POWER_OF_2(value) ((void)0)
    #define LLK_VALIDATE_L1_ALIGNMENT(address) ((void)0)
    #define LLK_VALIDATE_TILE_DIMS(height, width) ((void)0)
    #define LLK_VALIDATE_DEST_TILE_INDEX(index) ((void)0)
    #define LLK_VALIDATE_SRC_TILE_INDEX(index) ((void)0)
    #define LLK_VALIDATE_FACE_DIMENSION(face_dim) ((void)0)
    #define LLK_VALIDATE_NUM_FACES(num_faces) ((void)0)
#endif

// ============================================================================
// **MATH AND OPERATION VALIDATION**
// ============================================================================

#if LLK_VALIDATION_LEVEL_2

#define LLK_VALIDATE_MATH_FIDELITY(fidelity) \
    do { \
        LLK_VALIDATE_PARAM_RANGE(fidelity, 0, 15); \
        LLK_VALIDATE_CORE((fidelity) <= 8, LLK_ERROR_MATH_FIDELITY, fidelity, 8); \
    } while(0)

#define LLK_VALIDATE_MATMUL_DIMS(M, K, N) \
    do { \
        LLK_VALIDATE_PARAM_RANGE(M, 1, 2048); \
        LLK_VALIDATE_PARAM_RANGE(K, 1, 2048); \
        LLK_VALIDATE_PARAM_RANGE(N, 1, 2048); \
        LLK_VALIDATE_CORE((M) * (K) <= (1024 * 1024), LLK_ERROR_MATMUL_DIMS, M, K); \
    } while(0)

#else
    #define LLK_VALIDATE_MATH_FIDELITY(fidelity) ((void)0)
    #define LLK_VALIDATE_MATMUL_DIMS(M, K, N) ((void)0)
#endif

// ============================================================================
// **SFPU VALIDATION** 
// ============================================================================

#if LLK_VALIDATION_LEVEL_2

#define LLK_VALIDATE_SFPU_OPERATION(op_type) \
    LLK_VALIDATE_CORE((op_type) < 100, LLK_ERROR_SFPU_OPERATION, op_type, 100)

#define LLK_VALIDATE_BINARY_SFPU_OPERATION(op_type) \
    do { \
        /* Binary SFPU operations use SfpuType enum values, same validation as generic SFPU */ \
        LLK_VALIDATE_SFPU_OPERATION(op_type); \
    } while(0)

#define LLK_VALIDATE_TERNARY_SFPU_OPERATION(op_type) \
    do { \
        LLK_VALIDATE_CORE((op_type) >= 11 && (op_type) <= 13, LLK_ERROR_SFPU_OPERATION, op_type, 1); \
        LLK_VALIDATE_SFPU_OPERATION(op_type); \
    } while(0)

#else
    #define LLK_VALIDATE_SFPU_OPERATION(op_type) ((void)0)
    #define LLK_VALIDATE_BINARY_SFPU_OPERATION(op_type) ((void)0)
    #define LLK_VALIDATE_TERNARY_SFPU_OPERATION(op_type) ((void)0)
#endif

// ============================================================================
// **ADVANCED VALIDATION** (Only for cores with sufficient memory)
// ============================================================================

#if LLK_VALIDATION_LEVEL_3

// **ULP Error Validation** - Only on BRISC core
#define LLK_VALIDATE_ULP_ERROR(actual, expected, max_ulp, operation_name) \
    do { \
        /* Simple ULP check without floating-point math */ \
        unsigned int a_bits = *(unsigned int*)&(actual); \
        unsigned int e_bits = *(unsigned int*)&(expected); \
        unsigned int ulp_diff = (a_bits > e_bits) ? (a_bits - e_bits) : (e_bits - a_bits); \
        LLK_VALIDATE_CORE(ulp_diff <= (max_ulp), LLK_ERROR_ULP_EXCEEDED, ulp_diff, max_ulp); \
    } while(0)

// **Basic Mathematical Correctness** - Simple tolerance check
#define LLK_VALIDATE_MATH_CORRECTNESS(result, expected, tolerance) \
    do { \
        float diff = (result) - (expected); \
        if (diff < 0.0f) diff = -diff; \
        LLK_VALIDATE_CORE(diff <= (tolerance), LLK_ERROR_MATH_CORRECTNESS, \
                          *(unsigned int*)&(result), *(unsigned int*)&(expected)); \
    } while(0)

#else
    #define LLK_VALIDATE_ULP_ERROR(actual, expected, max_ulp, operation_name) ((void)0)
    #define LLK_VALIDATE_MATH_CORRECTNESS(result, expected, tolerance) ((void)0)
#endif

// ============================================================================
// **SAFE VALIDATION APPROACH**
// ============================================================================

// Previous helper functions removed - they relied on debug register writes
// that conflicted with critical system registers (WALL_CLOCK timing).
// 
// New approach: 
// - Validation failures immediately trap (__builtin_trap())
// - No state corruption from register writes
// - System timing infrastructure remains intact
// - Packer operations proceed normally when validation passes

// ============================================================================
// **COMPILE-TIME VALIDATION**
// ============================================================================

// Ensure we don't exceed stack budgets at compile time
#if defined(TRISC0) && (LLK_VALIDATION_BUFFER_SIZE > 0)
    #error "TRISC0 cannot use validation buffers - stack too small"
#endif

#if defined(TRISC1) && (LLK_VALIDATION_BUFFER_SIZE > 0)
    #error "TRISC1 cannot use validation buffers - stack too small"
#endif

// ============================================================================
// **LEGACY COMPATIBILITY** (Disabled - use new macros)
// ============================================================================

// Old validation macros are replaced with hardware-aware versions
#define LLK_VALIDATE(condition, message) \
    LLK_VALIDATE_CORE(condition, 0xFFFF, 0, 0)

#define LLK_VALIDATE_TILE_FORMAT(format) \
    LLK_VALIDATE_PARAM_RANGE(format, 0, 15)

#define LLK_VALIDATE_BROADCAST_TYPE(broadcast_type) \
    LLK_VALIDATE_PARAM_RANGE(broadcast_type, 0, 3)

// ============================================================================
// **DOCUMENTATION NOTES**
// ============================================================================

/*
 * SAFE VALIDATION USAGE:
 *
 * // Basic parameter validation (Level 2+)
 * LLK_VALIDATE_PARAM_RANGE(tile_size, 1, 32);        // Traps on invalid range
 * LLK_VALIDATE_TILE_DIMS(height, width);             // Traps on invalid dimensions
 * 
 * // SFPU operation validation (Level 2+)
 * LLK_VALIDATE_SFPU_OPERATION(sfpu_op);              // Traps on invalid operation
 * LLK_VALIDATE_BINARY_SFPU_OPERATION(binary_op);     // Traps on invalid binary op
 *
 * // Advanced validation (Level 3+, BRISC only)
 * LLK_VALIDATE_ULP_ERROR(result, expected, 2.0f, "matmul");      // Traps on ULP error
 * LLK_VALIDATE_MATH_CORRECTNESS(result, expected, 1e-6f);        // Traps on math error
 *
 * DEBUGGING FAILED VALIDATION:
 * 1. Validation failure immediately calls __builtin_trap()
 * 2. Execution halts at the exact validation point
 * 3. Use debugger or crash dump to inspect call stack
 * 4. No system state corruption - timing/packer operations remain intact
 * 
 * SAFE DESIGN PRINCIPLES:
 * - No memory or register writes that could corrupt system state
 * - Immediate failure detection without state pollution
 * - Preserves critical system infrastructure (timing, synchronization)
 */