# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0

import pytest
import torch
from helpers.chip_architecture import ChipArchitecture, get_chip_architecture
from helpers.device import collect_results, write_stimuli_to_l1
from helpers.format_config import DataFormat
from helpers.golden_generators import UntilizeGolden, get_golden_generator
from helpers.llk_params import DestAccumulation, format_dict
from helpers.param_config import input_output_formats, parametrize
from helpers.stimuli_generator import generate_stimuli
from helpers.test_config import run_test
from helpers.utils import passed_test


def generate_dest_acc_and_input_dimensions():
    dest_acc_list = [DestAccumulation.No, DestAccumulation.Yes]
    combinations = []

    for dest_acc in dest_acc_list:
        if dest_acc == DestAccumulation.No:
            dimensions_list = [
                [32, 32],
                [32, 64],
                [32, 96],
                [32, 128],
                [32, 160],
                [32, 192],
                [32, 224],
                [32, 256],
                [64, 32],
                [64, 64],
                [64, 96],
                [64, 128],
                [96, 32],
                [96, 64],
                [128, 32],
                [128, 64],
                [160, 32],
                [192, 32],
                [224, 32],
                [256, 32],
            ]
        else:
            dimensions_list = [
                [32, 32],
                [32, 64],
                [32, 96],
                [32, 128],
                [64, 32],
                [64, 64],
                [96, 32],
                [128, 32],
            ]

        for dimensions in dimensions_list:
            combinations.append((dest_acc, dimensions))

    return combinations


@parametrize(
    test_name="pack_untilize_test",
    formats=input_output_formats(
        [
            DataFormat.Float16_b,
            # DataFormat.Float16,
            # DataFormat.Float32,  # Test Float32 with both 32bit mode dest (full precision) and 16bit mode dest (precision loss)
            # DataFormat.Int32,
            # DataFormat.Bfp8_b,
        ],  # Pack Untilize doesn't work for block float formats (Bfp8_b); we only include as input format in our test
        same=get_chip_architecture() == ChipArchitecture.QUASAR,
    ),
    dest_acc_and_dimensions=generate_dest_acc_and_input_dimensions(),
)
def test_pack_untilize(test_name, formats, dest_acc_and_dimensions):
    dest_acc, input_dimensions = dest_acc_and_dimensions

    if formats.output_format == DataFormat.Bfp8_b:
        pytest.skip("Pack Untilize does not support Bfp8_b format")

    if (formats.input_format == DataFormat.Int32) ^ (
        formats.output_format == DataFormat.Int32
    ):
        pytest.skip("Pack Untilize does not support mixing Int32 with other formats")

    #    if (
    #        formats.input_format == DataFormat.Int32
    #        and formats.output_format == DataFormat.Int32
    #        and dest_acc == DestAccumulation.No
    #    ):
    #        pytest.skip("Dest must be in 32bit mode when input and output are Int32")

    if (
        get_chip_architecture() == ChipArchitecture.QUASAR
        and formats.input_format == DataFormat.Bfp8_b
    ):
        pytest.skip("No Bfp8_b format on QUASAR")

    if formats.input_format == DataFormat.Float32 and dest_acc == DestAccumulation.No:
        pytest.skip("Float32:dest_acc=No is not supported")

    if formats.input_format == DataFormat.Float16 and dest_acc == DestAccumulation.Yes:
        pytest.skip("Float16:dest_acc=Yes is not supported")

    dest_acc = dest_acc_and_dimensions[0]
    input_dimensions = dest_acc_and_dimensions[1]
    # input_dimensions = [32, 64]
    # input_dimensions = [32, 32]

    src_A, src_B, tile_cnt = generate_stimuli(
        formats.input_format, formats.input_format, input_dimensions=input_dimensions
    )
    # 1, 5
    # if input_dimensions == [64, 64]:
    #     src_A = torch.arange(1, 17).repeat_interleave(256)
    #     src_B = torch.arange(1, 17).repeat_interleave(256)
    #     print(f"src_A: {src_A}")

    generate_golden = get_golden_generator(UntilizeGolden)
    golden_tensor = generate_golden(src_A, formats.output_format, input_dimensions)

    # dest_acc = DestAccumulation.Yes if formats.input_format.is_32_bit() else DestAccumulation.No

    test_config = {
        "formats": formats,
        "testname": test_name,
        "tile_cnt": tile_cnt,
        "input_A_dimensions": input_dimensions,
        "input_B_dimensions": input_dimensions,
        "unpack_to_dest": formats.input_format.is_32_bit(),
        "dest_acc": dest_acc,
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
    assert len(res_from_L1) == len(golden_tensor)

    res_tensor = torch.tensor(res_from_L1, dtype=format_dict[formats.output_format])

    # print(f"res_tensor: {res_tensor}")
    # print(f"golden_tensor: {golden_tensor}")
    # Find index of first 1
    # indices = (res_tensor == 1.0).nonzero(as_tuple=True)[0]

    # if indices.numel() > 0:
    #     first_index = indices[0].item()
    #     print(f"First index of 1: {first_index}")
    # else:
    #     print("No value of 1 found in the tensor.")

    # indices2 = (res_tensor == 2.0).nonzero(as_tuple=True)[0]

    # if indices2.numel() > 0:
    #     first_index = indices2[0].item()
    #     print(f"First index of 2: {first_index}")
    # else:
    #     print("No value of 2 found in the tensor.")

    # indices3 = (res_tensor == 3.0).nonzero(as_tuple=True)[0]

    # if indices3.numel() > 0:
    #     first_index = indices3[0].item()
    #     print(f"First index of 3: {first_index}")
    # else:
    #     print("No value of 3 found in the tensor.")

    # if input_dimensions == [64, 64]:
    #     print(f"res_tensor: {res_tensor}")
    #     print(f"golden_tensor: {golden_tensor}")

    assert passed_test(golden_tensor, res_tensor, formats.output_format)
