// SPDX-FileCopyrightText: © 2026 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#include <cstdint>
#include <numeric>

#include "ckernel.h"
#include "stream.h"

// Globals required by LLK infrastructure
std::uint32_t unp_cfg_context          = 0;
std::uint32_t pack_sync_tile_dst_ptr   = 0;
std::uint32_t math_sync_tile_dst_index = 0;

constexpr std::uintptr_t STREAM_ADDRESS = 0x70000; // @ajankovic magic number
constexpr std::uint32_t STREAM_DEPTH    = 8;

using stream_type = llk::stream_t<STREAM_DEPTH>;

constexpr std::size_t PACKET_COUNT                 = 13;
constexpr std::uint32_t PACKET_SIZES[PACKET_COUNT] = {1, 3, 7, 16, 32, 64, 100, 128, 200, 300, 50, 25, 13};
constexpr std::size_t STREAM_SIZE                  = 939;

// PRNG seeds - separate for data and delays to simplify debugging
constexpr std::uint32_t DATA_SEED           = 0xDEADBEEF;
constexpr std::uint32_t PRODUCER_DELAY_SEED = 0xCAFEBABE;
constexpr std::uint32_t CONSUMER_DELAY_SEED = 0xFEEDFACE;

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

// Insert random number of nops (0 to 100) to stress test timing
inline void delay(prng_t& rng)
{
    std::uint32_t nops = rng.next() % 101;
    for (std::uint32_t i = 0; i < nops; ++i)
    {
        asm volatile("nop");
    }
}

#ifdef LLK_TRISC_UNPACK

void run_kernel(const volatile struct RuntimeParams* params)
{
    (void)params;

    stream_type& stream = *reinterpret_cast<stream_type*>(STREAM_ADDRESS);

    // Local buffer for generating test data
    char send[512];

    // Initialize RNG with fixed seed (must match consumer)
    prng_t rand(DATA_SEED);
    // Separate RNG for delays (different seed, doesn't affect data)
    prng_t delay_rand(PRODUCER_DELAY_SEED);

    for (std::size_t packet_idx = 0; packet_idx < PACKET_COUNT; ++packet_idx)
    {
        const std::uint32_t packet_size = PACKET_SIZES[packet_idx];

        // Fill send buffer with deterministic pattern
        for (std::uint32_t i = 0; i < packet_size; ++i)
        {
            send[i] = rand.byte();
        }

        // Push the packet
        stream.push(send, packet_size);

        // Random delay between packets (0-100 nops)
        delay(delay_rand);
    }
}

#endif

#ifdef LLK_TRISC_MATH

#include "llk_assert.h"

// Consumer thread - pops packets of various sizes and verifies
void run_kernel(const volatile struct RuntimeParams* params)
{
    (void)params;

    stream_type& stream = *reinterpret_cast<stream_type*>(STREAM_ADDRESS);

    // Local buffer for receiving data
    char recv[512];

    // Initialize RNG with same seed as producer
    prng_t rand(DATA_SEED);
    // Separate RNG for delays (different seed from producer for variety)
    prng_t delay_rand(CONSUMER_DELAY_SEED);

    std::uint32_t errors = 0;

    for (std::size_t packet_idx = 0; packet_idx < PACKET_COUNT; ++packet_idx)
    {
        // Random delay before pop (0-100 nops)
        delay(delay_rand);

        const std::uint32_t packet_size = PACKET_SIZES[packet_idx];

        // Pop the packet
        stream.pop(recv, packet_size);

        // Verify the data
        for (std::uint32_t i = 0; i < packet_size; ++i)
        {
            const char expected = rand.byte();
            if (recv[i] != expected)
            {
                ++errors;
            }
        }
    }

    // Assert no errors occurred
    LLK_ASSERT(errors == 0, "Stream data verification failed");
}

#endif

#ifdef LLK_TRISC_PACK

// Pack thread - not used in this test, just placeholder
void run_kernel(const volatile struct RuntimeParams* params)
{
    (void)params;
    // Nothing to do
}

#endif
