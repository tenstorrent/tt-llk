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
    location: str = "0,0", workload_info: Optional[Dict] = None, iteration: int = 0
) -> Optional[PerformanceAnalysis]:
    """
    Automatically analyze all performance counters and provide insights

    Args:
        location: Device location (default: "0,0")
        workload_info: Optional dict with 'tile_ops' and 'macs' for context
        iteration: Which profiling iteration (0=default 5 counters, 1-11=additional signals)

    Returns:
        PerformanceAnalysis object or None if no data available
    """
    try:
        # Read counter data from L1 (0x2F800)
        # 32 baseline words + 18 extra words from atomic counter_sel switching
        perf_data = read_words_from_device(
            location=location, addr=0x2F800, word_count=50
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
    analysis: Optional[PerformanceAnalysis], workload_info: Optional[Dict] = None
):
    """
    Print all performance counters equally (11 per thread via atomic counter_sel switching)

    Args:
        analysis: PerformanceAnalysis object from analyze_performance()
        workload_info: Optional dict with 'tile_ops' and 'macs' for context
    """
    if not analysis:
        return

    print("\n" + "=" * 90)
    print("⚡ PERFORMANCE COUNTER DATA (11 signals per thread via atomic switching)")
    print("=" * 90)

    # Read all counter data (11 signals × 3 threads × 2 values = 66 words)
    perf_data = read_words_from_device(
        location="0,0", addr=0x2F800, word_count=84  # Extra space for safety
    )

    # UNPACK Thread - 11 signals
    unpack_signals = [
        ("INST_UNPACK", 0, 1),
        ("INST_CFG", 32, 33),
        ("INST_SYNC", 34, 35),
        ("INST_THCON", 36, 37),
        ("INST_XSEARCH", 38, 39),
        ("INST_MOVE", 40, 41),
        ("INST_MATH", 42, 43),
        ("INST_PACK", 44, 45),
        ("STALLED", 46, 47),
        ("SRCA_CLEARED", 48, 49),
        ("SRCB_CLEARED", 50, 51),
    ]

    # MATH Thread - 11 signals
    math_signals = [
        ("INST_MATH", 2, 3),
        ("INST_UNPACK", 44, 45),
        ("INST_PACK", 46, 47),
        ("STALLED", 48, 49),
        ("SRCA_CLEARED", 50, 51),
        ("SRCB_CLEARED", 52, 53),
        ("SRCA_VALID", 54, 55),
        ("SRCB_VALID", 56, 57),
        ("STALL_THCON", 58, 59),
        ("STALL_PACK0", 60, 61),
        ("STALL_MATH", 62, 63),
    ]

    # PACK Thread - 11 signals
    pack_signals = [
        ("INST_PACK", 4, 5),
        ("INST_MATH", 64, 65),
        ("INST_PACK_2", 66, 67),
        ("STALLED", 68, 69),
        ("SRCA_CLEARED", 70, 71),
        ("SRCB_CLEARED", 72, 73),
        ("SRCA_VALID", 74, 75),
        ("SRCB_VALID", 76, 77),
        ("STALL_PACK0", 78, 79),
        ("STALL_MOVE", 80, 81),
        ("STALL_SFPU", 82, 83),
    ]

    print("\n🔧 UNPACK Thread (11 signals):")
    for name, cycles_idx, count_idx in unpack_signals:
        if cycles_idx < len(perf_data) and count_idx < len(perf_data):
            cycles = perf_data[cycles_idx]
            count = perf_data[count_idx]
            if cycles not in [0, 0xFFFFFFFF]:
                print(f"   {name:20s}: cycles={cycles:10d}, count={count:8d}")

    print("\n🔧 MATH Thread (11 signals):")
    for name, cycles_idx, count_idx in math_signals:
        if cycles_idx < len(perf_data) and count_idx < len(perf_data):
            cycles = perf_data[cycles_idx]
            count = perf_data[count_idx]
            if cycles not in [0, 0xFFFFFFFF]:
                print(f"   {name:20s}: cycles={cycles:10d}, count={count:8d}")

    print("\n🔧 PACK Thread (11 signals):")
    for name, cycles_idx, count_idx in pack_signals:
        if cycles_idx < len(perf_data) and count_idx < len(perf_data):
            cycles = perf_data[cycles_idx]
            count = perf_data[count_idx]
            if cycles not in [0, 0xFFFFFFFF]:
                print(f"   {name:20s}: cycles={cycles:10d}, count={count:8d}")

    print("\n" + "=" * 70 + "\n")
