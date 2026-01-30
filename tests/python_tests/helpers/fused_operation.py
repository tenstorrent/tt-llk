# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
#
# SPDX-License-Identifier: Apache-2.0

from dataclasses import dataclass
from typing import Tuple, Type

import torch

from .chip_architecture import ChipArchitecture, get_chip_architecture
from .format_config import DataFormat
from .fused_math import Math, MatmulFpu
from .fused_operand import Operand, OperandMapping
from .fused_packer import Packer
from .fused_unpacker import Unpacker, UnpackerTilizeA
from .llk_params import (
    BroadcastType,
    DataCopyType,
    DestSync,
    MathFidelity,
    StochasticRounding,
    Tilize,
    Transpose,
    format_tile_sizes,
)
from .matmul_sweep import validate_tile_dimensions

# Import for type checking MultiStageMath
try:
    from .math_stage import MultiStageMath
except ImportError:
    MultiStageMath = None


@dataclass
class FusedOperation:
    unpacker: Type[Unpacker]
    math: Math
    packer: Type[Packer]
    operand_mapping: OperandMapping
    stage_id: int = 0
    num_stages: int = 1
    math_fidelity: MathFidelity = MathFidelity.HiFi4
    unpack_to_dest: bool = False
    unpack_transpose_faces: Transpose = Transpose.No
    unpack_transpose_within_face: Transpose = Transpose.No
    math_transpose_faces: Transpose = Transpose.No
    throttle: int = 0
    stochastic_rnd: StochasticRounding = StochasticRounding.No
    data_copy_type: DataCopyType = DataCopyType.A2D
    tiny_tiles: bool = False
    partial_face_A: bool = False
    partial_face_B: bool = False
    partial_face: bool = False
    num_faces: int = 4
    num_faces_A: int = 4
    num_faces_B: int = 4
    in0_tile_r_dim: int = 32
    in0_tile_c_dim: int = 32
    in1_tile_r_dim: int = 32
    in1_tile_c_dim: int = 32
    face_r_dim: int = 16
    face_c_dim: int = 16
    dest_sync: DestSync = DestSync.Half
    dst_index: int = 0
    srca_reuse_count: int = 4
    output_pack_dims: Tuple[int, int] = None
    batch_size: int = 0
    broadcast_type: BroadcastType = BroadcastType.None_

    def __post_init__(self):
        mapping = self.operand_mapping
        registry = mapping.operand_registry

        src_a = registry.get(mapping.src_a)
        src_b = registry.get(mapping.src_b)
        output = registry.get(mapping.output)

        TILE_SIZES = {
            DataFormat.Bfp8_b: 68,
            DataFormat.Float32: 256,
        }

        pack_size = TILE_SIZES.get(output.data_format, 128)
        unpack_size_a = TILE_SIZES.get(src_a.data_format, 128)
        unpack_size_b = TILE_SIZES.get(src_b.data_format, 128)

        if self.tiny_tiles:
            pack_size = (pack_size // self.num_faces) * (
                self.in0_tile_r_dim // self.face_r_dim
            )
            unpack_size_a = (unpack_size_a // self.num_faces_A) * (
                self.in0_tile_r_dim // self.face_r_dim
            )

        self.tile_size_pack = pack_size
        self.tile_size_unpack_a = unpack_size_a
        self.tile_size_unpack_b = unpack_size_b

        self.tile_size = 16 * 16 * self.num_faces

        self.buffer_A_tile_size = format_tile_sizes[self.src_a.data_format]
        self.buffer_B_tile_size = format_tile_sizes[self.src_b.data_format]
        self.buffer_Res_tile_size = format_tile_sizes[self.output.data_format]

        num_rows = 32
        num_cols = 32

        validate_tile_dimensions(self.src_a.dimensions[0], num_rows)
        validate_tile_dimensions(self.src_a.dimensions[1], num_cols)
        validate_tile_dimensions(self.src_b.dimensions[0], num_rows)
        validate_tile_dimensions(self.src_b.dimensions[1], num_cols)

        self.rt_dim = self.output.dimensions[0] // num_rows
        self.ct_dim = self.output.dimensions[1] // num_cols
        self.kt_dim = self.src_a.dimensions[1] // num_cols

        self.dest_tiles_h = self.output.dimensions[0] // num_rows
        self.dest_tiles_w = self.output.dimensions[1] // num_cols

        if (
            get_chip_architecture() == ChipArchitecture.BLACKHOLE
            and self.unpacker is UnpackerTilizeA
            and self.src_a.data_format != DataFormat.Bfp8_b
        ):
            self.bh_tilize = Tilize.Yes
        else:
            self.bh_tilize = Tilize.No

        if self.batch_size <= 0 or self.batch_size > self.output.tile_count:
            self.batch_size = self.output.tile_count

        # Check if math.fpu exists (for both Math and MultiStageMath)
        if hasattr(self.math, "fpu") and isinstance(self.math.fpu, MatmulFpu):
            tile_count = self.output.tile_count
            if self.batch_size != 1 and self.batch_size != tile_count:
                self.batch_size = tile_count

        if self.broadcast_type != BroadcastType.None_:
            self.data_copy_type = DataCopyType.B2D

    def is_multi_stage(self) -> bool:
        """Check if this operation uses MultiStageMath."""
        return MultiStageMath is not None and isinstance(self.math, MultiStageMath)

    @property
    def src_a(self) -> Operand:
        mapping = self.operand_mapping
        return mapping.operand_registry.get(mapping.src_a)

    @property
    def src_b(self) -> Operand:
        mapping = self.operand_mapping
        return mapping.operand_registry.get(mapping.src_b)

    @property
    def output(self) -> Operand:
        mapping = self.operand_mapping
        return mapping.operand_registry.get(mapping.output)

    @property
    def max_output_dimensions(self) -> Operand:
        mapping = self.operand_mapping
        return mapping.resolve_output_dimensions(mapping.operand_registry)

    def unpack(self, config) -> str:
        unpacker_instance = self.unpacker()
        return unpacker_instance.exec(self, config)

    def do_math(self, config) -> str:
        return self.math.exec(self, config)

    def pack(self, config) -> str:
        packer_instance = self.packer()
        return packer_instance.exec(self, config)

    def golden(self, config) -> torch.Tensor:
        # calculate l1 golden
        src_a_dims = self.src_a.dimensions
        src_b_dims = self.src_b.dimensions

        tensor_a = self.src_a.raw_data.view(src_a_dims)
        tensor_b = self.src_b.raw_data.view(src_b_dims)

        # For MultiStageMath, we need to collect stage inputs
        stage_inputs = None
        if self.is_multi_stage():
            stage_inputs = self._collect_stage_inputs_raw()

        tensor_a, tensor_b = self.unpacker().golden(tensor_a, tensor_b, self, config)

        if self.is_multi_stage():
            l1_golden_tensor = self.math.golden(
                tensor_a, tensor_b, self, config, stage_inputs
            )
        else:
            l1_golden_tensor = self.math.golden(tensor_a, tensor_b, self, config)

        l1_golden_tensor = self.packer().golden(l1_golden_tensor, self, config)

        self.output.l1_golden = l1_golden_tensor.flatten()

        # calculate master golden
        golden_tensor_a = self.src_a.master_golden.view(src_a_dims)
        golden_tensor_b = self.src_b.master_golden.view(src_b_dims)

        # For MultiStageMath, collect master golden stage inputs
        master_stage_inputs = None
        if self.is_multi_stage():
            master_stage_inputs = self._collect_stage_inputs_master()

        golden_tensor_a, golden_tensor_b = self.unpacker().golden(
            golden_tensor_a, golden_tensor_b, self, config
        )

        if self.is_multi_stage():
            master_golden_tensor = self.math.golden(
                golden_tensor_a, golden_tensor_b, self, config, master_stage_inputs
            )
        else:
            master_golden_tensor = self.math.golden(
                golden_tensor_a, golden_tensor_b, self, config
            )

        master_golden_tensor = self.packer().golden(master_golden_tensor, self, config)

        self.output._master_golden = master_golden_tensor.flatten()

        return master_golden_tensor

    def _collect_stage_inputs_raw(self):
        """Collect raw data inputs for each reuse_dest stage."""
        if not self.is_multi_stage():
            return None

        stage_inputs = []
        registry = self.operand_mapping.operand_registry

        for stage in self.math.reuse_stages:
            operand_name = stage.unpacker_config.input_operand
            if operand_name and registry.exists(operand_name):
                operand = registry.get(operand_name)
                stage_inputs.append(operand.raw_data.view(operand.dimensions))
            else:
                # Default to src_b if no operand specified
                stage_inputs.append(self.src_b.raw_data.view(self.src_b.dimensions))

        return stage_inputs

    def _collect_stage_inputs_master(self):
        """Collect master golden inputs for each reuse_dest stage."""
        if not self.is_multi_stage():
            return None

        stage_inputs = []
        registry = self.operand_mapping.operand_registry

        for stage in self.math.reuse_stages:
            operand_name = stage.unpacker_config.input_operand
            if operand_name and registry.exists(operand_name):
                operand = registry.get(operand_name)
                stage_inputs.append(operand.master_golden.view(operand.dimensions))
            else:
                # Default to src_b if no operand specified
                stage_inputs.append(
                    self.src_b.master_golden.view(self.src_b.dimensions)
                )

        return stage_inputs

    def __str__(self):
        # Handle both Math and MultiStageMath
        if self.is_multi_stage():
            math_str = f"MultiStageMath with {len(self.math.stages)} stages"
            fpu_str = ", ".join(str(s.fpu) for s in self.math.stages)
            sfpu_str = str([op.__str__() for op in self.math.sfpu])
        else:
            math_str = str(self.math)
            fpu_str = str(self.math.fpu)
            sfpu_str = str([op.__str__() for op in self.math.sfpu])

        return (
            f"\n{'='*60}\n"
            f"Operation {self.stage_id}\n"
            f"{'='*60}\n"
            f"  Unpacker: {self.unpacker.__name__}\n"
            f"  Math: {math_str}\n"
            f"    Fpu Math Op: {fpu_str}\n"
            f"    Sfpu Math Ops: {sfpu_str}\n"
            f"  Packer: {self.packer.__name__}\n"
            f"  Src_A: {self.src_a}\n"
            f"  Src_B: {self.src_b}\n"
            f"  Output: {self.output}\n"
            f"  Math Fidelity: {self.math_fidelity}\n"
            f"  Unpack Transpose Faces: {self.unpack_transpose_faces}\n"
            f"  Unpack Transpose Within Faces: {self.unpack_transpose_within_face}\n"
            f"  Batch Size: {self.batch_size}\n"
            f"  Broadcast Type: {self.broadcast_type}\n"
        )
