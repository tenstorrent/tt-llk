# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0

import pytest
import torch
from helpers.chip_architecture import ChipArchitecture, get_chip_architecture
from helpers.device import collect_results, write_stimuli_to_l1
from helpers.format_config import DataFormat
from helpers.llk_params import DestAccumulation, format_dict
from helpers.param_config import input_output_formats, parametrize
from helpers.stimuli_generator import generate_stimuli
from helpers.test_config import run_test
from helpers.utils import passed_test


@parametrize(
    test_name="sfpu_eltwise_max_test",
    formats=input_output_formats(
        [
            DataFormat.Float32,
            DataFormat.Float16,
            DataFormat.Float16_b,
            DataFormat.Bfp8_b,
        ]
    ),
    dest_acc=[DestAccumulation.No, DestAccumulation.Yes],
    input_dimensions=[[64, 32]],  # 64x32 = 2 tiles (each tile is 32x32)
)
def test_sfpu_eltwise_max_float(test_name, formats, dest_acc, input_dimensions):

    chip_arch = get_chip_architecture()

    if (
        chip_arch == ChipArchitecture.BLACKHOLE
        and formats.input_format == DataFormat.Float16
        and dest_acc == DestAccumulation.No
    ):
        pytest.skip(
            "Float16_a isn't supported for SFPU on Blackhole without being converted to 32-bit intermediate format in dest register"
        )

    sfpu_eltwise_max(test_name, formats, dest_acc, input_dimensions)


def sfpu_eltwise_max(test_name, formats, dest_acc, input_dimensions):

    # Generate stimuli - src_A contains 2 tiles
    src_A, src_B, tile_cnt = generate_stimuli(
        formats.input_format, formats.input_format, input_dimensions=input_dimensions
    )

    tile_size = 1024  # 32x32 elements per tile

    # Convert to torch tensor for golden calculation
    torch_format = format_dict[
        (
            formats.output_format
            if formats.output_format != DataFormat.Bfp8_b
            else DataFormat.Float16_b
        )
    ]
    src_A_tensor = torch.tensor(src_A, dtype=torch_format)

    # Split into two tiles
    tile_0 = src_A_tensor[:tile_size]
    tile_1 = src_A_tensor[tile_size : 2 * tile_size]

    # Golden: elementwise max of the two tiles
    golden_tensor = torch.maximum(tile_0, tile_1)

    unpack_to_dest = formats.input_format.is_32_bit()

    # Force dest_acc for certain formats
    if formats.input_format in [DataFormat.Float16, DataFormat.Float32]:
        dest_acc = DestAccumulation.Yes

    test_config = {
        "formats": formats,
        "testname": test_name,
        "dest_acc": dest_acc,
        "input_A_dimensions": input_dimensions,
        "input_B_dimensions": input_dimensions,
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

    torch_format = format_dict[formats.output_format]
    res_tensor = torch.tensor(res_from_L1, dtype=torch_format)[
        :tile_size
    ]  # Only first tile has result

    assert len(res_tensor) == len(
        golden_tensor
    ), f"Result length mismatch: got {len(res_tensor)}, expected {len(golden_tensor)}"

    assert passed_test(
        golden_tensor, res_tensor, formats.output_format
    ), f"Eltwise max test failed for format {formats.output_format}"
