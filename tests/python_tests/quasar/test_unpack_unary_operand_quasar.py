# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0


import pytest
import torch
from helpers.chip_architecture import ChipArchitecture
from helpers.constraints import (
    get_valid_dest_accumulation_modes,
    get_valid_unpack_to_dest_modes,
)
from helpers.device import (
    collect_results,
    write_stimuli_to_l1,
)
from helpers.format_config import DataFormat
from helpers.golden_generators import (
    DataCopyGolden,
    TransposeGolden,
    get_golden_generator,
)
from helpers.llk_params import (
    DataCopyType,
    ImpliedMathFormat,
    Transpose,
    UnpackerEngine,
    format_dict,
)
from helpers.param_config import (
    generate_unary_input_dimensions,
    input_output_formats,
    parametrize,
)
from helpers.stimuli_generator import generate_stimuli
from helpers.test_config import BootMode, run_test
from helpers.utils import passed_test


@pytest.mark.quasar
@parametrize(
    test_name="unpack_unary_operand_quasar_test",
    formats=input_output_formats(
        [
            DataFormat.Float16_b,
            DataFormat.Float16,
            DataFormat.Float32,
        ]
    ),
    unpack_to_dest=lambda formats: get_valid_unpack_to_dest_modes(formats),
    dest_acc=lambda formats, unpack_to_dest: get_valid_dest_accumulation_modes(
        ChipArchitecture.QUASAR, formats, unpack_to_dest
    ),
    transpose_en=lambda unpack_to_dest: (
        [Transpose.No] if unpack_to_dest else [Transpose.No, Transpose.Yes]
    ),
    unpacker_sel=lambda unpack_to_dest: (
        [UnpackerEngine.UnpDest]
        if unpack_to_dest
        else [UnpackerEngine.UnpA, UnpackerEngine.UnpB]
    ),
    input_dimensions=lambda dest_acc: generate_unary_input_dimensions(dest_acc),
)
def test_unpack_unary_operand_quasar(
    test_name,
    formats,
    unpack_to_dest,
    dest_acc,
    transpose_en,
    unpacker_sel,
    input_dimensions,
    boot_mode=BootMode.DEFAULT,
):

    src_A, src_B, tile_cnt = generate_stimuli(
        formats.input_format,
        formats.input_format,
        input_dimensions=input_dimensions,
    )

    golden_src = (
        src_B if unpacker_sel == UnpackerEngine.UnpB else src_A
    )  # use A for UnpA and UnpDest
    if transpose_en == Transpose.Yes:
        generate_golden = get_golden_generator(TransposeGolden)
        golden_tensor = generate_golden.transpose_faces_multi_tile(
            golden_src,
            formats.output_format,
            num_tiles=tile_cnt,
            tilize=False,
            input_dimensions=input_dimensions,
        )
        golden_tensor = generate_golden.transpose_within_faces_multi_tile(
            golden_tensor,
            formats.output_format,
            num_tiles=tile_cnt,
            untilize=False,
            input_dimensions=input_dimensions,
        )
    else:
        generate_golden = get_golden_generator(DataCopyGolden)
        golden_tensor = generate_golden(
            golden_src,
            formats.output_format,
            num_faces=4,
            input_dimensions=input_dimensions,
        )

    test_config = {
        "formats": formats,
        "testname": test_name,
        "dest_acc": dest_acc,
        "input_A_dimensions": input_dimensions,
        "input_B_dimensions": input_dimensions,
        "unpack_to_dest": unpack_to_dest,
        "tile_cnt": tile_cnt,
        "unpack_transpose_faces": transpose_en,
        "unpack_transpose_within_face": transpose_en,
        "unpacker_engine_sel": unpacker_sel,
        "implied_math_format": ImpliedMathFormat.Yes,
        "data_copy_type": (
            DataCopyType.B2D
            if unpacker_sel == UnpackerEngine.UnpB
            else DataCopyType.A2D
        ),
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
