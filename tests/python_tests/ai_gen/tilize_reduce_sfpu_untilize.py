# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0
"""Fused Tilize + Reduce + SFPU + Untilize test.

This test implements the pipeline:
unpack_tilize → reduce → sfpu_unary → pack_untilize

This represents a common ML pattern where:
1. Row-major input tensors are tilized for efficient computation
2. Data is reduced across specified dimensions (row/column/scalar)
3. Unary activations are applied to the reduction result
4. Results are untilized back to row-major format in L1

The test sweeps across data formats (Float16, Float16_b), reduce dimensions,
pool types, and a selection of stable SFPU unary operations.
"""
from __future__ import annotations

import pytest
import torch

from helpers.chip_architecture import ChipArchitecture, get_chip_architecture
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
from helpers.format_config import DataFormat
from helpers.golden_generators import (
    ReduceGolden,
    UnarySFPUGolden,
    get_golden_generator,
)
from helpers.param_config import input_output_formats
from helpers.stimuli_generator import generate_stimuli
from helpers.test_config import run_test
from helpers.tilize_untilize import tilize, untilize
from helpers.utils import passed_test

# -----------------------------------------------------------------------------
# 1. Parameter space definition
# -----------------------------------------------------------------------------

# Test both Float16 and Float16_b formats
supported_formats = [
    DataFormat.Float16,
    DataFormat.Float16_b,
]

test_formats = input_output_formats(supported_formats, same=True)

# Sweep across all reduce dimensions and pool types that HW supports
reduce_dims = [
    ReduceDimension.Column,
    ReduceDimension.Row,
    ReduceDimension.Scalar,
]

pool_types = [ReducePool.Sum, ReducePool.Max, ReducePool.Average]

# Stable SFPU unary operations - avoiding problematic ones
unary_ops = [
    MathOperation.Abs,
    MathOperation.Gelu,
    MathOperation.Neg,
    MathOperation.Silu,
    MathOperation.Sqrt,
    MathOperation.Square,
]

# Test parameters for comprehensive coverage
math_fidelities = [MathFidelity.LoFi, MathFidelity.HiFi2, MathFidelity.HiFi4]
dest_accs = [DestAccumulation.Yes, DestAccumulation.No]
approx_modes = [ApproximationMode.No, ApproximationMode.Yes]

# Assemble full parameter list
all_params: list[dict] = [
    {
        "testname": "tilize_reduce_sfpu_untilize",
        "formats": fmt,
        "reduce_dim": rdim,
        "pool_type": ptype,
        "unary_op": uop,
        "math_fidelity": fidelity,
        "dest_acc": dest_acc,
        "approx_mode": approx_mode,
    }
    for fmt in test_formats
    for rdim in reduce_dims
    for ptype in pool_types
    for uop in unary_ops
    for fidelity in math_fidelities
    for dest_acc in dest_accs
    for approx_mode in approx_modes
]

param_ids = [
    f"{cfg['reduce_dim'].name}|{cfg['pool_type'].name}|{cfg['unary_op'].name}|{cfg['formats'].input_format.name}|{cfg['math_fidelity'].name}|{cfg['dest_acc'].name}|{cfg['approx_mode'].name}"
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
def test_tilize_reduce_sfpu_untilize(config):
    """Run the fused Tilize+Reduce+SFPU+Untilize kernel and compare with golden."""

    # ------------------------- Extract config ------------------------------
    fmt = config["formats"]
    reduce_dim: ReduceDimension = config["reduce_dim"]
    pool_type: ReducePool = config["pool_type"]
    unary_op: MathOperation = config["unary_op"]
    math_fidelity: MathFidelity = config["math_fidelity"]
    dest_acc: DestAccumulation = config["dest_acc"]
    approx_mode: ApproximationMode = config["approx_mode"]

    # ------------------------- Skip conditions -----------------------------
    # Skip Square operation with LoFi math fidelity due to known issues
    if unary_op == MathOperation.Square and math_fidelity == MathFidelity.LoFi:
        pytest.skip("Square operation in LoFi is not fully functional yet")

    # Skip certain operations on Blackhole with specific format combinations
    if (
        fmt.input_format == fmt.output_format == DataFormat.Float16
        and unary_op
        in [
            MathOperation.Sqrt,
            MathOperation.Square,
        ]
        and dest_acc == DestAccumulation.No
        and get_chip_architecture() == ChipArchitecture.BLACKHOLE
    ):
        pytest.skip("BFP8 does not support certain operations on Blackhole")

    # --------------------- Generate input stimuli -------------------------
    input_dimensions = [32, 32]
    src_A, src_B, tile_cnt = generate_stimuli(
        fmt.input_format,
        fmt.input_format,
        input_dimensions=input_dimensions,
    )

    # For Reduce operations, we need appropriate src_B values
    if pool_type in [ReducePool.Max, ReducePool.Sum]:
        # For sum/max, src_B should be 1 to not affect the result
        src_B = torch.full((1024,), 1)
    else:
        # For average, divide by the number of elements being reduced
        if reduce_dim in [ReduceDimension.Column, ReduceDimension.Row]:
            src_B = torch.full((1024,), 1 / 32)
        else:
            src_B = torch.full((1024,), torch.sqrt(torch.tensor(1 / 1024)))

    # Handle domain restrictions for operations that require specific input ranges
    if unary_op == MathOperation.Sqrt:
        # Ensure input will be non-negative for sqrt
        src_A = src_A.abs()

    # --------------------- Golden computation -----------------------------
    # Step 1: Tilize input (convert row-major to tiled format)
    tilized_A = tilize(src_A, fmt.input_format)

    # Step 2: Apply reduce operation
    gen_reduce = get_golden_generator(ReduceGolden)
    reduce_out = gen_reduce(tilized_A, reduce_dim, pool_type, fmt.output_format)

    # Step 3: Apply unary SFPU operation
    gen_unary = get_golden_generator(UnarySFPUGolden)
    sfpu_out = gen_unary(unary_op, reduce_out, fmt.output_format, math_fidelity)

    # Step 4: Untilize result (convert back to row-major format)
    # Calculate output dimensions after reduction
    if reduce_dim == ReduceDimension.Row:
        output_dims = [1, 32]
    elif reduce_dim == ReduceDimension.Column:
        output_dims = [32, 1]
    else:  # Scalar
        output_dims = [1, 1]

    golden_tensor = untilize(sfpu_out, fmt.output_format)

    # --------------------- Hardware test execution ------------------------
    # Write stimuli to L1 (in row-major format, will be tilized by kernel)
    res_address = write_stimuli_to_l1(
        src_A, src_B, fmt.input_format, fmt.input_format, tile_count=tile_cnt
    )

    # Configure test parameters for kernel
    test_config = {
        "testname": config["testname"],
        "formats": fmt,
        "mathop": _reduce_to_mathop[reduce_dim],
        "reduce_dim": reduce_dim,
        "pool_type": pool_type,
        "unary_op": unary_op,
        "math_fidelity": math_fidelity,
        "dest_acc": dest_acc,
        "approx_mode": approx_mode,
        "tile_cnt": tile_cnt,
        "input_dimensions": input_dimensions,
        "output_dimensions": output_dims,
    }

    # Build and execute kernel
    run_test(test_config)

    # --------------------- Collect and validate results -------------------
    # Calculate expected result size
    expected_size = output_dims[0] * output_dims[1]

    # Read results from L1
    res_from_L1 = collect_results(
        fmt,
        tile_count=1,  # Result is always single tile after untilize
        address=res_address,
    )

    # Verify result length
    assert len(res_from_L1) == len(
        golden_tensor
    ), f"Result length mismatch: got {len(res_from_L1)}, expected {len(golden_tensor)}"

    # Convert to tensor for comparison
    torch_format = format_dict[fmt.output_format]
    res_tensor = torch.tensor(res_from_L1, dtype=torch_format)

    # Validate against golden reference
    assert passed_test(golden_tensor, res_tensor, fmt.output_format), (
        f"Tilize->Reduce({reduce_dim.name},{pool_type.name})->SFPU({unary_op.name})->Untilize failed "
        f"for format {fmt.input_format.name}->{fmt.output_format.name}, "
        f"dest_acc={dest_acc.name}, fidelity={math_fidelity.name}, approx={approx_mode.name}"
    )
