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


@parametrize(
    test_name="pack_untilize_test",
    formats=input_output_formats(
        [
            DataFormat.Float16_b,
            DataFormat.Float16,
<<<<<<< HEAD
            DataFormat.Float32,  # Test Float32 with both 32bit mode dest (full precision) and 16bit mode dest (precision loss)
            DataFormat.Int32,
            DataFormat.Bfp8_b,
        ]  # Pack Untilize doesn't work for block float formats (Bfp8_b); we only include as input format in our test
=======
            # DataFormat.Float16_b,
            # DataFormat.Float32,
            # DataFormat.Int32,
            # DataFormat.Bfp8_b,
        ],  # Pack Untilize doesn't work for block float formats (Bfp8_b); we only include as input format in our test
        same=get_chip_architecture() == ChipArchitecture.QUASAR,
>>>>>>> b0db3b75 (Add pack untilize test for qsr)
    ),
    dest_acc=[DestAccumulation.No, DestAccumulation.Yes],
)
def test_pack_untilize(test_name, formats, dest_acc):
    if formats.output_format == DataFormat.Bfp8_b:
        pytest.skip("Pack Untilize does not support Bfp8_b format")

    if (formats.input_format == DataFormat.Int32) ^ (
        formats.output_format == DataFormat.Int32
    ):
        pytest.skip("Pack Untilize does not support mixing Int32 with other formats")

    if (
<<<<<<< HEAD
        formats.input_format == DataFormat.Int32
        and formats.output_format == DataFormat.Int32
        and dest_acc == DestAccumulation.No
    ):
        pytest.skip("Dest must be in 32bit mode when input and output are Int32")

    input_dimensions = [32, 128]
=======
        get_chip_architecture() == ChipArchitecture.QUASAR
        and formats.input_format == DataFormat.Bfp8_b
    ):
        pytest.skip("No Bfp8_b format on QUASAR")

    # input_dimensions = [32, 128]
    input_dimensions = [32, 32]
>>>>>>> b0db3b75 (Add pack untilize test for qsr)

    src_A, src_B, tile_cnt = generate_stimuli(
        formats.input_format, formats.input_format, input_dimensions=input_dimensions
    )

    src_A = torch.arange(1, 5).repeat_interleave(256)
    src_B = torch.arange(1, 5).repeat_interleave(256)

    generate_golden = get_golden_generator(UntilizeGolden)
    golden_tensor = generate_golden(src_A, formats.output_format, input_dimensions)

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

    print(res_tensor)
    # Find index of first 1
    indices = (res_tensor == 1.0).nonzero(as_tuple=True)[0]

    if indices.numel() > 0:
        first_index = indices[0].item()
        print(f"First index of 1: {first_index}")
    else:
        print("No value of 1 found in the tensor.")

    indices2 = (res_tensor == 2.0).nonzero(as_tuple=True)[0]

    if indices2.numel() > 0:
        first_index = indices2[0].item()
        print(f"First index of 2: {first_index}")
    else:
        print("No value of 2 found in the tensor.")

    indices3 = (res_tensor == 3.0).nonzero(as_tuple=True)[0]

    if indices3.numel() > 0:
        first_index = indices3[0].item()
        print(f"First index of 3: {first_index}")
    else:
        print("No value of 3 found in the tensor.")

    assert passed_test(golden_tensor, res_tensor, formats.output_format)
