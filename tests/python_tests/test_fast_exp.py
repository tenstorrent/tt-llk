# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0


import pytest
import torch
from helpers.chip_architecture import ChipArchitecture, get_chip_architecture
from helpers.device import collect_results, write_stimuli_to_l1
from helpers.format_config import DataFormat, InputOutputFormat
from helpers.golden_generators import UnarySFPUGolden, get_golden_generator
from helpers.llk_params import (
    ApproximationMode,
    DestAccumulation,
    MathOperation,
    format_dict,
)
from helpers.param_config import input_output_formats, parametrize
from helpers.profiler import Profiler
from helpers.stimuli_generator import generate_stimuli
from helpers.test_config import ProfilerBuild, run_test


@parametrize(
    test_name="fast_exp_test",
    formats=input_output_formats(
        [
            DataFormat.Float16_b,
        ]
    ),
    input_dimensions=[
        [32, 32]
    ],  # [[32, 32], [32, 64], [64, 32], [64, 64], [128, 32], [32, 128]],
    approx_mode=[ApproximationMode.Yes],
    mathop=[MathOperation.Exp],
    dest_acc=[DestAccumulation.No],  # , DestAccumulation.Yes],
)
def test_eltwise_unary_sfpu_float(
    test_name, approx_mode, formats, mathop, dest_acc, input_dimensions
):
    arch = get_chip_architecture()

    if dest_acc == DestAccumulation.No and arch == ChipArchitecture.BLACKHOLE:
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
        test_name, formats, dest_acc, approx_mode, mathop, input_dimensions
    )


def eltwise_unary_sfpu(
    test_name, formats, dest_acc, approx_mode, mathop, input_dimensions
):
    torch.manual_seed(0)

    src_A, src_B, tile_cnt = generate_stimuli(
        formats.input_format, formats.input_format, input_dimensions=input_dimensions
    )

    # min_val = -20
    # max_val = 0
    size = src_A.shape[0]
    # for i in range(size):
    #    src_A[i] = min_val + (max_val - min_val) * i / (size - 1.0)

    generate_golden = get_golden_generator(UnarySFPUGolden)
    golden_tensor = generate_golden(
        mathop,
        src_A,
        formats.output_format,
        dest_acc,
        formats.input_format,
        input_dimensions,
    )

    unpack_to_dest = (
        formats.input_format.is_32_bit()
        and dest_acc
        == DestAccumulation.Yes  # If dest_acc is off, we unpack Float32 into 16-bit format in src registers (later copied over in dest reg for SFPU op)
    )
    test_config = {
        "formats": formats,
        "testname": test_name,
        "dest_acc": dest_acc,
        "input_A_dimensions": input_dimensions,
        "input_B_dimensions": input_dimensions,
        "mathop": mathop,
        "approx_mode": approx_mode,
        "unpack_to_dest": unpack_to_dest,
        "tile_cnt": tile_cnt,
    }

    # for config_idx, exp_config in enumerate(
    #    [
    #        (2, 1, True),
    #        (2, 2, True),
    #        (2, 3, True),
    #        (2, 4, True),
    #        (2, 5, True),
    #        (4, 1, True),
    #        (4, 2, True),
    #        (4, 3, True),
    #        (4, 4, True),
    #        (4, 5, True),
    #        (6, 1, True),
    #        (6, 2, True),
    #        (6, 3, True),
    #        (6, 4, True),
    #        (6, 5, True),
    #        (0, 0, False),
    #    ]
    # ):
    if True:
        config_idx = 0
        exp_config = (4, 2)

        # Update configuration header.
        # with open("tt_llk_wormhole_b0/common/inc/sfpu/exp_config_params.h", "w") as f:
        #    if exp_config[2]:
        #        f.write("#define EXPONENTIAL_TESTING\n")
        #    f.write("#define EXP_CONFIG_NUM_TERMS " + str(exp_config[0]) + "\n")
        #    f.write("#define EXP_CONFIG_NUM_SCALE " + str(exp_config[1]) + "\n")

        res_address = write_stimuli_to_l1(
            test_config,
            src_A,
            src_B,
            formats.input_format,
            formats.input_format,
            tile_count_A=tile_cnt,
            tile_count_B=tile_cnt,
        )

        run_test(test_config, profiler_build=ProfilerBuild.Yes)
        runtime = Profiler.get_data(test_config["testname"])
        timestamps = runtime.math().timestamps()  # .marker("TEST_TIMESTAMP").frame()
        zones = runtime.zones().marker("TILE_LOOP").frame()
        print(zones)

        res_from_L1 = collect_results(formats, tile_count=tile_cnt, address=res_address)

        assert len(res_from_L1) == len(golden_tensor)

        torch_format = format_dict[formats.output_format]
        res_tensor = torch.tensor(res_from_L1, dtype=torch_format)

        # assert passed_test(
        #    golden_tensor,
        #    res_tensor,
        #    formats.output_format,
        #    custom_atol=0.1,
        #    custom_rtol=0.1,
        #    one_face_check=False,
        # )

        src_A_np = src_A.double().numpy()
        res_tensor_np = res_tensor.double().numpy()
        golden_tensor_np = golden_tensor.double().numpy()

        import numpy as np

        N = 5
        print("")
        print("INPUT_SIZE:", size)
        print("INPUT_1: ", src_A_np[0:N])
        print("OUTPUT_1:", res_tensor_np[0:N])
        print("GOLDEN_1:", golden_tensor_np[0:N])
        print("")
        print("INPUT_2: ", src_A_np[size - N : size])
        print("OUTPUT_2:", res_tensor_np[size - N : size])
        print("GOLDEN_2:", golden_tensor_np[size - N : size])
        print(
            "MAXABSDIFF:",
            np.max(np.abs(res_tensor_np[0:size] - golden_tensor_np[0:size])),
        )
        print(
            "MAXABSDIFF_256:",
            np.max(np.abs(res_tensor_np[0:256] - golden_tensor_np[0:256])),
        )

        # Write results to file.
        # with open(
        #    "exponential_accuracy_test.txt", "w" if config_idx == 0 else "a"
        # ) as f:
        #    if config_idx == 0:
        #        f.write("experiment, num_terms, num_scale, i, input, output, golden\n")
        #    for i in range(size):
        #        f.write(
        #            f"{config_idx}, {exp_config[0]}, {exp_config[1]}, {i}, {src_A_np[i]}, {res_tensor_np[i]}, {golden_tensor_np[i]}\n"
        #        )
