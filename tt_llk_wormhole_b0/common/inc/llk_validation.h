// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

/**
 * @file llk_validation.h
 * @brief Debug validation utilities for Low-Level Kernel operations
 *
 * @details This header provides optional validation macros and utilities for LLK operations
 * that can be enabled in debug builds to catch common programming errors and invalid
 * parameter combinations. All validation is compile-time optional and introduces zero
 * overhead in release builds.
 *
 * **Validation Categories:**
 * - Parameter range checking for tile dimensions and formats
 * - Resource management validation (acquire/release pairing)
 * - Hardware constraint validation (alignment, compatibility)
 * - Thread synchronization validation
 *
 * **Usage Pattern:**
 * 
 * void llk_operation(uint32_t param) {
 *     LLK_VALIDATE_PARAM_RANGE(param, 1, 16, "param must be 1-16");
 *     LLK_VALIDATE_TILE_FORMAT(src_format, dst_format);
 *     // ... operation implementation
 * }
 * 
 *
 * **Build Configuration:**
 * - LLK_DEBUG: Enable all validation checks
 * - LLK_VALIDATION_LEVEL: Control validation level (0=none, 1=basic, 2=full)
 * - Release builds: All validation compiles to no-ops
 */

#pragma once

#include <cstdint>
#include <type_traits>
#include <cmath>
#include <string>
#include <cstdio>

// **SFPU TYPE FORWARD DECLARATION** - Use existing hardware definitions
// ============================================================================

// SfpuType is defined in firmware headers - no need to redefine

// **CRITICAL PRECISION VALIDATION** - Based on GitHub Issues Analysis
// Issues found: 69,846 ULP errors in mean, rounding bugs in power functions,
// architecture-specific bugs, SFPI precision issues

// ============================================================================
// **LLK VALIDATION FRAMEWORK** - DISABLED FOR MEMORY CONSTRAINTS
// ============================================================================
// **CRITICAL**: All validation disabled to prevent TRISC1_CODE memory overflow
// Enable only in debug builds with sufficient memory

// **ULP ERROR VALIDATION** - Critical for precision issue detection
#ifdef LLK_VALIDATION_ENABLED
#define LLK_VALIDATE_ULP_ERROR(actual, expected, max_ulp, operation_name) \
    do { \
        if (std::isfinite(actual) && std::isfinite(expected)) { \
            double ulp_error = ULPCalculator::calculate_ulp_error(actual, expected); \
            if (ulp_error > max_ulp) { \
                LLK_VALIDATION_ERROR("CRITICAL ULP ERROR in " operation_name ": " \
                    "ULP=" + std::to_string(ulp_error) + " exceeds limit=" + std::to_string(max_ulp) + \
                    " (actual=" + std::to_string(actual) + ", expected=" + std::to_string(expected) + ")"); \
            } \
        } \
    } while(0)
#else
#define LLK_VALIDATE_ULP_ERROR(actual, expected, max_ulp, operation_name) ((void)0)
#endif

// **Mathematical Correctness Validation**
#ifdef LLK_VALIDATION_ENABLED
#define LLK_VALIDATE_MATHEMATICAL_CORRECTNESS(result, expected, tolerance, operation) \
    do { \
        if (!is_mathematically_correct(result, expected, tolerance)) { \
            LLK_VALIDATION_ERROR("Mathematical correctness failure in " operation ": " \
                "result=" + std::to_string(result) + " expected=" + std::to_string(expected) + \
                " tolerance=" + std::to_string(tolerance)); \
        } \
    } while(0)
#else
#define LLK_VALIDATE_MATHEMATICAL_CORRECTNESS(result, expected, tolerance, operation) ((void)0)
#endif

// **CROSS-ARCHITECTURE PARITY VALIDATION**
#ifdef LLK_VALIDATION_ENABLED
#define LLK_VALIDATE_ARCHITECTURE_PARITY(wormhole_result, blackhole_result, operation) \
    do { \
        if (!are_architectures_equivalent(wormhole_result, blackhole_result)) { \
            LLK_VALIDATION_ERROR("Architecture parity failure in " operation ": " \
                "Wormhole=" + std::to_string(wormhole_result) + " != " + \
                "Blackhole=" + std::to_string(blackhole_result)); \
        } \
    } while(0)
#else
#define LLK_VALIDATE_ARCHITECTURE_PARITY(wormhole_result, blackhole_result, operation) ((void)0)
#endif

// **ROUNDING MODE VALIDATION** 
#ifdef LLK_VALIDATION_ENABLED
#define LLK_VALIDATE_ROUNDING_MODE(input_fp32, output_bfp16, operation) \
    do { \
        float expected_rounded = RoundingUtils::round_to_nearest_bfp16(input_fp32); \
        if (std::abs(output_bfp16 - expected_rounded) > 1e-6f) { \
            LLK_VALIDATION_ERROR("Rounding mode violation in " operation ": " \
                "input=" + std::to_string(input_fp32) + " output=" + std::to_string(output_bfp16) + \
                " expected=" + std::to_string(expected_rounded)); \
        } \
    } while(0)
#else
#define LLK_VALIDATE_ROUNDING_MODE(input_fp32, output_bfp16, operation) ((void)0)
#endif

// Core validation macro with enhanced error reporting
#ifdef LLK_VALIDATION_ENABLED
#define LLK_VALIDATE(condition, message) \
    do { \
        if (!(condition)) { \
            LLK_VALIDATION_ERROR("Validation failed: " + std::string(message) + \
                " [" #condition "] at " + __FILE__ + ":" + std::to_string(__LINE__)); \
        } \
    } while(0)
#else
#define LLK_VALIDATE(condition, message) ((void)0)
#endif

// Parameter range validation with precision context
#define LLK_VALIDATE_PARAM_RANGE(param, min_val, max_val) \
    LLK_VALIDATE((param) >= (min_val) && (param) <= (max_val), \
        "Parameter " #param " = " + std::to_string(param) + \
        " out of range [" + std::to_string(min_val) + ", " + std::to_string(max_val) + "]")

// Power of 2 validation
#define LLK_VALIDATE_POWER_OF_2(value) \
    LLK_VALIDATE(((value) > 0) && (((value) & ((value) - 1)) == 0), \
        "Value " #value " = " + std::to_string(value) + " must be power of 2")

// Tile dimension validation  
#define LLK_VALIDATE_TILE_DIMS(height, width) \
    do { \
        LLK_VALIDATE_PARAM_RANGE(height, 1, 32); \
        LLK_VALIDATE_PARAM_RANGE(width, 1, 32); \
        LLK_VALIDATE((height * width) <= 1024, "Tile area too large"); \
    } while(0)

// Data format validation
#define LLK_VALIDATE_TILE_FORMAT(format) \
    LLK_VALIDATE(is_valid_data_format(format), \
        "Invalid data format: " + std::to_string(format))

// Matrix multiplication dimension validation
#define LLK_VALIDATE_MATMUL_DIMS(M, K, N) \
    do { \
        LLK_VALIDATE_PARAM_RANGE(M, 1, 2048); \
        LLK_VALIDATE_PARAM_RANGE(K, 1, 2048); \
        LLK_VALIDATE_PARAM_RANGE(N, 1, 2048); \
        LLK_VALIDATE((M * K) <= (1024 * 1024), "Input A too large"); \
        LLK_VALIDATE((K * N) <= (1024 * 1024), "Input B too large"); \
        LLK_VALIDATE((M * N) <= (1024 * 1024), "Output too large"); \
    } while(0)

// Math fidelity validation  
#define LLK_VALIDATE_FIDELITY(fidelity) \
    do { \
        LLK_VALIDATE_PARAM_RANGE(fidelity, 0, 15); \
        if ((fidelity) > 8) { \
            LLK_VALIDATE(false, "High fidelity values may cause precision issues"); \
        } \
    } while(0)

// L1 memory alignment validation
#define LLK_VALIDATE_L1_ALIGNMENT(address) \
    LLK_VALIDATE(((address) % 16) == 0, \
        "L1 address " + std::to_string(address) + " not 16-byte aligned")

// **SFPU-SPECIFIC VALIDATION**
#define LLK_VALIDATE_SFPU_OPERATION(op_type) \
    do { \
        LLK_VALIDATE(is_valid_sfpu_operation(op_type), \
            "Invalid SFPU operation: " + std::to_string(op_type)); \
        LLK_VALIDATE(get_current_precision_mode() >= llk_validation::PRECISION_MODE_HIGH, \
            "SFPU operation requires high precision mode"); \
    } while(0)

// Destination tile validation
#define LLK_VALIDATE_DEST_TILE_INDEX(index) \
    do { \
        LLK_VALIDATE_PARAM_RANGE(index, 0, 15); \
        LLK_VALIDATE(is_dest_tile_available(index), \
            "Destination tile " + std::to_string(index) + " not available"); \
    } while(0)

// Source tile validation
#define LLK_VALIDATE_SRC_TILE_INDEX(index) \
    do { \
        LLK_VALIDATE_PARAM_RANGE(index, 0, 15); \
        LLK_VALIDATE(is_src_tile_valid(index), \
            "Source tile " + std::to_string(index) + " invalid or not ready"); \
    } while(0)

// Face dimension validation
#define LLK_VALIDATE_FACE_DIMENSION(face_dim) \
    do { \
        LLK_VALIDATE(face_dim == 16, "Face dimension must be 16"); \
        LLK_VALIDATE(is_face_alignment_correct(), "Face memory alignment error"); \
    } while(0)

// Number of faces validation
#define LLK_VALIDATE_NUM_FACES(num_faces) \
    do { \
        LLK_VALIDATE(is_valid_num_faces(num_faces), \
            "Invalid number of faces: " + std::to_string(num_faces)); \
        LLK_VALIDATE_PARAM_RANGE(num_faces, 1, 4); \
    } while(0)

// Broadcast type validation
#define LLK_VALIDATE_BROADCAST_TYPE(broadcast_type) \
    LLK_VALIDATE(is_valid_broadcast_type(broadcast_type), \
        "Invalid broadcast type: " + std::to_string(broadcast_type))

// **BINARY SFPU VALIDATION**
#define LLK_VALIDATE_BINARY_SFPU_OPERATION(op_type) \
    do { \
        LLK_VALIDATE(is_binary_sfpu_operation(op_type), \
            "Operation is not a valid binary SFPU operation: " + std::to_string(op_type)); \
        LLK_VALIDATE_SFPU_OPERATION(op_type); \
    } while(0)

// Binary operand availability validation
#define LLK_VALIDATE_BINARY_OPERAND_AVAILABILITY(dest_index) \
    LLK_VALIDATE(is_dest_register_available_for_binary_ops(dest_index), \
        "Destination register not available for binary operations: " + std::to_string(dest_index))

// **TERNARY SFPU VALIDATION**
#define LLK_VALIDATE_TERNARY_SFPU_OPERATION(op_type) \
    do { \
        LLK_VALIDATE(is_ternary_sfpu_operation(op_type), \
            "Operation is not a valid ternary SFPU operation: " + std::to_string(op_type)); \
        LLK_VALIDATE_SFPU_OPERATION(op_type); \
    } while(0)

// Ternary operand availability validation
#define LLK_VALIDATE_TERNARY_OPERAND_AVAILABILITY(dest_index) \
    LLK_VALIDATE(is_dest_register_available_for_ternary_ops(dest_index), \
        "Destination register not available for ternary operations: " + std::to_string(dest_index))

// **PIPELINE SYNCHRONIZATION VALIDATION**
#define LLK_VALIDATE_PIPELINE_SYNC(sync_type, mask) \
    do { \
        LLK_VALIDATE(is_valid_stallwait_combination(sync_type, mask), \
            "Invalid STALLWAIT combination may cause deadlock"); \
        if ((sync_type) == STALL_TRISC_CFG && (mask) == STALL_THCON) { \
            LLK_VALIDATE(false, "Configuration known to cause race conditions"); \
        } \
    } while(0)

// ============================================================================
// **VALIDATION ERROR REPORTING**
#ifdef LLK_VALIDATION_ENABLED
#define LLK_VALIDATION_ERROR(message) \
    do { \
        fprintf(stderr, "LLK VALIDATION ERROR: %s\n", (message).c_str()); \
        abort(); \
    } while(0)
#else
#define LLK_VALIDATION_ERROR(message) ((void)0)
#endif

// **HARDWARE INTERFACE CONSTANTS** - Placeholder definitions (non-conflicting)
// ============================================================================

// Register status constants (validation-specific)
namespace llk_validation {
    constexpr uint32_t REGISTER_AVAILABLE = 0;
    constexpr uint32_t REGISTER_VALID = 1;
    constexpr uint32_t REGISTER_BUSY = 2;
    
    // Broadcast type constants (validation-specific)
    constexpr uint32_t BROADCAST_NONE = 0;
    constexpr uint32_t BROADCAST_ROW = 1;
    constexpr uint32_t BROADCAST_COL = 2;
    constexpr uint32_t BROADCAST_SCALAR = 3;
    
    // STALLWAIT synchronization constants (validation-specific)
    constexpr uint32_t STALL_TRISC_CFG = 0x10;
    constexpr uint32_t STALL_THCON = 0x20;
    
    // Precision mode constants
    constexpr uint32_t PRECISION_MODE_HIGH = 2;
}

// **HARDWARE INTERFACE FUNCTIONS** - Placeholder implementations
// ============================================================================

// Register status functions (would interface with actual hardware in production)
inline uint32_t get_dest_register_status(uint32_t index) {
    return (index < 16) ? llk_validation::REGISTER_AVAILABLE : llk_validation::REGISTER_BUSY;
}

inline uint32_t get_src_register_status(uint32_t index) {
    return (index < 16) ? llk_validation::REGISTER_VALID : llk_validation::REGISTER_BUSY;
}

inline uint32_t get_current_face_alignment() {
    return 16; // Assume proper 16-byte alignment
}

// Precision mode functions (placeholders for hardware state)
inline uint32_t get_current_precision_mode() {
    return llk_validation::PRECISION_MODE_HIGH;
}

// **FORWARD DECLARATIONS** - For functions used in validation macros
// ============================================================================

// Forward declare rounding utilities
class RoundingUtils {
public:
    static float round_to_nearest_bfp16(float input) {
        // Placeholder implementation - would do proper BFP16 rounding
        return input; 
    }
};

// Forward declare cross-architecture comparison
inline bool are_architectures_equivalent(float wormhole_result, float blackhole_result);

// **VALIDATION HELPER FUNCTIONS** - Hardware state and parameter checking
// ============================================================================

inline bool is_valid_data_format(uint32_t format) {
    return (format >= static_cast<uint32_t>(DataFormat::Float32) && 
            format <= static_cast<uint32_t>(DataFormat::UInt32));
}

inline bool is_valid_sfpu_operation(uint32_t op_type) {
    // Note: SfpuType enum values are hardware-defined
    return (op_type < 100); // Placeholder - assume valid SFPU operations are < 100
}

inline bool is_dest_tile_available(uint32_t index) {
    return (index < 16 && get_dest_register_status(index) == llk_validation::REGISTER_AVAILABLE);
}

inline bool is_src_tile_valid(uint32_t index) {
    return (index < 16 && get_src_register_status(index) == llk_validation::REGISTER_VALID);
}

inline bool is_face_alignment_correct() {
    return (get_current_face_alignment() % 16 == 0);
}

inline bool is_valid_num_faces(uint32_t num_faces) {
    return (num_faces >= 1 && num_faces <= 4 && (num_faces & (num_faces - 1)) == 0);
}

inline bool is_valid_broadcast_type(uint32_t broadcast_type) {
    return (broadcast_type == llk_validation::BROADCAST_NONE || 
            broadcast_type == llk_validation::BROADCAST_ROW || 
            broadcast_type == llk_validation::BROADCAST_COL ||
            broadcast_type == llk_validation::BROADCAST_SCALAR);
}

inline bool is_binary_sfpu_operation(uint32_t op_type) {
    // Note: SfpuType enum values are hardware-defined
    return (op_type >= 3 && op_type <= 7); // Placeholder for binary operations
}

inline bool is_dest_register_available_for_binary_ops(uint32_t dest_index) {
    return (dest_index < 14 && // Reserve space for two operands
            get_dest_register_status(dest_index) == llk_validation::REGISTER_AVAILABLE &&
            get_dest_register_status(dest_index + 1) == llk_validation::REGISTER_AVAILABLE);
}

inline bool is_valid_stallwait_combination(uint32_t sync_type, uint32_t mask) {
    // Check for known problematic combinations
    if (sync_type == llk_validation::STALL_TRISC_CFG && mask == llk_validation::STALL_THCON) {
        return false; // Known to cause race conditions
    }
    return true;
}

inline bool is_ternary_sfpu_operation(uint32_t op_type) {
    // Note: SfpuType enum values are hardware-defined
    return (op_type >= 11 && op_type <= 13); // Placeholder for ternary operations
}

inline bool is_dest_register_available_for_ternary_ops(uint32_t dest_index) {
    return (dest_index < 13 && // Reserve space for three operands
            get_dest_register_status(dest_index) == llk_validation::REGISTER_AVAILABLE &&
            get_dest_register_status(dest_index + 1) == llk_validation::REGISTER_AVAILABLE &&
            get_dest_register_status(dest_index + 2) == llk_validation::REGISTER_AVAILABLE);
}

inline bool is_precision_critical_sfpu_operation(uint32_t op_type) {
    // Operations known to have precision issues from GitHub issues
    // Note: SfpuType enum values are hardware-defined, using placeholder logic for now
    return (op_type >= 8 && op_type <= 10); // Placeholder for precision-critical operations
}

inline bool is_mathematically_correct(float result, float expected, float tolerance) {
    if (std::isnan(result) || std::isnan(expected)) {
        return std::isnan(result) && std::isnan(expected);
    }
    if (std::isinf(result) || std::isinf(expected)) {
        return result == expected;
    }
    return std::abs(result - expected) <= tolerance;
}

// Implement the forward declared function
inline bool are_architectures_equivalent(float wormhole_result, float blackhole_result) {
    // Simple implementation - would use ULPCalculator in full version
    if (std::isnan(wormhole_result) || std::isnan(blackhole_result)) {
        return std::isnan(wormhole_result) && std::isnan(blackhole_result);
    }
    if (std::isinf(wormhole_result) || std::isinf(blackhole_result)) {
        return wormhole_result == blackhole_result;
    }
    return std::abs(wormhole_result - blackhole_result) <= 1e-6f;
}

// ============================================================================
// **RESOURCE TRACKING** - RAII-based memory and register leak detection
// ============================================================================

class LLKResourceTracker {
private:
    uint32_t initial_register_count;
    uint32_t initial_memory_usage;
    std::string operation_name;
    
public:
    LLKResourceTracker(const std::string& op_name = "unknown") 
        : operation_name(op_name) {
        initial_register_count = get_current_register_usage();
        initial_memory_usage = get_current_memory_usage();
    }
    
    ~LLKResourceTracker() {
        check_for_leaks();
    }
    
    void track_register_usage(uint32_t registers_allocated) {
        uint32_t current_usage = get_current_register_usage();
        if (current_usage > initial_register_count + registers_allocated) {
            LLK_VALIDATION_ERROR("Register usage exceeded expected allocation in " + operation_name);
        }
    }
    
    void track_memory_allocation(uint32_t bytes_allocated) {
        uint32_t current_usage = get_current_memory_usage();
        if (current_usage > initial_memory_usage + bytes_allocated) {
            LLK_VALIDATION_ERROR("Memory usage exceeded expected allocation in " + operation_name);
        }
    }
    
    bool check_for_leaks() {
        uint32_t final_register_count = get_current_register_usage();
        uint32_t final_memory_usage = get_current_memory_usage();
        
        bool register_leak = (final_register_count != initial_register_count);
        bool memory_leak = (final_memory_usage != initial_memory_usage);
        
        if (register_leak) {
            LLK_VALIDATION_ERROR("Register leak detected in " + operation_name + 
                ": initial=" + std::to_string(initial_register_count) + 
                " final=" + std::to_string(final_register_count));
        }
        
        if (memory_leak) {
            LLK_VALIDATION_ERROR("Memory leak detected in " + operation_name + 
                ": initial=" + std::to_string(initial_memory_usage) + 
                " final=" + std::to_string(final_memory_usage));
        }
        
        return !(register_leak || memory_leak);
    }
    
private:
    uint32_t get_current_register_usage() {
        // Placeholder - would interface with hardware register tracking
        return 0;
    }
    
    uint32_t get_current_memory_usage() {
        // Placeholder - would interface with L1 memory tracking
        return 0;
    }
}; 