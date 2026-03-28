# SPDX-FileCopyrightText: © 2026 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0

from dataclasses import dataclass

import pytest
import torch
from helpers.format_config import DataFormat
from helpers.golden_generators import (
    MatmulGolden,
    TransposeGolden,
    get_golden_generator,
)
from helpers.llk_params import (
    DestAccumulation,
    DestSync,
    MathFidelity,
    Transpose,
    format_dict,
)
from helpers.param_config import input_output_formats
from helpers.stimuli_config import StimuliConfig
from helpers.stimuli_generator import convert_to_l1_view, generate_face_matmul_data
from helpers.test_config import TestConfig
from helpers.test_variant_parameters import (
    CRK_TILE_DIMM,
    DEST_INDEX,
    DEST_SYNC,
    IN_TILE_DIMS,
    MATH_FIDELITY,
    NUM_FACES,
    PARTIAL_FACE,
    THROTTLE_LEVEL,
    TILE_COUNT,
    UNPACK_TRANS_FACES,
    UNPACK_TRANS_WITHIN_FACE,
    RuntimeParameter,
    TemplateParameter,
)
from helpers.tilize_untilize import tilize_block
from helpers.utils import passed_test


class MATMUL_CUSTOM_NO_MOP(TemplateParameter):
    def convert_to_cpp(self) -> str:
        return "#define USE_MATMUL_CUSTOM_NO_MOP 1"


@dataclass
class IN1_STRIDE_TILES(RuntimeParameter):
    in1_stride_tiles: int

    def convert_to_cpp(self) -> str:
        return f"constexpr std::uint32_t IN1_STRIDE_TILES = {self.in1_stride_tiles};"

    def convert_to_struct_fields(self) -> tuple[str, str]:
        return "std::uint32_t IN1_STRIDE_TILES;", "I"


def embed_tiles_at_positions(tilized_tiles, total_tiles, positions):
    tile_elements = 32 * 32
    flat_tiles = (
        list(tilized_tiles.flatten())
        if hasattr(tilized_tiles, "flatten")
        else list(tilized_tiles)
    )
    full_buffer = [0.0] * (total_tiles * tile_elements)
    for tile_idx, position in enumerate(positions):
        src_offset = tile_idx * tile_elements
        dst_offset = position * tile_elements
        full_buffer[dst_offset : dst_offset + tile_elements] = flat_tiles[
            src_offset : src_offset + tile_elements
        ]
    return torch.tensor(full_buffer)


def flatten_values(values):
    if hasattr(values, "flatten"):
        flattened = values.flatten()
        return flattened if hasattr(flattened, "cpu") else torch.tensor(list(flattened))
    return torch.tensor(list(values))


TARGET_FORMATS = input_output_formats([DataFormat.Float16_b])[0]
TEST_CASES = [
    pytest.param(
        {
            "name": "sdpa_qkt_rt2_ct4",
            "in0_dimensions": [64, 128],
            "in1_dimensions": [128, 128],
            "golden_in1_dimensions": [128, 128],
            "tile_count": 8,
            "tile_count_a": 8,
            "tile_count_b": 16,
            "ct_dim": 4,
            "rt_dim": 2,
            "kt_dim": 4,
            "dest_sync": DestSync.Half,
            "in1_stride_tiles": 4,
            "in1_tile_positions": list(range(16)),
        },
        id="sdpa_qkt_rt2_ct4",
    ),
    pytest.param(
        {
            "name": "full_rt4_ct4",
            "in0_dimensions": [128, 128],
            "in1_dimensions": [128, 128],
            "golden_in1_dimensions": [128, 128],
            "tile_count": 16,
            "tile_count_a": 16,
            "tile_count_b": 16,
            "ct_dim": 4,
            "rt_dim": 4,
            "kt_dim": 4,
            "dest_sync": DestSync.Full,
            "in1_stride_tiles": 4,
            "in1_tile_positions": list(range(16)),
        },
        id="full_rt4_ct4",
    ),
    pytest.param(
        {
            "name": "sdpa_qkt_padded_rt2_ct1_stride4",
            "in0_dimensions": [64, 128],
            "in1_dimensions": [128, 128],
            "golden_in1_dimensions": [128, 32],
            "tile_count": 2,
            "tile_count_a": 8,
            "tile_count_b": 16,
            "ct_dim": 1,
            "rt_dim": 2,
            "kt_dim": 4,
            "dest_sync": DestSync.Half,
            "in1_stride_tiles": 4,
            "in1_tile_positions": [0, 4, 8, 12],
        },
        id="sdpa_qkt_padded_rt2_ct1_stride4",
    ),
]


@pytest.mark.parametrize(
    "math_fidelity",
    [
        MathFidelity.LoFi,
        MathFidelity.HiFi2,
        MathFidelity.HiFi3,
        MathFidelity.HiFi4,
    ],
    ids=lambda value: f"math_fidelity:{value.name}",
)
@pytest.mark.parametrize("case", TEST_CASES)
def test_matmul_custom_transpose(math_fidelity, case, workers_tensix_coordinates):
    in0_dimensions = case["in0_dimensions"]
    in1_dimensions = case["in1_dimensions"]
    golden_in1_dimensions = case["golden_in1_dimensions"]
    golden_in1_tile_count = (golden_in1_dimensions[0] * golden_in1_dimensions[1]) // (
        32 * 32
    )
    transpose = Transpose.Yes

    in0 = generate_face_matmul_data(
        num_faces=4,
        stimuli_format=TARGET_FORMATS.input_format,
        input_dimensions=in0_dimensions,
        is_matrix_A=True,
        face_r_dim=16,
    )
    in1 = generate_face_matmul_data(
        num_faces=4,
        stimuli_format=TARGET_FORMATS.input_format,
        input_dimensions=golden_in1_dimensions,
        is_matrix_A=False,
    )

    transpose_golden = get_golden_generator(TransposeGolden)
    in1_golden = transpose_golden.transpose_faces_multi_tile(
        in1,
        TARGET_FORMATS.input_format,
        num_tiles=golden_in1_tile_count,
        tilize=True,
        input_dimensions=golden_in1_dimensions,
    )
    in1_golden = transpose_golden.transpose_within_faces_multi_tile(
        in1_golden,
        TARGET_FORMATS.input_format,
        num_tiles=golden_in1_tile_count,
        untilize=True,
        input_dimensions=golden_in1_dimensions,
    )

    matmul_golden = get_golden_generator(MatmulGolden)
    golden_tensor = matmul_golden(
        in0,
        in1_golden,
        TARGET_FORMATS.output_format,
        math_fidelity,
        input_A_dimensions=in0_dimensions,
        input_B_dimensions=golden_in1_dimensions,
        tilize=True,
        input_A_format=TARGET_FORMATS.input_format,
        input_B_format=TARGET_FORMATS.input_format,
    )

    tilized_in0 = tilize_block(
        in0, dimensions=in0_dimensions, stimuli_format=TARGET_FORMATS.input_format
    )
    tilized_in1 = tilize_block(
        in1,
        dimensions=golden_in1_dimensions,
        stimuli_format=TARGET_FORMATS.input_format,
    )
    tilized_in0_l1_view = convert_to_l1_view(
        tilized_in0, in0_dimensions, tile_dimensions=[32, 32]
    )
    tilized_in1_l1_view = embed_tiles_at_positions(
        tilized_in1, case["tile_count_b"], case["in1_tile_positions"]
    )

    configuration = TestConfig(
        "sources/matmul_custom_transpose_test.cpp",
        TARGET_FORMATS,
        templates=[
            MATMUL_CUSTOM_NO_MOP(),
            MATH_FIDELITY(math_fidelity),
            DEST_SYNC(case["dest_sync"]),
            THROTTLE_LEVEL(0),
        ],
        runtimes=[
            TILE_COUNT(case["tile_count"]),
            NUM_FACES(4, 4, 4),
            UNPACK_TRANS_FACES(transpose),
            UNPACK_TRANS_WITHIN_FACE(transpose),
            IN1_STRIDE_TILES(case["in1_stride_tiles"]),
            PARTIAL_FACE(
                partial_a=False,
                partial_face_pack=False,
                partial_b=False,
                partial_face_math=False,
            ),
            CRK_TILE_DIMM(case["ct_dim"], case["rt_dim"], case["kt_dim"]),
            IN_TILE_DIMS(32, 32, 32, 32),
            DEST_INDEX(0),
        ],
        variant_stimuli=StimuliConfig(
            flatten_values(tilized_in0_l1_view),
            TARGET_FORMATS.input_format,
            flatten_values(tilized_in1_l1_view),
            TARGET_FORMATS.input_format,
            TARGET_FORMATS.output_format,
            tile_count_A=case["tile_count_a"],
            tile_count_B=case["tile_count_b"],
            tile_count_res=case["tile_count"],
        ),
        dest_acc=DestAccumulation.No,
    )

    res_from_l1 = configuration.run(workers_tensix_coordinates).result

    assert len(res_from_l1) == len(golden_tensor)

    res_tensor = torch.tensor(
        res_from_l1, dtype=format_dict[TARGET_FORMATS.output_format]
    )
    golden_l1 = convert_to_l1_view(
        golden_tensor,
        (in0_dimensions[0], in1_dimensions[1]),
        tile_dimensions=[32, 32],
    )

    assert passed_test(golden_l1, res_tensor, TARGET_FORMATS.output_format)
