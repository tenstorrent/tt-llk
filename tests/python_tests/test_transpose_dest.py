# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0

import pytest
import torch

from helpers.device import (
    collect_results,
    write_stimuli_to_l1,
)
from helpers.format_arg_mapping import DestAccumulation, format_dict
from helpers.format_config import DataFormat
from helpers.golden_generators import (
    DataCopyGolden,
    TransposeGolden,
    get_golden_generator,
)
from helpers.param_config import (
    input_output_formats,
    parametrize,
)
from helpers.stimuli_generator import generate_stimuli
from helpers.test_config import run_test
from helpers.utils import passed_test


@parametrize(
    test_name="transpose_dest_test",
    formats=input_output_formats(
        [
            DataFormat.Int32,
            DataFormat.Float32,
            DataFormat.Float16,
            DataFormat.Float16_b,
            DataFormat.Bfp8_b,
        ]
    ),
    dest_acc=[DestAccumulation.No, DestAccumulation.Yes],
    math_transpose_faces=[True, False],
)
def test_transpose_dest(test_name, formats, dest_acc, math_transpose_faces):

    if formats.input_format.is_32_bit() == False and math_transpose_faces == False:
        pytest.skip("Not supported.")

    if (formats.input_format == DataFormat.Int32) ^ (
        formats.output_format == DataFormat.Int32
    ):
        pytest.skip("Mixing Int32 with other formats is not supported.")

    input_dimensions = [32, 32]

    src_A, src_B, tile_cnt = generate_stimuli(
        formats.input_format,
        formats.input_format,
        input_dimensions=input_dimensions,
    )

    generate_datacopy_golden = get_golden_generator(DataCopyGolden)
    datacopy_tensor = generate_datacopy_golden(src_A, formats.output_format)
    t_matrix = get_golden_generator(TransposeGolden)
    transpose_dest_golden = t_matrix.transpose_faces(
        datacopy_tensor,
        formats.output_format,
        tilize=False,
        input_dimensions=input_dimensions,
    )
    transpose_dest_golden = t_matrix.transpose_within_faces(
        transpose_dest_golden,
        formats.output_format,
        untilize=False,
        input_dimensions=input_dimensions,
    )

    unpack_to_dest = formats.input_format.is_32_bit()

    # <math_transpose_faces=false, is_32bit=true> can be combined with unpack_transpose_faces=true.
    unpack_transpose_faces = (
        formats.input_format.is_32_bit() and not math_transpose_faces
    )

    test_config = {
        "formats": formats,
        "testname": test_name,
        "dest_acc": dest_acc,
        "unpack_to_dest": unpack_to_dest,
        "tile_cnt": tile_cnt,
        "unpack_transpose_faces": unpack_transpose_faces,
        "math_transpose_faces": math_transpose_faces,
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

    assert len(res_from_L1) == len(transpose_dest_golden)

    torch_format = format_dict[formats.output_format]
    res_tensor = torch.tensor(res_from_L1, dtype=torch_format)

    # print(f"GOLDEN: {transpose_dest_golden}")
    # print(f"RESULT: {res_tensor}")

    assert passed_test(transpose_dest_golden, res_tensor, formats.output_format)
