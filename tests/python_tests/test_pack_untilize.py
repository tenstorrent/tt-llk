# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0

import pytest
import torch
from helpers.data_format_inference import infer_data_formats
from helpers.debug_utils import dump_test_failure
from helpers.device import collect_results, write_stimuli_to_l1
from helpers.format_config import DataFormat
from helpers.golden_generators import UntilizeGolden, get_golden_generator
from helpers.llk_params import DestAccumulation, DstSync, format_dict
from helpers.param_config import (
    input_output_formats,
    parametrize,
)
from helpers.stimuli_generator import generate_stimuli
from helpers.test_config import run_test
from helpers.utils import passed_test


def get_block_ct_dim(full_ct_dim, dest_acc):
    max_bct = 4 if dest_acc == DestAccumulation.Yes else 8

    for bct in range(
        max_bct, 0, -1
    ):  # range(start, stop, step) - goes from max_bct down to 1
        if full_ct_dim % bct == 0:
            return bct

    return 1


@parametrize(
    test_name="pack_untilize_test",
    formats=input_output_formats(
        [
            DataFormat.Float16_b,
            DataFormat.Float16,
            DataFormat.Float32,  # Test Float32 with both 32bit mode dest (full precision) and 16bit mode dest (precision loss)
            DataFormat.Int32,
            DataFormat.Bfp8_b,
        ]  # Pack Untilize doesn't work for block float formats (Bfp8_b); we only include as input format in our test
    ),
    dest_acc=[DestAccumulation.No, DestAccumulation.Yes],
    input_dimensions=[[64, 288]],
    dst_sync=[DstSync.SyncHalf, DstSync.SyncFull],
)
def test_pack_untilize(test_name, formats, dest_acc, input_dimensions, dst_sync):
    if formats.output_format == DataFormat.Bfp8_b:
        pytest.skip("Pack Untilize does not support Bfp8_b format")

    if (formats.input_format == DataFormat.Int32) ^ (
        formats.output_format == DataFormat.Int32
    ):
        pytest.skip("Pack Untilize does not support mixing Int32 with other formats")

    if (
        formats.input_format == DataFormat.Int32
        and formats.output_format == DataFormat.Int32
        and dest_acc == DestAccumulation.No
    ):
        pytest.skip("Dest must be in 32bit mode when input and output are Int32")

    data_formats = infer_data_formats(
        formats.input_format,
        formats.output_format,
        dest_acc,
        False,
    )

    # Handling a hardware limitation: cannot convert 8-bit exponent datums to Float16 without storing them as intermediate Float32 in dest register.
    # For wormhole architecture, gasket cannot perform this conversion and packer takes input Float32 (from dest register) converting to Float16_A.
    # For blackhole architecture, gasket able to convert Float32 to Float16_A before packing (reduces work on packer).`
    if (
        formats.input_format == DataFormat.Float16
        and data_formats.pack_src.is_32_bit()
        and dest_acc == DestAccumulation.No
    ):
        pytest.skip(
            "Due to hardware limitation, cannot convert 8-bit exponent datums to Float16 without storing them as intermediate Float32 in dest register. Therefore using dest_acc=No is not supported in this case."
        )

    src_A, src_B, tile_cnt = generate_stimuli(
        formats.input_format, formats.input_format, input_dimensions=input_dimensions
    )

    generate_golden = get_golden_generator(UntilizeGolden)
    golden_tensor = generate_golden(src_A, formats.output_format, input_dimensions)

    test_config = {
        "formats": formats,
        "testname": test_name,
        "tile_cnt": tile_cnt,
        "input_A_dimensions": input_dimensions,
        "input_B_dimensions": input_dimensions,
        "unpack_to_dest": False,
        "dest_acc": dest_acc,
        "dst_sync": dst_sync,
        "block_ct_dim": get_block_ct_dim(input_dimensions[1] // 32, dest_acc),
        "num_faces": 4,
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

    res_from_L1 = collect_results(formats, tile_count=tile_cnt, address=res_address)
    assert len(res_from_L1) == len(golden_tensor)

    res_tensor = torch.tensor(res_from_L1, dtype=format_dict[formats.output_format])

    test_passed = passed_test(golden_tensor, res_tensor, formats.output_format)

    if not test_passed:
        # Generate comprehensive debug output for test failure analysis
        dump_test_failure(
            test_name=test_name,
            golden_tensor=golden_tensor,
            result_tensor=res_tensor,
            test_params={
                "formats": str(formats),
                "input_format": str(formats.input_format),
                "output_format": str(formats.output_format),
                "dest_acc": str(dest_acc),
                "input_dimensions": input_dimensions,
                "dst_sync": str(dst_sync),
                "tile_cnt": tile_cnt,
                "src_A_shape": (
                    list(src_A.shape) if hasattr(src_A, "shape") else len(src_A)
                ),
                "src_B_shape": (
                    list(src_B.shape) if hasattr(src_B, "shape") else len(src_B)
                ),
                "data_formats": str(data_formats),
            },
        )

    assert test_passed
