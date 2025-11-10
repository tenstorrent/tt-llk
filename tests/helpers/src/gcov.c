// SPDX-FileCopyrightText: (c) 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0
// All code in this file is adapted from gcc/libgcc/libgcov-driver.c

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

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

#define MAX(X, Y)                    ((X) > (Y) ? (X) : (Y))
#define GCOV_TAG_FOR_COUNTER(COUNT)  (GCOV_TAG_COUNTER_BASE + ((gcov_unsigned_t)(COUNT) << 17))
#define GCOV_TAG_COUNTER_LENGTH(NUM) ((NUM) * 2 * GCOV_WORD_SIZE)

#define GCOV_WORD_SIZE           4
#define GCOV_TAG_FUNCTION_LENGTH (3 * GCOV_WORD_SIZE)
#define GCOV_VERSION             0x4235312A // for GCC 15.1.0
#define GCOV_TAG_FUNCTION        ((gcov_unsigned_t)0x01000000)
#define GCOV_TAG_COUNTER_BASE    ((gcov_unsigned_t)0x01a10000)
#define GCOV_DATA_MAGIC          ((gcov_unsigned_t)0x67636461)
#define GCOV_FILENAME_MAGIC      ((gcov_unsigned_t)0x6763666e)

    void __gcov_merge_add(gcov_type *counters, unsigned n_counters)
    {
    }

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

    struct gcov_ctr_info
    {
        gcov_unsigned_t num; /* number of counters.  */
        gcov_type *values;   /* their values.  */
    };

    struct gcov_fn_info
    {
        const struct gcov_info *key;     /* comdat key */
        gcov_unsigned_t ident;           /* unique ident of function */
        gcov_unsigned_t lineno_checksum; /* function lineo_checksum */
        gcov_unsigned_t cfg_checksum;    /* function cfg checksum */
        struct gcov_ctr_info ctrs[1];    /* instrumented counters */
    };

    static inline int are_all_counters_zero(const struct gcov_ctr_info *ci_ptr)
    {
        for (unsigned i = 0; i < ci_ptr->num; i++)
        {
            if (ci_ptr->values[i] != 0)
            {
                return 0;
            }
        }

        return 1;
    }

    static inline void dump_unsigned(gcov_unsigned_t word, void (*dump_fn)(const void *, unsigned, void *), void *arg)
    {
        (*dump_fn)(&word, sizeof(word), arg);
    }

    static inline void dump_counter(gcov_type counter, void (*dump_fn)(const void *, unsigned, void *), void *arg)
    {
        dump_unsigned((gcov_unsigned_t)counter, dump_fn, arg);

        if (sizeof(counter) > sizeof(gcov_unsigned_t))
        {
            dump_unsigned((gcov_unsigned_t)(counter >> 32), dump_fn, arg);
        }
        else
        {
            dump_unsigned(0, dump_fn, arg);
        }
    }

    static size_t strlen(const char *s)
    {
        size_t n;
        for (n = 0; s[n]; n++)
            ;
        return n;
    }

    static void write_one_filename(const char *filename, void (*dump_fn)(const void *, unsigned, void *), void *arg)
    {
        dump_unsigned(GCOV_FILENAME_MAGIC, dump_fn, arg);
        dump_unsigned(GCOV_VERSION, dump_fn, arg);

        int filename_length = strlen(filename) + 1;
        dump_unsigned(filename_length, dump_fn, arg);
        dump_fn(filename, filename_length, arg);
    }

    static void write_one_data(const struct gcov_info *gi_ptr, void (*dump_fn)(const void *, unsigned, void *), void *arg)
    {
        unsigned f_ix;

        dump_unsigned(GCOV_DATA_MAGIC, dump_fn, arg);
        dump_unsigned(GCOV_VERSION, dump_fn, arg);
        dump_unsigned(gi_ptr->stamp, dump_fn, arg);
        dump_unsigned(gi_ptr->checksum, dump_fn, arg);

        /* Write execution counts for each function.  */
        for (f_ix = 0; f_ix != gi_ptr->n_functions; f_ix++)
        {
            const struct gcov_fn_info *gfi_ptr;
            const struct gcov_ctr_info *ci_ptr;
            gcov_unsigned_t length;
            unsigned t_ix;
            gfi_ptr = gi_ptr->functions[f_ix];
            if (gfi_ptr && gfi_ptr->key == gi_ptr)
            {
                length = GCOV_TAG_FUNCTION_LENGTH;
            }
            else
            {
                length = 0;
            }

            dump_unsigned(GCOV_TAG_FUNCTION, dump_fn, arg);
            dump_unsigned(length, dump_fn, arg);
            if (!length)
            {
                continue;
            }

            dump_unsigned(gfi_ptr->ident, dump_fn, arg);
            dump_unsigned(gfi_ptr->lineno_checksum, dump_fn, arg);
            dump_unsigned(gfi_ptr->cfg_checksum, dump_fn, arg);

            ci_ptr = gfi_ptr->ctrs;
            for (t_ix = 0; t_ix < GCOV_COUNTERS; t_ix++)
            {
                gcov_position_t n_counts;

                if (!gi_ptr->merge[t_ix])
                {
                    continue;
                }
                n_counts = ci_ptr->num;

                if (!(t_ix == GCOV_COUNTER_V_TOPN || t_ix == GCOV_COUNTER_V_INDIR))
                {
                    dump_unsigned(GCOV_TAG_FOR_COUNTER(t_ix), dump_fn, arg);
                    if (are_all_counters_zero(ci_ptr)) /* Do not stream when all counters are zero.  */
                    {
                        dump_unsigned(GCOV_TAG_COUNTER_LENGTH(-n_counts), dump_fn, arg);
                    }
                    else
                    {
                        dump_unsigned(GCOV_TAG_COUNTER_LENGTH(n_counts), dump_fn, arg);
                        for (unsigned i = 0; i < n_counts; i++)
                        {
                            dump_counter(ci_ptr->values[i], dump_fn, arg);
                        }
                    }
                }

                ci_ptr++;
            }
        }

        dump_unsigned(0, dump_fn, arg);
    }

    void __gcov_info_to_gcda(const struct gcov_info *gi_ptr, void (*dump_fn)(const void *, unsigned, void *), void *arg)
    {
        write_one_filename(gi_ptr->filename, dump_fn, arg);
        write_one_data(gi_ptr, dump_fn, arg);
    }

#ifdef __cplusplus
}
#endif
