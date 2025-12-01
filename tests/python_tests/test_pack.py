# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0
"""Test pack operation with various configurations.

Tests the LLK pack kernel with:
- Different data formats (Float16_b, Float16, Float32, Int32, Bfp8_b)
- Destination accumulation modes
- Variable tile dimensions
- ReLU activation
- Destination sync modes (SyncHalf for double-buffering, SyncFull for single-buffering)
"""

import pytest
import torch
from helpers.device import collect_results, write_stimuli_to_l1
from helpers.format_config import DataFormat
from helpers.golden_generators import PackGolden, get_golden_generator
from helpers.llk_params import (
    DestAccumulation,
    DstSync,
    format_dict,
)
from helpers.param_config import input_output_formats, parametrize
from helpers.stimuli_generator import generate_stimuli
from helpers.test_config import run_test
from helpers.utils import passed_test


@parametrize(
    test_name="pack_test",
    formats=input_output_formats(
        [
            DataFormat.Float16_b,
            DataFormat.Float16,
            DataFormat.Float32,
            DataFormat.Int32,
            DataFormat.Bfp8_b,
        ]
    ),
    dest_acc=[DestAccumulation.No, DestAccumulation.Yes],
    input_dimensions=[[32, 32], [64, 64], [32, 64], [64, 32]],
    relu_config=[0, 1],
    dst_sync=[DstSync.SyncHalf, DstSync.SyncFull],
)
def test_pack(test_name, formats, dest_acc, input_dimensions, relu_config, dst_sync):
    # Skip invalid format combinations
    if formats.output_format == DataFormat.Bfp8_b:
        pytest.skip("Pack does not support Bfp8_b output format")

    if (formats.input_format == DataFormat.Int32) ^ (
        formats.output_format == DataFormat.Int32
    ):
        pytest.skip("Pack does not support mixing Int32 with other formats")

    if (
        formats.input_format == DataFormat.Int32
        and formats.output_format == DataFormat.Int32
        and dest_acc == DestAccumulation.No
    ):
        pytest.skip("Dest must be in 32bit mode when input and output are Int32")

    # Skip ReLU with integer formats
    if relu_config != 0 and formats.output_format in [DataFormat.Int32]:
        pytest.skip("ReLU not applicable for integer formats")

    # Generate test data
    src_A, src_B, tile_cnt = generate_stimuli(
        formats.input_format,
        formats.input_format,
        input_dimensions=input_dimensions,
    )

    # Generate golden output
    generate_golden = get_golden_generator(PackGolden)
    golden_tensor = generate_golden(
        src_A,
        formats.output_format,
        input_dimensions=input_dimensions,
        relu_config=relu_config,
    )

    test_config = {
        "formats": formats,
        "testname": test_name,
        "tile_cnt": tile_cnt,
        "input_dimensions": input_dimensions,
        "input_A_dimensions": input_dimensions,
        "input_B_dimensions": input_dimensions,
        "unpack_to_dest": formats.input_format.is_32_bit(),
        "dest_acc": dest_acc,
        "relu_config": relu_config,
        "dst_sync": dst_sync,
    }

    res_address = write_stimuli_to_l1(
        test_config,
        src_A,
        src_B,
        formats.input_format,
        formats.input_format,
        tile_count_A=tile_cnt,
        tile_count_B=tile_cnt,
    )

    run_test(test_config)

    res_from_L1 = collect_results(
        formats,
        tile_count=tile_cnt,
        address=res_address,
        tile_dimensions=input_dimensions,
    )
    assert len(res_from_L1) == len(golden_tensor)

    torch_format = format_dict[formats.output_format]
    res_tensor = torch.tensor(res_from_L1, dtype=torch_format)

    assert passed_test(golden_tensor, res_tensor, formats.output_format)
