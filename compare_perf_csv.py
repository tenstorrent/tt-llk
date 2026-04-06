#!/usr/bin/env python3
# SPDX-FileCopyrightText: © 2026 Tenstorrent AI ULC
#
# SPDX-License-Identifier: Apache-2.0

"""
Compare two performance CSV files and generate a comparison report
"""
import argparse
import sys
from pathlib import Path

import pandas as pd


def process_csv_file(csv_path):
    df = pd.read_csv(csv_path)
    if df.empty:
        raise ValueError(f"CSV file is empty: {csv_path}")
    if "marker" not in df.columns:
        raise ValueError(f"CSV file missing required 'marker' column: {csv_path}")
    marker_idx = df.columns.get_loc("marker")
    sweep_names = list(df.columns[:marker_idx])
    stat_names = list(df.columns[marker_idx + 1 :])
    return df, sweep_names, stat_names


def compare_csv_files(baseline_path, comparison_path, output_path=None):
    baseline_path = Path(baseline_path)
    comparison_path = Path(comparison_path)
    print(f"Baseline:   {baseline_path}")
    print(f"Comparison: {comparison_path}")
    df_baseline, sweep_baseline, stats_baseline = process_csv_file(baseline_path)
    df_comp, sweep_comp, stats_comp = process_csv_file(comparison_path)
    print(
        f"\nTotal rows:\n  Baseline:   {len(df_baseline)}\n  Comparison: {len(df_comp)}"
    )
    sweep_comp_set = set(sweep_comp)
    common_sweeps = [s for s in sweep_baseline if s in sweep_comp_set]
    if not common_sweeps:
        raise ValueError("No common sweep parameters found")
    if "marker" not in common_sweeps:
        common_sweeps.append("marker")
    stats_baseline_mean = [s for s in stats_baseline if s.startswith("mean(")]
    stats_comp_mean_set = set(s for s in stats_comp if s.startswith("mean("))
    common_stats = [s for s in stats_baseline_mean if s in stats_comp_mean_set]
    if not common_stats:
        raise ValueError("No common statistics found")
    merge_columns = common_sweeps.copy()
    for col in merge_columns:
        if col in df_baseline.columns:
            df_baseline[col] = df_baseline[col].astype(str)
        if col in df_comp.columns:
            df_comp[col] = df_comp[col].astype(str)
    df_merged = df_baseline[merge_columns + common_stats].merge(
        df_comp[merge_columns + common_stats],
        on=merge_columns,
        how="inner",
        suffixes=("_baseline", "_comparison"),
    )
    print(f"\nMatched rows: {len(df_merged)}")
    for stat in common_stats:
        df_merged[f"{stat}_diff"] = (
            df_merged[f"{stat}_comparison"] - df_merged[f"{stat}_baseline"]
        )
        with pd.option_context("mode.use_inf_as_na", True):
            df_merged[f"{stat}_pct_change"] = (
                (df_merged[f"{stat}_baseline"] - df_merged[f"{stat}_comparison"])
                / df_merged[f"{stat}_baseline"]
                * 100
            )
            df_merged[f"{stat}_pct_change"] = df_merged[f"{stat}_pct_change"].replace(
                [float("inf"), float("-inf")], float("nan")
            )
    ordered_columns = [col for col in merge_columns if col != "marker"] + ["marker"]
    for stat in common_stats:
        ordered_columns.extend(
            [
                f"{stat}_baseline",
                f"{stat}_comparison",
                f"{stat}_diff",
                f"{stat}_pct_change",
            ]
        )
    df_result = df_merged[ordered_columns]
    print("\n" + "=" * 80)
    print("SUMMARY STATISTICS")
    print("=" * 80)
    for marker in df_result["marker"].unique():
        md = df_result[df_result["marker"] == marker]
        print(f"\n{'=' * 80}\nMarker: {marker} ({len(md)} rows)\n{'=' * 80}")
        for stat in common_stats:
            dc = f"{stat}_diff"
            pc = f"{stat}_pct_change"
            print(f"\n{stat}:")
            print(f"  Mean difference:      {md[dc].mean():>10.4f}")
            print(f"  Median difference:    {md[dc].median():>10.4f}")
            print(
                f"  Mean improvement:     {md[pc].mean():>10.2f}% (positive = better)"
            )
    if output_path:
        output_path = Path(output_path)
        output_path.parent.mkdir(parents=True, exist_ok=True)
    else:
        output_path = (
            baseline_path.parent
            / f"{baseline_path.stem}_vs_{comparison_path.stem}_comparison.csv"
        )
    df_result.to_csv(output_path, index=False)
    print(f"\n✓ Comparison saved to: {output_path}")
    return df_result


def main():
    parser = argparse.ArgumentParser(description="Compare two performance CSV files")
    parser.add_argument("baseline", help="Path to baseline CSV file")
    parser.add_argument("comparison", help="Path to comparison CSV file")
    parser.add_argument("-o", "--output", default=None, help="Output CSV file path")
    args = parser.parse_args()
    print("=" * 80)
    print("Performance CSV Comparison Tool")
    print("=" * 80 + "\n")
    try:
        df_result = compare_csv_files(args.baseline, args.comparison, args.output)
        print(f"\nComparison complete! Total rows compared: {len(df_result)}")
        sys.exit(0)
    except Exception as e:
        print(f"\n✗ Error: {e}", file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()
