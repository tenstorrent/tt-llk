# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
from enum import Enum

import pytest

from ..build import MakeTestVariantBuilder, TestVariantBuilder
from ..build.build import ProfilerBuild, TestVariantBuild
from ..chip_architecture import ChipArchitecture, get_chip_architecture
from ..device import BootMode


def _test_requires_profiler(request) -> bool:
    """
    The test requires a profiler to work under one of the following conditions:
    - Test is marked with @pytest.mark.profiler
    - Test is marked with @pytest.mark.perf
    - Test name matches "test_perf_*"
    """

    if request.node.name.startswith("test_perf_"):
        return True
    markers = [marker.name for marker in request.node.own_markers]

    if "profiler" in markers:
        return True

    if "perf" in markers:
        return True

    return False


@pytest.fixture(scope="function")
def test_env(request) -> "TestEnvironment":
    """
    Fixture that sets up the environment for test execution

    Automatically infers:
    - Chip Architecture
    - Profiler build requirements
    - Builder

    Returns:
        TestEnvironment: Configured test environment instance
    """
    chip_arch = get_chip_architecture()

    # Check if the test has the profiler marker
    profiler_build = ProfilerBuild.No

    if _test_requires_profiler(request):
        profiler_build = ProfilerBuild.Yes

    # todo: add manual override for boot mode
    boot_mode = BootMode.BRISC

    # for now, only one builder is supported
    builder = MakeTestVariantBuilder()

    return TestEnvironment(
        chip_arch=chip_arch,
        profiler_build=profiler_build,
        boot_mode=boot_mode,
        builder=builder,
    )


class ProfilerBuild(Enum):
    Yes = "true"
    No = "false"


class BootMode(Enum):
    BRISC = "brisc"
    TRISC = "trisc"
    EXALENS = "exalens"


class TestEnvironment:

    def __init__(
        self,
        chip_arch: ChipArchitecture,
        profiler_build: ProfilerBuild,
        boot_mode: BootMode,
        builder: TestVariantBuilder,
    ):
        self._chip_arch = chip_arch
        self._profiler_build = profiler_build
        self._boot_mode = boot_mode
        self._builder = builder

    def get_chip_arch(self) -> ChipArchitecture:
        return self._chip_arch

    def get_profiler_build(self) -> ProfilerBuild:
        return self._profiler_build

    def get_boot_mode(self) -> BootMode:
        return self._boot_mode

    def build(self, test: str, test_config: dict) -> TestVariantBuild:
        return self._builder.build(self, test=test, test_config=test_config)
