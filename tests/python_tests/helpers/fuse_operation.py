# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
#
# SPDX-License-Identifier: Apache-2.0

from dataclasses import dataclass, field
from typing import Dict, Optional, Type

from .fuse_math import Math
from .fuse_packer import Packer
from .fuse_unpacker import Unpacker


@dataclass
class PipelineOperation:
    unpacker: Type[Unpacker]
    math: Type[Math]
    packer: Type[Packer]
    config: Dict
    _generated_config: Optional[Dict] = field(default=None, init=False, repr=False)

    @property
    def generated_config(self) -> Dict:
        if self._generated_config is None:
            from .test_config import generate_operation_config

            self._generated_config = generate_operation_config(self.config)
        return self._generated_config

    def unpack(self) -> str:
        unpacker_instance = self.unpacker()
        return unpacker_instance.unpack(self.generated_config)

    def do_math(self) -> str:
        math_instance = self.math()
        return math_instance.math(self.generated_config)

    def pack(self) -> str:
        packer_instance = self.packer()
        return packer_instance.pack(self.generated_config)
