# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0

import torch

from helpers.chip_architecture import get_chip_architecture
from helpers.device import (
    collect_results,
    write_stimuli_to_l1,
)
from helpers.format_arg_mapping import (
    DestAccumulation,
    FaceRDim,
    NarrowTile,
    NumFaces,
    Transpose,
    format_dict,
)
from helpers.format_config import DataFormat, StochasticRoundingType
from helpers.golden_generators import TilizeGolden, get_golden_generator
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
            # DataFormat.Bfp8_b,      # Commented out for initial testing
        ]
    ),
    stoch_rnd_types=[
        StochasticRoundingType.NONE,
        StochasticRoundingType.Fpu,
        StochasticRoundingType.Pack,
        StochasticRoundingType.All,
    ],
    transpose=[Transpose.No],
    narrow_tile=[NarrowTile.No],
    dest_acc=[DestAccumulation.No, DestAccumulation.Yes],
    face_r_dim=[FaceRDim.Face16, FaceRDim.Face32],
    num_faces=[NumFaces.Four, NumFaces.Two, NumFaces.One],
)
def test_unpack_tilize_comprehensive(
    test_name,
    formats,
    stoch_rnd_types,
    transpose,
    narrow_tile,
    dest_acc,
    face_r_dim,
    num_faces,
):
    """Comprehensive parameter sweep test for unpack_tilize operation."""

    # Get architecture
    arch = get_chip_architecture()

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
    )

    golden_tensor = golden_tensor.to(torch_format)
    # Write stimuli to L1
    res_address = write_stimuli_to_l1(
        src_A, src_B, formats.input_format, formats.input_format, tile_count=tile_cnt
    )

    # Build the complete test config
    test_config = {
        "formats": formats,
        "testname": test_name,
        "tile_cnt": tile_cnt,
        "input_dimensions": input_dimensions,
        "unpack_to_dest": unpack_to_dest,
        "stoch_rnd_type": stoch_rnd_types,
        "unpack_transpose_faces": transpose.value,
        "unpack_transpose_within_face": transpose.value,
        "dest_acc": dest_acc,
        "num_faces": num_faces.value,
    }

    # Run the test
    run_test(test_config)

    # Collect and verify results
    res_from_L1 = collect_results(formats, tile_count=tile_cnt, address=res_address)
    assert len(res_from_L1) == len(golden_tensor)

    res_tensor = torch.tensor(res_from_L1, dtype=format_dict[formats.output_format])

    assert passed_test(golden_tensor, res_tensor, formats.output_format)
