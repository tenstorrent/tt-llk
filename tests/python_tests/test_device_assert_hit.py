# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0

import pytest
from helpers.device import DeviceAssertionError, RiscCore
from helpers.format_config import DataFormat
from helpers.param_config import (
    input_output_formats,
    parametrize,
)
from helpers.test_config import run_test

# Base test that checks that we can catch asserts from the device on the host.


@parametrize(
    test_name="device_assert_hit_test",
    formats=input_output_formats([DataFormat.Float16_b]),  # any format will do
)
def test_device_assert_hit(test_name, formats):

    test_config = {
        "formats": formats,
        "testname": test_name,
    }

    with pytest.raises(DeviceAssertionError) as e:
        run_test(test_config)

    assert len(e.value.cores) == 1
    assert e.value.cores[0] == RiscCore.TRISC1
