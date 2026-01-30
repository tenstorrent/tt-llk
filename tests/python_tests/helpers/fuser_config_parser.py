# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
#
# SPDX-License-Identifier: Apache-2.0

import os
from pathlib import Path
from typing import Any, Dict, List, Type

import yaml
from helpers.format_config import DataFormat
from helpers.fused_math import (
    BinarySfpu,
    DatacopyFpu,
    EltwiseFpu,
    Math,
    MatmulFpu,
    ReduceFpu,
    ReuseDestEltwiseFpu,
    UnarySfpu,
)
from helpers.fused_operand import OperandRegistry
from helpers.fused_operation import FusedOperation
from helpers.fused_packer import Packer
from helpers.fused_unpacker import (
    MatmulUnpacker,
    Unpacker,
    UnpackerA,
    UnpackerAB,
    UnpackerTilizeA,
)
from helpers.llk_params import (
    ApproximationMode,
    BroadcastType,
    DestSync,
    EltwiseBinaryReuseDestType,
    MathOperation,
    ReducePool,
    Transpose,
)
from helpers.math_stage import (
    MathStage,
    MultiStageMath,
    ReusableEltwiseFpu,
    StageUnpackerConfig,
)

from .fuser_config import FuserConfig, GlobalConfig
from .llk_params import DestAccumulation, MathFidelity

FUSER_CONFIG_DIR = (
    Path(os.environ.get("LLK_HOME")) / "tests" / "python_tests" / "fuser_config"
)

UNPACKER_MAP: Dict[str, Type[Unpacker]] = {
    "UnpackerA": UnpackerA,
    "UnpackerAB": UnpackerAB,
    "UnpackerTilizeA": UnpackerTilizeA,
    "MatmulUnpacker": MatmulUnpacker,
}

PACKER_MAP: Dict[str, Type[Packer]] = {
    "Packer": Packer,
}

DATA_FORMAT_MAP: Dict[str, DataFormat] = {
    "Float16_b": DataFormat.Float16_b,
    "Float16": DataFormat.Float16,
    "Float32": DataFormat.Float32,
    "Bfp8_b": DataFormat.Bfp8_b,
}

MATH_FIDELITY_MAP: Dict[str, MathFidelity] = {
    "LoFi": MathFidelity.LoFi,
    "HiFi2": MathFidelity.HiFi2,
    "HiFi3": MathFidelity.HiFi3,
    "HiFi4": MathFidelity.HiFi4,
}

DEST_ACCUMULATION_MAP: Dict[str, DestAccumulation] = {
    "Yes": DestAccumulation.Yes,
    "No": DestAccumulation.No,
}

DEST_SYNC_MAP: Dict[str, DestSync] = {
    "Full": DestSync.Full,
    "Half": DestSync.Half,
}

TRANSPOSE_MAP: Dict[str, Transpose] = {
    "Yes": Transpose.Yes,
    "No": Transpose.No,
}

FPU_OPERATION_MAP: Dict[str, MathOperation] = {
    "Elwadd": MathOperation.Elwadd,
    "Elwmul": MathOperation.Elwmul,
    "Elwsub": MathOperation.Elwsub,
}

SFPU_UNARY_OPERATION_MAP: Dict[str, MathOperation] = {
    "Abs": MathOperation.Abs,
    "Acosh": MathOperation.Acosh,
    "Asinh": MathOperation.Asinh,
    "Atanh": MathOperation.Atanh,
    "Celu": MathOperation.Celu,
    "Cos": MathOperation.Cos,
    "Elu": MathOperation.Elu,
    "Exp": MathOperation.Exp,
    "Exp2": MathOperation.Exp2,
    "Fill": MathOperation.Fill,
    "Gelu": MathOperation.Gelu,
    "Hardsigmoid": MathOperation.Hardsigmoid,
    "Log": MathOperation.Log,
    "Log1p": MathOperation.Log1p,
    "Neg": MathOperation.Neg,
    "Reciprocal": MathOperation.Reciprocal,
    "ReluMax": MathOperation.ReluMax,
    "ReluMin": MathOperation.ReluMin,
    "Rsqrt": MathOperation.Rsqrt,
    "Silu": MathOperation.Silu,
    "Sin": MathOperation.Sin,
    "Sqrt": MathOperation.Sqrt,
    "Square": MathOperation.Square,
    "Threshold": MathOperation.Threshold,
}

SFPU_BINARY_OPERATION_MAP: Dict[str, MathOperation] = {
    "SfpuElwadd": MathOperation.SfpuElwadd,
    "SfpuElwmul": MathOperation.SfpuElwmul,
    "SfpuElwsub": MathOperation.SfpuElwsub,
    "SfpuElwLeftShift": MathOperation.SfpuElwLeftShift,
    "SfpuElwRightShift": MathOperation.SfpuElwRightShift,
    "SfpuElwLogicalRightShift": MathOperation.SfpuElwLogicalRightShift,
    "SfpuXlogy": MathOperation.SfpuXlogy,
    "SfpuAddTopRow": MathOperation.SfpuAddTopRow,
}

SFPU_TERNARY_OPERATION_MAP: Dict[str, MathOperation] = {
    "SfpuWhere": MathOperation.SfpuWhere,
    "TTNNWhere": MathOperation.TTNNWhere,
}

REDUCE_OPERATION_MAP: Dict[str, MathOperation] = {
    "ReduceColumn": MathOperation.ReduceColumn,
    "ReduceRow": MathOperation.ReduceRow,
    "ReduceScalar": MathOperation.ReduceScalar,
}

REDUCE_POOL_MAP: Dict[str, ReducePool] = {
    "Sum": ReducePool.Sum,
    "Min": ReducePool.Min,
    "Max": ReducePool.Max,
    "Average": ReducePool.Average,
}

APPROXIMATION_MODE_MAP: Dict[str, ApproximationMode] = {
    "Yes": ApproximationMode.Yes,
    "No": ApproximationMode.No,
}

BROADCAST_TYPE_MAP: Dict[str, BroadcastType] = {
    "None": BroadcastType.None_,
    "Row": BroadcastType.Row,
    "Column": BroadcastType.Column,
    "Scalar": BroadcastType.Scalar,
}

REUSE_DEST_TYPE_MAP: Dict[str, EltwiseBinaryReuseDestType] = {
    "NONE": EltwiseBinaryReuseDestType.NONE,
    "DEST_TO_SRCA": EltwiseBinaryReuseDestType.DEST_TO_SRCA,
    "DEST_TO_SRCB": EltwiseBinaryReuseDestType.DEST_TO_SRCB,
}


def parse_sfpu_list(sfpu_configs: List[Dict[str, Any]]) -> List:
    """Parse a list of SFPU configurations."""
    sfpu_ops = []
    for sfpu_config in sfpu_configs:
        sfpu_type = sfpu_config.get("type")

        if sfpu_type == "UnarySfpu":
            operation = SFPU_UNARY_OPERATION_MAP[sfpu_config["operation"]]
            approx_mode = APPROXIMATION_MODE_MAP.get(
                sfpu_config.get("approximation_mode", "No"), ApproximationMode.No
            )
            iterations = sfpu_config.get("iterations", 8)
            dest_idx = sfpu_config.get("dst_dest_tile_index", 0)
            fill_const_value = sfpu_config.get("fill_const_value", 1.0)

            sfpu_ops.append(
                UnarySfpu(
                    operation, approx_mode, iterations, dest_idx, fill_const_value
                )
            )

        elif sfpu_type == "BinarySfpu":
            operation = SFPU_BINARY_OPERATION_MAP[sfpu_config["operation"]]
            approx_mode = APPROXIMATION_MODE_MAP.get(
                sfpu_config.get("approximation_mode", "No"), ApproximationMode.No
            )
            iterations = sfpu_config.get("iterations", 8)
            src1_dest_tile_index = sfpu_config.get("src1_dest_tile_index", 0)
            src2_dest_tile_index = sfpu_config.get("src2_dest_tile_index", 0)
            dst_dest_tile_index = sfpu_config.get("dst_dest_tile_index", 0)

            sfpu_ops.append(
                BinarySfpu(
                    operation,
                    approx_mode,
                    iterations,
                    src1_dest_tile_index,
                    src2_dest_tile_index,
                    dst_dest_tile_index,
                )
            )
        else:
            raise ValueError(f"Unsupported SFPU type: {sfpu_type}")

    return sfpu_ops


def parse_fpu(
    fpu_config: Dict[str, Any],
    reuse_dest_type: EltwiseBinaryReuseDestType = EltwiseBinaryReuseDestType.NONE,
):
    """Parse an FPU configuration."""
    fpu_type = fpu_config.get("fpu", "Datacopy")

    # For reuse_dest stages, use ReusableEltwiseFpu
    if reuse_dest_type != EltwiseBinaryReuseDestType.NONE:
        if fpu_type not in FPU_OPERATION_MAP:
            raise ValueError(
                f"Reuse dest stages only support eltwise FPU operations. "
                f"Got: {fpu_type}, valid: {list(FPU_OPERATION_MAP.keys())}"
            )
        math_op = FPU_OPERATION_MAP[fpu_type]
        return ReusableEltwiseFpu(math_op, reuse_dest_type)

    # Normal FPU parsing
    if fpu_type in FPU_OPERATION_MAP:
        math_op = FPU_OPERATION_MAP[fpu_type]
        return EltwiseFpu(math_op)
    elif fpu_type in REDUCE_OPERATION_MAP:
        math_op = REDUCE_OPERATION_MAP[fpu_type]
        pool_str = fpu_config.get("reduce_pool", "Max")
        try:
            pool = REDUCE_POOL_MAP[pool_str]
        except KeyError:
            raise ValueError(f"Unsupported reduce pool: {pool_str}")
        return ReduceFpu(math_op, pool=pool)
    elif fpu_type == "Datacopy":
        return DatacopyFpu()
    elif fpu_type == "Matmul":
        return MatmulFpu()
    else:
        raise ValueError(f"Unsupported FPU type: {fpu_type}")


def parse_stage_unpacker_config(
    stage_config: Dict[str, Any], is_first_stage: bool
) -> StageUnpackerConfig:
    """Parse unpacker configuration for a MathStage."""
    unpacker_config = stage_config.get("unpacker", {})

    input_operand = unpacker_config.get("input_operand", "")

    broadcast_type_name = unpacker_config.get("broadcast_type", "None")
    broadcast_type = BROADCAST_TYPE_MAP.get(broadcast_type_name, BroadcastType.None_)

    # For first stage, no reuse_dest
    if is_first_stage:
        reuse_dest_type = EltwiseBinaryReuseDestType.NONE
    else:
        reuse_dest_type_name = unpacker_config.get("reuse_dest_type", "DEST_TO_SRCA")
        reuse_dest_type = REUSE_DEST_TYPE_MAP.get(
            reuse_dest_type_name, EltwiseBinaryReuseDestType.DEST_TO_SRCA
        )

    transpose_faces = unpacker_config.get("transpose_faces", False)
    transpose_within_face = unpacker_config.get("transpose_within_face", False)

    return StageUnpackerConfig(
        input_operand=input_operand,
        broadcast_type=broadcast_type,
        reuse_dest_type=reuse_dest_type,
        transpose_faces=transpose_faces,
        transpose_within_face=transpose_within_face,
    )


def parse_math_stage(stage_config: Dict[str, Any], stage_index: int) -> MathStage:
    """Parse a single MathStage from configuration."""
    is_first_stage = stage_index == 0

    # Parse unpacker config
    unpacker_config = parse_stage_unpacker_config(stage_config, is_first_stage)

    # Parse FPU
    fpu = parse_fpu(stage_config, unpacker_config.reuse_dest_type)

    # Parse SFPU list
    sfpu_ops = []
    if "sfpu" in stage_config:
        sfpu_ops = parse_sfpu_list(stage_config["sfpu"])

    return MathStage(
        unpacker_config=unpacker_config,
        fpu=fpu,
        sfpu=sfpu_ops,
        stage_index=stage_index,
    )


def parse_math_operation(
    math_config: Dict[str, Any], operands: OperandRegistry
) -> "Math | MultiStageMath":
    """
    Parse math configuration. Supports two formats:

    1. Legacy format (single FPU + SFPU list + optional post_eltwise_fpu):
       math:
         fpu: "Elwadd"
         sfpu: [...]
         post_eltwise_fpu: [...]  # deprecated, use stages instead

    2. New stages format (list of MathStages):
       math:
         stages:
           - fpu: "Datacopy"
             sfpu: [...]
             unpacker:
               input_operand: "input_A"
           - fpu: "Elwadd"
             sfpu: [...]
             unpacker:
               input_operand: "input_B"
               reuse_dest_type: "DEST_TO_SRCA"
    """
    # Check if using new stages format
    if "stages" in math_config:
        stages = []
        for i, stage_config in enumerate(math_config["stages"]):
            stage = parse_math_stage(stage_config, i)
            stages.append(stage)
        return MultiStageMath(stages)

    # Legacy format parsing
    fpu = parse_fpu(math_config)
    sfpu_ops = parse_sfpu_list(math_config.get("sfpu", []))

    # Parse post_eltwise_fpu operations (legacy, kept for backwards compatibility)
    post_eltwise_fpu_ops = []
    if "post_eltwise_fpu" in math_config:
        for post_fpu_config in math_config["post_eltwise_fpu"]:
            operation_name = post_fpu_config.get("operation")
            if operation_name not in FPU_OPERATION_MAP:
                raise ValueError(
                    f"Unsupported post eltwise FPU operation: {operation_name}. "
                    f"Valid operations are: {list(FPU_OPERATION_MAP.keys())}"
                )
            operation = FPU_OPERATION_MAP[operation_name]

            reuse_dest_type_name = post_fpu_config.get(
                "reuse_dest_type", "DEST_TO_SRCA"
            )
            if reuse_dest_type_name not in REUSE_DEST_TYPE_MAP:
                raise ValueError(
                    f"Unsupported reuse dest type: {reuse_dest_type_name}. "
                    f"Valid types are: {list(REUSE_DEST_TYPE_MAP.keys())}"
                )
            reuse_dest_type = REUSE_DEST_TYPE_MAP[reuse_dest_type_name]

            iterations = post_fpu_config.get("iterations", 1)

            post_eltwise_fpu_ops.append(
                ReuseDestEltwiseFpu(
                    operation=operation,
                    reuse_dest_type=reuse_dest_type,
                    iterations=iterations,
                )
            )

    return Math(fpu, sfpu_ops, post_eltwise_fpu_ops)


def parse_operation(
    op_config: Dict[str, Any], operands: OperandRegistry
) -> FusedOperation:
    input_format_name = op_config.get("input_format", "Float16_b")
    input_format = DATA_FORMAT_MAP.get(input_format_name)
    if input_format is None:
        raise ValueError(
            f"Invalid input_format '{input_format_name}'. "
            f"Expected one of: {list(DATA_FORMAT_MAP.keys())}"
        )
    output_format_name = op_config.get("output_format", "Float16_b")
    output_format = DATA_FORMAT_MAP.get(output_format_name)
    if output_format is None:
        raise ValueError(
            f"Invalid output_format '{output_format_name}'. "
            f"Expected one of: {list(DATA_FORMAT_MAP.keys())}"
        )
    operand_mapping = operands.create_mapping(
        src_a=op_config["src_a"],
        src_b=op_config["src_b"],
        output=op_config["output"],
        src_a_dims=op_config.get("src_a_dims", [32, 32]),
        src_b_dims=op_config.get("src_b_dims", [32, 32]),
        output_dims=op_config.get("output_dims", [32, 32]),
        input_format=input_format,
        output_format=output_format,
        src_a_const_value=op_config.get("src_a_const_value"),
        src_b_const_value=op_config.get("src_b_const_value"),
    )

    unpacker_name = op_config.get("unpacker", "UnpackerA")
    unpacker = UNPACKER_MAP.get(unpacker_name)
    if unpacker is None:
        valid_unpackers = ", ".join(UNPACKER_MAP.keys())
        raise ValueError(
            f"Invalid unpacker '{unpacker_name}' in operation config. "
            f"Valid unpackers are: {valid_unpackers}"
        )

    math = parse_math_operation(op_config.get("math", {}), operands)

    packer_name = op_config.get("packer", "Packer")
    packer = PACKER_MAP.get(packer_name)
    if packer is None:
        valid_packers = ", ".join(PACKER_MAP.keys())
        raise ValueError(
            f"Invalid packer {packer_name} in operation config. "
            f"Valid packers are: {valid_packers}"
        )

    kwargs = {}

    if "math_fidelity" in op_config:
        kwargs["math_fidelity"] = MATH_FIDELITY_MAP[op_config["math_fidelity"]]
    if "dest_sync" in op_config:
        kwargs["dest_sync"] = DEST_SYNC_MAP[op_config["dest_sync"]]
    if "unpack_transpose_within_face" in op_config:
        kwargs["unpack_transpose_within_face"] = TRANSPOSE_MAP[
            op_config["unpack_transpose_within_face"]
        ]
    if "unpack_transpose_faces" in op_config:
        kwargs["unpack_transpose_faces"] = TRANSPOSE_MAP[
            op_config["unpack_transpose_faces"]
        ]
    if "batch_size" in op_config:
        kwargs["batch_size"] = op_config["batch_size"]
    if "broadcast_type" in op_config:
        kwargs["broadcast_type"] = BROADCAST_TYPE_MAP[op_config["broadcast_type"]]

    return FusedOperation(
        operand_mapping=operand_mapping,
        unpacker=unpacker,
        math=math,
        packer=packer,
        **kwargs,
    )


def load_fuser_config(test_name: str) -> FuserConfig:
    yaml_path = FUSER_CONFIG_DIR / f"{test_name}.yaml"
    yaml_file = Path(yaml_path)
    if not yaml_file.exists():
        raise FileNotFoundError(f"YAML file does not exist: {yaml_path}")

    with open(yaml_file, "r") as f:
        config = yaml.safe_load(f)

    dest_acc = DEST_ACCUMULATION_MAP[config.get("dest_acc", "No")]
    profiler_enabled = config.get("profiler_enabled", False)
    loop_factor = config.get("loop_factor", 16)

    operands = OperandRegistry()

    pipeline = []
    for op_config in config.get("operations", []):
        operation = parse_operation(op_config, operands)
        pipeline.append(operation)

    fuser_config = FuserConfig(
        pipeline=pipeline,
        global_config=GlobalConfig(
            dest_acc=dest_acc,
            test_name=test_name,
            profiler_enabled=profiler_enabled,
            loop_factor=loop_factor,
        ),
    )

    return fuser_config
