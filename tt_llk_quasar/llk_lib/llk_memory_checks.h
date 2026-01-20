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
 * Hardware L1 address fields use 16-byte granularity, not byte addresses.
 * The hardware expects addresses in units of 16-byte blocks, so we convert
 * byte addresses to 16-byte-aligned addresses by dividing by 16.
 *
 * For quasar, tensix L1 address fields do not use an off-by-one convention,
 * therefore, no -1 is needed in the transformation (like in Wormhole/Blackhole).
 *
 * @param buffer_address The physical L1 address (byte-aligned)
 * @return Transformed address for LLK use: address / 16
 */
constexpr inline std::uint32_t L1_ADDRESS(std::uint32_t buffer_address)
{
    return buffer_address >> 4;
}

namespace ckernel
{

// L1 Memory Layout for Tile Data Verification (Quasar Architecture)
// ==================================================================
//
// This file defines the valid memory regions in L1 where tile data can be safely stored.
// The layout is based on dev_mem_map.h hardware definitions and excludes all reserved
// system memory areas.
//
// QUASAR: Significantly larger L1 memory (4 MB) with 8 DM cores and 4 TRISC cores
// Much more reserved memory for firmware, DM kernels, global/local storage compared to previous generations
//
// Memory Region Breakdown (from dev_mem_map.h):
// -----------------------------------------------
// 0x0        - 0x803B0   (512.92 KB): RESERVED - System firmware (mailbox, debug, DM/TRISC FW),
//                                       global storage (8 DM cores + 4 TRISC cores),
//                                       local storage (8 DM cores), DM kernel code (8 × 48KB),
//                                       NoC/Fabric counters, routing tables, and packet header pools
//                                       Calculated as: MEM_MAP_END
//
// 0x803B0  - 0x400000  (3583.08 KB = 3.50 MB): AVAILABLE - Valid memory for tile data buffers
//                                                This is the primary usable L1 region for computation
//
// Total L1: 4 MB (MEM_L1_SIZE)
//
// Exact Value Calculations (all from dev_mem_map.h chain):
// ---------------------------------------------------------
// MEM_MAP_END calculation chain (Quasar specific with 8 DM cores):
//   MEM_MAILBOX_BASE (16) + MEM_MAILBOX_SIZE (30272) = MEM_MAILBOX_END (30288)
//   → MEM_ZEROS_BASE = ((30288 + 31) & ~31) = 30304
//   → MEM_LLK_DEBUG_BASE = 30304 + 512 = 30816
//   → MEM_DM_FIRMWARE_BASE = 30816 + 1024 = 31840
//   → [8 DM firmware slots, then 4 TRISC firmware slots]
//   → MEM_TRISC3_FIRMWARE_BASE + SIZE = 50272 (0xC460)
//   → MEM_DM_GLOBAL_BASE = 50272 (8 × 1KB DM global) → 58464
//   → MEM_TRISC_GLOBAL_BASE = 58464 (4 × 1KB TRISC global) → 60544
//   → MEM_DM_LOCAL_BASE = 60544 (8 × 8KB DM local) → 125024
//   → MEM_TRISC_LOCAL_BASE = 125024 (skipped in calc, uses DM_LOCAL_BASE reference)
//   → MEM_DM_KERNEL_BASE = 125024 (8 × 48KB DM kernel) = 520224 (0x7E860)
//   → MEM_NOC_COUNTER_BASE = 520224 + 80 = 520304 (0x7E8B0)
//   → MEM_FABRIC_COUNTER_BASE = 520304 + 112 = 520416 (0x7E920) [8 DMs: 3×8×4+16 bytes]
//   → MEM_FABRIC_CONNECTION_LOCK_BASE = 520416 + 144 = 520560 (0x7E9B0)
//   → MEM_TENSIX_ROUTING_TABLE_BASE = 520560 + 484 + 1024 + 1024 + 12 = 522704 (0x7F3A0)
//   → MEM_TENSIX_FABRIC_CONNECTIONS_BASE = 522704 + 656 = 523360 (0x7F630)
//   → MEM_PACKET_HEADER_POOL_BASE = 523360 + 3456 = 526816 (0x803B0)
//   → MEM_MAP_END = 526816

// Start of available memory region for tile data (LLK transformed address)
// Immediately after all reserved system memory (firmware, DM kernel code, global/local storage,
// counters, routing tables, fabric metadata)
// Physical Boundary: 0x803B0 (526,816 bytes = 512.92 KB)
// Transformed Value: L1_ADDRESS(MEM_MAP_END) = (0x803B0 / 16) - 1
// Derived from: MEM_MAP_END from dev_mem_map.h
constexpr std::uint32_t L1_REGION_START = L1_ADDRESS(MEM_MAP_END);

// End of L1 memory (LLK transformed address) - total available L1 size
// This is the absolute upper bound for any L1 address
// Physical Boundary: 0x400000 (4,194,304 bytes = 4 MB)
// Transformed Value: L1_ADDRESS(MEM_L1_SIZE) = (0x400000 / 16) - 1
// Defined by: MEM_L1_SIZE (4 * 1024 * 1024) from dev_mem_map.h
constexpr std::uint32_t L1_REGION_END = L1_ADDRESS(MEM_L1_SIZE);

} // namespace ckernel

/**
 * @brief Check if an LLK-transformed address is valid for tile data
 *
 * Validates that the transformed address falls within the usable L1 memory region:
 * - Start (transformed): L1_ADDRESS(0x803B0) = L1_ADDRESS(MEM_MAP_END)
 * - End (transformed):   L1_ADDRESS(0x400000) = L1_ADDRESS(MEM_L1_SIZE)
 * - Physical Range: 0x803B0 - 0x400000 (3,669,072 bytes ≈ 3,583.1 KB = 3.50 MB)
 *
 * This single contiguous region is available for all tile data and computational buffers.
 * The reserved area (0x0 - 0x803B0 ≈ 512.92 KB) contains system firmware, DM kernel code,
 * global/local storage for 8 DM cores and 4 TRISC cores, NoC counters, fabric counters,
 * routing tables, fabric metadata, and packet header pools.
 *
 * QUASAR SPECIFICS:
 * - Significantly larger L1 (4 MB vs 1.5 MB in Blackhole): +2.5 MB available
 * - 8 DM cores instead of TRISC-only: Expanded compute architecture
 * - 4 TRISC cores (vs 4 in previous, but shared with DM architecture)
 * - Much larger firmware footprint (1 DM @ 12KB + 4 TRISC @ 1.5KB each)
 * - Larger mailbox (30.3 KB vs 12.8 KB): Support for expanded fabric
 * - 8 DM global storage (8 KB) + per-DM local storage (64 KB total)
 * - Large DM kernel allocation (8 × 48 KB = 384 KB for DM code)
 * - Fabric counter scaled for 8 DMs (3 × 8 × 4 + 16 bytes)
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
