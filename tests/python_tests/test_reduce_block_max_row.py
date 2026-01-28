# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0

import torch
from helpers.format_config import DataFormat
from helpers.golden_generators import ReduceBlockMaxRowGolden, get_golden_generator
from helpers.llk_params import (
    DestAccumulation,
    format_dict,
)
from helpers.param_config import input_output_formats, parametrize
from helpers.stimuli_config import StimuliConfig
from helpers.stimuli_generator import generate_stimuli
from helpers.test_config import TestConfig
from helpers.test_variant_parameters import (
    BLOCK_CT_DIM,
)
from helpers.utils import passed_test


@parametrize(
    formats=input_output_formats(
        [
            DataFormat.Float16_b,
        ]
    ),
    dest_acc=[DestAccumulation.No, DestAccumulation.Yes],
    block_ct_dim=[1, 2, 4, 8, 16],
)
def test_reduce_block_max_row(
    formats, dest_acc, block_ct_dim, workers_tensix_coordinates
):
    """
    Functional test for reduce_block_max_row operation.

    This tests the custom block-based max row reduction that processes
    multiple tiles in the width dimension as a single block operation.

    The operation:
    - Takes block_ct_dim tiles arranged horizontally (32 rows x (32*block_ct_dim) cols)
    - Finds the maximum value in each row across all tiles
    - Returns a single tile with max values in the first column

    Parameters swept (from tt-metal test_sdpa_reduce_c.cpp):
    - block_ct_dim: 1, 2, 4, 8, 16 (k_chunk_sizes in SDPA test)
    - dest_acc: No (16-bit), Yes (32-bit FP32 destination accumulation)
    - formats: Float16_b only (as per API constraints)
    """

    # Input is block_ct_dim tiles worth of data (32 x 32*block_ct_dim)
    input_dimensions = [32, 32 * block_ct_dim]

    src_A, tile_cnt_A, src_B, tile_cnt_B = generate_stimuli(
        stimuli_format_A=formats.input_format,
        input_dimensions_A=input_dimensions,
        stimuli_format_B=formats.input_format,
        input_dimensions_B=[32, 32],  # Scale tile
    )

    # Create scale tile with all 1.0 values (required by reduce_block_max_row API)
    src_B = torch.full((1024,), 1.0)
    tile_cnt_B = 1

    # Generate golden output
    generate_golden = get_golden_generator(ReduceBlockMaxRowGolden)
    golden_tensor = generate_golden(src_A, block_ct_dim, formats.output_format)

    configuration = TestConfig(
        "sources/reduce_block_max_row_test.cpp",
        formats,
        templates=[BLOCK_CT_DIM(block_ct_dim)],
        variant_stimuli=StimuliConfig(
            src_A,
            formats.input_format,
            src_B,
            formats.input_format,
            formats.output_format,
            tile_count_A=block_ct_dim,  # block_ct_dim tiles
            tile_count_B=tile_cnt_B,  # 1 scale tile
            tile_count_res=1,  # 1 output tile
        ),
        dest_acc=dest_acc,
    )

    res_from_L1 = configuration.run(workers_tensix_coordinates)

    assert len(res_from_L1) == len(
        golden_tensor
    ), "Result tensor and golden tensor are not of the same length"

    res_tensor = torch.tensor(res_from_L1, dtype=format_dict[formats.output_format])

    assert passed_test(
        golden_tensor, res_tensor, formats.output_format
    ), "Assert against golden failed"
