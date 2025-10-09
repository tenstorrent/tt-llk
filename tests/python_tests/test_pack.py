# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0

import torch

from helpers.device import (
    collect_results,
    write_stimuli_to_l1,
)
from helpers.format_arg_mapping import DestAccumulation, format_dict
from helpers.format_config import DataFormat
from helpers.golden_generators import (
    DataCopyGolden,
    PackPartialFaceGolden,
    get_golden_generator,
)
from helpers.param_config import (
    input_output_formats,
    parametrize,
)
from helpers.stimuli_generator import generate_stimuli
from helpers.test_config import run_test
from helpers.utils import passed_test


def generate_pack_combinations():
    # r_dim_list = [1, 2, 4, 8, 16, 32]
    # c_dim_list = [8, 16, 32, 64]
    tile_dim_list = [
        (1, 32),
        (2, 32),
        (4, 32),
        (8, 32),
    ]  # , (2, 32), (4, 32), (8, 32),(16, 32), (32, 32), (32, 16), (32, 8), (16, 16), (16, 8), (8, 8)]
    # num_faces_list = [1, 2, 4]
    # partial_face_list = [False, True]

    combinations = []

    for tile_dim in tile_dim_list:
        if tile_dim[0] == 32 and tile_dim[1] == 32:
            num_faces = 4
            partial_face = False
        elif tile_dim[0] <= 16 and tile_dim[1] <= 16:
            num_faces = 1
            partial_face = False if tile_dim[0] == tile_dim[1] else True
        else:
            num_faces = 2
            partial_face = (
                False if tile_dim[0] % 16 == 0 and tile_dim[1] % 16 == 0 else True
            )

        combinations.append((tile_dim, num_faces, partial_face))

    return combinations


@parametrize(
    test_name="pack_test",
    formats=input_output_formats(
        [
            # DataFormat.Float16_b,
            DataFormat.Float16,
            # DataFormat.Float32,
            # DataFormat.Bfp8_b,
        ]
    ),
    dest_acc=[DestAccumulation.No],  # , DestAccumulation.Yes],
    tile_dim_combinations=generate_pack_combinations(),
)
def test_pack(test_name, formats, dest_acc, tile_dim_combinations):

    input_dimensions = [32, 32]
    tile_dim = tile_dim_combinations[0]
    in0_tile_r_dim = tile_dim[0]
    in0_tile_c_dim = tile_dim[1]
    num_faces = tile_dim_combinations[1]
    partial_face = tile_dim_combinations[2]

    src_A, src_B, tile_cnt = generate_stimuli(
        formats.input_format,
        formats.input_format,
        input_dimensions=input_dimensions,
    )

    if partial_face:
        generate_golden = get_golden_generator(PackPartialFaceGolden)
        golden_tensor = generate_golden(
            src_A, formats.output_format, [in0_tile_r_dim, in0_tile_c_dim], tile_cnt
        )
    else:
        generate_golden = get_golden_generator(DataCopyGolden)
        golden_tensor = generate_golden(
            src_A, formats.output_format, 4, input_dimensions
        )

    unpack_to_dest = (
        formats.input_format.is_32_bit() and dest_acc == DestAccumulation.Yes
    )

    test_config = {
        "formats": formats,
        "testname": test_name,
        "dest_acc": dest_acc,
        "input_A_dimensions": input_dimensions,
        "input_B_dimensions": input_dimensions,
        "unpack_to_dest": unpack_to_dest,
        "tile_cnt": tile_cnt,
        "num_faces": num_faces,
        "partial_face_A": partial_face,
        "in0_tile_r_dim": in0_tile_r_dim,
        "in0_tile_c_dim": in0_tile_c_dim,
    }

    res_address = write_stimuli_to_l1(
        test_config,
        src_A,
        src_B,
        formats.input_format,
        formats.input_format,
        tile_count_A=tile_cnt,
        tile_count_B=tile_cnt,
        num_faces=4,
    )

    run_test(test_config)

    res_from_L1 = collect_results(
        formats, tile_count=tile_cnt, address=res_address, num_faces=4
    )
    print(f"srca A: {src_A}")
    print(f"before RES_FROM_L1: {res_from_L1}")
    if partial_face:
        res_from_L1 = res_from_L1[: in0_tile_r_dim * in0_tile_c_dim]

    assert len(res_from_L1) == len(golden_tensor)

    print(f"GOLDEN: {golden_tensor}")
    print(f"RES_FROM_L1: {res_from_L1}")

    torch_format = format_dict[formats.output_format]
    res_tensor = torch.tensor(res_from_L1, dtype=torch_format)

    assert passed_test(golden_tensor, res_tensor, formats.output_format)
