# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0

import pytest
import os
import logging
from helpers import HardwareController


@pytest.fixture(scope="function", autouse=True)
def manage_hardware_controller():
    # Setup: initialize the hardware controller
    controller = HardwareController()
    controller.reset_card()
    yield controller


def pytest_configure(config):
    log_file = "pytest_errors.log"
    # Clear the log file if it exists
    if os.path.exists(log_file):
        open(log_file, "w").close()  # This clears the file
    logging.basicConfig(
        filename="pytest_errors.log",
        level=logging.ERROR,
        format="%(asctime)s - %(levelname)s - %(message)s",
    )


def pytest_runtest_logreport(report):
    # Capture errors when tests fail
    if report.failed:
        error_message = f"Test {report.nodeid} failed: {report.longrepr}"
        # Add a new line after an error has been written
        formatted_message = error_message + "\n"
        logging.error(formatted_message)

# Modify how the nodeid is generated
def pytest_collection_modifyitems(items):
    for item in items:
        # Modify the test item to hide the function name and only show parameters
        # item.nodeid is immutable, so we should modify how the test is represented
        original_nodeid = item.nodeid
        # Split the nodeid to separate the filename and the function name along with parameters
        file_part, params_part = original_nodeid.split('::', 1)
        item._nodeid = file_part + '::[' + params_part.split('[')[1]

def pytest_runtest_protocol(item, nextitem):
    """
    This hook can modify the test item before it's executed.
    We're going to set the test function name to an empty string.
    """
    # Modify the nodeid to show only the parameters, not the function name
    file_part, param_part = item.nodeid.split('::', 1)
    item.name = f"| {param_part.split('[')[1]}"

    # Continue the test execution as usual
    return None