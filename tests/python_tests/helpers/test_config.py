# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0

import shutil
from concurrent.futures import ThreadPoolExecutor
from dataclasses import dataclass
from enum import Enum
from pathlib import Path
from typing import ClassVar

from ttexalens.tt_exalens_lib import parse_elf

from .chip_architecture import ChipArchitecture, get_chip_architecture
from .data_format_inference import data_formats, is_format_combination_outlier
from .device import (
    BootMode,
    pull_coverage_stream_from_tensix,
    run_elf_files,
    wait_for_tensix_operations_finished,
)
from .format_config import DataFormat, FormatConfig
from .llk_params import (
    DestAccumulation,
)
from .stimuli_config import StimuliConfig
from .target_config import TestTargetConfig


class ProfilerBuild(Enum):
    Yes = "true"
    No = "false"


class CoverageBuild(Enum):
    Yes = "true"
    No = "false"


from .test_variant_parameters import RuntimeParameter, TemplateParameter
from .utils import create_directories, run_shell_command


@dataclass
class TestConfig:

    # === STATIC VARIABLES ===

    CURRENT_CONFIG: ClassVar["TestConfig"] = None

    # Architecture Selection
    ARCH_NON_COMPUTE: ClassVar[str]
    ARCH_COMPUTE: ClassVar[str]
    ARCH_DEFINE: ClassVar[str]
    ARCH_LLK_ROOT: ClassVar[str]
    ARCH: ClassVar[str]

    # Artefact directories
    BUILD_DIR: ClassVar[str]
    SHARED_DIR: ClassVar[str]
    SHARED_OBJ_DIR: ClassVar[str]
    SHARED_ELF_DIR: ClassVar[str]
    COVERAGE_INFO_DIR: ClassVar[str]

    # Sources directories
    LLK_ROOT: ClassVar[Path]
    TESTS_WORKING_DIR: ClassVar[Path]
    TOOL_PATH: ClassVar[Path]
    HEADER_DIR: ClassVar[Path]

    HELPERS: ClassVar[Path]
    RISCV_SOURCES: ClassVar[Path]
    LINKER_SCRIPTS: ClassVar[Path]

    # Toolchain paths
    GXX: ClassVar[str]
    OBJDUMP: ClassVar[str]
    OBJCOPY: ClassVar[str]
    GCOV: ClassVar[str]
    GCOV_TOOL: ClassVar[str]

    # Compilation options
    OPTIONS_ALL: ClassVar[str] = None
    OPTIONS_LINK: ClassVar[str] = None
    INITIAL_OPTIONS_COMPILE: ClassVar[str] = None
    INCLUDES: ClassVar[str] = None

    OPTIONS_COMPILE: ClassVar[str] = None
    MEMORY_LAYOUT_LD_SCRIPT: ClassVar[str] = None
    NON_COVERAGE_OPTIONS_COMPILE: ClassVar[str] = None

    SHARED_ARTEFACTS_AVAILABLE: ClassVar[bool] = False
    KERNEL_COMPONENTS: ClassVar[list[str]] = ["unpack", "math", "pack"]

    #

    @staticmethod
    def setup_arch():
        chip_arch = get_chip_architecture()
        if chip_arch == ChipArchitecture.WORMHOLE:
            TestConfig.ARCH_NON_COMPUTE = "-mcpu=tt-wh"
            TestConfig.ARCH_COMPUTE = "-mcpu=tt-wh-tensix"
            TestConfig.ARCH_DEFINE = "-DARCH_WORMHOLE"
            TestConfig.ARCH_LLK_ROOT = "tt_llk_wormhole_b0"
            TestConfig.ARCH = "wormhole"
        elif chip_arch == ChipArchitecture.BLACKHOLE:
            TestConfig.ARCH_NON_COMPUTE = "-mcpu=tt-bh"
            TestConfig.ARCH_COMPUTE = "-mcpu=tt-bh-tensix"
            TestConfig.ARCH_DEFINE = "-DARCH_BLACKHOLE"
            TestConfig.ARCH_LLK_ROOT = "tt_llk_blackhole"
            TestConfig.ARCH = "blackhole"
        elif chip_arch == ChipArchitecture.QUASAR:
            # until there is official support for quasar in SFPI fallback to BH
            TestConfig.ARCH_NON_COMPUTE = "-mcpu=tt-bh"
            TestConfig.ARCH_COMPUTE = "-mcpu=tt-bh-tensix"
            TestConfig.ARCH_DEFINE = "-DARCH_BLACKHOLE"
            TestConfig.ARCH_LLK_ROOT = "tt_llk_quasar"
            TestConfig.ARCH = "quasar"
        else:
            raise ValueError(
                "Must provide CHIP_ARCH environment variable (wormhole / blackhole / quasar)"
            )

    @staticmethod
    def setup_paths(sources_path: Path, build_path: Path):
        TestConfig.LLK_ROOT = sources_path
        TestConfig.TESTS_WORKING_DIR = TestConfig.LLK_ROOT / "tests"
        TestConfig.TOOL_PATH = TestConfig.LLK_ROOT / "tests/sfpi/compiler/bin"
        TestConfig.HEADER_DIR = (
            TestConfig.TESTS_WORKING_DIR / f"hw_specific/{TestConfig.ARCH}/inc"
        )

        TestConfig.HELPERS = TestConfig.TESTS_WORKING_DIR / "helpers"
        TestConfig.RISCV_SOURCES = TestConfig.TESTS_WORKING_DIR / "helpers/src"
        TestConfig.LINKER_SCRIPTS = TestConfig.TESTS_WORKING_DIR / "helpers/ld"

        # Toolchain paths
        TestConfig.GXX = str((TestConfig.TOOL_PATH / "riscv-tt-elf-g++").absolute())
        TestConfig.OBJDUMP = str(
            (TestConfig.TOOL_PATH / "riscv-tt-elf-objdump").absolute()
        )
        TestConfig.OBJCOPY = str(
            (TestConfig.TOOL_PATH / "riscv-tt-elf-objcopy").absolute()
        )
        TestConfig.GCOV = str((TestConfig.TOOL_PATH / "riscv-tt-elf-gcov").absolute())
        TestConfig.GCOV_TOOL = str(
            (TestConfig.TOOL_PATH / "riscv-tt-elf-gcov-tool").absolute()
        )

        shutil.rmtree(build_path, ignore_errors=True)  # Always have a fresh build

        TestConfig.BUILD_DIR = build_path
        TestConfig.SHARED_DIR = TestConfig.BUILD_DIR / "shared"
        TestConfig.SHARED_OBJ_DIR = TestConfig.SHARED_DIR / "obj"
        TestConfig.SHARED_ELF_DIR = TestConfig.SHARED_DIR / "elf"
        TestConfig.COVERAGE_INFO_DIR = TestConfig.BUILD_DIR / "coverage_info"

        create_directories(
            [
                TestConfig.BUILD_DIR,
                TestConfig.SHARED_DIR,
                TestConfig.SHARED_OBJ_DIR,
                TestConfig.SHARED_ELF_DIR,
                TestConfig.COVERAGE_INFO_DIR,
            ]
        )

    @staticmethod
    def setup_compilation_options():
        TestConfig.OPTIONS_ALL = f"-g -O3 -std=c++17 -ffast-math"
        TestConfig.OPTIONS_LINK = "-nodefaultlibs -fexceptions -Wl,-z,max-page-size=16 -Wl,-z,common-page-size=16 -nostartfiles -Wl,--trace"
        TestConfig.INITIAL_OPTIONS_COMPILE = f"-fno-use-cxa-atexit -Wall -fno-exceptions -fno-rtti -Wunused-parameter -Wfloat-equal -Wpointer-arith -Wnull-dereference -Wredundant-decls -Wuninitialized -nostdlib -fno-builtin -Wmaybe-uninitialized -DTENSIX_FIRMWARE -DENV_LLK_INFRA {TestConfig.ARCH_DEFINE}"
        TestConfig.INCLUDES = f"-I../{TestConfig.ARCH_LLK_ROOT}/llk_lib -I../{TestConfig.ARCH_LLK_ROOT}/common/inc -I../{TestConfig.ARCH_LLK_ROOT}/common/inc/sfpu -Isfpi/compiler/lib/gcc/riscv-tt-elf/*/include -I{TestConfig.HEADER_DIR} -Ifirmware/riscv/common -Isfpi/include -Ihelpers/include"

    @staticmethod
    def setup_build(sources_path: Path, build_path: Path):
        TestConfig.setup_arch()
        TestConfig.setup_paths(sources_path, build_path)
        TestConfig.setup_compilation_options()

    # === Instance fields and methods ===
    def __init__(
        self,
        # Required arguments
        test_name: str,
        formats: FormatConfig,
        templates: set[TemplateParameter],
        runtimes: set[RuntimeParameter],
        variant_stimuli: StimuliConfig,
        # Arguments with valid default values
        boot_mode: BootMode = BootMode.DEFAULT,
        profiler_build: ProfilerBuild = ProfilerBuild.No,
        L1_to_L1_iterations: int = 1,
        unpack_to_dest: bool = False,
        disable_format_inference: bool = False,
        dest_acc: DestAccumulation = DestAccumulation.No,
        tiny_tiles: bool = False,
    ):

        if TestTargetConfig().with_coverage:
            self.coverage_build = CoverageBuild.Yes
        else:
            self.coverage_build = CoverageBuild.No

        self.test_name = test_name
        self.formats = formats
        self.templates = templates
        self.runtimes = runtimes
        self.variant_stimuli = variant_stimuli

        self.boot_mode = boot_mode
        self.profiler_build = profiler_build
        self.L1_to_L1_iterations = L1_to_L1_iterations
        self.unpack_to_dest = unpack_to_dest
        self.disable_format_inference = disable_format_inference
        self.dest_acc = dest_acc
        self.tiny_tiles = tiny_tiles

        if (
            self.coverage_build == CoverageBuild.Yes
            and self.profiler_build == ProfilerBuild.Yes
        ):
            raise ValueError(
                "You can't build profiler and coverage build at the same time, profiling tests will fail"
            )

        self.variant_id = "finnish_hashing_of_test_config_object"

    def resolve_compile_options(self) -> tuple[str, str, str]:

        if (
            TestConfig.OPTIONS_COMPILE is not None
            and TestConfig.MEMORY_LAYOUT_LD_SCRIPT is not None
            and TestConfig.NON_COVERAGE_OPTIONS_COMPILE is not None
        ):
            return (
                TestConfig.OPTIONS_COMPILE,
                MEMORY_LAYOUT_LD_SCRIPT,
                NON_COVERAGE_OPTIONS_COMPILE,
            )

        MEMORY_LAYOUT_LD_SCRIPT = (
            f"{TestConfig.LINKER_SCRIPTS}/memory.{TestConfig.ARCH}.ld"
        )
        OPTIONS_COMPILE = f"{TestConfig.INCLUDES} {TestConfig.INITIAL_OPTIONS_COMPILE} "

        if self.boot_mode == BootMode.TRISC:
            OPTIONS_COMPILE += "-DLLK_BOOT_MODE_TRISC "
        else:
            OPTIONS_COMPILE += "-DLLK_BOOT_MODE_BRISC "

        NON_COVERAGE_OPTIONS_COMPILE = OPTIONS_COMPILE

        if self.coverage_build == CoverageBuild.Yes:
            NON_COVERAGE_OPTIONS_COMPILE = OPTIONS_COMPILE
            OPTIONS_COMPILE += (
                "-fprofile-arcs -ftest-coverage -fprofile-info-section -DCOVERAGE "
            )
            MEMORY_LAYOUT_LD_SCRIPT = (
                f"{TestConfig.LINKER_SCRIPTS}/memory.{TestConfig.ARCH}.debug.ld"
            )

        if self.profiler_build == ProfilerBuild.Yes:
            OPTIONS_COMPILE += "-DLLK_PROFILER "

        return (OPTIONS_COMPILE, MEMORY_LAYOUT_LD_SCRIPT, NON_COVERAGE_OPTIONS_COMPILE)

    def select_kernel_source_and_flag(self) -> tuple[Path, str]:
        """
        Mimics the Makefile logic:
        - Returns the first existing kernel source file for the testname.
        - Returns the TRISC flag (empty if source is from quasar).
        """

        candidates = [
            TestConfig.TESTS_WORKING_DIR
            / "sources"
            / "ai_gen"
            / f"{self.test_name}.cpp",
            TestConfig.TESTS_WORKING_DIR
            / "sources"
            / "quasar"
            / f"{self.test_name}.cpp",
            TestConfig.TESTS_WORKING_DIR / "sources" / f"{self.test_name}.cpp",
        ]
        for candidate in candidates:
            if candidate.exists():
                kernel_source_file = candidate
                break
        else:
            raise FileNotFoundError(f"No kernel source found for {self.test_name}")

        # Set TRISC flag: only empty if source is from quasar

        kernel_trisc_flag = "-DCOMPILE_FOR_TRISC="
        if kernel_source_file.parts[-3] == "quasar":
            kernel_trisc_flag = ""

        return kernel_source_file, kernel_trisc_flag

    def build_shared_artefacts(self):
        if TestConfig.SHARED_ARTEFACTS_AVAILABLE:
            return

        local_options_compile, local_memory_layout_ld, local_non_coverage = (
            self.resolve_compile_options()
        )

        # tmu-crt0.o : tmu-crt0.S
        run_shell_command(
            f"""{TestConfig.GXX} {TestConfig.ARCH_NON_COMPUTE} {TestConfig.OPTIONS_ALL} {local_options_compile} -c -o {TestConfig.SHARED_OBJ_DIR / "tmu-crt0.o"} {TestConfig.HELPERS / "tmu-crt0.S"}""",
            TestConfig.TESTS_WORKING_DIR,
        )

        # brisc.o : brisc.cpp
        run_shell_command(
            f"""{TestConfig.GXX} {TestConfig.ARCH_NON_COMPUTE} {TestConfig.OPTIONS_ALL} {local_non_coverage} -c -o {TestConfig.SHARED_OBJ_DIR / "brisc.o"} {TestConfig.RISCV_SOURCES / "brisc.cpp"}""",
            TestConfig.TESTS_WORKING_DIR,
        )

        COVERAGE_DEPS = ""
        if self.coverage_build == CoverageBuild.Yes:
            COVERAGE_DEPS = f"{TestConfig.SHARED_OBJ_DIR}/coverage.o -lgcov"
            # coverage.o : coverage.cpp
            run_shell_command(
                f"""{TestConfig.GXX} {TestConfig.ARCH_NON_COMPUTE} {TestConfig.OPTIONS_ALL} {local_non_coverage} -fno-strict-aliasing -c -o {TestConfig.SHARED_OBJ_DIR / "coverage.o"} {TestConfig.RISCV_SOURCES / "coverage.cpp"}""",
                TestConfig.TESTS_WORKING_DIR,
            )

        # Kernel mains
        _, kernel_trisc_flag = self.select_kernel_source_and_flag()

        def build_kernel_part_main(name: str):
            run_shell_command(  # main_%.o
                f"""{TestConfig.GXX} {TestConfig.ARCH_COMPUTE} {TestConfig.OPTIONS_ALL} {local_options_compile} {kernel_trisc_flag} -DLLK_TRISC_{name.upper()} -c -o {TestConfig.SHARED_OBJ_DIR / f"main_{name}.o"} {TestConfig.RISCV_SOURCES / "trisc.cpp"}""",
                TestConfig.TESTS_WORKING_DIR,
            )

        with ThreadPoolExecutor(max_workers=3) as executor:
            futures = [
                executor.submit(build_kernel_part_main, name)
                for name in TestConfig.KERNEL_COMPONENTS
            ]
            for fut in futures:
                fut.result()

        # brisc.elf : tmu-crt0.o brisc.o coverage.o
        run_shell_command(
            f"""{TestConfig.GXX} {TestConfig.ARCH_NON_COMPUTE} {TestConfig.OPTIONS_ALL} {TestConfig.OPTIONS_LINK} {TestConfig.SHARED_OBJ_DIR / "tmu-crt0.o"} {TestConfig.SHARED_OBJ_DIR / "brisc.o"} {COVERAGE_DEPS} -T{local_memory_layout_ld} -T{TestConfig.LINKER_SCRIPTS / "brisc.ld"} -T{TestConfig.LINKER_SCRIPTS / "sections.ld"} -o {TestConfig.SHARED_ELF_DIR / "brisc.elf"}""",
            TestConfig.TESTS_WORKING_DIR,
        )

        TestConfig.SHARED_ARTEFACTS_AVAILABLE = True

    def invfer_data_formats(self) -> list[str]:
        header_content: list[str] = [
            "// Data formats inferred by Python inference model"
        ]

        # Profiler Tests don't pass formats to the test config, so we need to set them here
        if "profiler" in self.test_name:
            format = DataFormat.Float16
            self.formats = FormatConfig(format, format, format, format, format)

        # Dest accumulation
        header_content.append(
            f"constexpr bool is_fp32_dest_acc_en = {self.dest_acc.value};"
        )

        # Check if this is an outlier format combination that requires dest_acc to be enabled
        # Automatically enable dest_acc for outlier combinations
        if is_format_combination_outlier(
            self.formats.input_format, self.formats.output_format, self.dest_acc
        ):
            self.dest_acc = DestAccumulation.Yes

        # Fused Test L1 to L1 : Input of first run is used as input for the second run ...
        # Not fusing: single L1-to-L1 iteration, so we retrieve one format configuration
        # L1_to_L1_iterations is the number of times we perform llk operations from L1 input tensor to L1 output tensor
        # If L1_to_L1_ITERATIONS is 1, we take input tensor from L1 -> unpack -> math -> pack -> L1
        # If L1_to_L1_ITERATIONS is greater than 1, we perform multiple iterations of unpack -> math -> pack, by taking results tensor in L1 to be input tensor of next iteration

        formats_config = data_formats(
            input_format=self.formats.input_format,
            output_format=self.formats.output_format,
            is_fp32_dest_acc_en=self.dest_acc,
            num_iterations=self.L1_to_L1_iterations,
            unpacking_to_dest=self.unpack_to_dest == "true",
            chip_arch=get_chip_architecture(),
            disable_format_inference=self.disable_format_inference,
        )

        self.unpack_to_dest = self.unpack_to_dest = str(self.unpack_to_dest).lower()
        header_content.append(f"constexpr bool unpack_to_dest = {self.unpack_to_dest};")
        header_content.append(
            f"constexpr bool UNPACKING_TO_DEST = {self.unpack_to_dest};"
        )

        # Check if we need to generate multiple format configurations

        if self.L1_to_L1_iterations > 1:
            # Generate format data as arrays that params.h can use to construct FormatConfig objects
            header_content.extend(
                [
                    "// Format data for multiple L1-to-L1 iterations",
                    f"constexpr std::uint32_t L1_to_L1_ITERATIONS = {self.L1_to_L1_iterations};",
                    "#define FUSED_MULTIPLE_RUNS true",
                ]
            )

            # Create array of format configurations for multiple L1-to-L1 iterations
            unpack_a_in_values = [
                f"static_cast<std::underlying_type_t<DataFormat>>(DataFormat::{fmt.unpack_A_src.name})"
                for fmt in formats_config
            ]
            unpack_a_out_values = [
                f"static_cast<std::underlying_type_t<DataFormat>>(DataFormat::{fmt.unpack_A_dst.name})"
                for fmt in formats_config
            ]
            math_values = [
                f"static_cast<std::underlying_type_t<DataFormat>>(DataFormat::{fmt.math.name})"
                for fmt in formats_config
            ]
            pack_in_values = [
                f"static_cast<std::underlying_type_t<DataFormat>>(DataFormat::{fmt.pack_src.name})"
                for fmt in formats_config
            ]
            pack_out_values = [
                f"static_cast<std::underlying_type_t<DataFormat>>(DataFormat::{fmt.pack_dst.name})"
                for fmt in formats_config
            ]

            header_content.extend(
                [
                    f"constexpr std::array<std::underlying_type_t<DataFormat>, L1_to_L1_ITERATIONS> UNPACK_A_IN_LIST = {{{', '.join(unpack_a_in_values)}}};",
                    f"constexpr std::array<std::underlying_type_t<DataFormat>, L1_to_L1_ITERATIONS> UNPACK_A_OUT_LIST = {{{', '.join(unpack_a_out_values)}}};",
                    f"constexpr std::array<std::underlying_type_t<DataFormat>, L1_to_L1_ITERATIONS> MATH_FORMAT_LIST = {{{', '.join(math_values)}}};",
                    f"constexpr std::array<std::underlying_type_t<DataFormat>, L1_to_L1_ITERATIONS> PACK_IN_LIST = {{{', '.join(pack_in_values)}}};",
                    f"constexpr std::array<std::underlying_type_t<DataFormat>, L1_to_L1_ITERATIONS> PACK_OUT_LIST = {{{', '.join(pack_out_values)}}};",
                ]
            )

        else:
            # Single iteration - use simple format inference
            # Generate format data as individual constants for single iteration
            formats_config = formats_config[0]
            header_content.extend(
                [
                    "// Format data for single L1-to-L1 iteration",
                    f"constexpr auto UNPACK_A_IN = static_cast<std::underlying_type_t<DataFormat>>(DataFormat::{formats_config.unpack_A_src.name});",
                    f"constexpr auto UNPACK_A_OUT = static_cast<std::underlying_type_t<DataFormat>>(DataFormat::{formats_config.unpack_A_dst.name});",
                    f"constexpr auto MATH_FORMAT = static_cast<std::underlying_type_t<DataFormat>>(DataFormat::{formats_config.math.name});",
                    f"constexpr auto PACK_IN = static_cast<std::underlying_type_t<DataFormat>>(DataFormat::{formats_config.pack_src.name});",
                    f"constexpr auto PACK_OUT = static_cast<std::underlying_type_t<DataFormat>>(DataFormat::{formats_config.pack_dst.name});",
                ]
            )

        header_content.append("")

        return header_content

    def generate_build_header(self) -> str:
        header_content: list[str] = [
            "// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC",
            "//",
            "// SPDX-License-Identifier: Apache-2.0",
            "// AUTO-GENERATED CONFIGURATION HEADER. DO NOT EDIT MANUALLY!",
            "",
            "#pragma once",
            "",
            "#include <array>",
            "#include <type_traits>",
            "",
            '#include "operand.h"',
            '#include "llk_defs.h"',
            '#include "llk_sfpu_types.h"',
            # Conditionally include perf.h based on architecture
            (
                '#include "perf.h"'
                if get_chip_architecture() != ChipArchitecture.QUASAR
                else ""
            ),
            '#include "tensix_types.h"',
            "",
            "// Basic configuration",
            "constexpr std::uint32_t TILE_SIZE_CNT = 0x1000;",
        ]

        header_content.extend(
            self.variant_stimuli.generate_stimuli_header_addresses(self.formats)
        )

        TILE_SIZES = {
            DataFormat.Bfp8_b: 68,
            DataFormat.Float32: 256,
        }

        pack_size = TILE_SIZES.get(self.formats.output_format, 128)
        unpack_size_a = TILE_SIZES.get(self.formats.input_format, 128)
        unpack_size_b = TILE_SIZES.get(self.formats.input_format, 128)

        # Tiny tile flag, used to handle dimension
        # if self.tiny_tiles:
        #     pack_size = (pack_size // self.num_faces) * (self.in0_tile_r_dim // face_r_dim)
        #     unpack_size_a = (unpack_size_a // num_faces_A) * (in0_tile_r_dim // face_r_dim)

        # All are RT, used in only few tests, but there wasn't any mechanism not to include them
        header_content.extend(
            [
                f"constexpr std::uint32_t TILE_SIZE_PACK = {pack_size};",
                f"constexpr std::uint32_t TILE_SIZE_UNPACK_A = {unpack_size_a};",
                f"constexpr std::uint32_t TILE_SIZE_UNPACK_B = {unpack_size_b};",
            ]
        )

        for parameter in self.templates:
            header_content.append(parameter.covert_to_cpp())

        for parameter in self.runtimes:
            header_content.append(parameter.covert_to_cpp())

        header_content.extend(self.invfer_data_formats())
        return "\n".join(header_content)

    def build_elfs(self):
        VARIANT_DIR = TestConfig.BUILD_DIR / self.test_name / self.variant_id
        VARIANT_OBJ_DIR = VARIANT_DIR / "obj"
        VARIANT_ELF_DIR = VARIANT_DIR / "elf"

        create_directories([VARIANT_DIR, VARIANT_OBJ_DIR, VARIANT_ELF_DIR])

        self.build_shared_artefacts()

        local_options_compile, local_memory_layout_ld, _ = (
            self.resolve_compile_options()
        )

        header_content = self.generate_build_header()
        with open(VARIANT_DIR / "build.h", "w") as f:
            f.write(header_content)

        kernel_source_file, kernel_trisc_flag = self.select_kernel_source_and_flag()

        SFPI_DEPS = ""
        COVERAGE_DEPS = ""
        if self.coverage_build == CoverageBuild.Yes:
            SFPI_DEPS = "-lgcov"
            COVERAGE_DEPS = TestConfig.SHARED_OBJ_DIR / "coverage.o"

        def build_kernel_part(name: str):
            run_shell_command(  # kernel_%.o
                f"""{TestConfig.GXX} {TestConfig.ARCH_COMPUTE} {TestConfig.OPTIONS_ALL} -I{VARIANT_DIR} {local_options_compile} {kernel_trisc_flag} -DLLK_TRISC_{name.upper()} -c -o {VARIANT_OBJ_DIR / f"kernel_{name}.o"} {TestConfig.RISCV_SOURCES / kernel_source_file}""",
                TestConfig.TESTS_WORKING_DIR,
            )

            run_shell_command(  # %.elf : tmu-crt0.o main_%.o kernel_%.o coverage.o
                f"""{TestConfig.GXX} {TestConfig.ARCH_COMPUTE} {TestConfig.OPTIONS_ALL} {TestConfig.OPTIONS_LINK} {TestConfig.SHARED_OBJ_DIR / "tmu-crt0.o"} {TestConfig.SHARED_OBJ_DIR / f"main_{name}.o"} {VARIANT_OBJ_DIR / f"kernel_{name}.o"} {COVERAGE_DEPS} {SFPI_DEPS} -T{local_memory_layout_ld} -T{TestConfig.LINKER_SCRIPTS / f"{name}.ld"} -T{TestConfig.LINKER_SCRIPTS / "sections.ld"} -o {VARIANT_ELF_DIR / f"{name}.elf"}""",
                TestConfig.TESTS_WORKING_DIR,
            )

        with ThreadPoolExecutor(max_workers=3) as executor:
            futures = [
                executor.submit(build_kernel_part, name)
                for name in TestConfig.KERNEL_COMPONENTS
            ]
            for fut in futures:
                fut.result()

        if self.profiler_build == ProfilerBuild.Yes:
            # Extract profiler metadata

            PROFILER_VARIANT_META_DIR = Path(
                TestConfig.BUILD_DIR
                / "profiler_meta"
                / self.test_name
                / self.variant_id
            )

            PROFILER_VARIANT_META_DIR.mkdir(exist_ok=True, parents=True)

            for component in TestConfig.KERNEL_COMPONENTS:
                elf_path = VARIANT_ELF_DIR / f"{component}.elf"
                meta_bin_path = PROFILER_VARIANT_META_DIR / f"{component}.meta.bin"
                run_shell_command(
                    f"{TestConfig.OBJCOPY} -O binary -j .profiler_meta {elf_path} {meta_bin_path}",
                    TestConfig.TESTS_WORKING_DIR,
                )

    def merge_coverage_streams_into_info(self):
        VARIANT_DIR = TestConfig.BUILD_DIR / self.test_name / self.variant_id
        VARIANT_OBJ_DIR = VARIANT_DIR / "obj"

        for component in TestConfig.KERNEL_COMPONENTS:
            stream_path = VARIANT_DIR / f"{component}.raw.stream"
            run_shell_command(
                f"{TestConfig.GCOV_TOOL} merge-stream {stream_path}",
                TestConfig.TESTS_WORKING_DIR,
            )

        lcov_output = (
            TestConfig.COVERAGE_INFO_DIR / f"{self.test_name}_{self.variant_id}.info"
        )
        command = (
            f"lcov --gcov-tool {TestConfig.GCOV} --capture "
            f"--directory {VARIANT_OBJ_DIR} "
            f"--directory {TestConfig.SHARED_OBJ_DIR} "
            f"--output-file {lcov_output} "
            "--rc lcov_branch_coverage=1"
        )
        run_shell_command(command, TestConfig.TESTS_WORKING_DIR)

    def generate_info_file_for_run(self, location="0,0"):
        VARIANT_DIR = TestConfig.BUILD_DIR / self.test_name / self.variant_id / "elf"
        for trisc_name in TestConfig.KERNEL_COMPONENTS:
            pull_coverage_stream_from_tensix(
                location,
                parse_elf(VARIANT_DIR / f"{trisc_name}.elf"),
                VARIANT_DIR / f"{trisc_name}.raw.stream",
            )

    def run(self, location):

        self.variant_stimuli.write()
        self.build_elfs()

        elfs = run_elf_files(
            self.test_name, self.variant_id, self.boot_mode, location=location
        )
        wait_for_tensix_operations_finished(elfs, location)

        if self.coverage_build == CoverageBuild.Yes:
            self.generate_info_file_for_run(location)
            self.merge_coverage_streams_into_info()

        # shutil.rmtree(TestConfig.BUILD_DIR / self.test_name / self.variant_id)

        return self.variant_stimuli.collect_results(location)
