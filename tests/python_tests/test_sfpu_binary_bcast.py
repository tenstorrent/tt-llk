# SPDX-FileCopyrightText: © 2026 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0

from dataclasses import dataclass
from enum import Enum

import torch
from conftest import skip_for_blackhole, skip_for_quasar
from helpers.format_config import DataFormat, InputOutputFormat
from helpers.golden_generators import BinarySFPUGolden, get_golden_generator
from helpers.llk_params import (
    ApproximationMode,
    DestAccumulation,
    MathOperation,
    format_dict,
)
from helpers.param_config import (
    input_output_formats,
    parametrize,
)
from helpers.stimuli_config import StimuliConfig
from helpers.stimuli_generator import generate_stimuli
from helpers.test_config import TestConfig
from helpers.test_variant_parameters import (
    APPROX_MODE,
    MATH_OP,
    TemplateParameter,
)
from helpers.utils import passed_test


class SfpuBcastDim(Enum):
    BCAST_COL = 0
    BCAST_ROW = 1


@dataclass
class SFPU_BCAST_DIM(TemplateParameter):
    bcast_dim: SfpuBcastDim

    def convert_to_cpp(self) -> str:
        return f"constexpr std::uint32_t BCAST_DIM_VAL = {self.bcast_dim.value};"


@dataclass
class DST_INDEXES(TemplateParameter):
    dst_index_data: int = 0
    dst_index_bcast: int = 1

    def convert_to_cpp(self) -> str:
        lines = [
            f"constexpr std::uint32_t DST_INDEX_DATA_VAL = {self.dst_index_data};",
            f"constexpr std::uint32_t DST_INDEX_BCAST_VAL = {self.dst_index_bcast};",
        ]
        return "\n".join(lines)


# Face layout in a 32×32 row-major tile:
#   Face 0: rows  0-15, cols  0-15
#   Face 1: rows  0-15, cols 16-31
#   Face 2: rows 16-31, cols  0-15
#   Face 3: rows 16-31, cols 16-31
_FACE_RANGES = [
    (slice(0, 16), slice(0, 16)),
    (slice(0, 16), slice(16, 32)),
    (slice(16, 32), slice(0, 16)),
    (slice(16, 32), slice(16, 32)),
]


def _prepare_bcast_tile(
    src_bcast: torch.Tensor,
    bcast_dim: SfpuBcastDim,
) -> torch.Tensor:
    """Prepare the broadcast tile in row-major 32×32 space.

    BCAST_ROW: replicate row 0 of each face across all rows of that face.
    BCAST_COL: replicate column 0 of each face across all columns of that face.
    """
    tile = src_bcast.flatten()[:1024].clone().reshape(32, 32)

    for row_sl, col_sl in _FACE_RANGES:
        face = tile[row_sl, col_sl]
        if bcast_dim == SfpuBcastDim.BCAST_ROW:
            tile[row_sl, col_sl] = face[0].unsqueeze(0).expand_as(face)
        else:
            tile[row_sl, col_sl] = face[:, 0].unsqueeze(1).expand_as(face)

    return tile.flatten()


SUPPORTED_ELTWISE_OPS = [
    MathOperation.SfpuElwadd,
    MathOperation.SfpuElwsub,
    # MathOperation.SfpuElwmul,
]

SUPPORTED_FORMATS = input_output_formats(
    [
        DataFormat.Float32,
    ]
)


@skip_for_blackhole
@skip_for_quasar
@parametrize(
    formats=SUPPORTED_FORMATS,
    bcast_dim=[SfpuBcastDim.BCAST_COL, SfpuBcastDim.BCAST_ROW],
    eltwise_op=SUPPORTED_ELTWISE_OPS,
    dest_acc=[DestAccumulation.Yes],
)
def test_sfpu_binary_bcast(
    formats: InputOutputFormat,
    bcast_dim: SfpuBcastDim,
    eltwise_op: MathOperation,
    dest_acc: DestAccumulation,
    workers_tensix_coordinates: str,
):
    torch.manual_seed(42)
    torch.set_printoptions(precision=10)

    input_dimensions = [64, 32]

    src_data, _, _, _ = generate_stimuli(
        stimuli_format_A=formats.input_format,
        input_dimensions_A=input_dimensions,
        stimuli_format_B=formats.input_format,
        input_dimensions_B=input_dimensions,
    )

    torch.manual_seed(123)
    src_bcast_raw, _, _, _ = generate_stimuli(
        stimuli_format_A=formats.input_format,
        input_dimensions_A=input_dimensions,
        stimuli_format_B=formats.input_format,
        input_dimensions_B=input_dimensions,
    )

    src_bcast = _prepare_bcast_tile(src_bcast_raw, bcast_dim)

    # Pack both tiles into a single 2-tile row-major buffer (tile 0 = data, tile 1 = bcast).
    # generate_stimuli with [64,32] already gives 2048 elements; overwrite tile 1.
    combined_src = src_data.flatten()[:1024].clone()
    combined_src = torch.cat([combined_src, src_bcast[:1024]])

    generate_golden = get_golden_generator(BinarySFPUGolden)
    golden_full = generate_golden(
        eltwise_op,
        combined_src,
        0,
        1,
        0,
        32,
        input_dimensions,
        formats.output_format,
    ).flatten()
    golden_tensor = golden_full[:1024]

    unpack_to_dest = (
        formats.input_format.is_32_bit() and dest_acc == DestAccumulation.Yes
    )

    dummy_B = torch.zeros(1024, dtype=format_dict[formats.input_format])

    configuration = TestConfig(
        "sources/sfpu_binary_bcast_test.cpp",
        formats,
        templates=[
            APPROX_MODE(ApproximationMode.No),
            MATH_OP(mathop=eltwise_op),
            SFPU_BCAST_DIM(bcast_dim),
            DST_INDEXES(dst_index_data=0, dst_index_bcast=1),
        ],
        runtimes=[],
        variant_stimuli=StimuliConfig(
            combined_src,
            formats.input_format,
            dummy_B,
            formats.input_format,
            formats.output_format,
            tile_count_A=2,
            tile_count_B=1,
            tile_count_res=1,
        ),
        dest_acc=dest_acc,
        unpack_to_dest=unpack_to_dest,
    )

    res_from_L1 = configuration.run(workers_tensix_coordinates).result

    assert len(res_from_L1) == len(
        golden_tensor
    ), f"Result ({len(res_from_L1)}) and golden ({len(golden_tensor)}) size mismatch"

    torch_format = format_dict[formats.output_format]
    res_tensor = torch.tensor(res_from_L1, dtype=torch_format).flatten()

    assert passed_test(
        golden_tensor, res_tensor, formats.output_format
    ), "Assert against golden failed"
