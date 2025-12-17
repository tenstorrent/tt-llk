# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0

import torch
from helpers.chip_architecture import ChipArchitecture, get_chip_architecture
from helpers.device import collect_results, write_stimuli_to_l1
from helpers.format_config import DataFormat
from helpers.golden_generators import DataCopyGolden, TilizeGolden, get_golden_generator
from helpers.llk_params import DestAccumulation, Tilize, format_dict
from helpers.param_config import (
    input_output_formats,
    parametrize,
)
from helpers.stimuli_generator import generate_stimuli
from helpers.test_config import run_test
from helpers.utils import passed_test


def get_valid_tilize_datacopy(formats):
    """
    Get valid tilize options for _llk_math_eltwise_unary_datacopy_

    - Blackhole and Wormhole have differing APIs:
        - Blackhole: Has tilize argument (SW workaround for HW bug)
        - Wormhole: No tilize argument (No SW workaround needed)
    - Tilize cannot be enabled if input format is Bfp8_b (HW limitation)

    Therefore we only test tilization on Blackhole
    """

    chip_arch = get_chip_architecture()

    if chip_arch == ChipArchitecture.WORMHOLE:
        return [Tilize.No]

    if formats.input_format == DataFormat.Bfp8_b:
        return [Tilize.No]

    return [Tilize.No, Tilize.Yes]


def get_valid_num_faces_datacopy(tilize):
    """
    Get valid num_faces options for _llk_math_eltwise_unary_datacopy_

    - Number of faces must be 4 when tilization is enabled (SW limitation)
    - Otherwise num_faces can be 1, 2, or 4
    """

    if tilize == Tilize.Yes:
        return [4]

    return [1, 2, 4]


@parametrize(
    test_name="eltwise_unary_datacopy_test",
    formats=input_output_formats(
        [
            # DataFormat.Float32,
            # DataFormat.Float16,
            DataFormat.Float16_b,
            # DataFormat.Bfp8_b,
        ]
    ),
    dest_acc=[DestAccumulation.No],
    num_faces=[4],
    tilize=[Tilize.No],
    dest_index=[0],
    input_dimensions=[[32, 32], [32, 64], [64, 32], [64, 64], [128, 64], [64, 128]],
)
def test_unary_datacopy(
    test_name, formats, dest_acc, num_faces, tilize, dest_index, input_dimensions
):

    src_A, src_B, tile_cnt = generate_stimuli(
        formats.input_format,
        formats.input_format,
        input_dimensions=input_dimensions,
    )

    if tilize == Tilize.No:
        generate_golden = get_golden_generator(DataCopyGolden)
        golden_tensor = generate_golden(
            src_A, formats.output_format, num_faces, input_dimensions
        )
    else:
        generate_golden = get_golden_generator(TilizeGolden)
        golden_tensor = generate_golden(src_A, input_dimensions, formats.output_format)

    unpack_to_dest = True

    test_config = {
        "formats": formats,
        "testname": test_name,
        "dest_acc": dest_acc,
        "input_A_dimensions": input_dimensions,
        "input_B_dimensions": input_dimensions,
        "unpack_to_dest": unpack_to_dest,
        "tile_cnt": tile_cnt,
        "num_faces": num_faces,
        "tilize": tilize,
        "dest_index": dest_index,
    }

    res_address = write_stimuli_to_l1(
        test_config,
        src_A,
        src_B,
        formats.input_format,
        formats.input_format,
        tile_count_A=tile_cnt,
        tile_count_B=tile_cnt,
        num_faces=num_faces,
    )

    run_test(test_config)

    res_from_L1 = collect_results(
        formats, tile_count=tile_cnt, address=res_address, num_faces=num_faces
    )

    assert len(res_from_L1) == len(golden_tensor)

    torch_format = format_dict[formats.output_format]
    res_tensor = torch.tensor(res_from_L1, dtype=torch_format)

    assert passed_test(golden_tensor, res_tensor, formats.output_format)
