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
    MathOperation,
    PerfRunType,
    format_dict,
)
from helpers.param_config import input_output_formats, parametrize
from helpers.perf import PerfConfig
from helpers.profiler import Profiler
from helpers.stimuli_config import StimuliConfig
from helpers.stimuli_generator import generate_stimuli
from helpers.test_config import TestConfig
from helpers.test_variant_parameters import (
    APPROX_MODE,
    INPUT_DIMENSIONS,
    LOOP_FACTOR,
    MATH_OP,
    TILE_COUNT,
)


@parametrize(
    formats=input_output_formats(
        [
            DataFormat.Float16_b,
            # DataFormat.Float32,
        ]
    ),
    approx_mode=[ApproximationMode.Yes],
    mathop=[MathOperation.Exp],
    dest_acc=[DestAccumulation.No],  # , DestAccumulation.Yes],
)
def test_eltwise_unary_sfpu_float(
    perf_report,
    formats: list[InputOutputFormat],
    approx_mode: ApproximationMode,
    mathop: MathOperation,
    dest_acc: DestAccumulation,
    workers_tensix_coordinates: str,
):

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

    torch.manual_seed(0)
    input_dimensions = [32, 32]

    src_A, tile_cnt_A, src_B, tile_cnt_B = generate_stimuli(
        stimuli_format_A=formats.input_format,
        input_dimensions_A=input_dimensions,
        stimuli_format_B=formats.input_format,
        input_dimensions_B=input_dimensions,
    )

    min_val, max_val = -10, 0.7
    # min_val, max_val = 0, 1
    size = 128  # src_A.shape[0]
    for i in range(size):
        src_A[i] = min_val + (max_val - min_val) * i / (size - 1.0)
    src_A[0] = -100
    src_A[1] = -200
    src_A[2] = -300

    generate_golden = get_golden_generator(UnarySFPUGolden)
    golden_tensor = generate_golden(
        mathop,
        src_A,
        formats.output_format,
        dest_acc,
        formats.input_format,
        input_dimensions,
    )

    templates = [
        INPUT_DIMENSIONS(input_dimensions, input_dimensions),
        APPROX_MODE(approx_mode),
        MATH_OP(mathop=mathop),
    ]
    variant_stimuli = StimuliConfig(
        src_A,
        formats.input_format,
        src_B,
        formats.input_format,
        formats.output_format,
        tile_count_A=tile_cnt_A,
        tile_count_B=tile_cnt_B,
        tile_count_res=tile_cnt_A,
    )

    profile = False
    if profile:
        configuration = PerfConfig(
            "sources/fast_exp_test.cpp",
            formats,
            run_types=[
                PerfRunType.L1_TO_L1,
            ],
            templates=templates,
            runtimes=[TILE_COUNT(1), LOOP_FACTOR(2)],
            variant_stimuli=variant_stimuli,
            dest_acc=dest_acc,
            unpack_to_dest=(
                formats.input_format.is_32_bit() and dest_acc == DestAccumulation.Yes
            ),
        )
        configuration.run(perf_report, location=workers_tensix_coordinates)

        runtime = Profiler.get_data(
            configuration.test_name,
            configuration.variant_id,
            workers_tensix_coordinates,
        )
        timestamps = runtime.math().timestamps()  # .marker("TEST_TIMESTAMP").frame()
        zones = runtime.zones().marker("TILE_LOOP").frame()
        print(zones)
        return
    else:
        configuration = TestConfig(
            "sources/fast_exp_test.cpp",
            formats,
            templates=templates,
            runtimes=[TILE_COUNT(tile_cnt_A)],
            variant_stimuli=variant_stimuli,
            dest_acc=dest_acc,
            unpack_to_dest=(
                formats.input_format.is_32_bit() and dest_acc == DestAccumulation.Yes
            ),
        )
        res_from_L1 = configuration.run(workers_tensix_coordinates)

    assert len(res_from_L1) == len(
        golden_tensor
    ), "Result tensor and golder tensor are not of the same length"

    torch_format = format_dict[formats.output_format]
    res_tensor = torch.tensor(res_from_L1, dtype=torch_format)

    src_A_np = src_A.double().numpy()
    res_tensor_np = res_tensor.double().numpy()
    golden_tensor_np = golden_tensor.double().numpy()

    import numpy as np

    mad = 256.0 * 1.4426950408889634 * src_A_np + 32500.818359375
    # expected = mad
    # expected = np.clip(src_A_np, -88.5, 1e30)
    # expected = np.abs(src_A_np)
    # expected = np.abs(mad)
    # expected = 0.5*(mad + np.abs(mad))
    # expected = np.round(0.5*(mad + np.abs(mad))).astype(np.uint16).astype(np.int32)
    expected = (
        np.round(0.5 * (mad + np.abs(mad))).astype(np.uint16).astype(np.int32) << 15
    ).view(np.float32)
    # expected = np.exp(src_A_np)
    for i in range(size):
        print(
            i,
            "\t",
            src_A_np[i],
            "\t",
            # golden_tensor_np[i],
            res_tensor_np[i],
            "\t",
            # np.max(np.abs(res_tensor_np[i] - golden_tensor_np[i])),
            expected[i],
            "\t\t",
            # res_tensor_np[i] - expected[i],
        )

    N = 5
    print("")
    print("INPUT_SIZE:", size)
    print("INPUT: ", src_A_np[0:N])
    print("OUTPUT:", res_tensor_np[0:N])
    print("GOLDEN:", golden_tensor_np[0:N])
    print("")
    print("INPUT: ", src_A_np[size - N : size])
    print("OUTPUT:", res_tensor_np[size - N : size])
    print("GOLDEN:", golden_tensor_np[size - N : size])

    print(
        "MAXABSDIFF:",
        np.max(np.abs(res_tensor_np[0:256] - golden_tensor_np[0:256])),
    )
    print(
        "MAXRELDIFF:",
        np.max(
            np.abs(
                (res_tensor_np[0:256] - golden_tensor_np[0:256])
                / (golden_tensor_np[0:256] + 1e-8)
            )
        ),
    )
    print(
        "MEANERRSQ:",
        np.mean((res_tensor_np[0:256] - golden_tensor_np[0:256]) ** 2),
    )
