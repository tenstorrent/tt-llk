# SPDX-FileCopyrightText: © 2026 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0

from typing import Dict, List

import pandas as pd
from helpers.chip_architecture import ChipArchitecture, get_chip_architecture
from helpers.test_config import TestConfig

# Results schema is identical to helpers.counters.read_perf_counters output
CounterResult = Dict[str, object]

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


def _get_platform_bandwidth() -> Dict[str, object]:
    """Get bandwidth parameters for the current chip architecture."""
    arch = get_chip_architecture()
    if arch not in _PLATFORM_BANDWIDTH:
        raise ValueError(f"No bandwidth config for architecture: {arch}")
    return _PLATFORM_BANDWIDTH[arch]


def _sum_counts(
    results: List[CounterResult], bank: str, names: List[str], mux: int = None
) -> int:
    total = 0
    for r in results:
        if r.get("bank") != bank:
            continue
        if r.get("counter_name") in names:
            if mux is None or r.get("l1_mux") == mux:
                total += int(r.get("count", 0))
    return total


def _get_cycles(results: List[CounterResult], bank: str) -> int:
    for r in results:
        if r.get("bank") == bank:
            return int(r.get("cycles", 0))
    return 0


def _rate(count: int, cycles: int) -> float:
    return (count / cycles) if cycles else 0.0


def _compute_thread_metrics(results: List[CounterResult]) -> Dict[str, object]:
    """Compute metrics for a single thread's counter data."""
    hw = _get_platform_bandwidth()

    cycles_instrn = _get_cycles(results, "INSTRN_THREAD")
    cycles_fpu = _get_cycles(results, "FPU")
    cycles_unpack = _get_cycles(results, "TDMA_UNPACK")
    cycles_pack = _get_cycles(results, "TDMA_PACK")
    cycles_l1 = _get_cycles(results, "L1")

    wall_cycles = max(cycles_instrn, cycles_fpu, cycles_unpack, cycles_pack, cycles_l1)

    # Instruction issue and stalls
    stalled = _sum_counts(results, "INSTRN_THREAD", ["STALLED"])
    instr_issue = _sum_counts(
        results,
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
    fpu_valid = _sum_counts(results, "FPU", ["FPU_OP_VALID"])
    sfpu_valid = _sum_counts(results, "FPU", ["SFPU_OP_VALID"])
    compute_util = _rate(fpu_valid + sfpu_valid, cycles_fpu)
    fpu_rate = _rate(fpu_valid, cycles_fpu)
    sfpu_rate = _rate(sfpu_valid, cycles_fpu)

    # Unpacker/Packer utilization
    unpack_busy = sum(
        _sum_counts(results, "TDMA_UNPACK", [f"UNPACK_BUSY_{i}"]) for i in range(4)
    )
    pack_busy = _sum_counts(results, "TDMA_PACK", ["PACK_BUSY_10"]) + _sum_counts(
        results, "TDMA_PACK", ["PACK_BUSY_11"]
    )
    unpack_util = _rate(unpack_busy, max(1, cycles_unpack * 4))
    pack_util = _rate(pack_busy, max(1, cycles_pack * 2))

    # NoC activity
    noc_in = _sum_counts(
        results,
        "L1",
        [
            "NOC_RING0_INCOMING_1",
            "NOC_RING0_INCOMING_0",
            "NOC_RING1_INCOMING_1",
            "NOC_RING1_INCOMING_0",
        ],
    )
    noc_out = _sum_counts(
        results,
        "L1",
        [
            "NOC_RING0_OUTGOING_1",
            "NOC_RING0_OUTGOING_0",
            "NOC_RING1_OUTGOING_1",
            "NOC_RING1_OUTGOING_0",
        ],
    )
    noc_txn_per_cycle = _rate(noc_in + noc_out, cycles_l1)
    noc_bytes_per_cycle = noc_txn_per_cycle * hw["noc_word_bytes"]

    # L1 arbitration/congestion
    l1_arb = _sum_counts(
        results,
        "L1",
        [
            "L1_ARB_TDMA_BUNDLE_1",
            "L1_ARB_TDMA_BUNDLE_0",
            "L1_ARB_UNPACKER",
            "TDMA_BUNDLE_1_ARB",
            "TDMA_BUNDLE_0_ARB",
        ],
    )
    l1_no_arb = _sum_counts(results, "L1", ["L1_NO_ARB_UNPACKER"], mux=0)
    l1_congestion_index = l1_arb / max(1, l1_arb + l1_no_arb)

    # Bandwidth estimates
    unpack_bw = unpack_util * hw["unpack_max_bytes_per_cycle"]
    pack_bw = pack_util * hw["pack_max_bytes_per_cycle"]

    # Rates
    stall_rate = _rate(stalled, cycles_instrn)
    issue_rate = _rate(instr_issue, cycles_instrn)

    # Bound scores
    risc_bound_score = stall_rate - (issue_rate * 0.5)

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
    results_requests: Dict[str, List[CounterResult]],
    results_grants: Dict[str, List[CounterResult]],
) -> Dict[str, float]:
    """Aggregate thread metrics into kernel-level metrics."""
    hw = _get_platform_bandwidth()

    grants_metrics = []
    requests_metrics = []

    for thread in sorted(
        set(list(results_requests.keys()) + list(results_grants.keys()))
    ):
        if results_grants.get(thread):
            grants_metrics.append(_compute_thread_metrics(results_grants[thread]))
        if results_requests.get(thread):
            requests_metrics.append(_compute_thread_metrics(results_requests[thread]))

    if not grants_metrics:
        return {}

    avg = lambda xs: (sum(xs) / len(xs)) if xs else 0.0

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

    # Bandwidth estimates
    metrics["unpack_bw_bytes_per_cycle"] = (
        metrics["unpack_util"] * hw["unpack_max_bytes_per_cycle"]
    )
    metrics["pack_bw_bytes_per_cycle"] = (
        metrics["pack_util"] * hw["pack_max_bytes_per_cycle"]
    )

    # Bound scores
    metrics["math_bound_score"] = metrics["compute_util"]
    metrics["unpack_bound_score"] = metrics["unpack_util"]
    metrics["pack_bound_score"] = metrics["pack_util"]
    metrics["risc_bound_score"] = metrics["risc_stall_rate"] - (
        metrics["risc_issue_rate"] * 0.5
    )

    # Kernel bound classification
    kernel_compute_score = metrics["compute_util"]
    kernel_data_score = (
        (metrics["unpack_util"] + metrics["pack_util"]) / 2.0
        + metrics["l1_congestion_index"]
        + metrics["noc_txn_per_cycle"]
    )
    metrics["kernel_compute_score"] = kernel_compute_score
    metrics["kernel_data_score"] = kernel_data_score
    metrics["kernel_bound"] = (
        "compute-bound"
        if kernel_compute_score >= kernel_data_score
        else "data-movement-bound"
    )

    # Dominant component
    bound_scores = [
        ("math-bound", metrics["math_bound_score"]),
        ("unpack-bound", metrics["unpack_bound_score"]),
        ("pack-bound", metrics["pack_bound_score"]),
        ("risc-bound", metrics["risc_bound_score"]),
    ]
    dominant = max(bound_scores, key=lambda x: x[1])
    metrics["dominant_bound"] = dominant[0]
    metrics["dominant_bound_score"] = dominant[1]

    # Acceptance ratios
    sum_grants = lambda key: sum([m[key] for m in grants_metrics])
    sum_requests = lambda key: (
        sum([m[key] for m in requests_metrics]) if requests_metrics else 0.0
    )

    total_noc_grants = sum_grants("noc_in") + sum_grants("noc_out")
    total_noc_requests = sum_requests("noc_in") + sum_requests("noc_out")
    metrics["noc_acceptance"] = (
        (total_noc_grants / total_noc_requests) if total_noc_requests else 1.0
    )

    total_unpack_grants = sum_grants("unpack_busy")
    total_unpack_requests = sum_requests("unpack_busy")
    metrics["unpack_acceptance"] = (
        (total_unpack_grants / total_unpack_requests) if total_unpack_requests else 1.0
    )

    total_pack_grants = sum_grants("pack_busy")
    total_pack_requests = sum_requests("pack_busy")
    metrics["pack_acceptance"] = (
        (total_pack_grants / total_pack_requests) if total_pack_requests else 1.0
    )

    return metrics


def print_metrics(
    results_requests: Dict[str, List[CounterResult]],
    results_grants: Dict[str, List[CounterResult]],
) -> None:
    """Print kernel metrics to console."""
    metrics = _aggregate_metrics(results_requests, results_grants)

    if not metrics:
        print("No metrics to display.")
        return

    print("\n" + "=" * 80)
    print("KERNEL PERFORMANCE METRICS")
    print("=" * 80)

    # Bound classification
    print(f"\n┌{'─' * 78}┐")
    print(f"│ {'BOUND CLASSIFICATION':^76} │")
    print(f"├{'─' * 78}┤")
    print(f"│  Kernel Bound:    {metrics.get('kernel_bound', 'unknown'):<56} │")
    print(
        f"│  Dominant:        {metrics.get('dominant_bound', 'unknown'):<40} (score: {metrics.get('dominant_bound_score', 0):.3f})  │"
    )
    print(f"└{'─' * 78}┘")

    # Utilization metrics
    print(f"\n┌{'─' * 78}┐")
    print(f"│ {'UTILIZATION METRICS':^76} │")
    print(f"├{'─' * 78}┤")
    print(f"│  {'Metric':<30} {'Value':>15} {'Description':<28} │")
    print(f"│  {'─' * 30} {'─' * 15} {'─' * 28} │")
    print(
        f"│  {'Compute Utilization':<30} {metrics.get('compute_util', 0):>15.4f} {'FPU + SFPU activity':<28} │"
    )
    print(
        f"│  {'FPU Rate':<30} {metrics.get('fpu_rate', 0):>15.4f} {'FPU ops per cycle':<28} │"
    )
    print(
        f"│  {'SFPU Rate':<30} {metrics.get('sfpu_rate', 0):>15.4f} {'SFPU ops per cycle':<28} │"
    )
    print(
        f"│  {'Unpack Utilization':<30} {metrics.get('unpack_util', 0):>15.4f} {'TDMA unpack activity':<28} │"
    )
    print(
        f"│  {'Pack Utilization':<30} {metrics.get('pack_util', 0):>15.4f} {'TDMA pack activity':<28} │"
    )
    print(f"└{'─' * 78}┘")

    # Bandwidth metrics
    print(f"\n┌{'─' * 78}┐")
    print(f"│ {'BANDWIDTH ESTIMATES':^76} │")
    print(f"├{'─' * 78}┤")
    print(f"│  {'Metric':<30} {'Value':>15} {'Unit':<28} │")
    print(f"│  {'─' * 30} {'─' * 15} {'─' * 28} │")
    print(
        f"│  {'Unpack BW':<30} {metrics.get('unpack_bw_bytes_per_cycle', 0):>15.2f} {'bytes/cycle':<28} │"
    )
    print(
        f"│  {'Pack BW':<30} {metrics.get('pack_bw_bytes_per_cycle', 0):>15.2f} {'bytes/cycle':<28} │"
    )
    print(
        f"│  {'NoC BW':<30} {metrics.get('noc_bytes_per_cycle', 0):>15.2f} {'bytes/cycle':<28} │"
    )
    print(
        f"│  {'NoC Transactions':<30} {metrics.get('noc_txn_per_cycle', 0):>15.4f} {'txn/cycle':<28} │"
    )
    print(f"└{'─' * 78}┘")

    # L1/NoC metrics
    print(f"\n┌{'─' * 78}┐")
    print(f"│ {'L1/NOC METRICS':^76} │")
    print(f"├{'─' * 78}┤")
    print(f"│  {'Metric':<30} {'Value':>15} {'Description':<28} │")
    print(f"│  {'─' * 30} {'─' * 15} {'─' * 28} │")
    print(
        f"│  {'L1 Congestion Index':<30} {metrics.get('l1_congestion_index', 0):>15.4f} {'Higher = more contention':<28} │"
    )
    print(f"└{'─' * 78}┘")

    # RISC metrics
    print(f"\n┌{'─' * 78}┐")
    print(f"│ {'RISC THREAD METRICS':^76} │")
    print(f"├{'─' * 78}┤")
    print(f"│  {'Metric':<30} {'Value':>15} {'Description':<28} │")
    print(f"│  {'─' * 30} {'─' * 15} {'─' * 28} │")
    print(
        f"│  {'RISC Stall Rate':<30} {metrics.get('risc_stall_rate', 0):>15.4f} {'Fraction of time stalled':<28} │"
    )
    print(
        f"│  {'RISC Issue Rate':<30} {metrics.get('risc_issue_rate', 0):>15.4f} {'Instructions per cycle':<28} │"
    )
    print(f"└{'─' * 78}┘")

    # Acceptance ratios
    print(f"\n┌{'─' * 78}┐")
    print(f"│ {'ACCEPTANCE RATIOS (REQUESTS → GRANTS)':^76} │")
    print(f"├{'─' * 78}┤")
    print(f"│  {'Metric':<30} {'Value':>15} {'Description':<28} │")
    print(f"│  {'─' * 30} {'─' * 15} {'─' * 28} │")
    print(
        f"│  {'NoC Acceptance':<30} {metrics.get('noc_acceptance', 0):>15.4f} {'1.0 = no backpressure':<28} │"
    )
    print(
        f"│  {'Unpack Acceptance':<30} {metrics.get('unpack_acceptance', 0):>15.4f} {'1.0 = no backpressure':<28} │"
    )
    print(
        f"│  {'Pack Acceptance':<30} {metrics.get('pack_acceptance', 0):>15.4f} {'1.0 = no backpressure':<28} │"
    )
    print(f"└{'─' * 78}┘")

    # Bound scores
    print(f"\n┌{'─' * 78}┐")
    print(f"│ {'BOUND SCORES':^76} │")
    print(f"├{'─' * 78}┤")
    print(f"│  {'Component':<30} {'Score':>15} {'Rank':<28} │")
    print(f"│  {'─' * 30} {'─' * 15} {'─' * 28} │")
    scores = [
        ("Math", metrics.get("math_bound_score", 0)),
        ("Unpack", metrics.get("unpack_bound_score", 0)),
        ("Pack", metrics.get("pack_bound_score", 0)),
        ("RISC", metrics.get("risc_bound_score", 0)),
    ]
    for i, (name, score) in enumerate(
        sorted(scores, key=lambda x: x[1], reverse=True), 1
    ):
        rank_str = f"#{i}" + (" (dominant)" if i == 1 else "")
        print(f"│  {name:<30} {score:>15.4f} {rank_str:<28} │")
    print(f"└{'─' * 78}┘")

    print(f"\n  Wall Cycles: {int(metrics.get('wall_cycles', 0)):,}")
    print("\n" + "=" * 80 + "\n")


def export_metrics(
    results_requests: Dict[str, List[CounterResult]],
    results_grants: Dict[str, List[CounterResult]],
    filename: str,
    test_params: Dict[str, object] = None,
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
