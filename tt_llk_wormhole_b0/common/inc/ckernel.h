// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

/**
 * @file ckernel.h
 * @brief Core Kernel Infrastructure for Wormhole B0 Tensix Architecture
 *
 * This header provides the fundamental infrastructure for TT-LLK (Tenstorrent Low-Level Kernel)
 * operations on Wormhole B0 Tensix cores. It includes hardware abstraction, synchronization
 * primitives, memory management, and core system utilities.
 *
 * @author Tenstorrent AI ULC
 * @version 1.0
 * @date 2025
 *
 * # Key Components
 * - **Synchronization**: tensix_sync(), mop_sync(), semaphore operations
 * - **Memory Management**: Configuration register access, mailbox communication
 * - **Hardware Control**: Register I/O, address generation, state management
 * - **Debug Support**: Performance monitoring, event tracing, runtime profiling
 *
 * # Usage Example
 * ```cpp
 * #include "ckernel.h"
 *
 * // Initialize kernel state
 * ckernel::reset_cfg_state_id();
 * ckernel::reset_dest_offset_id();
 *
 * // Perform synchronized operation
 * ckernel::tensix_sync();
 * operation();
 * ckernel::mop_sync();
 * ```
 *
 * @warning This header contains low-level hardware interfaces. Incorrect usage
 *          can lead to hardware lockups or data corruption.
 *
 * @see ckernel_ops.h for instruction generation
 * @see ckernel_template.h for MOP programming
 */

#pragma once

#include "ckernel_instr_params.h"
#include "llk_defs.h"
#include "risc_attribs.h"

// MT: This should be dissolved and moved to the appropriate place
#include "tensix.h"

/**
 * @defgroup CompilerOptimizations Compiler Optimization Macros
 * @brief Macros for performance optimization and code generation hints
 * @{
 */

/**
 * @brief Compiler hint that a branch is unlikely to be taken
 *
 * This macro provides the compiler with branch prediction information to optimize
 * instruction scheduling and cache behavior.
 *
 * @param condition The condition expression to evaluate
 * @return The boolean value of condition, with unlikely prediction hint
 *
 * @code
 * if (UNLIKELY(error_condition)) {
 *     handle_error();  // This branch is optimized as unlikely
 * }
 * @endcode
 */
#define UNLIKELY(condition) __builtin_expect(static_cast<bool>(condition), 0)

/**
 * @brief Force loop unrolling with specified factor
 *
 * @param factor Number of loop iterations to unroll
 *
 * @code
 * UNROLL_LOOP(4)
 * for (int i = 0; i < 8; i++) {
 *     // Loop will be unrolled 4x for performance
 * }
 * @endcode
 */
#define UNROLL_LOOP(factor) GCC unroll factor

/**
 * @brief Force function inlining for critical performance paths
 *
 * Use this attribute for functions that must be inlined to avoid
 * function call overhead in performance-critical code.
 */
#define TT_ALWAYS_INLINE inline __attribute__((always_inline))

/** @} */ // end of CompilerOptimizations group

/**
 * @defgroup ConfigurationOptions Compile-time Configuration Options
 * @brief Feature flags and configuration macros
 * @{
 */

/**
 * @brief Enable destination buffer double buffering
 *
 * When enabled (1), allows overlapped computation and data movement for improved
 * pipeline utilization. When disabled (0), uses single buffer mode.
 *
 * @note Double buffering requires additional memory but improves throughput
 */
#ifndef EN_DEST_DOUBLE_BUFFERING
#define EN_DEST_DOUBLE_BUFFERING 1
#endif

/**
 * @brief Enable local memory optimization
 *
 * When enabled (1), utilizes core-local memory for frequently accessed data.
 * When disabled (0), uses standard L1 memory access patterns.
 */
#ifndef LOCAL_MEM_EN
#define LOCAL_MEM_EN 0
#endif

/**
 * @brief Enable debug instrumentation for TTI (Tensix Tensor Instruction) operations
 *
 * When enabled (1), adds debug hooks to TTI instruction generation.
 * Should be disabled (0) in production for optimal performance.
 */
#ifndef GPR_DEBUG_TTI
#define GPR_DEBUG_TTI 0
#endif

/**
 * @brief Enable debug instrumentation for register file operations
 *
 * When enabled (1), adds debug tracking for register file accesses.
 * Should be disabled (0) in production for optimal performance.
 */
#ifndef GPR_DEBUG_REGFILE
#define GPR_DEBUG_REGFILE 0
#endif

/** @} */ // end of ConfigurationOptions group

#include <cstdint>

#include "ckernel_include.h"

/**
 * @namespace ckernel
 * @brief Core kernel functionality and hardware abstraction layer
 *
 * This namespace contains all core kernel infrastructure including hardware register
 * access, synchronization primitives, and system utilities for Wormhole B0 Tensix cores.
 */
namespace ckernel
{

/**
 * @defgroup HardwareConstants Hardware Configuration Constants
 * @brief Hardware-specific constants and control values
 * @{
 */

/**
 * @brief Pack counter flush control bits
 *
 * Bitmask used to flush all pack operation counters during reset or reconfiguration.
 * Combines flags for per-plane, reads-per-plane, and XYs-per-tile counters.
 *
 * @note This constant is used internally by pack reset operations
 */
constexpr uint PACK_FLUSH_COUNTERS = // counters flush
    (1 << PACK_COUNTERS_SEC2_pack_per_xy_plane_SHAMT) | (1 << PACK_COUNTERS_SEC2_pack_reads_per_xy_plane_SHAMT) |
    (1 << PACK_COUNTERS_SEC2_pack_xys_per_tile_SHAMT);

/**
 * @brief Default reset value for hardware registers
 *
 * Standard reset value used to initialize various hardware registers to a known state.
 */
constexpr uint RESET_VAL = 0;

/**
 * @brief Kernel execution status: operation in progress
 *
 * Status value indicating that a kernel operation is currently executing.
 * Used for inter-thread synchronization and status polling.
 */
constexpr uint KERNEL_IN_PROGRESS = 15;

/**
 * @brief Kernel execution status: operation complete
 *
 * Status value indicating that a kernel operation has completed successfully.
 * Used for inter-thread synchronization and completion detection.
 */
constexpr uint KERNEL_COMPLETE = 1;

/** @} */ // end of HardwareConstants group

/**
 * @defgroup HardwareRegisters Global Hardware Register Pointers
 * @brief Direct access pointers to Tensix hardware registers
 * @{
 */

/**
 * @brief Base pointer to main Tensix register space
 *
 * Provides direct access to the primary Tensix hardware registers.
 * Used for low-level hardware control and configuration.
 *
 * @warning Direct register access bypasses safety checks. Use with caution.
 */
extern volatile uint tt_reg_ptr *reg_base;

/**
 * @brief Base pointer to PC buffer registers
 *
 * Used for synchronization, semaphore operations, and inter-thread communication.
 * The PC buffer provides lightweight synchronization mechanisms.
 *
 * @note Access pattern: pc_buf_base[offset] where offset determines function
 */
extern volatile uint tt_reg_ptr *pc_buf_base;

/**
 * @brief Base pointer to general purpose register file
 *
 * Provides access to the Tensix general purpose register file for data storage
 * and temporary value management during operations.
 */
extern volatile uint tt_reg_ptr *regfile;

/** @} */ // end of HardwareRegisters group

} // namespace ckernel

/**
 * @brief Global instruction buffer for hardware instruction sequences
 *
 * Low-level buffer used to store generated instruction sequences before execution.
 * This buffer interfaces directly with the Tensix instruction execution engine.
 *
 * @warning This is a low-level hardware interface. Do not access directly.
 * @see ckernel_template.h for safe instruction sequence generation
 */
extern volatile uint32_t __instrn_buffer[];

namespace ckernel
{

/**
 * @defgroup InstructionManagement Instruction Buffer Management
 * @brief Instruction generation and execution infrastructure
 * @{
 */

/**
 * @brief Type-safe reference to the global instruction buffer
 *
 * Provides a type-safe interface to the instruction buffer for generated
 * instruction sequences. Used by template system for MOP programming.
 *
 * @note Use ckernel_template methods instead of direct access
 */
constexpr inline volatile uint32_t(tt_reg_ptr &instrn_buffer)[] = __instrn_buffer;

/** @} */ // end of InstructionManagement group

/**
 * @defgroup CommunicationInfrastructure Inter-Thread Communication
 * @brief Mailbox and communication primitives
 * @{
 */

/**
 * @brief Mailbox base pointers for inter-thread communication
 *
 * Array of pointers to mailbox registers for communication between different
 * Tensix threads (RISC-V cores). Index corresponds to thread ID.
 *
 * @note Use mailbox_read() and mailbox_write() functions for safe access
 */
extern volatile uint tt_reg_ptr *mailbox_base[4];

/**
 * @brief Debug event scratch memory for performance monitoring
 *
 * Scratch memory space used for recording debug events, performance counters,
 * and runtime profiling information.
 */
extern volatile uint tt_reg_ptr *dbg_event_scratch;

/**
 * @brief TRISC to L1 mailbox for L1 memory communication
 *
 * Specialized mailbox for communication between TRISC threads and L1 memory
 * subsystem. Used for memory management and data movement coordination.
 */
extern volatile uint tt_reg_ptr *trisc_l1_mailbox;

/**
 * @brief Debug buffer in L1 memory for trace data
 *
 * L1 memory buffer used for storing debug traces, performance data,
 * and runtime diagnostics information.
 */
extern volatile uint8_t tt_l1_ptr *debug_buffer;

/** @} */ // end of CommunicationInfrastructure group

/**
 * @defgroup GlobalState Global Kernel State Management
 * @brief Kernel state variables and configuration tracking
 * @{
 */

/**
 * @brief Current configuration state ID (0 or 1)
 *
 * Tracks the active configuration state for double-buffered configuration.
 * Allows switching between two hardware configuration states for overlapped
 * reconfiguration and execution.
 *
 * @note Use flip_cfg_state_id() and reset_cfg_state_id() for safe manipulation
 */
extern uint32_t cfg_state_id;

/**
 * @brief Current destination buffer offset ID (0 or 1)
 *
 * Tracks the active destination buffer section for double-buffered operations.
 * Enables ping-pong buffering between computation and data movement.
 *
 * @note Use update_dest_offset_id() and reset_dest_offset_id() for safe manipulation
 */
extern uint32_t dest_offset_id;

/**
 * @brief Current debug event index for trace recording
 *
 * Index into the debug event buffer for recording performance events
 * and runtime traces. Incremented automatically during debug operations.
 */
extern uint32_t dbg_event_index;

/**
 * @brief End boundary for debug event recording
 *
 * Upper limit for debug event recording to prevent buffer overruns.
 * Debug operations will stop recording when dbg_event_index reaches this value.
 */
extern uint32_t dbg_event_end;

/** @} */ // end of GlobalState group

/**
 * @defgroup DebugInfrastructure Debug and Profiling Support
 * @brief Debug mailbox and performance monitoring
 * @{
 */

/**
 * @brief Debug mailbox for performance data collection
 *
 * Specialized mailbox for collecting performance counters, timing data,
 * and runtime statistics for analysis and optimization.
 */
extern volatile uint16_t tt_reg_ptr *debug_mailbox_base;

/**
 * @brief Current index into debug mailbox
 *
 * Tracks the current position in the debug mailbox for sequential
 * data recording. Incremented automatically during debug operations.
 */
extern uint8_t mailbox_index;

/**
 * @brief End boundary for debug mailbox
 *
 * Upper limit for debug mailbox to prevent overflow. Debug operations
 * will stop when mailbox_index reaches this value.
 */
const extern uint8_t mailbox_end;

/** @} */ // end of DebugInfrastructure group

// Internal scope to namespace methods only (C++ does not allow namespace private ownership)
namespace internal
{
}

/**
 * @defgroup SynchronizationPrimitives Core Synchronization Functions
 * @brief Critical synchronization mechanisms for Tensix core coordination
 * @{
 */

/**
 * @brief Full Tensix core synchronization barrier
 *
 * Ensures all pending operations on the Tensix core complete before proceeding.
 * This is a heavyweight synchronization primitive that guarantees complete
 * pipeline drain and memory consistency.
 *
 * @details The function works by:
 * 1. Writing to PC buffer to order all pending writes
 * 2. Reading from PC buffer, which blocks until the core is idle
 * 3. Ensuring all instructions, memory operations, and DMA transfers complete
 *
 * @warning This is an expensive operation (~10-50 cycles). Use sparingly.
 *
 * @note Use this function when:
 * - Switching between major operation phases
 * - Before critical configuration changes
 * - When absolute ordering is required
 *
 * @code
 * // Example: Ensure completion before configuration change
 * compute_operation();
 * ckernel::tensix_sync();  // Wait for completion
 * reconfigure_hardware();
 * @endcode
 *
 * @see mop_sync() for lighter-weight MOP-only synchronization
 */
inline void tensix_sync()
{
    volatile uint foo     = 0;
    volatile uint *fooptr = &foo;
    // Write to pc buffer to push all writes ahead of us.. otherwise, the pc buffer read can bypass older writes
    pc_buf_base[1] = foo;

    // Now read -- this read will block until we're idle
    *fooptr = pc_buf_base[1];
}

/**
 * @brief Micro-operation (MOP) synchronization barrier
 *
 * Ensures all pending MOP (Micro-Operation) sequences complete before proceeding.
 * This is a lighter-weight synchronization primitive that waits specifically
 * for MOP execution completion without full core synchronization.
 *
 * @details The function works by:
 * 1. Writing to PC buffer to order pending writes
 * 2. Reading from PC buffer, which blocks until MOPs complete
 * 3. Does not wait for other pipeline stages or DMA operations
 *
 * @note This is more efficient than tensix_sync() when only MOP completion is needed.
 *
 * @performance Typically 5-20 cycles, much faster than tensix_sync()
 *
 * @code
 * // Example: Wait for MOP completion before next sequence
 * execute_mop_sequence();
 * ckernel::mop_sync();     // Wait for MOP completion
 * execute_next_mop();
 * @endcode
 *
 * @see tensix_sync() for full core synchronization
 */
inline void mop_sync()
{
    volatile uint foo     = 0;
    volatile uint *fooptr = &foo;
    // Write to pc buffer to push all writes ahead of us.. otherwise, the pc buffer read can bypass older writes
    pc_buf_base[2] = foo;

    // Now read -- this read will block until mops are done
    *fooptr = pc_buf_base[2];
}

/** @} */ // end of SynchronizationPrimitives group

/**
 * @defgroup UtilityFunctions Utility and Helper Functions
 * @brief General utility functions for hardware operations
 * @{
 */

/**
 * @brief Synchronize register file write operation
 *
 * Ensures a register file write operation completes before proceeding.
 * Used to maintain data consistency when register values are critical
 * for subsequent operations.
 *
 * @param index Register index to synchronize
 *
 * @note Implementation is provided later in the file
 */
inline void sync_regfile_write(const uint index);

/**
 * @brief Validate field value fits within specified bit width
 *
 * Compile-time and runtime validation to ensure a value fits within
 * the specified number of bits. Used to prevent hardware field overflow.
 *
 * @tparam T Value type (typically uint32_t, uint16_t, etc.)
 * @param val Value to validate
 * @param wid Bit width of the target field
 * @return true if value fits within bit width, false otherwise
 *
 * @code
 * // Validate 4-bit field value
 * uint32_t increment = 12;
 * if (ckernel::is_valid(increment, 4)) {
 *     // Safe to use increment in 4-bit field
 *     configure_address_increment(increment);
 * }
 * @endcode
 */
template <typename T>
static constexpr bool is_valid(const T val, const uint8_t wid)
{
    const T mask = (1 << wid) - 1;
    return (val & mask) == val;
}

/** @} */ // end of UtilityFunctions group

/**
 * @defgroup RegisterIO Hardware Register I/O Operations
 * @brief Low-level register access and hardware control
 * @{
 */

/**
 * @brief Write to memory-mapped I/O register
 *
 * Writes data to a hardware register in the specified register space.
 * Handles address encoding and register space selection.
 *
 * @param space Register space identifier (selects hardware subsystem)
 * @param addr Register address within the space (6-bit address)
 * @param data 32-bit data value to write
 *
 * @note The address is limited to 6 bits (0x3F), with space providing
 *       the upper bits for complete address generation.
 *
 * @warning Direct register writes bypass safety checks. Ensure register
 *          compatibility and timing requirements are met.
 *
 * @code
 * // Write to configuration register
 * ckernel::mmio_register_write(CFG_SPACE, 0x10, config_value);
 * @endcode
 */
inline void mmio_register_write(register_space_e space, uint addr, uint data)
{
    const uint regaddr = (space << 6) | (addr & 0x3F);
    reg_base[regaddr]  = data;
}

/**
 * @defgroup SemaphoreOperations Semaphore Synchronization
 * @brief Hardware semaphore operations for inter-thread coordination
 * @{
 */

/**
 * @brief Read current semaphore value
 *
 * Reads the current value of a hardware semaphore for synchronization checking.
 * Semaphores are used for coordinating operations between different threads
 * and pipeline stages.
 *
 * @param index Semaphore index (identifies specific semaphore)
 * @return Current semaphore value (typically 0 or positive count)
 *
 * @note Semaphore values typically represent resource counts or completion status
 *
 * @code
 * // Wait for operation completion
 * while (ckernel::semaphore_read(MATH_COMPLETE_SEM) == 0) {
 *     // Wait for math operation to complete
 * }
 * @endcode
 */
inline uint8_t semaphore_read(const uint8_t index)
{
    return pc_buf_base[PC_BUF_SEMAPHORE_BASE + index];
}

/**
 * @brief Post (signal) a semaphore
 *
 * Signals completion or availability by setting the semaphore to 0.
 * This operation notifies waiting threads that a resource is available
 * or an operation has completed.
 *
 * @param index Semaphore index to signal
 *
 * @note In this implementation, posting sets semaphore to 0 (completion signal)
 *
 * @code
 * // Signal operation completion
 * complete_operation();
 * ckernel::semaphore_post(OPERATION_COMPLETE_SEM);
 * @endcode
 */
inline void semaphore_post(const uint8_t index)
{
    pc_buf_base[PC_BUF_SEMAPHORE_BASE + index] = 0;
}

/**
 * @brief Acquire (get) a semaphore
 *
 * Acquires a semaphore by setting it to 1, indicating resource usage
 * or operation initiation. Used to claim exclusive access or signal
 * operation start.
 *
 * @param index Semaphore index to acquire
 *
 * @note In this implementation, getting sets semaphore to 1 (busy signal)
 *
 * @code
 * // Acquire resource for exclusive access
 * ckernel::semaphore_get(RESOURCE_LOCK_SEM);
 * use_exclusive_resource();
 * ckernel::semaphore_post(RESOURCE_LOCK_SEM);  // Release
 * @endcode
 */
inline void semaphore_get(const uint8_t index)
{
    pc_buf_base[PC_BUF_SEMAPHORE_BASE + index] = 1;
}

/** @} */ // end of SemaphoreOperations group

/**
 * @defgroup TensixSemaphores Advanced Tensix Thread Semaphores
 * @brief Hardware-accelerated semaphore operations with stall control
 * @{
 */

/**
 * @brief Post a Tensix thread semaphore with optional stall synchronization
 *
 * Advanced semaphore post operation that can optionally wait for specific
 * hardware resources before signaling. Uses Tensix Thread Instruction (TTI)
 * for hardware-accelerated semaphore management.
 *
 * @tparam WaitRes Stall resource to wait for (default: no stall)
 * @param index Tensix semaphore index to post
 *
 * @details If WaitRes is specified, the function will stall until the
 *          specified resource is available before posting the semaphore.
 *          This ensures proper ordering of operations.
 *
 * @code
 * // Post semaphore immediately
 * ckernel::t6_semaphore_post<>(sem_index);
 *
 * // Post semaphore after math operations complete
 * ckernel::t6_semaphore_post<p_stall::MATH>(sem_index);
 * @endcode
 */
template <uint WaitRes = p_stall::NONE>
inline void t6_semaphore_post(const uint8_t index)
{
    if constexpr (WaitRes != p_stall::NONE)
    {
        TTI_STALLWAIT(p_stall::STALL_SYNC, WaitRes);
    }

    TTI_SEMPOST(semaphore::t6_sem(index));
}

/**
 * @brief Acquire a Tensix thread semaphore with optional stall synchronization
 *
 * Advanced semaphore acquire operation that can optionally wait for specific
 * hardware resources before acquiring. Uses hardware-accelerated semaphore
 * management for optimal performance.
 *
 * @tparam WaitRes Stall resource to wait for (default: no stall)
 * @param index Tensix semaphore index to acquire
 *
 * @details If WaitRes is specified, the function will stall until the
 *          specified resource is available before acquiring the semaphore.
 *
 * @code
 * // Acquire semaphore immediately
 * ckernel::t6_semaphore_get<>(sem_index);
 *
 * // Acquire semaphore after pack operations complete
 * ckernel::t6_semaphore_get<p_stall::PACK>(sem_index);
 * @endcode
 */
template <uint WaitRes = p_stall::NONE>
inline void t6_semaphore_get(const uint8_t index)
{
    if constexpr (WaitRes != p_stall::NONE)
    {
        TTI_STALLWAIT(p_stall::STALL_SYNC, WaitRes);
    }

    TTI_SEMGET(semaphore::t6_sem(index));
}

/**
 * @brief Wait for semaphore to reach maximum value
 *
 * Blocks execution until the specified semaphore reaches its maximum
 * configured value. Used for waiting on resource availability or
 * completion of batch operations.
 *
 * @tparam WaitRes Resource to wait for during stall
 * @param index Tensix semaphore index to monitor
 *
 * @note This function will block indefinitely until condition is met
 */
template <uint WaitRes>
inline void t6_semaphore_wait_on_max(const uint8_t index)
{
    TTI_SEMWAIT(WaitRes, semaphore::t6_sem(index), p_stall::STALL_ON_MAX);
}

/**
 * @brief Wait for semaphore to reach zero
 *
 * Blocks execution until the specified semaphore reaches zero.
 * Commonly used for waiting on operation completion or resource
 * release.
 *
 * @tparam WaitRes Resource to wait for during stall
 * @param index Tensix semaphore index to monitor
 *
 * @note This function will block indefinitely until condition is met
 */
template <uint WaitRes>
inline void t6_semaphore_wait_on_zero(const uint8_t index)
{
    TTI_SEMWAIT(WaitRes, semaphore::t6_sem(index), p_stall::STALL_ON_ZERO);
}

/**
 * @brief Initialize a Tensix thread semaphore with value range
 *
 * Configures a Tensix semaphore with specified minimum and maximum values.
 * The semaphore will operate within this range, enabling counter-based
 * synchronization patterns.
 *
 * @param index Tensix semaphore index to initialize
 * @param min_value Minimum semaphore value (typically 0)
 * @param max_value Maximum semaphore value (sets capacity)
 *
 * @note The semaphore will start at the minimum value after initialization
 *
 * @code
 * // Initialize semaphore for 4-resource pool
 * ckernel::t6_semaphore_init(resource_sem, 0, 4);
 * @endcode
 */
inline void t6_semaphore_init(const uint8_t index, const uint8_t min_value, const uint8_t max_value)
{
    TTI_SEMINIT(max_value, min_value, semaphore::t6_sem(index));
}

/** @} */ // end of TensixSemaphores group

/**
 * @defgroup MutexOperations Mutual Exclusion (Mutex) Operations
 * @brief Hardware mutex operations for exclusive resource access
 * @{
 */

/**
 * @brief Acquire a hardware mutex for exclusive access
 *
 * Acquires a Tensix hardware mutex for exclusive access to shared resources.
 * This operation will block until the mutex becomes available, ensuring
 * atomic access to critical sections.
 *
 * @param index Mutex index to acquire
 *
 * @note Always pair with t6_mutex_release() to prevent deadlocks
 *
 * @code
 * // Protect critical section with mutex
 * ckernel::t6_mutex_acquire(shared_resource_mutex);
 * access_shared_resource();
 * ckernel::t6_mutex_release(shared_resource_mutex);
 * @endcode
 */
inline void t6_mutex_acquire(const uint8_t index)
{
    TTI_ATGETM(index);
}

/**
 * @brief Release a hardware mutex
 *
 * Releases a previously acquired Tensix hardware mutex, allowing other
 * threads to acquire exclusive access to the protected resource.
 *
 * @param index Mutex index to release
 *
 * @warning Only release mutexes that were previously acquired by this thread
 *
 * @see t6_mutex_acquire() for mutex acquisition
 */
inline void t6_mutex_release(const uint8_t index)
{
    TTI_ATRELM(index);
}

/** @} */ // end of MutexOperations group

/**
 * @defgroup ConfigurationManagement Configuration State Management
 * @brief Hardware configuration register access and state management
 * @{
 */

/**
 * @brief Calculate configuration register address for current state
 *
 * Returns the effective address of a configuration register accounting for
 * the current configuration state ID. Supports double-buffered configuration
 * by mapping to different register banks.
 *
 * @param cfg_addr32 Base configuration register address
 * @return Effective address including state offset
 *
 * @note This function automatically handles state ID mapping for double-buffered config
 */
inline uint cfg_addr(uint cfg_addr32)
{
    return (cfg_state_id == 0) ? cfg_addr32 : (CFG_STATE_SIZE * 4) + cfg_addr32;
}

/**
 * @brief Write to configuration register with state management
 *
 * Writes data to a configuration register, automatically handling the current
 * configuration state for double-buffered operation. Ensures writes go to
 * the correct register bank.
 *
 * @param cfg_addr32 Configuration register address (base address)
 * @param data 32-bit data value to write
 *
 * @note Configuration registers are local to prevent direct access that
 *       might ignore the current state ID
 *
 * @code
 * // Write to configuration register
 * ckernel::cfg_write(UNPACK_CFG_REG, config_value);
 * @endcode
 */
inline void cfg_write(uint cfg_addr32, uint data)
{
    // Declared here instead of globally to prevent direct access, which might ignore current state ID
    volatile uint tt_reg_ptr *cfg_regs = reinterpret_cast<volatile uint tt_reg_ptr *>(TENSIX_CFG_BASE);
    cfg_regs[cfg_addr(cfg_addr32)]     = data;
}

/**
 * @brief Read from configuration register with state management
 *
 * Reads data from a configuration register, automatically handling the current
 * configuration state for double-buffered operation. Ensures reads come from
 * the correct register bank.
 *
 * @param cfg_addr32 Configuration register address (base address)
 * @return 32-bit register value
 *
 * @note Configuration registers are local to prevent direct access that
 *       might ignore the current state ID
 *
 * @code
 * // Read current configuration
 * uint32_t current_config = ckernel::cfg_read(PACK_CFG_REG);
 * @endcode
 */
inline uint cfg_read(uint cfg_addr32)
{
    // Declared here instead of globally to prevent direct access, which might ignore current state ID
    volatile uint *cfg_regs = reinterpret_cast<volatile uint *>(TENSIX_CFG_BASE);
    return cfg_regs[cfg_addr(cfg_addr32)];
}

// Return pointer to CFG with the right base address for the current state
inline volatile uint *tt_reg_ptr get_cfg_pointer()
{
    if (cfg_state_id == 0)
    {
        return reinterpret_cast<volatile uint tt_reg_ptr *>(TENSIX_CFG_BASE);
    }

    return reinterpret_cast<volatile uint tt_reg_ptr *>(TENSIX_CFG_BASE + CFG_STATE_SIZE * 16);
}

inline volatile uint short *tt_reg_ptr get_cfg16_pointer()
{
    if (cfg_state_id == 0)
    {
        return reinterpret_cast<volatile uint short tt_reg_ptr *>(TENSIX_CFG_BASE);
    }

    return reinterpret_cast<volatile uint short tt_reg_ptr *>(TENSIX_CFG_BASE + CFG_STATE_SIZE * 16);
}

inline void flip_cfg_state_id()
{
    cfg_state_id = 1 - cfg_state_id;
    TT_SETC16(CFG_STATE_ID_StateID_ADDR32, cfg_state_id); // Program the current state ID
    TTI_NOP;
    TTI_NOP;
}

inline void reset_cfg_state_id()
{
    cfg_state_id = 0;
}

inline void reset_dest_offset_id()
{
    dest_offset_id = 0;
}

inline void update_dest_offset_id()
{
    // ping-pong between 0 and 1
    dest_offset_id = 1 - dest_offset_id;
}

inline uint32_t get_dest_buffer_base()
{
    return (0 != dest_offset_id) ? DEST_REGISTER_HALF_SIZE : 0x0;
}

// MOP run version without zmask
inline void mop_run(const uint8_t type, const uint8_t count)
{
    TTI_MOP(type, count - 1, 0); // Run the MOP
}

// Register read (workaround for bug
// tenstorrent/tensix#976
// now handled by the compiler)
// workaround is needed only for GS
inline uint reg_read(uint32_t addr)
{
    volatile uint tt_reg_ptr *p_reg = reinterpret_cast<volatile uint tt_reg_ptr *>(addr);
    return p_reg[0];
}

inline void reg_write(uint32_t addr, uint32_t data)
{
    volatile uint tt_reg_ptr *p_reg = reinterpret_cast<volatile uint tt_reg_ptr *>(addr);
    p_reg[0]                        = data;
}

inline void wait(uint32_t cycles)
{
    volatile uint tt_reg_ptr *clock_lo = reinterpret_cast<volatile uint tt_reg_ptr *>(RISCV_DEBUG_REG_WALL_CLOCK_L);
    volatile uint tt_reg_ptr *clock_hi = reinterpret_cast<volatile uint tt_reg_ptr *>(RISCV_DEBUG_REG_WALL_CLOCK_H);
    uint64_t wall_clock_timestamp      = clock_lo[0] | ((uint64_t)clock_hi[0] << 32);
    uint64_t wall_clock                = 0;
    do
    {
        wall_clock = clock_lo[0] | ((uint64_t)clock_hi[0] << 32);
    } while (wall_clock < (wall_clock_timestamp + cycles));
}

// Clear dest
inline void zeroacc()
{
    // Clear dest
    addr_mod_t {
        .srca = {.incr = 0},
        .srcb = {.incr = 0},
        .dest = {.incr = 0},
    }
        .set(ADDR_MOD_1);
    TT_ZEROACC(p_zeroacc::CLR_ALL, ADDR_MOD_1, 0);
}

inline void zerosrc()
{
    TTI_ZEROSRC(0, 0, 1, 3); // Zero all srcA&B banks
}

inline void sync_regfile_write(const uint index)
{
    volatile uint foo     = 0x0;
    volatile uint *fooptr = &foo;
    *fooptr               = regfile[index];
}

inline void cfg_rmw(uint32_t cfg_addr32, uint32_t cfg_shamt, uint32_t cfg_mask, uint32_t val)
{
    uint32_t wrdata = val;

    // Avoid multiplication of variables!
    // const uint32_t addr = (cfg_state_id * CFG_STATE_SIZE * 4) + cfg_addr32;
    const uint32_t addr = (cfg_state_id == 0) ? cfg_addr32 : (CFG_STATE_SIZE * 4) + cfg_addr32;

    // Declared here instead of globally to prevent direct access, which might ignore current state ID
    volatile uint tt_reg_ptr *cfg_regs = reinterpret_cast<volatile uint tt_reg_ptr *>(TENSIX_CFG_BASE);
    uint32_t cfg_data                  = cfg_regs[addr];

    // Shift and mask wrdata to properly align within 32-bit DWORD
    wrdata <<= cfg_shamt;
    wrdata &= cfg_mask;

    // Zero-out relevant bits in cfg data
    cfg_data &= ~cfg_mask;

    // Or new data bits
    cfg_data |= wrdata;

    // Update cfg regs
    cfg_regs[addr] = cfg_data;
}

inline void cfg_rmw_gpr(uint32_t cfg_addr32, uint32_t cfg_shamt, uint32_t cfg_mask, uint32_t gpr_index)
{
    const uint32_t wrdata = regfile[gpr_index];
    cfg_rmw(cfg_addr32, cfg_shamt, cfg_mask, wrdata);
}

template <uint CfgAddr32, uint Shamt, uint Mask>
inline void cfg_reg_rmw_tensix(uint32_t val)
{
    uint32_t wrdata = val << Shamt;
    uint8_t mask_b0 = Mask & 0xff;

    if (mask_b0 != 0)
    {
        uint8_t data_b0 = wrdata & 0xff;
        TT_RMWCIB0(mask_b0, data_b0, CfgAddr32);
    }
    wrdata >>= 8;
    uint8_t mask_b1 = (Mask >> 8) & 0xff;

    if (mask_b1 != 0)
    {
        uint8_t data_b1 = (wrdata) & 0xff;
        TT_RMWCIB1(mask_b1, data_b1, CfgAddr32);
    }

    wrdata >>= 8;
    uint8_t mask_b2 = (Mask >> 16) & 0xff;

    if (mask_b2 != 0)
    {
        uint8_t data_b2 = (wrdata) & 0xff;
        TT_RMWCIB2(mask_b2, data_b2, CfgAddr32);
    }

    wrdata >>= 8;
    uint8_t mask_b3 = (Mask >> 24) & 0xff;
    if (mask_b3 != 0)
    {
        uint8_t data_b3 = (wrdata) & 0xff;
        TT_RMWCIB3(mask_b3, data_b3, CfgAddr32);
    }
}

inline void mailbox_write(const uint8_t thread, const uint32_t data)
{
    mailbox_base[thread + 1][0] = data;
}

// Blocking read
inline uint32_t mailbox_read(const uint8_t thread)
{
    return mailbox_base[thread + 1][0];
}

inline bool mailbox_not_empty(const uint8_t thread)
{
    return mailbox_base[thread + 1][1] > 0;
}

inline void mailbox_write_full(const uint8_t thread, const uint32_t data)
{
    mailbox_base[thread][0] = data;
}

// Blocking read
inline uint32_t mailbox_read_full(const uint8_t thread)
{
    return mailbox_base[thread][0];
}

inline bool mailbox_not_empty_full(const uint8_t thread)
{
    return mailbox_base[thread][1] > 0;
}

inline void trisc_l1_mailbox_write(const uint data)
{
    trisc_l1_mailbox[0] = data;
}

inline uint trisc_l1_mailbox_read()
{
    return trisc_l1_mailbox[0];
}

template <class T>
inline std::uint32_t memory_cast(T *object_ptr)
{
    return reinterpret_cast<uint32_t>(object_ptr);
}

inline void record_mailbox_value(uint16_t event_value)
{
    if (mailbox_index < mailbox_end)
    {
        debug_mailbox_base[mailbox_index] = event_value;
        mailbox_index++;
    }
}

inline void record_mailbox_value_with_index(uint8_t index, uint16_t event_value)
{
    if (index < mailbox_end)
    {
        debug_mailbox_base[index] = event_value;
    }
}

// Initialize debug scratch mailbox values and range
inline void clear_mailbox_values(uint16_t value = 0)
{
    for (int i = 0; i < mailbox_end; i++)
    {
        debug_mailbox_base[i] = value;
    }
}

inline uint64_t read_wall_clock()
{
    uint32_t timestamp_low  = reg_read(RISCV_DEBUG_REG_WALL_CLOCK_L);
    uint32_t timestamp_high = reg_read(RISCV_DEBUG_REG_WALL_CLOCK_H);
    return ((uint64_t)timestamp_high << 32) | timestamp_low;
}

inline void record_kernel_runtime(uint64_t kernel_runtime)
{
    debug_mailbox_base[mailbox_end - 4] = kernel_runtime & 0xffff;
    debug_mailbox_base[mailbox_end - 3] = (kernel_runtime >> 16) & 0xffff;
    debug_mailbox_base[mailbox_end - 2] = (kernel_runtime >> 32) & 0xffff;
    debug_mailbox_base[mailbox_end - 1] = (kernel_runtime >> 48) & 0xffff;
}

void debug_dump(const uint8_t *data, uint32_t byte_size);
void debug_dump_seek(uint8_t offset);

inline void init_prng_seed(const uint seed)
{
    // The seed for PRNG should at least be initialized during chip boot-up time.
    volatile uint tt_reg_ptr *cfg  = get_cfg_pointer();
    cfg[PRNG_SEED_Seed_Val_ADDR32] = seed;

    // TODO: ckernel::wait does not work properly. Use ckernel::wait when fixed.
    for (int i = 0; i < 600; i++)
    {
        TTI_SFPNOP;
    }
}

inline constexpr bool is_valid_instruction_mode(InstrModLoadStore mode)
{
    return mode == InstrModLoadStore::INT32_2S_COMP || mode == InstrModLoadStore::INT32 || mode == InstrModLoadStore::LO16;
}

} // namespace ckernel
