# SPDX-FileCopyrightText: © 2026 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0


from itertools import chain, product

import pytest
import torch
from conftest import skip_for_coverage
from helpers.chip_architecture import ChipArchitecture
from helpers.format_config import DataFormat, InputOutputFormat
from helpers.golden_generators import (
    TILE_DIMENSIONS,
    UnarySFPUGolden,
    get_golden_generator,
)
from helpers.llk_params import (
    ApproximationMode,
    BlocksCalculationAlgorithm,
    DestAccumulation,
    FastMode,
    MathOperation,
    format_dict,
)
from helpers.param_config import (
    get_num_blocks_and_num_tiles_in_block,
    input_output_formats,
    parametrize,
)
from helpers.stimuli_config import StimuliConfig
from helpers.stimuli_generator import generate_stimuli
from helpers.test_config import TestConfig
from helpers.test_variant_parameters import (
    APPROX_MODE,
    CLAMP_NEGATIVE,
    FAST_MODE,
    MATH_OP,
    NUM_BLOCKS,
    NUM_TILES_IN_BLOCK,
    TILE_COUNT,
    DestSync,
)
from helpers.utils import passed_test

SUPPORTED_FAST_MODE_OPS = [
    # MathOperation.Log1p,
    # MathOperation.Exp,
    # MathOperation.Rsqrt,
    # MathOperation.Sqrt,
]

ALL_MATHOPS = [
    MathOperation.Abs,
    MathOperation.Atanh,
    MathOperation.Asinh,
    MathOperation.Acosh,
    MathOperation.Cos,
    MathOperation.Log,
    MathOperation.Reciprocal,
    MathOperation.Sin,
    MathOperation.Square,
    MathOperation.Tanh,
    MathOperation.Celu,
    MathOperation.Silu,
    MathOperation.Gelu,
    MathOperation.Neg,
    MathOperation.Fill,
    MathOperation.Elu,
    MathOperation.Exp,
    # MathOperation.Log1p,
    # MathOperation.Rsqrt,
    # MathOperation.Hardsigmoid,
    MathOperation.Exp2,
    MathOperation.Threshold,
    MathOperation.ReluMax,
    MathOperation.ReluMin,
    # MathOperation.Sqrt,
]

FORMATS = input_output_formats(
    [
        DataFormat.Float32,
        # DataFormat.Float16,
        DataFormat.Float16_b,
        DataFormat.Bfp8_b,
        DataFormat.Bfp4_b,
    ]
)

FLOAT_TEST_PARAMS = list(
    chain(
        (
            (fmt, approx, mathop, fast, dest)
            for fmt, approx, mathop, fast, dest in product(
                FORMATS,
                [ApproximationMode.No, ApproximationMode.Yes],
                SUPPORTED_FAST_MODE_OPS,
                [FastMode.No, FastMode.Yes],
                [DestAccumulation.No, DestAccumulation.Yes],
            )
        ),
        (
            (fmt, approx, mathop, FastMode.No, dest)
            for fmt, approx, mathop, dest in product(
                FORMATS,
                [ApproximationMode.No, ApproximationMode.Yes],
                [op for op in ALL_MATHOPS if op not in SUPPORTED_FAST_MODE_OPS],
                [DestAccumulation.No, DestAccumulation.Yes],
            )
        ),
    )
)


# Skipped because of: https://github.com/tenstorrent/tt-llk/issues/1435
@skip_for_coverage
@pytest.mark.nightly
@pytest.mark.parametrize(
    "formats,approx_mode,mathop,fast_mode,dest_acc",
    FLOAT_TEST_PARAMS,
)
@pytest.mark.parametrize(
    "input_dimensions",
    # [[64, 64], [128, 256]],
    [[32, 32]],
)
def test_eltwise_unary_sfpu_float(
    formats: list[InputOutputFormat],
    approx_mode: ApproximationMode,
    mathop: MathOperation,
    fast_mode: FastMode,
    dest_acc: DestAccumulation,
    input_dimensions: list[int],
    workers_tensix_coordinates: str,
):
    if TestConfig.WITH_COVERAGE and mathop in [
        MathOperation.Acosh,
        MathOperation.Log,
        MathOperation.Log1p,
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
        MathOperation.Tanh,
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
            formats.input_format in (DataFormat.Bfp8_b, DataFormat.Bfp4_b)
            or formats.output_format in (DataFormat.Bfp8_b, DataFormat.Bfp4_b)
        )
    ):
        pytest.skip(
            reason="Exp-related operations are not supported for BFP formats in approximation mode."
        )

    if (
        formats.input_format == DataFormat.Float16
        and formats.output_format == DataFormat.Bfp4_b
        and dest_acc == DestAccumulation.No
    ):
        pytest.skip(reason="Float16 to Bfp4_b with dest_acc=No is not supported")

    if formats.output_format == DataFormat.Bfp4_b and mathop in [
        MathOperation.Sin,
    ]:
        pytest.skip(reason="Bfp4_b failing for these mathops")

    eltwise_unary_sfpu(
        "sources/eltwise_unary_sfpu_test.cpp",
        formats,
        dest_acc,
        approx_mode,
        mathop,
        fast_mode,
        input_dimensions,
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
    input_dimensions=[[128, 256]],
)
def test_eltwise_unary_sfpu_int(
    formats: list[InputOutputFormat],
    approx_mode: ApproximationMode,
    mathop: MathOperation,
    fast_mode: FastMode,
    dest_acc: DestAccumulation,
    input_dimensions: list[int],
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
        input_dimensions,
        workers_tensix_coordinates,
    )


def eltwise_unary_sfpu(
    test_name,
    formats: list[InputOutputFormat],
    dest_acc,
    approx_mode,
    mathop,
    fast_mode: FastMode,
    input_dimensions: list[int],
    workers_tensix_coordinates,
):
    torch.manual_seed(0)
    torch.set_printoptions(precision=10)

    src_A, tile_cnt_A, src_B, tile_cnt_B = generate_stimuli(
        stimuli_format_A=formats.input_format,
        input_dimensions_A=input_dimensions,
        stimuli_format_B=formats.input_format,
        input_dimensions_B=input_dimensions,
    )

    generate_golden = get_golden_generator(UnarySFPUGolden)
    golden_tensor = generate_golden(
        mathop,
        src_A,
        formats.output_format,
        dest_acc,
        formats.input_format,
        input_dimensions,
    )

    num_blocks, num_tiles_in_block = get_num_blocks_and_num_tiles_in_block(
        DestSync.Half,
        dest_acc,
        formats,
        input_dimensions,
        TILE_DIMENSIONS,
        BlocksCalculationAlgorithm.Standard,
    )

    configuration = TestConfig(
        test_name,
        formats,
        templates=[
            APPROX_MODE(approx_mode),
            FAST_MODE(fast_mode),
            CLAMP_NEGATIVE(True),
            MATH_OP(mathop=mathop),
        ],
        runtimes=[
            TILE_COUNT(tile_cnt_A),
            NUM_BLOCKS(num_blocks),
            NUM_TILES_IN_BLOCK(num_tiles_in_block),
        ],
        variant_stimuli=StimuliConfig(
            src_A,
            formats.input_format,
            src_B,
            formats.input_format,
            formats.output_format,
            tile_count_A=tile_cnt_A,
            tile_count_B=tile_cnt_B,
            tile_count_res=tile_cnt_A,
        ),
        dest_acc=dest_acc,
        # If dest_acc is off, we unpack Float32 into 16-bit format in src registers (later copied over in dest reg for SFPU op)
        unpack_to_dest=(
            formats.input_format.is_32_bit() and dest_acc == DestAccumulation.Yes
        ),
    )

    res_from_L1 = configuration.run(workers_tensix_coordinates).result

    # res_from_L1 = res_from_L1[:1024]
    # golden_tensor = golden_tensor[:1024]
    assert len(res_from_L1) == len(
        golden_tensor
    ), "Result tensor and golden tensor are not of the same length"

    torch_format = format_dict[formats.output_format]
    res_tensor = torch.tensor(res_from_L1, dtype=torch_format)

    test_passed = passed_test(golden_tensor, res_tensor, formats.output_format)

    if not test_passed:
        rows, cols = input_dimensions
        torch.set_printoptions(precision=4, linewidth=10000)
        print(f"\n=== Mathop: {mathop} ===")
        print(f"\n=== Dest Acc: {dest_acc} ===")
        print(f"\n=== Approval Mode: {approx_mode} ===")
        print(f"\n=== Fast Mode: {fast_mode} ===")
        print(f"\n=== Input Format: {formats.input_format} ===")
        print(f"\n=== Output Format: {formats.output_format} ===")
        print(f"\n=== src_A ({rows}x{cols}) ===")
        print(src_A.view(rows, cols))
        print(f"\n=== Golden before quantization ({rows}x{cols}) ===")
        _gen = get_golden_generator(UnarySFPUGolden)
        from helpers.format_config import DataFormat as _DF

        _raw_golden = _gen(
            mathop,
            src_A,
            _DF.Float32,
            dest_acc,
            formats.input_format,
            input_dimensions,
        )
        print(_raw_golden.view(rows, cols))
        print(f"\n=== Golden ({rows}x{cols}) ===")
        print(golden_tensor.view(rows, cols))
        print(f"\n=== Result ({rows}x{cols}) ===")
        print(res_tensor.view(rows, cols))
        print(f"\n=== Diff (Result - Golden, zeros are matches) ===")
        diff = (
            res_tensor.view(rows, cols).float() - golden_tensor.view(rows, cols).float()
        )
        print(diff)
        mismatch_mask = diff != 0
        mismatch_count = mismatch_mask.sum().item()
        total = rows * cols
        print(
            f"\n=== Mismatches: {mismatch_count}/{total} ({100*mismatch_count/total:.1f}%) ==="
        )
        print(f"=== Mismatch positions (row, col): ===")
        mismatch_rows, mismatch_cols = torch.where(mismatch_mask)
        for r, c in zip(mismatch_rows.tolist(), mismatch_cols.tolist()):
            print(
                f"  [{r:2d},{c:2d}]  input={src_A.view(rows,cols)[r,c].item():.4f}  golden={golden_tensor.view(rows,cols)[r,c].item():.4f}  result={res_tensor.view(rows,cols)[r,c].item():.4f}  diff={diff[r,c].item():.4f}"
            )

        if formats.output_format == formats.output_format.Bfp4_b:
            from helpers.pack import float_to_bfp4_block
            from helpers.tilize_untilize import tilize_block

            tile_dim = (rows, cols)
            g_til = tilize_block(
                golden_tensor.float().flatten(), tile_dim, formats.input_format
            ).flatten()
            r_til = tilize_block(
                res_tensor.float().flatten(), tile_dim, formats.input_format
            ).flatten()
            n = g_til.numel()
            print(f"\n=== BFP4 encoding (tilized blocks of 16, golden vs result) ===")
            print(
                f"{'blk':>4}  {'exp_g':>5}  {'exp_r':>5}  {'mantissas_golden (s|mag)':^48}  {'mantissas_result (s|mag)':^48}"
            )
            for blk in range(n // 16):
                s, e = blk * 16, (blk + 1) * 16
                exp_g, mant_g = float_to_bfp4_block(g_til[s:e])
                exp_r, mant_r = float_to_bfp4_block(r_til[s:e])

                def fmt_mants(mants):
                    return " ".join(f"{m >> 3}|{m & 7:03b}" for m in mants)

                match = " " if exp_g == exp_r and mant_g == mant_r else "*"
                print(
                    f"{blk:>4}{match} {exp_g:>5}  {exp_r:>5}  {fmt_mants(mant_g)}  {fmt_mants(mant_r)}"
                )

    assert test_passed, "Assert against golden failed"


# Test exponential with APPROX_MODE=true, FAST_MODE=true, and CLAMP_NEGATIVE=true/false
@pytest.mark.parametrize("clamp_negative", [True, False])
def test_exponential_clamp_negative(
    clamp_negative: bool,
    workers_tensix_coordinates: str,
):
    torch.manual_seed(0)
    input_dimensions = [32, 32]
    formats = InputOutputFormat(DataFormat.Float16_b, DataFormat.Float16_b)
    dest_acc = DestAccumulation.No

    # Generate custom stimuli with range [-5, 0.7]
    num_elements = input_dimensions[0] * input_dimensions[1]
    src_A = torch.rand(num_elements, dtype=torch.bfloat16) * 5.7 - 5.0
    # Set some values to be large and negative:
    src_A[0] = -10000
    src_A[1] = -1000
    src_A[2] = -200
    src_A[3] = -100
    src_A[4] = -88.5

    src_B = torch.zeros(num_elements, dtype=torch.bfloat16)
    tile_cnt_A = (input_dimensions[0] // 32) * (input_dimensions[1] // 32)
    tile_cnt_B = tile_cnt_A

    generate_golden = get_golden_generator(UnarySFPUGolden)
    golden_tensor = generate_golden(
        MathOperation.Exp,
        src_A,
        formats.output_format,
        dest_acc,
        formats.input_format,
        input_dimensions,
    )

    num_blocks, num_tiles_in_block = get_num_blocks_and_num_tiles_in_block(
        DestSync.Half,
        dest_acc,
        formats,
        input_dimensions,
        TILE_DIMENSIONS,
        BlocksCalculationAlgorithm.Standard,
    )

    configuration = TestConfig(
        "sources/eltwise_unary_sfpu_test.cpp",
        formats,
        templates=[
            APPROX_MODE(ApproximationMode.Yes),
            FAST_MODE(FastMode.Yes),
            CLAMP_NEGATIVE(clamp_negative),
            MATH_OP(mathop=MathOperation.Exp),
        ],
        runtimes=[
            TILE_COUNT(tile_cnt_A),
            NUM_BLOCKS(num_blocks),
            NUM_TILES_IN_BLOCK(num_tiles_in_block),
        ],
        variant_stimuli=StimuliConfig(
            src_A,
            formats.input_format,
            src_B,
            formats.input_format,
            formats.output_format,
            tile_count_A=tile_cnt_A,
            tile_count_B=tile_cnt_B,
            tile_count_res=tile_cnt_A,
        ),
        dest_acc=dest_acc,
        unpack_to_dest=False,
    )

    res_from_L1 = configuration.run(workers_tensix_coordinates).result

    assert len(res_from_L1) == len(
        golden_tensor
    ), "Result tensor and golden tensor are not of the same length"

    torch_format = format_dict[formats.output_format]
    res_tensor = torch.tensor(res_from_L1, dtype=torch_format)

    # When clamp_negative = False require inputs < -88 to be negative (but not necessarily correct),
    # and don't include them in the resulting isclose check.
    if not clamp_negative:
        assert torch.all(
            res_tensor[:5] <= 0
        ), "Some of the first 5 elements are positive"
        res_tensor[:5] = golden_tensor[:5]

    # Use relaxed tolerance for this test
    atol, rtol = 0.02, 0.02
    is_close = torch.isclose(golden_tensor, res_tensor, rtol=rtol, atol=atol)
    is_nan = torch.isnan(golden_tensor) & torch.isnan(res_tensor)
    is_valid = is_close | is_nan

    assert torch.all(
        is_valid
    ), f"Test failed: {(~is_valid).sum()} elements outside tolerance (atol={atol}, rtol={rtol})"
