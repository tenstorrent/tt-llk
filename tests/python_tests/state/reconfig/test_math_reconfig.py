# SPDX-FileCopyrightText: © 2026 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0

from itertools import product

from helpers.dump import TensixDump
from helpers.format_config import DataFormat, FormatConfig
from helpers.llk_params import (
    DestAccumulation,
)
from helpers.param_config import parametrize
from helpers.test_config import TestConfig
from helpers.test_variant_parameters import TO_FROM_INT8


def gen_valid_formats(
    formats: list[DataFormat],
) -> list[tuple[DataFormat, DataFormat, DataFormat, DataFormat]]:
    groups = [
        [f for f in formats if f.is_exponent_A()],
        [f for f in formats if f.is_exponent_B()],
        [f for f in formats if f.is_integer()],
    ]

    return [
        combo
        for prev_group in groups
        for next_group in groups
        for combo in product(prev_group, prev_group, next_group, next_group)
    ]


def get_valid_to_from_int8(
    formats: tuple[DataFormat, DataFormat, DataFormat, DataFormat],
) -> bool:
    if any(f.is_integer() for f in formats):
        return True

    return False


def get_valid_dest_acc(to_from_int8: bool) -> bool:
    if to_from_int8:
        return DestAccumulation.Yes

    return [DestAccumulation.No, DestAccumulation.Yes]


@parametrize(
    formats=gen_valid_formats(
        [
            DataFormat.Float16,
            DataFormat.Float16_b,
            DataFormat.Bfp8,
            DataFormat.Bfp8_b,
            DataFormat.Float32,
            DataFormat.Int32,
            DataFormat.Tf32,
            DataFormat.UInt32,
            DataFormat.UInt16,
            DataFormat.Int8,
            DataFormat.UInt8,
        ]
    ),
    to_from_int8=lambda formats: get_valid_to_from_int8(formats),
    dest_acc=lambda to_from_int8: get_valid_dest_acc(to_from_int8),
)
def test_math_reconfig(
    formats,
    to_from_int8,
    dest_acc,
    workers_tensix_coordinates,
):
    prev_a, prev_b, next_a, next_b = formats

    configuration = TestConfig(
        "sources/state/reconfig/math_reconfig_test.cpp",
        FormatConfig(
            prev_a, prev_b, next_a, next_b, DataFormat.Float32
        ),  # ikr, but there is no less painful way to do this right now
        templates=[
            TO_FROM_INT8(to_from_int8),
        ],
        runtimes=[],
        dest_acc=dest_acc,
    )

    dumps = configuration.run(workers_tensix_coordinates).dumps

    TensixDump.assert_equal(dumps[0], dumps[1])
