# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
#
# SPDX-License-Identifier: Apache-2.0

"""
Automatic Performance Profiler Analyzer

This module automatically analyzes comprehensive performance counter data
collected from C++ kernels. Just call analyze_performance() with the location
and it will print detailed insights about bottlenecks, stalls, and utilization.
"""

from dataclasses import dataclass
from typing import Dict, List, Optional, Tuple

from ttexalens.tt_exalens_lib import read_words_from_device

# CFG register base (matches C++ PERF_COUNTER_CFG_BASE = 100)
# CFG registers are at TENSIX_CFG_BASE (0x00820000 for quasar, read via register_store)
PERF_COUNTER_CFG_BASE = 100


# Counter indices in perf_data array (matches C++ perf_indices namespace offsets from PERF_COUNTER_CFG_BASE)
class PerfIndices:
    # Instruction counters (stored at CFG_BASE+0 through CFG_BASE+11)
    INST_UNPACK_CYCLES = 0
    INST_UNPACK_COUNT = 1
    INST_MATH_CYCLES = 2
    INST_MATH_COUNT = 3
    INST_PACK_CYCLES = 4
    INST_PACK_COUNT = 5
    INST_SYNC_CYCLES = 6
    INST_SYNC_COUNT = 7
    INST_CFG_CYCLES = 8
    INST_CFG_COUNT = 9
    INST_MOVE_CYCLES = 10
    INST_MOVE_COUNT = 11

    # TDMA counters (stored at CFG_BASE+12 through CFG_BASE+19)
    UNPACK_DMA_CYCLES = 12
    UNPACK_DMA_BUSY = 13
    PACK_DMA_CYCLES = 14
    PACK_DMA_BUSY = 15
    MATH_INSTR_VALID_CYCLES = 16
    MATH_INSTR_VALID_COUNT = 17
    SRCB_WREN_CYCLES = 18
    SRCB_WREN_COUNT = 19

    # Stall counters (stored at CFG_BASE+20 through CFG_BASE+31)
    SRCA_STALL_CYCLES = 20
    SRCA_STALL_COUNT = 21
    SRCB_STALL_CYCLES = 22
    SRCB_STALL_COUNT = 23
    DEST_STALL_CYCLES = 24
    DEST_STALL_COUNT = 25
    MATH_STALL_CYCLES = 26
    MATH_STALL_COUNT = 27
    PACK_STALL_CYCLES = 28
    PACK_STALL_COUNT = 29
    UNPACK_STALL_CYCLES = 30
    UNPACK_STALL_COUNT = 31

    TOTAL_CFGS = 32  # Total CFG registers used


@dataclass
class ThreadMetrics:
    """Performance metrics for a single thread"""

    name: str
    cycles: int
    instructions: int
    utilization: float
    dma_busy_cycles: int
    dma_utilization: float
    stall_cycles: int
    stall_percentage: float


@dataclass
class PerformanceAnalysis:
    """Complete performance analysis results"""

    total_cycles: int
    unpack: Optional[ThreadMetrics]
    math: Optional[ThreadMetrics]
    pack: Optional[ThreadMetrics]
    bottleneck: str
    bottleneck_reason: str
    recommendations: List[str]
    extra_counters: Optional[Dict] = (
        None  # Extra INSTRN_THREAD counters via counter_sel switching
    )


def analyze_performance(
    location: str = "0,0",
    workload_info: Optional[Dict] = None,
    iteration: int = 0,
    iteration_data: Optional[List[Dict]] = None,
) -> Optional[PerformanceAnalysis]:
    """
    Automatically analyze all performance counters and provide insights

    Args:
        location: Device location (default: "0,0")
        workload_info: Optional dict with 'tile_ops' and 'macs' for context
        iteration: Which profiling iteration (0=default 5 counters, 1-11=additional signals)
        iteration_data: Optional list of dicts with multi-iteration profiling data
                       Each dict: {'iteration': int, 'data': list of counter values}

    Returns:
        PerformanceAnalysis object or None if no data available
    """
    try:
        # If multi-iteration data provided, use the first iteration for baseline analysis
        # (all iterations should have same overall cycles, just different signal selections)
        if iteration_data:
            perf_data = iteration_data[0]["data"]
        else:
            # Read counter data from L1 (0x2F800)
            # 32 baseline words (5 counters × 3 threads × 2 values each, plus spacing)
            perf_data = read_words_from_device(
                location=location, addr=0x2F800, word_count=32
            )

        if not perf_data or len(perf_data) < 32:
            return None

        # Check if any thread was active (marker = 0xFFFFFFFF during profiling)
        if all(
            perf_data[i] == 0
            for i in [
                PerfIndices.INST_UNPACK_CYCLES,
                PerfIndices.INST_MATH_CYCLES,
                PerfIndices.INST_PACK_CYCLES,
            ]
        ):
            return None

        # Parse UNPACK thread metrics
        unpack = None
        if perf_data[PerfIndices.INST_UNPACK_CYCLES] not in [0, 0xFFFFFFFF]:
            unpack_cycles = perf_data[PerfIndices.INST_UNPACK_CYCLES]
            unpack_instr = perf_data[PerfIndices.INST_UNPACK_COUNT]
            unpack_dma_busy = perf_data[PerfIndices.UNPACK_DMA_BUSY]
            unpack_stall = perf_data[PerfIndices.SRCB_STALL_COUNT]

            unpack = ThreadMetrics(
                name="UNPACK",
                cycles=unpack_cycles,
                instructions=unpack_instr,
                utilization=(
                    (unpack_instr / unpack_cycles * 100) if unpack_cycles > 0 else 0
                ),
                dma_busy_cycles=unpack_dma_busy,
                dma_utilization=(
                    (unpack_dma_busy / unpack_cycles * 100) if unpack_cycles > 0 else 0
                ),
                stall_cycles=unpack_stall,
                stall_percentage=(
                    (unpack_stall / unpack_cycles * 100) if unpack_cycles > 0 else 0
                ),
            )

        # Parse MATH thread metrics
        math = None
        if perf_data[PerfIndices.INST_MATH_CYCLES] not in [0, 0xFFFFFFFF]:
            math_cycles = perf_data[PerfIndices.INST_MATH_CYCLES]
            math_instr = perf_data[PerfIndices.INST_MATH_COUNT]
            math_valid = perf_data[PerfIndices.MATH_INSTR_VALID_COUNT]
            math_stall = perf_data[PerfIndices.MATH_STALL_COUNT]

            math = ThreadMetrics(
                name="MATH",
                cycles=math_cycles,
                instructions=math_instr,
                utilization=(math_instr / math_cycles * 100) if math_cycles > 0 else 0,
                dma_busy_cycles=math_valid,
                dma_utilization=(
                    (math_valid / math_cycles * 100) if math_cycles > 0 else 0
                ),
                stall_cycles=math_stall,
                stall_percentage=(
                    (math_stall / math_cycles * 100) if math_cycles > 0 else 0
                ),
            )

        # Parse PACK thread metrics
        pack = None
        if perf_data[PerfIndices.INST_PACK_CYCLES] not in [0, 0xFFFFFFFF]:
            pack_cycles = perf_data[PerfIndices.INST_PACK_CYCLES]
            pack_instr = perf_data[PerfIndices.INST_PACK_COUNT]
            pack_dma_busy = perf_data[PerfIndices.PACK_DMA_BUSY]
            pack_stall = perf_data[PerfIndices.PACK_STALL_COUNT]

            pack = ThreadMetrics(
                name="PACK",
                cycles=pack_cycles,
                instructions=pack_instr,
                utilization=(pack_instr / pack_cycles * 100) if pack_cycles > 0 else 0,
                dma_busy_cycles=pack_dma_busy,
                dma_utilization=(
                    (pack_dma_busy / pack_cycles * 100) if pack_cycles > 0 else 0
                ),
                stall_cycles=pack_stall,
                stall_percentage=(
                    (pack_stall / pack_cycles * 100) if pack_cycles > 0 else 0
                ),
            )

        # Parse extra counters from atomic counter_sel switching (if available)
        extra_counters = None
        if len(perf_data) >= 50:
            extra_counters = {
                "unpack": {
                    "INST_CFG": {"cycles": perf_data[32], "count": perf_data[33]},
                    "INST_STALLED": {"cycles": perf_data[34], "count": perf_data[35]},
                    "UNPACK_BUSY": {"cycles": perf_data[36], "count": perf_data[37]},
                },
                "math": {
                    "INST_UNPACK": {"cycles": perf_data[38], "count": perf_data[39]},
                    "INST_STALLED": {"cycles": perf_data[40], "count": perf_data[41]},
                    "MATH_BUSY": {"cycles": perf_data[42], "count": perf_data[43]},
                },
                "pack": {
                    "INST_MATH": {"cycles": perf_data[44], "count": perf_data[45]},
                    "INST_STALLED": {"cycles": perf_data[46], "count": perf_data[47]},
                    "PACK_BUSY": {"cycles": perf_data[48], "count": perf_data[49]},
                },
            }

        # Determine total cycles (max of all threads)
        total_cycles = max(
            unpack.cycles if unpack else 0,
            math.cycles if math else 0,
            pack.cycles if pack else 0,
        )

        # Analyze bottleneck
        bottleneck, reason = _identify_bottleneck(unpack, math, pack)

        # Generate recommendations
        recommendations = _generate_recommendations(
            unpack, math, pack, bottleneck, workload_info
        )

        return PerformanceAnalysis(
            total_cycles=total_cycles,
            unpack=unpack,
            math=math,
            pack=pack,
            bottleneck=bottleneck,
            bottleneck_reason=reason,
            recommendations=recommendations,
            extra_counters=extra_counters if extra_counters else None,
        )

    except Exception as e:
        print(f"Warning: Could not analyze performance counters: {e}")
        return None


def _identify_bottleneck(
    unpack: Optional[ThreadMetrics],
    math: Optional[ThreadMetrics],
    pack: Optional[ThreadMetrics],
) -> Tuple[str, str]:
    """Identify the pipeline bottleneck"""

    threads = []
    if unpack:
        threads.append(
            (
                "UNPACK",
                unpack.utilization,
                unpack.dma_utilization,
                unpack.stall_percentage,
            )
        )
    if math:
        threads.append(
            ("MATH", math.utilization, math.dma_utilization, math.stall_percentage)
        )
    if pack:
        threads.append(
            ("PACK", pack.utilization, pack.dma_utilization, pack.stall_percentage)
        )

    if not threads:
        return "UNKNOWN", "No profiling data available"

    # Check if all threads have similar low utilization
    avg_util = sum(t[1] for t in threads) / len(threads)
    if avg_util < 20:
        # Check DMA saturation
        if unpack and unpack.dma_utilization > 70:
            return "MEMORY", f"UNPACK DMA saturated at {unpack.dma_utilization:.1f}%"
        if pack and pack.dma_utilization > 70:
            return "MEMORY", f"PACK DMA saturated at {pack.dma_utilization:.1f}%"

        # Check stalls
        max_stall = max((t[3] for t in threads), default=0)
        if max_stall > 50:
            stall_thread = max(threads, key=lambda t: t[3])
            return (
                stall_thread[0],
                f"{stall_thread[0]} stalled {stall_thread[3]:.1f}% waiting for data",
            )

        return (
            "OVERHEAD",
            f"Low utilization across all stages ({avg_util:.1f}% avg) - likely setup overhead",
        )

    # Find lowest utilization thread
    bottleneck_thread = min(threads, key=lambda t: t[1])
    return bottleneck_thread[0], f"Lowest utilization at {bottleneck_thread[1]:.1f}%"


def _generate_recommendations(
    unpack: Optional[ThreadMetrics],
    math: Optional[ThreadMetrics],
    pack: Optional[ThreadMetrics],
    bottleneck: str,
    workload_info: Optional[Dict],
) -> List[str]:
    """Generate actionable recommendations"""

    recommendations = []

    if bottleneck == "MEMORY":
        recommendations.append("🔴 MEMORY-BOUND: Optimize data movement")
        recommendations.append("   - Increase tile batching to amortize DMA overhead")
        recommendations.append("   - Check L1 memory layout for bank conflicts")
        recommendations.append("   - Consider data reuse strategies")

    elif bottleneck == "OVERHEAD":
        recommendations.append("🟡 OVERHEAD-DOMINATED: Workload too small")
        recommendations.append("   - Increase workload size to amortize setup costs")
        if workload_info and workload_info.get("tile_ops", 0) < 8:
            recommendations.append(
                f"   - Current: {workload_info['tile_ops']} tile ops (try 16+)"
            )
        recommendations.append("   - Setup overhead is fixed; scale computation")

    elif bottleneck == "MATH":
        if math and math.stall_percentage > 50:
            recommendations.append("🔴 MATH STARVED: Waiting for input data")
            recommendations.append("   - UNPACK cannot keep up with MATH demand")
            recommendations.append("   - Optimize unpacker or reduce math fidelity")
        else:
            recommendations.append("🟡 MATH LIMITED: Computational bottleneck")
            recommendations.append("   - Consider lower fidelity mode if acceptable")
            recommendations.append("   - Pipeline is balanced; optimize math kernel")

    elif bottleneck == "UNPACK":
        if unpack and unpack.dma_utilization > 70:
            recommendations.append("🔴 UNPACK DMA SATURATED: Memory bandwidth limit")
            recommendations.append("   - Memory system cannot feed data fast enough")
            recommendations.append("   - Optimize data layout or access patterns")
        else:
            recommendations.append("🟡 UNPACK LIMITED: Data preparation bottleneck")
            recommendations.append("   - Optimize unpacker configuration")
            recommendations.append("   - Consider different data formats")

    elif bottleneck == "PACK":
        if pack and pack.dma_utilization > 70:
            recommendations.append("🔴 PACK DMA SATURATED: Write bandwidth limit")
            recommendations.append("   - Memory writes cannot keep up with computation")
            recommendations.append("   - Consider write-combining or buffering")
        else:
            recommendations.append("🟡 PACK LIMITED: Output preparation bottleneck")
            recommendations.append("   - Optimize packer configuration")
            recommendations.append("   - Check destination format efficiency")

    # Add utilization insights
    if unpack and math and pack:
        util_spread = max(unpack.utilization, math.utilization, pack.utilization) - min(
            unpack.utilization, math.utilization, pack.utilization
        )
        if util_spread < 5:
            recommendations.append(
                "✅ BALANCED PIPELINE: All stages have similar utilization"
            )
        else:
            recommendations.append(
                f"⚠️  IMBALANCED PIPELINE: {util_spread:.1f}% utilization spread"
            )

    return recommendations


def print_performance_analysis(
    analysis: Optional[PerformanceAnalysis],
    workload_info: Optional[Dict] = None,
    iteration_data: Optional[List[Dict]] = None,
):
    """
    Print performance counter data from multi-iteration profiling (comprehensive multi-category coverage)

    Args:
        analysis: PerformanceAnalysis object from analyze_performance()
        workload_info: Optional dict with 'tile_ops' and 'macs' for context
        iteration_data: Optional list of dicts with multi-iteration data
    """
    if not analysis:
        return

    print("\n" + "=" * 100)
    num_iterations = len(iteration_data) if iteration_data else 0
    print(
        f"⚡ PERFORMANCE COUNTER DATA (Multi-Iteration: {num_iterations} iterations, measuring all 66 counters)"
    )
    print("=" * 100)

    # Define signal arrays (matching C++ perf_counters.h)
    # UNPACK: 25 counters across 5 iterations (5 per iteration)
    # Only FPU and TDMA_PACK are unused placeholders for UNPACK thread
    unpack_signal_names = [
        "INST_CFG",
        "FPU_OP_VALID",
        "MATH_INSTR_SRC_READY",
        "UNPACK_NO_ARB",
        "DSTAC_RDEN_RAW_0",
        "INST_SYNC",
        "SFPU_OP_VALID",
        "MATH_NOT_D2A_STALL",
        "UNPACK_ARB_1",
        "DSTAC_RDEN_RAW_1",
        "INST_THCON",
        "FPU_OP_VALID",
        "MATH_FIDELITY_PHASES",
        "TDMA_BUNDLE_0_ARB",
        "DSTAC_RDEN_RAW_2",
        "INST_XSEARCH",
        "SFPU_OP_VALID",
        "MATH_INSTR_BUF_RDEN",
        "TDMA_BUNDLE_1_ARB",
        "DSTAC_RDEN_RAW_3",
        "INST_MOVE",
        "FPU_OP_VALID",
        "MATH_INSTR_VALID",
        "NOC_RING0_OUTGOING_0",
        "PACK_NOT_DEST_STALL",
        "INST_MATH",
        "SFPU_OP_VALID",
        "TDMA_SRCB_REGIF_WREN",
        "NOC_RING0_OUTGOING_1",
        "PACK_NOT_SB_STALL",
        "INST_UNPACK",
        "FPU_OP_VALID",
        "TDMA_SRCA_REGIF_WREN",
        "NOC_RING0_INCOMING_0",
        "PACK_BUSY_10",
        "INST_PACK",
        "SFPU_OP_VALID",
        "UNPACK_BUSY_0",
        "NOC_RING0_INCOMING_1",
        "PACK_BUSY_11",
        "STALLED",
        "FPU_OP_VALID",
        "UNPACK_BUSY_1",
        "TDMA_PACKER_2_WR",
        "DSTAC_RDEN_RAW_0",
        "SRCA_CLEARED_0",
        "SFPU_OP_VALID",
        "UNPACK_BUSY_2",
        "TDMA_EXT_UNPACK_9",
        "DSTAC_RDEN_RAW_1",
        "SRCA_CLEARED_1",
        "FPU_OP_VALID",
        "UNPACK_BUSY_3",
        "TDMA_EXT_UNPACK_10",
        "DSTAC_RDEN_RAW_2",
        "SRCA_CLEARED_2",
        "SFPU_OP_VALID",
        "MATH_INSTR_SRC_READY",
        "TDMA_EXT_UNPACK_11",
        "DSTAC_RDEN_RAW_3",
        "SRCB_CLEARED_0",
        "FPU_OP_VALID",
        "MATH_NOT_D2A_STALL",
        "NOC_RING1_OUTGOING_0",
        "PACK_NOT_DEST_STALL",
        "SRCB_CLEARED_1",
        "SFPU_OP_VALID",
        "MATH_FIDELITY_PHASES",
        "NOC_RING1_OUTGOING_1",
        "PACK_NOT_SB_STALL",
    ]

    # MATH: 70 counters across 14 iterations (5 per iteration)
    math_signal_names = [
        "INST_CFG",
        "FPU_OP_VALID",
        "MATH_INSTR_SRC_READY",
        "UNPACK_NO_ARB",
        "DSTAC_RDEN_RAW_0",
        "INST_SYNC",
        "SFPU_OP_VALID",
        "MATH_NOT_D2A_STALL",
        "UNPACK_ARB_1",
        "DSTAC_RDEN_RAW_1",
        "INST_THCON",
        "FPU_OP_VALID",
        "MATH_FIDELITY_PHASES",
        "TDMA_BUNDLE_0_ARB",
        "DSTAC_RDEN_RAW_2",
        "INST_XSEARCH",
        "SFPU_OP_VALID",
        "MATH_INSTR_BUF_RDEN",
        "TDMA_BUNDLE_1_ARB",
        "DSTAC_RDEN_RAW_3",
        "INST_MOVE",
        "FPU_OP_VALID",
        "MATH_INSTR_VALID",
        "NOC_RING0_OUTGOING_0",
        "PACK_NOT_DEST_STALL",
        "INST_MATH",
        "SFPU_OP_VALID",
        "TDMA_SRCB_REGIF_WREN",
        "NOC_RING0_OUTGOING_1",
        "PACK_NOT_SB_STALL",
        "INST_UNPACK",
        "FPU_OP_VALID",
        "TDMA_SRCA_REGIF_WREN",
        "NOC_RING0_INCOMING_0",
        "PACK_BUSY_10",
        "INST_PACK",
        "SFPU_OP_VALID",
        "UNPACK_BUSY_0",
        "NOC_RING0_INCOMING_1",
        "PACK_BUSY_11",
        "STALLED",
        "FPU_OP_VALID",
        "UNPACK_BUSY_1",
        "TDMA_PACKER_2_WR",
        "DSTAC_RDEN_RAW_0",
        "SRCA_CLEARED_0",
        "SFPU_OP_VALID",
        "UNPACK_BUSY_2",
        "TDMA_EXT_UNPACK_9",
        "DSTAC_RDEN_RAW_1",
        "SRCA_CLEARED_1",
        "FPU_OP_VALID",
        "UNPACK_BUSY_3",
        "TDMA_EXT_UNPACK_10",
        "DSTAC_RDEN_RAW_2",
        "SRCA_CLEARED_2",
        "SFPU_OP_VALID",
        "MATH_INSTR_SRC_READY",
        "TDMA_EXT_UNPACK_11",
        "DSTAC_RDEN_RAW_3",
        "SRCB_CLEARED_0",
        "FPU_OP_VALID",
        "MATH_NOT_D2A_STALL",
        "NOC_RING1_OUTGOING_0",
        "PACK_NOT_DEST_STALL",
        "SRCB_CLEARED_1",
        "SFPU_OP_VALID",
        "MATH_FIDELITY_PHASES",
        "NOC_RING1_OUTGOING_1",
        "PACK_NOT_SB_STALL",
    ]

    # PACK: 70 counters across 14 iterations (5 per iteration)
    pack_signal_names = [
        "INST_CFG",
        "FPU_OP_VALID",
        "MATH_INSTR_SRC_READY",
        "UNPACK_NO_ARB",
        "DSTAC_RDEN_RAW_0",
        "INST_SYNC",
        "SFPU_OP_VALID",
        "MATH_NOT_D2A_STALL",
        "UNPACK_ARB_1",
        "DSTAC_RDEN_RAW_1",
        "INST_THCON",
        "FPU_OP_VALID",
        "MATH_FIDELITY_PHASES",
        "TDMA_BUNDLE_0_ARB",
        "DSTAC_RDEN_RAW_2",
        "INST_XSEARCH",
        "SFPU_OP_VALID",
        "MATH_INSTR_BUF_RDEN",
        "TDMA_BUNDLE_1_ARB",
        "DSTAC_RDEN_RAW_3",
        "INST_MOVE",
        "FPU_OP_VALID",
        "MATH_INSTR_VALID",
        "NOC_RING0_OUTGOING_0",
        "PACK_NOT_DEST_STALL",
        "INST_MATH",
        "SFPU_OP_VALID",
        "TDMA_SRCB_REGIF_WREN",
        "NOC_RING0_OUTGOING_1",
        "PACK_NOT_SB_STALL",
        "INST_UNPACK",
        "FPU_OP_VALID",
        "TDMA_SRCA_REGIF_WREN",
        "NOC_RING0_INCOMING_0",
        "PACK_BUSY_10",
        "INST_PACK",
        "SFPU_OP_VALID",
        "UNPACK_BUSY_0",
        "NOC_RING0_INCOMING_1",
        "PACK_BUSY_11",
        "STALLED",
        "FPU_OP_VALID",
        "UNPACK_BUSY_1",
        "TDMA_PACKER_2_WR",
        "DSTAC_RDEN_RAW_0",
        "SRCA_CLEARED_0",
        "SFPU_OP_VALID",
        "UNPACK_BUSY_2",
        "TDMA_EXT_UNPACK_9",
        "DSTAC_RDEN_RAW_1",
        "SRCA_CLEARED_1",
        "FPU_OP_VALID",
        "UNPACK_BUSY_3",
        "TDMA_EXT_UNPACK_10",
        "DSTAC_RDEN_RAW_2",
        "SRCA_CLEARED_2",
        "SFPU_OP_VALID",
        "MATH_INSTR_SRC_READY",
        "TDMA_EXT_UNPACK_11",
        "DSTAC_RDEN_RAW_3",
        "SRCB_CLEARED_0",
        "FPU_OP_VALID",
        "MATH_NOT_D2A_STALL",
        "NOC_RING1_OUTGOING_0",
        "PACK_NOT_DEST_STALL",
        "SRCB_CLEARED_1",
        "SFPU_OP_VALID",
        "MATH_FIDELITY_PHASES",
        "NOC_RING1_OUTGOING_1",
        "PACK_NOT_SB_STALL",
    ]

    if iteration_data:
        # Multi-iteration mode: aggregate results from all iterations
        # Each iteration measures 5 signals per thread
        # L1 layout: UNPACK[0,1], MATH[2,3], PACK[4,5], (spacing), UNPACK[6,7], ...

        # Build dictionaries mapping signal name to (cycles, count)
        unpack_results = {}
        math_results = {}
        pack_results = {}

        for iter_info in iteration_data:
            iteration = iter_info["iteration"]
            perf_data = iter_info["data"]
            base_signal = iteration * 5  # 5 signals per iteration

            # Read 5 counter banks (0-4), each writes UNPACK, MATH, PACK values
            # Layout matches C++ stop_profiling():
            #   l1_mem[0,1]=UNPACK counter1, l1_mem[2,3]=MATH counter1, l1_mem[4,5]=PACK counter1
            #   l1_mem[6,7]=UNPACK counter2, l1_mem[8,9]=MATH counter2, l1_mem[10,11]=PACK counter2
            # But with spacing: UNPACK uses [0,6,12,20,22], MATH [2,8,16,24,26], PACK [4,10,14,28,30]

            # NEW LAYOUT: Each thread writes to separate L1 regions
            # UNPACK: offsets 0-139 (14 iterations × 10 words)
            # MATH: offsets 200-339 (14 iterations × 10 words)
            # PACK: offsets 400-539 (14 iterations × 10 words)
            # Each iteration has 10 words: 5 counters × 2 words (cycles, count)
            # Layout per iteration: [INSTRN(0,1), FPU(2,3), TDMA_UNPACK/PACK(4,5), L1(6,7), TDMA_PACK/UNPACK(8,9)]

            for i in range(5):
                signal_idx = base_signal + i

                # UNPACK: read from offset 0 + iteration*10 + i*2
                if signal_idx < len(unpack_signal_names):
                    offset = 0 + (iteration * 10) + (i * 2)
                    if offset < len(perf_data) and offset + 1 < len(perf_data):
                        cycles = perf_data[offset]
                        count = perf_data[offset + 1]
                        signal_name = unpack_signal_names[signal_idx]
                        unpack_results[signal_name] = (cycles, count)

                # MATH: read from offset 200 + iteration*10 + i*2
                if signal_idx < len(math_signal_names):
                    offset = 200 + (iteration * 10) + (i * 2)
                    if offset < len(perf_data) and offset + 1 < len(perf_data):
                        cycles = perf_data[offset]
                        count = perf_data[offset + 1]
                        signal_name = math_signal_names[signal_idx]
                        math_results[signal_name] = (cycles, count)

                # PACK: read from offset 400 + iteration*10 + i*2
                if signal_idx < len(pack_signal_names):
                    offset = 400 + (iteration * 10) + (i * 2)
                    if offset < len(perf_data) and offset + 1 < len(perf_data):
                        cycles = perf_data[offset]
                        count = perf_data[offset + 1]
                        signal_name = pack_signal_names[signal_idx]
                        pack_results[signal_name] = (cycles, count)

        # Sort by cycles value to check for patterns
        print("\n🔧 UNPACK Thread (TRISC0) - Sorted by cycles:")
        sorted_unpack = sorted(unpack_results.items(), key=lambda x: x[1][0])
        for signal_name, (cycles, count) in sorted_unpack:
            print(f"   {signal_name:25s}: cycles={cycles:10d}, count={count:8d}")

        print("\n🔧 MATH Thread (TRISC1) - Sorted by cycles:")
        sorted_math = sorted(math_results.items(), key=lambda x: x[1][0])
        for signal_name, (cycles, count) in sorted_math:
            print(f"   {signal_name:25s}: cycles={cycles:10d}, count={count:8d}")

        print("\n🔧 PACK Thread (TRISC2) - Sorted by cycles:")
        sorted_pack = sorted(pack_results.items(), key=lambda x: x[1][0])
        for signal_name, (cycles, count) in sorted_pack:
            print(f"   {signal_name:25s}: cycles={cycles:10d}, count={count:8d}")
    else:
        # Legacy single-iteration mode
        print(
            "\n⚠️  No multi-iteration data provided - run test with multiple iterations for full signal coverage"
        )

    print("\n" + "=" * 100 + "\n")
