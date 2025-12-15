# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
#
# SPDX-License-Identifier: Apache-2.0

"""
Performance Counter Configuration and Data Collection

This module provides a user-friendly API for configuring and collecting
hardware performance counters from Tensix cores.

Example usage:
    from helpers.perf_counters import PerfCounterConfig, CounterBank

    # Configure which counters to measure
    config = PerfCounterConfig()
    config.add_counter(CounterBank.FPU, "FPU_OP_VALID")
    config.add_counter(CounterBank.INSTRN_THREAD, "INST_UNPACK")
    config.add_counter(CounterBank.TDMA_UNPACK, "UNPACK_BUSY_0")
    config.add_counter(CounterBank.TDMA_PACK, "PACK_BUSY_10")
    config.add_counter(CounterBank.L1, "NOC_RING0_OUTGOING_0")

    # Measure grants or requests
    config.set_mode("grants")  # or "requests"

    # Collect data after test runs
    data = collect_perf_counters(config)
"""

from enum import Enum
from typing import Dict, List, Optional

from ttexalens.tt_exalens_lib import read_words_from_device, write_words_to_device


class CounterBank(Enum):
    """Hardware counter banks - each can monitor one event at a time"""

    INSTRN_THREAD = "INSTRN_THREAD"
    FPU = "FPU"
    TDMA_UNPACK = "TDMA_UNPACK"
    L1 = "L1"
    TDMA_PACK = "TDMA_PACK"


class CounterMode(Enum):
    """Counter mode: grants (bit 16=1) or requests (bit 16=0)"""

    GRANTS = "grants"
    REQUESTS = "requests"


# Counter definitions per bank
COUNTER_DEFINITIONS = {
    CounterBank.FPU: {
        "FPU_OP_VALID": 0,
        "SFPU_OP_VALID": 1,
    },
    CounterBank.INSTRN_THREAD: {
        "INST_CFG": 0,
        "INST_SYNC": 1,
        "INST_THCON": 2,
        "INST_XSEARCH": 3,
        "INST_MOVE": 4,
        "INST_MATH": 5,
        "INST_UNPACK": 6,
        "INST_PACK": 7,
        "STALLED": 8,
        "SRCA_CLEARED_0": 9,
        "SRCA_CLEARED_1": 10,
        "SRCA_CLEARED_2": 11,
        "SRCB_CLEARED_0": 12,
        "SRCB_CLEARED_1": 13,
        "SRCB_CLEARED_2": 14,
        "SRCA_VALID_0": 15,
        "SRCA_VALID_1": 16,
        "SRCA_VALID_2": 17,
        "SRCB_VALID_0": 18,
        "SRCB_VALID_1": 19,
        "SRCB_VALID_2": 20,
        "STALL_THCON": 21,
        "STALL_PACK0": 22,
        "STALL_MATH": 23,
        "STALL_SEM_ZERO": 24,
        "STALL_SEM_MAX": 25,
        "STALL_MOVE": 26,
        "STALL_TRISC_REG_ACCESS": 27,
        "STALL_SFPU": 28,
    },
    CounterBank.TDMA_UNPACK: {
        "MATH_INSTR_SRC_READY": 0,
        "MATH_NOT_D2A_STALL": 1,
        "MATH_FIDELITY_PHASES": 2,
        "MATH_INSTR_BUF_RDEN": 3,
        "MATH_INSTR_VALID": 4,
        "TDMA_SRCB_REGIF_WREN": 5,
        "TDMA_SRCA_REGIF_WREN": 6,
        "UNPACK_BUSY_0": 7,
        "UNPACK_BUSY_1": 8,
        "UNPACK_BUSY_2": 9,
        "UNPACK_BUSY_3": 10,
    },
    CounterBank.TDMA_PACK: {
        "DSTAC_RDEN_RAW_0": 0,
        "DSTAC_RDEN_RAW_1": 1,
        "DSTAC_RDEN_RAW_2": 2,
        "DSTAC_RDEN_RAW_3": 3,
        "PACK_NOT_DEST_STALL": 4,
        "PACK_NOT_SB_STALL": 5,
        "PACK_BUSY_10": 6,
        "PACK_BUSY_11": 7,
    },
    CounterBank.L1: {
        # Format: "counter_name": (counter_id, mux_ctrl_bit4)
        # MUX_CTRL bit 4 = 0
        "NOC_RING0_INCOMING_1": (0, 0),
        "NOC_RING0_INCOMING_0": (1, 0),
        "NOC_RING0_OUTGOING_1": (2, 0),
        "NOC_RING0_OUTGOING_0": (3, 0),
        "L1_ARB_TDMA_BUNDLE_1": (4, 0),
        "L1_ARB_TDMA_BUNDLE_0": (5, 0),
        "L1_ARB_UNPACKER": (6, 0),
        "L1_NO_ARB_UNPACKER": (7, 0),
        # MUX_CTRL bit 4 = 1
        "NOC_RING1_INCOMING_1": (0, 1),
        "NOC_RING1_INCOMING_0": (1, 1),
        "NOC_RING1_OUTGOING_1": (2, 1),
        "NOC_RING1_OUTGOING_0": (3, 1),
        "TDMA_BUNDLE_1_ARB": (4, 1),
        "TDMA_BUNDLE_0_ARB": (5, 1),
        "TDMA_EXT_UNPACK_9_10": (6, 1),
        "TDMA_PACKER_2_WR": (7, 1),
    },
}

# L1 memory addresses
PERF_COUNTER_CONFIG_ADDR = 0x2F7F0  # Configuration data (5 words)
PERF_COUNTER_DATA_ADDR = (
    0x2F800  # Counter output data (10 words: 5 counters × 2 outputs each)
)


class PerfCounterConfig:
    """
    Configuration for performance counter measurement.

    Users specify exactly 5 counters to measure (one per hardware bank).
    Each counter must come from a different bank.
    """

    def __init__(self):
        self.counters: List[tuple[CounterBank, str]] = []
        self.mode: CounterMode = CounterMode.GRANTS

    def add_counter(self, bank: CounterBank, counter_name: str) -> "PerfCounterConfig":
        """
        Add a counter to measure.

        Args:
            bank: Which hardware bank the counter belongs to
            counter_name: Name of the counter (e.g., "FPU_OP_VALID")

        Returns:
            self for method chaining

        Raises:
            ValueError: If counter is invalid, bank already used, or mode incompatible
        """
        # Validate counter exists
        if bank not in COUNTER_DEFINITIONS:
            raise ValueError(f"Invalid bank: {bank}")

        if counter_name not in COUNTER_DEFINITIONS[bank]:
            available = ", ".join(COUNTER_DEFINITIONS[bank].keys())
            raise ValueError(
                f"Counter '{counter_name}' not found in bank {bank.value}. "
                f"Available counters: {available}"
            )

        # Check we don't exceed 5 counters
        if len(self.counters) >= 5:
            raise ValueError("Cannot add more than 5 counters (one per hardware bank)")

        # Check bank not already used
        used_banks = {bank for bank, _ in self.counters}
        if bank in used_banks:
            raise ValueError(f"Bank {bank.value} already has a counter configured")

        self.counters.append((bank, counter_name))
        return self

    def set_mode(self, mode: str) -> "PerfCounterConfig":
        """
        Set whether to measure grants or requests.

        Args:
            mode: Either "grants" or "requests"

        Returns:
            self for method chaining

        Raises:
            ValueError: If mode is invalid or incompatible with existing L1 counters
        """
        if mode.lower() == "grants":
            self.mode = CounterMode.GRANTS
        elif mode.lower() == "requests":
            self.mode = CounterMode.REQUESTS
        else:
            raise ValueError(f"Invalid mode: {mode}. Must be 'grants' or 'requests'")

        return self

    def validate(self) -> None:
        """
        Validate the configuration.

        Raises:
            ValueError: If configuration is invalid
        """
        if len(self.counters) == 0:
            raise ValueError("No counters configured. Add at least one counter.")

        if len(self.counters) > 5:
            raise ValueError(f"Too many counters: {len(self.counters)}. Maximum is 5.")

        # Check for duplicate banks
        banks = [bank for bank, _ in self.counters]
        if len(banks) != len(set(banks)):
            raise ValueError("Each counter must be from a different bank")

    def get_config_data(self) -> List[int]:
        """
        Generate configuration data to write to device.

        Returns:
            List of 5 words containing bank and counter_sel configuration
        """
        self.validate()

        # Each config word encodes: [bank_id(8 bits), counter_sel(8 bits), mode_bit(1 bit)]
        config_words = []

        bank_to_id = {
            CounterBank.INSTRN_THREAD: 0,
            CounterBank.FPU: 1,
            CounterBank.TDMA_UNPACK: 2,
            CounterBank.L1: 3,
            CounterBank.TDMA_PACK: 4,
        }

        mode_bit = 1 if self.mode == CounterMode.GRANTS else 0

        for bank, counter_name in self.counters:
            bank_id = bank_to_id[bank]
            counter_info = COUNTER_DEFINITIONS[bank][counter_name]

            # Handle L1 counters which have (counter_id, mux_ctrl_bit4) tuple format
            if isinstance(counter_info, tuple):
                counter_sel, mux_ctrl_bit4 = counter_info
            else:
                counter_sel = counter_info
                mux_ctrl_bit4 = 0

            # Encode: [mux_ctrl_bit4(17), mode_bit(16), counter_sel(8-15), bank_id(0-7)]
            config_word = (
                (mux_ctrl_bit4 << 17) | (mode_bit << 16) | (counter_sel << 8) | bank_id
            )
            config_words.append(config_word)

        # Pad to 5 words if fewer counters configured
        while len(config_words) < 5:
            config_words.append(0)

        return config_words


def write_perf_config(config: PerfCounterConfig, location: str = "0,0") -> None:
    """
    Write performance counter configuration to device.

    Args:
        config: Performance counter configuration
        location: Device location (default: "0,0")
    """
    config_data = config.get_config_data()
    write_words_to_device(
        location=location, addr=PERF_COUNTER_CONFIG_ADDR, data=config_data
    )


def clear_perf_counters(location: str = "0,0") -> None:
    """
    Clear performance counter configuration and data.

    Args:
        location: Device location (default: "0,0")
    """
    # Clear config
    zeros_config = [0] * 5
    write_words_to_device(
        location=location, addr=PERF_COUNTER_CONFIG_ADDR, data=zeros_config
    )

    # Clear data
    zeros_data = [0] * 10  # 5 counters × 2 words each (cycles_low, count_low)
    write_words_to_device(
        location=location, addr=PERF_COUNTER_DATA_ADDR, data=zeros_data
    )


def collect_perf_counters(config: PerfCounterConfig, location: str = "0,0") -> Dict:
    """
    Collect performance counter data from device.

    Args:
        config: Performance counter configuration used
        location: Device location (default: "0,0")

    Returns:
        Dictionary mapping counter names to (cycles, count) tuples
    """
    config.validate()

    # Read 10 words: 5 counters × 2 outputs each (cycles, count)
    data = read_words_from_device(
        location=location, addr=PERF_COUNTER_DATA_ADDR, word_count=10
    )

    if not data or len(data) < len(config.counters) * 2:
        return {}

    results = {}
    for i, (bank, counter_name) in enumerate(config.counters):
        cycles_offset = i * 2
        count_offset = i * 2 + 1

        cycles = data[cycles_offset]
        count = data[count_offset]

        results[counter_name] = {
            "bank": bank.value,
            "cycles": cycles,
            "count": count,
            "mode": config.mode.value,
        }

    return results


def print_perf_counters(results: Dict, workload_info: Optional[Dict] = None) -> None:
    """
    Print performance counter results in a formatted way.

    Args:
        results: Results from collect_perf_counters()
        workload_info: Optional workload metadata to display
    """
    if not results:
        print("\n⚠️  No performance counter data available")
        return

    print("\n" + "=" * 80)
    print("⚡ PERFORMANCE COUNTER RESULTS")
    print("=" * 80)

    if workload_info:
        print("\nWorkload Info:")
        for key, value in workload_info.items():
            print(f"  {key}: {value}")
        print()

    # Group by bank
    by_bank = {}
    for counter_name, data in results.items():
        bank = data["bank"]
        if bank not in by_bank:
            by_bank[bank] = []
        by_bank[bank].append((counter_name, data))

    # Print by bank
    for bank_name in sorted(by_bank.keys()):
        print(f"\n[{bank_name}]")
        for counter_name, data in by_bank[bank_name]:
            mode = data["mode"]
            cycles = data["cycles"]
            count = data["count"]
            print(f"  {counter_name:30s}: cycles={cycles:10d}, {mode}={count:8d}")

    print("\n" + "=" * 80 + "\n")
