# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0

import torch
from helpers.device import collect_results, write_stimuli_to_l1
from helpers.format_arg_mapping import (
    DestAccumulation,
    MathFidelity,
    MathOperation,
    format_dict,
)
from helpers.format_config import DataFormat
from helpers.param_config import input_output_formats, parametrize
from helpers.stimuli_generator import generate_stimuli
from helpers.test_config import run_test
from helpers.tilize_untilize import tilize
from helpers.utils import passed_test


def generate_broadcast_golden(src_a, src_b, broadcast_type, math_op, output_format):
    """Generate golden results for broadcast operations.

    Args:
        src_a: Source A tensor (already tilized, 1024 elements)
        src_b: Source B tensor (already tilized, 1024 elements)
        broadcast_type: Type of broadcast (ROW, COL, SCALAR)
        math_op: Math operation to perform
        output_format: Output data format

    Returns:
        Golden tensor with broadcast operation applied
    """
    # Convert to tensors if needed
    if not isinstance(src_a, torch.Tensor):
        src_a = torch.tensor(src_a, dtype=format_dict[output_format])
    if not isinstance(src_b, torch.Tensor):
        src_b = torch.tensor(src_b, dtype=format_dict[output_format])

    # Since inputs are already tilized, we need to untilize them first for proper broadcast
    from helpers.tilize_untilize import untilize

    a_untilized = untilize(src_a, output_format).view(32, 32)
    b_untilized = untilize(src_b, output_format).view(32, 32)

    if broadcast_type == "ROW":
        # Broadcast first row of B to all rows
        b_broadcasted = b_untilized[0:1, :].expand(32, 32)
    elif broadcast_type == "ROW_LAST":
        # Broadcast last row (row 31) of B to all rows
        b_broadcasted = b_untilized[-1:, :].expand(32, 32)
    elif broadcast_type == "COL":
        # Broadcast first column of B to all columns
        b_broadcasted = b_untilized[:, 0:1].expand(32, 32)
    elif broadcast_type == "SCALAR":
        # Broadcast single element to entire tile
        b_broadcasted = b_untilized[0, 0].expand(32, 32)
    else:
        # No broadcast
        b_broadcasted = b_untilized

    # Perform operation
    if math_op == MathOperation.Elwadd:
        result = a_untilized + b_broadcasted
    elif math_op == MathOperation.Elwsub:
        result = a_untilized - b_broadcasted
    elif math_op == MathOperation.Elwmul:
        result = a_untilized * b_broadcasted
    else:
        raise ValueError(f"Unsupported math operation: {math_op}")

    # Tilize the result back
    result_tilized = tilize(result.flatten())
    return result_tilized.to(dtype=format_dict[output_format])


@parametrize(
    test_name="eltwise_add_srcb_broadcast_test",
    formats=input_output_formats(
        [
            DataFormat.Float16_b,
        ]
    ),
    mathop=[MathOperation.Elwadd],
    dest_acc=[DestAccumulation.No],
    broadcast_type=["ROW", "COL", "SCALAR", "ROW_LAST"],
    math_fidelity=[MathFidelity.LoFi],
)
def test_eltwise_add_srcb_broadcast(
    test_name,
    formats,
    mathop,
    dest_acc,
    broadcast_type,
    math_fidelity,
):
    """Test element-wise addition with srcB broadcast.

    This test performs element-wise addition with broadcasting on srcB.
    Input dimensions are fixed at 32x32 (single tile).
    All srcB broadcast options are swept: ROW, COL, SCALAR.
    """

    # Fixed input dimensions of 32x32
    input_dimensions = [32, 32]

    # Generate stimuli - single tile for each operand
    src_A, src_B, tile_cnt = generate_stimuli(
        formats.input_format, formats.input_format, input_dimensions=input_dimensions
    )

    src_a = torch.ones(1024)
    src_b = torch.ones(1024)
    src_b[0:32] = 2
    src_b[-32:] = 4

    # Tilize inputs (convert from row-major to tile format)
    src_A_tilized = tilize(src_a)
    src_B_tilized = tilize(src_b)

    # Generate golden results
    golden_tensor = generate_broadcast_golden(
        src_A_tilized, src_B_tilized, broadcast_type, mathop, formats.output_format
    )

    # Test configuration
    test_config = {
        "formats": formats,
        "testname": test_name,
        "dest_acc": dest_acc,
        "input_A_dimensions": input_dimensions,
        "input_B_dimensions": input_dimensions,
        "mathop": mathop,
        "math_fidelity": math_fidelity,
        "tile_cnt": tile_cnt,
        "broadcast_type": broadcast_type,
    }

    # Write stimuli to L1
    res_address = write_stimuli_to_l1(
        test_config,
        src_A_tilized,
        src_B_tilized,
        formats.input_format,
        formats.input_format,
        tile_count_A=tile_cnt,
        tile_count_B=tile_cnt,
    )

    # Run the test
    run_test(test_config)

    # Collect results
    res_from_L1 = collect_results(formats, tile_count=tile_cnt, address=res_address)
    assert len(res_from_L1) == len(golden_tensor)

    # Convert to tensor and compare
    torch_format = format_dict[formats.output_format]
    res_tensor = torch.tensor(res_from_L1, dtype=torch_format)

    print("Golden tensor:")
    print(golden_tensor.view(32, 32))
    print("Res tensor:")
    print(res_tensor.view(32, 32))

    assert passed_test(golden_tensor, res_tensor, formats.output_format)
