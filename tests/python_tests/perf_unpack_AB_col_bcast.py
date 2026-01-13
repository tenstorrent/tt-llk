# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0

import pytest
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
    tile_count=1,
    math_fidelity=[MathFidelity.LoFi],
    dest_acc=[DestAccumulation.No],
)
def test_perf_unpack_AB_col_bcast(
    perf_report,
    formats,
    mathop,
    tile_count,
    math_fidelity,
    dest_acc,
    workers_tensix_coordinates,
):
    """
    Performance test for unpackAB with column broadcast on srcB.

    Measures the full L1_TO_L1 pipeline with column broadcast on srcB.
    The TILE_LOOP zone captures the complete operation from unpack to pack.
    """

    configuration = ProfilerConfig(
        "sources/unpack_AB_col_bcast_perf.cpp",
        formats,
        run_types=[
            PerfRunType.L1_TO_L1,  # Full operation to get the unpack zone measurement
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
