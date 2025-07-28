// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

/**
 * @file ckernel_structs.h
 * @brief Core data structures for Wormhole B0 Tensix synchronization and communication
 *
 * @details This header defines fundamental data structures essential for coordinating
 * the complex 3-thread Tensix engine architecture in Wormhole B0. These structures
 * provide synchronization primitives and communication mechanisms that enable efficient
 * parallel execution across Unpack, Math, and Pack threads.
 *
 * **Wormhole B0 Synchronization Architecture:**
 * - **Non-Atomic Semaphores**: Resource counters (not traditional blocking semaphores)
 * - **Thread Coordination**: Manages data flow between Unpack→Math→Pack pipeline
 * - **Hardware Integration**: Maps to Tensix engine synchronization primitives
 * - **DEST Register Management**: Coordinates access to the 1024×16 or 512×16 DEST register file
 *
 * **Semaphore Behavior (Wormhole B0 Specific):**
 * Unlike traditional semaphores, Wormhole B0 semaphores are resource counters:
 * - **SEMPOST**: Unconditional increment (signals resource availability)
 * - **SEMGET**: Unconditional decrement (claims resource)
 * - **SEMWAIT**: Blocking wait on specific condition (zero or max value)
 * - **No Ownership**: Any thread can increment/decrement any semaphore
 *
 * **3-Thread Pipeline Coordination:**
 * ```
 * Thread 0 (Unpack): L1 → SRCA/SRCB registers
 *     ↓ (semaphore sync)
 * Thread 1 (Math): SRCA/SRCB → Compute → DEST registers  
 *     ↓ (semaphore sync)
 * Thread 2 (Pack): DEST registers → L1
 * ```
 *
 * **Performance Considerations:**
 * - **Double Buffering**: DEST registers support dual-bank operation
 * - **Hardware Sync**: Embedded synchronization between Unpack/Math stages
 * - **Pipeline Efficiency**: Semaphores enable overlapped execution phases
 * - **Minimal Overhead**: Optimized for high-frequency synchronization operations
 */

#pragma once

namespace ckernel
{

/**
 * @brief Semaphore definitions for Wormhole B0 Tensix 3-thread pipeline coordination
 * @details Provides semaphore constants for synchronizing the complex data flow
 * through the Unpack→Math→Pack pipeline. These are resource counters that track
 * availability rather than traditional blocking semaphores.
 */
struct semaphore
{
    /**
     * @brief Math↔Pack synchronization on DEST register access
     * @details Coordinates access to the DEST register file between Math thread
     * (Thread 1) computation and Pack thread (Thread 2) data movement to L1.
     * Ensures proper double-buffering and prevents data races.
     */
    constexpr static uint32_t MATH_PACK           = 1;
    
    /**
     * @brief Unpack→Math synchronization for DEST register readiness
     * @details Synchronizes Unpack thread with Math thread when unpacking
     * directly to DEST registers, ensuring data is ready for computation.
     */
    constexpr static uint32_t UNPACK_TO_DEST      = 2;
    
    /**
     * @brief Operand synchronization across Unpack↔Pack and Unpack↔Math
     * @details Manages operand availability and release across thread boundaries,
     * coordinating when source data is ready and when results can be consumed.
     */
    constexpr static uint32_t UNPACK_OPERAND_SYNC = 3;
    
    /**
     * @brief Pack completion synchronization for performance monitoring
     * @details Tracks beginning and end of each pack iteration, enabling
     * performance event recording and controlled delay insertion for debugging.
     */
    constexpr static uint32_t PACK_DONE           = 4;
    
    /**
     * @brief TRISC↔Unpack synchronization for hardware kernel coordination
     * @details Synchronizes TRISC processor control with Unpack thread execution,
     * ensuring proper hardware kernel launch and completion signaling.
     */
    constexpr static uint32_t UNPACK_SYNC         = 5;
    
    /**
     * @brief Unpack/Math completion synchronization for performance monitoring
     * @details Tracks beginning and end of unpack or math iterations for performance
     * event recording. Note: Should be used exclusively for either unpack OR math,
     * not both simultaneously to avoid synchronization conflicts.
     */
    constexpr static uint32_t UNPACK_MATH_DONE = 6;
    
    /**
     * @brief Math completion synchronization for Unpack→DEST operations
     * @details Ensures Math thread has completed operations before Unpack
     * thread writes new data to DEST registers, preventing data corruption.
     */
    constexpr static uint32_t MATH_DONE        = 7;

    /**
     * @brief Convert semaphore index to Tensix hardware semaphore mask
     * @param sem_index Semaphore index (0-7 for Wormhole B0)
     * @return Hardware semaphore bit mask for Tensix instruction encoding
     * @details Provides bit-mask encoding for Tensix semaphore instructions
     * (SEMINIT, SEMPOST, SEMGET, SEMWAIT) compatible with hardware requirements.
     */
    constexpr static uint16_t t6_sem(const uint8_t sem_index)
    {
        return (1 << sem_index);
    }
};

/**
 * @brief Mutex definitions for Wormhole B0 Tensix critical section protection
 * @details Provides mutex constants for protecting critical code sections that
 * require atomic operations across the 3-thread Tensix engine. Unlike semaphores,
 * mutexes provide true test-and-set atomic primitives for mutual exclusion.
 */
struct mutex
{
    /**
     * @brief Mutex for atomic register read-modify-write operations
     * @details Protects critical sections performing atomic register updates
     * that must be completed without interruption from other threads.
     * Essential for maintaining register state consistency across the
     * Unpack, Math, and Pack threads when accessing shared resources.
     */
    constexpr static uint32_t REG_RMW = 0;
};

/**
 * @brief Base address for semaphores in PC buffer communication
 * @details Starting address offset for semaphore operations in the PC buffer
 * system, enabling efficient communication between TRISC processors and
 * the Tensix engine threads for kernel coordination and resource management.
 */
constexpr uint8_t PC_BUF_SEMAPHORE_BASE = 8;

/**
 * @brief Half-size of DEST register file in 16×16 faces (Wormhole B0 specific)
 * @details Architecture-specific constant defining half the DEST register capacity
 * in terms of 16×16 data faces. Used for double-buffering calculations where
 * DEST registers are split between Math and Pack thread access regions.
 * Total DEST capacity = 2 × 32 = 64 faces in 16-bit mode.
 */
constexpr uint8_t MATH_HALF_DEST_SIZE   = 32;

/**
 * @brief Maximum number of configuration states supported by Wormhole B0
 * @details Wormhole B0 supports dual configuration states (State 0 and State 1),
 * enabling dynamic reconfiguration without performance penalties. Each thread
 * can independently select which configuration state to use via CFG_STATE_ID.
 */
constexpr uint8_t MAX_CONFIG_STATES     = 2;

/**
 * @brief Firmware message enumeration for TRISC↔Tensix communication
 * @details Defines message types for communication between TRISC processors
 * and compute kernels, enabling coordination of configuration changes,
 * execution control, and performance monitoring across the 3-thread pipeline.
 */
enum firmware_msg_e
{
    /**
     * @brief Command to flip/switch configuration state ID
     * @details Instructs kernel to switch between dual configuration states (0↔1)
     * for dynamic reconfiguration without pipeline stalls. Enables rapid
     * context switching for different operation modes or data formats.
     */
    FLIP_STATE_ID        = 1,
    
    /**
     * @brief Command to execute queued instructions
     * @details Signals kernel to begin execution of queued Tensix instructions,
     * coordinating the start of Unpack, Math, and Pack operations for
     * synchronized pipeline execution.
     */
    RUN_INSTRUCTIONS     = 2,
    
    /**
     * @brief Command to reset destination register offset identifier
     * @details Resets DEST register addressing offset to initial state,
     * typically used for restarting operations or clearing accumulated
     * addressing state for fresh computation sequences.
     */
    RESET_DEST_OFFSET_ID = 3,
    
    /**
     * @brief Command to configure performance monitoring scratch registers
     * @details Sets up performance monitoring and debugging scratch register
     * values for tracking execution metrics, timing analysis, and
     * system performance characterization.
     */
    SET_PERF_SCRATCH     = 4
};

} // namespace ckernel
