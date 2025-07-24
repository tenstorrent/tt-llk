# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0

import os
from pathlib import Path

import pytest
from ttexalens import tt_exalens_init
from ttexalens.tt_exalens_lib import (
    arc_msg,
)

from helpers.chip_architecture import ChipArchitecture, get_chip_architecture
from helpers.device import reset_mailboxes
from helpers.format_config import InputOutputFormat
from helpers.log_utils import _format_log
from helpers.logging_config import TestLogContext, get_test_logger, setup_pytest_logging
from helpers.target_config import TestTargetConfig, initialize_test_target_from_pytest


def init_llk_home():
    if "LLK_HOME" in os.environ:
        return
    os.environ["LLK_HOME"] = str(Path(__file__).resolve().parents[2])


def check_hardware_headers():
    """Check if hardware-specific headers have been downloaded for the current architecture."""
    # Get the chip architecture
    chip_arch = get_chip_architecture()
    arch_name = chip_arch.value.lower()  # Convert enum to string

    # Get the project root (LLK_HOME)
    llk_home = Path(os.environ.get("LLK_HOME"))
    header_dir = llk_home / "tests" / "hw_specific" / arch_name / "inc"

    required_headers = [
        "cfg_defines.h",
        "dev_mem_map.h",
        "tensix.h",
        "tensix_dev_map.h",
        "tensix_types.h",
    ]

    # Check if header directory exists
    if not header_dir.exists():
        pytest.exit(
            f"ERROR: Hardware-specific header directory not found: {header_dir}\n\n"
            f"SOLUTION: Run the setup script to download required headers:\n"
            f"  cd {llk_home}/tests\n"
            f"  ./setup_testing_env.sh\n",
            returncode=1,
        )

    # Check for required headers
    missing_headers = []
    for header in required_headers:
        if not (header_dir / header).exists():
            missing_headers.append(header)

    if missing_headers:
        pytest.exit(
            f"ERROR: Missing required hardware headers for {arch_name}:\n"
            + "\n".join(f"  {header}" for header in missing_headers)
            + "\n\n"
            f"SOLUTION: Run the setup script to download missing headers:\n"
            f"  cd {llk_home}/tests\n"
            f"  ./setup_testing_env.sh\n",
            returncode=1,
        )

    print(f"✓ Hardware-specific headers for {arch_name} are present")


@pytest.fixture(scope="session", autouse=True)
def setup_logging():
    """Setup loguru logging for the entire test session."""
    setup_pytest_logging()
    test_logger = get_test_logger("test_session")
    yield
    test_logger.info("Test session completed")    


@pytest.fixture(autouse=True)
def reset_mailboxes_fixture():
    reset_mailboxes()
    yield


@pytest.fixture(autouse=True)
def test_logger(request):
    """Pytest fixture to automatically create per-test loggers for ALL tests."""
    # Get the full test name including parameters
    test_name = request.node.name

    # Create a test-specific logger context
    with TestLogContext(test_name, log_level="INFO") as test_logger:
        yield test_logger

    initialize_test_target_from_pytest(config)
    test_target = TestTargetConfig()

    if test_target.run_simulator:
        tt_exalens_init.init_ttexalens_remote(port=test_target.simulator_port)
    else:
        tt_exalens_init.init_ttexalens()


def pytest_runtest_logreport(report):
    # Capture errors when tests fail
    test_logger = get_test_logger("test_session")
    if report.failed:
        error_msg = f"Test {report.nodeid} failed"

        # Add more details about the failure
        if hasattr(report, "longrepr") and report.longrepr:
            # Extract assertion details
            longrepr_str = str(report.longrepr)
            lines = longrepr_str.split("\n")

            # Find the assertion line
            assertion_line = None
            for line in lines:
                stripped = line.strip()
                if stripped.startswith("assert ") or "AssertionError" in stripped:
                    assertion_line = stripped
                    break

            if assertion_line:
                error_msg += f" - {assertion_line}"
            else:
                # Fallback to the first line of the error
                error_lines = [
                    line.strip()
                    for line in lines
                    if line.strip() and not line.startswith("_")
                ]
                if error_lines:
                    error_msg += f" - {error_lines[0]}"

        test_logger.error(f"{error_msg}\n")

        # Also log detailed failure info if available
        if hasattr(report, "longrepr") and report.longrepr:
            test_logger.error(f"Detailed failure information for {report.nodeid}:")
            test_logger.error(f"{report.longrepr}")
            test_logger.error("=" * 80)  # Separator between test failures


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
    if report.when != "call":
        return

    callspec = getattr(report.item, "callspec", None)
    if callspec is None:
        return

    print(f"\nParameters: {_stringify_params(callspec.params)}")


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

    # Check if hardware-specific headers are present
    check_hardware_headers()

    test_target = TestTargetConfig()
    if not test_target.run_simulator:
        # Send ARC message for GO BUSY signal. This should increase device clock speed.
        _send_arc_message("GO_BUSY", test_target.device_id)


def pytest_sessionfinish(session, exitstatus):
    test_logger = get_test_logger("test_session")
    BOLD = "\033[1m"
    YELLOW = "\033[93m"
    RESET = "\033[0m"
    if _format_log:
        test_logger.info(
            f"\n\n{BOLD}{YELLOW} Cases Where Dest Accumulation Turned On:{RESET}"
        )
        for input_fmt, output_fmt in _format_log:
            test_logger.warning(f"{BOLD}{YELLOW}  {input_fmt} -> {output_fmt}{RESET}")

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
