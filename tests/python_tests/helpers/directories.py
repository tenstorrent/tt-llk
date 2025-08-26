# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
import os
from pathlib import Path

from helpers.chip_architecture import ChipArchitecture

# This file must track the makefile structure


def _mkdirs(path: Path) -> Path:
    path.mkdir(parents=True, exist_ok=True)
    return path


class VariantDir:

    def __init__(self, path: Path):
        self.path = path

    def __str__(self):
        return str(self.path)

    def __truediv__(self, subdir: str) -> Path:
        return _mkdirs(self.path / subdir)

    def obj_dir(self) -> Path:
        return self / "obj"

    def elf_dir(self) -> Path:
        return self / "elf"

    def bin_dir(self) -> Path:
        return self / "bin"

    def include_dir(self) -> Path:
        return self / "include"

    def profiler_meta_dir(self) -> Path:
        return self / "profiler_meta"

    def assert_meta_dir(self) -> Path:
        return self / "assert_meta"


class TestDir:
    def __init__(self, path: Path):
        self.path = path

    def __str__(self):
        return str(self.path)

    def variant_dir(self, variant: str) -> VariantDir:
        return VariantDir(_mkdirs(self.path / variant))


def get_root_dir():
    root = os.environ.get("LLK_HOME")
    if root is None:
        raise AssertionError(
            "Environment variable LLK_HOME must be set to the root of the project"
        )

    return Path(root)


def get_tests_dir():
    # must exist already
    return get_root_dir() / "tests"


def get_build_dir():
    return _mkdirs(get_tests_dir() / "build")


def get_arch_dir(chip_arch: ChipArchitecture):
    return _mkdirs(get_build_dir() / chip_arch.value)


def get_test_dir(chip_arch: ChipArchitecture, test_name: str):
    return TestDir(_mkdirs(get_arch_dir(chip_arch) / test_name))
