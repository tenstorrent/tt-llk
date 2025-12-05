# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
#
# SPDX-License-Identifier: Apache-2.0

import os
import shutil
from concurrent.futures import ThreadPoolExecutor
from enum import Enum
from pathlib import Path

from .build_h_gen import generate_build_header
from .chip_architecture import ChipArchitecture, get_chip_architecture
from .device import BootMode
from .utils import create_directories, run_shell_command


class ProfilerBuild(Enum):
    Yes = "true"
    No = "false"


class CoverageBuild(Enum):
    Yes = "true"
    No = "false"


# Architecture Selection
ARCH_NON_COMPUTE = None
ARCH_COMPUTE = None
ARCH_DEFINE = None
ARCH_LLK_ROOT = None
ARCH = None


def get_arch_flags() -> tuple[str, str, str, str, str]:
    global ARCH_NON_COMPUTE, ARCH_COMPUTE, ARCH_DEFINE, ARCH_LLK_ROOT, ARCH
    if (
        ARCH_COMPUTE is not None
        and ARCH_NON_COMPUTE is not None
        and ARCH_DEFINE is not None
    ):
        return (ARCH_NON_COMPUTE, ARCH_COMPUTE, ARCH_DEFINE, ARCH_LLK_ROOT, ARCH)

    chip_arch = get_chip_architecture()

    if chip_arch == ChipArchitecture.WORMHOLE:
        ARCH_NON_COMPUTE = "-mcpu=tt-wh"
        ARCH_COMPUTE = "-mcpu=tt-wh-tensix"
        ARCH_DEFINE = "-DARCH_WORMHOLE"
        ARCH_LLK_ROOT = "tt_llk_wormhole_b0"
        ARCH = "wormhole"
    elif chip_arch == ChipArchitecture.BLACKHOLE:
        ARCH_NON_COMPUTE = "-mcpu=tt-bh"
        ARCH_COMPUTE = "-mcpu=tt-bh-tensix"
        ARCH_DEFINE = "-DARCH_BLACKHOLE"
        ARCH_LLK_ROOT = "tt_llk_blackhole"
        ARCH = "blackhole"
    elif chip_arch == ChipArchitecture.QUASAR:
        # until there is official support for quasar in SFPI fallback to BH
        ARCH_NON_COMPUTE = "-mcpu=tt-bh"
        ARCH_COMPUTE = "-mcpu=tt-bh-tensix"
        ARCH_DEFINE = "-DARCH_BLACKHOLE"
        ARCH_LLK_ROOT = "tt_llk_quasar"
        ARCH = "quasar"
    else:
        raise ValueError(
            "Must provide CHIP_ARCH environment variable (wormhole / blackhole / quasar)"
        )

    return (ARCH_NON_COMPUTE, ARCH_COMPUTE, ARCH_DEFINE, ARCH_LLK_ROOT, ARCH)


get_arch_flags()

# --- Directories ---

# Source directories

LLK_ROOT = Path(os.environ.get("LLK_HOME"))
TESTS_WORKING_DIR = LLK_ROOT / "tests"
TOOL_PATH = LLK_ROOT / "tests/sfpi/compiler/bin"
HEADER_DIR = TESTS_WORKING_DIR / f"hw_specific/{ARCH}/inc"

HELPERS = TESTS_WORKING_DIR / "helpers"
RISCV_SOURCES = TESTS_WORKING_DIR / "helpers/src"
LINKER_SCRIPTS = TESTS_WORKING_DIR / "helpers/ld"

# Toolchain paths
GXX = str((TOOL_PATH / "riscv-tt-elf-g++").absolute())
OBJDUMP = str((TOOL_PATH / "riscv-tt-elf-objdump").absolute())
OBJCOPY = str((TOOL_PATH / "riscv-tt-elf-objcopy").absolute())
GCOV = str((TOOL_PATH / "riscv-tt-elf-gcov").absolute())
GCOV_TOOL = str((TOOL_PATH / "riscv-tt-elf-gcov-tool").absolute())

# Compiler and Linker Flags
OPTIONS_ALL = f"-g -O3 -std=c++17 -ffast-math"
OPTIONS_LINK = "-nodefaultlibs -fexceptions -Wl,-z,max-page-size=16 -Wl,-z,common-page-size=16 -nostartfiles -Wl,--trace"

INITIAL_OPTIONS_COMPILE = f"-fno-use-cxa-atexit -Wall -fno-exceptions -fno-rtti -Wunused-parameter -Wfloat-equal -Wpointer-arith -Wnull-dereference -Wredundant-decls -Wuninitialized -nostdlib -fno-builtin -Wmaybe-uninitialized -DTENSIX_FIRMWARE -DENV_LLK_INFRA {ARCH_DEFINE}"

INCLUDES = f"-I../{ARCH_LLK_ROOT}/llk_lib -I../{ARCH_LLK_ROOT}/common/inc -I../{ARCH_LLK_ROOT}/common/inc/sfpu -Isfpi/compiler/lib/gcc/riscv-tt-elf/*/include -I{HEADER_DIR} -Ifirmware/riscv/common -Isfpi/include -Ihelpers/include"

KERNEL_COMPONENTS = ["unpack", "math", "pack"]

# Artefact directories
BUILD_DIR = None
SHARED_DIR = None
SHARED_OBJ_DIR = None
SHARED_ELF_DIR = None
COVERAGE_INFO_DIR = None


def setup_build_dir(path: Path):
    global BUILD_DIR, SHARED_DIR, SHARED_OBJ_DIR, SHARED_ELF_DIR, COVERAGE_INFO_DIR

    shutil.rmtree(path, ignore_errors=True)  # Always have a fresh build

    BUILD_DIR = path
    SHARED_DIR = BUILD_DIR / "shared"
    SHARED_OBJ_DIR = SHARED_DIR / "obj"
    SHARED_ELF_DIR = SHARED_DIR / "elf"
    COVERAGE_INFO_DIR = BUILD_DIR / "coverage_info"

    create_directories(
        [BUILD_DIR, SHARED_DIR, SHARED_OBJ_DIR, SHARED_ELF_DIR, COVERAGE_INFO_DIR]
    )


OPTIONS_COMPILE = None
MEMORY_LAYOUT_LD_SCRIPT = None
NON_COVERAGE_OPTIONS_COMPILE = None


def resolve_compile_options(
    boot_mode: BootMode, profiler_build: ProfilerBuild, coverage_build: CoverageBuild
) -> tuple[str, str, str]:
    global OPTIONS_COMPILE, MEMORY_LAYOUT_LD_SCRIPT, NON_COVERAGE_OPTIONS_COMPILE

    if (
        OPTIONS_COMPILE is not None
        and MEMORY_LAYOUT_LD_SCRIPT is not None
        and NON_COVERAGE_OPTIONS_COMPILE is not None
    ):
        return (OPTIONS_COMPILE, MEMORY_LAYOUT_LD_SCRIPT, NON_COVERAGE_OPTIONS_COMPILE)

    MEMORY_LAYOUT_LD_SCRIPT = f"{LINKER_SCRIPTS}/memory.{ARCH}.debug.ld"
    OPTIONS_COMPILE = f"{INCLUDES} {INITIAL_OPTIONS_COMPILE} "

    if boot_mode == BootMode.TRISC:
        OPTIONS_COMPILE += "-DLLK_BOOT_MODE_TRISC "
    else:
        OPTIONS_COMPILE += "-DLLK_BOOT_MODE_BRISC "

    NON_COVERAGE_OPTIONS_COMPILE = OPTIONS_COMPILE

    if coverage_build == CoverageBuild.Yes and profiler_build == ProfilerBuild.Yes:
        raise ValueError("You can't build profiler and coverage build at the same time")

    if coverage_build == CoverageBuild.Yes:
        NON_COVERAGE_OPTIONS_COMPILE = OPTIONS_COMPILE
        OPTIONS_COMPILE += (
            "-fprofile-arcs -ftest-coverage -fprofile-info-section -DCOVERAGE "
        )
        MEMORY_LAYOUT_LD_SCRIPT = f"{LINKER_SCRIPTS}/memory.{ARCH}.debug.ld"

    if profiler_build == ProfilerBuild.Yes:
        OPTIONS_COMPILE += "-DLLK_PROFILER "

    return (OPTIONS_COMPILE, MEMORY_LAYOUT_LD_SCRIPT, NON_COVERAGE_OPTIONS_COMPILE)


SHARED_ARTEFACTS_AVAILABLE = False


def build_shared_artefacts(
    test_name: str,
    boot_mode: BootMode,
    profiler_build: ProfilerBuild,
    coverage_build: CoverageBuild,
) -> bool:
    global SHARED_ARTEFACTS_AVAILABLE, BUILD_DIR, SHARED_DIR, SHARED_OBJ_DIR, SHARED_ELF_DIR, COVERAGE_INFO_DIR
    # Build shared files - brisc.o, coverage.o, C-runtime, brisc.elf and main_%.o files
    if SHARED_ARTEFACTS_AVAILABLE:
        return True

    local_options_compile, local_memory_layout_ld, local_non_coverage = (
        resolve_compile_options(boot_mode, profiler_build, coverage_build)
    )

    # tmu-crt0.o : tmu-crt0.S
    run_shell_command(
        f"""{GXX} {ARCH_NON_COMPUTE} {OPTIONS_ALL} {local_options_compile} -c -o {SHARED_OBJ_DIR / "tmu-crt0.o"} {HELPERS / "tmu-crt0.S"}""",
        TESTS_WORKING_DIR,
    )

    # brisc.o : brisc.cpp
    run_shell_command(
        f"""{GXX} {ARCH_NON_COMPUTE} {OPTIONS_ALL} {local_non_coverage} -c -o {SHARED_OBJ_DIR / "brisc.o"} {RISCV_SOURCES / "brisc.cpp"}""",
        TESTS_WORKING_DIR,
    )

    COVERAGE_DEPS = ""
    if coverage_build == CoverageBuild.Yes:
        COVERAGE_DEPS = f"{SHARED_OBJ_DIR}/coverage.o -lgcov"
        # coverage.o : coverage.cpp
        run_shell_command(
            f"""{GXX} {ARCH_NON_COMPUTE} {OPTIONS_ALL} {local_non_coverage} -fno-strict-aliasing -c -o {SHARED_OBJ_DIR / "coverage.o"} {RISCV_SOURCES / "coverage.cpp"}""",
            TESTS_WORKING_DIR,
        )

    # Kernel mains
    _, kernel_trisc_flag = select_kernel_source_and_flag(test_name)

    def build_kernel_part_main(name: str):
        run_shell_command(  # main_%.o
            f"""{GXX} {ARCH_COMPUTE} {OPTIONS_ALL} {local_options_compile} {kernel_trisc_flag} -DLLK_TRISC_{name.upper()} -c -o {SHARED_OBJ_DIR / f"main_{name}.o"} {RISCV_SOURCES / "trisc.cpp"}""",
            TESTS_WORKING_DIR,
        )

    with ThreadPoolExecutor(max_workers=3) as executor:
        futures = [
            executor.submit(build_kernel_part_main, name) for name in KERNEL_COMPONENTS
        ]
        for fut in futures:
            fut.result()

    # brisc.elf : tmu-crt0.o brisc.o coverage.o
    run_shell_command(
        f"""{GXX} {ARCH_NON_COMPUTE} {OPTIONS_ALL} {OPTIONS_LINK} {SHARED_OBJ_DIR / "tmu-crt0.o"} {SHARED_OBJ_DIR / "brisc.o"} {COVERAGE_DEPS} -T{local_memory_layout_ld} -T{LINKER_SCRIPTS / "brisc.ld"} -T{LINKER_SCRIPTS / "sections.ld"} -o {SHARED_ELF_DIR / "brisc.elf"}""",
        TESTS_WORKING_DIR,
    )

    SHARED_ARTEFACTS_AVAILABLE = True


def select_kernel_source_and_flag(test_name: str) -> tuple[Path, str]:
    """
    Mimics the Makefile logic:
    - Returns the first existing kernel source file for the testname.
    - Returns the TRISC flag (empty if source is from quasar).
    """

    candidates = [
        TESTS_WORKING_DIR / "sources" / "ai_gen" / f"{test_name}.cpp",
        TESTS_WORKING_DIR / "sources" / "quasar" / f"{test_name}.cpp",
        TESTS_WORKING_DIR / "sources" / f"{test_name}.cpp",
    ]
    for candidate in candidates:
        if candidate.exists():
            kernel_source_file = candidate
            break
    else:
        raise FileNotFoundError(f"No kernel source found for {test_name}")

    # Set TRISC flag: only empty if source is from quasar

    kernel_trisc_flag = "-DCOMPILE_FOR_TRISC="
    if kernel_source_file.parts[-3] == "quasar":
        kernel_trisc_flag = ""

    return kernel_source_file, kernel_trisc_flag


def build_test_variant(
    test_config: dict,
    variant_id: str,
    boot_mode: BootMode,
    profiler_build: ProfilerBuild = ProfilerBuild.No,
    coverage_build: CoverageBuild = CoverageBuild.No,
):
    global BUILD_DIR, SHARED_DIR, SHARED_OBJ_DIR, SHARED_ELF_DIR, COVERAGE_INFO_DIR
    VARIANT_DIR = BUILD_DIR / test_config["testname"] / variant_id
    VARIANT_OBJ_DIR = VARIANT_DIR / "obj"
    VARIANT_ELF_DIR = VARIANT_DIR / "elf"

    create_directories([VARIANT_DIR, VARIANT_OBJ_DIR, VARIANT_ELF_DIR])

    build_shared_artefacts(
        test_config["testname"], boot_mode, profiler_build, coverage_build
    )

    local_options_compile, local_memory_layout_ld, _ = resolve_compile_options(
        boot_mode, profiler_build, coverage_build
    )

    header_content = generate_build_header(test_config)
    with open(VARIANT_DIR / "build.h", "w") as f:
        f.write(header_content)

    kernel_source_file, kernel_trisc_flag = select_kernel_source_and_flag(
        test_config["testname"]
    )

    SFPI_DEPS = ""
    COVERAGE_DEPS = ""
    if coverage_build == CoverageBuild.Yes:
        SFPI_DEPS = "-lgcov"
        COVERAGE_DEPS = SHARED_OBJ_DIR / "coverage.o"

    def build_kernel_part(name: str):
        run_shell_command(  # kernel_%.o
            f"""{GXX} {ARCH_COMPUTE} {OPTIONS_ALL} -I{VARIANT_DIR} {local_options_compile} {kernel_trisc_flag} -DLLK_TRISC_{name.upper()} -c -o {VARIANT_OBJ_DIR / f"kernel_{name}.o"} {RISCV_SOURCES / kernel_source_file}""",
            TESTS_WORKING_DIR,
        )

        run_shell_command(  # %.elf : tmu-crt0.o main_%.o kernel_%.o coverage.o
            f"""{GXX} {ARCH_COMPUTE} {OPTIONS_ALL} {OPTIONS_LINK} {SHARED_OBJ_DIR / "tmu-crt0.o"} {SHARED_OBJ_DIR / f"main_{name}.o"} {VARIANT_OBJ_DIR / f"kernel_{name}.o"} {COVERAGE_DEPS} {SFPI_DEPS} -T{local_memory_layout_ld} -T{LINKER_SCRIPTS / f"{name}.ld"} -T{LINKER_SCRIPTS / "sections.ld"} -o {VARIANT_ELF_DIR / f"{name}.elf"}""",
            TESTS_WORKING_DIR,
        )

    with ThreadPoolExecutor(max_workers=3) as executor:
        futures = [
            executor.submit(build_kernel_part, name) for name in KERNEL_COMPONENTS
        ]
        for fut in futures:
            fut.result()

    if profiler_build == ProfilerBuild.Yes:
        # Extract profiler metadata

        PROFILER_VARIANT_META_DIR = Path(
            BUILD_DIR / "profiler_meta" / test_config["testname"] / variant_id
        )

        PROFILER_VARIANT_META_DIR.mkdir(exist_ok=True, parents=True)

        for component in KERNEL_COMPONENTS:
            elf_path = VARIANT_ELF_DIR / f"{component}.elf"
            meta_bin_path = PROFILER_VARIANT_META_DIR / f"{component}.meta.bin"
            run_shell_command(
                f"{OBJCOPY} -O binary -j .profiler_meta {elf_path} {meta_bin_path}",
                TESTS_WORKING_DIR,
            )


def merge_coverage_streams_into_into(test_config: dict, variant_id: str):

    VARIANT_DIR = BUILD_DIR / test_config["testname"] / variant_id
    VARIANT_OBJ_DIR = VARIANT_DIR / "obj"

    for component in KERNEL_COMPONENTS:
        stream_path = VARIANT_DIR / f"{component}.raw.stream"
        run_shell_command(f"{GCOV_TOOL} merge-stream {stream_path}", TESTS_WORKING_DIR)

    lcov_output = COVERAGE_INFO_DIR / f"{test_config['testname']}_{variant_id}.info"
    command = (
        f"lcov --gcov-tool {GCOV} --capture "
        f"--directory {VARIANT_OBJ_DIR} "
        f"--directory {SHARED_OBJ_DIR} "
        f"--output-file {lcov_output} "
        "--rc lcov_branch_coverage=1"
    )
    run_shell_command(command, TESTS_WORKING_DIR)
