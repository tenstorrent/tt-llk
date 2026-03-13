# SPDX-FileCopyrightText: © 2026 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0

import pytest
import torch
from helpers.constraints import get_valid_dest_accumulation_modes
from helpers.format_config import DataFormat
from helpers.golden_generators import (
    FACE_DIM,
    FACES_PER_TILE,
    TILE_DIM,
    BroadcastGolden,
    get_golden_generator,
)
from helpers.llk_params import (
    BroadcastType,
    DestAccumulation,
    ImpliedMathFormat,
    format_dict,
)
from helpers.param_config import input_output_formats, parametrize
from helpers.stimuli_config import StimuliConfig
from helpers.test_config import BootMode, TestConfig
from helpers.test_variant_parameters import (
    BROADCAST_TYPE,
    DEST_SYNC,
    IMPLIED_MATH_FORMAT,
    INPUT_DIMENSIONS,
    NUM_FACES,
    TEST_FACE_DIMS,
    TILE_COUNT,
)
from helpers.utils import passed_test


def get_valid_dest_acc_unary_broadcast(formats):
    """Dest_acc valid for unary broadcast: 32-bit => Yes; 16-bit with Float32 out => Yes."""
    accs = list(get_valid_dest_accumulation_modes(formats))
    if formats.input_format.is_32_bit():
        accs = [a for a in accs if a == DestAccumulation.Yes]
    elif formats.output_format == DataFormat.Float32:
        accs = [a for a in accs if a == DestAccumulation.Yes]
    return accs if accs else [DestAccumulation.Yes]


@pytest.mark.quasar
@parametrize(
    formats=input_output_formats(
        [
            DataFormat.Float16_b,
            # DataFormat.Float32,
            DataFormat.MxFp8R,
            DataFormat.MxFp8P,
        ],
        same=True,
    ),
    dest_acc=lambda formats: get_valid_dest_acc_unary_broadcast(formats),
    broadcast_type=[
        BroadcastType.Scalar,
        BroadcastType.Column,
        BroadcastType.Row,
    ],
    implied_math_format=[ImpliedMathFormat.No, ImpliedMathFormat.Yes],
    input_dimensions=[[32, 32]],
)
def test_unary_broadcast_quasar(
    formats,
    dest_acc,
    broadcast_type,
    implied_math_format,
    input_dimensions,
    boot_mode=BootMode.DEFAULT,
):

    torch_format = format_dict[formats.input_format]
    rows, cols = input_dimensions
    num_elements = rows * cols
    tile_cnt_B = (rows // TILE_DIM) * (cols // TILE_DIM)

    src_B = torch.randn(num_elements, dtype=torch_format)

    generate_broadcast_golden = get_golden_generator(BroadcastGolden)
    golden_tensor = generate_broadcast_golden(
        broadcast_type,
        src_B,
        formats.output_format,
        num_faces=FACES_PER_TILE,
        tile_cnt=tile_cnt_B,
        face_r_dim=FACE_DIM,
    )

    unpack_to_dest = (
        formats.input_format.is_32_bit() and dest_acc == DestAccumulation.Yes
    )

    src_A = src_B

    configuration = TestConfig(
        "sources/quasar/eltwise_unary_broadcast_quasar_test.cpp",
        formats,
        templates=[
            INPUT_DIMENSIONS(input_dimensions[0], input_dimensions[1]),
            IMPLIED_MATH_FORMAT(implied_math_format),
            BROADCAST_TYPE(broadcast_type),
            DEST_SYNC(),
        ],
        runtimes=[
            TILE_COUNT(tile_cnt_B),
            NUM_FACES(FACES_PER_TILE),
            TEST_FACE_DIMS(),
        ],
        variant_stimuli=StimuliConfig(
            src_A,
            formats.input_format,
            src_B,
            formats.input_format,
            formats.output_format,
            tile_count_A=tile_cnt_B,
            tile_count_B=tile_cnt_B,
            tile_count_res=tile_cnt_B,
            num_faces=FACES_PER_TILE,
        ),
        unpack_to_dest=unpack_to_dest,
        dest_acc=dest_acc,
        boot_mode=boot_mode,
        disable_format_inference=(implied_math_format == ImpliedMathFormat.Yes),
    )

    res_from_L1 = configuration.run().result

    assert len(res_from_L1) == len(
        golden_tensor
    ), "Result tensor and golden tensor are not of the same length"

    torch_format = format_dict[formats.output_format]
    res_tensor = torch.tensor(res_from_L1, dtype=torch_format)

    from helpers.tilize_untilize import untilize_block

    golden_untilized = untilize_block(
        golden_tensor.flatten(), formats.output_format, input_dimensions
    )
    res_untilized = untilize_block(
        res_tensor.flatten(), formats.output_format, input_dimensions
    )

    assert passed_test(
        golden_untilized.flatten(), res_untilized.flatten(), formats.output_format
    ), "Assert against golden failed"
