# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0

"""Centralized logging configuration using loguru for the testing framework."""

import contextvars
import sys
from pathlib import Path
from typing import Optional

from loguru import logger

# Context variable to track current test
current_test_context = contextvars.ContextVar("current_test", default=None)

# Global test_logger variable that gets updated automatically
test_logger = logger  # Default to regular logger


def setup_test_logging(
    log_level: str = "INFO",
    log_file: Optional[str] = None,
    enable_console: bool = True,
    enable_colors: bool = True,
    test_name: Optional[str] = None,
) -> None:
    """
    Configure loguru logging for the testing framework.

    Args:
        log_level: Logging level (DEBUG, INFO, WARNING, ERROR, CRITICAL)
        log_file: Optional file path for logging output
        enable_console: Whether to log to console
        enable_colors: Whether to use colored output
        test_name: Optional test name for contextualized logging
    """
    # Remove default handler to avoid duplicates
    logger.remove()

    # Console logging format
    console_format = (
        "<green>{time:YYYY-MM-DD HH:mm:ss.SSS}</green> | "
        "<level>{level: <8}</level> | "
        "<cyan>{name}</cyan>:<cyan>{function}</cyan>:<cyan>{line}</cyan>"
    )

    if test_name:
        console_format += f" | <magenta>[{test_name}]</magenta>"

    console_format += " - <level>{message}</level>"

    # Add console handler if enabled
    if enable_console:
        logger.add(
            sys.stderr,
            format=console_format,
            level=log_level,
            colorize=enable_colors,
            backtrace=True,
            diagnose=True,
        )

    # Add file handler if specified
    if log_file:
        file_format = (
            "{time:YYYY-MM-DD HH:mm:ss.SSS} | {level: <8} | " "{name}:{function}:{line}"
        )
        if test_name:
            file_format += f" | [{test_name}]"
        file_format += " - {message}"

        logger.add(
            log_file,
            format=file_format,
            level=log_level,
            rotation="10 MB",
            retention="7 days",
            compression="gz",
        )


def get_test_logger(name: str = "tt_llk_test") -> "logger":
    """
    Get a loguru logger instance for testing.

    Args:
        name: Logger name/context

    Returns:
        Configured loguru logger
    """
    return logger.bind(context=name)


def setup_pytest_logging():
    """Setup logging specifically for pytest runs."""
    # Get log directory
    log_dir = Path("test_logs")
    log_dir.mkdir(exist_ok=True)

    # Setup with default test configuration
    setup_test_logging(
        log_level="INFO",
        log_file=str(log_dir / "test_run.log"),
        enable_console=True,
        enable_colors=True,
    )

    # Add a separate debug log file
    logger.add(
        str(log_dir / "debug.log"),
        format="{time:YYYY-MM-DD HH:mm:ss.SSS} | {level: <8} | {name}:{function}:{line} - {message}",
        level="DEBUG",
        rotation="50 MB",
        retention="3 days",
    )


def sanitize_test_name(name: str) -> str:
    """Sanitize test name for use in file names, making them clean and readable."""
    import re

    # Extract the base test name (everything before the first parameter)
    base_match = re.match(r"^(test_\w+)", name)
    base_name = base_match.group(1) if base_match else "test"

    # Extract key parameters and create a compact representation
    params = []

    # Look for format information
    input_fmt = re.search(r"(?:unpack_src=|input_format=)(\w+)", name)
    output_fmt = re.search(r"(?:pack_dst=|output_format=)(\w+)", name)

    if input_fmt and output_fmt:
        if input_fmt.group(1) == output_fmt.group(1):
            params.append(input_fmt.group(1))  # Same format
        else:
            params.append(f"{input_fmt.group(1)}_to_{output_fmt.group(1)}")
    elif input_fmt:
        params.append(input_fmt.group(1))
    elif output_fmt:
        params.append(output_fmt.group(1))

    # Look for other key parameters
    dest_acc = re.search(r"dest_acc=(\w+)", name)
    if dest_acc and dest_acc.group(1) == "Yes":
        params.append("DestAcc")

    math_fidelity = re.search(r"math_fidelity=(\w+)", name)
    if math_fidelity:
        params.append(math_fidelity.group(1))

    # Combine base name with parameters
    if params:
        sanitized = f"{base_name}_{'_'.join(params)}"
    else:
        # Fallback: clean up the original name
        sanitized = name
        # Remove verbose parameter names
        replacements = {
            "unpack_src=": "",
            "pack_dst=": "",
            "dest_acc=": "",
            "math_fidelity=": "",
            "input_format=": "",
            "output_format=": "",
            "testname=": "",
            "formats=": "",
        }

        for old, new in replacements.items():
            sanitized = sanitized.replace(old, new)

        # Clean up spacing and separators
        sanitized = re.sub(r"\s+", "_", sanitized)  # Replace spaces with underscores
        sanitized = re.sub(
            r"_+", "_", sanitized
        )  # Replace multiple underscores with single
        sanitized = sanitized.strip("_")  # Remove leading/trailing underscores

    # Clean up any remaining invalid characters
    sanitized = re.sub(r'[<>:"/\\|?*\[\]]', "_", sanitized)

    # Limit length to avoid filesystem issues
    if len(sanitized) > 60:  # Even shorter for readability
        sanitized = sanitized[:60].rstrip("_")

    return sanitized


def _update_global_test_logger():
    """Update the global test_logger variable with the current test's logger."""
    global test_logger
    current_test = current_test_context.get(None)
    if current_test:
        test_logger = logger.bind(test_instance=current_test)
    else:
        test_logger = logger


# Context managers for test-specific logging
class TestLogContext:
    """Improved context manager for test-specific logging with better parametrized test support."""

    def __init__(self, test_name: str, log_level: str = "INFO"):
        self.test_name = test_name
        self.log_level = log_level
        self.handler_id = None
        self.sanitized_name = sanitize_test_name(test_name)
        self.context_token = None

    def __enter__(self):
        global test_logger

        # Set the current test context
        self.context_token = current_test_context.set(self.test_name)

        # Update the global test_logger
        _update_global_test_logger()

        # Create log directory if it doesn't exist
        log_dir = Path("test_logs")
        log_dir.mkdir(exist_ok=True)

        # Use a unique handler for this test
        log_file = log_dir / f"{self.sanitized_name}.log"

        # Clear existing log file for this test
        if log_file.exists():
            log_file.unlink()

        self.handler_id = logger.add(
            str(log_file),
            format="{time:YYYY-MM-DD HH:mm:ss.SSS} | {level: <6} | {name}:{function}:{line} - {message}",
            level=self.log_level,
            # Filter to only include logs from this specific test context
            filter=lambda record: record["extra"].get("test_instance")
            == self.test_name,
        )

        # Return a bound logger that will be filtered to this test's log file
        return logger.bind(test_instance=self.test_name)

    def __exit__(self, exc_type, exc_val, exc_tb):
        global test_logger

        if self.handler_id:
            logger.remove(self.handler_id)
        if self.context_token:
            current_test_context.reset(self.context_token)

        # Reset the global test_logger
        _update_global_test_logger()


def get_current_test_logger():
    """
    Get a logger for the current test context.
    This allows you to use loguru anywhere without passing test_logger around.

    Returns:
        Bound logger for current test, or regular logger if no test context
    """
    current_test = current_test_context.get(None)
    if current_test:
        return logger.bind(test_instance=current_test)
    else:
        return logger
