# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0


import pytest
import torch

from helpers.device import (
    collect_results,
    write_stimuli_to_l1,
)
from helpers.format_arg_mapping import (
    ApproximationMode,
    DestAccumulation,
    MathOperation,
    ReducePool,
    format_dict,
)
from helpers.format_config import DataFormat
from helpers.golden_generators import UnarySFPUGolden, get_golden_generator
from helpers.param_config import (
    input_output_formats,
    parametrize,
)
from helpers.stimuli_generator import generate_random_face, generate_stimuli
from helpers.test_config import run_test
from helpers.tilize_untilize import untilize
from helpers.utils import passed_test


def generate_random_face_with_column_sums_multiple_of_32(
    stimuli_format=DataFormat.Float16_b,
    multiplier=32,
):
    """
    Generate a 16x16 face where each column sum is a multiple of 32.
    This is useful for testing averaging operations that divide by 32.

    Args:
        stimuli_format: The data format to use
        multiplier: The multiplier for column sums (default 32)

    Returns:
        torch.Tensor: 16x16 face with column sums that are multiples of the specified multiplier
    """
    # Create a 16x16 face
    face = torch.zeros((16, 16), dtype=format_dict[stimuli_format])

    if stimuli_format.is_integer():
        # For integer formats, generate random values and adjust to ensure column sums are multiples of 32
        for col in range(16):
            # Generate 15 random values
            random_values = torch.randint(
                low=1, high=10, size=(15,), dtype=format_dict[stimuli_format]
            )
            face[:15, col] = random_values

            # Calculate what the 16th value should be to make the sum a multiple of 32
            current_sum = torch.sum(face[:15, col])
            remainder = current_sum % multiplier
            if remainder != 0:
                # Adjust the last value to make the sum a multiple of 32
                face[15, col] = multiplier - remainder
            else:
                # Sum is already a multiple of 32, set last value to 0
                face[15, col] = 0
    else:
        # For floating point formats, generate values that are already multiples of 32
        # This ensures that when we divide by 32, we get clean integer results
        for col in range(16):
            # Generate 15 random integer values (0-10) and multiply by 32
            random_integers = torch.randint(low=0, high=11, size=(15,))
            face[:15, col] = (
                random_integers.to(dtype=format_dict[stimuli_format]) * multiplier
            )

            # Calculate what the 16th value should be to make the sum a multiple of 32
            current_sum = torch.sum(face[:15, col])
            remainder = current_sum % multiplier
            if remainder != 0:
                # Adjust the last value to make the sum a multiple of 32
                face[15, col] = multiplier - remainder
            else:
                # Sum is already a multiple of 32, set last value to 0
                face[15, col] = 0

    return face.flatten()


@parametrize(
    test_name="eltwise_unary_sfpu_reduce_test",
    formats=input_output_formats(
        [DataFormat.Float32, DataFormat.UInt16, DataFormat.UInt32, DataFormat.Int32],
        same=True,
    ),
    mathop=[MathOperation.ReduceColumn],
    dest_acc=[DestAccumulation.No, DestAccumulation.Yes],
    negative_number=[False, True],
    reduce_pool=[ReducePool.Average, ReducePool.Sum],
)
def test_eltwise_unary_sfpu_column_sum(
    test_name, formats, dest_acc, mathop, reduce_pool, negative_number
):
    if negative_number and formats.input_format in [
        DataFormat.UInt16,
        DataFormat.UInt32,
    ]:
        pytest.skip(
            f"Skipping negative_numbers=True for unsigned format {formats.input_format}"
        )

    input_dimensions = [32, 32]
    torch_format = format_dict[formats.input_format]
    generate_face_function = generate_random_face
    if reduce_pool == ReducePool.Average:
        generate_face_function = generate_random_face_with_column_sums_multiple_of_32  # when averaging sum columns, sum must be multiple of 32 (num rows per column)
    src_A, src_B, tile_cnt = generate_stimuli(
        formats.input_format, formats.input_format, input_dimensions=input_dimensions
    )

    # Generate 4 faces with all 2s for easy verification (column sums = 32*2 = 64, which is a multiple of 32)
    face0 = generate_face_function(formats.input_format)
    face1 = generate_face_function(formats.input_format)
    face2 = generate_face_function(formats.input_format)
    face3 = generate_face_function(formats.input_format)

    # Stack all 4 faces to create the complete 32x32 tile
    src_A = torch.stack([face0, face1, face2, face3]).flatten()

    # Apply negative flag if requested
    if negative_number:
        src_A = src_A * -1

    generate_golden = get_golden_generator(UnarySFPUGolden)
    golden_tensor = generate_golden(
        mathop,
        src_A,
        formats.output_format,
        dest_acc,
        formats.input_format,
        reduce_pool,
    )

    test_config = {
        "formats": formats,
        "testname": test_name,
        "dest_acc": dest_acc,
        "input_A_dimensions": input_dimensions,
        "input_B_dimensions": input_dimensions,
        "mathop": mathop,
        "pool_type": reduce_pool,
        "approx_mode": ApproximationMode.No,
        "unpack_to_dest": True,
        "tile_cnt": 1,
    }

    res_address = write_stimuli_to_l1(
        test_config,
        src_A,
        src_B,
        formats.input_format,
        formats.input_format,
        tile_count_A=1,
        tile_count_B=1,
    )
    run_test(test_config)

    torch_format = format_dict[formats.output_format]
    res_from_L1 = collect_results(formats, tile_count=1, address=res_address)

    res_tensor = torch.tensor(res_from_L1, dtype=torch_format)

    res_tensor = untilize(res_tensor, formats.output_format).flatten()[:32]

    # For column sum, we only compare the first 32 elements (column sums)
    assert len(res_tensor) == len(golden_tensor)

    assert passed_test(golden_tensor, res_tensor, formats.output_format)
