// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cstdint>

#include "core_config.h"
#include "dev_mem_map.h"

/**
 * @brief Transform L1 address to the format used by LLK functions
 *
 * LLK (Low-Level Kernel) uses a transformed address format where addresses are
 * divided by 16 (shifted right by 4 bits) and then decremented by 1.
 * This transformation is required for all L1 buffer addresses used in LLK operations.
 *
 * @param buffer_address The physical L1 address (byte-aligned)
 * @return Transformed address for LLK use: (address / 16) - 1
 */
constexpr inline std::uint32_t L1_ADDRESS(std::uint32_t buffer_address)
{
    return (buffer_address >> 4) - 1;
}

namespace ckernel
{

// L1 Memory Layout for Tile Data Verification
// ============================================
//
// This file defines the valid memory regions in L1 where tile data can be safely stored.
// The layout is based on dev_mem_map.h hardware definitions and excludes all reserved
// system memory areas.
//
// Memory Region Breakdown (from dev_mem_map.h):
// ----------------------------------------------
// 0x0     - 0x79F0   (30.48 KB): RESERVED - System firmware, mailboxes, counters, routing
//                                 tables, fabric metadata, and packet header pools
//                                 Calculated as: MEM_MAP_END = MEM_PACKET_HEADER_POOL_BASE
//                                                            + MEM_PACKET_HEADER_POOL_SIZE
//
// 0x79F0  - 0x16E000 (1433.52 KB): AVAILABLE - Valid memory for tile data buffers
//                                   This is the primary usable L1 region for computation
//
// Total L1: 1464 KB (MEM_L1_SIZE)
//
// Exact Value Calculations (all from dev_mem_map.h chain):
// ---------------------------------------------------------
// MEM_MAP_END calculation chain:
//   MEM_MAILBOX_BASE (16) + MEM_MAILBOX_SIZE (12768) = MEM_MAILBOX_END (12784)
//   → MEM_ZEROS_BASE = ((12784 + 31) & ~31) = 12800
//   → MEM_LLK_DEBUG_BASE = 12800 + 512 = 13312
//   → MEM_BRISC_FIRMWARE_BASE = 13312 + 1024 = 14336
//   → MEM_NCRISC_FIRMWARE_BASE = 14336 + 6144 = 20480
//   → MEM_TRISC0_FIRMWARE_BASE = 20480 + 2048 = 22528
//   → MEM_TRISC1_FIRMWARE_BASE = 22528 + 1536 = 24064
//   → MEM_TRISC2_FIRMWARE_BASE = 24064 + 1536 = 25600
//   → MEM_NOC_COUNTER_BASE = 25600 + 1536 = 27136 (0x6A00)
//   → MEM_FABRIC_COUNTER_BASE = 27136 + 80 = 27216 (0x6A50)
//   → MEM_TENSIX_ROUTING_TABLE_BASE = 27216 + 32 = 27248 (0x6A70)
//   → MEM_TENSIX_FABRIC_CONNECTIONS_BASE = 27248 + 484 + 768 + 1024 + 12 = 29536 (0x7360)
//   → MEM_PACKET_HEADER_POOL_BASE = 29536 + 656 = 30192 (0x75F0)
//   → MEM_MAP_END = 30192 + 1024 = 31216 (0x79F0)

// Start of available memory region for tile data (LLK transformed address)
// Immediately after all reserved system memory (firmware, mailboxes, counters, routing tables)
// Physical Boundary: 0x79F0 (31,216 bytes = 30.48 KB)
// Transformed Value: L1_ADDRESS(MEM_MAP_END) = (0x79F0 / 16) - 1
// Derived from: MEM_MAP_END from dev_mem_map.h
constexpr std::uint32_t L1_REGION_START = L1_ADDRESS(MEM_MAP_END);

// End of L1 memory (LLK transformed address) - total available L1 size
// This is the absolute upper bound for any L1 address
// Physical Boundary: 0x16E000 (1,499,136 bytes = 1464 KB)
// Transformed Value: L1_ADDRESS(MEM_L1_SIZE) = (0x16E000 / 16) - 1
// Defined by: MEM_L1_SIZE (1464 * 1024) from dev_mem_map.h
constexpr std::uint32_t L1_REGION_END = L1_ADDRESS(MEM_L1_SIZE);

} // namespace ckernel

/**
 * @brief Check if an LLK-transformed address is valid for tile data
 *
 * Validates that the transformed address falls within the usable L1 memory region:
 * - Start (transformed): L1_ADDRESS(0x79F0) = L1_ADDRESS(MEM_MAP_END)
 * - End (transformed):   L1_ADDRESS(0x16E000) = L1_ADDRESS(MEM_L1_SIZE)
 * - Physical Range: 0x79F0 - 0x16E000 (1,433,520 bytes ≈ 1,399.5 KB)
 *
 * This single contiguous region is available for all tile data and computational buffers.
 * The reserved area (0x0 - 0x79F0 ≈ 30.48 KB) contains system firmware, mailboxes, counters,
 * routing tables, fabric metadata, and packet header pools.
 *
 * IMPORTANT: This function takes and compares LLK TRANSFORMED addresses.
 * If you have a physical address, transform it first using L1_ADDRESS():
 *   transformed_addr = L1_ADDRESS(physical_addr) = (physical_addr / 16) - 1
 * Or pass this function the result from L1_ADDRESS() calls.
 *
 * @param address The LLK-transformed L1 address to validate
 * @return true if address is within valid tile data region [L1_REGION_START, L1_REGION_END)
 */
inline static bool is_valid_L1_address(const std::uint32_t address)
{
    return (address >= ckernel::L1_REGION_START && address < ckernel::L1_REGION_END);
}
