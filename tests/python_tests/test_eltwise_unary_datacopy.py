# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0

import torch
from helpers.format_config import DataFormat
from helpers.golden_generators import DataCopyGolden, TilizeGolden, get_golden_generator
from helpers.llk_params import DestAccumulation, Tilize, format_dict
from helpers.param_config import (
    generate_tilize_aware_datacopy_combinations,
    input_output_formats,
    parametrize,
)
from helpers.stimuli_config import StimuliConfig
from helpers.stimuli_generator import generate_stimuli
from helpers.test_config import TestConfig
from helpers.test_variant_parameters import DEST_INDEX, INPUT_DIMENSIONS, TILIZE
from helpers.utils import passed_test

DATACOPY_FORMATS = input_output_formats(
    [
        DataFormat.Float32,
        DataFormat.Float16,
        DataFormat.Float16_b,
        DataFormat.Bfp8_b,
    ]
)


@parametrize(
    test_name="eltwise_unary_datacopy_test",
    datacopy_parameters=generate_tilize_aware_datacopy_combinations(
        DATACOPY_FORMATS, result_tiles=4
    ),
)
def test_unary_datacopy(test_name, datacopy_parameters, workers_tensix_coordinates):

    input_dimensions = [64, 64]

    formats = datacopy_parameters[0]
    dest_acc = datacopy_parameters[1]
    num_faces = datacopy_parameters[2]
    tilize_en = datacopy_parameters[3]
    dest_index = datacopy_parameters[4]

    src_A, tile_cnt_A, src_B, tile_cnt_B = generate_stimuli(
        stimuli_format_A=formats.input_format,
        input_dimensions_A=input_dimensions,
        stimuli_format_B=formats.input_format,
        input_dimensions_B=input_dimensions,
    )

    if tilize_en == Tilize.No:
        generate_golden = get_golden_generator(DataCopyGolden)
        golden_tensor = generate_golden(
            src_A, formats.output_format, num_faces, input_dimensions
        )
    else:
        generate_golden = get_golden_generator(TilizeGolden)
        golden_tensor = generate_golden(src_A, input_dimensions, formats.output_format)

    unpack_to_dest = (
        False
        if tilize_en == Tilize.Yes and formats.input_format == DataFormat.Float32
        else formats.input_format.is_32_bit() and dest_acc == DestAccumulation.Yes
    )

    configuration = TestConfig(
        test_name,
        formats,
        templates=[
            INPUT_DIMENSIONS(input_dimensions, input_dimensions),
            TILIZE(tilize_en),
        ],
        runtimes=[
            DEST_INDEX(dest_index),
        ],
        variant_stimuli=StimuliConfig(
            src_A,
            formats.input_format,
            src_B,
            formats.input_format,
            formats.output_format,
            tile_count_A=tile_cnt_A,
            tile_count_B=tile_cnt_B,
            tile_count_res=tile_cnt_A,
            num_faces=num_faces,
        ),
        dest_acc=dest_acc,
        unpack_to_dest=unpack_to_dest,
    )

    res_from_L1 = configuration.run(workers_tensix_coordinates, delete_artefacts=False)

    assert len(res_from_L1) == len(golden_tensor)

    torch_format = format_dict[formats.output_format]
    res_tensor = torch.tensor(res_from_L1, dtype=torch_format)

    assert passed_test(golden_tensor, res_tensor, formats.output_format)
