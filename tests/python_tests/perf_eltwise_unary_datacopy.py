# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0

import pytest
from helpers.format_config import DataFormat
from helpers.llk_params import (
    DestAccumulation,
    PerfRunType,
)
from helpers.param_config import (
    input_output_formats,
    parametrize,
)
from helpers.perf import PerfConfig
from helpers.stimuli_config import StimuliConfig
from helpers.test_variant_parameters import (
    LOOP_FACTOR,
    TILE_COUNT,
)


@pytest.mark.perf
@parametrize(
    formats=input_output_formats(
        [
            DataFormat.Float16_b,
        ]
    ),
    dest_acc=[DestAccumulation.No],
)
def test_perf_eltwise_unary_datacopy(
    perf_report,
    formats,
    dest_acc,
    workers_tensix_coordinates,
):
    """
    Performance test for eltwise unary datacopy operation.

    This tests the simple unpack -> datacopy (A2D) -> pack path:
    - Unpack to srcA
    - Copy from srcA to dest
    - Pack from dest to L1

    Parameters:
    - 8 tiles input/output
    - No dest accumulation (16-bit mode)
    - No unpack to dest
    - Float16_b format only
    """

    tile_count = 4

    configuration = PerfConfig(
        "sources/eltwise_unary_datacopy_perf.cpp",
        formats,
        run_types=[
            # PerfRunType.L1_TO_L1,
            # PerfRunType.UNPACK_ISOLATE,  # TODO: Fix UNPACK_ISOLATE mode - currently hangs
            PerfRunType.MATH_ISOLATE,
            PerfRunType.PACK_ISOLATE,
            # PerfRunType.L1_CONGESTION,
        ],
        templates=[],
        runtimes=[TILE_COUNT(tile_count), LOOP_FACTOR(16)],
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
        unpack_to_dest=False,
        dest_acc=dest_acc,
    )

    configuration.run(perf_report, location=workers_tensix_coordinates)
