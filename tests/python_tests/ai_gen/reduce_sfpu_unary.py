# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0
"""Fused Reduce + SFPU-unary test (AB-unpack → Reduce → SFPU → Pack).

This test mirrors the flow implemented in
`tests/sources/ai_gen/reduce_sfpu_unary.cpp`.
It performs a reduction (Row/Column/Scalar) followed immediately by an SFPU
unary operation on the reduction result.  Finally the data are packed back to
L1.  Host-side we generate stimuli, golden reference (Reduce + Unary) and
compare the Tensix results.

For initial bring-up we limit the sweep to Float16_b format only – the most
commonly used configuration – while still sweeping across reduce dimensions,
pool types and a subset of unary ops.
"""
from __future__ import annotations

import pytest
import torch

from helpers.device import collect_results, write_stimuli_to_l1
from helpers.format_arg_mapping import (
    ApproximationMode,
    DestAccumulation,
    MathFidelity,
    MathOperation,
    ReduceDimension,
    ReducePool,
    format_dict,
)
from helpers.format_config import DataFormat, InputOutputFormat
from helpers.golden_generators import (
    ReduceGolden,
    UnarySFPUGolden,
    get_golden_generator,
)
from helpers.stimuli_generator import generate_stimuli
from helpers.test_config import ProfilerBuild, run_test
from helpers.tilize_untilize import untilize
from helpers.utils import passed_test

# -----------------------------------------------------------------------------
# 1. Parameter space definition
# -----------------------------------------------------------------------------

# Only Float16_b format requested for now
formats_under_test = [InputOutputFormat(DataFormat.Float16_b, DataFormat.Float16_b)]

# Sweep across all reduce dimensions and pool types that HW supports
reduce_dims = [
    ReduceDimension.Column,
    ReduceDimension.Scalar,
    ReduceDimension.Row,
]

pool_types = [ReducePool.Sum, ReducePool.Max, ReducePool.Average]

# A small subset of frequently used SFPU unary ops – easy to validate
unary_ops = [
    MathOperation.Abs,
    MathOperation.Sqrt,
    MathOperation.Square,
]

# Assemble full parameter list
all_params: list[dict] = [
    {
        "formats": fmt,
        "reduce_dim": rdim,
        "pool_type": ptype,
        "unary_op": uop,
    }
    for fmt in formats_under_test
    for rdim in reduce_dims
    for ptype in pool_types
    for uop in unary_ops
]

param_ids = [
    f"{cfg['reduce_dim'].name}|{cfg['pool_type'].name}|{cfg['unary_op'].name}"
    for cfg in all_params
]

# Mapping ReduceDimension → MathOperation enum (needed for build header)
_reduce_to_mathop = {
    ReduceDimension.Row: MathOperation.ReduceRow,
    ReduceDimension.Column: MathOperation.ReduceColumn,
    ReduceDimension.Scalar: MathOperation.ReduceScalar,
}


# -----------------------------------------------------------------------------
# 2. Test implementation
# -----------------------------------------------------------------------------


@pytest.mark.parametrize("config", all_params, ids=param_ids)
def test_reduce_sfpu_unary(config):
    """Run the fused Reduce+SFPU kernel on Tensix and compare with golden."""

    # ------------------------- Extract config ------------------------------
    fmt: InputOutputFormat = config["formats"]
    reduce_dim: ReduceDimension = config["reduce_dim"]
    pool_type: ReducePool = config["pool_type"]
    unary_op: MathOperation = config["unary_op"]

    # --------------------- Generate input stimuli -------------------------
    input_dimensions = [32, 32]
    src_A, src_B, tile_cnt = generate_stimuli(
        fmt.input_format,
        fmt.input_format,
        input_dimensions=input_dimensions,
    )

    # For Reduce-sum we don't need src_B but unpack_AB expects two operands –
    # reuse existing logic: fill B with ones so it has no influence.
    if pool_type in [
        ReducePool.Max,
        ReducePool.Sum,
    ]:  # result in srcA should be divided by 1
        src_B = torch.full((1024,), 1)
    else:
        # reduce average divides by length of elements in array we reduce
        if reduce_dim in [ReduceDimension.Column, ReduceDimension.Row]:
            src_B = torch.full((1024,), 1 / 32)
        else:
            src_B = torch.full((1024,), torch.sqrt(torch.tensor(1 / 1024)))

    # --------------------- Golden computation -----------------------------
    POSITIVE_ONLY_UNARY = {MathOperation.Sqrt}

    gen_reduce = get_golden_generator(ReduceGolden)

    reduce_out = gen_reduce(src_A, reduce_dim, pool_type, fmt.output_format)

    # Guard for ops requiring non-negative domain (e.g. sqrt)
    if unary_op in POSITIVE_ONLY_UNARY and torch.min(reduce_out).item() < 0:
        # Shift src_A positively until reduce result is non-negative
        offset = abs(torch.min(reduce_out).item()) + 0.1
        src_A = (src_A + offset).to(src_A.dtype)
        reduce_out = gen_reduce(src_A, reduce_dim, pool_type, fmt.output_format)

    gen_unary = get_golden_generator(UnarySFPUGolden)
    golden_tensor = gen_unary(
        unary_op, reduce_out, fmt.output_format, MathFidelity.LoFi
    )

    # --------------------- Device execution -------------------------------
    res_address = write_stimuli_to_l1(
        src_A,
        src_B,
        fmt.input_format,
        fmt.input_format,
        tile_count=tile_cnt,
    )

    test_config = {
        "testname": "reduce_sfpu_unary",
        "formats": fmt,
        "dest_acc": DestAccumulation.No,
        "approx_mode": ApproximationMode.No,
        "mathop": _reduce_to_mathop[reduce_dim],
        "unary_op": unary_op,
        "reduce_dim": reduce_dim,
        "pool_type": pool_type,
        "math_fidelity": MathFidelity.LoFi,
        "tile_cnt": tile_cnt,
        "input_dimensions": input_dimensions,
    }

    run_test(test_config, profiler_build=ProfilerBuild.No)

    # ----------------------- Result comparison ----------------------------
    res_from_L1 = collect_results(
        fmt,
        tile_count=tile_cnt,
        address=res_address,
    )

    assert len(res_from_L1) == len(golden_tensor)

    res_tensor = torch.tensor(res_from_L1, dtype=format_dict[fmt.output_format])
    res_tensor = untilize(res_tensor, fmt.output_format)

    # Extract the leftmost column (first column) from both tensors, reshape to 1x32, and print
    print("res_tensor leftmost column:", res_tensor.view(32, 32)[:, 0].view(1, 32))
    print(
        "golden_tensor leftmost column:", golden_tensor.view(32, 32)[:, 0].view(1, 32)
    )

    assert passed_test(golden_tensor, res_tensor, fmt.output_format)
