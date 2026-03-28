# SPDX-FileCopyrightText: © 2026 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0

from dataclasses import dataclass

import pytest
from helpers.format_config import DataFormat
from helpers.llk_params import DestAccumulation, MathFidelity, PerfRunType, Transpose
from helpers.matmul_sweep import (
    generate_matmul_dimension_combinations,
    generate_tile_dims,
)
from helpers.param_config import input_output_formats, parametrize
from helpers.perf import PerfConfig
from helpers.stimuli_config import StimuliConfig
from helpers.test_variant_parameters import (
    CRK_TILE_DIMM,
    DEST_SYNC,
    LOOP_FACTOR,
    MATH_FIDELITY,
    NUM_FACES,
    THROTTLE_LEVEL,
    TILE_COUNT,
    UNPACK_TRANS_FACES,
    TemplateParameter,
)


@dataclass
class MATMUL_CUSTOM_NO_MOP(TemplateParameter):
    def convert_to_cpp(self) -> str:
        return "#define USE_MATMUL_CUSTOM_NO_MOP 1"


@pytest.mark.perf
@parametrize(
    combos=generate_matmul_dimension_combinations(8, kt_dims=[1, 2, 4]),
    formats=input_output_formats([DataFormat.Float16_b]),
    dest_acc=[DestAccumulation.No],
    math_fidelity=[MathFidelity.LoFi],
)
def test_perf_matmul_custom(
    perf_report, combos, formats, dest_acc, math_fidelity, workers_tensix_coordinates
):
    run_types = [
        PerfRunType.L1_TO_L1,
        PerfRunType.UNPACK_ISOLATE,
        PerfRunType.MATH_ISOLATE,
        PerfRunType.PACK_ISOLATE,
        PerfRunType.L1_CONGESTION,
    ]

    dims = generate_tile_dims(combos)
    variant_tile_count = dims.rt_dim * dims.ct_dim * dims.kt_dim

    configuration = PerfConfig(
        "sources/matmul_perf.cpp",
        formats,
        run_types,
        templates=[
            MATMUL_CUSTOM_NO_MOP(),
            MATH_FIDELITY(math_fidelity),
            DEST_SYNC(),
            THROTTLE_LEVEL(),
        ],
        runtimes=[
            UNPACK_TRANS_FACES(Transpose.No),
            NUM_FACES(),
            LOOP_FACTOR(16),
            TILE_COUNT(variant_tile_count),
            CRK_TILE_DIMM(dims.ct_dim, dims.rt_dim, dims.kt_dim),
        ],
        variant_stimuli=StimuliConfig(
            None,
            formats.input_format,
            None,
            formats.input_format,
            formats.output_format,
            tile_count_A=variant_tile_count,
            tile_count_B=variant_tile_count,
            tile_count_res=variant_tile_count,
        ),
        dest_acc=dest_acc,
    )

    configuration.run(perf_report, location=workers_tensix_coordinates)
