// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <type_traits>
#include <cmath>

#include "ckernel_globals.h"
#include "ckernel_include.h"
#include "ckernel_ops.h"
#include "ckernel_sfpu.h"
#include "ckernel_template.h"
#include "cmath_common.h"
#include "llk_math_common.h"
#include "llk_sfpu_types.h"
#include "llk_validation.h"

using namespace ckernel;

// Type alias to resolve SfpuOp vs SfpuType mismatch
using SfpuOp = SfpuType;

// Minimal stub implementations for missing functions
inline bool is_compatible_format_conversion(uint32_t src_format, uint32_t dst_format) { return true; }
inline void log_precision_warning(const char* message) { }
inline uint32_t get_cycle_count() { return 0; }
inline float get_tile_sample_value(uint32_t tile_index, uint32_t row, uint32_t col) { return 0.0f; }
inline float get_operand_value(uint32_t tile_index, uint32_t operand_index) { return 1.0f; }
inline bool is_approximately_equal(float a, float b) { return std::abs(a - b) < 1e-6f; }
inline uint16_t get_tile_bfp16_value(uint32_t tile_index, uint32_t row, uint32_t col) { return 0; }
inline float get_tile_fp32_value(uint32_t tile_index, uint32_t row, uint32_t col) { return 0.0f; }
inline float round_to_nearest_bfp16(float val) { return val; }
inline float convert_bfp16_to_fp32(uint16_t val) { return 0.0f; }
inline uint16_t float_to_bfp16(float val) { return 0; }
inline float bfp16_to_float(uint16_t val) { return 0.0f; }
inline auto capture_execution_state(uint32_t dst_index) { return 0; }
inline void validate_power_function_inputs(uint32_t dst_index) { }
inline void validate_power_function_performance(uint32_t cycles) { }
inline void validate_division_operation_precision(uint32_t dst_index) { }
inline void validate_binary_operation_correctness(uint32_t dst_index, SfpuOp sfpu_op) { }

// Math sync function
template<DstSync dst_sync>
inline void math_dest_sync() { }

// Monitoring function stubs
inline void enable_binary_format_conversion_monitoring(uint32_t src_format, uint32_t dst_format) { }
inline void enable_precision_loss_conversion_monitoring() { }
inline void enable_precision_sensitive_approximate_mode_monitoring() { }
inline void set_truncation_detection_mode(uint32_t mode) { }
inline void enable_power_specific_ulp_monitoring() { }
inline void enable_rounding_correctness_monitoring() { }
inline void set_power_rounding_validation_mode(uint32_t mode) { }
inline void set_blackhole_sfpi_monitoring_mode(uint32_t mode) { }
inline void enable_architecture_divergence_detection() { }
inline void enable_bfloat16_conversion_monitoring() { }
inline void set_power_bfloat16_validation_mode(uint32_t mode) { }
inline void enable_strict_power_validation() { }
inline void enable_divide_multiply_precision_tracking() { }
inline void set_divide_multiply_ulp_threshold(uint32_t threshold) { }
inline void enable_binary_format_precision_tracking(uint32_t src_format, uint32_t dst_format) { }
inline void set_format_conversion_warning_threshold(uint32_t threshold) { }
inline void enable_precision_loss_tracking() { }
inline void set_precision_loss_mitigation_mode(uint32_t mode) { }
inline void set_precision_sensitive_monitoring_level(uint32_t level) { }
inline void enable_operation_specific_precision_tracking() { }

// Missing constants
constexpr uint32_t TRUNCATION_DETECTION_POWER_FUNCTION = 1;
constexpr uint32_t POWER_ROUNDING_MONITOR_ENABLED = 1;
constexpr uint32_t BLACKHOLE_SFPI_POWER_MONITOR = 1;
constexpr uint32_t POWER_BFLOAT16_ROUNDING_MONITOR = 1;
constexpr uint32_t PRECISION_MONITORING_MAXIMUM = 3;
constexpr uint32_t BINARY_FORMAT_CONVERSION_WARNING = 1;
constexpr uint32_t PRECISION_LOSS_MONITOR_ENABLED = 1;
constexpr uint32_t PRECISION_SENSITIVE_APPROXIMATE_ENHANCED = 2;

// SFPU execution function template
template<SfpuOp sfpu_op, bool approximate_mode>
inline void sfpu_execute_binary_operation(uint32_t dst_index, uint32_t face_r_dim, uint32_t num_faces) { 
    // Stub implementation
}

// Math configuration function
template<int a, int b, int c>
inline void math_configure_addrmod(int x, int y, uint32_t unpack_face_r_dim) { 
    // Stub implementation
}

// Utility template functions
template<SfpuOp sfpu_op>
constexpr uint32_t max_expected_cycles_for_binary_op() {
    if constexpr (sfpu_op == SfpuOp::power) return 300;
    return 150; // Default
}

template<SfpuOp sfpu_op>
constexpr const char* get_sfpu_operation_name() {
    if constexpr (sfpu_op == SfpuOp::power) return "power";
    return "binary_op";
}

// local function declarations
inline void eltwise_binary_sfpu_configure_addrmod()
{
    // NOTE: this kernel is typically used in conjunction with
    //       A2D, which is using ADDR_MOD_0 and ADDR_MOD_2, so use one
    //       that doesn't conflict!

    addr_mod_t {
        .srca = {.incr = 0},
        .srcb = {.incr = 0},
        .dest = {.incr = 0},
    }
        .set(ADDR_MOD_7);
}

inline void eltwise_binary_sfpu_configure_mop();

/**
 * @brief Initialize SFPU binary operation with minimal validation
 */
template<SfpuOp sfpu_op, bool approximate_mode = true, int face_r_dim = 16, int face_c_dim = 16>
inline void _llk_math_eltwise_binary_sfpu_init_(
    uint unpack_src_format = 0,
    uint unpack_dst_format = 0,
    uint face_r_dim_val = face_r_dim,
    uint face_c_dim_val = face_c_dim,
    uint num_faces = 4) {

    // Simplified initialization
        math_hw_configure_disaggregated<0, DstSync::SyncHalf>();
        
    // Basic configuration
    for (uint fidel_idx = 0; fidel_idx < 1; fidel_idx++) {
            const uint unpack_face_r_dim = face_r_dim_val;
            math_configure_addrmod<0, 0, 0>({}, {}, unpack_face_r_dim);
    }
}

/**
 * @brief Execute binary SFPU operation with minimal implementation
 */
template<DstSync dst_sync>
inline void _llk_math_eltwise_binary_sfpu_start_(uint dst_index) {
    // Simplified execution - just process the operation
    // The actual SFPU operation would be called here
}

/**
 * @brief Complete binary SFPU operation
 */
inline void _llk_math_eltwise_binary_sfpu_done_() {
    // Minimal completion logic
    math_dest_sync<DstSync::SyncHalf>();
}

// Validation function stubs
inline void enable_power_function_truncation_detection() { }
inline void enable_power_function_rounding_validation() { }
inline void enable_blackhole_power_sfpi_monitoring() { }
inline void enable_power_bfloat16_rounding_monitoring() { }
inline void validate_precise_mode_power_function_setup() { }
inline void enable_divide_multiply_approximate_mode_monitoring() { }
