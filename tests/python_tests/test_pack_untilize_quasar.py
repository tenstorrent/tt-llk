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
    """Generate all combinations of dest_acc and input dimensions."""
    num_tile_rows = 32
    num_tile_cols = 32
    dest_acc_list = [DestAccumulation.No, DestAccumulation.Yes]
    combinations = []

    for dest_acc in dest_acc_list:
        max_tiles_in_dest = 8 if dest_acc == DestAccumulation.No else 4
        for rows in range(1, max_tiles_in_dest + 1):
            for cols in range(1, (max_tiles_in_dest // rows) + 1):
                dimensions = [rows * num_tile_rows, cols * num_tile_cols]
                combinations.append((dest_acc, dimensions))

    return combinations


@parametrize(
    test_name="quasar_pack_untilize_test",
    formats=input_output_formats(
        [
            DataFormat.Float16_b,
            # DataFormat.Float16,
            # DataFormat.Float32,
        ],
        same=get_chip_architecture() == ChipArchitecture.QUASAR,
    ),
    dest_acc_and_dimensions=generate_dest_acc_and_input_dimensions(),
)
def test_pack_untilize_quasar(test_name, formats, dest_acc_and_dimensions):

    dest_acc, input_dimensions = dest_acc_and_dimensions

    if formats.input_format == DataFormat.Float32 and dest_acc == DestAccumulation.No:
        pytest.skip("Does not work")
    if formats.input_format == DataFormat.Float16 and dest_acc == DestAccumulation.Yes:
        pytest.skip("Does not work")

    src_A, src_B, tile_cnt = generate_stimuli(
        formats.input_format, formats.input_format, input_dimensions=input_dimensions
    )

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

    assert passed_test(golden_tensor, res_tensor, formats.output_format)
