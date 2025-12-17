# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0

import pytest
import torch
from conftest import skip_for_blackhole, skip_for_wormhole
from helpers.chip_architecture import ChipArchitecture, get_chip_architecture
from helpers.device import collect_results, write_stimuli_to_l1
from helpers.format_config import DataFormat
from helpers.golden_generators import UnarySFPUGolden, get_golden_generator
from helpers.llk_params import (
    ApproximationMode,
    DestAccumulation,
    MathOperation,
    format_dict,
)
from helpers.param_config import input_output_formats, parametrize
from helpers.stimuli_generator import generate_stimuli
from helpers.test_config import run_test
from helpers.utils import passed_test


@pytest.mark.quasar
@skip_for_blackhole
@skip_for_wormhole
@parametrize(
    test_name="sfpu_rsqrt_quasar_test",
    formats=input_output_formats(
        [
            DataFormat.Float32,
            DataFormat.Float16,
            DataFormat.Float16_b,
        ],
        same=True,
    ),
    approx_mode=[ApproximationMode.No, ApproximationMode.Yes],
    dest_acc=[DestAccumulation.No, DestAccumulation.Yes],
)
def test_sfpu_rsqrt_quasar(test_name, formats, approx_mode, dest_acc):
    """
    Test reciprocal square root (rsqrt) operation on Quasar architecture.

    Uses PyTorch's rsqrt as the golden reference and generates input stimuli
    in the range (0, 1] to match PyTorch's rsqrt behavior.
    """
    # Ensure we're running on Quasar
    chip_arch = get_chip_architecture()
    if chip_arch != ChipArchitecture.QUASAR:
        pytest.skip(f"This test is Quasar-specific, but running on {chip_arch}")

    torch.manual_seed(0)
    torch.set_printoptions(precision=10)
    input_dimensions = [64, 64]

    # Generate stimuli using the standard helper function
    src_A, src_B, tile_cnt = generate_stimuli(
        formats.input_format, formats.input_format, input_dimensions=input_dimensions
    )

    # Ensure input range is (0, 1] for rsqrt
    # Clamp to avoid exactly 0, ensure max is 1.0
    src_A = torch.clamp(src_A.abs(), min=1e-7, max=1.0)

    # Generate golden reference using the UnarySFPUGolden generator
    # This handles format conversions and quantization properly
    # The generator internally uses PyTorch/math rsqrt: 1 / sqrt(x)
    generate_golden = get_golden_generator(UnarySFPUGolden)
    golden_tensor = generate_golden(
        MathOperation.Rsqrt,
        src_A,
        formats.output_format,
        dest_acc,
        formats.input_format,
        input_dimensions,
    )

    unpack_to_dest = (
        formats.input_format.is_32_bit() and dest_acc == DestAccumulation.Yes
    )

    test_config = {
        "formats": formats,
        "testname": test_name,
        "dest_acc": dest_acc,
        "input_A_dimensions": input_dimensions,
        "input_B_dimensions": input_dimensions,
        "mathop": MathOperation.Rsqrt,
        "approx_mode": approx_mode,
        "unpack_to_dest": unpack_to_dest,
        "tile_cnt": tile_cnt,
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

    assert passed_test(golden_tensor, res_tensor, formats.output_format)
