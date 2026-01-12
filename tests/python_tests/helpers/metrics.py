# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0

from typing import Dict, List, Optional

# Results schema is identical to helpers.counters.read_perf_counters output
CounterResult = Dict[str, object]


def _sum_counts(
    results: List[CounterResult], bank: str, names: List[str], mux: Optional[int] = None
) -> int:
    total = 0
    for r in results:
        if r.get("bank") != bank:
            continue
        if r.get("counter_name") in names:
            if mux is None or r.get("mux_ctrl_bit4") == mux:
                total += int(r.get("count", 0))
    return total


def _get_cycles(results: List[CounterResult], bank: str) -> int:
    for r in results:
        if r.get("bank") == bank:
            return int(r.get("cycles", 0))
    return 0


def _rate(count: int, cycles: int) -> float:
    return (count / cycles) if cycles else 0.0


class HWParams:
    def __init__(
        self,
        noc_word_bytes: int = 0,
        unpack_max_bytes_per_cycle: float = 0.0,
        pack_max_bytes_per_cycle: float = 0.0,
    ) -> None:
        # If 0, bandwidths will be reported as transactions/cycle instead of bytes/s
        self.noc_word_bytes = noc_word_bytes
        self.unpack_max_bytes_per_cycle = unpack_max_bytes_per_cycle
        self.pack_max_bytes_per_cycle = pack_max_bytes_per_cycle


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


def compute_thread_metrics(
    results: List[CounterResult], hw: Optional[HWParams] = None
) -> Dict[str, object]:
    hw = hw or HWParams()

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
    noc_bytes_per_cycle = (
        noc_txn_per_cycle * hw.noc_word_bytes if hw.noc_word_bytes else 0.0
    )

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

    # Bandwidth estimates if theoretical maxima are provided
    unpack_bw_bytes_per_cycle = (
        unpack_util * hw.unpack_max_bytes_per_cycle
        if hw.unpack_max_bytes_per_cycle
        else 0.0
    )
    pack_bw_bytes_per_cycle = (
        pack_util * hw.pack_max_bytes_per_cycle if hw.pack_max_bytes_per_cycle else 0.0
    )

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


def summarize_kernel_metrics(
    results_by_thread: Dict[str, List[CounterResult]], hw: Optional[HWParams] = None
) -> str:
    hw = hw or HWParams()
    lines: List[str] = []
    for thread, results in results_by_thread.items():
        m = compute_thread_metrics(results, hw)
        lines.append(f"== {thread} ==")
        lines.append(
            f"cycles(instrn/fpu/unpack/pack/l1) = {m['cycles']['instrn']}/{m['cycles']['fpu']}/{m['cycles']['unpack']}/{m['cycles']['pack']}/{m['cycles']['l1']}"
        )
        lines.append(
            f"compute util={m['compute']['utilization']:.3f} fpu={m['compute']['fpu_rate']:.3f} sfpu={m['compute']['sfpu_rate']:.3f}"
        )
        lines.append(
            f"unpack util={m['unpack']['utilization']:.3f} pack util={m['pack']['utilization']:.3f} L1 cong={m['l1']['congestion_index']:.3f}"
        )
        if hw.noc_word_bytes:
            lines.append(
                f"NoC bytes/cycle ≈ {m['l1']['noc_bytes_per_cycle']:.2f} (txn/cycle {m['l1']['noc_txn_per_cycle']:.2f})"
            )
        else:
            lines.append(
                f"NoC txn/cycle ≈ {m['l1']['noc_txn_per_cycle']:.2f} (set noc_word_bytes to get bytes)"
            )
        if hw.unpack_max_bytes_per_cycle or hw.pack_max_bytes_per_cycle:
            lines.append(
                f"Unpacker est BW={m['unpack']['est_bw_bytes_per_cycle']:.2f} B/cyc, Packer est BW={m['pack']['est_bw_bytes_per_cycle']:.2f} B/cyc"
            )
        lines.append(
            f"RISC stalls={m['risc']['stall_rate']:.3f} instr_issue={m['risc']['instr_issue_rate']:.3f}"
        )
        top_bound = m["bound_classification"][0]
        lines.append(f"Likely bound: {top_bound[0]} (score {top_bound[1]:.3f})")
        lines.append("")
    return "\n".join(lines)


def summarize_kernel_metrics_dual(
    results_by_thread_requests: Dict[str, List[CounterResult]],
    results_by_thread_grants: Dict[str, List[CounterResult]],
    hw: Optional[HWParams] = None,
) -> str:
    """
    Produce a side-by-side summary using both REQUESTS and GRANTS measurements.
    Useful for understanding attempted vs accepted operations and arbitration effects.
    """
    hw = hw or HWParams()
    lines: List[str] = []
    # Aggregate metrics (use GRANTS for truth, REQUESTS for pressure where meaningful)
    gr_metrics: List[Dict[str, object]] = []
    req_metrics: List[Dict[str, object]] = []
    for thread in sorted(
        set(
            list(results_by_thread_requests.keys())
            + list(results_by_thread_grants.keys())
        )
    ):
        req = results_by_thread_requests.get(thread, [])
        gr = results_by_thread_grants.get(thread, [])
        m_req = compute_thread_metrics(req, hw) if req else None
        m_gr = compute_thread_metrics(gr, hw) if gr else None
        if m_gr:
            gr_metrics.append(m_gr)
        if m_req:
            req_metrics.append(m_req)

    # Kernel-level overview (averaged across threads)
    if gr_metrics:
        avg = lambda xs: (sum(xs) / len(xs)) if xs else 0.0
        avg_compute_util = avg([float(m["compute"]["utilization"]) for m in gr_metrics])
        avg_fpu_rate = avg([float(m["compute"]["fpu_rate"]) for m in gr_metrics])
        avg_sfpu_rate = avg([float(m["compute"]["sfpu_rate"]) for m in gr_metrics])
        avg_unpack_util = avg([float(m["unpack"]["utilization"]) for m in gr_metrics])
        avg_pack_util = avg([float(m["pack"]["utilization"]) for m in gr_metrics])
        avg_l1_cong = avg([float(m["l1"]["congestion_index"]) for m in gr_metrics])
        avg_noc_txn = avg([float(m["l1"]["noc_txn_per_cycle"]) for m in gr_metrics])

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
        avg_stall_rate = avg([float(m["risc"]["stall_rate"]) for m in gr_metrics])
        avg_issue_rate = avg([float(m["risc"]["instr_issue_rate"]) for m in gr_metrics])
        kernel_risc_score = max(avg_stall_rate, avg_issue_rate * 0.0) - (
            avg_issue_rate * 0.5
        )
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
        sum_gr = lambda fn: sum([fn(m) for m in gr_metrics])
        sum_req = lambda fn: sum([fn(m) for m in req_metrics]) if req_metrics else 0.0
        total_arb = sum_gr(lambda m: float(m["l1"]["arb_pulses"]))
        total_no_arb = sum_gr(lambda m: float(m["l1"]["no_arb_pulses"]))
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
        if hw.unpack_max_bytes_per_cycle:
            lines.append(
                _format_bw(
                    "Unpacker BW", avg_unpack_util, hw.unpack_max_bytes_per_cycle
                )
            )
        else:
            lines.append(
                f"Unpacker: util={avg_unpack_util:.3f} (set peak bytes/cycle to show BW)"
            )
        # Math throughput (utilization; not byte bandwidth)
        lines.append(
            f"Math: util={avg_compute_util:.3f} (compute engine throughput; not bytes)"
        )
        # Packer BW
        if hw.pack_max_bytes_per_cycle:
            lines.append(
                _format_bw("Packer BW", avg_pack_util, hw.pack_max_bytes_per_cycle)
            )
        else:
            lines.append(
                f"Packer: util={avg_pack_util:.3f} (set peak bytes/cycle to show BW)"
            )
        # NoC BW
        if hw.noc_word_bytes:
            avg_noc_bpc = avg(
                [float(m["l1"]["noc_bytes_per_cycle"]) for m in gr_metrics]
            )
            lines.append(
                f"NoC BW ≈ {avg_noc_bpc:.2f} B/cyc (txn/cyc {avg_noc_txn:.2f})."
            )
        else:
            lines.append(
                f"NoC activity ≈ {avg_noc_txn:.2f} txn/cyc (set noc_word_bytes for bytes/cycle)."
            )
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
        # Only TDMA (unpack/pack busy) and NIU (ring in/out) have actionable REQ vs GR semantics.
        total_noc_gr = sum_gr(
            lambda m: float(m["l1"]["noc_in_counts"] + m["l1"]["noc_out_counts"])
        )
        total_noc_req = sum_req(
            lambda m: float(m["l1"]["noc_in_counts"] + m["l1"]["noc_out_counts"])
        )
        noc_acceptance = (total_noc_gr / total_noc_req) if total_noc_req else 1.0
        total_unpack_gr = sum_gr(lambda m: float(m["unpack"]["busy_counts"]))
        total_unpack_req = sum_req(lambda m: float(m["unpack"]["busy_counts"]))
        unpack_acceptance = (
            (total_unpack_gr / total_unpack_req) if total_unpack_req else 1.0
        )
        total_pack_gr = sum_gr(lambda m: float(m["pack"]["busy_counts"]))
        total_pack_req = sum_req(lambda m: float(m["pack"]["busy_counts"]))
        pack_acceptance = (total_pack_gr / total_pack_req) if total_pack_req else 1.0

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
            "Note: Arbitration pulses and stall counters are mode-independent; use GRANTS for truth and ignore REQ for those."
        )
        lines.append("")

    # Summary now differentiates REQUESTS vs GRANTS where meaningful.

    return "\n".join(lines)
