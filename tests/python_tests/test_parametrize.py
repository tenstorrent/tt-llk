# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0

from helpers.format_config import DataFormat
from helpers.llk_params import DestAccumulation, MathFidelity, MathOperation
from helpers.param_config import input_output_formats, parametrize


def get_valid_math_fidelities(format, operation):
    if operation in [MathOperation.Elwadd, MathOperation.Elwsub]:
        return [MathFidelity.LoFi]

    if format.input == DataFormat.Bfp8_b:
        return [MathFidelity.LoFi, MathFidelity.HiFi2]

    return [
        MathFidelity.LoFi,
        MathFidelity.HiFi2,
        MathFidelity.HiFi3,
        MathFidelity.HiFi4,
    ]


def get_valid_dest_accumulation_modes(format):
    if (
        format.input in [DataFormat.Bfp8_b, DataFormat.Float16_b]
        and format.output == DataFormat.Float16
    ):
        return [DestAccumulation.Yes]

    return [DestAccumulation.No, DestAccumulation.Yes]


# ... in test file ...


@parametrize(
    test_name="eltwise_binary_fpu_perf",
    math_fidelity=lambda formats, mathop: get_valid_math_fidelities(formats, mathop),
    dest_acc=lambda formats: get_valid_dest_accumulation_modes(formats),
    formats=input_output_formats(
        [DataFormat.Bfp8_b, DataFormat.Float16, DataFormat.Float16_b]
    ),
    mathop=[MathOperation.Elwadd, MathOperation.Elwsub, MathOperation.Elwmul],
)
def test_eltwise_binary_fpu(test_name, formats, mathop, math_fidelity, dest_acc):
    pass
