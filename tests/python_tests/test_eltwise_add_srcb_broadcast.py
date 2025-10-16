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
from helpers.format_config import BroadcastType, DataFormat
from helpers.golden_generators import EltwiseBroadcastGolden, get_golden_generator
from helpers.param_config import input_output_formats, parametrize
from helpers.stimuli_generator import generate_stimuli
from helpers.test_config import run_test
from helpers.tilize_untilize import tilize_block
from helpers.utils import passed_test


@parametrize(
    test_name="eltwise_add_srcb_broadcast_test",
    formats=input_output_formats([DataFormat.Float16_b, DataFormat.Float16]),
    mathop=[MathOperation.Elwadd, MathOperation.Elwsub, MathOperation.Elwmul],
    dest_acc=[DestAccumulation.No],
    broadcast_type=[
        BroadcastType.Row,
        BroadcastType.Column,
        BroadcastType.Scalar,
        BroadcastType.RowLast,
    ],
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
    src_A_tilized = tilize_block(
        src_A, input_dimensions, formats.input_format
    ).flatten()
    src_B_tilized = tilize_block(
        src_B, input_dimensions, formats.input_format
    ).flatten()

    # Generate golden results
    generate_golden = get_golden_generator(EltwiseBroadcastGolden)
    golden_tensor = generate_golden(
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
