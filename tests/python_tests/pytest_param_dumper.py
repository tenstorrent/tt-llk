#!/usr/bin/env python3
# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0

"""
Pytest plugin to dump all test parameters during test collection.
This captures the actual parameter values that pytest generates.

Usage:
    pytest --collect-only --param-dump=params_dump.json
    pytest --collect-only --param-dump=params_dump.csv --param-format=csv
"""

import json
from collections import defaultdict
from pathlib import Path

import pandas as pd


def pytest_addoption(parser):
    """Add command line options for parameter dumping."""
    parser.addoption(
        "--param-dump",
        action="store",
        default=None,
        help="Path to dump test parameters to (e.g., params_dump.json)",
    )
    parser.addoption(
        "--param-format",
        action="store",
        default="json",
        choices=["json", "csv", "excel", "all"],
        help="Format for parameter dump output",
    )
    parser.addoption(
        "--param-summary",
        action="store_true",
        default=False,
        help="Generate summary statistics for parameters",
    )


def pytest_collection_modifyitems(config, items):
    """Hook to capture test items after collection."""
    dump_path = config.getoption("--param-dump")
    if not dump_path:
        return

    dump_format = config.getoption("--param-format")
    generate_summary = config.getoption("--param-summary")

    # Extract parameter information from all test items
    test_data = []
    param_values = defaultdict(set)  # Track unique values per parameter
    param_usage = defaultdict(int)  # Track usage count per parameter

    for item in items:
        # Get basic test information
        test_info = {
            "test_id": item.nodeid,
            "file": str(item.fspath.relative_to(Path.cwd())),
            "test_name": item.name,
            "function": item.function.__name__ if hasattr(item, "function") else None,
            "class": item.cls.__name__ if item.cls else None,
            "module": item.module.__name__ if hasattr(item, "module") else None,
            "parameters": {},
        }

        # Extract parameters if this is a parametrized test
        if hasattr(item, "callspec"):
            callspec = item.callspec
            test_info["param_id"] = callspec.id if hasattr(callspec, "id") else None

            # Get all parameter values
            for param_name, param_value in callspec.params.items():
                # Convert complex objects to string representation
                if hasattr(param_value, "__dict__"):
                    # For custom objects, try to get a meaningful representation
                    value_repr = str(param_value)
                    if hasattr(param_value, "__class__"):
                        value_repr = f"{param_value.__class__.__name__}({value_repr})"
                else:
                    value_repr = repr(param_value)

                test_info["parameters"][param_name] = value_repr
                param_values[param_name].add(value_repr)
                param_usage[param_name] += 1

        test_data.append(test_info)

    # Generate output based on format
    output_dir = Path(dump_path).parent
    output_dir.mkdir(exist_ok=True)
    base_name = Path(dump_path).stem

    if dump_format in ["json", "all"]:
        # JSON format - detailed dump
        json_path = output_dir / f"{base_name}.json"
        with open(json_path, "w") as f:
            json.dump(
                {
                    "total_tests": len(test_data),
                    "total_parametrized": sum(1 for t in test_data if t["parameters"]),
                    "tests": test_data,
                    "unique_parameter_values": {
                        k: list(v) for k, v in param_values.items()
                    },
                    "parameter_usage_count": dict(param_usage),
                },
                f,
                indent=2,
            )
        print(f"Dumped detailed parameters to: {json_path}")

    if dump_format in ["csv", "all"]:
        # CSV format - flattened view
        csv_data = []
        for test in test_data:
            row = {
                "test_id": test["test_id"],
                "file": test["file"],
                "test_name": test["test_name"],
                "class": test["class"],
            }
            # Add each parameter as a column
            for param_name, param_value in test.get("parameters", {}).items():
                row[f"param_{param_name}"] = param_value
            csv_data.append(row)

        df = pd.DataFrame(csv_data)
        csv_path = output_dir / f"{base_name}.csv"
        df.to_csv(csv_path, index=False)
        print(f"Dumped parameters to CSV: {csv_path}")

    if dump_format in ["excel", "all"]:
        # Excel format with multiple sheets
        excel_path = output_dir / f"{base_name}.xlsx"
        with pd.ExcelWriter(excel_path) as writer:
            # Sheet 1: Test details
            test_df = (
                pd.DataFrame(csv_data)
                if "csv_data" in locals()
                else pd.DataFrame(
                    [
                        {
                            "test_id": t["test_id"],
                            "file": t["file"],
                            "test_name": t["test_name"],
                            "class": t["class"],
                            **{
                                f"param_{k}": v
                                for k, v in t.get("parameters", {}).items()
                            },
                        }
                        for t in test_data
                    ]
                )
            )
            test_df.to_excel(writer, sheet_name="Tests", index=False)

            # Sheet 2: Parameter summary
            param_summary = []
            for param, values in param_values.items():
                param_summary.append(
                    {
                        "parameter": param,
                        "unique_values": len(values),
                        "usage_count": param_usage[param],
                        "sample_values": ", ".join(list(values)[:5]),  # First 5 values
                    }
                )
            summary_df = pd.DataFrame(param_summary)
            summary_df.to_excel(writer, sheet_name="Parameter Summary", index=False)

            # Sheet 3: Coverage matrix
            if param_values:
                # Create a matrix of test vs parameters
                coverage_matrix = []
                for test in test_data:
                    if test["parameters"]:
                        row = {"test": test["test_name"]}
                        for param in param_values.keys():
                            row[param] = "✓" if param in test["parameters"] else ""
                        coverage_matrix.append(row)

                if coverage_matrix:
                    coverage_df = pd.DataFrame(coverage_matrix)
                    coverage_df.to_excel(
                        writer, sheet_name="Coverage Matrix", index=False
                    )

        print(f"Dumped parameters to Excel: {excel_path}")

    if generate_summary:
        # Generate summary report
        summary_path = output_dir / f"{base_name}_summary.txt"
        with open(summary_path, "w") as f:
            f.write("TEST PARAMETER SUMMARY\n")
            f.write("=" * 50 + "\n\n")

            f.write(f"Total tests collected: {len(test_data)}\n")
            f.write(
                f"Parametrized tests: {sum(1 for t in test_data if t['parameters'])}\n"
            )
            f.write(f"Unique parameters: {len(param_values)}\n\n")

            f.write("Parameter Usage:\n")
            f.write("-" * 30 + "\n")
            for param, count in sorted(
                param_usage.items(), key=lambda x: x[1], reverse=True
            ):
                f.write(
                    f"  {param}: {count} tests ({len(param_values[param])} unique values)\n"
                )

            f.write("\n\nParameter Value Samples:\n")
            f.write("-" * 30 + "\n")
            for param, values in sorted(param_values.items()):
                f.write(f"\n{param}:\n")
                for i, value in enumerate(sorted(values)[:10]):  # Show first 10 values
                    f.write(f"  - {value}\n")
                if len(values) > 10:
                    f.write(f"  ... and {len(values) - 10} more values\n")

        print(f"Generated summary report: {summary_path}")

    # Print quick summary to console
    print(f"\nParameter Collection Summary:")
    print(f"  Total tests: {len(test_data)}")
    print(f"  Parametrized tests: {sum(1 for t in test_data if t['parameters'])}")
    print(f"  Unique parameters: {len(param_values)}")
    print(f"\nTop 5 most used parameters:")
    for param, count in sorted(param_usage.items(), key=lambda x: x[1], reverse=True)[
        :5
    ]:
        print(f"    {param}: {count} tests")
