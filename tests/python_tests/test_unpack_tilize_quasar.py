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
from helpers.llk_params import DestAccumulation, DestSync, UnpackerEngine, format_dict
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
    1. Tilize 32b data into dest is not yet supported

    Returns: List of (format, dest_acc, transpose, unpacker_sel) tuples
    """
    combinations = []

    for fmt in formats_list:
        if fmt.input_format != fmt.output_format:
            continue

        if fmt.input_format.is_32_bit():
            continue  # Tilize 32b data into dest not supported
        else:
            dest_acc_modes = [DestAccumulation.No, DestAccumulation.Yes]
            unpacker_engines = [UnpackerEngine.UNP_A, UnpackerEngine.UNP_B]

        for dest_acc in dest_acc_modes:
            dimensions_list = generate_input_dimensions(dest_acc)
            for unpacker_sel in unpacker_engines:
                for dimensions in dimensions_list:
                    combinations.extend([(fmt, dest_acc, unpacker_sel, dimensions)])

    return combinations


def generate_input_dimensions(dest_acc, dest_sync=DestSync.Half):
    """Generate all possible input dimensions, depending on dest_acc and dest_sync,
    that can fit into dest.

    Key rules:
    1. The possible input dimensions are determined by dest_sync and dest_acc

    Args:
        dest_acc: Dest accumulation mode
        dest_sync: Dest sync mode

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


UNPACK_TILIZE_FORMATS = input_output_formats(
    [
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

    generate_golden = get_golden_generator(TilizeGolden)

    golden_src = src_A if unpacker_sel == UnpackerEngine.UNP_A else src_B
    golden_tensor = generate_golden(
        golden_src, input_dimensions, formats.output_format, 4
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

    assert passed_test(golden_tensor, res_tensor, formats.output_format)
