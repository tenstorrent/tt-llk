// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "ckernel_instr_params.h"
#include "ckernel_ops.h"
#include "llk_defs.h"
#include "risc_attribs.h"

// MT: This should be dissolved and moved to the appropriate place
#include "tensix.h"

// This header is included on non-trisc builds, for reasons
// unknown. lltt is only available on trisc
#if defined(COMPILE_FOR_TRISC)
#include <utility>

#include "lltt.h"
#endif

/**
 * @file ckernel.h
 * @brief Main compute kernel framework header providing core utilities and hardware abstraction
 *
 * @details This file serves as the primary interface for the compute kernel framework, providing
 * essential utilities for hardware interaction, synchronization, configuration management, and
 * inter-thread communication. It forms the foundation upon which all kernel operations are built.
 *
 * **Core Framework Components:**
 * - **Hardware Abstraction**: Register access, MMIO operations, and hardware configuration
 * - **Synchronization Primitives**: Semaphores, mutexes, and thread coordination mechanisms
 * - **Configuration Management**: State management, context switching, and parameter handling
 * - **Inter-Thread Communication**: Mailbox operations for data exchange between threads
 * - **Debug Infrastructure**: Performance monitoring, event tracking, and diagnostic utilities
 * - **Memory Management**: Address calculation, buffer management, and memory coordination
 *
 * **Architecture Integration:**
 * - Direct hardware register access for maximum performance
 * - Tensix-specific optimizations and instruction sequences
 * - Multi-threaded coordination for pipeline efficiency
 * - Hardware-accelerated synchronization and communication
 *
 * **Performance Characteristics:**
 * - Zero-overhead abstractions for common operations
 * - Template-based optimizations for compile-time efficiency
 * - Hardware-specific instruction sequences for optimal performance
 * - Minimal synchronization overhead for high-throughput operations
 *
 * **Usage Context:**
 * This header is included by all kernel implementations and provides the essential
 * infrastructure for compute operations, data movement, and thread coordination
 * across the entire Tensix compute pipeline.
 */

//
// **Compiler Optimization Macros**
//

/**
 * @brief Compiler hint for unlikely branch conditions
 * @details Provides optimization hint to compiler for branch prediction optimization
 */
#define UNLIKELY(condition) __builtin_expect(static_cast<bool>(condition), 0)

/**
 * @brief Loop unrolling directive for performance optimization
 * @details Instructs compiler to unroll loops by specified factor for better performance
 */
#define UNROLL_LOOP(factor) GCC unroll factor

//
// **Kernel Configuration Flags**
//

#ifndef EN_DEST_DOUBLE_BUFFERING
/**
 * @brief Enable destination register double-buffering
 * @details Controls whether destination registers use double-buffering for continuous operation
 */
#define EN_DEST_DOUBLE_BUFFERING 1
#endif

#ifndef LOCAL_MEM_EN
/**
 * @brief Enable local memory usage
 * @details Controls whether kernel can utilize local memory for operations
 */
#define LOCAL_MEM_EN 0
#endif

#ifndef GPR_DEBUG_TTI
/**
 * @brief Enable GPR debugging with TTI instructions
 * @details Controls debug instrumentation for General Purpose Register operations
 */
#define GPR_DEBUG_TTI 0
#endif

#ifndef GPR_DEBUG_REGFILE
/**
 * @brief Enable register file debugging
 * @details Controls debug instrumentation for register file operations
 */
#define GPR_DEBUG_REGFILE 0
#endif

/**
 * @brief Force function inlining for performance-critical code
 * @details Ensures critical functions are always inlined to eliminate call overhead
 */
#define TT_ALWAYS_INLINE inline __attribute__((always_inline))

#include <cstdint>

#include "ckernel_include.h"

namespace ckernel
{

//
// **System Constants and Configuration**
//

/**
 * @brief Pack counter flush configuration
 * @details Bit mask configuration for flushing pack operation counters
 */
constexpr uint PACK_FLUSH_COUNTERS = // counters flush
    (1 << PACK_COUNTERS_SEC2_pack_per_xy_plane_SHAMT) | (1 << PACK_COUNTERS_SEC2_pack_reads_per_xy_plane_SHAMT) |
    (1 << PACK_COUNTERS_SEC2_pack_xys_per_tile_SHAMT);

/**
 * @brief Kernel execution state constants
 * @details Standard values for tracking kernel execution state
 */
constexpr uint RESET_VAL          = 0;   ///< Reset/initial state value
constexpr uint KERNEL_IN_PROGRESS = 15;  ///< Kernel execution in progress indicator
constexpr uint KERNEL_COMPLETE    = 1;   ///< Kernel execution completed indicator

//
// **Global Hardware Interface Pointers**
//

/**
 * @brief Base pointer for register access
 * @details Primary interface for memory-mapped register operations
 */
extern volatile uint tt_reg_ptr *reg_base;

/**
 * @brief Base pointer for PC buffer operations
 * @details Interface for program counter buffer and command dispatch
 */
extern volatile uint tt_reg_ptr *pc_buf_base;

/**
 * @brief General purpose register file interface
 * @details Access to hardware general purpose register file
 */
extern volatile uint tt_reg_ptr *regfile;

#if !defined(INSTRN_BUF_BASE)
// Once tt_metal's submodule use of tt_llk is updated, this shim can
// be cleaned up.
#define INSTRN_BUFFER_TNG
} // namespace ckernel
extern volatile uint32_t __instrn_buffer[];
namespace ckernel
{
/**
 * @brief Instruction buffer for kernel instruction storage
 * @details Buffer for storing and managing kernel instruction sequences
 */
constexpr inline volatile uint32_t(tt_reg_ptr &instrn_buffer)[] = __instrn_buffer;
#else // defined(INSTRN_BUF_BASE)
extern volatile uint tt_reg_ptr *instrn_buffer;
#endif

/**
 * @brief Mailbox base pointers for inter-thread communication
 * @details Array of mailbox interfaces for thread-to-thread data exchange
 */
extern volatile uint tt_reg_ptr *mailbox_base[4];

/**
 * @brief Debug event scratch memory interface
 * @details Scratch memory area for debug event storage and monitoring
 */
extern volatile uint tt_reg_ptr *dbg_event_scratch;

/**
 * @brief TRISC-L1 mailbox interface
 * @details Specialized mailbox for TRISC to L1 memory communication
 */
extern volatile uint tt_reg_ptr *trisc_l1_mailbox;

/**
 * @brief Debug buffer for diagnostic information
 * @details L1 memory buffer for storing debug and diagnostic data
 */
extern volatile uint8_t tt_l1_ptr *debug_buffer;

//
// **Global State Variables**
//

/**
 * @brief Current configuration state identifier
 * @details Tracks active configuration state for context switching
 */
extern uint32_t cfg_state_id;

/**
 * @brief Destination register offset identifier
 * @details Tracks current destination register offset for double-buffering
 */
extern uint32_t dest_offset_id;

/**
 * @brief Debug event tracking variables
 * @details Current index and end marker for debug event management
 */
extern uint32_t dbg_event_index;
extern uint32_t dbg_event_end;

/**
 * @brief Debug mailbox interface and management
 * @details 16-bit mailbox interface with index tracking for debug operations
 */
extern volatile uint16_t tt_reg_ptr *debug_mailbox_base;
extern uint8_t mailbox_index;
const extern uint8_t mailbox_end;

// Internal scope to namespace methods only (C++ does not allow namespace private ownership)
namespace internal
{
}

//
// **Hardware Synchronization Primitives**
//

/**
 * @brief Synchronize with Tensix hardware pipeline
 *
 * @details Forces synchronization with the Tensix hardware pipeline by ensuring
 * all pending writes are completed before proceeding. Essential for maintaining
 * data consistency when hardware operations must complete before software continues.
 *
 * **Synchronization Mechanism:**
 * 1. Write to PC buffer to push all pending writes through the pipeline
 * 2. Read from PC buffer to block until all operations are idle
 * 3. Ensures all hardware units have completed their operations
 *
 * **Use Cases:**
 * - Before reading configuration that may have been recently written
 * - After critical register updates that affect subsequent operations
 * - When precise timing coordination is required between software and hardware
 * - Debug scenarios requiring deterministic hardware state
 *
 * **Performance Note:**
 * This is a blocking operation that should be used judiciously to avoid
 * unnecessary performance penalties in high-throughput scenarios.
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
 * @brief Synchronize with Memory Operation Processor (MOP)
 *
 * @details Forces synchronization with MOP operations by ensuring all pending
 * memory operations are completed before proceeding. Critical for maintaining
 * memory consistency when subsequent operations depend on MOP completion.
 *
 * **MOP Synchronization:**
 * - Ensures all memory movement operations are complete
 * - Blocks until MOP units are idle
 * - Critical for data-dependent operations
 * - Prevents memory access conflicts
 *
 * **Use Cases:**
 * - After memory transfer operations that must complete before data access
 * - Before operations that depend on specific memory contents
 * - When coordinating between different memory movement phases
 * - Debug scenarios requiring deterministic memory state
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

/**
 * @brief Synchronize register file write operation
 * @details Ensures register file write operations are completed and visible
 * @param index Register file index to synchronize
 */
inline void sync_regfile_write(const uint index);

//
// **Hardware Register Operations**
//

/**
 * @brief Validate field value against bit width constraints
 *
 * @details Template function that verifies a value fits within the specified
 * bit width. Used for compile-time and runtime validation of register field
 * values to prevent overflow and ensure hardware compatibility.
 *
 * **Validation Logic:**
 * - Creates a mask for the specified bit width
 * - Checks if value fits within the mask
 * - Compile-time evaluation when possible
 * - Runtime validation for dynamic values
 *
 * @tparam T Data type of the value to validate
 * @param val Value to validate against bit width
 * @param wid Bit width constraint (number of bits)
 * @return true if value fits within bit width, false otherwise
 */
template <typename T>
static constexpr bool is_valid(const T val, const uint8_t wid)
{
    const T mask = (1 << wid) - 1;
    return (val & mask) == val;
}

/**
 * @brief Write to memory-mapped I/O register
 *
 * @details Performs a memory-mapped register write operation to the specified
 * register space and address. Provides direct hardware register access with
 * proper address encoding for different register spaces.
 *
 * **Address Encoding:**
 * - Register space encoded in upper bits
 * - Address masked to 6-bit range for safety
 * - Direct hardware register access
 * - Immediate effect on hardware state
 *
 * **Register Spaces:**
 * - TDMA_REGS: Tensix DMA register space
 * - LOCAL_REGS: Local processor register space
 * - ADDR_COUNTERS: Address counter register space
 *
 * @param space Target register space identifier
 * @param addr Register address within the space (6 bits max)
 * @param data Data value to write to the register
 */
inline void mmio_register_write(register_space_e space, uint addr, uint data)
{
    const uint regaddr = (space << 6) | (addr & 0x3F);  // Encode space and mask address
    reg_base[regaddr]  = data;                          // Direct register write
}

//
// **Semaphore Operations**
//

/**
 * @brief Read current semaphore value
 *
 * @details Reads the current value of the specified semaphore without modifying
 * it. Used for polling semaphore state and implementing custom synchronization
 * logic based on semaphore values.
 *
 * @param index Semaphore index (0-7 for standard semaphores)
 * @return Current semaphore value
 */
inline uint8_t semaphore_read(const uint8_t index)
{
    return pc_buf_base[PC_BUF_SEMAPHORE_BASE + index];
}

/**
 * @brief Post to semaphore (increment/signal)
 *
 * @details Increments the specified semaphore value, typically used to signal
 * completion of an operation or availability of a resource. Part of the
 * producer-consumer synchronization pattern.
 *
 * @param index Semaphore index to post to
 */
inline void semaphore_post(const uint8_t index)
{
    pc_buf_base[PC_BUF_SEMAPHORE_BASE + index] = 0;  // Post operation
}

/**
 * @brief Get from semaphore (decrement/wait)
 *
 * @details Decrements the specified semaphore value, typically used to acquire
 * a resource or acknowledge completion. Part of the producer-consumer
 * synchronization pattern.
 *
 * @param index Semaphore index to get from
 */
inline void semaphore_get(const uint8_t index)
{
    pc_buf_base[PC_BUF_SEMAPHORE_BASE + index] = 1;  // Get operation
}

//
// **Tensix Thread Semaphore Operations**
//

/**
 * @brief Post to Tensix thread semaphore with optional stall
 *
 * @details Template function that posts to a Tensix thread semaphore with
 * optional stall wait before the operation. Enables synchronized semaphore
 * operations with hardware coordination.
 *
 * **Stall Options:**
 * - WaitRes: Specifies what to wait for before posting
 * - NONE: No stall, immediate operation
 * - MATH, SFPU, etc.: Wait for specific hardware units
 *
 * @tparam WaitRes Stall condition before posting (default: no stall)
 * @param index Semaphore index for Tensix thread coordination
 */
template <uint WaitRes = p_stall::NONE>
inline void t6_semaphore_post(const uint8_t index)
{
    if constexpr (WaitRes != p_stall::NONE)
    {
        TTI_STALLWAIT(p_stall::STALL_SYNC, WaitRes);  // Optional stall before posting
    }

    TTI_SEMPOST(semaphore::t6_sem(index));  // Hardware semaphore post
}

/**
 * @brief Get from Tensix thread semaphore with optional stall
 *
 * @details Template function that gets from a Tensix thread semaphore with
 * optional stall wait before the operation. Enables synchronized semaphore
 * operations with hardware coordination.
 *
 * @tparam WaitRes Stall condition before getting (default: no stall)
 * @param index Semaphore index for Tensix thread coordination
 */
template <uint WaitRes = p_stall::NONE>
inline void t6_semaphore_get(const uint8_t index)
{
    if constexpr (WaitRes != p_stall::NONE)
    {
        TTI_STALLWAIT(p_stall::STALL_SYNC, WaitRes);  // Optional stall before getting
    }

    TTI_SEMGET(semaphore::t6_sem(index));  // Hardware semaphore get
}

/**
 * @brief Wait for semaphore to reach maximum value
 *
 * @details Template function that blocks until the specified semaphore reaches
 * its maximum value. Used for synchronization patterns where maximum semaphore
 * value indicates resource availability or completion state.
 *
 * @tparam WaitRes Stall condition for the wait operation
 * @param index Semaphore index to wait on
 */
template <uint WaitRes>
inline void t6_semaphore_wait_on_max(const uint8_t index)
{
    TTI_SEMWAIT(WaitRes, semaphore::t6_sem(index), p_stall::STALL_ON_MAX);
}

/**
 * @brief Wait for semaphore to reach zero value
 *
 * @details Template function that blocks until the specified semaphore reaches
 * zero value. Used for synchronization patterns where zero semaphore value
 * indicates resource availability or completion state.
 *
 * @tparam WaitRes Stall condition for the wait operation
 * @param index Semaphore index to wait on
 */
template <uint WaitRes>
inline void t6_semaphore_wait_on_zero(const uint8_t index)
{
    TTI_SEMWAIT(WaitRes, semaphore::t6_sem(index), p_stall::STALL_ON_ZERO);
}

/**
 * @brief Initialize Tensix thread semaphore with min/max values
 *
 * @details Initializes a Tensix thread semaphore with specified minimum and
 * maximum values. Sets up the semaphore range for proper operation within
 * the specified bounds.
 *
 * **Semaphore Range:**
 * - min_value: Lower bound for semaphore operation
 * - max_value: Upper bound for semaphore operation
 * - Hardware enforces these bounds automatically
 * - Critical for proper resource counting
 *
 * @param index Semaphore index to initialize
 * @param min_value Minimum semaphore value (lower bound)
 * @param max_value Maximum semaphore value (upper bound)
 */
inline void t6_semaphore_init(const uint8_t index, const uint8_t min_value, const uint8_t max_value)
{
    TTI_SEMINIT(max_value, min_value, semaphore::t6_sem(index));
}

//
// **Mutex Operations**
//

/**
 * @brief Acquire Tensix thread mutex
 *
 * @details Acquires the specified mutex for atomic operation protection.
 * Blocks until the mutex is available, ensuring exclusive access to
 * shared resources across multiple threads.
 *
 * **Mutex Usage:**
 * - Provides atomic operation protection
 * - Ensures exclusive access to shared resources
 * - Hardware-implemented mutex primitives
 * - Deadlock prevention through proper ordering
 *
 * @param index Mutex index to acquire
 */
inline void t6_mutex_acquire(const uint8_t index)
{
    TTI_ATGETM(index);  // Atomic get mutex
}

/**
 * @brief Release Tensix thread mutex
 *
 * @details Releases the specified mutex, allowing other threads to acquire
 * it. Must be called after completing the atomic operation to prevent
 * deadlock and enable other threads to proceed.
 *
 * @param index Mutex index to release
 */
inline void t6_mutex_release(const uint8_t index)
{
    TTI_ATRELM(index);  // Atomic release mutex
}

//
// **Configuration Management**
//

/**
 * @brief Calculate configuration register address for current state
 *
 * @details Returns the actual configuration register address based on the
 * current configuration state ID. Enables context switching between different
 * configuration states without explicit address management.
 *
 * **State Addressing:**
 * - State 0: Base address (cfg_addr32)
 * - State 1: Offset address (base + CFG_STATE_SIZE * 4)
 * - Automatic state-aware addressing
 * - Transparent context switching
 *
 * @param cfg_addr32 Base configuration register address
 * @return Actual address for current configuration state
 */
inline uint cfg_addr(uint cfg_addr32)
{
    return (cfg_state_id == 0) ? cfg_addr32 : (CFG_STATE_SIZE * 4) + cfg_addr32;
}

/**
 * @brief Write to configuration register with state awareness
 *
 * @details Writes data to a configuration register using the current
 * configuration state for address calculation. Automatically handles
 * state-based addressing without requiring explicit state management.
 *
 * **State-Aware Writing:**
 * - Uses current cfg_state_id for address calculation
 * - Prevents direct access that might ignore state ID
 * - Ensures configuration consistency across states
 * - Critical for proper context switching
 *
 * @param cfg_addr32 Base configuration register address
 * @param data Data value to write to the configuration register
 */
inline void cfg_write(uint cfg_addr32, uint data)
{
    // Declared here instead of globally to prevent direct access, which might ignore current state ID
    volatile uint tt_reg_ptr *cfg_regs = reinterpret_cast<volatile uint tt_reg_ptr *>(TENSIX_CFG_BASE);
    cfg_regs[cfg_addr(cfg_addr32)]     = data;  // State-aware write
}

/**
 * @brief Read from configuration register with state awareness
 *
 * @details Reads data from a configuration register using the current
 * configuration state for address calculation. Automatically handles
 * state-based addressing for consistent configuration access.
 *
 * @param cfg_addr32 Base configuration register address
 * @return Data value read from the configuration register
 */
inline uint cfg_read(uint cfg_addr32)
{
    // Declared here instead of globally to prevent direct access, which might ignore current state ID
    volatile uint *cfg_regs = reinterpret_cast<volatile uint *>(TENSIX_CFG_BASE);
    return cfg_regs[cfg_addr(cfg_addr32)];  // State-aware read
}

/**
 * @brief Get configuration pointer for current state
 *
 * @details Returns a pointer to the configuration register base address
 * for the current configuration state. Enables direct configuration
 * access with proper state offset applied.
 *
 * **State-Based Addressing:**
 * - State 0: TENSIX_CFG_BASE
 * - State 1: TENSIX_CFG_BASE + CFG_STATE_SIZE * 16
 * - Direct pointer access with state awareness
 * - Efficient for multiple configuration operations
 *
 * @return Pointer to configuration base for current state
 */
inline volatile uint *tt_reg_ptr get_cfg_pointer()
{
    if (cfg_state_id == 0)
    {
        return reinterpret_cast<volatile uint tt_reg_ptr *>(TENSIX_CFG_BASE);
    }

    return reinterpret_cast<volatile uint tt_reg_ptr *>(TENSIX_CFG_BASE + CFG_STATE_SIZE * 16);
}

/**
 * @brief Get 16-bit configuration pointer for current state
 *
 * @details Returns a 16-bit pointer to the configuration register base
 * for the current configuration state. Used for 16-bit configuration
 * operations with proper state offset applied.
 *
 * @return 16-bit pointer to configuration base for current state
 */
inline volatile uint short *tt_reg_ptr get_cfg16_pointer()
{
    if (cfg_state_id == 0)
    {
        return reinterpret_cast<volatile uint short tt_reg_ptr *>(TENSIX_CFG_BASE);
    }

    return reinterpret_cast<volatile uint short tt_reg_ptr *>(TENSIX_CFG_BASE + CFG_STATE_SIZE * 16);
}

/**
 * @brief Flip configuration state for context switching
 *
 * @details Switches between configuration states (0 ↔ 1) for context
 * switching operations. Updates the hardware state ID register and
 * includes necessary NOPs for timing requirements.
 *
 * **State Switching:**
 * - Toggles between state 0 and state 1
 * - Updates hardware state ID register
 * - Includes timing NOPs for hardware synchronization
 * - Essential for double-buffering configuration operations
 */
inline void flip_cfg_state_id()
{
    cfg_state_id = 1 - cfg_state_id;                             // Toggle state (0 ↔ 1)
    TT_SETC16(CFG_STATE_ID_StateID_ADDR32, cfg_state_id);       // Program the current state ID
    TTI_NOP;                                                     // Timing requirement
    TTI_NOP;                                                     // Timing requirement
}

/**
 * @brief Reset configuration state to initial value
 * @details Resets the configuration state ID to 0 (initial state)
 */
inline void reset_cfg_state_id()
{
    cfg_state_id = 0;
}

/**
 * @brief Reset destination offset ID to initial value
 * @details Resets the destination offset ID to 0 for double-buffering reset
 */
inline void reset_dest_offset_id()
{
    dest_offset_id = 0;
}

/**
 * @brief Update destination offset ID for double-buffering
 * @details Toggles destination offset ID between 0 and 1 for double-buffering operations
 */
inline void update_dest_offset_id()
{
    // ping-pong between 0 and 1
    dest_offset_id = 1 - dest_offset_id;
}

/**
 * @brief Get destination buffer base address
 *
 * @details Returns the base address for destination buffer operations
 * based on the current destination offset ID. Enables double-buffering
 * by alternating between buffer sections.
 *
 * **Double-Buffering:**
 * - offset_id = 0: Base address 0x0
 * - offset_id = 1: Base address DEST_REGISTER_HALF_SIZE
 * - Automatic address calculation for buffer switching
 * - Critical for continuous operation
 *
 * @return Base address for current destination buffer section
 */
inline uint32_t get_dest_buffer_base()
{
    return (0 != dest_offset_id) ? DEST_REGISTER_HALF_SIZE : 0x0;
}

//
// **Memory Operation Processor (MOP) Control**
//

/**
 * @brief Execute Memory Operation Processor (MOP) without Z-mask
 *
 * @details Executes a MOP operation with specified type and count.
 * MOP operations handle bulk memory transfers and data movement
 * with hardware acceleration.
 *
 * **MOP Operation:**
 * - type: Specifies the type of memory operation
 * - count: Number of operations to perform (decremented by 1 for hardware)
 * - No Z-mask applied (all elements active)
 * - Hardware-accelerated execution
 *
 * @param type MOP operation type identifier
 * @param count Number of operations to perform
 */
inline void mop_run(const uint8_t type, const uint8_t count)
{
    TTI_MOP(type, count - 1, 0); // Run the MOP (count-1 for hardware indexing)
}

//
// **Low-Level Register Access**
//

/**
 * @brief Read from hardware register
 *
 * @details Performs a direct register read operation with proper
 * volatile semantics. Provides low-level register access for
 * hardware monitoring and debugging.
 *
 * **Register Access:**
 * - Direct memory-mapped register read
 * - Volatile semantics prevent optimization
 * - Used for hardware state monitoring
 * - Essential for debug and diagnostic operations
 *
 * @param addr Register address to read from
 * @return Value read from the register
 */
inline uint reg_read(uint32_t addr)
{
    volatile uint tt_reg_ptr *p_reg = reinterpret_cast<volatile uint tt_reg_ptr *>(addr);
    return p_reg[0];
}

/**
 * @brief Write to hardware register
 *
 * @details Performs a direct register write operation with proper
 * volatile semantics. Provides low-level register access for
 * hardware control and configuration.
 *
 * @param addr Register address to write to
 * @param data Data value to write to the register
 */
inline void reg_write(uint32_t addr, uint32_t data)
{
    volatile uint tt_reg_ptr *p_reg = reinterpret_cast<volatile uint tt_reg_ptr *>(addr);
    p_reg[0]                        = data;
}

/**
 * @brief Wait for specified number of clock cycles
 *
 * @details Implements a cycle-accurate wait by reading the hardware
 * wall clock and waiting until the specified number of cycles have
 * elapsed. Used for precise timing control in time-critical operations.
 *
 * **Timing Implementation:**
 * - Uses hardware wall clock for accurate timing
 * - 64-bit timestamp for long duration support
 * - Cycle-accurate waiting
 * - No dependency on software timers
 *
 * **Use Cases:**
 * - Hardware initialization delays
 * - Precise timing for hardware coordination
 * - Debug timing control
 * - Performance measurement intervals
 *
 * @param cycles Number of clock cycles to wait
 */
inline void wait(uint32_t cycles)
{
    volatile uint tt_reg_ptr *clock_lo = reinterpret_cast<volatile uint tt_reg_ptr *>(RISCV_DEBUG_REG_WALL_CLOCK_L);
    volatile uint tt_reg_ptr *clock_hi = reinterpret_cast<volatile uint tt_reg_ptr *>(RISCV_DEBUG_REG_WALL_CLOCK_H);
    uint64_t wall_clock_timestamp      = clock_lo[0] | ((uint64_t)clock_hi[0] << 32);  // Get start timestamp
    uint64_t wall_clock                = 0;
    do
    {
        wall_clock = clock_lo[0] | ((uint64_t)clock_hi[0] << 32);  // Read current timestamp
    } while (wall_clock < (wall_clock_timestamp + cycles));        // Wait until target reached
}

//
// **Destination Register Management**
//

/**
 * @brief Clear destination registers (zero accumulator)
 *
 * @details Clears all destination registers to zero using hardware
 * zero accumulator operation. Essential for initializing destination
 * state before mathematical operations.
 *
 * **Zero Accumulator Operation:**
 * - Clears all destination register banks
 * - Hardware-accelerated clearing
 * - Prepares destination for accumulation operations
 * - Critical for deterministic mathematical results
 */
inline void zeroacc()
{
    // Clear dest
    addr_mod_t {
        .srca = {.incr = 0},
        .srcb = {.incr = 0},
        .dest = {.incr = 0},
    }
        .set(ADDR_MOD_1);
    TT_ZEROACC(p_zeroacc::CLR_ALL, 0, 0, ADDR_MOD_1, 0);  // Hardware zero accumulator operation
}

/**
 * @brief Clear source registers
 *
 * @details Clears all source register banks (A and B) to zero.
 * Used for initializing source state before loading new operand data.
 *
 * **Source Clearing:**
 * - Clears both source A and source B banks
 * - Prepares source registers for new data
 * - Ensures clean state for mathematical operations
 * - Prevents stale data interference
 */
inline void zerosrc()
{
    TTI_ZEROSRC(0, 0, 1, 3); // Zero all srcA&B banks
}

/**
 * @brief Synchronize register file write operation
 *
 * @details Ensures register file write operations are completed and visible
 * by performing a read operation on the specified register file index.
 * Used for synchronizing register file state.
 *
 * @param index Register file index to synchronize
 */
inline void sync_regfile_write(const uint index)
{
    volatile uint foo     = 0x0;
    volatile uint *fooptr = &foo;
    *fooptr               = regfile[index];  // Read to ensure write completion
}

//
// **Configuration Read-Modify-Write Operations**
//

/**
 * @brief Perform read-modify-write operation on configuration register
 *
 * @details Executes an atomic read-modify-write operation on a configuration
 * register with proper bit field manipulation. Essential for updating
 * specific fields within configuration registers without affecting other bits.
 *
 * **RMW Operation Sequence:**
 * 1. Read current configuration register value
 * 2. Shift and mask new value to proper bit position
 * 3. Clear target bits in current value
 * 4. OR new bits into cleared space
 * 5. Write updated value back to register
 *
 * **Bit Field Manipulation:**
 * - cfg_shamt: Bit shift amount for field positioning
 * - cfg_mask: Bit mask for field isolation
 * - Atomic operation prevents race conditions
 * - State-aware addressing
 *
 * @param cfg_addr32 Configuration register address
 * @param cfg_shamt Bit shift amount for field positioning
 * @param cfg_mask Bit mask for field selection
 * @param val New value for the specified field
 */
inline void cfg_rmw(uint32_t cfg_addr32, uint32_t cfg_shamt, uint32_t cfg_mask, uint32_t val)
{
    uint32_t wrdata = val;

    // Avoid multiplication of variables!
    // const uint32_t addr = (cfg_state_id * CFG_STATE_SIZE * 4) + cfg_addr32;
    const uint32_t addr = (cfg_state_id == 0) ? cfg_addr32 : (CFG_STATE_SIZE * 4) + cfg_addr32;

    // Declared here instead of globally to prevent direct access, which might ignore current state ID
    volatile uint tt_reg_ptr *cfg_regs = reinterpret_cast<volatile uint tt_reg_ptr *>(TENSIX_CFG_BASE);
    uint32_t cfg_data                  = cfg_regs[addr];  // Read current value

    // Shift and mask wrdata to properly align within 32-bit DWORD
    wrdata <<= cfg_shamt;   // Position new value
    wrdata &= cfg_mask;     // Apply field mask

    // Zero-out relevant bits in cfg data
    cfg_data &= ~cfg_mask;  // Clear target bits

    // Or new data bits
    cfg_data |= wrdata;     // Insert new value

    // Update cfg regs
    cfg_regs[addr] = cfg_data;  // Write updated value
}

/**
 * @brief Perform read-modify-write using GPR value
 *
 * @details Executes a read-modify-write operation using a value from
 * the general purpose register file. Convenient for updating configuration
 * fields with values computed in GPRs.
 *
 * @param cfg_addr32 Configuration register address
 * @param cfg_shamt Bit shift amount for field positioning
 * @param cfg_mask Bit mask for field selection
 * @param gpr_index GPR index containing the value to write
 */
inline void cfg_rmw_gpr(uint32_t cfg_addr32, uint32_t cfg_shamt, uint32_t cfg_mask, uint32_t gpr_index)
{
    const uint32_t wrdata = regfile[gpr_index];  // Get value from GPR
    cfg_rmw(cfg_addr32, cfg_shamt, cfg_mask, wrdata);  // Perform RMW operation
}

/**
 * @brief Template-based configuration register read-modify-write
 *
 * @details Template function for compile-time optimized read-modify-write
 * operations on configuration registers. Uses template parameters for
 * maximum performance through compile-time optimization.
 *
 * **Template Optimization:**
 * - Compile-time constants for address, shift, and mask
 * - Optimized instruction generation
 * - Byte-wise RMW operations for efficiency
 * - Only processes bytes that are actually modified
 *
 * **Byte-wise Processing:**
 * The function processes each byte of the 32-bit register separately,
 * only performing RMW operations on bytes that have non-zero mask bits.
 * This minimizes the number of hardware operations required.
 *
 * @tparam CfgAddr32 Configuration register address (compile-time constant)
 * @tparam Shamt Bit shift amount (compile-time constant)
 * @tparam Mask Bit mask for field selection (compile-time constant)
 * @param val Value to write to the specified field
 */
template <uint CfgAddr32, uint Shamt, uint Mask>
inline void cfg_reg_rmw_tensix(uint32_t val)
{
    uint32_t wrdata = val << Shamt;  // Position value
    uint8_t mask_b0 = Mask & 0xff;   // Extract byte 0 mask

    if (mask_b0 != 0)
    {
        uint8_t data_b0 = wrdata & 0xff;  // Extract byte 0 data
        TT_RMWCIB0(mask_b0, data_b0, CfgAddr32);  // RMW byte 0
    }
    wrdata >>= 8;
    uint8_t mask_b1 = (Mask >> 8) & 0xff;  // Extract byte 1 mask

    if (mask_b1 != 0)
    {
        uint8_t data_b1 = (wrdata) & 0xff;  // Extract byte 1 data
        TT_RMWCIB1(mask_b1, data_b1, CfgAddr32);  // RMW byte 1
    }

    wrdata >>= 8;
    uint8_t mask_b2 = (Mask >> 16) & 0xff;  // Extract byte 2 mask

    if (mask_b2 != 0)
    {
        uint8_t data_b2 = (wrdata) & 0xff;  // Extract byte 2 data
        TT_RMWCIB2(mask_b2, data_b2, CfgAddr32);  // RMW byte 2
    }

    wrdata >>= 8;
    uint8_t mask_b3 = (Mask >> 24) & 0xff;  // Extract byte 3 mask
    if (mask_b3 != 0)
    {
        uint8_t data_b3 = (wrdata) & 0xff;  // Extract byte 3 data
        TT_RMWCIB3(mask_b3, data_b3, CfgAddr32);  // RMW byte 3
    }
}

//
// **Inter-Thread Mailbox Operations**
//

/**
 * @brief Write data to inter-thread mailbox
 *
 * @details Writes data to the mailbox for the specified thread, enabling
 * communication and data exchange between different execution threads.
 * Essential for coordinating operations across the kernel pipeline.
 *
 * **Mailbox Communication:**
 * - Thread-specific mailbox addressing
 * - Atomic write operation
 * - Non-blocking write (may overwrite previous data)
 * - Used for parameter passing and coordination
 *
 * **Thread Mapping:**
 * - 0: Unpack thread
 * - 1: Math thread  
 * - 2: Pack thread
 * - 3: Reserved/special purposes
 *
 * @param thread Target thread ID (0-3)
 * @param data Data value to write to the mailbox
 */
inline void mailbox_write(const uint8_t thread, const uint32_t data)
{
    mailbox_base[thread + 1][0] = data;  // Write to thread mailbox (offset by 1)
}

/**
 * @brief Read data from inter-thread mailbox (blocking)
 *
 * @details Performs a blocking read from the specified thread's mailbox.
 * The operation will block until data is available in the mailbox.
 *
 * @param thread Source thread ID (0-3)
 * @return Data value read from the mailbox
 */
inline uint32_t mailbox_read(const uint8_t thread)
{
    return mailbox_base[thread + 1][0];  // Blocking read from thread mailbox
}

/**
 * @brief Check if mailbox contains data (non-blocking)
 *
 * @details Non-blocking check to determine if the specified thread's
 * mailbox contains data. Used for polling-based mailbox operations.
 *
 * @param thread Thread ID to check (0-3)
 * @return true if mailbox contains data, false if empty
 */
inline bool mailbox_not_empty(const uint8_t thread)
{
    return mailbox_base[thread + 1][1] > 0;  // Check mailbox status
}

/**
 * @brief Write to full mailbox interface
 *
 * @details Writes to the full mailbox interface (direct thread indexing).
 * Alternative mailbox interface with different addressing scheme.
 *
 * @param thread Thread ID (direct indexing)
 * @param data Data value to write
 */
inline void mailbox_write_full(const uint8_t thread, const uint32_t data)
{
    mailbox_base[thread][0] = data;  // Direct thread indexing
}

/**
 * @brief Read from full mailbox interface (blocking)
 *
 * @details Blocking read from the full mailbox interface.
 *
 * @param thread Thread ID (direct indexing)
 * @return Data value read from the mailbox
 */
inline uint32_t mailbox_read_full(const uint8_t thread)
{
    return mailbox_base[thread][0];  // Blocking read with direct indexing
}

/**
 * @brief Check full mailbox status (non-blocking)
 *
 * @details Non-blocking check for data in the full mailbox interface.
 *
 * @param thread Thread ID to check (direct indexing)
 * @return true if mailbox contains data, false if empty
 */
inline bool mailbox_not_empty_full(const uint8_t thread)
{
    return mailbox_base[thread][1] > 0;  // Check status with direct indexing
}

/**
 * @brief Write to TRISC-L1 mailbox
 *
 * @details Writes data to the specialized TRISC to L1 memory mailbox.
 * Used for communication between TRISC processor and L1 memory operations.
 *
 * @param data Data value to write to TRISC-L1 mailbox
 */
inline void trisc_l1_mailbox_write(const uint data)
{
    trisc_l1_mailbox[0] = data;  // Write to TRISC-L1 mailbox
}

/**
 * @brief Read from TRISC-L1 mailbox
 *
 * @details Reads data from the TRISC to L1 memory mailbox.
 *
 * @return Data value read from TRISC-L1 mailbox
 */
inline uint trisc_l1_mailbox_read()
{
    return trisc_l1_mailbox[0];  // Read from TRISC-L1 mailbox
}

//
// **Utility Functions**
//

/**
 * @brief Cast object pointer to memory address
 *
 * @details Template function that converts an object pointer to its
 * memory address as a 32-bit unsigned integer. Used for passing
 * object addresses through mailboxes and registers.
 *
 * @tparam T Object type
 * @param object_ptr Pointer to object
 * @return 32-bit memory address of the object
 */
template <class T>
inline std::uint32_t memory_cast(T *object_ptr)
{
    return reinterpret_cast<uint32_t>(object_ptr);
}

//
// **Debug Mailbox Operations**
//

/**
 * @brief Record value to debug mailbox
 *
 * @details Records a 16-bit event value to the debug mailbox at the
 * current index position. Used for debugging and performance monitoring
 * by capturing event values during kernel execution.
 *
 * **Debug Recording:**
 * - 16-bit values for compact storage
 * - Sequential recording with automatic index increment
 * - Bounds checking to prevent overflow
 * - Used for performance analysis and debugging
 *
 * @param event_value 16-bit event value to record
 */
inline void record_mailbox_value(uint16_t event_value)
{
    if (mailbox_index < mailbox_end)
    {
        debug_mailbox_base[mailbox_index] = event_value;  // Record value
        mailbox_index++;                                  // Increment index
    }
}

/**
 * @brief Record value to specific debug mailbox index
 *
 * @details Records a 16-bit event value to a specific index in the
 * debug mailbox. Used for recording values at predetermined positions
 * for structured debug data organization.
 *
 * @param index Specific mailbox index to write to
 * @param event_value 16-bit event value to record
 */
inline void record_mailbox_value_with_index(uint8_t index, uint16_t event_value)
{
    if (index < mailbox_end)
    {
        debug_mailbox_base[index] = event_value;  // Record at specific index
    }
}

/**
 * @brief Initialize debug mailbox with specified value
 *
 * @details Clears all debug mailbox entries by setting them to the
 * specified value (default: 0). Used for initializing debug mailbox
 * state before recording new debug information.
 *
 * @param value Value to initialize all mailbox entries (default: 0)
 */
inline void clear_mailbox_values(uint16_t value = 0)
{
    for (int i = 0; i < mailbox_end; i++)
    {
        debug_mailbox_base[i] = value;  // Initialize all entries
    }
}

/**
 * @brief Read 64-bit wall clock timestamp
 *
 * @details Reads the current 64-bit wall clock timestamp from hardware
 * registers. Used for precise timing measurements and performance analysis.
 *
 * **Wall Clock Reading:**
 * - 64-bit timestamp for extended range
 * - High precision timing
 * - Hardware-based time source
 * - Used for performance measurement
 *
 * @return Current 64-bit wall clock timestamp
 */
inline uint64_t read_wall_clock()
{
    uint32_t timestamp_low  = reg_read(RISCV_DEBUG_REG_WALL_CLOCK_L);  // Read low 32 bits
    uint32_t timestamp_high = reg_read(RISCV_DEBUG_REG_WALL_CLOCK_H);  // Read high 32 bits
    return ((uint64_t)timestamp_high << 32) | timestamp_low;           // Combine into 64-bit value
}

/**
 * @brief Record kernel runtime to debug mailbox
 *
 * @details Records a 64-bit kernel runtime value to the last 4 entries
 * of the debug mailbox. The runtime is split into 16-bit chunks for
 * storage in the 16-bit mailbox entries.
 *
 * **Runtime Storage Format:**
 * - mailbox_end-4: bits 0-15 (lowest 16 bits)
 * - mailbox_end-3: bits 16-31
 * - mailbox_end-2: bits 32-47  
 * - mailbox_end-1: bits 48-63 (highest 16 bits)
 *
 * @param kernel_runtime 64-bit kernel runtime value to record
 */
inline void record_kernel_runtime(uint64_t kernel_runtime)
{
    debug_mailbox_base[mailbox_end - 4] = kernel_runtime & 0xffff;         // Bits 0-15
    debug_mailbox_base[mailbox_end - 3] = (kernel_runtime >> 16) & 0xffff; // Bits 16-31
    debug_mailbox_base[mailbox_end - 2] = (kernel_runtime >> 32) & 0xffff; // Bits 32-47
    debug_mailbox_base[mailbox_end - 1] = (kernel_runtime >> 48) & 0xffff; // Bits 48-63
}

/**
 * @brief Debug data dump to debug buffer
 * @details Dumps binary data to debug buffer for analysis (implementation in source file)
 * @param data Pointer to data to dump
 * @param byte_size Size of data in bytes
 */
void debug_dump(const uint8_t *data, uint32_t byte_size);

/**
 * @brief Seek debug buffer position
 * @details Sets debug buffer position offset (implementation in source file)
 * @param offset Byte offset for debug buffer positioning
 */
void debug_dump_seek(uint8_t offset);

// If the TRACK_x bit is set, then the Tensix hardware will automatically
// stall TRISC memory accesses and/or Tensix instructions to x in order
// to guarantee correct ordering. This should eliminate most cases where
// we used to need a tensix_sync() or a sync_regfile_write().
//
// The EN_SUBDIVIDED_CFG_FOR_UNPACR is more subtle. If it is turned off,
// then the global CFG registers are treated as one big entity, and ANY
// access from Tensix instructions will be synchronized with ANY access
// from this TRISC. If it is on, then we distinguish between accesses
// target CFG regs for unpacker 0, CFG regs for unpacker 1, and all the
// others (meaning that no synchronization will happen between,

//
// **Control and Status Register (CSR) Operations**
//

/**
 * @brief Write to Control and Status Register with optional fence
 *
 * @details Writes a value to the specified CSR with optional memory fence
 * for ordering guarantees. CSRs provide access to processor control and
 * status information.
 *
 * **CSR Access:**
 * - Direct processor CSR access
 * - Optional memory fence for ordering
 * - Template-based compile-time optimization
 * - Used for processor configuration and status
 *
 * @tparam csr_num CSR number to write to (12-bit range)
 * @tparam fence Whether to insert memory fence before operation
 * @param val Value to write to the CSR
 */
template <uint16_t csr_num, bool fence = true>
inline void csr_write(uint32_t val)
{
    static_assert(csr_num < (1 << 12), "Given CSR number is out of range");

    if constexpr (fence)
    {
        asm volatile("fence");  // Memory ordering fence
    }
    asm volatile("csrw %[csr_num], %[val] \n" : : [csr_num] "i"(csr_num), [val] "r"(val));
}

/**
 * @brief Read from Control and Status Register with optional fence
 *
 * @details Reads a value from the specified CSR with optional memory fence
 * for ordering guarantees. Template-based for compile-time optimization.
 *
 * @tparam csr_num CSR number to read from (12-bit range)
 * @tparam fence Whether to insert memory fence before operation
 * @return Value read from the CSR
 */
template <uint16_t csr_num, bool fence = true>
inline uint32_t csr_read()
{
    static_assert(csr_num < (1 << 12), "Given CSR number is out of range");
    uint32_t ret;

    if constexpr (fence)
    {
        asm volatile("fence");  // Memory ordering fence
    }
    asm volatile("csrr %[ret], %[csr_num] \n" : [ret] "=r"(ret) : [csr_num] "i"(csr_num));

    return ret;
}

//
// **Hardware Status Monitoring**
//

/**
 * @brief Queue status union for hardware unit monitoring
 *
 * @details Union providing structured access to hardware queue status
 * information. Enables monitoring of various hardware units and their
 * queue states for performance analysis and debugging.
 *
 * **Hardware Units Monitored:**
 * - replay, mop, thcon, xmov: Core execution units
 * - unpack, pack: Data movement units
 * - cfg, sync: Configuration and synchronization
 * - tdma: Tensix DMA operations
 * - sfpu, fpu: Mathematical processing units
 * - sfpucc: SFPU condition codes
 *
 * **Status Types:**
 * - Local status: Individual unit status
 * - Global status: System-wide unit status
 * - Used for performance monitoring and debugging
 */
union qstatus_u
{
    uint32_t val;  ///< Raw 32-bit status value

    struct
    {
        unsigned replay : 1;    ///< Replay unit status
        unsigned mop    : 1;    ///< Memory operation processor status
        unsigned thcon  : 1;    ///< Thread control unit status
        unsigned xmov   : 1;    ///< Data movement unit status
        unsigned unpack : 1;    ///< Unpack unit status
        unsigned pack   : 1;    ///< Pack unit status
        unsigned cfg    : 1;    ///< Configuration unit status
        unsigned sync   : 1;    ///< Synchronization unit status
        unsigned tdma   : 1;    ///< Tensix DMA status
        unsigned _sfpu  : 1;    ///< SFPU status (avoiding name conflict)
        unsigned fpu    : 1;    ///< FPU status
        unsigned sfpucc : 2;    ///< SFPU condition codes

        unsigned global_replay : 1;    ///< Global replay unit status
        unsigned global_mop    : 1;    ///< Global MOP status
        unsigned global_thcon  : 1;    ///< Global thread control status
        unsigned global_xmov   : 1;    ///< Global data movement status
        unsigned global_unpack : 1;    ///< Global unpack status
        unsigned global_pack   : 1;    ///< Global pack status
        unsigned global_cfg    : 1;    ///< Global configuration status
        unsigned global_sync   : 1;    ///< Global synchronization status
        unsigned global_tdma   : 1;    ///< Global TDMA status
        unsigned global_sfpu   : 1;    ///< Global SFPU status
        unsigned global_fpu    : 1;    ///< Global FPU status
        unsigned global_sfpucc : 2;    ///< Global SFPU condition codes
    };
};

/**
 * @brief Busy status union for hardware unit activity monitoring
 *
 * @details Union providing structured access to hardware unit busy status
 * information. Similar to qstatus_u but focused on busy/active states
 * rather than queue states.
 *
 * **Busy Status Monitoring:**
 * - Individual unit busy states
 * - Global system busy states
 * - Used for synchronization and timing analysis
 * - Critical for determining hardware idle states
 */
union bstatus_u
{
    uint32_t val;  ///< Raw 32-bit status value

    struct
    {
        unsigned replay : 1;    ///< Replay unit busy status
        unsigned mop    : 1;    ///< MOP busy status
        unsigned thcon  : 1;    ///< Thread control busy status
        unsigned xmov   : 1;    ///< Data movement busy status
        unsigned unpack : 1;    ///< Unpack unit busy status
        unsigned pack   : 1;    ///< Pack unit busy status
        unsigned cfg    : 1;    ///< Configuration unit busy status
        unsigned sync   : 1;    ///< Synchronization unit busy status
        unsigned tdma   : 1;    ///< TDMA busy status
        unsigned _sfpu  : 1;    ///< SFPU busy status
        unsigned fpu    : 1;    ///< FPU busy status

        unsigned global_replay : 1;    ///< Global replay busy status
        unsigned global_mop    : 1;    ///< Global MOP busy status
        unsigned global_thcon  : 1;    ///< Global thread control busy status
        unsigned global_xmov   : 1;    ///< Global data movement busy status
        unsigned global_unpack : 1;    ///< Global unpack busy status
        unsigned global_pack   : 1;    ///< Global pack busy status
        unsigned global_cfg    : 1;    ///< Global configuration busy status
        unsigned global_sync   : 1;    ///< Global synchronization busy status
        unsigned global_tdma   : 1;    ///< Global TDMA busy status
        unsigned global_sfpu   : 1;    ///< Global SFPU busy status
        unsigned global_fpu    : 1;    ///< Global FPU busy status
    };
};

//
// **Specialized Hardware Operations**
//

/**
 * @brief Initialize Pseudo-Random Number Generator (PRNG) seed
 *
 * @details Initializes the hardware PRNG with the specified seed value.
 * Includes necessary timing delays for proper hardware initialization.
 * Used for operations requiring random number generation.
 *
 * **PRNG Initialization:**
 * - Sets hardware PRNG seed value
 * - Includes timing delays for hardware stabilization
 * - Critical for reproducible random number sequences
 * - Used in certain mathematical and testing operations
 *
 * **Timing Requirements:**
 * The function includes a 600-cycle delay to ensure proper PRNG
 * initialization. This delay is implemented using SFPU NOPs.
 *
 * @param seed Seed value for PRNG initialization
 */
inline void init_prng_seed(const uint seed)
{
    // The seed for PRNG should at least be initialized during chip boot-up time.
    volatile uint tt_reg_ptr *cfg  = get_cfg_pointer();    // Get configuration pointer
    cfg[PRNG_SEED_Seed_Val_ADDR32] = seed;                 // Set PRNG seed

    // TODO: ckernel::wait does not work properly. Use ckernel::wait when fixed.
    for (int i = 0; i < 600; i++)
    {
        TTI_SFPNOP;  // Timing delay for PRNG stabilization
    }
}

/**
 * @brief Validate instruction mode for load/store operations
 *
 * @details Compile-time function that validates whether the specified
 * instruction mode is valid for load/store operations. Used for
 * compile-time verification of instruction parameters.
 *
 * **Valid Modes:**
 * - INT32_2S_COMP: 32-bit two's complement integer
 * - INT32: 32-bit integer
 * - LO16: Lower 16-bit access
 *
 * @param mode Instruction mode to validate
 * @return true if mode is valid for load/store operations
 */
inline constexpr bool is_valid_instruction_mode(InstrModLoadStore mode)
{
    return mode == InstrModLoadStore::INT32_2S_COMP || mode == InstrModLoadStore::INT32 || mode == InstrModLoadStore::LO16;
}

/**
 * @brief Apply sign-magnitude conversion for data casting
 *
 * @details Performs sign-magnitude conversion using SFPU cast and sign
 * operations. Includes workaround for Blackhole RTL bug requiring
 * additional sign setting operation after cast.
 *
 * **Conversion Process:**
 * 1. Perform SFPU cast operation
 * 2. Apply sign setting operation (RTL bug workaround)
 * 3. Result provides proper sign-magnitude representation
 *
 * **RTL Bug Workaround:**
 * The SFPSETSGN operation is required after cast due to a bug in
 * Blackhole RTL (Reference: tenstorrent/tt-llk-bh#16).
 *
 * @param src Source register for cast operation
 * @param dst Destination register for cast result
 * @param cast_mode Cast mode specifying conversion type
 */
inline void apply_sign_magnitude_conversion(uint src, uint dst, InstrModCast cast_mode)
{
    TTI_SFPCAST(src /*lreg*/, dst /*ldest*/, cast_mode);                    // Perform cast operation
    // Required after cast due to a bug in Blackhole RTL (Refer tenstorrent/tt-llk-bh#16)
    TTI_SFPSETSGN(0 /* imm */, dst /*lreg_c*/, src /*ldest*/, 0 /*imod*/); // Set sign (RTL bug workaround)
}

} // namespace ckernel
