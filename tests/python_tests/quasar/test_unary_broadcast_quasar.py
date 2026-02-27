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
from helpers.param_config import (
    generate_unary_input_dimensions,
    input_output_formats,
)
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


def generate_unary_broadcast_combinations():
    """Generate all unary broadcast test combinations."""
    formats_list = input_output_formats(
        [
            DataFormat.Float16_b,
            DataFormat.Float32,
        ],
    )

    broadcast_types = [
        BroadcastType.Scalar,
        BroadcastType.Column,
        BroadcastType.Row,
    ]

    implied_math_formats = [
        ImpliedMathFormat.No,
        ImpliedMathFormat.Yes,
    ]

    combinations = []
    for fmt in formats_list:
        dest_acc_modes = get_valid_dest_accumulation_modes(fmt)
        if fmt.input_format.is_32_bit():
            dest_acc_modes = (DestAccumulation.Yes,)
        for dest_acc in dest_acc_modes:
            # When dest register is in 16-bit mode, packer cannot convert Float16/16_b -> Float32
            if (
                not fmt.input_format.is_32_bit()
                and fmt.output_format == DataFormat.Float32
                and dest_acc == DestAccumulation.No
            ):
                continue

            for broadcast_type in broadcast_types:
                for implied_math_format in implied_math_formats:
                    for input_dimensions in generate_unary_input_dimensions(dest_acc):
                        combinations.append(
                            (
                                fmt,
                                dest_acc,
                                broadcast_type,
                                implied_math_format,
                                input_dimensions,
                            )
                        )

    return combinations


UNARY_BROADCAST_COMBINATIONS = generate_unary_broadcast_combinations()


@pytest.mark.quasar
@pytest.mark.parametrize(
    "formats,dest_acc,broadcast_type,implied_math_format,input_dimensions",
    UNARY_BROADCAST_COMBINATIONS,
)
def test_unary_broadcast_quasar(
    formats,
    dest_acc,
    broadcast_type,
    implied_math_format,
    input_dimensions,
    boot_mode=BootMode.DEFAULT,
):

    # Generate input stimuli with face-specific values: face 0=1s, face 1=2s, face 2=3s, face 3=4s
    torch_format = format_dict[formats.input_format]
    rows, cols = input_dimensions
    num_elements = rows * cols
    tile_cnt_B = (rows // TILE_DIM) * (cols // TILE_DIM)

    # Create tilized tensor: each tile has 4 faces, each face has 256 elements (16x16)
    # Face 0 = 1s, Face 1 = 2s, Face 2 = 3s, Face 3 = 4s
    elements_per_face = FACE_DIM * FACE_DIM  # 256
    elements_per_tile = elements_per_face * FACES_PER_TILE  # 1024

    src_B_list = []
    for tile_idx in range(tile_cnt_B):
        for face_idx in range(FACES_PER_TILE):
            face_value = float(face_idx + 1)  # 1, 2, 3, 4
            face_data = torch.full((elements_per_face,), face_value, dtype=torch_format)
            src_B_list.append(face_data)

    src_B = torch.cat(src_B_list)[:num_elements]

    # Generate golden tensor with broadcast (returns tilized)
    generate_broadcast_golden = get_golden_generator(BroadcastGolden)
    golden_tensor = generate_broadcast_golden(
        broadcast_type,
        src_B,
        formats.output_format,
        num_faces=FACES_PER_TILE,
        tile_cnt=tile_cnt_B,
        face_r_dim=FACE_DIM,
    )

    # Determine unpack_to_dest based on format
    # unpack_to_dest is used for 32-bit data with dest_acc=Yes
    unpack_to_dest = (
        formats.input_format.is_32_bit() and dest_acc == DestAccumulation.Yes
    )

    # When unpack_to_dest is true, use src_A data for buffer_A (UNPACKER0)
    # When unpack_to_dest is false, use src_B data for buffer_B (UNPACKER1)
    # For now, we generate the same data for both, but buffer_A will be used when unpack_to_dest is true
    src_A = src_B  # Same data for both buffers

    configuration = TestConfig(
        "sources/quasar/eltwise_unary_broadcast_quasar_test.cpp",
        formats,
        templates=[
            INPUT_DIMENSIONS(input_dimensions, input_dimensions),
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
        # MX formats require disable_format_inference to match C++ IMPLIED_MATH_FORMAT setting
        disable_format_inference=(implied_math_format == ImpliedMathFormat.Yes),
    )

    res_from_L1 = configuration.run()

    assert len(res_from_L1) == len(
        golden_tensor
    ), "Result tensor and golden tensor are not of the same length"

    torch_format = format_dict[formats.output_format]
    res_tensor = torch.tensor(res_from_L1, dtype=torch_format)

    # Untilize both for correct display in passed_test (which does .view(32, 32) assuming untilized format)
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
