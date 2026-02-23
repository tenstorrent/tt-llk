# SPDX-FileCopyrightText: © 2026 Tenstorrent AI ULC
#
# SPDX-License-Identifier: Apache-2.0

from dataclasses import dataclass
from typing import Union


@dataclass
class BlockData:
    block_x: Union[int, str]
    block_y: Union[int, str]
    block_tiles_x: int
    block_tiles_y: int
    tile_count_x: int
    tile_count_y: int
    full_x_limit: int
    full_y_limit: int
