// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#define DEF_GCOV_COUNTER(COUNTER, NAME, MERGE_FN) COUNTER,

enum
{
#include "gcov-counter.def"
    GCOV_COUNTERS
};

#undef DEF_GCOV_COUNTER

typedef unsigned gcov_unsigned_t;
typedef int64_t gcov_type;
typedef unsigned gcov_position_t __attribute__((mode(SI)));
typedef void (*gcov_merge_fn)(gcov_type *, gcov_unsigned_t);

struct gcov_summary
{
    gcov_unsigned_t runs; /* Number of program runs.  */
    gcov_type sum_max;    /* Sum of individual run max values.  */
};

struct gcov_info
{
    gcov_unsigned_t version; /* expected version number */
    struct gcov_info *next;  /* link to next, used by libgcov */

    gcov_unsigned_t stamp;    /* uniquifying time stamp */
    gcov_unsigned_t checksum; /* unique object checksum */
    const char *filename;     /* output file name */

    gcov_merge_fn merge[GCOV_COUNTERS]; /* merge functions (null for
                                        unused) */

    gcov_unsigned_t n_functions; /* number of functions */
    struct gcov_fn_info **functions;
    struct gcov_summary summary;
};
