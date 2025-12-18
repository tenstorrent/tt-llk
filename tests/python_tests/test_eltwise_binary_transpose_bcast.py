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
from helpers.stimuli_generator import generate_stimuli
from helpers.test_config import run_test
from helpers.utils import passed_test


@parametrize(
    test_name="eltwise_binary_transpose_bcast_test",
    formats=input_output_formats(
        [
            DataFormat.Float16_b,
            # DataFormat.Float16,
        ]
    ),
    broadcast_type=[
        BroadcastType.Column,
        # BroadcastType.Row,
        # BroadcastType.Scalar,
    ],
    dest_acc=[DestAccumulation.No],
    math_fidelity=[MathFidelity.LoFi],
    transpose_srca=[Transpose.Yes],
    input_dimensions=[[32, 32]],
)
def test_eltwise_binary_transpose_bcast(
    test_name,
    formats,
    broadcast_type,
    dest_acc,
    math_fidelity,
    transpose_srca,
    input_dimensions,
):
    src_A, src_B, tile_cnt = generate_stimuli(
        formats.input_format,
        formats.input_format,
        input_dimensions=input_dimensions,
    )

    src_A = torch.ones(input_dimensions[0] * input_dimensions[1]) * 10
    src_A[: input_dimensions[1]] = 99
    src_B = torch.ones(input_dimensions[0] * input_dimensions[1]) * 3

    print("src_A:")
    print(src_A.view(input_dimensions[0], input_dimensions[1]))
    print("--------------------------------")

    print("src_B:")
    print(src_B.view(input_dimensions[0], input_dimensions[1]))
    print("--------------------------------")

    # Generate golden for srcA with face-based transpose (hardware does this)
    transpose_golden = get_golden_generator(TransposeGolden)

    # Apply both transpose operations to srcA
    # First transpose within faces (16x16 blocks), then transpose face arrangement
    temp_tensor = transpose_golden.transpose_within_faces(
        src_A, formats.output_format, input_dimensions, num_faces=4
    )
    transposed_src_A = transpose_golden.transpose_faces(
        temp_tensor, formats.output_format, input_dimensions, num_faces=4
    )

    print("transposed_src_A (expected from hardware):")
    print(transposed_src_A.view(input_dimensions[0], input_dimensions[1]))
    print("--------------------------------")

    # Generate golden for srcB with broadcast
    broadcast_golden = get_golden_generator(BroadcastGolden)
    broadcast_src_B = broadcast_golden(
        broadcast_type,
        src_B,
        formats.output_format,
        num_faces=4,
        tile_cnt=tile_cnt,
        face_r_dim=16,
    )

    # Convert to tensors for computation
    if not isinstance(transposed_src_A, torch.Tensor):
        transposed_src_A = torch.tensor(
            transposed_src_A, dtype=format_dict[formats.output_format]
        )
    if not isinstance(broadcast_src_B, torch.Tensor):
        broadcast_src_B = torch.tensor(
            broadcast_src_B, dtype=format_dict[formats.output_format]
        )

    # Perform element-wise subtraction using golden generator
    binary_golden = get_golden_generator(EltwiseBinaryGolden)
    golden_tensor = binary_golden(
        MathOperation.Elwsub,
        transposed_src_A,
        broadcast_src_B,
        formats.output_format,
        math_fidelity,
    )

    print("golden_tensor:")
    print(golden_tensor.view(input_dimensions[0], input_dimensions[1]))
    print("--------------------------------")

    # Build test configuration
    # Enable hardware transpose (both flags required for full transpose)
    test_config = {
        "formats": formats,
        "testname": test_name,
        "dest_acc": dest_acc,
        "input_A_dimensions": input_dimensions,
        "input_B_dimensions": input_dimensions,
        "mathop": MathOperation.Elwsub,
        "math_fidelity": math_fidelity,
        "tile_cnt": tile_cnt,
        "broadcast_type": broadcast_type,
        "unpack_transpose_faces": transpose_srca,  # Enable face rearrangement
        "unpack_transpose_within_face": transpose_srca,  # Enable within-face transpose
        "num_faces": 4,
        "unpack_to_dest": False,
    }

    res_address = write_stimuli_to_l1(
        test_config,
        src_A,  # Send original data (hardware will transpose)
        src_B,
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

    # res_tensor = untilize(res_tensor)

    print("res_tensor:")
    print(res_tensor.view(input_dimensions[0], input_dimensions[1]))
    print("--------------------------------")

    assert passed_test(golden_tensor, res_tensor, formats.output_format)
