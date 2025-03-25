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
