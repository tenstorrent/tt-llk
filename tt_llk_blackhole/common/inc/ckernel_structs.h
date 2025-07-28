// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

/**
 * @file ckernel_structs.h
 * @brief Core data structures for kernel synchronization and communication
 *
 * @details Defines fundamental data structures used for synchronization, communication,
 * and coordination within the compute kernel framework. These structures provide the
 * foundation for multi-threaded kernel execution and firmware-kernel interaction.
 *
 * **Structure Categories:**
 * - **Synchronization**: Semaphores for thread coordination
 * - **Mutual Exclusion**: Mutexes for atomic operations
 * - **Communication**: Firmware message types and protocols
 * - **Configuration**: System constants and limits
 *
 * **Threading Model:**
 * The kernel framework operates with multiple specialized threads:
 * - Unpack Thread: Data input and formatting
 * - Math Thread: Mathematical computations
 * - Pack Thread: Data output and formatting
 *
 * **Hardware Integration:**
 * All structures are designed for direct hardware interaction with:
 * - Tensix core synchronization primitives
 * - Hardware semaphore and mutex systems
 * - Firmware communication interfaces
 * - Memory management subsystems
 */

namespace ckernel
{

/**
 * @brief Semaphore management structure for inter-thread synchronization
 *
 * @details Provides semaphore identifiers and utilities for coordinating execution
 * between different threads in the kernel pipeline. Each semaphore serves a specific
 * synchronization purpose in the compute pipeline flow.
 *
 * **Semaphore Types and Purposes:**
 * - **MATH_PACK**: Synchronizes math and pack threads on destination register access
 * - **UNPACK_TO_DEST**: Coordinates unpack and math threads for destination writes  
 * - **UNPACK_OPERAND_SYNC**: Manages operand sharing between unpack, math, and pack
 * - **PACK_DONE**: Signals completion of pack operations for performance monitoring
 * - **UNPACK_SYNC**: Synchronizes between TRISC and unpack hardware kernels
 * - **UNPACK_MATH_DONE**: Shared semaphore for unpack or math completion events
 * - **MATH_DONE**: Ensures math completion before unpacking to destination
 *
 * **Hardware Mapping:**
 * Each semaphore corresponds to hardware synchronization primitives in the Tensix
 * architecture, enabling efficient thread coordination without software overhead.
 *
 * **Performance Characteristics:**
 * - Hardware-accelerated synchronization
 * - Minimal overhead for critical path operations
 * - Supports real-time kernel execution requirements
 * - Optimized for high-frequency synchronization patterns
 */
// Semaphores mapping and trisc space -> tensix space conversion
struct semaphore
{
    /**
     * @brief Math-Pack synchronization semaphore
     * @details Coordinates access to destination registers between math and pack threads
     */
    constexpr static uint32_t MATH_PACK           = 1; // math <-> pack sync on dest register
    
    /**
     * @brief Unpack-to-destination synchronization semaphore  
     * @details Synchronizes unpack and math threads when unpacking directly to destination
     */
    constexpr static uint32_t UNPACK_TO_DEST      = 2; // unpack <-> math sync on unpack to dest
    
    /**
     * @brief Operand synchronization semaphore
     * @details Manages operand get/release coordination between unpack, math, and pack threads
     */
    constexpr static uint32_t UNPACK_OPERAND_SYNC = 3; // unpack <-> pack, math sync on operand get/release
    
    /**
     * @brief Pack completion synchronization semaphore
     * @details Signals beginning and end of pack iterations for performance monitoring and delay insertion
     */
    constexpr static uint32_t PACK_DONE           = 4; // Wait for beginning and end of each pack-iteration. For recording perf events and inserting delay.
    
    /**
     * @brief Unpack hardware synchronization semaphore
     * @details Synchronizes between TRISC and unpack hardware kernel operations
     */
    constexpr static uint32_t UNPACK_SYNC         = 5; // trisc <-> unpack sync on hw kernel
    
    // Wait for beginning and end of each unpack or math iteration. For recording perf events and inserting delay.
    // This semaphore should only be used for either unpack or math. Not both at the same time.
    /**
     * @brief Unpack/Math completion synchronization semaphore
     * @details Shared semaphore for unpack or math completion events (exclusive use)
     * @warning Should only be used for either unpack OR math, not both simultaneously
     */
    constexpr static uint32_t UNPACK_MATH_DONE = 6;
    
    /**
     * @brief Math completion synchronization semaphore
     * @details Ensures math operations complete before unpacking to destination registers
     */
    constexpr static uint32_t MATH_DONE        = 7; // wait for math to finish when unpacking to dest

    /**
     * @brief Convert semaphore index to Tensix hardware semaphore format
     *
     * @details Converts a semaphore index to the hardware-specific bit pattern used
     * by Tensix architecture semaphore systems. Each semaphore is represented as a
     * single bit in a 16-bit field.
     *
     * **Hardware Format:**
     * - Uses bit position encoding (1 << sem_index)
     * - Compatible with Tensix T6 semaphore hardware
     * - Supports up to 16 concurrent semaphores
     * - Direct hardware register interface
     *
     * @param sem_index Semaphore index (0-15)
     * @return Hardware semaphore bit pattern for T6 architecture
     *
     * @note Index must be valid (0-15) for proper hardware operation
     * @note Result used directly in hardware semaphore register writes
     * @note Bit position encoding enables efficient hardware implementation
     */
    constexpr static uint16_t t6_sem(const uint8_t sem_index)
    {
        return (1 << sem_index);
    }
};

/**
 * @brief Mutex management structure for atomic operations
 *
 * @details Provides mutex identifiers for protecting critical sections that require
 * atomic read-modify-write operations. Mutexes ensure data consistency when multiple
 * threads access shared resources.
 *
 * **Mutex Types:**
 * - **REG_RMW**: Protects atomic register read-modify-write operations
 *
 * **Atomic Operation Protection:**
 * Mutexes are essential for operations that must complete atomically:
 * - Register updates from multiple threads
 * - Shared counter modifications
 * - Configuration state changes
 * - Critical resource allocation
 *
 * **Hardware Integration:**
 * - Maps to hardware mutex primitives in Tensix architecture
 * - Provides hardware-accelerated mutual exclusion
 * - Minimal overhead for performance-critical sections
 * - Supports nested locking with proper unlock ordering
 */
struct mutex
{
    /**
     * @brief Register read-modify-write mutex
     * @details Protects atomic register operations when accessed from different threads
     */
    constexpr static uint32_t REG_RMW = 0; // used for atomic register read-modify-write from different threads
};

/**
 * @brief Base address for PC buffer semaphores
 * @details Starting index for semaphores allocated within the PC buffer system
 */
constexpr uint8_t PC_BUF_SEMAPHORE_BASE = 8;  // base address for semaphores in PC buffer

/**
 * @brief Architecture-specific half destination register size
 * @details Size of half the destination register file in 16x16 face units (Blackhole architecture)
 */
constexpr uint8_t MATH_HALF_DEST_SIZE   = 32; // arch specific 1/2 dest registers size in 16x16 faces

/**
 * @brief Maximum number of configuration states supported
 * @details Limit on concurrent configuration states for kernel context switching
 */
constexpr uint8_t MAX_CONFIG_STATES     = 2;

/**
 * @brief Firmware message types for kernel communication
 *
 * @details Enumeration of message types used for communication between firmware
 * and compute kernels. These messages control kernel execution, configuration,
 * and performance monitoring.
 *
 * **Message Categories:**
 * - **Control Messages**: Execution control and state management
 * - **Configuration Messages**: Runtime configuration updates
 * - **Performance Messages**: Monitoring and profiling support
 *
 * **Message Flow:**
 * ```
 * Firmware → Kernel: Command/configuration messages
 * Kernel → Firmware: Status and completion notifications
 * ```
 *
 * **Usage Context:**
 * - Sent via firmware-kernel communication interface
 * - Processed by kernel message handling routines
 * - Critical for kernel lifecycle management
 * - Essential for dynamic reconfiguration support
 */
// Firmware messages to ckernels
enum firmware_msg_e
{
    /**
     * @brief Flip configuration state ID
     * @details Commands kernel to switch to alternate configuration state for context switching
     */
    FLIP_STATE_ID        = 1,
    
    /**
     * @brief Execute kernel instructions
     * @details Commands kernel to begin execution of loaded instruction sequence
     */
    RUN_INSTRUCTIONS     = 2,
    
    /**
     * @brief Reset destination offset identifier
     * @details Resets destination register offset tracking to initial state
     */
    RESET_DEST_OFFSET_ID = 3,
    
    /**
     * @brief Set performance scratch area
     * @details Configures scratch memory area for performance monitoring and profiling
     */
    SET_PERF_SCRATCH     = 4
};

} // namespace ckernel
