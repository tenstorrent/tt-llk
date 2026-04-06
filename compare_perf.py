#!/usr/bin/env python3
# SPDX-FileCopyrightText: © 2026 Tenstorrent AI ULC
#
# SPDX-License-Identifier: Apache-2.0

"""Compare perf data across 3 runs: main, branch (no counters), branch (with counters).

Produces per-test and aggregate comparison CSVs showing mean/std diffs and improvement %.

Usage:
    python compare_perf.py [--main DIR] [--branch DIR] [--counters DIR] [--output DIR]
"""
import argparse
import os

import pandas as pd

TESTS = [
    "perf_eltwise_binary_fpu",
    "perf_unpack_tilize",
    "perf_pack_untilize",
    "perf_unpack_untilize",
    "perf_matmul",
]

METRICS = [
    "mean(L1_TO_L1)",
    "mean(UNPACK_ISOLATE)",
    "mean(MATH_ISOLATE)",
    "mean(PACK_ISOLATE)",
    "mean(L1_CONGESTION[UNPACK])",
    "mean(L1_CONGESTION[PACK])",
]


def load_post_csv(directory: str, test_name: str) -> pd.DataFrame | None:
    path = os.path.join(directory, f"{test_name}.post.csv")
    if not os.path.exists(path):
        print(f"  WARNING: {path} not found, skipping")
        return None
    return pd.read_csv(path)


def get_param_cols(df: pd.DataFrame) -> list[str]:
    return [
        c for c in df.columns
        if not c.startswith("mean(") and not c.startswith("std(")
        and "_mean(" not in c and "_std(" not in c
        and not c.startswith("TEXT_SIZE")
    ]


def compare_pair(
    baseline: pd.DataFrame,
    comparison: pd.DataFrame,
    label: str,
) -> pd.DataFrame:
    """Compare two runs, return per-marker summary stats."""
    param_cols = [c for c in get_param_cols(baseline) if c in comparison.columns]
    merged = baseline.merge(comparison, on=param_cols, suffixes=("_base", "_cmp"))

    if len(merged) == 0:
        print(f"  WARNING: 0 rows merged for {label}")
        return pd.DataFrame()

    rows = []
    for marker in sorted(merged["marker"].unique()):
        m = merged[merged["marker"] == marker]
        for metric in METRICS:
            base_col = f"{metric}_base"
            cmp_col = f"{metric}_cmp"
            if base_col not in merged.columns or cmp_col not in merged.columns:
                continue

            diff = m[cmp_col] - m[base_col]
            base_mean = m[base_col].mean()

            rows.append(
                {
                    "comparison": label,
                    "marker": marker,
                    "metric": metric,
                    "n_configs": len(m),
                    "baseline_mean": round(base_mean, 2),
                    "comparison_mean": round(m[cmp_col].mean(), 2),
                    "mean_diff": round(diff.mean(), 2),
                    "median_diff": round(diff.median(), 2),
                    "std_diff": round(diff.std(), 2),
                    "min_diff": round(diff.min(), 2),
                    "max_diff": round(diff.max(), 2),
                    "p5_diff": round(diff.quantile(0.05), 2),
                    "p95_diff": round(diff.quantile(0.95), 2),
                    "improvement_pct": round(
                        (diff.mean() / base_mean * -100) if base_mean != 0 else 0, 2
                    ),
                }
            )

    return pd.DataFrame(rows)


def print_summary(df: pd.DataFrame, title: str):
    """Print a human-readable summary table."""
    if df.empty:
        return
    print(f"\n{'=' * 100}")
    print(f"  {title}")
    print(f"{'=' * 100}")

    for marker in sorted(df["marker"].unique()):
        m = df[df["marker"] == marker]
        print(f"\n  --- {marker} ---")
        print(
            f"  {'Metric':<30} {'Mean Diff':>10} {'Median':>10} "
            f"{'P5':>10} {'P95':>10} {'Imp%':>10}"
        )
        print(f"  {'-' * 30} {'-' * 10} {'-' * 10} {'-' * 10} {'-' * 10} {'-' * 10}")
        for _, r in m.iterrows():
            print(
                f"  {r['metric']:<30} {r['mean_diff']:>10.2f} {r['median_diff']:>10.2f} "
                f"{r['p5_diff']:>10.2f} {r['p95_diff']:>10.2f} {r['improvement_pct']:>+10.2f}%"
            )


def main():
    parser = argparse.ArgumentParser(description="Compare perf data across runs")
    parser.add_argument(
        "--main",
        default="/proj_sw/user_dev/mvlahovic/main_perf_data",
        help="Directory with main branch perf data",
    )
    parser.add_argument(
        "--branch",
        default="/proj_sw/user_dev/mvlahovic/perf_data",
        help="Directory with branch (no counters) perf data",
    )
    parser.add_argument(
        "--counters",
        default="/proj_sw/user_dev/mvlahovic/perf_counters_data",
        help="Directory with branch (with counters) perf data",
    )
    parser.add_argument(
        "--output",
        default="/proj_sw/user_dev/mvlahovic",
        help="Directory to write comparison CSVs",
    )
    args = parser.parse_args()

    comparisons = [
        (
            "main_vs_branch",
            args.main,
            args.branch,
            "Main vs Branch (compilation overhead)",
        ),
        (
            "branch_vs_counters",
            args.branch,
            args.counters,
            "Branch vs Counters (counter overhead)",
        ),
        (
            "main_vs_counters",
            args.main,
            args.counters,
            "Main vs Counters (total overhead)",
        ),
    ]

    for comp_name, base_dir, cmp_dir, title in comparisons:
        print(f"\n{'#' * 100}")
        print(f"# {title}")
        print(f"# Baseline: {base_dir}")
        print(f"# Comparison: {cmp_dir}")
        print(f"{'#' * 100}")

        all_results = []

        for test in TESTS:
            print(f"\n  Loading {test}...")
            base_df = load_post_csv(base_dir, test)
            cmp_df = load_post_csv(cmp_dir, test)

            if base_df is None or cmp_df is None:
                continue

            result = compare_pair(base_df, cmp_df, f"{comp_name}/{test}")
            if not result.empty:
                result.insert(0, "test", test)
                all_results.append(result)
                print_summary(result, f"{title} — {test}")

        if all_results:
            combined = pd.concat(all_results, ignore_index=True)

            # Per-test CSV
            out_path = os.path.join(args.output, f"{comp_name}.csv")
            combined.to_csv(out_path, index=False)
            print(f"\n  Saved: {out_path}")

            # Aggregate across all tests
            agg = (
                combined.groupby(["comparison", "marker", "metric"])
                .agg(
                    n_configs=("n_configs", "sum"),
                    baseline_mean=("baseline_mean", "mean"),
                    comparison_mean=("comparison_mean", "mean"),
                    mean_diff=("mean_diff", "mean"),
                    median_diff=("median_diff", "mean"),
                )
                .reset_index()
            )
            agg["improvement_pct"] = round(
                agg["mean_diff"] / agg["baseline_mean"] * -100, 2
            )
            agg_path = os.path.join(args.output, f"{comp_name}_aggregate.csv")
            agg.to_csv(agg_path, index=False)
            print(f"  Saved: {agg_path}")

            print_summary(
                agg.rename(
                    columns={
                        "mean_diff": "mean_diff",
                        "median_diff": "median_diff",
                    }
                ).assign(p5_diff=0, p95_diff=0),
                f"AGGREGATE: {title}",
            )

    print("\n\nDone! Comparison CSVs written to:", args.output)


if __name__ == "__main__":
    main()
