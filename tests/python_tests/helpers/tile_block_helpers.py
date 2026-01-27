# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
#
# SPDX-License-Identifier: Apache-2.0

"""
Helper functions for calculating tile block parameters for destination register banking.

This module provides utilities to compute NUM_TILES_IN_BLOCK and NUM_BLOCKS
based on data format and total tile count, ensuring proper destination register
bank switching during test execution.
"""

from helpers.format_config import DataFormat


def get_max_tiles_in_dest(data_format: DataFormat) -> int:
    """
    Get the maximum number of tiles that can fit in a destination register bank.

    The destination register bank size depends on the data format:
    - Float32/Int32 formats: 4 tiles per bank (32-bit formats take more space)
    - All other formats: 8 tiles per bank (16-bit and smaller formats)

    Args:
        data_format: The data format being used

    Returns:
        Maximum number of tiles that fit in one destination register bank
    """
    if data_format == DataFormat.Float32 or data_format == DataFormat.Int32:
        return 4
    else:
        return 8


def calculate_num_blocks_and_tiles(
    total_tiles: int, data_format: DataFormat, mode: str = "default"
) -> tuple[int, int]:
    """
    Calculate the number of blocks and tiles per block for destination register banking.

    This function computes how to distribute tiles across destination register banks
    based on the total number of tiles and data format.

    Args:
        total_tiles: Total number of tiles to process
        data_format: The data format being used (affects max tiles per bank)
        mode: Calculation mode - "default" for most operations, "untilize" for special untilize handling

    Returns:
        Tuple of (num_blocks, num_tiles_in_block) where:
        - num_blocks: Number of destination register bank switches needed
        - num_tiles_in_block: Number of tiles in the last block

    Examples:
        >>> calculate_num_blocks_and_tiles(10, DataFormat.Float32)
        (3, 2)  # 3 blocks: 4 tiles, 4 tiles, 2 tiles

        >>> calculate_num_blocks_and_tiles(16, DataFormat.Float16_b)
        (2, 8)  # 2 blocks: 8 tiles, 8 tiles
    """
    max_tiles_in_block = get_max_tiles_in_dest(data_format)

    if mode == "untilize":
        # For untilize operations, the block size is calculated differently
        # based on row tiles rather than total tiles.
        # This is a placeholder - actual implementation would need input dimensions
        # to calculate tiles per row. For now, use default behavior.
        # TODO: Enhance this when untilize tests are updated
        pass

    # Calculate number of blocks (ceiling division)
    num_blocks = (total_tiles + max_tiles_in_block - 1) // max_tiles_in_block

    # Calculate tiles in the last block
    # If total_tiles is evenly divisible, last block has max_tiles_in_block tiles
    # Otherwise, it has the remainder
    num_tiles_in_block = total_tiles % max_tiles_in_block or max_tiles_in_block

    return num_blocks, num_tiles_in_block


def calculate_num_blocks_and_tiles_for_untilize(
    input_dimensions: list[int], tile_dimensions: list[int], data_format: DataFormat
) -> tuple[int, int]:
    """
    Calculate block parameters specifically for untilize operations.

    Untilize operations process tiles row-by-row, so the block size is determined
    by the number of tiles in a single row, not the total tile count.

    Args:
        input_dimensions: [rows, cols] of the input matrix in elements
        tile_dimensions: [tile_rows, tile_cols] dimensions of each tile
        data_format: The data format being used

    Returns:
        Tuple of (num_blocks, num_tiles_in_block) optimized for untilize

    Examples:
        >>> # 10x3 tiles (320x96 elements with 32x32 tiles)
        >>> calculate_num_blocks_and_tiles_for_untilize([320, 96], [32, 32], DataFormat.Float16_b)
        (10, 3)  # Process 3 tiles at a time, 10 times (once per row)
    """
    max_tiles_in_block = get_max_tiles_in_dest(data_format)

    # Calculate number of tile rows and columns
    tile_rows, tile_cols = tile_dimensions
    tiles_per_row = input_dimensions[1] // tile_cols
    num_tile_rows = input_dimensions[0] // tile_rows

    # For untilize, block size is based on tiles per row, capped at max_tiles_in_block
    # We want to process as many tiles per row as possible in each block
    if tiles_per_row <= max_tiles_in_block:
        # Can fit entire row in one block
        num_tiles_in_block = tiles_per_row
        num_blocks = num_tile_rows
    else:
        # Need multiple blocks per row
        blocks_per_row = (tiles_per_row + max_tiles_in_block - 1) // max_tiles_in_block
        num_blocks = num_tile_rows * blocks_per_row
        # Last block in each row may have fewer tiles
        num_tiles_in_block = tiles_per_row % max_tiles_in_block or max_tiles_in_block

    return num_blocks, num_tiles_in_block
