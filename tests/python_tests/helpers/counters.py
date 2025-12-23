# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
#
# SPDX-License-Identifier: Apache-2.0

from typing import Dict, List

from ttexalens.tt_exalens_lib import read_words_from_device, write_words_to_device

# L1 Memory addresses - separate per TRISC thread
PERF_COUNTER_UNPACK_CONFIG_ADDR = 0x2F7D0  # 5 words: UNPACK metadata
PERF_COUNTER_UNPACK_DATA_ADDR = 0x2F7E4  # 10 words: UNPACK results
PERF_COUNTER_MATH_CONFIG_ADDR = 0x2F80C  # 5 words: MATH metadata
PERF_COUNTER_MATH_DATA_ADDR = 0x2F820  # 10 words: MATH results
PERF_COUNTER_PACK_CONFIG_ADDR = 0x2F848  # 5 words: PACK metadata
PERF_COUNTER_PACK_DATA_ADDR = 0x2F85C  # 10 words: PACK results

COUNTER_BANK_NAMES = {
    0: "INSTRN_THREAD",
    1: "FPU",
    2: "TDMA_UNPACK",
    3: "L1",
    4: "TDMA_PACK",
}

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
        # Format: (counter_id, mux_ctrl_bit4) -> name
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


def counter(bank: str, counter_name: str, mux_ctrl_bit4: int = 0) -> Dict:
    bank_name_to_id = {v: k for k, v in COUNTER_BANK_NAMES.items()}
    if bank not in bank_name_to_id:
        raise ValueError(
            f"Unknown bank: {bank}. Valid banks: {list(bank_name_to_id.keys())}"
        )

    bank_id = bank_name_to_id[bank]

    # Find counter ID by name
    if bank not in COUNTER_NAMES:
        raise ValueError(f"No counter definitions for bank: {bank}")

    counter_map = COUNTER_NAMES[bank]

    # For L1 counters, search with (counter_id, mux_ctrl_bit4) tuple
    if bank == "L1":
        # Reverse lookup: name -> (counter_id, mux_ctrl_bit4)
        counter_id = None
        for (cid, mux), name in counter_map.items():
            if name == counter_name and mux == mux_ctrl_bit4:
                counter_id = cid
                break

        if counter_id is None:
            available = [
                f"{name} (mux={mux})" for (_, mux), name in counter_map.items()
            ]
            raise ValueError(
                f"Unknown L1 counter: {counter_name} with mux_ctrl_bit4={mux_ctrl_bit4}. Available: {available}"
            )

        return {"bank": bank, "counter_id": counter_id, "mux_ctrl_bit4": mux_ctrl_bit4}
    else:
        # For non-L1 counters, reverse lookup by name
        counter_id = None
        for cid, name in counter_map.items():
            if name == counter_name:
                counter_id = cid
                break

        if counter_id is None:
            available = list(counter_map.values())
            raise ValueError(
                f"Unknown counter: {counter_name} in bank {bank}. Available: {available}"
            )

        return {"bank": bank, "counter_id": counter_id}


def configure_perf_counters(
    counters: List[Dict],
    location: str = "0,0",
    thread: str = "MATH",
    mode: str = "GRANTS",
) -> None:
    thread = thread.upper()
    if thread == "UNPACK":
        config_addr = PERF_COUNTER_UNPACK_CONFIG_ADDR
    elif thread == "MATH":
        config_addr = PERF_COUNTER_MATH_CONFIG_ADDR
    elif thread == "PACK":
        config_addr = PERF_COUNTER_PACK_CONFIG_ADDR
    else:
        raise ValueError(f"Unknown thread: {thread}. Must be UNPACK, MATH, or PACK")

    if len(counters) > 5:
        raise ValueError(
            f"Cannot configure more than 5 counters per thread in a single batch. Got {len(counters)}. Use measure_perf_counters_batched() for >5 counters."
        )

    # Reverse lookup for bank names
    bank_name_to_id = {v: k for k, v in COUNTER_BANK_NAMES.items()}

    mode_bit = 0 if mode.upper() == "GRANTS" else 1

    # Encode counter configurations
    config_words = []
    for counter in counters:
        bank_name = counter["bank"]
        counter_id = counter["counter_id"]
        mux_ctrl_bit4 = counter.get("mux_ctrl_bit4", 0)

        bank_id = bank_name_to_id.get(bank_name)
        if bank_id is None:
            raise ValueError(f"Unknown bank: {bank_name}")

        # Encode: [mux_ctrl_bit4(17), mode(16), counter_id(8-15), bank(0-7)]
        config_word = (
            (mux_ctrl_bit4 << 17) | (mode_bit << 16) | (counter_id << 8) | bank_id
        )
        config_words.append(config_word)

    # Pad to 5 words
    while len(config_words) < 5:
        config_words.append(0)

    # Write to L1
    write_words_to_device(location=location, addr=config_addr, data=config_words)


def read_perf_counters(location: str = "0,0", thread: str = "MATH") -> List[Dict]:
    thread = thread.upper()
    if thread == "UNPACK":
        config_addr = PERF_COUNTER_UNPACK_CONFIG_ADDR
        data_addr = PERF_COUNTER_UNPACK_DATA_ADDR
    elif thread == "MATH":
        config_addr = PERF_COUNTER_MATH_CONFIG_ADDR
        data_addr = PERF_COUNTER_MATH_DATA_ADDR
    elif thread == "PACK":
        config_addr = PERF_COUNTER_PACK_CONFIG_ADDR
        data_addr = PERF_COUNTER_PACK_DATA_ADDR
    else:
        raise ValueError(f"Unknown thread: {thread}. Must be UNPACK, MATH, or PACK")

    # Read metadata (5 words)
    metadata = read_words_from_device(location=location, addr=config_addr, word_count=5)

    # Read results (10 words: 5 counters × 2 values)
    data = read_words_from_device(location=location, addr=data_addr, word_count=10)

    if not metadata or not data:
        return []

    results = []
    for i in range(5):
        config_word = metadata[i]
        if config_word == 0:
            continue  # Unused slot

        # Decode metadata: [mux_ctrl_bit4(17), mode(16), counter_id(8-15), bank(0-7)]
        bank_id = config_word & 0xFF
        counter_id = (config_word >> 8) & 0xFF
        mode_bit = (config_word >> 16) & 0x1
        mux_ctrl_bit4 = (config_word >> 17) & 0x1

        bank_name = COUNTER_BANK_NAMES.get(bank_id, f"UNKNOWN_{bank_id}")
        mode = "GRANTS" if mode_bit == 0 else "REQUESTS"

        # Get counter name
        if bank_name == "L1":
            counter_name = COUNTER_NAMES["L1"].get(
                (counter_id, mux_ctrl_bit4), f"L1_UNKNOWN_{counter_id}_{mux_ctrl_bit4}"
            )
        else:
            counter_name = COUNTER_NAMES.get(bank_name, {}).get(
                counter_id, f"{bank_name}_UNKNOWN_{counter_id}"
            )

        # Extract results
        cycles = data[i * 2]
        count = data[i * 2 + 1]

        results.append(
            {
                "bank": bank_name,
                "counter_name": counter_name,
                "counter_id": counter_id,
                "cycles": cycles,
                "count": count,
                "mode": mode,
                "mux_ctrl_bit4": mux_ctrl_bit4 if bank_name == "L1" else None,
            }
        )

    return results


def print_perf_counters(results: List[Dict], thread: str = None) -> None:
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
    by_bank = {}
    for result in results:
        bank = result["bank"]
        if bank not in by_bank:
            by_bank[bank] = []
        by_bank[bank].append(result)

    # Print by bank
    for bank_name in sorted(by_bank.keys()):
        print(f"\n{bank_name}:")
        print("-" * 80)
        for result in by_bank[bank_name]:
            name = result["counter_name"]
            cycles = result["cycles"]
            count = result["count"]
            mode = result["mode"]
            print(
                f"  {name:30s} | Cycles: {cycles:12d} | Count: {count:12d} | Mode: {mode}"
            )

    print("=" * 80)


def measure_perf_counters_batched(
    counters: List[Dict],
    run_test_func,
    test_config,
    boot_mode,
    location: str = "0,0",
    thread: str = "MATH",
    mode: str = "GRANTS",
) -> List[Dict]:
    if len(counters) <= 5:
        # Single batch - configure and run once
        configure_perf_counters(counters, location, thread, mode)
        run_test_func(test_config, boot_mode)
        return read_perf_counters(location, thread)

    # Multiple batches needed
    num_batches = (len(counters) + 4) // 5  # Ceiling division
    all_results = []

    for batch_idx in range(num_batches):
        start_idx = batch_idx * 5
        end_idx = min(start_idx + 5, len(counters))
        batch_counters = counters[start_idx:end_idx]

        # Configure this batch
        configure_perf_counters(batch_counters, location, thread, mode)

        # Run test
        run_test_func(test_config, boot_mode)

        # Read results
        batch_results = read_perf_counters(location, thread)
        all_results.extend(batch_results)

    return all_results
