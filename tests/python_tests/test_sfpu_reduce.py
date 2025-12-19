# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0


import pytest
import torch
from helpers.device import (
    collect_results,
    write_stimuli_to_l1,
)
from helpers.format_config import DataFormat, InputOutputFormat
from helpers.golden_generators import (
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
from helpers.test_config import run_test
from helpers.tilize_untilize import tilize_block, untilize_block
from helpers.utils import passed_test

max_tiles = 4  # max number of tiles in 32-bit dest is 4
tile_dim = 32

dimension_combinations = [
    [m, n]
    for m in range(tile_dim, max_tiles * tile_dim + 1, tile_dim)
    for n in range(tile_dim, max_tiles * tile_dim + 1, tile_dim)
    if m * n <= max_tiles * tile_dim * tile_dim
]


def get_format_input_bounds(formats: InputOutputFormat) -> list[tuple[int, int]]:
    """Get valid stimuli bounds based on data format.
    - range needs to be cut off at 1000 for Sum reduction kernels with UInt16 input format to avoid overflow.
    """
    if formats.input_format in [DataFormat.UInt32, DataFormat.UInt16]:
        return [(0, 1000)]
    return [(-1000, 1000), (0, 1000), (-1000, 0)]


@parametrize(
    test_name="sfpu_reduce_test",
    formats=input_output_formats(
        [
            DataFormat.Float32,
            DataFormat.Int32,
            # DataFormat.UInt32,
            # DataFormat.UInt16,
            DataFormat.Float16_b,
        ],
        same=True,
    ),
    mathop=[MathOperation.ReduceColumn],
    dest_acc=[DestAccumulation.No, DestAccumulation.Yes],
    input_bounds=lambda formats: get_format_input_bounds(formats),
    reduce_pool=[
        ReducePool.Max
    ],  # , ReducePool.Sum, ReducePool.Average], #ReducePool.Min
    dimension_combinations=dimension_combinations,
)
def test_sfpu_reduce(
    test_name,
    formats,
    dest_acc,
    mathop,
    reduce_pool,
    input_bounds,
    dimension_combinations,
):
    min_value, max_value = input_bounds
    input_dimensions = dimension_combinations
    torch_format = format_dict[formats.input_format]

    if formats.input_format.is_32_bit() and dest_acc == DestAccumulation.No:
        pytest.skip("Float32 format with dest accumulation is not supported")

    # STIMULI GENERATION
    ELEMENTS_PER_TILE = 1024
    tile_cnt = input_dimensions[0] * input_dimensions[1] // ELEMENTS_PER_TILE
    src_A = torch.randint(
        low=min_value, high=max_value, size=(tile_cnt * 1024,), dtype=torch_format
    )
    src_B = torch.zeros_like(src_A)

    # Max Reduction can do block and single tile reduction whereas Sum/Avg only do single tile reduction, convert Sum/Avg golden to do block reduction by retilizing input to src_A
    # Dimensions for Max reduction work column wise, for Sum/Avg processing tiles independently is same as column reduction on dst block dimension [32, num_tiles * 32] where num rows is 32 i.e RT_DIM=1 (same as a single tile)

    # For MAX: use actual input dimensions to properly setup the block
    # For SUM/AVG: use dst_dim to simulate per-tile processing
    if reduce_pool == ReducePool.Max:
        golden_dim = input_dimensions  # e.g., [128, 32] for vertical blocks, [32, 128] for horizontal
    else:
        golden_dim = [32, tile_cnt * 32]  # SUM/AVG process tiles independently

    src_A = tilize_block(
        src_A, golden_dim, stimuli_format=formats.input_format
    ).flatten()  # Input tensor is tilized in dst register
    src_A_untilized = untilize_block(
        src_A, formats.input_format, golden_dim
    )  # Passed into golden since PyTorch library has no concept of tilization

    golden_tensor = get_golden_generator(UnarySFPUGolden)(
        MathOperation.ReduceColumn,
        src_A_untilized,
        formats.output_format,
        dest_acc,
        formats.input_format,
        input_dimensions,
        reduce_pool=reduce_pool,
    )

    test_config = {
        "formats": formats,
        "testname": test_name,
        "dest_acc": dest_acc,
        "input_A_dimensions": input_dimensions,
        "input_B_dimensions": input_dimensions,
        "mathop": mathop,
        "pool_type": reduce_pool,
        "approx_mode": ApproximationMode.No,
        "unpack_to_dest": True,
        "tile_cnt": tile_cnt,
        "disable_format_inference": True,
    }

    res_address = write_stimuli_to_l1(
        test_config,
        src_A,
        src_B,
        formats.input_format,
        formats.input_format,
        tile_count_A=tile_cnt,
        tile_count_B=1,
    )
    run_test(test_config)

    # MAX/MIN produces BLOCK_CT_DIM output tiles (one per column of tiles)
    # SUM/AVG produces tile_cnt tiles (one per input tile)
    if reduce_pool == ReducePool.Max:
        block_ct_dim = input_dimensions[1] // 32  # Number of tile columns
        output_tile_cnt = block_ct_dim
        result_dim = [32, block_ct_dim * 32]  # Result dimensions
    else:
        output_tile_cnt = tile_cnt
        result_dim = golden_dim  # Use same dim as golden for SUM/AVG

    res_from_L1 = collect_results(
        formats, tile_count=output_tile_cnt, address=res_address
    )
    res_tensor = torch.tensor(res_from_L1, dtype=format_dict[formats.output_format])
    res_tensor = untilize_block(res_tensor, formats.output_format, result_dim)

    assert passed_test(golden_tensor[0], res_tensor[0], formats.output_format)
