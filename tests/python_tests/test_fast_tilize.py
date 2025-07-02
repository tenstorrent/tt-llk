# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0

import pytest
import torch

from helpers.device import (
    collect_results,
    write_stimuli_to_l1,
)
from helpers.format_arg_mapping import DestAccumulation, format_dict
from helpers.format_config import DataFormat, InputOutputFormat
from helpers.golden_generators import TilizeGolden, get_golden_generator
from helpers.perf import PerfRunType, perf_benchmark, write_to_report
from helpers.stimuli_generator import generate_stimuli
from helpers.test_config import run_test
from helpers.utils import passed_test


def generate_input_dimensions(max_size):
    dimensions = []
    for width in range(1, max_size + 1):
        max_height = max_size // width
        heights = [
            1,
            2,
            3,
            (max_height // 2) - 1,
            max_height // 2,
            (max_height // 2) + 1,
            max_height - 2,
            max_height - 1,
            max_height,
        ]
        heights = [h for h in heights if h > 0 and h <= max_height]
        heights = list(set(heights))
        for height in heights:
            dimensions.append((width, height))
    return dimensions


# @skip_for_blackhole
@pytest.mark.parametrize("input_format", [DataFormat.Float32, DataFormat.Float16_b])
@pytest.mark.parametrize(
    "output_format", [DataFormat.Float32, DataFormat.Float16_b, DataFormat.Bfp8_b]
)
@pytest.mark.parametrize("fp32_dest", [DestAccumulation.Yes, DestAccumulation.No])
@pytest.mark.parametrize("input_width, input_height", generate_input_dimensions(32))
def test_fast_tilize(input_format, output_format, fp32_dest, input_width, input_height):

    input_dimensions = [input_height * 32, input_width * 32]

    src_A, src_B, tile_cnt = generate_stimuli(
        input_format, input_format, input_dimensions=input_dimensions
    )

    generate_golden = get_golden_generator(TilizeGolden)
    golden_tensor = generate_golden(src_A, input_dimensions, output_format)

    res_address = write_stimuli_to_l1(
        src_A, src_B, input_format, input_format, tile_count=tile_cnt
    )

    formats = InputOutputFormat(input_format, output_format)

    test_config = {
        "formats": formats,
        "testname": "fast_tilize_test",
        "tile_cnt": tile_cnt,
        "input_dimensions": input_dimensions,
        "dest_acc": fp32_dest,
    }

    run_test(test_config)

    res_from_L1 = collect_results(formats, tile_count=tile_cnt, address=res_address)
    assert len(res_from_L1) == len(golden_tensor)

    res_tensor = torch.tensor(res_from_L1, dtype=format_dict[output_format])

    assert passed_test(golden_tensor, res_tensor, formats.output_format)


# @skip_for_blackhole
@pytest.mark.perf
@pytest.mark.parametrize("input_format", [DataFormat.Float32, DataFormat.Float16_b])
@pytest.mark.parametrize(
    "output_format", [DataFormat.Float32, DataFormat.Float16_b, DataFormat.Bfp8_b]
)
@pytest.mark.parametrize("fp32_dest", [DestAccumulation.Yes, DestAccumulation.No])
@pytest.mark.parametrize("input_width, input_height", generate_input_dimensions(16))
def test_fast_tilize_perf(
    input_format, output_format, fp32_dest, input_width, input_height
):

    input_dimensions = [input_height * 32, input_width * 32]

    formats = InputOutputFormat(input_format, output_format)

    test_config = {
        "formats": formats,
        "testname": "fast_tilize_test",
        "tile_cnt": input_height * input_width,
        "input_dimensions": input_dimensions,
        "dest_acc": fp32_dest,
    }

    results = perf_benchmark(test_config, [PerfRunType.L1_TO_L1], 2)
    write_to_report(test_config, results)
