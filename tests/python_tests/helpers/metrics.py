# SPDX-FileCopyrightText: © 2026 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0

from typing import Dict, List

import pandas as pd
from helpers.chip_architecture import ChipArchitecture, get_chip_architecture
from helpers.test_config import TestConfig

# Results schema is identical to helpers.counters.read_perf_counters output
CounterResult = Dict[str, object]

# Platform-specific bandwidth parameters for metrics calculations.
# These values are used to convert utilization ratios into estimated bytes/cycle.
#
# Keys:
#   noc_word_bytes: Size of one NoC flit in bytes
#   unpack_max_bytes_per_cycle: Theoretical peak unpacker bandwidth
#   pack_max_bytes_per_cycle: Theoretical peak packer bandwidth
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


def _level_label(
    value: float, low: float, high: float, desc_low: str, desc_mid: str, desc_high: str
) -> str:
    if value < low:
        return desc_low
    if value < high:
        return desc_mid
    return desc_high


def _format_bw(label: str, util: float, peak_bpc: float) -> str:
    if peak_bpc:
        bw = util * peak_bpc
        return f"{label}: util={util:.3f} → est ~{bw:.2f} B/cyc"
    return f"{label}: util={util:.3f} (set peak bytes/cycle for BW)"


def compute_thread_metrics(results: List[CounterResult]) -> Dict[str, object]:
    hw = _get_platform_bandwidth()

    # Cycles per bank (shared per-bank window)
    cycles_instrn = _get_cycles(results, "INSTRN_THREAD")
    cycles_fpu = _get_cycles(results, "FPU")
    cycles_unpack = _get_cycles(results, "TDMA_UNPACK")
    cycles_pack = _get_cycles(results, "TDMA_PACK")
    cycles_l1 = _get_cycles(results, "L1")

    wall_cycles = max(cycles_instrn, cycles_fpu, cycles_unpack, cycles_pack, cycles_l1)

    # Instruction issue and stalls
    stalled = _sum_counts(
        results, "INSTRN_THREAD", ["STALLED"]
    )  # all-stall pulse count
    stall_components = {
        "stall_pack0": _sum_counts(results, "INSTRN_THREAD", ["STALL_PACK0"]),
        "stall_math": _sum_counts(results, "INSTRN_THREAD", ["STALL_MATH"]),
        "stall_thcon": _sum_counts(results, "INSTRN_THREAD", ["STALL_THCON"]),
        "stall_sem_zero": _sum_counts(results, "INSTRN_THREAD", ["STALL_SEM_ZERO"]),
        "stall_sem_max": _sum_counts(results, "INSTRN_THREAD", ["STALL_SEM_MAX"]),
        "stall_move": _sum_counts(results, "INSTRN_THREAD", ["STALL_MOVE"]),
        "stall_trisc_reg_access": _sum_counts(
            results, "INSTRN_THREAD", ["STALL_TRISC_REG_ACCESS"]
        ),
        "stall_sfpu": _sum_counts(results, "INSTRN_THREAD", ["STALL_SFPU"]),
    }
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

    # Compute engine utilization (per FPU bank window)
    fpu_valid = _sum_counts(
        results, "FPU", ["FPU_OP_VALID"]
    )  # pulses when FPU issues work
    sfpu_valid = _sum_counts(
        results, "FPU", ["SFPU_OP_VALID"]
    )  # pulses when SFPU issues work
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
    unpack_util = _rate(
        unpack_busy, max(1, cycles_unpack * 4)
    )  # average across 4 lanes
    pack_util = _rate(pack_busy, max(1, cycles_pack * 2))  # average across 2 lanes

    # NoC activity via L1 NIU counters (both mux slices)
    noc_in = _sum_counts(
        results,
        "L1",
        [
            "NOC_RING0_INCOMING_1",
            "NOC_RING0_INCOMING_0",
            "NOC_RING1_INCOMING_1",
            "NOC_RING1_INCOMING_0",
        ],
        mux=None,
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
        mux=None,
    )
    noc_txn_per_cycle = _rate(noc_in + noc_out, cycles_l1)
    noc_bytes_per_cycle = noc_txn_per_cycle * hw["noc_word_bytes"]

    # L1 arbitration/congestion proxy
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
        mux=None,
    )
    l1_no_arb = _sum_counts(results, "L1", ["L1_NO_ARB_UNPACKER"], mux=0)
    l1_congestion_index = l1_arb / max(1, l1_arb + l1_no_arb)

    # Bandwidth estimates based on platform theoretical maxima
    unpack_bw_bytes_per_cycle = unpack_util * hw["unpack_max_bytes_per_cycle"]
    pack_bw_bytes_per_cycle = pack_util * hw["pack_max_bytes_per_cycle"]

    # Heuristic bound classification
    stall_rate = _rate(stalled, cycles_instrn)
    instr_issue_rate = _rate(instr_issue, cycles_instrn)

    # Bound scores by category
    compute_bound_score = compute_util
    unpack_bound_score = unpack_util
    pack_bound_score = pack_util
    risc_bound_score = (
        max(
            stall_rate,
            _rate(
                stall_components["stall_trisc_reg_access"]
                + stall_components["stall_move"]
                + stall_components["stall_thcon"],
                cycles_instrn,
            ),
        )
        - instr_issue_rate * 0.5
    )

    classification = sorted(
        [
            ("math-bound", compute_bound_score),
            ("unpack-bound", unpack_bound_score),
            ("pack-bound", pack_bound_score),
            ("risc-bound", risc_bound_score),
        ],
        key=lambda x: x[1],
        reverse=True,
    )

    return {
        "wall_cycles": wall_cycles,
        "cycles": {
            "instrn": cycles_instrn,
            "fpu": cycles_fpu,
            "unpack": cycles_unpack,
            "pack": cycles_pack,
            "l1": cycles_l1,
        },
        "compute": {
            "utilization": compute_util,
            "fpu_rate": fpu_rate,
            "sfpu_rate": sfpu_rate,
            "fpu_valid": fpu_valid,
            "sfpu_valid": sfpu_valid,
        },
        "unpack": {
            "utilization": unpack_util,
            "busy_counts": unpack_busy,
            "est_bw_bytes_per_cycle": unpack_bw_bytes_per_cycle,
        },
        "pack": {
            "utilization": pack_util,
            "busy_counts": pack_busy,
            "est_bw_bytes_per_cycle": pack_bw_bytes_per_cycle,
        },
        "l1": {
            "congestion_index": l1_congestion_index,
            "noc_txn_per_cycle": noc_txn_per_cycle,
            "noc_bytes_per_cycle": noc_bytes_per_cycle,
            "noc_in_counts": noc_in,
            "noc_out_counts": noc_out,
            "arb_pulses": l1_arb,
            "no_arb_pulses": l1_no_arb,
        },
        "risc": {
            "stall_rate": stall_rate,
            "instr_issue_rate": instr_issue_rate,
            "stall_components": stall_components,
            "stalled": stalled,
            "instr_issue_total": instr_issue,
        },
        "bound_classification": classification,
        "bound_scores": {
            "math": compute_bound_score,
            "unpack": unpack_bound_score,
            "pack": pack_bound_score,
            "risc": risc_bound_score,
        },
    }


def summarize_kernel_metrics_dual(
    results_by_thread_requests: Dict[str, List[CounterResult]],
    results_by_thread_grants: Dict[str, List[CounterResult]],
) -> str:
    """
    Produce a side-by-side summary using both REQUESTS and GRANTS measurements.
    Useful for understanding attempted vs accepted operations and arbitration effects.

    Platform bandwidth parameters are automatically detected from the connected chip.

    Args:
        results_by_thread_requests: Counter results from REQUESTS mode run.
        results_by_thread_grants: Counter results from GRANTS mode run.
    """
    hw = _get_platform_bandwidth()
    lines: List[str] = []
    # Aggregate metrics (use GRANTS for truth, REQUESTS for pressure where meaningful)
    grants_metrics: List[Dict[str, object]] = []
    requests_metrics: List[Dict[str, object]] = []
    for thread in sorted(
        set(
            list(results_by_thread_requests.keys())
            + list(results_by_thread_grants.keys())
        )
    ):
        requests_data = results_by_thread_requests.get(thread, [])
        grants_data = results_by_thread_grants.get(thread, [])
        metrics_requests = (
            compute_thread_metrics(requests_data) if requests_data else None
        )
        metrics_grants = compute_thread_metrics(grants_data) if grants_data else None
        if metrics_grants:
            grants_metrics.append(metrics_grants)
        if metrics_requests:
            requests_metrics.append(metrics_requests)

    # Kernel-level overview (averaged across threads)
    if grants_metrics:
        avg = lambda xs: (sum(xs) / len(xs)) if xs else 0.0
        avg_compute_util = avg(
            [float(m["compute"]["utilization"]) for m in grants_metrics]
        )
        avg_fpu_rate = avg([float(m["compute"]["fpu_rate"]) for m in grants_metrics])
        avg_sfpu_rate = avg([float(m["compute"]["sfpu_rate"]) for m in grants_metrics])
        avg_unpack_util = avg(
            [float(m["unpack"]["utilization"]) for m in grants_metrics]
        )
        avg_pack_util = avg([float(m["pack"]["utilization"]) for m in grants_metrics])
        avg_l1_cong = avg([float(m["l1"]["congestion_index"]) for m in grants_metrics])
        avg_noc_txn = avg([float(m["l1"]["noc_txn_per_cycle"]) for m in grants_metrics])

        # Whole-kernel bound (compute vs data movement)
        kernel_compute_score = avg_compute_util
        kernel_data_score = (
            (avg_unpack_util + avg_pack_util) / 2.0 + avg_l1_cong + avg_noc_txn
        )
        kernel_movement_class = (
            "compute-bound"
            if kernel_compute_score >= kernel_data_score
            else "data-movement-bound"
        )

        # Dominant component (math/unpack/pack/RISC)
        kernel_math_score = avg_compute_util
        kernel_unpack_score = avg_unpack_util
        kernel_pack_score = avg_pack_util
        # Approximate RISC score from stalls minus issue
        avg_stall_rate = avg([float(m["risc"]["stall_rate"]) for m in grants_metrics])
        avg_issue_rate = avg(
            [float(m["risc"]["instr_issue_rate"]) for m in grants_metrics]
        )
        # RISC bound approximation: stalls penalized by instruction issue rate
        # Aligns with per-thread heuristic (stall minus fraction of issue)
        kernel_risc_score = avg_stall_rate - (avg_issue_rate * 0.5)
        kernel_bound = sorted(
            [
                ("math-bound", kernel_math_score),
                ("unpack-bound", kernel_unpack_score),
                ("pack-bound", kernel_pack_score),
                ("risc-bound", kernel_risc_score),
            ],
            key=lambda x: x[1],
            reverse=True,
        )[0]
        # 1) Kernel bound: compute vs data movement
        lines.append("1) Kernel Bound (Compute vs Data-Movement)")
        lines.append(
            "================================================================================"
        )
        lines.append(
            f"Likely bound: {kernel_movement_class} (compute={kernel_compute_score:.3f}, data={kernel_data_score:.3f})."
        )
        if kernel_movement_class == "compute-bound":
            lines.append(
                "Meaning: Compute engines are the dominant limiter relative to data movement."
            )
        else:
            lines.append(
                "Meaning: TDMA/L1/NoC activity dominates; moving data is the limiter."
            )
        lines.append("")

        # 2) Component Metrics (Unpack / Pack / Math / RISC)
        lines.append("2) Component Metrics (Unpack / Pack / Math / RISC)")
        lines.append(
            "================================================================================"
        )
        lines.append(f"Dominant: {kernel_bound[0]} (score {kernel_bound[1]:.3f}).")
        lines.append(
            f"Math:   util={avg_compute_util:.3f} → math-bound means compute engines (FPU/SFPU) set throughput."
        )
        lines.append(
            f"Unpack: util={avg_unpack_util:.3f} → unpack-bound means inbound DMA/memory fetch limits throughput."
        )
        lines.append(
            f"Pack:   util={avg_pack_util:.3f} → pack-bound means outbound DMA/writeback limits throughput."
        )
        lines.append(
            f"RISC:   stall={avg_stall_rate:.3f}, issue={avg_issue_rate:.3f} → risc-bound means instruction provisioning/stalls limit engines."
        )
        # Short rationale for the dominant one
        if kernel_bound[0] == "unpack-bound":
            lines.append(
                f"Why dominant: Unpack util ({avg_unpack_util:.3f}) exceeds compute ({avg_compute_util:.3f}) and pack ({avg_pack_util:.3f})."
            )
        elif kernel_bound[0] == "pack-bound":
            lines.append(
                f"Why dominant: Pack util ({avg_pack_util:.3f}) exceeds compute ({avg_compute_util:.3f}) and unpack ({avg_unpack_util:.3f})."
            )
        elif kernel_bound[0] == "math-bound":
            lines.append(
                f"Why dominant: Compute util ({avg_compute_util:.3f}) exceeds TDMA signals (unpack={avg_unpack_util:.3f}, pack={avg_pack_util:.3f})."
            )
        else:
            lines.append(
                f"Why dominant: RISC stalls ({avg_stall_rate:.3f}) are high relative to issue ({avg_issue_rate:.3f})."
            )
        lines.append("")

        # 3) L1 Congestion
        lines.append("3) L1 Congestion")
        lines.append(
            "================================================================================"
        )
        lines.append(f"Index ≈ {avg_l1_cong:.3f}.")
        # Summarize pulses across threads (sum not average for counts)
        sum_grants = lambda fn: sum([fn(m) for m in grants_metrics])
        sum_requests = lambda fn: (
            sum([fn(m) for m in requests_metrics]) if requests_metrics else 0.0
        )
        total_arb = sum_grants(lambda m: float(m["l1"]["arb_pulses"]))
        total_no_arb = sum_grants(lambda m: float(m["l1"]["no_arb_pulses"]))
        lines.append(
            f"Arbitration pulses: {int(total_arb)}; No-arb pulses: {int(total_no_arb)}."
        )
        lines.append(
            "Meaning: Index = arb / (arb + no-arb). Higher index implies more port conflicts and waiting in L1 arbitration."
        )
        lines.append(
            "NIU txn counters (RING in/out) indicate NoC traffic; arbitration pulses indicate contention at L1 ports, not NoC capacity."
        )
        lines.append("")

        # 4) Bandwidths (Unpacker / Math / Packer / NoC)
        lines.append("4) Bandwidths")
        lines.append(
            "================================================================================"
        )
        # Unpacker BW
        lines.append(
            _format_bw("Unpacker BW", avg_unpack_util, hw["unpack_max_bytes_per_cycle"])
        )
        # Math throughput (utilization; not byte bandwidth)
        lines.append(
            f"Math: util={avg_compute_util:.3f} (compute engine throughput; not bytes)"
        )
        # Packer BW
        lines.append(
            _format_bw("Packer BW", avg_pack_util, hw["pack_max_bytes_per_cycle"])
        )
        # NoC BW
        avg_noc_bpc = avg(
            [float(m["l1"]["noc_bytes_per_cycle"]) for m in grants_metrics]
        )
        lines.append(f"NoC BW ≈ {avg_noc_bpc:.2f} B/cyc (txn/cyc {avg_noc_txn:.2f}).")
        lines.append("")

        # 5) Instruction Rates (FPU / SFPU)
        lines.append("5) Instruction Rates (FPU / SFPU)")
        lines.append(
            "================================================================================"
        )
        lines.append(f"FPU instruction rate: {avg_fpu_rate:.3f} per FPU cycle.")
        lines.append(f"SFPU instruction rate: {avg_sfpu_rate:.3f} per FPU cycle.")
        lines.append(
            "Meaning: Average number of engine operations issued per cycle; higher = better utilization."
        )
        lines.append("")

        # 6) Acceptance (Requests → Grants) for meaningful counters
        # Only TDMA (unpack/pack busy) and NIU (ring in/out) have actionable REQUESTS vs GRANTS semantics.
        total_noc_grants = sum_grants(
            lambda m: float(m["l1"]["noc_in_counts"] + m["l1"]["noc_out_counts"])
        )
        total_noc_requests = sum_requests(
            lambda m: float(m["l1"]["noc_in_counts"] + m["l1"]["noc_out_counts"])
        )
        noc_acceptance = (
            (total_noc_grants / total_noc_requests) if total_noc_requests else 1.0
        )
        total_unpack_grants = sum_grants(lambda m: float(m["unpack"]["busy_counts"]))
        total_unpack_requests = sum_requests(
            lambda m: float(m["unpack"]["busy_counts"])
        )
        unpack_acceptance = (
            (total_unpack_grants / total_unpack_requests)
            if total_unpack_requests
            else 1.0
        )
        total_pack_grants = sum_grants(lambda m: float(m["pack"]["busy_counts"]))
        total_pack_requests = sum_requests(lambda m: float(m["pack"]["busy_counts"]))
        pack_acceptance = (
            (total_pack_grants / total_pack_requests) if total_pack_requests else 1.0
        )

        lines.append("6) Acceptance (Requests → Grants)")
        lines.append(
            "================================================================================"
        )
        lines.append(f"NoC acceptance: {noc_acceptance:.3f} (delivered/attempted txn).")
        lines.append(
            f"Unpack acceptance: {unpack_acceptance:.3f} (delivered/attempted TDMA work)."
        )
        lines.append(
            f"Pack acceptance: {pack_acceptance:.3f} (delivered/attempted TDMA work)."
        )
        lines.append(
            "Meaning: Values < 1.0 indicate backpressure or rejection; ≈1.0 means requests are being served."
        )
        lines.append(
            "Note: Arbitration pulses and stall counters are mode-independent; use GRANTS for truth and ignore REQUESTS for those."
        )
        lines.append("")

    # Summary now differentiates REQUESTS vs GRANTS where meaningful.

    return "\n".join(lines)


def compute_kernel_metrics(
    results_by_thread_requests: Dict[str, List[CounterResult]],
    results_by_thread_grants: Dict[str, List[CounterResult]],
) -> Dict[str, float]:
    """
    Compute kernel-level metrics from dual-mode counter results.

    Returns a flat dictionary of metric name -> value pairs suitable for CSV export.

    Args:
        results_by_thread_requests: Counter results from REQUESTS mode run.
        results_by_thread_grants: Counter results from GRANTS mode run.

    Returns:
        Dictionary of metric names to values.
    """
    hw = _get_platform_bandwidth()
    metrics = {}

    # Aggregate metrics from all threads
    grants_metrics: List[Dict[str, object]] = []
    requests_metrics: List[Dict[str, object]] = []

    for thread in sorted(
        set(
            list(results_by_thread_requests.keys())
            + list(results_by_thread_grants.keys())
        )
    ):
        requests_data = results_by_thread_requests.get(thread, [])
        grants_data = results_by_thread_grants.get(thread, [])
        metrics_requests = (
            compute_thread_metrics(requests_data) if requests_data else None
        )
        metrics_grants = compute_thread_metrics(grants_data) if grants_data else None
        if metrics_grants:
            grants_metrics.append(metrics_grants)
        if metrics_requests:
            requests_metrics.append(metrics_requests)

    if not grants_metrics:
        return metrics

    avg = lambda xs: (sum(xs) / len(xs)) if xs else 0.0

    # Core utilization metrics
    metrics["compute_util"] = avg(
        [float(m["compute"]["utilization"]) for m in grants_metrics]
    )
    metrics["fpu_rate"] = avg([float(m["compute"]["fpu_rate"]) for m in grants_metrics])
    metrics["sfpu_rate"] = avg(
        [float(m["compute"]["sfpu_rate"]) for m in grants_metrics]
    )
    metrics["unpack_util"] = avg(
        [float(m["unpack"]["utilization"]) for m in grants_metrics]
    )
    metrics["pack_util"] = avg(
        [float(m["pack"]["utilization"]) for m in grants_metrics]
    )

    # L1/NoC metrics
    metrics["l1_congestion_index"] = avg(
        [float(m["l1"]["congestion_index"]) for m in grants_metrics]
    )
    metrics["noc_txn_per_cycle"] = avg(
        [float(m["l1"]["noc_txn_per_cycle"]) for m in grants_metrics]
    )
    metrics["noc_bytes_per_cycle"] = avg(
        [float(m["l1"]["noc_bytes_per_cycle"]) for m in grants_metrics]
    )

    # RISC metrics
    metrics["risc_stall_rate"] = avg(
        [float(m["risc"]["stall_rate"]) for m in grants_metrics]
    )
    metrics["risc_issue_rate"] = avg(
        [float(m["risc"]["instr_issue_rate"]) for m in grants_metrics]
    )

    # Bandwidth estimates
    metrics["unpack_bw_bytes_per_cycle"] = (
        metrics["unpack_util"] * hw["unpack_max_bytes_per_cycle"]
    )
    metrics["pack_bw_bytes_per_cycle"] = (
        metrics["pack_util"] * hw["pack_max_bytes_per_cycle"]
    )

    # Bound classification scores
    metrics["math_bound_score"] = metrics["compute_util"]
    metrics["unpack_bound_score"] = metrics["unpack_util"]
    metrics["pack_bound_score"] = metrics["pack_util"]
    metrics["risc_bound_score"] = metrics["risc_stall_rate"] - (
        metrics["risc_issue_rate"] * 0.5
    )

    # Compute vs data movement
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

    # Acceptance ratios (REQUESTS vs GRANTS)
    sum_grants = lambda fn: sum([fn(m) for m in grants_metrics])
    sum_requests = lambda fn: (
        sum([fn(m) for m in requests_metrics]) if requests_metrics else 0.0
    )

    total_noc_grants = sum_grants(
        lambda m: float(m["l1"]["noc_in_counts"] + m["l1"]["noc_out_counts"])
    )
    total_noc_requests = sum_requests(
        lambda m: float(m["l1"]["noc_in_counts"] + m["l1"]["noc_out_counts"])
    )
    metrics["noc_acceptance"] = (
        (total_noc_grants / total_noc_requests) if total_noc_requests else 1.0
    )

    total_unpack_grants = sum_grants(lambda m: float(m["unpack"]["busy_counts"]))
    total_unpack_requests = sum_requests(lambda m: float(m["unpack"]["busy_counts"]))
    metrics["unpack_acceptance"] = (
        (total_unpack_grants / total_unpack_requests) if total_unpack_requests else 1.0
    )

    total_pack_grants = sum_grants(lambda m: float(m["pack"]["busy_counts"]))
    total_pack_requests = sum_requests(lambda m: float(m["pack"]["busy_counts"]))
    metrics["pack_acceptance"] = (
        (total_pack_grants / total_pack_requests) if total_pack_requests else 1.0
    )

    # Wall cycles (max across threads)
    metrics["wall_cycles"] = max([float(m["wall_cycles"]) for m in grants_metrics])

    return metrics


def print_kernel_metrics(
    results_requests: Dict[str, List[CounterResult]],
    results_grants: Dict[str, List[CounterResult]],
) -> None:
    """
    Print computed kernel metrics to console in a readable format.

    Args:
        results_requests: Counter results from REQUESTS mode.
        results_grants: Counter results from GRANTS mode.
    """
    metrics = compute_kernel_metrics(results_requests, results_grants)

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
    kernel_bound = metrics.get("kernel_bound", "unknown")
    dominant = metrics.get("dominant_bound", "unknown")
    dominant_score = metrics.get("dominant_bound_score", 0)
    print(f"│  Kernel Bound:    {kernel_bound:<56} │")
    print(f"│  Dominant:        {dominant:<40} (score: {dominant_score:.3f})  │")
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
    scores_sorted = sorted(scores, key=lambda x: x[1], reverse=True)
    for i, (name, score) in enumerate(scores_sorted, 1):
        rank_str = f"#{i}" + (" (dominant)" if i == 1 else "")
        print(f"│  {name:<30} {score:>15.4f} {rank_str:<28} │")
    print(f"└{'─' * 78}┘")

    # Wall cycles
    print(f"\n  Wall Cycles: {int(metrics.get('wall_cycles', 0)):,}")
    print("\n" + "=" * 80 + "\n")


def export_metrics_csv(
    results_requests: Dict[str, List[CounterResult]],
    results_grants: Dict[str, List[CounterResult]],
    filename: str,
    test_params: Dict[str, object] = None,
    worker_id: str = "gw0",
) -> None:
    """
    Export computed metrics to CSV file in perf_data directory.

    Args:
        results_requests: Counter results from REQUESTS mode.
        results_grants: Counter results from GRANTS mode.
        filename: Base filename (without extension), e.g., "test_matmul_metrics".
        test_params: Optional dictionary of test parameters to include in each row.
        worker_id: Worker ID for parallel test runs (e.g., "gw0", "master").
    """
    perf_dir = TestConfig.LLK_ROOT / "perf_data"
    perf_dir.mkdir(parents=True, exist_ok=True)

    # Compute kernel metrics
    metrics = compute_kernel_metrics(results_requests, results_grants)

    if not metrics:
        return

    # Add test parameters
    if test_params:
        metrics.update(test_params)

    # Create single-row DataFrame
    df = pd.DataFrame([metrics])

    output_path = perf_dir / f"{filename}.{worker_id}.csv"

    # Append to existing file or create new
    if output_path.exists():
        existing = pd.read_csv(output_path)
        df = pd.concat([existing, df], ignore_index=True)

    df.to_csv(output_path, index=False)
