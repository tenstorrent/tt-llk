// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0
#include <cstddef>
#include <cstdint>

// FIXME: add to build.h
#define LLK_LOG_ENABLE 1

#ifndef LLK_LOG_ENABLE

#define LOG(fmt, ...)

#else

#define CONCAT_IMPL(a, b) a##b
#define CONCAT(a, b)      CONCAT_IMPL(a, b)
#define STRINGIZE(x)      #x

#define LOG_IMPL(symbol, fmt, ...)                                    \
    do                                                                \
    {                                                                 \
        asm volatile(".pushsection .log_meta, \"\", @progbits\n\t"  \
                     ".type " STRINGIZE(symbol) ", @object\n\t"      \
                     ".local " STRINGIZE(symbol) "\n\t"              \
                     STRINGIZE(symbol) ":\n\t"                       \
                     ".string \"" fmt "\"\n\t"                       \
                     ".size " STRINGIZE(symbol) ", .-" STRINGIZE(symbol) "\n\t" \
                     ".popsection"); \
        extern const char symbol[];                                   \
        llk_log::verify(fmt, ##__VA_ARGS__);                          \
        llk_log::write(symbol, ##__VA_ARGS__);                        \
    } while (false)

#define LOG(fmt, ...) LOG_IMPL(CONCAT(_log_meta_, __COUNTER__), fmt, ##__VA_ARGS__)

namespace llk_log
{

template <size_t N, typename... Ts>
static constexpr void verify(const char (&fmt)[N], Ts... args)
{
    // FIXME: Format string should be validated, because if it is not valid,
    // the host might not be able to parse the log messages going forward
}

static inline volatile uint32_t* allocate(size_t bytes)
{
    // TODO: Implement a way to reserve bytes in the buffer

    // THEORY:
    //  use TTI_ATINCGET to allocate on WH
    //  use AMOADD to reserve on BH an QSR

    asm volatile("fence r, rw" ::: "memory"); // Acquire barrier (compiler hint on WH)

    return (volatile uint32_t*)0x16A000;
}

template <typename... Ts>
static constexpr size_t get_size(const void* fmt, Ts... args)
{
    size_t size = sizeof(fmt);
    size += (sizeof(args) + ...); // sums sizeof each argument
    return size;
}

static inline void push(volatile uint32_t*& buffer, const void* ptr)
{
    uintptr_t addr = reinterpret_cast<uintptr_t>(ptr);
    *buffer++      = static_cast<uint32_t>(addr);
}

static inline void push(volatile uint32_t*& buffer, const uint32_t arg)
{
    *buffer++ = arg;
}

template <typename... Ts>
inline void write(const void* fmt, const Ts... args)
{
    const size_t size         = get_size(fmt, args...);
    volatile uint32_t* buffer = allocate(size);

    push(buffer, fmt);
    (push(buffer, args), ...);
}
} // namespace llk_log

#endif
