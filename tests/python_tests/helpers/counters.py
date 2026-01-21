# SPDX-FileCopyrightText: © 2026 Tenstorrent AI ULC
#
# SPDX-License-Identifier: Apache-2.0

from collections import defaultdict
from typing import Dict, List, Tuple

from ttexalens.tt_exalens_lib import read_words_from_device, write_words_to_device

# L1 Memory addresses - separate per TRISC thread
# Support up to 66 counters: 66 config words + 132 data words per thread
PERF_COUNTER_UNPACK_CONFIG_ADDR = 0x2F7D0  # 66 words: UNPACK metadata
PERF_COUNTER_UNPACK_DATA_ADDR = 0x2F8D8  # 132 words: UNPACK results
PERF_COUNTER_MATH_CONFIG_ADDR = 0x2FAE8  # 66 words: MATH metadata
PERF_COUNTER_MATH_DATA_ADDR = 0x2FBF0  # 132 words: MATH results
PERF_COUNTER_PACK_CONFIG_ADDR = 0x2FE00  # 66 words: PACK metadata
PERF_COUNTER_PACK_DATA_ADDR = 0x2FF08  # 132 words: PACK results

COUNTER_SLOT_COUNT = 66  # Max counters per thread
COUNTER_DATA_WORD_COUNT = COUNTER_SLOT_COUNT * 2  # 2 words per counter (cycles + count)

_THREAD_ADDRESSES = {
    "UNPACK": (PERF_COUNTER_UNPACK_CONFIG_ADDR, PERF_COUNTER_UNPACK_DATA_ADDR),
    "MATH": (PERF_COUNTER_MATH_CONFIG_ADDR, PERF_COUNTER_MATH_DATA_ADDR),
    "PACK": (PERF_COUNTER_PACK_CONFIG_ADDR, PERF_COUNTER_PACK_DATA_ADDR),
}

COUNTER_BANK_NAMES = {
    0: "INSTRN_THREAD",
    1: "FPU",
    2: "TDMA_UNPACK",
    3: "L1",
    4: "TDMA_PACK",
}

# Reverse lookup: bank name -> bank id (computed once at module load)
_BANK_NAME_TO_ID = {v: k for k, v in COUNTER_BANK_NAMES.items()}

COUNTER_NAMES = {
    "INSTRN_THREAD": {
        0: "INST_CFG",
        1: "INST_SYNC",
        2: "INST_THCON",
        3: "INST_XSEARCH",
        4: "INST_MOVE",
        5: "INST_MATH",
        6: "INST_UNPACK",
        7: "INST_PACK",
        8: "STALLED",
        9: "SRCA_CLEARED_0",
        10: "SRCA_CLEARED_1",
        11: "SRCA_CLEARED_2",
        12: "SRCB_CLEARED_0",
        13: "SRCB_CLEARED_1",
        14: "SRCB_CLEARED_2",
        15: "SRCA_VALID_0",
        16: "SRCA_VALID_1",
        17: "SRCA_VALID_2",
        18: "SRCB_VALID_0",
        19: "SRCB_VALID_1",
        20: "SRCB_VALID_2",
        21: "STALL_THCON",
        22: "STALL_PACK0",
        23: "STALL_MATH",
        24: "STALL_SEM_ZERO",
        25: "STALL_SEM_MAX",
        26: "STALL_MOVE",
        27: "STALL_TRISC_REG_ACCESS",
        28: "STALL_SFPU",
    },
    "FPU": {
        0: "FPU_OP_VALID",
        1: "SFPU_OP_VALID",
    },
    "TDMA_UNPACK": {
        0: "MATH_INSTR_SRC_READY",
        1: "MATH_NOT_D2A_STALL",
        2: "MATH_FIDELITY_PHASES",
        3: "MATH_INSTR_BUF_RDEN",
        4: "MATH_INSTR_VALID",
        5: "TDMA_SRCB_REGIF_WREN",
        6: "TDMA_SRCA_REGIF_WREN",
        7: "UNPACK_BUSY_0",
        8: "UNPACK_BUSY_1",
        9: "UNPACK_BUSY_2",
        10: "UNPACK_BUSY_3",
    },
    "L1": {
        # Format: (counter_id, l1_mux) -> name
        (0, 0): "NOC_RING0_INCOMING_1",
        (1, 0): "NOC_RING0_INCOMING_0",
        (2, 0): "NOC_RING0_OUTGOING_1",
        (3, 0): "NOC_RING0_OUTGOING_0",
        (4, 0): "L1_ARB_TDMA_BUNDLE_1",
        (5, 0): "L1_ARB_TDMA_BUNDLE_0",
        (6, 0): "L1_ARB_UNPACKER",
        (7, 0): "L1_NO_ARB_UNPACKER",
        (0, 1): "NOC_RING1_INCOMING_1",
        (1, 1): "NOC_RING1_INCOMING_0",
        (2, 1): "NOC_RING1_OUTGOING_1",
        (3, 1): "NOC_RING1_OUTGOING_0",
        (4, 1): "TDMA_BUNDLE_1_ARB",
        (5, 1): "TDMA_BUNDLE_0_ARB",
        (6, 1): "TDMA_EXT_UNPACK_9_10",
        (7, 1): "TDMA_PACKER_2_WR",
    },
    "TDMA_PACK": {
        0: "DSTAC_RDEN_RAW_0",
        1: "DSTAC_RDEN_RAW_1",
        2: "DSTAC_RDEN_RAW_2",
        3: "DSTAC_RDEN_RAW_3",
        4: "PACK_NOT_DEST_STALL",
        5: "PACK_NOT_SB_STALL",
        6: "PACK_BUSY_10",
        7: "PACK_BUSY_11",
    },
}

# Reverse lookups for O(1) counter name -> id resolution (computed once at module load)
_L1_NAME_TO_ID = {(name, mux): cid for (cid, mux), name in COUNTER_NAMES["L1"].items()}

_COUNTER_NAME_TO_ID = {
    bank: {name: cid for cid, name in counters.items()}
    for bank, counters in COUNTER_NAMES.items()
    if bank != "L1"
}


def _build_all_counters() -> List[Dict]:
    """Build the complete list of all 66 performance counters across all banks."""
    counters = []

    # INSTRN_THREAD bank (29 counters)
    for name in COUNTER_NAMES["INSTRN_THREAD"].values():
        counters.append(
            {
                "bank": "INSTRN_THREAD",
                "counter_id": _COUNTER_NAME_TO_ID["INSTRN_THREAD"][name],
            }
        )

    # FPU bank (2 counters)
    for name in COUNTER_NAMES["FPU"].values():
        counters.append({"bank": "FPU", "counter_id": _COUNTER_NAME_TO_ID["FPU"][name]})

    # TDMA_UNPACK bank (11 counters)
    for name in COUNTER_NAMES["TDMA_UNPACK"].values():
        counters.append(
            {
                "bank": "TDMA_UNPACK",
                "counter_id": _COUNTER_NAME_TO_ID["TDMA_UNPACK"][name],
            }
        )

    # TDMA_PACK bank (8 counters)
    for name in COUNTER_NAMES["TDMA_PACK"].values():
        counters.append(
            {"bank": "TDMA_PACK", "counter_id": _COUNTER_NAME_TO_ID["TDMA_PACK"][name]}
        )

    # L1 bank with l1_mux=0 (8 counters)
    for (counter_id, l1_mux), name in COUNTER_NAMES["L1"].items():
        if l1_mux == 0:
            counters.append({"bank": "L1", "counter_id": counter_id, "l1_mux": 0})

    # L1 bank with l1_mux=1 (8 counters)
    for (counter_id, l1_mux), name in COUNTER_NAMES["L1"].items():
        if l1_mux == 1:
            counters.append({"bank": "L1", "counter_id": counter_id, "l1_mux": 1})

    # Total: 29 + 2 + 11 + 8 + 8 + 8 = 66 counters
    return counters


# Pre-built list of all 66 counters (computed once at module load)
ALL_COUNTERS = _build_all_counters()

# All threads that support performance counters
ALL_THREADS = ["UNPACK", "MATH", "PACK"]


def _get_thread_addresses(thread: str) -> Tuple[int, int]:
    """Get config and data addresses for a thread."""
    addresses = _THREAD_ADDRESSES.get(thread.upper())
    if addresses is None:
        raise ValueError(f"Unknown thread: {thread}. Must be UNPACK, MATH, or PACK")
    return addresses


def _configure_perf_counters(
    counters: List[Dict],
    location: str,
    thread: str,
    mode: str,
) -> None:
    """Internal: Configure performance counters for a single thread."""
    config_addr, data_addr = _get_thread_addresses(thread)

    if len(counters) > COUNTER_SLOT_COUNT:
        raise ValueError(
            f"Cannot configure more than {COUNTER_SLOT_COUNT} counters per thread. Got {len(counters)}."
        )

    # Bit16: 0 = REQUESTS, 1 = GRANTS
    mode_bit = 1 if mode.upper() == "GRANTS" else 0

    # Encode counter configurations
    config_words = []
    for counter in counters:
        bank_name = counter["bank"]
        counter_id = counter["counter_id"]
        l1_mux = counter.get("l1_mux", 0)

        bank_id = _BANK_NAME_TO_ID.get(bank_name)
        if bank_id is None:
            raise ValueError(f"Unknown bank: {bank_name}")

        # Encode: [valid(31), l1_mux(17), mode(16), counter_id(8-15), bank(0-7)]
        config_word = (
            (1 << 31)  # Valid bit to distinguish from empty slots
            | (l1_mux << 17)
            | (mode_bit << 16)
            | (counter_id << 8)
            | bank_id
        )
        config_words.append(config_word)

    # Pad to COUNTER_SLOT_COUNT words
    if len(config_words) < COUNTER_SLOT_COUNT:
        config_words.extend([0] * (COUNTER_SLOT_COUNT - len(config_words)))

    # Coalesce into a single write: config_words (66) + zero_data (132) = 198 words
    zero_data = [0] * COUNTER_DATA_WORD_COUNT
    combined_data = config_words + zero_data
    write_words_to_device(location=location, addr=config_addr, data=combined_data)


def _read_perf_counters(location: str, thread: str) -> List[Dict]:
    config_addr, data_addr = _get_thread_addresses(thread)

    # Read metadata
    metadata = read_words_from_device(
        location=location, addr=config_addr, word_count=COUNTER_SLOT_COUNT
    )

    # Reading counters from L1

    if not metadata:
        return []

    # Count valid configs (check bit 31)
    valid_count = sum(1 for m in metadata if (m & 0x80000000) != 0)

    if valid_count == 0:
        return []

    # Read ONLY data for valid counters (no UNKNOWN counters)
    data = read_words_from_device(
        location=location, addr=data_addr, word_count=valid_count * 2
    )

    if not data or len(data) < valid_count * 2:
        return []

    results = []
    data_idx = 0
    for i in range(COUNTER_SLOT_COUNT):
        config_word = metadata[i]
        if (config_word & 0x80000000) == 0:  # Check valid bit
            continue  # Unused slot

        # Decode metadata: [l1_mux(17), mode(16), counter_id(8-15), bank(0-7)]
        bank_id = config_word & 0xFF
        counter_id = (config_word >> 8) & 0xFF
        mode_bit = (config_word >> 16) & 0x1
        l1_mux = (config_word >> 17) & 0x1

        bank_name = COUNTER_BANK_NAMES.get(bank_id, f"UNKNOWN_{bank_id}")
        # Hardware doc: bit16 = 1 -> GRANTS, 0 -> REQUESTS
        mode = "GRANTS" if mode_bit == 1 else "REQUESTS"

        # Get counter name
        if bank_name == "L1":
            counter_name = COUNTER_NAMES["L1"].get(
                (counter_id, l1_mux), f"L1_UNKNOWN_{counter_id}_{l1_mux}"
            )
        else:
            counter_name = COUNTER_NAMES.get(bank_name, {}).get(
                counter_id, f"{bank_name}_UNKNOWN_{counter_id}"
            )

        # Extract results using data_idx
        cycles = data[data_idx * 2]
        count = data[data_idx * 2 + 1]
        data_idx += 1

        results.append(
            {
                "bank": bank_name,
                "counter_name": counter_name,
                "counter_id": counter_id,
                "cycles": cycles,
                "count": count,
                "mode": mode,
                "l1_mux": l1_mux if bank_name == "L1" else None,
            }
        )

    return results


def _print_perf_counters(results: List[Dict], thread: str = None) -> None:
    if not results:
        print("No performance counters configured")
        return

    header = "Performance Counter Results"
    if thread:
        header += f" - {thread} Thread"

    print("=" * 80)
    print(header)
    print("=" * 80)

    # Group by bank
    by_bank = defaultdict(list)
    for result in results:
        by_bank[result["bank"]].append(result)

    # Print by bank
    for bank_name in sorted(by_bank.keys()):
        print(f"\n{bank_name}:")
        print("-" * 80)
        # Optional diagnostic: detect identical counts within a bank
        counts = [r["count"] for r in by_bank[bank_name]]
        cycles_list = [r["cycles"] for r in by_bank[bank_name]]
        # The cycles register for a bank reflects the measurement window and is shared
        # across selections; identical cycles are expected when scanning different events
        shared_cycles = cycles_list[0] if cycles_list else 0
        if len(counts) > 1 and len(set(counts)) == 1:
            print(
                f"  [DIAG] All counter counts identical in bank {bank_name}: {counts[0]}"
            )
        if len(cycles_list) > 1 and len(set(cycles_list)) == 1:
            print(
                f"  [DIAG] Shared reference cycles (bank window) in {bank_name}: {shared_cycles}"
            )
        else:
            print(
                f"  [DIAG] Reference cycles vary in {bank_name}: {', '.join(str(c) for c in cycles_list)}"
            )

        # Helper to label mode semantics per bank/event
        def _mode_label(res: Dict) -> str:
            mode = res.get("mode", "")
            return mode.lower() if mode else ""

        # Print shared cycles once, then per-counter count and rate
        print(f"  Shared cycles: {shared_cycles}")
        for result in by_bank[bank_name]:
            name = result["counter_name"]
            count = result["count"]
            rate = (count / shared_cycles) if shared_cycles else 0.0
            mode_desc = _mode_label(result)
            print(
                f"  {name:30s} | Count: {count:12d} | Rate: {rate:8.3f} | Mode: {mode_desc}"
            )

    print("=" * 80)


def configure_all_counters(location: str = "0,0", mode: str = "GRANTS") -> None:
    """
    Configure all 66 performance counters on all threads (UNPACK, MATH, PACK).

    This is a convenience function that sets up complete counter coverage
    for performance analysis. The counters measure various hardware events
    across instruction threads, FPU, TDMA (unpack/pack), and L1/NoC.

    Args:
        location: Tensix core coordinates (e.g., "0,0").
        mode: Counter mode - "GRANTS" for actual work done, "REQUESTS" for attempted work.
    """
    for thread in ALL_THREADS:
        _configure_perf_counters(
            ALL_COUNTERS, location=location, thread=thread, mode=mode
        )


def read_all_counters(
    location: str = "0,0", verbose: bool = False
) -> Dict[str, List[Dict]]:
    """
    Read performance counter results from all threads.

    Args:
        location: Tensix core coordinates (e.g., "0,0").
        verbose: If True, print counter results for each thread.

    Returns:
        Dictionary mapping thread name to list of counter results.
    """
    results_by_thread = {}
    for thread in ALL_THREADS:
        results = _read_perf_counters(location=location, thread=thread)
        if results:
            if verbose:
                _print_perf_counters(results, thread=thread)
            results_by_thread[thread] = results
    return results_by_thread
