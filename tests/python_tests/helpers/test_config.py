# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0

import shutil
from hashlib import sha256

from .device import (
    BootMode,
    generate_info_file_for_run,
    run_elf_files,
    wait_for_tensix_operations_finished,
)
from .makefile import (
    CoverageBuild,
    ProfilerBuild,
    build_test_variant,
    merge_coverage_streams_into_into,
)
from .target_config import TestTargetConfig


def run_test(
    test_config: dict,
    boot_mode: BootMode = BootMode.DEFAULT,  # global override boot mode here
    profiler_build: ProfilerBuild = ProfilerBuild.No,
    location="0,0",
):
    """Run the test with the given configuration"""

    variant_id = sha256(f"{str(test_config)}".encode()).hexdigest()
    test_target = TestTargetConfig()

    coverage_bld = CoverageBuild.No
    if test_target.with_coverage:
        coverage_bld = CoverageBuild.Yes

    build_test_variant(test_config, variant_id, boot_mode, profiler_build, coverage_bld)

    # run test
    elfs = run_elf_files(
        test_config["testname"], variant_id, boot_mode, location=location
    )
    wait_for_tensix_operations_finished(elfs, location)

    if test_target.with_coverage:
        generate_info_file_for_run(
            f"/tmp/tt-llk-build/{test_config['testname']}/{variant_id}",
            0,
            location,
        )

        merge_coverage_streams_into_into(test_config, variant_id)

    shutil.rmtree(f"/tmp/tt-llk-build/{test_config['testname']}/{variant_id}")
