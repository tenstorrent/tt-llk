# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0

from typing import List

import pytest
import torch
from helpers.data_format_inference import infer_data_formats
from helpers.format_config import DataFormat, FormatConfig
from helpers.golden_generators import (
    DataCopyGolden,
    PackGolden,
    get_golden_generator,
)
from helpers.llk_params import (
    DestAccumulation,
    ImpliedMathFormat,
    PackerReluType,
    format_dict,
)
from helpers.param_config import (
    generate_unary_input_dimensions,
    input_output_formats,
    parametrize,
)
from helpers.stimuli_config import StimuliConfig
from helpers.stimuli_generator import generate_stimuli
from helpers.test_config import BootMode, TestConfig
from helpers.test_variant_parameters import (
    DEST_SYNC,
    IMPLIED_MATH_FORMAT,
    NUM_FACES,
    RELU_CONFIG,
    TEST_FACE_DIMS,
    TILE_COUNT,
)
from helpers.utils import passed_test
from test_pack import is_relu_threshold_tolerance_issue


def generate_qsr_pack_combinations(
    formats_list: List[FormatConfig],
):
    """
    Generate pack combinations.

    Args: Input-output format pairs

    Returns: List of (format, dest_acc, input_dimensions, relu_type) tuples
    """
    dimensions_cache = {
        DestAccumulation.No: tuple(
            generate_unary_input_dimensions(DestAccumulation.No)
        ),
        DestAccumulation.Yes: tuple(
            generate_unary_input_dimensions(DestAccumulation.Yes)
        ),
    }

    relu_types = [
        PackerReluType.NoRelu,
        PackerReluType.ZeroRelu,
        PackerReluType.MinThresholdRelu,
        PackerReluType.MaxThresholdRelu,
    ]

    combinations = []

    for fmt in formats_list:
        in_fmt = fmt.input_format

        dest_acc_modes = (
            (DestAccumulation.Yes,)
            if in_fmt.is_32_bit()
            else (DestAccumulation.No, DestAccumulation.Yes)
        )
        for dest_acc in dest_acc_modes:
            if (
                in_fmt != DataFormat.Float32
                and fmt.output_format == DataFormat.Float32
                and dest_acc == DestAccumulation.No
            ):
                # Skip if input format is not Float32 and output format is Float32 and dest_acc is No
                # This combination is not supported in the Quasar Packer format conversions
                continue
            for dimensions in dimensions_cache[dest_acc]:
                for relu_type in relu_types:
                    combinations.append((fmt, dest_acc, dimensions, relu_type))

    return combinations


PACK_FORMATS = input_output_formats(
    [
        DataFormat.Float16_b,
        DataFormat.Float16,
        DataFormat.Float32,
    ]
)


@pytest.mark.quasar
@parametrize(
    formats_dest_acc_input_dims=generate_qsr_pack_combinations(PACK_FORMATS),
)
def test_pack_quasar(formats_dest_acc_input_dims, boot_mode=BootMode.DEFAULT):
    (formats, dest_acc, input_dimensions, relu_type) = formats_dest_acc_input_dims[0]

    src_A, tile_cnt_A, src_B, _ = generate_stimuli(
        stimuli_format_A=formats.input_format,
        input_dimensions_A=input_dimensions,
        stimuli_format_B=formats.input_format,
        input_dimensions_B=input_dimensions,
    )

    num_faces = 4
    generate_golden = get_golden_generator(DataCopyGolden)
    golden_tensor = generate_golden(
        src_A,
        formats.output_format,
        num_faces=num_faces,
        input_dimensions=input_dimensions,
    )

    # Same method as test_pack.py for original ReLu testing and threshold tolerance issue
    unpack_to_dest = (
        formats.input_format.is_32_bit() and dest_acc == DestAccumulation.Yes
    )
    data_formats = infer_data_formats(
        input_format=formats.input_format,
        output_format=formats.output_format,
        is_fp32_dest_acc_en=dest_acc,
        unpacking_to_dest=unpack_to_dest,
    )

    if data_formats.pack_src.is_integer() and relu_type in [
        PackerReluType.MinThresholdRelu,
        PackerReluType.MaxThresholdRelu,
    ]:
        pytest.skip(
            "Pack source format cannot be an integer format with ReLu Type: "
            + str(relu_type)
        )

    tensor_average = (
        torch.mean(golden_tensor).item()
        if not formats.output_format.is_integer()
        else 0.0
    )

    relu_config = PackGolden.generate_relu_config(
        relu_type,
        relu_threshold=tensor_average,
        intermediate_format=data_formats.pack_src,
    )

    golden_tensor = PackGolden.apply_relu(
        golden_tensor,
        relu_config,
        data_formats.pack_src,
    )

    configuration = TestConfig(
        "sources/quasar/pack_quasar_test.cpp",
        formats,
        templates=[
            IMPLIED_MATH_FORMAT(ImpliedMathFormat.Yes),
            DEST_SYNC(),
        ],
        runtimes=[
            TEST_FACE_DIMS(),
            NUM_FACES(num_faces),
            TILE_COUNT(tile_cnt_A),
            RELU_CONFIG(relu_config),
        ],
        variant_stimuli=StimuliConfig(
            src_A,
            formats.input_format,
            src_B,
            formats.input_format,
            formats.output_format,
            tile_count_A=tile_cnt_A,
            tile_count_B=tile_cnt_A,
            tile_count_res=tile_cnt_A,
            num_faces=num_faces,
        ),
        unpack_to_dest=unpack_to_dest,
        dest_acc=dest_acc,
        boot_mode=boot_mode,
    )

    res_from_L1 = configuration.run().result

    assert len(res_from_L1) == len(
        golden_tensor
    ), "Result tensor and golden tensor are not of the same length"

    torch_format = format_dict[formats.output_format]
    res_tensor = torch.tensor(res_from_L1, dtype=torch_format)

    test_passed = passed_test(
        golden_tensor, res_tensor, formats.output_format, print_erros=False
    )

    # Same method as test_pack.py for original ReLu testing and threshold tolerance issue
    # Could be adjusted to have Golden model HW behaviour for pack ReLu activation which applies ReLu on datum format in dst and then converts to output format
    # Check issue/request: https://github.com/tenstorrent/tt-llk/issues/1391
    if (
        not test_passed
        and relu_type
        in [
            PackerReluType.MinThresholdRelu,
            PackerReluType.MaxThresholdRelu,
        ]
        and is_relu_threshold_tolerance_issue(
            golden_tensor,
            res_tensor,
            relu_config,
            data_formats.pack_src,
        )
    ):
        test_passed = True

    assert test_passed, "Assert against golden failed"
