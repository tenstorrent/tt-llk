# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0


import pytest
import torch
from helpers.format_config import DataFormat
from helpers.golden_generators import (
    BinarySFPUGolden,
    UnarySFPUGolden,
    get_golden_generator,
)
from helpers.llk_params import (
    ApproximationMode,
    DestAccumulation,
    MathOperation,
    ReducePool,
    format_dict,
)
from helpers.param_config import (
    input_output_formats,
    parametrize,
)
from helpers.stimuli_config import StimuliConfig
from helpers.stimuli_generator import generate_stimuli
from helpers.test_config import TestConfig
from helpers.test_variant_parameters import (
    ADD_TOP_ROW,
    APPROX_MODE,
    INPUT_DIMENSIONS,
    MATH_OP,
    TILE_COUNT,
)
from helpers.tilize_untilize import untilize
from helpers.utils import passed_test

max_tiles = 4  # max number of tiles in 32-bit dest is 4
tile_dim = 32

dimension_combinations = [
    [m, n]
    for m in range(tile_dim, max_tiles * tile_dim + 1, tile_dim)
    for n in range(tile_dim, max_tiles * tile_dim + 1, tile_dim)
    if m * n <= max_tiles * tile_dim * tile_dim
]


@parametrize(
    test_name="sfpu_reduce_test",
    formats=input_output_formats(
        [DataFormat.Float32, DataFormat.UInt32, DataFormat.Int32],
        same=True,
    ),
    mathop=[MathOperation.ReduceColumn],
    dest_acc=[DestAccumulation.No, DestAccumulation.Yes],
    negative_number=[False, True],
    reduce_pool=[ReducePool.Sum, ReducePool.Average],
    dimension_combinations=dimension_combinations,
    add_top_row=[False, True],
)
def test_sfpu_reduce(
    test_name,
    formats,
    dest_acc,
    mathop,
    reduce_pool,
    negative_number,
    dimension_combinations,
    add_top_row,
    workers_tensix_coordinates,
):
    if negative_number and formats.input_format == DataFormat.UInt32:
        pytest.skip(
            f"Skipping negative_numbers=True for unsigned format {formats.input_format}"
        )

    input_dimensions = dimension_combinations
    torch_format = format_dict[formats.input_format]

    src_A, tile_cnt_A, src_B, tile_cnt_B = generate_stimuli(
        stimuli_format_A=formats.input_format,
        input_dimensions_A=input_dimensions,
        stimuli_format_B=formats.input_format,
        input_dimensions_B=input_dimensions,
    )

    # Generate 4 faces with all 2s for easy verification (column sums = 32*2 = 64, which is a multiple of 32)
    sign = -1 if negative_number else 1
    src_A *= sign

    generate_golden = get_golden_generator(UnarySFPUGolden)
    golden_tensor = generate_golden(
        mathop,
        src_A,
        formats.output_format,
        dest_acc,
        formats.input_format,
        reduce_pool,
    )

    if add_top_row:
        # Add the top rows of all the tiles we reduced in dst register, accumulate the result in add_top_row_golden_tensor
        add_top_row_golden_tensor = torch.zeros(32, dtype=torch_format)
        generate_golden = get_golden_generator(BinarySFPUGolden)
        for i in range(tile_cnt_A):
            start, end = i * 32, (i + 1) * 32
            add_top_row_golden_tensor = generate_golden(
                MathOperation.SfpuAddTopRow,
                add_top_row_golden_tensor,
                golden_tensor[start:end],
                formats.output_format,
            )

    configuration = TestConfig(
        test_name,
        formats,
        templates=[
            INPUT_DIMENSIONS(input_dimensions, input_dimensions),
            APPROX_MODE(ApproximationMode.No),
            MATH_OP(mathop=mathop, pool_type=reduce_pool),
            ADD_TOP_ROW(add_top_row),
        ],
        runtimes=[TILE_COUNT(tile_cnt_A)],
        variant_stimuli=StimuliConfig(
            src_A,
            formats.input_format,
            src_B,
            formats.input_format,
            formats.output_format,
            tile_count_A=tile_cnt_A,
            tile_count_B=1,
            tile_count_res=tile_cnt_A,
        ),
        dest_acc=dest_acc,
        unpack_to_dest=True,
        disable_format_inference=True,
    )
    res_from_L1 = configuration.run(workers_tensix_coordinates)

    res_tensor = torch.tensor(res_from_L1, dtype=torch_format)

    # collect all only the first row from each tile in result tensor (f0 and f1 row, 32 datums)
    # SFPU reduce operation stores the result in the top row of each tile:
    # - 16 datums in face 0 (first 16 elements of top row)
    # - 16 datums in face 1 (last 16 elements of top row)
    # However, we pack out the full tile (1024 elements), so we need to extract
    # only the top row. Since the result is in tilized format, we must untilize
    # to get row-major ordering, then extract the first 32 elements which
    # correspond to the first row of face 0 and face 1.
    # We do so for each tile we reduced
    reduce_result = []
    golden_result = []
    for i in range(tile_cnt_A):
        # Calculate starting indices for this tile
        start_res = i * 1024  # Each tile has 1024 elements in result tensor
        start_golden = i * 32  # Each tile contributes 32 elements to golden

        # Extract and untilize the current tile, then get first 32 elements (top row)
        result_tile_i = untilize(
            res_tensor[start_res : start_res + 1024], formats.output_format
        ).flatten()[:32]

        # Accumulate results from all tiles
        reduce_result.extend(result_tile_i)
        golden_result.extend(golden_tensor[start_golden : start_golden + 32])

    # Convert to tensors and verify results match expected values
    reduce_tensor = torch.tensor(reduce_result, dtype=torch_format)
    golden_tensor = torch.tensor(golden_result, dtype=torch_format)
    assert passed_test(
        golden_tensor, reduce_tensor, formats.output_format
    ), "Assert against golden failed"
