# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0

import pytest
import torch
from helpers.constraints import get_valid_dest_accumulation_modes
from helpers.data_format_inference import infer_data_formats
from helpers.format_config import DataFormat
from helpers.golden_generators import UntilizeGolden, get_golden_generator
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
    ROW_NUM_DATUMS,
    TILE_COUNT,
)
from helpers.utils import passed_test


# Narrow row option works only on FULL_CT_DIM = BLOCK_CT_DIM = 1.
# This is not a test limitation but an api limitation.
# How it works is it takes row_dimension datums from each row in dest.
# It does that for each tile available and then either packs it tile after tile with garbage datums at the end of the tile
# or it packs it densely without any garbage datums, which is what it currently does.
def generate_narrow_row_golden(src_A, input_dimensions, row_dimension):
    """
    Generate golden tensor for narrow_row mode.
    Takes first row_dimension elements from each row in Face 0 and Face 2 of each tile.
    TODO: This currently only works for default tile dimensions. Once testing other tile dimensions, update accordingly.
    """
    tile_width = 32
    tile_height = 32
    tile_count = (input_dimensions[0] * input_dimensions[1]) // (
        tile_width * tile_height
    )
    elements_per_tile = tile_width * tile_height
    face_size = (
        tile_height * tile_width // 4
    )  # Each face is 1/4th of the tile (256 elements)

    golden_parts = []
    for tile_idx in range(tile_count):
        tile_data = src_A[
            tile_idx * elements_per_tile : (tile_idx + 1) * elements_per_tile
        ]
        face0 = tile_data[0:face_size].reshape(16, 16)
        face2 = tile_data[2 * face_size : 3 * face_size].reshape(16, 16)
        # Take first row_dimension elements from each row of Face 0 and Face 2
        narrow_face0 = face0[:, :row_dimension].flatten()
        narrow_face2 = face2[:, :row_dimension].flatten()
        golden_parts.append(torch.cat([narrow_face0, narrow_face2]))

    return torch.cat(golden_parts)


@parametrize(
    formats=input_output_formats(
        [
            DataFormat.Float16_b,
            DataFormat.Float16,
            DataFormat.Float32,  # Test Float32 with both 32bit mode dest (full precision) and 16bit mode dest (precision loss)
            DataFormat.Int32,
            DataFormat.Bfp8_b,
        ]  # Pack Untilize doesn't work for block float formats (Bfp8_b); we only include as input format in our test
    ),
    dest_acc=lambda formats: get_valid_dest_accumulation_modes(formats),
    input_dimensions=[[96, 288], [64, 64], [32, 128], [128, 128], [32, 64], [64, 32]],
    dest_sync=[DestSync.Half, DestSync.Full],
    narrow_row=lambda input_dimensions: (
        [True, False] if input_dimensions == [64, 32] else [False]
    ),
    row_dimension=lambda narrow_row, input_dimensions: (
        [2, 8, 16] if narrow_row else [32]
    ),  # This is a tile row dimension.
)
def test_pack_untilize(
    formats,
    dest_acc,
    input_dimensions,
    dest_sync,
    narrow_row,
    row_dimension,
    workers_tensix_coordinates,
):
    if TestConfig.WITH_COVERAGE and input_dimensions == [96, 288]:
        pytest.skip(
            "Skipping large dimension test in coverage mode, check issue: #1063 on TT-LLK repo"
        )

    if formats.output_format == DataFormat.Bfp8_b:
        pytest.skip("Pack Untilize does not support Bfp8_b format")

    if (formats.input_format == DataFormat.Int32) ^ (
        formats.output_format == DataFormat.Int32
    ):
        pytest.skip("Pack Untilize does not support mixing Int32 with other formats")

    data_formats = infer_data_formats(
        formats.input_format,
        formats.output_format,
        dest_acc,
        False,
    )

    # Handling a hardware limitation: cannot convert 8-bit exponent datums to Float16 without storing them as intermediate Float32 in dest register.
    # For wormhole architecture, gasket cannot perform this conversion and packer takes input Float32 (from dest register) converting to Float16_A.
    # For blackhole architecture, gasket is able to convert Float32 to Float16_A before packing (reduces work on packer).`
    if (
        formats.input_format == DataFormat.Float16
        and data_formats.pack_src.is_32_bit()
        and dest_acc == DestAccumulation.No
    ):
        pytest.skip(
            "Due to hardware limitation, cannot convert 8-bit exponent datums to Float16 without storing them as intermediate Float32 in dest register. Therefore using dest_acc=No is not supported in this case."
        )

    src_A, tile_cnt_A, src_B, tile_cnt_B = generate_stimuli(
        stimuli_format_A=formats.input_format,
        input_dimensions_A=input_dimensions,
        stimuli_format_B=formats.input_format,
        input_dimensions_B=input_dimensions,
        sfpu=False,
    )

    if narrow_row:
        golden_tensor = generate_narrow_row_golden(
            src_A, input_dimensions, row_dimension
        )
    else:
        generate_golden = get_golden_generator(UntilizeGolden)
        golden_tensor = generate_golden(src_A, formats.output_format, input_dimensions)

    unpack_to_dest = (
        formats.input_format.is_32_bit() and dest_acc == DestAccumulation.Yes
    )

    # _llk_pack_untilize_init_ has a static_assert that checks if block_ct_dim is less or equal to 8.
    # TODO: Update this logic to accept more than 8 tiles per block if the static_assert changes in the future.
    max_bct_dim = 8 if dest_acc == DestAccumulation.No else 4
    full_ct_dim = input_dimensions[1] // 32
    block_ct_dim = next(
        (bct for bct in range(max_bct_dim, 0, -1) if full_ct_dim % bct == 0), 1
    )

    configuration = TestConfig(
        "sources/pack_untilize_test.cpp",
        formats,
        templates=[
            INPUT_DIMENSIONS(
                input_dimensions,
                input_dimensions,
                block_ct_dim,
            ),
            DEST_SYNC(dest_sync),
            ROW_NUM_DATUMS(row_dimension),
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
        unpack_to_dest=unpack_to_dest,
    )

    res_from_L1 = configuration.run(workers_tensix_coordinates)

    if narrow_row:
        # Kernel is set to densely pack processed datums.
        res_from_L1 = res_from_L1[: input_dimensions[0] * row_dimension]

    assert len(res_from_L1) == len(
        golden_tensor
    ), "Result tensor and golden tensor are not of the same length"

    res_tensor = torch.tensor(res_from_L1, dtype=format_dict[formats.output_format])

    assert passed_test(
        golden_tensor, res_tensor, formats.output_format
    ), "Assert against golden failed"
