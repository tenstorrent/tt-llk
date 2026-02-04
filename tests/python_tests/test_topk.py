# test_topk.py
# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0

import pytest
import torch
from helpers.format_config import DataFormat, InputOutputFormat
from helpers.golden_generators import (
    TilizeGolden,
    TransposeGolden,
    get_golden_generator,
)
from helpers.llk_params import DestAccumulation, format_dict
from helpers.stimuli_config import StimuliConfig
from helpers.stimuli_generator import generate_stimuli
from helpers.test_config import TestConfig
from helpers.test_variant_parameters import INPUT_DIMENSIONS, TILE_COUNT


@pytest.mark.parametrize(
    "formats",
    [
        InputOutputFormat(DataFormat.Float16_b, DataFormat.Float16_b),
    ],
)
def test_topk_sfpu(formats: InputOutputFormat, workers_tensix_coordinates: str):
    # torch.manual_seed(0)
    # torch.set_printoptions(precision=5, linewidth=200, sci_mode=False)

    input_dimensions = [32, 128]

    src_A, tile_cnt_A, src_B, tile_cnt_B = generate_stimuli(
        stimuli_format_A=formats.input_format,
        input_dimensions_A=input_dimensions,
        stimuli_format_B=formats.input_format,
        input_dimensions_B=input_dimensions,
        sfpu=False,
    )

    src_A[0:64] = torch.arange(
        1, 65, dtype=src_A.dtype
    )  # Set first 64 elements to 1,2,3,...,64
    src_A[64:128] = torch.arange(
        65, 129, dtype=src_A.dtype
    )  # Set next 64 elements to 65,66,67,...,128
    src_A[128:4096] = 0.0
    src_A
    generate_golden = get_golden_generator(TilizeGolden)
    golden_tensor = generate_golden(src_A, input_dimensions, formats.output_format)

    # 3) Run kernel (TopK LLK math kernel under test)
    configuration = TestConfig(
        test_name="sources/topk_test.cpp",
        formats=formats,
        templates=[
            INPUT_DIMENSIONS(input_dimensions, input_dimensions),
        ],
        runtimes=[
            TILE_COUNT(tile_cnt_A),
        ],
        variant_stimuli=StimuliConfig(
            golden_tensor,
            formats.input_format,
            src_B,
            formats.input_format,
            formats.output_format,
            tile_count_A=tile_cnt_A,
            tile_count_B=tile_cnt_B,
            tile_count_res=tile_cnt_A,
        ),
        dest_acc=DestAccumulation.No,
        unpack_to_dest=False,
    )

    res_from_L1 = configuration.run(workers_tensix_coordinates)
    res_tensor = torch.tensor(res_from_L1, dtype=format_dict[formats.output_format])

    # Print face-by-face comparison for all tiles
    print("\n" + "=" * 80)
    print("FACE-BY-FACE COMPARISON FOR ALL TILES")
    print("=" * 80)

    num_tiles = len(res_tensor) // 1024  # Each tile is 1024 elements (32x32)

    for tile_idx in range(num_tiles):
        print(f"\n{'#'*80}")
        print(f"# TILE {tile_idx}")
        print(f"{'#'*80}")

        for face_idx in range(
            4
        ):  # 4 faces per tile (32x32 tile = 1024 elements = 4 faces of 256 elements)
            tile_offset = tile_idx * 1024
            start_idx = tile_offset + face_idx * 256
            end_idx = start_idx + 256

            print(f"\n{'='*80}")
            print(
                f"TILE {tile_idx} - FACE {face_idx} (elements {start_idx}-{end_idx-1})"
            )
            print(f"{'='*80}")

            # Result face
            result_face = res_tensor[start_idx:end_idx].view(16, 16)
            print(f"\nRESULT Tile {tile_idx} Face {face_idx}:")
            for i in range(16):
                print(" ".join(f"{x:6.2f}" for x in result_face[i].tolist()))

            # Golden face
            golden_face = golden_tensor[start_idx:end_idx].view(16, 16)
            print(f"\nGOLDEN Tile {tile_idx} Face {face_idx}:")
            for i in range(16):
                print(" ".join(f"{x:6.2f}" for x in golden_face[i].tolist()))

    print("\n" + "=" * 80 + "\n")

    # Compute golden using proper transpose generator that understands tilized data
    transpose_golden = get_golden_generator(TransposeGolden)

    # Apply transpose to srcA: hardware does transpose_faces then transpose_within_faces
    src_A_transposed = transpose_golden.transpose_faces_multi_tile(
        src_A,
        formats.input_format,
        num_tiles=tile_cnt_A,
        tilize=True,  # Tilize before transpose (models hardware behavior)
        input_dimensions=input_dimensions,
    )

    # test_passed = passed_test(
    #     golden_tensor, res_tensor, formats.output_format, print_erros=True
    # )
