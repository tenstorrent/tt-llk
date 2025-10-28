// SPDX-FileCopyrightText: (c) 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0
#ifdef __cplusplus
extern "C"
{
#endif

#include <stddef.h>
#include <stdint.h>

#include "gcov.h"

#define COVERAGE_OVERFLOW 0xDEADBEEF

    // Symbols pointing to per-TU coverage data from -fprofile-info-section.
    extern const struct gcov_info* __gcov_info_start[];
    extern const struct gcov_info* __gcov_info_end[];

    // Start address and region length of per-RISC REGION_GCOV.
    // This region stores the actual gcda, and the host reads it
    // and dumps it into a file.
    extern uint8_t __coverage_start[];
    extern uint8_t __coverage_end[];

    // The first value in the coverage segment is the number of bytes written.
    // Note, in gcov_dump, that it gets set to 4 - that is to accommodate for the
    // value itself. The covdump.py script uses it to know how much data to
    // extract.

    static void write_data(const void* _data, unsigned int length, [[maybe_unused]] void* arg)
    {
        uint8_t* data     = (uint8_t*)_data;
        uint32_t* written = (uint32_t*)__coverage_start;
        if (*written == COVERAGE_OVERFLOW)
        {
            return;
        }

        uint8_t* mem = __coverage_start + *written; // Start writing from here.
        if (mem + length >= __coverage_end)
        {
            // Not enough space in the segment, write overflow sentinel and return.
            *written = COVERAGE_OVERFLOW;
            return;
        }

        for (unsigned int i = 0; i < length; i++)
        {
            mem[i] = data[i];
        }
        *written += length;
    }

    static size_t strlen(const char* s)
    {
        size_t n;
        for (n = 0; s[n]; n++)
            ;
        return n;
    }

    // This callback is called once at the beginning of the data for each TU.
    static void filename(const char* fname, [[maybe_unused]] void* arg)
    {
        // We'll write the pointer to the filename as the second word in the
        // segment, and its length as the third. The script will then use the
        // filename to find the gcno.
        *(const char**)(__coverage_start + 4) = fname;
        *(uint32_t*)(__coverage_start + 8)    = strlen(fname);
    }

    void gcov_dump(void)
    {
        // First three words are for region metadata, start writing past that.
        *(uint32_t*)__coverage_start = 12;

        const struct gcov_info* const* info = __gcov_info_start;
        __asm__ volatile("" : "+r"(info)); // Prevent optimizations.
        __gcov_info_to_gcda(*info, filename, write_data, NULL, NULL);
    }

#ifdef __cplusplus
}
#endif
