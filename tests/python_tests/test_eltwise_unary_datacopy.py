# SPDX-FileCopyrightText: © 2026 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0

import pytest
import torch
from helpers.chip_architecture import ChipArchitecture, get_chip_architecture
from helpers.constraints import (
    get_valid_dest_accumulation_modes,
)
from helpers.format_config import DataFormat
from helpers.golden_generators import (
    TILE_DIMENSIONS,
    DataCopyGolden,
    TilizeGolden,
    get_golden_generator,
)
from helpers.llk_params import (
    BlocksCalculationAlgorithm,
    DestAccumulation,
    DestSync,
    Tilize,
    format_dict,
)
from helpers.param_config import (
    get_num_blocks_and_num_tiles_in_block,
    input_output_formats,
    parametrize,
)
from helpers.stimuli_config import StimuliConfig
from helpers.stimuli_generator import generate_stimuli
from helpers.test_config import TestConfig
from helpers.test_variant_parameters import (
    DEST_INDEX,
    NUM_BLOCKS,
    NUM_FACES,
    NUM_TILES_IN_BLOCK,
    TILE_COUNT,
    TILIZE,
    generate_input_dim,
)
from helpers.utils import passed_test


def get_valid_tilize_datacopy(formats):
    """
    Get valid tilize options for _llk_math_eltwise_unary_datacopy_

    - Blackhole and Wormhole have differing APIs:
        - Blackhole: Has tilize argument (SW workaround for HW bug)
        - Wormhole: No tilize argument (No SW workaround needed)
    - Tilize cannot be enabled if input format is Bfp8_b (HW limitation)

    Therefore we only test tilization on Blackhole
    """

    chip_arch = get_chip_architecture()

    if chip_arch == ChipArchitecture.WORMHOLE:
        return [Tilize.No]

    if formats.input_format == DataFormat.Bfp8_b:
        return [Tilize.No]

    return [Tilize.No, Tilize.Yes]


def get_valid_num_faces_datacopy(tilize):
    """
    Get valid num_faces options for _llk_math_eltwise_unary_datacopy_

    - Number of faces must be 4 when tilization is enabled (SW limitation)
    - Otherwise num_faces can be 1, 2, or 4
    """

    if tilize == Tilize.Yes:
        return [4]

    return [1, 2, 4]


@parametrize(
    formats=input_output_formats(
        [
            DataFormat.Float32,
            DataFormat.Float16,
            DataFormat.Float16_b,
            DataFormat.Bfp8_b,
            DataFormat.Fp8_e4m3,
        ]
    ),
    dest_acc=lambda formats: get_valid_dest_accumulation_modes(formats),
    num_faces=lambda tilize: get_valid_num_faces_datacopy(tilize),
    tilize=lambda formats: get_valid_tilize_datacopy(formats),
    input_dimensions=[[64, 64], [32, 256], [128, 256]],
)
def test_unary_datacopy(
    formats, dest_acc, num_faces, tilize, input_dimensions, workers_tensix_coordinates
):

    # skip if Fp8_e4m3 for wormhole
    if get_chip_architecture() == ChipArchitecture.WORMHOLE and (
        formats.input_format == DataFormat.Fp8_e4m3
        or formats.output_format == DataFormat.Fp8_e4m3
    ):
        pytest.skip("Fp8_e4m3 not supported on wormhole")

    src_A, tile_cnt_A, src_B, tile_cnt_B = generate_stimuli(
        stimuli_format_A=formats.input_format,
        input_dimensions_A=input_dimensions,
        stimuli_format_B=formats.input_format,
        input_dimensions_B=input_dimensions,
    )

    if tilize == Tilize.No:
        generate_golden = get_golden_generator(DataCopyGolden)
        golden_tensor = generate_golden(
            src_A, formats.output_format, num_faces, input_dimensions
        )
    else:
        generate_golden = get_golden_generator(TilizeGolden)
        golden_tensor = generate_golden(src_A, input_dimensions, formats.output_format)

    unpack_to_dest = (
        False
        if tilize == Tilize.Yes and formats.input_format == DataFormat.Float32
        else formats.input_format.is_32_bit() and dest_acc == DestAccumulation.Yes
    )

    blocks_calculation_algorithm = (
        BlocksCalculationAlgorithm.Standard
        if tilize == Tilize.No
        else BlocksCalculationAlgorithm.Tilize
    )
    num_blocks, num_tiles_in_block = get_num_blocks_and_num_tiles_in_block(
        DestSync.Half,
        dest_acc,
        formats,
        input_dimensions,
        TILE_DIMENSIONS,
        blocks_calculation_algorithm,
    )

    configuration = TestConfig(
        "sources/eltwise_unary_datacopy_test.cpp",
        formats,
        templates=[
            generate_input_dim(input_dimensions, input_dimensions),
            TILIZE(tilize),
        ],
        runtimes=[
            DEST_INDEX(0),
            TILE_COUNT(tile_cnt_A),
            NUM_FACES(num_faces),
            NUM_BLOCKS(num_blocks),
            NUM_TILES_IN_BLOCK(num_tiles_in_block),
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

    res_from_L1 = configuration.run(workers_tensix_coordinates).result

    assert len(res_from_L1) == len(golden_tensor)

    torch_format = format_dict[formats.output_format]
    res_tensor = torch.tensor(res_from_L1, dtype=torch_format)

    assert passed_test(golden_tensor, res_tensor, formats.output_format)


FP8_FACE_VALUES = [
    0.125,
    0.25,
    0.5,
    1.0,
    2.0,
    4.0,
    8.0,
    16.0,
    32.0,
    64.0,
    128.0,
    0.0625,
    0.03125,
    0.015625,
    3.0,
    6.0,
]


def _build_fp8_tilize_stimuli(input_dimensions):
    """Build row-major stimuli with a unique FP8-exact value per face quadrant.

    Each 16x16 face within each tile gets a distinct value from FP8_FACE_VALUES
    so that any face-offset bug is immediately visible in the output.

    Face layout within a tile (32x32):
      Face 0: rows 0-15,  cols 0-15   Face 1: rows 0-15,  cols 16-31
      Face 2: rows 16-31, cols 0-15   Face 3: rows 16-31, cols 16-31
    """
    rows, cols = input_dimensions
    num_tile_cols = cols // 32
    total_faces = num_tile_cols * 4
    assert total_faces <= len(
        FP8_FACE_VALUES
    ), f"Need {total_faces} unique values but only have {len(FP8_FACE_VALUES)}"

    data = torch.empty(rows * cols, dtype=torch.bfloat16)
    for r in range(rows):
        face_row = 0 if r < 16 else 1
        for c in range(cols):
            tile_col = c // 32
            face_col = 0 if (c % 32) < 16 else 1
            face_idx = face_row * 2 + face_col
            global_idx = tile_col * 4 + face_idx
            data[r * cols + c] = FP8_FACE_VALUES[global_idx]
    return data


@pytest.mark.parametrize("input_dimensions", [[32, 32], [32, 64], [32, 128]])
def test_unary_datacopy_fp8_tilize_debug(input_dimensions, workers_tensix_coordinates):
    """FP8 tilize debug test with unique value per face quadrant.

    Detects HW tileize_mode inter-face column offset bugs for 1-byte formats.
    Each 16x16 face in every tile gets a distinct FP8-exact value so that any
    face reading from the wrong L1 address is immediately apparent.
    """
    from helpers.param_config import InputOutputFormat

    input_format = DataFormat.Fp8_e4m3
    output_format = DataFormat.Fp8_e4m3
    formats = InputOutputFormat(input_format, output_format)
    tilize = Tilize.Yes
    num_faces = 4
    dest_acc = DestAccumulation.No

    if get_chip_architecture() == ChipArchitecture.WORMHOLE:
        pytest.skip("Fp8_e4m3 not supported on wormhole")

    src_A = _build_fp8_tilize_stimuli(input_dimensions)
    num_tile_cols = input_dimensions[1] // 32
    tile_cnt_A = num_tile_cols
    src_B = src_A.clone()
    tile_cnt_B = tile_cnt_A

    generate_golden = get_golden_generator(TilizeGolden)
    golden_tensor = generate_golden(src_A, input_dimensions, output_format)

    unpack_to_dest = False

    num_blocks, num_tiles_in_block = get_num_blocks_and_num_tiles_in_block(
        DestSync.Half,
        dest_acc,
        formats,
        input_dimensions,
        TILE_DIMENSIONS,
        BlocksCalculationAlgorithm.Tilize,
    )

    configuration = TestConfig(
        "sources/eltwise_unary_datacopy_test.cpp",
        formats,
        templates=[
            generate_input_dim(input_dimensions, input_dimensions),
            TILIZE(tilize),
        ],
        runtimes=[
            DEST_INDEX(0),
            TILE_COUNT(tile_cnt_A),
            NUM_FACES(num_faces),
            NUM_BLOCKS(num_blocks),
            NUM_TILES_IN_BLOCK(num_tiles_in_block),
        ],
        variant_stimuli=StimuliConfig(
            src_A,
            input_format,
            src_B,
            input_format,
            output_format,
            tile_count_A=tile_cnt_A,
            tile_count_B=tile_cnt_B,
            tile_count_res=tile_cnt_A,
            num_faces=num_faces,
            write_full_tiles=True,
        ),
        dest_acc=dest_acc,
        unpack_to_dest=unpack_to_dest,
    )

    fc = configuration.formats_config[0]
    print(f"\n{'='*60}")
    print(
        f"FP8 TILIZE DEBUG - {input_dimensions[0]}x{input_dimensions[1]} "
        f"({num_tile_cols} tile(s))"
    )
    print(f"  unpack_A_src = {fc.unpack_A_src}")
    print(f"  unpack_A_dst = {fc.unpack_A_dst}")
    print(f"  math         = {fc.math}")
    print(f"  pack_src     = {fc.pack_src}")
    print(f"  pack_dst     = {fc.pack_dst}")
    print(f"{'='*60}")

    res_from_L1 = configuration.run(workers_tensix_coordinates).result

    torch_format = format_dict[output_format]
    res_tensor = torch.tensor(res_from_L1, dtype=torch_format)

    all_ok = True
    for tile in range(num_tile_cols):
        print(f"\n--- Tile {tile} ---")
        for face in range(4):
            idx = tile * 1024 + face * 256
            g_uniq = torch.unique(golden_tensor[idx : idx + 256]).tolist()
            a_uniq = torch.unique(res_tensor[idx : idx + 256]).tolist()
            expected = FP8_FACE_VALUES[tile * 4 + face]
            status = "OK" if a_uniq == g_uniq else "MISMATCH"
            if status == "MISMATCH":
                all_ok = False
            print(
                f"  Face {face}: expected={expected:<8g}  "
                f"golden={g_uniq}  actual={a_uniq}  [{status}]"
            )

    print(f"\n{'='*60}")

    assert len(res_from_L1) == len(golden_tensor)
    assert passed_test(
        golden_tensor, res_tensor, output_format
    ), f"FP8 tilize face mismatch for {input_dimensions}"
