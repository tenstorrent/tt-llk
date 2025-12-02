# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0

from typing import List

import pytest
import torch
from helpers.device import (
    collect_results,
    write_stimuli_to_l1,
)
from helpers.format_config import DataFormat, FormatConfig
from helpers.golden_generators import (
    DataCopyGolden,
    get_golden_generator,
)
from helpers.llk_params import DestAccumulation, ImpliedMathFormat, format_dict
from helpers.param_config import (
    generate_unary_input_dimensions,
    input_output_formats,
    parametrize,
)
from helpers.stimuli_generator import generate_stimuli
from helpers.test_config import BootMode, run_test
from helpers.utils import passed_test


def generate_qsr_pack_combinations(
    formats_list: List[FormatConfig],
):
    """
    Generate pack combinations for Quasar pack tests.

    Args:
        formats_list: List of input/output format pairs

    Returns:
        List of (format, dest_acc, input_dimensions) tuples
    """

    def is_supported_format_conversion(in_fmt, out_fmt):
        """Check if the format conversion is supported by packer. These format conversions are NOT dependent on the dest register mode."""
        # Skip if mixing integer and non-integer formats
        if in_fmt.is_integer() ^ out_fmt.is_integer():
            return False
        # If input format is Int16, output format must also be Int16, and vice versa
        if (in_fmt == DataFormat.Int16) ^ (out_fmt == DataFormat.Int16):
            return False
        return True

    def get_dest_acc_modes(in_fmt):
        """Determine valid dest register modes depending on the input format."""
        # Having Int16 in src registers and Int32 in the dest register is not supported
        if in_fmt == DataFormat.Int16:
            return (DestAccumulation.No,)
        if in_fmt.is_32_bit():
            return (DestAccumulation.Yes,)
        return (DestAccumulation.No, DestAccumulation.Yes)

    def is_supported_dest_mode_dependent_conversion(in_fmt, out_fmt, dest_acc):
        """Check if the format conversion is supported by packer. These format conversions are dependent on the dest register mode."""
        # Upcasting to Float32/Int32 requires dest_acc enabled
        if (
            out_fmt.is_32_bit()
            and not in_fmt.is_32_bit()
            and dest_acc == DestAccumulation.No
        ):
            return False
        # Int8<->UInt8 conversion requires dest_acc enabled
        if (
            dest_acc == DestAccumulation.No
            and in_fmt in (DataFormat.Int8, DataFormat.UInt8)
            and in_fmt != out_fmt
        ):
            return False
        return True

    dimensions_cache = {
        DestAccumulation.No: tuple(
            generate_unary_input_dimensions(DestAccumulation.No)
        ),
        DestAccumulation.Yes: tuple(
            generate_unary_input_dimensions(DestAccumulation.Yes)
        ),
    }

    combinations = []
    for fmt in formats_list:
        in_fmt, out_fmt = fmt.input_format, fmt.output_format

        if not is_supported_format_conversion(in_fmt, out_fmt):
            continue

        for dest_acc in get_dest_acc_modes(in_fmt):
            if is_supported_dest_mode_dependent_conversion(in_fmt, out_fmt, dest_acc):
                for dimensions in dimensions_cache[dest_acc]:
                    combinations.append((fmt, dest_acc, dimensions))

    return combinations


PACK_FORMATS = input_output_formats(
    [
        DataFormat.Float16_b,
        DataFormat.Float16,
        DataFormat.Float32,
        DataFormat.Int32,
        DataFormat.Int8,
        DataFormat.UInt8,
        DataFormat.Int16,
    ]
)
ALL_PACK_COMBINATIONS = generate_qsr_pack_combinations(PACK_FORMATS)


@pytest.mark.quasar
@parametrize(
    test_name="pack_quasar_test",
    formats_dest_acc_input_dims=ALL_PACK_COMBINATIONS,
)
def test_pack_quasar(
    test_name, formats_dest_acc_input_dims, boot_mode=BootMode.DEFAULT
):
    formats, dest_acc, input_dimensions = formats_dest_acc_input_dims

    src_A, src_B, tile_cnt = generate_stimuli(
        formats.input_format,
        formats.input_format,
        input_dimensions=input_dimensions,
    )

    generate_golden = get_golden_generator(DataCopyGolden)
    golden_tensor = generate_golden(
        src_A, formats.output_format, num_faces=4, input_dimensions=input_dimensions
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
        "unpack_to_dest": unpack_to_dest,
        "tile_cnt": tile_cnt,
        "implied_math_format": ImpliedMathFormat.Yes,
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

    run_test(test_config, boot_mode=boot_mode)

    res_from_L1 = collect_results(
        formats, tile_count=tile_cnt, address=res_address, num_faces=4
    )

    assert len(res_from_L1) == len(golden_tensor)

    torch_format = format_dict[formats.output_format]
    res_tensor = torch.tensor(res_from_L1, dtype=torch_format)

    assert passed_test(golden_tensor, res_tensor, formats.output_format)
