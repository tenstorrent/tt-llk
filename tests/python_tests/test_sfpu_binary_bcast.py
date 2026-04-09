# SPDX-FileCopyrightText: © 2026 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0

from dataclasses import dataclass
from enum import Enum

import torch
from conftest import skip_for_blackhole, skip_for_quasar
from helpers.format_config import DataFormat, InputOutputFormat
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
from helpers.tilize_untilize import untilize
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


_FACE_R_DIM = 16
_FACE_C_DIM = 16
_NUM_FACES = 4
_ELEMENTS_PER_FACE = _FACE_R_DIM * _FACE_C_DIM

_BINARY_OPS = {
    MathOperation.SfpuElwadd: torch.add,
    MathOperation.SfpuElwsub: torch.sub,
    MathOperation.SfpuElwmul: torch.mul,
}


def _prepare_bcast_tile(
    src_bcast: torch.Tensor,
    bcast_dim: SfpuBcastDim,
) -> torch.Tensor:
    """Prepare the broadcast tile in row-major 32×32 space.

    BCAST_ROW: replicate row 0 across all 32 rows of the tile.
    BCAST_COL: replicate column 0 across all 32 columns of the tile.
    """
    tile = src_bcast.flatten()[:1024].clone().reshape(32, 32)

    if bcast_dim == SfpuBcastDim.BCAST_ROW:
        tile = tile[0].unsqueeze(0).expand_as(tile)
    else:
        tile = tile[:, 0].unsqueeze(1).expand_as(tile)

    return tile.contiguous().flatten()


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
    bcast_dim=[SfpuBcastDim.BCAST_ROW, SfpuBcastDim.BCAST_COL],
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
    input_dimensions = [32, 32]

    src_A, tile_cnt_A, src_B, tile_cnt_B = generate_stimuli(
        stimuli_format_A=formats.input_format,
        input_dimensions_A=input_dimensions,
        stimuli_format_B=formats.input_format,
        input_dimensions_B=input_dimensions,
    )

    # Debug overrides — comment out to use random stimuli
    src_A = torch.ones((32, 32), dtype=src_A.dtype) * 5

    src_B = torch.zeros((32, 32), dtype=src_B.dtype)
    src_B[:, 0] = 2
    src_B[0, 1:] = 3
    src_B[0, 0] = 2

    print("src_A (before bcast):")
    print(src_A.view(32, 32))
    print("src_B (before bcast):")
    print(src_B.view(32, 32))

    src_B = _prepare_bcast_tile(src_B, bcast_dim)

    print(f"src_B after _prepare_bcast_tile (bcast_dim={bcast_dim}):")
    print(src_B.view(32, 32))

    op = _BINARY_OPS[eltwise_op]
    golden_tensor = op(src_A.flatten()[:1024], src_B[:1024])

    print("golden_tensor:")
    print(golden_tensor.view(32, 32))

    unpack_to_dest = (
        formats.input_format.is_32_bit() and dest_acc == DestAccumulation.Yes
    )

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
            src_A,
            formats.input_format,
            src_B,
            formats.input_format,
            formats.output_format,
            tile_count_A=tile_cnt_A,
            tile_count_B=tile_cnt_B,
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

    print("res_from_L1 (face-ordered):")
    print(res_tensor.view(32, 32))

    res_tensor = untilize(res_tensor, stimuli_format=formats.output_format)

    print("res_from_L1 (untilized to row-major):")
    print(res_tensor.view(32, 32))

    assert passed_test(
        golden_tensor, res_tensor, formats.output_format
    ), "Assert against golden failed"
