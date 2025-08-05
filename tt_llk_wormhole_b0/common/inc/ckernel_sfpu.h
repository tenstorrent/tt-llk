// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

/**
 * @file ckernel_sfpu.h
 * @brief SFPU Function Collection for Wormhole B0 Tensix
 *
 * This header provides a comprehensive collection of all SFPU (Special Function
 * Processing Unit) functionality for Wormhole B0 Tensix cores. It serves as a
 * convenient single-include header that provides access to all mathematical
 * functions, activations, and special operations available on the SFPU.
 *
 * @author Tenstorrent AI ULC
 * @version 1.0
 * @date 2025
 *
 * # SFPU Architecture
 *
 * The SFPU is a specialized processing unit within Tensix that provides:
 * - **Mathematical Functions**: exp, log, sqrt, power, trigonometric functions
 * - **Activation Functions**: ReLU, GELU, Sigmoid, Tanh, ELU, and variants
 * - **Data Type Operations**: Casting, conversion, quantization
 * - **Utility Functions**: Fill, comparison, bitwise operations
 * - **Advanced Operations**: TopK, cumulative sum, dropout, reshuffle
 *
 * # Function Categories
 *
 * ## Core Mathematical Functions
 * - **Exponential/Logarithmic**: exp, exp2, log, power, sqrt, recip
 * - **Trigonometric**: sin, cos, tan and their derivatives
 * - **Arithmetic**: abs, sign, negative, square, cube
 *
 * ## Neural Network Activations
 * - **Linear**: ReLU, clamp, hardtanh
 * - **Sigmoid Family**: sigmoid, tanh, silu, hardsigmoid
 * - **Advanced**: GELU, ELU, dropout
 *
 * ## Data Processing
 * - **Type Conversion**: fp32↔fp16, integer casting, quantization
 * - **Bitwise Operations**: shifts, logical operations
 * - **Comparison**: max, min, comparison operations
 * - **Utility**: fill, where, conditional operations
 *
 * # Usage Pattern
 *
 * ```cpp
 * #include "ckernel_sfpu.h"
 *
 * // Example: Apply GELU activation
 * calculate_gelu<true>(ITERATIONS);
 *
 * // Example: Apply sigmoid activation
 * calculate_sigmoid<true>(ITERATIONS);
 *
 * // Example: Exponential function
 * calculate_exp<true>(ITERATIONS);
 * ```
 *
 * # Performance Considerations
 *
 * - **Approximation Modes**: Many functions offer fast approximation modes
 * - **Template Parameters**: Compile-time optimization for different modes
 * - **Iteration Control**: Efficient loop handling for batch processing
 * - **Register Management**: Optimized for SFPU register usage patterns
 *
 * @note This header includes the entire SFPU function library
 * @note For specific functions, individual headers can be included directly
 * @note All SFPU functions operate on data in the destination register space
 *
 * @see sfpu/ subdirectory for individual function implementations
 * @see ckernel.h for core kernel infrastructure
 * @see sfpi.h for low-level SFPU instruction interface
 */

#pragma once

#include <limits>

#include "ckernel.h"
#include "ckernel_defs.h"
#include "ckernel_globals.h"
#include "sfpi.h"

// Core Mathematical Functions
#include "sfpu/ckernel_sfpu_abs.h"          ///< Absolute value operations
#include "sfpu/ckernel_sfpu_exp.h"          ///< Exponential functions (e^x)
#include "sfpu/ckernel_sfpu_exp2.h"         ///< Base-2 exponential functions (2^x)
#include "sfpu/ckernel_sfpu_log.h"          ///< Logarithmic functions
#include "sfpu/ckernel_sfpu_negative.h"     ///< Negation operations (-x)
#include "sfpu/ckernel_sfpu_power.h"        ///< Power operations (x^y)
#include "sfpu/ckernel_sfpu_recip.h"        ///< Reciprocal operations (1/x)
#include "sfpu/ckernel_sfpu_sign.h"         ///< Sign extraction operations
#include "sfpu/ckernel_sfpu_sqrt.h"         ///< Square root operations
#include "sfpu/ckernel_sfpu_square.h"       ///< Square operations (x^2)
#include "sfpu/ckernel_sfpu_trigonometry.h" ///< Trigonometric functions (sin, cos, tan)

// Neural Network Activation Functions
#include "sfpu/ckernel_sfpu_activations.h"     ///< General activation function utilities
#include "sfpu/ckernel_sfpu_clamp.h"           ///< Value clamping operations
#include "sfpu/ckernel_sfpu_elu.h"             ///< Exponential Linear Unit (ELU)
#include "sfpu/ckernel_sfpu_gelu.h"            ///< Gaussian Error Linear Unit (GELU)
#include "sfpu/ckernel_sfpu_hardtanh.h"        ///< Hard Tanh (clamped linear)
#include "sfpu/ckernel_sfpu_relu.h"            ///< Rectified Linear Unit (ReLU)
#include "sfpu/ckernel_sfpu_sigmoid.h"         ///< Sigmoid activation
#include "sfpu/ckernel_sfpu_silu.h"            ///< Sigmoid Linear Unit (SiLU/Swish)
#include "sfpu/ckernel_sfpu_tanh.h"            ///< Hyperbolic tangent activation
#include "sfpu/ckernel_sfpu_tanh_derivative.h" ///< Tanh derivative for backpropagation

// Integer and Bitwise Operations
#include "sfpu/ckernel_sfpu_add_int.h"        ///< Integer addition operations
#include "sfpu/ckernel_sfpu_binary_bitwise.h" ///< Bitwise logical operations
#include "sfpu/ckernel_sfpu_max_int32.h"      ///< 32-bit integer maximum operations
#include "sfpu/ckernel_sfpu_mul_int.h"        ///< Integer multiplication operations
#include "sfpu/ckernel_sfpu_shift.h"          ///< Bit shift operations
#include "sfpu/ckernel_sfpu_sub_int.h"        ///< Integer subtraction operations

// Data Type and Conversion Operations
#include "sfpu/ckernel_sfpu_cast_fp32_to_fp16a.h" ///< Specific FP32→FP16 conversion
#include "sfpu/ckernel_sfpu_converter.h"          ///< Format conversion utilities
#include "sfpu/ckernel_sfpu_quant.h"              ///< Quantization operations
#include "sfpu/ckernel_sfpu_typecast.h"           ///< Type casting between data formats

// Comparison and Selection Operations
#include "sfpu/ckernel_sfpu_binary.h" ///< Binary operations between operands
#include "sfpu/ckernel_sfpu_comp.h"   ///< Comparison operations
#include "sfpu/ckernel_sfpu_max.h"    ///< Maximum value operations
#include "sfpu/ckernel_sfpu_where.h"  ///< Conditional selection (where/select)

// Advanced and Specialized Operations
#include "sfpu/ckernel_sfpu_cumsum.h"         ///< Cumulative sum operations
#include "sfpu/ckernel_sfpu_dropout.h"        ///< Dropout regularization
#include "sfpu/ckernel_sfpu_reshuffle_rows.h" ///< Row reshuffling operations
#include "sfpu/ckernel_sfpu_topk.h"           ///< Top-K selection operations

// Utility and Support Operations
#include "sfpu/ckernel_sfpu_fill.h"         ///< Fill operations with constants
#include "sfpu/ckernel_sfpu_is_fp16_zero.h" ///< FP16 zero detection utilities
#include "sfpu/ckernel_sfpu_load_config.h"  ///< Configuration loading utilities
#include "sfpu/ckernel_sfpu_rounding_ops.h" ///< Rounding and truncation operations

/*
 * Note: The above includes provide access to all SFPU functionality.
 * Individual function implementations are contained within their respective
 * headers in the sfpu/ subdirectory. This aggregation header serves as a
 * convenient single-include point for accessing the complete SFPU function
 * library.
 *
 * For performance-critical applications, consider including only the specific
 * SFPU headers needed rather than this comprehensive collection.
 */
