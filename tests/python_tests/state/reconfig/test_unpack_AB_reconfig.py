# SPDX-FileCopyrightText: © 2026 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0


import pytest
from helpers.format_config import DataFormat, FormatConfig
from helpers.llk_params import (
    DestAccumulation,
)
from helpers.param_config import parametrize
from helpers.tensix_dump import TensixDump
from helpers.test_config import TestConfig
from helpers.test_variant_parameters import (
    DST_FORMAT_A,
    DST_FORMAT_A_NEXT,
    DST_FORMAT_B,
    DST_FORMAT_B_NEXT,
    FACE_R_DIM_A,
    FACE_R_DIM_A_NEXT,
    FACE_R_DIM_B,
    FACE_R_DIM_B_NEXT,
    NUM_FACES_A,
    NUM_FACES_A_NEXT,
    NUM_FACES_B,
    NUM_FACES_B_NEXT,
    SRC_FORMAT_A,
    SRC_FORMAT_A_NEXT,
    SRC_FORMAT_B,
    SRC_FORMAT_B_NEXT,
    TILE_SIZE_A,
    TILE_SIZE_A_NEXT,
    TILE_SIZE_B,
    TILE_SIZE_B_NEXT,
    TO_FROM_INT8,
)


def get_valid_num_faces(row_dim: int) -> list[int]:
    if row_dim == 16:
        return [1, 2, 4]

    return [1, 2]


def get_valid_to_from_int8(
    format_from: DataFormat,
    format_to: DataFormat,
) -> bool:
    return format_from.is_integer() or format_to.is_integer()


def get_valid_dest_acc(to_from_int8: bool) -> bool:
    if to_from_int8:
        return DestAccumulation.Yes

    return [DestAccumulation.No, DestAccumulation.Yes]


ALL_FORMATS = [
    DataFormat.Float16,
    DataFormat.Float16_b,
    DataFormat.Bfp8,
    DataFormat.Bfp8_b,
    DataFormat.Float32,
    # DataFormat.Int32, # lol
    DataFormat.Tf32,
    # DataFormat.UInt32, # lol
    DataFormat.UInt16,
    DataFormat.Int8,
    DataFormat.UInt8,
]


@parametrize(
    format_from=ALL_FORMATS,
    format_to=ALL_FORMATS,
    row_dim_a=[8, 16],
    row_dim_b=[8, 16],
    num_faces_a=lambda row_dim_a: get_valid_num_faces(row_dim_a),
    num_faces_b=lambda row_dim_b: get_valid_num_faces(row_dim_b),
    tile_size_a=0xA,
    tile_size_b=0xB,
    row_dim_a_next=[8, 16],
    row_dim_b_next=[8, 16],
    num_faces_a_next=lambda row_dim_a_next: get_valid_num_faces(row_dim_a_next),
    num_faces_b_next=lambda row_dim_b_next: get_valid_num_faces(row_dim_b_next),
    tile_size_a_next=0xC,
    tile_size_b_next=0xD,
    to_from_int8=lambda format_from, format_to: get_valid_to_from_int8(
        format_from, format_to
    ),
    dest_acc=lambda to_from_int8: get_valid_dest_acc(to_from_int8),
)
def test_unpack_AB_reconfig(
    format_from,
    format_to,
    row_dim_a,
    row_dim_b,
    num_faces_a,
    num_faces_b,
    tile_size_a,
    tile_size_b,
    row_dim_a_next,
    row_dim_b_next,
    num_faces_a_next,
    num_faces_b_next,
    tile_size_a_next,
    tile_size_b_next,
    to_from_int8,
    dest_acc,
    workers_tensix_coordinates,
):

    print(format_from, format_to, to_from_int8)

    configuration = TestConfig(
        "sources/state/reconfig/unpack_AB_reconfig_test.cpp",
        formats=FormatConfig(
            DataFormat.Float16,
            DataFormat.Float16,
            DataFormat.Float16,
            DataFormat.Float16,
            DataFormat.Float16,
        ),
        templates=[
            SRC_FORMAT_A(format_from),
            SRC_FORMAT_B(format_from),
            DST_FORMAT_A(format_from),
            DST_FORMAT_B(format_from),
            SRC_FORMAT_A_NEXT(format_to),
            SRC_FORMAT_B_NEXT(format_to),
            DST_FORMAT_A_NEXT(format_to),
            DST_FORMAT_B_NEXT(format_to),
            TO_FROM_INT8(to_from_int8),
        ],
        runtimes=[
            FACE_R_DIM_A(row_dim_a),
            FACE_R_DIM_B(row_dim_b),
            NUM_FACES_A(num_faces_a),
            NUM_FACES_B(num_faces_b),
            TILE_SIZE_A(tile_size_a),
            TILE_SIZE_B(tile_size_b),
            FACE_R_DIM_A_NEXT(row_dim_a_next),
            FACE_R_DIM_B_NEXT(row_dim_b_next),
            NUM_FACES_A_NEXT(num_faces_a_next),
            NUM_FACES_B_NEXT(num_faces_b_next),
            TILE_SIZE_A_NEXT(tile_size_a_next),
            TILE_SIZE_B_NEXT(tile_size_b_next),
        ],
        dest_acc=dest_acc,
    )

    _, dumps = configuration.run(workers_tensix_coordinates)

    if num_faces_a != num_faces_a_next:
        pytest.xfail("NUM_FACES_A != NUM_FACES_A_NEXT")

    if num_faces_b != num_faces_b_next:
        pytest.xfail("NUM_FACES_B != NUM_FACES_B_NEXT")

    if row_dim_a != row_dim_a_next:
        pytest.xfail("FACE_R_DIM_A != FACE_R_DIM_A_NEXT")

    if row_dim_b != row_dim_b_next:
        pytest.xfail("FACE_R_DIM_B != FACE_R_DIM_B_NEXT")

    TensixDump.assert_equal(dumps[0], dumps[1])
