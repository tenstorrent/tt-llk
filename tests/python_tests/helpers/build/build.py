# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
from ..device import BootMode
from ..build import ProfilerBuild
from ..chip_architecture import ChipArchitecture

from dataclasses import dataclass
import hashlib
import json
from pathlib import Path

from helpers.environment import TestEnvironment
from helpers.test_config import generate_build_header
from helpers.device import BootMode


@dataclass(frozen=True)
class TestVariantElfs:
    brisc: Path
    unpack: Path
    math: Path
    pack: Path


@dataclass(frozen=True)
class TestVariantBuild:
    test: str
    variant: str
    test_config: dict

    directory: Path

    build_header: Path

    elfs: TestVariantElfs
    profiler_meta: list[str]
    assert_meta: list[str]


class TestVariantBuilder:

    def _generate_variant_id(self, test_config: dict) -> str:
        # todo: find a lightweight way to do this
        json_str = json.dumps(test_config.to_dict(), sort_keys=True)
        config_hash = hashlib.sha1(json_str.encode(), usedforsecurity=False)
        return config_hash.hexdigest()

    def write_build_header(
        self, env: TestEnvironment, include_dir: Path, test_config: dict
    ):
        header_content = generate_build_header(
            test_config,
            profiler_build=env.get_profiler_build(),
            boot_mode=env.get_boot_mode(),
        )

        with open(include_dir / "build.h", "w") as f:
            f.write(header_content)

    def build(
        self,
        chip_architecture: ChipArchitecture,
        test: str,
        test_config: dict,
        profiler_build: ProfilerBuild,
        boot_mode: BootMode,
    ):
        raise "Not implemented"
