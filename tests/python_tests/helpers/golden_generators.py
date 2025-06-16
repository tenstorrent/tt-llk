# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
import math

import torch

from helpers.format_arg_mapping import (
    MathFidelity,
    MathOperation,
    format_dict,
)
from helpers.format_config import DataFormat

golden_registry = {}


def register_golden(cls):
    """Register a golden class by its type."""
    golden_registry[cls] = cls()
    return cls


def get_golden(cls):
    """Retrieve the registered golden class instance."""
    if cls not in golden_registry:
        raise KeyError(f"Golden class {cls.__name__} is not registered.")
    return golden_registry[cls]


class FidelityMasking:
    def apply_fidelity_masking(self, operand1, operand2, math_fidelity):
        if math_fidelity in [MathFidelity.LoFi, MathFidelity.HiFi2]:
            # Apply mask to operand2
            for element in operand1:
                element = (
                    element.to(torch.int32) & 0xFFFE
                )  # ⚠️ fix: this line does not change the tensor in place

        if math_fidelity == MathFidelity.LoFi:
            # Apply stricter mask to operand1
            for element in operand2:
                element = (
                    element.to(torch.int32) & 0xFFF8
                )  # ⚠️ fix: this line does not change the tensor in place


def to_tensor(operand, data_format):
    torch_format = format_dict.get(data_format)
    return operand.clone().detach().to(torch_format)


@register_golden
class MatmulGolden(FidelityMasking):
    def __call__(self, operand1, operand2, data_format, math_fidelity):
        torch_format = format_dict[data_format]

        # Clone and detach to avoid modifying original input
        operand1_matrix = to_tensor(operand1, data_format).view(32, 32)
        operand2_matrix = to_tensor(operand2, data_format).view(32, 32)

        self.apply_fidelity_masking(operand1, operand2, math_fidelity)

        result = torch.matmul(operand1_matrix, operand2_matrix)
        return result.view(1024).to(torch_format)


@register_golden
class DataCopyGolden:
    def __call__(self, operand1, data_format):
        torch_format = format_dict[data_format]
        return torch.tensor(operand1, dtype=torch_format)


@register_golden
class UnarySFPUGolden:
    def __init__(self):
        self.ops = {
            MathOperation.Abs: self._abs,
            MathOperation.Cos: self._cos,
            MathOperation.Log: self._log,
            MathOperation.Reciprocal: self._reciprocal,
            MathOperation.Sin: self._sin,
            MathOperation.Sqrt: self._sqrt,
            MathOperation.Square: self._square,
            MathOperation.Celu: self._celu,
        }

    def __call__(self, operation, operand1, data_format):
        if operation not in self.ops:
            raise ValueError(f"Unsupported operation: {operation}")

        tensor = to_tensor(operand1, data_format)
        result = [self.ops[operation](x, data_format) for x in tensor.tolist()[:1024]]
        return torch.tensor(result, dtype=format_dict[data_format])

    # Operation methods
    def _abs(self, x, fmt):
        return abs(x)

    def _cos(self, x, fmt):
        return math.cos(x)

    def _log(self, x, fmt):
        return math.log(x) if x != 0 else float("nan")

    def _reciprocal(self, x, fmt):
        return 1 / x if x != 0 else float("nan")

    def _sin(self, x, fmt):
        return math.sin(x)

    def _sqrt(self, x, fmt):
        return math.sqrt(x)

    def _square(self, x, fmt):
        return x * x

    def _celu(self, x, data_format):
        input_tensor = (
            x
            if isinstance(x, torch.Tensor)
            else torch.tensor(x, dtype=format_dict[data_format])
        )
        return torch.nn.functional.celu(input_tensor, alpha=1.0).item()


@register_golden
class EltwiseBinaryGolden(FidelityMasking):
    def __call__(self, op, operand1, operand2, data_format, math_fidelity):
        tensor1 = to_tensor(operand1, data_format)
        tensor2 = to_tensor(operand2, data_format)

        if data_format == DataFormat.Float16_b:
            self.apply_fidelity_masking(tensor1, tensor2, math_fidelity)

        return self.apply_eltwise_op(op, tensor1, tensor2)

    def apply_eltwise_op(self, op, t1, t2):
        if op == MathOperation.Elwadd:
            return t1 + t2
        elif op == MathOperation.Elwsub:
            return t1 - t2
        elif op == MathOperation.Elwmul:
            return t1 * t2
        else:
            raise ValueError(f"Unsupported operation: {op}")


@register_golden
class FillDestGolden(EltwiseBinaryGolden):
    def __call__(self, operand1, operand2, data_format, op):

        tensor1 = to_tensor(operand1, format_dict[data_format])
        tensor2 = to_tensor(operand2, format_dict[data_format])

        res = []
        max_tiles_in_dest = 8 if data_format.is_32_bit() else 16
        for i in range(max_tiles_in_dest):
            res.extend(self.apply_eltwise_op(op, tensor1, tensor2).tolist())
        return torch.tensor(res, dtype=format_dict[data_format])


@register_golden
class UntilizeGolden:
    def __call__(self, operand1, data_format):
        from helpers.tilize_untilize import untilize

        result = untilize(operand1, data_format)
        return result.flatten()


@register_golden
class TilizeGolden:
    def __call__(self, operand1, data_format):
        from helpers.tilize_untilize import tilize

        result = tilize(operand1, data_format)
        return result.flatten()
