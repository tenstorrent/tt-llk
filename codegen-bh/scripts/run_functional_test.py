#!/usr/bin/env python3
# SPDX-FileCopyrightText: (c) 2026 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0

"""
Run functional tests for Blackhole LLK kernels.

This script is focused ONLY on running functional tests.
For compilation checking, use check_compile.py.

Usage:
    # Run functional tests for a specific kernel
    python scripts/run_functional_test.py sigmoid --arch blackhole

    # Run with specific data format
    python scripts/run_functional_test.py exp --format Float16_b --arch blackhole

    # Run specific test cases only (quick smoke test)
    python scripts/run_functional_test.py relu --quick --arch blackhole

    # List available tests
    python scripts/run_functional_test.py --list --arch blackhole

    # Verbose output
    python scripts/run_functional_test.py tanh -v --arch blackhole
"""

import argparse
import subprocess
import sys
from dataclasses import dataclass
from enum import Enum
from pathlib import Path
from typing import Optional


class KernelType(Enum):
    """Types of LLK kernels."""

    SFPU = "sfpu"
    MATH = "math"
    PACK = "pack"
    UNPACK = "unpack"


@dataclass
class FunctionalTestResult:
    """Result of functional testing."""

    kernel: str
    test_file: str
    passed: bool
    total_tests: int
    passed_tests: int
    failed_tests: int
    output: str
    return_code: int
    arch: str = "blackhole"

    def summary(self) -> str:
        """Generate human-readable summary."""
        status = "PASSED" if self.passed else "FAILED"
        return (
            f"Functional Test: {self.kernel}\n"
            f"  Architecture: {self.arch}\n"
            f"  Test file: {self.test_file}\n"
            f"  Status: {status}\n"
            f"  Tests: {self.passed_tests}/{self.total_tests} passed"
        )


# Test directory paths
TESTS_DIR = Path(__file__).parent.parent.parent / "tests"
PYTHON_TESTS_DIR = TESTS_DIR / "python_tests"


# Mapping of kernel names to their types
KERNEL_TYPES = {
    # SFPU operations
    "sigmoid": KernelType.SFPU,
    "relu": KernelType.SFPU,
    "exp": KernelType.SFPU,
    "gelu": KernelType.SFPU,
    "tanh": KernelType.SFPU,
    "sqrt": KernelType.SFPU,
    "recip": KernelType.SFPU,
    "reciprocal": KernelType.SFPU,
    "rsqrt": KernelType.SFPU,
    "square": KernelType.SFPU,
    # Math operations
    "reduce": KernelType.MATH,
    "matmul": KernelType.MATH,
    "eltwise_binary": KernelType.MATH,
    "eltwise_unary": KernelType.MATH,
    # Pack operations
    "pack": KernelType.PACK,
    "pack_untilize": KernelType.PACK,
    # Unpack operations
    "unpack_tilize": KernelType.UNPACK,
}


def get_test_info(kernel_name: str, arch: str = "blackhole") -> Optional[dict]:
    """Get test file and filter for a kernel on given architecture."""
    name = kernel_name.lower()

    # Blackhole-specific test mappings
    # Note: These may need to be updated based on actual test availability
    blackhole_tests = {
        # SFPU operations - general test files
        "exp": {"file": f"test_sfpu_exp.py", "filter": None},
        "relu": {"file": f"test_sfpu_relu.py", "filter": None},
        "reciprocal": {"file": f"test_sfpu_recip.py", "filter": None},
        "recip": {"file": f"test_sfpu_recip.py", "filter": None},
        "sqrt": {"file": f"test_sfpu_sqrt.py", "filter": None},
        "tanh": {"file": f"test_sfpu_tanh.py", "filter": None},
        "sigmoid": {"file": f"test_sfpu_sigmoid.py", "filter": None},
        "gelu": {"file": f"test_sfpu_gelu.py", "filter": None},
        "rsqrt": {"file": f"test_sfpu_rsqrt.py", "filter": None},
        "square": {"file": f"test_sfpu_square.py", "filter": None},
        # Math tests
        "reduce": {"file": f"test_reduce.py", "filter": None},
        "matmul": {"file": f"test_matmul.py", "filter": None},
        "eltwise_binary": {"file": f"test_eltwise_binary.py", "filter": None},
        "eltwise_unary": {"file": f"test_eltwise_unary_datacopy.py", "filter": None},
        # Pack tests
        "pack": {"file": f"test_pack.py", "filter": None},
        "pack_untilize": {"file": f"test_pack_untilize.py", "filter": None},
        # Unpack tests
        "unpack_tilize": {"file": f"test_unpack_tilize.py", "filter": None},
    }

    if name in blackhole_tests:
        return blackhole_tests[name]

    # Try to find a matching test file
    pattern = f"test_*{name}*.py"
    matches = list(PYTHON_TESTS_DIR.glob(pattern))
    if matches:
        return {"file": matches[0].name, "filter": None}

    return None


def run_functional_test(
    kernel_name: str,
    arch: str = "blackhole",
    data_format: Optional[str] = None,
    quick: bool = False,
    verbose: bool = False,
    extra_args: Optional[list] = None,
) -> FunctionalTestResult:
    """
    Run functional tests for a kernel.

    Args:
        kernel_name: Name of the kernel (e.g., "sigmoid", "exp")
        arch: Architecture (blackhole or wormhole)
        data_format: Specific data format to test (e.g., "Float16_b")
        quick: Run minimal test cases for quick validation
        verbose: Enable verbose output
        extra_args: Additional pytest arguments

    Returns:
        FunctionalTestResult with test outcomes
    """
    test_info = get_test_info(kernel_name, arch)

    if not test_info:
        return FunctionalTestResult(
            kernel=kernel_name,
            test_file="",
            passed=False,
            total_tests=0,
            passed_tests=0,
            failed_tests=0,
            output=f"No functional tests found for '{kernel_name}' on {arch}",
            return_code=-1,
            arch=arch,
        )

    test_file = test_info["file"]
    test_filter = test_info.get("filter")

    test_path = PYTHON_TESTS_DIR / test_file
    if not test_path.exists():
        # Try looking in subdirectories
        alt_paths = list(PYTHON_TESTS_DIR.rglob(test_file))
        if alt_paths:
            test_path = alt_paths[0]
            test_file = str(test_path.relative_to(PYTHON_TESTS_DIR))
        else:
            return FunctionalTestResult(
                kernel=kernel_name,
                test_file=test_file,
                passed=False,
                total_tests=0,
                passed_tests=0,
                failed_tests=0,
                output=f"Test file not found: {test_file}. Tests may not be available for this kernel on {arch}.",
                return_code=-1,
                arch=arch,
            )

    # Build pytest command
    cmd = ["pytest", test_file]

    if verbose:
        cmd.append("-v")

    # Build filter expression
    filter_parts = []
    if test_filter:
        filter_parts.append(test_filter)
    if data_format:
        filter_parts.append(data_format)
    if quick:
        # For quick mode, test only one format and small dimensions
        filter_parts.append("32, 32")  # Small input dimensions

    if filter_parts:
        cmd.extend(["-k", " and ".join(filter_parts)])

    # Add timeout
    cmd.extend(["--timeout", "300"])

    # Set architecture via environment or marker if needed
    # This depends on how the test infrastructure handles architecture selection

    # Add extra args
    if extra_args:
        cmd.extend(extra_args)

    print(f"Running: {' '.join(cmd)}")
    print(f"Working directory: {PYTHON_TESTS_DIR}")
    print(f"Architecture: {arch}")
    print("-" * 60)

    try:
        env = {"ARCH_NAME": arch}  # Set architecture in environment
        import os

        full_env = os.environ.copy()
        full_env.update(env)

        result = subprocess.run(
            cmd,
            cwd=PYTHON_TESTS_DIR,
            capture_output=True,
            text=True,
            timeout=600,  # 10 minute timeout
            env=full_env,
        )

        output = result.stdout + result.stderr

        # Parse test counts from pytest output
        total, passed, failed = _parse_pytest_output(output)

        return FunctionalTestResult(
            kernel=kernel_name,
            test_file=test_file,
            passed=result.returncode == 0,
            total_tests=total,
            passed_tests=passed,
            failed_tests=failed,
            output=output,
            return_code=result.returncode,
            arch=arch,
        )

    except subprocess.TimeoutExpired:
        return FunctionalTestResult(
            kernel=kernel_name,
            test_file=test_file,
            passed=False,
            total_tests=0,
            passed_tests=0,
            failed_tests=0,
            output="Tests timed out after 10 minutes",
            return_code=-1,
            arch=arch,
        )
    except Exception as e:
        return FunctionalTestResult(
            kernel=kernel_name,
            test_file=test_file,
            passed=False,
            total_tests=0,
            passed_tests=0,
            failed_tests=0,
            output=f"Error running tests: {e}",
            return_code=-1,
            arch=arch,
        )


def _parse_pytest_output(output: str) -> tuple[int, int, int]:
    """Parse pytest output to extract test counts."""
    import re

    # Look for patterns like "5 passed", "2 failed", "1 error"
    passed = 0
    failed = 0
    errors = 0

    passed_match = re.search(r"(\d+) passed", output)
    if passed_match:
        passed = int(passed_match.group(1))

    failed_match = re.search(r"(\d+) failed", output)
    if failed_match:
        failed = int(failed_match.group(1))

    error_match = re.search(r"(\d+) error", output)
    if error_match:
        errors = int(error_match.group(1))

    total = passed + failed + errors
    return total, passed, failed + errors


def list_available_tests(arch: str = "blackhole"):
    """List all available functional tests."""
    print(f"Available functional tests for {arch}:\n")

    # Group by kernel type
    by_type: dict[KernelType, list[str]] = {}
    for kernel, ktype in KERNEL_TYPES.items():
        if ktype not in by_type:
            by_type[ktype] = []
        by_type[ktype].append(kernel)

    for ktype in KernelType:
        kernels = by_type.get(ktype, [])
        if not kernels:
            continue

        print(f"{ktype.value.upper()} kernels:")
        for kernel in sorted(set(kernels)):
            test_info = get_test_info(kernel, arch)
            if test_info:
                test_file = test_info["file"]
                # Check if test file exists
                test_path = PYTHON_TESTS_DIR / test_file
                if not test_path.exists():
                    alt_paths = list(PYTHON_TESTS_DIR.rglob(test_file))
                    if alt_paths:
                        status = "found"
                    else:
                        status = "(test file not found)"
                else:
                    status = "found"
                print(f"  {kernel:15} -> {test_file} {status}")
            else:
                print(f"  {kernel:15} -> No test defined")
        print()


def main():
    parser = argparse.ArgumentParser(
        description="Run functional tests for Blackhole LLK kernels",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
    # Run all tests for sigmoid
    python scripts/run_functional_test.py sigmoid --arch blackhole

    # Quick smoke test
    python scripts/run_functional_test.py exp --quick --arch blackhole

    # Test specific format
    python scripts/run_functional_test.py relu --format Float16_b --arch blackhole

    # List available tests
    python scripts/run_functional_test.py --list --arch blackhole

    # Verbose output
    python scripts/run_functional_test.py tanh -v --arch blackhole
""",
    )

    parser.add_argument("kernel", nargs="?", help="Kernel name to test")
    parser.add_argument(
        "--list", "-l", action="store_true", help="List available tests"
    )
    parser.add_argument(
        "--arch",
        "-a",
        default="blackhole",
        choices=["blackhole", "wormhole"],
        help="Target architecture",
    )
    parser.add_argument("--format", "-f", help="Specific data format (e.g., Float16_b)")
    parser.add_argument(
        "--quick", "-q", action="store_true", help="Quick smoke test (minimal cases)"
    )
    parser.add_argument("--verbose", "-v", action="store_true", help="Verbose output")
    parser.add_argument("extra", nargs="*", help="Extra pytest arguments")

    args = parser.parse_args()

    if args.list:
        list_available_tests(args.arch)
        return 0

    if not args.kernel:
        parser.print_help()
        print("\nError: Please specify a kernel name or use --list")
        return 1

    result = run_functional_test(
        args.kernel,
        arch=args.arch,
        data_format=args.format,
        quick=args.quick,
        verbose=args.verbose,
        extra_args=args.extra,
    )

    print(f"\n{'='*60}")
    print(result.summary())
    print(f"{'='*60}")

    if not result.passed and args.verbose:
        print("\nFull output:")
        print(result.output)

    return 0 if result.passed else 1


if __name__ == "__main__":
    sys.exit(main())
