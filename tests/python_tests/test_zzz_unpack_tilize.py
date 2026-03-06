# SPDX-FileCopyrightText: © 2026 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0
import pytest
import torch
from helpers.format_config import DataFormat
from helpers.golden_generators import TilizeGolden, get_golden_generator
from helpers.llk_params import DestAccumulation, format_dict
from helpers.param_config import input_output_formats, parametrize
from helpers.stimuli_config import StimuliConfig
from helpers.stimuli_generator import generate_stimuli
from helpers.test_config import TestConfig
from helpers.test_variant_parameters import (
    TILE_COUNT,
    generate_input_dim,
)
from helpers.utils import passed_test


@parametrize(
    formats=input_output_formats(
        [
            DataFormat.Float16_b,
            DataFormat.Float16,
            DataFormat.Float32,
            DataFormat.Bfp8_b,  # Unpack Tilize doesn't work for block float formats (Bfp8_b) due to shared exponent at start of input tensor
        ]
    ),
)
def test_unpack_tilize_float(formats, workers_tensix_coordinates):
    formats = formats[0]
    if formats.input_format == DataFormat.Bfp8_b:
        pytest.skip("Unpack Tilize does not support Bfp8_b input format")

    unpack_tilize(formats, workers_tensix_coordinates)


@parametrize(
    formats=input_output_formats([DataFormat.Float32], same=True),
    dest_acc=[DestAccumulation.Yes],
)
def test_unpack_tilize_float32_lossless(formats, dest_acc, workers_tensix_coordinates):
    unpack_tilize(
        formats,
        workers_tensix_coordinates,
        unpack_to_dest=True,
        validate_lossless=True,
        dest_acc=dest_acc,
    )


@parametrize(formats=input_output_formats([DataFormat.Int32]))
def test_unpack_tilize_int(formats, workers_tensix_coordinates):
    formats = formats[0]
    unpack_tilize(formats, workers_tensix_coordinates, unpack_to_dest=True)


@parametrize(formats=input_output_formats([DataFormat.Int8]))
def test_unpack_tilize_int8(formats, workers_tensix_coordinates):
    formats = formats[0]
    unpack_tilize(
        formats,
        workers_tensix_coordinates,
        unpack_to_dest=False,
        dest_acc=DestAccumulation.Yes,
    )


def unpack_tilize(
    formats,
    workers_tensix_coordinates,
    unpack_to_dest=False,
    validate_lossless=False,
    dest_acc=None,
):
    input_dimensions = [32, 64]
    src_A, tile_cnt_A, src_B, tile_cnt_B = generate_stimuli(
        stimuli_format_A=formats.input_format,
        input_dimensions_A=input_dimensions,
        stimuli_format_B=formats.input_format,
        input_dimensions_B=input_dimensions,
    )

    generate_golden = get_golden_generator(TilizeGolden)
    golden_tensor = generate_golden(src_A, input_dimensions, formats.output_format)

    configuration = TestConfig(
        "sources/unpack_tilize_test.cpp",
        formats,
        templates=[],
        runtimes=[
            generate_input_dim(input_dimensions, input_dimensions),
            TILE_COUNT(tile_cnt_A),
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
        ),
        unpack_to_dest=unpack_to_dest,
        **({"dest_acc": dest_acc} if dest_acc is not None else {}),
    )

    res_from_L1 = configuration.run(workers_tensix_coordinates).result

    assert len(res_from_L1) == len(
        golden_tensor
    ), "Result tensor and golden tensor are not of the same length"

    res_tensor = torch.tensor(res_from_L1, dtype=format_dict[formats.output_format])

    # Print face-by-face comparison (16x16 datums per face, 4 faces per 32x32 tile)
    print("\n=== Face-by-Face Comparison (16x16 datums each) ===")
    is_int_format = formats.output_format in [DataFormat.Int8, DataFormat.Int32]
    for tile_idx in range(len(golden_tensor) // 1024):  # 1024 = 32x32
        print(f"\n--- Tile {tile_idx} ---")
        for face_idx in range(4):  # 4 faces per tile
            # Each face is 16x16 = 256 datums
            # Face layout: face 0 (top-left), face 1 (top-right), face 2 (bottom-left), face 3 (bottom-right)
            face_offset = tile_idx * 1024 + face_idx * 256

            print(f"\n  Face {face_idx} - RESULT:")
            for row in range(16):
                row_start = face_offset + row * 16
                row_data = res_tensor[row_start : row_start + 16]
                if is_int_format:
                    print(
                        f"    Row {row:2d}: {' '.join(f'{int(val):4d}' for val in row_data.tolist())}"
                    )
                else:
                    print(
                        f"    Row {row:2d}: {' '.join(f'{val:6.2f}' for val in row_data.tolist())}"
                    )

            # print(f"\n  Face {face_idx} - L1:")
            # for row in range(16):
            #     row_start = face_offset + row * 16
            #     row_data = src_A[row_start:row_start + 16]
            #     if is_int_format:
            #         print(f"    Row {row:2d}: {' '.join(f'{int(val):4d}' for val in row_data.tolist())}")
            #     else:
            #         print(f"    Row {row:2d}: {' '.join(f'{val:6.2f}' for val in row_data.tolist())}")

            print(f"\n  Face {face_idx} - GOLDEN:")
            for row in range(16):
                row_start = face_offset + row * 16
                row_data = golden_tensor[row_start : row_start + 16]
                if is_int_format:
                    print(
                        f"    Row {row:2d}: {' '.join(f'{int(val):4d}' for val in row_data.tolist())}"
                    )
                else:
                    print(
                        f"    Row {row:2d}: {' '.join(f'{val:6.2f}' for val in row_data.tolist())}"
                    )

    if validate_lossless:
        # Lossless validation
        diff = golden_tensor - res_tensor
        abs_diff = diff.abs()
        assert torch.allclose(golden_tensor, res_tensor, atol=0, rtol=1e-6), (
            f"Float32 tilize lost precision! Input and output differ.\n"
            f"Max difference: {abs_diff.max().item()}\n"
            f"Num different elements: {(abs_diff > 1e-6).sum()}\n"
            f"Expected (golden): {golden_tensor[:10]}\n"
            f"Got (result): {res_tensor[:10]}"
        )
    else:
        # Standard validation with relaxed tolerances
        assert passed_test(
            golden_tensor, res_tensor, formats.output_format, print_errors=False
        ), "Assert against golden failed"
