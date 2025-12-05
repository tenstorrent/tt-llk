# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
#
# SPDX-License-Identifier: Apache-2.0

from dataclasses import dataclass
from typing import Optional, Tuple

from .llk_params import (
    ApproximationMode,
    BroadcastType,
    DataCopyType,
    DestAccumulation,
    DestSync,
    EltwiseBinaryReuseDestType,
    ImpliedMathFormat,
    MathFidelity,
    PerfRunType,
    ReducePool,
    StochasticRounding,
    UnpackerEngine,
)
from .matmul_sweep import validate_tile_dimensions

# Base parameter classes


@dataclass
class TemplateParameter:
    def covert_to_cpp(self) -> str:
        raise NotImplementedError(
            "This is abstract function that needs to be implemented for every  template parameter"
        )


@dataclass
class RuntimeParameter:
    def covert_to_cpp(self) -> str:
        raise NotImplementedError(
            "This is abstract function that needs to be implemented for every runtime parameter"
        )


# === TEMPLATE PARAMETER IMPLEMENTATIONS ===


@dataclass
class THROTTLE_LEVEL(TemplateParameter):
    level: int

    def covert_to_cpp(self) -> str:
        return f"constexpr int THROTTLE_LEVEL = {self.level};"


@dataclass
class MATH_TRANSPOSE_FACES(TemplateParameter):
    value: bool

    def covert_to_cpp(self) -> str:
        return f"constexpr bool MATH_TRANSPOSE_FACES = {str(self.value).lower()};"


@dataclass
class STOCHASTIC_ROUNDING(TemplateParameter):
    type: StochasticRounding

    def covert_to_cpp(self) -> str:
        return f"constexpr auto STOCHASTIC_RND = ckernel::{self.type};"


@dataclass
class DATA_COPY_TYPE(TemplateParameter):
    type: DataCopyType

    def covert_to_cpp(self) -> str:
        return f"constexpr auto DATA_COPY_TYPE = ckernel::DataCopyType::{self.type};"


@dataclass
class BROADCAST_TYPE(TemplateParameter):
    type: BroadcastType

    def covert_to_cpp(self) -> str:
        return f"constexpr auto BROADCAST_TYPE = ckernel::BroadcastType::{self.type};"


@dataclass
class ACC_TO_DEST(TemplateParameter):
    value: bool

    def covert_to_cpp(self) -> str:
        return f"constexpr bool ACC_TO_DEST = {str(self.value).lower()};"


@dataclass
class REUSE_DEST_TYPE(TemplateParameter):
    type: EltwiseBinaryReuseDestType

    def covert_to_cpp(self) -> str:
        return f"constexpr auto REUSE_DEST_TYPE = ckernel::EltwiseBinaryReuseDestType::{self.type};"


@dataclass
class DISABLE_SRC_ZERO_FLAG(TemplateParameter):
    value: bool

    def covert_to_cpp(self) -> str:
        return f"constexpr bool disable_src_zero_flag = {str(self.value).lower()};"


@dataclass
class MATH_FIDELITY(TemplateParameter):  # This one is mandatory
    fidelity: MathFidelity

    def covert_to_cpp(self) -> str:
        return f"constexpr std::uint32_t MATH_FIDELITY = {self.fidelity};"


@dataclass
class APPROX_MODE(TemplateParameter):  # This one is mandatory
    mode: ApproximationMode

    def covert_to_cpp(self) -> str:
        return f"constexpr bool APPROX_MODE = {self.mode};"


@dataclass
class DEST_SYNC(TemplateParameter):  # This one is mandatory
    mode: DestSync

    def covert_to_cpp(self) -> str:
        return f"constexpr auto dest_sync = ckernel::DstSync::Sync{self.mode};"


@dataclass
class TILIZE(TemplateParameter):
    value: bool

    def covert_to_cpp(self) -> str:
        return f"constexpr bool tilize_en = {str(self.value).lower()};"


@dataclass
class IMPLIED_MATH_FORMAT(TemplateParameter):  # TODO should be false by default
    data: ImpliedMathFormat

    def covert_to_cpp(self) -> str:
        return f"constexpr bool IMPLIED_MATH_FORMAT = {self.data.value};"


@dataclass
class UNPACKER_ENGINE_SEL(
    TemplateParameter
):  # TODO should be UnpackerEngine.UnpA by default
    value: UnpackerEngine

    def covert_to_cpp(self) -> str:
        return f"constexpr uint UNPACKER_ENGINE_SEL = p_unpacr::{self.value};"


@dataclass
class PERF_RUN_TYPE(TemplateParameter):
    type: PerfRunType

    def covert_to_cpp(self) -> str:
        return f"\nconstexpr auto PERF_RUN_TYPE = PerfRunType::{self.type.name};"


@dataclass
class DEST_ACC(TemplateParameter):  # TODO Should be no by default
    dest_acc: DestAccumulation

    def covert_to_cpp(self) -> str:
        return f"constexpr bool is_fp32_dest_acc_en = {self.dest_acc.value};"


@dataclass
class REDUCE_POOL_TYPE(TemplateParameter):
    type: ReducePool

    def covert_to_cpp(self) -> str:
        return f"constexpr auto POOL_TYPE = ckernel::PoolType::{self.type};"


@dataclass
class InputDimensions(TemplateParameter):
    srcA: Tuple[int, int]
    srcA: Tuple[int, int]
    block_ct_dim: Optional[int]
    block_rt_dim: Optional[int]

    def covert_to_cpp(self) -> str:
        num_rows, num_cols = 32, 32
        validate_tile_dimensions(self.srcA[0], num_rows)
        validate_tile_dimensions(self.srcA[1], num_cols)
        validate_tile_dimensions(self.srcB[0], num_rows)
        validate_tile_dimensions(self.srcB[1], num_cols)

        full_ct_dim = self.srcB[1] // num_cols
        full_rt_dim = self.srcA[0] // num_rows

        if self.block_ct_dim is None:
            self.block_ct_dim = full_ct_dim

        if self.block_rt_dim is None:
            self.block_rt_dim = full_rt_dim

        lines: list[str] = [
            f"constexpr uint32_t FULL_RT_DIM = {full_rt_dim};",
            f"constexpr uint32_t FULL_CT_DIM = {full_ct_dim};",
            f"constexpr uint32_t BLOCK_CT_DIM = {self.block_ct_dim};",  # RT + TP
            f"constexpr uint32_t BLOCK_RT_DIM = {self.block_rt_dim};",  # RT + TP
        ]
        return "\n".join(lines)


# @dataclass
# class (TemplateParameter):
#     def covert_to_cpp(self) -> str:
#         return ""

# @dataclass
# class (TemplateParameter):
#     def covert_to_cpp(self) -> str:
#         return ""

# === RUNTIME PARAMETER IMPLEMENTATIONS ===


@dataclass
class LOOP_FACTOR(RuntimeParameter):
    factor: int

    def covert_to_cpp(self) -> str:
        f"constexpr int LOOP_FACTOR = {self.factor};"


@dataclass
class UNPACK_TRANS_FACES(RuntimeParameter):
    value: bool

    def covert_to_cpp(self) -> str:
        f"constexpr bool UNPACK_TRANSPOSE_FACES = {str(self.value).lower()};"


@dataclass
class UNPACK_TRANS_WITHING_FACE(RuntimeParameter):
    value: bool

    def covert_to_cpp(self) -> str:
        return (
            f"constexpr bool UNPACK_TRANSPOSE_WITHIN_FACE = {str(self.value).lower()};"
        )


@dataclass
class NARROW_TILE(RuntimeParameter):
    value: bool

    def covert_to_cpp(self) -> str:
        return f"constexpr bool NARROW_TILE = {self.value};"


@dataclass
class DEST_INDEX(RuntimeParameter):
    index: int

    def covert_to_cpp(self) -> str:
        return f"constexpr int DST_INDEX = {self.index};"


@dataclass
class TILE_COUNT(RuntimeParameter):
    count: int

    def covert_to_cpp(self) -> str:
        return f"constexpr int TILE_CNT = {self.count};"


@dataclass
class SRCA_REUSE_COUNT(RuntimeParameter):
    count: int

    def covert_to_cpp(self) -> str:
        return f"constexpr int SRCA_REUSE_COUNT = {self.count};"


@dataclass
class PARTIAL_FACE(RuntimeParameter):
    partial_face: Optional[bool]
    partial_a: Optional[bool]
    partial_b: Optional[bool]

    def covert_to_cpp(self) -> str:
        lines: list[str] = []

        if self.partial_a:
            lines.append(
                f"constexpr bool PARTIAL_FACE_A = {str(self.partial_a).lower()};"
            )
            lines.append(
                f"constexpr bool PARTIAL_FACE_PACK = {str(self.partial_a).lower()};"
            )

        if self.partial_a is None and self.partial_face:
            lines.append(
                f"constexpr bool PARTIAL_FACE_A = {str(self.partial_face).lower()};"
            )
            lines.append(
                f"constexpr bool PARTIAL_FACE_PACK = {str(self.partial_face).lower()};"
            )

        if self.partial_b:
            lines.append(
                f"constexpr bool PARTIAL_FACE_B = {str(self.partial_b).lower()};"
            )
            lines.append(
                f"constexpr bool PARTIAL_FACE_MATH = {str(self.partial_b).lower()};"
            )

        if self.partial_b is None and self.partial_face:
            lines.append(
                f"constexpr bool PARTIAL_FACE_B = {str(self.partial_face).lower()};"
            )
            lines.append(
                f"constexpr bool PARTIAL_FACE_MATH = {str(self.partial_face).lower()};"
            )

        return "\n".join(lines)


@dataclass
class CRK_TILE_DIMM(RuntimeParameter):
    c_dimm: int
    r_dimm: int
    k_dimm: int

    def covert_to_cpp(self) -> str:
        lines: list[str] = [
            f"constexpr uint32_t RT_DIM = {self.r_dimm};",
            f"constexpr uint32_t CT_DIM = {self.c_dimm};",
            f"constexpr uint32_t KT_DIM = {self.k_dimm};",
        ]

        return "\n".join(lines)


# @dataclass
# class (RuntimeParameter):
#     def covert_to_cpp(self) -> str:
#         return ""
