// SPDX-FileCopyrightText: © 2026 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#include <cstdint>

#include "ckernel.h"
#include "stream.h"

// Globals required by LLK infrastructure
std::uint32_t unp_cfg_context          = 0;
std::uint32_t pack_sync_tile_dst_ptr   = 0;
std::uint32_t math_sync_tile_dst_index = 0;

constexpr std::uintptr_t STREAM_ADDRESS = 0x70000;
constexpr std::uint32_t STREAM_DEPTH    = 8;

using stream_type = llk::stream_t<STREAM_DEPTH>;

constexpr std::size_t PACKET_COUNT                 = 13;
constexpr std::uint32_t PACKET_SIZES[PACKET_COUNT] = {1, 3, 7, 16, 32, 64, 100, 128, 200, 300, 50, 25, 13};

constexpr std::uint32_t DATA_SEED = 0xDEADBEEF;

struct prng_t
{
    std::uint32_t state;

    constexpr prng_t(std::uint32_t seed = 0xDEADBEEF) : state(seed ? seed : 1)
    {
    }

    constexpr std::uint32_t next()
    {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        return state;
    }

    constexpr char byte()
    {
        return static_cast<char>(next() & 0xFF);
    }
};

#ifdef LLK_TRISC_UNPACK

void run_kernel(const volatile struct RuntimeParams* params)
{
    (void)params;
    // Not used — host is the consumer
}

#endif

#ifdef LLK_TRISC_MATH

#include "llk_assert.h"

// Producer — pushes deterministic packets for the host to pop
void run_kernel(const volatile struct RuntimeParams* params)
{
    (void)params;

    // write_idx must be zero before the first push (host zeroes stream memory)
    auto write_idx = *reinterpret_cast<const volatile std::uint32_t*>(STREAM_ADDRESS);
    LLK_ASSERT(write_idx == 0, "write_idx not initialized to zero");

    stream_type& stream = *reinterpret_cast<stream_type*>(STREAM_ADDRESS);

    char send[512];

    prng_t rand(DATA_SEED);

    for (std::size_t packet_idx = 0; packet_idx < PACKET_COUNT; ++packet_idx)
    {
        const std::uint32_t packet_size = PACKET_SIZES[packet_idx];

        for (std::uint32_t i = 0; i < packet_size; ++i)
        {
            send[i] = rand.byte();
        }

        stream.push(send, packet_size);
    }
}

#endif

#ifdef LLK_TRISC_PACK

void run_kernel(const volatile struct RuntimeParams* params)
{
    (void)params;
    // Not used
}

#endif
