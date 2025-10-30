# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0

from typing import List

import pytest
import torch
from helpers.device import (
    collect_results,
    write_stimuli_to_l1,
)
from helpers.format_config import DataFormat, FormatConfig
from helpers.golden_generators import (
    TilizeGolden,
    get_golden_generator,
)
from helpers.llk_params import DestAccumulation, UnpackerEngine, format_dict
from helpers.param_config import (
    input_output_formats,
    parametrize,
)
from helpers.stimuli_generator import generate_stimuli
from helpers.test_config import BootMode, run_test
from helpers.utils import passed_test


def generate_unpack_tilize_combinations(
    formats_list: List[FormatConfig],
):
    """
    Generate unpack_tilize combinations.

    Rules:
    1.
    2.
    3.

    Returns: List of (format, dest_acc, transpose, unpacker_sel) tuples
    """
    combinations = []

    for fmt in formats_list:
        if fmt.input_format != fmt.output_format:
            continue

        if fmt.input_format.is_32_bit():
            continue  # Tilize 32b data into dest not supported
        else:
            dest_acc_modes = [DestAccumulation.No]  # DestAccumulation.Yes]
            unpacker_engines = [UnpackerEngine.UNP_A, UnpackerEngine.UNP_B]

        for dest_acc in dest_acc_modes:
            for unpacker_sel in unpacker_engines:
                dimensions_list = generate_dest_acc_and_input_dimensions(dest_acc)
                # dimensions_list = [[64, 64]] #generate_dest_acc_and_input_dimensions(dest_acc)
                for dimensions in dimensions_list:
                    combinations.extend([(fmt, dest_acc, unpacker_sel, dimensions)])

    return combinations


def generate_dest_acc_and_input_dimensions(dest_acc_mode):
    """Generate all combinations of dest_acc and input dimensions."""
    num_tile_rows = 32
    num_tile_cols = 32
    combinations = []

    max_tiles_in_dest = 8 if dest_acc_mode == DestAccumulation.No else 4
    for rows in range(1, max_tiles_in_dest + 1):
        for cols in range(1, (max_tiles_in_dest // rows) + 1):
            dimensions = [rows * num_tile_rows, cols * num_tile_cols]
            combinations.append(dimensions)

    return combinations


UNPACK_TILIZE_FORMATS = input_output_formats(
    [
        # DataFormat.Float16_b,
        DataFormat.Float16_b,
        DataFormat.Float16,
        # DataFormat.Float32,
    ]
)
ALL_UNPACK_TILIZE_COMBINATIONS = generate_unpack_tilize_combinations(
    UNPACK_TILIZE_FORMATS
)


@parametrize(
    test_name="quasar_unpack_tilize_test",
    formats_dest_acc_unpack_sel_dimensions=ALL_UNPACK_TILIZE_COMBINATIONS,
)
def test_unpack_tilize_quasar(
    test_name, formats_dest_acc_unpack_sel_dimensions, boot_mode=BootMode.DEFAULT
):
    formats = formats_dest_acc_unpack_sel_dimensions[0]
    dest_acc = formats_dest_acc_unpack_sel_dimensions[1]
    unpacker_sel = formats_dest_acc_unpack_sel_dimensions[2]
    input_dimensions = formats_dest_acc_unpack_sel_dimensions[3]

    if formats.input_format == DataFormat.Float16 and dest_acc == DestAccumulation.Yes:
        pytest.skip("Does not work")

    # input_dimensions = [32, 32]

    src_A, src_B, tile_cnt = generate_stimuli(
        formats.input_format,
        formats.input_format,
        input_dimensions=input_dimensions,
    )

    # Create test pattern: repeat [1,2,3,4] pattern for each tile's worth of data (1024 elements per tile)
    # src_A = torch.arange(1, 5).repeat_interleave(256).repeat(tile_cnt)
    # src_B = torch.arange(1, 5).repeat_interleave(256).repeat(tile_cnt)

    generate_golden = get_golden_generator(TilizeGolden)
    if unpacker_sel == UnpackerEngine.UNP_A:
        golden_tensor = generate_golden(
            src_A, input_dimensions, formats.output_format, 4
        )
    else:
        golden_tensor = generate_golden(
            src_B, input_dimensions, formats.output_format, 4
        )

    unpack_to_dest = (
        formats.input_format.is_32_bit() and dest_acc == DestAccumulation.Yes
    )

    test_config = {
        "formats": formats,
        "testname": test_name,
        "dest_acc": dest_acc,
        "input_A_dimensions": input_dimensions,
        "input_B_dimensions": input_dimensions,
        "unpack_to_dest": unpack_to_dest,
        "tile_cnt": tile_cnt,
        "unpacker_engine_sel": unpacker_sel,
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

    run_test(test_config, boot_mode=boot_mode)

    res_from_L1 = collect_results(
        formats, tile_count=tile_cnt, address=res_address, num_faces=4
    )

    assert len(res_from_L1) == len(golden_tensor)

    torch_format = format_dict[formats.output_format]
    res_tensor = torch.tensor(res_from_L1, dtype=torch_format)

    print(res_tensor)
    print(golden_tensor)

    assert passed_test(golden_tensor, res_tensor, formats.output_format)
