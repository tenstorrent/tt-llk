# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
#
# SPDX-License-Identifier: Apache-2.0

from typing import List

import torch

from .fuse_math import BinarySfpu, MatmulFpu, UnarySfpu
from .fuse_operation import PipelineOperation
from .golden_generators import (
    BinarySFPUGolden,
    MatmulGolden,
    UnarySFPUGolden,
    get_golden_generator,
)
from .utils import passed_test


class FuseGolden:
    def __init__(self, verbose: bool = True):
        self.verbose = verbose
        self.results = []

    def check_operation(self, operation: PipelineOperation, step_number: int) -> bool:
        if self.verbose:
            print(f"\n{'='*60}")
            print(f"Operation {step_number}")
            print(f"{'='*60}")

        src_a = operation.src_a
        src_b = operation.src_b
        output = operation.output

        if self.verbose:
            print(
                f"  Input A: {src_a.name} (dims: {src_a.dimensions}, format: {src_a.data_format})"
            )
            print(
                f"  Input B: {src_b.name} (dims: {src_b.dimensions}, format: {src_b.data_format})"
            )
            print(
                f"  Output:  {output.name} (dims: {output.dimensions}, format: {output.data_format})"
            )
            print(f"  Math Fidelity: {operation.math_fidelity}")
            print(f"  Dest Accumulation: {operation.dest_acc}")

        golden_tensor = self._generate_golden(operation, src_a, src_b)

        res_tensor = output.data
        passed = passed_test(golden_tensor, res_tensor, output.data_format)

        result = {
            "step": step_number,
            "operation": str(operation.math.__class__.__name__),
            "src_a": src_a.name,
            "src_b": src_b.name,
            "output": output.name,
            "passed": passed,
        }
        self.results.append(result)

        if self.verbose:
            print("✓ PASS") if passed else print("✗ FAIL")

        return passed

    def _generate_golden(
        self, operation: PipelineOperation, src_a, src_b
    ) -> torch.Tensor:
        math_fidelity = operation.math_fidelity
        dest_acc = operation.dest_acc

        output = operation.output

        from .format_config import InputOutputFormat

        formats = InputOutputFormat(
            input_format=src_a.data_format, output_format=output.data_format
        )

        if isinstance(operation.math.fpu, type) and issubclass(
            operation.math.fpu, MatmulFpu
        ):
            return self._generate_matmul_golden(
                operation, src_a, src_b, formats, math_fidelity
            )
        else:
            return src_a.raw_data

    def _generate_matmul_golden(
        self, operation, src_a, src_b, formats, math_fidelity
    ) -> torch.Tensor:
        generate_golden = get_golden_generator(MatmulGolden)
        golden = generate_golden(
            src_a.raw_data,
            src_b.raw_data,
            formats.output,
            math_fidelity,
            input_A_dimensions=src_a.dimensions,
            input_B_dimensions=src_b.dimensions,
            tilize=True,
        )

        golden = self._apply_sfpu_operations(operation, golden, formats)

        return golden

    def _apply_sfpu_operations(self, operation, golden, formats) -> torch.Tensor:
        dest_acc = operation.dest_acc

        for sfpu_op in operation.math.sfpu:
            if isinstance(sfpu_op, UnarySfpu):
                golden = self._apply_unary_sfpu(sfpu_op, golden, formats, dest_acc)
            elif isinstance(sfpu_op, BinarySfpu):
                golden = self._apply_binary_sfpu(sfpu_op, golden, formats)

        return golden

    def _apply_unary_sfpu(self, sfpu_op, golden, formats, dest_acc) -> torch.Tensor:
        generate_sfpu_golden = get_golden_generator(UnarySFPUGolden)

        return generate_sfpu_golden(
            sfpu_op.operation,
            golden,
            formats.output,
            dest_acc,
            formats.input,
        )

    def _apply_binary_sfpu(self, sfpu_op, golden, formats) -> torch.Tensor:
        generate_binary_golden = get_golden_generator(BinarySFPUGolden)
        return generate_binary_golden(sfpu_op.operation, golden, golden, formats.output)

    def check_pipeline(self, pipeline: List[PipelineOperation]) -> bool:
        for i, operation in enumerate(pipeline, start=1):
            passed = self.check_operation(operation, i)
            if not passed:
                return False

        return True

    def get_results(self) -> List[dict]:
        return self.results.copy()
