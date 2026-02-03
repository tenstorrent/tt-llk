# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
#
# SPDX-License-Identifier: Apache-2.0

from typing import TYPE_CHECKING, List, Type

import torch

from .golden_generators import (
    BinarySFPUGolden,
    DataCopyGolden,
    EltwiseBinaryGolden,
    MatmulGolden,
    ReduceGolden,
    UnarySFPUGolden,
    get_golden_generator,
)

if TYPE_CHECKING:
    from .fused_operation import FusedOperation
    from .fuser_config import GlobalConfig
    from .fused_unpacker import Unpacker

from .chip_architecture import ChipArchitecture
from .llk_params import (
    ApproximationMode,
    BroadcastType,
    DataCopyType,
    DestSync,
    MathOperation,
    PerfRunType,
    ReduceDimension,
    ReducePool,
    Transpose,
)
from .tilize_untilize import tilize_block, untilize_block


class FusedCompute:
    def __init__(
        self,
        unpacker=None,
        fpu=None,
        sfpu=None,
        unpack_transpose_faces: Transpose = Transpose.No,
        unpack_transpose_within_face: Transpose = Transpose.No,
        broadcast_type: BroadcastType = BroadcastType.None_,
        data_copy_type: DataCopyType = DataCopyType.A2D,
    ):
        if fpu is None and sfpu is None:
            raise ValueError("Compute unit need fpu or sfpu unit")
        if fpu is not None and sfpu is not None:
            raise ValueError("Compute unit can be only fpu or sfpu")
        if sfpu is not None and unpacker is not None:
            raise ValueError("Sfpu unit does not support unpacker")
        if fpu is not None and unpacker not in fpu.supported_unpackers():
            raise ValueError(f"{fpu} does not support {unpacker}")

        self.unpacker = unpacker
        self.fpu = fpu
        self.sfpu = sfpu
        self.unpack_transpose_faces = unpack_transpose_faces
        self.unpack_transpose_within_face = unpack_transpose_within_face
        self.broadcast_type = broadcast_type

        if (
            self.broadcast_type != BroadcastType.None_
            and data_copy_type == DataCopyType.A2D
        ):
            self.data_copy_type = DataCopyType.B2D
        elif (
            self.broadcast_type == BroadcastType.None_
            and data_copy_type == DataCopyType.B2D
        ):
            self.data_copy_type = DataCopyType.A2D
        else:
            self.data_copy_type = data_copy_type

    def math_init(self, operation: "FusedOperation", config: "GlobalConfig") -> str:
        if self.fpu is not None:
            return self.fpu.init(operation, config, self)
        elif self.sfpu is not None:
            return self.sfpu.init(operation, config, self)
        else:
            raise ValueError("fpu and sfpu are not defined")

    def math_calculate(
        self, operation: "FusedOperation", config: "GlobalConfig", batch_tile_cnt
    ) -> str:
        if self.fpu is not None:
            code = ""
            for tile_idx in range(batch_tile_cnt):
                code += self.fpu.calculate(operation, config, self, tile_idx)
            return code
        elif self.sfpu is not None:
            return self.sfpu.calculate(operation, config, self)
        else:
            raise ValueError("fpu and sfpu are not defined")

    def math_uninit(self, operation: "FusedOperation", config: "GlobalConfig") -> str:
        if self.fpu is not None:
            return self.fpu.uninit(operation, config, self)
        elif self.sfpu is not None:
            return self.sfpu.uninit(operation, config, self)
        else:
            raise ValueError("fpu and sfpu are not defined")


class NewMath:
    operations: List[FusedCompute]

    def __init__(self, operations: List[FusedCompute]):
        self.operations = operations

    def get_unpackers(self) -> List["Unpacker"]:
        unpackers: List["Unpacker"] = []

        for operation in self.operations:
            if operation.unpacker is not None:
                unpackers.append(operation.unpacker)

        return unpackers

    def get_math_units(self) -> List["Unpacker"]:
        math_units = []

        for operation in self.operations:
            if operation.fpu is not None:
                math_units.append(operation.fpu)

            if operation.sfpu is not None:
                math_units.append(operation.sfpu)

        return math_units

    def has_unpacker(self, unpacker) -> bool:
        for operation in self.operations:
            if isinstance(operation.unpacker, unpacker):
                return True

        return False

    def has_fpu(self, fpu) -> bool:
        for operation in self.operations:
            if isinstance(operation.fpu, fpu):
                return True

        return False

    def get_fused_compute_with_unpacker(self) -> List["FusedCompute"]:
        return [op for op in self.operations if op.unpacker is not None]

    def get_reduce_pack_mask(self) -> str:
        # all_masks = set()
        reduce_op = None
        for operation in self.operations:
            if isinstance(operation.fpu, ReduceFpu):
                reduce_op = operation.fpu.operation

        if reduce_op is None:
            return None

        return f"ReduceDim::{reduce_op.cpp_enum_value}"

    def unpack_operand_constants(
        self, operation: "FusedOperation", config: "GlobalConfig"
    ) -> str:
        stage = operation.stage_id
        buffer_A_address = operation.src_a.l1_address
        buffer_B_address = operation.src_b.l1_address
        buffer_A_tile_size = operation.buffer_A_tile_size
        buffer_B_tile_size = operation.buffer_B_tile_size
        unpack_a_src = operation.unpack_a_in
        unpack_a_dst = operation.unpack_a_out
        unpack_b_src = operation.unpack_a_in
        unpack_b_dst = operation.unpack_a_out

        code = (
            f"    // Operation {stage}: Fused Unpack\n"
            f"    UNUSED const Operand buffer_A{stage}({hex(buffer_A_address)}, {buffer_A_tile_size});\n"
            f"    UNUSED const Operand buffer_B{stage}({hex(buffer_B_address)}, {buffer_B_tile_size});\n"
            f"    UNUSED const uint32_t unpack_a_src_format{stage} = static_cast<std::underlying_type_t<DataFormat>>(DataFormat::{unpack_a_src.name});\n"
            f"    UNUSED const uint32_t unpack_a_dst_format{stage} = static_cast<std::underlying_type_t<DataFormat>>(DataFormat::{unpack_a_dst.name});\n"
            f"    UNUSED const uint32_t unpack_b_src_format{stage} = static_cast<std::underlying_type_t<DataFormat>>(DataFormat::{unpack_b_src.name});\n"
            f"    UNUSED const uint32_t unpack_b_dst_format{stage} = static_cast<std::underlying_type_t<DataFormat>>(DataFormat::{unpack_b_dst.name});\n"
        )
        return code

    def unpack_body(self, operation: "FusedOperation", config: "GlobalConfig") -> str:
        fused_ops_with_unpacker = self.get_fused_compute_with_unpacker()

        if not fused_ops_with_unpacker:
            return ""

        return self._unpack_batched_body(operation, config, fused_ops_with_unpacker)

    def _unpack_batched_body(
        self,
        operation: "FusedOperation",
        config: "GlobalConfig",
        fused_ops_with_unpacker: List["FusedCompute"],
    ) -> str:
        batch_size = operation.batch_size
        tile_cnt = operation.output.tile_count

        first_unpacker = fused_ops_with_unpacker[0].unpacker()

        code = self.unpack_operand_constants(operation, config)
        code += first_unpacker.hw_configure(operation, config)

        code += first_unpacker.packer_sync(operation)

        num_full_batches = tile_cnt // batch_size
        remaining_tiles = tile_cnt % batch_size

        if num_full_batches > 0:
            code += (
                f"    for (uint32_t batch = 0; batch < {num_full_batches}; ++batch)\n"
            )
            code += "    {\n"
            for fused_op in fused_ops_with_unpacker:
                unpacker_instance = fused_op.unpacker()
                code += unpacker_instance.init(operation, config, fused_op)
                code += f"        for (uint32_t i = 0; i < {batch_size}; ++i)\n"
                code += "        {\n"
                code += f"            uint32_t tile_idx = batch * {batch_size} + i;\n"
                code += unpacker_instance.unpack(
                    operation, config, fused_op, "tile_idx"
                )
                code += "        }\n"
                code += unpacker_instance.uninit(operation, config, fused_op)
            code += "    }\n"

        if remaining_tiles > 0:
            if num_full_batches > 0:
                code += "    {\n"
                code += f"        const uint32_t batch_offset = {num_full_batches * batch_size};\n"
                for fused_op in fused_ops_with_unpacker:
                    unpacker_instance = fused_op.unpacker()
                    code += unpacker_instance.init(operation, config, fused_op)
                    code += (
                        f"        for (uint32_t i = 0; i < {remaining_tiles}; ++i)\n"
                    )
                    code += "        {\n"
                    code += "            uint32_t tile_idx = batch_offset + i;\n"
                    code += unpacker_instance.unpack(
                        operation, config, fused_op, "tile_idx"
                    )
                    code += "        }\n"
                    code += unpacker_instance.uninit(operation, config, fused_op)
                code += "    }\n"
            else:
                for fused_op in fused_ops_with_unpacker:
                    unpacker_instance = fused_op.unpacker()
                    code += unpacker_instance.init(operation, config, fused_op)
                    code += f"    for (uint32_t i = 0; i < {remaining_tiles}; ++i)\n"
                    code += "    {\n"
                    code += unpacker_instance.unpack(operation, config, fused_op, "i")
                    code += "    }\n"
                    code += unpacker_instance.uninit(operation, config, fused_op)

        return code

    def math_hw_configure(
        self, operation: "FusedOperation", config: "GlobalConfig"
    ) -> str:
        stage = operation.stage_id
        dest_acc = config.dest_acc.value
        if stage == 0:
            code = f"_llk_math_hw_configure_<{dest_acc}>(math_format{stage}, math_format{stage});\n"
        else:
            code = f"_llk_math_reconfig_data_format_<{dest_acc}, false>(math_format{stage}, math_format{stage});\n"

        code += f"_llk_math_pack_sync_init_<dest_sync{stage}, {dest_acc}>();\n\n"

        return code

    def _wait_for_dest(self, operation: "FusedOperation") -> str:
        return f"_llk_math_wait_for_dest_available_<dest_sync{operation.stage_id}>();\n"

    def _dest_section_done(
        self, operation: "FusedOperation", config: "GlobalConfig"
    ) -> str:
        return f"_llk_math_dest_section_done_<dest_sync{operation.stage_id}, {config.dest_acc.value}>();\n"

    def _matmul_tile_loop(
        self, operation: "FusedOperation", config: "GlobalConfig", body_fn
    ) -> str:
        rt_dim = operation.rt_dim
        ct_dim = operation.ct_dim
        code = f"for (uint32_t mt = 0; mt < {rt_dim}; ++mt) {{\n"
        code += f"for (uint32_t nt = 0; nt < {ct_dim}; ++nt) {{\n"
        code += body_fn()
        code += "}\n"
        code += "}\n"
        return code

    def _batch_loop(
        self, operation: "FusedOperation", config: "GlobalConfig", body_fn
    ) -> str:
        batch_size = operation.batch_size
        tile_cnt = operation.output.tile_count

        num_full_batches = tile_cnt // batch_size
        remaining_tiles = tile_cnt % batch_size

        code = ""

        if num_full_batches > 0:
            code += (
                f"for (uint32_t batch = 0; batch < {num_full_batches}; ++batch) {{\n"
            )
            code += body_fn(batch_size)
            code += "}\n"

        if remaining_tiles > 0:
            code += "{\n"
            code += body_fn(remaining_tiles)
            code += "}\n"

        return code

    def _math_constants(
        self, operation: "FusedOperation", config: "GlobalConfig"
    ) -> str:
        stage = operation.stage_id
        math_format = operation.output.data_format
        dest_sync = operation.dest_sync

        dest_sync_map = {
            DestSync.Half: "SyncHalf",
            DestSync.Full: "SyncFull",
        }
        dest_sync_str = dest_sync_map.get(dest_sync, "SyncHalf")

        code = f"// Operation {stage}: Math Setup\n"
        code += f"const uint32_t math_format{stage} = static_cast<std::underlying_type_t<DataFormat>>(DataFormat::{math_format.name});\n"
        code += f"constexpr DstSync dest_sync{stage} = DstSync::{dest_sync_str};\n"

        return code

    def math_body(self, operation: "FusedOperation", config: "GlobalConfig") -> str:
        code = self._math_constants(operation, config)
        code += self.math_hw_configure(operation, config)

        def batch_body(batch_tile_cnt):
            body = self._wait_for_dest(operation)
            for compute_unit in self.operations:
                body += compute_unit.math_init(operation, config)
                body += compute_unit.math_calculate(operation, config, batch_tile_cnt)
                body += compute_unit.math_uninit(operation, config)
            body += self._dest_section_done(operation, config)
            return body

        code += self._batch_loop(operation, config, batch_body)

        return code

    def golden(
        self,
        tensor_a: torch.Tensor,
        tensor_b: torch.Tensor,
        operation: "FusedOperation",
        config: "GlobalConfig",
    ) -> torch.Tensor:
        return torch.ones(operation.output.dimensions)


class Fpu:
    def init(
        self,
        operation: "FusedOperation",
        config: "GlobalConfig",
        compute_unit: "FusedCompute",
    ) -> str:
        return ""

    def supported_unpackers(self) -> List["Unpacker"]:
        return []

    def calculate(
        self,
        operation: "FusedOperation",
        config: "GlobalConfig",
        compute_unit: "FusedCompute",
        tile_idx: int,
    ) -> str:
        return ""

    def uninit(
        self,
        operation: "FusedOperation",
        config: "GlobalConfig",
        compute_unit: "FusedCompute",
    ) -> str:
        return ""

    def golden(
        self,
        tensor_a: torch.Tensor,
        tensor_b: torch.Tensor,
        operation: "FusedOperation",
        config: "GlobalConfig",
        compute_unit: "FusedCompute",
    ) -> torch.Tensor:
        return torch.Tensor()

    def get_headers(self) -> List[str]:
        return []

    def __str__(self) -> str:
        return self.__class__.__name__


class MatmulFpu(Fpu):
    def get_headers(self) -> List[str]:
        return [
            "llk_math_common.h",
            "llk_math_matmul.h",
        ]

    def supported_unpackers(self) -> List["Unpacker"]:
        from .fused_unpacker import MatmulUnpacker

        return [MatmulUnpacker]

    def golden(
        self,
        tensor_a: torch.Tensor,
        tensor_b: torch.Tensor,
        operation: "FusedOperation",
        config: "GlobalConfig",
        compute_unit: "FusedCompute",
    ) -> torch.Tensor:
        src_a = operation.src_a
        src_b = operation.src_b
        output_format = operation.output.data_format
        math_fidelity = operation.math_fidelity

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

    def init(
        self,
        operation: "FusedOperation",
        config: "GlobalConfig",
        compute_unit: "FusedCompute",
    ) -> str:
        stage = operation.stage_id
        batch_size = operation.batch_size
        math_fidelity = operation.math_fidelity.value
        ct_dim = operation.ct_dim
        rt_dim = operation.rt_dim
        transpose = "true" if compute_unit.unpack_transpose_faces.value else "false"

        if batch_size == 1:
            return (
                f"// Operation {stage}: Matmul FPU\n"
                f"_llk_math_matmul_init_<{math_fidelity}>(\n"
                f"    TILE_R_DIM, TILE_C_DIM, TILE_R_DIM, TILE_C_DIM, false, {transpose}, 1, 1\n"
                f");\n"
            )
        else:
            return (
                f"// Operation {stage}: Matmul FPU\n"
                f"_llk_math_matmul_init_<{math_fidelity}>(\n"
                f"    TILE_R_DIM, TILE_C_DIM, TILE_R_DIM, TILE_C_DIM, false, {transpose}, {ct_dim}, {rt_dim}\n"
                f");\n"
            )

    def calculate(
        self,
        operation: "FusedOperation",
        config: "GlobalConfig",
        compute_unit: "FusedCompute",
        tile_idx: int,
    ) -> str:
        batch_size = operation.batch_size
        ct_dim = operation.ct_dim
        rt_dim = operation.rt_dim
        kt_dim = operation.kt_dim
        math_fidelity = operation.math_fidelity.value
        if batch_size == 1:
            return (
                f"for (uint32_t j = 0; j < {kt_dim}; j++)\n"
                f"{{\n"
                f"    _llk_math_matmul_<{math_fidelity}>(0, 1, 1);\n"
                f"}}\n"
            )
        else:
            return (
                f"for (uint32_t j = 0; j < {kt_dim}; j++)\n"
                f"{{\n"
                f"    _llk_math_matmul_<{math_fidelity}>(0, {ct_dim}, {rt_dim});\n"
                f"}}\n"
            )


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

    def supported_unpackers(self) -> List["Unpacker"]:
        from .fused_unpacker import UnpackerA, UnpackerAB

        return [UnpackerAB, UnpackerA]

    def golden(
        self,
        tensor_a: torch.Tensor,
        tensor_b: torch.Tensor,
        operation: "FusedOperation",
        config: "GlobalConfig",
        compute_unit: "FusedCompute",
    ) -> torch.Tensor:
        output_format = operation.output.data_format
        math_fidelity = operation.math_fidelity

        generate_golden = get_golden_generator(EltwiseBinaryGolden)
        golden_tensor = generate_golden(
            self.operation, tensor_a, tensor_b, output_format, math_fidelity
        ).flatten()

        return golden_tensor

    def init(
        self,
        operation: "FusedOperation",
        config: "GlobalConfig",
        compute_unit: "FusedCompute",
    ) -> str:
        stage = operation.stage_id
        math_fidelity = operation.math_fidelity.value
        op = self.operation.cpp_enum_value
        num_faces = operation.num_faces
        broadcast_type = f"BroadcastType::{compute_unit.broadcast_type.value}"

        return (
            f"    // Operation {stage}: Eltwise {op} FPU\n"
            f"    _llk_math_eltwise_binary_init_<ckernel::EltwiseBinaryType::{op}, {broadcast_type}, {math_fidelity}>({num_faces}, 0);\n"
        )

    def calculate(
        self,
        operation: "FusedOperation",
        config: "GlobalConfig",
        compute_unit: "FusedCompute",
        tile_idx: int,
    ) -> str:
        stage = operation.stage_id
        math_fidelity = operation.math_fidelity.value
        dest_acc = config.dest_acc.value
        op = self.operation.cpp_enum_value
        num_faces = operation.num_faces
        broadcast_type = f"BroadcastType::{compute_unit.broadcast_type.value}"

        return (
            f"    _llk_math_eltwise_binary_<{op}, {broadcast_type}, dest_sync{stage},\n"
            f"        {dest_acc}, {math_fidelity}, EltwiseBinaryReuseDestType::NONE>({num_faces}, {tile_idx}, false\n"
            f"    );\n"
        )

    def __str__(self) -> str:
        return f"EltwiseFpu({self.operation})"


class ReduceFpu(Fpu):
    def __init__(self, operation: MathOperation, pool: ReducePool = ReducePool.Max):
        if operation not in MathOperation.get_reduce_operations():
            raise ValueError(f"Operation {operation} is not a valid REDUCE operation.")
        self.operation = operation
        self.pool = pool

    def get_headers(self) -> List[str]:
        return [
            "llk_math_common.h",
            "llk_math_reduce.h",
        ]

    def supported_unpackers(self) -> List["Unpacker"]:
        from .fused_unpacker import UnpackerAB

        return [UnpackerAB]

    def reduce_dim(self) -> str:
        return f"ReduceDim::{self.operation.cpp_enum_value}"

    def reduce_dim_golden(self) -> ReduceDimension:
        if self.operation == MathOperation.ReduceColumn:
            return ReduceDimension.Column
        elif self.operation == MathOperation.ReduceRow:
            return ReduceDimension.Row
        elif self.operation == MathOperation.ReduceScalar:
            return ReduceDimension.Scalar
        else:
            raise ValueError(f"Unsupported reduce operation: {self.operation}")

    def golden(
        self,
        tensor_a: torch.Tensor,
        tensor_b: torch.Tensor,
        operation: "FusedOperation",
        config: "GlobalConfig",
        compute_unit: "FusedCompute",
    ) -> torch.Tensor:
        output_format = operation.output.data_format
        tile_cnt = operation.output.tile_count
        dimensions = operation.output.dimensions
        num_faces = operation.num_faces

        reduce_dim = self.reduce_dim_golden()
        pool_type = self.pool

        tensor_a = tilize_block(
            tensor_a, dimensions, output_format, num_faces
        ).flatten()

        generate_golden = get_golden_generator(ReduceGolden)
        golden_tensor = generate_golden(
            tensor_a,
            reduce_dim,
            pool_type,
            output_format,
            tile_cnt=tile_cnt,
        ).flatten()

        golden_tensor = untilize_block(golden_tensor, output_format, dimensions)

        return golden_tensor.flatten()

    def init(
        self,
        operation: "FusedOperation",
        config: "GlobalConfig",
        compute_unit: "FusedCompute",
    ) -> str:
        stage = operation.stage_id
        math_fidelity = operation.math_fidelity.value
        dest_acc = config.dest_acc.value
        pool_type_cpp = f"PoolType::{self.pool.value}"
        reduce_dim_cpp = self.reduce_dim()

        return (
            f"    // Operation {stage}: Reduce {self.operation.cpp_enum_value} FPU\n"
            f"    _llk_math_reduce_init_<{pool_type_cpp}, {reduce_dim_cpp}, {dest_acc}, {math_fidelity}, false>();\n"
        )

    def calculate(
        self,
        operation: "FusedOperation",
        config: "GlobalConfig",
        compute_unit: "FusedCompute",
        tile_idx: int,
    ) -> str:
        math_fidelity = operation.math_fidelity.value
        dest_acc = config.dest_acc.value
        num_faces = operation.num_faces
        pool_type_cpp = f"PoolType::{self.pool.value}"
        reduce_dim_cpp = self.reduce_dim()

        return (
            f"    _llk_math_reduce_<{pool_type_cpp}, {reduce_dim_cpp}, {dest_acc}, {math_fidelity}, false, false>(\n"
            f"        {tile_idx}, false, {num_faces}\n"
            f"    );\n"
        )

    def uninit(
        self,
        operation: "FusedOperation",
        config: "GlobalConfig",
        fused_compute: "FusedCompute",
    ) -> str:
        unp_a_src_format = (
            f"ckernel::to_underlying(DataFormat::{operation.src_a.data_format})"
        )

        if config.architecture == ChipArchitecture.WORMHOLE:
            return f"_llk_math_reduce_uninit_({unp_a_src_format});\n"

        return ""

    def __str__(self) -> str:
        return f"ReduceFpu({self.operation}, {self.pool})"


class DatacopyFpu(Fpu):
    def get_headers(self) -> List[str]:
        return [
            "llk_math_common.h",
            "llk_math_eltwise_unary_datacopy.h",
        ]

    def supported_unpackers(self) -> List["Unpacker"]:
        from .fused_unpacker import UnpackerA, UnpackerTilizeA

        return [UnpackerA, UnpackerTilizeA]

    def golden(
        self,
        tensor_a: torch.Tensor,
        tensor_b: torch.Tensor,
        operation: "FusedOperation",
        config: "GlobalConfig",
        compute_unit: "FusedCompute",
    ) -> torch.Tensor:
        if compute_unit.broadcast_type != BroadcastType.None_:
            source_tensor = tensor_b
        else:
            source_tensor = tensor_a

        golden_generator = get_golden_generator(DataCopyGolden)
        golden_tensor = golden_generator(
            source_tensor,
            operation.output.data_format,
            num_faces=operation.num_faces,
            input_dimensions=operation.src_a.dimensions,
            face_r_dim=operation.face_r_dim,
        )

        return golden_tensor

    def init(
        self,
        operation: "FusedOperation",
        config: "GlobalConfig",
        compute_unit: "FusedCompute",
    ) -> str:
        stage = operation.stage_id
        dest_acc = config.dest_acc.value
        tilize_en = "true" if operation.bh_tilize.value else "false"
        broadcast_type = f"BroadcastType::{compute_unit.broadcast_type.value}"
        data_copy_type = f"DataCopyType::{compute_unit.data_copy_type.name}"
        num_faces = operation.num_faces
        is_int_fpu_en = dest_acc

        code = f"    // Operation {stage}: Datacopy FPU\n"
        if config.architecture == ChipArchitecture.BLACKHOLE:
            code += (
                f"    _llk_math_eltwise_unary_datacopy_init_<{data_copy_type}, {dest_acc}, {broadcast_type}, {tilize_en}, {is_int_fpu_en}>(\n"
                f"        {num_faces}, math_format{stage}\n"
                f"    );\n"
            )
        elif config.architecture == ChipArchitecture.WORMHOLE:
            code += (
                f"    _llk_math_eltwise_unary_datacopy_init_<{data_copy_type}, {dest_acc}, {broadcast_type}, {is_int_fpu_en}>(\n"
                f"        {num_faces}, math_format{stage}\n"
                f"    );\n"
            )
        else:
            raise ValueError("Unsupported architecture for DatacopyFpu")

        return code

    def calculate(
        self,
        operation: "FusedOperation",
        config: "GlobalConfig",
        compute_unit: "FusedCompute",
        tile_idx: int,
    ) -> str:
        stage = operation.stage_id
        dest_acc = config.dest_acc.value
        broadcast_type = f"BroadcastType::{compute_unit.broadcast_type.value}"
        unpack_to_dest = "true" if operation.unpack_to_dest else "false"
        data_copy_type = f"DataCopyType::{compute_unit.data_copy_type.name}"
        num_faces = operation.num_faces

        if config.architecture == ChipArchitecture.BLACKHOLE:
            code = (
                f"    _llk_math_eltwise_unary_datacopy_<{data_copy_type}, dest_sync{stage}, {dest_acc}, {broadcast_type}, {unpack_to_dest}>(\n"
                f"        {tile_idx}, math_format{stage}, math_format{stage}, {num_faces}\n"
                f"    );\n"
            )
        elif config.architecture == ChipArchitecture.WORMHOLE:
            code = (
                f"    _llk_math_eltwise_unary_datacopy_<{data_copy_type}, dest_sync{stage}, {dest_acc}, {broadcast_type}, {unpack_to_dest}>(\n"
                f"        {tile_idx}, math_format{stage}, math_format{stage}\n"
                f"    );\n"
            )
        else:
            raise ValueError("Unsupported architecture for DatacopyFpu")

        return code


class Sfpu:
    def init(
        self,
        operation: "FusedOperation",
        config: "GlobalConfig",
        compute_unit: "FusedCompute",
    ) -> str:
        return ""

    def calculate(
        self,
        operation: "FusedOperation",
        config: "GlobalConfig",
        compute_unit: "FusedCompute",
    ) -> str:
        return ""

    def uninit(
        self,
        operation: "FusedOperation",
        config: "GlobalConfig",
        compute_unit: "FusedCompute",
    ) -> str:
        return ""

    def golden(
        self,
        tensor: torch.Tensor,
        operation: "FusedOperation",
        config: "GlobalConfig",
        compute_unit: "FusedCompute",
        batch_dims: tuple,
        batch_tile_cnt: int,
    ) -> torch.Tensor:
        return tensor

    def get_headers(self) -> List[str]:
        return []

    def __str__(self) -> str:
        return f"{self.__name__}"


class UnarySfpu(Sfpu):
    def __init__(
        self,
        operation: MathOperation,
        approx_mode: ApproximationMode = ApproximationMode.No,
        iterations: int = 8,
        dest_idx: int = 0,
        fill_const_value=5,
    ):
        if not operation in MathOperation.get_sfpu_unary_operations():
            raise ValueError(
                f"Operation {operation} is not a valid SFPU unary operation."
            )
        self.iterations = iterations
        self.approx_mode = approx_mode
        self.operation = operation
        self.dest_idx = dest_idx
        self.fill_const_value = fill_const_value

    def get_headers(self) -> List[str]:
        return [
            "ckernel_defs.h",
            "ckernel_sfpu.h",
            "llk_math_common.h",
            "llk_math_eltwise_unary_sfpu.h",
            "sfpu_operations.h",
        ]

    def golden(
        self,
        tensor: torch.Tensor,
        operation: "FusedOperation",
        config: "GlobalConfig",
        compute_unit: "FusedCompute",
        batch_dims: tuple,
        batch_tile_cnt: int,
    ) -> torch.Tensor:
        format_input = operation.src_a.data_format
        format_output = operation.output.data_format
        dest_acc = config.dest_acc

        generate_sfpu_golden = get_golden_generator(UnarySFPUGolden)

        return generate_sfpu_golden(
            self.operation,
            tensor,
            format_output,
            dest_acc,
            format_input,
            batch_dims,
            self.iterations,
            self.dest_idx,
            self.fill_const_value,
            skip_tilize=True,
        )

    def init(
        self,
        operation: "FusedOperation",
        config: "GlobalConfig",
        compute_unit: "FusedCompute",
    ) -> str:
        stage = operation.stage_id

        return (
            f"    // Operation {stage}: Unary {self.operation.cpp_enum_value} SFPU\n"
            f"    _llk_math_eltwise_unary_sfpu_init_<SfpuType::{self.operation.cpp_enum_value}>();\n"
        )

    def calculate(
        self,
        operation: "FusedOperation",
        config: "GlobalConfig",
        compute_unit: "FusedCompute",
    ) -> str:
        stage = operation.stage_id
        dest_acc = config.dest_acc.value
        op = f"SfpuType::{self.operation.cpp_enum_value}"

        return (
            f"    _llk_math_eltwise_unary_sfpu_start_<dest_sync{stage}>({self.dest_idx});\n"
            f"    test_utils::call_sfpu_operation<{self.approx_mode.value}, {dest_acc}, {self.iterations}>({op}, math_format{stage}, {self.fill_const_value});\n"
            f"    _llk_math_eltwise_unary_sfpu_done_();\n"
        )

    def __str__(self) -> str:
        return f"UnarySfpu({self.operation})"


class BinarySfpu(Sfpu):
    def __init__(
        self,
        operation: MathOperation,
        approx_mode: ApproximationMode = ApproximationMode.No,
        iterations: int = 8,
        dst_index_in0: int = 0,
        dst_index_in1: int = 1,
        dst_index_out: int = 0,
    ):
        if not operation in MathOperation.get_sfpu_binary_operations():
            raise ValueError(
                f"Operation {operation} is not a valid SFPU binary operation."
            )
        self.operation = operation
        self.approx_mode = approx_mode
        self.iterations = iterations
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
        self,
        tensor: torch.Tensor,
        operation: "FusedOperation",
        config: "GlobalConfig",
        compute_unit: "FusedCompute",
        batch_dims: tuple,
        batch_tile_cnt: int,
    ) -> torch.Tensor:
        math_format = operation.output.data_format

        generate_binary_golden = get_golden_generator(BinarySFPUGolden)
        golden_tensor = generate_binary_golden(
            self.operation,
            tensor,
            self.dst_index_in0,
            self.dst_index_in1,
            self.dst_index_out,
            self.iterations,
            batch_dims,
            math_format,
            skip_tilize=True,
        )

        return golden_tensor

    def init(
        self,
        operation: "FusedOperation",
        config: "GlobalConfig",
        compute_unit: "FusedCompute",
    ) -> str:
        stage = operation.stage_id

        return (
            f"    // Operation {stage}: Binary {self.operation.cpp_enum_value} SFPU\n"
            f"    _llk_math_eltwise_binary_sfpu_init_<SfpuType::add1>();\n"
        )

    def calculate(
        self,
        operation: "FusedOperation",
        config: "GlobalConfig",
        compute_unit: "FusedCompute",
    ) -> str:
        stage = operation.stage_id
        op = f"ckernel::BinaryOp::{self.operation.cpp_enum_value}"
        approx_mode = self.approx_mode.value
        iterations = self.iterations
        src1 = self.dst_index_in0
        src2 = self.dst_index_in1
        dst = self.dst_index_out

        if self.operation == MathOperation.SfpuAddTopRow:
            format = "0"
        else:
            format = f"math_format{stage}"

        return (
            f"    _llk_math_eltwise_binary_sfpu_start_<dest_sync{stage}>(0);\n"
            f"    test_utils::call_binary_sfpu_operation<{approx_mode}, {op}, {iterations}, {format}>({src1}, {src2}, {dst});\n"
            f"    _llk_math_eltwise_binary_sfpu_done_();\n"
        )

    def __str__(self) -> str:
        return f"BinarySfpu({self.operation})"


class SfpuWhere(Sfpu):
    def __init__(
        self,
        approx_mode: ApproximationMode = ApproximationMode.No,
        iterations: int = 8,
        dst_index_in0: int = 0,
        dst_index_in1: int = 1,
        dst_index_in2: int = 2,
        dst_index_out: int = 0,
    ):
        self.operation = MathOperation.SfpuWhere
        self.approx_mode = approx_mode
        self.iterations = iterations
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
        self,
        tensor: torch.Tensor,
        operation: "FusedOperation",
        config: "GlobalConfig",
    ) -> torch.Tensor:
        return tensor

    def init(self, operation: "FusedOperation", config: "GlobalConfig") -> str:
        return ""

    def calculate(self, operation: "FusedOperation", config: "GlobalConfig") -> str:
        return ""

    def exec(self, operation: "FusedOperation", config: "GlobalConfig") -> str:
        stage = operation.stage_id
        src1 = self.dst_index_in0
        src2 = self.dst_index_in1
        src3 = self.dst_index_in2
        dst = self.dst_index_out

        code = (
            f"    // Operation {stage}: Binary {self.operation.cpp_enum_value} SFPU\n"
            f"    _llk_math_eltwise_ternary_sfpu_init_<SfpuType::where>();\n"
            f"    ckernel::sfpu::_init_where_<{self.approx_mode.value}>();\n"
            f"    _llk_math_eltwise_ternary_sfpu_start_<dest_sync{stage}>(0);\n"
            f"    ckernel::sfpu::_calculate_where_<false, math_format{stage}, {self.iterations}>({src1}, {src2}, {src3}, {dst});\n"
            f"    _llk_math_eltwise_ternary_sfpu_done_();\n"
        )

        return code

    def __str__(self) -> str:
        return "SfpuWhere"


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

    def golden(
        self,
        tensor_a: torch.Tensor,
        tensor_b: torch.Tensor,
        operation: "FusedOperation",
        config: "GlobalConfig",
    ) -> torch.Tensor:
        batch_size = operation.batch_size
        data_format = operation.output.data_format
        tile_cnt = operation.output.tile_count
        tile_size = 1024

        result = self.fpu.golden(tensor_a, tensor_b, operation, config).flatten()

        result_tilized = tilize_block(
            result, operation.max_output_dimensions, data_format
        ).flatten()

        batch_start = 0
        for batch_start in range(0, tile_cnt, batch_size):
            batch_end = min(batch_start + batch_size, tile_cnt)
            batch_tile_cnt = batch_end - batch_start

            batch_start_elem = batch_start * tile_size
            batch_end_elem = batch_end * tile_size
            batch_tensor = result_tilized[batch_start_elem:batch_end_elem].clone()

            batch_dims = (batch_tile_cnt * 32, 32)

            for sfpu in self.sfpu:
                batch_tensor = sfpu.golden(
                    batch_tensor, operation, config, batch_dims, batch_tile_cnt
                )

            result_tilized[batch_start_elem:batch_end_elem] = batch_tensor.flatten()

        dimensions = operation.output.dimensions
        num_elements = dimensions[0] * dimensions[1]

        result = untilize_block(
            result_tilized.flatten()[:num_elements], data_format, dimensions
        ).reshape(dimensions)

        return result

    def hw_configure(self, operation: "FusedOperation", config: "GlobalConfig") -> str:
        stage = operation.stage_id
        dest_acc = config.dest_acc.value
        if stage == 0:
            code = f"_llk_math_hw_configure_<{dest_acc}>(math_format{stage}, math_format{stage});\n"
        else:
            code = f"_llk_math_reconfig_data_format_<{dest_acc}, false>(math_format{stage}, math_format{stage});\n"

        code += f"_llk_math_pack_sync_init_<dest_sync{stage}, {dest_acc}>();\n\n"

        return code

    def _sfpu_code(
        self,
        operation: "FusedOperation",
        config: "GlobalConfig",
        calc_only: bool = False,
    ) -> str:
        code = ""
        for sfpu in self.sfpu:
            code += "\n"
            if not calc_only:
                code += sfpu.init(operation, config)
            code += sfpu.calculate(operation, config)
            if not calc_only:
                code += sfpu.uninit(operation, config)
        return code

    def _wait_for_dest(self, operation: "FusedOperation") -> str:
        return f"_llk_math_wait_for_dest_available_<dest_sync{operation.stage_id}>();\n"

    def _dest_section_done(
        self, operation: "FusedOperation", config: "GlobalConfig"
    ) -> str:
        return f"_llk_math_dest_section_done_<dest_sync{operation.stage_id}, {config.dest_acc.value}>();\n"

    def _matmul_tile_loop(
        self, operation: "FusedOperation", config: "GlobalConfig", body_fn
    ) -> str:
        rt_dim = operation.rt_dim
        ct_dim = operation.ct_dim
        code = f"for (std::uint32_t mt = 0; mt < {rt_dim}; ++mt) {{\n"
        code += f"for (std::uint32_t nt = 0; nt < {ct_dim}; ++nt) {{\n"
        code += body_fn()
        code += "}\n"
        code += "}\n"
        return code

    def _batch_loop(
        self, operation: "FusedOperation", config: "GlobalConfig", body_fn
    ) -> str:
        batch_size = operation.batch_size
        tile_cnt = operation.output.tile_count

        num_full_batches = tile_cnt // batch_size
        remaining_tiles = tile_cnt % batch_size

        code = ""

        if num_full_batches > 0:
            code += f"for (std::uint32_t batch = 0; batch < {num_full_batches}; ++batch) {{\n"
            code += body_fn(batch_size)
            code += "}\n"

        if remaining_tiles > 0:
            code += "{\n"
            code += body_fn(remaining_tiles)
            code += "}\n"

        return code

    def calculate(self, operation: "FusedOperation", config: "GlobalConfig") -> str:
        batch_size = operation.batch_size
        code = self.hw_configure(operation, config)

        if isinstance(self.fpu, MatmulFpu):
            if batch_size == 1:
                code += self.fpu.init(operation, config, rt_dim=1, ct_dim=1)

                def matmul_body():
                    body = self._wait_for_dest(operation)
                    body += self.fpu.calculate(operation, config, rt_dim=1, ct_dim=1)
                    body += self._sfpu_code(operation, config)
                    body += self._dest_section_done(operation, config)
                    return body

                code += self._matmul_tile_loop(operation, config, matmul_body)
            else:
                code += self.fpu.init(operation, config)
                code += self._wait_for_dest(operation)
                code += self.fpu.calculate(operation, config)
                code += self._sfpu_code(operation, config)
                code += self._dest_section_done(operation, config)
        else:
            code += self.fpu.init(operation, config)

            def batch_body(batch_tile_cnt):
                body = self._wait_for_dest(operation)
                for tile_idx in range(batch_tile_cnt):
                    body += self.fpu.calculate(operation, config, tile_idx)
                body += self._sfpu_code(operation, config)
                body += self._dest_section_done(operation, config)
                return body

            code += self._batch_loop(operation, config, batch_body)

        code += self.fpu.uninit(operation, config)
        return code

    def perf_clear_valid(
        self, operation: "FusedOperation", config: "GlobalConfig"
    ) -> str:
        from .fused_unpacker import MatmulUnpacker, UnpackerTilizeA

        if operation.unpacker is UnpackerTilizeA:
            valid_cnt = operation.output.tile_count
            return f"    _perf_math_loop_clear_valid<true, true>({valid_cnt});\n"
        elif operation.unpacker is MatmulUnpacker:
            rt_dim = operation.rt_dim
            kt_dim = operation.kt_dim
            ct_dim = operation.ct_dim
            return f"    _perf_math_matmul_mock(1, {rt_dim}, {kt_dim}, {ct_dim});\n"
        else:
            valid_cnt = operation.output.tile_count * operation.num_faces
            return f"    _perf_math_loop_clear_valid<true, true>({valid_cnt});\n"

    def calculate_with_perf(
        self, operation: "FusedOperation", config: "GlobalConfig"
    ) -> str:
        if config.perf_run_type == PerfRunType.PACK_ISOLATE:
            return ""

        if config.perf_run_type in (
            PerfRunType.UNPACK_ISOLATE,
            PerfRunType.L1_CONGESTION,
        ):
            return self.perf_clear_valid(operation, config)

        # Use sync unless explicitly in an isolation mode
        use_sync = config.perf_run_type not in (
            PerfRunType.MATH_ISOLATE,
            PerfRunType.UNPACK_ISOLATE,
            PerfRunType.PACK_ISOLATE,
            PerfRunType.L1_CONGESTION,
        )
        batch_size = operation.batch_size

        if isinstance(self.fpu, MatmulFpu) and batch_size == 1:

            def matmul_body():
                body = self._wait_for_dest(operation) if use_sync else ""
                body += self.fpu.calculate(operation, config, rt_dim=1, ct_dim=1)
                body += self._sfpu_code(operation, config, calc_only=True)
                body += self._dest_section_done(operation, config) if use_sync else ""
                return body

            return self._matmul_tile_loop(operation, config, matmul_body)

        tile_cnt = 1 if isinstance(self.fpu, MatmulFpu) else operation.output.tile_count
        code = ""
        batch_start = 0
        for batch_start in range(0, tile_cnt, batch_size):
            if use_sync:
                code += self._wait_for_dest(operation)
            batch_end = min(batch_start + batch_size, tile_cnt)
            for tile_idx in range(batch_end - batch_start):
                code += self.fpu.calculate(operation, config, tile_idx)
            code += self._sfpu_code(operation, config, calc_only=True)
            if use_sync:
                code += self._dest_section_done(operation, config)
        return code

    def exec_perf(self, operation: "FusedOperation", config: "GlobalConfig") -> str:
        batch_size = operation.batch_size
        is_matmul_tile_by_tile = isinstance(self.fpu, MatmulFpu) and batch_size == 1

        code = "{\n"
        code += '    ZONE_SCOPED("INIT")\n'
        code += self.hw_configure(operation, config)
        if is_matmul_tile_by_tile:
            code += self.fpu.init(operation, config, rt_dim=1, ct_dim=1)
        else:
            code += self.fpu.init(operation, config)
        code += "    PROFILER_SYNC();\n"
        code += "}\n"

        code += "{\n"
        code += '    ZONE_SCOPED("TILE_LOOP")\n'
        code += f"    for(int loop = 0; loop < {config.loop_factor}; loop++)\n"
        code += "    {\n"
        code += self.calculate_with_perf(operation, config)
        code += "    }\n"
        code += "    PROFILER_SYNC();\n"
        code += "}\n"

        return code

    def exec(self, operation: "FusedOperation", config: "GlobalConfig") -> str:
        stage = operation.stage_id
        format = f"DataFormat::{operation.math_format.name}"
        code = (
            f"    // Operation {stage}: Math Setup\n"
            f"    const std::uint32_t math_format{stage} = ckernel::to_underlying({format});\n"
            f"    const DstSync dest_sync{stage} = DstSync::Sync{operation.dest_sync.name};\n"
        )

        if config.profiler_enabled:
            code += self.exec_perf(operation, config)

        else:
            code += self.calculate(operation, config)

        return code
