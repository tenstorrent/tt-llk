# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
#
# SPDX-License-Identifier: Apache-2.0

from typing import TYPE_CHECKING, List, Type

if TYPE_CHECKING:
    from .fuse_operation import PipelineOperation

from .llk_params import ApproximationMode, MathOperation


class Fpu:
    def exec(self, operation_config: "PipelineOperation") -> str:
        return ""

    def get_headers(self) -> List[str]:
        return []


class MatmulFpu(Fpu):
    def get_headers(self) -> List[str]:
        return [
            "llk_math_common.h",
            "llk_math_matmul.h",
        ]

    def exec(self, operation_config: "PipelineOperation") -> str:
        CT_DIM = operation_config.ct_dim
        RT_DIM = operation_config.rt_dim
        KT_DIM = operation_config.kt_dim
        math_format = operation_config.math_format
        MATH_FORMAT = f"static_cast<std::underlying_type_t<DataFormat>>(DataFormat::{math_format.name})"

        fidelity = operation_config.math_fidelity
        MATH_FIDELITY = fidelity.value

        dest_acc = operation_config.dest_acc.value

        code = f"""
    _llk_math_matmul_init_<{MATH_FIDELITY}, DstTileFaceLayout::RowMajor>(TILE_R_DIM, TILE_C_DIM, TILE_R_DIM, TILE_C_DIM, false, 0, {CT_DIM}, {RT_DIM}, {KT_DIM});
    _llk_math_pack_sync_init_<DstSync::SyncHalf, {dest_acc}>();
    _llk_math_hw_configure_<false, false>(
        {MATH_FORMAT},
        {MATH_FORMAT}
    );
    _llk_math_wait_for_dest_available_<DstSync::SyncHalf>();
    for (uint32_t j = 0; j < {KT_DIM}; j++)
    {{
        _llk_math_matmul_<{MATH_FIDELITY}, DstTileFaceLayout::RowMajor>(0, 0, {CT_DIM}, {RT_DIM}, {KT_DIM});
    }}
"""
        return code


class Sfpu:
    operation: MathOperation
    approx_mode: ApproximationMode
    iterations: int = 32

    def __init__(
        self,
        operation: str,
        approx_mode: ApproximationMode = ApproximationMode.No,
        iterations: int = 32,
    ):
        self.operation = operation
        self.approx_mode = approx_mode
        self.iterations = iterations

    def exec(self, operation_config: "PipelineOperation") -> str:
        return ""

    def get_headers(self) -> List[str]:
        return []


class UnarySfpu(Sfpu):
    def __init__(
        self,
        operation: MathOperation,
        approx_mode: ApproximationMode = ApproximationMode.No,
        iterations: int = 32,
    ):
        if not operation in MathOperation.get_sfpu_unary_operations():
            raise ValueError(
                f"Operation {operation} is not a valid SFPU unary operation."
            )
        super().__init__(operation, approx_mode, iterations)

    def get_headers(self) -> List[str]:
        return [
            "ckernel_defs.h",
            "ckernel_sfpu.h",
            "llk_math_common.h",
            "llk_math_eltwise_unary_sfpu.h",
            "sfpu_operations.h",
        ]

    def exec(self, operation_config: "PipelineOperation") -> str:
        math_format = operation_config.math_format
        dest_acc = operation_config.dest_acc.value
        code = f"""
    _llk_math_eltwise_unary_sfpu_init_<SfpuType::{self.operation.cpp_enum_value}>();
    _llk_math_eltwise_unary_sfpu_start_<DstSync::SyncHalf>(0);
    test_utils::call_sfpu_operation<{self.iterations}, {dest_acc}, {self.approx_mode.value}>(
        SfpuType::{self.operation.cpp_enum_value},
        static_cast<std::underlying_type_t<DataFormat>>(DataFormat::{math_format.name})
    );
    _llk_math_eltwise_unary_sfpu_done_();
"""
        return code


class BinarySfpu(Sfpu):
    def __init__(
        self,
        operation: MathOperation,
        approx_mode: ApproximationMode = ApproximationMode.No,
        iterations: int = 32,
        dst_index_in0: int = 0,
        dst_index_in1: int = 1,
        dst_index_out: int = 0,
    ):
        if not operation in MathOperation.get_sfpu_binary_operations():
            raise ValueError(
                f"Operation {operation} is not a valid SFPU binary operation."
            )
        super().__init__(operation, approx_mode, iterations)
        self.dst_index_in0 = dst_index_in0
        self.dst_index_in1 = dst_index_in1
        self.dst_index_out = dst_index_out

    def get_headers(self) -> List[str]:
        return [
            "ckernel_defs.h",
            "ckernel_sfpu.h",
            "ckernel_sfpu_binary.h",
            "llk_math_common.h",
            "llk_math_eltwise_binary_sfpu.h",
            "sfpu_operations.h",
        ]

    def exec(self, operation_config: "PipelineOperation") -> str:
        math_format = operation_config.math_format
        MATH_FORMAT = f"static_cast<std::underlying_type_t<DataFormat>>(DataFormat::{math_format.name})"
        code = f"""
    _llk_math_eltwise_binary_sfpu_init_<SfpuType::add1>();
    _llk_math_eltwise_binary_sfpu_start_<DstSync::SyncHalf>(0);
    test_utils::call_binary_sfpu_operation<{self.approx_mode.value}, ckernel::BinaryOp::{self.operation.cpp_enum_value}, {self.iterations}, {MATH_FORMAT}>({self.dst_index_in0}, {self.dst_index_in1}, {self.dst_index_out});
    _llk_math_eltwise_binary_sfpu_done_();
"""
        return code


class SfpuWhere(Sfpu):
    dst_index_in0: int = 0
    dst_index_in1: int = 1
    dst_index_in2: int = 2
    dst_index_out: int = 0

    def __init__(
        self,
        approx_mode: ApproximationMode = ApproximationMode.No,
        iterations: int = 32,
        dst_index_in0: int = 0,
        dst_index_in1: int = 1,
        dst_index_in2: int = 2,
        dst_index_out: int = 0,
    ):
        super().__init__("where", approx_mode, iterations)
        self.dst_index_in0 = dst_index_in0
        self.dst_index_in1 = dst_index_in1
        self.dst_index_in2 = dst_index_in2
        self.dst_index_out = dst_index_out

    def get_headers(self) -> List[str]:
        return [
            "ckernel_defs.h",
            "ckernel_sfpu.h",
            "ckernel_sfpu_where.h",
            "llk_math_common.h",
            "llk_math_eltwise_ternary_sfpu.h",
        ]

    def exec(self, operation_config: "PipelineOperation") -> str:
        math_format = operation_config.math_format
        MATH_FORMAT = f"static_cast<std::underlying_type_t<DataFormat>>(DataFormat::{math_format.name})"
        code = f"""
    _llk_math_eltwise_ternary_sfpu_init_<SfpuType::where>();
    ckernel::sfpu::_init_where_<{self.approx_mode.value}>();
    _llk_math_eltwise_ternary_sfpu_start_<DstSync::SyncHalf>(0);
    ckernel::sfpu::_calculate_where_<false, static_cast<DataFormat>({MATH_FORMAT}), {self.iterations}>({self.dst_index_in0}, {self.dst_index_in1}, {self.dst_index_in2}, {self.dst_index_out});
    _llk_math_eltwise_ternary_sfpu_done_();
"""
        return code


class Math:
    fpu: Type[Fpu]
    sfpu: List[Sfpu]

    def __init__(self, fpu: Type[Fpu], sfpu: List[Sfpu]):
        self.fpu = fpu
        self.sfpu = sfpu

    def get_headers(self) -> List[str]:
        headers = set()

        fpu_instance = self.fpu()
        headers.update(fpu_instance.get_headers())

        for sfpu in self.sfpu:
            headers.update(sfpu.get_headers())

        return sorted(list(headers))

    def exec(self, operation_config: "PipelineOperation") -> str:
        fpu_instance = self.fpu()
        code = fpu_instance.exec(operation_config)

        for sfpu in self.sfpu:
            code += sfpu.exec(operation_config)

        dest_acc = operation_config.dest_acc.value
        code += f"""
    _llk_math_dest_section_done_<DstSync::SyncHalf, {dest_acc}>();
"""
        return code
