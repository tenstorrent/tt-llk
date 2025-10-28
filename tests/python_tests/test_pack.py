# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0

import pytest
import torch
from helpers.device import (
    collect_results,
    write_stimuli_to_l1,
)
from helpers.format_config import DataFormat
from helpers.golden_generators import (
    DataCopyGolden,
    get_golden_generator,
)
from helpers.llk_params import DestAccumulation, format_dict
from helpers.param_config import (
    input_output_formats,
    parametrize,
)
from helpers.stimuli_generator import generate_stimuli
from helpers.test_config import BootMode, run_test
from helpers.utils import passed_test


def generate_input_dimensions(dest_acc):
    if dest_acc == DestAccumulation.No:
        return [
            [32, 32],
            [32, 64],
            [32, 96],
            [32, 128],
            [32, 160],
            [32, 192],
            [32, 224],
            [32, 256],
            [64, 32],
            [64, 64],
            [64, 96],
            [64, 128],
            [96, 32],
            [96, 64],
            [128, 32],
            [128, 64],
            [160, 32],
            [192, 32],
            [224, 32],
            [256, 32],
        ]
    else:
        return [
            [32, 32],
            [32, 64],
            [32, 96],
            [32, 128],
            [64, 32],
            [64, 64],
            [96, 32],
            [128, 32],
        ]


@parametrize(
    test_name="pack_test",
    formats=input_output_formats(
        [
            DataFormat.Float16_b,
            # DataFormat.Float16,
            # DataFormat.Float32,
            # DataFormat.Int32,
        ],
        same=True,
    ),
    dest_acc=[DestAccumulation.No],  # DestAccumulation.Yes],
    input_dimensions=generate_input_dimensions(DestAccumulation.No),
)
def test_pack(
    test_name, formats, dest_acc, input_dimensions, boot_mode=BootMode.DEFAULT
):

    if formats.input_format == DataFormat.Float32 and dest_acc == DestAccumulation.No:
        pytest.skip("Fails, probably due to data inference")

    if formats.input_format == DataFormat.Int32 and dest_acc == DestAccumulation.No:
        pytest.skip("Fails, we usually skip this case")

    if formats.input_format == DataFormat.Float16 and dest_acc == DestAccumulation.Yes:
        pytest.skip("Fails, probably due to data inference")

    # if formats.input_format == DataFormat.Float16 and dest_acc == DestAccumulation.No:
    #    pytest.skip("Fails when ran after Fp16_b:dest_acc=No, makes Fp32:dest_acc=Yes fail when ran before")

    # input_dimensions = [32, 32]

    src_A, src_B, tile_cnt = generate_stimuli(
        formats.input_format,
        formats.input_format,
        input_dimensions=input_dimensions,
    )

    generate_golden = get_golden_generator(DataCopyGolden)
    golden_tensor = generate_golden(src_A, formats.output_format, 4, input_dimensions)

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

    run_test(test_config, boot_mode=boot_mode)

    res_from_L1 = collect_results(
        formats, tile_count=tile_cnt, address=res_address, num_faces=4
    )

    assert len(res_from_L1) == len(golden_tensor)

    torch_format = format_dict[formats.output_format]
    res_tensor = torch.tensor(res_from_L1, dtype=torch_format)

    assert passed_test(golden_tensor, res_tensor, formats.output_format)
