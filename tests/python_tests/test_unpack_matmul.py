# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0

import torch

from helpers.device import (
    collect_results,
    write_stimuli_to_l1,
)
from helpers.format_arg_mapping import (
    DestAccumulation,
    MathFidelity,
    StochasticRnd,
    Transpose,
    format_dict,
)
from helpers.format_config import DataFormat
from helpers.golden_generators import (
    MatmulGolden,
    TilizeGolden,
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
    test_name="unpack_matmul_test",
    formats=input_output_formats(
        [
            DataFormat.Float16_b,
            DataFormat.Float16,
            # DataFormat.Bfp8_b,
            DataFormat.Float32,
        ]
    ),
    dest_acc=[DestAccumulation.No, DestAccumulation.Yes],
    math_fidelity=[
        MathFidelity.LoFi,
        MathFidelity.HiFi2,
        MathFidelity.HiFi3,
        MathFidelity.HiFi4,
    ],
    stochastic_rnd=[
        StochasticRnd.Fpu,
        StochasticRnd.Pack,
        StochasticRnd.All,
        StochasticRnd.No,
    ],
    transpose=[Transpose.No, Transpose.Yes],
)
def test_matmul(test_name, formats, dest_acc, math_fidelity, stochastic_rnd, transpose):

    torch_format = format_dict[formats.output_format]

    input_dimensions = [32, 32]  # Will be sweeping over dimensions

    _, src_B, tile_cnt = generate_stimuli(
        formats.input_format, formats.input_format, input_dimensions=input_dimensions
    )
    tilize_function = get_golden_generator(TilizeGolden)
    src_B = tilize_function(src_B, input_dimensions, formats.input_format).flatten()

    src_B_golden = src_B
    src_A_golden = torch.eye(input_dimensions[0], dtype=torch.float16).flatten()

    if transpose == Transpose.Yes:
        transpose_function = get_golden_generator(TransposeGolden)
        src_B_haloize = transpose_function("within_faces", src_B, formats.input_format)
        src_B_haloize_and_transpose = transpose_function(
            "faces", src_B_haloize, formats.input_format
        )
        src_B_golden = src_B_haloize_and_transpose

    generate_golden = get_golden_generator(MatmulGolden)
    golden_tensor = generate_golden(
        src_A_golden,  # not tilized
        src_B_golden,  # needs to be transposed and tilized
        formats.output_format,
        math_fidelity,
        input_dimensions=input_dimensions,
    )

    src_A = tilize_function(src_A_golden, input_dimensions, formats.input_format)
    res_address = write_stimuli_to_l1(
        src_A,  # tilized
        src_B,  # tilized not transpoed
        formats.input_format,
        formats.input_format,
        tile_count=tile_cnt,
    )

    test_config = {
        "formats": formats,
        "testname": test_name,
        "dest_acc": dest_acc,
        "math_fidelity": math_fidelity,
        "tile_cnt": tile_cnt,
        "input_dimensions": input_dimensions,
        "stochastic_rnd": stochastic_rnd,
        "unpack_transpose_faces": transpose.value,
        "unpack_transpose_within_face": transpose.value,  # matmul transposes both faces and within faces, there is no option for one or the other
    }

    run_test(test_config)

    res_from_L1 = collect_results(formats, tile_count=tile_cnt, address=res_address)
    assert len(res_from_L1) == len(golden_tensor)

    res_tensor = torch.tensor(res_from_L1, dtype=torch_format)

    assert passed_test(golden_tensor, res_tensor, formats.output_format)
