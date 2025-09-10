# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
from ..environment.environment import TestEnvironment
from ..directories import get_tests_dir
from ..utils import run_shell_command
from ..directories import mkdirs
from .build import ProfilerBuild, TestBuilder, TestVariantBuild, TestVariantElfs


class MakeTestVariantBuilder(TestBuilder):

    def make(self, env: TestEnvironment, test: str, variant: str):
        args = [
            "make",
            "-j 6",
            "--silent",
            f"test={test}",
            f"variant={variant}",
            "all",
        ]

        if env.get_profiler_build() == ProfilerBuild.Yes:
            args.append("profiler")

        run_shell_command(" ".join(args), cwd=get_tests_dir())

    def build(
        self,
        env: TestEnvironment,
        test: str,
        test_config: dict,
    ):
        """Only builds the files required to run a test"""

        chip_arch = env.get_chip_arch()

        variant = self._generate_variant_id(test_config)
        variant_dir = mkdirs(
            get_tests_dir() / "build" / chip_arch.value / test / variant
        )

        include_dir = mkdirs(variant_dir / "inc")

        self.write_build_header(env, include_dir, test_config)
        self.make(env, test, variant)

        elf_dir = variant_dir / "elf"

        return TestVariantBuild(
            test=test,
            variant=variant,
            test_config=test_config,
            directory=variant_dir.path,
            build_header=include_dir / "build.h",
            elfs=TestVariantElfs(
                unpack=elf_dir / "unpack.elf",
                math=elf_dir / "math.elf",
                pack=elf_dir / "pack.elf",
            ),
            profiler_meta=mkdirs(elf_dir / "profiler"),
            assert_meta=mkdirs(elf_dir / "assert"),
        )
