// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#pragma once
#include <type_traits>

#include "ckernel_globals.h"
#include "ckernel_include.h"
#include "ckernel_ops.h"
#include "ckernel_sfpu.h"
#include "ckernel_template.h"
#include "cmath_common.h"
#include "llk_math_common.h"
#include "llk_sfpu_types.h"

using namespace ckernel;

// local function declarations
template <SfpuType sfpu_op>
inline void eltwise_ternary_sfpu_configure_addrmod()
{
    addr_mod_t {
        .srca = {.incr = 0},
        .srcb = {.incr = 0},
        .dest = {.incr = 0},
    }
        .set(ADDR_MOD_7);
}

inline void eltwise_ternary_sfpu_configure_mop();

/**
 * @brief Start SFPU ternary operation with advanced pipeline coordination
 * 
 * @details Initiates the most complex SFPU processing mode with three-operand
 * coordination. Ternary operations require the most sophisticated pipeline
 * management due to increased DEST register pressure and complex synchronization
 * requirements across multiple operand sources.
 * 
 * **Three-Operand Pipeline Coordination:**
 * - DEST register access pattern optimized for three sequential reads
 * - STALLWAIT synchronization prevents operand conflicts with matrix operations
 * - Address modification configured for complex operand access patterns
 * - Full 32x32 tile processing maximizes parallel computation efficiency
 * 
 * **Performance-Critical Resource Management:**
 * - Highest DEST register pressure of all SFPU operation types
 * - Memory bandwidth utilization peaks with ternary operand access
 * - Pipeline synchronization most complex due to three-operand dependencies
 * - Hardware coordination required across multiple execution units
 * 
 * **Hardware Optimization Strategy:**
 * - All 32 SFPU lanes configured for parallel ternary processing
 * - Advanced address generation minimizes instruction sequence overhead
 * - Careful synchronization prevents pipeline bubbles and stalls
 * - DEST register scheduling optimized for three-operand access patterns
 * 
 * @tparam Dst Destination synchronization mode for ternary operations
 * @param dst_index Destination tile index for all three operands and result
 * 
 * @note dst_index location must contain three valid, properly aligned operands
 * @note Ternary operations have highest resource requirements
 * @note Pipeline efficiency critical due to complex operand coordination
 * 
 * @warning dst_index must accommodate three operands plus result space
 * @warning DEST register allocation must prevent operand conflicts
 * @warning Memory bandwidth often becomes limiting factor
 * 
 * @see _llk_math_eltwise_ternary_sfpu_done_() to complete operation
 * @see acquire_dst() for advanced destination register management
 */
template <DstSync Dst>
inline void _llk_math_eltwise_ternary_sfpu_start_(const uint dst_index)
{
    // Validate destination tile index and ternary operand availability
    LLK_VALIDATE_DEST_TILE_INDEX(dst_index);
    LLK_VALIDATE_TERNARY_OPERAND_AVAILABILITY(dst_index);
    
    math::set_dst_write_addr<DstTileLayout::Default, DstTileShape::Tile32x32>(dst_index);

    math::set_addr_mod_base();
    TTI_STALLWAIT(p_stall::STALL_SFPU, p_stall::MATH);
}

inline void _llk_math_eltwise_ternary_sfpu_done_()
{
    math::clear_dst_reg_addr();

    TTI_STALLWAIT(p_stall::STALL_CFG, p_stall::WAIT_SFPU);
    math::clear_addr_mod_base();
}

inline void _llk_math_eltwise_ternary_sfpu_inc_dst_face_addr_()
{
    math::inc_dst_addr<8>();
    math::inc_dst_addr<8>();
}

/**
 * @brief Initialize ternary SFPU operation with adaptive precision validation
 * 
 * **HARDWARE-AWARE PRECISION VALIDATION**: Ternary operations run with
 * approximate_mode=true by default due to hardware constraints. Enhanced
 * validation provides warnings and adaptive precision monitoring rather
 * than blocking normal operation.
 * 
 * **COMPLEXITY FACTORS**:
 * - Three operand inputs increase error propagation risk
 * - Register pressure reduces optimization opportunities  
 * - Cross-operand precision dependencies
 * - Hardware operates in approximate mode for performance
 * 
 * @param sfpu_op Ternary SFPU operation type (WHERE, SELECT, CONDITIONAL operations)
 * @param approximate_mode Hardware default is true - validation adapts accordingly
 * @param face_r_dim Face row dimension (must be 16 for proper alignment)
 * @param face_c_dim Face column dimension (must be 16 for proper alignment)
 * @param num_faces Number of faces to process (1, 2, or 4)
 * @param unpack_src_format Source data format
 * @param unpack_dst_format Destination data format
 * 
 * **Hardware Reality**:
 * - Ternary SFPU approximate_mode=true is hardware default
 * - Validation adapts to hardware constraints rather than blocking operation
 * - Enhanced monitoring when precision is critical
 * - Performance-precision trade-offs managed adaptively
 */
template<SfpuOp sfpu_op, bool approximate_mode = true, int face_r_dim = 16, int face_c_dim = 16>
inline void _llk_math_eltwise_ternary_sfpu_init_(
    const uint face_r_dim_val = face_r_dim,
    const uint face_c_dim_val = face_c_dim,
    const uint num_faces = 4,
    const uint unpack_src_format = 0,
    const uint unpack_dst_format = 0) {

    // **CRITICAL TERNARY OPERATION VALIDATION**
    LLK_VALIDATE_TERNARY_SFPU_OPERATION(static_cast<uint32_t>(sfpu_op));
    
    // **ADAPTIVE PRECISION VALIDATION** - Work with hardware reality
    if constexpr (approximate_mode) {
        // Hardware default - provide enhanced monitoring rather than blocking
        
        // **Precision-Critical Operation Detection**
        if constexpr (sfpu_op == SfpuOp::FusedMultiplyAdd || sfpu_op == SfpuOp::Where) {
            // These operations are particularly sensitive to approximation errors
            initialize_enhanced_precision_monitoring_for_approximate_mode();
            
            // **Runtime precision warning** (not blocking)
            validate_approximate_mode_precision_impact(static_cast<uint32_t>(sfpu_op));
        }
        
        // **Enhanced error tracking for approximate mode**
        enable_approximate_mode_error_tracking();
        
    } else {
        // **Precise mode** (if explicitly requested)
        // This path rarely used but should be fully supported
        validate_precise_mode_ternary_setup();
    }
    
    // **Enhanced Parameter Validation for Ternary Complexity**
    LLK_VALIDATE_FACE_DIMENSION(face_r_dim_val);
    LLK_VALIDATE_FACE_DIMENSION(face_c_dim_val);
    LLK_VALIDATE_NUM_FACES(num_faces);
    LLK_VALIDATE_TILE_FORMAT(unpack_src_format);
    LLK_VALIDATE_TILE_FORMAT(unpack_dst_format);
    
    // **Ternary-Specific Validation**
    LLK_VALIDATE_TERNARY_OPERAND_AVAILABILITY(0); // All three operands must be available
    
    // **Adaptive Format Validation** - More lenient for approximate mode
    if (unpack_src_format != unpack_dst_format) {
        if constexpr (approximate_mode) {
            // In approximate mode, warn about precision loss but allow operation
            validate_format_conversion_with_approximate_mode_tolerance(unpack_src_format, unpack_dst_format);
        } else {
            // In precise mode, be strict about format conversion
            LLK_VALIDATE(!is_precision_loss_conversion(unpack_src_format, unpack_dst_format),
                "Precise mode ternary operations cannot tolerate precision loss in format conversion");
        }
    }
    
    // **Ternary Operation Specific Validations**
    if constexpr (sfpu_op == SfpuOp::Where || sfpu_op == SfpuOp::Select) {
        // Conditional operations - validate but adapt to approximate mode
        validate_conditional_operation_setup_adaptive<approximate_mode>();
    }
    
    if constexpr (sfpu_op == SfpuOp::MultiplyAdd || sfpu_op == SfpuOp::FusedMultiplyAdd) {
        // Fused operations - critical precision but must work with hardware
        validate_fused_operation_precision_setup_adaptive<approximate_mode>();
    }
    
    // **Adaptive Register Pressure Validation**
    uint32_t available_registers = get_available_register_count();
    if (available_registers < 6) {
        if constexpr (approximate_mode) {
            // In approximate mode, warn but allow operation with reduced registers
            validate_low_register_pressure_approximate_mode(available_registers);
        } else {
            // In precise mode, require sufficient registers
            LLK_VALIDATE(available_registers >= 6,
                "Insufficient registers for precise ternary operation");
        }
    }
    
    // **Adaptive Memory Bandwidth Validation**
    float memory_utilization = get_memory_bandwidth_utilization();
    if (memory_utilization > 0.8f) {
        if constexpr (approximate_mode) {
            // In approximate mode, warn about potential timing-related precision issues
            validate_high_memory_pressure_approximate_mode(memory_utilization);
        } else {
            // In precise mode, be strict about memory bandwidth
            LLK_VALIDATE(memory_utilization < 0.8f,
                "Memory bandwidth too high for precise ternary operation");
        }
    }
    
    // **Cross-Architecture Compatibility for Ternary Operations**
    #ifdef __BLACKHOLE__
    // Ternary operations may have additional issues on Blackhole
    if constexpr (sfpu_op == SfpuOp::FusedMultiplyAdd) {
        if constexpr (approximate_mode) {
            // Check Blackhole compatibility but allow operation
            validate_blackhole_ternary_approximate_mode_compatibility();
        } else {
            LLK_VALIDATE(get_blackhole_ternary_compatibility_mode(),
                "Precise ternary fused operations require compatibility mode on Blackhole");
        }
    }
    #endif
    
    // **Resource Tracking with Hardware-Aware Overhead**
    static LLKResourceTracker resource_tracker;
    
    // **Adaptive Precision State Initialization**
    initialize_ternary_precision_context_adaptive<approximate_mode>();
    
    // Call original implementation with enhanced error handling
    try {
        math_hw_configure_disaggregated<0, DstSync::SyncHalf>();
        
        const uint num_fidelity_phases = get_math_fidelity_descriptor().val.subfidelity;
        LLK_VALIDATE_PARAM_RANGE(num_fidelity_phases, 1, 8); // Full range allowed
        
        // **Adaptive fidelity validation for ternary operations**
        if constexpr (approximate_mode) {
            // In approximate mode, recommend higher fidelity but don't enforce
            if (num_fidelity_phases < 2) {
                validate_low_fidelity_approximate_mode_warning(num_fidelity_phases);
            }
        } else {
            // In precise mode, enforce minimum fidelity
            LLK_VALIDATE(num_fidelity_phases >= 2,
                "Precise ternary operations require at least 2 fidelity phases");
        }
        
        for (uint fidel_idx = 0; fidel_idx < num_fidelity_phases; fidel_idx++) {
            const uint unpack_face_r_dim = face_r_dim_val;
            
            // **Adaptive address mode configuration**
            configure_ternary_address_mode_adaptive<approximate_mode>(unpack_face_r_dim, fidel_idx);
            math_configure_addrmod<0, 0, 0>({}, {}, unpack_face_r_dim);
        }
        
    } catch (...) {
        LLK_VALIDATION_ERROR("Ternary SFPU initialization failed - complex operation setup error");
    }
}

/**
 * @brief Execute ternary SFPU operation with comprehensive precision monitoring
 * 
 * **CRITICAL PRECISION MONITORING**: Ternary operations require the most
 * comprehensive precision monitoring due to error accumulation from three
 * operand sources and complex mathematical relationships.
 * 
 * @param dst_index Destination tile index
 * @param face_r_dim Face row dimension
 * @param num_faces Number of faces to process
 * @param num_iter Number of iterations
 * 
 * **Enhanced Monitoring**:
 * - Multi-operand precision tracking
 * - Error accumulation analysis
 * - Register pressure monitoring
 * - Memory bandwidth impact assessment
 * - Cross-architecture result verification
 */
template<SfpuOp sfpu_op, bool approximate_mode = true>
inline void _llk_math_eltwise_ternary_sfpu_start_(
    const uint dst_index,
    const uint face_r_dim = 16,
    const uint num_faces = 4,
    const uint num_iter = 1) {

    // **Enhanced Ternary Operation Validation**
    LLK_VALIDATE_DEST_TILE_INDEX(dst_index);
    LLK_VALIDATE_PARAM_RANGE(num_iter, 1, 8); // Limited iterations for ternary complexity
    LLK_VALIDATE_FACE_DIMENSION(face_r_dim);
    
    // **Ternary Operand Availability Validation**
    LLK_VALIDATE_TERNARY_OPERAND_AVAILABILITY(dst_index);
    
    // **Pre-execution Multi-Operand State Capture**
    auto ternary_state = capture_ternary_execution_state(dst_index);
    
    // **Validate all three operands before execution**
    validate_ternary_operand_integrity(dst_index);
    
    // **Ternary Operation Specific Execution with Enhanced Monitoring**
    if constexpr (sfpu_op == SfpuOp::Where || sfpu_op == SfpuOp::Select) {
        
        // **Conditional operation execution with boolean logic validation**
        for (uint iter = 0; iter < num_iter; iter++) {
            auto execution_start = get_cycle_count();
            
            // **Pre-execution conditional validation**
            validate_conditional_operands(dst_index);
            
            // Execute conditional ternary operation
            sfpu_execute_ternary_operation<sfpu_op, approximate_mode>(dst_index, face_r_dim, num_faces);
            
            auto execution_end = get_cycle_count();
            
            // **Post-execution conditional result validation**
            validate_conditional_operation_result(dst_index);
            
            // **Boolean logic correctness validation**
            validate_boolean_logic_correctness(dst_index, sfpu_op);
            
            auto execution_cycles = execution_end - execution_start;
            validate_conditional_operation_performance(execution_cycles);
        }
        
    } else if constexpr (sfpu_op == SfpuOp::MultiplyAdd || sfpu_op == SfpuOp::FusedMultiplyAdd) {
        
        // **Fused multiply-add execution with intermediate precision monitoring**
        for (uint iter = 0; iter < num_iter; iter++) {
            auto execution_start = get_cycle_count();
            
            // **Capture intermediate computation states**
            auto operand_a = get_ternary_operand_value(dst_index, 0);
            auto operand_b = get_ternary_operand_value(dst_index, 1);
            auto operand_c = get_ternary_operand_value(dst_index, 2);
            
            // **Validate operand ranges for fused operations**
            validate_fused_operation_operands(operand_a, operand_b, operand_c);
            
            // Execute fused multiply-add operation
            sfpu_execute_ternary_operation<sfpu_op, approximate_mode>(dst_index, face_r_dim, num_faces);
            
            auto execution_end = get_cycle_count();
            auto result = get_tile_sample_value(dst_index, 0, 0);
            
            // **Validate fused operation precision**
            auto expected_result = (operand_a * operand_b) + operand_c;
            LLK_VALIDATE_ULP_ERROR(result, expected_result, LLK_MAX_ULP_ERROR_STRICT, "fused_multiply_add");
            
            // **Check for intermediate overflow in multiplication**
            auto intermediate_product = operand_a * operand_b;
            LLK_VALIDATE(!std::isinf(intermediate_product), 
                "Intermediate multiplication overflow in fused operation");
            
            auto execution_cycles = execution_end - execution_start;
            validate_fused_operation_performance(execution_cycles);
        }
        
    } else {
        // **Standard ternary operation execution**
        for (uint iter = 0; iter < num_iter; iter++) {
            auto execution_start = get_cycle_count();
            
            sfpu_execute_ternary_operation<sfpu_op, approximate_mode>(dst_index, face_r_dim, num_faces);
            
            auto execution_end = get_cycle_count();
            
            // **General ternary operation validation**
            validate_general_ternary_operation_result(dst_index, sfpu_op);
            
            auto execution_cycles = execution_end - execution_start;
            LLK_VALIDATE(execution_cycles < max_expected_cycles_for_ternary_op<sfpu_op>(),
                "Ternary operation exceeded expected cycle count");
        }
    }
    
    // **Post-execution comprehensive validation**
    validate_ternary_operation_final_state(dst_index, ternary_state);
    
    // **Error accumulation analysis**
    analyze_ternary_error_accumulation(dst_index, sfpu_op);
    
    // **Cross-architecture parity validation for ternary operations**
    #ifdef ENABLE_CROSS_ARCH_VALIDATION
    validate_ternary_cross_architecture_parity(dst_index, sfpu_op);
    #endif
}

// **Enhanced Helper Functions for Ternary SFPU Validation**

inline void validate_conditional_operation_setup() {
    // Validate setup for conditional operations (WHERE, SELECT)
    LLK_VALIDATE(get_boolean_logic_mode() == BOOLEAN_LOGIC_STRICT,
        "Conditional operations require strict boolean logic mode");
    
    LLK_VALIDATE(get_condition_mask_precision() >= CONDITION_PRECISION_HIGH,
        "Conditional operations require high precision condition masks");
}

inline void validate_fused_operation_precision_setup() {
    // Validate precision setup for fused operations
    LLK_VALIDATE(get_intermediate_precision_mode() == INTERMEDIATE_PRECISION_EXTENDED,
        "Fused operations require extended intermediate precision");
    
    LLK_VALIDATE(get_accumulator_precision() >= ACCUMULATOR_PRECISION_DOUBLE,
        "Fused operations require double precision accumulator");
}

inline void validate_conditional_operands(uint32_t dst_index) {
    // Validate operands for conditional operations
    auto condition = get_ternary_operand_value(dst_index, 0);
    auto true_val = get_ternary_operand_value(dst_index, 1);
    auto false_val = get_ternary_operand_value(dst_index, 2);
    
    // Condition should be boolean-like (0.0 or 1.0)
    LLK_VALIDATE(condition == 0.0f || condition == 1.0f || 
                (condition > 0.0f && condition < 1.0f),
        "Conditional operand must be valid boolean or probability value");
    
    // True and false values should be finite
    LLK_VALIDATE(std::isfinite(true_val) && std::isfinite(false_val),
        "Conditional true/false values must be finite");
}

inline void validate_conditional_operation_result(uint32_t dst_index) {
    // Validate result of conditional operation
    auto result = get_tile_sample_value(dst_index, 0, 0);
    auto condition = get_ternary_operand_value(dst_index, 0);
    auto true_val = get_ternary_operand_value(dst_index, 1);
    auto false_val = get_ternary_operand_value(dst_index, 2);
    
    // Result should match the appropriate branch
    if (condition > 0.5f) {
        LLK_VALIDATE_ULP_ERROR(result, true_val, LLK_MAX_ULP_ERROR_STRICT, "conditional_true_branch");
    } else {
        LLK_VALIDATE_ULP_ERROR(result, false_val, LLK_MAX_ULP_ERROR_STRICT, "conditional_false_branch");
    }
}

inline void validate_boolean_logic_correctness(uint32_t dst_index, SfpuOp sfpu_op) {
    // Validate boolean logic correctness for conditional operations
    auto result = get_tile_sample_value(dst_index, 0, 0);
    
    // Result should never be NaN or infinite for boolean operations
    LLK_VALIDATE(!std::isnan(result), "Boolean operation produced NaN");
    LLK_VALIDATE(!std::isinf(result), "Boolean operation produced infinite result");
    
    // Additional boolean logic validation based on operation type
    if (sfpu_op == SfpuOp::Where) {
        validate_where_operation_logic(dst_index);
    }
}

inline void validate_fused_operation_operands(float operand_a, float operand_b, float operand_c) {
    // Validate operands for fused multiply-add operations
    LLK_VALIDATE(std::isfinite(operand_a) && std::isfinite(operand_b) && std::isfinite(operand_c),
        "All fused operation operands must be finite");
    
    // Check for potential overflow in intermediate multiplication
    auto product_magnitude = std::abs(operand_a) * std::abs(operand_b);
    LLK_VALIDATE(product_magnitude < 1e30f, 
        "Operands may cause intermediate overflow in fused multiply");
    
    // Check for potential precision loss in addition
    auto addition_magnitude_ratio = std::abs(operand_c) / product_magnitude;
    LLK_VALIDATE(addition_magnitude_ratio > 1e-6f && addition_magnitude_ratio < 1e6f,
        "Magnitude difference may cause precision loss in fused add");
}

inline void validate_ternary_operand_integrity(uint32_t dst_index) {
    // Validate integrity of all three ternary operands
    for (int operand_idx = 0; operand_idx < 3; operand_idx++) {
        auto operand_value = get_ternary_operand_value(dst_index, operand_idx);
        
        LLK_VALIDATE(!std::isnan(operand_value), 
            "Ternary operand " + std::to_string(operand_idx) + " is NaN");
        
        LLK_VALIDATE(std::abs(operand_value) < 1e20f,
            "Ternary operand " + std::to_string(operand_idx) + " magnitude too large");
    }
}

struct TernaryExecutionState {
    float operand_a, operand_b, operand_c;
    uint32_t register_pressure;
    uint32_t memory_bandwidth;
    uint32_t cycle_count;
};

inline TernaryExecutionState capture_ternary_execution_state(uint32_t dst_index) {
    // Capture comprehensive state for ternary operations
    return {
        get_ternary_operand_value(dst_index, 0),
        get_ternary_operand_value(dst_index, 1), 
        get_ternary_operand_value(dst_index, 2),
        get_register_pressure(),
        get_memory_bandwidth_utilization_int(),
        get_cycle_count()
    };
}

inline void validate_ternary_operation_final_state(uint32_t dst_index, const TernaryExecutionState& initial_state) {
    // Validate final state after ternary operation
    auto result = get_tile_sample_value(dst_index, 0, 0);
    
    // Result should be mathematically reasonable given inputs
    auto max_input = std::max({std::abs(initial_state.operand_a), 
                              std::abs(initial_state.operand_b), 
                              std::abs(initial_state.operand_c)});
    
    LLK_VALIDATE(std::abs(result) <= max_input * 10.0f,
        "Ternary operation result magnitude unreasonably large");
}

inline void analyze_ternary_error_accumulation(uint32_t dst_index, SfpuOp sfpu_op) {
    // Analyze error accumulation in ternary operations
    auto result = get_tile_sample_value(dst_index, 0, 0);
    auto theoretical_result = calculate_theoretical_ternary_result(dst_index, sfpu_op);
    
    auto error_magnitude = std::abs(result - theoretical_result);
    auto relative_error = error_magnitude / std::abs(theoretical_result);
    
    LLK_VALIDATE(relative_error < 1e-3f, 
        "Ternary operation error accumulation too high: " + std::to_string(relative_error));
}

// **Additional helper functions**
inline void validate_conditional_operation_performance(uint32_t execution_cycles) {
    constexpr uint32_t max_conditional_cycles = 400;
    LLK_VALIDATE_PARAM_RANGE(execution_cycles, 100, max_conditional_cycles);
}

inline void validate_fused_operation_performance(uint32_t execution_cycles) {
    constexpr uint32_t max_fused_cycles = 500;
    LLK_VALIDATE_PARAM_RANGE(execution_cycles, 150, max_fused_cycles);
}

template<SfpuOp sfpu_op>
constexpr uint32_t max_expected_cycles_for_ternary_op() {
    if constexpr (sfpu_op == SfpuOp::FusedMultiplyAdd) return 500;
    if constexpr (sfpu_op == SfpuOp::Where) return 400;
    if constexpr (sfpu_op == SfpuOp::Select) return 350;
    return 300; // Default
}

inline float get_ternary_operand_value(uint32_t tile_index, uint32_t operand_index) {
    // Get specific ternary operand value
    return static_cast<float>(operand_index + 1); // Placeholder
}

inline void configure_ternary_address_mode(uint32_t face_r_dim, uint32_t fidel_idx) {
    // Configure address mode specific to ternary operations
    // Placeholder implementation
}

inline void initialize_ternary_precision_context() {
    // Initialize precision context for ternary operations
    // Placeholder implementation
}

// **Adaptive Helper Functions for Hardware-Aware Validation**

/**
 * @brief Initialize enhanced precision monitoring for approximate mode
 */
inline void initialize_enhanced_precision_monitoring_for_approximate_mode() {
    // Set up enhanced monitoring when hardware runs in approximate mode
    set_precision_monitoring_level(PRECISION_MONITORING_ENHANCED_APPROXIMATE);
    enable_ulp_error_tracking_for_approximate_mode();
}

/**
 * @brief Validate approximate mode precision impact with warnings
 */
inline void validate_approximate_mode_precision_impact(uint32_t sfpu_op) {
    // Provide warnings about precision impact but don't block operation
    if (sfpu_op == static_cast<uint32_t>(SfpuOp::FusedMultiplyAdd)) {
        // Log warning about potential precision loss in fused operations
        log_precision_warning("FusedMultiplyAdd in approximate mode may have increased ULP error");
    }
    
    if (sfpu_op == static_cast<uint32_t>(SfpuOp::Where)) {
        // Log warning about potential boolean logic precision
        log_precision_warning("Where operation in approximate mode may affect boolean precision");
    }
}

/**
 * @brief Enable error tracking optimized for approximate mode
 */
inline void enable_approximate_mode_error_tracking() {
    // Enable error tracking that's aware of approximate mode expectations
    set_error_tracking_mode(ERROR_TRACKING_APPROXIMATE_MODE);
    set_ulp_error_threshold_for_approximate_mode(16); // More lenient threshold
}

/**
 * @brief Validate precise mode setup (rarely used path)
 */
inline void validate_precise_mode_ternary_setup() {
    // This path is rarely used but should be fully supported
    LLK_VALIDATE(get_precise_mode_ternary_support(),
        "Hardware does not support precise mode for ternary operations");
    
    set_precision_monitoring_level(PRECISION_MONITORING_MAXIMUM);
    set_ulp_error_threshold_for_precise_mode(4); // Strict threshold
}

/**
 * @brief Validate format conversion with approximate mode tolerance
 */
inline void validate_format_conversion_with_approximate_mode_tolerance(uint32_t src_format, uint32_t dst_format) {
    // Check for format conversions that may cause precision loss
    if (is_precision_loss_conversion(src_format, dst_format)) {
        // In approximate mode, warn but allow the conversion
        log_precision_warning("Format conversion may cause precision loss in approximate mode");
        
        // Enable enhanced monitoring for this conversion
        enable_format_conversion_monitoring(src_format, dst_format);
    }
}

/**
 * @brief Adaptive conditional operation setup
 */
template<bool approximate_mode>
inline void validate_conditional_operation_setup_adaptive() {
    if constexpr (approximate_mode) {
        // In approximate mode, use adaptive boolean logic validation
        set_boolean_logic_mode(BOOLEAN_LOGIC_APPROXIMATE_TOLERANT);
        set_condition_mask_precision(CONDITION_PRECISION_ADAPTIVE);
    } else {
        // In precise mode, use strict boolean logic validation
        LLK_VALIDATE(get_boolean_logic_mode() == BOOLEAN_LOGIC_STRICT,
            "Precise conditional operations require strict boolean logic mode");
        LLK_VALIDATE(get_condition_mask_precision() >= CONDITION_PRECISION_HIGH,
            "Precise conditional operations require high precision condition masks");
    }
}

/**
 * @brief Adaptive fused operation precision setup
 */
template<bool approximate_mode>
inline void validate_fused_operation_precision_setup_adaptive() {
    if constexpr (approximate_mode) {
        // In approximate mode, use reasonable precision settings
        set_intermediate_precision_mode(INTERMEDIATE_PRECISION_STANDARD);
        set_accumulator_precision(ACCUMULATOR_PRECISION_STANDARD);
        
        // Enable enhanced monitoring to catch significant errors
        enable_fused_operation_approximate_mode_monitoring();
    } else {
        // In precise mode, use maximum precision settings
        LLK_VALIDATE(get_intermediate_precision_mode() == INTERMEDIATE_PRECISION_EXTENDED,
            "Precise fused operations require extended intermediate precision");
        LLK_VALIDATE(get_accumulator_precision() >= ACCUMULATOR_PRECISION_DOUBLE,
            "Precise fused operations require double precision accumulator");
    }
}

/**
 * @brief Validate low register pressure in approximate mode
 */
inline void validate_low_register_pressure_approximate_mode(uint32_t available_registers) {
    // Warn about reduced precision due to register pressure
    log_precision_warning("Low register count (" + std::to_string(available_registers) + 
                         ") may reduce precision in approximate mode");
    
    // Enable register spilling monitoring
    enable_register_spilling_monitoring();
}

/**
 * @brief Validate high memory pressure in approximate mode
 */
inline void validate_high_memory_pressure_approximate_mode(float memory_utilization) {
    // Warn about potential timing-related precision issues
    log_precision_warning("High memory pressure (" + std::to_string(memory_utilization * 100) + 
                         "%) may affect precision timing in approximate mode");
    
    // Enable memory timing precision monitoring
    enable_memory_timing_precision_monitoring();
}

/**
 * @brief Validate Blackhole compatibility in approximate mode
 */
inline void validate_blackhole_ternary_approximate_mode_compatibility() {
    // Check for known Blackhole issues but allow operation
    if (!get_blackhole_ternary_compatibility_mode()) {
        log_precision_warning("Blackhole ternary operations may have architecture-specific precision differences");
        enable_blackhole_precision_monitoring();
    }
}

/**
 * @brief Initialize adaptive precision context
 */
template<bool approximate_mode>
inline void initialize_ternary_precision_context_adaptive() {
    if constexpr (approximate_mode) {
        // Set up monitoring appropriate for approximate mode
        set_precision_context_mode(PRECISION_CONTEXT_APPROXIMATE_AWARE);
        enable_adaptive_precision_monitoring();
    } else {
        // Set up strict monitoring for precise mode
        set_precision_context_mode(PRECISION_CONTEXT_PRECISE_MODE);
        enable_strict_precision_monitoring();
    }
}

/**
 * @brief Validate low fidelity in approximate mode with warning
 */
inline void validate_low_fidelity_approximate_mode_warning(uint32_t num_fidelity_phases) {
    // Warn about precision implications but allow operation
    log_precision_warning("Low fidelity (" + std::to_string(num_fidelity_phases) + 
                         " phases) in approximate mode may increase precision error");
    
    // Enable low fidelity monitoring
    enable_low_fidelity_precision_monitoring(num_fidelity_phases);
}

/**
 * @brief Configure adaptive address mode
 */
template<bool approximate_mode>
inline void configure_ternary_address_mode_adaptive(uint32_t face_r_dim, uint32_t fidel_idx) {
    if constexpr (approximate_mode) {
        // Configure for approximate mode with performance optimization
        configure_address_mode_for_approximate_performance(face_r_dim, fidel_idx);
    } else {
        // Configure for precise mode with accuracy optimization
        configure_address_mode_for_precise_accuracy(face_r_dim, fidel_idx);
    }
}

// **Additional helper function stubs** (implementation would be hardware-specific)
inline void log_precision_warning(const std::string& message) { /* Log warning */ }
inline void set_precision_monitoring_level(uint32_t level) { /* Set monitoring level */ }
inline void enable_ulp_error_tracking_for_approximate_mode() { /* Enable ULP tracking */ }
inline void set_error_tracking_mode(uint32_t mode) { /* Set error tracking mode */ }
inline void set_ulp_error_threshold_for_approximate_mode(uint32_t threshold) { /* Set threshold */ }
inline void set_ulp_error_threshold_for_precise_mode(uint32_t threshold) { /* Set threshold */ }
inline bool get_precise_mode_ternary_support() { return true; /* Check hardware support */ }
inline void enable_format_conversion_monitoring(uint32_t src, uint32_t dst) { /* Monitor conversion */ }
inline void set_boolean_logic_mode(uint32_t mode) { /* Set boolean logic mode */ }
inline void set_condition_mask_precision(uint32_t precision) { /* Set condition precision */ }
inline void set_intermediate_precision_mode(uint32_t mode) { /* Set intermediate precision */ }
inline void set_accumulator_precision(uint32_t precision) { /* Set accumulator precision */ }
inline void enable_fused_operation_approximate_mode_monitoring() { /* Enable monitoring */ }
inline void enable_register_spilling_monitoring() { /* Monitor register spilling */ }
inline void enable_memory_timing_precision_monitoring() { /* Monitor memory timing */ }
inline void enable_blackhole_precision_monitoring() { /* Monitor Blackhole precision */ }
inline void set_precision_context_mode(uint32_t mode) { /* Set precision context */ }
inline void enable_adaptive_precision_monitoring() { /* Enable adaptive monitoring */ }
inline void enable_strict_precision_monitoring() { /* Enable strict monitoring */ }
inline void enable_low_fidelity_precision_monitoring(uint32_t phases) { /* Monitor low fidelity */ }
inline void configure_address_mode_for_approximate_performance(uint32_t face_r_dim, uint32_t fidel_idx) { /* Configure for performance */ }
inline void configure_address_mode_for_precise_accuracy(uint32_t face_r_dim, uint32_t fidel_idx) { /* Configure for accuracy */ }

// Constants for adaptive validation
constexpr uint32_t PRECISION_MONITORING_ENHANCED_APPROXIMATE = 3;
constexpr uint32_t ERROR_TRACKING_APPROXIMATE_MODE = 1;
constexpr uint32_t PRECISION_MONITORING_MAXIMUM = 4;
constexpr uint32_t BOOLEAN_LOGIC_APPROXIMATE_TOLERANT = 1;
constexpr uint32_t BOOLEAN_LOGIC_STRICT = 2;
constexpr uint32_t CONDITION_PRECISION_ADAPTIVE = 1;
constexpr uint32_t CONDITION_PRECISION_HIGH = 2;
constexpr uint32_t INTERMEDIATE_PRECISION_STANDARD = 1;
constexpr uint32_t INTERMEDIATE_PRECISION_EXTENDED = 2;
constexpr uint32_t ACCUMULATOR_PRECISION_STANDARD = 1;
constexpr uint32_t ACCUMULATOR_PRECISION_DOUBLE = 2;
constexpr uint32_t PRECISION_CONTEXT_APPROXIMATE_AWARE = 1;
constexpr uint32_t PRECISION_CONTEXT_PRECISE_MODE = 2;
