# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0

from typing import List

import torch
from helpers.device import BootMode
from helpers.format_config import DataFormat, FormatConfig, is_dest_acc_needed
from helpers.golden_generators import MatmulGolden, get_golden_generator
from helpers.llk_params import DestAccumulation, MathFidelity, format_dict
from helpers.matmul_sweep import (
    generate_matmul_dimension_combinations,
    generate_tile_dims,
)
from helpers.param_config import input_output_formats, parametrize
from helpers.stimuli_config import StimuliConfig
from helpers.stimuli_generator import generate_stimuli
from helpers.test_config import TestConfig
from helpers.test_variant_parameters import (
    CRK_TILE_DIMM,
    INPUT_DIMENSIONS,
    MATH_FIDELITY,
    NUM_FACES,
    TILE_COUNT,
)
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
    math_fidelity,
    format_dest_acc_and_dims,
    workers_tensix_coordinates,
    boot_mode=BootMode.DEFAULT,
):
    torch_format = format_dict[format_dest_acc_and_dims[0].output_format]

    formats = format_dest_acc_and_dims[0]
    dest_acc = format_dest_acc_and_dims[1]
    input_A_dimensions = format_dest_acc_and_dims[2][0]
    input_B_dimensions = format_dest_acc_and_dims[2][1]

    src_A, tile_cnt_A, src_B, tile_cnt_B = generate_stimuli(
        stimuli_format_A=formats.input_format,
        input_dimensions_A=input_A_dimensions,
        stimuli_format_B=formats.input_format,
        input_dimensions_B=input_B_dimensions,
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
        # Golden cannot model FPU strided for tilized data computation, so we tilize output after computation
        tilize=True,
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

    configuration = TestConfig(
        "sources/matmul_test.cpp",
        formats,
        templates=[
            MATH_FIDELITY(math_fidelity),
            INPUT_DIMENSIONS(input_A_dimensions, input_B_dimensions),
        ],
        runtimes=[
            NUM_FACES(),
            TILE_COUNT(matmul_dims.output_tile_cnt),
            CRK_TILE_DIMM(matmul_dims.ct_dim, matmul_dims.rt_dim, matmul_dims.kt_dim),
        ],
        variant_stimuli=StimuliConfig(
            tilized_A.flatten(),
            formats.input_format,
            tilized_B.flatten(),
            formats.input_format,
            formats.output_format,
            tile_count_A=tile_cnt_A,
            tile_count_B=tile_cnt_B,
            tile_count_res=matmul_dims.output_tile_cnt,
        ),
        dest_acc=dest_acc,
        boot_mode=boot_mode,
    )

    # Configure performance counters from Python
    from helpers.counters import configure_perf_counters, counter

    # UNPACK counters (same as C++ version)
    unpack_counters = [
        counter("INSTRN_THREAD", "INST_UNPACK"),
        counter("INSTRN_THREAD", "INST_CFG"),
        counter("FPU", "FPU_OP_VALID"),
        counter("FPU", "SFPU_OP_VALID"),
        counter("TDMA_UNPACK", "UNPACK_BUSY_0"),
        counter("TDMA_UNPACK", "UNPACK_BUSY_1"),
        counter("TDMA_PACK", "PACK_NOT_DEST_STALL"),
        counter("L1", "NOC_RING0_INCOMING_1", mux_ctrl_bit4=0),
    ]

    # MATH counters (same as C++ version)
    math_counters = [
        counter("INSTRN_THREAD", "INST_MATH"),
        counter("INSTRN_THREAD", "STALLED"),
        counter("FPU", "FPU_OP_VALID"),
        counter("FPU", "SFPU_OP_VALID"),
        counter("TDMA_UNPACK", "MATH_INSTR_VALID"),
        counter("TDMA_UNPACK", "MATH_INSTR_SRC_READY"),
        counter("TDMA_PACK", "PACK_NOT_DEST_STALL"),
        counter("L1", "L1_ARB_TDMA_BUNDLE_0", mux_ctrl_bit4=0),
    ]

    # PACK counters (same as C++ version)
    pack_counters = [
        counter("INSTRN_THREAD", "INST_PACK"),
        counter("INSTRN_THREAD", "INST_CFG"),
        counter("FPU", "SFPU_OP_VALID"),
        counter("FPU", "FPU_OP_VALID"),
        counter("TDMA_PACK", "PACK_BUSY_10"),
        counter("TDMA_PACK", "PACK_BUSY_11"),
        counter("TDMA_UNPACK", "UNPACK_BUSY_0"),
        counter("L1", "NOC_RING0_OUTGOING_1", mux_ctrl_bit4=0),
    ]

    # Configure counters for each thread
    configure_perf_counters(
        unpack_counters, location=workers_tensix_coordinates, thread="UNPACK"
    )
    configure_perf_counters(
        math_counters, location=workers_tensix_coordinates, thread="MATH"
    )
    configure_perf_counters(
        pack_counters, location=workers_tensix_coordinates, thread="PACK"
    )

    # Run the test - counters configured from Python
    res_from_L1 = configuration.run(workers_tensix_coordinates)

    # Read and print the counter results
    from helpers.counters import print_perf_counters, read_perf_counters

    for thread in ["UNPACK", "MATH", "PACK"]:
        results = read_perf_counters(thread=thread)
        if results:
            print_perf_counters(results, thread=thread)

    print("\n" + "=" * 80)
    print("Counter Collection Summary")
    print("=" * 80)
    for thread in ["UNPACK", "MATH", "PACK"]:
        results = read_perf_counters(thread=thread)
        count = len(results) if results else 0
        status = "✓" if count == 8 else "✗"
        print(f"{status} {thread}: {count}/8 counters collected")
    print("=" * 80)

    assert len(res_from_L1) == len(
        golden_tensor
    ), "Result tensor and golder tensor are not of the same length"

    res_tensor = torch.tensor(res_from_L1, dtype=torch_format)

    assert passed_test(
        golden_tensor, res_tensor, formats.output_format
    ), "Assert against golden failed"
