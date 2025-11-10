// SPDX-FileCopyrightText: (c) 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0
#include <cstdint>
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

    static void write_data(const void* _data, unsigned int length)
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

    void gcov_dump(void)
    {
        // First word is for file length, start writing past that.
        *(uint32_t*)__coverage_start = 4;

        const struct gcov_info* const* info = __gcov_info_start;
        __asm__ volatile("" : "+r"(info)); // Prevent optimizations.
        uint16_t compilation_units = (__gcov_info_end - __gcov_info_start);

        for (uint16_t ind = 0; ind < compilation_units; ind++)
        {
            __gcov_info_to_gcda(info[ind], write_data, NULL);
        }
    }

#ifdef __cplusplus
}
#endif
