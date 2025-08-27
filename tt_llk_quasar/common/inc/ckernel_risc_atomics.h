// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0
#include <atomic>
#include <climits>
#include <cstdint>

inline int32_t amomin(int32_t volatile *ptr, int32_t const against)
{
    int32_t old_val;
    asm volatile("amomin.w %[ret], %[against_val], (%[addr_val])\n" : [ret] "=r"(old_val) : [against_val] "r"(against), [addr_val] "r"(ptr));

    return old_val;
}

inline uint32_t amominu(uint32_t volatile *ptr, uint32_t const against)
{
    uint32_t old_val;
    asm volatile("amominu.w %[ret], %[against_val], (%[addr_val])\n" : [ret] "=r"(old_val) : [against_val] "r"(against), [addr_val] "r"(ptr));

    return old_val;
}

inline int32_t amomax(int32_t volatile *ptr, int32_t const against)
{
    int32_t old_val;
    asm volatile("amomax.w %[ret], %[against_val], (%[addr_val])\n" : [ret] "=r"(old_val) : [against_val] "r"(against), [addr_val] "r"(ptr));

    return old_val;
}

inline uint32_t amomaxu(uint32_t volatile *ptr, uint32_t const against)
{
    uint32_t old_val;
    asm volatile("amomaxu.w %[ret], %[against_val], (%[addr_val])\n" : [ret] "=r"(old_val) : [against_val] "r"(against), [addr_val] "r"(ptr));

    return old_val;
}

template <typename T>
inline std::atomic<T> *mk_atomic_ptr(T *ptr)
{
    // C++ is very persnickety... only way to get around a compile error
    // was to invite satan into our living room and use reinterpret_cast
    std::atomic<T> *p_ato = reinterpret_cast<std::atomic<uint32_t> *>(ptr);
    return p_ato;
}

template <typename T>
inline T load_acquire(T *ptr)
{
    static_assert(sizeof(T) * CHAR_BIT == 32, "load_acquire: operand must be 32bit");

    uint32_t ret;
    asm volatile("amoadd.w.aq %[ret], zero, (%[ptr])\n" : [ret] "=r"(ret) : [ptr] "r"(ptr));

    return *reinterpret_cast<T *>(&ret);
}

// Returns original value from before store
template <typename T>
inline T store_release(T *ptr, T val)
{
    static_assert(sizeof(T) * CHAR_BIT == 32, "store_release: operand must be 32bit");

    uint32_t *val_ptr = reinterpret_cast<uint32_t *>(&val);
    uint32_t ret;

    asm volatile("amoswap.w.rl %[ret], %[val], (%[ptr])\n" : [ret] "=r"(ret) : [val] "r"(*val_ptr), [ptr] "r"(ptr));

    return *reinterpret_cast<T *>(ret);
}

#if 0
//I started working on this to amuse myself while my tests were running.
//None of this is tested
typedef std::atomic<int32_t>* semaphore;

semaphore create_semaphore_at(int32_t *l1_addr) {
    semaphore ret = reinterpret_cast<std::atomic<int32_t> *>(l1_addr);
    store_release(ret, int32_t(0));
    return ret;
}

inline void semaphore_inc(semaphore s) {
    std::atomic_fetch_add(s, int32_t(1));
}

inline void semaphore_dec(semaphore s) {
    std::atomic_fetch_add(s, int32_t(-1));
}

void spinlock_on_nonzero_semaphore(semaphore s) {
    while (load_acquire(s) != 0);
}

struct mutex {
    enum {
        AVAIL = 0,
        LOCKED
    };

    uint32_t val = AVAIL;

    std::atomic<uint32_t>* operator&() {
        return reinterpret_cast<std::atomic<int32_t> *>(&val);
    }
};

#endif
