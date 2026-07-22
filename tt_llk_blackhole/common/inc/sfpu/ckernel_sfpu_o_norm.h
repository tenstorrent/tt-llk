// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cstdint>

#include "ckernel_sfpu_exp.h"
#include "ckernel_sfpu_recip.h"
#include "ckernel_sfpu_sqrt.h"
#include "sfpi.h"

namespace ckernel
{
namespace sfpu
{

// ============================================================================
// Fused o_norm SFPU kernel  (Kimi-K3 / KDA "o_norm" node)
//
//     o_norm = RMSNorm(o) * gamma2 * sigmoid(g_out)
//
// where, for every head, RMSNorm reduces over the head-dim (D) elements:
//
//     rms(o)      = sqrt( (1/D) * sum_d o[d]^2 + eps )
//     o_norm[d]   = ( o[d] / rms(o) ) * gamma2[d] * sigmoid(g_out[d])
//
// ---------------------------------------------------------------------------
// Tilization contract (IMPORTANT)
// ---------------------------------------------------------------------------
// This kernel is built from *basic* SFPU (sfpi) primitives only - it performs
// no cross-lane SFPTRANSP shuffle. To keep the reduction free of transposes,
// the caller must lay the operands out so that the reduction dimension (the
// head-dim D that RMSNorm normalizes over) runs along the Dest *row/iteration*
// axis, while the independent outputs (the heads) occupy the SFPU lanes.
//
// Concretely, with `NUM_REDUCE_TILES` tiles stacked along the row axis:
//   - o, gamma2 and g_out each occupy `NUM_REDUCE_TILES` consecutive Dest
//     tiles (D = NUM_REDUCE_TILES * 32 reduction elements).
//   - gamma2[d] is broadcast across the lane (head) axis.
//   - The per-head sum of squares is accumulated per lane across the row axis,
//     so no lane-to-lane communication is required.
//
// The companion golden (`ONormGolden` in the Python test suite) reduces over
// the matching logical axis. The row/lane <-> logical-dimension mapping and the
// numeric tolerances must be validated on Blackhole hardware / simulator.
// ============================================================================

// Number of sfpi Dest rows per 32x32 tile (matches ckernel_sfpu_binary.h).
constexpr std::uint32_t O_NORM_DST_TILE_SIZE_SFPI = 32;

// sigmoid(x) = 1 / (1 + exp(-x)) built from basic SFPU primitives.
// One Newton-Raphson refinement is applied on top of the fast reciprocal
// approximation, which does not depend on any programmable const register
// (so it does not clash with the sqrt/rsqrt init below).
sfpi_inline sfpi::vFloat _o_norm_sigmoid_(sfpi::vFloat x)
{
    sfpi::vFloat denom = _sfpu_exp_(-x) + 1.0f;
    sfpi::vFloat y     = sfpi::approx_recip(denom);
    // One Newton-Raphson step: y <- y * (2 - denom * y).
    // Written as y * -(denom*y - 2) to avoid a (float - vFloat) expression.
    sfpi::vFloat t = denom * y - 2.0f;
    y              = y * (-t);
    return y;
}

/**
 * @brief Fused o_norm = RMSNorm(o) * gamma2 * sigmoid(g_out).
 *
 * Reads three operands from Dest (o, gamma2, g_out), each spanning
 * NUM_REDUCE_TILES tiles along the reduction (row) axis, and writes the result
 * back to the output tiles. Computed in two passes: (1) accumulate the per-lane
 * sum of squares of o, (2) normalize and apply the gamma2 / sigmoid(g_out)
 * scaling.
 *
 * @tparam APPROXIMATION_MODE Use the low-precision sqrt/rsqrt approximation.
 * @tparam fp32_dest_acc_en   Whether Dest is in FP32 accumulation mode.
 * @tparam NUM_REDUCE_TILES   Number of tiles spanning the head-dim (D = 32 * this).
 * @param dst_o       Base Dest tile index of the o operand.
 * @param dst_gamma   Base Dest tile index of the gamma2 operand.
 * @param dst_gout    Base Dest tile index of the g_out operand.
 * @param dst_out     Base Dest tile index of the output (may alias dst_o).
 * @param eps         RMSNorm epsilon for numerical stability.
 */
template <bool APPROXIMATION_MODE, bool fp32_dest_acc_en, int NUM_REDUCE_TILES>
inline void _calculate_o_norm_(
    const std::uint32_t dst_o, const std::uint32_t dst_gamma, const std::uint32_t dst_gout, const std::uint32_t dst_out, const float eps)
{
    constexpr std::uint32_t TILE     = O_NORM_DST_TILE_SIZE_SFPI;
    constexpr int N_ROWS             = NUM_REDUCE_TILES * static_cast<int>(TILE);
    constexpr float inv_reduce_count = 1.0f / static_cast<float>(N_ROWS);

    const std::uint32_t o_base     = dst_o * TILE;
    const std::uint32_t gamma_base = dst_gamma * TILE;
    const std::uint32_t gout_base  = dst_gout * TILE;
    const std::uint32_t out_base   = dst_out * TILE;

    // Pass 1: per-lane (per-head) sum of squares over the reduction axis.
    sfpi::vFloat acc = 0.0f;
    for (int r = 0; r < N_ROWS; r++)
    {
        sfpi::vFloat x = sfpi::dst_reg[o_base + r];
        acc            = acc + x * x;
    }

    // rms^-1 = rsqrt( mean(o^2) + eps ), computed once per lane and shared by
    // every reduction element.
    sfpi::vFloat mean_sq = acc * inv_reduce_count + eps;
    sfpi::vFloat inv_rms = _calculate_sqrt_body_<APPROXIMATION_MODE, /*RECIPROCAL=*/true, /*FAST_APPROX=*/false>(mean_sq);

    // Pass 2: normalize and apply gamma2 * sigmoid(g_out) scaling.
    for (int r = 0; r < N_ROWS; r++)
    {
        sfpi::vFloat x     = sfpi::dst_reg[o_base + r];
        sfpi::vFloat gamma = sfpi::dst_reg[gamma_base + r];
        sfpi::vFloat gout  = sfpi::dst_reg[gout_base + r];

        sfpi::vFloat result = x * inv_rms * gamma * _o_norm_sigmoid_(gout);

        if constexpr (fp32_dest_acc_en)
        {
            sfpi::dst_reg[out_base + r] = result;
        }
        else
        {
            sfpi::dst_reg[out_base + r] = sfpi::reinterpret<sfpi::vFloat>(float_to_fp16b(result, 0));
        }
    }
}

template <bool APPROXIMATION_MODE>
inline void _init_o_norm_()
{
    // Program the const registers used by the sqrt/rsqrt approximation.
    // The sigmoid path relies on _sfpu_exp_ + approx_recip, neither of which
    // needs initialization, so no additional setup is required here.
    _init_sqrt_<APPROXIMATION_MODE>();
}

} // namespace sfpu
} // namespace ckernel
