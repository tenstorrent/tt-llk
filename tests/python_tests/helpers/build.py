# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
import hashlib
import json
from dataclasses import dataclass
from enum import Enum
from pathlib import Path

from helpers.device import BootMode
from helpers.directories import get_tests_dir
from helpers.environment import TestEnvironment
from helpers.test_config import TestConfig, write_build_header
from helpers.utils import run_shell_command


@dataclass
class TestVariantElfs:
    brisc: Path
    unpack: Path
    math: Path
    pack: Path


@dataclass
class TestVariantBuild:
    test_name: str
    test_config: TestConfig

    directory: Path

    build_header: Path

    elfs: TestVariantElfs
    profiler_meta: list[str]
    assert_meta: list[str]


class ProfilerBuild(Enum):
    Yes = "true"
    No = "false"


def make_build(
    environment: TestEnvironment,
    test: str,
    variant: str,
    profiler_build: ProfilerBuild = ProfilerBuild.No,
):

    command = "make -j 6 --silent "

    command += f"test={test} "
    command += f"variant={variant} "

    command += "all "

    if profiler_build == ProfilerBuild.Yes:
        command += "profiler "

    run_shell_command(command, cwd=get_tests_dir())


def _generate_variant(test_config: dict) -> str:
    json_str = json.dumps(test_config, sort_keys=True)
    config_hash = hashlib.sha1(json_str.encode(), usedforsecurity=False)
    return config_hash.hexdigest()


def build_variant(
    environment: TestEnvironment,
    test_name: str,
    test_config: dict,
    profiler_build: ProfilerBuild = ProfilerBuild.No,
    boot_mode: BootMode = BootMode.BRISC,
):
    """Only builds the files required to run a test"""

    chip_arch = environment.get_chip_arch()

    variant = _generate_variant(test_config)
    variant_dir = environment.variant_dir(test_name, variant)

    write_build_header(
        variant_dir.include_dir(),
        test_config,
        profiler_build=profiler_build,
        boot_mode=boot_mode,
    )
    make_build(environment, test_name, variant, profiler_build=profiler_build)

    return TestVariantBuild(
        test_name=test_name,
        variant=variant,
        test_config=test_config,
        directory=variant_dir.path,
        build_header=variant_dir.include_dir() / "build.h",
        elfs=TestVariantElfs(
            unpack=variant_dir.elf_dir() / "unpack.elf",
            math=variant_dir.elf_dir() / "math.elf",
            pack=variant_dir.elf_dir() / "pack.elf",
        ),
        profiler_meta=variant_dir.profiler_meta_dir(),
        assert_meta=variant_dir.assert_meta_dir(),
    )
