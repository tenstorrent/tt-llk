# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0

"""Centralized logging configuration using loguru for the testing framework."""

import contextvars
import sys
from pathlib import Path
from typing import Optional

from loguru import logger

# Context variable to track current test
current_test_context = contextvars.ContextVar("current_test", default="")

# Global test_logger variable that gets updated automatically
test_logger = logger  # Default to regular logger


def setup_test_logging(
    log_level: str = "INFO",
    log_file: Optional[str] = None,
    enable_console: bool = True,
    enable_colors: bool = True,
    test_name: Optional[str] = None,
    cleanup_empty_files: bool = True,
) -> None:
    """
    Configure loguru logging for the testing framework.

    Args:
        log_level: Logging level (DEBUG, INFO, WARNING, ERROR, CRITICAL)
        log_file: Optional file path for logging output
        enable_console: Whether to log to console
        enable_colors: Whether to use colored output
        test_name: Optional test name for contextualized logging
        cleanup_empty_files: Whether to clean up empty log files from previous runs
    """
    # Clean up empty log files from previous runs
    if cleanup_empty_files:
        removed = cleanup_empty_log_files()
        if removed > 0:
            logger.debug(f"Cleaned up {removed} empty log files from previous runs")

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


def sanitize_test_name(name: str) -> str:
    """Sanitize test name for use in file names, including all parameters with their values."""
    import re

    # Extract the base test name (everything before the first parameter)
    base_match = re.match(r"^(test_\w+)", name)
    base_name = base_match.group(1) if base_match else "test"

    # Find all parameter=value pairs in the test name
    # Pattern matches: parameter_name=value where value can be alphanumeric, underscores, or dots
    param_pattern = r"(\w+)=([A-Za-z0-9_\.]+)"
    param_matches = re.findall(param_pattern, name)

    if not param_matches:
        # Fallback: if no parameters found, clean up the original name
        sanitized = name
        # Clean up spacing and separators
        sanitized = re.sub(r"\s+", "_", sanitized)  # Replace spaces with underscores
        sanitized = re.sub(
            r"_+", "_", sanitized
        )  # Replace multiple underscores with single
        sanitized = sanitized.strip("_")  # Remove leading/trailing underscores
        # Clean up any remaining invalid characters
        sanitized = re.sub(r'[<>:"/\\|?*\[\]]', "_", sanitized)
        return sanitized[:100].rstrip("_")  # Reasonable length limit

    # Build parameter string with all parameters
    param_parts = []
    for param_name, param_value in param_matches:
        # Convert common values to more readable formats
        if param_value.lower() in ["yes", "true", "on"]:
            param_parts.append(f"{param_name}_Yes")
        elif param_value.lower() in ["no", "false", "off"]:
            param_parts.append(f"{param_name}_No")
        elif param_value.startswith("Turned"):
            # Handle "Turned On" case
            param_parts.append(f"{param_name}_TurnedOn")
        else:
            # For other values, use them directly
            param_parts.append(f"{param_name}_{param_value}")

    # Combine base name with all parameters
    if param_parts:
        sanitized = f"{base_name}__{'__'.join(param_parts)}"
    else:
        sanitized = base_name

    # Clean up any remaining invalid characters for filenames
    sanitized = re.sub(r'[<>:"/\\|?*\[\]]', "_", sanitized)

    # Replace multiple underscores with double underscores for readability
    sanitized = re.sub(r"___+", "__", sanitized)  # Replace 3+ underscores with double

    # Limit length to avoid filesystem issues while preserving information
    if len(sanitized) > 200:  # Generous limit to include all params
        # If too long, try to truncate from the middle while keeping important parts
        parts = sanitized.split("__")
        if len(parts) > 4:
            # Keep first 2 and last 2 parts, indicate truncation
            sanitized = f"{parts[0]}__{parts[1]}__TRUNCATED__{parts[-2]}__{parts[-1]}"
        if len(sanitized) > 200:
            sanitized = sanitized[:200].rstrip("_")

    return sanitized


def _update_global_test_logger():
    """Update the global test_logger variable with the current test's logger."""
    global test_logger
    current_test = current_test_context.get("")
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
        self.log_file_path = None

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
        self.log_file_path = log_dir / f"{self.sanitized_name}.log"

        # Only remove existing file if it exists (but don't create it yet)
        if self.log_file_path.exists():
            self.log_file_path.unlink()

        self.handler_id = logger.add(
            str(self.log_file_path),
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

        # Log any exception that occurred during the test
        if exc_type is not None:
            bound_logger = logger.bind(test_instance=self.test_name)

            if exc_type == AssertionError:
                bound_logger.error(f"ASSERTION FAILURE in test '{self.test_name}'")
                bound_logger.error(f"Assertion Error: {exc_val}")

                # Try to get more context about the assertion
                if exc_tb:
                    import traceback

                    # Get the last frame in the test (not in this context manager)
                    tb_lines = traceback.format_tb(exc_tb)
                    if tb_lines:
                        # Find the assertion line
                        for line in tb_lines:
                            if "assert" in line.lower():
                                bound_logger.error(f"Failed assertion: {line.strip()}")
                                break
                        else:
                            # If we didn't find an assert line, log the last traceback line
                            bound_logger.error(
                                f"Error location: {tb_lines[-1].strip()}"
                            )

                    # Log the full traceback for debugging
                    bound_logger.error("Full traceback:")
                    for line in tb_lines:
                        for sub_line in line.rstrip().split("\n"):
                            if sub_line.strip():
                                bound_logger.error(f"  {sub_line}")

            elif exc_type in (RuntimeError, ValueError, TypeError):
                bound_logger.error(f"TEST ERROR in test '{self.test_name}'")
                bound_logger.error(f"{exc_type.__name__}: {exc_val}")

                if exc_tb:
                    import traceback

                    tb_lines = traceback.format_tb(exc_tb)
                    bound_logger.error("Error traceback:")
                    for line in tb_lines:
                        for sub_line in line.rstrip().split("\n"):
                            if sub_line.strip():
                                bound_logger.error(f"  {sub_line}")
            else:
                # For other exceptions, log them generically
                bound_logger.error(f"UNEXPECTED ERROR in test '{self.test_name}'")
                bound_logger.error(f"{exc_type.__name__}: {exc_val}")

        if self.handler_id:
            logger.remove(self.handler_id)

        # Clean up empty log files after test completion (only if no exception occurred)
        if exc_type is None:
            self._cleanup_empty_log_file()

        if self.context_token:
            current_test_context.reset(self.context_token)

        # Reset the global test_logger
        _update_global_test_logger()

        # Don't suppress the exception - let pytest handle it normally
        return False

    def _cleanup_empty_log_file(self):
        """Remove the log file if it's empty or only contains whitespace."""
        if self.log_file_path and self.log_file_path.exists():
            try:
                # Check if file is empty or contains only whitespace
                content = self.log_file_path.read_text().strip()
                if not content:
                    self.log_file_path.unlink()
                    # Optional: log the cleanup action to a central log
                    logger.debug(f"Removed empty log file: {self.log_file_path}")
            except Exception as e:
                # Don't fail the test if cleanup fails
                logger.warning(f"Failed to cleanup log file {self.log_file_path}: {e}")


def get_current_test_logger():
    """
    Get a logger for the current test context.
    This allows you to use loguru anywhere without passing test_logger around.

    Returns:
        Bound logger for current test, or regular logger if no test context
    """
    current_test = current_test_context.get("")
    if current_test:
        return logger.bind(test_instance=current_test)
    else:
        return logger


def cleanup_empty_log_files(log_directory: str = "test_logs") -> int:
    """
    Clean up empty log files from the specified directory.

    Args:
        log_directory: Directory to clean up (default: "test_logs")

    Returns:
        Number of files removed
    """
    log_dir = Path(log_directory)
    if not log_dir.exists():
        return 0

    removed_count = 0
    for log_file in log_dir.glob("*.log"):
        try:
            if log_file.is_file():
                content = log_file.read_text().strip()
                if not content:
                    log_file.unlink()
                    removed_count += 1
                    logger.debug(f"Removed empty log file: {log_file}")
        except Exception as e:
            logger.warning(f"Failed to check/remove log file {log_file}: {e}")

    return removed_count
