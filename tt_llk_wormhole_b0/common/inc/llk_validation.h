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
#include <cassert>
#include "ckernel_defs.h"
#include "llk_defs.h"

namespace ckernel {
namespace validation {

//==============================================================================
// Validation Configuration
//==============================================================================

#ifndef LLK_VALIDATION_LEVEL
    #ifdef LLK_DEBUG
        #define LLK_VALIDATION_LEVEL 2  // Full validation in debug builds
    #else
        #define LLK_VALIDATION_LEVEL 0  // No validation in release builds
    #endif
#endif

//==============================================================================
// Core Validation Macros
//==============================================================================

#if LLK_VALIDATION_LEVEL > 0

/**
 * @brief Basic parameter validation with custom message
 * @param condition Boolean condition that must be true
 * @param message Error message for assertion
 */
#define LLK_VALIDATE(condition, message) \
    assert((condition) && (message))

/**
 * @brief Validate parameter is within specified range (inclusive)
 * @param param Parameter value to check
 * @param min_val Minimum valid value (inclusive)
 * @param max_val Maximum valid value (inclusive) 
 * @param message Error message for out-of-range values
 */
#define LLK_VALIDATE_PARAM_RANGE(param, min_val, max_val, message) \
    assert(((param) >= (min_val)) && ((param) <= (max_val)) && (message))

/**
 * @brief Validate parameter is a power of 2
 * @param param Parameter value to check
 * @param message Error message for non-power-of-2 values
 */
#define LLK_VALIDATE_POWER_OF_2(param, message) \
    assert(((param) > 0) && (((param) & ((param) - 1)) == 0) && (message))

#else
    // Release builds: all validation compiles to no-ops
    #define LLK_VALIDATE(condition, message) ((void)0)
    #define LLK_VALIDATE_PARAM_RANGE(param, min_val, max_val, message) ((void)0)
    #define LLK_VALIDATE_POWER_OF_2(param, message) ((void)0)
#endif

//==============================================================================
// Advanced Validation (Level 2+)
//==============================================================================

#if LLK_VALIDATION_LEVEL >= 2

/**
 * @brief Validate tile dimensions are hardware-compatible
 * @param r_dim Row dimension of tile
 * @param c_dim Column dimension of tile
 */
#define LLK_VALIDATE_TILE_DIMS(r_dim, c_dim) do { \
    LLK_VALIDATE_PARAM_RANGE(r_dim, 1, 32, "tile row dimension must be 1-32"); \
    LLK_VALIDATE_PARAM_RANGE(c_dim, 1, 32, "tile column dimension must be 1-32"); \
    LLK_VALIDATE_POWER_OF_2(r_dim, "tile row dimension must be power of 2"); \
    LLK_VALIDATE_POWER_OF_2(c_dim, "tile column dimension must be power of 2"); \
} while(0)

/**
 * @brief Validate data format compatibility between source and destination
 * @param src_format Source data format
 * @param dst_format Destination data format
 */
#define LLK_VALIDATE_TILE_FORMAT(src_format, dst_format) do { \
    LLK_VALIDATE(is_valid_data_format(src_format), "invalid source data format"); \
    LLK_VALIDATE(is_valid_data_format(dst_format), "invalid destination data format"); \
    LLK_VALIDATE(is_compatible_format_conversion(src_format, dst_format), \
                 "incompatible format conversion"); \
} while(0)

/**
 * @brief Validate matrix multiplication dimensions are compatible
 * @param m_dim M dimension (rows of A, rows of result)
 * @param n_dim N dimension (columns of B, columns of result)
 * @param k_dim K dimension (columns of A, rows of B)
 */
#define LLK_VALIDATE_MATMUL_DIMS(m_dim, n_dim, k_dim) do { \
    LLK_VALIDATE_PARAM_RANGE(m_dim, 1, 128, "M dimension must be 1-128"); \
    LLK_VALIDATE_PARAM_RANGE(n_dim, 1, 128, "N dimension must be 1-128"); \
    LLK_VALIDATE_PARAM_RANGE(k_dim, 1, 128, "K dimension must be 1-128"); \
} while(0)

/**
 * @brief Validate fidelity configuration is valid
 * @param fidelity_desc Fidelity descriptor value
 */
#define LLK_VALIDATE_FIDELITY(fidelity_desc) do { \
    LLK_VALIDATE(((fidelity_desc) == 0x0) || ((fidelity_desc) == 0x1) || \
                 ((fidelity_desc) == 0x3), \
                 "fidelity must be 0x0 (1 phase), 0x1 (2 phases), or 0x3 (4 phases)"); \
} while(0)

/**
 * @brief Validate L1 address alignment
 * @param addr L1 memory address
 */
#define LLK_VALIDATE_L1_ALIGNMENT(addr) do { \
    LLK_VALIDATE(((addr) & 0xF) == 0, "L1 address must be 16-byte aligned"); \
} while(0)

#else
    // Level 1 builds: advanced validation disabled
    #define LLK_VALIDATE_TILE_DIMS(r_dim, c_dim) ((void)0)
    #define LLK_VALIDATE_TILE_FORMAT(src_format, dst_format) ((void)0)
    #define LLK_VALIDATE_MATMUL_DIMS(m_dim, n_dim, k_dim) ((void)0)
    #define LLK_VALIDATE_FIDELITY(fidelity_desc) ((void)0)
    #define LLK_VALIDATE_L1_ALIGNMENT(addr) ((void)0)
#endif

//==============================================================================
// Validation Helper Functions
//==============================================================================

#if LLK_VALIDATION_LEVEL > 0

/**
 * @brief Check if data format is valid
 * @param format DataFormat enumeration value
 * @return true if format is supported by hardware
 */
constexpr bool is_valid_data_format(uint32_t format) {
    return (format < static_cast<uint32_t>(DataFormat::Invalid));
}

/**
 * @brief Check if format conversion is supported
 * @param src_format Source format
 * @param dst_format Destination format
 * @return true if conversion is supported by hardware
 */
constexpr bool is_compatible_format_conversion(uint32_t src_format, uint32_t dst_format) {
    // Allow all conversions for now - hardware handles most combinations
    // More specific validation can be added based on hardware limitations
    return is_valid_data_format(src_format) && is_valid_data_format(dst_format);
}

/**
 * @brief Validate resource acquisition state
 * @param acquired Current acquisition state
 * @param resource_name Name of resource for error messages
 */
inline void validate_resource_state(bool acquired, const char* resource_name) {
    LLK_VALIDATE(acquired, "resource must be acquired before use");
    (void)resource_name; // Suppress unused parameter warning in release builds
}

/**
 * @brief Validate tile index is within bounds
 * @param tile_index Index of tile to validate
 * @param max_tiles Maximum number of tiles
 */
inline void validate_tile_index(uint32_t tile_index, uint32_t max_tiles) {
    LLK_VALIDATE_PARAM_RANGE(tile_index, 0, max_tiles - 1, "tile index out of bounds");
}

#else
    // Release builds: all helper functions become no-ops
    constexpr bool is_valid_data_format(uint32_t) { return true; }
    constexpr bool is_compatible_format_conversion(uint32_t, uint32_t) { return true; }
    inline void validate_resource_state(bool, const char*) { }
    inline void validate_tile_index(uint32_t, uint32_t) { }
#endif

//==============================================================================
// Resource Management Validation
//==============================================================================

#if LLK_VALIDATION_LEVEL >= 2

/**
 * @brief RAII-style resource tracker for debug builds
 * @details Automatically tracks resource acquisition/release in debug builds
 * and validates proper pairing of acquire/release calls.
 */
class ResourceTracker {
private:
    bool acquired_;
    const char* resource_name_;

public:
    explicit ResourceTracker(const char* name) : acquired_(false), resource_name_(name) {}
    
    ~ResourceTracker() {
        LLK_VALIDATE(!acquired_, "resource not released before destruction");
    }
    
    void acquire() {
        LLK_VALIDATE(!acquired_, "resource already acquired");
        acquired_ = true;
    }
    
    void release() {
        LLK_VALIDATE(acquired_, "resource not acquired");
        acquired_ = false;
    }
    
    bool is_acquired() const { return acquired_; }
};

/**
 * @brief Macro to declare resource tracker for debug builds
 * @param name Variable name for the tracker
 * @param resource_name String name of the resource
 */
#define LLK_DECLARE_RESOURCE_TRACKER(name, resource_name) \
    ResourceTracker name(resource_name)

/**
 * @brief Macro to track resource acquisition
 * @param tracker Resource tracker instance
 */
#define LLK_TRACK_ACQUIRE(tracker) (tracker).acquire()

/**
 * @brief Macro to track resource release
 * @param tracker Resource tracker instance
 */
#define LLK_TRACK_RELEASE(tracker) (tracker).release()

#else
    // Release builds: resource tracking disabled
    #define LLK_DECLARE_RESOURCE_TRACKER(name, resource_name) ((void)0)
    #define LLK_TRACK_ACQUIRE(tracker) ((void)0)
    #define LLK_TRACK_RELEASE(tracker) ((void)0)
#endif

} // namespace validation
} // namespace ckernel

//==============================================================================
// Convenience Aliases (Global Namespace) 
//==============================================================================

// Note: Validation macros (LLK_VALIDATE, LLK_VALIDATE_PARAM_RANGE, etc.) 
// are already available in global scope and don't need using declarations.
// Only bring namespace-scoped utility functions into global scope for convenience.

using ckernel::validation::is_valid_data_format;
using ckernel::validation::is_compatible_format_conversion;
using ckernel::validation::validate_resource_state;
using ckernel::validation::validate_tile_index; 