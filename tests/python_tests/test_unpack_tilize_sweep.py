# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0

import pytest
import torch

from helpers.chip_architecture import ChipArchitecture, get_chip_architecture
from helpers.device import (
    collect_results,
    write_stimuli_to_l1,
)
from helpers.format_arg_mapping import (
    DestAccumulation,
    NarrowTile,
    NumFaces,
    StochasticRounding,
    Transpose,
    format_dict,
)
from helpers.format_config import DataFormat
from helpers.golden_generators import (
    TilizeGolden,
    get_golden_generator,
)
from helpers.param_config import (
    input_output_formats,
    parametrize,
)
from helpers.stimuli_generator import generate_stimuli
from helpers.test_config import run_test
from helpers.utils import passed_test


@parametrize(
    test_name="unpack_tilize_sweep_test",
    formats=input_output_formats(
        [
            DataFormat.Float32,
            DataFormat.Float16,
            DataFormat.Float16_b,
            DataFormat.Bfp8_b,
        ]
    ),
    stoch_rnd_type=[
        StochasticRounding.No,
        StochasticRounding.Fpu,
        StochasticRounding.Pack,
        StochasticRounding.All,
    ],
    transpose=[Transpose.Yes, Transpose.No],
    narrow_tile=[NarrowTile.No],
    dest_acc=[DestAccumulation.No, DestAccumulation.Yes],
    num_faces=[NumFaces.Four, NumFaces.Two, NumFaces.One],
)
def test_unpack_tilize_comprehensive(
    test_name,
    formats,
    stoch_rnd_type,
    transpose,
    narrow_tile,
    dest_acc,
    num_faces,
):
    """Comprehensive parameter sweep test for unpack_tilize operation."""

    # Get architecture
    arch = get_chip_architecture()

    # Skip num_faces=1 for Wormhole due to unpack_tilize API (0-loop issue)
    if num_faces == NumFaces.One and arch == ChipArchitecture.WORMHOLE:
        pytest.skip("unpack_tilize API has 0-loop issue for num_faces=1 on Wormhole")

    # Skip BFP8_b format (input or output) for Blackhole
    if formats.input_format == DataFormat.Bfp8_b:
        pytest.skip(
            "BFP8_b input format not supported on Blackhole/Wormhole for tilize unpacker"
        )

    # Skip BFP8_b output format with num_faces != 4 for Blackhole only
    if (
        arch == ChipArchitecture.BLACKHOLE
        and formats.output_format == DataFormat.Bfp8_b
        and num_faces != NumFaces.Four
    ):
        pytest.skip("BFP8_b output format with num_faces != 4 issue on Blackhole")

    # Determine unpack_to_dest based on format
    unpack_to_dest = formats.input_format in [DataFormat.Int32, DataFormat.UInt32]

    input_dimensions = [32, 32]  # Standard dimensions for multi-tile testing

    # Generate test data
    src_A, src_B, tile_cnt = generate_stimuli(
        formats.input_format,
        formats.input_format,
        input_dimensions=input_dimensions,
    )

    torch_format = format_dict[formats.output_format]

    # Generate golden reference - tilization
    tilize_function = get_golden_generator(TilizeGolden)
    golden_tensor = tilize_function(
        src_A,
        input_dimensions,
        formats.output_format,
        num_faces.value,
    )

    golden_tensor = golden_tensor.to(torch_format)

    # Build the complete test config
    test_config = {
        "formats": formats,
        "testname": test_name,
        "tile_cnt": tile_cnt,
        "input_dimensions": input_dimensions,
        "unpack_to_dest": unpack_to_dest,
        "stoch_rnd_type": stoch_rnd_type,
        "unpack_transpose_faces": Transpose.No.value,
        "unpack_transpose_within_face": transpose.value,
        "dest_acc": dest_acc,
        "num_faces": num_faces.value,
        "narrow_tile": narrow_tile.value,
    }

    # Write stimuli to L1
    res_address = write_stimuli_to_l1(
        test_config,
        src_A,
        src_B,
        formats.input_format,
        formats.input_format,
        tile_count_A=tile_cnt,
        tile_count_B=tile_cnt,
        num_faces=num_faces.value,
    )

    # Run the test
    run_test(test_config)

    # Collect and verify results
    res_from_L1 = collect_results(
        formats, tile_count=tile_cnt, address=res_address, num_faces=num_faces.value
    )
    assert len(res_from_L1) == len(golden_tensor)

    res_tensor = torch.tensor(res_from_L1, dtype=format_dict[formats.output_format])

    assert passed_test(golden_tensor, res_tensor, formats.output_format)
