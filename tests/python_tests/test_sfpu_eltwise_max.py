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
    input_dimensions=[
        [32, 32],
        [32, 64],
        [64, 32],
        [64, 64],
    ],  # Base dimensions for srcA and srcB (each 1 tile)
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

    # Generate stimuli - src_A and src_B each have input_dimensions
    src_A, src_B, tile_cnt = generate_stimuli(
        formats.input_format, formats.input_format, input_dimensions=input_dimensions
    )
    golden_tensor = torch.maximum(src_A, src_B).flatten()
    src_A_combined = torch.cat([src_A, src_B], dim=0)

    # Tile counts: srcA has 2x tiles (A tiles + B tiles), srcB not used
    tile_cnt_A = tile_cnt * 2
    tile_cnt_B = 0

    # Output tile count = input tile count (one output per A,B pair)
    output_tile_cnt = tile_cnt

    # Dimensions for combined srcA (doubled in first dimension)
    srcA_dimensions = [input_dimensions[0] * 2, input_dimensions[1]]

    unpack_to_dest = formats.input_format.is_32_bit()

    # Force dest_acc for certain formats
    if formats.input_format in [DataFormat.Float16, DataFormat.Float32]:
        dest_acc = DestAccumulation.Yes

    test_config = {
        "formats": formats,
        "testname": test_name,
        "dest_acc": dest_acc,
        "input_A_dimensions": srcA_dimensions,
        "input_B_dimensions": input_dimensions,
        "unpack_to_dest": unpack_to_dest,
        "tile_cnt": tile_cnt_A,  # C++ needs total tile count for srcA
    }

    res_address = write_stimuli_to_l1(
        test_config,
        src_A_combined,
        src_B,  # Not used (tile_cnt_B = 0)
        formats.input_format,
        formats.input_format,
        tile_count_A=tile_cnt_A,
        tile_count_B=tile_cnt_B,
    )

    run_test(test_config)

    res_from_L1 = collect_results(
        formats, tile_count=output_tile_cnt, address=res_address
    )

    torch_format = format_dict[formats.output_format]
    res_tensor = torch.tensor(res_from_L1, dtype=torch_format).flatten()

    assert len(res_tensor) == len(golden_tensor)

    assert passed_test(golden_tensor, res_tensor, formats.output_format)
