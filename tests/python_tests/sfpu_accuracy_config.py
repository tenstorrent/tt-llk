# SPDX-FileCopyrightText: © 2026 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0
#
# Op registry for SFPU accuracy tests.
#
# Each entry maps an op name (string key) to a UnaryOpSpec that bundles:
#   - op_name        – string identifier, used in CSV file names
#   - mathop         – MathOperation enum value (selects the SFPU instruction via MATH_OP template)
#   - golden_fn      – callable: (torch.Tensor[float64]) → torch.Tensor[float64]
#   - approx_mode    – ApproximationMode (default: No)
#   - fast_mode      – FastMode (default: No)
#
# To add a new op:
#   1. Add an entry to OP_REGISTRY below.
#   2. Add a range profile entry in sfpu_accuracy_ranges.py.
#   (No C++ changes required – the generic kernel handles all SfpuType values.)

from dataclasses import dataclass, field
from typing import Callable

import torch
import torch.nn.functional as F
from helpers.llk_params import ApproximationMode, FastMode, MathOperation


@dataclass
class UnaryOpSpec:
    op_name: str
    mathop: MathOperation
    golden_fn: Callable
    approx_mode: ApproximationMode = field(default_factory=lambda: ApproximationMode.No)
    fast_mode: FastMode = field(default_factory=lambda: FastMode.No)


OP_REGISTRY: dict[str, UnaryOpSpec] = {
    "exp": UnaryOpSpec(
        op_name="exp",
        mathop=MathOperation.Exp,
        golden_fn=torch.exp,
    ),
    "log": UnaryOpSpec(
        op_name="log",
        mathop=MathOperation.Log,
        golden_fn=torch.log,
    ),
    "tanh": UnaryOpSpec(
        op_name="tanh",
        mathop=MathOperation.Tanh,
        golden_fn=torch.tanh,
    ),
    "relu": UnaryOpSpec(
        op_name="relu",
        mathop=MathOperation.Relu,
        golden_fn=torch.relu,
    ),
    "gelu": UnaryOpSpec(
        op_name="gelu",
        mathop=MathOperation.Gelu,
        golden_fn=lambda x: F.gelu(x.float()).double(),
    ),
    "sqrt": UnaryOpSpec(
        op_name="sqrt",
        mathop=MathOperation.Sqrt,
        golden_fn=torch.sqrt,
    ),
    "rsqrt": UnaryOpSpec(
        op_name="rsqrt",
        mathop=MathOperation.Rsqrt,
        golden_fn=torch.rsqrt,
    ),
    "reciprocal": UnaryOpSpec(
        op_name="reciprocal",
        mathop=MathOperation.Reciprocal,
        golden_fn=torch.reciprocal,
    ),
}
