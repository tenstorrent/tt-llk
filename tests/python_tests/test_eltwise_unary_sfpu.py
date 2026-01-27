# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0


import pytest
import torch
from helpers.chip_architecture import ChipArchitecture
from helpers.format_config import DataFormat, InputOutputFormat
from helpers.golden_generators import UnarySFPUGolden, get_golden_generator
from helpers.llk_params import (
    ApproximationMode,
    DestAccumulation,
    FastMode,
    MathOperation,
    format_dict,
)
from helpers.param_config import input_output_formats, parametrize
from helpers.stimuli_config import StimuliConfig
from helpers.test_config import TestConfig
from helpers.test_variant_parameters import (
    APPROX_MODE,
    FAST_MODE,
    INPUT_DIMENSIONS,
    MATH_OP,
    TILE_COUNT,
)
from helpers.utils import passed_test

SUPPORTED_FAST_MODE_OPS = [
    MathOperation.Log1p,
    MathOperation.Exp,
    MathOperation.Rsqrt,
    MathOperation.Sqrt,
]

ALL_MATHOPS = [
    # MathOperation.Abs,
    # MathOperation.Atanh,
    # MathOperation.Asinh,
    # MathOperation.Acosh,
    # MathOperation.Cos,
    # MathOperation.Log,
    # MathOperation.Log1p,
    # MathOperation.Reciprocal,
    # MathOperation.Sin,
    # MathOperation.Sqrt,
    # MathOperation.Rsqrt,
    # MathOperation.Square,
    # MathOperation.Tanh,
    # MathOperation.Celu,
    # MathOperation.Silu,
    # MathOperation.Gelu,
    # MathOperation.Neg,
    # MathOperation.Fill,
    # MathOperation.Elu,
    # MathOperation.Exp,
    # MathOperation.Exp2,
    # MathOperation.Hardsigmoid,
    # MathOperation.Threshold,
    # MathOperation.ReluMax,
    # MathOperation.ReluMin,
    MathOperation.LoadConstFirstRow,
]

FORMATS = input_output_formats(
    [
        # DataFormat.Float32,
        # DataFormat.Float16,
        DataFormat.Float16_b,
        # DataFormat.Bfp8_b,
    ]
)

# Isolated test case: 32x32, Float16_b, no dest acc, no approx mode, LoadConstFirstRow
FLOAT_TEST_PARAMS = [
    (
        FORMATS[0],
        ApproximationMode.No,
        MathOperation.LoadConstFirstRow,
        FastMode.No,
        DestAccumulation.No,
    ),
]


@pytest.mark.nightly
@pytest.mark.parametrize(
    "formats,approx_mode,mathop,fast_mode,dest_acc",
    FLOAT_TEST_PARAMS,
)
def test_eltwise_unary_sfpu_float(
    formats: list[InputOutputFormat],
    approx_mode: ApproximationMode,
    mathop: MathOperation,
    fast_mode: FastMode,
    dest_acc: DestAccumulation,
    workers_tensix_coordinates: str,
):
    if TestConfig.WITH_COVERAGE and mathop in [
        MathOperation.Acosh,
        MathOperation.Log,
        MathOperation.Reciprocal,
        MathOperation.Sin,
        MathOperation.Sqrt,
        MathOperation.Rsqrt,
        MathOperation.Square,
        MathOperation.Celu,
        MathOperation.Silu,
        MathOperation.Neg,
        MathOperation.Exp2,
        MathOperation.Hardsigmoid,
        MathOperation.Threshold,
        MathOperation.ReluMax,
        MathOperation.ReluMin,
    ]:
        # SFPI Issue link: https://github.com/tenstorrent/tt-metal/issues/33268
        pytest.skip(
            reason="When these SPFU ops get compiled with coverage, `#pragma GCC unroll X` marked loops get compiled to invalid assembly"
        )

    if mathop == MathOperation.ReluMin:
        pytest.skip(reason="https://github.com/tenstorrent/tt-llk/issues/1120")

    if mathop == MathOperation.Tanh and approx_mode == ApproximationMode.Yes:
        pytest.skip(reason="Metal tanh does not support approximation mode")

    if TestConfig.WITH_COVERAGE and mathop == MathOperation.Gelu:
        # Issue link: https://github.com/tenstorrent/tt-llk/issues/883
        pytest.skip(
            reason="Compilation error when this mathop gets compiled with coverage"
        )

    if (
        dest_acc == DestAccumulation.No
        and TestConfig.CHIP_ARCH == ChipArchitecture.BLACKHOLE
    ):
        if formats.input_format == DataFormat.Float16 or formats == InputOutputFormat(
            DataFormat.Float32, DataFormat.Float16
        ):
            pytest.skip(reason="This combination is not supported on BH architecture")

    if (
        approx_mode == ApproximationMode.Yes
        and mathop in [MathOperation.Exp, MathOperation.Exp2, MathOperation.Elu]
        and (
            formats.input_format == DataFormat.Bfp8_b
            or formats.output_format == DataFormat.Bfp8_b
        )
    ):
        pytest.skip(
            reason="Exp-related operations are not supported for bf8_b format in approximation mode."
        )

    eltwise_unary_sfpu(
        "sources/eltwise_unary_sfpu_test.cpp",
        formats,
        dest_acc,
        approx_mode,
        mathop,
        fast_mode,
        workers_tensix_coordinates,
    )


@parametrize(
    formats=input_output_formats([DataFormat.Int32]),
    approx_mode=[ApproximationMode.No, ApproximationMode.Yes],
    mathop=[
        MathOperation.Neg,
        MathOperation.Fill,
    ],
    fast_mode=[FastMode.No, FastMode.Yes],
    dest_acc=[DestAccumulation.Yes],
)
def test_eltwise_unary_sfpu_int(
    formats: list[InputOutputFormat],
    approx_mode: ApproximationMode,
    mathop: MathOperation,
    fast_mode: FastMode,
    dest_acc: DestAccumulation,
    workers_tensix_coordinates: str,
):
    if formats.input_format == DataFormat.Int32:
        pytest.skip(reason=f"Int32 tests break fast tilize, tracked in #495")

    eltwise_unary_sfpu(
        "sources/eltwise_unary_sfpu_int.cpp",
        formats,
        dest_acc,
        approx_mode,
        mathop,
        fast_mode,
        workers_tensix_coordinates,
    )


def eltwise_unary_sfpu(
    test_name,
    formats: list[InputOutputFormat],
    dest_acc,
    approx_mode,
    mathop,
    fast_mode: FastMode,
    workers_tensix_coordinates,
):
    torch.manual_seed(0)
    torch.set_printoptions(precision=10)
    input_dimensions = [32, 32]

    # src_A, tile_cnt_A, src_B, tile_cnt_B = generate_stimuli(
    #     stimuli_format_A=formats.input_format,
    #     input_dimensions_A=input_dimensions,
    #     stimuli_format_B=formats.input_format,
    #     input_dimensions_B=input_dimensions,
    # )

    torch_dtype = format_dict[formats.input_format]
    src_A = torch.ones(input_dimensions[0], input_dimensions[1], dtype=torch_dtype)
    src_B = torch.ones(input_dimensions[0], input_dimensions[1], dtype=torch_dtype)
    tile_cnt_A = 1
    tile_cnt_B = 1

    generate_golden = get_golden_generator(UnarySFPUGolden)
    golden_tensor = generate_golden(
        mathop,
        src_A,
        formats.output_format,
        dest_acc,
        formats.input_format,
        input_dimensions,
    )

    configuration = TestConfig(
        test_name,
        formats,
        templates=[
            INPUT_DIMENSIONS(input_dimensions, input_dimensions),
            APPROX_MODE(approx_mode),
            FAST_MODE(fast_mode),
            MATH_OP(mathop=mathop),
        ],
        runtimes=[TILE_COUNT(tile_cnt_A)],
        variant_stimuli=StimuliConfig(
            src_A,
            formats.input_format,
            src_B,
            formats.input_format,
            formats.output_format,
            tile_count_A=tile_cnt_A,
            tile_count_B=tile_cnt_B,
            tile_count_res=2,  # Read 2 tiles: dest index 0 and dest index 1
        ),
        dest_acc=dest_acc,
        # If dest_acc is off, we unpack Float32 into 16-bit format in src registers (later copied over in dest reg for SFPU op)
        unpack_to_dest=(
            formats.input_format.is_32_bit() and dest_acc == DestAccumulation.Yes
        ),
    )

    res_from_L1 = configuration.run(workers_tensix_coordinates)

    # Split result into two tiles (1024 elements each for 32x32)
    tile_size = 32 * 32
    torch_format = format_dict[formats.output_format]

    # First tile (dest index 0)
    res_tensor = torch.tensor(res_from_L1[:tile_size], dtype=torch_format)

    # Second tile (dest index 1) - print as 32x32
    dest_index_1_data = res_from_L1[tile_size : tile_size * 2]
    dest_index_1_tensor = torch.tensor(dest_index_1_data, dtype=torch_format).reshape(
        32, 32
    )
    print("\n========== DEST INDEX 1 (32x32 tile) ==========")
    print(dest_index_1_tensor)
    print("================================================\n")

    assert len(res_tensor) == len(
        golden_tensor
    ), "Result tensor and golden tensor are not of the same length"

    assert passed_test(
        golden_tensor, res_tensor, formats.output_format
    ), "Assert against golden failed"
