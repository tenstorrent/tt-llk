# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC

import csv
import os
import shutil
from dataclasses import dataclass
from enum import Enum
from pathlib import Path
from statistics import mean, variance

from helpers.device import run_elf_files, wait_for_tensix_operations_finished
from helpers.profiler import Profiler, build_with_profiler


@dataclass
class PerfZone:
    start: int
    end: int
    duration: int


@dataclass
class PerfThreadData:
    init: PerfZone
    tile_loop: PerfZone
    kernel: PerfZone


@dataclass
class PerfData:
    unpack: PerfThreadData
    math: PerfThreadData
    pack: PerfThreadData


def _parse_thread(thread_data) -> PerfThreadData:
    zones = {}
    markers = {"kernel", "init", "tile_loop"}

    for entry in thread_data:
        marker = entry.full_marker.marker.lower()
        if marker in markers:
            zones[marker] = PerfZone(
                start=entry.start, end=entry.end, duration=entry.duration
            )

    if len(zones) < len(markers):
        missing = markers - zones.keys()
        raise AssertionError(
            f"Missing zones after perf run: {', '.join(sorted(missing))}"
        )

    return PerfThreadData(**zones)


def process_profiler_data(profiler_data) -> PerfData:
    return PerfData(
        unpack=_parse_thread(profiler_data.unpack),
        math=_parse_thread(profiler_data.math),
        pack=_parse_thread(profiler_data.pack),
    )


def timing_l1_to_l1(perf_data: PerfData) -> int:
    """Time to perform the whole operation (compute)"""
    return (perf_data.pack.tile_loop.end - perf_data.unpack.tile_loop.start,)


def timing_unpack(perf_data: PerfData) -> int:
    return (perf_data.unpack.tile_loop.duration,)


def timing_math(perf_data: PerfData) -> int:
    return (perf_data.math.tile_loop.duration,)


def timing_pack(perf_data: PerfData) -> int:
    return (perf_data.pack.tile_loop.duration,)


def timing_l1_congestion(perf_data: PerfData) -> int:
    return (
        perf_data.unpack.tile_loop.duration,
        perf_data.pack.tile_loop.duration,
    )


RUN_COUNT = 8


def _build_l1_to_l1(test_config):
    test_config["perf_run_type"] = PerfRunType.L1_TO_L1
    return build_with_profiler(test_config)


def _build_unpack_isolate(test_config):
    test_config["perf_run_type"] = PerfRunType.UNPACK_ISOLATE
    return build_with_profiler(test_config)


def process_runs(runs, test_config):
    tile_cnt = test_config.get("tile_cnt", 1)

    return tuple(
        {
            "mean": mean(column) / tile_cnt,
            "variance": variance(column) / (tile_cnt * tile_cnt),
        }
        for column in zip(*runs)
    )


class PerfRunType(Enum):
    L1_TO_L1 = 1
    UNPACK_ISOLATE = 2
    MATH_ISOLATE = 3
    PACK_ISOLATE = 4
    L1_CONGESTION = 5


def perf_benchmark(test_config, run_types: list[PerfRunType]):
    # todo: support all types of runs
    SUPPORTED_RUNS = {PerfRunType.L1_TO_L1, PerfRunType.UNPACK_ISOLATE}
    RUN_CONFIGURATIONS = {
        PerfRunType.L1_TO_L1: (_build_l1_to_l1, timing_l1_to_l1),
        PerfRunType.UNPACK_ISOLATE: (_build_unpack_isolate, timing_unpack),
        # Add new run types here as they're implemented:
        # PerfRunType.MATH_ISOLATE: (_build_math_isolate, timing_math),
        # PerfRunType.PACK_ISOLATE: (_build_pack_isolate, timing_pack),
        # PerfRunType.L1_CONGESTION: (_build_l1_congestion, timing_l1_congestion),
    }

    results = {}

    for type in run_types:
        assert type in SUPPORTED_RUNS, f"ERROR: run_type={type} not implemented"
        build, get_timing = RUN_CONFIGURATIONS[type]

        profiler_meta = build(test_config)

        runs = []
        for _ in range(RUN_COUNT):
            run_elf_files(test_config["testname"])
            wait_for_tensix_operations_finished()

            profiler_data = Profiler.get_data(profiler_meta)
            perf_data = process_profiler_data(profiler_data)

            runs.append(get_timing(perf_data))

        results[type] = process_runs(runs, test_config)

    return results


def delete_reports():
    assert "LLK_HOME" in os.environ, "Environment variable LLK_HOME is not set"
    root = os.environ["LLK_HOME"]

    path = Path(root) / "perf"
    print(path)

    if path.exists() and path.is_dir():
        shutil.rmtree(path)


def write_to_report(test_config, run_types, results):
    assert "LLK_HOME" in os.environ, "Environment variable LLK_HOME is not set"
    root = os.environ["LLK_HOME"]

    filename = f"{test_config['testname']}.csv"
    output_path = Path(root) / "perf" / filename
    output_path.parent.mkdir(parents=True, exist_ok=True)

    exclude = {"testname", "tile_cnt", "formats"}  # fix: include format info
    sweep_columns = [param for param in test_config.keys() if not param in exclude]

    result_columns = []
    for run_type in run_types:
        result_columns.append(f"mean({run_type.name})")
        result_columns.append(f"variance({run_type.name})")

    row = [test_config[k] for k in sweep_columns]
    for run_type in run_types:
        stats = results[run_type]
        for stat in stats:
            # fix : multiple stats per run type
            row.append(stat["mean"])
            row.append(stat["variance"])

    # Write to CSV
    first_entry = not os.path.exists(output_path)
    with open(output_path, "a", newline="") as csvfile:
        writer = csv.writer(csvfile)
        if first_entry:
            writer.writerow(sweep_columns + result_columns)
        writer.writerow(row)
