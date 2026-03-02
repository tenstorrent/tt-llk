# SPDX-FileCopyrightText: © 2026 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0

import pytest
import torch
from helpers.format_config import DataFormat
from helpers.llk_params import DestAccumulation, PerfRunType, Tilize, format_dict
from helpers.param_config import (
    input_output_formats,
    parametrize,
)
from helpers.perf import PerfConfig
from helpers.stimuli_config import StimuliConfig
from helpers.test_variant_parameters import (
    DEST_INDEX,
    LOOP_FACTOR,
    NUM_FACES,
    TILE_COUNT,
    TILIZE,
    generate_input_dim,
)


@pytest.mark.perf
@parametrize(
    formats=input_output_formats(
        [
            DataFormat.Float16_b,
        ]
    ),
    dest_acc=[DestAccumulation.No],
    num_faces=[4],
    tilize=[Tilize.No],
    dest_index=0,
    loop_factor=[1, 16, 32, 64, 128, 256],
)
def test_perf_custom_pack_w_acc(
    perf_report,
    formats,
    dest_acc,
    num_faces,
    tilize,
    dest_index,
    loop_factor,
    workers_tensix_coordinates,
):
    input_dimensions = [64, 128]
    tile_cnt = (input_dimensions[0] // 32) * (input_dimensions[1] // 32)

    src_A = (
        torch.ones(
            input_dimensions[0] * input_dimensions[1],
            dtype=format_dict[formats.input_format],
        )
        * 3
    )

    src_B = torch.zeros_like(src_A)

    unpack_to_dest = (
        False
        if tilize == Tilize.Yes and formats.input_format == DataFormat.Float32
        else formats.input_format.is_32_bit() and dest_acc == DestAccumulation.Yes
    )

    configuration = PerfConfig(
        "sources/custom_pack_w_acc_perf.cpp",
        formats,
        run_types=[
            PerfRunType.PACK_ISOLATE,
        ],
        templates=[
            generate_input_dim(input_dimensions, input_dimensions),
            TILIZE(tilize),
            LOOP_FACTOR(loop_factor),
        ],
        runtimes=[
            DEST_INDEX(0),
            TILE_COUNT(8),
            NUM_FACES(4),
        ],
        variant_stimuli=StimuliConfig(
            src_A,
            formats.input_format,
            src_B,
            formats.input_format,
            formats.output_format,
            tile_count_A=8,
            tile_count_B=8,
            tile_count_res=8,
            num_faces=4,
        ),
        dest_acc=dest_acc,
        unpack_to_dest=unpack_to_dest,
    )

    configuration.run(perf_report, location=workers_tensix_coordinates)
