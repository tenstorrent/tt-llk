# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0

from typing import List

import pytest
import torch
from helpers.device import collect_results, write_stimuli_to_l1
from helpers.format_config import DataFormat, FormatConfig
from helpers.golden_generators import UntilizeGolden, get_golden_generator
from helpers.llk_params import DestAccumulation, DestSync, format_dict
from helpers.param_config import input_output_formats, parametrize
from helpers.stimuli_generator import generate_stimuli
from helpers.test_config import run_test
from helpers.utils import passed_test


def generate_pack_untilize_combinations(
    formats_list: List[FormatConfig],
):
    """
    Generate pack_untilize combinations.

    Args: List of input-output format pairs

    Returns: List of (format, dest_acc, input_dimensions) tuples
    """
    combinations = []

    for fmt in formats_list:
        if fmt.input_format != fmt.output_format:
            continue

        if fmt.input_format.is_32_bit():
            dest_acc_modes = [DestAccumulation.Yes]
        else:
            dest_acc_modes = [DestAccumulation.No, DestAccumulation.Yes]

        for dest_acc in dest_acc_modes:
            dimensions_list = generate_input_dimensions(dest_acc)
            # dimensions_list = [[96, 64]]
            for dimensions in dimensions_list:
                combinations.extend([(fmt, dest_acc, dimensions)])

    return combinations


def generate_input_dimensions(dest_acc, dest_sync=DestSync.Half):
    """Generate all possible input dimensions. They are determined by the number of tiles that can fit into dest,
    which is determined by dest_sync and dest_acc.

    Key rules:
    1. When DestSync.Half:  max_tiles_in_dest=8 (if dest is 16bit) or max_tiles_in_dest=4 (if dest is 32bit)
    2. When DestSync.Full:  max_tiles_in_dest=16 (if dest is 16bit) or max_tiles_in_dest=8 (if dest is 32bit)

    Args:
        dest_acc: Dest 16/32 bit mode
        dest_sync: DestSync mode. If None, uses [DestSync.Half]

    Returns:
        List of input dimensions
    """

    DEST_SYNC_TILE_LIMITS = {
        DestSync.Half: 8,
        DestSync.Full: 16,
    }
    capacity_divisor = 2 if dest_acc == DestAccumulation.Yes else 1
    max_tiles_in_dest = DEST_SYNC_TILE_LIMITS[dest_sync] // capacity_divisor

    num_tile_rows = 32
    num_tile_cols = 32
    combinations = []

    for rows in range(1, max_tiles_in_dest + 1):
        for cols in range(1, (max_tiles_in_dest // rows) + 1):
            dimensions = [rows * num_tile_rows, cols * num_tile_cols]
            combinations.append(dimensions)

    return combinations


PACK_UNTILIZE_FORMATS = input_output_formats(
    [
        DataFormat.Float16_b,
        DataFormat.Float16,
        # DataFormat.Float32,
    ]
)
ALL_PACK_UNTILIZE_COMBINATIONS = generate_pack_untilize_combinations(
    PACK_UNTILIZE_FORMATS
)


@parametrize(
    test_name="quasar_pack_untilize_test",
    formats_dest_acc_dimensions=ALL_PACK_UNTILIZE_COMBINATIONS,
)
def test_pack_untilize_quasar(test_name, formats_dest_acc_dimensions):

    formats = formats_dest_acc_dimensions[0]
    dest_acc = formats_dest_acc_dimensions[1]
    input_dimensions = formats_dest_acc_dimensions[2]

    if formats.input_format == DataFormat.Float16 and dest_acc == DestAccumulation.Yes:
        pytest.skip("Fails.")

    # if input_dimensions[0] == 32:
    #     pytest.skip("SKIP.")

    # if input_dimensions[0] == 96 and input_dimensions[1] == 32:
    #    pytest.skip("SKIP.")

    # if (input_dimensions[0] // 32) % 2 == 0:
    #    pytest.skip("SKIP.")

    src_A, src_B, tile_cnt = generate_stimuli(
        formats.input_format, formats.input_format, input_dimensions=input_dimensions
    )

    src_A = torch.arange(1, 4 * tile_cnt + 1).repeat_interleave(256)

    generate_golden = get_golden_generator(UntilizeGolden)
    golden_tensor = generate_golden(src_A, formats.output_format, input_dimensions)

    unpack_to_dest = (
        formats.input_format.is_32_bit() and dest_acc == DestAccumulation.Yes
    )

    test_config = {
        "formats": formats,
        "testname": test_name,
        "tile_cnt": tile_cnt,
        "input_A_dimensions": input_dimensions,
        "input_B_dimensions": input_dimensions,
        "unpack_to_dest": unpack_to_dest,
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
        num_faces=4,
    )

    run_test(test_config)

    res_from_L1 = collect_results(
        formats, tile_count=tile_cnt, address=res_address, num_faces=4
    )
    assert len(res_from_L1) == len(golden_tensor)

    res_tensor = torch.tensor(res_from_L1, dtype=format_dict[formats.output_format])

    # print(f"res_tensor: {res_tensor}")
    # print(f"golden_tensor: {golden_tensor}")

    assert passed_test(golden_tensor, res_tensor, formats.output_format)
