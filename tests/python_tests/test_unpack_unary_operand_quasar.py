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
    DataCopyGolden,
    TransposeGolden,
    get_golden_generator,
)
from helpers.llk_params import DestAccumulation, Transpose, UnpackerEngine, format_dict
from helpers.param_config import (
    input_output_formats,
    parametrize,
)
from helpers.stimuli_generator import generate_stimuli
from helpers.test_config import BootMode, run_test
from helpers.utils import passed_test


def generate_unpack_unary_operand_combinations(
    formats_list: List[FormatConfig],
):
    """
    Generate unpack_unary_operand combinations.

    Rules:
    1. When unpacking to dest, UNP_A is used.
    2. When unpacking to dest, transpose is not supported.

    Returns: List of (format, dest_acc, transpose, unpacker_sel) tuples
    """
    combinations = []

    for fmt in formats_list:
        if fmt.input_format != fmt.output_format:
            continue

        if fmt.input_format.is_32_bit():
            dest_acc_modes = [DestAccumulation.Yes]
            transpose_modes = [Transpose.No]
            unpacker_engines = [UnpackerEngine.UNP_A]
        else:
            dest_acc_modes = [DestAccumulation.No, DestAccumulation.Yes]
            transpose_modes = [Transpose.No, Transpose.Yes]
            unpacker_engines = [UnpackerEngine.UNP_A, UnpackerEngine.UNP_B]

        for dest_acc in dest_acc_modes:
            for transpose in transpose_modes:
                for unpacker_sel in unpacker_engines:
                    combinations.extend([(fmt, dest_acc, transpose, unpacker_sel)])

    return combinations


UNPACK_FORMATS = input_output_formats(
    [
        DataFormat.Float16_b,
        # DataFormat.Float16,
        DataFormat.Float32,
    ]
)
DEST_ACC_MODES = [DestAccumulation.No, DestAccumulation.Yes]
ALL_UNPACK_UNARY_OPERAND_COMBINATIONS = generate_unpack_unary_operand_combinations(
    UNPACK_FORMATS
)


@parametrize(
    test_name="quasar_unpack_unary_operand_test",
    formats_dest_acc_and_transpose_unpack_sel=ALL_UNPACK_UNARY_OPERAND_COMBINATIONS,
)
def test_unpack_unary_operand_quasar(
    test_name, formats_dest_acc_and_transpose_unpack_sel, boot_mode=BootMode.DEFAULT
):
    formats = formats_dest_acc_and_transpose_unpack_sel[0]
    dest_acc = formats_dest_acc_and_transpose_unpack_sel[1]
    transpose = formats_dest_acc_and_transpose_unpack_sel[2]
    unpacker_sel = formats_dest_acc_and_transpose_unpack_sel[3]

    if formats.input_format == DataFormat.Float16 and dest_acc == DestAccumulation.Yes:
        pytest.skip("Does not work")

    input_dimensions = [32, 32]

    src_A, src_B, tile_cnt = generate_stimuli(
        formats.input_format,
        formats.input_format,
        input_dimensions=input_dimensions,
    )

    golden_src = src_A if unpacker_sel == UnpackerEngine.UNP_A else src_B
    if transpose == Transpose.Yes:
        generate_golden = get_golden_generator(TransposeGolden)
        golden_tensor = generate_golden.transpose_faces_multi_tile(
            golden_src,
            formats.output_format,
            num_tiles=tile_cnt,
            tilize=False,
            input_dimensions=input_dimensions,
        )
        golden_tensor = generate_golden.transpose_within_faces_multi_tile(
            golden_tensor,
            formats.output_format,
            num_tiles=tile_cnt,
            untilize=False,
            input_dimensions=input_dimensions,
        )
    else:
        generate_golden = get_golden_generator(DataCopyGolden)
        golden_tensor = generate_golden(
            golden_src,
            formats.output_format,
            4,
            input_dimensions,
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
        "unpack_transpose_faces": transpose,
        "unpack_transpose_within_face": transpose,
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
