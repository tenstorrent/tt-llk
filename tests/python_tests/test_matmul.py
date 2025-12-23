# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0

from typing import List

import torch
from helpers.counters import (
    counter,
    measure_perf_counters_batched,
    print_perf_counters,
)
from helpers.device import BootMode, collect_results, write_stimuli_to_l1
from helpers.format_config import DataFormat, FormatConfig, is_dest_acc_needed
from helpers.golden_generators import MatmulGolden, get_golden_generator
from helpers.llk_params import DestAccumulation, MathFidelity, format_dict
from helpers.matmul_sweep import (
    generate_matmul_dimension_combinations,
    generate_tile_dims,
)
from helpers.param_config import input_output_formats, parametrize
from helpers.stimuli_generator import generate_stimuli
from helpers.tilize_untilize import tilize_block
from helpers.utils import passed_test


def generate_format_aware_matmul_combinations(
    formats_list: List[FormatConfig],
    dest_acc_modes: List[DestAccumulation],
):
    """
    Generate matmul dimension combinations for multiple tiles.

    Rules:
    1. Format outliers (Float16_b->Float16, Bfp8_b->Float16) MUST use dest_acc=Yes
    2. Running matmul tests on DestSync.Half, max tile count is 8
    3. When dest_acc=Yes: max 4 tiles (32-bit dest register)
    4. When dest_acc=No: max 8 tiles (16-bit dest register)

    Returns: List of (format, dest_acc, dimensions) tuples
    """
    combinations = []

    for fmt in formats_list:
        base_max_tiles = 4 if is_dest_acc_needed(fmt) else 8

        for dest_acc in dest_acc_modes:
            max_tiles = 4 if dest_acc == DestAccumulation.Yes else base_max_tiles
            dimensions_list = generate_matmul_dimension_combinations(max_tiles)
            combinations.extend([(fmt, dest_acc, dims) for dims in dimensions_list])

    return combinations


# Generate format-aware combinations
MATMUL_FORMATS = input_output_formats(
    [
        DataFormat.Float16_b,
        DataFormat.Float16,
        DataFormat.Float32,
    ]
)
DEST_ACC_MODES = [DestAccumulation.No, DestAccumulation.Yes]
ALL_MATMUL_COMBINATIONS = generate_format_aware_matmul_combinations(
    MATMUL_FORMATS, DEST_ACC_MODES
)


@parametrize(
    test_name="matmul_test",
    math_fidelity=[
        MathFidelity.LoFi,
        MathFidelity.HiFi2,
        MathFidelity.HiFi3,
        MathFidelity.HiFi4,
    ],
    format_dest_acc_and_dims=ALL_MATMUL_COMBINATIONS,
)
# Note: this test is used to test boot modes, that is why it has them piped as default arguments to the test itself
def test_matmul(
    test_name, math_fidelity, format_dest_acc_and_dims, boot_mode=BootMode.DEFAULT
):
    torch_format = format_dict[format_dest_acc_and_dims[0].output_format]

    formats = format_dest_acc_and_dims[0]
    dest_acc = format_dest_acc_and_dims[1]
    input_A_dimensions = format_dest_acc_and_dims[2][0]
    input_B_dimensions = format_dest_acc_and_dims[2][1]

    src_A, _, tile_cnt_A = generate_stimuli(
        formats.input_format,
        formats.input_format,
        input_dimensions=input_A_dimensions,
        sfpu=False,
    )
    src_B, _, tile_cnt_B = generate_stimuli(
        formats.input_format,
        formats.input_format,
        input_dimensions=input_B_dimensions,
        sfpu=False,
    )

    # Calculate all matmul dimensions using helper function
    matmul_dims = generate_tile_dims((input_A_dimensions, input_B_dimensions))

    generate_golden = get_golden_generator(MatmulGolden)
    golden_tensor = generate_golden(
        src_A,
        src_B,
        formats.output_format,
        math_fidelity,
        input_A_dimensions=input_A_dimensions,
        input_B_dimensions=input_B_dimensions,
        tilize=True,  # Golden cannot model FPU strided for tilized data computation, so we tilize output after computation
    )

    if formats.input_format != DataFormat.Bfp8_b:
        tilized_A = tilize_block(
            src_A, dimensions=input_A_dimensions, stimuli_format=formats.input_format
        )
        tilized_B = tilize_block(
            src_B, dimensions=input_B_dimensions, stimuli_format=formats.input_format
        )
    else:
        # BFP8 format requires special handling for tilization
        tilized_A = src_A
        tilized_B = src_B

    test_config = {
        "formats": formats,
        "testname": test_name,
        "dest_acc": dest_acc,
        "math_fidelity": math_fidelity,
        "tile_cnt": matmul_dims.output_tile_cnt,
        "input_A_dimensions": input_A_dimensions,
        "input_B_dimensions": input_B_dimensions,
        "output_dimensions": matmul_dims.output_dimensions,
        "rt_dim": matmul_dims.rt_dim,
        "ct_dim": matmul_dims.ct_dim,
        "kt_dim": matmul_dims.kt_dim,
    }

    # Use the new helper function for writing stimuli
    res_address = write_stimuli_to_l1(
        test_config,
        tilized_A.flatten(),
        tilized_B.flatten(),
        formats.input_format,
        formats.input_format,
        tile_cnt_A,
        tile_cnt_B,
    )

    # Configure performance counters from Python (more than 5 per thread to test batching)
    unpack_counters = [
        counter("INSTRN_THREAD", "INST_UNPACK"),
        counter("FPU", "FPU_OP_VALID"),
        counter("TDMA_UNPACK", "UNPACK_BUSY_0"),
        counter("L1", "NOC_RING0_INCOMING_1", mux_ctrl_bit4=0),
        counter("TDMA_PACK", "DSTAC_RDEN_RAW_0"),
        counter("TDMA_UNPACK", "UNPACK_BUSY_1"),  # 6th counter - triggers batching
        counter("L1", "NOC_RING1_INCOMING_1", mux_ctrl_bit4=1),  # 7th counter
    ]

    math_counters = [
        counter("INSTRN_THREAD", "INST_MATH"),
        counter("FPU", "FPU_OP_VALID"),
        counter("TDMA_UNPACK", "MATH_INSTR_VALID"),
        counter("L1", "L1_ARB_TDMA_BUNDLE_0", mux_ctrl_bit4=0),
        counter("TDMA_PACK", "PACK_NOT_DEST_STALL"),
        counter("FPU", "SFPU_OP_VALID"),  # 6th counter
        counter("TDMA_UNPACK", "MATH_INSTR_BUF_RDEN"),  # 7th counter
        counter("L1", "NOC_RING0_OUTGOING_0", mux_ctrl_bit4=0),  # 8th counter
    ]

    pack_counters = [
        counter("INSTRN_THREAD", "INST_PACK"),
        counter("FPU", "SFPU_OP_VALID"),
        counter("TDMA_UNPACK", "MATH_INSTR_BUF_RDEN"),
        counter("L1", "NOC_RING0_OUTGOING_1", mux_ctrl_bit4=0),
        counter("TDMA_PACK", "PACK_BUSY_10"),
        counter("TDMA_PACK", "PACK_BUSY_11"),  # 6th counter
    ]

    # Import run_test
    from helpers.test_config import run_test

    # Measure counters with batching for each thread
    all_results = {}
    for thread, counters in [
        ("UNPACK", unpack_counters),
        ("MATH", math_counters),
        ("PACK", pack_counters),
    ]:
        print(f"\n{'='*80}")
        print(f"Measuring {len(counters)} counters for {thread} thread")
        print(f"{'='*80}")
        results = measure_perf_counters_batched(
            counters, run_test, test_config, boot_mode, thread=thread
        )
        print(f"Got {len(results)} results for {thread}")
        all_results[thread] = results
        print_perf_counters(results, thread=thread)

    total_tile_ops = matmul_dims.rt_dim * matmul_dims.ct_dim * matmul_dims.kt_dim

    res_from_L1 = collect_results(
        formats, tile_count=matmul_dims.output_tile_cnt, address=res_address
    )
    assert len(res_from_L1) == len(golden_tensor)

    res_tensor = torch.tensor(res_from_L1, dtype=torch_format)

    assert passed_test(golden_tensor, res_tensor, formats.output_format)
