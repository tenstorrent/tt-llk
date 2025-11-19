# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
#
# SPDX-License-Identifier: Apache-2.0

from dataclasses import dataclass
from typing import Dict, Type

from .fuse_math import Math
from .fuse_packer import Packer
from .fuse_unpacker import Unpacker


@dataclass
class PipelineOperation:
    unpacker: Type[Unpacker]
    math: Type[Math]
    packer: Type[Packer]
    config: Dict

    def unpack(self) -> str:
        unpacker_instance = self.unpacker()
        return unpacker_instance.unpack(self.config)

    def do_math(self) -> str:
        math_instance = self.math()
        return math_instance.math(self.config)

    def pack(self) -> str:
        packer_instance = self.packer()
        return packer_instance.pack(self.config)
