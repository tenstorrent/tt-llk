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
"""

import pandas as pd
from helpers.test_config import TestConfig


def _sum_count(df: pd.DataFrame, bank: str, counter_name: str) -> int:
    """Sum count for a specific counter across all threads."""
    mask = (df["bank"] == bank) & (df["counter_name"] == counter_name)
    return int(df.loc[mask, "count"].sum())


def _avg_count(df: pd.DataFrame, bank: str, counter_name: str) -> float:
    """Average count for a specific counter across all threads."""
    mask = (df["bank"] == bank) & (df["counter_name"] == counter_name)
    result = df.loc[mask, "count"]
    return float(result.mean()) if len(result) > 0 else 0.0


def _max_cycles(df: pd.DataFrame, bank: str) -> int:
    """Get max cycles for a specific bank across all threads."""
    bank_df = df[df["bank"] == bank]
    if bank_df.empty:
        return 0
    return int(bank_df["cycles"].max())


def _safe_div(numerator: float, denominator: float) -> float | None:
    """Safe division returning None if denominator is 0."""
    return (numerator / denominator) if denominator > 0 else None


def _pct(value: float | None) -> float | None:
    """Convert ratio to percentage."""
    return (value * 100.0) if value is not None else None


def compute_metrics(df: pd.DataFrame) -> dict:
    """
    Compute performance metrics from counter DataFrame.

    Args:
        df: DataFrame from read_counters() with columns:
            thread, bank, counter_name, counter_id, cycles, count, l1_mux

    Returns:
        Dictionary of computed metrics.
    """
    if df.empty:
        return {}

    # === Unpacker Write Efficiency ===
    # Ratio of write cycles to busy cycles - measures how efficiently unpacker uses busy time
    # SRCA_WRITE: Cycles where unpacker0 wrote data to srcA
    # UNPACK0_BUSY: Cycles where unpacker0 was busy processing thread 0 instructions
    # Higher ratio (closer to 1.0) means unpacker spends most busy time writing (efficient)
    # Lower ratio means unpacker is busy but not writing (stalled/waiting)
    srca_write = _avg_count(df, "TDMA_UNPACK", "SRCA_WRITE")
    srcb_write = _avg_count(df, "TDMA_UNPACK", "SRCB_WRITE")
    unpack0_busy = _avg_count(df, "TDMA_UNPACK", "UNPACK0_BUSY_THREAD0")
    unpack1_busy = _avg_count(df, "TDMA_UNPACK", "UNPACK1_BUSY_THREAD0")

    unpack0_efficiency = _safe_div(srca_write, unpack0_busy)
    unpack1_efficiency = _safe_div(srcb_write, unpack1_busy)

    # Combined unpacker efficiency (average of both)
    if unpack0_efficiency is not None and unpack1_efficiency is not None:
        unpack_efficiency = (unpack0_efficiency + unpack1_efficiency) / 2.0
    elif unpack0_efficiency is not None:
        unpack_efficiency = unpack0_efficiency
    elif unpack1_efficiency is not None:
        unpack_efficiency = unpack1_efficiency
    else:
        unpack_efficiency = None

    # === Packer Efficiency ===
    # Ratio of destination-available time to packer busy time
    # PACKER_DEST_READ_AVAILABLE: Cycles where packer could read from destination (data valid)
    # PACKER_BUSY: Cycles where packer pipeline has active instructions
    # Higher ratio (closer to 1.0) means packer has data available when busy (efficient)
    # Lower ratio means packer is busy but waiting for destination data (dest stalling)
    # NOTE: Only valid with HW dvalid synchronization, not with STALLWAIT
    packer_dest_available = _avg_count(df, "TDMA_PACK", "PACKER_DEST_READ_AVAILABLE")
    packer_busy = _avg_count(df, "TDMA_PACK", "PACKER_BUSY")
    pack_efficiency = _safe_div(packer_dest_available, packer_busy)

    return {
        # Raw counts
        "srca_write_count": srca_write,
        "srcb_write_count": srcb_write,
        "unpack0_busy_count": unpack0_busy,
        "unpack1_busy_count": unpack1_busy,
        "packer_dest_available_count": packer_dest_available,
        "packer_busy_count": packer_busy,
        # Efficiency ratios (0.0 - 1.0)
        "unpack0_efficiency": unpack0_efficiency,
        "unpack1_efficiency": unpack1_efficiency,
        "unpack_efficiency": unpack_efficiency,
        "pack_efficiency": pack_efficiency,
        # Efficiency percentages (0.0 - 100.0)
        "unpack0_efficiency_pct": _pct(unpack0_efficiency),
        "unpack1_efficiency_pct": _pct(unpack1_efficiency),
        "unpack_efficiency_pct": _pct(unpack_efficiency),
        "pack_efficiency_pct": _pct(pack_efficiency),
    }


def print_metrics(results: pd.DataFrame) -> None:
    """Print performance metrics to console."""
    metrics = compute_metrics(results)

    if not metrics:
        print("No metrics to display.")
        return

    def fmt(value, decimals=2):
        """Format a value, returning 'N/A' for None."""
        if value is None:
            return "N/A"
        return f"{value:.{decimals}f}"

    print("\n" + "=" * 70)
    print("PERFORMANCE METRICS")
    print("=" * 70)

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

    print("\n" + "=" * 70 + "\n")


def export_metrics(
    results: pd.DataFrame,
    filename: str,
    test_params: dict = None,
    worker_id: str = "gw0",
) -> None:
    """Export metrics to CSV file in perf_data directory."""
    perf_dir = TestConfig.LLK_ROOT / "perf_data"
    perf_dir.mkdir(parents=True, exist_ok=True)

    metrics = compute_metrics(results)

    if not metrics:
        return

    if test_params:
        metrics.update(test_params)

    df = pd.DataFrame([metrics])
    output_path = perf_dir / f"{filename}.{worker_id}.csv"

    if output_path.exists():
        existing = pd.read_csv(output_path)
        df = pd.concat([existing, df], ignore_index=True)

    df.to_csv(output_path, index=False)
