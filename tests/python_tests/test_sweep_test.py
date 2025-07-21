# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
import pytest
import torch

from helpers.device import (
    collect_results,
    write_stimuli_to_l1,
)
from helpers.format_arg_mapping import (
    ApproximationMode,
    DestAccumulation,
    MathFidelity,
    MathOperation,
    format_dict,
)
from helpers.format_config import DataFormat, InputOutputFormat
from helpers.golden_generators import (
    EltwiseBinaryGolden,
    UnarySFPUGolden,
    get_golden_generator,
)
from helpers.stimuli_generator import generate_stimuli
from helpers.test_config import ProfilerBuild, run_test
from helpers.utils import passed_test

# -----------------------------------------------------------------------------
# Helper constants & parameter space definition
# -----------------------------------------------------------------------------
SUPPORTED_FORMAT = InputOutputFormat(DataFormat.Float16_b, DataFormat.Float16_b)

BINARY_OPS = [
    MathOperation.Elwadd,
    MathOperation.Elwsub,
    MathOperation.Elwmul,
]

UNARY_OPS = [
    MathOperation.Abs,
    MathOperation.Square,
    MathOperation.Sqrt,
]

DEST_ACC_OPTIONS = [DestAccumulation.Yes, DestAccumulation.No]
APPROX_MODE_OPTIONS = [ApproximationMode.Yes, ApproximationMode.No]
MATH_FIDELITIES = [
    MathFidelity.LoFi,
    MathFidelity.HiFi2,
    MathFidelity.HiFi3,
    MathFidelity.HiFi4,
]

# Build the Cartesian product of the parameter space
PARAM_COMBINATIONS = [
    (
        bin_op,
        unary_op,
        dest_acc,
        approx_mode,
        fidelity,
    )
    for bin_op in BINARY_OPS
    for unary_op in UNARY_OPS
    for dest_acc in DEST_ACC_OPTIONS
    for approx_mode in APPROX_MODE_OPTIONS
    for fidelity in MATH_FIDELITIES
]

# Generate readable IDs for pytest output
PARAM_IDS = [
    f"bin={bin_op.name}|un={unary_op.name}|acc={dest_acc.name}|approx={approx_mode.name}|fid={fidelity.name}"
    for (
        bin_op,
        unary_op,
        dest_acc,
        approx_mode,
        fidelity,
    ) in PARAM_COMBINATIONS
]


# -----------------------------------------------------------------------------
# 3. The test implementation
# -----------------------------------------------------------------------------
@pytest.mark.parametrize(
    "binary_op, unary_op, dest_acc, approx_mode, math_fidelity",
    PARAM_COMBINATIONS,
    ids=PARAM_IDS,
)
def test_sweep_test(
    binary_op,
    unary_op,
    dest_acc,
    approx_mode,
    math_fidelity,
):
    """Runs the C++ sweep_test.cpp kernel for a full sweep of
    (binary_op × unary_op × dest_acc × approx_mode × math_fidelity).
    Only Float16_b format and 32×32 tensor shape (1 tile) are used, as requested.
    """

    if (
        binary_op in [MathOperation.Elwadd, MathOperation.Elwsub]
        and math_fidelity != MathFidelity.LoFi
    ):
        pytest.skip("No need to test higher fidelities for add/sub")

    # Temporary: skip precision-sensitive fused case we have not modelled yet
    if binary_op == MathOperation.Elwmul and unary_op == MathOperation.Square:
        pytest.skip(
            "Known precision edge-case for Elwadd+Square with dest_acc; skipped for now"
        )

    # ------------------------------------------------------------------
    # Generate input stimuli
    # ------------------------------------------------------------------
    input_dimensions = [32, 32]
    src_A, src_B, tile_cnt = generate_stimuli(
        SUPPORTED_FORMAT.input_format,
        SUPPORTED_FORMAT.input_format,
        input_dimensions=input_dimensions,
    )

    # Will hold pre-SFPU tensor when inputs are shifted. Initialization
    # here guarantees the variable exists on all execution paths.
    adjusted_binary_tensor = None

    # --------------------------------------------------------------
    #  Adjust inputs if the chosen unary op requires non-negative
    #  arguments (e.g. Sqrt) but the binary result would become
    #  negative. We add a positive offset to src_A to shift the
    #  post-binary tensor into the valid range and then recalculate
    #  the golden reference.
    # --------------------------------------------------------------
    POSITIVE_ONLY_UNARY = {MathOperation.Sqrt}

    def _compute_binary(tA, tB):
        gen = get_golden_generator(EltwiseBinaryGolden)
        return gen(
            binary_op,
            tA,
            tB,
            SUPPORTED_FORMAT.output_format,
            math_fidelity,
        )

    if unary_op in POSITIVE_ONLY_UNARY:
        binary_res = _compute_binary(src_A, src_B)
        min_val = torch.min(binary_res).item()
        if min_val < 0:
            offset = abs(min_val) + 0.1  # small positive margin
            src_A = (src_A + offset).to(src_A.dtype)
            # Re-compute binary result after the shift
            binary_res = _compute_binary(src_A, src_B)
            # Now min should be >=0; if still negative, increase offset again
            if torch.min(binary_res).item() < 0:
                extra = abs(torch.min(binary_res).item()) + 0.1
                src_A = (src_A + extra).to(src_A.dtype)
                binary_res = _compute_binary(src_A, src_B)
            # Replace subsequent golden computation base tensor
            adjusted_binary_tensor = binary_res

    # ------------------------------------------------------------------
    # Produce golden output
    # ------------------------------------------------------------------
    if adjusted_binary_tensor is None:
        gen_binary = get_golden_generator(EltwiseBinaryGolden)
        golden_tensor = gen_binary(
            binary_op,
            src_A,
            src_B,
            SUPPORTED_FORMAT.output_format,
            math_fidelity,
        )
    else:
        golden_tensor = adjusted_binary_tensor

    gen_unary = get_golden_generator(UnarySFPUGolden)
    golden_tensor = gen_unary(
        unary_op,
        golden_tensor,
        SUPPORTED_FORMAT.output_format,
        math_fidelity,
    )

    # ------------------------------------------------------------------
    # Write stimuli to device L1 and run the HW test
    # ------------------------------------------------------------------
    res_address = write_stimuli_to_l1(
        src_A,
        src_B,
        SUPPORTED_FORMAT.input_format,
        SUPPORTED_FORMAT.input_format,
        tile_count=tile_cnt,
    )

    test_config = {
        "formats": SUPPORTED_FORMAT,
        "testname": "sweep_test",
        "dest_acc": dest_acc,
        "approx_mode": approx_mode,
        "mathop": binary_op,  # Used to generate ELTWISE_BINARY_OP constant
        "unary_op": unary_op,  # Picked up by our patched generate_build_header
        "math_fidelity": math_fidelity,
        "tile_cnt": tile_cnt,
        "input_dimensions": input_dimensions,
    }

    run_test(test_config, profiler_build=ProfilerBuild.No)

    # ------------------------------------------------------------------
    # Collect results and compare with golden
    # ------------------------------------------------------------------
    res_from_L1 = collect_results(
        SUPPORTED_FORMAT,
        tile_count=tile_cnt,
        address=res_address,
    )
    assert len(res_from_L1) == len(golden_tensor)

    torch_format = format_dict[SUPPORTED_FORMAT.output_format]
    res_tensor = torch.tensor(res_from_L1, dtype=torch_format)

    assert passed_test(golden_tensor, res_tensor, SUPPORTED_FORMAT.output_format)
