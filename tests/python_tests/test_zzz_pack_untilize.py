# SPDX-FileCopyrightText: © 2026 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0

import pytest
import torch
from helpers.chip_architecture import ChipArchitecture, get_chip_architecture
from helpers.data_format_inference import infer_data_formats
from helpers.format_config import DataFormat
from helpers.golden_generators import (
    TILE_DIMENSIONS,
    UntilizeGolden,
    get_golden_generator,
)
from helpers.llk_params import (
    BlocksCalculationAlgorithm,
    DestAccumulation,
    DestSync,
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
    DEST_SYNC,
    NUM_FACES,
    TILE_COUNT,
    TILE_DST_CT_OFFSET,
    generate_input_dim,
)
from helpers.utils import passed_test


@parametrize(
    formats=input_output_formats(
        [
            # DataFormat.Float16_b,
            DataFormat.Float16,
            # DataFormat.Float32,
            # DataFormat.Int32,
            # DataFormat.Bfp8_b,
        ]
    ),
    dest_acc=[DestAccumulation.No],
    # input_dimensions=[[64, 64], [32, 128], [128, 128], [32, 64]],
    input_dimensions=[[64, 64], [32, 128], [128, 128], [32, 64], [32, 96], [32, 224]],
    dest_sync=[DestSync.Half],
    tile_dst_ct_offset=[0],
)
def test_pack_untilize(
    formats,
    dest_acc,
    input_dimensions,
    dest_sync,
    tile_dst_ct_offset,
    workers_tensix_coordinates,
):
    if TestConfig.WITH_COVERAGE and input_dimensions == [64, 512]:
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

    # TODO: Checkout issue #1405 on tt-llk.
    if (
        get_chip_architecture() == ChipArchitecture.WORMHOLE
        and formats.input_format
        in (DataFormat.Float16_b, DataFormat.Float16, DataFormat.Bfp8_b)
        and formats.output_format == DataFormat.Float32
        and dest_acc == DestAccumulation.No
        and input_dimensions == [64, 512]
    ):
        pytest.skip("Wormhole pack_untilize does not support this format combination.")

    src_A, tile_cnt_A, src_B, tile_cnt_B = generate_stimuli(
        stimuli_format_A=formats.input_format,
        input_dimensions_A=input_dimensions,
        stimuli_format_B=formats.input_format,
        input_dimensions_B=input_dimensions,
        sfpu=False,
        sequential_A=True,
    )

    generate_golden = get_golden_generator(UntilizeGolden)

    golden_tensor = generate_golden(src_A, formats.output_format, input_dimensions)

    unpack_to_dest = (
        formats.input_format.is_32_bit() and dest_acc == DestAccumulation.Yes
    )

    # _llk_pack_untilize_init_ has a static_assert that checks if block_ct_dim is less or equal to 8.
    # TODO: Update this logic to accept more than 8 tiles per block if the static_assert changes in the future.
    _, block_ct_dim = get_num_blocks_and_num_tiles_in_block(
        dest_sync,
        dest_acc,
        formats,
        input_dimensions,
        TILE_DIMENSIONS,
        BlocksCalculationAlgorithm.Untilize,
    )

    configuration = TestConfig(
        "sources/pack_untilize_test.cpp",
        formats,
        templates=[
            generate_input_dim(
                input_dimensions,
                input_dimensions,
                block_ct_dim,
            ),
            DEST_SYNC(dest_sync),
            TILE_DST_CT_OFFSET(tile_dst_ct_offset),
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

    res_from_L1 = configuration.run(workers_tensix_coordinates).result

    assert len(res_from_L1) == len(
        golden_tensor
    ), "Result tensor and golden tensor are not of the same length"

    res_tensor = torch.tensor(res_from_L1, dtype=format_dict[formats.output_format])

    assert passed_test(
        golden_tensor, res_tensor, formats.output_format
    ), "Assert against golden failed"


@parametrize(
    formats=input_output_formats(
        [
            # DataFormat.Float16_b,
            DataFormat.Float16,
            # DataFormat.Float32,
            # DataFormat.Int32,
            # DataFormat.Bfp8_b,
        ]
    ),
    dest_acc=[DestAccumulation.No],
    input_dimensions=[[64, 64], [32, 128], [128, 128], [32, 64], [32, 96], [32, 224]],
    # input_dimensions=[[32, 64]],
    dest_sync=[DestSync.Half],
    tile_dst_ct_offset=[0],
)
def test_pack_untilize_4intf(
    formats,
    dest_acc,
    input_dimensions,
    dest_sync,
    tile_dst_ct_offset,
    workers_tensix_coordinates,
):
    """
    Functional test for 4-interface optimized pack_untilize.

    Tests the new _llk_pack_untilize_4intf_* API which uses all 4 packer
    interfaces for 2x bandwidth. Input data is converted to interleaved
    layout (T0F0 T0F1 T1F0 T1F1 | ...) before running the kernel.

    This test should produce identical results to test_pack_untilize but
    with better performance (measured in perf tests).
    """
    if TestConfig.WITH_COVERAGE and input_dimensions == [64, 512]:
        pytest.skip(
            "Skipping large dimension test in coverage mode, check issue: #1063 on TT-LLK repo"
        )

    if formats.output_format == DataFormat.Bfp8_b:
        pytest.skip("Pack Untilize does not support Bfp8_b format")

    if (formats.input_format == DataFormat.Int32) ^ (
        formats.output_format == DataFormat.Int32
    ):
        pytest.skip("Pack Untilize does not support mixing Int32 with other formats")

    # 4-interface optimization requires Blackhole architecture
    if get_chip_architecture() != ChipArchitecture.BLACKHOLE:
        pytest.skip("4-interface optimization currently only implemented for Blackhole")

    data_formats = infer_data_formats(
        formats.input_format,
        formats.output_format,
        dest_acc,
        False,
    )

    if (
        formats.input_format == DataFormat.Float16
        and data_formats.pack_src.is_32_bit()
        and dest_acc == DestAccumulation.No
    ):
        pytest.skip(
            "Due to hardware limitation, cannot convert 8-bit exponent datums to Float16 without storing them as intermediate Float32 in dest register."
        )

    src_A, tile_cnt_A, src_B, tile_cnt_B = generate_stimuli(
        stimuli_format_A=formats.input_format,
        input_dimensions_A=input_dimensions,
        stimuli_format_B=formats.input_format,
        input_dimensions_B=input_dimensions,
        sfpu=False,
        sequential_A=True,
    )

    # Make tile 3 (index 2) distinctive with even numbers 2050, 2052, 2054...
    # Float16 can represent these exactly (magnitude 2048-4095 represents every 2nd integer)
    tile_size = 32 * 32
    if tile_cnt_A >= 3:
        tile_3_start = 2 * tile_size  # Start of 3rd tile
        tile_3_end = 3 * tile_size  # End of 3rd tile
        # Generate even numbers: 2050, 2052, 2054, ..., 2050+1023*2 = 4096
        tile_3_data = torch.arange(2050, 2050 + 1024 * 2, step=2, dtype=src_A.dtype)
        src_A[tile_3_start:tile_3_end] = tile_3_data

    def print_tile(tile_data, tile_name):
        """Print a 32x32 tile with gaps between 16x16 faces"""
        print(f"\n{'='*80}")
        print(f"{tile_name}")
        print(f"{'='*80}")

        # Tiles are stored as faces sequentially: [F0: 0-255][F1: 256-511][F2: 512-767][F3: 768-1023]
        # Reconstruct the 32x32 tile with proper face layout:
        # F0 (upper-left) | F1 (upper-right)
        # F2 (lower-left) | F3 (lower-right)
        tile_2d = torch.zeros((32, 32), dtype=tile_data.dtype)

        # Each face is 16x16 = 256 elements, stored row-major within the face
        f0 = tile_data[0:256].reshape(16, 16)
        f1 = tile_data[256:512].reshape(16, 16)
        f2 = tile_data[512:768].reshape(16, 16)
        f3 = tile_data[768:1024].reshape(16, 16)

        # Place faces in correct positions
        tile_2d[0:16, 0:16] = f0  # Upper-left
        tile_2d[0:16, 16:32] = f1  # Upper-right
        tile_2d[16:32, 0:16] = f2  # Lower-left
        tile_2d[16:32, 16:32] = f3  # Lower-right
        for row in range(32):
            row_str = ""
            for col in range(32):
                val = tile_2d[row, col].item()
                row_str += f"{val:7.1f}"
                # Add gap after 16th column (between F0/F2 and F1/F3)
                if col == 15:
                    row_str += "  |  "
            print(row_str)
            # Add gap after 16th row (between F0/F1 and F2/F3)
            if row == 15:
                print("-" * 80)

    print(f"\n=== Original src_A (before interleaving) ===")
    print(f"Shape: {src_A.shape}, Length: {len(src_A)}, num_tiles: {tile_cnt_A}")

    # Print each tile in original format
    tile_size = 32 * 32
    for i in range(min(tile_cnt_A, 3)):  # Print first 3 tiles
        tile_data = src_A[i * tile_size : (i + 1) * tile_size]
        print_tile(tile_data, f"Original Tile {i}")

    # Convert input data to interleaved layout required by 4-interface API
    from helpers.interleaved_layout import convert_to_interleaved_layout

    # Reshape src_A into list of tiles and convert layout
    tile_size = 32 * 32  # Full tile
    num_tiles = len(src_A) // tile_size
    tiles = [
        src_A[i * tile_size : (i + 1) * tile_size].reshape(32, 32)
        for i in range(num_tiles)
    ]

    # Convert to tile-level interleaved layout - returns list of 32x32 tiles
    interleaved_tiles = convert_to_interleaved_layout(tiles)

    print(f"\n{'='*80}")
    print(f"=== After interleaving ===")
    print(f"Number of interleaved tiles: {len(interleaved_tiles)}")
    print(f"{'='*80}")

    # Print each interleaved tile
    for i, tile in enumerate(interleaved_tiles):
        print_tile(tile.flatten(), f"Interleaved Tile {i}")

    # Flatten each tile and concatenate into a single tensor
    src_A_interleaved = torch.cat([tile.flatten() for tile in interleaved_tiles], dim=0)

    print(f"\n{'='*80}")
    print(f"=== Final src_A_interleaved summary ===")
    print(f"Shape: {src_A_interleaved.shape}, Length: {len(src_A_interleaved)}")
    print(f"{'='*80}\n")

    generate_golden = get_golden_generator(UntilizeGolden)

    # Golden is generated from original (non-interleaved) data
    golden_tensor = generate_golden(src_A, formats.output_format, input_dimensions)

    unpack_to_dest = (
        formats.input_format.is_32_bit() and dest_acc == DestAccumulation.Yes
    )

    _, block_ct_dim = get_num_blocks_and_num_tiles_in_block(
        dest_sync,
        dest_acc,
        formats,
        input_dimensions,
        TILE_DIMENSIONS,
        BlocksCalculationAlgorithm.Untilize,
    )

    configuration = TestConfig(
        "sources/pack_untilize_4intf_test.cpp",  # Use 4-interface kernel
        formats,
        templates=[
            generate_input_dim(
                input_dimensions,
                input_dimensions,
                block_ct_dim,
            ),
            DEST_SYNC(dest_sync),
            TILE_DST_CT_OFFSET(tile_dst_ct_offset),
        ],
        runtimes=[TILE_COUNT(tile_cnt_A), NUM_FACES(4)],
        variant_stimuli=StimuliConfig(
            src_A_interleaved,  # Use interleaved data
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

    res_from_L1 = configuration.run(workers_tensix_coordinates).result

    assert len(res_from_L1) == len(
        golden_tensor
    ), "Result tensor and golden tensor are not of the same length"

    res_tensor = torch.tensor(res_from_L1, dtype=format_dict[formats.output_format])

    assert passed_test(
        golden_tensor, res_tensor, formats.output_format
    ), "4-interface optimization: Assert against golden failed"
