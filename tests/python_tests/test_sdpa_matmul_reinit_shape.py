# SPDX-FileCopyrightText: © 2026 Tenstorrent AI ULC
#
# SPDX-License-Identifier: Apache-2.0

import pytest
import torch
from helpers.device import read_from_device
from helpers.format_config import DataFormat
from helpers.golden_generators import MatmulGolden, get_golden_generator
from helpers.llk_params import MathFidelity, format_dict
from helpers.param_config import input_output_formats
from helpers.stimuli_config import StimuliConfig
from helpers.stimuli_generator import generate_stimuli
from helpers.test_config import TestConfig, TestMode
from helpers.test_variant_parameters import MATH_FIDELITY
from helpers.tilize_untilize import tilize_block
from helpers.unpack import unpack_res_tiles
from helpers.utils import passed_test

FORMATS = input_output_formats([DataFormat.Float16_b])[0]

MATMUL_A_ADDR = 0x1A000
MATMUL_B_ADDR = 0x1E000
SUB_A_ADDR = 0x26000
SUB_B_ADDR = 0x28000
MATMUL_OUT0_ADDR = 0x28800
MATMUL_OUT1_ADDR = 0x2E800

MATMUL_A_DIMS = [64, 128]
MATMUL_B_DIMS = [128, 128]
MATMUL_OUT_TILES = 8
SUB_A_DIMS = [32, 128]
SUB_B_DIMS = [32, 32]
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
def test_sdpa_matmul_reinit_shape(math_fidelity, workers_tensix_coordinates):
    src_a, tile_cnt_a, src_b, tile_cnt_b = generate_stimuli(
        stimuli_format_A=FORMATS.input_format,
        input_dimensions_A=MATMUL_A_DIMS,
        stimuli_format_B=FORMATS.input_format,
        input_dimensions_B=MATMUL_B_DIMS,
        sfpu=False,
    )
    sub_a, sub_tile_cnt_a, sub_b, sub_tile_cnt_b = generate_stimuli(
        stimuli_format_A=FORMATS.input_format,
        input_dimensions_A=SUB_A_DIMS,
        stimuli_format_B=FORMATS.input_format,
        input_dimensions_B=SUB_B_DIMS,
        sfpu=False,
    )

    golden = get_golden_generator(MatmulGolden)(
        src_a,
        src_b,
        FORMATS.output_format,
        math_fidelity,
        input_A_dimensions=MATMUL_A_DIMS,
        input_B_dimensions=MATMUL_B_DIMS,
        tilize=True,
        input_A_format=FORMATS.input_format,
        input_B_format=FORMATS.input_format,
    )

    tilized_a = tilize_block(
        src_a, dimensions=MATMUL_A_DIMS, stimuli_format=FORMATS.input_format
    )
    tilized_b = tilize_block(
        src_b, dimensions=MATMUL_B_DIMS, stimuli_format=FORMATS.input_format
    )
    tilized_sub_a = tilize_block(
        sub_a, dimensions=SUB_A_DIMS, stimuli_format=FORMATS.input_format
    )
    tilized_sub_b = tilize_block(
        sub_b, dimensions=SUB_B_DIMS, stimuli_format=FORMATS.input_format
    )

    configuration = TestConfig(
        "sources/sdpa_matmul_reinit_shape_test.cpp",
        FORMATS,
        templates=[MATH_FIDELITY(math_fidelity)],
        runtimes=[],
        variant_stimuli=None,
    )

    configuration.generate_variant_hash()
    if TestConfig.MODE in [TestMode.PRODUCE, TestMode.DEFAULT]:
        configuration.build_elfs()

    if TestConfig.MODE == TestMode.PRODUCE:
        pytest.skip(TestConfig.SKIP_JUST_FOR_COMPILE_MARKER)

    input_packer = StimuliConfig.get_packer(FORMATS.input_format)
    StimuliConfig.write_matrix(
        tilized_a.flatten(),
        tile_cnt_a,
        input_packer,
        MATMUL_A_ADDR,
        TILE_SIZE_BYTES,
        4,
        16,
        workers_tensix_coordinates,
    )
    StimuliConfig.write_matrix(
        tilized_b.flatten(),
        tile_cnt_b,
        input_packer,
        MATMUL_B_ADDR,
        TILE_SIZE_BYTES,
        4,
        16,
        workers_tensix_coordinates,
    )
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

    configuration.run_elf_files(workers_tensix_coordinates)
    configuration.wait_for_tensix_operations_finished(workers_tensix_coordinates)

    read_bytes = MATMUL_OUT_TILES * TILE_SIZE_BYTES
    result0 = read_from_device(
        workers_tensix_coordinates, MATMUL_OUT0_ADDR, num_bytes=read_bytes
    )
    result1 = read_from_device(
        workers_tensix_coordinates, MATMUL_OUT1_ADDR, num_bytes=read_bytes
    )

    unpacked0 = unpack_res_tiles(
        result0, FORMATS.output_format, MATMUL_OUT_TILES, False, 4, 16
    )
    unpacked1 = unpack_res_tiles(
        result1, FORMATS.output_format, MATMUL_OUT_TILES, False, 4, 16
    )

    tensor0 = torch.tensor(unpacked0, dtype=format_dict[FORMATS.output_format])
    tensor1 = torch.tensor(unpacked1, dtype=format_dict[FORMATS.output_format])

    assert passed_test(
        golden, tensor0, FORMATS.output_format, print_errors=False
    ), "initial matmul output mismatch"
    assert passed_test(
        golden, tensor1, FORMATS.output_format, print_errors=False
    ), "matmul output after short reinit mismatch"
