# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
#
# SPDX-License-Identifier: Apache-2.0

from dataclasses import dataclass
from typing import Optional

from .llk_params import (
    ApproximationMode,
    BroadcastType,
    DataCopyType,
    DestSync,
    EltwiseBinaryReuseDestType,
    MathFidelity,
    ReducePool,
    StochasticRounding,
)

# Base parameter classes


@dataclass
class RuntimeParameter:
    def covert_to_cpp(self) -> str:
        raise NotImplementedError(
            "This is abstract function that needs to be implemented for every parameter"
        )


@dataclass
class TemplateParameter:
    def covert_to_cpp(self) -> str:
        raise NotImplementedError(
            "This is abstract function that needs to be implemented for every parameter"
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
        return f"constexpr bool MATH_TRANSPOSE_FACES = {self.value};"


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
class BROADCAST_TYPE(TemplateParameter):
    def covert_to_cpp(self) -> str:
        return ""


@dataclass
class ACC_TO_DEST(TemplateParameter):
    value: bool

    def covert_to_cpp(self) -> str:
        return f"constexpr bool ACC_TO_DEST = {self.value};"


@dataclass
class REUSE_DEST_TYPE(TemplateParameter):
    type: EltwiseBinaryReuseDestType

    def covert_to_cpp(self) -> str:
        return f"constexpr auto REUSE_DEST_TYPE = ckernel::EltwiseBinaryReuseDestType::{self.type};"


@dataclass
class DISABLE_SRC_ZERO_FLAG(TemplateParameter):
    value: bool

    def covert_to_cpp(self) -> str:
        return f"constexpr bool disable_src_zero_flag = {self.value};"


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
        return f"constexpr bool tilize_en = {self.value};"


# @dataclass
# class BROADCAST_TYPE(TemplateParameter):
#     def covert_to_cpp(self) -> str:
#         return ""

# @dataclass
# class BROADCAST_TYPE(TemplateParameter):
#     def covert_to_cpp(self) -> str:
#         return ""


@dataclass
class REDUCE_POOL_TYPE(TemplateParameter):
    type: ReducePool

    def covert_to_cpp(self) -> str:
        return f"constexpr auto POOL_TYPE = ckernel::PoolType::{self.type};"


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
    both_a_and_b: Optional[bool]
    partial_a: Optional[bool]
    partial_b: Optional[bool]

    def covert_to_cpp(self) -> str:  # TODO Rewrite this
        # partial_face_A = str(
        #     test_config.get("partial_face_A", test_config.get("partial_face", False))  # RT
        # ).lower()
        # header_content.append(f"constexpr bool PARTIAL_FACE_A = {partial_face_A};")
        # header_content.append(f"constexpr bool PARTIAL_FACE_PACK = {partial_face_A};")

        # partial_face_B = str(
        #     test_config.get("partial_face_B", test_config.get("partial_face", False))  # RT
        # ).lower()
        # header_content.append(f"constexpr bool PARTIAL_FACE_B = {partial_face_B};")
        # header_content.append(f"constexpr bool PARTIAL_FACE_MATH = {partial_face_B};")

        return ""


@dataclass
class CRK_TILE_DIMM(RuntimeParameter):
    c_dimm: int
    r_dimm: int
    k_dimm: int

    def covert_to_cpp(self) -> str:
        return f"constexpr uint32_t RT_DIM = {self.r_dimm};\nconstexpr uint32_t CT_DIM = {self.c_dimm};\nconstexpr uint32_t KT_DIM = {self.k_dimm};"


# @dataclass
# class (RuntimeParameter):
#     def covert_to_cpp(self) -> str:
#         return ""

# @dataclass
# class (RuntimeParameter):
#     def covert_to_cpp(self) -> str:
#         return ""

# @dataclass
# class (RuntimeParameter):
#     def covert_to_cpp(self) -> str:
#         return ""

# @dataclass
# class (RuntimeParameter):
#     def covert_to_cpp(self) -> str:
#         return ""
