# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0

import pytest
import torch

from conftest import skip_for_blackhole
from helpers.device import (
    collect_results,
    write_stimuli_to_l1,
)
from helpers.format_arg_mapping import (
    DestAccumulation,
    MathFidelity,
    MathOperation,
    format_dict,
)
from helpers.format_config import DataFormat
from helpers.param_config import (
    input_output_formats,
    parametrize,
)
from helpers.stimuli_generator import generate_stimuli
from helpers.test_config import run_test
from helpers.utils import passed_test


@skip_for_blackhole
@parametrize(
    test_name="col_tile_sdpa_test",
    formats=input_output_formats(
        [
            DataFormat.Float16_b,
        ]
    ),
    mathop=[MathOperation.Elwsub],
    dest_acc=[DestAccumulation.No],
    math_fidelity=[
        MathFidelity.LoFi,
    ],
    input_dimensions=[
        [128, 32],
        # [128, 64],
        # [64, 128],
    ],
)
def test_unp_bcast_sub_sdpa(
    test_name, formats, mathop, dest_acc, math_fidelity, input_dimensions
):

    if mathop != MathOperation.Elwmul and math_fidelity != MathFidelity.LoFi:
        pytest.skip("Fidelity does not affect Elwadd and Elwsub operations")

    src_A, src_B, tile_cnt = generate_stimuli(
        formats.input_format, formats.input_format, input_dimensions=input_dimensions
    )
    src_A = src_A[:1024]  # take just 1 tile on A

    reshaped_a = src_A.reshape(64, 16)  # Single tile A: 64x16
    reshaped_b = src_B.reshape(64 * 4, 16)  # Four tiles B: 256x16

    take = []
    for tile in range(4):  # For each of the 4 B tiles
        take.append(reshaped_a[0])  # Always use row 0 from A
        take.append(reshaped_a[16])  # Always use row 16 from A

    result = torch.stack(take)  # Shape: [8, 16]
    result = result.repeat_interleave(8, dim=0)  # Shape: [64, 16]
    flattened_a = result.view(-1, 128)  # Shape: [8, 128]

    # For B: process all 4 tiles (256 rows total)
    reshaped = reshaped_b.view(-1, 8, 16)  # Shape: [32, 8, 16]
    flattened_b = reshaped.view(-1, 128)  # Shape: [32, 128]

    # Create pattern for indexing
    num_segments = len(flattened_a) // 2  # 4 segments
    pattern = []

    for seg in range(num_segments):
        base = seg * 2
        seg_pattern = [base, base, base + 1, base + 1, base, base, base + 1, base + 1]
        pattern.extend(seg_pattern)

    pattern_a = torch.tensor(pattern)
    pattern_b = torch.arange(len(flattened_b))

    golden_tensor = []

    # Compute subtraction for all combinations
    for i in range(len(pattern_b)):
        golden_tensor.append((flattened_a[pattern_a[i]] - flattened_b[pattern_b[i]]))

    # Flatten result
    golden_tensor = torch.cat(golden_tensor, dim=0)

    # ******************************************************************************

    test_config = {
        "formats": formats,
        "testname": test_name,
        "dest_acc": dest_acc,
        "input_A_dimensions": input_dimensions,
        "input_B_dimensions": input_dimensions,
        "mathop": mathop,
        "math_fidelity": math_fidelity,
        "tile_cnt": tile_cnt,
    }

    res_address = write_stimuli_to_l1(
        test_config,
        src_A,
        src_B,
        formats.input_format,
        formats.input_format,
        tile_count_A=1,
        tile_count_B=tile_cnt,
    )

    run_test(test_config)

    res_from_L1 = collect_results(formats, tile_count=tile_cnt, address=res_address)
    assert len(res_from_L1) == len(golden_tensor)

    torch_format = format_dict[formats.output_format]
    res_tensor = torch.tensor(res_from_L1, dtype=torch_format)

    assert passed_test(golden_tensor, res_tensor, formats.output_format)
