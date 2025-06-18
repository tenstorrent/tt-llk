# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
import math

import torch

from helpers.format_arg_mapping import (
    MathFidelity,
    MathOperation,
    ReduceDimension,
    ReducePool,
    format_dict,
)

golden_registry = {}


def register_golden(cls):
    """Register a golden class by its type."""
    golden_registry[cls] = cls()
    return cls


def get_golden_generator(cls):
    """Retrieve the registered golden class instance."""
    if cls not in golden_registry:
        raise KeyError(f"Golden class {cls.__name__} is not registered.")
    return golden_registry[cls]


class FidelityMasking:
    def _apply_fidelity_masking(self, operand1, operand2, math_fidelity):
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

        self._apply_fidelity_masking(operand1, operand2, math_fidelity)

        # Clone and detach to avoid modifying original input
        operand1_matrix = to_tensor(operand1, data_format).view(32, 32)
        operand2_matrix = to_tensor(operand2, data_format).view(32, 32)

        return (
            torch.matmul(operand1_matrix, operand2_matrix).view(1024).to(torch_format)
        )


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
        self.data_format = None

    def __call__(self, operation, operand1, data_format):
        if operation not in self.ops:
            raise ValueError(f"Unsupported operation: {operation}")
        self.data_format = data_format
        tensor = to_tensor(operand1, data_format)
        result = [self.ops[operation](x) for x in tensor.tolist()]
        return torch.tensor(result, dtype=format_dict[data_format])

    # Operation methods
    def _abs(self, x):
        return abs(x)

    def _cos(self, x):
        return math.cos(x)

    def _log(self, x):
        return math.log(x) if x != 0 else float("nan")

    def _reciprocal(self, x):
        return 1 / x if x != 0 else float("nan")

    def _sin(self, x):
        return math.sin(x)

    def _sqrt(self, x):
        return math.sqrt(x)

    def _square(self, x):
        return x * x

    def _celu(self, x):
        input_tensor = (
            x
            if isinstance(x, torch.Tensor)
            else torch.tensor(x, dtype=format_dict[self.data_format])
        )
        return torch.nn.functional.celu(input_tensor, alpha=1.0).item()


@register_golden
class EltwiseBinaryGolden(FidelityMasking):
    def __init__(self):
        self.ops = {
            MathOperation.Elwadd: self._add,
            MathOperation.Elwsub: self._sub,
            MathOperation.Elwmul: self._mul,
        }

    def __call__(self, op, operand1, operand2, data_format, math_fidelity):
        if op not in self.ops:
            raise ValueError(f"Unsupported Eltwise operation: {op}")

        t1 = to_tensor(operand1, data_format)
        t2 = to_tensor(operand2, data_format)

        self._apply_fidelity_masking(t1, t2, math_fidelity)

        return self.ops[op](t1, t2)

    # Operation methods
    def _add(self, t1, t2):
        return t1 + t2

    def _sub(self, t1, t2):
        return t1 - t2

    def _mul(self, t1, t2):
        return t1 * t2


@register_golden
class BinarySFPUGolden(EltwiseBinaryGolden):
    def __init__(self):
        self.ops = {
            MathOperation.SfpuElwadd: self._add,
            MathOperation.SfpuElwsub: self._sub,
            MathOperation.SfpuElwmul: self._mul,
            MathOperation.SfpuXlogy: self._xlogy,
            MathOperation.SfpuElwRightShift: self._right_shift,
            MathOperation.SfpuElwLeftShift: self._left_shift,
        }

    def __call__(self, operation, operand1, operand2, data_format):
        if operation not in self.ops:
            raise ValueError(f"Unsupported SFPU operation: {operation}")

        t1 = to_tensor(operand1, data_format)
        t2 = to_tensor(operand2, data_format)

        return self.ops[operation](t1, t2)

    # Operation methods are cover by Eltwise Binary Golden
    def _xlogy(self, t1, t2):
        return torch.xlogy(t1, t2)

    def _right_shift(self, t1, t2):
        return torch.bitwise_right_shift(t1, t2)

    def _left_shift(self, t1, t2):
        return torch.bitwise_left_shift(t1, t2)


@register_golden
class FillDestGolden(EltwiseBinaryGolden):
    def __call__(self, operand1, operand2, data_format, op):

        tensor1 = to_tensor(operand1, data_format)
        tensor2 = to_tensor(operand2, data_format)

        res = []
        max_tiles_in_dest = 8 if data_format.is_32_bit() else 16
        for i in range(max_tiles_in_dest):
            res.extend(self.ops[op](tensor1, tensor2).tolist())
        return torch.tensor(res, dtype=format_dict[data_format])


@register_golden
class ReduceGolden:
    def __init__(self):
        self.dim_handlers = {
            ReduceDimension.Column: self._reduce_column,
            ReduceDimension.Row: self._reduce_row,
            ReduceDimension.Scalar: self._reduce_scalar,
        }

    def __call__(self, operand, reduce_dim, pool_type, data_format):
        if reduce_dim not in self.dim_handlers:
            raise ValueError(f"Unsupported reduce dimension: {reduce_dim}")

        f0 = operand[:256].view(16, 16)
        f1 = operand[256:512].view(16, 16)
        f2 = operand[512:768].view(16, 16)
        f3 = operand[768:].view(16, 16)
        faces = [f0, f1, f2, f3]
        if reduce_dim == ReduceDimension.Scalar:
            faces = operand
        return self.dim_handlers[reduce_dim](faces, pool_type, data_format)

    def _reduce_column(self, faces, pool_type, data_format):
        left_half = torch.cat((faces[0], faces[2]), 0)
        right_half = torch.cat((faces[1], faces[3]), 0)

        result = torch.zeros(32, 32, dtype=format_dict[data_format])
        result[0, 0:16] = self._apply_pooling(left_half, pool_type, dim=0)
        result[0, 16:32] = self._apply_pooling(right_half, pool_type, dim=0)

        return result.view(1024)

    def _reduce_row(self, faces, pool_type, data_format):
        left_half = torch.cat((faces[0], faces[2]), 1)
        right_half = torch.cat((faces[1], faces[3]), 1)

        result = torch.zeros(32, 32, dtype=format_dict[data_format])
        result[0:16, 0] = self._apply_pooling(left_half, pool_type, dim=1).view(16)
        result[16:32, 0] = self._apply_pooling(right_half, pool_type, dim=1).view(16)

        return result.view(1024)

    def _reduce_scalar(self, operand, pool_type, data_format):
        tensor = operand.view(1024)
        result = torch.zeros(32, 32, dtype=format_dict[data_format])
        result[0, 0] = self._apply_pooling(tensor, pool_type, dim=0)
        return result.view(1024)

    def _apply_pooling(self, tensor, pool_type, dim):
        if pool_type == ReducePool.Max:
            return torch.max(tensor, dim=dim).values
        elif pool_type == ReducePool.Average:
            return torch.mean(tensor, dim=dim)
        elif pool_type == ReducePool.Sum:
            return torch.sum(tensor, dim=dim)
        else:
            raise ValueError(f"Unsupported pool type: {pool_type}")


@register_golden
class UntilizeGolden:
    def __call__(self, operand, data_format):
        from helpers.tilize_untilize import untilize

        result = untilize(operand, data_format)
        return result.flatten()


@register_golden
class TilizeGolden:
    def __call__(self, operand, data_format):
        from helpers.tilize_untilize import tilize

        result = tilize(operand, data_format)
        return result.flatten()
