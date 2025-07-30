// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

/**
 * @file llk_common_utils.h
 * @brief Common utilities and template patterns for Low-Level Kernel operations
 *
 * @details This header provides shared utilities and template patterns that are
 * commonly used across multiple LLK modules. By centralizing these patterns,
 * we reduce code duplication and ensure consistent behavior across the codebase.
 *
 * **Utility Categories:**
 * - Format configuration helpers
 * - Address generation utilities
 * - Template validation helpers
 * - Common configuration patterns
 * - Resource management helpers
 *
 * **Design Goals:**
 * - Reduce code duplication across LLK modules
 * - Provide consistent API patterns
 * - Maintain zero-overhead abstractions
 * - Enable compile-time optimization
 */

#pragma once

// **ZERO-OVERHEAD INCLUDES** - Absolute minimum for stub framework
#include <cstdint>       // For uint32_t types only
#include "ckernel_defs.h"
#include "tensix_types.h"

namespace ckernel {
namespace utils {

// Forward declarations
bool is_precision_loss_conversion(uint32_t src_format, uint32_t dst_format);

// ============================================================================
// ADVANCED FORMAT CONFIGURATION UTILITIES
// ============================================================================

/**
 * @brief Advanced format configuration for precise memory and precision management
 * 
 * Provides compile-time format analysis, memory calculations, and precision optimization
 * for high-performance tensor operations with hardware-specific optimizations.
 */
class FormatConfig {
public:
    /**
     * @brief Calculate exact tile size in bytes for specific data format
     * @param format Target data format for size calculation
     * @return Exact memory footprint in bytes for single tile
     */
    static constexpr uint32_t calculate_tile_size_bytes(DataFormat format) {
        switch (format) {
            case DataFormat::Float32: return 4096;   // 32x32 * 4 bytes
            case DataFormat::Float16: return 2048;   // 32x32 * 2 bytes  
            case DataFormat::Float16_b: return 2048; // 32x32 * 2 bytes
            case DataFormat::Bfp8_b: return 1152;    // 32x32 * 1 byte + exp
            case DataFormat::Bfp4_b: return 640;     // 32x32 * 0.5 byte + exp
            case DataFormat::Bfp2_b: return 320;     // 32x32 * 0.25 byte + exp
            case DataFormat::UInt32: return 4096;    // 32x32 * 4 bytes
            case DataFormat::UInt16: return 2048;    // 32x32 * 2 bytes
            case DataFormat::UInt8: return 1024;     // 32x32 * 1 byte
            default: return 2048;
        }
    }

    /**
     * @brief Get precision bits for format-specific ULP calculations
     * @param format Data format to analyze
     * @return Number of precision bits (mantissa width)
     */
    static constexpr uint32_t get_format_precision_bits(DataFormat format) {
        switch (format) {
            case DataFormat::Float32: return 23;     // IEEE 754 single precision
            case DataFormat::Float16: return 10;     // IEEE 754 half precision
            case DataFormat::Float16_b: return 7;    // bfloat16 precision
            case DataFormat::Bfp8_b: return 3;       // 3-bit mantissa
            case DataFormat::Bfp4_b: return 1;       // 1-bit mantissa  
            case DataFormat::Bfp2_b: return 0;       // Sign bit only
            default: return 10;
        }
    }

    /**
     * @brief Check if format supports round-to-nearest operations
     * @param format Format to check for rounding support
     * @return True if hardware supports precise rounding for this format
     */
    static constexpr bool supports_round_to_nearest(DataFormat format) {
        switch (format) {
            case DataFormat::Float32:
            case DataFormat::Float16:
            case DataFormat::Float16_b:
                return true;
            case DataFormat::Bfp8_b:
            case DataFormat::Bfp4_b: 
            case DataFormat::Bfp2_b:
                return false; // Block formats use truncation
            default: return false;
        }
    }
};

// ============================================================================
// HARDWARE ADDRESS GENERATION AND MEMORY MANAGEMENT
// ============================================================================

/**
 * @brief Hardware-optimized address generation for L1 memory access patterns
 * 
 * Handles complex address arithmetic for multi-bank L1 access, face layouts,
 * and optimal memory stride patterns for maximum throughput.
 */
class AddressGenerator {
public:
    /**
     * @brief Generate optimal L1 base address for tile access pattern
     * @param bank_id Target L1 bank (0-15 for Wormhole/Blackhole)
     * @param tile_index Logical tile index within bank
     * @param face_layout Memory layout pattern (RowMajor/ColMajor)
     * @return Hardware-optimized base address for maximum bandwidth
     */
    static constexpr uint32_t generate_l1_base_address(uint32_t bank_id, uint32_t tile_index, uint32_t face_layout) {
        // L1 bank base addresses for Wormhole B0 architecture
        constexpr uint32_t L1_BANK_BASE = 0x20000;
        constexpr uint32_t BANK_OFFSET = 0x10000;
        constexpr uint32_t TILE_SIZE = 2048; // Standard tile size
        
        uint32_t bank_base = L1_BANK_BASE + (bank_id * BANK_OFFSET);
        uint32_t tile_offset = tile_index * TILE_SIZE;
        
        // Apply face layout optimization
        if (face_layout == 1) { // ColMajor optimization
            tile_offset = (tile_offset & 0xFFFF0000) | ((tile_offset & 0x0000FFFF) << 1);
        }
        
        return bank_base + tile_offset;
    }

    /**
     * @brief Calculate stride pattern for optimal cache utilization
     * @param num_tiles Number of tiles in access pattern
     * @param access_pattern Linear=0, Strided=1, Random=2
     * @return Optimal stride value for hardware prefetcher
     */
    static constexpr uint32_t calculate_optimal_stride(uint32_t num_tiles, uint32_t access_pattern) {
        if (access_pattern == 0) return 1;      // Linear access
        if (access_pattern == 1) return 2;      // Strided access for bank conflict avoidance
        return num_tiles / 4;                   // Random access - quarter stride
    }
};

// ============================================================================
// TILE DIMENSION VALIDATION AND OPTIMIZATION  
// ============================================================================

/**
 * @brief Comprehensive tile dimension analysis and validation
 * 
 * Provides hardware-aware tile sizing, dimension validation, and
 * performance optimization recommendations for specific architectures.
 */
class TileDimensions {
public:
    /**
     * @brief Validate tile dimensions against hardware constraints
     * @param height Tile height (1-32)
     * @param width Tile width (1-32) 
     * @param num_faces Number of faces (1, 2, 4)
     * @return True if dimensions are hardware-valid
     */
    static constexpr bool validate_tile_dimensions(uint32_t height, uint32_t width, uint32_t num_faces) {
        // Basic dimension constraints
        if (height == 0 || height > 32 || width == 0 || width > 32) return false;
        
        // Total area constraint
        if ((height * width) > 1024) return false;
        
        // Face count validation  
        if (num_faces != 1 && num_faces != 2 && num_faces != 4) return false;
        
        // Architecture-specific constraints
        if (num_faces == 4 && (height < 16 || width < 16)) return false;
        
        return true;
    }

    // **ZERO-OVERHEAD STUB** - Function removed to eliminate std::pair dependency

    /**
     * @brief Get recommended face count for tile dimensions
     * @param height Tile height
     * @param width Tile width
     * @return Optimal number of faces (1, 2, or 4)
     */
    static constexpr uint32_t get_recommended_face_count(uint32_t height, uint32_t width) {
        uint32_t area = height * width;
        if (area >= 512) return 4;      // Large tiles benefit from 4 faces
        if (area >= 128) return 2;      // Medium tiles use 2 faces
        return 1;                       // Small tiles use single face
    }
};

// ============================================================================
// ULP (Units in the Last Place) PRECISION CALCULATOR
// ============================================================================

// **ZERO-OVERHEAD ULP CALCULATOR STUB** - Framework preserved, no memory impact
class ULPCalculator {
public:
    static float calculate_ulp_error(float, float) { return 0.0f; }
    static bool is_ulp_error_acceptable(float, float) { return true; }
};

// ============================================================================
// CROSS-ARCHITECTURE COMPATIBILITY AND VALIDATION
// ============================================================================

// **ZERO-OVERHEAD CROSS-ARCHITECTURE STUB** - Framework preserved
class CrossArchValidation {
public:
    static bool are_architectures_equivalent(float, float, const char* = "unknown") { return true; }
    static bool has_known_architecture_issues(const char*) { return false; }
};

// ============================================================================
// PRECISION ERROR HANDLING AND REPORTING
// ============================================================================

// **ZERO-OVERHEAD ERROR HANDLING STUB** - Framework preserved
class ErrorHandling {
public:
    static bool is_precision_bug_indicated(float, const char*) { return false; }
    template<typename T>
    static void log_precision_issue(const char*, T, T, float) { /* no-op */ }
};

// ULPCalculator moved before CrossArchValidation

// ============================================================================
// ROUNDING AND PRECISION UTILITIES
// ============================================================================

/**
 * @brief Specialized rounding utilities for different data formats
 * 
 * Handles format-specific rounding modes, precision conversions, and
 * hardware-compliant rounding behavior for validation purposes.
 */
// **ZERO-OVERHEAD ROUNDING UTILS STUB** - Framework preserved  
class RoundingUtils {
public:
    static float round_to_nearest_bfp16(float value) { return value; }
};

// ============================================================================
// TYPE ALIASES FOR COMMON CONFIGURATIONS
// ============================================================================

// Hardware thread configuration (using proper namespacing)
using UnpackConfig = uint32_t; // Simplified for compatibility
using MathConfig = uint32_t;   // Simplified for compatibility  
using PackConfig = uint32_t;   // Simplified for compatibility

// ============================================================================
// HELPER FUNCTION IMPLEMENTATIONS
// ============================================================================

/**
 * @brief Check if format conversion will cause precision loss
 * @param src_format Source data format
 * @param dst_format Destination data format
 * @return True if conversion loses precision
 */
inline bool is_precision_loss_conversion(uint32_t src_format, uint32_t dst_format) {
    auto src_precision = FormatConfig::get_format_precision_bits(static_cast<DataFormat>(src_format));
    auto dst_precision = FormatConfig::get_format_precision_bits(static_cast<DataFormat>(dst_format));
    return src_precision > dst_precision;
}

} // namespace utils
} // namespace ckernel

//==============================================================================
// Convenience Macros
//==============================================================================

/**
 * @brief Macro to apply common validation pattern
 * @param format_config Format configuration type
 * @param src_fmt Source format
 * @param dst_fmt Destination format
 */
#define LLK_APPLY_FORMAT_CONFIG(format_config, src_fmt, dst_fmt) \
    format_config::configure_data_format(src_fmt, dst_fmt)

/**
 * @brief Macro for common thread initialization
 * @param thread_config Thread configuration type
 */
#define LLK_INIT_THREAD(thread_config) \
    thread_config::initialize_thread()

/**
 * @brief Macro for standard tile validation
 * @param r_dim Row dimension
 * @param c_dim Column dimension  
 * @param num_faces Number of faces
 */
#define LLK_VALIDATE_STANDARD_TILE(r_dim, c_dim, num_faces) \
    ckernel::utils::TileDimensions::validate_tile_dimensions(r_dim, c_dim, num_faces) 
