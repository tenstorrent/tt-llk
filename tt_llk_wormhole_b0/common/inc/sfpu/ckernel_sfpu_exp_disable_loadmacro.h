// SPDX-FileCopyrightText: © 2026 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

namespace ckernel::sfpu
{

// Non-LOADMACRO _calculate_exponential_: falls back to the scalar piecewise implementation
// for all template parameter combinations. The FAST_APPROX pipelined paths (replay-based)
// are not available without LOADMACRO.
template <bool APPROXIMATION_MODE, bool SCALE_EN, int ITERATIONS, bool FAST_APPROX, bool SKIP_POSITIVE_CHECK, bool CLAMP_NEGATIVE = true>
void _calculate_exponential_(const std::uint16_t exp_base_scale_factor)
{
    for (int d = 0; d < ITERATIONS; d++)
    {
        sfpi::vFloat val    = sfpi::dst_reg[0];
        sfpi::vFloat result = _calculate_exponential_piecewise_<APPROXIMATION_MODE, SCALE_EN, SKIP_POSITIVE_CHECK>(val, exp_base_scale_factor);
        sfpi::dst_reg[0]    = result;
        sfpi::dst_reg++;
    }
}

// Non-LOADMACRO _init_exponential_:
// - FAST_APPROX paths: no init needed (piecewise calculate is self-contained).
// - APPROXIMATION_MODE=true, FAST_APPROX=false: set constants used by piecewise (preserved from original).
// - APPROXIMATION_MODE=false: call _init_sfpu_reciprocal_ (preserved from original).
template <bool APPROXIMATION_MODE, bool FAST_APPROX, std::uint32_t scale, bool CLAMP_NEGATIVE = true>
inline void _init_exponential_()
{
    if constexpr (FAST_APPROX && APPROXIMATION_MODE)
    {
        // All FAST_APPROX paths used LOADMACRO or recorded replay buffers containing
        // SFPLOADMACRO. The piecewise fallback doesn't require any of this setup.
    }
    else if constexpr (APPROXIMATION_MODE)
    {
        // Non-LOADMACRO path: set piecewise constants (preserved from original).
        sfpi::vConstFloatPrgm0 = 1.442695f;
        sfpi::vConstFloatPrgm1 = sfpi::s2vFloat16b(p_exp::C23_73);
        sfpi::vConstFloatPrgm2 = sfpi::s2vFloat16b(p_exp::ADJ_EXP);
    }
    else
    {
        // Non-LOADMACRO path: initialise for _sfpu_reciprocal_<2> (preserved from original).
        _init_sfpu_reciprocal_<false>();
    }
}

} // namespace ckernel::sfpu
