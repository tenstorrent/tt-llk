# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0

import pytest
from helpers import HardwareController


@pytest.fixture(scope="function", autouse=True)
def hardware_controller(request):
    # Setup: initialize the hardware controller
    controller = HardwareController()

    yield controller

    # Teardown: reset the hardware only if the test failed
    if request.node.result.failed:
        controller.reset()


@pytest.hookimpl(hookwrapper=True)
def pytest_runtest_makereport(item, call):
    # Execute all other hooks to obtain the report object
    outcome = yield
    result = outcome.get_result()

    # Attach the test result to the item
    if result.when == "call":
        item.result = result
