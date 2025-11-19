# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC

import json
import os
import shutil
from dataclasses import fields, is_dataclass
from enum import Enum
from pathlib import Path
from typing import Any

import pandas as pd
import plotly
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


def _stats_timings(perf_data: pd.DataFrame) -> pd.DataFrame:

    # dont aggregate marker column
    timings = perf_data.columns.drop("marker")
    result = perf_data.groupby("marker", as_index=False)[timings].agg(["mean", "std"])

    columns = ["marker"]
    columns += [f"{stat}({col})" for col in timings for stat in ["mean", "std"]]

    result.columns = columns
    return result


def _stats_l1_to_l1(data: ProfilerData) -> pd.DataFrame:
    groups = data.zones().raw().groupby(["marker"])

    timings = []
    for (marker,), group in groups:

        unpack_start = group[
            (group["thread"] == "UNPACK") & (group["type"] == "ZONE_START")
        ].reset_index(drop=True)

        pack_end = group[
            (group["thread"] == "PACK") & (group["type"] == "ZONE_END")
        ].reset_index(drop=True)

        if len(unpack_start) == 0 or len(pack_end) == 0:
            raise ValueError(
                "Zone must be captured on both unpack and pack for L1_TO_L1 to work properly"
            )

        if len(unpack_start) != len(pack_end):
            raise ValueError(
                f"Unpack and pack must be paired properly for L1_TO_L1 to work properly"
            )

        durations = pack_end["timestamp"] - unpack_start["timestamp"]

        marker_timings = pd.DataFrame(
            {
                "marker": marker,
                PerfRunType.L1_TO_L1.name: durations,
            }
        )
        timings.append(marker_timings)

    return _stats_timings(pd.concat(timings, ignore_index=True))


def _stats_thread(stat: str, raw_thread: pd.DataFrame) -> pd.DataFrame:
    start_entries = raw_thread[(raw_thread["type"] == "ZONE_START")].reset_index(
        drop=True
    )

    end_entries = raw_thread[(raw_thread["type"] == "ZONE_END")].reset_index(drop=True)

    if len(start_entries) != len(end_entries):
        raise ValueError(
            f"Mismatched start/end zones: {len(start_entries)} != {len(end_entries)}"
        )

    timings = pd.DataFrame(
        {
            "marker": start_entries["marker"],
            stat: end_entries["timestamp"] - start_entries["timestamp"],
        }
    )

    return _stats_timings(timings)


def _stats_unpack_isolate(data: ProfilerData) -> pd.DataFrame:
    return _stats_thread(PerfRunType.UNPACK_ISOLATE.name, data.unpack().raw())


def _stats_math_isolate(data: ProfilerData) -> pd.DataFrame:
    return _stats_thread(PerfRunType.MATH_ISOLATE.name, data.math().raw())


def _stats_pack_isolate(data: ProfilerData) -> pd.DataFrame:
    return _stats_thread(PerfRunType.PACK_ISOLATE.name, data.pack().raw())


def _stats_l1_congestion(data: ProfilerData) -> pd.DataFrame:
    stats = [
        _stats_thread(f"{PerfRunType.L1_CONGESTION.name}[UNPACK]", data.unpack().raw()),
        _stats_thread(f"{PerfRunType.L1_CONGESTION.name}[PACK]", data.pack().raw()),
    ]

    return pd.concat(stats, ignore_index=True)


def perf_benchmark(
    test_config, run_types: list[PerfRunType], run_count=2, boot_mode=BootMode.DEFAULT
):  # global override boot mode for perf tests here

    STATS_FUNCTION = {
        PerfRunType.L1_TO_L1: _stats_l1_to_l1,
        PerfRunType.UNPACK_ISOLATE: _stats_unpack_isolate,
        PerfRunType.MATH_ISOLATE: _stats_math_isolate,
        PerfRunType.PACK_ISOLATE: _stats_pack_isolate,
        PerfRunType.L1_CONGESTION: _stats_l1_congestion,
    }
    SUPPORTED_RUNS = STATS_FUNCTION.keys()

    results = []

    for run_type in run_types:
        if run_type not in SUPPORTED_RUNS:
            raise AssertionError(f"ERROR: run_type={run_type} not implemented")

        get_stats = STATS_FUNCTION[run_type]

        test_config["perf_run_type"] = run_type
        build_test(test_config, boot_mode, ProfilerBuild.Yes)

        runs = []
        for _ in range(run_count):
            reset_mailboxes()
            run_elf_files(test_config["testname"], boot_mode)
            wait_for_tensix_operations_finished()

            profiler_data = Profiler.get_data(test_config["testname"])

            runs.append(profiler_data)

        results.append(get_stats(ProfilerData.concat(runs)))

    results = pd.concat(results, ignore_index=True)

    # combine all run types into a single row in the dataframe
    report = results.groupby("marker").first().reset_index()

    return report


class PerfReport:
    """
    Lazy evaluation container for performance benchmark data.

    Allows for lazy evaluated query and append operation to the report.

    """

    def __init__(
        self,
        frames: list[pd.DataFrame] | None = None,
        masks: list[pd.Series] | None = None,
    ):
        self._frames = frames or [pd.DataFrame()]
        self._masks = masks or [pd.Series()]
        self._stat_columns = set()
        self._sweep_columns = set()

    def append(
        self,
        frame: pd.DataFrame,
        stat_columns: set[str] | None = None,
        sweep_columns: set[str] | None = None,
    ) -> None:
        self._frames.append(frame)
        self._masks.append(pd.Series(True, index=frame.index))

        # Update column types when provided
        if stat_columns is not None:
            self._stat_columns.update(stat_columns)
        if sweep_columns is not None:
            self._sweep_columns.update(sweep_columns)

    def frame(self) -> pd.DataFrame:
        # merge
        frame = pd.concat(self._frames, ignore_index=True)
        mask = pd.concat(self._masks, ignore_index=True)

        # apply masks
        frame = frame[mask]

        # save
        self._frames = [frame]
        self._masks = [pd.Series(True, index=frame.index)]

        return frame

    def filter(self, column: str, value: Any) -> "PerfReport":
        """Filter: Generic column filter"""
        mask_chain = [
            mask & (frame[column] == value)
            for frame, mask in zip(self._frames, self._masks)
        ]
        filtered_report = PerfReport(frames=self._frames, masks=mask_chain)
        filtered_report._stat_columns = self._stat_columns.copy()
        filtered_report._sweep_columns = self._sweep_columns.copy()
        return filtered_report

    def marker(self, marker: str) -> "PerfReport":
        """Filter: Marker"""
        return self.filter("marker", marker)

    @property
    def sweep_names(self) -> list[str]:
        """Get names of sweep parameter columns"""
        df = self.frame()
        if df.empty:
            return []

        # Use tracked sweep columns if available
        if self._sweep_columns:
            return [col for col in df.columns if col in self._sweep_columns]

        # Fallback: everything before 'marker' column are sweep parameters
        columns = df.columns.tolist()
        if "marker" not in columns:
            raise AssertionError("DataFrame must contain 'marker' column")
        marker_index = columns.index("marker")
        return columns[:marker_index]

    @property
    def stat_names(self) -> list[str]:
        """Get names of statistics columns"""
        df = self.frame()
        if df.empty:
            return []

        # Use tracked stat columns if available
        if self._stat_columns:
            return [col for col in df.columns if col in self._stat_columns]

        # Fallback: everything after 'marker' column are statistics
        columns = df.columns.tolist()
        if "marker" not in columns:
            raise AssertionError("DataFrame must contain 'marker' column")
        marker_index = columns.index("marker")
        return columns[marker_index + 1 :]


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


def _get_sweep_values(params):
    return [
        value
        for param in params.values()
        for value in (_dataclass_values(param) if is_dataclass(param) else [param])
    ]


def _get_sweep(params):
    """Returns a DataFrame containing the sweep values for the given parameters"""

    names = _get_sweep_names(params)
    values = _get_sweep_values(params)

    return pd.DataFrame([values], columns=names)


def update_report(report: PerfReport, test_config, results):
    # TODO: make this more robust, handle nested dataclasses, etc.

    exclude = {
        "testname",
        "perf_run_type",
    }

    params = {
        param: value for param, value in test_config.items() if param not in exclude
    }

    sweep = _get_sweep(params)

    combined = sweep.merge(results, how="cross")

    # Set column types
    sweep_columns = set(sweep.columns)
    stat_columns = set(col for col in results.columns if col != "marker")

    report.append(combined, stat_columns=stat_columns, sweep_columns=sweep_columns)


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
    root = os.environ.get("LLK_HOME")
    if not root:
        raise AssertionError("Environment variable LLK_HOME is not set")

    benchmark_dir = create_benchmark_dir(testname)
    output_path = benchmark_dir / f"{testname}.csv"

    report.frame().to_csv(output_path, index=False)


def dump_scatter(testname: str, report: PerfReport):
    """Generate an interactive HTML performance report"""

    if not report.sweep_names:
        raise AssertionError(
            "No sweep parameters found - cannot generate performance report"
        )
    if not report.stat_names:
        raise AssertionError("No statistics found - cannot generate performance report")

    dir = create_benchmark_dir(testname)
    output_path = dir / f"{testname}.html"

    # Get the DataFrame
    df = report.frame()

    if df.empty:
        raise AssertionError("DataFrame is empty - no performance data to plot")

    # Convert DataFrame to records for JavaScript consumption
    records = df.to_dict("records")

    # Add index to each record for plotting
    for i, record in enumerate(records):
        record["index"] = i + 1
        # Convert sweep parameter values to strings for consistent filtering
        for param in report.sweep_names:
            record[param] = str(record[param])

    # Select only mean stats to plot and define default visible metric
    stat_names_to_plot = [col for col in report.stat_names if col.startswith("mean")]
    if not stat_names_to_plot:
        raise AssertionError("No mean statistics found - cannot generate plots")
    if "mean(L1_TO_L1)" in stat_names_to_plot:
        default_visible_metric = "mean(L1_TO_L1)"
    else:
        default_visible_metric = stat_names_to_plot[0]

    # Get embedded Plotly.js as string
    plotly_js_code = plotly.offline.get_plotlyjs()

    # Build custom HTML with fully embedded Plotly.js
    html = f"""
<!DOCTYPE html>
<html>
<head>
    <meta charset="utf-8" />
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Performance Report: {testname}</title>
    <script type="text/javascript">{plotly_js_code}</script>
    <style>
    html, body {{
        margin: 0;
        padding: 0;
        height: 100%;
        width: 100%;
        font-family: sans-serif;
        overflow-x: hidden;
    }}

    .container {{
        display: flex;
        flex-direction: column;
        height: 100vh;
        width: 100%;
    }}

    h1 {{
        margin: 15px 20px 10px 20px;
        color: #333;
        font-size: 24px;
        flex-shrink: 0;
    }}

    .controls {{
        margin: 10px 20px 15px 20px;
        padding: 15px;
        background-color: #f5f5f5;
        border-radius: 5px;
        flex-shrink: 0;
    }}

    .controls label {{
        margin-right: 15px;
        font-weight: bold;
    }}

    .controls button {{
        margin-left: 20px;
        padding: 5px 10px;
        background-color: #007bff;
        color: white;
        border: none;
        border-radius: 3px;
        cursor: pointer;
    }}

    .controls button:hover {{
        background-color: #0056b3;
    }}

    #plot {{
        flex: 1;
        margin: 0 20px 20px 20px;
        min-height: 0;
    }}
    </style>
</head>
<body>
    <div class="container">
        <h1>Interactive Performance Sweep Report: {testname}</h1>
        <div class="controls" id="controls"></div>
        <div id="plot"></div>
    </div>

    <script>
    const data = {json.dumps(records)};
    const paramNames = {json.dumps(report.sweep_names)};
    const metricNames = {json.dumps(stat_names_to_plot)};
    const defaultVisibleMetric = {json.dumps(default_visible_metric)};

    let selections = {{}};
    let metricVisibility = {{}};

    // Initialize selections and visibility
    paramNames.forEach(param => selections[param] = '');
    metricNames.forEach(metric => {{
        metricVisibility[metric] = (metric === defaultVisibleMetric) ? true : 'legendonly';
    }});

    // Create controls
    function createControls() {{
        const controlsDiv = document.getElementById('controls');

        // Create dropdowns for each parameter
        paramNames.forEach(param => {{
            const label = document.createElement('label');
            label.textContent = param + ': ';
            const select = document.createElement('select');
            select.id = param;
            select.onchange = () => {{
                selections[param] = select.value;
                updateDropdownOptions();
                updatePlot();
            }};
            label.appendChild(select);
            controlsDiv.appendChild(label);
        }});

        // Add reset all filters button
        const resetBtn = document.createElement('button');
        resetBtn.textContent = 'Reset All Filters';
        resetBtn.onclick = () => {{
            // Reset all selections
            paramNames.forEach(param => {{
                selections[param] = '';
                document.getElementById(param).value = '';
            }});
            updateDropdownOptions();
            updatePlot();
        }};
        controlsDiv.appendChild(resetBtn);

        updateDropdownOptions();
    }}

    // Update dropdown options based on current selections
    function updateDropdownOptions() {{
        // Filter data based on current selections
        const filteredData = data.filter(record =>
            Object.entries(selections).every(([param, value]) => !value || record[param] === value)
        );

        // Update each dropdown with available values
        paramNames.forEach(param => {{
            const select = document.getElementById(param);
            const currentValue = select.value;
            const availableValues = [...new Set(filteredData.map(record => record[param]))];

            // Rebuild options
            select.innerHTML = '<option value="">All</option>' +
                availableValues.map(value => {{
                    const selected = value === currentValue ? 'selected' : '';
                    return `<option value="${{value}}" ${{selected}}>${{value}}</option>`;
                }}).join('');

            // Update selection
            selections[param] = select.value;
        }});
    }}

    // Create the plot
    function createPlot() {{
        const plotDiv = document.getElementById('plot');

        // Create initial traces for all metrics
        const traces = metricNames.map(metric => {{
            const hoverText = data.map(record =>
                paramNames.map(param => `${{param}}=${{record[param]}}`).join(', ')
            );

            return {{
                x: data.map(record => record.index),
                y: data.map(record => record[metric]),
                type: 'bar',
                name: metric,
                hovertemplate: `<b>%{{customdata}}</b><br>${{metric}}: %{{y}}<extra></extra>`,
                customdata: hoverText,
                visible: metricVisibility[metric]
            }};
        }});

        const layout = {{
            title: {{
                text: 'Performance Report',
                font: {{ size: 18 }},
                x: 0.5,
                xanchor: 'center'
            }},
            xaxis: {{
                title: {{
                    text: 'Sweep Index (hover for parameter details)',
                    font: {{ size: 14, color: '#333' }}
                }},
                showgrid: true,
                gridcolor: '#e0e0e0',
                tickfont: {{ size: 12, color: '#333' }}
            }},
            yaxis: {{
                title: {{
                    text: 'Cycles / Tile',
                    font: {{ size: 14, color: '#333' }}
                }},
                showgrid: true,
                gridcolor: '#e0e0e0',
                tickfont: {{ size: 12, color: '#333' }}
            }},
            legend: {{
                title: {{ text: 'Metrics', font: {{ size: 12 }} }},
                orientation: 'v',
                x: 1.02,
                xanchor: 'left',
                y: 1,
                yanchor: 'top'
            }},
            template: 'plotly_white',
            margin: {{ l: 60, r: 120, t: 50, b: 60 }},
            autosize: true
        }};

        Plotly.newPlot(plotDiv, traces, layout, {{responsive: true, displayModeBar: true}});

        // Handle legend clicks to toggle metric visibility
        plotDiv.on('plotly_legendclick', function(eventData) {{
            const metricName = eventData.data[eventData.curveNumber].name;
            const currentVisibility = metricVisibility[metricName];
            metricVisibility[metricName] = (currentVisibility === true) ? 'legendonly' : true;
            updatePlot();
            return false; // Prevent default legend click behavior
        }});

        // Resize plot when window resizes
        window.addEventListener('resize', function() {{
            Plotly.Plots.resize(plotDiv);
        }});
    }}

    // Update the plot based on current filters and settings
    function updatePlot() {{
        const plotDiv = document.getElementById('plot');

        // Apply filters
        paramNames.forEach(param => {{
            const select = document.getElementById(param);
            selections[param] = select.value;
        }});

        const filteredData = data.filter(record =>
            Object.entries(selections).every(([param, value]) => !value || record[param] === value)
        );

        const indices = filteredData.map(record => record.index);

        // Create traces for visible metrics
        const traces = metricNames.map(metric => {{
            const visible = metricVisibility[metric];
            const yValues = filteredData.map(record => record[metric]);
            const hoverText = filteredData.map(record =>
                paramNames.map(param => `${{param}}=${{record[param]}}`).join(', ')
            );

            return {{
                x: indices,
                y: yValues,
                type: 'bar',
                name: metric,
                visible: visible,
                customdata: hoverText,
                hovertemplate: `<b>%{{customdata}}</b><br>${{metric}}: %{{y}}<extra></extra>`
            }};
        }});

        const layout = {{
            title: {{
                text: 'Performance Report',
                font: {{ size: 18 }},
                x: 0.5,
                xanchor: 'center'
            }},
            xaxis: {{
                title: {{
                    text: 'Sweep Index (hover for parameter details)',
                    font: {{ size: 14, color: '#333' }}
                }},
                showgrid: true,
                gridcolor: '#e0e0e0',
                tickfont: {{ size: 12, color: '#333' }}
            }},
            yaxis: {{
                title: {{
                    text: 'Cycles / Tile',
                    font: {{ size: 14, color: '#333' }}
                }},
                showgrid: true,
                gridcolor: '#e0e0e0',
                tickfont: {{ size: 12, color: '#333' }}
            }},
            legend: {{
                title: {{ text: 'Metrics', font: {{ size: 12 }} }},
                orientation: 'v',
                x: 1.02,
                xanchor: 'left',
                y: 1,
                yanchor: 'top'
            }},
            template: 'plotly_white',
            margin: {{ l: 60, r: 120, t: 50, b: 60 }},
            autosize: true
        }};

        Plotly.react(plotDiv, traces, layout);
    }}

    // Initialize everything
    createControls();
    createPlot();
    </script>
</body>
</html>
"""

    with open(str(output_path), "w") as f:
        f.write(html)
