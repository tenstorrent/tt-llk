# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0

from collections import namedtuple
from enum import Enum, auto

import torch

from .format_config import DataFormat

format_dict = {
    DataFormat.Float32: torch.float32,
    DataFormat.Float16: torch.float16,
    DataFormat.Float16_b: torch.bfloat16,
    DataFormat.Bfp8_b: torch.bfloat16,  # BFP8 not native to PyTorch, is represented as bfloat16
    DataFormat.Int32: torch.int32,
    DataFormat.UInt32: torch.int64,
    DataFormat.UInt16: torch.int32,
    DataFormat.Int8: torch.int8,
    DataFormat.UInt8: torch.uint8,
}


class MathOpType(Enum):
    """Enum for different types of math operations."""

    SFPU_UNARY = auto()
    SFPU_BINARY = auto()
    FPU_BINARY = auto()
    REDUCE = auto()


# Named tuple for operation specification
OpSpec = namedtuple("OpSpec", ["cpp_name", "operation_type"])


class MathOperation(Enum):
    """
    An enumeration class that holds all the math operations supported by the LLKs.
    Each enum value is an OpSpec namedtuple containing (cpp_name, operation_type).
    Used to avoid hardcoding the operation strings in the test scripts using strings. This avoid typos and future errors.
    MathOperations(Enum) class instances can be compared via unique values.
    When you have a set of related constants and you want to leverage the benefits of enumeration (unique members, comparisons, introspection, etc.).
    It's a good choice for things like state machines, categories, or settings where values should not be changed or duplicated.
    """

    # FPU binary operations (sorted alphabetically)
    Elwadd = OpSpec("ELWADD", MathOpType.FPU_BINARY)
    Elwmul = OpSpec("ELWMUL", MathOpType.FPU_BINARY)
    Elwsub = OpSpec("ELWSUB", MathOpType.FPU_BINARY)

    # SFPU unary operations (sorted alphabetically)
    Abs = OpSpec("abs", MathOpType.SFPU_UNARY)
    Celu = OpSpec("celu", MathOpType.SFPU_UNARY)
    Cos = OpSpec("cosine", MathOpType.SFPU_UNARY)
    Gelu = OpSpec("gelu", MathOpType.SFPU_UNARY)
    Log = OpSpec("log", MathOpType.SFPU_UNARY)
    Neg = OpSpec("neg", MathOpType.SFPU_UNARY)
    Reciprocal = OpSpec("reciprocal", MathOpType.SFPU_UNARY)
    Sin = OpSpec("sine", MathOpType.SFPU_UNARY)
    Silu = OpSpec("silu", MathOpType.SFPU_UNARY)
    Sqrt = OpSpec("sqrt", MathOpType.SFPU_UNARY)
    Square = OpSpec("square", MathOpType.SFPU_UNARY)

    # SFPU binary operations (sorted alphabetically)
    SfpuElwadd = OpSpec("ADD", MathOpType.SFPU_BINARY)
    SfpuElwLeftShift = OpSpec("LSHFT", MathOpType.SFPU_BINARY)
    SfpuElwLogicalRightShift = OpSpec("LOGICAL_RSHFT", MathOpType.SFPU_BINARY)
    SfpuElwmul = OpSpec("MUL", MathOpType.SFPU_BINARY)
    SfpuElwRightShift = OpSpec("RSHFT", MathOpType.SFPU_BINARY)
    SfpuElwsub = OpSpec("SUB", MathOpType.SFPU_BINARY)
    SfpuXlogy = OpSpec("XLOGY", MathOpType.SFPU_BINARY)

    # Reduce operations (sorted alphabetically)
    ReduceColumn = OpSpec("REDUCE_COL", MathOpType.REDUCE)
    ReduceRow = OpSpec("REDUCE_ROW_", MathOpType.REDUCE)
    ReduceScalar = OpSpec("REDUCE_SCALAR", MathOpType.REDUCE)

    @property
    def cpp_name(self):
        """Get the C++ constant for this operation."""
        return self.value.cpp_name

    @property
    def operation_type(self):
        """Get the operation type for this operation."""
        return self.value.operation_type


# Dynamically generate operation type sets
def _get_operations_by_type(op_type: MathOpType):
    """Get all operations of a specific type."""
    return {op for op in MathOperation if op.operation_type == op_type}


SFPU_UNARY_OPERATIONS = _get_operations_by_type(MathOpType.SFPU_UNARY)
SFPU_BINARY_OPERATIONS = _get_operations_by_type(MathOpType.SFPU_BINARY)
FPU_BINARY_OPERATIONS = _get_operations_by_type(MathOpType.FPU_BINARY)
REDUCE_OPERATIONS = _get_operations_by_type(MathOpType.REDUCE)


class ReduceDimension(Enum):
    Column = "ReduceDim::REDUCE_COL"
    Row = "ReduceDim::REDUCE_ROW"
    Scalar = "ReduceDim::REDUCE_SCALAR"
    No = " "


class ReducePool(Enum):
    Max = "PoolType::MAX"
    Sum = "PoolType::SUM"
    Average = "PoolType::AVG"
    No = " "


class DestAccumulation(Enum):
    Yes = "DEST_ACC"
    No = ""


class ApproximationMode(Enum):
    Yes = "true"
    No = "false"


class MathFidelity(Enum):
    LoFi = 0
    HiFi2 = 2
    HiFi3 = 3
    HiFi4 = 4
    Invalid = 5


class Mailbox(Enum):
    Unpacker = 0x19FFC
    Math = 0x19FF8
    Packer = 0x19FF4


format_tile_sizes = {
    DataFormat.Bfp8_b: 1088,
    DataFormat.Float16: 2048,
    DataFormat.Float16_b: 2048,
    DataFormat.Float32: 4096,
    DataFormat.Int32: 4096,
}


class L1BufferLocations(Enum):
    srcA = 0x18FE0
    srcB = 0x18FE4
    Result = 0x18FE8
