# SPDX-FileCopyrightText: © 2026 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0

import pandas as pd
from helpers.chip_architecture import ChipArchitecture, get_chip_architecture
from helpers.test_config import TestConfig

# Platform-specific bandwidth parameters for metrics calculations.
_PLATFORM_BANDWIDTH = {
    ChipArchitecture.WORMHOLE: {
        "noc_word_bytes": 32,
        "unpack_max_bytes_per_cycle": 80.0,
        "pack_max_bytes_per_cycle": 80.0,
    },
    ChipArchitecture.BLACKHOLE: {
        "noc_word_bytes": 256,
        "unpack_max_bytes_per_cycle": 120.0,
        "pack_max_bytes_per_cycle": 120.0,
    },
    ChipArchitecture.QUASAR: {
        "noc_word_bytes": 32,
        "unpack_max_bytes_per_cycle": 80.0,
        "pack_max_bytes_per_cycle": 80.0,
    },
}


def _get_platform_bandwidth() -> dict:
    """Get bandwidth parameters for the current chip architecture."""
    arch = get_chip_architecture()
    if arch not in _PLATFORM_BANDWIDTH:
        raise ValueError(f"No bandwidth config for architecture: {arch}")
    return _PLATFORM_BANDWIDTH[arch]


def _sum_counts(df: pd.DataFrame, bank: str, names: list, mux: int = None) -> int:
    """Sum counter counts for given bank and counter names."""
    mask = (df["bank"] == bank) & (df["counter_name"].isin(names))
    if mux is not None:
        mask = mask & (df["l1_mux"] == mux)
    return int(df.loc[mask, "count"].sum())


def _get_cycles(df: pd.DataFrame, bank: str) -> int:
    """Get cycles for a specific bank."""
    bank_df = df[df["bank"] == bank]
    if bank_df.empty:
        return 0
    return int(bank_df["cycles"].iloc[0])


def _rate(count: int, cycles: int) -> float | None:
    """Calculate rate as count/cycles, returning None if cycles is 0."""
    return (count / cycles) if cycles else None


def _compute_thread_metrics(df: pd.DataFrame) -> dict:
    """Compute metrics for a single thread's counter data."""
    hw = _get_platform_bandwidth()

    cycles_instrn = _get_cycles(df, "INSTRN_THREAD")
    cycles_fpu = _get_cycles(df, "FPU")
    cycles_unpack = _get_cycles(df, "TDMA_UNPACK")
    cycles_pack = _get_cycles(df, "TDMA_PACK")
    cycles_l1 = _get_cycles(df, "L1")

    wall_cycles = max(cycles_instrn, cycles_fpu, cycles_unpack, cycles_pack, cycles_l1)

    # Instruction issue and stalls
    stalled = _sum_counts(df, "INSTRN_THREAD", ["STALLED"])
    instr_issue = _sum_counts(
        df,
        "INSTRN_THREAD",
        [
            "INST_CFG",
            "INST_SYNC",
            "INST_THCON",
            "INST_XSEARCH",
            "INST_MOVE",
            "INST_MATH",
            "INST_UNPACK",
            "INST_PACK",
        ],
    )

    # Compute engine utilization
    fpu_valid = _sum_counts(df, "FPU", ["FPU_OP_VALID"])
    sfpu_valid = _sum_counts(df, "FPU", ["SFPU_OP_VALID"])
    compute_util = _rate(fpu_valid + sfpu_valid, cycles_fpu)
    fpu_rate = _rate(fpu_valid, cycles_fpu)
    sfpu_rate = _rate(sfpu_valid, cycles_fpu)

    # Unpacker/Packer utilization
    unpack_busy = sum(
        _sum_counts(df, "TDMA_UNPACK", [f"UNPACK_BUSY_{i}"]) for i in range(4)
    )
    pack_busy = _sum_counts(df, "TDMA_PACK", ["PACK_BUSY_10"]) + _sum_counts(
        df, "TDMA_PACK", ["PACK_BUSY_11"]
    )
    unpack_util = _rate(unpack_busy, cycles_unpack * 4) if cycles_unpack else None
    pack_util = _rate(pack_busy, cycles_pack * 2) if cycles_pack else None

    # NoC activity
    noc_in = _sum_counts(
        df,
        "L1",
        [
            "NOC_RING0_INCOMING_1",
            "NOC_RING0_INCOMING_0",
            "NOC_RING1_INCOMING_1",
            "NOC_RING1_INCOMING_0",
        ],
    )
    noc_out = _sum_counts(
        df,
        "L1",
        [
            "NOC_RING0_OUTGOING_1",
            "NOC_RING0_OUTGOING_0",
            "NOC_RING1_OUTGOING_1",
            "NOC_RING1_OUTGOING_0",
        ],
    )
    noc_txn_per_cycle = _rate(noc_in + noc_out, cycles_l1)
    noc_bytes_per_cycle = (
        noc_txn_per_cycle * hw["noc_word_bytes"]
        if noc_txn_per_cycle is not None
        else None
    )

    # L1 arbitration/congestion
    l1_arb = _sum_counts(
        df,
        "L1",
        [
            "L1_ARB_TDMA_BUNDLE_1",
            "L1_ARB_TDMA_BUNDLE_0",
            "L1_ARB_UNPACKER",
            "TDMA_BUNDLE_1_ARB",
            "TDMA_BUNDLE_0_ARB",
        ],
    )
    l1_no_arb = _sum_counts(df, "L1", ["L1_NO_ARB_UNPACKER"], mux=0)
    l1_total = l1_arb + l1_no_arb
    l1_congestion_index = (l1_arb / l1_total) if l1_total else None

    # Bandwidth estimates
    unpack_bw = (
        unpack_util * hw["unpack_max_bytes_per_cycle"]
        if unpack_util is not None
        else None
    )
    pack_bw = (
        pack_util * hw["pack_max_bytes_per_cycle"] if pack_util is not None else None
    )

    # Rates
    stall_rate = _rate(stalled, cycles_instrn)
    issue_rate = _rate(instr_issue, cycles_instrn)

    # Bound scores (None if underlying metrics are None)
    risc_bound_score = (
        stall_rate - (issue_rate * 0.5)
        if stall_rate is not None and issue_rate is not None
        else None
    )

    return {
        "wall_cycles": wall_cycles,
        "compute_util": compute_util,
        "fpu_rate": fpu_rate,
        "sfpu_rate": sfpu_rate,
        "unpack_util": unpack_util,
        "pack_util": pack_util,
        "unpack_busy": unpack_busy,
        "pack_busy": pack_busy,
        "unpack_bw": unpack_bw,
        "pack_bw": pack_bw,
        "l1_congestion_index": l1_congestion_index,
        "noc_txn_per_cycle": noc_txn_per_cycle,
        "noc_bytes_per_cycle": noc_bytes_per_cycle,
        "noc_in": noc_in,
        "noc_out": noc_out,
        "stall_rate": stall_rate,
        "issue_rate": issue_rate,
        "math_bound_score": compute_util,
        "unpack_bound_score": unpack_util,
        "pack_bound_score": pack_util,
        "risc_bound_score": risc_bound_score,
    }


def _aggregate_metrics(
    results_requests: pd.DataFrame,
    results_grants: pd.DataFrame,
) -> dict:
    """Aggregate thread metrics into kernel-level metrics."""
    hw = _get_platform_bandwidth()

    grants_metrics = []
    requests_metrics = []

    # Get unique threads from both DataFrames
    threads_grants = (
        set(results_grants["thread"].unique()) if not results_grants.empty else set()
    )
    threads_requests = (
        set(results_requests["thread"].unique())
        if not results_requests.empty
        else set()
    )
    all_threads = sorted(threads_grants | threads_requests)

    for thread in all_threads:
        grants_df = (
            results_grants[results_grants["thread"] == thread]
            if not results_grants.empty
            else pd.DataFrame()
        )
        requests_df = (
            results_requests[results_requests["thread"] == thread]
            if not results_requests.empty
            else pd.DataFrame()
        )

        if not grants_df.empty:
            grants_metrics.append(_compute_thread_metrics(grants_df))
        if not requests_df.empty:
            requests_metrics.append(_compute_thread_metrics(requests_df))

    if not grants_metrics:
        return {}

    def avg(xs):
        """Average of non-None values, or None if all are None."""
        valid = [x for x in xs if x is not None]
        return (sum(valid) / len(valid)) if valid else None

    metrics = {
        "compute_util": avg([m["compute_util"] for m in grants_metrics]),
        "fpu_rate": avg([m["fpu_rate"] for m in grants_metrics]),
        "sfpu_rate": avg([m["sfpu_rate"] for m in grants_metrics]),
        "unpack_util": avg([m["unpack_util"] for m in grants_metrics]),
        "pack_util": avg([m["pack_util"] for m in grants_metrics]),
        "l1_congestion_index": avg([m["l1_congestion_index"] for m in grants_metrics]),
        "noc_txn_per_cycle": avg([m["noc_txn_per_cycle"] for m in grants_metrics]),
        "noc_bytes_per_cycle": avg([m["noc_bytes_per_cycle"] for m in grants_metrics]),
        "risc_stall_rate": avg([m["stall_rate"] for m in grants_metrics]),
        "risc_issue_rate": avg([m["issue_rate"] for m in grants_metrics]),
        "wall_cycles": max([m["wall_cycles"] for m in grants_metrics]),
    }

    # Bandwidth estimates (None if utilization is None)
    metrics["unpack_bw_bytes_per_cycle"] = (
        metrics["unpack_util"] * hw["unpack_max_bytes_per_cycle"]
        if metrics["unpack_util"] is not None
        else None
    )
    metrics["pack_bw_bytes_per_cycle"] = (
        metrics["pack_util"] * hw["pack_max_bytes_per_cycle"]
        if metrics["pack_util"] is not None
        else None
    )

    # Bound scores
    metrics["math_bound_score"] = metrics["compute_util"]
    metrics["unpack_bound_score"] = metrics["unpack_util"]
    metrics["pack_bound_score"] = metrics["pack_util"]
    metrics["risc_bound_score"] = (
        metrics["risc_stall_rate"] - (metrics["risc_issue_rate"] * 0.5)
        if metrics["risc_stall_rate"] is not None
        and metrics["risc_issue_rate"] is not None
        else None
    )

    # Kernel bound classification (only if we have the required metrics)
    kernel_compute_score = metrics["compute_util"]
    if (
        metrics["unpack_util"] is not None
        and metrics["pack_util"] is not None
        and metrics["l1_congestion_index"] is not None
        and metrics["noc_txn_per_cycle"] is not None
    ):
        kernel_data_score = (
            (metrics["unpack_util"] + metrics["pack_util"]) / 2.0
            + metrics["l1_congestion_index"]
            + metrics["noc_txn_per_cycle"]
        )
    else:
        kernel_data_score = None

    metrics["kernel_compute_score"] = kernel_compute_score
    metrics["kernel_data_score"] = kernel_data_score

    if kernel_compute_score is not None and kernel_data_score is not None:
        metrics["kernel_bound"] = (
            "compute-bound"
            if kernel_compute_score >= kernel_data_score
            else "data-movement-bound"
        )
    else:
        metrics["kernel_bound"] = None

    # Dominant component (filter out None scores)
    bound_scores = [
        ("math-bound", metrics["math_bound_score"]),
        ("unpack-bound", metrics["unpack_bound_score"]),
        ("pack-bound", metrics["pack_bound_score"]),
        ("risc-bound", metrics["risc_bound_score"]),
    ]
    valid_scores = [(name, score) for name, score in bound_scores if score is not None]
    if valid_scores:
        dominant = max(valid_scores, key=lambda x: x[1])
        metrics["dominant_bound"] = dominant[0]
        metrics["dominant_bound_score"] = dominant[1]
    else:
        metrics["dominant_bound"] = None
        metrics["dominant_bound_score"] = None

    # Acceptance ratios (None if no requests data)
    def sum_values(metrics_list, key):
        """Sum values, treating None as 0."""
        return sum(m[key] or 0 for m in metrics_list)

    total_noc_grants = sum_values(grants_metrics, "noc_in") + sum_values(
        grants_metrics, "noc_out"
    )
    total_noc_requests = (
        sum_values(requests_metrics, "noc_in") + sum_values(requests_metrics, "noc_out")
        if requests_metrics
        else 0
    )
    metrics["noc_acceptance"] = (
        (total_noc_grants / total_noc_requests) if total_noc_requests else None
    )

    total_unpack_grants = sum_values(grants_metrics, "unpack_busy")
    total_unpack_requests = (
        sum_values(requests_metrics, "unpack_busy") if requests_metrics else 0
    )
    metrics["unpack_acceptance"] = (
        (total_unpack_grants / total_unpack_requests) if total_unpack_requests else None
    )

    total_pack_grants = sum_values(grants_metrics, "pack_busy")
    total_pack_requests = (
        sum_values(requests_metrics, "pack_busy") if requests_metrics else 0
    )
    metrics["pack_acceptance"] = (
        (total_pack_grants / total_pack_requests) if total_pack_requests else None
    )

    return metrics


def print_metrics(
    results_requests: pd.DataFrame,
    results_grants: pd.DataFrame,
) -> None:
    """Print kernel metrics to console."""
    metrics = _aggregate_metrics(results_requests, results_grants)

    if not metrics:
        print("No metrics to display.")
        return

    def fmt(value, decimals=4):
        """Format a value, returning 'N/A' for None."""
        if value is None:
            return "N/A"
        return f"{value:.{decimals}f}"

    def fmt_or_na(value, decimals=4, width=15):
        """Format value right-aligned, or 'N/A' centered."""
        if value is None:
            return f"{'N/A':>{width}}"
        return f"{value:>{width}.{decimals}f}"

    print("\n" + "=" * 80)
    print("KERNEL PERFORMANCE METRICS")
    print("=" * 80)

    # Bound classification
    kernel_bound = metrics.get("kernel_bound") or "N/A"
    dominant_bound = metrics.get("dominant_bound") or "N/A"
    dominant_score = metrics.get("dominant_bound_score")
    dominant_score_str = (
        f"(score: {dominant_score:.3f})"
        if dominant_score is not None
        else "(score: N/A)"
    )

    print(f"\n┌{'─' * 78}┐")
    print(f"│ {'BOUND CLASSIFICATION':^76} │")
    print(f"├{'─' * 78}┤")
    print(f"│  Kernel Bound:    {kernel_bound:<56} │")
    print(f"│  Dominant:        {dominant_bound:<40} {dominant_score_str:<15} │")
    print(f"└{'─' * 78}┘")

    # Utilization metrics
    print(f"\n┌{'─' * 78}┐")
    print(f"│ {'UTILIZATION METRICS':^76} │")
    print(f"├{'─' * 78}┤")
    print(f"│  {'Metric':<30} {'Value':>15} {'Description':<28} │")
    print(f"│  {'─' * 30} {'─' * 15} {'─' * 28} │")
    print(
        f"│  {'Compute Utilization':<30} {fmt_or_na(metrics.get('compute_util'))} {'FPU + SFPU activity':<28} │"
    )
    print(
        f"│  {'FPU Rate':<30} {fmt_or_na(metrics.get('fpu_rate'))} {'FPU ops per cycle':<28} │"
    )
    print(
        f"│  {'SFPU Rate':<30} {fmt_or_na(metrics.get('sfpu_rate'))} {'SFPU ops per cycle':<28} │"
    )
    print(
        f"│  {'Unpack Utilization':<30} {fmt_or_na(metrics.get('unpack_util'))} {'TDMA unpack activity':<28} │"
    )
    print(
        f"│  {'Pack Utilization':<30} {fmt_or_na(metrics.get('pack_util'))} {'TDMA pack activity':<28} │"
    )
    print(f"└{'─' * 78}┘")

    # Bandwidth metrics
    print(f"\n┌{'─' * 78}┐")
    print(f"│ {'BANDWIDTH ESTIMATES':^76} │")
    print(f"├{'─' * 78}┤")
    print(f"│  {'Metric':<30} {'Value':>15} {'Unit':<28} │")
    print(f"│  {'─' * 30} {'─' * 15} {'─' * 28} │")
    print(
        f"│  {'Unpack BW':<30} {fmt_or_na(metrics.get('unpack_bw_bytes_per_cycle'), 2)} {'bytes/cycle':<28} │"
    )
    print(
        f"│  {'Pack BW':<30} {fmt_or_na(metrics.get('pack_bw_bytes_per_cycle'), 2)} {'bytes/cycle':<28} │"
    )
    print(
        f"│  {'NoC BW':<30} {fmt_or_na(metrics.get('noc_bytes_per_cycle'), 2)} {'bytes/cycle':<28} │"
    )
    print(
        f"│  {'NoC Transactions':<30} {fmt_or_na(metrics.get('noc_txn_per_cycle'))} {'txn/cycle':<28} │"
    )
    print(f"└{'─' * 78}┘")

    # L1/NoC metrics
    print(f"\n┌{'─' * 78}┐")
    print(f"│ {'L1/NOC METRICS':^76} │")
    print(f"├{'─' * 78}┤")
    print(f"│  {'Metric':<30} {'Value':>15} {'Description':<28} │")
    print(f"│  {'─' * 30} {'─' * 15} {'─' * 28} │")
    print(
        f"│  {'L1 Congestion Index':<30} {fmt_or_na(metrics.get('l1_congestion_index'))} {'Higher = more contention':<28} │"
    )
    print(f"└{'─' * 78}┘")

    # RISC metrics
    print(f"\n┌{'─' * 78}┐")
    print(f"│ {'RISC THREAD METRICS':^76} │")
    print(f"├{'─' * 78}┤")
    print(f"│  {'Metric':<30} {'Value':>15} {'Description':<28} │")
    print(f"│  {'─' * 30} {'─' * 15} {'─' * 28} │")
    print(
        f"│  {'RISC Stall Rate':<30} {fmt_or_na(metrics.get('risc_stall_rate'))} {'Fraction of time stalled':<28} │"
    )
    print(
        f"│  {'RISC Issue Rate':<30} {fmt_or_na(metrics.get('risc_issue_rate'))} {'Instructions per cycle':<28} │"
    )
    print(f"└{'─' * 78}┘")

    # Acceptance ratios
    print(f"\n┌{'─' * 78}┐")
    print(f"│ {'ACCEPTANCE RATIOS (REQUESTS → GRANTS)':^76} │")
    print(f"├{'─' * 78}┤")
    print(f"│  {'Metric':<30} {'Value':>15} {'Description':<28} │")
    print(f"│  {'─' * 30} {'─' * 15} {'─' * 28} │")
    print(
        f"│  {'NoC Acceptance':<30} {fmt_or_na(metrics.get('noc_acceptance'))} {'1.0 = no backpressure':<28} │"
    )
    print(
        f"│  {'Unpack Acceptance':<30} {fmt_or_na(metrics.get('unpack_acceptance'))} {'1.0 = no backpressure':<28} │"
    )
    print(
        f"│  {'Pack Acceptance':<30} {fmt_or_na(metrics.get('pack_acceptance'))} {'1.0 = no backpressure':<28} │"
    )
    print(f"└{'─' * 78}┘")

    # Bound scores
    print(f"\n┌{'─' * 78}┐")
    print(f"│ {'BOUND SCORES':^76} │")
    print(f"├{'─' * 78}┤")
    print(f"│  {'Component':<30} {'Score':>15} {'Rank':<28} │")
    print(f"│  {'─' * 30} {'─' * 15} {'─' * 28} │")
    scores = [
        ("Math", metrics.get("math_bound_score")),
        ("Unpack", metrics.get("unpack_bound_score")),
        ("Pack", metrics.get("pack_bound_score")),
        ("RISC", metrics.get("risc_bound_score")),
    ]
    # Sort with None values at the end
    valid_scores = [(n, s) for n, s in scores if s is not None]
    na_scores = [(n, s) for n, s in scores if s is None]
    sorted_scores = sorted(valid_scores, key=lambda x: x[1], reverse=True) + na_scores

    for i, (name, score) in enumerate(sorted_scores, 1):
        if score is not None:
            rank_str = f"#{i}" + (" (dominant)" if i == 1 else "")
            print(f"│  {name:<30} {score:>15.4f} {rank_str:<28} │")
        else:
            print(f"│  {name:<30} {'N/A':>15} {'':<28} │")
    print(f"└{'─' * 78}┘")

    print(f"\n  Wall Cycles: {int(metrics.get('wall_cycles', 0)):,}")
    print("\n" + "=" * 80 + "\n")


def export_metrics(
    results_requests: pd.DataFrame,
    results_grants: pd.DataFrame,
    filename: str,
    test_params: dict = None,
    worker_id: str = "gw0",
) -> None:
    """Export kernel metrics to CSV file."""
    perf_dir = TestConfig.LLK_ROOT / "perf_data"
    perf_dir.mkdir(parents=True, exist_ok=True)

    metrics = _aggregate_metrics(results_requests, results_grants)

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
