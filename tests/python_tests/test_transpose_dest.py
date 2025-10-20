# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0

import torch

from helpers.device import (
    collect_results,
    write_stimuli_to_l1,
)
from helpers.format_arg_mapping import DestAccumulation, Transpose, format_dict
from helpers.format_config import DataFormat
from helpers.golden_generators import (
    TransposeGolden,
    get_golden_generator,
)
from helpers.param_config import (
    generate_transpose_dest_combinations,
    input_output_formats,
    parametrize,
)
from helpers.stimuli_generator import generate_stimuli
from helpers.test_config import run_test
from helpers.utils import passed_test

TRANSPOSE_DEST_FLOAT_FORMATS = input_output_formats(
    [
        DataFormat.Float32,
        DataFormat.Float16,
        DataFormat.Float16_b,
        DataFormat.Bfp8_b,
    ]
)


@parametrize(
    test_name="transpose_dest_test",
    fmt_dest_acc_math_transp_unpack_to_dest=generate_transpose_dest_combinations(
        TRANSPOSE_DEST_FLOAT_FORMATS
    ),
)
def test_transpose_dest_float(test_name, fmt_dest_acc_math_transp_unpack_to_dest):

    transpose_dest(
        test_name,
        formats=fmt_dest_acc_math_transp_unpack_to_dest[0],
        dest_acc=fmt_dest_acc_math_transp_unpack_to_dest[1],
        math_transpose_faces=fmt_dest_acc_math_transp_unpack_to_dest[2],
        unpack_to_dest=fmt_dest_acc_math_transp_unpack_to_dest[3],
    )


@parametrize(
    test_name="transpose_dest_test",
    formats=input_output_formats([DataFormat.Int32]),
    dest_acc=[DestAccumulation.Yes],
    math_transpose_faces=[True, False],
    unpack_to_dest=[True],
)
def test_transpose_dest_int(
    test_name, formats, dest_acc, math_transpose_faces, unpack_to_dest
):
    transpose_dest(test_name, formats, dest_acc, math_transpose_faces, unpack_to_dest)


def transpose_dest(test_name, formats, dest_acc, math_transpose_faces, unpack_to_dest):

    input_dimensions = [64, 64]

    src_A, src_B, tile_cnt = generate_stimuli(
        formats.input_format,
        formats.input_format,
        input_dimensions=input_dimensions,
    )

    t_matrix = get_golden_generator(TransposeGolden)
    golden_tensor = t_matrix.transpose_faces(
        src_A,
        formats.output_format,
        tilize=False,
        input_dimensions=input_dimensions,
    )
    golden_tensor = t_matrix.transpose_within_faces(
        golden_tensor,
        formats.output_format,
        untilize=False,
        input_dimensions=input_dimensions,
    )

    # <math_transpose_faces=false, is_32bit=true> can be combined with unpack_transpose_faces=true.
    unpack_transpose_faces = (
        Transpose.Yes.value
        if (unpack_to_dest and not math_transpose_faces)
        else Transpose.No.value
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

    assert len(res_from_L1) == len(golden_tensor)

    torch_format = format_dict[formats.output_format]
    res_tensor = torch.tensor(res_from_L1, dtype=torch_format)

    assert passed_test(golden_tensor, res_tensor, formats.output_format)
