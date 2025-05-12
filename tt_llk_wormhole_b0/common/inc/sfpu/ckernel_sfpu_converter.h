// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

namespace ckernel::sfpu
{

union Converter
{
    float f;
    uint32_t u;

    static float to_float(uint32_t _v)
    {
        Converter c {};
        c.u = _v;
        return c.f;
    }
};

} // namespace ckernel::sfpu
