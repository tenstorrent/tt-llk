# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0

import torch
from helpers.device import collect_results, write_stimuli_to_l1
from helpers.format_config import DataFormat
from helpers.golden_generators import ReduceGolden, get_golden_generator
from helpers.llk_params import (
    DestAccumulation,
    MathOperation,
    ReduceDimension,
    ReducePool,
    format_dict,
)
from helpers.param_config import input_output_formats, parametrize
from helpers.perf_counters import (
    CounterBank,
    PerfCounterConfig,
    clear_perf_counters,
    collect_perf_counters,
    print_perf_counters,
    write_perf_config,
)
from helpers.stimuli_generator import generate_stimuli

TILE_HEIGHT = 32
TILE_WIDTH = 32
from helpers.test_config import run_test
from helpers.tilize_untilize import untilize
from helpers.utils import passed_test

# Helper dictionary to map reduce dimensions to math operations
mathop_mapping = {
    ReduceDimension.Row: MathOperation.ReduceRow,
    ReduceDimension.Column: MathOperation.ReduceColumn,
    ReduceDimension.Scalar: MathOperation.ReduceScalar,
}


@parametrize(
    test_name="reduce_test",
    formats=input_output_formats(
        [
            DataFormat.Float16_b,
            DataFormat.Float16,
            DataFormat.Float32,
            DataFormat.Bfp8_b,
        ]
    ),
    dest_acc=[DestAccumulation.No],
    reduce_dim=[ReduceDimension.Row, ReduceDimension.Column, ReduceDimension.Scalar],
    pool_type=[ReducePool.Max, ReducePool.Average, ReducePool.Sum],
)
def test_reduce(test_name, formats, dest_acc, reduce_dim, pool_type):

    input_dimensions = [32, 32]

    src_A, src_B, tile_cnt = generate_stimuli(
        formats.input_format, formats.input_format, input_dimensions=input_dimensions
    )

    if pool_type in [
        ReducePool.Max,
        ReducePool.Sum,
    ]:  # result in srcA should be divided by 1
        src_B = torch.full((1024,), 1)
    else:
        # reduce average divides by length of elements in array we reduce
        src_B = torch.full((1024,), 1 / 32)

    generate_golden = get_golden_generator(ReduceGolden)
    golden_tensor = generate_golden(src_A, reduce_dim, pool_type, formats.output_format)

    mathop = mathop_mapping[reduce_dim]

    test_config = {
        "formats": formats,
        "testname": test_name,
        "dest_acc": dest_acc,
        "reduce_dim": reduce_dim,
        "pool_type": pool_type,
        "mathop": mathop,
    }

    res_address = write_stimuli_to_l1(
        test_config,
        src_A,
        src_B,
        formats.input_format,
        formats.input_format,
        tile_count_A=tile_cnt,
        tile_count_B=tile_cnt,
    )

    # Configure performance counters
    perf_config = PerfCounterConfig()
    perf_config.add_counter(CounterBank.FPU, "SFPU_OP_VALID")
    perf_config.add_counter(CounterBank.INSTRN_THREAD, "INST_MATH")
    perf_config.add_counter(CounterBank.TDMA_UNPACK, "MATH_INSTR_VALID")
    perf_config.add_counter(CounterBank.L1, "NOC_RING0_OUTGOING_0")
    perf_config.add_counter(CounterBank.TDMA_PACK, "PACK_BUSY_10")
    perf_config.set_mode("grants")

    clear_perf_counters()
    write_perf_config(perf_config)

    run_test(test_config)

    reduce_dim_str = str(reduce_dim)
    if "Row" in reduce_dim_str:
        elements_reduced = TILE_HEIGHT
    elif "Column" in reduce_dim_str:
        elements_reduced = TILE_WIDTH
    elif "Scalar" in reduce_dim_str:
        elements_reduced = TILE_HEIGHT * TILE_WIDTH
    else:
        elements_reduced = TILE_HEIGHT

    workload_info = {
        "test": "reduce",
        "tile_ops": tile_cnt,
        "reduce_dimension": reduce_dim_str,
        "elements_per_tile": elements_reduced,
        "operations": tile_cnt * elements_reduced,
    }

    results = collect_perf_counters(perf_config)
    print_perf_counters(results, workload_info)

    res_from_L1 = collect_results(formats, tile_count=tile_cnt, address=res_address)
    assert len(res_from_L1) == len(golden_tensor)

    res_tensor = torch.tensor(res_from_L1, dtype=format_dict[formats.output_format])
    res_tensor = untilize(res_tensor, formats.output_format)

    assert passed_test(golden_tensor, res_tensor, formats.output_format)
