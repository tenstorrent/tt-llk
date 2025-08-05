// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

/**
 * @file ckernel_structs.h
 * @brief Core Data Structures and Constants for Wormhole B0 Tensix Kernel
 *
 * This header defines fundamental data structures, constants, and enumerations
 * used throughout the kernel system. It provides synchronization primitives,
 * architecture-specific constants, and communication interfaces between
 * different system components.
 *
 * @author Tenstorrent AI ULC
 * @version 1.0
 * @date 2025
 *
 * # Key Components
 *
 * - **Synchronization Primitives**: Semaphore and mutex definitions for thread coordination
 * - **Architecture Constants**: Hardware-specific sizing and configuration limits
 * - **Communication Interfaces**: Message types for firmware-kernel communication
 * - **Buffer Management**: Constants for PC buffer and semaphore organization
 *
 * # Synchronization Architecture
 *
 * The kernel uses a sophisticated synchronization system with multiple semaphores
 * to coordinate between different processing threads (unpack, math, pack) and
 * ensure proper pipeline operation without data races or corruption.
 *
 * # Thread Coordination
 *
 * The synchronization primitives enable coordination between:
 * - **Unpack ↔ Math**: Data availability and consumption
 * - **Math ↔ Pack**: Result availability and output processing
 * - **Firmware ↔ Kernel**: Control and configuration messages
 * - **Multi-thread**: Atomic operations and critical sections
 *
 * @note These structures provide the foundation for all kernel synchronization
 * @note Proper use of synchronization primitives is critical for correct operation
 *
 * @see ckernel.h for synchronization function implementations
 * @see ckernel_globals.h for related global variable declarations
 */

#pragma once

/**
 * @namespace ckernel
 * @brief Core kernel functionality and hardware abstraction layer
 */
namespace ckernel
{

/**
 * @defgroup KernelStructures Core Kernel Data Structures
 * @brief Fundamental structures and constants for kernel operation
 * @{
 */

/**
 * @struct semaphore
 * @brief Semaphore identifier constants for inter-thread synchronization
 *
 * Defines semaphore identifiers used for coordinating between different processing
 * threads in the Tensix pipeline. Each semaphore serves a specific coordination
 * purpose and ensures proper data flow and synchronization across the pipeline.
 *
 * # Semaphore Architecture:
 * - **Pipeline Coordination**: Math ↔ Pack, Unpack ↔ Math synchronization
 * - **Performance Monitoring**: Begin/end event coordination for measurement
 * - **Resource Management**: Operand acquisition and release coordination
 * - **Hardware Integration**: TRISC ↔ hardware kernel synchronization
 */
struct semaphore
{
    constexpr static uint32_t MATH_PACK           = 1; ///< Math ↔ Pack sync on destination register
    constexpr static uint32_t UNPACK_TO_DEST      = 2; ///< Unpack ↔ Math sync on unpack-to-dest operations
    constexpr static uint32_t UNPACK_OPERAND_SYNC = 3; ///< Unpack ↔ Pack,Math sync on operand get/release
    constexpr static uint32_t PACK_DONE           = 4; ///< Pack iteration begin/end sync for perf events
    constexpr static uint32_t UNPACK_SYNC         = 5; ///< TRISC ↔ Unpack sync on hardware kernel
    /**
     * @brief Unpack or Math iteration sync for performance events
     *
     * Used for begin/end synchronization of unpack or math iterations for
     * performance event recording and delay insertion. Should only be used
     * for either unpack OR math, not both simultaneously.
     */
    constexpr static uint32_t UNPACK_MATH_DONE = 6;
    constexpr static uint32_t MATH_DONE        = 7; ///< Math completion sync when unpacking to dest

    /**
     * @brief Convert semaphore index to T6 semaphore bitmask
     *
     * Converts a semaphore index to the corresponding T6 hardware semaphore
     * bitmask format used by the synchronization hardware.
     *
     * @param sem_index Semaphore index (0-15)
     * @return 16-bit bitmask with bit set at sem_index position
     */
    constexpr static uint16_t t6_sem(const uint8_t sem_index)
    {
        return (1 << sem_index);
    }
};

/**
 * @struct mutex
 * @brief Mutex identifier constants for atomic operations
 *
 * Defines mutex identifiers used for atomic operations and critical sections
 * that require exclusive access across multiple threads.
 */
struct mutex
{
    constexpr static uint32_t REG_RMW = 0; ///< Atomic register read-modify-write operations
};

/**
 * @defgroup ArchitectureConstants Architecture-Specific Constants
 * @brief Hardware and architecture-specific sizing and configuration constants
 * @{
 */

constexpr uint8_t PC_BUF_SEMAPHORE_BASE = 8;  ///< Base address for semaphores in PC buffer
constexpr uint8_t MATH_HALF_DEST_SIZE   = 32; ///< Half destination register size (16x16 faces)
constexpr uint8_t MAX_CONFIG_STATES     = 2;  ///< Maximum number of configuration states

/** @} */ // end of ArchitectureConstants group

/**
 * @enum firmware_msg_e
 * @brief Firmware message types for kernel communication
 *
 * Defines message types used for communication between firmware and kernel
 * components. These messages control kernel execution, configuration, and
 * performance monitoring.
 */
enum firmware_msg_e
{
    FLIP_STATE_ID        = 1, ///< Flip configuration state identifier
    RUN_INSTRUCTIONS     = 2, ///< Execute kernel instructions
    RESET_DEST_OFFSET_ID = 3, ///< Reset destination offset identifier
    SET_PERF_SCRATCH     = 4  ///< Set performance scratch register
};

/** @} */ // end of KernelStructures group

} // namespace ckernel
