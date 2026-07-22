# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0

# Test for the fused o_norm SFPU LLK kernel (Kimi-K3 / KDA "o_norm" node):
#
#     o_norm = RMSNorm(o) * gamma2 * sigmoid(g_out)
#
# The RMSNorm reduction runs along the row axis (dim=0) of each (D, H) operand
# block; see ckernel_sfpu_o_norm.h for the full tilization contract.

import pytest
import torch
from helpers.format_config import DataFormat
from helpers.golden_generators import ONormGolden, get_golden_generator
from helpers.llk_params import ApproximationMode, DestAccumulation
from helpers.param_config import input_output_formats, parametrize
from helpers.stimuli_config import StimuliConfig
from helpers.test_config import TestConfig
from helpers.test_variant_parameters import APPROX_MODE, O_NORM_CONFIG
from helpers.utils import passed_test

# Head-dim = 32 * NUM_REDUCE_TILES. A single reduce tile keeps all three
# operands (o, gamma2, g_out) resident in Dest at once.
NUM_REDUCE_TILES = 1
EPS = 1e-6
HEAD_DIM = 32 * NUM_REDUCE_TILES  # rows (reduction axis)
NUM_HEADS = 32  # columns


@parametrize(
    formats=input_output_formats(
        [
            DataFormat.Float16_b,
            DataFormat.Float32,
        ],
        same=True,
    ),
    dest_acc=[DestAccumulation.No, DestAccumulation.Yes],
)
def test_o_norm(formats, dest_acc):
    if (
        formats.input == DataFormat.Float32 and formats.output == DataFormat.Float32
    ) and dest_acc == DestAccumulation.No:
        pytest.skip("DataFormat.Float32 not supported with DestAccumulation.No")

    if (
        formats.input == DataFormat.Float16_b and formats.output == DataFormat.Float16_b
    ) and dest_acc == DestAccumulation.Yes:
        pytest.skip("DataFormat.Float16_b not supported with DestAccumulation.Yes")

    torch.manual_seed(0)
    torch_fmt = torch.float32

    # o and g_out are per-(head, head-dim); gamma2 is per head-dim (broadcast
    # across heads).
    o = torch.randn(HEAD_DIM, NUM_HEADS, dtype=torch_fmt)
    gamma2_vec = torch.randn(HEAD_DIM, 1, dtype=torch_fmt)
    gamma2 = gamma2_vec.expand(HEAD_DIM, NUM_HEADS).contiguous()
    g_out = torch.randn(HEAD_DIM, NUM_HEADS, dtype=torch_fmt)

    golden = get_golden_generator(ONormGolden)(o, gamma2, g_out, eps=EPS)

    configuration = TestConfig(
        "sources/o_norm_test.cpp",
        formats,
        templates=[
            APPROX_MODE(ApproximationMode.No),
            O_NORM_CONFIG(num_reduce_tiles=NUM_REDUCE_TILES, eps=EPS),
        ],
        runtimes=[],
        variant_stimuli=StimuliConfig(
            o.flatten(),
            formats.input_format,
            gamma2.flatten(),
            formats.input_format,
            formats.output_format,
            tile_count_A=NUM_REDUCE_TILES,
            tile_count_B=NUM_REDUCE_TILES,
            tile_count_res=NUM_REDUCE_TILES,
            buffer_C=g_out.flatten(),
            stimuli_C_format=formats.input_format,
            tile_count_C=NUM_REDUCE_TILES,
        ),
        unpack_to_dest=formats.input_format.is_32_bit(),
        dest_acc=dest_acc,
        compile_time_formats=True,
    )

    res_from_L1 = configuration.run().result
    res_from_L1 = res_from_L1[: HEAD_DIM * NUM_HEADS]

    golden_tensor = torch.tensor(golden, dtype=torch.float32)
    res_tensor = torch.tensor(res_from_L1, dtype=torch.float32)

    assert passed_test(
        golden_tensor, res_tensor, formats.output_format
    ), "o_norm result does not match golden"
