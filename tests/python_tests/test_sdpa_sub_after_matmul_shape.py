# SPDX-FileCopyrightText: © 2026 Tenstorrent AI ULC
#
# SPDX-License-Identifier: Apache-2.0

import pytest
import torch
from helpers.device import read_from_device
from helpers.format_config import DataFormat
from helpers.golden_generators import (
    BroadcastGolden,
    EltwiseBinaryGolden,
    MatmulGolden,
    get_golden_generator,
)
from helpers.llk_params import (
    BroadcastType,
    DestAccumulation,
    MathFidelity,
    MathOperation,
    format_dict,
)
from helpers.param_config import input_output_formats
from helpers.stimuli_config import StimuliConfig
from helpers.stimuli_generator import generate_stimuli
from helpers.test_config import TestConfig, TestMode
from helpers.test_variant_parameters import MATH_FIDELITY
from helpers.tilize_untilize import tilize_block, untilize_block
from helpers.unpack import unpack_res_tiles
from helpers.utils import passed_test

FORMATS = input_output_formats([DataFormat.Float16_b])[0]

SUB_A_ADDR = 0x1A000
SUB_B_ADDR = 0x1E000
SUB_OUT0_ADDR = 0x1F000
MATMUL_A_ADDR = 0x23000
MATMUL_B_ADDR = 0x27000
MATMUL_OUT_ADDR = 0x2F000
SUB_OUT1_ADDR = 0x33000

SUB_A_DIMS = [64, 128]
SUB_B_DIMS = [64, 32]
SUB_OUT_TILES = 8

MATMUL_A_DIMS = [64, 128]
MATMUL_B_DIMS = [128, 128]
MATMUL_OUT_TILES = 8

TILE_SIZE_BYTES = 2048


@pytest.mark.parametrize(
    "math_fidelity",
    [
        MathFidelity.LoFi,
        MathFidelity.HiFi2,
        MathFidelity.HiFi3,
        MathFidelity.HiFi4,
    ],
)
def test_sdpa_sub_after_matmul_shape(math_fidelity, workers_tensix_coordinates):
    sub_a, sub_tile_cnt_a, sub_b, sub_tile_cnt_b = generate_stimuli(
        stimuli_format_A=FORMATS.input_format,
        input_dimensions_A=SUB_A_DIMS,
        stimuli_format_B=FORMATS.input_format,
        input_dimensions_B=SUB_B_DIMS,
        sfpu=False,
    )
    matmul_a, matmul_tile_cnt_a, matmul_b, matmul_tile_cnt_b = generate_stimuli(
        stimuli_format_A=FORMATS.input_format,
        input_dimensions_A=MATMUL_A_DIMS,
        stimuli_format_B=FORMATS.input_format,
        input_dimensions_B=MATMUL_B_DIMS,
        sfpu=False,
    )

    tilized_sub_a = tilize_block(
        sub_a, dimensions=SUB_A_DIMS, stimuli_format=FORMATS.input_format
    )
    tilized_sub_b = tilize_block(
        sub_b, dimensions=SUB_B_DIMS, stimuli_format=FORMATS.input_format
    )
    tilized_matmul_a = tilize_block(
        matmul_a, dimensions=MATMUL_A_DIMS, stimuli_format=FORMATS.input_format
    )
    tilized_matmul_b = tilize_block(
        matmul_b, dimensions=MATMUL_B_DIMS, stimuli_format=FORMATS.input_format
    )

    sub_b_broadcasted_tilized = get_golden_generator(BroadcastGolden)(
        BroadcastType.Column,
        tilized_sub_b.flatten(),
        FORMATS.input_format,
        num_faces=4,
        tile_cnt=sub_tile_cnt_b,
        face_r_dim=16,
    )
    sub_b_broadcasted = (
        untilize_block(sub_b_broadcasted_tilized, FORMATS.input_format, SUB_B_DIMS)
        .view(SUB_B_DIMS[0], SUB_B_DIMS[1])
        .repeat(1, SUB_A_DIMS[1] // SUB_B_DIMS[1])
        .flatten()
    )
    sub_golden_matrix = get_golden_generator(EltwiseBinaryGolden)(
        MathOperation.Elwsub,
        torch.tensor(sub_a, dtype=format_dict[FORMATS.input_format]),
        sub_b_broadcasted,
        FORMATS.output_format,
        math_fidelity,
    )
    sub_golden = tilize_block(
        sub_golden_matrix,
        dimensions=SUB_A_DIMS,
        stimuli_format=FORMATS.output_format,
    ).flatten()

    matmul_golden = get_golden_generator(MatmulGolden)(
        matmul_a,
        matmul_b,
        FORMATS.output_format,
        math_fidelity,
        input_A_dimensions=MATMUL_A_DIMS,
        input_B_dimensions=MATMUL_B_DIMS,
        tilize=True,
        input_A_format=FORMATS.input_format,
        input_B_format=FORMATS.input_format,
    )

    configuration = TestConfig(
        "sources/sdpa_sub_after_matmul_shape_test.cpp",
        FORMATS,
        templates=[MATH_FIDELITY(math_fidelity)],
        runtimes=[],
        variant_stimuli=None,
        dest_acc=DestAccumulation.No,
    )

    configuration.generate_variant_hash()
    if TestConfig.MODE in [TestMode.PRODUCE, TestMode.DEFAULT]:
        configuration.build_elfs()

    if TestConfig.MODE == TestMode.PRODUCE:
        pytest.skip(TestConfig.SKIP_JUST_FOR_COMPILE_MARKER)

    input_packer = StimuliConfig.get_packer(FORMATS.input_format)
    StimuliConfig.write_matrix(
        tilized_sub_a.flatten(),
        sub_tile_cnt_a,
        input_packer,
        SUB_A_ADDR,
        TILE_SIZE_BYTES,
        4,
        16,
        workers_tensix_coordinates,
    )
    StimuliConfig.write_matrix(
        tilized_sub_b.flatten(),
        sub_tile_cnt_b,
        input_packer,
        SUB_B_ADDR,
        TILE_SIZE_BYTES,
        4,
        16,
        workers_tensix_coordinates,
    )
    StimuliConfig.write_matrix(
        tilized_matmul_a.flatten(),
        matmul_tile_cnt_a,
        input_packer,
        MATMUL_A_ADDR,
        TILE_SIZE_BYTES,
        4,
        16,
        workers_tensix_coordinates,
    )
    StimuliConfig.write_matrix(
        tilized_matmul_b.flatten(),
        matmul_tile_cnt_b,
        input_packer,
        MATMUL_B_ADDR,
        TILE_SIZE_BYTES,
        4,
        16,
        workers_tensix_coordinates,
    )

    configuration.run_elf_files(workers_tensix_coordinates)
    configuration.wait_for_tensix_operations_finished(workers_tensix_coordinates)

    sub_result0 = read_from_device(
        workers_tensix_coordinates,
        SUB_OUT0_ADDR,
        num_bytes=SUB_OUT_TILES * TILE_SIZE_BYTES,
    )
    matmul_result = read_from_device(
        workers_tensix_coordinates,
        MATMUL_OUT_ADDR,
        num_bytes=MATMUL_OUT_TILES * TILE_SIZE_BYTES,
    )
    sub_result1 = read_from_device(
        workers_tensix_coordinates,
        SUB_OUT1_ADDR,
        num_bytes=SUB_OUT_TILES * TILE_SIZE_BYTES,
    )

    unpacked_sub0 = unpack_res_tiles(
        sub_result0, FORMATS.output_format, SUB_OUT_TILES, False, 4, 16
    )
    unpacked_matmul = unpack_res_tiles(
        matmul_result, FORMATS.output_format, MATMUL_OUT_TILES, False, 4, 16
    )
    unpacked_sub1 = unpack_res_tiles(
        sub_result1, FORMATS.output_format, SUB_OUT_TILES, False, 4, 16
    )

    sub_tensor0 = torch.tensor(unpacked_sub0, dtype=format_dict[FORMATS.output_format])
    matmul_tensor = torch.tensor(
        unpacked_matmul, dtype=format_dict[FORMATS.output_format]
    )
    sub_tensor1 = torch.tensor(unpacked_sub1, dtype=format_dict[FORMATS.output_format])

    assert passed_test(
        sub_golden, sub_tensor0, FORMATS.output_format, print_errors=True
    ), "initial sub output mismatch"
    assert passed_test(
        matmul_golden, matmul_tensor, FORMATS.output_format, print_errors=True
    ), "matmul output mismatch"
    assert passed_test(
        sub_golden, sub_tensor1, FORMATS.output_format, print_errors=True
    ), "sub output after matmul mismatch"
