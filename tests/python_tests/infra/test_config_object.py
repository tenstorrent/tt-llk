# SPDX-FileCopyrightText: © 2026 Tenstorrent AI ULC
#
# SPDX-License-Identifier: Apache-2.0


import pytest
from helpers.param_config import input_output_formats, parametrize
from helpers.test_config import TestConfig, TestMode
from perf_matmul import (
    DataFormat,
    DestAccumulation,
    MathFidelity,
    input_output_formats,
    matmul_combos,
    test_perf_matmul,
)


@pytest.mark.order(1)
@parametrize(
    combos=matmul_combos(
        formats=input_output_formats(
            [
                DataFormat.Float16_b,
                DataFormat.Float16,
                DataFormat.Float32,
                DataFormat.Bfp8_b,
            ]
        ),
        dest_acc=[DestAccumulation.No, DestAccumulation.Yes],
    ),
    math_fidelity=[
        MathFidelity.LoFi,
        MathFidelity.HiFi2,
        MathFidelity.HiFi3,
        MathFidelity.HiFi4,
    ],
)
def test_variant_hash_generation(combos, math_fidelity):
    TestConfig.MODE = TestMode.PRODUCE
    test_perf_matmul(None, combos, math_fidelity)


@pytest.mark.order(2)
@parametrize(
    combos=matmul_combos(
        formats=input_output_formats(
            [
                DataFormat.Float16_b,
                DataFormat.Float16,
                DataFormat.Float32,
                DataFormat.Bfp8_b,
            ]
        ),
        dest_acc=[DestAccumulation.No, DestAccumulation.Yes],
    ),
    math_fidelity=[
        MathFidelity.LoFi,
        MathFidelity.HiFi2,
        MathFidelity.HiFi3,
        MathFidelity.HiFi4,
    ],
)
def test_variant_hash_consumption(combos, math_fidelity):
    TestConfig.MODE = TestMode.CONSUME
    test_perf_matmul(None, combos, math_fidelity)
