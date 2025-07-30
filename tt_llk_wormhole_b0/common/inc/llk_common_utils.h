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

#include <cstdint>
#include <cmath>
#include <algorithm>
#include <string>
#include <type_traits>
#include <utility>
#include <cstring>
#include <sstream>
#include <iomanip>
#include <cstdio>
#include <limits>
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

    /**
     * @brief Calculate optimal tile dimensions for target area
     * @param target_area Desired tile area (elements)
     * @return Pair of (height, width) optimized for hardware
     */
    static constexpr std::pair<uint32_t, uint32_t> calculate_optimal_dimensions(uint32_t target_area) {
        if (target_area <= 32) {
            return {1, std::min(target_area, static_cast<uint32_t>(32))};
        }
        
        // Find closest factors that minimize memory bank conflicts
        for (uint32_t h = 32; h >= 1; --h) {
            if (target_area % h == 0) {
                uint32_t w = target_area / h;
                if (w <= 32) {
                    return {h, w};
                }
            }
        }
        
        return {32, 32}; // Default to maximum tile size
    }

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

/**
 * @brief Precise ULP error calculation for floating-point validation
 * 
 * Implements IEEE 754-compliant ULP distance calculation for rigorous
 * precision testing and hardware validation across different data formats.
 * 
 * **Critical for SFPU operations** - Based on findings from GitHub issues showing 
 * catastrophic precision errors (69,846 ULP in mean operation, severe rounding bugs 
 * in power functions). This class provides the mathematical foundation for detecting 
 * such issues before they reach production.
 */
class ULPCalculator {
public:
    /**
     * @brief Calculate ULP (Units in the Last Place) error between two values
     * @param actual Hardware-computed floating point result
     * @param expected Mathematically correct reference value
     * @return ULP distance as double precision value
     */
    static double calculate_ulp_error(float actual, float expected) {
        // Handle special cases
        if (std::isnan(actual) || std::isnan(expected)) {
            return std::isnan(actual) && std::isnan(expected) ? 0.0 : std::numeric_limits<double>::infinity();
        }
        
        if (std::isinf(actual) || std::isinf(expected)) {
            return (actual == expected) ? 0.0 : std::numeric_limits<double>::infinity();
        }
        
        // Bit-level comparison for exact ULP calculation
        uint32_t actual_bits, expected_bits;
        std::memcpy(&actual_bits, &actual, sizeof(float));
        std::memcpy(&expected_bits, &expected, sizeof(float));
        
        // Handle sign differences
        if ((actual_bits ^ expected_bits) & 0x80000000) {
            // Different signs - calculate distance through zero
            uint32_t actual_dist = (actual_bits & 0x7FFFFFFF);
            uint32_t expected_dist = (expected_bits & 0x7FFFFFFF);
            return static_cast<double>(actual_dist + expected_dist);
        }
        
        // Same sign - direct ULP distance
        return static_cast<double>(actual_bits > expected_bits ? 
                                 actual_bits - expected_bits : 
                                 expected_bits - actual_bits);
    }

    /**
     * @brief Check if ULP error is within acceptable bounds
     * @param ulp_error Calculated ULP error
     * @param max_ulp_error Maximum acceptable ULP error threshold
     * @return True if error is within acceptable range
     */
    static bool is_ulp_error_acceptable(double ulp_error, double max_ulp_error) {
        constexpr double EMERGENCY_ULP_THRESHOLD = 10000.0; // Emergency ceiling
        
        if (ulp_error > EMERGENCY_ULP_THRESHOLD) {
            return false; // Catastrophic precision loss
        }
        
        return ulp_error <= max_ulp_error;
    }
};

// ============================================================================
// CROSS-ARCHITECTURE COMPATIBILITY AND VALIDATION
// ============================================================================

/**
 * @brief Cross-architecture validation and compatibility checking
 * 
 * Handles validation across Wormhole B0, Blackhole, and future architectures
 * with architecture-specific bug detection and workaround application.
 */
class CrossArchValidation {
public:
    /**
     * @brief Compare results across architectures for consistency
     * @param wormhole_result Result from Wormhole B0 execution
     * @param blackhole_result Result from Blackhole execution  
     * @param operation_name Name of operation for error reporting
     * @return True if results are within acceptable cross-arch tolerance
     */
    static bool are_architectures_equivalent(float wormhole_result, float blackhole_result, const char* operation_name = "unknown") {
        constexpr float MAX_ARCH_DIFF_ULP = 8.0f; // Allow up to 8 ULP difference
        
        double ulp_diff = ULPCalculator::calculate_ulp_error(wormhole_result, blackhole_result);
        
        if (ulp_diff > MAX_ARCH_DIFF_ULP) {
            // Log architecture divergence for analysis
            fprintf(stderr, "ARCHITECTURE DIVERGENCE in %s: Wormhole=%.6f Blackhole=%.6f ULP_diff=%.1f\n",
                    operation_name, wormhole_result, blackhole_result, ulp_diff);
            return false;
        }
        
        return true;
    }

    /**
     * @brief Check for known architecture-specific issues
     * @param operation_name Operation to check for known issues
     * @return True if operation has known cross-architecture problems
     */
    static bool has_known_architecture_issues(const char* operation_name) {
        std::string op(operation_name);
        
        // Known Blackhole issues from GitHub repository analysis
        if (op.find("power") != std::string::npos) {
            return true; // Power function has different precision on Blackhole
        }
        if (op.find("reciprocal") != std::string::npos) {
            return true; // 24,749 ULP error reported in issues
        }
        if (op.find("mean") != std::string::npos) {
            return true; // 69,846 ULP error specific to certain architectures
        }
        if (op.find("rms_norm") != std::string::npos) {
            return true; // Higher ATOL than primitive operations
        }
        
        return false;
    }
    
    /**
     * @brief Get recommended ULP tolerance for cross-architecture validation
     * @param operation_name Operation name for context-specific tolerance
     * @return Recommended maximum ULP difference between architectures
     */
    static double get_recommended_cross_arch_tolerance(const char* operation_name) {
        std::string op(operation_name);
        
        // Operation-specific tolerances based on known issues
        if (op.find("power") != std::string::npos) {
            return 16.0; // Power function needs higher tolerance
        }
        if (op.find("mean") != std::string::npos) {
            return 32.0; // Mean operation has higher variance
        }
        if (op.find("reciprocal") != std::string::npos) {
            return 24.0; // Based on known 24,749 ULP issue
        }
        
        return 8.0; // Default tolerance for most operations
    }
};

// ============================================================================
// PRECISION ERROR HANDLING AND REPORTING
// ============================================================================

/**
 * @brief Advanced error handling for precision-critical operations
 * 
 * Provides detailed error analysis, ULP calculations, and automatic
 * precision issue detection with operation-specific thresholds.
 */
class ErrorHandling {
public:
    /**
     * @brief Generate detailed precision error message for debugging
     * @param operation Operation name for context
     * @param actual Hardware-computed result  
     * @param expected Mathematically correct result
     * @param ulp_error Calculated ULP error magnitude
     * @return Formatted error message with diagnostic information
     */
    template<typename T>
    static std::string generate_precision_error_message(const char* operation, T actual, T expected, double ulp_error) {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(6);
        oss << "PRECISION ERROR in " << operation << ":\n"
            << "  Actual result:    " << actual << "\n"
            << "  Expected result:  " << expected << "\n"
            << "  ULP error:        " << ulp_error << "\n"
            << "  Relative error:   " << std::abs((actual - expected) / expected) * 100.0 << "%\n";
            
        // Add context-specific analysis
        if (is_precision_bug_indicated(ulp_error, operation)) {
            oss << "  *** MATCHES KNOWN BUG PATTERN ***\n";
        }
        
        return oss.str();
    }

    /**
     * @brief Detect if precision error indicates known hardware bug
     * @param ulp_error ULP error magnitude
     * @param operation Operation name for bug pattern matching
     * @return True if error pattern matches known precision bugs
     */
    static bool is_precision_bug_indicated(double ulp_error, const char* operation) {
        std::string op_str(operation);
        
        // Check for specific known precision bugs from GitHub issues
        if (op_str.find("power") != std::string::npos && ulp_error > 2.0) {
            return true; // Power function truncation bug (9^2 = 80.5)
        }
        
        if (op_str.find("mean") != std::string::npos && ulp_error > 50000.0) {
            return true; // ttnn.mean 69,846 ULP error pattern
        }
        
        if (op_str.find("reciprocal") != std::string::npos && ulp_error > 20000.0) {
            return true; // Reciprocal 24,749 ULP error pattern
        }
        
        if (ulp_error > 10000.0) {
            return true; // Catastrophic precision loss (emergency threshold)
        }
        
        return false;
    }
    
    /**
     * @brief Get severity level for precision error
     * @param ulp_error ULP error magnitude
     * @param operation Operation name for context
     * @return Severity level (0=acceptable, 1=warning, 2=error, 3=critical)
     */
    static int get_precision_error_severity(double ulp_error, const char* operation) {
        if (ulp_error > 10000.0) {
            return 3; // Critical - catastrophic precision loss
        }
        if (is_precision_bug_indicated(ulp_error, operation)) {
            return 2; // Error - matches known bug pattern
        }
        if (ulp_error > 100.0) {
            return 1; // Warning - elevated precision error
        }
        return 0; // Acceptable precision
    }
    
    /**
     * @brief Log precision error with appropriate severity
     * @param operation Operation name
     * @param actual Hardware result
     * @param expected Expected result
     * @param ulp_error ULP error magnitude
     */
    template<typename T>
    static void log_precision_issue(const char* operation, T actual, T expected, double ulp_error) {
        int severity = get_precision_error_severity(ulp_error, operation);
        const char* severity_names[] = {"INFO", "WARNING", "ERROR", "CRITICAL"};
        
        std::string message = generate_precision_error_message(operation, actual, expected, ulp_error);
        
        fprintf(stderr, "[%s] %s", severity_names[severity], message.c_str());
        
        // For critical errors, also trigger validation system
        if (severity >= 3) {
            #ifdef LLK_VALIDATION_ENABLED
            LLK_VALIDATION_ERROR("Critical precision error detected: " + message);
            #endif
        }
    }
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
class RoundingUtils {
public:
    /**
     * @brief Round float32 to bfloat16 with proper IEEE compliance
     * @param value Input float32 value
     * @return Properly rounded bfloat16 equivalent
     */
    static float round_to_nearest_bfp16(float value) {
        if (std::isnan(value) || std::isinf(value)) return value;
        
        uint32_t bits;
        std::memcpy(&bits, &value, sizeof(float));
        
        // BF16 rounding: truncate to 16 bits with round-to-nearest-even
        uint32_t round_bit = (bits >> 15) & 1;
        uint32_t sticky_bits = bits & 0x7FFF;
        
        if (round_bit && (sticky_bits || ((bits >> 16) & 1))) {
            bits += 0x10000; // Round up
        }
        
        bits &= 0xFFFF0000; // Truncate to BF16 precision
        
        float result;
        std::memcpy(&result, &bits, sizeof(float));
        return result;
    }
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
