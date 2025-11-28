# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
#
# SPDX-License-Identifier: Apache-2.0

from typing import List

from helpers.device import (
    BootMode,
    collect_pipeline_results,
    write_pipeline_operands_to_l1,
)
from helpers.format_config import DataFormat, FormatConfig, is_dest_acc_needed
from helpers.fuse_golden import FuseGolden
from helpers.fuse_math import BinarySfpu, Math, MatmulFpu, UnarySfpu
from helpers.fuse_operand import OperandRegistry
from helpers.fuse_operation import PipelineOperation
from helpers.fuse_packer import MatmulPacker
from helpers.fuse_unpacker import MatmulUnpacker
from helpers.llk_params import (
    ApproximationMode,
    DestAccumulation,
    MathFidelity,
    MathOperation,
)
from helpers.matmul_sweep import generate_matmul_dimension_combinations
from helpers.param_config import input_output_formats, parametrize
from helpers.test_config import run_fuse_test


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
            combinations.extend(
                [
                    (fmt, dest_acc, dims)
                    for dims in [[[32, 32], [32, 32]], [[64, 64], [64, 64]]]
                ]
            )

    return combinations


# Generate format-aware combinations
MATMUL_FORMATS = input_output_formats(
    [
        DataFormat.Float16_b,
        # DataFormat.Float16,
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
    formats = format_dest_acc_and_dims[0]
    dest_acc = format_dest_acc_and_dims[1]
    input_A_dimensions = format_dest_acc_and_dims[2][0]
    input_B_dimensions = format_dest_acc_and_dims[2][1]

    operands = OperandRegistry()

    pipeline = [
        PipelineOperation(
            operand_mapping=operands.create_mapping(
                src_a="input_A",
                src_b="input_B",
                output="matmul_result",
                src_a_dims=input_A_dimensions,
                src_b_dims=input_B_dimensions,
                input_format=formats.input_format,
                output_format=formats.output_format,
            ),
            unpacker=MatmulUnpacker,
            math=Math(MatmulFpu, []),
            packer=MatmulPacker,
            dest_acc=dest_acc,
            math_fidelity=math_fidelity,
        ),
        PipelineOperation(
            operand_mapping=operands.create_mapping(
                src_a="matmul_result",
                src_b="input_C",
                output="final_output",
                src_b_dims=input_B_dimensions,
                input_format=formats.output_format,
                output_format=formats.output_format,
            ),
            unpacker=MatmulUnpacker,
            math=Math(
                MatmulFpu,
                [
                    UnarySfpu(
                        MathOperation.Sqrt,
                        ApproximationMode.No,
                        32 * operands.get("final_output").tile_count,
                    ),
                    UnarySfpu(
                        MathOperation.Neg,
                        ApproximationMode.No,
                        32 * operands.get("final_output").tile_count,
                    ),
                    BinarySfpu(
                        MathOperation.SfpuElwadd,
                        ApproximationMode.No,
                        32 * operands.get("final_output").tile_count,
                        0,
                        0,
                        0,
                    ),
                ],
            ),
            packer=MatmulPacker,
            dest_acc=dest_acc,
            math_fidelity=math_fidelity,
        ),
    ]

    write_pipeline_operands_to_l1(pipeline)

    run_fuse_test(pipeline, boot_mode)

    collect_pipeline_results(pipeline)

    golden = FuseGolden()
    assert golden.check_pipeline(pipeline)
