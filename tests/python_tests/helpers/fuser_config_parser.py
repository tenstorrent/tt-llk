# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
#
# SPDX-License-Identifier: Apache-2.0

# from __future__ import annotations

import os
from enum import Enum
from pathlib import Path
from typing import Annotated, List, Literal, Optional, Type, Union

import yaml
from pydantic import (
    BaseModel,
    ConfigDict,
    Field,
    ValidationError,
    field_validator,
    model_validator,
)

FUSER_CONFIG_DIR = (
    Path(os.environ.get("LLK_HOME", ".")) / "tests" / "python_tests" / "fuser_config"
)


def _get_runtime_types():
    from helpers.format_config import DataFormat
    from helpers.fused_fpu import DatacopyFpu, EltwiseFpu, MatmulFpu, ReduceFpu
    from helpers.fused_math import ComputeNode, ComputePipeline
    from helpers.fused_operand import OperandRegistry
    from helpers.fused_operation import FusedOperation
    from helpers.fused_packer import Packer
    from helpers.fused_sfpu import BinarySfpu, UnarySfpu
    from helpers.fused_unpacker import (
        MatmulUnpacker,
        UnpackerA,
        UnpackerAB,
        UnpackerTilizeA,
    )
    from helpers.fuser_config import FuserConfig, GlobalConfig
    from helpers.llk_params import (
        ApproximationMode,
        BroadcastType,
        DestAccumulation,
        DestSync,
        EltwiseBinaryReuseDestType,
        MathFidelity,
        MathOperation,
        ReducePool,
        Transpose,
    )

    return {
        "DataFormat": DataFormat,
        "DatacopyFpu": DatacopyFpu,
        "EltwiseFpu": EltwiseFpu,
        "MatmulFpu": MatmulFpu,
        "ReduceFpu": ReduceFpu,
        "ComputeNode": ComputeNode,
        "ComputePipeline": ComputePipeline,
        "OperandRegistry": OperandRegistry,
        "FusedOperation": FusedOperation,
        "Packer": Packer,
        "BinarySfpu": BinarySfpu,
        "UnarySfpu": UnarySfpu,
        "UnpackerA": UnpackerA,
        "UnpackerAB": UnpackerAB,
        "UnpackerTilizeA": UnpackerTilizeA,
        "MatmulUnpacker": MatmulUnpacker,
        "ApproximationMode": ApproximationMode,
        "BroadcastType": BroadcastType,
        "DestAccumulation": DestAccumulation,
        "DestSync": DestSync,
        "EltwiseBinaryReuseDestType": EltwiseBinaryReuseDestType,
        "MathFidelity": MathFidelity,
        "MathOperation": MathOperation,
        "ReducePool": ReducePool,
        "Transpose": Transpose,
        "FuserConfig": FuserConfig,
        "GlobalConfig": GlobalConfig,
    }


def _enum_values(enum_cls: type) -> str:
    return ", ".join(e.value for e in enum_cls)


def format_validation_error(error: ValidationError) -> str:
    messages = []
    for err in error.errors():
        loc = ".".join(str(x) for x in err["loc"])
        msg = err["msg"]
        inp = err.get("input")

        if "Input should be" in msg:
            start = msg.find("'")
            if start != -1:
                valid_part = msg[start:].replace("'", "")
                messages.append(f"'{loc}': got '{inp}', expected: {valid_part}")
            else:
                messages.append(f"'{loc}': {msg}")
        elif "Extra inputs are not permitted" in msg:
            messages.append(f"'{loc}': unknown field")
        elif "Field required" in msg:
            messages.append(f"'{loc}': required field missing")
        elif "Value error" in msg:
            custom_msg = msg.replace("Value error, ", "")
            messages.append(f"'{loc}': {custom_msg}")
        else:
            messages.append(f"'{loc}': {msg}")

    return "\n".join(messages)


class YesNo(str, Enum):
    Yes = "Yes"
    No = "No"

    def to_bool(self) -> bool:
        return self == YesNo.Yes


class DataFormatEnum(str, Enum):
    Float16_b = "Float16_b"
    Float16 = "Float16"
    Float32 = "Float32"
    Bfp8_b = "Bfp8_b"

    def to_runtime(self):
        rt = _get_runtime_types()
        return getattr(rt["DataFormat"], self.value)


class MathFidelityEnum(str, Enum):
    LoFi = "LoFi"
    HiFi2 = "HiFi2"
    HiFi3 = "HiFi3"
    HiFi4 = "HiFi4"

    def to_runtime(self):
        rt = _get_runtime_types()
        return getattr(rt["MathFidelity"], self.value)


class DestSyncEnum(str, Enum):
    Full = "Full"
    Half = "Half"

    def to_runtime(self):
        rt = _get_runtime_types()
        return getattr(rt["DestSync"], self.value)


class UnpackerEnum(str, Enum):
    UnpackerA = "UnpackerA"
    UnpackerAB = "UnpackerAB"
    UnpackerTilizeA = "UnpackerTilizeA"
    MatmulUnpacker = "MatmulUnpacker"

    def to_runtime(self) -> Type:
        rt = _get_runtime_types()
        return rt[self.value]


class PackerEnum(str, Enum):
    Packer = "Packer"

    def to_runtime(self) -> Type:
        rt = _get_runtime_types()
        return rt[self.value]


class BroadcastTypeEnum(str, Enum):
    None_ = "None"
    Row = "Row"
    Column = "Column"
    Scalar = "Scalar"

    def to_runtime(self):
        rt = _get_runtime_types()
        name = "None_" if self.value == "None" else self.value
        return getattr(rt["BroadcastType"], name)


class ReuseDestEnum(str, Enum):
    NONE = "NONE"
    DEST_TO_SRCA = "DEST_TO_SRCA"
    DEST_TO_SRCB = "DEST_TO_SRCB"

    def to_runtime(self):
        rt = _get_runtime_types()
        return getattr(rt["EltwiseBinaryReuseDestType"], self.value)


class ReducePoolEnum(str, Enum):
    Sum = "Sum"
    Min = "Min"
    Max = "Max"
    Average = "Average"

    def to_runtime(self):
        rt = _get_runtime_types()
        return getattr(rt["ReducePool"], self.value)


class FpuOperationEnum(str, Enum):
    Elwadd = "Elwadd"
    Elwmul = "Elwmul"
    Elwsub = "Elwsub"
    Datacopy = "Datacopy"
    Matmul = "Matmul"
    ReduceColumn = "ReduceColumn"
    ReduceRow = "ReduceRow"
    ReduceScalar = "ReduceScalar"

    def is_eltwise(self) -> bool:
        return self in {
            FpuOperationEnum.Elwadd,
            FpuOperationEnum.Elwmul,
            FpuOperationEnum.Elwsub,
        }

    def is_reduce(self) -> bool:
        return self in {
            FpuOperationEnum.ReduceColumn,
            FpuOperationEnum.ReduceRow,
            FpuOperationEnum.ReduceScalar,
        }

    def to_math_operation(self):
        rt = _get_runtime_types()
        return getattr(rt["MathOperation"], self.value)


class UnaryOperationEnum(str, Enum):
    Abs = "Abs"
    Acosh = "Acosh"
    Asinh = "Asinh"
    Atanh = "Atanh"
    Celu = "Celu"
    Cos = "Cos"
    Elu = "Elu"
    Exp = "Exp"
    Exp2 = "Exp2"
    Fill = "Fill"
    Gelu = "Gelu"
    Hardsigmoid = "Hardsigmoid"
    Log = "Log"
    Log1p = "Log1p"
    Neg = "Neg"
    Reciprocal = "Reciprocal"
    ReluMax = "ReluMax"
    ReluMin = "ReluMin"
    Rsqrt = "Rsqrt"
    Silu = "Silu"
    Sin = "Sin"
    Sqrt = "Sqrt"
    Square = "Square"
    Tanh = "Tanh"
    Threshold = "Threshold"

    def to_math_operation(self):
        rt = _get_runtime_types()
        return getattr(rt["MathOperation"], self.value)


class BinaryOperationEnum(str, Enum):
    SfpuElwadd = "SfpuElwadd"
    SfpuElwmul = "SfpuElwmul"
    SfpuElwsub = "SfpuElwsub"
    SfpuElwLeftShift = "SfpuElwLeftShift"
    SfpuElwRightShift = "SfpuElwRightShift"
    SfpuElwLogicalRightShift = "SfpuElwLogicalRightShift"
    SfpuXlogy = "SfpuXlogy"
    SfpuAddTopRow = "SfpuAddTopRow"

    def to_math_operation(self):
        rt = _get_runtime_types()
        return getattr(rt["MathOperation"], self.value)


class FpuMathSchema(BaseModel):
    model_config = ConfigDict(extra="forbid")

    type: Literal["Fpu"]
    operation: FpuOperationEnum
    unpacker: Optional[UnpackerEnum] = None
    broadcast_type: Optional[BroadcastTypeEnum] = None
    reuse_dest: Optional[ReuseDestEnum] = None
    reduce_pool: Optional[ReducePoolEnum] = None
    unpack_transpose_within_face: Optional[YesNo] = None
    unpack_transpose_faces: Optional[YesNo] = None

    @model_validator(mode="after")
    def validate_fpu_config(self) -> "FpuMathSchema":
        if self.operation.is_reduce() and self.reduce_pool is None:
            raise ValueError(
                f"Reduce operations require reduce_pool: {_enum_values(ReducePoolEnum)}"
            )

        if not self.operation.is_reduce() and self.reduce_pool is not None:
            raise ValueError(
                f"reduce_pool: only for Reduce*, not '{self.operation.value}'"
            )

        if self.unpacker is not None:
            if self.operation == FpuOperationEnum.Datacopy:
                if self.unpacker not in {
                    UnpackerEnum.UnpackerA,
                    UnpackerEnum.UnpackerTilizeA,
                }:
                    raise ValueError(
                        f"Datacopy: unpacker must be UnpackerA or UnpackerTilizeA, got '{self.unpacker.value}'"
                    )
            elif self.operation == FpuOperationEnum.Matmul:
                if self.unpacker != UnpackerEnum.MatmulUnpacker:
                    raise ValueError(
                        f"Matmul: unpacker must be MatmulUnpacker, got '{self.unpacker.value}'"
                    )
            elif self.operation.is_reduce():
                if self.unpacker != UnpackerEnum.UnpackerAB:
                    raise ValueError(
                        f"Reduce: unpacker must be UnpackerAB, got '{self.unpacker.value}'"
                    )
            elif self.operation.is_eltwise():
                if (
                    self.reuse_dest is not None
                    and self.reuse_dest != ReuseDestEnum.NONE
                ):
                    if self.unpacker != UnpackerEnum.UnpackerA:
                        raise ValueError(
                            f"Eltwise with reuse_dest: unpacker must be UnpackerA, got '{self.unpacker.value}'"
                        )
                elif self.unpacker != UnpackerEnum.UnpackerAB:
                    raise ValueError(
                        f"Eltwise: unpacker must be UnpackerAB, got '{self.unpacker.value}'"
                    )

        if self.reuse_dest is not None and self.reuse_dest != ReuseDestEnum.NONE:
            if not self.operation.is_eltwise():
                raise ValueError(
                    f"reuse_dest: only for Eltwise operations, not '{self.operation.value}'"
                )

        return self

    def to_compute_node(self):
        rt = _get_runtime_types()

        if self.operation.is_eltwise():
            fpu = rt["EltwiseFpu"](self.operation.to_math_operation())
        elif self.operation.is_reduce():
            fpu = rt["ReduceFpu"](
                self.operation.to_math_operation(), pool=self.reduce_pool.to_runtime()
            )
        elif self.operation == FpuOperationEnum.Datacopy:
            fpu = rt["DatacopyFpu"]()
        elif self.operation == FpuOperationEnum.Matmul:
            fpu = rt["MatmulFpu"]()
        else:
            raise ValueError(f"Unknown FPU operation: {self.operation}")

        kwargs = {}
        if self.unpacker:
            kwargs["unpacker"] = self.unpacker.to_runtime()
        if self.unpack_transpose_within_face:
            kwargs["unpack_transpose_within_face"] = (
                rt["Transpose"].Yes
                if self.unpack_transpose_within_face == YesNo.Yes
                else rt["Transpose"].No
            )
        if self.unpack_transpose_faces:
            kwargs["unpack_transpose_faces"] = (
                rt["Transpose"].Yes
                if self.unpack_transpose_faces == YesNo.Yes
                else rt["Transpose"].No
            )
        if self.broadcast_type:
            kwargs["broadcast_type"] = self.broadcast_type.to_runtime()
        if self.reuse_dest:
            kwargs["reuse_dest"] = self.reuse_dest.to_runtime()

        return rt["ComputeNode"](fpu=fpu, sfpu=None, **kwargs)


class UnarySfpuMathSchema(BaseModel):
    model_config = ConfigDict(extra="forbid")

    type: Literal["UnarySfpu"]
    operation: UnaryOperationEnum
    approximation_mode: YesNo = YesNo.No
    iterations: Annotated[int, Field(ge=1)] = 8
    dst_dest_tile_index: Annotated[int, Field(ge=0)] = 0
    fill_const_value: float = 1.0

    def to_compute_node(self):
        rt = _get_runtime_types()
        approx = (
            rt["ApproximationMode"].Yes
            if self.approximation_mode == YesNo.Yes
            else rt["ApproximationMode"].No
        )

        sfpu = rt["UnarySfpu"](
            self.operation.to_math_operation(),
            approx,
            self.iterations,
            self.dst_dest_tile_index,
            self.fill_const_value,
        )
        return rt["ComputeNode"](unpacker=None, fpu=None, sfpu=sfpu)


class BinarySfpuMathSchema(BaseModel):
    model_config = ConfigDict(extra="forbid")

    type: Literal["BinarySfpu"]
    operation: BinaryOperationEnum
    approximation_mode: YesNo = YesNo.No
    iterations: Annotated[int, Field(ge=1)] = 8
    src1_dest_tile_index: Annotated[int, Field(ge=0)] = 0
    src2_dest_tile_index: Annotated[int, Field(ge=0)] = 0
    dst_dest_tile_index: Annotated[int, Field(ge=0)] = 0

    def to_compute_node(self):
        rt = _get_runtime_types()
        approx = (
            rt["ApproximationMode"].Yes
            if self.approximation_mode == YesNo.Yes
            else rt["ApproximationMode"].No
        )

        sfpu = rt["BinarySfpu"](
            self.operation.to_math_operation(),
            approx,
            self.iterations,
            self.src1_dest_tile_index,
            self.src2_dest_tile_index,
            self.dst_dest_tile_index,
        )
        return rt["ComputeNode"](unpacker=None, fpu=None, sfpu=sfpu)


MathSchema = Annotated[
    Union[FpuMathSchema, UnarySfpuMathSchema, BinarySfpuMathSchema],
    Field(discriminator="type"),
]


class OperationSchema(BaseModel):
    model_config = ConfigDict(extra="forbid")

    src_a: str = Field(..., min_length=1)
    src_b: str = Field(..., min_length=1)
    output: str = Field(..., min_length=1)

    src_a_dims: Annotated[List[int], Field(min_length=2, max_length=2)] = [32, 32]
    src_b_dims: Annotated[List[int], Field(min_length=2, max_length=2)] = [32, 32]
    output_dims: Annotated[List[int], Field(min_length=2, max_length=2)] = [32, 32]

    input_format: DataFormatEnum = DataFormatEnum.Float16_b
    output_format: DataFormatEnum = DataFormatEnum.Float16_b

    src_a_const_value: Optional[float] = None
    src_b_const_value: Optional[float] = None

    math: List[MathSchema] = Field(..., min_length=1)

    packer: PackerEnum = PackerEnum.Packer
    math_fidelity: MathFidelityEnum = MathFidelityEnum.LoFi
    dest_sync: Optional[DestSyncEnum] = None
    batch_size: Annotated[int, Field(ge=1)] = 1

    unpack_transpose_within_face: Optional[YesNo] = None
    unpack_transpose_faces: Optional[YesNo] = None

    @field_validator("src_a_dims", "src_b_dims", "output_dims")
    @classmethod
    def validate_dimensions(cls, v: List[int]) -> List[int]:
        for dim in v:
            if dim <= 0:
                raise ValueError(f"must be positive, got {dim}")
            if dim % 32 != 0:
                raise ValueError(f"must be multiple of 32, got {dim}")
        return v

    @model_validator(mode="after")
    def validate_operation(self) -> "OperationSchema":
        has_matmul = any(
            isinstance(m, FpuMathSchema) and m.operation == FpuOperationEnum.Matmul
            for m in self.math
        )

        if has_matmul:
            if self.src_a_dims[1] != self.src_b_dims[0]:
                raise ValueError(
                    f"Matmul: src_a[1]={self.src_a_dims[1]} != src_b[0]={self.src_b_dims[0]}"
                )

        return self

    def to_fused_operation(self, operands):
        rt = _get_runtime_types()

        operand_mapping = operands.create_mapping(
            src_a=self.src_a,
            src_b=self.src_b,
            output=self.output,
            src_a_dims=self.src_a_dims,
            src_b_dims=self.src_b_dims,
            output_dims=self.output_dims,
            input_format=self.input_format.to_runtime(),
            output_format=self.output_format.to_runtime(),
            src_a_const_value=self.src_a_const_value,
            src_b_const_value=self.src_b_const_value,
        )

        math_ops = [m.to_compute_node() for m in self.math]

        kwargs = {
            "math_fidelity": self.math_fidelity.to_runtime(),
        }
        if self.dest_sync:
            kwargs["dest_sync"] = self.dest_sync.to_runtime()
        if self.batch_size:
            kwargs["batch_size"] = self.batch_size

        return rt["FusedOperation"](
            operand_mapping=operand_mapping,
            math=rt["ComputePipeline"](math_ops),
            packer=self.packer.to_runtime(),
            **kwargs,
        )


class FuserConfigSchema(BaseModel):
    model_config = ConfigDict(extra="forbid")

    dest_acc: YesNo = YesNo.No
    profiler_enabled: bool = False
    loop_factor: Annotated[int, Field(ge=1)] = 16
    operations: List[OperationSchema] = Field(..., min_length=1)

    @model_validator(mode="after")
    def validate_config(self) -> "FuserConfigSchema":
        outputs = set()
        for i, op in enumerate(self.operations):
            if op.output in outputs:
                raise ValueError(f"op[{i}].output='{op.output}' already defined")
            outputs.add(op.output)
        return self

    def to_fuser_config(self, test_name: str):
        rt = _get_runtime_types()

        dest_acc = (
            rt["DestAccumulation"].Yes
            if self.dest_acc == YesNo.Yes
            else rt["DestAccumulation"].No
        )

        operands = rt["OperandRegistry"]()
        pipeline = [op.to_fused_operation(operands) for op in self.operations]

        return rt["FuserConfig"](
            pipeline=pipeline,
            global_config=rt["GlobalConfig"](
                dest_acc=dest_acc,
                test_name=test_name,
                profiler_enabled=self.profiler_enabled,
                loop_factor=self.loop_factor,
            ),
        )

    @classmethod
    def validate_file(cls, yaml_path: Union[str, Path]) -> "FuserConfigSchema":
        yaml_path = Path(yaml_path)
        if not yaml_path.exists():
            raise FileNotFoundError(f"File not found: {yaml_path}")

        with open(yaml_path, "r") as f:
            config_dict = yaml.safe_load(f)

        try:
            return cls.model_validate(config_dict)
        except ValidationError as e:
            raise ValueError(
                f"Validation failed for {yaml_path.name}:\n{format_validation_error(e)}"
            ) from None

    @classmethod
    def validate_string(cls, yaml_content: str) -> "FuserConfigSchema":
        config_dict = yaml.safe_load(yaml_content)
        try:
            return cls.model_validate(config_dict)
        except ValidationError as e:
            raise ValueError(
                f"Validation failed:\n{format_validation_error(e)}"
            ) from None

    @classmethod
    def load(cls, test_name: str):
        yaml_path = FUSER_CONFIG_DIR / f"{test_name}.yaml"
        schema = cls.validate_file(yaml_path)
        return schema.to_fuser_config(test_name)


load_fuser_config = FuserConfigSchema.load
