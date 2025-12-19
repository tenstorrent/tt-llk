# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0

import torch
from helpers.device import collect_results, write_stimuli_to_l1
from helpers.format_config import DataFormat
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
from helpers.tilize_untilize import tilize_block
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

    # Initialize srcA: all rows are 10s except row 1 (index 1) which is 5s
    src_A = torch.ones(tile_dimensions[0], tile_dimensions[1]) * 10
    src_A[0, :] = 5  # Row 1 is all 5s
    src_A = src_A.flatten()

    # Initialize srcB: all 1s except first column which is 3s
    src_B = torch.ones(tile_dimensions[0], tile_dimensions[1]) * 1
    src_B[:, 0] = 3  # First column is all 3s
    src_B = src_B.flatten()

    print("srcA (before transpose):")
    print(src_A.view(tile_dimensions[0], tile_dimensions[1]))
    print("--------------------------------")

    print("srcB (before column broadcast):")
    print(src_B.view(tile_dimensions[0], tile_dimensions[1]))
    print("--------------------------------")

    src_A = tilize_block(src_A, tile_dimensions, formats.input_format)
    src_B = tilize_block(src_B, tile_dimensions, formats.input_format)

    # Manually compute expected golden output:
    # After transpose, srcA will have all 10s except column 0 which is 5s
    transposed_src_A = torch.ones(tile_dimensions[0], tile_dimensions[1]) * 10
    transposed_src_A[:, 0] = 5  # Column 0 is all 5s

    # After column broadcast, srcB broadcasts column 0 to all columns
    # So the entire tile becomes all 3s
    broadcast_src_B = torch.ones(tile_dimensions[0], tile_dimensions[1]) * 3

    # Element-wise subtraction: transposed_srcA - broadcast_srcB
    golden_tensor = transposed_src_A - broadcast_src_B
    golden_tensor = golden_tensor.flatten()

    # print("Expected transposed_srcA:")
    # print(transposed_src_A)
    # print("--------------------------------")

    # print("Expected broadcast_srcB:")
    # print(broadcast_src_B)
    # print("--------------------------------")

    print("Golden tensor (transposed_srcA - broadcast_srcB):")
    print(golden_tensor.view(tile_dimensions[0], tile_dimensions[1]))
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
        src_A,  # Hardware will transpose this
        src_B,  # Hardware will broadcast column 0 of this
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

    print("Result tensor from hardware:")
    print(res_tensor.view(tile_dimensions[0], tile_dimensions[1]))
    print("--------------------------------")

    assert passed_test(golden_tensor, res_tensor, formats.output_format)
