# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0

import torch
from helpers.device import collect_results, write_stimuli_to_l1
from helpers.format_config import DataFormat
from helpers.golden_generators import (
    BroadcastGolden,
    EltwiseBinaryGolden,
    TransposeGolden,
    get_golden_generator,
)
from helpers.llk_params import (
    BroadcastType,
    DestAccumulation,
    MathFidelity,
    MathOperation,
    Transpose,
    format_dict,
)
from helpers.param_config import input_output_formats, parametrize
from helpers.test_config import run_test
from helpers.tilize_untilize import tilize, tilize_block, untilize
from helpers.utils import passed_test


@parametrize(
    test_name="eltwise_binary_transpose_bcast_test",
    formats=input_output_formats(
        [
            DataFormat.Float16_b,
        ]
    ),
    broadcast_type=[BroadcastType.Column],
    dest_acc=[DestAccumulation.No],
    math_fidelity=[MathFidelity.LoFi],
    transpose_srca=[Transpose.Yes],
)
def test_eltwise_binary_transpose_bcast(
    test_name,
    formats,
    broadcast_type,
    dest_acc,
    math_fidelity,
    transpose_srca,
):
    # 1 tile: 32x32
    tile_dimensions = [32, 32]
    tile_cnt = 1

    src_A = torch.ones(tile_dimensions[0], tile_dimensions[1]) * 10
    src_A[0, :] = 5  # Row 0 is all 5s
    src_A = src_A.flatten()

    # Tilize the input data for hardware
    src_A_tilized = tilize_block(src_A, tile_dimensions, formats.input_format)

    # Initialize srcB: column 0 is all 3s, rest is 1s
    # After column broadcast (per-face): entire tile becomes all 3s
    src_B = torch.ones(tile_dimensions[0], tile_dimensions[1]) * 1
    src_B[:, 0] = 3  # First column is all 3s
    src_B = src_B.flatten()

    src_B_tilized = tilize_block(src_B, tile_dimensions, formats.input_format)

    print("srcA (before transpose):")
    print(src_A.view(tile_dimensions[0], tile_dimensions[1]))
    print("--------------------------------")

    print("srcB (before column broadcast):")
    print(src_B.view(tile_dimensions[0], tile_dimensions[1]))
    print("--------------------------------")

    # Compute golden using proper transpose generator that understands tilized data
    transpose_golden = get_golden_generator(TransposeGolden)

    # Apply transpose to srcA: hardware does transpose_faces then transpose_within_faces
    src_A_transposed = transpose_golden.transpose_faces_multi_tile(
        src_A,
        formats.input_format,
        num_tiles=tile_cnt,
        tilize=True,  # Tilize before transpose (models hardware behavior)
        input_dimensions=tile_dimensions,
    )
    src_A_transposed = transpose_golden.transpose_within_faces_multi_tile(
        src_A_transposed,
        formats.input_format,
        num_tiles=tile_cnt,
        untilize=True,  # Untilize after transpose for golden comparison
        input_dimensions=tile_dimensions,
    )

    print("srcA after transpose (golden):")
    print(src_A_transposed.view(tile_dimensions[0], tile_dimensions[1]))
    print("--------------------------------")

    # For column broadcast on tilized data, we need to use BroadcastGolden
    # which understands that column broadcast operates on tilized format:
    # - Face 0's column 0 broadcasts to faces 0 and 1
    # - Face 2's column 0 broadcasts to faces 2 and 3
    src_B_tilized_for_bcast = tilize(
        src_B, stimuli_format=formats.input_format, num_faces=4
    )
    broadcast_golden = get_golden_generator(BroadcastGolden)
    src_B_broadcasted_tilized = broadcast_golden(
        broadcast_type,
        src_B_tilized_for_bcast,  # Tilized data
        formats.input_format,
        num_faces=4,
        tile_cnt=tile_cnt,
        face_r_dim=16,
    )
    # Untilize for display only
    src_B_broadcasted_display = untilize(
        src_B_broadcasted_tilized, stimuli_format=formats.output_format
    )

    print("srcB after column broadcast (golden - displayed in row-major):")
    print(src_B_broadcasted_display.view(tile_dimensions[0], tile_dimensions[1]))
    print("--------------------------------")

    # Now retilize the transposed srcA for golden computation
    # (transposes were done on tilized data then untilized, we need to tilize again)
    src_A_transposed_tilized = tilize(
        src_A_transposed, stimuli_format=formats.output_format, num_faces=4
    )

    # Compute element-wise subtraction in tilized format
    binary_golden = get_golden_generator(EltwiseBinaryGolden)
    golden_tensor = binary_golden(
        MathOperation.Elwsub,
        src_A_transposed_tilized,  # Tilized
        src_B_broadcasted_tilized,  # Tilized
        formats.output_format,
        math_fidelity,
    )

    # Untilize for display only
    golden_display = untilize(golden_tensor, stimuli_format=formats.output_format)
    print("Golden tensor (transposed_srcA - broadcast_srcB, displayed in row-major):")
    print(golden_display.view(tile_dimensions[0], tile_dimensions[1]))
    print("--------------------------------")

    # Build test configuration
    test_config = {
        "formats": formats,
        "testname": test_name,
        "dest_acc": dest_acc,
        "input_A_dimensions": tile_dimensions,
        "input_B_dimensions": tile_dimensions,
        "mathop": MathOperation.Elwsub,
        "math_fidelity": math_fidelity,
        "tile_cnt": tile_cnt,
        "broadcast_type": broadcast_type,
        "unpack_transpose_faces": transpose_srca,
        "unpack_transpose_within_face": transpose_srca,
        "num_faces": 4,
        "unpack_to_dest": False,
    }

    res_address = write_stimuli_to_l1(
        test_config,
        src_A_tilized,  # Hardware will transpose this
        src_B_tilized,  # Hardware will broadcast column 0 of this
        formats.input_format,
        formats.input_format,
        tile_count_A=tile_cnt,
        tile_count_B=tile_cnt,
    )

    run_test(test_config)

    res_from_L1 = collect_results(formats, tile_count=tile_cnt, address=res_address)

    assert len(res_from_L1) == len(golden_tensor)

    torch_format = format_dict[formats.output_format]
    res_tensor = torch.tensor(res_from_L1, dtype=torch_format)

    # Untilize for display only
    res_display = untilize(res_tensor, stimuli_format=formats.output_format)
    print("Result tensor from hardware (displayed in row-major):")
    print(res_display.view(tile_dimensions[0], tile_dimensions[1]))
    print("--------------------------------")

    # Compare in tilized format
    assert passed_test(golden_tensor, res_tensor, formats.output_format)
