// SPDX-FileCopyrightText: © 2026 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cstdint>

#include "llk_defs.h"

namespace ckernel {

constexpr bool should_skip_hifi2_for_bf16_bfp4_matmul(
    const MathFidelity math_fidelity, const std::uint32_t in0_dst_format, const std::uint32_t in1_dst_format) {
    return math_fidelity == MathFidelity::HiFi3 &&
           in0_dst_format == static_cast<std::uint32_t>(DataFormat::Float16_b) &&
           (in1_dst_format == static_cast<std::uint32_t>(DataFormat::Bfp4) ||
            in1_dst_format == static_cast<std::uint32_t>(DataFormat::Bfp4_b));
}

constexpr bool should_skip_hifi2_for_bf16_bfp4_matmul(
    const MathFidelity math_fidelity, const DataFormat in0_dst_format, const DataFormat in1_dst_format) {
    return should_skip_hifi2_for_bf16_bfp4_matmul(
        math_fidelity, static_cast<std::uint32_t>(in0_dst_format), static_cast<std::uint32_t>(in1_dst_format));
}

constexpr std::uint32_t get_matmul_fidelity_phase_count(
    const MathFidelity math_fidelity, const std::uint32_t in0_dst_format, const std::uint32_t in1_dst_format) {
    if (math_fidelity == MathFidelity::LoFi) {
        return 1;
    }

    return should_skip_hifi2_for_bf16_bfp4_matmul(math_fidelity, in0_dst_format, in1_dst_format)
               ? 2
               : static_cast<std::uint32_t>(math_fidelity);
}

constexpr std::uint32_t get_matmul_fidelity_increment(
    const MathFidelity math_fidelity, const std::uint32_t in0_dst_format, const std::uint32_t in1_dst_format) {
    if (math_fidelity == MathFidelity::LoFi) {
        return 0;
    }

    return should_skip_hifi2_for_bf16_bfp4_matmul(math_fidelity, in0_dst_format, in1_dst_format) ? 2 : 1;
}

static_assert(
    should_skip_hifi2_for_bf16_bfp4_matmul(MathFidelity::HiFi3, DataFormat::Float16_b, DataFormat::Bfp4_b));
static_assert(
    get_matmul_fidelity_phase_count(
        MathFidelity::HiFi3, static_cast<std::uint32_t>(DataFormat::Float16_b), static_cast<std::uint32_t>(DataFormat::Bfp4_b)) ==
    2);
static_assert(
    get_matmul_fidelity_increment(
        MathFidelity::HiFi3, static_cast<std::uint32_t>(DataFormat::Float16_b), static_cast<std::uint32_t>(DataFormat::Bfp4_b)) ==
    2);
static_assert(
    get_matmul_fidelity_phase_count(
        MathFidelity::HiFi3, static_cast<std::uint32_t>(DataFormat::Float16_b), static_cast<std::uint32_t>(DataFormat::Float16_b)) ==
    3);
static_assert(
    get_matmul_fidelity_increment(
        MathFidelity::HiFi3, static_cast<std::uint32_t>(DataFormat::Float16_b), static_cast<std::uint32_t>(DataFormat::Float16_b)) ==
    1);

}  // namespace ckernel
