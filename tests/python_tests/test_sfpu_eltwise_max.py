# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0


import torch
from helpers.device import (
    collect_results,
    write_stimuli_to_l1,
)
from helpers.format_config import DataFormat
from helpers.llk_params import (
    ApproximationMode,
    DestAccumulation,
    format_dict,
)
from helpers.param_config import (
    input_output_formats,
    parametrize,
)
from helpers.test_config import run_test
from helpers.tilize_untilize import tilize
from helpers.utils import passed_test


@parametrize(
    test_name="sfpu_eltwise_max_test",
    formats=input_output_formats(
        [
            # DataFormat.Float32,
            DataFormat.Float16_b,
            # DataFormat.Float16,
        ],
        same=True,
    ),
    dest_acc=[DestAccumulation.No],  # , DestAccumulation.Yes],
    approx_mode=[ApproximationMode.No],
)
def test_sfpu_eltwise_max(
    test_name,
    formats,
    dest_acc,
    approx_mode,
):

    torch_format = format_dict[formats.input_format]

    # Generate random test data for two tiles
    # Tile 0: random values
    tile_0 = torch.randn(1024, dtype=torch_format)

    # Tile 1: random values (different from tile 0)
    tile_1 = torch.randn(1024, dtype=torch_format)

    # DEBUG

    # tile_0 = torch.ones(1024, dtype=torch_format) * 5
    # for i in range(16):
    #     tile_0[i] = i

    # tile_1 = torch.ones(1024, dtype=torch_format) * 3

    # Tilize inputs for hardware
    tile_0_tilized = tilize(tile_0)
    tile_1_tilized = tilize(tile_1)

    # Concatenate both tiles into src_A (TILE_CNT=2)
    src_A = torch.cat([tile_0_tilized, tile_1_tilized])
    src_B = torch.zeros_like(src_A)  # Not used, but needed for write_stimuli_to_l1

    # Generate golden result using torch.maximum (elementwise max)
    golden_result = torch.maximum(tile_0, tile_1)
    golden_tensor = tilize(golden_result)

    test_config = {
        "formats": formats,
        "testname": test_name,
        "dest_acc": dest_acc,
        "approx_mode": approx_mode,
        "unpack_to_dest": True,
        "tile_cnt": 2,
        "disable_format_inference": True,
    }

    # Write both tiles to buffer_A in L1
    res_address = write_stimuli_to_l1(
        test_config,
        src_A,
        src_B,
        formats.input_format,
        formats.input_format,
        tile_count_A=2,
        tile_count_B=1,
    )

    # Run the test
    run_test(test_config)

    # Collect and verify results
    res_from_L1 = collect_results(formats, tile_count=1, address=res_address)
    res_tensor = torch.tensor(res_from_L1, dtype=format_dict[formats.output_format])

    # Compare whole tensors
    print(f"Result tensor: {res_tensor}")
    print(f"Golden tensor: {golden_tensor}")

    assert passed_test(golden_tensor, res_tensor, formats.output_format)
