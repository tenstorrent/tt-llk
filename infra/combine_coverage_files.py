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


def merge_coverage_files(coverage_dir, output_path):
    """Merge coverage files from the coverage directory."""

    if not os.path.isdir(coverage_dir):
        print(f"{coverage_dir} does not exist. Early exit.")
        return False

    # Find all .info files
    info_files = glob.glob(os.path.join(coverage_dir, "*.info"))

    if not info_files:
        print("No coverage files found")
        return False

    print(f"Found {len(info_files)} coverage files")

    merged_path = "/tmp/tt-llk-build/merged_coverage.info"

    # Initialize with first file if merged file doesn't exist
    if not os.path.isfile(merged_path):
        first_file = info_files[0]
        success, _, stderr = run_command(f"cp '{first_file}' '{merged_path}'")
        if not success:
            return False

    for info_file in info_files:
        if info_file == merged_path:
            continue

        cmd = f"lcov -q -a '{merged_path}' -a '{info_file}' -o '{merged_path}'"
        success, stdout, stderr = run_command(cmd, check=False)

        if not success:
            print(f"Warning: Failed to merge {info_file}, skipping")
            print(f"Error: {stderr}")
            continue


def main():
    coverage_dir = "/tmp/tt-llk-build/coverage_info"
    output_path = "./tests"

    success = merge_coverage_files(coverage_dir, output_path)

    sys.exit(0 if success else 1)


if __name__ == "__main__":
    main()
