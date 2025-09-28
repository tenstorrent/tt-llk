# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0


import torch

from helpers.device import (
    collect_results,
    write_stimuli_to_l1,
)
from helpers.format_arg_mapping import (
    ApproximationMode,
    DestAccumulation,
    MathOperation,
    format_dict,
)
from helpers.format_config import DataFormat
from helpers.golden_generators import UnarySFPUGolden, get_golden_generator
from helpers.param_config import (
    input_output_formats,
    parametrize,
)
from helpers.stimuli_generator import generate_stimuli
from helpers.test_config import run_test
from helpers.tilize_untilize import untilize
from helpers.utils import passed_test


@parametrize(
    test_name="eltwise_unary_sfpu_column_sum_test",
    formats=input_output_formats([DataFormat.Int32]),
    mathop=[MathOperation.SumColumns],
    dest_acc=[DestAccumulation.Yes],
)
def test_eltwise_unary_sfpu_column_sum(test_name, formats, dest_acc, mathop):
    torch.manual_seed(0)
    torch.set_printoptions(precision=10)
    input_dimensions = [32, 32]

    src_A, src_B, tile_cnt = generate_stimuli(
        formats.input_format, formats.input_format, input_dimensions=input_dimensions
    )
    src_A = torch.stack(
        [
            torch.full((16, 16), 1),  # Face 0
            torch.full((16, 16), 2),  # Face 1
            torch.full((16, 16), 3),  # Face 2
            torch.full((16, 16), 4),  # Face 3
        ]
    ).flatten()
    print("SRC_A: ", untilize(src_A, formats.input_format).flatten().view(32, 32))

    generate_golden = get_golden_generator(UnarySFPUGolden)
    golden_tensor = generate_golden(
        mathop, src_A, formats.output_format, dest_acc, formats.input_format
    )

    test_config = {
        "formats": formats,
        "testname": test_name,
        "dest_acc": dest_acc,
        "input_A_dimensions": input_dimensions,
        "input_B_dimensions": input_dimensions,
        "mathop": mathop,
        "approx_mode": ApproximationMode.No,
        "unpack_to_dest": True,
        "tile_cnt": tile_cnt,
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

    torch_format = format_dict[formats.output_format]
    res_from_L1 = collect_results(formats, tile_count=tile_cnt, address=res_address)
    print(
        "RES_FROM_L1: ",
        untilize(torch.tensor(res_from_L1, dtype=torch_format), formats.output_format)
        .flatten()
        .view(32, 32),
    )
    res_tensor = torch.tensor(res_from_L1, dtype=torch_format)
    res_tensor = untilize(res_tensor, formats.output_format).flatten()[:32]

    # For column sum, we only compare the first 32 elements (column sums)
    assert len(res_tensor) == len(golden_tensor)

    assert passed_test(golden_tensor, res_tensor, formats.output_format)
