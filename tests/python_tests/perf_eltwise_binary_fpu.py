# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0

import pytest
from helpers.constraints import (
    get_valid_math_fidelities,
)
from helpers.format_config import DataFormat
from helpers.llk_params import (
    DestAccumulation,
    MathFidelity,
    MathOperation,
    PerfRunType,
)
from helpers.param_config import input_output_formats, parametrize
from helpers.profiler import ProfilerConfig
from helpers.stimuli_config import StimuliConfig
from helpers.test_variant_parameters import (
    MATH_FIDELITY,
    MATH_OP,
    TILE_COUNT,
)


@pytest.mark.perf
@parametrize(
    formats=input_output_formats([DataFormat.Float16_b]),
    mathop=[MathOperation.Elwsub],
    tile_count=16,
    math_fidelity=lambda formats, mathop: get_valid_math_fidelities(
        formats, mathop, PERF_RUN=True
    ),
    dest_acc=[DestAccumulation.No],
)
def test_perf_eltwise_binary_fpu(
    perf_report,
    formats,
    mathop,
    tile_count,
    math_fidelity,
    dest_acc,
    workers_tensix_coordinates,
):
    if mathop != MathOperation.Elwmul and math_fidelity != MathFidelity.LoFi:
        pytest.skip("Fidelity does not affect Elwadd and Elwsub operations")

    configuration = ProfilerConfig(
        "sources/eltwise_binary_fpu_perf.cpp",
        formats,
        run_types=[
            PerfRunType.L1_TO_L1,
            PerfRunType.UNPACK_ISOLATE,
            PerfRunType.MATH_ISOLATE,
            PerfRunType.PACK_ISOLATE,
            PerfRunType.L1_CONGESTION,
        ],
        templates=[MATH_FIDELITY(math_fidelity), MATH_OP(mathop=mathop)],
        runtimes=[TILE_COUNT(tile_count)],
        variant_stimuli=StimuliConfig(
            None,
            formats.input_format,
            None,
            formats.input_format,
            formats.output_format,
            tile_count_A=tile_count,
            tile_count_B=tile_count,
            tile_count_res=tile_count,
        ),
        dest_acc=dest_acc,
    )

    configuration.run(perf_report, location=workers_tensix_coordinates)
