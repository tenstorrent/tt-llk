# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0

import pytest
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
from helpers.golden_generators import TilizeGolden, TransposeGolden, get_golden_generator
from helpers.param_config import (
    clean_params,
    input_output_formats,
)
from helpers.stimuli_generator import generate_stimuli
from helpers.test_config import run_test
from helpers.utils import passed_test

# Global tracking variables
failed_combinations = []
passed_count = 0
total_count = 0


# SUPPORTED FORMATS FOR TEST - REDUCED FOR INITIAL TESTING
supported_formats = [
    DataFormat.Float32,
    DataFormat.Float16,
    DataFormat.Float16_b,
    # DataFormat.Bfp8_b,      # Commented out for initial testing
]


stoch_rnd_types = [
    StochasticRoundingType.NONE,
    StochasticRoundingType.Fpu,
    StochasticRoundingType.Pack,
    StochasticRoundingType.All,
]


transpose_values = [Transpose.No]  # Disable transpose testing for now

# Disable narrow tile testing for now
narrow_tile_values = [NarrowTile.No]

dest_acc_values = [DestAccumulation.No, DestAccumulation.Yes]
# haloize_values removed - haloize is always disabled (set to 0) in tilize operations
face_r_dim_values = [FaceRDim.Face16, FaceRDim.Face32]
num_faces_values = [NumFaces.Four, NumFaces.Two, NumFaces.One]

# Use cross-format testing to include all format combinations
test_formats = input_output_formats(supported_formats, False)


def generate_tilize_params():
    """Generate all valid parameter combinations for tilize testing using Z3 solver."""

    # Calculate total possible combinations before filtering
    total_possible = (
        len(test_formats)
        * len(stoch_rnd_types)
        * len(transpose_values)
        * len(narrow_tile_values)
        * len(dest_acc_values)
        * 1  # haloize always disabled in tilize
        * len(face_r_dim_values)
        * len(num_faces_values)
    )

    # Generate parameter combinations
    all_params = []
    filtered_count = 0

    for fmt in test_formats:
        for stoch_rnd in stoch_rnd_types:
            for transpose in transpose_values:
                for narrow in narrow_tile_values:
                    for dest_acc in dest_acc_values:
                        # haloize loop removed - not used in tilize operations
                        for face_dim in face_r_dim_values:
                            for num_faces in num_faces_values:

                                # No constraints needed for now (transpose disabled)

                                param_tuple = (
                                    "unpack_tilize_sweep_test",
                                    fmt,
                                    stoch_rnd,
                                    transpose,
                                    narrow,
                                    dest_acc,
                                    face_dim,
                                    num_faces,
                                )
                                all_params.append(param_tuple)

    return all_params


# Generate all parameter combinations
all_params = generate_tilize_params()

# Set total count for tracking
total_count = len(all_params)


def create_simple_ids(all_params):
    """Create simple test IDs for parameter combinations."""
    ids = []

    for (
        testname,
        formats,
        stoch_rnd,
        transpose,
        narrow,
        dest_acc,
        face_dim,
        num_faces,
    ) in all_params:

        # Create a concise ID string
        fmt_str = f"{formats.input_format.name}->{formats.output_format.name}"
        stoch_str = stoch_rnd.name.lower()
        trans_str = "transpose" if transpose == Transpose.Yes else "notranspose"
        narrow_str = "narrow" if narrow == NarrowTile.Yes else "full"
        dest_str = "destacc" if dest_acc == DestAccumulation.Yes else "nodest"
        face_str = f"f{face_dim.value}"
        faces_str = f"{num_faces.value}faces"

        id_str = f"{fmt_str}_{stoch_str}_{trans_str}_{narrow_str}_{dest_str}_{face_str}_{faces_str}"
        ids.append(id_str)

    return ids


param_ids = create_simple_ids(all_params)


@pytest.mark.parametrize(
    "testname, formats, stoch_rnd_type, transpose, "
    "narrow_tile, dest_acc, face_r_dim, num_faces",
    clean_params(all_params),
    ids=param_ids,
)
def test_unpack_tilize_comprehensive(
    testname,
    formats,
    stoch_rnd_type,
    transpose,
    narrow_tile,
    dest_acc,
    face_r_dim,
    num_faces,
):
    """Comprehensive parameter sweep test for unpack_tilize operation."""

    # Create test identifier
    test_id = f"{formats.input_format.name}->{formats.output_format.name}_{stoch_rnd_type.name.lower()}_{'transpose' if transpose == Transpose.Yes else 'notranspose'}_{'narrow' if narrow_tile == NarrowTile.Yes else 'full'}_{'destacc' if dest_acc == DestAccumulation.Yes else 'nodest'}_f{face_r_dim.value}_{num_faces.value}faces"

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
        "testname": testname,
        "tile_cnt": tile_cnt,
        "input_dimensions": input_dimensions,
        "unpack_to_dest": unpack_to_dest,
        "stoch_rnd_type": stoch_rnd_type,
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
