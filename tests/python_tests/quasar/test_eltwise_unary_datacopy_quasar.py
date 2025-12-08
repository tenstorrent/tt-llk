# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0


import pytest
import torch
from helpers.chip_architecture import ChipArchitecture
from helpers.constraints import (
    get_valid_dest_accumulation_modes,
)
from helpers.device import (
    collect_results,
    write_stimuli_to_l1,
)
from helpers.format_config import DataFormat
from helpers.golden_generators import (
    DataCopyGolden,
    get_golden_generator,
)
from helpers.llk_params import (
    DataCopyType,
    DestAccumulation,
    ImpliedMathFormat,
    UnpackerEngine,
    format_dict,
)
from helpers.param_config import (
    calculate_edgecase_dest_indices,
    generate_unary_input_dimensions,
    input_output_formats,
    parametrize,
)
from helpers.stimuli_generator import generate_stimuli
from helpers.test_config import run_test
from helpers.utils import passed_test


@pytest.mark.quasar
@parametrize(
    test_name="eltwise_unary_datacopy_quasar_test",
    formats=input_output_formats(
        [
            DataFormat.Float16_b,
            DataFormat.Float16,
            DataFormat.Float32,
        ]
    ),
    dest_acc=lambda formats: get_valid_dest_accumulation_modes(
        ChipArchitecture.QUASAR, formats, unpack_to_dest=False
    ),
    data_copy_type=[DataCopyType.A2D, DataCopyType.B2D],
    input_dimensions=lambda dest_acc: generate_unary_input_dimensions(dest_acc),
    dest_index=lambda dest_acc, input_dimensions: calculate_edgecase_dest_indices(
        True if dest_acc == DestAccumulation.Yes else False,
        input_dimensions[0] // 32 * input_dimensions[1] // 32,
    ),
    implied_math_format=[ImpliedMathFormat.Yes, ImpliedMathFormat.No],
)
def test_eltwise_unary_datacopy_quasar(
    test_name,
    formats,
    dest_acc,
    data_copy_type,
    input_dimensions,
    dest_index,
    implied_math_format,
):
    src_A, src_B, tile_cnt = generate_stimuli(
        formats.input_format,
        formats.input_format,
        input_dimensions=input_dimensions,
    )

    golden_src = src_B if data_copy_type == DataCopyType.B2D else src_A
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
        "unpack_to_dest": False,
        "tile_cnt": tile_cnt,
        "unpacker_engine_sel": (
            UnpackerEngine.UnpB
            if data_copy_type == DataCopyType.B2D
            else UnpackerEngine.UnpA
        ),
        "data_copy_type": data_copy_type,
        "implied_math_format": implied_math_format,
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
        num_faces=4,
    )

    run_test(test_config)

    res_from_L1 = collect_results(
        formats, tile_count=tile_cnt, address=res_address, num_faces=4
    )

    assert len(res_from_L1) == len(golden_tensor)

    torch_format = format_dict[formats.output_format]
    res_tensor = torch.tensor(res_from_L1, dtype=torch_format)

    assert passed_test(golden_tensor, res_tensor, formats.output_format)
