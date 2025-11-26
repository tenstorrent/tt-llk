# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
#
# SPDX-License-Identifier: Apache-2.0

from typing import List

import torch
from helpers.device import (
    BootMode,
    collect_results,
    write_pipeline_operands_to_l1,
)
from helpers.format_config import DataFormat, FormatConfig, is_dest_acc_needed
from helpers.fuse_math import BinarySfpu, Math, MatmulFpu, UnarySfpu
from helpers.fuse_operand import OperandMapping, OperandRegistry
from helpers.fuse_operation import PipelineOperation
from helpers.fuse_packer import MatmulPacker
from helpers.fuse_unpacker import MatmulUnpacker
from helpers.golden_generators import MatmulGolden, get_golden_generator
from helpers.llk_params import (
    ApproximationMode,
    DestAccumulation,
    MathFidelity,
    format_dict,
)
from helpers.matmul_sweep import (
    generate_matmul_dimension_combinations,
    generate_tile_dims,
)
from helpers.param_config import input_output_formats, parametrize
from helpers.test_config import run_fuse_test
from helpers.utils import passed_test


def generate_format_aware_matmul_combinations(
    formats_list: List[FormatConfig],
    dest_acc_modes: List[DestAccumulation],
):
    """
    Generate matmul dimension combinations for multiple tiles.

    Rules:
    1. Format outliers (Float16_b->Float16, Bfp8_b->Float16) MUST use dest_acc=Yes
    2. Running matmul tests on DestSync.Half, max tile count is 8
    3. When dest_acc=Yes: max 4 tiles (32-bit dest register)
    4. When dest_acc=No: max 8 tiles (16-bit dest register)

    Returns: List of (format, dest_acc, dimensions) tuples
    """
    combinations = []

    for fmt in formats_list:
        base_max_tiles = 4 if is_dest_acc_needed(fmt) else 8

        for dest_acc in dest_acc_modes:
            max_tiles = 4 if dest_acc == DestAccumulation.Yes else base_max_tiles
            dimensions_list = generate_matmul_dimension_combinations(max_tiles)
            combinations.extend([(fmt, dest_acc, dims) for dims in dimensions_list])

    return combinations


# Generate format-aware combinations
MATMUL_FORMATS = input_output_formats(
    [
        DataFormat.Float16_b,
        DataFormat.Float16,
        DataFormat.Float32,
    ]
)
DEST_ACC_MODES = [DestAccumulation.No, DestAccumulation.Yes]
ALL_MATMUL_COMBINATIONS = generate_format_aware_matmul_combinations(
    MATMUL_FORMATS, DEST_ACC_MODES
)


@parametrize(
    test_name="fuse_test",
    math_fidelity=[
        MathFidelity.LoFi,
        MathFidelity.HiFi2,
        MathFidelity.HiFi3,
        MathFidelity.HiFi4,
    ],
    format_dest_acc_and_dims=ALL_MATMUL_COMBINATIONS,
)
def test_matmul(
    test_name, math_fidelity, format_dest_acc_and_dims, boot_mode=BootMode.DEFAULT
):
    torch_format = format_dict[format_dest_acc_and_dims[0].output_format]

    formats = format_dest_acc_and_dims[0]
    dest_acc = format_dest_acc_and_dims[1]
    input_A_dimensions = format_dest_acc_and_dims[2][0]
    input_B_dimensions = format_dest_acc_and_dims[2][1]

    operands = OperandRegistry()

    operands.add_input(
        "input_A",
        dimensions=input_A_dimensions,
        data_format=formats.input_format,
    )

    operands.add_input(
        "input_B",
        dimensions=input_B_dimensions,
        data_format=formats.input_format,
    )

    operands.add_output("matmul_result")

    operands.add_output("final_output")

    matmul_dims = generate_tile_dims((input_A_dimensions, input_B_dimensions))

    src_A = operands.get("input_A").raw_data
    src_B = operands.get("input_B").raw_data
    tilized_A = operands.get("input_A").data
    tilized_B = operands.get("input_B").data

    generate_golden = get_golden_generator(MatmulGolden)
    golden_tensor = generate_golden(
        src_A,
        src_B,
        formats.output_format,
        math_fidelity,
        input_A_dimensions=input_A_dimensions,
        input_B_dimensions=input_B_dimensions,
        tilize=True,
    )

    test_config1 = {
        "formats": formats,
        "testname": test_name,
        "dest_acc": dest_acc,
        "math_fidelity": math_fidelity,
        "tile_cnt": matmul_dims.output_tile_cnt,
        "input_A_dimensions": input_A_dimensions,
        "input_B_dimensions": input_B_dimensions,
        "output_dimensions": matmul_dims.output_dimensions,
        "rt_dim": matmul_dims.rt_dim,
        "ct_dim": matmul_dims.ct_dim,
        "kt_dim": matmul_dims.kt_dim,
        "tilized_A": tilized_A,
        "tilized_B": tilized_B,
    }

    test_config2 = {
        "formats": formats,
        "testname": test_name,
        "dest_acc": dest_acc,
        "math_fidelity": math_fidelity,
        "tile_cnt": matmul_dims.output_tile_cnt,
        "input_A_dimensions": input_A_dimensions,
        "input_B_dimensions": input_B_dimensions,
        "output_dimensions": matmul_dims.output_dimensions,
        "rt_dim": matmul_dims.rt_dim,
        "ct_dim": matmul_dims.ct_dim,
        "kt_dim": matmul_dims.kt_dim,
        "tilized_A": tilized_A,
        "tilized_B": tilized_B,
    }

    pipeline = [
        PipelineOperation(
            unpacker=MatmulUnpacker,
            math=Math(MatmulFpu, []),
            packer=MatmulPacker,
            config=test_config1,
            operand_mapping=OperandMapping(
                inputs={"A": "input_A", "B": "input_B"},
                outputs={"result": "matmul_result"},
            ),
        ),
        PipelineOperation(
            unpacker=MatmulUnpacker,
            math=Math(
                MatmulFpu,
                [
                    UnarySfpu("sqrt", ApproximationMode.No, 32),
                    UnarySfpu("neg", ApproximationMode.Yes, 32),
                    BinarySfpu("ADD", ApproximationMode.No, 32, 0, 0, 0),
                    # SfpuWhere(ApproximationMode.No, 32, 0, 1, 2, 0),
                ],
            ),
            packer=MatmulPacker,
            config=test_config2,
            operand_mapping=OperandMapping(
                inputs={"A": "matmul_result", "B": "input_B"},
                outputs={"result": "final_output"},
            ),
        ),
    ]

    res_address = write_pipeline_operands_to_l1(
        pipeline,
        operands,
    )

    run_fuse_test(pipeline, boot_mode)

    res_from_L1 = collect_results(
        formats, tile_count=matmul_dims.output_tile_cnt, address=res_address
    )
    assert len(res_from_L1) == len(golden_tensor)

    res_tensor = torch.tensor(res_from_L1, dtype=torch_format)

    assert passed_test(golden_tensor, res_tensor, formats.output_format)
