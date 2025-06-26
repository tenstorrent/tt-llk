# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0

import pytest

from helpers.format_arg_mapping import (
    DestAccumulation,
    MathFidelity,
    MathOperation,
)
from helpers.format_config import DataFormat
from helpers.param_config import (
    clean_params,
    generate_param_ids,
    generate_params,
    input_output_formats,
)
from helpers.perf import ALL_RUN_TYPES, perf_benchmark, write_to_report, PerfRunType

# SUPPORTED FORMATS FOR TEST
supported_formats = [DataFormat.Float16_b]

test_formats = input_output_formats(supported_formats)
all_params = generate_params(
    ["eltwise_binary_fpu_perf"],
    test_formats,
    dest_acc=[DestAccumulation.No],
    mathop=[MathOperation.Elwadd],
    math_fidelity=[
        MathFidelity.LoFi,
    ],
)
param_ids = generate_param_ids(all_params)


@pytest.mark.perf
@pytest.mark.parametrize(
    "testname, formats, dest_acc, mathop, math_fidelity",
    clean_params(all_params),
    ids=param_ids,
)
def test_perf_eltwise_binary_fpu(testname, formats, dest_acc, mathop, math_fidelity):

    # MathFidelity is only used for Elwmul
    if mathop != MathOperation.Elwmul and math_fidelity != MathFidelity.LoFi:
        pytest.skip("Fidelity does not affect Elwadd and Elwsub operations")

    for num_tiles in [1, 2, 3, 4, 5, 8, 16, 32, 64, 128, 256, 512, 1024]:
        test_config = {
            "testname": testname,
            "num_iterations": 8,
            "tile_cnt": num_tiles,
            "formats": formats,
            "dest_acc": dest_acc,
            "mathop": mathop,
            "math_fidelity": math_fidelity,
        }

        results = perf_benchmark(test_config, [PerfRunType.L1_TO_L1], test_config["num_iterations"])
        write_to_report(test_config, results)
