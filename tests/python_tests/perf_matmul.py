# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0

from typing import List

import pytest

from helpers.format_arg_mapping import DestAccumulation, MathFidelity
from helpers.format_config import (
    DataFormat,
    FormatConfig,
    InputOutputFormat,
    is_dest_acc_needed,
)
from helpers.matmul_sweep import (
    generate_matmul_dimension_combinations,
    generate_tile_dims,
)
from helpers.param_config import parametrize
from helpers.perf import PerfRunType, Profiler, perf_benchmark, update_report


def matmul_combos(
    formats: List[FormatConfig],
    dest_acc: List[DestAccumulation],
):
    def _dest_bank_max_tiles(format: FormatConfig, dest_acc: DestAccumulation):
        if is_dest_acc_needed(format) or dest_acc == DestAccumulation.Yes:
            return 4
        return 8

    unique_max_tiles = set(
        _dest_bank_max_tiles(fmt, acc) for fmt in formats for acc in dest_acc
    )
    dimensions = {
        max_tiles: generate_matmul_dimension_combinations(max_tiles)
        for max_tiles in unique_max_tiles
    }

    return [
        (format, accumulation, dims)
        for format in formats
        for accumulation in dest_acc
        for dims in dimensions[_dest_bank_max_tiles(format, accumulation)]
    ]


@pytest.mark.perf
@parametrize(
    test_name="matmul_perf",
    combos=matmul_combos(
        formats=[InputOutputFormat(DataFormat.Float16_b, DataFormat.Float32)],
        # formats=input_output_formats(
        #     [
        #         DataFormat.Float16_b,
        #         DataFormat.Float32,
        #     ]
        # ),
        dest_acc=[DestAccumulation.Yes],
    ),
    math_fidelity=[
        MathFidelity.HiFi2,
    ],
)
def test_perf_matmul(perf_report, test_name, combos, math_fidelity):

    formats, dest_acc, (matrix_a, matrix_b) = combos

    if is_dest_acc_needed(formats) and dest_acc == DestAccumulation.No:
        pytest.skip("Dest accumulation must be enabled for this format")

    run_types = [PerfRunType.PACK_ISOLATE]

    # Calculate all matmul dimensions using helper function
    matmul_dims = generate_tile_dims((matrix_a, matrix_b))

    test_config = {
        "formats": formats,
        "testname": test_name,
        "loop_factor": 256,
        "tile_cnt": matmul_dims.output_tile_cnt,
        "input_A_dimensions": matrix_a,
        "input_B_dimensions": matrix_b,
        "output_dimensions": matmul_dims.output_dimensions,
        "rt_dim": matmul_dims.rt_dim,
        "ct_dim": matmul_dims.ct_dim,
        "kt_dim": matmul_dims.kt_dim,
        "dest_acc": dest_acc,
        "math_fidelity": math_fidelity,
    }

    results = perf_benchmark(test_config, run_types)
    profiler_data = Profiler.get_data(test_name)
    Profiler.dump_csv(profiler_data, filename="profiler_data.csv")
    update_report(perf_report, test_config, results)
