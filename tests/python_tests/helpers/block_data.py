# SPDX-FileCopyrightText: © 2026 Tenstorrent AI ULC
#
# SPDX-License-Identifier: Apache-2.0

from dataclasses import dataclass
from typing import Union


@dataclass
class BlockData:
    block_x: Union[int, str]
    block_y: Union[int, str]
    block_tiles_x: Union[int, str]
    block_tiles_y: Union[int, str]
    tile_count_x: Union[int, str]
    tile_count_y: Union[int, str]
    full_x_limit: Union[int, str]
    full_y_limit: Union[int, str]
    tile_id_global: Union[int, str]
    tile_id_block: Union[int, str]
