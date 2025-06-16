# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0


from helpers.device import (
    run_elf_files,
    wait_for_tensix_operations_finished,
)
from helpers.profiler import Profiler, build_with_profiler


def test_profiler_primitives():

    test_config = {
        "testname": "profiler_primitives_test",
    }

    profiler_meta = build_with_profiler(test_config)
    assert profiler_meta is not None, "Profiler metadata should not be None"

    run_elf_files("profiler_primitives_test")
    wait_for_tensix_operations_finished()

    runtime = Profiler.get_data(profiler_meta)

    # ZONE_SCOPED
    zone = runtime.unpack[0]
    zone_marker = zone.full_marker
    assert (
        zone_marker.marker == "TEST_ZONE"
    ), f"Expected zone_maker.marker = 'TEST_ZONE', got {zone_marker.marker}"
    assert zone_marker.file.endswith(
        "profiler_primitives_test.cpp"
    ), f"expected zone_marker.file to end with 'profiler_primitives_test.cpp', got {zone_marker.file}"
    assert (
        zone_marker.line == 17
    ), f"Expected zone_marker.line = 17, got {zone_marker.line}"
    assert (
        zone_marker.id == 42158
    ), f"Expected zone_marker.id = 42158, got {zone_marker.id}"
    assert zone.start > 0, f"Expected zone.start > 0, got {zone.start}"
    assert zone.end > zone.start, f"Expected zone.end > {zone.start}, got {zone.end}"
    assert (
        zone.duration == zone.end - zone.start
    ), f"Expected zone.duration = {zone.end - zone.start}, got {zone.duration}"

    # TIMESTAMP
    timestamp = runtime.math[0]
    timestamp_marker = timestamp.full_marker
    assert (
        timestamp_marker.marker == "TEST_TIMESTAMP"
    ), f"Expected timestamp_marker.marker = 'TEST_TIMESTAMP', got {timestamp_marker.marker}"
    assert timestamp_marker.file.endswith(
        "profiler_primitives_test.cpp"
    ), f"expected timestamp_marker.file to end with 'profiler_primitives_test.cpp', got {timestamp_marker.file}"
    assert (
        timestamp_marker.line == 26
    ), f"Expected timestamp_marker.line = 26, got {timestamp_marker.line}"
    assert (
        timestamp_marker.id == 28111
    ), f"Expected timestamp_marker.id = 28111, got {timestamp_marker.id}"
    assert (
        timestamp.timestamp > 0
    ), f"Expected timestamp.timestamp > 0, got {timestamp.timestamp}"
    assert (
        timestamp.data is None
    ), f"Expected timestamp.date to be None, got {timestamp.data}"

    timestamp_data = runtime.pack[0]
    timestamp_data_marker = timestamp_data.full_marker
    assert (
        timestamp_data_marker.marker == "TEST_TIMESTAMP_DATA"
    ), f"Expected timestamp_data_marker.marker = 'TEST_TIMESTAMP_DATA', got {timestamp_data_marker.marker}"
    assert timestamp_data_marker.file.endswith(
        "profiler_primitives_test.cpp"
    ), f"expected timestamp_data_marker.file to end with 'profiler_primitives_test.cpp', got {timestamp_data_marker.file}"
    assert (
        timestamp_data_marker.line == 35
    ), f"Expected timestamp_data_marker.line = 35, got {timestamp_data_marker.line}"
    assert (
        timestamp_data_marker.id == 18694
    ), f"Expected timestamp_data_marker.id = 18694, got {timestamp_data_marker.id}"
    assert (
        timestamp_data.timestamp > 0
    ), f"Expected timestamp_data.timestamp > 0, got {timestamp_data.timestamp}"
    assert (
        timestamp_data.data == 0xBADC0FFE0DDF00D
    ), f"Expected timestamp_data.data = 0xBADC0FFE0DDF00D, got {hex(timestamp_data.data)}"
