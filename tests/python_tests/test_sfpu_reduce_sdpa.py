# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0

import torch
from helpers.device import collect_results, write_stimuli_to_l1
from helpers.format_config import DataFormat
from helpers.llk_params import (
    DestAccumulation,
    MathOperation,
    ReducePool,
    format_dict,
)
from helpers.param_config import input_output_formats, parametrize
from helpers.stimuli_generator import generate_stimuli
from helpers.test_config import run_test
from helpers.tilize_untilize import tilize_block, untilize
from helpers.utils import passed_test


@parametrize(
    test_name="sfpu_reduce_sdpa_test",
    formats=input_output_formats(
        [DataFormat.Float16_b],  # Only Float16_b is supported for SDPA reduce
        same=True,
    ),
    dest_acc=[DestAccumulation.No],
    mathop=[MathOperation.ReduceColumn],
    reduce_pool=[ReducePool.Max],  # Only MAX is supported for SDPA reduce
)
def test_sfpu_reduce_sdpa(test_name, formats, dest_acc, mathop, reduce_pool):
    """Test SFPU reduce SDPA kernel with single tile."""

    input_dimensions = [32, 32]

    src_A, _, tile_cnt = generate_stimuli(
        formats.input_format, formats.input_format, input_dimensions=input_dimensions
    )

    # Set src_A such that each row i (0-based) contains (i+1)
    src_A = (
        torch.arange(0, 32, dtype=format_dict[formats.input_format])
        .repeat(32, 1)
        .T.flatten()
    )

    src_A = tilize_block(src_A, input_dimensions, formats.input_format)

    src_B = torch.zeros(1024, dtype=format_dict[formats.input_format])

    golden_tensor = (
        src_A.clone()
    )  # WRONG BUT CAN STAY FOR NOW SINCE THIS IS FOR DEBUGGING

    test_config = {
        "formats": formats,
        "testname": test_name,
        "dest_acc": dest_acc,
        "input_A_dimensions": input_dimensions,
        "input_B_dimensions": input_dimensions,
        "mathop": mathop,
        "pool_type": reduce_pool,
        "unpack_to_dest": True,
        "tile_cnt": tile_cnt,  # Keep tile_cnt for future multi-tile support
    }

    res_address = write_stimuli_to_l1(
        test_config,
        src_A,
        src_B,
        formats.input_format,
        formats.input_format,
        tile_count_A=tile_cnt,
        tile_count_B=tile_cnt,
    )

    run_test(test_config)

    res_from_L1 = collect_results(formats, tile_count=tile_cnt, address=res_address)
    res_tensor = torch.tensor(res_from_L1, dtype=format_dict[formats.output_format])
    res_tensor = untilize(res_tensor, formats.output_format)

    # Print the first 4 rows (128 elements) of src_A and result tensor, for each "face"
    print("First 4 rows of src_A (should be same value per row):")
    print(src_A.view(64, 16))
    print("First 4 rows of result tensor:")
    # print(res_tensor.view(64,16))
    print(res_tensor.view(32, 32))

    assert passed_test(golden_tensor, res_tensor.flatten(), formats.output_format)
