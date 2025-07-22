# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
import pytest
import torch

from helpers.device import (
    collect_results,
    write_stimuli_to_l1,
)
from helpers.format_arg_mapping import (
    DestAccumulation,
    MathOperation,
    ReduceDimension,
    ReducePool,
    format_dict,
)
from helpers.format_config import DataFormat, InputOutputFormat
from helpers.golden_generators import (
    ReduceGolden,
    TilizeGolden,
    get_golden_generator,
)
from helpers.param_config import (
    input_output_formats,
)
from helpers.stimuli_generator import generate_stimuli
from helpers.test_config import ProfilerBuild, run_test
from helpers.tilize_untilize import untilize
from helpers.utils import passed_test

# Helper dictionary to map reduce dimensions to math operations
mathop_mapping = {
    ReduceDimension.Row: MathOperation.ReduceRow,
    ReduceDimension.Column: MathOperation.ReduceColumn,
    ReduceDimension.Scalar: MathOperation.ReduceScalar,
}

# -----------------------------------------------------------------------------
# 2. Helper constants & parameter space definition
# -----------------------------------------------------------------------------

# SUPPORTED FORMATS FOR TEST - following the same pattern as other tests
supported_formats = [
    DataFormat.Float16_b,  # Most widely used
    DataFormat.Float16,  # Standard FP16
    DataFormat.Float32,  # High precision
    DataFormat.Bfp8_b,  # Block float format
    # Note: unpack_tilize works with these formats
]

# REDUCE DIMENSIONS TO TEST
reduce_dimensions = [
    ReduceDimension.Row,
    ReduceDimension.Column,
    ReduceDimension.Scalar,
]

# POOL TYPES TO TEST
pool_types = [
    ReducePool.Max,
    ReducePool.Sum,
    ReducePool.Average,
]

# Generate format combinations (both same and mixed format combinations)
test_formats = input_output_formats(supported_formats, same=True) + [
    # Add some mixed format combinations that are commonly tested
    InputOutputFormat(DataFormat.Float16_b, DataFormat.Float32),
    InputOutputFormat(DataFormat.Float16, DataFormat.Float32),
]

# Generate parameter combinations manually for comprehensive testing
all_params = [
    {
        "testname": "unpack_tilize_reduce",
        "formats": fmt,
        "dest_acc": dest_acc,
        "reduce_dim": reduce_dim,
        "pool_type": pool_type,
    }
    for fmt in test_formats
    for dest_acc in [DestAccumulation.No]  # Reduce typically uses No dest_acc
    for reduce_dim in reduce_dimensions
    for pool_type in pool_types
]

# Generate parameter IDs manually
param_ids = [
    f"dim={p['reduce_dim'].name}|pool={p['pool_type'].name}|fmt={p['formats'].input_format.name}->{p['formats'].output_format.name}|acc={p['dest_acc'].name}"
    for p in all_params
]


# -----------------------------------------------------------------------------
# 3. The test implementation
# -----------------------------------------------------------------------------
@pytest.mark.parametrize("config", all_params, ids=param_ids)
def test_sweep_test(config):
    """
    Runs the C++ unpack_tilize_reduce.cpp kernel for a full sweep of all parameter combinations.

    This fused operation:
    1. Takes row major input data (src_A)
    2. Applies unpack_tilize to convert to tilized format
    3. Performs reduce operation on tilized data
    4. Outputs reduced result

    Tests across all commonly used formats with 32×32 tensor shape (1 tile).
    """

    # Extract parameters from config
    formats = config["formats"]
    reduce_dim = config["reduce_dim"]
    pool_type = config["pool_type"]
    dest_acc = config["dest_acc"]

    # ------------------------------------------------------------------
    # Generate input stimuli
    # ------------------------------------------------------------------
    input_dimensions = [32, 32]  # Single tile for reduce operations
    src_A, src_B, tile_cnt = generate_stimuli(
        formats.input_format,
        formats.input_format,
        input_dimensions=input_dimensions,
    )

    # Set src_B constants for reduce operation (exactly like test_reduce.py)
    if pool_type in [ReducePool.Max, ReducePool.Sum]:
        # result in srcA should be divided by 1
        src_B = torch.full((1024,), 1)
    else:
        # reduce average divides by length of elements in array we reduce
        if reduce_dim in [ReduceDimension.Column, ReduceDimension.Row]:
            src_B = torch.full((1024,), 1 / 32)
        else:
            src_B = torch.full((1024,), torch.sqrt(torch.tensor(1 / 1024)))

    # ------------------------------------------------------------------
    # Produce golden output
    # ------------------------------------------------------------------
    # Golden reference calculation follows the fusion flow:
    # 1. First, apply tilize operation to input (simulating unpack_tilize)
    generate_tilize_golden = get_golden_generator(TilizeGolden)
    tilized_A = generate_tilize_golden(src_A, input_dimensions, formats.output_format)

    # 2. Then apply reduce operation on the tilized input
    generate_reduce_golden = get_golden_generator(ReduceGolden)
    golden_tensor = generate_reduce_golden(
        tilized_A, reduce_dim, pool_type, formats.output_format
    )

    # ------------------------------------------------------------------
    # Write stimuli to device L1 and run the HW test
    # ------------------------------------------------------------------
    res_address = write_stimuli_to_l1(
        src_A,  # Row major format - hardware will handle unpack_tilize
        src_B,  # Scaling factors for reduce operation
        formats.input_format,
        formats.input_format,
        tile_count=tile_cnt,
    )

    # Get the math operation for reduce
    mathop = mathop_mapping[reduce_dim]

    # Create test config based on the parametrized config
    test_config = config.copy()
    test_config.update(
        {
            "testname": "unpack_tilize_reduce",
            "tile_cnt": tile_cnt,
            "input_dimensions": input_dimensions,
            "mathop": mathop,
            "L1_to_L1_iterations": 2,  # Fused operation: unpack_tilize + reduce
        }
    )

    run_test(test_config, profiler_build=ProfilerBuild.No)

    # ------------------------------------------------------------------
    # Collect results and compare with golden
    # ------------------------------------------------------------------
    res_from_L1 = collect_results(
        formats,
        tile_count=tile_cnt,
        address=res_address,
    )
    assert len(res_from_L1) == len(golden_tensor)

    torch_format = format_dict[formats.output_format]
    res_tensor = torch.tensor(res_from_L1, dtype=torch_format)

    # Untilize the result for comparison (as done in test_reduce.py)
    res_tensor = untilize(res_tensor, formats.output_format)

    assert passed_test(
        golden_tensor,
        res_tensor,
        formats.output_format,
        test_config.get(
            "L1_to_L1_iterations"
        ),  # Account for accumulated precision loss in fused operation
    )
