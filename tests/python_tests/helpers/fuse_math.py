# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
#
# SPDX-License-Identifier: Apache-2.0

from typing import TYPE_CHECKING, List, Type

import torch

from .golden_generators import (
    BinarySFPUGolden2,
    DataCopyGolden,
    EltwiseBinaryGolden,
    MatmulGolden,
    TilizeGolden,
    UnarySFPUGolden,
    get_golden_generator,
)

if TYPE_CHECKING:
    from .fuse_operation import PipelineOperation

from .chip_architecture import ChipArchitecture
from .format_config import DataFormat
from .llk_params import ApproximationMode, MathOperation, Tilize
from .tilize_untilize import tilize_block, untilize_block

from .llk_params import ApproximationMode, MathOperation


class Fpu:
    def exec(self, operation_config: "PipelineOperation") -> str:
        return ""

    def golden(self, operation_config: "PipelineOperation") -> torch.Tensor:
        return torch.Tensor()

    def get_headers(self) -> List[str]:
        return []


class MatmulFpu(Fpu):
    def get_headers(self) -> List[str]:
        return [
            "llk_math_common.h",
            "llk_math_matmul.h",
        ]

    def golden(
        self,
        tensor_a: torch.Tensor,
        tensor_b: torch.Tensor,
        operation_config: "PipelineOperation",
    ) -> torch.Tensor:
        src_a = operation_config.src_a
        src_b = operation_config.src_b
        output_format = operation_config.output.data_format
        math_fidelity = operation_config.math_fidelity

        generate_golden = get_golden_generator(MatmulGolden)
        golden = generate_golden(
            tensor_a,
            tensor_b,
            output_format,
            math_fidelity,
            input_A_dimensions=src_a.dimensions,
            input_B_dimensions=src_b.dimensions,
            tilize=False,
        )
        return golden

    def exec(self, operation_config: "PipelineOperation") -> str:
        stage = operation_config.stage_id
        CT_DIM = operation_config.ct_dim
        RT_DIM = operation_config.rt_dim
        KT_DIM = operation_config.kt_dim
        math_format = operation_config.math_format
        MATH_FORMAT = f"static_cast<std::underlying_type_t<DataFormat>>(DataFormat::{math_format.name})"

        fidelity = operation_config.math_fidelity
        MATH_FIDELITY = fidelity.value

        dest_acc = operation_config.dest_acc.value

        code = f"\n\t// Operation {stage}: Matmul FPU\n"

        code += f"""
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


class EltwiseFpu(Fpu):
    def __init__(self, operation: MathOperation):
        if not operation in MathOperation.get_fpu_binary_operations():
            raise ValueError(
                f"Operation {operation} is not a valid FPU binary operation."
            )
        self.operation = operation

    def get_headers(self) -> List[str]:
        return [
            "llk_math_common.h",
            "llk_math_eltwise_binary.h",
        ]

    def golden(
        self,
        tensor_a: torch.Tensor,
        tensor_b: torch.Tensor,
        operation_config: "PipelineOperation",
    ) -> torch.Tensor:
        output_format = operation_config.output.data_format
        math_fidelity = operation_config.math_fidelity

        generate_golden = get_golden_generator(EltwiseBinaryGolden)
        golden_tensor = generate_golden(
            self.operation, tensor_a, tensor_b, output_format, math_fidelity
        ).flatten()

        return golden_tensor

    def exec(self, operation_config: "PipelineOperation") -> str:
        stage = operation_config.stage_id
        math_format = operation_config.math_format
        MATH_FORMAT = f"static_cast<std::underlying_type_t<DataFormat>>(DataFormat::{math_format.name})"

        fidelity = operation_config.math_fidelity
        MATH_FIDELITY = fidelity.value

        dest_acc = operation_config.dest_acc.value
        tile_cnt = operation_config.output.tile_count

        code = (
            f"\n\t// Operation {stage}: Eltwise {self.operation.cpp_enum_value} FPU\n"
        )

        code += f"""
    _llk_math_pack_sync_init_<DstSync::SyncHalf, {dest_acc}>();
    _llk_math_hw_configure_<false, false>(
        {MATH_FORMAT},
        {MATH_FORMAT}
    );
    _llk_math_eltwise_binary_init_<ckernel::EltwiseBinaryType::{self.operation.cpp_enum_value}, BroadcastType::NONE, {MATH_FIDELITY}>(4, 0, 0);

    _llk_math_wait_for_dest_available_<DstSync::SyncHalf>();
    for (int i = 0; i < {tile_cnt}; i++)
    {{
        _llk_math_eltwise_binary_<
            {self.operation.cpp_enum_value},
            BroadcastType::NONE,
            DstSync::SyncHalf,
            {dest_acc},
            {MATH_FIDELITY},
            EltwiseBinaryReuseDestType::NONE>(4, i, false);
    }}
"""
        return code


class DatacopyFpu(Fpu):
    def get_headers(self) -> List[str]:
        return [
            "llk_math_common.h",
            "llk_math_eltwise_unary_datacopy.h",
        ]

    def golden(
        self,
        tensor_a: torch.Tensor,
        tensor_b: torch.Tensor,
        operation_config: "PipelineOperation",
    ) -> torch.Tensor:
        golden_tensor = tensor_a
        if operation_config.tilize == Tilize.Yes:
            golden_generator = get_golden_generator(TilizeGolden)
            golden_tensor = golden_generator(
                golden_tensor,
                operation_config.src_a.dimensions,
                operation_config.output.data_format,
                operation_config.num_faces,
            )
        golden_generator = get_golden_generator(DataCopyGolden)
        golden_tensor = golden_generator(
            golden_tensor,
            operation_config.output.data_format,
            num_faces=operation_config.num_faces,
            input_dimensions=operation_config.src_a.dimensions,
            face_r_dim=operation_config.face_r_dim,
        )

        return golden_tensor

    def exec(self, operation_config: "PipelineOperation") -> str:
        stage = operation_config.stage_id
        math_format = operation_config.math_format
        MATH_FORMAT = f"static_cast<std::underlying_type_t<DataFormat>>(DataFormat::{math_format.name})"

        dest_acc = operation_config.dest_acc.value
        tile_cnt = operation_config.output.tile_count
        tilize_en = operation_config.tilize.value
        brodcast_type = "NONE"  # TODO: make dynamic based on operation_config
        unpack_to_dest = "true" if operation_config.unpack_to_dest else "false"
        data_copy_type = operation_config.data_copy_type.value
        num_faces = operation_config.num_faces
        is_int_fpu_en = dest_acc
        dst_index = operation_config.dst_index

        code = f"\n\t// Operation {stage}: Datacopy FPU\n"

        if operation_config.architecture == ChipArchitecture.BLACKHOLE:
            code += f"""
    _llk_math_eltwise_unary_datacopy_init_<DataCopyType::{data_copy_type}, {dest_acc}, BroadcastType::{brodcast_type}, {tilize_en}, {is_int_fpu_en}>(
        0, 0, {num_faces}, {MATH_FORMAT});
"""
        elif operation_config.architecture == ChipArchitecture.WORMHOLE:
            code += f"""
    _llk_math_eltwise_unary_datacopy_init_<DataCopyType::{data_copy_type}, {dest_acc}, BroadcastType::{brodcast_type}, {is_int_fpu_en}>(0, 0, {num_faces}, {MATH_FORMAT});
"""
        else:
            raise ValueError("Unsupported architecture for DatacopyFpu")

        code += f"""
    _llk_math_pack_sync_init_<DstSync::SyncHalf, {dest_acc}>();
    _llk_math_hw_configure_<false, false>(
        {MATH_FORMAT},
        {MATH_FORMAT}
    );
    _llk_math_wait_for_dest_available_<DstSync::SyncHalf>();
    for (int i = 0; i < {tile_cnt}; ++i)
    {{
    """
        if operation_config.architecture == ChipArchitecture.BLACKHOLE:
            code += f"""
        _llk_math_eltwise_unary_datacopy_<DataCopyType::{data_copy_type}, DstSync::SyncHalf, {dest_acc}, BroadcastType::{brodcast_type}, {unpack_to_dest}>(
            {dst_index} + i, {MATH_FORMAT}, {MATH_FORMAT}, {num_faces});
        """
        elif operation_config.architecture == ChipArchitecture.WORMHOLE:
            code += f"""
        _llk_math_eltwise_unary_datacopy_<DataCopyType::{data_copy_type}, DstSync::SyncHalf, {dest_acc}, BroadcastType::{brodcast_type}, {unpack_to_dest}>(
            {dst_index} + i, {MATH_FORMAT}, {MATH_FORMAT});
"""
        else:
            raise ValueError("Unsupported architecture for DatacopyFpu")
        code += f"""
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

    def golden(
        self, tensor: torch.Tensor, operation_config: "PipelineOperation"
    ) -> torch.Tensor:
        return tensor

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

    def golden(
        self, tensor: torch.Tensor, operation_config: "PipelineOperation"
    ) -> torch.Tensor:
        format_input = operation_config.src_a.data_format
        format_output = operation_config.output.data_format
        dest_acc = operation_config.dest_acc

        generate_sfpu_golden = get_golden_generator(UnarySFPUGolden)

        return generate_sfpu_golden(
            self.operation,
            tensor,
            format_output,
            dest_acc,
            format_input,
        )

    def exec(self, operation_config: "PipelineOperation") -> str:
        stage = operation_config.stage_id
        math_format = operation_config.math_format
        dest_acc = operation_config.dest_acc.value

        code = f"\n\t// Operation {stage}: Unary {self.operation.cpp_enum_value} SFPU\n"

        code += f"""
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

    def golden(
        self, tensor: torch.Tensor, operation_config: "PipelineOperation"
    ) -> torch.Tensor:
        math_format = operation_config.output.data_format
        dimensions = operation_config.output.dimensions

        if math_format != DataFormat.Bfp8_b:
            tilized_tensor = tilize_block(tensor.flatten(), dimensions, math_format)
        else:
            tilized_tensor = tensor.flatten()

        generate_binary_golden = get_golden_generator(BinarySFPUGolden2)
        tilized_result = generate_binary_golden(
            self.operation,
            tilized_tensor.flatten(),
            self.dst_index_in0,
            self.dst_index_in1,
            self.dst_index_out,
            self.iterations,
            math_format,
        )

        if math_format != DataFormat.Bfp8_b:
            golden_tensor = untilize_block(tilized_result, math_format, dimensions)
        else:
            golden_tensor = tilized_result

        return golden_tensor

    def exec(self, operation_config: "PipelineOperation") -> str:
        stage = operation_config.stage_id
        math_format = operation_config.math_format
        MATH_FORMAT = f"static_cast<std::underlying_type_t<DataFormat>>(DataFormat::{math_format.name})"
        code = (
            f"\n\t// Operation {stage}: Binary {self.operation.cpp_enum_value} SFPU\n"
        )

        code += f"""
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

    def golden(
        self, tensor: torch.Tensor, operation_config: "PipelineOperation"
    ) -> torch.Tensor:
        return tensor

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
    fpu: Fpu
    sfpu: List[Sfpu]

    def __init__(self, fpu: Type[Fpu], sfpu: List[Sfpu] = []):
        self.fpu = fpu
        self.sfpu = sfpu

    def get_headers(self) -> List[str]:
        headers = set()

        headers.update(self.fpu.get_headers())

        for sfpu in self.sfpu:
            headers.update(sfpu.get_headers())

        return sorted(list(headers))

    def golden(self, operation_config: "PipelineOperation") -> torch.Tensor:
        src_a_dims = operation_config.src_a.dimensions
        src_b_dims = operation_config.src_b.dimensions
        tensor_a = operation_config.src_a.raw_data.view(src_a_dims)
        tensor_b = operation_config.src_b.raw_data.view(src_b_dims)

        golden_tensor = self.fpu.golden(tensor_a, tensor_b, operation_config)

        for sfpu in self.sfpu:
            golden_tensor = sfpu.golden(golden_tensor, operation_config)

        return golden_tensor

    def exec(self, operation_config: "PipelineOperation") -> str:
        code = self.fpu.exec(operation_config)

        for sfpu in self.sfpu:
            code += sfpu.exec(operation_config)

        dest_acc = operation_config.dest_acc.value
        code += f"""
    _llk_math_dest_section_done_<DstSync::SyncHalf, {dest_acc}>();
"""
        return code
