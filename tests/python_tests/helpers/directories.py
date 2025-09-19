# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
import os
from pathlib import Path

# This file must track the makefile structure


def mkdirs(path: Path) -> Path:
    path.mkdir(parents=True, exist_ok=True)
    return path


def get_root_dir():
    root = os.environ.get("LLK_HOME")
    if root is None:
        raise AssertionError(
            "Environment variable LLK_HOME must be set to the root of the project"
        )

    return Path(root)


def get_tests_dir():
    return get_root_dir() / "tests"
