# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
#
# SPDX-License-Identifier: Apache-2.0

from typing import List

import torch

from .fuse_operation import PipelineOperation
from .llk_params import format_dict
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

        golden_tensor = operation.golden()

        res_tensor = torch.tensor(
            output.raw_data, dtype=format_dict[output.data_format]
        )

        if golden_tensor.ndim != 1:
            golden_tensor = golden_tensor.flatten()
        if res_tensor.ndim != 1:
            res_tensor = res_tensor.flatten()

        if self.verbose:
            print("\nGolden:")
            head = ", ".join(f"{x:.2f}" for x in golden_tensor[:32].tolist())
            tail = ", ".join(f"{x:.2f}" for x in golden_tensor[-32:].tolist())
            print(f"{head}\n...\n{tail}\n")
            print("Result:")
            head = ", ".join(f"{x:.2f}" for x in res_tensor[:32].tolist())
            tail = ", ".join(f"{x:.2f}" for x in res_tensor[-32:].tolist())
            print(f"{head}\n...\n{tail}\n")

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

    def check_pipeline(self, pipeline: List[PipelineOperation]) -> bool:
        for i, operation in enumerate(pipeline, start=1):
            passed = self.check_operation(operation, i)
            if not passed:
                return False

        return True

    def get_results(self) -> List[dict]:
        return self.results.copy()
