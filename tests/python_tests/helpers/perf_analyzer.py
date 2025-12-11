# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
#
# SPDX-License-Identifier: Apache-2.0

from typing import Dict, List, Optional

from ttexalens.tt_exalens_lib import read_words_from_device

PERF_COUNTER_DATA_ADDR = 0x2F800
PERF_TOTAL_WORDS = 1080

TILE_HEIGHT = 32
TILE_WIDTH = 32
MACS_PER_TILE = TILE_HEIGHT * TILE_WIDTH


def collect_perf_counter_data(location: str = "0,0"):
    perf_data = read_words_from_device(
        location=location, addr=PERF_COUNTER_DATA_ADDR, word_count=PERF_TOTAL_WORDS
    )

    # Split data: first 540 words = grants (bit 16=1), next 540 = requests (bit 16=0)
    return {
        "grants": perf_data[: PERF_TOTAL_WORDS // 2],  # First 540 words
        "requests": perf_data[PERF_TOTAL_WORDS // 2 :],  # Next 540 words
    }


def clear_perf_counter_memory(location: str = "0,0"):
    from ttexalens.tt_exalens_lib import write_words_to_device

    write_words_to_device(location=location, addr=0x2F7FC, data=0)

    zeros = [0] * PERF_TOTAL_WORDS
    write_words_to_device(location=location, addr=PERF_COUNTER_DATA_ADDR, data=zeros)


def print_performance_analysis(
    workload_info: Optional[Dict] = None,
    iteration_data: Optional[List[Dict]] = None,
):
    if not iteration_data:
        return

    print("\n" + "=" * 100)
    num_iterations = len(iteration_data) if iteration_data else 0
    print(
        f"⚡ PERFORMANCE COUNTER DATA (Multi-Iteration: {num_iterations} iterations, measuring all 66 counters)"
    )
    print("=" * 100)

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
        unpack_results = {}
        math_results = {}
        pack_results = {}

        grant_data = iteration_data.get("grants", [])
        request_data = iteration_data.get("requests", [])

        for iteration in range(14):
            base_signal = iteration * 5

            for i in range(5):
                signal_idx = base_signal + i

                if signal_idx < len(unpack_signal_names):
                    offset = 0 + (iteration * 10) + (i * 2)
                    if offset < len(grant_data) and offset + 1 < len(grant_data):
                        grant_cycles = grant_data[offset]
                        grant_count = grant_data[offset + 1]

                        req_cycles = 0
                        req_count = 0
                        if offset < len(request_data) and offset + 1 < len(
                            request_data
                        ):
                            req_cycles = request_data[offset]
                            req_count = request_data[offset + 1]

                        signal_name = unpack_signal_names[signal_idx]
                        unpack_results[signal_name] = (
                            grant_cycles,
                            grant_count,
                            req_cycles,
                            req_count,
                        )

                if signal_idx < len(math_signal_names):
                    offset = 200 + (iteration * 10) + (i * 2)
                    if offset < len(grant_data) and offset + 1 < len(grant_data):
                        grant_cycles = grant_data[offset]
                        grant_count = grant_data[offset + 1]

                        req_cycles = 0
                        req_count = 0
                        if offset < len(request_data) and offset + 1 < len(
                            request_data
                        ):
                            req_cycles = request_data[offset]
                            req_count = request_data[offset + 1]

                        signal_name = math_signal_names[signal_idx]
                        math_results[signal_name] = (
                            grant_cycles,
                            grant_count,
                            req_cycles,
                            req_count,
                        )

                if signal_idx < len(pack_signal_names):
                    offset = 400 + (iteration * 10) + (i * 2)
                    if offset < len(grant_data) and offset + 1 < len(grant_data):
                        grant_cycles = grant_data[offset]
                        grant_count = grant_data[offset + 1]

                        req_cycles = 0
                        req_count = 0
                        if offset < len(request_data) and offset + 1 < len(
                            request_data
                        ):
                            req_cycles = request_data[offset]
                            req_count = request_data[offset + 1]

                        signal_name = pack_signal_names[signal_idx]
                        pack_results[signal_name] = (
                            grant_cycles,
                            grant_count,
                            req_cycles,
                            req_count,
                        )

        def get_category(signal_name):
            if signal_name.startswith("FPU_") or signal_name.startswith("SFPU_"):
                return (0, "FPU")
            elif signal_name.startswith("INST_"):
                return (1, "INSTRN_THREAD")
            elif signal_name.startswith("UNPACK_") or signal_name.startswith(
                "TDMA_SRC"
            ):
                return (2, "TDMA_UNPACK")
            elif signal_name.startswith("PACK_") or signal_name.startswith("DSTAC_"):
                return (3, "TDMA_PACK")
            elif signal_name.startswith("MATH_"):
                return (2, "TDMA_UNPACK")
            elif (
                signal_name.startswith("NOC_")
                or signal_name.startswith("TDMA_BUNDLE")
                or signal_name.startswith("TDMA_PACKER")
                or signal_name.startswith("TDMA_EXT")
            ):
                return (4, "L1")
            elif signal_name.startswith("SRC") or signal_name == "STALLED":
                return (1, "INSTRN_THREAD")
            else:
                return (5, "OTHER")

        print("\n🔧 UNPACK Thread (TRISC0) - Sorted by category:")
        sorted_unpack = sorted(
            unpack_results.items(), key=lambda x: (get_category(x[0])[0], x[0])
        )
        current_category = None
        for signal_name, (
            grant_cycles,
            grant_count,
            req_cycles,
            req_count,
        ) in sorted_unpack:
            category = get_category(signal_name)[1]
            if category != current_category:
                print(f"\n   [{category}]")
                current_category = category
            print(
                f"   {signal_name:25s}: cycles={grant_cycles:10d}, requests={req_count:8d}, grants={grant_count:8d}"
            )

        print("\n🔧 MATH Thread (TRISC1) - Sorted by category:")
        sorted_math = sorted(
            math_results.items(), key=lambda x: (get_category(x[0])[0], x[0])
        )
        current_category = None
        for signal_name, (
            grant_cycles,
            grant_count,
            req_cycles,
            req_count,
        ) in sorted_math:
            category = get_category(signal_name)[1]
            if category != current_category:
                print(f"\n   [{category}]")
                current_category = category
            print(
                f"   {signal_name:25s}: cycles={grant_cycles:10d}, requests={req_count:8d}, grants={grant_count:8d}"
            )

        print("\n🔧 PACK Thread (TRISC2) - Sorted by category:")
        sorted_pack = sorted(
            pack_results.items(), key=lambda x: (get_category(x[0])[0], x[0])
        )
        current_category = None
        for signal_name, (
            grant_cycles,
            grant_count,
            req_cycles,
            req_count,
        ) in sorted_pack:
            category = get_category(signal_name)[1]
            if category != current_category:
                print(f"\n   [{category}]")
                current_category = category
            print(
                f"   {signal_name:25s}: cycles={grant_cycles:10d}, requests={req_count:8d}, grants={grant_count:8d}"
            )
    else:
        print(
            "\n⚠️  No multi-iteration data provided - run test with multiple iterations for full signal coverage"
        )

    print("\n" + "=" * 100 + "\n")
