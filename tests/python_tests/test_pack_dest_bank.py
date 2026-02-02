# SPDX-FileCopyrightText: © 2026 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0

import torch
from helpers.chip_architecture import ChipArchitecture, get_chip_architecture
from helpers.format_config import DataFormat
from helpers.llk_params import DestAccumulation, Tilize, format_dict
from helpers.param_config import (
    input_output_formats,
    parametrize,
)
from helpers.stimuli_config import StimuliConfig
from helpers.test_config import TestConfig
from helpers.test_variant_parameters import (
    DEST_INDEX,
    INPUT_DIMENSIONS,
    NUM_FACES,
    TILE_COUNT,
    TILIZE,
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
            # DataFormat.Float32,
            # DataFormat.Float16,
            DataFormat.Float16_b,
            # DataFormat.Bfp8_b,
        ]
    ),
    dest_acc=[DestAccumulation.Yes],
    num_faces=4,
    tilize=[Tilize.No],
    dest_index=0,
)
def test_pack_dest_bank(
    formats, dest_acc, num_faces, tilize, dest_index, workers_tensix_coordinates
):

    input_dimensions = [64, 128]

    # src_A, tile_cnt_A, src_B, tile_cnt_B = generate_stimuli(
    #     stimuli_format_A=formats.input_format,
    #     input_dimensions_A=input_dimensions,
    #     stimuli_format_B=formats.input_format,
    #     input_dimensions_B=input_dimensions,
    # )

    # Calculate tile count: 64 rows / 32 = 2 tile rows, 128 cols / 32 = 4 tile cols
    tile_rows = input_dimensions[0] // 32
    tile_cols = input_dimensions[1] // 32
    tile_cnt_A = tile_rows * tile_cols  # Should be 8 tiles

    # Create custom stimuli: each tile filled with (tile_index + 1)
    # Each tile has 32x32 = 1024 values
    src_A = []
    for tile_idx in range(tile_cnt_A):
        tile_value = float(tile_idx + 1)
        src_A.extend([tile_value] * 1024)  # 1024 values per tile

    src_A = torch.tensor(src_A, dtype=torch.float32)

    # src_B is not used in this test but needed for StimuliConfig
    src_B = torch.zeros_like(src_A)
    tile_cnt_B = tile_cnt_A

    # For datacopy, golden output should match input (converted to output format)
    # Convert to torch tensor with the appropriate format
    torch_format = format_dict[formats.output_format]
    golden_tensor = src_A.to(torch_format)

    unpack_to_dest = (
        False
        if tilize == Tilize.Yes and formats.input_format == DataFormat.Float32
        else formats.input_format.is_32_bit() and dest_acc == DestAccumulation.Yes
    )

    configuration = TestConfig(
        "sources/pack_dest_bank_test.cpp",
        formats,
        templates=[
            INPUT_DIMENSIONS(input_dimensions, input_dimensions),
            TILIZE(tilize),
        ],
        runtimes=[DEST_INDEX(dest_index), TILE_COUNT(tile_cnt_A), NUM_FACES(num_faces)],
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

    res_from_L1 = configuration.run(workers_tensix_coordinates)

    assert len(res_from_L1) == len(golden_tensor)

    res_tensor = torch.tensor(res_from_L1, dtype=torch_format)

    assert passed_test(golden_tensor, res_tensor, formats.output_format)
