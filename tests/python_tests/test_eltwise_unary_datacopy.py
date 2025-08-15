# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0

import pytest
import torch

from helpers.chip_architecture import ChipArchitecture, get_chip_architecture
from helpers.device import (
    collect_results,
    write_stimuli_to_l1,
)
from helpers.format_arg_mapping import DestAccumulation, DestSync, format_dict
from helpers.format_config import DataFormat
from helpers.golden_generators import DataCopyGolden, TilizeGolden, get_golden_generator
from helpers.param_config import (
    input_output_formats,
    parametrize,
)
from helpers.stimuli_generator import generate_stimuli
from helpers.test_config import run_test
from helpers.utils import passed_test


@parametrize(
    test_name="eltwise_unary_datacopy_test",
    formats=input_output_formats(
        [
            DataFormat.Float32,
            DataFormat.Float16,
            DataFormat.Float16_b,
            DataFormat.Bfp8_b,
        ]
    ),
    dest_acc=[DestAccumulation.Yes, DestAccumulation.No],
    num_faces=[1, 2, 4],
    dest_sync=[DestSync.Half, DestSync.Full],
    tilize_en=[False, True],
)
def test_unary_datacopy(test_name, formats, dest_acc, num_faces, dest_sync, tilize_en):
    if get_chip_architecture() == ChipArchitecture.WORMHOLE and tilize_en:
        pytest.skip("Datacopy has no tilize argument for WORMHOLE")

    if num_faces != 4 and tilize_en:
        pytest.skip("Pack does not support less than 4 faces when tilize = true")

    if formats.input_format == DataFormat.Bfp8_b and tilize_en:
        pytest.skip("Unpack Tilize does not support Bfp8_b input format")

    unary_datacopy(test_name, formats, dest_acc, num_faces, dest_sync, tilize_en)


def unary_datacopy(test_name, formats, dest_acc, num_faces, dest_sync, tilize_en):

    input_dimensions = [64, 64]

    src_A, src_B, tile_cnt = generate_stimuli(
        formats.input_format,
        formats.input_format,
        input_dimensions=input_dimensions,
    )

    if not tilize_en:
        generate_golden = get_golden_generator(DataCopyGolden)
        golden_tensor = generate_golden(
            src_A, formats.output_format, num_faces, input_dimensions
        )
    else:
        generate_golden = get_golden_generator(TilizeGolden)
        golden_tensor = generate_golden(src_A, input_dimensions, formats.output_format)

    unpack_to_dest = (
        False
        if tilize_en and formats.input_format == DataFormat.Float32
        else formats.input_format.is_32_bit() and dest_acc == DestAccumulation.Yes
    )

    test_config = {
        "formats": formats,
        "testname": test_name,
        "dest_acc": dest_acc,
        "input_A_dimensions": input_dimensions,
        "input_B_dimensions": input_dimensions,
        "unpack_to_dest": unpack_to_dest,
        "tile_cnt": tile_cnt,
        "num_faces": num_faces,
        "dest_sync": dest_sync,
        "tilize": tilize_en,
    }

    res_address = write_stimuli_to_l1(
        test_config,
        src_A,
        src_B,
        formats.input_format,
        formats.input_format,
        tile_count_A=tile_cnt,
        tile_count_B=tile_cnt,
        num_faces=num_faces,
    )

    run_test(test_config)

    res_from_L1 = collect_results(
        formats, tile_count=tile_cnt, address=res_address, num_faces=num_faces
    )

    assert len(res_from_L1) == len(golden_tensor)

    torch_format = format_dict[formats.output_format]
    res_tensor = torch.tensor(res_from_L1, dtype=torch_format)

    assert passed_test(golden_tensor, res_tensor, formats.output_format)
