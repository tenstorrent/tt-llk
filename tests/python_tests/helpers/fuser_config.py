# SPDX-FileCopyrightText: © 2026 Tenstorrent AI ULC
#
# SPDX-License-Identifier: Apache-2.0

from dataclasses import dataclass
from typing import List

from helpers.device import (
    collect_pipeline_results,
    write_pipeline_operands_to_l1,
)
from ttexalens.tt_exalens_lib import read_words_from_device

from .chip_architecture import ChipArchitecture, get_chip_architecture
from .data_format_inference import data_formats, is_format_combination_outlier
from .device import wait_for_tensix_operations_finished
from .format_config import DataFormat, FormatConfig
from .fused_operation import FusedOperation
from .llk_params import DestAccumulation, PerfRunType
from .perf import PerfReport
from .profiler import ProfilerConfig, ProfilerData
from .test_config import BootMode, ProfilerBuild, TestConfig, TestMode


@dataclass
class GlobalConfig:
    test_name: str = "fused_test"
    architecture: ChipArchitecture = None
    dest_acc: DestAccumulation = DestAccumulation.No
    regenerate_cpp: bool = False
    perf_run_type: PerfRunType = None
    loop_factor: int = 16


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

    def run(self, worker_id="master", location="0,0", run_count=2):
        from .fused_generator import FUSED_TESTS_DIR, FusedKernelGenerator
        from .fused_golden import FusedGolden

        write_pipeline_operands_to_l1(self.pipeline)

        cpp_path = FUSED_TESTS_DIR / f"{self.global_config.test_name}.cpp"
        code_generator = FusedKernelGenerator(self)
        code_generator.write_kernel(cpp_path, self.global_config.regenerate_cpp)

        profiler_build = (
            ProfilerBuild.Yes
            if self.global_config.perf_run_type is not None
            else ProfilerBuild.No
        )

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
            profiler_build=profiler_build,
            skip_build_header=True,
        )

        test_config.generate_variant_hash()
        test_config.build_elfs()

        if self.global_config.perf_run_type is not None:
            perf_report = PerfReport()
            runs = []

            for _ in range(run_count):
                elfs = test_config.run_elf_files(location)
                wait_for_tensix_operations_finished(elfs, location)

                meta = ProfilerConfig._get_meta(
                    test_config.test_name, test_config.variant_id
                )
                buffer_data = [
                    read_words_from_device(
                        addr=addr,
                        word_count=ProfilerConfig.BUFFER_LENGTH,
                        location=location,
                    )
                    for addr in ProfilerConfig.THREAD_BUFFER
                ]
                runs.append(ProfilerConfig._parse_buffers(buffer_data, meta))

            get_stats = ProfilerConfig.STATS_FUNCTION[self.global_config.perf_run_type]
            results = get_stats(ProfilerData.concat(runs))
            results["test_name"] = self.global_config.test_name
            results["loop_factor"] = self.global_config.loop_factor
            perf_report.append(results)

            if TestConfig.MODE != TestMode.PRODUCE:
                csv_prefix = f"{self.global_config.test_name}_fused_test"
                perf_report.dump_csv(f"{csv_prefix}.{worker_id}.csv")
                perf_report.post_process()
                perf_report.dump_csv(f"{csv_prefix}.{worker_id}.post.csv")
        else:
            elfs = test_config.run_elf_files(location)
            wait_for_tensix_operations_finished(elfs, location)
            collect_pipeline_results(self.pipeline)
            golden = FusedGolden()
            assert golden.check_pipeline(self)
