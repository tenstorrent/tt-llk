// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

/**
 * @file llk_hardware_limits.h
 * @brief Hardware limits and constraints for Wormhole B0 Tensix architecture
 *
 * @details This header defines all hardware-specific limits and constraints
 * for the Wormhole B0 architecture. These constants are used throughout the
 * LLK library for validation and optimization.
 *
 * **Architecture Reference**: Wormhole B0 Tensix Programming Guide
 * **Documentation Source**: /docs/llk/refactor_docs/
 */

#pragma once

#include <cstdint>

namespace llk_hardware_limits
{

// ============================================================================
// **MEMORY ARCHITECTURE LIMITS**
// ============================================================================

namespace memory
{
// L1 Scratchpad RAM Configuration
constexpr uint32_t L1_TOTAL_SIZE_KIB              = 1464; // Total L1 capacity per Tensix tile
constexpr uint32_t L1_BANK_COUNT                  = 16;   // Number of L1 banks
constexpr uint32_t L1_BANK_SIZE_KIB               = 91;   // Size per bank (91.5 KiB rounded down)
constexpr uint32_t L1_ACCESS_WIDTH_BITS           = 128;  // Bank access width
constexpr uint32_t L1_ACCESS_PORTS                = 16;   // Number of access ports
constexpr uint32_t L1_NARROW_WRITE_PENALTY_CYCLES = 5;    // Cycles for narrow writes (atomic RMW)
constexpr uint32_t L1_ATOMIC_OPERATION_CYCLES     = 5;    // Cycles for atomic operations

// Instruction Cache Limits
constexpr uint32_t ICACHE_RISCV_B_SIZE_BYTES  = 2048;  // RISCV B instruction cache
constexpr uint32_t ICACHE_RISCV_T0_SIZE_BYTES = 2048;  // RISCV T0 instruction cache
constexpr uint32_t ICACHE_RISCV_T1_SIZE_BYTES = 512;   // RISCV T1 instruction cache
constexpr uint32_t ICACHE_RISCV_T2_SIZE_BYTES = 2048;  // RISCV T2 instruction cache
constexpr uint32_t ICACHE_RISCV_NC_SIZE_BYTES = 512;   // RISCV NC instruction cache
constexpr uint32_t IRAM_RISCV_NC_SIZE_BYTES   = 16384; // RISCV NC instruction RAM (16 KiB)

// PCBuf Limits
constexpr uint32_t PCBUF_COUNT       = 3;  // Number of PCBufs per tile
constexpr uint32_t PCBUF_FIFO_DEPTH  = 16; // FIFO depth (32-bit values)
constexpr uint32_t PCBUF_CONFIG_BITS = 10; // Configuration bits per PCBuf
} // namespace memory

// ============================================================================
// **REGISTER FILE CONSTRAINTS**
// ============================================================================

namespace registers
{
// Destination Register (Dst) Limits
constexpr uint32_t DST_16B_ROWS                 = 1024; // Rows in 16-bit mode
constexpr uint32_t DST_16B_COLS                 = 16;   // Columns in 16-bit mode
constexpr uint32_t DST_32B_ROWS                 = 512;  // Rows in 32-bit mode
constexpr uint32_t DST_32B_COLS                 = 16;   // Columns in 32-bit mode
constexpr uint32_t DST_READ_AFTER_WRITE_LATENCY = 4;    // Cycles before read after write
constexpr uint32_t DST_ALIGNED_BLOCK_ROWS       = 8;    // Aligned block size (rows)
constexpr uint32_t DST_ALIGNED_BLOCK_COLS       = 16;   // Aligned block size (cols)

// Source Register (SrcA/SrcB) Limits
constexpr uint32_t SRC_BANK_COUNT      = 2;  // Banks per source register
constexpr uint32_t SRC_BANK_ROWS       = 64; // Rows per bank
constexpr uint32_t SRC_BANK_COLS       = 16; // Columns per bank
constexpr uint32_t SRC_DATA_WIDTH_BITS = 19; // Data width per element

// GPR (General Purpose Register) Limits
constexpr uint32_t GPR_COUNT_PACK   = 64; // GPRs available for pack operations
constexpr uint32_t GPR_COUNT_UNPACK = 64; // GPRs available for unpack operations
constexpr uint32_t GPR_COUNT_MATH   = 64; // GPRs available for math operations
} // namespace registers

// ============================================================================
// **PERFORMANCE CONSTRAINTS**
// ============================================================================

namespace performance
{
// Clock and Timing
constexpr uint32_t CLOCK_FREQUENCY_HZ          = 1000000000; // 1 GHz standard clock
constexpr uint32_t TENSIX_SYNC_OVERHEAD_CYCLES = 20;         // High-cost sync operation
constexpr uint32_t MOP_SYNC_OVERHEAD_CYCLES    = 8;          // Medium-cost sync operation

// Matrix Unit (FPU) Performance Limits
constexpr uint32_t MATRIX_UNIT_THROUGHPUT_IPC      = 1; // Instructions per cycle
constexpr uint32_t MATRIX_UNIT_LATENCY_CYCLES      = 5; // Instruction latency
constexpr uint32_t MATRIX_UNIT_MAX_FIDELITY_PHASES = 4; // Maximum fidelity phases

// Memory Bandwidth Limits (bits per cycle)
constexpr uint32_t RISCV_NARROW_STORE_BPC = 6;   // RISCV narrow store bandwidth
constexpr uint32_t RISCV_LOAD_BPC         = 18;  // RISCV load bandwidth
constexpr uint32_t MOVER_READ_BPC         = 93;  // Mover read bandwidth
constexpr uint32_t MOVER_WRITE_BPC        = 93;  // Mover write bandwidth
constexpr uint32_t UNPACKER_MAX_READ_BPC  = 640; // 5 × 128-bit reads
constexpr uint32_t PACKER_MAX_WRITE_BPC   = 512; // 4 × 128-bit writes
constexpr uint32_t NOC_READ_BPC           = 256; // NoC read bandwidth
constexpr uint32_t NOC_WRITE_BPC          = 256; // NoC write bandwidth

// SFPU Performance Targets
constexpr uint32_t SFPU_FAST_MODE_MIN_CYCLES    = 2;  // Fast mode minimum cycles
constexpr uint32_t SFPU_FAST_MODE_MAX_CYCLES    = 4;  // Fast mode maximum cycles
constexpr uint32_t SFPU_PRECISE_MODE_MIN_CYCLES = 6;  // Precise mode minimum cycles
constexpr uint32_t SFPU_PRECISE_MODE_MAX_CYCLES = 16; // Precise mode maximum cycles
} // namespace performance

// ============================================================================
// **DATA TYPE CONSTRAINTS**
// ============================================================================

namespace data_types
{
// Precision Limits
constexpr uint32_t FP32_EXPONENT_BITS = 8;  // FP32 exponent width
constexpr uint32_t FP32_MANTISSA_BITS = 23; // FP32 mantissa width
constexpr uint32_t BF16_EXPONENT_BITS = 8;  // BF16 exponent width
constexpr uint32_t BF16_MANTISSA_BITS = 7;  // BF16 mantissa width
constexpr uint32_t FP16_EXPONENT_BITS = 5;  // FP16 exponent width
constexpr uint32_t FP16_MANTISSA_BITS = 10; // FP16 mantissa width
constexpr uint32_t TF32_EXPONENT_BITS = 8;  // TF32 exponent width
constexpr uint32_t TF32_MANTISSA_BITS = 10; // TF32 mantissa width

// Integer Type Ranges
constexpr int32_t INT8_MIN_VALUE      = -1023;        // Integer "8" minimum
constexpr int32_t INT8_MAX_VALUE      = 1023;         // Integer "8" maximum
constexpr int32_t INT8_PRACTICAL_MIN  = -127;         // Practical range minimum
constexpr int32_t INT8_PRACTICAL_MAX  = 127;          // Practical range maximum
constexpr int32_t INT16_MAX_MAGNITUDE = 32767;        // Integer "16" max magnitude
constexpr int64_t INT32_MAX_MAGNITUDE = 2147483647LL; // Integer "32" max magnitude
} // namespace data_types

// ============================================================================
// **INSTRUCTION AND TEMPLATE LIMITS**
// ============================================================================

namespace instructions
{
// Template System Limits
constexpr uint32_t MAX_OUTER_LOOP_COUNT       = 65535; // Maximum outer loop iterations
constexpr uint32_t MAX_INNER_LOOP_COUNT       = 65535; // Maximum inner loop iterations
constexpr uint32_t TEMPLATE_INSTRUCTION_SLOTS = 8;     // Available instruction slots in template

// Address Mode Limits
constexpr uint32_t ADDRESS_MODE_COUNT    = 8;  // Number of configurable address modes
constexpr uint32_t ADDRESS_INCREMENT_MAX = 15; // Maximum address increment value

// Configuration State Limits
constexpr uint32_t MAX_CFG_STATES          = 16;      // Maximum configuration states
constexpr uint32_t MAX_DEST_OFFSETS        = 16;      // Maximum destination offsets
constexpr uint32_t MAILBOX_TIMEOUT_DEFAULT = 1000000; // Default mailbox timeout cycles
} // namespace instructions

// ============================================================================
// **TILE AND FACE CONSTRAINTS**
// ============================================================================

namespace tiles
{
// Standard Tile Dimensions
constexpr uint32_t TILE_HEIGHT = 32; // Standard tile height
constexpr uint32_t TILE_WIDTH  = 32; // Standard tile width
constexpr uint32_t FACE_HEIGHT = 16; // Face height (half tile)
constexpr uint32_t FACE_WIDTH  = 16; // Face width (half tile)

// Valid Face Counts
constexpr uint32_t VALID_FACE_COUNT_1 = 1; // Single face operation
constexpr uint32_t VALID_FACE_COUNT_2 = 2; // Dual face operation
constexpr uint32_t VALID_FACE_COUNT_4 = 4; // Quad face operation

// Tile Alignment Requirements
constexpr uint32_t TILE_DIM_ALIGNMENT = 16; // Tile dimensions must be multiples of 16
constexpr uint32_t MAX_TILE_DIM       = 32; // Maximum tile dimension
} // namespace tiles

// ============================================================================
// **BANDWIDTH AND THROUGHPUT CALCULATIONS**
// ============================================================================

namespace throughput
{
// Matrix Unit Theoretical Maximum (TFLOP/s at 1 GHz)
constexpr float MATRIX_UNIT_1_PHASE_TFLOPS = 4.096f; // 1 fidelity phase
constexpr float MATRIX_UNIT_2_PHASE_TFLOPS = 2.048f; // 2 fidelity phases
constexpr float MATRIX_UNIT_3_PHASE_TFLOPS = 1.366f; // 3 fidelity phases
constexpr float MATRIX_UNIT_4_PHASE_TFLOPS = 1.024f; // 4 fidelity phases

// SFPU Theoretical Maximum (TFLOP/s at 1 GHz)
constexpr float SFPU_FP32_TFLOPS = 0.064f; // SFPU FP32 performance

// Wormhole B0 Tile Counts (for system calculations)
constexpr uint32_t WORMHOLE_B0_MIN_TILES = 64; // Minimum tile count
constexpr uint32_t WORMHOLE_B0_STD_TILES = 72; // Standard tile count (n150)
constexpr uint32_t WORMHOLE_B0_MAX_TILES = 80; // Maximum tile count (Galaxy)
} // namespace throughput

} // namespace llk_hardware_limits
