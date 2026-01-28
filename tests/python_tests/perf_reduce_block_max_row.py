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
    BLOCK_CT_DIM,
    TILE_COUNT,
)


@pytest.mark.perf
@parametrize(
    formats=input_output_formats(
        [
            DataFormat.Float16_b,
        ]
    ),
    dest_acc=[DestAccumulation.No, DestAccumulation.Yes],
    block_ct_dim=[1, 2, 4, 8, 16],
)
def test_perf_reduce_block_max_row(
    perf_report,
    formats,
    dest_acc,
    block_ct_dim,
    workers_tensix_coordinates,
):
    """
    Performance test for reduce_block_max_row operation.

    This tests the custom block-based max row reduction that processes
    multiple tiles in the width dimension as a single block operation.

    Parameters swept (from tt-metal test_sdpa_reduce_c.cpp):
    - block_ct_dim: 1, 2, 4, 8, 16 (k_chunk_sizes in SDPA test)
    - dest_acc: No (16-bit), Yes (32-bit FP32 destination accumulation)
    - formats: Float16_b only (as per API constraints)
    """

    # Total tiles = block_ct_dim (processed as a block)
    tile_count = block_ct_dim

    configuration = PerfConfig(
        "sources/reduce_block_max_row_perf.cpp",
        formats,
        run_types=[
            PerfRunType.L1_TO_L1,
            PerfRunType.UNPACK_ISOLATE,
            PerfRunType.MATH_ISOLATE,
            PerfRunType.PACK_ISOLATE,
            PerfRunType.L1_CONGESTION,
        ],
        templates=[
            BLOCK_CT_DIM(block_ct_dim),
        ],
        runtimes=[TILE_COUNT(tile_count)],
        variant_stimuli=StimuliConfig(
            None,
            formats.input_format,
            None,
            formats.input_format,
            formats.output_format,
            tile_count_A=tile_count,
            tile_count_B=1,  # Scale tile (single tile with 1.0 values)
            tile_count_res=1,  # One output tile (reduced result)
        ),
        unpack_to_dest=False,
        dest_acc=dest_acc,
    )

    configuration.run(perf_report, location=workers_tensix_coordinates)
