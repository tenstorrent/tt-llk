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


def analyze_performance(
    location: str = "0,0", workload_info: Optional[Dict] = None
) -> Optional[PerformanceAnalysis]:
    """
    Automatically analyze all performance counters and provide insights

    Args:
        location: Device location (default: "0,0")
        workload_info: Optional dict with 'tile_ops' and 'macs' for context

    Returns:
        PerformanceAnalysis object or None if no data available
    """
    try:
        # Read counter data from L1 (0x2F800)
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
    analysis: Optional[PerformanceAnalysis], workload_info: Optional[Dict] = None
):
    """
    Print comprehensive performance analysis with insights

    Args:
        analysis: PerformanceAnalysis object from analyze_performance()
        workload_info: Optional dict with 'tile_ops' and 'macs' for context
    """
    if not analysis:
        return

    print("\n" + "=" * 70)
    print("⚡ AUTOMATIC PERFORMANCE ANALYSIS")
    print("=" * 70)

    # Pipeline summary
    print("\n📊 PIPELINE SUMMARY:")
    print(f"   Total execution: {analysis.total_cycles} cycles")

    if workload_info:
        tile_ops = workload_info.get("tile_ops", 0)
        macs = workload_info.get("macs", 0)
        if tile_ops > 0:
            print(f"   Workload: {tile_ops} tile ops, {macs:,} MACs")
            print(f"   Efficiency: {analysis.total_cycles / tile_ops:.1f} cycles/tile")

    # Thread details
    print("\n🔧 THREAD BREAKDOWN:")

    if analysis.unpack:
        u = analysis.unpack
        print(f"\n   UNPACK Thread:")
        print(f"     Instructions: {u.instructions} ({u.utilization:.1f}% util)")
        print(
            f"     DMA Activity: {u.dma_busy_cycles} cycles ({u.dma_utilization:.1f}% busy)"
        )
        print(f"     Stalls:       {u.stall_cycles} cycles ({u.stall_percentage:.1f}%)")

    if analysis.math:
        m = analysis.math
        print(f"\n   MATH Thread:")
        print(f"     Instructions: {m.instructions} ({m.utilization:.1f}% util)")
        print(
            f"     Active:       {m.dma_busy_cycles} cycles ({m.dma_utilization:.1f}%)"
        )
        print(f"     Stalls:       {m.stall_cycles} cycles ({m.stall_percentage:.1f}%)")

    if analysis.pack:
        p = analysis.pack
        print(f"\n   PACK Thread:")
        print(f"     Instructions: {p.instructions} ({p.utilization:.1f}% util)")
        print(
            f"     DMA Activity: {p.dma_busy_cycles} cycles ({p.dma_utilization:.1f}% busy)"
        )
        print(f"     Stalls:       {p.stall_cycles} cycles ({p.stall_percentage:.1f}%)")

    # Bottleneck analysis
    print(f"\n🎯 BOTTLENECK: {analysis.bottleneck}")
    print(f"   {analysis.bottleneck_reason}")

    # Recommendations
    if analysis.recommendations:
        print("\n💡 RECOMMENDATIONS:")
        for rec in analysis.recommendations:
            print(f"   {rec}")

    print("\n" + "=" * 70 + "\n")
