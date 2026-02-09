# SPDX-FileCopyrightText: © 2026 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0

from itertools import product

from helpers.format_config import DataFormat, FormatConfig
from helpers.llk_params import (
    DestAccumulation,
)
from helpers.param_config import parametrize
from helpers.tensix_dump import TensixDump
from helpers.test_config import TestConfig
from helpers.test_variant_parameters import TO_FROM_INT8


def gen_valid_format_pairs(
    formats: list[DataFormat],
) -> list[tuple[DataFormat, DataFormat]]:
    formats_float_a = [f for f in formats if f.is_exponent_A()]
    formats_float_b = [f for f in formats if f.is_exponent_B()]
    formats_integer = [f for f in formats if f.is_integer()]

    return [
        *product(formats_float_a, formats_float_a),
        *product(formats_float_b, formats_float_b),
        *product(formats_integer, formats_integer),
    ]


def get_valid_to_from_int8(
    pair_from: tuple[DataFormat, DataFormat], pair_to: tuple[DataFormat, DataFormat]
) -> bool:
    from_a, from_b = pair_from
    to_a, to_b = pair_to

    if (
        from_a.is_integer()
        or from_b.is_integer()
        or to_a.is_integer()
        or to_b.is_integer()
    ):
        return True

    return False


def get_valid_dest_acc(to_from_int8: bool) -> bool:
    if to_from_int8:
        return DestAccumulation.Yes

    return [DestAccumulation.No, DestAccumulation.Yes]


valid_format_pairs = gen_valid_format_pairs(
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
)


@parametrize(
    pair_from=valid_format_pairs,
    pair_to=valid_format_pairs,
    to_from_int8=lambda pair_from, pair_to: get_valid_to_from_int8(pair_from, pair_to),
    dest_acc=lambda to_from_int8: get_valid_dest_acc(to_from_int8),
)
def test_math_A_reconfig(
    pair_from,
    pair_to,
    to_from_int8,
    dest_acc,
    workers_tensix_coordinates,
):
    configuration = TestConfig(
        "sources/state/reconfig/math_AB_reconfig_test.cpp",
        # HACK! HACK! HACK! we need to pass 4 DataFormats into the test for math, so we will misuse the slots meant for unpack and pack
        FormatConfig(
            pair_from[0], pair_from[1], pair_to[0], pair_to[1], DataFormat.Float32
        ),
        templates=[
            TO_FROM_INT8(to_from_int8),
        ],
        runtimes=[],
        dest_acc=dest_acc,
    )

    _, dumps = configuration.run(workers_tensix_coordinates)

    TensixDump.assert_equal(dumps[0], dumps[1])
