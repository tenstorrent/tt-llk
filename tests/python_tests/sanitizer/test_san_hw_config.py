# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0

from helpers.format_config import DataFormat
from helpers.param_config import (
    input_output_formats,
    parametrize,
)
from helpers.test_config import run_test


@parametrize(
    test_name="san_hw_config_test",
    formats=input_output_formats([DataFormat.Float16_b]),
)
def test_san_hw_config(test_name, formats):
    test_config = {
        "testname": test_name,
        "formats": formats,
    }

    run_test(test_config)
