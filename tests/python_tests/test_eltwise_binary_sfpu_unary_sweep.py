# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0

import pytest
import torch

from helpers.chip_architecture import ChipArchitecture, get_chip_architecture
from helpers.device import (
    collect_results,
    write_stimuli_to_l1,
)
from helpers.domain_safe_stimuli import (
    generate_domain_safe_stimuli,
    is_combination_domain_safe,
    should_use_domain_safe_stimuli,
)
from helpers.format_arg_mapping import (
    ApproximationMode,
    DestAccumulation,
    MathFidelity,
    MathOperation,
    format_dict,
)
from helpers.format_config import DataFormat
from helpers.golden_generators import (
    EltwiseBinaryGolden,
    UnarySFPUGolden,
    get_golden_generator,
)
from helpers.param_config import (
    clean_params,
    generate_params,
    input_output_formats,
)
from helpers.stimuli_generator import generate_stimuli
from helpers.test_config import run_test
from helpers.utils import passed_test

# SUPPORTED FORMATS FOR TEST
supported_formats = [
    DataFormat.Float16,
    DataFormat.Float16_b,
    DataFormat.Float32,
    DataFormat.Bfp8_b,
]

# Define all elementwise binary operations to sweep
elementwise_binary_ops = [
    MathOperation.Elwadd,  # UNCOMMENTED - should work fine
    MathOperation.Elwmul,  # UNCOMMENTED - should work fine
    MathOperation.Elwsub,
]

# Define all SFPU unary operations to sweep
sfpu_unary_ops = [
    MathOperation.Abs,
    MathOperation.Cos,
    MathOperation.Sin,
    MathOperation.Log,
    MathOperation.Reciprocal,
    MathOperation.Sqrt,
    MathOperation.Square,
    MathOperation.Gelu,
    MathOperation.Celu,
    MathOperation.Silu,
    MathOperation.Neg,
]

test_formats = input_output_formats(supported_formats, same=False)

# Generate parameter combinations for each operation pair
test_params = []
for eltwise_op in elementwise_binary_ops:
    for sfpu_op in sfpu_unary_ops:
        # Generate base parameters for this operation combination
        base_params = generate_params(
            ["eltwise_binary_sfpu_unary_sweep_test"],
            test_formats,
            dest_acc=[DestAccumulation.No, DestAccumulation.Yes],
            mathop=[eltwise_op],
            math_fidelity=[
                MathFidelity.LoFi,
                MathFidelity.HiFi2,
                MathFidelity.HiFi3,
                MathFidelity.HiFi4,
            ],
            approx_mode=[ApproximationMode.No, ApproximationMode.Yes],
        )

        # Clean the base parameters and add SFPU operation info
        cleaned_base = clean_params(base_params)
        for param in cleaned_base:
            # Add SFPU operation as the last element
            extended_param = param + (sfpu_op,)
            test_params.append(extended_param)


# Generate custom parameter IDs for our extended parameters
def generate_custom_param_ids(params):
    """Generate parameter IDs for our custom parameter structure."""
    ids = []
    for param in params:
        (
            testname,
            format_config,
            dest_acc,
            approx_mode,
            mathop,
            math_fidelity,
            sfpu_op,
        ) = param

        # Create a descriptive ID using InputOutputFormat structure
        format_str = (
            f"{format_config.input_format.name}->{format_config.output_format.name}"
        )
        id_parts = [
            f"eltwise={mathop.name}",
            f"sfpu={sfpu_op.name}",
            f"fmt={format_str}",
            f"acc={dest_acc.name}",
            f"fid={math_fidelity.name}",
            f"approx={approx_mode.name}",
        ]
        ids.append(" | ".join(id_parts))
    return ids


param_ids = generate_custom_param_ids(test_params)


@pytest.mark.parametrize(
    "testname,format_config,dest_acc,approx_mode,mathop,math_fidelity,sfpu_op",
    test_params,
    ids=param_ids,
)
def test_eltwise_binary_sfpu_unary_sweep(
    testname,
    format_config,
    dest_acc,
    mathop,
    math_fidelity,
    approx_mode,
    sfpu_op,
):
    """
    Comprehensive test that sweeps through all combinations of:
    1. llk_unpack_AB (unpacking two tiles)
    2. Elementwise binary operations (ELWADD, ELWMUL, ELWSUB)
    3. SFPU unary operations (all supported unary ops)
    4. Regular pack operation

    This test validates the fusion of elementwise binary and SFPU unary operations
    across different data formats, math fidelities, and destination accumulation modes.

    Uses a single-pass approach: unpack AB -> elementwise binary -> SFPU unary -> pack.
    """

    input_dimensions = [32, 32]

    if (
        mathop in [MathOperation.Elwadd, MathOperation.Elwsub]
        and math_fidelity != MathFidelity.LoFi
    ):
        pytest.skip("Fidelity doesn't affect elwadd and elwsub")

    # Skip problematic combinations following the working test pattern
    if sfpu_op in [MathOperation.Cos, MathOperation.Sin]:
        pytest.skip("Cos and Sin operations are not fully functional yet")
    if sfpu_op == MathOperation.Square and math_fidelity == MathFidelity.LoFi:
        pytest.skip("Square operation in LoFi is not fully functional yet")

    # Skip mathematically invalid domain combinations for fused operations
    # Some combinations can be made safe with domain-constrained stimuli
    if mathop == MathOperation.Elwsub and sfpu_op in [
        MathOperation.Log,  # Still problematic even with constraints
        MathOperation.Reciprocal,  # Still problematic even with constraints
        # MathOperation.Sqrt,       # CAN BE ENABLED with domain-safe stimuli
        # MathOperation.Celu,       # CAN BE ENABLED with domain-safe stimuli
    ]:
        # Check if this combination can be made safe with constrained stimuli
        if not is_combination_domain_safe(mathop, sfpu_op):
            pytest.skip(
                "Subtraction can produce negative/near-zero values incompatible with Log/Reciprocal operations"
            )

    # Multiplication can produce very small values that break Reciprocal or very large values that break Log/Square
    # Some combinations can be made safe with domain-constrained stimuli
    if mathop == MathOperation.Elwmul and sfpu_op in [
        MathOperation.Reciprocal,  # CAN BE ENABLED with domain-safe stimuli
        MathOperation.Log,  # CAN BE ENABLED with domain-safe stimuli
        MathOperation.Square,  # CAN BE ENABLED with domain-safe stimuli
    ]:
        # Check if this combination can be made safe with constrained stimuli
        if not is_combination_domain_safe(mathop, sfpu_op):
            pytest.skip(
                "Multiplication can produce extreme values that cause precision/domain issues with Reciprocal"
            )

    # BFP8 format limitations (following working test pattern)
    if (
        format_config.input_format == format_config.output_format == DataFormat.Bfp8_b
        and sfpu_op in [MathOperation.Log, MathOperation.Reciprocal]
    ):
        pytest.skip(
            "BFP8 format does not support Log and Reciprocal operations reliably"
        )

    # Square operation can cause overflow with certain elementwise results
    if sfpu_op == MathOperation.Square and format_config.output_format in [
        DataFormat.Float16,
        DataFormat.Float16_b,
        DataFormat.Bfp8_b,
    ]:
        pytest.skip(
            "Square operation may overflow with elementwise results in limited precision formats"
        )

    # Complex activation functions are sensitive to input ranges from elementwise operations
    if (
        mathop in [MathOperation.Elwsub, MathOperation.Elwmul]
        and sfpu_op in [MathOperation.Celu, MathOperation.Gelu, MathOperation.Silu]
        and format_config.output_format in [DataFormat.Float16, DataFormat.Float16_b]
    ):
        pytest.skip(
            "Complex activation functions may be unstable with elementwise operation results in Float16"
        )

    # Additional BFP8 restrictions for complex operations
    if format_config.output_format == DataFormat.Bfp8_b and sfpu_op in [
        MathOperation.Square,
        MathOperation.Celu,
    ]:
        pytest.skip("BFP8 format may not handle complex SFPU operations reliably")

    # Architecture-specific restrictions (following working test pattern)
    if (
        format_config.input_format == format_config.output_format == DataFormat.Float16
        and sfpu_op in [MathOperation.Log, MathOperation.Sqrt, MathOperation.Square]
        and dest_acc == DestAccumulation.No
        and get_chip_architecture() == ChipArchitecture.BLACKHOLE
    ):
        pytest.skip(
            "Float16 format with specific SFPU ops not supported on Blackhole architecture"
        )

    torch_format = format_dict.get(format_config.output_format)

    # Generate stimuli for two input tensors
    # Use domain-safe stimuli for problematic operation combinations
    if should_use_domain_safe_stimuli(mathop, sfpu_op, format_config.input_format):
        src_A, src_B, tile_cnt = generate_domain_safe_stimuli(
            mathop, sfpu_op, format_config.input_format, input_dimensions
        )
    else:
        src_A, src_B, tile_cnt = generate_stimuli(
            format_config.input_format,
            format_config.input_format,
            input_dimensions=input_dimensions,
        )

    # Generate golden result following the working pattern:
    # 1. First apply elementwise binary operation
    generate_eltwise_golden = get_golden_generator(EltwiseBinaryGolden)
    golden_tensor = generate_eltwise_golden(
        mathop, src_A, src_B, format_config.output_format, math_fidelity
    )

    # 2. Then apply SFPU unary operation to the result
    generate_sfpu_golden = get_golden_generator(UnarySFPUGolden)
    golden_tensor = generate_sfpu_golden(
        sfpu_op, golden_tensor, format_config.output_format
    )
    golden_tensor = golden_tensor.to(torch_format)

    # Write stimuli to L1 memory (untilized for elementwise operations)
    res_address = write_stimuli_to_l1(
        src_A,
        src_B,
        format_config.input_format,
        format_config.input_format,
        tile_count=tile_cnt,
    )

    # Configure test parameters for single-pass fused operation
    test_config = {
        "testname": testname,
        "formats": format_config,
        "dest_acc": dest_acc,
        "mathop": mathop,
        "sfpu_unary_op": sfpu_op,
        "math_fidelity": math_fidelity,
        "approx_mode": approx_mode,
        "tile_cnt": tile_cnt,
    }

    # Run the test
    run_test(test_config)

    # Collect results from device
    res_from_L1 = collect_results(
        format_config, tile_count=tile_cnt, address=res_address
    )
    res_tensor = torch.tensor(res_from_L1, dtype=torch_format)

    # Verify results match golden
    assert passed_test(
        golden_tensor,
        res_tensor,
        format_config.output_format,
    ), (
        f"Test failed for {testname} with "
        f"eltwise_op={mathop.name}, sfpu_op={sfpu_op.name}, "
        f"format={format_config.output_format.name}, "
        f"math_fidelity={math_fidelity.name}, "
        f"dest_acc={dest_acc.name}"
    )


# Additional test for specific operation combinations (for debugging)
@pytest.mark.parametrize(
    "eltwise_op,sfpu_op,input_format,output_format",
    [
        (
            MathOperation.Elwadd,
            MathOperation.Abs,
            DataFormat.Float16_b,
            DataFormat.Float16_b,
        ),
        (
            MathOperation.Elwmul,
            MathOperation.Sqrt,
            DataFormat.Float32,
            DataFormat.Float32,
        ),
    ],
)
def test_specific_fused_operations(eltwise_op, sfpu_op, input_format, output_format):
    """Test specific combinations of fused operations for targeted validation."""

    from helpers.format_config import InputOutputFormat

    # Create a simple format configuration
    format_config = InputOutputFormat(input_format, output_format)
    torch_format = format_dict.get(format_config.output_format)

    # Generate simple test data
    src_A, src_B, tile_cnt = generate_stimuli(
        format_config.input_format,
        format_config.input_format,
        input_dimensions=[32, 32],
    )

    # Generate golden result using the same two-phase approach
    generate_eltwise_golden = get_golden_generator(EltwiseBinaryGolden)
    golden_tensor = generate_eltwise_golden(
        eltwise_op, src_A, src_B, format_config.output_format, MathFidelity.HiFi4
    )

    generate_sfpu_golden = get_golden_generator(UnarySFPUGolden)
    golden_tensor = generate_sfpu_golden(
        sfpu_op, golden_tensor, format_config.output_format
    )
    golden_tensor = golden_tensor.to(torch_format)

    # Write stimuli to L1
    res_address = write_stimuli_to_l1(
        src_A,
        src_B,
        format_config.input_format,
        format_config.input_format,
        tile_count=tile_cnt,
    )

    # Configure and run test
    test_config = {
        "testname": "eltwise_binary_sfpu_unary_sweep_test",
        "formats": format_config,
        "dest_acc": DestAccumulation.No,
        "mathop": eltwise_op,
        "sfpu_unary_op": sfpu_op,
        "math_fidelity": MathFidelity.HiFi4,
        "approx_mode": ApproximationMode.No,
        "tile_cnt": tile_cnt,
    }

    run_test(test_config)

    # Collect and verify results
    res_from_L1 = collect_results(
        format_config, tile_count=tile_cnt, address=res_address
    )
    res_tensor = torch.tensor(res_from_L1, dtype=torch_format)

    assert passed_test(
        golden_tensor,
        res_tensor,
        format_config.output_format,
    ), (
        f"Specific test failed for eltwise_op={eltwise_op.name}, "
        f"sfpu_op={sfpu_op.name}, format={format_config.output_format.name}"
    )
