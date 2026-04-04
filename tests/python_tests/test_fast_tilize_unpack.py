# SPDX-FileCopyrightText: © 2026 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0

"""
Phase 1 test for BH fast-tilize unpack.

Validates that _llk_unpack_fast_tilize_* correctly reads row-major data from L1
and populates SrcA with the expected interleaved layout.

Readback: standard A2D math datacopy + standard pack.

Per dvalid (4 unpack reads → 1 SrcA bank → 1 DEST bank → 1 L1 tile):
  - Each read: 128 datums from one tensor row → 8 SrcA rows
  - CH1_Z stride = 16 rows creates 8-row gaps between reads
  - SrcA bank layout: [8 data, 8 gap] × 4 reads = 64 rows
  - Standard A2D copies all 64 rows to DEST (4 faces × 16 rows)
  - Standard pack writes 1 tile to L1

Output tile per dvalid k:
  face i: rows 0-7 = tensor row (4k+i), cols 0..127; rows 8-15 = gap (zeros)
"""

import pytest
import torch
from helpers.chip_architecture import ChipArchitecture, get_chip_architecture
from helpers.format_config import DataFormat
from helpers.llk_params import DestAccumulation, format_dict
from helpers.param_config import input_output_formats, parametrize
from helpers.stimuli_config import StimuliConfig
from helpers.stimuli_generator import generate_stimuli
from helpers.test_config import TestConfig
from helpers.test_variant_parameters import (
    LOOP_FACTOR,
    NUM_FACES,
    TILE_COUNT,
    generate_input_dim,
)

TILE_R = 32
TILE_C = 32
FACE_C = 16
UNIT_DIM = 4


def generate_phase1_golden(
    src_flat: torch.Tensor,
    input_height_tiles: int,
    input_width_tiles: int,
    output_format: DataFormat,
) -> torch.Tensor:
    """
    Golden for: fast-tilize unpack → standard A2D datacopy → standard pack.

    Output tile k from dvalid k:
      face i (0..3): 16 rows × 16 cols = 256 datums
        rows 0-7:  tensor_row[4*k + i], columns 0..127, packed as 8×16
        rows 8-15: zeros (gap from CH1_Z stride)
    """
    width = input_width_tiles * TILE_C
    src = src_flat.reshape(input_height_tiles * TILE_R, width)

    golden_tiles = []

    for tile_row in range(input_height_tiles):
        for unit_start_tile in range(0, input_width_tiles, UNIT_DIM):
            col_start = unit_start_tile * TILE_C
            col_end = col_start + UNIT_DIM * TILE_C

            for dvalid_idx in range(8):
                tile_datums = []
                for face_idx in range(4):
                    tensor_row_idx = tile_row * TILE_R + dvalid_idx * 4 + face_idx
                    row_data = src[tensor_row_idx, col_start:col_end]

                    # 8 data rows (128 datums from tensor row)
                    for r in range(8):
                        tile_datums.extend(
                            row_data[r * FACE_C : (r + 1) * FACE_C].tolist()
                        )
                    # 8 gap rows (zeros)
                    tile_datums.extend([0.0] * (8 * FACE_C))

                golden_tiles.extend(tile_datums)

    return torch.tensor(golden_tiles, dtype=format_dict[output_format])


@parametrize(
    formats=input_output_formats([DataFormat.Float16_b], same=True),
    dest_acc=[DestAccumulation.No],
    dimensions=[(1, 4)],
)
def test_fast_tilize_unpack(formats, dest_acc, dimensions, workers_tensix_coordinates):
    if get_chip_architecture() != ChipArchitecture.BLACKHOLE:
        pytest.skip("BH only")

    input_height_tiles, input_width_tiles = dimensions
    assert input_width_tiles % 4 == 0, "ct_dim must be divisible by 4"

    input_dimensions = [input_height_tiles * TILE_R, input_width_tiles * TILE_C]

    src_A, tile_cnt_A, src_B, tile_cnt_B = generate_stimuli(
        stimuli_format_A=formats.input_format,
        input_dimensions_A=input_dimensions,
        stimuli_format_B=formats.input_format,
        input_dimensions_B=input_dimensions,
    )

    num_output_tiles = 1  # 1 dvalid = 1 output tile

    golden_tensor = generate_phase1_golden(
        src_A, input_height_tiles, input_width_tiles, formats.output_format
    )[
        :1024
    ]  # trim to 1 tile

    configuration = TestConfig(
        "sources/fast_tilize_phase1_test.cpp",
        formats,
        templates=[],
        runtimes=[
            generate_input_dim(input_dimensions, input_dimensions),
            TILE_COUNT(num_output_tiles),
            LOOP_FACTOR(1),
            NUM_FACES(4),
        ],
        variant_stimuli=StimuliConfig(
            src_A,
            formats.input_format,
            src_B,
            formats.input_format,
            formats.output_format,
            tile_count_A=tile_cnt_A,
            tile_count_B=tile_cnt_B,
            tile_count_res=num_output_tiles,
        ),
        dest_acc=dest_acc,
        compile_time_formats=True,
    )

    res_from_L1 = configuration.run(workers_tensix_coordinates).result

    assert len(res_from_L1) == len(
        golden_tensor
    ), f"Result length {len(res_from_L1)} != golden length {len(golden_tensor)}"

    res_tensor = torch.tensor(res_from_L1, dtype=format_dict[formats.output_format])

    # Validate data rows and gap rows per face
    mismatches = 0
    for face_idx in range(4):
        base = face_idx * 256
        # Data rows (first 8 rows of each face)
        data_slice = slice(base, base + 8 * FACE_C)
        if not torch.equal(res_tensor[data_slice], golden_tensor[data_slice]):
            mismatches += 1
            if mismatches <= 3:
                print(
                    f"Mismatch face {face_idx} data: "
                    f"got {res_tensor[base:base+4].tolist()}, "
                    f"expected {golden_tensor[base:base+4].tolist()}"
                )
        # Gap rows (last 8 rows of each face — should be zeros)
        gap_start = base + 8 * FACE_C
        gap_slice = slice(gap_start, gap_start + 8 * FACE_C)
        if not torch.all(res_tensor[gap_slice] == 0):
            mismatches += 1
            if mismatches <= 3:
                print(
                    f"Non-zero gap face {face_idx}: "
                    f"got {res_tensor[gap_start:gap_start+4].tolist()}"
                )

    assert mismatches == 0, f"{mismatches} mismatches out of {4} faces"
