// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cstddef>
#include <cstdint>

#include "ckernel.h"

#define DO_PRAGMA(x) _Pragma(#x)

// Logic to convert zone name -> 16bit numeric id
#define Stringize(L)       #L
#define ExpandStringize(L) Stringize(L)
#define $Line              ExpandStringize(__LINE__)

constexpr uint32_t hashString32(const char* str, size_t n, uint32_t basis = UINT32_C(2166136261))
{
    return n == 0 ? basis : hashString32(str + 1, n - 1, (basis ^ str[0]) * UINT32_C(16777619));
}

template <size_t N>
constexpr uint32_t hashString16(const char (&s)[N])
{
    auto longHash = hashString32(s, N - 1);
    return ((longHash) ^ (longHash >> 16)) & 0xFFFF;
}

// clang-format off
#define ZONE_FULL_NAME(name) "KERNEL_PROFILER" ":" __FILE__ ":" $Line ":" name
// clang-format on

#define ZONE_ID(name) hashString16(ZONE_FULL_NAME(name))

namespace llk_profiler
{

constexpr uint32_t ENTRY_TYPE_SHAMT = 28;
constexpr uint32_t ENTRY_ID_SHAMT   = ENTRY_TYPE_SHAMT - 16;
constexpr uint32_t ENTRY_META_SHAMT = ENTRY_ID_SHAMT;

constexpr uint32_t ENTRY_META_MASK = ~((1 << ENTRY_META_SHAMT) - 1);

constexpr uint32_t ENTRY_EXISTS_BIT = 0b1000 << ENTRY_TYPE_SHAMT;

constexpr uint32_t TIMESTAMP_ENTRY      = 0b1000 << ENTRY_TYPE_SHAMT;
constexpr uint32_t TIMESTAMP_DATA_ENTRY = 0b1001 << ENTRY_TYPE_SHAMT;
constexpr uint32_t ZONE_START_ENTRY     = 0b1010 << ENTRY_TYPE_SHAMT;
constexpr uint32_t ZONE_END_ENTRY       = 0b1011 << ENTRY_TYPE_SHAMT;

// Initialize id of the core executing the kernel
#if defined(LLK_TRISC_UNPACK)
constexpr uint32_t trisc_id = 0;
#elif defined(LLK_TRISC_MATH)
constexpr uint32_t trisc_id = 1;
#elif defined(LLK_TRISC_PACK)
constexpr uint32_t trisc_id = 2;
#endif

constexpr uint32_t BUFFER_SIZE  = 0x400;
constexpr uint32_t BUFFER_START = 0x16E000 - 3 * (BUFFER_SIZE * sizeof(uint32_t));

using buffer_ptr_t           = volatile tt_l1_ptr uint32_t (*)[BUFFER_SIZE];
volatile buffer_ptr_t buffer = reinterpret_cast<buffer_ptr_t>(BUFFER_START);
uint32_t write_idx           = 0;
uint32_t open_zone_cnt       = 0;

__attribute__((noinline)) void init()
{
    open_zone_cnt = 0;

    // Each core calls this function to initialize the profiler
    write_idx = 0;
    for (uint32_t i = 0; i < BUFFER_SIZE; i++)
    {
        buffer[trisc_id][i] = 0;
    }
}

__attribute__((always_inline)) inline bool is_buffer_full()
{
    // the buffer is considered full when there is not enough space to store:
    // - timestamp with data (TIMESTAMP_DATA_ENTRY) (size = 8B)
    // - new zone (ZONE_START_ENTRY + ZONE_END_ENTRY) (size = 8B)
    // after closing all of the currently open zones
    return (BUFFER_SIZE - (write_idx + open_zone_cnt)) < 2;
}

__attribute__((always_inline)) inline void write_event(uint32_t type, uint32_t id16)
{
    uint32_t timestamp_high = ckernel::reg_read(RISCV_DEBUG_REG_WALL_CLOCK_H);
    uint32_t timestamp_low  = ckernel::reg_read(RISCV_DEBUG_REG_WALL_CLOCK_L);

    uint32_t type_numeric = static_cast<uint32_t>(type);
    uint32_t meta         = (type_numeric << ENTRY_TYPE_SHAMT) | (id16 << ENTRY_ID_SHAMT);

    buffer[trisc_id][write_idx++] = meta | (timestamp_high & ~ENTRY_META_MASK);
    buffer[trisc_id][write_idx++] = timestamp_low;
}

__attribute__((always_inline)) inline void write_data(uint64_t data)
{
    buffer[trisc_id][write_idx++] = data >> 32;
    buffer[trisc_id][write_idx++] = data & 0xFFFFFFFF;
}

template <uint32_t id16>
class zone_scoped
{
public:
    inline __attribute__((always_inline)) zone_scoped()
    {
        if (is_buffer_full())
        {
            return;
        }

        write_event(ZONE_START_ENTRY, id16);
        open_zone_cnt++;
    }

    ~zone_scoped()
    {
        write_event(ZONE_END_ENTRY, id16);
        open_zone_cnt--;
    }
};

#define ZONE_SCOPED(name)                    \
    DO_PRAGMA(message(ZONE_FULL_NAME(name))) \
    llk_profiler::zone_scoped<ZONE_ID(name)>();

#define TIMESTAMP(name)                      \
    DO_PRAGMA(message(ZONE_FULL_NAME(name))) \
    llk_profiler::write_event(TIMESTAMP_ENTRY, ZONE_ID(name));

#define TIMESTAMP_DATA(name, data)                                  \
    DO_PRAGMA(message(ZONE_FULL_NAME(name)))                        \
    llk_profiler::write_event(TIMESTAMP_DATA_ENTRY, ZONE_ID(name)); \
    llk_profiler::write_data(data);

} // namespace llk_profiler
