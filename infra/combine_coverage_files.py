#!/usr/bin/env python3
# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
#
# SPDX-License-Identifier: Apache-2.0

import glob
import os
import subprocess
import sys


def run_command(cmd, check=True):
    """Run a shell command and return the result."""
    try:
        result = subprocess.run(
            cmd, shell=True, check=check, capture_output=True, text=True
        )
        return result.returncode == 0, result.stdout, result.stderr
    except subprocess.CalledProcessError as e:
        return False, e.stdout, e.stderr


def merge_coverage_files(coverage_dir):
    """Merge coverage files from the coverage directory."""

    if not os.path.isdir(coverage_dir):
        print(f"{coverage_dir} does not exist. Early exit.")
        return True

    info_files = glob.glob(os.path.join(coverage_dir, "*.info"))

    if not info_files:
        print("No coverage files found, exiting . . .")
        return True

    print(f"Found {len(info_files)} coverage files")

    merged_path = "/tmp/tt-llk-build/merged_coverage.info"

    if not os.path.isfile(merged_path):
        first_file = info_files[0]
        success, _, stderr = run_command(f"cp '{first_file}' '{merged_path}'")
        if not success:
            print(f"Failed to initialize: {stderr}")
            return False

    info_files.pop(0)

    for info_file in info_files:
        cmd = f"lcov -q -a {merged_path} -a {info_file} -o {merged_path}"
        success, _, stderr = run_command(cmd, check=False)

        if not success:
            print(f"Warning: Failed to merge {info_file}, skipping")
            print(f"Error: {stderr}")
            continue

    return True


def main():
    coverage_dir = "/tmp/tt-llk-build/coverage_info"

    print(f"\n\n--- Combining coverage data from {coverage_dir} ---")
    success = merge_coverage_files(coverage_dir)

    sys.exit(0 if success else 1)


if __name__ == "__main__":
    main()
