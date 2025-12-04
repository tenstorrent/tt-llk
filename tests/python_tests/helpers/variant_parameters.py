# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
#
# SPDX-License-Identifier: Apache-2.0

from dataclasses import dataclass

# === Template parameters ===


@dataclass
class TemplateParameter:
    """Base class for template parameters that can generate C++ code"""

    def generate(self) -> str:
        """Override this to generate the C++ code"""
        raise NotImplementedError


# === Runtime parameters ===


@dataclass
class RuntimeParameter:
    """Base class for runtime parameters that can generate C++ code"""

    def generate(self) -> str:
        """Override this to generate the C++ code"""
        raise NotImplementedError
