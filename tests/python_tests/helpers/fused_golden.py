# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
#
# SPDX-License-Identifier: Apache-2.0

from typing import List

import torch

from .fused_operation import FusedOperation
from .llk_params import format_dict
from .utils import passed_test


class FuseGolden:
    def __init__(self, verbose: bool = True):
        self.verbose = verbose
        self.results = []

    def check_operation(self, operation: FusedOperation, step_number: int) -> bool:
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

        res_tensor = torch.tensor(
            output.raw_data, dtype=format_dict[output.data_format]
        )
        l1_golden = torch.tensor(output.l1_golden, dtype=format_dict[src_a.data_format])
        master_golden = torch.tensor(
            output.master_golden, dtype=format_dict[output.data_format]
        )

        if res_tensor.ndim != 1:
            res_tensor = res_tensor.flatten()
        if l1_golden.ndim != 1:
            l1_golden = l1_golden.flatten()
        if master_golden.ndim != 1:
            master_golden = master_golden.flatten()

        if self.verbose:
            print("Result:")
            head = ", ".join(f"{x:.2f}" for x in res_tensor[:32].tolist())
            tail = ", ".join(f"{x:.2f}" for x in res_tensor[-32:].tolist())
            print(f"{head}\n...\n{tail}\n")
            print("\nL1 Golden:")
            head = ", ".join(f"{x:.2f}" for x in l1_golden[:32].tolist())
            tail = ", ".join(f"{x:.2f}" for x in l1_golden[-32:].tolist())
            print(f"{head}\n...\n{tail}\n")
            print("\nMaster Golden:")
            head = ", ".join(f"{x:.2f}" for x in master_golden[:32].tolist())
            tail = ", ".join(f"{x:.2f}" for x in master_golden[-32:].tolist())
            print(f"{head}\n...\n{tail}\n")

        print("Checking l1-to-golden data... ")
        passed = passed_test(l1_golden, res_tensor, output.data_format)
        print("Checking golden-to-golden data... ")
        passed = passed_test(master_golden, res_tensor, output.data_format)

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

    def check_pipeline(self, pipeline: List[FusedOperation]) -> bool:
        for i, operation in enumerate(pipeline, start=1):
            operation.golden()
            passed = self.check_operation(operation, i)
            if not passed:
                return False

        return True

    def get_results(self) -> List[dict]:
        return self.results.copy()
