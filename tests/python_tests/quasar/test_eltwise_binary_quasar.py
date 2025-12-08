# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0

import pytest
import torch
from helpers.chip_architecture import ChipArchitecture
from helpers.constraints import (
    get_valid_dest_accumulation_modes,
    get_valid_math_fidelities,
)
from helpers.device import (
    collect_results,
    write_stimuli_to_l1,
)
from helpers.format_config import DataFormat
from helpers.golden_generators import (
    EltwiseBinaryGolden,
    get_golden_generator,
)
from helpers.llk_params import (
    ImpliedMathFormat,
    MathOperation,
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
    test_name="eltwise_binary_test",
    formats=input_output_formats(
        [
            DataFormat.Float16_b,
            DataFormat.Float16,
            DataFormat.Float32,
        ],
    ),
    mathop=[
        MathOperation.Elwadd,
        MathOperation.Elwsub,
        MathOperation.Elwmul,
    ],
    math_fidelity=lambda formats, mathop: get_valid_math_fidelities(formats, mathop),
    dest_acc=lambda formats, mathop: get_valid_dest_accumulation_modes(
        ChipArchitecture.QUASAR, formats, unpack_to_dest=False
    ),
    implied_math_format=[
        ImpliedMathFormat.No,
        ImpliedMathFormat.Yes,
    ],
    input_dimensions=lambda dest_acc: generate_unary_input_dimensions(dest_acc),
    num_faces=[4],
)
def test_eltwise_binary(
    test_name,
    formats,
    mathop,
    math_fidelity,
    dest_acc,
    implied_math_format,
    input_dimensions,
    num_faces,
    boot_mode=BootMode.DEFAULT,
):

    # Generate stimuli for both operands
    src_A, src_B, tile_cnt = generate_stimuli(
        formats.input_format,
        formats.input_format,
        input_dimensions=input_dimensions,
    )

    # Generate golden result using eltwise binary golden generator
    generate_golden = get_golden_generator(EltwiseBinaryGolden)
    golden_tensor = generate_golden(
        mathop,
        src_A,
        src_B,
        formats.output_format,
        math_fidelity,
    )

    test_config = {
        "formats": formats,
        "testname": test_name,
        "mathop": mathop,
        "math_fidelity": math_fidelity,
        "implied_math_format": implied_math_format,
        "dest_acc": dest_acc,
        "input_A_dimensions": input_dimensions,
        "input_B_dimensions": input_dimensions,
        "unpack_to_dest": False,
        "tile_cnt": tile_cnt,
        "num_faces": num_faces,
    }

    # Write both operands to L1 memory
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

    # Run the C++ kernel
    run_test(test_config, boot_mode=boot_mode)

    # Collect results from L1 memory
    res_from_L1 = collect_results(
        formats, tile_count=tile_cnt, address=res_address, num_faces=num_faces
    )

    # Verify results match golden
    assert len(res_from_L1) == len(golden_tensor)

    torch_format = format_dict[formats.output_format]
    res_tensor = torch.tensor(res_from_L1, dtype=torch_format)

    assert passed_test(golden_tensor, res_tensor, formats.output_format)
