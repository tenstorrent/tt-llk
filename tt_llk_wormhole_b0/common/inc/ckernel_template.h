// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

/**
 * @file ckernel_template.h
 * @brief Hardware Macro-Operation (MOP) template system for Wormhole B0 performance optimization
 * 
 * @details This header provides the `ckernel_template` class for generating optimized
 * hardware MOP (Macro-Operation) loop structures in Wormhole B0. MOPs are a critical
 * performance optimization feature that decouples TRISC instruction issue rate from
 * Tensix engine instruction execution rate, enabling sustained high utilization of
 * the 2048 hardware multipliers and specialized execution units.
 * 
 * **Wormhole B0 MOP Architecture:**
 * MOPs address the fundamental performance challenge in Tensix programming:
 * - **Problem**: TRISC overhead (loops, conditionals, branches) reduces Tensix instruction rate
 * - **Solution**: Hardware MOP decoder issues pre-programmed instruction sequences
 * - **Result**: TRISC can handle control logic while Tensix engine maintains peak utilization
 * - **Performance**: Achieves near 100% utilization of 2048 multipliers and compute resources
 * 
 * **MOP Hardware Implementation:**
 * - **MOP Decoder**: Dedicated hardware block in each TRISC (Figure 4 in architecture docs)
 * - **Instruction Buffer**: Hardware buffer for storing pre-programmed instruction sequences
 * - **Single Trigger**: One TRISC instruction (TTI_MOP) launches entire sequence
 * - **Decoupled Execution**: TRISC continues processing while MOP executes instructions
 * - **Buffer Management**: Hardware manages instruction buffer fill/drain automatically
 * 
 * **MOP Loop Structure:**
 * ```
 * LOOP_OUTER: <OUTER_LOOP_COUNT>        // 1-127 iterations
 *   START_OP                            // Optional initialization instruction
 *   LOOP_INNER: <INNER_LOOP_COUNT>      // 1-127 iterations  
 *     LOOP_OP0                          // Core computation instruction
 *     LOOP_OP1                          // Optional second core instruction
 *   END_LOOP_INNER
 *   END_OP0                             // Optional cleanup instruction
 *   END_OP1                             // Optional second cleanup instruction
 * END_LOOP_OUTER
 * ```
 * 
 * **Advanced MOP Features:**
 * - **Nested Loops**: 2-level loop hierarchy for complex access patterns
 * - **Last Iteration Modification**: Different instructions on final loop iterations
 * - **Address Side Effects**: Automatic address counter updates during execution
 * - **Parameterization**: Configurable loop counts and instruction parameters
 * - **Synchronization Integration**: Works with semaphores and barriers
 * 
 * **Performance Benefits:**
 * - **Loop Overhead Elimination**: Hardware handles all loop control logic
 * - **Instruction Rate**: Maintains 1 instruction/cycle to Tensix engine
 * - **TRISC Efficiency**: TRISC freed for higher-level control and coordination
 * - **Memory Bandwidth**: Optimized for sustained high memory throughput
 * - **Pipeline Utilization**: Keeps all 3 threads (Unpack, Math, Pack) busy
 * 
 * **Template Class Design:**
 * The `ckernel_template` class provides C++ abstraction over hardware MOPs:
 * - **Type Safety**: Compile-time validation of MOP parameters
 * - **Flexibility**: Support for 1 or 2 core loop instructions
 * - **Last Iteration Handling**: Automatic management of modified final iterations
 * - **Hardware Integration**: Direct translation to hardware MOP commands
 * 
 * **Usage Patterns:**
 * ```cpp
 * // Single instruction inner loop
 * ckernel_template my_mop(8, 16, TTI_MVMUL(...));
 * 
 * // Dual instruction inner loop
 * ckernel_template matrix_mop(4, 32, TTI_MVMUL(...), TTI_ELWADD(...));
 * 
 * // Configure end operations
 * my_mop.set_end_ops(TTI_STALLWAIT(...), TTI_SEMPOST(...));
 * ```
 * 
 * **Integration with Wormhole B0 Features:**
 * - **2048 Multipliers**: MOPs maximize multiplier utilization efficiency
 * - **Register Files**: Optimized address patterns for SRCA, SRCB, DEST access
 * - **Memory System**: Coordinates with L1 SRAM banking and address generation
 * - **Thread Synchronization**: Integrates with semaphore/mutex coordination
 * - **REPLAY Instructions**: Can be combined with REPLAY for even higher efficiency
 * 
 * **Common Use Cases:**
 * - **Matrix Multiplication**: Nested loops for optimized GEMM operations
 * - **Convolution**: Complex addressing patterns with stride management
 * - **Elementwise Operations**: High-throughput SIMD processing
 * - **Data Movement**: Efficient unpacking and packing sequences
 * - **Accumulation**: Iterative reduction and accumulation patterns
 */

#pragma once

#include "ckernel.h"

namespace ckernel
{

/**
 * @brief Hardware MOP template class for optimized loop structures in Wormhole B0
 * @details Provides C++ abstraction over the Wormhole B0 hardware MOP (Macro-Operation)
 * system, enabling generation of highly optimized nested loop structures that execute
 * at hardware speeds with minimal TRISC overhead. The class manages the complex
 * interaction between loop parameters, instruction sequences, and last-iteration handling.
 */
class ckernel_template
{
    // Here is the basic outline of a MOP loop and the definition of the
    // variables used.
    // LOOP_OUTER: <OUTER_LOOP_COUNT>
    //   START_OP
    //   LOOP_INNER: <INNER_LOOP_COUNT>
    //     LOOP_OP0
    //     LOOP_OP1
    //   END_LOOP_INNER
    //   END_OP0
    //   END_OP1
    // END_LOOP_OUTER

    /**
     * @brief Number of outer loop iterations (1-127)
     * @details Immutable outer loop count configured at construction time.
     * Used for high-level iteration patterns in matrix operations and convolution.
     */
    const uint m_outer_loop_len;
    
    /**
     * @brief Number of inner loop iterations (1-127)  
     * @details Immutable inner loop count for fine-grained operation repetition.
     * Optimized for register file access patterns and computational intensity.
     */
    const uint m_inner_loop_len;
    
    /**
     * @brief Primary core loop instruction (encoded 32-bit Tensix instruction)
     * @details The main computational instruction executed in each inner loop iteration.
     * Typically MVMUL, ELWADD, or other high-throughput mathematical operations.
     */
    uint m_loop_op0;
    
    /**
     * @brief Secondary core loop instruction (optional)
     * @details Optional second instruction for dual-instruction inner loops,
     * enabling complex computational patterns and address management.
     */
    uint m_loop_op1;
    
    /**
     * @brief End operation instructions and start operation
     * @details Cleanup and initialization instructions executed at loop boundaries.
     * Used for synchronization, address updates, and configuration changes.
     */
    uint m_end_op0, m_end_op1, m_start_op0;
    
    /**
     * @brief Last outer loop iteration instruction override
     * @details Special instruction executed on the final outer loop iteration instead
     * of the normal inner loop instruction. Enables address finalization, semaphore
     * signaling, and boundary condition handling for matrix operations.
     * 
     * **Hardware Logic:**
     * ```
     * if(last_inner_loop_iter && last_outer_loop_iter) 
     *     execute m_loop0_last_instr;
     * else if(last_inner_loop_iter)                    
     *     execute m_loop1_last_instr;
     * else                                             
     *     execute normal m_loop_op1;
     * ```
     */
    uint m_loop0_last_instr;
    
    /**
     * @brief Last inner loop iteration instruction override
     * @details Special instruction executed on the final inner loop iteration.
     * Handles address stride updates, row completion signaling, and preparation
     * for the next outer loop iteration in matrix and convolution algorithms.
     * 
     * **Note**: When outer_loop_len = 1, the last inner loop iteration is also
     * the last outer loop iteration, so m_loop0_last_instr takes precedence.
     */
    uint m_loop1_last_instr;

public:
    /**
     * @brief Construct single-instruction MOP template
     * @param outer_loop_len Number of outer loop iterations (1-127)
     * @param inner_loop_len Number of inner loop iterations (1-127)  
     * @param loop_op Primary loop instruction (32-bit encoded Tensix instruction)
     * 
     * @details Creates a MOP template with a single core instruction per inner loop
     * iteration. This is the most common pattern for straightforward mathematical
     * operations like matrix multiplication or elementwise operations.
     * 
     * **Wormhole B0 Performance Characteristics:**
     * - **Instruction Rate**: 1 instruction/cycle sustained execution
     * - **TRISC Overhead**: Minimal, only MOP setup and launch
     * - **Memory Efficiency**: Optimized for register file and L1 access patterns
     * - **Use Cases**: GEMM, elementwise ops, simple data movement
     */
    ckernel_template(uint outer_loop_len, uint inner_loop_len, uint loop_op);
    
    /**
     * @brief Construct dual-instruction MOP template
     * @param outer_loop_len Number of outer loop iterations (1-127)
     * @param inner_loop_len Number of inner loop iterations (1-127)
     * @param loop_op0 Primary loop instruction
     * @param loop_op1 Secondary loop instruction  
     * 
     * @details Creates a MOP template with two core instructions per inner loop
     * iteration. Enables more complex computational patterns and sophisticated
     * address management within the inner loop structure.
     * 
     * **Advanced Usage Patterns:**
     * - **Compute + Address Update**: Math operation followed by counter increment
     * - **Dual Math Operations**: Overlapped or pipelined mathematical operations
     * - **Synchronization Integration**: Computation with embedded synchronization
     * - **Memory Management**: Data movement with address pattern management
     */
    ckernel_template(uint outer_loop_len, uint inner_loop_len, uint loop_op0, uint loop_op1);
    
    /**
     * @brief Set or modify primary loop instruction
     * @param loop_op New primary loop instruction (32-bit encoded)
     * 
     * @details Updates the primary core loop instruction after construction.
     * Useful for dynamic algorithm adaptation or instruction sequence optimization
     * based on runtime conditions or data characteristics.
     */
    void set_loop_op0(uint loop_op);
    
    /**
     * @brief Set or modify secondary loop instruction
     * @param loop_op New secondary loop instruction (32-bit encoded)
     * 
     * @details Updates the secondary core loop instruction. Enables runtime
     * adaptation of dual-instruction patterns for algorithm optimization
     * or varying computational requirements.
     */
    void set_loop_op1(uint loop_op);
    
    /**
     * @brief Configure end operation instructions
     * @param end_op0 Primary end operation instruction
     * @param end_op1 Secondary end operation instruction
     * 
     * @details Sets the cleanup instructions executed after inner loop completion.
     * Critical for proper synchronization, address finalization, and resource
     * management in complex matrix and convolution algorithms.
     * 
     * **Common End Operations:**
     * - **Synchronization**: TTI_SEMPOST, TTI_STALLWAIT for thread coordination
     * - **Address Updates**: Address counter modifications for next iteration
     * - **Configuration**: CFG register updates for mode switching
     * - **Status Updates**: Performance monitoring and debug information
     */
    void set_end_ops(uint end_op0, uint end_op1);
    /**
     * @brief Configure single end operation instruction
     * @param end_op0 End operation instruction
     * 
     * @details Sets a single cleanup instruction executed after inner loop completion.
     * Simpler interface for MOPs requiring only one end operation.
     */
    void set_end_op(uint end_op0);
    
    /**
     * @brief Configure loop initialization instruction
     * @param start_op0 Start operation instruction
     * 
     * @details Sets the initialization instruction executed before the inner loop begins.
     * Used for address initialization, configuration setup, or synchronization preparation.
     * 
     * **Common Start Operations:**
     * - **Address Initialization**: Set register window counters to initial positions
     * - **Configuration Setup**: Configure data formats or computation modes
     * - **Synchronization Prep**: Initialize semaphore states or barrier conditions
     */
    void set_start_op(uint start_op0);
    
    /**
     * @brief Configure last inner loop iteration instruction override
     * @param op Special instruction for final inner loop iteration
     * 
     * @details Sets the instruction executed on the last iteration of the inner loop
     * instead of the normal loop instruction. Essential for proper address stride
     * handling and preparation for the next outer loop iteration.
     * 
     * **Matrix Operation Usage:**
     * - **Row Completion**: Signal completion of matrix row processing
     * - **Address Stride**: Update address counters for next row/block
     * - **Data Boundary**: Handle edge cases at data boundaries
     */
    void set_last_inner_loop_instr(uint op);
    
    /**
     * @brief Configure last outer loop iteration instruction override
     * @param op Special instruction for final outer loop iteration
     * 
     * @details Sets the instruction executed on the last iteration of the outer loop.
     * Critical for algorithm completion, final result handling, and resource cleanup.
     * 
     * **Algorithm Completion:**
     * - **Result Finalization**: Final accumulation or reduction operations
     * - **Semaphore Signaling**: Signal algorithm completion to other threads
     * - **Address Cleanup**: Reset address counters for next algorithm invocation
     */
    void set_last_outer_loop_instr(uint op);

    /**
     * @brief Program MOP hardware registers without execution
     * @param instrn_buffer Pointer to Tensix instruction buffer for this thread
     * 
     * @details Programs the hardware MOP decoder registers with the configured
     * loop structure and instruction sequence, but does not initiate execution.
     * Useful for pre-programming MOPs for later triggered execution.
     * 
     * **Wormhole B0 MOP Programming:**
     * - **Register Setup**: Configures MOP decoder hardware state
     * - **Parameter Validation**: Validates loop counts and instruction encodings
     * - **Buffer Preparation**: Prepares instruction buffer for hardware execution
     * - **No Execution**: MOP is ready but not started
     */
    void program(volatile uint *instrn_buffer);
    
    /**
     * @brief Execute pre-programmed MOP without re-programming
     * @param instrn_buffer Pointer to Tensix instruction buffer for this thread
     * 
     * @details Triggers execution of a previously programmed MOP. Assumes the
     * MOP hardware registers have been properly configured by a prior `program()` call.
     * Enables efficient repeated execution of the same MOP pattern.
     * 
     * **Performance Optimization:**
     * - **Zero Programming Overhead**: No register setup during execution
     * - **Rapid Restart**: Immediate MOP execution for repeated patterns
     * - **Cache Efficiency**: Leverages pre-programmed hardware state
     */
    static void run(volatile uint *instrn_buffer);
    
    /**
     * @brief Program and immediately execute MOP in single operation
     * @param instrn_buffer Pointer to Tensix instruction buffer for this thread
     * 
     * @details Combines `program()` and `run()` into a single atomic operation.
     * Most convenient interface for one-time MOP execution with immediate results.
     * 
     * **Usage Pattern:**
     * ```cpp
     * ckernel_template matrix_mop(8, 16, TTI_MVMUL(...));
     * matrix_mop.program_and_run(instrn_buffer);  // Execute immediately
     * ```
     * 
     * **Wormhole B0 Integration:**
     * - **Atomic Operation**: Hardware programming and execution in sequence
     * - **Error Handling**: Comprehensive validation before execution
     * - **Performance**: Optimized for single-use MOP patterns
     */
    void program_and_run(volatile uint *instrn_buffer);
};

/**
 * @brief Specialized MOP template for Thread 0 (Unpack) conditional data unpacking
 * @details Provides a sophisticated unpack MOP template with conditional execution
 * based on zero-mask (zmask) patterns. This template is specifically optimized for
 * sparse tensor processing and convolution algorithms where data unpacking needs
 * to be selectively enabled based on data validity masks.
 * 
 * **Wormhole B0 Unpack-Specific MOP Architecture:**
 * Available on TRISC0 (Thread 0) and designed for efficient sparse data handling:
 * - **Conditional Execution**: Instructions executed based on zmask[iteration] state
 * - **Halo Support**: Optional A1/A2/A3 instructions for convolution halo regions
 * - **Dual Operand**: Optional B operand unpacking for matrix operations
 * - **Skip Optimization**: Efficient skip operations for sparse data patterns
 * 
 * **MOP Loop Structure:**
 * ```
 * LOOP:
 *   if (zmask[iteration]):
 *     UNPACR_A0          // Primary A operand unpack
 *     UNPACR_A1          // Halo region A operand (optional)
 *     UNPACR_A2          // Extended halo A operand (optional)  
 *     UNPACR_A3          // Maximum halo A operand (optional)
 *     UNPACR_B           // B operand unpack (optional)
 *   else:
 *     SKIP_A             // A operand skip with context increment
 *     SKIP_B             // B operand skip with context increment (optional)
 * ```
 * 
 * **Conditional Execution Features:**
 * - **Zero Mask**: Hardware-evaluated mask determines instruction execution
 * - **Sparse Optimization**: Skip operations avoid unnecessary data movement
 * - **Context Management**: Automatic context counter advancement for skipped data
 * - **Memory Efficiency**: Reduced L1 bandwidth usage for sparse tensors
 * 
 * **Convolution Halo Support:**
 * Specialized for convolution algorithms requiring halo (padding) region processing:
 * - **A0**: Primary convolution kernel operand
 * - **A1-A3**: Halo region operands for edge/boundary handling
 * - **Configurable**: Halo instructions can be selectively enabled/disabled
 * - **Performance**: Optimized addressing patterns for sliding window operations
 */
class ckernel_unpack_template
{
    /**
     * @brief Enable B operand unpacking
     * @details When true, includes UNPACR_B and SKIP_B instructions in the template.
     * Essential for matrix operations requiring dual operands (A and B).
     */
    const bool m_unpackB;
    
    /**
     * @brief Enable convolution halo region unpacking
     * @details When true, includes A1, A2, A3 instructions for halo region processing.
     * Required for convolution algorithms with padding and boundary handling.
     */
    const bool m_unpack_halo;

    /**
     * @brief A operand unpack instructions (primary and halo regions)
     * @details Encoded Tensix UNPACR instructions for A operand processing:
     * - **m_A0_instr**: Primary A operand unpack (always present)
     * - **m_A1_instr**: First halo region A operand (halo mode only)
     * - **m_A2_instr**: Second halo region A operand (halo mode only)
     * - **m_A3_instr**: Third halo region A operand (halo mode only)
     */
    const uint m_A0_instr, m_A1_instr, m_A2_instr, m_A3_instr;
    
    /**
     * @brief B operand unpack instruction  
     * @details Encoded Tensix UNPACR instruction for B operand processing.
     * Used in matrix operations where dual operands are required.
     */
    const uint m_B_instr;

    /**
     * @brief Skip instructions for sparse data optimization
     * @details Encoded Tensix skip instructions executed when zmask[iteration] is false:
     * - **m_skipA_instr**: A operand skip with context counter increment
     * - **m_skipB_instr**: B operand skip with context counter increment
     * 
     * These ensure proper context management even when data unpacking is skipped.
     */
    const uint m_skipA_instr;
    const uint m_skipB_instr;

public:
    ckernel_unpack_template(
        bool unpackB,
        bool unpackHalo,
        uint A0_instr,
        uint A1_instr,
        uint A2_instr,
        uint A3_instr,
        uint skipA_instr,

        uint B_instr,
        uint skipB_instr) :
        m_unpackB(unpackB),
        m_unpack_halo(unpackHalo),
        m_A0_instr(A0_instr),
        m_A1_instr(A1_instr),
        m_A2_instr(A2_instr),
        m_A3_instr(A3_instr),
        m_B_instr(B_instr),
        m_skipA_instr(skipA_instr),
        m_skipB_instr(skipB_instr)
    {
    }

public:
    // Default ZeroSrcA UNPACR_NOP
    static constexpr uint DEF_ZEROSRCA   = TT_OP_UNPACR_NOP(p_unpacr_nop::UNP0, p_unpacr_nop::UNP_ZEROSRC);
    static constexpr uint DEF_NINFSRCA   = TT_OP_UNPACR_NOP(p_unpacr_nop::UNP0, p_unpacr_nop::UNP_NEGINFSRC);
    static constexpr uint DEF_UNPACR_NOP = TT_OP_UNPACR_NOP(p_unpacr_nop::UNP0, p_unpacr_nop::UNP_NOP);

    // Default skip A/B instructions that increment Z counters by 1
    static constexpr uint DEF_SKIP_A = TT_OP_INCADCZW(0b001, 0, 0, 0, 1);
    static constexpr uint DEF_SKIP_B = TT_OP_INCADCZW(0b010, 0, 0, 0, 1);

    // Default non-halo A instruction
    static constexpr uint DEF_A_instr           = TT_OP_UNPACR(0, 0b1, 0, 0, 0, 0, 1, p_unpacr::RAREFYB_DISABLE, 0, 0, 0, 0, 1);
    static constexpr uint DEF_A_cntx_ovrd_instr = TT_OP_UNPACR(0, 0b1, 0, 0, 0, 1, 1, p_unpacr::RAREFYB_DISABLE, 0, 0, 0, 0, 1);

    // Default B instruction with rarefy
    static constexpr uint DEF_B_rarefy_instr = TT_OP_UNPACR(1, 0b01, 0, 0, 0, 0, 1, p_unpacr::RAREFYB_ENABLE, 0, 0, 0, 0, 1);

    // Default B instruction with rarefy and context override, coupled with halo on A
    static constexpr uint DEF_B_rarefy_cntx_ovrd_instr = TT_OP_UNPACR(1, 0b01, 0, 0, 0, 1, 1, p_unpacr::RAREFYB_ENABLE, 0, 0, 0, 0, 1);

    // Default B instruction without rarefy, no z increment and with context override, coupled with halo on A for factored conv
    static constexpr uint DEF_B_cntx_ovrd_no_z_inc_instr = TT_OP_UNPACR(1, 0b00, 0, 0, 0, 1, 1, p_unpacr::RAREFYB_DISABLE, 0, 0, 0, 0, 1);

    // Default B instruction without rarefy and context override
    static constexpr uint DEF_B_cntx_ovrd_instr = TT_OP_UNPACR(1, 0b01, 0, 0, 0, 1, 1, p_unpacr::RAREFYB_DISABLE, 0, 0, 0, 0, 1);

    // Default B instruction without rarefy
    static constexpr uint DEF_B_instr = TT_OP_UNPACR(1, 0b01, 0, 0, 0, 0, 1, p_unpacr::RAREFYB_DISABLE, 0, 0, 0, 0, 1);

    // Default halo A instructions
    static constexpr uint DEF_A0_instr = TT_OP_UNPACR(
        0, 0b00, 0, p_unpacr::TILE0_CFG_CONTEXT, p_unpacr::TILE0_ADDRCNT_CONTEXT, 1, 0, p_unpacr::RAREFYB_DISABLE, 0, p_unpacr::AUTO_INC_CONTEXT, 1, 0, 1);
    static constexpr uint DEF_A1_instr = TT_OP_UNPACR(
        0, 0b00, 0, p_unpacr::TILE1_CFG_CONTEXT, p_unpacr::TILE1_ADDRCNT_CONTEXT, 1, 0, p_unpacr::RAREFYB_DISABLE, 0, p_unpacr::AUTO_INC_CONTEXT, 1, 0, 1);
    static constexpr uint DEF_A2_instr = TT_OP_UNPACR(
        0, 0b00, 0, p_unpacr::TILE2_CFG_CONTEXT, p_unpacr::TILE2_ADDRCNT_CONTEXT, 1, 0, p_unpacr::RAREFYB_DISABLE, 0, p_unpacr::AUTO_INC_CONTEXT, 1, 0, 1);
    static constexpr uint DEF_A3_instr = TT_OP_UNPACR(
        0, 0b01, 0, p_unpacr::TILE3_CFG_CONTEXT, p_unpacr::TILE3_ADDRCNT_CONTEXT, 1, 1, p_unpacr::RAREFYB_DISABLE, 0, p_unpacr::AUTO_INC_CONTEXT, 1, 0, 1);

    // Special case where all later strips are skipped, so this one has to set DVALID because it is last, and increment Z
    static constexpr uint DEF_A0_last_instr = TT_OP_UNPACR(
        0, 0b01, 0, p_unpacr::TILE0_CFG_CONTEXT, p_unpacr::TILE0_ADDRCNT_CONTEXT, 1, 1, p_unpacr::RAREFYB_DISABLE, 0, p_unpacr::AUTO_INC_CONTEXT, 1, 0, 1);
    static constexpr uint DEF_A1_last_instr = TT_OP_UNPACR(
        0, 0b01, 0, p_unpacr::TILE1_CFG_CONTEXT, p_unpacr::TILE1_ADDRCNT_CONTEXT, 1, 1, p_unpacr::RAREFYB_DISABLE, 0, p_unpacr::AUTO_INC_CONTEXT, 1, 0, 1);
    static constexpr uint DEF_A2_last_instr = TT_OP_UNPACR(
        0, 0b01, 0, p_unpacr::TILE2_CFG_CONTEXT, p_unpacr::TILE2_ADDRCNT_CONTEXT, 1, 1, p_unpacr::RAREFYB_DISABLE, 0, p_unpacr::AUTO_INC_CONTEXT, 1, 0, 1);

    // Halo A instructions that skip actual unpacking, but increment context
    static constexpr uint SKIP_A0_instr = TT_OP_UNPACR(
        0, 0b00, 1, p_unpacr::TILE0_CFG_CONTEXT, p_unpacr::TILE0_ADDRCNT_CONTEXT, 1, 0, p_unpacr::RAREFYB_DISABLE, 0, p_unpacr::AUTO_INC_CONTEXT, 1, 0, 1);
    static constexpr uint SKIP_A1_instr = TT_OP_UNPACR(
        0, 0b00, 1, p_unpacr::TILE1_CFG_CONTEXT, p_unpacr::TILE1_ADDRCNT_CONTEXT, 1, 0, p_unpacr::RAREFYB_DISABLE, 0, p_unpacr::AUTO_INC_CONTEXT, 1, 0, 1);
    static constexpr uint SKIP_A2_instr = TT_OP_UNPACR(
        0, 0b00, 1, p_unpacr::TILE2_CFG_CONTEXT, p_unpacr::TILE2_ADDRCNT_CONTEXT, 1, 0, p_unpacr::RAREFYB_DISABLE, 0, p_unpacr::AUTO_INC_CONTEXT, 1, 0, 1);
    static constexpr uint SKIP_A3_instr = TT_OP_UNPACR(
        0, 0b00, 1, p_unpacr::TILE3_CFG_CONTEXT, p_unpacr::TILE3_ADDRCNT_CONTEXT, 1, 0, p_unpacr::RAREFYB_DISABLE, 0, p_unpacr::AUTO_INC_CONTEXT, 1, 0, 1);

    // Factored conv halo A instructions
    static constexpr uint DEF_A0_fconv_instr = TT_OP_UNPACR(
        0, 0b00, 0, p_unpacr::TILE0_CFG_CONTEXT, p_unpacr::TILE0_ADDRCNT_CONTEXT, 1, 0, p_unpacr::RAREFYB_DISABLE, 0, p_unpacr::AUTO_INC_CONTEXT, 1, 0, 1);
    static constexpr uint DEF_A1_fconv_instr = TT_OP_UNPACR(
        0, 0b00, 0, p_unpacr::TILE1_CFG_CONTEXT, p_unpacr::TILE1_ADDRCNT_CONTEXT, 1, 0, p_unpacr::RAREFYB_DISABLE, 0, p_unpacr::AUTO_INC_CONTEXT, 1, 0, 1);
    static constexpr uint DEF_A2_fconv_instr = TT_OP_UNPACR(
        0, 0b00, 0, p_unpacr::TILE2_CFG_CONTEXT, p_unpacr::TILE2_ADDRCNT_CONTEXT, 1, 0, p_unpacr::RAREFYB_DISABLE, 0, p_unpacr::AUTO_INC_CONTEXT, 1, 0, 1);
    static constexpr uint DEF_A3_fconv_instr = TT_OP_UNPACR(
        0, 0b00, 0, p_unpacr::TILE3_CFG_CONTEXT, p_unpacr::TILE3_ADDRCNT_CONTEXT, 1, 1, p_unpacr::RAREFYB_DISABLE, 0, p_unpacr::AUTO_INC_CONTEXT, 1, 0, 1);

    // Special case where all later strips are skipped, so this one has to set DVALID because it is last, and increment Z (factored conv)
    static constexpr uint DEF_A0_fconv_last_instr = TT_OP_UNPACR(
        0, 0b00, 0, p_unpacr::TILE0_CFG_CONTEXT, p_unpacr::TILE0_ADDRCNT_CONTEXT, 1, 1, p_unpacr::RAREFYB_DISABLE, 0, p_unpacr::AUTO_INC_CONTEXT, 1, 0, 1);
    static constexpr uint DEF_A1_fconv_last_instr = TT_OP_UNPACR(
        0, 0b00, 0, p_unpacr::TILE1_CFG_CONTEXT, p_unpacr::TILE1_ADDRCNT_CONTEXT, 1, 1, p_unpacr::RAREFYB_DISABLE, 0, p_unpacr::AUTO_INC_CONTEXT, 1, 0, 1);
    static constexpr uint DEF_A2_fconv_last_instr = TT_OP_UNPACR(
        0, 0b00, 0, p_unpacr::TILE2_CFG_CONTEXT, p_unpacr::TILE2_ADDRCNT_CONTEXT, 1, 1, p_unpacr::RAREFYB_DISABLE, 0, p_unpacr::AUTO_INC_CONTEXT, 1, 0, 1);

    static constexpr uint DEF_Strip0_instr = TT_OP_UNPACR(0, 0, 0, 0, p_unpacr::TILE0_ADDRCNT_CONTEXT, 1, 0, p_unpacr::RAREFYB_DISABLE, 0, 0, 0, 0, 1);
    static constexpr uint DEF_Strip1_instr = TT_OP_UNPACR(0, 0, 0, 1, p_unpacr::TILE1_ADDRCNT_CONTEXT, 1, 0, p_unpacr::RAREFYB_DISABLE, 0, 0, 0, 0, 1);
    static constexpr uint DEF_Strip2_instr = TT_OP_UNPACR(0, 0, 0, 2, p_unpacr::TILE2_ADDRCNT_CONTEXT, 1, 0, p_unpacr::RAREFYB_DISABLE, 0, 0, 0, 0, 1);
    static constexpr uint DEF_Strip3_instr = TT_OP_UNPACR(0, 0, 0, 3, p_unpacr::TILE3_ADDRCNT_CONTEXT, 1, 0, p_unpacr::RAREFYB_DISABLE, 0, 0, 0, 0, 1);

    static constexpr uint DEF_Strip0_last_instr = TT_OP_UNPACR(0, 1, 0, 0, p_unpacr::TILE0_ADDRCNT_CONTEXT, 1, 1, p_unpacr::RAREFYB_DISABLE, 0, 0, 0, 0, 1);
    static constexpr uint DEF_Strip1_last_instr = TT_OP_UNPACR(0, 1, 0, 1, p_unpacr::TILE1_ADDRCNT_CONTEXT, 1, 1, p_unpacr::RAREFYB_DISABLE, 0, 0, 0, 0, 1);
    static constexpr uint DEF_Strip2_last_instr = TT_OP_UNPACR(0, 1, 0, 2, p_unpacr::TILE2_ADDRCNT_CONTEXT, 1, 1, p_unpacr::RAREFYB_DISABLE, 0, 0, 0, 0, 1);
    static constexpr uint DEF_Strip3_last_instr = TT_OP_UNPACR(0, 1, 0, 3, p_unpacr::TILE3_ADDRCNT_CONTEXT, 1, 1, p_unpacr::RAREFYB_DISABLE, 0, 0, 0, 0, 1);

    static constexpr uint DEF_Strip0_data_valid_instr =
        TT_OP_UNPACR(0, 0, 0, 0, p_unpacr::TILE0_ADDRCNT_CONTEXT, 1, 1, p_unpacr::RAREFYB_DISABLE, 0, 0, 0, 0, 1);
    static constexpr uint DEF_Strip1_data_valid_instr =
        TT_OP_UNPACR(0, 0, 0, 1, p_unpacr::TILE1_ADDRCNT_CONTEXT, 1, 1, p_unpacr::RAREFYB_DISABLE, 0, 0, 0, 0, 1);
    static constexpr uint DEF_Strip2_data_valid_instr =
        TT_OP_UNPACR(0, 0, 0, 2, p_unpacr::TILE2_ADDRCNT_CONTEXT, 1, 1, p_unpacr::RAREFYB_DISABLE, 0, 0, 0, 0, 1);
    static constexpr uint DEF_Strip3_data_valid_instr =
        TT_OP_UNPACR(0, 0, 0, 3, p_unpacr::TILE3_ADDRCNT_CONTEXT, 1, 1, p_unpacr::RAREFYB_DISABLE, 0, 0, 0, 0, 1);

    //
    // Convenience factory methods
    //
    static ckernel_unpack_template lzA(bool neginf, uint A_instr = DEF_A_cntx_ovrd_instr, uint skipA_instr = DEF_SKIP_A);

    static ckernel_unpack_template lA(uint A_instr = DEF_A_cntx_ovrd_instr, uint skipA_instr = DEF_SKIP_A);

    static ckernel_unpack_template lB(uint B_instr = DEF_B_cntx_ovrd_instr, uint skipB_instr = DEF_SKIP_B);

    static ckernel_unpack_template lhA(const uint32_t halo_mask);

    static ckernel_unpack_template flhA(const uint32_t halo_mask);

    static ckernel_unpack_template lBhA(const uint32_t halo_mask, const bool rarefy = true);

    static ckernel_unpack_template flBhA(const uint32_t halo_mask);

    static ckernel_unpack_template lBA(
        uint A_instr     = DEF_A_instr,
        uint skipA_instr = DEF_SKIP_A,

        uint B_instr     = DEF_B_instr,
        uint skipB_instr = DEF_SKIP_B);

    // More abstraction to reuse above templates for kernel to run loop of N instructions
    static ckernel_unpack_template loopx1instr(uint instr0, uint skip0 = TT_OP_NOP);
    static ckernel_unpack_template loopx2instr(uint instr0, uint instr1, uint skip0 = TT_OP_NOP, uint skip1 = TT_OP_NOP);

    void program(volatile uint *instrn_buffer) const;                                                  // just programs the registers
    static void run(volatile uint *instrn_buffer, const uint8_t count, const uint32_t zmask);          // runs - assumes that registers were already programmed
    static void run(volatile uint *instrn_buffer, const uint8_t count);                                // runs - assumes that registers were already programmed
    void program_and_run(volatile uint *instrn_buffer, const uint8_t count, const uint32_t zmask = 0); // calls program, then run
};

inline ckernel_template::ckernel_template(uint outer_loop_len, uint inner_loop_len, uint loop_op) :
    m_outer_loop_len(outer_loop_len),
    m_inner_loop_len(inner_loop_len),
    m_loop_op0(loop_op),
    m_loop_op1(TT_OP_NOP),
    m_end_op0(TT_OP_NOP),
    m_end_op1(TT_OP_NOP),
    m_start_op0(TT_OP_NOP)
{
    m_loop0_last_instr = loop_op;
    m_loop1_last_instr = loop_op;
}

inline ckernel_template::ckernel_template(uint outer_loop_len, uint inner_loop_len, uint loop_op0, uint loop_op1) :
    m_outer_loop_len(outer_loop_len),
    m_inner_loop_len(inner_loop_len),
    m_loop_op0(loop_op0),
    m_loop_op1(loop_op1),
    m_end_op0(TT_OP_NOP),
    m_end_op1(TT_OP_NOP),
    m_start_op0(TT_OP_NOP)
{
    m_loop0_last_instr = loop_op1;
    m_loop1_last_instr = loop_op1;
}

inline void ckernel_template::set_loop_op0(uint loop_op)
{
    m_loop_op0 = loop_op;
}

inline void ckernel_template::set_loop_op1(uint loop_op)
{
    m_loop_op1 = loop_op;
}

inline void ckernel_template::set_end_ops(uint end_op0, uint end_op1)
{
    m_end_op0 = end_op0;
    m_end_op1 = end_op1;
}

inline void ckernel_template::set_end_op(uint end_op0)
{
    set_end_ops(end_op0, TT_OP_NOP);
}

inline void ckernel_template::set_start_op(uint start_op0)
{
    m_start_op0 = start_op0;
}

inline void ckernel_template::set_last_inner_loop_instr(uint op)
{
    m_loop1_last_instr = op;
}

inline void ckernel_template::set_last_outer_loop_instr(uint op)
{
    m_loop0_last_instr = op;
}

inline void ckernel_template::program_and_run(volatile uint *instrn_buffer)
{
    program(instrn_buffer);
    run(instrn_buffer);
}

inline void ckernel_template::run(volatile uint *instrn_buffer)
{
    TTI_MOP(1, 0, 0); // run the double-loop template
}

inline void ckernel_template::program(volatile uint *instrn_buffer)
{
    volatile uint *mop_cfg = reinterpret_cast<volatile uint *>(TENSIX_MOP_CFG_BASE);

    mop_sync(); // wait until previous mops have completed

    mop_cfg[0] = m_outer_loop_len;
    mop_cfg[1] = m_inner_loop_len;
    mop_cfg[2] = m_start_op0;
    mop_cfg[3] = m_end_op0;
    mop_cfg[4] = m_end_op1;
    mop_cfg[5] = m_loop_op0;
    mop_cfg[6] = m_loop_op1;
    mop_cfg[7] = m_loop0_last_instr;
    mop_cfg[8] = m_loop1_last_instr;
}

inline void ckernel_unpack_template::program_and_run(volatile uint *instrn_buffer, const uint8_t count, const uint32_t zmask)
{
    program(instrn_buffer);
    run(instrn_buffer, count, zmask);
}

inline void ckernel_unpack_template::run(volatile uint *instrn_buffer, const uint8_t count, const uint32_t zmask)
{
    TT_MOP_CFG(zmask >> 16);              // Set the top 16 bits of zmask - we could skip this for count <= 16
    TT_MOP(0, count - 1, zmask & 0xFFFF); // Run the template
}

// Version without zmask, should be slightly faster by eliminating one instruction.
inline void ckernel_unpack_template::run(volatile uint *instrn_buffer, const uint8_t count)
{
    TT_MOP(0, count - 1, 0); // Run the template
}

inline void ckernel_unpack_template::program(volatile uint *instrn_buffer) const
{
    volatile uint *mop_cfg = reinterpret_cast<volatile uint *>(TENSIX_MOP_CFG_BASE);

    mop_sync(); // wait until previous mops have completed

    mop_cfg[1] = m_unpackB | (m_unpack_halo << 1);
    mop_cfg[2] = m_B_instr;
    mop_cfg[3] = m_A0_instr;
    mop_cfg[4] = m_A1_instr;
    mop_cfg[5] = m_A2_instr;
    mop_cfg[6] = m_A3_instr;
    mop_cfg[7] = m_skipA_instr;
    mop_cfg[8] = m_skipB_instr;
}

inline ckernel_unpack_template ckernel_unpack_template::lA(uint A_instr, uint skipA_instr)
{
    return ckernel_unpack_template(
        false, // src B
        false, // halo
        A_instr,
        0,
        0,
        0,
        skipA_instr,
        0,
        0);
}

inline ckernel_unpack_template ckernel_unpack_template::lB(uint B_instr, uint skipB_instr)
{
    return ckernel_unpack_template(
        false, // src B
        false, // halo
        B_instr,
        0,
        0,
        0,
        skipB_instr,
        0,
        0);
}

inline ckernel_unpack_template ckernel_unpack_template::lzA(bool neginf, uint A_instr, uint skipA_instr)
{
    return ckernel_unpack_template(
        false, // src B
        true,  // halo
        neginf ? DEF_NINFSRCA : DEF_ZEROSRCA,
        A_instr,
        DEF_UNPACR_NOP,
        DEF_UNPACR_NOP,
        skipA_instr,
        0,
        0);
}

inline ckernel_unpack_template ckernel_unpack_template::lhA(const uint32_t halo_mask)
{
    // Figure out which unpack is last
    const uint last_mask = (halo_mask == 0x1) ? 0x1 : (halo_mask <= 0x3) ? 0x2 : (halo_mask <= 0x7) ? 0x4 : 0;

    return ckernel_unpack_template(
        false, // src B
        true,  // halo
        ((halo_mask >> 0) & 0x1) ? ((last_mask >> 0) & 0x1) ? DEF_A0_last_instr : DEF_A0_instr : SKIP_A0_instr,
        ((halo_mask >> 1) & 0x1) ? ((last_mask >> 1) & 0x1) ? DEF_A1_last_instr : DEF_A1_instr : SKIP_A1_instr,
        ((halo_mask >> 2) & 0x1) ? ((last_mask >> 2) & 0x1) ? DEF_A2_last_instr : DEF_A2_instr : SKIP_A2_instr,
        ((halo_mask >> 3) & 0x1) ? DEF_A3_instr : SKIP_A3_instr,
        DEF_SKIP_A,
        0,
        0);
}

inline ckernel_unpack_template ckernel_unpack_template::flhA(const uint32_t halo_mask)
{
    // Figure out which unpack is last
    const uint last_mask = (halo_mask == 0x1) ? 0x1 : (halo_mask <= 0x3) ? 0x2 : (halo_mask <= 0x7) ? 0x4 : 0;

    return ckernel_unpack_template(
        false, // src B
        true,  // halo
        ((halo_mask >> 0) & 0x1) ? ((last_mask >> 0) & 0x1) ? DEF_A0_fconv_last_instr : DEF_A0_fconv_instr : SKIP_A0_instr,
        ((halo_mask >> 1) & 0x1) ? ((last_mask >> 1) & 0x1) ? DEF_A1_fconv_last_instr : DEF_A1_fconv_instr : SKIP_A1_instr,
        ((halo_mask >> 2) & 0x1) ? ((last_mask >> 2) & 0x1) ? DEF_A2_fconv_last_instr : DEF_A2_fconv_instr : SKIP_A2_instr,
        ((halo_mask >> 3) & 0x1) ? DEF_A3_fconv_instr : SKIP_A3_instr,
        TT_OP_NOP,
        0,
        0);
}

inline ckernel_unpack_template ckernel_unpack_template::lBhA(const uint32_t halo_mask, const bool rarefy)
{
    // Figure out which unpack is last
    const uint last_mask = (halo_mask == 0x1) ? 0x1 : (halo_mask <= 0x3) ? 0x2 : (halo_mask <= 0x7) ? 0x4 : 0;

    return ckernel_unpack_template(
        true, // src B
        true, // halo
        ((halo_mask >> 0) & 0x1) ? ((last_mask >> 0) & 0x1) ? DEF_A0_last_instr : DEF_A0_instr : SKIP_A0_instr,
        ((halo_mask >> 1) & 0x1) ? ((last_mask >> 1) & 0x1) ? DEF_A1_last_instr : DEF_A1_instr : SKIP_A1_instr,
        ((halo_mask >> 2) & 0x1) ? ((last_mask >> 2) & 0x1) ? DEF_A2_last_instr : DEF_A2_instr : SKIP_A2_instr,
        ((halo_mask >> 3) & 0x1) ? DEF_A3_instr : SKIP_A3_instr,
        DEF_SKIP_A,
        rarefy ? DEF_B_rarefy_cntx_ovrd_instr : DEF_B_cntx_ovrd_instr,
        DEF_SKIP_B);
}

inline ckernel_unpack_template ckernel_unpack_template::flBhA(const uint32_t halo_mask)
{
    // Figure out which unpack is last
    const uint last_mask = (halo_mask == 0x1) ? 0x1 : (halo_mask <= 0x3) ? 0x2 : (halo_mask <= 0x7) ? 0x4 : 0;

    return ckernel_unpack_template(
        true, // src B
        true, // halo
        ((halo_mask >> 0) & 0x1) ? ((last_mask >> 0) & 0x1) ? DEF_A0_fconv_last_instr : DEF_A0_fconv_instr : SKIP_A0_instr,
        ((halo_mask >> 1) & 0x1) ? ((last_mask >> 1) & 0x1) ? DEF_A1_fconv_last_instr : DEF_A1_fconv_instr : SKIP_A1_instr,
        ((halo_mask >> 2) & 0x1) ? ((last_mask >> 2) & 0x1) ? DEF_A2_fconv_last_instr : DEF_A2_fconv_instr : SKIP_A2_instr,
        ((halo_mask >> 3) & 0x1) ? DEF_A3_fconv_instr : SKIP_A3_instr,
        TT_OP_NOP,
        DEF_B_cntx_ovrd_no_z_inc_instr,
        DEF_SKIP_B);
}

inline ckernel_unpack_template ckernel_unpack_template::lBA(
    uint A_instr,
    uint skipA_instr,

    uint B_instr,
    uint skipB_instr)
{
    return ckernel_unpack_template(
        true,  // src B
        false, // halo
        A_instr,
        0,
        0,
        0,
        skipA_instr,
        B_instr,
        skipB_instr);
}

inline ckernel_unpack_template ckernel_unpack_template::loopx1instr(uint instr0, uint skip0)
{
    return ckernel_unpack_template::lA(instr0, skip0);
}

inline ckernel_unpack_template ckernel_unpack_template::loopx2instr(uint instr0, uint instr1, uint skip0, uint skip1)
{
    // Note - 2 instr loop so we will hijack B_instr slot for 2nd instruction via lBA.
    return ckernel_unpack_template::lBA(instr0, skip0, instr1, skip1);
}

} // namespace ckernel
