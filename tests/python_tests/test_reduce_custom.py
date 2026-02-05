# SPDX-FileCopyrightText: © 2026 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0


from dataclasses import dataclass

import numpy as np
import torch
from helpers.device import BootMode
from helpers.format_config import DataFormat
from helpers.golden_generators import get_golden_generator
from helpers.llk_params import DestAccumulation, MathFidelity, format_dict
from helpers.param_config import input_output_formats, parametrize
from helpers.stimuli_config import StimuliConfig
from helpers.stimuli_generator import generate_stimuli
from helpers.test_config import TestConfig
from helpers.test_variant_parameters import (
    MATH_FIDELITY,
    NUM_FACES,
    TILE_COUNT,
    RuntimeParameter,
)
from helpers.tilize_untilize import tilize_block
from helpers.utils import passed_test


class ReduceCustomGolden:
    """Golden model for reduce_custom (max row reduction)"""

    @staticmethod
    def generate(
        src_A,
        src_B,
        output_format,
        math_fidelity,
        input_dimensions,
        tilize=False,
        **kwargs,
    ):
        """
        Perform max row reduction on input data.
        Input: 64 rows x 128 cols (2 tiles high x 4 tiles wide)
        Output: 64 rows x 32 cols (2 tiles, each representing max of one row of input tiles)

        For each of the 64 rows, we take the max across all 128 columns,
        and replicate it in the first column of the output tile.
        """
        rows = input_dimensions[0]
        cols = input_dimensions[1]

        # Reshape input to rows x cols
        src_array = np.array(src_A, dtype=np.float32).reshape(rows, cols)

        # Calculate max for each row across all columns
        row_max = np.max(src_array, axis=1, keepdims=True)  # Shape: (64, 1)

        # Create output: 64 rows x 32 cols
        # First column contains the max value, rest are zeros (only first column matters)
        output = np.zeros((rows, 32), dtype=np.float32)
        output[:, 0] = row_max.squeeze()

        # Convert to list
        result = output.flatten().tolist()

        if tilize:
            # Tilize the output
            result_tensor = torch.tensor(
                result, dtype=format_dict[output_format]
            ).reshape(rows, 32)
            tilized = tilize_block(
                result_tensor.flatten().tolist(),
                dimensions=[rows, 32],
                stimuli_format=output_format,
            )
            return tilized

        return torch.tensor(result, dtype=format_dict[output_format])


# Only Float16_b format for this test
REDUCE_FORMATS = input_output_formats(
    [
        DataFormat.Float16_b,
    ]
)
DEST_ACC_MODES = [DestAccumulation.No, DestAccumulation.Yes]


@dataclass
class TILE_COUNT_INPUT(RuntimeParameter):
    """Custom runtime parameter for input tile count"""

    tile_cnt_input: int = 0

    def covert_to_cpp(self) -> str:
        return f"constexpr int TILE_CNT_INPUT = {self.tile_cnt_input};"

    def convert_to_struct_fields(self) -> tuple[str, str]:
        return "int TILE_CNT_INPUT;", "i"


@parametrize(
    math_fidelity=[
        MathFidelity.LoFi,
        MathFidelity.HiFi2,
    ],
    format_and_dest_acc=[
        (fmt, dest_acc) for fmt in REDUCE_FORMATS for dest_acc in DEST_ACC_MODES
    ],
)
def test_reduce_custom(
    math_fidelity,
    format_and_dest_acc,
    workers_tensix_coordinates,
    boot_mode=BootMode.DEFAULT,
):
    """
    Test reduce_custom functionality with:
    - Input: 64 elements in height, 128 in width (2x4 tiles)
    - Operation: Row-wise max reduction
    - Output: 2 tiles (tile 0 of first row, tile 0 of second row)
    """
    torch_format = format_dict[format_and_dest_acc[0].output_format]

    formats = format_and_dest_acc[0]
    dest_acc = format_and_dest_acc[1]

    # Input dimensions: 64 rows x 128 cols (2 tiles high x 4 tiles wide)
    input_dimensions = [64, 128]

    # Generate input stimuli
    src_A, tile_cnt_A, src_B, tile_cnt_B = generate_stimuli(
        stimuli_format_A=formats.input_format,
        input_dimensions_A=input_dimensions,
        stimuli_format_B=formats.input_format,
        input_dimensions_B=[32, 32],  # Scale tile (1.0)
        sfpu=False,
    )

    # Override src_B to be all 1.0 (identity scale)
    src_B = [1.0] * (32 * 32)

    # Generate golden output
    generate_golden = get_golden_generator(ReduceCustomGolden)
    golden_tensor = generate_golden(
        src_A,
        src_B,
        formats.output_format,
        math_fidelity,
        input_dimensions=input_dimensions,
        tilize=True,
    )

    # Tilize input
    if formats.input_format != DataFormat.Bfp8_b:
        tilized_A = tilize_block(
            src_A, dimensions=input_dimensions, stimuli_format=formats.input_format
        )
        tilized_B = tilize_block(
            src_B, dimensions=[32, 32], stimuli_format=formats.input_format
        )
    else:
        tilized_A = src_A
        tilized_B = src_B

    # Output: 2 tiles (one for each row of input tiles)
    output_tile_cnt = 2

    configuration = TestConfig(
        "sources/reduce_custom_test.cpp",
        formats,
        templates=[
            MATH_FIDELITY(math_fidelity),
        ],
        runtimes=[
            NUM_FACES(),
            TILE_COUNT(output_tile_cnt),
            TILE_COUNT_INPUT(tile_cnt_A),
        ],
        variant_stimuli=StimuliConfig(
            tilized_A.flatten(),
            formats.input_format,
            tilized_B.flatten(),
            formats.input_format,
            formats.output_format,
            tile_count_A=tile_cnt_A,
            tile_count_B=1,  # Scale tile
            tile_count_res=output_tile_cnt,
        ),
        dest_acc=dest_acc,
        boot_mode=boot_mode,
    )

    res_from_L1 = configuration.run(workers_tensix_coordinates)

    assert len(res_from_L1) == len(
        golden_tensor
    ), f"Result tensor and golden tensor are not of the same length: {len(res_from_L1)} vs {len(golden_tensor)}"

    res_tensor = torch.tensor(res_from_L1, dtype=torch_format)

    assert passed_test(
        golden_tensor, res_tensor, formats.output_format
    ), "Assert against golden failed"
