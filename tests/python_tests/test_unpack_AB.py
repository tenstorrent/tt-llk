# SPDX-FileCopyrightText: © 2026 Tenstorrent AI ULC
#
# SPDX-License-Identifier: Apache-2.0

from itertools import product

import pytest
import torch
from helpers.chip_architecture import ChipArchitecture, get_chip_architecture
from helpers.format_config import DataFormat
from helpers.golden_generators import (
    BroadcastGolden,
    EltwiseBinaryGolden,
    TransposeGolden,
    get_golden_generator,
)
from helpers.llk_params import (
    BroadcastType,
    DestAccumulation,
    MathFidelity,
    MathOperation,
    StochasticRounding,
    Transpose,
    format_dict,
)
from helpers.param_config import generate_params, input_output_formats
from helpers.stimuli_config import StimuliConfig
from helpers.stimuli_generator import generate_stimuli
from helpers.test_config import TestConfig
from helpers.test_variant_parameters import (
    ACC_TO_DEST,
    BROADCAST_TYPE,
    DISABLE_SRC_ZERO_FLAG,
    NUM_FACES,
    PARTIAL_FACE,
    STOCHASTIC_ROUNDING,
    TEST_FACE_DIMS,
    TILE_COUNT,
    UNPACK_TRANS_FACES,
    UNPACK_TRANS_WITHIN_FACE,
)
from helpers.utils import passed_test

# SUPPORTED FORMATS FOR TEST
supported_formats = [
    DataFormat.Float32,
    DataFormat.Float16,
    DataFormat.Float16_b,
    DataFormat.Bfp8_b,
]

# Define parameter lists
broadcast_types = [
    BroadcastType.None_,
    # BroadcastType.Column,
    # BroadcastType.Row,
    # BroadcastType.Scalar,
]
dest_acc = [DestAccumulation.Yes, DestAccumulation.No]
disable_src_zero_flags = [False, True]
stochastic_rnd = [
    StochasticRounding.No,
    StochasticRounding.Fpu,
    StochasticRounding.Pack,
    StochasticRounding.All,
]

transpose_of_faces_values = [Transpose.No, Transpose.Yes]
within_face_16x16_transpose_values = [Transpose.No, Transpose.Yes]
num_faces_values = [1, 2, 4]
face_r_dim_values = [1, 2, 4, 8, 16]

# Use only cross_test_formats as it already includes same-format combinations
test_formats = input_output_formats(supported_formats, False)

# Generate unpack_A specific parameter combinations using itertools.product
unpack_A_param_combinations = list(
    product(
        broadcast_types,
        disable_src_zero_flags,
        dest_acc,
        stochastic_rnd,
        transpose_of_faces_values,
        within_face_16x16_transpose_values,
        num_faces_values,
        face_r_dim_values,
    )
)

# Create unified parameter combinations
# This combines the power of generate_params with unpack_A specific parameters
all_params = []
# Use generate_params for base parameter structure (like datacopy test)
# Convert itertools.product to list
base_params = list(
    generate_params(testnames=["sources/unpack_AB_test.cpp"], formats=test_formats)
)

# Extend base params with unpack_A specific parameters
for base_param in base_params:
    base_testname = base_param[0]
    formats = base_param[1]

    for unpack_params in unpack_A_param_combinations:
        broadcast_type = unpack_params[0]
        disable_src_zero = unpack_params[1]
        dest_acc = unpack_params[2]
        stochastic_rnd = unpack_params[3]
        transpose_of_faces = unpack_params[4]
        within_face_16x16_transpose = unpack_params[5]
        num_faces = unpack_params[6]
        face_r_dim = unpack_params[7]

        # Create complete parameter tuple matching test signature
        combined_params = (
            base_testname,  # testname
            formats,  # formats
            broadcast_type,  # broadcast_type
            disable_src_zero,  # disable_src_zero
            dest_acc,  # dest_acc
            stochastic_rnd,  # stochastic_rnd
            transpose_of_faces,  # transpose_of_faces
            within_face_16x16_transpose,  # within_face_16x16_transpose
            num_faces,  # num_faces
            face_r_dim,  # face_r_dim
        )
        all_params.append(combined_params)


def filter_params_with_constraints(all_params):
    """Filter valid parameter combinations based on hardware constraints"""

    arch = get_chip_architecture()
    is_wormhole = arch == ChipArchitecture.WORMHOLE
    valid_params = []

    for params in all_params:
        # Extract parameters from tuple
        (
            testname,
            formats,
            broadcast_type,
            disable_src_zero,
            dest_acc,
            stochastic_rnd,
            transpose_of_faces,
            within_face_16x16_transpose,
            num_faces,
            face_r_dim,
        ) = params

        # Fast checks first: simple integer/enum comparisons
        # For partial faces (face_r_dim < 16), require num_faces = 2
        if face_r_dim < 16:
            if num_faces != 2:
                continue
            # Block Bfp8_b input/output for partial faces
            if (
                formats.input_format == DataFormat.Bfp8_b
                or formats.output_format == DataFormat.Bfp8_b
            ):
                continue

        if transpose_of_faces == Transpose.Yes and num_faces != 4:
            continue

        # User constraint: transpose_of_faces and within_face_16x16_transpose are mutually inclusive
        if transpose_of_faces != within_face_16x16_transpose:
            continue

        # Block transpose operations for face_r_dim < 16
        if transpose_of_faces == Transpose.Yes and face_r_dim < 16:
            continue

        # BROADCAST + dest_acc: ALL COMBINATIONS BROKEN (BLOCK ENTIRELY)
        # Check this early as it eliminates many combinations
        if broadcast_type != BroadcastType.None_ and dest_acc == DestAccumulation.Yes:
            continue

        # Broadcast type checks (fast enum comparisons)
        broadcast_none = broadcast_type == BroadcastType.None_

        # COL broadcast requires 4 faces
        if broadcast_type == BroadcastType.Column and num_faces != 4:
            continue

        # ROW broadcast constraint: Requires 4 faces for proper row broadcast
        if broadcast_type == BroadcastType.Row:
            if num_faces != 4:
                continue
            # Block Wormhole Row broadcast with outlier format combinations
            if (
                is_wormhole
                and formats.input_format in (DataFormat.Float16_b, DataFormat.Bfp8_b)
                and formats.output_format == DataFormat.Float16
            ):
                continue

        # SCALAR broadcast + dest_acc not allowed (already checked above, but explicit)
        if broadcast_type == BroadcastType.Scalar and dest_acc == DestAccumulation.Yes:
            continue

        # Exclude dest_acc=yes for simple datacopy operations
        if (
            dest_acc == DestAccumulation.Yes
            and transpose_of_faces == Transpose.No
            and broadcast_none
        ):
            continue

        # Hardware constraint: unpack_to_dest can only be true if dest_acc is false
        # But unpack_to_dest = is_32_bit() and dest_acc, so we must block
        # any case where is_32_bit() and dest_acc are both true
        if formats.input_format.is_32_bit() and dest_acc == DestAccumulation.Yes:
            # This would result in unpack_to_dest=True, but hardware requires dest_acc=No
            # when unpack_to_dest=True. Block this combination.
            continue

        # Format-specific checks (most expensive, do last)
        # Block Bfp8_b output with stochastic rounding (Pack or All)
        if formats.output_format == DataFormat.Bfp8_b:
            if stochastic_rnd in (StochasticRounding.Pack, StochasticRounding.All):
                continue

        # Block Float16/Float16_b transpose combinations that produce garbage values on CI runners
        if (
            formats.input_format in (DataFormat.Float16_b, DataFormat.Float16)
            and broadcast_none
            and dest_acc == DestAccumulation.Yes
            and transpose_of_faces == Transpose.Yes
            and within_face_16x16_transpose == Transpose.Yes
        ):
            continue

        # All constraints passed, add to valid params
        valid_params.append(params)

    return valid_params


# Apply constraint filtering
all_params = filter_params_with_constraints(all_params)


def create_simple_ids(all_params):
    """Create comprehensive but readable IDs for unpack_A tests"""
    ids = []
    for i, params in enumerate(all_params):
        # params = (testname, formats, broadcast_type, disable_src_zero,
        #           dest_acc, stoch_rnd_type, transpose_of_faces,
        #           within_face_16x16_transpose, num_faces, face_r_dim)

        testname = params[0]
        formats = params[1]
        broadcast_type = params[2]
        disable_src_zero = params[3]
        dest_acc = params[4]
        stochastic_rnd = params[5]
        transpose_of_faces = params[6]
        within_face_16x16_transpose = params[7]
        num_faces = params[8]
        face_r_dim = params[9]

        # Create a comprehensive but readable ID
        id_parts = [
            f"in_{formats.input_format.name}",
            f"out_{formats.output_format.name}",
            f"bcast_{broadcast_type.name}",
            f"disable_src_zero_{disable_src_zero}",
            f"dest_acc{dest_acc}",
            f"stoch_rnd_{stochastic_rnd.name}",
            f"transpose_faces_{transpose_of_faces.name}",
            f"within_face_transpose_{within_face_16x16_transpose.name}",
            f"num_faces_{num_faces}",
            f"face_r_dim_{face_r_dim}",
        ]

        id_str = "-".join(id_parts)
        ids.append(id_str)

    return ids


param_ids = create_simple_ids(all_params)


@pytest.mark.parametrize(
    "testname, formats, broadcast_type, disable_src_zero, dest_acc, "
    "stochastic_rnd, transpose_of_faces, "
    "within_face_16x16_transpose, num_faces, face_r_dim, ",
    all_params,
    ids=param_ids,
)
def test_unpack_AB(
    testname,
    formats,
    broadcast_type,
    disable_src_zero,
    dest_acc,
    stochastic_rnd,
    transpose_of_faces,
    within_face_16x16_transpose,
    num_faces,
    face_r_dim,
    workers_tensix_coordinates,
):

    # torch.manual_seed(0.0)
    # Configure input dimensions based on face_r_dim
    # For partial faces (face_r_dim < 16), use [face_r_dim x 32] input tensors
    if face_r_dim < 16:
        input_dimensions = [face_r_dim, 32]  # [1x32], [2x32], [4x32], [8x32]
        partial_face = True
    else:
        input_dimensions = [32, 32]
        partial_face = False

    src_A, tile_cnt_A, src_B, tile_cnt_B = generate_stimuli(
        stimuli_format_A=formats.input_format,
        input_dimensions_A=input_dimensions,
        stimuli_format_B=formats.input_format,
        input_dimensions_B=input_dimensions,
        face_r_dim=face_r_dim,
        num_faces=num_faces,
    )

    op_A = src_A
    op_B = src_B

    # generate golden tensor with proper broadcast and transpose handling
    # PRIORITY: Broadcast types take precedence over transpose operations
    if broadcast_type in (
        BroadcastType.Scalar,
        BroadcastType.Column,
        BroadcastType.Row,
    ):
        # Broadcast: replicate values according to broadcast type
        generate_broadcast_golden = get_golden_generator(BroadcastGolden)
        op_B = generate_broadcast_golden(
            broadcast_type,
            op_B,
            formats.input_format,
            num_faces=num_faces,
            face_r_dim=face_r_dim,
        )

    if transpose_of_faces == Transpose.Yes:
        # Both transpose flags are ALWAYS on together (mutually inclusive constraint)
        transpose_golden = get_golden_generator(TransposeGolden)
        # First apply within-face transpose, then face transpose
        op_A_temp = transpose_golden.transpose_within_faces(
            op_A, formats.input_format, input_dimensions, num_faces
        )
        op_A = transpose_golden.transpose_faces(
            op_A_temp, formats.input_format, input_dimensions, num_faces
        )

        # # Now for op_B
        # if broadcast_type == BroadcastType.None_:
        #     # Only transpose if no broadcast applied
        #     op_B_temp = transpose_golden.transpose_within_faces(
        #         op_B, formats.input_format, input_dimensions, num_faces
        #     )
        #     op_B = transpose_golden.transpose_faces(
        #         op_B_temp, formats.input_format, input_dimensions, num_faces
        #     )

    face_c_dim = 16
    elements_per_tile_needed = face_r_dim * face_c_dim * num_faces
    op_A = op_A[:elements_per_tile_needed]
    op_B = op_B[:elements_per_tile_needed]

    # Add two operands together
    add_golden = get_golden_generator(EltwiseBinaryGolden)
    golden_tensor = add_golden(
        MathOperation.Elwadd, op_A, op_B, formats.output_format, MathFidelity.LoFi
    )

    configuration = TestConfig(
        testname,
        formats,
        templates=[
            STOCHASTIC_ROUNDING(stochastic_rnd),
            BROADCAST_TYPE(broadcast_type),
            ACC_TO_DEST(True if dest_acc == DestAccumulation.Yes else False),
            PARTIAL_FACE(
                partial_a=partial_face,
                partial_face_pack=partial_face,
                partial_b=partial_face,
                partial_face_math=partial_face,
            ),
            DISABLE_SRC_ZERO_FLAG(disable_src_zero),
        ],
        runtimes=[
            UNPACK_TRANS_FACES(transpose_of_faces),
            UNPACK_TRANS_WITHIN_FACE(within_face_16x16_transpose),
            NUM_FACES(num_faces),
            TILE_COUNT(tile_cnt_A),
            TEST_FACE_DIMS(face_r_dim=face_r_dim),
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
            num_faces=num_faces,
            face_r_dim=face_r_dim,
        ),
        dest_acc=dest_acc,
        unpack_to_dest=(
            formats.input_format.is_32_bit() and dest_acc == DestAccumulation.Yes
        ),
    )

    res_from_L1 = configuration.run(workers_tensix_coordinates)

    assert len(res_from_L1) == len(
        golden_tensor
    ), "Result tensor and golder tensor are not of the same length"

    res_tensor = torch.tensor(res_from_L1, dtype=format_dict[formats.output_format])
    assert passed_test(
        golden_tensor, res_tensor, formats.output_format
    ), "Assert against golden failed"
