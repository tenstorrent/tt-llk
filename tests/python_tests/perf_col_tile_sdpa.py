# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0

import pytest

from conftest import skip_for_blackhole
from helpers.format_arg_mapping import (
    DestAccumulation,
    MathFidelity,
    MathOperation,
)
from helpers.format_config import DataFormat
from helpers.param_config import (
    input_output_formats,
    parametrize,
)
from helpers.perf import (
    PerfRunType,
    perf_benchmark,
    update_report,
)


@skip_for_blackhole
@pytest.mark.perf
@parametrize(
    test_name="col_tile_sdpa_perf",
    formats=input_output_formats([DataFormat.Float16_b]),
    mathop=[MathOperation.Elwsub],
    math_fidelity=[
        MathFidelity.LoFi,
    ],
    tile_count=8,
    dest_acc=[DestAccumulation.No],
)
def test_perf_col_tile_sdpa(
    perf_report, test_name, formats, mathop, math_fidelity, dest_acc, tile_count
):

    # MathFidelity is only used for Elwmul
    if mathop != MathOperation.Elwmul and math_fidelity != MathFidelity.LoFi:
        pytest.skip("Fidelity does not affect Elwadd and Elwsub operations")

    test_config = {
        "testname": test_name,
        "mathop": mathop,
        "formats": formats,
        "math_fidelity": math_fidelity,
        "tile_cnt": tile_count,
        "dest_acc": dest_acc,
    }

    results = perf_benchmark(test_config, [PerfRunType.L1_TO_L1], 100)
    update_report(perf_report, test_config, results)
