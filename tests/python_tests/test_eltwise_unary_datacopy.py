# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0

import pytest
import torch

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
            # DataFormat.Float32,
            DataFormat.Bfp8_b,
            DataFormat.Float16,
            # DataFormat.Float16_b,
            # DataFormat.Bfp8_b,
        ]
    ),
    dest_acc=[DestAccumulation.No, DestAccumulation.Yes],
    num_faces=[1, 2, 4],
    # num_faces = 1,
    dest_sync=[DstSync.SyncHalf, DstSync.SyncFull],
    tilize_en=[True, False],
)
def test_unary_datacopy(test_name, formats, dest_acc, num_faces, dest_sync, tilize_en):
    if formats.input_format == DataFormat.Bfp8_b and tilize_en:
        pytest.skip("Unpack Tilize does not support Bfp8_b input format")

    input_dimensions = [32, 32]

    src_A, src_B, tile_cnt = generate_stimuli(
        formats.input_format,
        formats.input_format,
        input_dimensions=input_dimensions,
    )

    if tilize_en:
        generate_golden = get_golden_generator(TilizeGolden)
        golden_tensor = generate_golden(
            src_A, input_dimensions, formats.output_format, num_faces
        )
    else:
        generate_golden = get_golden_generator(DataCopyGolden)
        golden_tensor = generate_golden(
            src_A, formats.output_format, num_faces, input_dimensions
        )

    res_address = write_stimuli_to_l1(
        src_A,
        src_B,
        formats.input_format,
        formats.input_format,
        tile_count=tile_cnt,
        num_faces=num_faces,
    )

    unpack_to_dest = formats.input_format.is_32_bit()
    if formats.input_format == DataFormat.Float32 and tilize_en:
        unpack_to_dest = False

    test_config = {
        "formats": formats,
        "testname": test_name,
        "dest_acc": dest_acc,
        "input_dimensions": input_dimensions,
        "unpack_to_dest": unpack_to_dest,
        "tile_cnt": tile_cnt,
        "num_faces": num_faces,
        "dest_sync": dest_sync,
        "tilize_en": tilize_en,
    }

    run_test(test_config)

    res_from_L1 = collect_results(
        formats, tile_count=tile_cnt, address=res_address, num_faces=num_faces
    )

    assert len(res_from_L1) == len(golden_tensor)

    torch_format = format_dict[formats.output_format]
    res_tensor = torch.tensor(res_from_L1, dtype=torch_format)

    assert passed_test(golden_tensor, res_tensor, formats.output_format)
