// SPDX-FileCopyrightText: (c) 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0
#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

    // This header provides the interface to libgcov's routines. Don't include
    // this directly, include coverage.h instead and call gcov_dump.

    void __gcov_info_to_gcda(const struct gcov_info *gi_ptr, void (*dump_fn)(const void *, unsigned), void *arg);

#ifdef __cplusplus
}
#endif
