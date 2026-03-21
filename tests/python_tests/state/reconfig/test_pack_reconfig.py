# SPDX-FileCopyrightText: © 2026 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0

import copy

from helpers.dump import TensixDump
from helpers.format_config import DataFormat, FormatConfig
from helpers.llk_params import (
    DestAccumulation,
)
from helpers.param_config import parametrize
from helpers.test_config import TestConfig
from helpers.test_variant_parameters import (
    CONFIGURE_TEST_RUN_IDX,
    P_DST_FORMAT,
    P_DST_FORMAT_NEXT,
    P_FACE_R_DIM,
    P_FACE_R_DIM_NEXT,
    P_NARROW_TILE,
    P_NARROW_TILE_NEXT,
    P_NUM_FACES,
    P_NUM_FACES_NEXT,
    P_PARTIAL_FACE,
    P_PARTIAL_FACE_NEXT,
    P_SRC_FORMAT,
    P_SRC_FORMAT_NEXT,
    P_TILE_C_DIM,
    P_TILE_C_DIM_NEXT,
    P_TILE_SIZE,
    P_TILE_SIZE_NEXT,
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


SHAPES = {
    (1, 32): (1, 32, 2, 1, 0),
    (2, 32): (2, 32, 2, 1, 0),
    (4, 32): (4, 32, 2, 1, 0),
    (8, 32): (8, 32, 2, 1, 0),
    (16, 32): (16, 32, 2, 0, 0),
    (32, 32): (16, 32, 4, 0, 0),
    (32, 16): (16, 16, 2, 0, 1),
    (32, 8): (16, 8, 2, 1, 1),
    (16, 8): (16, 8, 1, 1, 1),
    (8, 8): (8, 8, 1, 1, 1),
}


def sanitize(dump: dict) -> dict:
    out = copy.deepcopy(dump)
    for gpr in out.get("gpr", []):
        if isinstance(gpr, dict):
            # state/reconfig/test_pack_reconfig.py::test_pack_AB_reconfig[format_from:Float16-format_from_next:Float16-format_to:Float16-format_to_next:Float16-shape:(1, 32)-shape_next:(2, 32)] ✓
            gpr.pop("tile_header", None)
            # state/reconfig/test_pack_reconfig.py::test_pack_AB_reconfig[format_from:Float16-format_from_next:Float16-format_to:Float16-format_to_next:Float16-shape:(1, 32)-shape_next:(2, 32)] ✓
            gpr.pop("tmp_lo", None)
            # state/reconfig/test_pack_reconfig.py::test_pack_AB_reconfig[format_from:Float16-format_from_next:Float16-format_to:Float16-format_to_next:Bfp8-shape:(1, 32)-shape_next:(1, 32)] ✓
            gpr.pop("tmp_hi", None)
            # state/reconfig/test_pack_reconfig.py::test_pack_AB_reconfig[format_from:Float16-format_from_next:Float16-format_to:Float16-format_to_next:Float16-shape:(1, 32)-shape_next:(2, 32)] ✓
            for k in list(gpr.keys()):
                if k.startswith("exp") and "sec_size" in k:
                    gpr.pop(k, None)

    ac = out.get("address_counters")
    if isinstance(ac, dict):
        # state/reconfig/test_pack_reconfig.py::test_pack_AB_reconfig[format_from:UInt8-format_from_next:UInt8-format_to:UInt8-format_to_next:UInt8-shape:(8, 8)-shape_next:(32, 8)] ✓
        ac.pop("adcs2_packers_channel1_x_cr", None)
        ac.pop("adcs2_packers_channel1_x_counter", None)

    for pc in out.get("pack_config", []):
        if isinstance(pc, dict):
            # state/reconfig/test_pack_reconfig.py::test_pack_AB_reconfig[format_from:Float16-format_from_next:Float16-format_to:Float16-format_to_next:Float16-shape:(1, 32)-shape_next:(2, 32)] ✓
            pc.pop("l1_dest_addr", None)
            # state/reconfig/test_pack_reconfig.py::test_pack_AB_reconfig[format_from:Float16-format_from_next:Float16-format_to:Float16-format_to_next:Float16-shape:(1, 32)-shape_next:(2, 32)] ✓
            pc.pop("exp_section_size", None)
            # state/reconfig/test_pack_reconfig.py::test_pack_AB_reconfig[format_from:Float16-format_from_next:Float16-format_to:Float16_b-format_to_next:Float16-shape:(1, 32)-shape_next:(1, 32)] ✓
            pc.pop("exp_threshold_en", None)
            pc.pop("exp_threshold", None)

    for ctr in out.get("pack_counters", []):
        if isinstance(ctr, dict):
            # state/reconfig/test_pack_reconfig.py::test_pack_AB_reconfig[format_from:Float16-format_from_next:Float16-format_to:Float16-format_to_next:Float16-shape:(1, 32)-shape_next:(2, 32)] ✓
            ctr.pop("pack_reads_per_xy_plane", None)
    return out


@parametrize(
    format_from=ALL_FORMATS,
    format_from_next=ALL_FORMATS,
    format_to=ALL_FORMATS,
    format_to_next=ALL_FORMATS,
    shape=list(SHAPES.keys()),
    shape_next=list(SHAPES.keys()),
)
def test_pack_AB_reconfig(
    format_from,
    format_from_next,
    format_to,
    format_to_next,
    shape,
    shape_next,
    workers_tensix_coordinates,
):

    face_r_dim, tile_c_dim, num_faces, partial_face, narrow_tile = SHAPES[shape]
    (
        face_r_dim_next,
        tile_c_dim_next,
        num_faces_next,
        partial_face_next,
        narrow_tile_next,
    ) = SHAPES[shape_next]

    configuration = TestConfig(
        "sources/state/reconfig/pack_reconfig_test.cpp",
        formats=FormatConfig(
            DataFormat.Float16,
            DataFormat.Float16,
            DataFormat.Float16,
            DataFormat.Float16,
            DataFormat.Float16,
        ),
        templates=[
            P_SRC_FORMAT(format_from),
            P_DST_FORMAT(format_to),
            P_SRC_FORMAT_NEXT(format_from_next),
            P_DST_FORMAT_NEXT(format_to_next),
        ],
        runtimes=[
            P_TILE_SIZE(0xA),
            P_FACE_R_DIM(face_r_dim),
            P_TILE_C_DIM(tile_c_dim),
            P_NUM_FACES(num_faces),
            P_PARTIAL_FACE(partial_face),
            P_NARROW_TILE(narrow_tile),
            P_TILE_SIZE_NEXT(0xB),
            P_FACE_R_DIM_NEXT(face_r_dim_next),
            P_TILE_C_DIM_NEXT(tile_c_dim_next),
            P_NUM_FACES_NEXT(num_faces_next),
            P_PARTIAL_FACE_NEXT(partial_face_next),
            P_NARROW_TILE_NEXT(narrow_tile_next),
            CONFIGURE_TEST_RUN_IDX(0),
        ],
        dest_acc=DestAccumulation.Yes,
    )

    expected = configuration.run(workers_tensix_coordinates).dumps[0]

    configuration = TestConfig(
        "sources/state/reconfig/pack_reconfig_test.cpp",
        formats=FormatConfig(
            DataFormat.Float16,
            DataFormat.Float16,
            DataFormat.Float16,
            DataFormat.Float16,
            DataFormat.Float16,
        ),
        templates=[
            P_SRC_FORMAT(format_from),
            P_DST_FORMAT(format_to),
            P_SRC_FORMAT_NEXT(format_from_next),
            P_DST_FORMAT_NEXT(format_to_next),
        ],
        runtimes=[
            P_TILE_SIZE(0xA),
            P_FACE_R_DIM(face_r_dim),
            P_TILE_C_DIM(tile_c_dim),
            P_NUM_FACES(num_faces),
            P_PARTIAL_FACE(partial_face),
            P_NARROW_TILE(narrow_tile),
            P_TILE_SIZE_NEXT(0xB),
            P_FACE_R_DIM_NEXT(face_r_dim_next),
            P_TILE_C_DIM_NEXT(tile_c_dim_next),
            P_NUM_FACES_NEXT(num_faces_next),
            P_PARTIAL_FACE_NEXT(partial_face_next),
            P_NARROW_TILE_NEXT(narrow_tile_next),
            CONFIGURE_TEST_RUN_IDX(1),
        ],
        dest_acc=DestAccumulation.Yes,
    )

    actual = configuration.run(workers_tensix_coordinates).dumps[0]

    TensixDump.assert_equal(sanitize(expected), sanitize(actual))
