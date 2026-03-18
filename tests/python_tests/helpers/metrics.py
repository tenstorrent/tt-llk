# SPDX-FileCopyrightText: © 2026 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0

"""
Performance metrics calculation from hardware counter data.

Metrics calculated:
- Unpacker Write Efficiency: Measures how efficiently unpackers write data
  * SRCA_WRITE / UNPACK0_BUSY_THREAD0: Fraction of unpacker0 busy cycles spent writing data
  * SRCB_WRITE / UNPACK1_BUSY_THREAD0: Fraction of unpacker1 busy cycles spent writing data
  * Higher ratio (closer to 1.0) = more efficient, less stalling
  * Lower ratio = unpacker busy but not writing (stalled on dependencies)

- Packer Efficiency: Measures how efficiently packer uses destination data
  * PACKER_DEST_READ_AVAILABLE / PACKER_BUSY: Fraction of packer busy time with valid dest data
  * Higher ratio (closer to 1.0) = packer has data available, less dest stalling
  * Lower ratio = packer busy but waiting for destination to become valid
  * Only valid when using HW dvalid-based synchronization (not STALLWAIT)

- FPU Execution Efficiency: Measures how efficiently FPU executes available instructions
  * FPU_INSTRUCTION / FPU_INSTRN_AVAILABLE_1: Fraction of cycles with FPU work available where FPU executes
  * Higher ratio (closer to 1.0) = FPU executes whenever work is available (efficient)
  * Lower ratio = FPU instructions available but not executing (stalled in pipeline)

- Math Pipeline Utilization (EXPERIMENTAL): Measures math pipeline instruction flow efficiency
  * MATH_INSTRN_STARTED / MATH_INSTRN_AVAILABLE: Fraction of available math instructions that start execution
  * Higher ratio (closer to 1.0) = math pipeline efficiently moves instructions (no pipe stalls)
  * Lower ratio = instructions available in pipe but not starting (pipeline stalled)

- Math-to-Pack Handoff Efficiency (EXPERIMENTAL): Measures pipeline balance between math and pack stages
  * AVAILABLE_MATH / PACKER_BUSY: Fraction of packer busy time where math has output ready
  * Higher ratio (closer to 1.0) = math keeps up with packer, good pipeline balance
  * Lower ratio = packer busy but waiting for math output (math is bottleneck)

- Unpacker-to-Math Data Flow (EXPERIMENTAL): Measures unpacker write availability during busy cycles
  * SRCA_WRITE_AVAILABLE / UNPACK0_BUSY_THREAD0: Fraction of unpacker0 busy time with srcA buffer space
  * SRCB_WRITE_AVAILABLE / UNPACK1_BUSY_THREAD0: Fraction of unpacker1 busy time with srcB buffer space
  * Higher ratio (closer to 1.0) = unpacker can write when busy, no backpressure from math
  * Lower ratio = unpacker busy but srcA/B full, stalled by math not consuming data fast enough
"""

import pandas as pd

# ── Helpers ──────────────────────────────────────────────────────────


def _avg_count(df: pd.DataFrame, bank: str, counter_name: str) -> float:
    """Average count for a specific counter across all threads."""
    mask = (df["bank"] == bank) & (df["counter_name"] == counter_name)
    result = df.loc[mask, "count"]
    return float(result.mean()) if len(result) > 0 else 0.0


def _safe_div(numerator: float, denominator: float) -> float | None:
    """Safe division returning None if denominator is 0."""
    return (numerator / denominator) if denominator > 0 else None


def _pct(value: float | None) -> float | None:
    """Convert ratio to percentage."""
    return (value * 100.0) if value is not None else None


# ── Compute ──────────────────────────────────────────────────────────


def _compute_single(df: pd.DataFrame) -> dict:
    """
    Compute derived efficiency metrics from a single (zone, run) slice of counter data.

    Returns a flat dict of raw counts, efficiency ratios, and percentages.
    """
    if df.empty:
        return {}

    # Unpacker Write Efficiency
    srca_write = _avg_count(df, "TDMA_UNPACK", "SRCA_WRITE")
    srcb_write = _avg_count(df, "TDMA_UNPACK", "SRCB_WRITE")
    unpack0_busy = _avg_count(df, "TDMA_UNPACK", "UNPACK0_BUSY_THREAD0")
    unpack1_busy = _avg_count(df, "TDMA_UNPACK", "UNPACK1_BUSY_THREAD0")
    unpack0_efficiency = _safe_div(srca_write, unpack0_busy)
    unpack1_efficiency = _safe_div(srcb_write, unpack1_busy)

    if unpack0_efficiency is not None and unpack1_efficiency is not None:
        unpack_efficiency = (unpack0_efficiency + unpack1_efficiency) / 2.0
    elif unpack0_efficiency is not None:
        unpack_efficiency = unpack0_efficiency
    elif unpack1_efficiency is not None:
        unpack_efficiency = unpack1_efficiency
    else:
        unpack_efficiency = None

    # Packer Efficiency
    packer_dest_available = _avg_count(df, "TDMA_PACK", "PACKER_DEST_READ_AVAILABLE")
    packer_busy = _avg_count(df, "TDMA_PACK", "PACKER_BUSY")
    pack_efficiency = _safe_div(packer_dest_available, packer_busy)

    # FPU Execution Efficiency
    fpu_instruction = _avg_count(df, "FPU", "FPU_INSTRUCTION")
    fpu_instrn_available = _avg_count(df, "INSTRN_THREAD", "FPU_INSTRN_AVAILABLE_1")
    fpu_efficiency = _safe_div(fpu_instruction, fpu_instrn_available)

    # Math Pipeline Utilization (EXPERIMENTAL)
    math_instrn_started = _avg_count(df, "TDMA_UNPACK", "MATH_INSTRN_STARTED")
    math_instrn_available = _avg_count(df, "TDMA_UNPACK", "MATH_INSTRN_AVAILABLE")
    math_pipeline_util = _safe_div(math_instrn_started, math_instrn_available)

    # Math-to-Pack Handoff Efficiency (EXPERIMENTAL)
    available_math = _avg_count(df, "TDMA_PACK", "AVAILABLE_MATH")
    math_to_pack_efficiency = _safe_div(available_math, packer_busy)

    # Unpacker-to-Math Data Flow (EXPERIMENTAL)
    srca_write_available = _avg_count(df, "TDMA_UNPACK", "SRCA_WRITE_AVAILABLE")
    srcb_write_available = _avg_count(df, "TDMA_UNPACK", "SRCB_WRITE_AVAILABLE")
    unpack_to_math_flow0 = _safe_div(srca_write_available, unpack0_busy)
    unpack_to_math_flow1 = _safe_div(srcb_write_available, unpack1_busy)

    if unpack_to_math_flow0 is not None and unpack_to_math_flow1 is not None:
        unpack_to_math_flow = (unpack_to_math_flow0 + unpack_to_math_flow1) / 2.0
    elif unpack_to_math_flow0 is not None:
        unpack_to_math_flow = unpack_to_math_flow0
    elif unpack_to_math_flow1 is not None:
        unpack_to_math_flow = unpack_to_math_flow1
    else:
        unpack_to_math_flow = None

    return {
        # Raw counts
        "srca_write_count": srca_write,
        "srcb_write_count": srcb_write,
        "unpack0_busy_count": unpack0_busy,
        "unpack1_busy_count": unpack1_busy,
        "packer_dest_available_count": packer_dest_available,
        "packer_busy_count": packer_busy,
        "fpu_instruction_count": fpu_instruction,
        "fpu_instrn_available_count": fpu_instrn_available,
        "math_instrn_started_count": math_instrn_started,
        "math_instrn_available_count": math_instrn_available,
        "available_math_count": available_math,
        "srca_write_available_count": srca_write_available,
        "srcb_write_available_count": srcb_write_available,
        # Efficiency ratios (0.0 - 1.0)
        "unpack0_efficiency": unpack0_efficiency,
        "unpack1_efficiency": unpack1_efficiency,
        "unpack_efficiency": unpack_efficiency,
        "pack_efficiency": pack_efficiency,
        "fpu_efficiency": fpu_efficiency,
        "math_pipeline_util": math_pipeline_util,
        "math_to_pack_efficiency": math_to_pack_efficiency,
        "unpack_to_math_flow0": unpack_to_math_flow0,
        "unpack_to_math_flow1": unpack_to_math_flow1,
        "unpack_to_math_flow": unpack_to_math_flow,
        # Efficiency percentages (0.0 - 100.0)
        "unpack0_efficiency_pct": _pct(unpack0_efficiency),
        "unpack1_efficiency_pct": _pct(unpack1_efficiency),
        "unpack_efficiency_pct": _pct(unpack_efficiency),
        "pack_efficiency_pct": _pct(pack_efficiency),
        "fpu_efficiency_pct": _pct(fpu_efficiency),
        "math_pipeline_util_pct": _pct(math_pipeline_util),
        "math_to_pack_efficiency_pct": _pct(math_to_pack_efficiency),
        "unpack_to_math_flow0_pct": _pct(unpack_to_math_flow0),
        "unpack_to_math_flow1_pct": _pct(unpack_to_math_flow1),
        "unpack_to_math_flow_pct": _pct(unpack_to_math_flow),
    }


def compute_metrics(df: pd.DataFrame) -> list[dict]:
    """
    Compute derived metrics for each (zone, run_index) combination.

    Works for all cases:
    - Functional tests: 1 zone, 1 run → 1 result
    - Perf tests: 2 zones, 2 runs → 4 results

    Args:
        df: Raw counter DataFrame from read_counters(), optionally with
            'zone' and 'run_index' columns.

    Returns:
        List of dicts, each containing zone, run_index, and all computed metrics.
    """
    if df.empty:
        return []

    zones = sorted(df["zone"].unique()) if "zone" in df.columns else ["ZONE_0"]
    has_runs = "run_index" in df.columns

    results = []
    for zone in zones:
        zone_df = df[df["zone"] == zone] if "zone" in df.columns else df
        runs = sorted(zone_df["run_index"].unique()) if has_runs else [0]

        for run_idx in runs:
            run_df = zone_df[zone_df["run_index"] == run_idx] if has_runs else zone_df
            metrics = _compute_single(run_df)
            if metrics:
                metrics["zone"] = zone
                metrics["run_index"] = run_idx
                results.append(metrics)

    return results


# ── Export ────────────────────────────────────────────────────────────


def export_metrics(
    computed: list[dict],
    run_type_name: str,
    zone_names: list[str] | None = None,
) -> pd.DataFrame:
    """
    Aggregate computed metrics per zone and return a DataFrame for CSV export.

    For multiple runs: exports mean/std per metric.
    For single run: exports raw values.

    Args:
        computed: Output of compute_metrics().
        run_type_name: Run type prefix for column names (e.g., "L1_TO_L1").
        zone_names: Optional list mapping zone index to display name.
                    e.g., ["INIT", "TILE_LOOP"] maps ZONE_0→INIT, ZONE_1→TILE_LOOP.

    Returns:
        DataFrame with one row per zone, columns prefixed with run_type_name.
    """
    if not computed:
        return pd.DataFrame()

    zone_to_marker = {}
    if zone_names:
        for i, name in enumerate(zone_names):
            zone_to_marker[f"ZONE_{i}"] = name

    zones = sorted(set(m["zone"] for m in computed))
    skip_keys = {"zone", "run_index"}
    rows = []

    for zone in zones:
        zone_metrics = [m for m in computed if m["zone"] == zone]
        marker_name = zone_to_marker.get(zone, zone)
        row = {"marker": marker_name}

        if len(zone_metrics) >= 2:
            metrics_df = pd.DataFrame(zone_metrics)
            for col in metrics_df.columns:
                if col in skip_keys:
                    continue
                values = metrics_df[col].dropna()
                if len(values) >= 2:
                    row[f"{run_type_name}_mean({col})"] = float(values.mean())
                    row[f"{run_type_name}_std({col})"] = float(values.std())
        else:
            for k, v in zone_metrics[0].items():
                if k in skip_keys:
                    continue
                row[f"{run_type_name}_{k}"] = v

        rows.append(row)

    return pd.DataFrame(rows)


# ── Print ────────────────────────────────────────────────────────────


def _print_detail(metrics: dict) -> None:
    """Print detailed efficiency metrics for a single (zone, run) result."""

    def fmt(value, decimals=2):
        if value is None:
            return "N/A"
        return f"{value:.{decimals}f}"

    print(f"\n{'─' * 70}")
    print("  UNPACKER WRITE EFFICIENCY")
    print(f"{'─' * 70}")
    print("  Measures the fraction of unpacker busy cycles spent writing data.")
    print("  Higher ratio (→1.0) = efficient, unpacker writes when busy")
    print("  Lower ratio (→0.0) = inefficient, unpacker busy but stalled/waiting")
    print(f"{'─' * 70}")
    print(f"  {'Metric':<30} {'Writes':>12} {'Busy':>12} {'Efficiency':>12}")
    print(f"  {'─' * 30} {'─' * 12} {'─' * 12} {'─' * 12}")
    print(
        f"  {'Unpacker0 (SRCA):':<30} {metrics['srca_write_count']:>12.1f} {metrics['unpack0_busy_count']:>12.1f} {fmt(metrics['unpack0_efficiency']):>12}"
    )
    print(
        f"  {'Unpacker1 (SRCB):':<30} {metrics['srcb_write_count']:>12.1f} {metrics['unpack1_busy_count']:>12.1f} {fmt(metrics['unpack1_efficiency']):>12}"
    )
    print(
        f"  {'Combined Average:':<30} {'':<12} {'':<12} {fmt(metrics['unpack_efficiency']):>12}"
    )

    print(f"\n{'─' * 70}")
    print("  PACKER EFFICIENCY")
    print(f"{'─' * 70}")
    print("  Measures the fraction of packer busy cycles with valid dest data.")
    print("  Higher ratio (→1.0) = efficient, packer has data when busy")
    print("  Lower ratio (→0.0) = inefficient, packer stalled waiting for dest")
    print("  NOTE: Only valid with HW dvalid sync (not STALLWAIT)")
    print(f"{'─' * 70}")
    print(f"  {'Metric':<30} {'Dest Avail':>12} {'Busy':>12} {'Efficiency':>12}")
    print(f"  {'─' * 30} {'─' * 12} {'─' * 12} {'─' * 12}")
    print(
        f"  {'Packer:':<30} {metrics['packer_dest_available_count']:>12.1f} {metrics['packer_busy_count']:>12.1f} {fmt(metrics['pack_efficiency']):>12}"
    )

    print(f"\n{'─' * 70}")
    print("  FPU EXECUTION EFFICIENCY")
    print(f"{'─' * 70}")
    print("  Measures the fraction of FPU-available cycles where FPU executes.")
    print("  Higher ratio (→1.0) = efficient, FPU executes when work available")
    print("  Lower ratio (→0.0) = inefficient, FPU stalled despite available work")
    print(f"{'─' * 70}")
    print(f"  {'Metric':<30} {'FPU Instr':>12} {'Available':>12} {'Efficiency':>12}")
    print(f"  {'─' * 30} {'─' * 12} {'─' * 12} {'─' * 12}")
    print(
        f"  {'Math FPU:':<30} {metrics['fpu_instruction_count']:>12.1f} {metrics['fpu_instrn_available_count']:>12.1f} {fmt(metrics['fpu_efficiency']):>12}"
    )

    print(f"\n{'─' * 70}")
    print("  MATH PIPELINE UTILIZATION (EXPERIMENTAL)")
    print(f"{'─' * 70}")
    print("  Measures math pipeline instruction flow efficiency.")
    print("  Higher ratio (→1.0) = efficient, pipeline moves instructions smoothly")
    print("  Lower ratio (→0.0) = inefficient, pipeline stalls despite available work")
    print(f"{'─' * 70}")
    print(f"  {'Metric':<30} {'Started':>12} {'Available':>12} {'Utilization':>12}")
    print(f"  {'─' * 30} {'─' * 12} {'─' * 12} {'─' * 12}")
    print(
        f"  {'Math Pipeline:':<30} {metrics['math_instrn_started_count']:>12.1f} {metrics['math_instrn_available_count']:>12.1f} {fmt(metrics['math_pipeline_util']):>12}"
    )

    print(f"\n{'─' * 70}")
    print("  MATH-TO-PACK HANDOFF EFFICIENCY (EXPERIMENTAL)")
    print(f"{'─' * 70}")
    print("  Measures pipeline balance between math and pack stages.")
    print("  Higher ratio (→1.0) = efficient, math keeps up with packer demand")
    print("  Lower ratio (→0.0) = inefficient, packer waits for math output")
    print(f"{'─' * 70}")
    print(f"  {'Metric':<30} {'Math Avail':>12} {'Pack Busy':>12} {'Efficiency':>12}")
    print(f"  {'─' * 30} {'─' * 12} {'─' * 12} {'─' * 12}")
    print(
        f"  {'Math→Pack:':<30} {metrics['available_math_count']:>12.1f} {metrics['packer_busy_count']:>12.1f} {fmt(metrics['math_to_pack_efficiency']):>12}"
    )

    print(f"\n{'─' * 70}")
    print("  UNPACKER-TO-MATH DATA FLOW (EXPERIMENTAL)")
    print(f"{'─' * 70}")
    print("  Measures unpacker write availability during busy cycles.")
    print("  Higher ratio (→1.0) = efficient, unpacker can write (no backpressure)")
    print("  Lower ratio (→0.0) = inefficient, buffers full (math not consuming)")
    print(f"{'─' * 70}")
    print(f"  {'Metric':<30} {'Buf Avail':>12} {'Busy':>12} {'Data Flow':>12}")
    print(f"  {'─' * 30} {'─' * 12} {'─' * 12} {'─' * 12}")
    print(
        f"  {'Unpacker0→Math (srcA):':<30} {metrics['srca_write_available_count']:>12.1f} {metrics['unpack0_busy_count']:>12.1f} {fmt(metrics['unpack_to_math_flow0']):>12}"
    )
    print(
        f"  {'Unpacker1→Math (srcB):':<30} {metrics['srcb_write_available_count']:>12.1f} {metrics['unpack1_busy_count']:>12.1f} {fmt(metrics['unpack_to_math_flow1']):>12}"
    )
    print(
        f"  {'Combined Average:':<30} {'':<12} {'':<12} {fmt(metrics['unpack_to_math_flow']):>12}"
    )


def _print_stability(zone_metrics: list[dict]) -> None:
    """Print mean/std summary for multiple runs of the same zone."""
    if len(zone_metrics) < 2:
        return

    metrics_df = pd.DataFrame(zone_metrics)
    skip_keys = {"zone", "run_index"}

    key_counters = [
        ("srca_write_count", "SRCA_WRITE"),
        ("srcb_write_count", "SRCB_WRITE"),
        ("unpack0_busy_count", "UNPACK0_BUSY"),
        ("unpack1_busy_count", "UNPACK1_BUSY"),
        ("packer_busy_count", "PACKER_BUSY"),
        ("packer_dest_available_count", "PACKER_DEST_AVAIL"),
        ("fpu_instruction_count", "FPU_INSTRUCTION"),
        ("fpu_instrn_available_count", "FPU_INSTRN_AVAIL"),
    ]

    key_efficiencies = [
        ("unpack0_efficiency", "Unpack0 Efficiency"),
        ("unpack1_efficiency", "Unpack1 Efficiency"),
        ("pack_efficiency", "Pack Efficiency"),
        ("fpu_efficiency", "FPU Efficiency"),
    ]

    print(f"\n  STABILITY ACROSS {len(zone_metrics)} RUNS (mean +/- std)")
    print(f"  {'─' * 66}")
    print(f"  {'Counter':<25} {'Mean':>12} {'Std':>12} {'Std/Mean':>12}")
    print(f"  {'─' * 25} {'─' * 12} {'─' * 12} {'─' * 12}")

    for key, label in key_counters:
        if key in metrics_df.columns:
            values = metrics_df[key].dropna()
            if len(values) >= 2:
                mean_val = float(values.mean())
                std_val = float(values.std())
                cv = std_val / mean_val if mean_val > 0 else 0.0
                print(f"  {label:<25} {mean_val:>12.1f} {std_val:>12.1f} {cv:>11.1%}")

    print(f"\n  {'Efficiency':<25} {'Mean':>12} {'Std':>12}")
    print(f"  {'─' * 25} {'─' * 12} {'─' * 12}")

    for key, label in key_efficiencies:
        if key in metrics_df.columns:
            values = metrics_df[key].dropna()
            if len(values) >= 2:
                print(
                    f"  {label:<25} {float(values.mean()):>12.4f} {float(values.std()):>12.4f}"
                )


def print_metrics(df_or_computed) -> None:
    """
    Print performance metrics to console, grouped by zone.
    If multiple runs, also prints mean/std stability summary per zone.

    Accepts either:
    - A raw counter DataFrame (computes metrics automatically)
    - A list of dicts from compute_metrics()
    """
    if isinstance(df_or_computed, pd.DataFrame):
        computed = compute_metrics(df_or_computed)
    else:
        computed = df_or_computed

    if not computed:
        print("No metrics to display.")
        return

    print("\n" + "=" * 70)
    print("PERFORMANCE METRICS")
    print("=" * 70)

    zones = sorted(set(m["zone"] for m in computed))

    for i, zone in enumerate(zones):
        if i > 0:
            print()

        zone_metrics = [m for m in computed if m["zone"] == zone]

        print("\n" + "═" * 70)
        print(f"ZONE: {zone}")
        print("═" * 70)

        # Print detailed metrics for the first run (representative)
        _print_detail(zone_metrics[0])

        # Print stability summary if multiple runs
        _print_stability(zone_metrics)
