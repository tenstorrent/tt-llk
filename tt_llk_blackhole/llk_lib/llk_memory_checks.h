// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cstdint>

namespace ckernel
{

// Memory layout constants based on memory.blackhole.ld
// L1 Layout:
// BRISC_CODE:              0x0     - 0x4000  (16KB)
// BRISC_LOADER_INIT_MEM:   0x4000  - 0x6000  (8KB)
// TRISC0_CODE:             0x6000  - 0xA000  (16KB)
// TRISC0_LOADER_INIT_MEM:  0xA000  - 0xB000  (4KB)
// TRISC1_CODE:             0xB000  - 0xF000  (16KB)
// TRISC1_LOADER_INIT_MEM:  0xF000  - 0x10000 (4KB)
// TRISC2_CODE:             0x10000 - 0x14000 (16KB)
// TRISC2_LOADER_INIT_MEM:  0x14000 - 0x15000 (4KB)
// Firmware End:            0x15000 (84KB)
// RUNTIME_ARGS:            0x64000 - 0x64400 (1KB)
// L1 End:                  0x16E000 (1463KB total L1)

constexpr std::uint32_t L1_FIRMWARE_END     = 0x15000;
constexpr std::uint32_t L1_REGION1_START    = L1_FIRMWARE_END;
constexpr std::uint32_t L1_REGION1_END      = 0x64000;
constexpr std::uint32_t L1_RUNTIME_ARGS_END = 0x64400;
constexpr std::uint32_t L1_REGION2_START    = L1_RUNTIME_ARGS_END;
constexpr std::uint32_t L1_REGION2_END      = 0x16E000;

/**
 * @brief Transform L1 address to the format used by LLK functions
 *
 * @param buffer_address The physical L1 address
 * @return Transformed address (divided by 16 and decremented)
 */
inline static std::uint32_t L1_ADDRESS(std::uint32_t buffer_address)
{
    return (buffer_address / 16) - 1;
}

/**
 * @brief Check if an address is in L1 Region 1 (Firmware End → Runtime Args)
 *
 * @param address The L1 address to check
 * @return true if address is in Region 1 (0x15000 - 0x64000)
 */
inline static bool is_in_region1(const std::uint32_t address)
{
    return (address >= L1_ADDRESS(ckernel::L1_REGION1_START) && address < L1_ADDRESS(ckernel::L1_RUNTIME_ARGS_END));
}

/**
 * @brief Check if an address is in L1 Region 2 (Runtime Args → L1 End)
 *
 * @param address The L1 address to check
 * @return true if address is in Region 2 (0x64400 - 0x16E000)
 */
inline static bool is_in_region2(const std::uint32_t address)
{
    return (address >= L1_ADDRESS(ckernel::L1_REGION2_START) && address < L1_ADDRESS(ckernel::L1_REGION2_END));
}

/**
 * @brief Check if an address is a valid L1 address in Region 1 or Region 2
 *
 * @param address The L1 address to validate
 * @return true if address is in Region 1 or Region 2
 */
inline static bool is_valid_L1_address(const std::uint32_t address)
{
    return is_in_region1(address) || is_in_region2(address);
}

} // namespace ckernel
