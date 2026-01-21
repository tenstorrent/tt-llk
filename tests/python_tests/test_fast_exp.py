# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0


import pytest
import torch
from helpers.chip_architecture import ChipArchitecture

# from helpers.profiler import ProfilerConfig
from helpers.format_config import DataFormat, InputOutputFormat
from helpers.golden_generators import UnarySFPUGolden, get_golden_generator
from helpers.llk_params import (
    ApproximationMode,
    DestAccumulation,
    MathOperation,
    format_dict,
)
from helpers.param_config import input_output_formats, parametrize
from helpers.stimuli_config import StimuliConfig
from helpers.stimuli_generator import generate_stimuli
from helpers.test_config import TestConfig
from helpers.test_variant_parameters import (
    APPROX_MODE,
    INPUT_DIMENSIONS,
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

    min_val, max_val = -5, 5
    # min_val, max_val = 0, 1
    size = 256  # src_A.shape[0]
    for i in range(size):
        src_A[i] = min_val + (max_val - min_val) * i / (size - 1.0)

    generate_golden = get_golden_generator(UnarySFPUGolden)
    golden_tensor = generate_golden(
        mathop,
        src_A,
        formats.output_format,
        dest_acc,
        formats.input_format,
        input_dimensions,
    )

    # configuration = ProfilerConfig(
    configuration = TestConfig(
        "sources/fast_exp_test.cpp",
        formats,
        templates=[
            INPUT_DIMENSIONS(input_dimensions, input_dimensions),
            APPROX_MODE(approx_mode),
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
            tile_count_res=tile_cnt_A,
        ),
        dest_acc=dest_acc,
        # If dest_acc is off, we unpack Float32 into 16-bit format in src registers (later copied over in dest reg for SFPU op)
        unpack_to_dest=(
            formats.input_format.is_32_bit() and dest_acc == DestAccumulation.Yes
        ),
    )

    res_from_L1 = configuration.run(workers_tensix_coordinates)

    # runtime = Profiler.get_data(test_config["testname"])
    # timestamps = runtime.math().timestamps()  # .marker("TEST_TIMESTAMP").frame()
    # zones = runtime.zones().marker("TILE_LOOP").frame()
    # print(zones)

    assert len(res_from_L1) == len(
        golden_tensor
    ), "Result tensor and golder tensor are not of the same length"

    torch_format = format_dict[formats.output_format]
    res_tensor = torch.tensor(res_from_L1, dtype=torch_format)

    src_A_np = src_A.double().numpy()
    res_tensor_np = res_tensor.double().numpy()
    golden_tensor_np = golden_tensor.double().numpy()

    import numpy as np

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

    # for i in range(256):
    #    print(
    #        i,
    #        src_A_np[i],
    #        golden_tensor_np[i],
    #        res_tensor_np[i],
    #        np.max(np.abs(res_tensor_np[i] - golden_tensor_np[i])),
    #    )

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
