# SPDX-FileCopyrightText: © 2026 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0

"""
Full pipeline performance test for BH fast-tilize (unpack + math + pack).
Measures cycles per tile for the complete tilization pipeline.
Target: ≤ 20 cycles/tile (amortized across 4-tile unit).
"""

import pytest
from helpers.chip_architecture import ChipArchitecture, get_chip_architecture
from helpers.format_config import DataFormat, InputOutputFormat
from helpers.llk_params import DestAccumulation, PerfRunType
from helpers.param_config import parametrize
from helpers.perf import PerfConfig
from helpers.stimuli_config import StimuliConfig
from helpers.test_variant_parameters import (
    LOOP_FACTOR,
    NUM_FACES,
    TILE_COUNT,
    generate_input_dim,
)


# Fast-tilize modifies non-banked tensix config registers that persist through soft reset.
@pytest.fixture(autouse=True)
def warm_reset_device():
    try:
        import tt_umd

        tt_umd.WarmReset.warm_reset()
        from pathlib import Path

        import helpers.device as device_mod
        from helpers.test_config import TestConfig

        TestConfig.BRISC_ELF_LOADED = False
        TestConfig.LAST_LOADED_ELFS = Path()
        device_mod.common_counter = 0
    except Exception:
        pass


@pytest.mark.perf
@parametrize(
    input_format=[DataFormat.Float16_b],
    output_format=[DataFormat.Float16_b],
    dest_acc=[DestAccumulation.No, DestAccumulation.Yes],
    dimensions=[(1, 4), (1, 8), (2, 4), (2, 8), (3, 4), (3, 8), (4, 4), (4, 8)],
)
def test_fast_tilize_full_perf(
    perf_report,
    input_format,
    output_format,
    dest_acc,
    dimensions,
    workers_tensix_coordinates,
):
    if get_chip_architecture() != ChipArchitecture.BLACKHOLE:
        pytest.skip("BH only")

    input_height_tiles, input_width_tiles = dimensions
    assert input_width_tiles % 4 == 0, "ct_dim must be divisible by 4"

    tile_count = input_height_tiles * input_width_tiles
    input_dims = (input_height_tiles * 32, input_width_tiles * 32)

    formats = InputOutputFormat(input_format, output_format)

    configuration = PerfConfig(
        "sources/fast_tilize_bh_test.cpp",
        formats,
        run_types=[
            PerfRunType.L1_TO_L1,
            PerfRunType.PACK_ISOLATE,
            PerfRunType.MATH_ISOLATE,
            PerfRunType.UNPACK_ISOLATE,
        ],
        templates=[],
        runtimes=[
            generate_input_dim(input_dims, input_dims),
            TILE_COUNT(tile_count),
            LOOP_FACTOR(4),
            NUM_FACES(4),
        ],
        variant_stimuli=StimuliConfig(
            None,
            formats.input_format,
            None,
            formats.input_format,
            formats.output_format,
            tile_count_A=tile_count,
            tile_count_B=tile_count,
            tile_count_res=tile_count,
        ),
        dest_acc=dest_acc,
        compile_time_formats=True,
    )

    try:
        configuration.run(perf_report, run_count=2, location=workers_tensix_coordinates)
    except Exception as e:
        print(f"\n!!! PERF RUN ERROR: {type(e).__name__}: {e}\n")
        raise
