# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0

import pytest
from helpers import HardwareController


@pytest.fixture(scope="function", autouse=True)
def hardware_controller():
    # Setup: initialize the hardware controller
    controller = HardwareController()

    yield controller

    controller.reset()
