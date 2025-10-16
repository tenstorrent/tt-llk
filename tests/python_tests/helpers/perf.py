# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC

import csv
import json
import os
import shutil
from dataclasses import dataclass, field, fields, is_dataclass
from enum import Enum
from pathlib import Path
from statistics import mean, stdev
from typing import List

import plotly.graph_objects as go
import pytest
from helpers.device import (
    BootMode,
    reset_mailboxes,
    run_elf_files,
    wait_for_tensix_operations_finished,
)
from helpers.profiler import Profiler
from helpers.test_config import ProfilerBuild, build_test


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


class PerfRunType(Enum):
    L1_TO_L1 = 1
    UNPACK_ISOLATE = 2
    MATH_ISOLATE = 3
    PACK_ISOLATE = 4
    L1_CONGESTION = 5


ALL_RUN_TYPES = [type for type in PerfRunType]


def perf_benchmark(
    test_config, run_types: list[PerfRunType], run_count=2, boot_mode=BootMode.DEFAULT
):  # global override boot mode for perf tests here

    RUN_CONFIGURATIONS = {
        PerfRunType.L1_TO_L1: timing_l1_to_l1,
        PerfRunType.UNPACK_ISOLATE: timing_unpack,
        PerfRunType.MATH_ISOLATE: timing_math,
        PerfRunType.PACK_ISOLATE: timing_pack,
        PerfRunType.L1_CONGESTION: timing_l1_congestion,
    }
    SUPPORTED_RUNS = RUN_CONFIGURATIONS.keys()

    results = {}

    for type in run_types:
        assert type in SUPPORTED_RUNS, f"ERROR: run_type={type} not implemented"
        get_timing = RUN_CONFIGURATIONS[type]

        test_config["perf_run_type"] = type
        build_test(test_config, boot_mode, ProfilerBuild.Yes)

        runs = []
        for _ in range(run_count):
            reset_mailboxes()
            run_elf_files(test_config["testname"], boot_mode)
            wait_for_tensix_operations_finished()

            profiler_data = Profiler.get_data(test_config["testname"])
            perf_data = process_profiler_data(profiler_data)

            runs.append(get_timing(perf_data))

        results[type] = process_runs(runs, test_config)

    return results


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
        # This is possible on CI when the whole split of the test is skipped
        return

    dir = create_benchmark_dir(testname)
    output_path = dir / f"{testname}.html"

    # x-axis: sweep values (left to right, zipped for each sweep)
    # y-axis: stat values (for each run type, for each stat)
    # stat_names: e.g. mean(L1_TO_L1), mean(UNPACK_ISOLATE), ...
    # sweep_names: e.g. tile_cnt, param2, ...

    # ---- Build combined records ----
    records = []
    for i, (params, mvals) in enumerate(zip(report.sweep_values, report.stat_values)):
        entry = {"index": i + 1}
        entry.update({k: str(v) for k, v in zip(report.sweep_names, params)})
        entry.update({k: v for k, v in zip(report.stat_names, mvals)})
        records.append(entry)

    # ---- Create default Plotly figure ----
    fig = go.Figure()
    stat_names_to_plot = [m for m in report.stat_names if m.startswith("mean")]
    print(stat_names_to_plot)

    for m in stat_names_to_plot:
        fig.add_trace(
            go.Scatter(
                x=[r["index"] for r in records],
                y=[r[m] for r in records],
                mode="lines+markers",
                name=m,
                text=[
                    ", ".join([f"{p}={r[p]}" for p in report.sweep_names])
                    for r in records
                ],
                hovertemplate="<b>%{text}</b><br>" + f"{m}: " + "%{y}<extra></extra>",
            )
        )

    fig.update_layout(
        title=f"Performance Scatter Plot: {testname}",
        xaxis_title="Sweep index (see hover for values)",
        yaxis_title="Cycles / Tile",
        template="plotly_white",
        legend_title="Run Type / Stat",
    )

    # ---- Prepare data for embedding ----
    fig_json = fig.to_plotly_json()
    fig_data_json = json.dumps(fig_json["data"])
    fig_layout_json = json.dumps(fig_json["layout"])
    records_json = json.dumps(records)
    param_names_json = json.dumps(report.sweep_names)
    metric_names_json = json.dumps(stat_names_to_plot)

    # ---- Build HTML ----
    html = f"""
    <html>
    <head>
    <script src="https://cdn.plot.ly/plotly-latest.min.js"></script>
    <style>
    html, body {{
        margin: 0;
        padding: 0;
        height: 100%;
        width: 100%;
        font-family: sans-serif;
    }}
    #controls {{
        padding: 10px;
    }}
    #plot {{
        width: 100%;
        height: calc(100vh - 80px); /* fills the rest of the screen */
    }}
    </style>
    </head>
    <body>
    <h2>Interactive Performance Sweep Report</h2>
    <div id="controls"></div>
    <div id="plot"></div>

    <script>
    const allRecords = {records_json};
    const paramNames = {param_names_json};
    const metricNames = {metric_names_json};
    const layout = {fig_layout_json};

    let selections = {{}};

    // --- Create dropdowns ---
    const controlsDiv = document.getElementById('controls');
    paramNames.forEach(p => {{
    const values = [...new Set(allRecords.map(r => r[p]))];
    const label = document.createElement('label');
    label.textContent = p + ': ';
    const select = document.createElement('select');
    select.id = p;
    const allOpt = document.createElement('option');
    allOpt.value = '';
    allOpt.text = 'All';
    select.appendChild(allOpt);
    values.forEach(v => {{
        const opt = document.createElement('option');
        opt.value = v;
        opt.text = v;
        select.appendChild(opt);
    }});
    select.onchange = updatePlot;
    label.appendChild(select);
    controlsDiv.appendChild(label);
    selections[p] = '';
    }});

    // --- Draw initial plot ---
    let figData = {fig_data_json};
    Plotly.newPlot('plot', figData, layout, {{responsive: true}});

    // --- Update plot based on filters ---
    function updatePlot() {{
    paramNames.forEach(p => selections[p] = document.getElementById(p).value);
    const filtered = allRecords.filter(r => {{
        return Object.entries(selections).every(([k, v]) => !v || r[k] == v);
    }});

    const indices = filtered.map(r => r.index);
    const traces = metricNames.map(m => {{
        return {{
        x: indices,
        y: filtered.map(r => r[m]),
        mode: 'lines+markers',
        name: m,
        text: filtered.map(r => paramNames.map(p => `${{p}}=${{r[p]}}`).join(', ')),
        hovertemplate: `<b>%{{text}}</b><br>${{m}}: %{{y}}<extra></extra>`
        }};
    }});
    Plotly.react('plot', traces, layout);
    }}
    </script>
    </body>
    </html>
    """

    with open(str(output_path), "w") as f:
        f.write(html)
