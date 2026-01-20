# SPDX-FileCopyrightText: © 2026 Tenstorrent AI ULC
#
# SPDX-License-Identifier: Apache-2.0

from dataclasses import dataclass
from typing import List

from helpers.device import (
    collect_pipeline_results,
    write_pipeline_operands_to_l1,
)

from .chip_architecture import ChipArchitecture, get_chip_architecture
from .data_format_inference import data_formats, is_format_combination_outlier
from .format_config import DataFormat, FormatConfig
from .fused_operation import FusedOperation
from .llk_params import DestAccumulation


@dataclass
class GlobalConfig:
    test_name: str = "fused_test"
    architecture: ChipArchitecture = None
    dest_acc: DestAccumulation = DestAccumulation.No
    regenerate_cpp: bool = False


@dataclass
class FuserConfig:
    pipeline: List[FusedOperation]
    global_config: GlobalConfig

    def __post_init__(self):
        if self.global_config.architecture is None:
            self.global_config.architecture = get_chip_architecture()

        for operation in self.pipeline:
            if is_format_combination_outlier(
                operation.src_a.data_format,
                operation.output.data_format,
                self.global_config.dest_acc,
            ):
                self.global_config.dest_acc = DestAccumulation.Yes

        num_stages = len(self.pipeline)

        for i, operation in enumerate(self.pipeline):
            formats_config = data_formats(
                input_format=operation.src_a.data_format,
                output_format=operation.output.data_format,
                is_fp32_dest_acc_en=self.global_config.dest_acc,
                num_iterations=1,
                unpacking_to_dest=operation.unpack_to_dest,
                chip_arch=get_chip_architecture(),
                disable_format_inference=False,
            )[0]

            operation.unpack_a_in = formats_config.unpack_A_src
            operation.unpack_a_out = formats_config.unpack_A_dst
            operation.math_format = formats_config.math
            operation.pack_in = formats_config.pack_src
            operation.pack_out = formats_config.pack_dst
            operation.stage_id = i
            operation.num_stages = num_stages

    def run(self):
        from .fused_generator import FUSED_TESTS_DIR, FusedKernelGenerator
        from .test_config import BootMode, ProfilerBuild, TestConfig

        write_pipeline_operands_to_l1(self.pipeline)

        code_generator = FusedKernelGenerator(self)
        code_generator.write_kernel(self.global_config.test_name, self.regenerate_cpp)

        cpp_path = FUSED_TESTS_DIR / f"{self.test_name}.cpp"
        test_config = TestConfig(
            test_name=cpp_path,
            formats=FormatConfig(
                unpack_A_src=DataFormat.Float16,
                unpack_A_dst=DataFormat.Float16,
                pack_src=DataFormat.Float16,
                pack_dst=DataFormat.Float16,
                math=DataFormat.Float16,
            ),
            templates=set(),
            runtimes=set(),
            variant_stimuli=None,
            boot_mode=BootMode.DEFAULT,
            profiler_build=ProfilerBuild.No,
        )

        test_config.run_fused(self)

        collect_pipeline_results(self.pipeline)
