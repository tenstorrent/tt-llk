// SPDX-FileCopyrightText: © 2026 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cstdint>

#include "core_config.h"
#include "dev_mem_map.h"

/**
 * @brief Transform L1 address to the format used by LLK functions
 *
 * This transformation consists of two parts:
 *
 * 1. Division by 16 (shift right by 4 bits):
 *    Hardware L1 address fields use 16-byte granularity, not byte addresses.
 *    The hardware expects addresses in units of 16-byte blocks, so we convert
 *    byte addresses to 16-byte-aligned addresses by dividing by 16.
 *
 * 2. Decrement by 1 (on Tensix architectures like Wormhole/Blackhole):
 *    Tensix L1 address fields use an off-by-one convention: you program (addr_16B - 1),
 *    and the hardware internally increments the value before using it. This hardware
 *    quirk requires the subtraction of 1 in the address transformation.
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

// L1 Memory Layout for Tile Data Verification (Blackhole Architecture)
// ====================================================================
//
// This file defines the valid memory regions in L1 where tile data can be safely stored.
// The layout is based on dev_mem_map.h hardware definitions and excludes all reserved
// system memory areas.
//
// BLACKHOLE: Larger L1 size (1536 KB) and additional features (6-directional fabric)
// compared to Wormhole (1464 KB with 4-directional fabric)
//
// Memory Region Breakdown (from dev_mem_map.h):
// -----------------------------------------------
// 0x0      - 0x8740   (33.81 KB): RESERVED - System firmware, mailboxes, counters, routing
//                                   tables, fabric metadata, and packet header pools
//                                   Calculated as: MEM_MAP_END
//
// 0x8740  - 0x180000  (1502.19 KB): AVAILABLE - Valid memory for tile data buffers
//                                     This is the primary usable L1 region for computation
//
// Total L1: 1536 KB (MEM_L1_SIZE)
//
// Exact Value Calculations (all from dev_mem_map.h chain):
// ---------------------------------------------------------
// MEM_MAP_END calculation chain:
//   MEM_L1_INLINE_BASE (32) [new in Blackhole]
//   MEM_MAILBOX_BASE (96) + MEM_MAILBOX_SIZE (12768) = MEM_MAILBOX_END (12864)
//   → MEM_ZEROS_BASE = ((12864 + 31) & ~31) = 12896
//   → MEM_LLK_DEBUG_BASE = 12896 + 512 = 13408
//   → MEM_BRISC_FIRMWARE_BASE = 13408 + 1024 = 14432
//   → MEM_NCRISC_FIRMWARE_BASE = 14432 + 7168 (7K) = 21600
//   → MEM_TRISC0_FIRMWARE_BASE = 21600 + 1536 = 23136
//   → MEM_TRISC1_FIRMWARE_BASE = 23136 + 1536 = 24672
//   → MEM_TRISC2_FIRMWARE_BASE = 24672 + 1536 = 26208
//   → MEM_NOC_COUNTER_BASE = 26208 + 1536 = 27744 (0x6C40)
//   → MEM_FABRIC_COUNTER_BASE = 27744 + 80 = 27824 (0x6C90)
//   → MEM_FABRIC_CONNECTION_LOCK_BASE = 27824 + 32 = 27856 (0x6CB0) [new in BH]
//   → MEM_TENSIX_ROUTING_TABLE_BASE = 27856 + 144 = 28000 (0x6D40)
//   → MEM_TENSIX_FABRIC_CONNECTIONS_BASE = 28000 + 484 + 1024 + 1024 + 12 = 30544 (0x7730)
//   → MEM_PACKET_HEADER_POOL_BASE = 30544 + 656 = 31200 (0x79C0)
//   → MEM_MAP_END = 31200 + 3456 = 34656 (0x8740)

// Start of available memory region for tile data (LLK transformed address)
// Immediately after all reserved system memory (firmware, mailboxes, counters, routing tables, fabric metadata)
// Physical Boundary: 0x8740 (34,624 bytes = 33.81 KB)
// Transformed Value: L1_ADDRESS(MEM_MAP_END) = (0x8740 / 16) - 1
// Derived from: MEM_MAP_END from dev_mem_map.h
constexpr std::uint32_t L1_REGION_START = L1_ADDRESS(MEM_MAP_END);

// End of L1 memory (LLK transformed address) - total available L1 size
// This is the absolute upper bound for any L1 address
// Physical Boundary: 0x180000 (1,572,864 bytes = 1536 KB)
// Transformed Value: L1_ADDRESS(MEM_L1_SIZE) = (0x180000 / 16) - 1
// Defined by: MEM_L1_SIZE (1536 * 1024) from dev_mem_map.h
constexpr std::uint32_t L1_REGION_END = L1_ADDRESS(MEM_L1_SIZE);

/**
 * @brief Check if an LLK-transformed address is valid for tile data
 *
 * Validates that the transformed address falls within the usable L1 memory region:
 * - Start (transformed): L1_ADDRESS(0x8740) = L1_ADDRESS(MEM_MAP_END)
 * - End (transformed):   L1_ADDRESS(0x180000) = L1_ADDRESS(MEM_L1_SIZE)
 * - Physical Range: 0x8740 - 0x180000 (1,538,240 bytes ≈ 1,502.2 KB)
 *
 * This single contiguous region is available for all tile data and computational buffers.
 * The reserved area (0x0 - 0x8740 ≈ 33.81 KB) contains system firmware, mailboxes, counters,
 * routing tables, fabric metadata, fabric connection locks, and packet header pools.
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

} // namespace ckernel
