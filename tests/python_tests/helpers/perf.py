# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC

import csv
import os
import shutil
from dataclasses import dataclass, field, fields, is_dataclass
from enum import Enum
from pathlib import Path
from statistics import mean, stdev
from typing import List

import pandas as pd
import plotly.graph_objects as go
import pytest
from helpers.device import (
    BootMode,
    reset_mailboxes,
    run_elf_files,
    wait_for_tensix_operations_finished,
)
from helpers.profiler import Profiler, ProfilerData
from helpers.test_config import ProfilerBuild, build_test


class PerfRunType(Enum):
    L1_TO_L1 = 1
    UNPACK_ISOLATE = 2
    MATH_ISOLATE = 3
    PACK_ISOLATE = 4
    L1_CONGESTION = 5


ALL_RUN_TYPES = [type for type in PerfRunType]


def _timings_l1_to_l1(perf_data: pd.DataFrame) -> pd.Series:
    # todo: at some point we should support other markers
    runs = perf_data[
        (perf_data["run_type"] == PerfRunType.L1_TO_L1.name)
        & (perf_data["type"] == "ZONE")
        & (perf_data["marker"] == "TILE_LOOP")
    ]

    groups = runs.groupby(["run_index", "marker"])

    dataframes = []
    for (run_index, marker), group in groups:
        unpack = group[group["thread"] == "UNPACK"]
        pack = group[group["thread"] == "PACK"]

        if len(unpack) == 0 or len(pack) == 0:
            raise ValueError(
                "Zone must be captured on both unpack and pack for this to work properly"
            )

        if len(unpack) != len(pack):
            raise ValueError(
                f"Unpack and pack must be paired properly for L1_TO_L1 to work properly"
            )

        unpack_start = unpack["timestamp"]
        pack_end = pack["timestamp"] + pack["duration"]
        durations = pack_end - unpack_start

        group_df = pd.DataFrame(
            {
                "run_type": PerfRunType.L1_TO_L1.name,
                "run_index": run_index,
                "marker": marker,
                "duration": durations,
            }
        )
        dataframes.append(group_df)

    return pd.concat(dataframes, ignore_index=True)


def _timings_unpack(perf_data: pd.DataFrame) -> pd.DataFrame:
    # todo: at some point we should support other markers
    runs = perf_data[
        (perf_data["run_type"] == PerfRunType.UNPACK_ISOLATE.name)
        & (perf_data["type"] == "ZONE")
        & (perf_data["marker"] == "TILE_LOOP")
        & (perf_data["thread"] == "UNPACK")
    ]

    return pd.DataFrame(
        {
            "run_type": runs["run_type"],
            "run_index": runs["run_index"],
            "marker": runs["marker"],
            "duration": runs["duration"],
        }
    )


def _timings_math(perf_data: pd.DataFrame) -> pd.DataFrame:
    # todo: at some point we should support other markers
    runs = perf_data[
        (perf_data["run_type"] == PerfRunType.MATH_ISOLATE.name)
        & (perf_data["type"] == "ZONE")
        & (perf_data["marker"] == "TILE_LOOP")
        & (perf_data["thread"] == "MATH")
    ]

    return pd.DataFrame(
        {
            "run_type": runs["run_type"],
            "run_index": runs["run_index"],
            "marker": runs["marker"],
            "duration": runs["duration"],
        }
    )


def _timings_pack(perf_data: pd.DataFrame) -> pd.DataFrame:
    # todo: at some point we should support other markers
    runs = perf_data[
        (perf_data["run_type"] == PerfRunType.PACK_ISOLATE.name)
        & (perf_data["type"] == "ZONE")
        & (perf_data["marker"] == "TILE_LOOP")
        & (perf_data["thread"] == "PACK")
    ]

    return pd.DataFrame(
        {
            "run_type": runs["run_type"],
            "run_index": runs["run_index"],
            "marker": runs["marker"],
            "duration": runs["duration"],
        }
    )


def _timings_l1_congestion(perf_data: pd.DataFrame) -> pd.DataFrame:
    # todo: at some point we should support other markers
    runs = perf_data[
        (perf_data["run_type"] == PerfRunType.L1_CONGESTION.name)
        & (perf_data["type"] == "ZONE")
        & (perf_data["marker"] == "TILE_LOOP")
    ]

    return pd.DataFrame(
        {
            "run_type": runs["run_type"],
            "run_index": runs["run_index"],
            "marker": runs["marker"],
            "duration[UNPACK]": runs[runs["thread"] == "UNPACK"]["duration"],
            "duration[PACK]": runs[runs["thread"] == "PACK"]["duration"],
        }
    )


def process_runs(runs, test_config):
    loop_factor = test_config.get("loop_factor", 1)
    tile_cnt = test_config.get("tile_cnt", 1)

    return tuple(
        {
            "mean": mean(column) / loop_factor / tile_cnt,
            "stddev": stdev(column) / loop_factor / tile_cnt,
        }
        for column in zip(*runs)
    )


def _process_profiler_data(
    run_type: PerfRunType, run_index: int, profiler_data: ProfilerData
) -> pd.DataFrame:
    df = profiler_data.frame()
    df.insert(0, "run_type", run_type.name)
    df.insert(1, "run_index", run_index)
    return df


def _generate_report(perf_data: pd.DataFrame):
    # Group by run_type and marker to calculate aggregated statistics
    grouped = perf_data.groupby(["run_type", "marker"], as_index=False)
    result_dfs = []

    for (run_type, marker), group in grouped:
        # Create a base row with run_type and marker
        result_row = {"run_type": run_type, "marker": marker}

        # Find all duration columns (both 'duration' and 'duration[THREAD_NAME]' patterns)
        duration_cols = [col for col in group.columns if col.startswith("duration")]

        for col in duration_cols:
            # Get non-null values for this duration column
            values = group[col]

            if len(values) == 1:
                # Only one sample - set avg to NA as requested
                result_row[f"{col}.AVG"] = values[0]
            else:
                # Multiple samples - calculate mean and std
                result_row[f"{col}.AVG"] = values.mean()
                result_row[f"{col}.STD"] = values.std()

        result_dfs.append(pd.DataFrame([result_row]))

    return pd.concat(result_dfs, ignore_index=True)


def perf_benchmark(
    test_config, run_types: list[PerfRunType], run_count=2, boot_mode=BootMode.DEFAULT
):  # global override boot mode for perf tests here

    RUN_CONFIGURATIONS = {
        PerfRunType.L1_TO_L1: _timings_l1_to_l1,
        PerfRunType.UNPACK_ISOLATE: _timings_unpack,
        PerfRunType.MATH_ISOLATE: _timings_math,
        PerfRunType.PACK_ISOLATE: _timings_pack,
        PerfRunType.L1_CONGESTION: _timings_l1_congestion,
    }
    SUPPORTED_RUNS = RUN_CONFIGURATIONS.keys()

    results = []

    for run_type in run_types:
        assert run_type in SUPPORTED_RUNS, f"ERROR: run_type={run_type} not implemented"

        get_timing = RUN_CONFIGURATIONS[run_type]

        test_config["perf_run_type"] = run_type
        build_test(test_config, boot_mode, ProfilerBuild.Yes)

        runs = []
        for run_index in range(run_count):
            reset_mailboxes()
            run_elf_files(test_config["testname"], boot_mode)
            wait_for_tensix_operations_finished()

            profiler_data = Profiler.get_data(test_config["testname"])
            runs.append(_process_profiler_data(run_type, run_index, profiler_data))

        results.append(get_timing(pd.concat(runs, ignore_index=True)))

    results = pd.concat(results, ignore_index=True)
    report = _generate_report(results)

    report.to_csv(f"test.csv")
    return report


@dataclass
class PerfReport:
    sweep_names: List[str] = field(default_factory=list)
    stat_names: List[str] = field(default_factory=list)
    sweep_values: List[List] = field(default_factory=list)
    stat_values: List[List] = field(default_factory=list)


@pytest.fixture(scope="module")
def perf_report(request):
    report = PerfReport()

    test_module = request.path.stem

    delete_benchmark_dir(test_module)
    try:
        yield report
    except Exception as e:
        print("Perf: Unexpected error, Saving report anyway", e)

    dump_report(test_module, report)
    dump_scatter(test_module, report)


def _dataclass_names(parent, obj):
    """Provides the **names** of the columns for the report"""
    return [f"{parent}.{f.name}" for f in fields(obj)]


def _dataclass_values(obj):
    """Provides the **values** of the columns for the report"""
    return [getattr(obj, f.name) for f in fields(obj)]


def _get_sweep_names(params):
    names = []
    for param, value in params.items():
        if is_dataclass(value):
            names.extend(_dataclass_names(param, value))
        else:
            names.append(param)

    return names


def _get_stat_names(result):
    """Version with pre-allocation."""

    # Pre-calculate total size needed
    total_stats = sum(len(stats) for stats in result.values())
    names = [None] * (total_stats * 2)  # Pre-allocate exact size

    column = 0
    for run_type, stats in result.items():
        stats_count = len(stats)
        for idx in range(stats_count):
            idx_str = f"[{idx}]" if len(stats) > 1 else ""
            names[column] = f"mean({run_type.name}{idx_str})"
            names[column + 1] = f"stddev({run_type.name}{idx_str})"
            column += 2

    return names


def update_report(report: PerfReport, test_config, results):

    exclude = {
        "testname",
        "perf_run_type",
        "loop_factor",  # used to minimize the effect of profiler overhead for short loops
    }

    params = {
        param: value for param, value in test_config.items() if param not in exclude
    }

    if not report.sweep_names:
        report.sweep_names = _get_sweep_names(params)

    if not report.stat_names:
        report.stat_names = _get_stat_names(results)

    sweep = []
    for param in params.values():
        if is_dataclass(param):
            sweep.extend(_dataclass_values(param))
        else:
            sweep.append(param)
    report.sweep_values.append(sweep)

    stat_values = []
    for stats in results.values():
        for stat in stats:
            stat_values.append(stat["mean"])
            stat_values.append(stat["stddev"])
    report.stat_values.append(stat_values)


def delete_benchmark_dir(testname: str):
    root = os.environ.get("LLK_HOME")
    if not root:
        raise AssertionError("Environment variable LLK_HOME is not set")

    path = Path(root) / "perf_data" / testname

    if path.exists() and path.is_dir():
        shutil.rmtree(path)


def create_benchmark_dir(testname: str):
    root = os.environ.get("LLK_HOME")
    if not root:
        raise AssertionError("Environment variable LLK_HOME is not set")

    output_path = Path(root) / "perf_data" / testname
    output_path.mkdir(parents=True, exist_ok=True)

    return output_path


def dump_report(testname: str, report: PerfReport):
    if len(report.sweep_values) != len(report.stat_values):
        raise ValueError("Mismatch between sweep_values and stat_values lengths")

    root = os.environ.get("LLK_HOME")
    if not root:
        raise AssertionError("Environment variable LLK_HOME is not set")

    benchmark_dir = create_benchmark_dir(testname)
    output_path = benchmark_dir / f"{testname}.csv"

    header_row = report.sweep_names + report.stat_names
    data_rows = [
        sweep_vals + stat_vals
        for sweep_vals, stat_vals in zip(report.sweep_values, report.stat_values)
    ]

    # Write to CSV
    with open(output_path, "a", newline="") as csvfile:
        writer = csv.writer(csvfile)
        writer.writerow(header_row)
        writer.writerows(data_rows)


def dump_scatter(testname: str, report: PerfReport):
    # generate a scatter plot using plotly.graph_objects (no pandas required)

    if not report.sweep_names or not report.stat_names:
        raise ValueError("Report is missing names")

    dir = create_benchmark_dir(testname)
    output_path = dir / f"{testname}.html"

    # x-axis: sweep values (left to right, zipped for each sweep)
    # y-axis: stat values (for each run type, for each stat)
    # stat_names: e.g. mean(L1_TO_L1), mean(UNPACK_ISOLATE), ...
    # sweep_names: e.g. tile_cnt, param2, ...

    fig = go.Figure()

    mean_columns = [
        (name, i) for i, name in enumerate(report.stat_names) if name.startswith("mean")
    ]

    hover = [
        ", ".join(f"{name}={val}" for name, val in zip(report.sweep_names, sweep))
        for sweep in report.sweep_values
    ]

    # For each stat column (run type), plot all points
    for stat_name, stat_idx in mean_columns:
        y_vals = [stat[stat_idx] for stat in report.stat_values]

        fig.add_trace(
            go.Scatter(
                x=list(range(len(report.sweep_values))),
                y=y_vals,
                mode="markers+lines",
                name=stat_name,
                text=hover,
                hoverinfo="text+y",
            )
        )

    # X-axis label
    xaxis_title = "Sweep index (see hover for values)"

    fig.update_layout(
        title=f"Performance Scatter Plot: {testname}",
        xaxis_title=xaxis_title,
        yaxis_title="Cycles / Tile",
        legend_title="Run Type / Stat",
    )

    fig.write_html(str(output_path))
