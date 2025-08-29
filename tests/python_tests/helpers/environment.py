# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
from helpers.chip_architecture import ChipArchitecture, get_chip_architecture
from helpers.directories import TestDir, VariantDir, get_test_dir


class TestEnvironment:

    def get_chip_arch(self) -> ChipArchitecture:
        return get_chip_architecture()

    def test_dir(self, test_name: str) -> TestDir:
        return get_test_dir(self.get_chip_arch(), test_name=test_name)

    def variant_dir(self, test_name: str, variant: str) -> VariantDir:
        return self.test_dir(test_name).variant_dir(variant)
