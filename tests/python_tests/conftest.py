# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0

import logging
import os
import sys
import time
from pathlib import Path

import pytest
import requests
from requests.exceptions import ConnectionError, RequestException, Timeout
from ttexalens import tt_exalens_init
from ttexalens.tt_exalens_lib import (
    arc_msg,
)

from helpers.chip_architecture import ChipArchitecture, get_chip_architecture
from helpers.device import reset_mailboxes
from helpers.format_config import InputOutputFormat
from helpers.log_utils import _format_log
from helpers.target_config import TestTargetConfig, initialize_test_target_from_pytest


def init_llk_home():
    if "LLK_HOME" in os.environ:
        return
    os.environ["LLK_HOME"] = str(Path(__file__).resolve().parents[2])


@pytest.fixture(autouse=True)
def reset_mailboxes_fixture():
    reset_mailboxes()
    yield


@pytest.fixture(scope="session", autouse=True)
def download_headers():
    CHIP_ARCH = get_chip_architecture()
    if CHIP_ARCH not in [ChipArchitecture.WORMHOLE, ChipArchitecture.BLACKHOLE]:
        sys.exit(f"Unsupported CHIP_ARCH detected: {CHIP_ARCH.value}")

    LLK_HOME = os.environ.get("LLK_HOME")
    HEADER_DIR = os.path.join(LLK_HOME, "tests", "hw_specific", CHIP_ARCH.value, "inc")
    STAMP_FILE = os.path.join(HEADER_DIR, ".headers_downloaded")
    if os.path.exists(STAMP_FILE):
        print("Headers already downloaded. Skipping download.")
        return

    BASE_URL = f"https://raw.githubusercontent.com/tenstorrent/tt-metal/refs/heads/main/tt_metal/hw/inc/{CHIP_ARCH.value}"
    WORMHOLE_SPECIFIC_URL = f"https://raw.githubusercontent.com/tenstorrent/tt-metal/refs/heads/main/tt_metal/hw/inc/{CHIP_ARCH.value}/wormhole_b0_defines"
    HEADERS = [
        "cfg_defines.h",
        "dev_mem_map.h",
        "tensix.h",
        "tensix_dev_map.h",
        "tensix_types.h",
    ]

    # Create the header directory if it doesn't exist
    os.makedirs(HEADER_DIR, exist_ok=True)

    # Determine the specific URL based on CHIP_ARCH
    specific_url = (
        WORMHOLE_SPECIFIC_URL if CHIP_ARCH == ChipArchitecture.WORMHOLE else None
    )

    # Download headers
    def download_with_retries(url, header):
        RETRIES, RETRY_DELAY = 3, 2
        for attempt in range(1, RETRIES + 1):
            try:
                print(f"Attempt {attempt}: Downloading {header} from {url}...")
                response = requests.get(url, timeout=10)
                if response.status_code == 200:
                    with open(os.path.join(HEADER_DIR, header), "wb") as f:
                        f.write(response.content)
                    return True
                elif response.status_code == 429:  # Rate limited
                    retry_after = int(response.headers.get("Retry-After", RETRY_DELAY))
                    print(f"Rate limited. Waiting {retry_after} seconds...")
                    time.sleep(retry_after)
                else:
                    print(f"HTTP error {response.status_code} for {url}")
                    return False
            except (ConnectionError, Timeout) as e:
                print(f"Network issue on attempt {attempt}: {e}")
                if attempt < RETRIES:
                    time.sleep(RETRY_DELAY)
                    RETRY_DELAY *= 2
            except RequestException as e:
                print(f"Non-retryable error on attempt {attempt}: {e}")
                return False
        return False

    for header in HEADERS:
        header_url = f"{BASE_URL}/{header}"
        specific_header_url = f"{specific_url}/{header}" if specific_url else None

        # Try primary URL
        if not download_with_retries(header_url, header):
            # Fallback to specific URL
            if specific_header_url and not download_with_retries(
                specific_header_url, header
            ):
                print(f"Failed to download {header} after trying both URLs")
                sys.exit(1)
            elif not specific_header_url:
                print(f"Failed to download {header} after retries from primary URL")
                sys.exit(1)

    # Create the stamp file to indicate headers are downloaded
    with open(STAMP_FILE, "w") as f:
        f.write("Headers downloaded.\n")


def pytest_configure(config):
    log_file = "pytest_errors.log"
    # Clear the log file if it exists
    if os.path.exists(log_file):
        os.remove(log_file)
    logging.basicConfig(
        filename=log_file,
        level=logging.ERROR,
        format="%(asctime)s - %(levelname)s - %(message)s",
    )


def pytest_runtest_logreport(report):
    # Capture errors when tests fail
    if report.failed:
        logging.error(f"Test {report.nodeid} failed: {report.longrepr}\n")


def _stringify_params(params):
    parts = []
    for name, value in params.items():
        # todo: handle FormatConfig?
        if name == "test_name":
            continue
        elif isinstance(value, InputOutputFormat):
            parts.append(f"{name}.input={value.input}")
            parts.append(f"{name}.output={value.output}")
        elif isinstance(value, str):
            parts.append(f'{name}="{value}"')
        elif hasattr(value, "repr"):
            parts.append(f"{name}={value.repr()}")
        else:
            parts.append(f"{name}={str(value)}")

    return f"[{' | '.join(parts)}]"


def pytest_runtest_logreport(report):
    if report.when == "call":  # We are interested in the 'call' phase
        if hasattr(report.item, "callspec"):
            print(f"\nParameters: {_stringify_params(report.item.callspec.params)}")


@pytest.hookimpl(hookwrapper=True)
def pytest_runtest_makereport(item, call):
    # Execute all other hooks to obtain the report object
    outcome = yield
    report = outcome.get_result()

    # Attach the item to the report so it's available in logreport
    report.item = item
    return report


def pytest_sessionstart(session):
    # Default LLK_HOME environment variable
    init_llk_home()

    test_target = TestTargetConfig()
    if not test_target.run_simulator:
        # Send ARC message for GO BUSY signal. This should increase device clock speed.
        _send_arc_message("GO_BUSY", test_target.device_id)


def pytest_sessionfinish(session, exitstatus):
    BOLD = "\033[1m"
    YELLOW = "\033[93m"
    RESET = "\033[0m"
    if _format_log:
        print(f"\n\n{BOLD}{YELLOW} Cases Where Dest Accumulation Turned On:{RESET}")
        for input_fmt, output_fmt in _format_log:
            print(f"{BOLD}{YELLOW}  {input_fmt} -> {output_fmt}{RESET}")

    test_target = TestTargetConfig()
    if not test_target.run_simulator:
        # Send ARC message for GO IDLE signal. This should decrease device clock speed.
        _send_arc_message("GO_IDLE", test_target.device_id)


def _send_arc_message(message_type: str, device_id: int):
    """Helper to send ARC messages with better abstraction."""
    ARC_COMMON_PREFIX = 0xAA00
    message_codes = {"GO_BUSY": 0x52, "GO_IDLE": 0x54}

    arc_msg(
        device_id=device_id,
        msg_code=ARC_COMMON_PREFIX | message_codes[message_type],
        wait_for_done=True,
        arg0=0,
        arg1=0,
        timeout=10,
    )


# Define the possible custom command line options
def pytest_addoption(parser):
    parser.addoption(
        "--run_simulator", action="store_true", help="Run tests using the simulator."
    )
    parser.addoption(
        "--port",
        action="store",
        type=int,
        default=5555,
        help="Integer number of the server port.",
    )


# Use simulator or silicon to run tests depending on the given command line options
def pytest_configure(config):
    initialize_test_target_from_pytest(config)
    test_target = TestTargetConfig()

    if test_target.run_simulator:
        tt_exalens_init.init_ttexalens_remote(port=test_target.simulator_port)
    else:
        tt_exalens_init.init_ttexalens()


# Skip decorators for specific architectures
# These decorators can be used to skip tests based on the architecture
# For example, if you want to skip a test for the "wormhole" architecture,
# decorate the test with @skip_for_wormhole.

skip_for_wormhole = pytest.mark.skipif(
    get_chip_architecture() == ChipArchitecture.WORMHOLE,
    reason="Test is not supported on Wormhole architecture",
)

skip_for_blackhole = pytest.mark.skipif(
    get_chip_architecture() == ChipArchitecture.BLACKHOLE,
    reason="Test is not supported on Blackhole architecture",
)
