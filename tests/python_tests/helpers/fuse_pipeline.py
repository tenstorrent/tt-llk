# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
#
# SPDX-License-Identifier: Apache-2.0

from typing import List

from helpers.fuse_math import BinarySfpu, EltwiseFpu, Math, MatmulFpu, UnarySfpu
from helpers.fuse_operand import OperandRegistry
from helpers.fuse_operation import PipelineOperation
from helpers.fuse_packer import EltwisePacker, MatmulPacker
from helpers.fuse_unpacker import EltwiseUnpacker, MatmulUnpacker
from helpers.llk_params import (
    ApproximationMode,
    MathOperation,
)


def create_fuse_pipeline(
    format_dest_acc_and_dims,
    math_fidelity,
) -> List[PipelineOperation]:
    formats = format_dest_acc_and_dims[0]
    dest_acc = format_dest_acc_and_dims[1]
    input_A_dimensions = format_dest_acc_and_dims[2][0]
    input_B_dimensions = format_dest_acc_and_dims[2][1]

    operands = OperandRegistry()

    pipeline = [
        PipelineOperation(
            operand_mapping=operands.create_mapping(
                src_a="input_A",
                src_b="input_A",
                output="elwadd1",
                src_a_dims=input_A_dimensions,
                src_b_dims=input_A_dimensions,
                input_format=formats.input_format,
                output_format=formats.output_format,
            ),
            unpacker=EltwiseUnpacker,
            math=Math(EltwiseFpu(MathOperation.Elwadd), []),
            packer=EltwisePacker,
            dest_acc=dest_acc,
            math_fidelity=math_fidelity,
        ),
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
            math=Math(MatmulFpu(), []),
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
                MatmulFpu(),
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

    return pipeline
