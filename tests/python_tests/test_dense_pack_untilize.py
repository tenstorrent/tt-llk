# SPDX-FileCopyrightText: © 2026 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0

import torch
from conftest import skip_for_wormhole
from helpers.format_config import DataFormat
from helpers.llk_params import DestAccumulation, DestSync, format_dict
from helpers.param_config import (
    input_output_formats,
    parametrize,
)
from helpers.stimuli_config import StimuliConfig
from helpers.stimuli_generator import generate_stimuli
from helpers.test_config import TestConfig
from helpers.test_variant_parameters import (
    DEST_SYNC,
    INPUT_DIMENSIONS,
    NUM_FACES,
    TILE_COUNT,
)
from helpers.utils import passed_test


@skip_for_wormhole
@parametrize(
    formats=input_output_formats([DataFormat.Float16_b]),
    dest_acc=[DestAccumulation.No],
    input_dimensions=[[32, 256]],
    dest_sync=[DestSync.Half],
)
def test_pack_untilize(
    formats, dest_acc, input_dimensions, dest_sync, workers_tensix_coordinates
):

    src_A, tile_cnt_A, src_B, tile_cnt_B = generate_stimuli(
        stimuli_format_A=formats.input_format,
        input_dimensions_A=input_dimensions,
        stimuli_format_B=formats.input_format,
        input_dimensions_B=input_dimensions,
        sfpu=False,
    )

    valid_datum = 1.0
    invalid_datum = -1.0
    row_offset = 512
    face_offset = 16
    num_faces = (32 * 256) // (16 * 16)
    src_A = []
    for i in range(num_faces):
        for j in range(8):
            for k in range(16):
                src_A.append(valid_datum * (row_offset * j + face_offset * i + k))
        for j in range(8):
            for k in range(16):
                src_A.append(invalid_datum * (row_offset * j + face_offset * i + k))

    golden = []
    row_offset = 16
    block_offset = 16 * 16
    for i in range(8):
        for j in range(512 // 16):
            for k in range(16):
                golden.append(src_A[i * row_offset + j * block_offset + k])

    print("SrcA")
    for i in range(0, len(src_A), 16):
        row = (i // 16) % 16
        face = (i // 256) % 4
        tile = i // 1024
        print(f"T{tile}F{face}R{row}\t", src_A[i : i + 16])
    print("Golden")
    for i in range(0, len(golden), 16):
        segment = (i // 16) % 32
        row = i // 512
        print(f"R{row}S{segment}\t", golden[i : i + 16])

    src_A = torch.tensor(src_A, dtype=format_dict[formats.input_format])
    golden_tensor = torch.tensor(golden, dtype=format_dict[formats.output_format])

    configuration = TestConfig(
        "sources/dense_pack_untilize_test.cpp",
        formats,
        templates=[
            INPUT_DIMENSIONS(
                input_dimensions,
                input_dimensions,
                8,
            ),
            DEST_SYNC(dest_sync),
        ],
        runtimes=[TILE_COUNT(tile_cnt_A), NUM_FACES(4)],
        variant_stimuli=StimuliConfig(
            src_A,
            formats.input_format,
            src_B,
            formats.input_format,
            formats.output_format,
            tile_count_A=tile_cnt_A,
            tile_count_B=tile_cnt_B,
            tile_count_res=tile_cnt_A,
            sfpu=False,
        ),
        dest_acc=dest_acc,
        unpack_to_dest=False,
    )

    res_from_L1 = configuration.run(workers_tensix_coordinates)
    res_from_L1 = res_from_L1[0 : 8 * 512]

    assert len(res_from_L1) == len(
        golden_tensor
    ), "Result tensor and golden tensor are not of the same length"

    res_tensor = torch.tensor(res_from_L1, dtype=format_dict[formats.output_format])

    assert passed_test(
        golden_tensor, res_tensor, formats.output_format
    ), "Assert against golden failed"
