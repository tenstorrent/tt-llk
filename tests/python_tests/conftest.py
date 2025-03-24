# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0

import os
import pytest
from helpers import HardwareController
from helpers.output_test_results import format_results, pass_fail_results
import logging
import csv

# Global variable to store results of all test cases
all_test_results = []

@pytest.fixture(scope="function", autouse=True)
def manage_hardware_controller():
    # Setup: initialize the hardware controller
    controller = HardwareController()
    controller.reset_card()
    yield controller

def pytest_configure(config):
    log_file = 'pytest_errors.log'
    # Clear the log file if it exists
    if os.path.exists(log_file):
        open(log_file, 'w').close()  # This clears the file
    logging.basicConfig(filename='pytest_errors.log',
                        level=logging.ERROR,
                        format='%(asctime)s - %(levelname)s - %(message)s')

def pytest_runtest_logreport(report):
    # Capture errors when tests fail
    if report.failed:
        logging.error(f"Test {report.nodeid} failed: {report.longrepr}")
        
@pytest.fixture(scope='session', autouse=True)
def test_results():
    global all_test_results
    yield all_test_results  # This makes the fixture available globally
    format_results(all_test_results)
    all_test_results.extend(all_test_results)
    


