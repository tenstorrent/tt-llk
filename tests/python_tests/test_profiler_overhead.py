# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0

from helpers.device import (
    run_elf_files,
    wait_for_tensix_operations_finished,
)
from helpers.profiler import Profiler, build_with_profiler


def test_profiler_overhead():

    EXPECTED_OVERHEAD = 36

    test_config = {
        "testname": "profiler_overhead_test",
    }

    profiler_meta = build_with_profiler(test_config)
    assert profiler_meta is not None, "Profiler metadata should not be None"

    run_elf_files("profiler_overhead_test")
    wait_for_tensix_operations_finished()

    runtime = Profiler.get_data(profiler_meta)

    # filter out all zones that dont have marker "OVERHEAD"
    overhead_zones = [x for x in runtime.unpack if x.full_marker.marker == "OVERHEAD"]
    assert (
        len(overhead_zones) == 32
    ), f"Expected 32 overhead zones, got {len(overhead_zones)}"

    for loop_iterations, zone in enumerate(overhead_zones, 8):
        overhead = zone.duration - (loop_iterations * 10)

        abs_err = abs(overhead - EXPECTED_OVERHEAD)
        assert (
            abs_err < 5
        ), f"Expected overhead {EXPECTED_OVERHEAD}, got {overhead} cycles"
