# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0


import torch
from helpers.chip_architecture import get_chip_architecture
from helpers.device import collect_results, write_stimuli_to_l1
from helpers.format_config import DataFormat
from helpers.golden_generators import UnarySFPUGolden, get_golden_generator
from helpers.llk_params import (
    DestAccumulation,
    MathOperation,
    format_dict,
)
from helpers.param_config import input_output_formats, parametrize
from helpers.stimuli_generator import generate_stimuli
from helpers.test_config import run_test
from helpers.tilize_untilize import tilize, untilize
from helpers.utils import passed_test


@parametrize(
    test_name="transp_dest_row_sfpu_test",
    formats=input_output_formats(
        [
            DataFormat.Float16_b,
        ]
    ),
    dest_acc=[DestAccumulation.No],
    mathop=[MathOperation.TransposeRow],
)
def test_eltwise_unary_sfpu_float(test_name, formats, dest_acc, mathop):
    arch = get_chip_architecture()
    eltwise_unary_sfpu(test_name, formats, dest_acc, mathop)


def eltwise_unary_sfpu(test_name, formats, dest_acc, mathop):
    torch.manual_seed(0)
    # torch.set_printoptions(precision=10)
    input_dimensions = [32, 32]

    src_A, src_B, tile_cnt = generate_stimuli(
        formats.input_format, formats.input_format, input_dimensions=input_dimensions
    )

    src_A = torch.ones(1024)
    src_A[0:32] = torch.arange(32)

    src_A = tilize(src_A, stimuli_format=formats.input_format)

    generate_golden = get_golden_generator(UnarySFPUGolden)
    golden_tensor = generate_golden(
        mathop, src_A, formats.output_format, dest_acc, formats.input_format
    )

    unpack_to_dest = (
        formats.input_format.is_32_bit()
        and dest_acc
        == DestAccumulation.Yes  # If dest_acc is off, we unpack Float32 into 16-bit format in src registers (later copied over in dest reg for SFPU op)
    )
    test_config = {
        "formats": formats,
        "testname": test_name,
        "dest_acc": dest_acc,
        "input_A_dimensions": input_dimensions,
        "input_B_dimensions": input_dimensions,
        "mathop": mathop,
        "unpack_to_dest": unpack_to_dest,
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

    res_from_L1 = collect_results(formats, tile_count=tile_cnt, address=res_address)

    # res_from_L1 = res_from_L1[:1024]
    assert len(res_from_L1) == len(golden_tensor)

    torch_format = format_dict[formats.output_format]
    res_tensor = torch.tensor(res_from_L1, dtype=torch_format)

    res_tensor = untilize(res_tensor, stimuli_format=formats.output_format)

    print(res_tensor.view(32, 32))

    assert passed_test(golden_tensor, res_tensor, formats.output_format)
