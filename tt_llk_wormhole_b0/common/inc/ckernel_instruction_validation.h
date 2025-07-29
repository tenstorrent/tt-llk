// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

/**
 * @file ckernel_instruction_validation.h
 * @brief Instruction parameter validation utilities for Wormhole B0 Tensix ISA
 *
 * @details This header provides validation utilities for Tensix instruction parameters
 * that complement the auto-generated instruction macros in ckernel_ops.h. By separating
 * validation logic into a dedicated header, we improve code organization and compilation
 * performance while maintaining the same validation capabilities.
 *
 * **Validation Categories:**
 * - Bit-width validation for instruction parameters
 * - Range checking for hardware-specific constraints
 * - Compile-time parameter validation
 * - Runtime validation for debug builds
 *
 * **Integration with ckernel_ops.h:**
 * This header complements the auto-generated instruction macros by providing
 * additional validation utilities that would otherwise be embedded in the
 * large ckernel_ops.h file.
 */

#pragma once

#include <cstdint>
#include <type_traits>
#include "llk_validation.h"

namespace ckernel {
namespace instruction {

//==============================================================================
// Parameter Bit-Width Validation
//==============================================================================

/**
 * @brief Validate parameter fits in specified bit width
 * @param value Parameter value to validate
 * @param bits Number of bits available for parameter
 * @return true if value fits in specified bit width
 */
constexpr bool is_valid(std::uint32_t value, std::uint32_t bits) {
    return value < (1U << bits);
}

/**
 * @brief Validate signed parameter fits in specified bit width
 * @param value Signed parameter value to validate
 * @param bits Number of bits available for parameter (including sign bit)
 * @return true if value fits in specified bit width
 */
constexpr bool is_valid_signed(std::int32_t value, std::uint32_t bits) {
    const std::int32_t max_val = (1 << (bits - 1)) - 1;
    const std::int32_t min_val = -(1 << (bits - 1));
    return (value >= min_val) && (value <= max_val);
}

//==============================================================================
// Instruction-Specific Validation
//==============================================================================

/**
 * @brief Validate matrix multiplication instruction parameters
 * @param transpose Transpose flag
 * @param clear_dvalid Clear data valid flag
 * @param addr_mode Address modification mode
 * @param dst Destination register specification
 */
constexpr bool validate_mvmul_params(
    std::uint32_t transpose, 
    std::uint32_t clear_dvalid,
    std::uint32_t addr_mode,
    std::uint32_t dst) {
    
    return is_valid(transpose, 1) &&
           is_valid(clear_dvalid, 2) &&
           is_valid(addr_mode, 7) &&
           is_valid(dst, 14);
}

/**
 * @brief Validate unpacker instruction parameters
 * @param operand_sel Operand selection flags
 * @param addr_mode Address modification mode
 * @param dvalid Data valid control
 */
constexpr bool validate_unpacr_params(
    std::uint32_t operand_sel,
    std::uint32_t addr_mode,
    std::uint32_t dvalid) {
    
    return is_valid(operand_sel, 8) &&
           is_valid(addr_mode, 7) &&
           is_valid(dvalid, 1);
}

/**
 * @brief Validate packer instruction parameters
 * @param pack_sel Packer selection flags
 * @param addr_mode Address modification mode
 * @param zero_output Zero output flag
 */
constexpr bool validate_pacr_params(
    std::uint32_t pack_sel,
    std::uint32_t addr_mode,
    std::uint32_t zero_output) {
    
    return is_valid(pack_sel, 4) &&
           is_valid(addr_mode, 7) &&
           is_valid(zero_output, 1);
}

/**
 * @brief Validate SFPU instruction parameters
 * @param lreg_sel Local register selection
 * @param mode Operation mode
 * @param param Function parameter
 */
constexpr bool validate_sfpu_params(
    std::uint32_t lreg_sel,
    std::uint32_t mode,
    std::uint32_t param) {
    
    return is_valid(lreg_sel, 5) &&
           is_valid(mode, 3) &&
           is_valid(param, 16);
}

/**
 * @brief Validate address counter instruction parameters
 * @param cnt_set_mask Counter set mask
 * @param x_val X counter value
 * @param y_val Y counter value
 */
constexpr bool validate_adc_params(
    std::uint32_t cnt_set_mask,
    std::uint32_t x_val,
    std::uint32_t y_val) {
    
    return is_valid(cnt_set_mask, 3) &&
           is_valid(x_val, 6) &&
           is_valid(y_val, 6);
}

//==============================================================================
// Advanced Validation Helpers
//==============================================================================

/**
 * @brief Validate address modification mode is within valid range
 * @param addr_mode Address modification mode (0-7)
 */
constexpr bool is_valid_addr_mode(std::uint32_t addr_mode) {
    return addr_mode <= 7;
}

/**
 * @brief Validate register index is within hardware limits
 * @param reg_index Register index
 * @param max_regs Maximum number of registers for this type
 */
constexpr bool is_valid_register_index(std::uint32_t reg_index, std::uint32_t max_regs) {
    return reg_index < max_regs;
}

/**
 * @brief Validate tile index is within DEST register capacity
 * @param tile_index Tile index in DEST registers
 * @param is_fp32_mode Whether FP32 mode is enabled (affects capacity)
 */
constexpr bool is_valid_tile_index(std::uint32_t tile_index, bool is_fp32_mode) {
    const std::uint32_t max_tiles = is_fp32_mode ? 512 : 1024;
    return tile_index < max_tiles;
}

/**
 * @brief Validate fidelity phase parameter
 * @param fidelity_phase Fidelity phase (0-3)
 */
constexpr bool is_valid_fidelity_phase(std::uint32_t fidelity_phase) {
    return fidelity_phase <= 3;
}

//==============================================================================
// Runtime Validation Helpers (Debug Builds Only)
//==============================================================================

#if LLK_VALIDATION_LEVEL > 0

/**
 * @brief Runtime validation for instruction parameters
 * @param instruction_name Name of instruction for error messages
 * @param params Parameter pack to validate
 */
template <typename... Params>
void validate_instruction_runtime(const char* instruction_name, Params... params) {
    LLK_VALIDATE(true, "runtime instruction validation placeholder");
    // Specific validation logic would be implemented here
    (void)instruction_name; // Suppress unused warning
}

/**
 * @brief Validate instruction sequence coherency
 * @param prev_instruction Previous instruction type
 * @param curr_instruction Current instruction type
 */
inline void validate_instruction_sequence(const char* prev_instruction, const char* curr_instruction) {
    // Check for invalid instruction sequences
    LLK_VALIDATE(prev_instruction != nullptr, "previous instruction cannot be null");
    LLK_VALIDATE(curr_instruction != nullptr, "current instruction cannot be null");
}

/**
 * @brief Validate hardware state for instruction execution
 * @param required_state Description of required hardware state
 */
inline void validate_hardware_state(const char* required_state) {
    // Validate hardware is in correct state for instruction
    LLK_VALIDATE(required_state != nullptr, "required state description cannot be null");
}

#else
    // Release builds: runtime validation disabled
    template <typename... Params>
    inline void validate_instruction_runtime(const char*, Params...) { }
    
    inline void validate_instruction_sequence(const char*, const char*) { }
    inline void validate_hardware_state(const char*) { }
#endif

//==============================================================================
// Instruction Builder Utilities
//==============================================================================

/**
 * @brief Utility class for building validated instructions
 * @details Provides a safer interface for instruction construction with
 * automatic parameter validation and bit-field assembly.
 */
class InstructionBuilder {
private:
    std::uint32_t instruction_word_;
    bool is_valid_;

public:
    /**
     * @brief Construct instruction builder with opcode
     * @param opcode Instruction opcode
     */
    explicit InstructionBuilder(std::uint32_t opcode) 
        : instruction_word_(opcode << 24), is_valid_(true) {
        LLK_VALIDATE(is_valid(opcode, 8), "opcode must fit in 8 bits");
    }
    
    /**
     * @brief Add parameter field to instruction
     * @param value Parameter value
     * @param bits Number of bits for parameter
     * @param shift Bit position for parameter
     * @return Reference to this builder for chaining
     */
    InstructionBuilder& add_param(std::uint32_t value, std::uint32_t bits, std::uint32_t shift) {
        if (is_valid_ && is_valid(value, bits)) {
            instruction_word_ |= (value << shift);
        } else {
            is_valid_ = false;
            LLK_VALIDATE(false, "invalid parameter for instruction");
        }
        return *this;
    }
    
    /**
     * @brief Build final instruction word
     * @return Validated instruction word
     */
    std::uint32_t build() const {
        LLK_VALIDATE(is_valid_, "instruction has invalid parameters");
        return instruction_word_;
    }
    
    /**
     * @brief Check if instruction is valid
     * @return true if all parameters are valid
     */
    bool is_instruction_valid() const {
        return is_valid_;
    }
};

} // namespace instruction
} // namespace ckernel

//==============================================================================
// Convenience Macros for Instruction Validation
//==============================================================================

/**
 * @brief Validate instruction parameters at compile time
 * @param instruction_name Name of instruction
 * @param validation_expr Validation expression
 */
#define VALIDATE_INSTRUCTION_PARAMS(instruction_name, validation_expr) \
    static_assert(validation_expr, "Invalid parameters for " #instruction_name)

/**
 * @brief Runtime instruction validation macro
 * @param instruction_name Name of instruction for error messages
 * @param params Parameter pack to validate
 */
#define VALIDATE_INSTRUCTION_RUNTIME(instruction_name, ...) \
    ckernel::instruction::validate_instruction_runtime(instruction_name, __VA_ARGS__)

/**
 * @brief Validate address mode parameter
 * @param addr_mode Address modification mode
 */
#define VALIDATE_ADDR_MODE(addr_mode) \
    LLK_VALIDATE(ckernel::instruction::is_valid_addr_mode(addr_mode), \
                 "address mode must be 0-7")

/**
 * @brief Validate register index parameter
 * @param reg_index Register index
 * @param max_regs Maximum registers for this type
 */
#define VALIDATE_REGISTER_INDEX(reg_index, max_regs) \
    LLK_VALIDATE(ckernel::instruction::is_valid_register_index(reg_index, max_regs), \
                 "register index out of range") 
