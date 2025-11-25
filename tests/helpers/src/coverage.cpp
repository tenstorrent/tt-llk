// SPDX-FileCopyrightText: (c) 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0
#include <cstdint>
#ifdef __cplusplus
extern "C"
{
#endif

#include <cstddef>
#include <cstdint>

#include "gcov.h"
#include "gcov_structs.h"

#define COVERAGE_OVERFLOW 0xDEADBEEF

    // Symbols pointing to per-TU coverage data from -fprofile-info-section.
    extern const struct gcov_info* __gcov_info_start[];
    extern const struct gcov_info* __gcov_info_end[];

    // Start address and region length of per-RISC REGION_GCOV.
    // This region stores the actual gcda, and the host reads it
    // and dumps it into a file.
    extern uint8_t __coverage_start[];
    extern uint8_t __coverage_end[];

    void __gcov_merge_add(gcov_type*, unsigned)
    {
    }

    int gcov_error(const char*, ...)
    {
        return 0;
    }

    void abort()
    {
        while (1)
            ;
    }

    uint32_t __umodsi3(uint32_t a, uint32_t b)
    {
        return a % b;
    }

    uint32_t __udivsi3(uint32_t a, uint32_t b)
    {
        return a / b;
    }

    static void write_data(const void* _data, unsigned int length, void*)
    {
        uint8_t* data     = (uint8_t*)_data;
        uint32_t* written = (uint32_t*)__coverage_start;

        uint8_t* mem = __coverage_start + *written;

        for (unsigned int i = 0; i < length; i++)
        {
            mem[i] = data[i];
        }

        *written += length;
    }

    size_t strlen(const char* s)
    {
        size_t n = 0;
        for (; s[n] != '\0'; n++)
            ;
        return n;
    }

    void filename_dummy(const char*, void*)
    {
    }

    // The first value in the coverage segment is the number of bytes written to it.
    void gcov_dump(void)
    {
        // First word is for file length, start writing past that.
        *(uint32_t*)__coverage_start = 4;

        const struct gcov_info* const* info = __gcov_info_start;
        __asm__ volatile("" : "+r"(info)); // Prevent optimizations.
        uint16_t compilation_units = (__gcov_info_end - __gcov_info_start);

        for (uint16_t ind = 0; ind < compilation_units; ind++)
        {
            __gcov_filename_to_gcfn(info[ind]->filename, write_data, NULL);
            __gcov_info_to_gcda(info[ind], filename_dummy, write_data, NULL, NULL);
        }
    }

#ifdef __cplusplus
}
#endif
