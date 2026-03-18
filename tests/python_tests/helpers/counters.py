# SPDX-FileCopyrightText: © 2026 Tenstorrent AI ULC
#
# SPDX-License-Identifier: Apache-2.0

from typing import Dict, List

import pandas as pd
from loguru import logger
from ttexalens.tt_exalens_lib import read_words_from_device, write_words_to_device

from .test_config import TestConfig

# ============================================================================
# Constants and Configuration
# ============================================================================

# Derive all constants from TestConfig (single source of truth)
COUNTER_SLOT_COUNT = TestConfig._PERF_COUNTERS_CONFIG_WORDS  # 86 config slots
COUNTER_DATA_WORD_COUNT = (
    TestConfig._PERF_COUNTERS_DATA_WORDS
)  # 172 data words (86 * 2)
PERF_COUNTERS_STARTER_SHIFT = 8
PERF_COUNTERS_STARTER_MASK = 0x3
PERF_COUNTERS_STOPPER_SHIFT = 10
PERF_COUNTERS_STOPPER_MASK = 0x3


# Helper function to get L1 addresses for a zone
def get_counters_addr(zone: int):
    """
    Get L1 memory addresses for counter buffers for a specific zone.

    Zones are auto-assigned by MEASURE_PERF_COUNTERS() in declaration order:
      Zone 0 = first call (typically "INIT" or "KERNEL")
      Zone 1 = second call (typically "TILE_LOOP")
    """
    base_addr = (
        TestConfig.PERF_COUNTERS_BASE_ADDR + zone * TestConfig.PERF_COUNTERS_ZONE_SIZE
    )
    return {
        "config": base_addr,
        "data": base_addr + TestConfig._PERF_COUNTERS_CONFIG_WORDS * 4,
        "sync": base_addr + TestConfig._PERF_COUNTERS_BUFFER_SIZE,
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
        # Instruction availability counters (per-thread)
        0: "CFG_INSTRN_AVAILABLE_0",
        1: "CFG_INSTRN_AVAILABLE_1",
        2: "CFG_INSTRN_AVAILABLE_2",
        3: "SYNC_INSTRN_AVAILABLE_0",
        4: "SYNC_INSTRN_AVAILABLE_1",
        5: "SYNC_INSTRN_AVAILABLE_2",
        6: "THCON_INSTRN_AVAILABLE_0",
        7: "THCON_INSTRN_AVAILABLE_1",
        8: "THCON_INSTRN_AVAILABLE_2",
        9: "XSEARCH_INSTRN_AVAILABLE_0",
        10: "XSEARCH_INSTRN_AVAILABLE_1",
        11: "XSEARCH_INSTRN_AVAILABLE_2",
        12: "MOVE_INSTRN_AVAILABLE_0",
        13: "MOVE_INSTRN_AVAILABLE_1",
        14: "MOVE_INSTRN_AVAILABLE_2",
        15: "FPU_INSTRN_AVAILABLE_0",
        16: "FPU_INSTRN_AVAILABLE_1",
        17: "FPU_INSTRN_AVAILABLE_2",
        18: "UNPACK_INSTRN_AVAILABLE_0",
        19: "UNPACK_INSTRN_AVAILABLE_1",
        20: "UNPACK_INSTRN_AVAILABLE_2",
        21: "PACK_INSTRN_AVAILABLE_0",
        22: "PACK_INSTRN_AVAILABLE_1",
        23: "PACK_INSTRN_AVAILABLE_2",
        # Thread stalls
        24: "THREAD_STALLS_0",
        25: "THREAD_STALLS_1",
        26: "THREAD_STALLS_2",
        # Wait counters (shared across threads)
        27: "WAITING_FOR_SRCA_CLEAR",
        28: "WAITING_FOR_SRCB_CLEAR",
        29: "WAITING_FOR_SRCA_VALID",
        30: "WAITING_FOR_SRCB_VALID",
        # Per-thread wait counters
        31: "WAITING_FOR_THCON_IDLE_0",
        32: "WAITING_FOR_THCON_IDLE_1",
        33: "WAITING_FOR_THCON_IDLE_2",
        34: "WAITING_FOR_UNPACK_IDLE_0",
        35: "WAITING_FOR_UNPACK_IDLE_1",
        36: "WAITING_FOR_UNPACK_IDLE_2",
        37: "WAITING_FOR_PACK_IDLE_0",
        38: "WAITING_FOR_PACK_IDLE_1",
        39: "WAITING_FOR_PACK_IDLE_2",
        40: "WAITING_FOR_MATH_IDLE_0",
        41: "WAITING_FOR_MATH_IDLE_1",
        42: "WAITING_FOR_MATH_IDLE_2",
        43: "WAITING_FOR_NONZERO_SEM_0",
        44: "WAITING_FOR_NONZERO_SEM_1",
        45: "WAITING_FOR_NONZERO_SEM_2",
        46: "WAITING_FOR_NONFULL_SEM_0",
        47: "WAITING_FOR_NONFULL_SEM_1",
        48: "WAITING_FOR_NONFULL_SEM_2",
        49: "WAITING_FOR_MOVE_IDLE_0",
        50: "WAITING_FOR_MOVE_IDLE_1",
        51: "WAITING_FOR_MOVE_IDLE_2",
        52: "WAITING_FOR_MMIO_IDLE_0",
        53: "WAITING_FOR_MMIO_IDLE_1",
        54: "WAITING_FOR_MMIO_IDLE_2",
        55: "WAITING_FOR_SFPU_IDLE_0",
        56: "WAITING_FOR_SFPU_IDLE_1",
        57: "WAITING_FOR_SFPU_IDLE_2",
        # Thread instruction counts (bit 8 set = ID 256+n)
        256: "THREAD_INSTRUCTIONS_0",
        257: "THREAD_INSTRUCTIONS_1",
        258: "THREAD_INSTRUCTIONS_2",
    },
    "FPU": {
        0: "FPU_INSTRUCTION",
        1: "SFPU_INSTRUCTION",
        257: "FPU_OR_SFPU_INSTRN",  # Combined FPU/SFPU
    },
    "TDMA_UNPACK": {
        1: "DATA_HAZARD_STALLS_MOVD2A",
        3: "MATH_INSTRN_STARTED",
        4: "MATH_INSTRN_AVAILABLE",
        5: "SRCB_WRITE_AVAILABLE",
        6: "SRCA_WRITE_AVAILABLE",
        7: "UNPACK0_BUSY_THREAD0",
        8: "UNPACK1_BUSY_THREAD0",
        9: "UNPACK0_BUSY_THREAD1",
        10: "UNPACK1_BUSY_THREAD1",
        259: "SRCB_WRITE",  # Bit 8 set
        261: "SRCA_WRITE",  # Bit 8 set
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
        11: "PACKER_DEST_READ_AVAILABLE",
        18: "PACKER_BUSY",
        272: "AVAILABLE_MATH",  # Bit 8 set
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
    """Build the complete list of performance counters across all banks."""
    counters = []

    # INSTRN_THREAD bank (61 counters)
    for counter_id in COUNTER_NAMES["INSTRN_THREAD"].keys():
        counters.append({"bank": "INSTRN_THREAD", "counter_id": counter_id})

    # FPU bank (3 counters)
    for counter_id in COUNTER_NAMES["FPU"].keys():
        counters.append({"bank": "FPU", "counter_id": counter_id})

    # TDMA_UNPACK bank (11 counters)
    for counter_id in COUNTER_NAMES["TDMA_UNPACK"].keys():
        counters.append({"bank": "TDMA_UNPACK", "counter_id": counter_id})

    # TDMA_PACK bank (3 counters)
    for counter_id in COUNTER_NAMES["TDMA_PACK"].keys():
        counters.append({"bank": "TDMA_PACK", "counter_id": counter_id})

    # L1 bank with l1_mux=0 (8 counters)
    for (counter_id, l1_mux), name in COUNTER_NAMES["L1"].items():
        if l1_mux == 0:
            counters.append({"bank": "L1", "counter_id": counter_id, "l1_mux": 0})

    # L1 bank with l1_mux=1 (8 counters)
    for (counter_id, l1_mux), name in COUNTER_NAMES["L1"].items():
        if l1_mux == 1:
            counters.append({"bank": "L1", "counter_id": counter_id, "l1_mux": 1})

    return counters


# Pre-built list of all counters (computed once at module load)
# Total: 94 counters (61 INSTRN_THREAD + 3 FPU + 11 TDMA_UNPACK + 3 TDMA_PACK + 16 L1)
# Note: L1 has 8 counter IDs × 2 mux settings = 16 entries, but only 8 L1 counters
# can be active at once (determined by mux setting), so maximum concurrent counters = 86
ALL_COUNTERS = _build_all_counters()

# All threads that support performance counters
ALL_THREADS = ["UNPACK", "MATH", "PACK"]


def configure_counters(location: str = "0,0", num_zones: int = None) -> None:
    """
    Configure performance counters in the shared buffer for all threads (UNPACK, MATH, PACK).

    Writes counter configuration to L1 memory that all threads access. Configures all 94
    counter definitions (61 INSTRN_THREAD + 3 FPU + 11 TDMA_UNPACK + 3 TDMA_PACK + 16 L1).
    Note: Only 86 slots are available in hardware; L1 counters (16 defs, 8 active at once)
    require mux configuration before measurement starts.

    The counters are started/stopped by all threads via MEASURE_PERF_COUNTERS("name"),
    but only the last thread to finish (last stopper) reads the hardware and writes results.

    Args:
        location: Tensix core coordinates (e.g., "0,0").
        num_zones: Maximum number of zones to pre-configure. If None, uses TestConfig.PERF_COUNTERS_MAX_ZONES.
    """
    # Use TestConfig constant if not specified
    if num_zones is None:
        num_zones = TestConfig.PERF_COUNTERS_MAX_ZONES

    # Validate that zones fit within the available L1 region (before dump mailbox at 0x16AFE4)
    end_addr = (
        TestConfig.PERF_COUNTERS_BASE_ADDR
        + num_zones * TestConfig.PERF_COUNTERS_ZONE_SIZE
    )
    assert end_addr <= 0x16AFE4, (
        f"Perf counter zones overflow into dump mailbox: "
        f"{num_zones} zones end at 0x{end_addr:X}, mailbox at 0x16AFE4"
    )

    # Encode counter configurations
    config_words = []
    for counter in ALL_COUNTERS:
        valid_bit = 1 << 31
        l1_mux = counter.get("l1_mux", 0) & 0x1
        l1_mux_shifted = l1_mux << 17
        counter_id_shifted = (counter["counter_id"] & 0x1FF) << 8
        bank_id = _BANK_NAME_TO_ID[counter["bank"]] & 0xFF
        config_words.append(valid_bit | l1_mux_shifted | counter_id_shifted | bank_id)

    # Pad config words to full slot count
    config_words.extend([0] * (COUNTER_SLOT_COUNT - len(config_words)))

    # Configure all possible zones (C++ __COUNTER__ determines actual usage)
    for zone_id in range(num_zones):
        addrs = get_counters_addr(zone_id)
        # Write config to shared buffer
        write_words_to_device(
            location=location, addr=addrs["config"], data=config_words
        )

        # Clear data buffer completely (remove any stale values from previous runs)
        write_words_to_device(
            location=location,
            addr=addrs["data"],
            data=[0] * COUNTER_DATA_WORD_COUNT,
        )

        # Clear sync state and ATINCGET counters before kernel runs
        write_words_to_device(
            location=location,
            addr=addrs["sync"],
            data=[0] * (1 + 3 * 2 + 1),
        )


def read_counters(location: str = "0,0") -> pd.DataFrame:
    """
    Read performance counter results from the shared buffer.

    Auto-detects which zones were used by checking for valid sync words.
    In the shared buffer architecture, whichever thread finishes last (the "last stopper")
    reads the hardware counters and writes them to the shared buffer.

    Args:
        location: Tensix core coordinates (e.g., "0,0").

    Returns:
        DataFrame containing counter results, with columns:
        zone, starter_thread, stopper_thread, bank, counter_name, counter_id, cycles, count, l1_mux
    """
    all_results = []

    # Auto-detect zones by checking sync words (stop when we hit an uninitialized zone)
    max_zones = TestConfig.PERF_COUNTERS_MAX_ZONES
    for zone_id in range(max_zones):
        # Zone name is just the zone number
        zone_name = f"ZONE_{zone_id}"

        addrs = get_counters_addr(zone_id)
        # Read from the single shared buffer (last stopper wrote here)
        sync_ctrl = read_words_from_device(
            location=location, addr=addrs["sync"], word_count=3
        )
        if not sync_ctrl:
            break  # Stop if we can't read memory

        sync_word = sync_ctrl[0]

        # Skip zones that were never initialized (sync_word == 0)
        if sync_word == 0:
            continue  # Zone may be unused; keep scanning for later valid zones

        # Validate that counters were properly started and stopped
        GLOBAL_STARTED_BIT = 1 << 6
        GLOBAL_STOPPED_BIT = 1 << 7
        ALL_START_BITS = 0x7  # Bits 0-2 should all be set
        ALL_STOP_BITS = 0x7 << 3  # Bits 3-5 should all be set

        if not (sync_word & GLOBAL_STARTED_BIT):
            logger.warning(
                f"Zone {zone_id}: global started bit not set (sync_ctrl=0x{sync_word:08x}), skipping"
            )
            continue

        # Validate thread start/stop bits (warn only — CAS bits suffer from
        # L1 cache visibility issues, but counter data is still valid because
        # the stop_elect ATINCGET mechanism guarantees the last thread reads hardware).
        start_bits = sync_word & 0x7
        if start_bits != 0x7:
            missing = [
                t
                for t, b in [("UNPACK", 0x1), ("MATH", 0x2), ("PACK", 0x4)]
                if not (start_bits & b)
            ]
            logger.warning(
                f"Zone {zone_id}: missing start bits from {', '.join(missing)} (sync_ctrl=0x{sync_word:08x})"
            )

        if not (sync_word & GLOBAL_STOPPED_BIT):
            stop_bits = (sync_word >> 3) & 0x7
            missing = [
                t
                for t, b in [("UNPACK", 0x1), ("MATH", 0x2), ("PACK", 0x4)]
                if not (stop_bits & b)
            ]
            logger.warning(
                f"Zone {zone_id}: global stopped bit not set, missing stop from {', '.join(missing)} "
                f"(sync_ctrl=0x{sync_word:08x}). Counter data may still be valid."
            )

        stop_bits = (sync_word >> 3) & 0x7
        if stop_bits != 0x7:
            missing = [
                t
                for t, b in [("UNPACK", 0x1), ("MATH", 0x2), ("PACK", 0x4)]
                if not (stop_bits & b)
            ]
            logger.warning(
                f"Zone {zone_id}: not all threads stopped. Missing: {', '.join(missing)} (sync_ctrl=0x{sync_word:08x})"
            )

        starter_id = (
            sync_word >> PERF_COUNTERS_STARTER_SHIFT
        ) & PERF_COUNTERS_STARTER_MASK
        stopper_id = (
            sync_word >> PERF_COUNTERS_STOPPER_SHIFT
        ) & PERF_COUNTERS_STOPPER_MASK

        thread_map = {0: "UNPACK", 1: "MATH", 2: "PACK"}

        if starter_id not in thread_map:
            raise RuntimeError(
                f"Invalid starter id {starter_id}; sync_ctrl=0x{sync_word:08x}"
            )
        if stopper_id not in thread_map:
            raise RuntimeError(
                f"Invalid stopper id {stopper_id}; sync_ctrl=0x{sync_word:08x}"
            )

        starter_thread = thread_map[starter_id]
        stopper_thread = thread_map[stopper_id]

        # Read metadata from shared buffer
        metadata = read_words_from_device(
            location=location, addr=addrs["config"], word_count=COUNTER_SLOT_COUNT
        )
        if not metadata:
            continue

        # Count valid configs (check bit 31)
        valid_count = sum(1 for m in metadata if (m & 0x80000000) != 0)

        if valid_count == 0:
            continue

        # Read ONLY data for valid counters from shared buffer
        data = read_words_from_device(
            location=location, addr=addrs["data"], word_count=valid_count * 2
        )
        if not data or len(data) < valid_count * 2:
            continue

        data_idx = 0
        for i in range(COUNTER_SLOT_COUNT):
            config_word = metadata[i]
            if (config_word & 0x80000000) == 0:  # Check valid bit
                continue  # Unused slot

            # Decode metadata: [valid(31), l1_mux(17), counter_id(8-16), bank(0-7)]
            bank_id = config_word & 0xFF
            counter_id = (config_word >> 8) & 0x1FF  # 9 bits for counter_id
            l1_mux = (config_word >> 17) & 0x1

            bank_name = COUNTER_BANK_NAMES.get(bank_id, f"UNKNOWN_{bank_id}")

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

            all_results.append(
                {
                    "zone": zone_name,
                    "starter_thread": starter_thread,
                    "stopper_thread": stopper_thread,
                    "bank": bank_name,
                    "counter_name": counter_name,
                    "counter_id": counter_id,
                    "cycles": cycles,
                    "count": count,
                    "l1_mux": l1_mux if bank_name == "L1" else None,
                }
            )

    return pd.DataFrame(all_results)


def print_counters(results: pd.DataFrame) -> None:
    """
    Print all counter results to console in a readable format.

    Args:
        results: DataFrame with counter results (from read_counters).
    """
    if results.empty:
        print("No counter results to display.")
        return

    # Group by zone if zone column exists
    if "zone" in results.columns:
        zones = results["zone"].unique()
        for zone in zones:
            zone_results = results[results["zone"] == zone]
            print(f"\n{'═' * 100}")
            print(f"ZONE: {zone}")
            print(f"{'═' * 100}")
            _print_zone_counters(zone_results)
    else:
        print("\n" + "=" * 100)
        print("PERFORMANCE COUNTER RESULTS")
        print("=" * 100)
        _print_zone_counters(results)


def _print_zone_counters(results: pd.DataFrame) -> None:
    """Helper to print counters for a single zone."""
    if not results.empty:
        print(f"\n{'─' * 100}")

    for bank in ["INSTRN_THREAD", "FPU", "TDMA_UNPACK", "L1", "TDMA_PACK"]:
        bank_df = results[results["bank"] == bank]
        if bank_df.empty:
            continue

        cycles = bank_df["cycles"].iloc[0] if len(bank_df) > 0 else 0

        print(f"\n  ┌─ {bank} (cycles: {cycles:,})")
        print(f"  │ {'Counter Name':<40} {'Count':>15} {'Rate':>12}")
        print(f"  │ {'─' * 40} {'─' * 15} {'─' * 12}")

        for _, row in bank_df.iterrows():
            name = row["counter_name"]
            # Add mux info for L1 counters
            if pd.notna(row["l1_mux"]):
                name = f"{name} (mux{int(row['l1_mux'])})"
            count = row["count"]
            rate = (count / cycles) if cycles else 0.0
            print(f"  │ {name:<40} {count:>15,} {rate:>12.4f}")

        print(f"  └{'─' * 70}")

    print("\n" + "=" * 100 + "\n")


def export_counters(
    results: pd.DataFrame,
    filename: str,
    test_params: dict = None,
    worker_id: str = "gw0",
) -> None:
    """
    Export counter DataFrame to CSV file in perf_data directory.

    Args:
        results: DataFrame with counter results (from read_counters).
        filename: Base filename (without extension), e.g., "test_matmul_counters".
        test_params: Optional dictionary of test parameters to add as columns.
        worker_id: Worker ID for parallel test runs (e.g., "gw0", "master").
    """
    if results.empty:
        return

    perf_dir = TestConfig.LLK_ROOT / "perf_data"
    perf_dir.mkdir(parents=True, exist_ok=True)

    df = results.copy()

    # Add test params as columns
    if test_params:
        for key, value in test_params.items():
            df[key] = value

    output_path = perf_dir / f"{filename}.{worker_id}.csv"

    if output_path.exists():
        existing = pd.read_csv(output_path)
        df = pd.concat([existing, df], ignore_index=True)

    df.to_csv(output_path, index=False)
