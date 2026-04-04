# SPDX-FileCopyrightText: © 2026 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0

"""
Full fast-tilize test: unpack + math + pack → standard tilized output.

Input: row-major bf16 tensor [height, width].
Expected output: standard tilized tiles (4 faces of 16x16 per tile).
Uses TilizeGolden from the existing test infrastructure.
"""

import pytest
import torch
from helpers.chip_architecture import ChipArchitecture, get_chip_architecture
from helpers.format_config import DataFormat
from helpers.golden_generators import TilizeGolden, get_golden_generator
from helpers.llk_params import DestAccumulation, format_dict
from helpers.param_config import input_output_formats, parametrize
from helpers.stimuli_config import StimuliConfig
from helpers.stimuli_generator import generate_stimuli
from helpers.test_config import TestConfig
from helpers.test_variant_parameters import (
    LOOP_FACTOR,
    NUM_FACES,
    TILE_COUNT,
    generate_input_dim,
)
from helpers.utils import passed_test

TILE_R = 32
TILE_C = 32


@parametrize(
    formats=input_output_formats([DataFormat.Float16_b], same=True),
    dest_acc=[DestAccumulation.No],
    dimensions=[(1, 4), (1, 8), (2, 4)],
)
def test_fast_tilize_full(formats, dest_acc, dimensions, workers_tensix_coordinates):
    if get_chip_architecture() != ChipArchitecture.BLACKHOLE:
        pytest.skip("BH only")

    input_height_tiles, input_width_tiles = dimensions
    assert input_width_tiles % 4 == 0, "ct_dim must be divisible by 4"

    input_dimensions = [input_height_tiles * TILE_R, input_width_tiles * TILE_C]
    tile_count = input_height_tiles * input_width_tiles

    src_A, tile_cnt_A, src_B, tile_cnt_B = generate_stimuli(
        stimuli_format_A=formats.input_format,
        input_dimensions_A=input_dimensions,
        stimuli_format_B=formats.input_format,
        input_dimensions_B=input_dimensions,
    )

    # Standard tilize golden: row-major → tile format
    generate_golden = get_golden_generator(TilizeGolden)
    golden_tensor = generate_golden(src_A, input_dimensions, formats.output_format)

    configuration = TestConfig(
        "sources/fast_tilize_bh_test.cpp",
        formats,
        templates=[],
        runtimes=[
            generate_input_dim(input_dimensions, input_dimensions),
            TILE_COUNT(tile_count),
            LOOP_FACTOR(1),
            NUM_FACES(4),
        ],
        variant_stimuli=StimuliConfig(
            src_A,
            formats.input_format,
            src_B,
            formats.input_format,
            formats.output_format,
            tile_count_A=tile_cnt_A,
            tile_count_B=tile_cnt_B,
            tile_count_res=tile_count,
        ),
        dest_acc=dest_acc,
        compile_time_formats=True,
    )

    res_from_L1 = configuration.run(workers_tensix_coordinates).result

    assert len(res_from_L1) == len(
        golden_tensor
    ), f"Result length {len(res_from_L1)} != golden length {len(golden_tensor)}"

    res_tensor = torch.tensor(res_from_L1, dtype=format_dict[formats.output_format])

    # Dump output mapping for debugging pack
    FACE_C = 16
    src_2d = src_A.reshape(input_height_tiles * TILE_R, input_width_tiles * TILE_C)
    input_rows = {}
    for r in range(input_height_tiles * TILE_R):
        for cg in range(input_width_tiles * TILE_C // FACE_C):
            key = tuple(
                round(v, 3) for v in src_2d[r, cg * FACE_C : (cg + 1) * FACE_C].tolist()
            )
            input_rows[key] = (r, cg)

    print(f"\nOutput: {len(res_tensor)} datums = {len(res_tensor)//FACE_C} rows")
    for out_row in range(min(128, len(res_tensor) // FACE_C)):
        start = out_row * FACE_C
        row_data = tuple(
            round(v, 3) for v in res_tensor[start : start + FACE_C].tolist()
        )
        match = input_rows.get(row_data)
        all_zero = all(abs(v) < 1e-6 for v in row_data)
        tile = out_row // (4 * 16)  # tile = every 64 rows
        face = (out_row % 64) // 16
        fr = out_row % 16
        label = (
            f"input[{match[0]},cols {match[1]*16}:{match[1]*16+15}]"
            if match
            else ("ZERO" if all_zero else "???")
        )
        print(f"  row {out_row:3d} (tile {tile} face {face} r{fr:2d}): {label}")

    print()
    assert passed_test(
        golden_tensor, res_tensor, formats.output_format
    ), "Output doesn't match TilizeGolden"
