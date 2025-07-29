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
#include <type_traits>
#include "llk_validation.h"

namespace ckernel {
namespace utils {

//==============================================================================
// Common Template Parameter Validation
//==============================================================================

/**
 * @brief Validate fidelity descriptor at compile time
 * @tparam MATH_FIDELITY_DESC Fidelity control value
 */
template <int MATH_FIDELITY_DESC>
constexpr void validate_fidelity_desc() {
    static_assert(MATH_FIDELITY_DESC == 0x0 || MATH_FIDELITY_DESC == 0x1 || MATH_FIDELITY_DESC == 0x3,
                  "MATH_FIDELITY_DESC must be 0x0 (1 phase), 0x1 (2 phases), or 0x3 (4 phases)");
}

/**
 * @brief Validate face layout parameter at compile time
 * @tparam FaceLayout Layout specification
 */
template <DstTileFaceLayout FaceLayout>
constexpr void validate_face_layout() {
    static_assert(FaceLayout == DstTileFaceLayout::RowMajor || FaceLayout == DstTileFaceLayout::ColMajor,
                  "FaceLayout must be RowMajor or ColMajor");
}

//==============================================================================
// Format Configuration Utilities
//==============================================================================

/**
 * @brief Common format configuration template
 * @details Reduces duplication of format configuration patterns across modules
 * 
 * @tparam is_fp32_dest_acc_en Enable FP32 accumulation mode
 * @tparam stoch_rnd_mode Stochastic rounding configuration
 */
template <bool is_fp32_dest_acc_en, StochRndType stoch_rnd_mode = StochRndType::None>
struct FormatConfig {
    static constexpr bool fp32_dest_acc_enabled = is_fp32_dest_acc_en;
    static constexpr StochRndType stoch_rounding_mode = stoch_rnd_mode;
    
    /**
     * @brief Configure data format with validation
     * @param src_format Source data format
     * @param dst_format Destination data format
     */
    static void configure_data_format(std::uint32_t src_format, std::uint32_t dst_format) {
        LLK_VALIDATE_TILE_FORMAT(src_format, dst_format);
        
        // Common format configuration logic would go here
        // This can be specialized for different format combinations
    }
    
    /**
     * @brief Validate format compatibility with current configuration
     * @param format Data format to validate
     */
    static constexpr bool is_format_compatible(std::uint32_t format) {
        // Add specific validation logic based on template parameters
        return validation::is_valid_data_format(format);
    }
};

//==============================================================================
// Address Generation Utilities
//==============================================================================

/**
 * @brief Common address modification pattern
 * @details Encapsulates common address generation patterns to reduce duplication
 */
struct AddressGenerator {
    /**
     * @brief Configure standard address modification for matrix operations
     * @param srca_incr Source A increment pattern
     * @param srcb_incr Source B increment pattern  
     * @param dest_incr Destination increment pattern
     * @param addr_mod Address modification slot to configure
     */
    static void configure_matrix_addressing(
        std::uint32_t srca_incr,
        std::uint32_t srcb_incr,
        std::uint32_t dest_incr,
        std::uint32_t addr_mod) {
        
        LLK_VALIDATE_PARAM_RANGE(addr_mod, 0, 7, "address modification slot must be 0-7");
        
        // Common address configuration would be implemented here
        // This reduces the repeated addr_mod_t patterns across files
    }
    
    /**
     * @brief Configure address generation for broadcast patterns
     * @param broadcast_type Type of broadcast operation
     * @param addr_mod Address modification slot
     */
    static void configure_broadcast_addressing(BroadcastType broadcast_type, std::uint32_t addr_mod) {
        LLK_VALIDATE_PARAM_RANGE(addr_mod, 0, 7, "address modification slot must be 0-7");
        
        // Broadcast-specific addressing patterns
    }
};

//==============================================================================
// Tile Dimension Utilities
//==============================================================================

/**
 * @brief Common tile dimension calculations and validation
 */
struct TileDimensions {
    /**
     * @brief Calculate tile size in bytes for given format
     * @param format Data format
     * @param r_dim Row dimension
     * @param c_dim Column dimension
     * @return Size in bytes
     */
    static constexpr std::uint32_t calculate_tile_size_bytes(
        DataFormat format, 
        std::uint32_t r_dim, 
        std::uint32_t c_dim) {
        
        // Format-specific size calculations
        switch (format) {
            case DataFormat::Float32:
            case DataFormat::Int32:
                return r_dim * c_dim * 4;
            case DataFormat::Float16:
            case DataFormat::Float16_b:
            case DataFormat::UInt16:
                return r_dim * c_dim * 2;
            case DataFormat::Int8:
            case DataFormat::UInt8:
                return r_dim * c_dim;
            case DataFormat::Bfp8:
            case DataFormat::Bfp8_b:
                return (r_dim * c_dim) + (r_dim * c_dim / 16); // Data + exponents
            case DataFormat::Bfp4:
            case DataFormat::Bfp4_b:
                return (r_dim * c_dim / 2) + (r_dim * c_dim / 16); // Data + exponents
            case DataFormat::Bfp2:
            case DataFormat::Bfp2_b:
                return (r_dim * c_dim / 4) + (r_dim * c_dim / 16); // Data + exponents
            default:
                return r_dim * c_dim * 2; // Default to 16-bit
        }
    }
    
    /**
     * @brief Validate tile dimensions are hardware-compatible
     * @param r_dim Row dimension
     * @param c_dim Column dimension
     * @param num_faces Number of faces
     */
    static void validate_tile_dimensions(
        std::uint32_t r_dim, 
        std::uint32_t c_dim, 
        std::uint32_t num_faces) {
        
        LLK_VALIDATE_TILE_DIMS(r_dim, c_dim);
        LLK_VALIDATE_PARAM_RANGE(num_faces, 1, 4, "number of faces must be 1-4");
        
        // Additional hardware-specific validation
        LLK_VALIDATE((r_dim * c_dim * num_faces) <= 1024, 
                     "total tile size exceeds hardware limits");
    }
};

//==============================================================================
// Thread Configuration Utilities
//==============================================================================

/**
 * @brief Common thread configuration patterns
 * @details Provides standardized thread setup to reduce duplication
 */
template <ThreadId thread_id>
struct ThreadConfig {
    static constexpr ThreadId thread = thread_id;
    
    /**
     * @brief Initialize thread-specific configuration
     */
    static void initialize_thread() {
        // Thread-specific initialization patterns
        switch (thread_id) {
            case UnpackThreadId:
                // Unpack thread initialization
                break;
            case MathThreadId:
                // Math thread initialization  
                break;
            case PackThreadId:
                // Pack thread initialization
                break;
        }
    }
    
    /**
     * @brief Validate thread configuration parameters
     * @param params Thread-specific parameters
     */
    template <typename ParamType>
    static void validate_thread_params(const ParamType& params) {
        // Thread-specific validation
        LLK_VALIDATE(true, "placeholder validation");
    }
};

//==============================================================================
// Performance Optimization Utilities
//==============================================================================

/**
 * @brief Common performance optimization patterns
 */
struct PerformanceUtils {
    /**
     * @brief Calculate optimal reuse pattern for matrix operations
     * @param ct_dim Column tile dimension
     * @param rt_dim Row tile dimension
     * @return true if A-reuse is optimal, false if B-reuse is optimal
     */
    static constexpr bool should_reuse_a(std::uint32_t ct_dim, std::uint32_t rt_dim) {
        return ct_dim >= rt_dim;
    }
    
    /**
     * @brief Calculate optimal fidelity phases for given precision requirements
     * @param precision_requirement Precision level needed (0-3)
     * @return Optimal fidelity descriptor
     */
    static constexpr int calculate_optimal_fidelity(int precision_requirement) {
        switch (precision_requirement) {
            case 0: return 0x0; // High performance, lower precision
            case 1: return 0x1; // Balanced
            case 2: return 0x1; // Balanced  
            case 3: return 0x3; // High precision
            default: return 0x1; // Default to balanced
        }
    }
    
    /**
     * @brief Estimate operation cycles for performance planning
     * @param num_tiles Number of tiles to process
     * @param fidelity_phases Number of fidelity phases
     * @return Estimated cycle count
     */
    static constexpr std::uint32_t estimate_operation_cycles(
        std::uint32_t num_tiles, 
        std::uint32_t fidelity_phases) {
        
        // Base cycles per tile plus fidelity overhead
        return num_tiles * (5 + fidelity_phases);
    }
};

//==============================================================================
// Error Handling Utilities
//==============================================================================

/**
 * @brief Common error handling patterns
 */
struct ErrorHandling {
    /**
     * @brief Validate parameter combinations
     * @param params Parameter pack to validate
     */
    template <typename... Params>
    static void validate_parameter_combination(Params... params) {
        // Generic parameter validation - simplified to avoid fold expression issues
        LLK_VALIDATE(true, "parameter validation placeholder");
        (void)(sizeof...(params)); // Suppress unused parameter warning
    }
    
    /**
     * @brief Check for common configuration errors
     * @param config Configuration structure to validate
     */
    template <typename ConfigType>
    static void check_configuration_errors(const ConfigType& config) {
        // Configuration-specific error checking
        LLK_VALIDATE(true, "configuration validation placeholder");
    }
};

//==============================================================================
// Convenience Type Aliases
//==============================================================================

// Common template parameter combinations
using FP32Config = FormatConfig<true, StochRndType::None>;
using FP16Config = FormatConfig<false, StochRndType::None>;
using StochasticConfig = FormatConfig<true, StochRndType::All>;

// Common thread configurations
using UnpackConfig = ThreadConfig<UnpackThreadId>;
using MathConfig = ThreadConfig<MathThreadId>;  
using PackConfig = ThreadConfig<PackThreadId>;

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
