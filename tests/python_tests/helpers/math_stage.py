# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
#
# SPDX-License-Identifier: Apache-2.0

"""
MathStage module for defining multi-stage math operations within a single L1-to-L1 operation.

A MathStage represents a compute unit consisting of:
- An unpacker configuration (for loading data into SRC registers)
- An FPU operation
- A list of SFPU operations

Multiple MathStages can be chained together, all operating on the destination register,
with only a single pack operation at the end.

Example flow for 2 stages with batch_size=1:
    Stage 0: Unpack A,B -> FPU(A,B) -> SFPU chain -> result in DEST
    Stage 1: Unpack C (to SRCA, DEST reused as SRCB) -> FPU(C, DEST) -> SFPU chain -> result in DEST
    Pack: DEST -> L1
"""

from dataclasses import dataclass, field
from typing import TYPE_CHECKING, List, Optional

import torch

if TYPE_CHECKING:
    from .fused_operation import FusedOperation
    from .fuser_config import GlobalConfig

from .golden_generators import EltwiseBinaryGolden, get_golden_generator
from .llk_params import BroadcastType, EltwiseBinaryReuseDestType, MathOperation
from .tilize_untilize import tilize_block, untilize_block


@dataclass
class StageUnpackerConfig:
    """
    Configuration for the unpacker in a MathStage.

    For the first stage (stage_index=0), this configures normal unpacking.
    For subsequent stages, this configures unpacking with reuse_dest semantics.
    """

    # Input operand name (references operand in registry)
    input_operand: str

    # Broadcast type for this stage's unpacker
    broadcast_type: BroadcastType = BroadcastType.None_

    # For reuse dest stages: which register gets the dest data
    reuse_dest_type: EltwiseBinaryReuseDestType = EltwiseBinaryReuseDestType.NONE

    # Transpose options
    transpose_faces: bool = False
    transpose_within_face: bool = False

    def is_reuse_dest_stage(self) -> bool:
        """Returns True if this stage uses reuse dest semantics."""
        return self.reuse_dest_type != EltwiseBinaryReuseDestType.NONE

    def __str__(self) -> str:
        return (
            f"StageUnpackerConfig(operand={self.input_operand}, "
            f"broadcast={self.broadcast_type.value}, "
            f"reuse_dest={self.reuse_dest_type.name})"
        )


@dataclass(repr=False)
class MathStage:
    """
    Represents a single math stage: unpacker config + FPU operation + optional SFPU operations.

    Multiple MathStages can be chained within a single FusedOperation, all sharing
    the destination register with only one pack at the end.
    """

    # Unpacker configuration for this stage
    unpacker_config: StageUnpackerConfig

    # FPU operation (Datacopy, Elwadd, Elwmul, etc.)
    fpu: "Fpu"

    # List of SFPU operations to apply after FPU
    sfpu: List["Sfpu"] = field(default_factory=list)

    # Stage index within the operation (set by Math class)
    stage_index: int = 0

    def is_first_stage(self) -> bool:
        """Returns True if this is the first stage (normal unpacking)."""
        return self.stage_index == 0

    def is_reuse_dest_stage(self) -> bool:
        """Returns True if this stage uses reuse dest semantics."""
        return self.unpacker_config.is_reuse_dest_stage()

    def get_headers(self) -> List[str]:
        """Get all required headers for this stage (FPU + SFPU only, no unpack)."""
        headers = set()
        headers.update(self.fpu.get_headers())
        for sfpu in self.sfpu:
            headers.update(sfpu.get_headers())
        # Note: llk_unpack_A.h is NOT included here - unpack is done in unpack kernel
        return list(headers)

    def get_reuse_dest_type_cpp(self) -> str:
        """Get C++ enum value for reuse dest type."""
        return (
            f"EltwiseBinaryReuseDestType::{self.unpacker_config.reuse_dest_type.name}"
        )

    def __str__(self) -> str:
        sfpu_str = ", ".join(str(s) for s in self.sfpu) if self.sfpu else "None"
        return (
            f"MathStage(index={self.stage_index}, "
            f"fpu={self.fpu}, sfpu=[{sfpu_str}], "
            f"reuse_dest={self.unpacker_config.reuse_dest_type.name})"
        )


class ReusableEltwiseFpu:
    """
    Eltwise FPU operation that can work with reuse dest semantics.

    When reuse_dest_type is NONE: normal eltwise binary (A op B)
    When reuse_dest_type is DEST_TO_SRCA: dest loaded to SRCA, (DEST op B)
    When reuse_dest_type is DEST_TO_SRCB: dest loaded to SRCB, (A op DEST)
    """

    def __init__(
        self,
        operation: MathOperation,
        reuse_dest_type: EltwiseBinaryReuseDestType = EltwiseBinaryReuseDestType.NONE,
    ):
        if operation not in MathOperation.get_fpu_binary_operations():
            raise ValueError(
                f"Operation {operation} is not a valid FPU binary operation."
            )
        self.operation = operation
        self.reuse_dest_type = reuse_dest_type

    def get_headers(self) -> List[str]:
        return [
            "llk_math_common.h",
            "llk_math_eltwise_binary.h",
        ]

    def init(
        self,
        operation: "FusedOperation",
        config: "GlobalConfig",
        broadcast_type: BroadcastType = BroadcastType.None_,
    ) -> str:
        stage = operation.stage_id
        math_fidelity = operation.math_fidelity.value
        op = self.operation.cpp_enum_value
        num_faces = operation.num_faces
        broadcast_type_cpp = f"BroadcastType::{broadcast_type.value}"
        reuse_type = f"EltwiseBinaryReuseDestType::{self.reuse_dest_type.name}"

        return (
            f"    // Stage: Eltwise {op} FPU (reuse_dest={self.reuse_dest_type.name})\n"
            f"    _llk_math_eltwise_binary_init_<ckernel::EltwiseBinaryType::{op}, "
            f"{broadcast_type_cpp}, {math_fidelity}, {reuse_type}>({num_faces}, 0);\n"
        )

    def calculate(
        self,
        operation: "FusedOperation",
        config: "GlobalConfig",
        tile_idx: int,
        broadcast_type: BroadcastType = BroadcastType.None_,
    ) -> str:
        stage = operation.stage_id
        math_fidelity = operation.math_fidelity.value
        dest_acc = config.dest_acc.value
        op = self.operation.cpp_enum_value
        num_faces = operation.num_faces
        broadcast_type_cpp = f"BroadcastType::{broadcast_type.value}"
        reuse_type = f"EltwiseBinaryReuseDestType::{self.reuse_dest_type.name}"

        return (
            f"    _llk_math_eltwise_binary_<{op}, {broadcast_type_cpp}, dest_sync{stage},\n"
            f"        {dest_acc}, {math_fidelity}, {reuse_type}>({num_faces}, {tile_idx}, false\n"
            f"    );\n"
        )

    def golden(
        self,
        tensor_a: torch.Tensor,
        tensor_b: torch.Tensor,
        operation: "FusedOperation",
        config: "GlobalConfig",
    ) -> torch.Tensor:
        """
        Calculate golden for reusable eltwise FPU.

        When reuse_dest is NONE: normal A op B
        When reuse_dest is DEST_TO_SRCA: tensor_b is from new unpack, tensor_a is dest (result = dest op new_input)
        When reuse_dest is DEST_TO_SRCB: tensor_a is from new unpack, tensor_b is dest (result = new_input op dest)
        """
        output_format = operation.output.data_format
        math_fidelity = operation.math_fidelity

        generate_golden = get_golden_generator(EltwiseBinaryGolden)

        if self.reuse_dest_type == EltwiseBinaryReuseDestType.DEST_TO_SRCA:
            # DEST goes to SRCA, new input to SRCB
            # Result: DEST op NEW_INPUT
            result = generate_golden(
                self.operation, tensor_a, tensor_b, output_format, math_fidelity
            )
        elif self.reuse_dest_type == EltwiseBinaryReuseDestType.DEST_TO_SRCB:
            # NEW_INPUT goes to SRCA, DEST to SRCB
            # Result: NEW_INPUT op DEST
            result = generate_golden(
                self.operation, tensor_b, tensor_a, output_format, math_fidelity
            )
        else:
            # Normal: A op B
            result = generate_golden(
                self.operation, tensor_a, tensor_b, output_format, math_fidelity
            )

        return result.flatten()

    def __str__(self) -> str:
        return (
            f"ReusableEltwiseFpu({self.operation}, reuse={self.reuse_dest_type.name})"
        )


class MultiStageMath:
    """
    Manages multiple MathStages within a single FusedOperation.

    This enables chaining FPU+SFPU operations where each stage after the first
    uses reuse_dest to operate on data already in the destination register.

    Key features:
    - First stage: normal unpack A,B -> FPU -> SFPU chain
    - Subsequent stages: unpack with reuse_dest -> FPU -> SFPU chain
    - Single pack at the end
    - Proper batching: for batch_size=1, all stages execute per tile
    """

    def __init__(self, stages: List[MathStage]):
        if not stages:
            raise ValueError("MultiStageMath requires at least one stage")

        self.stages = stages

        # Set stage indices
        for i, stage in enumerate(self.stages):
            stage.stage_index = i

            # Validate: first stage should not have reuse_dest
            if i == 0 and stage.is_reuse_dest_stage():
                raise ValueError(
                    "First stage cannot use reuse_dest (no data in dest yet)"
                )

            # Validate: subsequent stages must have reuse_dest
            if i > 0 and not stage.is_reuse_dest_stage():
                raise ValueError(
                    f"Stage {i} must use reuse_dest (DEST_TO_SRCA or DEST_TO_SRCB)"
                )

    @property
    def first_stage(self) -> MathStage:
        return self.stages[0]

    @property
    def reuse_stages(self) -> List[MathStage]:
        return self.stages[1:]

    def get_headers(self) -> List[str]:
        """Get all required headers from all stages."""
        headers = set()
        for stage in self.stages:
            headers.update(stage.get_headers())
        return sorted(list(headers))

    def _generate_sfpu_code(
        self,
        stage: MathStage,
        operation: "FusedOperation",
        config: "GlobalConfig",
        calc_only: bool = False,
    ) -> str:
        """Generate SFPU code for a stage."""
        code = ""
        for sfpu in stage.sfpu:
            code += "\n"
            if not calc_only:
                code += sfpu.init(operation, config)
            code += sfpu.calculate(operation, config)
            if not calc_only:
                code += sfpu.uninit(operation, config)
        return code

    def _generate_stage_math(
        self,
        stage: MathStage,
        operation: "FusedOperation",
        config: "GlobalConfig",
        tile_idx: int,
    ) -> str:
        """Generate FPU + SFPU code for a single stage."""
        code = ""

        # Get FPU and its broadcast type
        fpu = stage.fpu
        broadcast_type = stage.unpacker_config.broadcast_type

        # FPU calculate
        if hasattr(fpu, "calculate"):
            # Check if FPU accepts broadcast_type parameter
            if isinstance(fpu, ReusableEltwiseFpu):
                code += fpu.calculate(operation, config, tile_idx, broadcast_type)
            else:
                code += fpu.calculate(operation, config, tile_idx)

        # SFPU chain
        code += self._generate_sfpu_code(stage, operation, config)

        return code

    def generate_single_tile_all_stages(
        self,
        operation: "FusedOperation",
        config: "GlobalConfig",
        tile_idx: int,
    ) -> str:
        """
        Generate code for all stages processing a single tile.
        Used when batch_size=1.

        Note: Unpack for reuse dest stages is done in the UNPACK kernel,
        not here in the MATH kernel.
        """
        code = ""

        for i, stage in enumerate(self.stages):
            code += f"\n    // === Stage {i} ===\n"
            # FPU + SFPU for this stage (no unpack here - that's in unpack kernel)
            code += self._generate_stage_math(stage, operation, config, tile_idx)

        return code

    def generate_init_all_stages(
        self,
        operation: "FusedOperation",
        config: "GlobalConfig",
    ) -> str:
        """Generate initialization code for all stages (FPU inits only, no unpack)."""
        code = ""

        for i, stage in enumerate(self.stages):
            fpu = stage.fpu
            broadcast_type = stage.unpacker_config.broadcast_type

            # FPU init for each stage
            if hasattr(fpu, "init"):
                if isinstance(fpu, ReusableEltwiseFpu):
                    code += fpu.init(operation, config, broadcast_type)
                else:
                    code += fpu.init(operation, config)

        return code

        return code

    def golden(
        self,
        tensor_a: torch.Tensor,
        tensor_b: torch.Tensor,
        operation: "FusedOperation",
        config: "GlobalConfig",
        stage_inputs: Optional[List[torch.Tensor]] = None,
    ) -> torch.Tensor:
        """
        Calculate golden result through all stages.

        Args:
            tensor_a: Input A for first stage
            tensor_b: Input B for first stage (if applicable)
            operation: The FusedOperation
            config: Global config
            stage_inputs: Optional list of input tensors for reuse_dest stages
                         (indexed by stage_index - 1 since first stage uses tensor_a/b)
        """
        batch_size = operation.batch_size
        data_format = operation.output.data_format
        tile_cnt = operation.output.tile_count
        tile_size = 1024

        # First stage FPU
        first_fpu = self.first_stage.fpu
        result = first_fpu.golden(tensor_a, tensor_b, operation, config).flatten()

        # Tilize for processing
        result_tilized = tilize_block(
            result, operation.max_output_dimensions, data_format
        ).flatten()

        # Process in batches
        for batch_start in range(0, tile_cnt, batch_size):
            batch_end = min(batch_start + batch_size, tile_cnt)
            batch_tile_cnt = batch_end - batch_start

            batch_start_elem = batch_start * tile_size
            batch_end_elem = batch_end * tile_size
            batch_tensor = result_tilized[batch_start_elem:batch_end_elem].clone()
            batch_dims = (batch_tile_cnt * 32, 32)

            # First stage SFPU chain
            for sfpu in self.first_stage.sfpu:
                batch_tensor = sfpu.golden(
                    batch_tensor, operation, config, batch_dims, batch_tile_cnt
                )

            # Process reuse dest stages
            for stage_idx, stage in enumerate(self.reuse_stages):
                # Get input for this stage
                if stage_inputs and stage_idx < len(stage_inputs):
                    stage_input = stage_inputs[stage_idx]
                    stage_input_tilized = tilize_block(
                        stage_input, operation.max_output_dimensions, data_format
                    ).flatten()[batch_start_elem:batch_end_elem]
                else:
                    # Default: use zeros or same tensor (for self-operations)
                    stage_input_tilized = batch_tensor.clone()

                # FPU with reuse dest
                fpu = stage.fpu
                if hasattr(fpu, "golden"):
                    batch_tensor = fpu.golden(
                        batch_tensor,  # dest (becomes SRCA or SRCB based on reuse type)
                        stage_input_tilized,  # new input
                        operation,
                        config,
                    )

                # SFPU chain for this stage
                for sfpu in stage.sfpu:
                    batch_tensor = sfpu.golden(
                        batch_tensor, operation, config, batch_dims, batch_tile_cnt
                    )

            result_tilized[batch_start_elem:batch_end_elem] = batch_tensor.flatten()

        # Untilize result
        dimensions = operation.output.dimensions
        num_elements = dimensions[0] * dimensions[1]

        result = untilize_block(
            result_tilized.flatten()[:num_elements], data_format, dimensions
        ).reshape(dimensions)

        return result

    # ==========================================
    # Interface methods for FusedOperation compatibility
    # ==========================================

    @property
    def fpu(self):
        """Return the first stage FPU for compatibility with existing code."""
        return self.first_stage.fpu

    @property
    def sfpu(self) -> List:
        """Return all SFPU operations from all stages."""
        all_sfpu = []
        for stage in self.stages:
            all_sfpu.extend(stage.sfpu)
        return all_sfpu

    def hw_configure(self, operation: "FusedOperation", config: "GlobalConfig") -> str:
        """Generate hardware configuration code."""
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

    def calculate(self, operation: "FusedOperation", config: "GlobalConfig") -> str:
        """Generate complete calculation code for all stages."""
        batch_size = operation.batch_size
        tile_cnt = operation.output.tile_count

        code = self.hw_configure(operation, config)

        # Initialize all stages
        code += self.generate_init_all_stages(operation, config)

        # Generate batched calculation
        num_full_batches = tile_cnt // batch_size
        remaining_tiles = tile_cnt % batch_size

        if num_full_batches > 0:
            code += (
                f"for (uint32_t batch = 0; batch < {num_full_batches}; ++batch) {{\n"
            )
            code += self._generate_batch_body(operation, config, batch_size)
            code += "}\n"

        if remaining_tiles > 0:
            code += "{\n"
            code += self._generate_batch_body(operation, config, remaining_tiles)
            code += "}\n"

        return code

    def _generate_batch_body(
        self,
        operation: "FusedOperation",
        config: "GlobalConfig",
        batch_tile_cnt: int,
    ) -> str:
        """Generate code for processing a batch of tiles through all stages."""
        code = self._wait_for_dest(operation)

        # For each tile in the batch, run all stages
        for tile_idx in range(batch_tile_cnt):
            code += self.generate_single_tile_all_stages(operation, config, tile_idx)

        code += self._dest_section_done(operation, config)
        return code

    def exec(self, operation: "FusedOperation", config: "GlobalConfig") -> str:
        """
        Generate complete math kernel code for this multi-stage operation.
        This is the main entry point called by FusedOperation.do_math().

        Note: All unpack operations (including reuse dest) are handled by the
        UNPACK kernel, not here. This only generates FPU + SFPU code.
        """
        stage = operation.stage_id
        format_str = f"DataFormat::{operation.math_format.name}"

        code = (
            f"    // Operation {stage}: Multi-Stage Math Setup\n"
            f"    const uint32_t math_format{stage} = static_cast<std::underlying_type_t<DataFormat>>({format_str});\n"
            f"    const DstSync dest_sync{stage} = DstSync::Sync{operation.dest_sync.name};\n"
        )

        if config.profiler_enabled:
            code += self._exec_perf(operation, config)
        else:
            code += self.calculate(operation, config)

        return code

    def _exec_perf(self, operation: "FusedOperation", config: "GlobalConfig") -> str:
        """Generate performance profiling code."""
        # For now, use simplified version
        code = "{\n"
        code += '    ZONE_SCOPED("INIT")\n'
        code += self.hw_configure(operation, config)
        code += self.generate_init_all_stages(operation, config)
        code += "    PROFILER_SYNC();\n"
        code += "}\n"

        code += "{\n"
        code += '    ZONE_SCOPED("TILE_LOOP")\n'
        code += f"    for(int loop = 0; loop < {config.loop_factor}; loop++)\n"
        code += "    {\n"
        code += self.calculate(operation, config)
        code += "    }\n"
        code += "    PROFILER_SYNC();\n"
        code += "}\n"

        return code

    def __str__(self) -> str:
        stages_str = "\n    ".join(str(s) for s in self.stages)
        return f"MultiStageMath(\n    {stages_str}\n)"
