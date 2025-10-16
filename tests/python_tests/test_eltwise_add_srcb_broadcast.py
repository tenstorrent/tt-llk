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
from helpers.tilize_untilize import tilize, tilize_block
from helpers.utils import passed_test


def generate_broadcast_golden(
    src_a, src_b, broadcast_type, math_op, output_format, tile_count
):

    # Convert to tensors if needed
    if not isinstance(src_a, torch.Tensor):
        src_a = torch.tensor(src_a, dtype=format_dict[output_format])
    if not isinstance(src_b, torch.Tensor):
        src_b = torch.tensor(src_b, dtype=format_dict[output_format])

    # Since inputs are already tilized, we need to untilize them first for proper broadcast
    from helpers.tilize_untilize import tilize, untilize

    results = []
    elements_per_tile = 32 * 32

    for tile_idx in range(tile_count):
        # Extract current tile data (already in tilized format)
        start_idx = tile_idx * elements_per_tile
        end_idx = (tile_idx + 1) * elements_per_tile

        a_tile = src_a[start_idx:end_idx]
        b_tile = src_b[start_idx:end_idx]

        # Untilize current tiles to get them in row-major format
        a_untilized = untilize(a_tile, output_format).view(32, 32)
        b_untilized = untilize(b_tile, output_format).view(32, 32)

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

        # Tilize the result back and add to results
        result_tilized = tilize(result.flatten(), output_format)
        results.append(result_tilized)

    # Concatenate all tile results
    return torch.cat(results).to(dtype=format_dict[output_format])


@parametrize(
    test_name="eltwise_add_srcb_broadcast_test",
    formats=input_output_formats([DataFormat.Float16_b, DataFormat.Float16]),
    mathop=[MathOperation.Elwadd, MathOperation.Elwsub, MathOperation.Elwmul],
    dest_acc=[DestAccumulation.No],
    broadcast_type=["ROW", "COL", "SCALAR", "ROW_LAST"],
    math_fidelity=[MathFidelity.LoFi],
    input_dimensions=[
        [32, 32],
        [32, 64],
        [64, 64],
        [64, 32],
    ],  # Support multiple tile configurations
)
def test_eltwise_add_srcb_broadcast(
    test_name,
    formats,
    mathop,
    dest_acc,
    broadcast_type,
    math_fidelity,
    input_dimensions,
):

    # Generate stimuli - single tile for each operand
    src_A, src_B, tile_cnt = generate_stimuli(
        formats.input_format, formats.input_format, input_dimensions=input_dimensions
    )

    # Tilize inputs (convert from row-major to tile format)
    if tile_cnt == 1:
        # For single tile, use regular tilize
        src_A_tilized = tilize(src_A)
        src_B_tilized = tilize(src_B)
    else:
        # For multiple tiles, use tilize_block
        src_A_tilized = tilize_block(
            src_A, input_dimensions, formats.input_format
        ).flatten()
        src_B_tilized = tilize_block(
            src_B, input_dimensions, formats.input_format
        ).flatten()

    # Generate golden results
    golden_tensor = generate_broadcast_golden(
        src_A_tilized,
        src_B_tilized,
        broadcast_type,
        mathop,
        formats.output_format,
        tile_cnt,
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

    assert passed_test(golden_tensor, res_tensor, formats.output_format)
