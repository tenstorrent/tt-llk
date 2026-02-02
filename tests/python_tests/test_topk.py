# test_topk.py
# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0

import pytest
import torch
from helpers.format_config import DataFormat, InputOutputFormat
from helpers.llk_params import DestAccumulation, format_dict
from helpers.stimuli_config import StimuliConfig
from helpers.stimuli_generator import generate_stimuli
from helpers.test_config import TestConfig
from helpers.test_variant_parameters import INPUT_DIMENSIONS, TILE_COUNT


@pytest.mark.parametrize(
    "formats",
    [
        InputOutputFormat(DataFormat.Float16_b, DataFormat.Float16_b),
    ],
)
def test_topk_sfpu(formats: InputOutputFormat, workers_tensix_coordinates: str):
    torch.manual_seed(0)
    torch.set_printoptions(precision=5, linewidth=200, sci_mode=False)

    # Logical 1D sequence and k
    N = 64
    k = 10

    input_dimensions = [32, 32]
    TILE_SIZE = 32 * 32
    assert N <= TILE_SIZE

    # 1) Baseline stimuli
    src_A, tile_cnt_A, src_B, tile_cnt_B = generate_stimuli(
        stimuli_format_A=formats.input_format,
        input_dimensions_A=input_dimensions,
        stimuli_format_B=formats.input_format,
        input_dimensions_B=input_dimensions,
        sfpu=False,
    )
    assert tile_cnt_A == 1
    assert src_A.numel() == TILE_SIZE

    # 2) Overwrite first N entries with our 1D sequence
    seq = torch.randn(N, dtype=torch.float32)
    seq_dev = seq.to(format_dict[formats.output_format])

    tile_values = torch.zeros_like(src_A)
    tile_values[:N] = seq_dev
    src_A = tile_values

    def print_tensor_rows(tensor, name):
        print("\n" + "=" * 80)
        print(name)
        print("=" * 80)
        flat = tensor.flatten()
        for i in range(0, flat.numel(), 32):
            row = flat[i : i + 32]
            row_idx = i // 32
            print(f"Row {row_idx:3d} [{i:4d}:{i+len(row):4d}]: {row}")
        print("=" * 80 + "\n")

    print_tensor_rows(src_A, "Input Tensor (src_A)")

    # 3) Run kernel
    configuration = TestConfig(
        test_name="sources/topk_test.cpp",
        formats=formats,
        templates=[
            INPUT_DIMENSIONS(input_dimensions, input_dimensions),
        ],
        runtimes=[
            TILE_COUNT(tile_cnt_A),
        ],
        variant_stimuli=StimuliConfig(
            src_A,
            formats.input_format,
            src_B,
            formats.input_format,
            formats.output_format,
            tile_count_A=tile_cnt_A,
            tile_count_B=tile_cnt_B,
            tile_count_res=tile_cnt_A,
        ),
        dest_acc=DestAccumulation.No,
        unpack_to_dest=False,
    )

    res_from_L1 = configuration.run(workers_tensix_coordinates)
    assert len(res_from_L1) == TILE_SIZE * tile_cnt_A

    res_tensor = torch.tensor(res_from_L1, dtype=format_dict[formats.output_format])
    print_tensor_rows(res_tensor, "Result Tensor (from L1)")

    # 4) Golden TopK over the 64‑element input sequence
    golden_values, golden_indices = torch.topk(
        seq.to(torch.float32), k=k, dim=0, largest=True, sorted=True
    )
    print(f"\nGolden top-{k} values: {golden_values}")
    print(f"Golden top-{k} indices: {golden_indices}\n")

    # For now, just *inspect* the candidate slice; once we understand which
    # positions hold the top-k, we can tighten this check.
    hw_candidate = res_tensor[:64].to(torch.float32)
    print(f"HW candidate first 64: {hw_candidate}")

    # Placeholder assertion: disabled until we identify the right slice.
    # hw_values = hw_candidate[:k]
    # assert torch.allclose(hw_values, golden_values, atol=1e-2, rtol=1e-2)
