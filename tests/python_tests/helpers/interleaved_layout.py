# SPDX-FileCopyrightText: © 2026 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0

"""
Helper functions for 4-interface pack_untilize optimization tests.

This module provides utilities to convert tile data from standard layout
to interleaved layout required by the 4-interface optimization.
"""

from typing import List

import torch


def convert_to_interleaved_layout(
    tiles: List[torch.Tensor],
    num_faces: int = 4,
    face_r_dim: int = 16,
    face_c_dim: int = 16,
) -> List[torch.Tensor]:
    """
    Convert tiles from standard layout to interleaved layout for 4-interface optimization.

    Standard layout (tile-by-tile, each tile has 4 faces stored sequentially):
        W=0: T0 (F0 F1 F2 F3) | W=1: T1 (F0 F1 F2 F3) | W=2: T2 (F0 F1 F2 F3) | ...

    Interleaved layout (by tile pairs, faces from 2 tiles arranged in one 32x32 tile):
        W=0: (T0F0 T0F1 T1F0 T1F1) | W=1: (T0F2 T0F3 T1F2 T1F3) | ...

    For block_ct_dim=2 (2 tiles T0, T1):
        W=0: 32x32 tile with F0=T0F0, F1=T0F1, F2=T1F0, F3=T1F1
        W=1: 32x32 tile with F0=T0F2, F1=T0F3, F2=T1F2, F3=T1F3

    This enables 4-interface PACR at W=0 to read:
        - Intf0 (offset 0): T0F0 row
        - Intf1 (offset 256): T0F1 row
        - Intf2 (offset 512): T1F0 row
        - Intf3 (offset 768): T1F1 row
    Producing one complete row from both T0 and T1 (64 datums total).

    Args:
        tiles: List of tiles in standard layout, each tile shape [32, 32]
        num_faces: Number of faces per tile (4 for full 32x32 tiles)
        face_r_dim: Rows per face (16 for standard 32x32 tiles)
        face_c_dim: Columns per face (16 for standard 32x32 tiles)

    Returns:
        List of 32x32 tiles in interleaved layout. For N input tiles, returns N output tiles
        (each containing faces from 2 original tiles, except possible remainder).
    """
    assert (
        num_faces == 4
    ), "4-interface optimization currently only supports num_faces == 4"
    assert all(
        t.shape == torch.Size([32, 32]) for t in tiles
    ), "All tiles must be 32x32"

    num_tiles = len(tiles)
    interleaved_tiles = []

    # Process tiles in pairs
    for pair_idx in range(0, num_tiles, 2):
        if pair_idx + 1 < num_tiles:
            # We have a complete pair
            t0 = tiles[pair_idx]
            t1 = tiles[pair_idx + 1]

            # Extract faces from both tiles
            # Face layout in standard 32x32 tile:
            #   F0 (upper-left):  [0:16, 0:16]
            #   F1 (upper-right): [0:16, 16:32]
            #   F2 (lower-left):  [16:32, 0:16]
            #   F3 (lower-right): [16:32, 16:32]

            t0_f0 = t0[0:16, 0:16]  # T0 Face 0
            t0_f1 = t0[0:16, 16:32]  # T0 Face 1
            t0_f2 = t0[16:32, 0:16]  # T0 Face 2
            t0_f3 = t0[16:32, 16:32]  # T0 Face 3

            t1_f0 = t1[0:16, 0:16]  # T1 Face 0
            t1_f1 = t1[0:16, 16:32]  # T1 Face 1
            t1_f2 = t1[16:32, 0:16]  # T1 Face 2
            t1_f3 = t1[16:32, 16:32]  # T1 Face 3

            # Create interleaved tiles (32x32 each)
            # Tile 0 of pair: Contains top faces from both original tiles
            # Spatial arrangement: T0F0 | T0F1
            #                      T1F0 | T1F1
            interleaved_tile_0 = torch.cat(
                [
                    torch.cat([t0_f0, t0_f1], dim=1),  # Top half: T0F0 | T0F1
                    torch.cat([t1_f0, t1_f1], dim=1),  # Bottom half: T1F0 | T1F1
                ],
                dim=0,
            )

            # Tile 1 of pair: Contains bottom faces from both original tiles
            # Spatial arrangement: T0F2 | T0F3
            #                      T1F2 | T1F3
            interleaved_tile_1 = torch.cat(
                [
                    torch.cat([t0_f2, t0_f3], dim=1),  # Top half: T0F2 | T0F3
                    torch.cat([t1_f2, t1_f3], dim=1),  # Bottom half: T1F2 | T1F3
                ],
                dim=0,
            )

            interleaved_tiles.append(interleaved_tile_0)
            interleaved_tiles.append(interleaved_tile_1)

        else:
            # Odd tile at the end, keep in standard layout (4 faces as-is)
            interleaved_tiles.append(tiles[pair_idx])

    return interleaved_tiles


def convert_from_interleaved_layout(
    interleaved_tiles: List[torch.Tensor],
    num_faces: int = 4,
    face_r_dim: int = 16,
    face_c_dim: int = 16,
) -> List[torch.Tensor]:
    """
    Convert tiles from interleaved layout back to standard layout.

    This is the inverse operation of convert_to_interleaved_layout.
    Useful for verification and debugging.

    Args:
        interleaved_tiles: List of 32x32 tiles in interleaved layout
        num_faces: Number of faces per tile (4 for full 32x32 tiles)
        face_r_dim: Rows per face (16 for standard 32x32 tiles)
        face_c_dim: Columns per face (16 for standard 32x32 tiles)

    Returns:
        List of 32x32 tiles in standard layout
    """
    assert (
        num_faces == 4
    ), "4-interface optimization currently only supports num_faces == 4"
    assert all(
        t.shape == torch.Size([32, 32]) for t in interleaved_tiles
    ), "All tiles must be 32x32"

    num_interleaved_tiles = len(interleaved_tiles)
    standard_tiles = []

    # Process interleaved tiles in pairs
    for pair_idx in range(0, num_interleaved_tiles, 2):
        if pair_idx + 1 < num_interleaved_tiles:
            # We have a pair of interleaved tiles
            interleaved_0 = interleaved_tiles[
                pair_idx
            ]  # Contains T0F0, T0F1, T1F0, T1F1
            interleaved_1 = interleaved_tiles[
                pair_idx + 1
            ]  # Contains T0F2, T0F3, T1F2, T1F3

            # Extract faces from interleaved tiles
            t0_f0 = interleaved_0[0:16, 0:16]  # Upper-left
            t0_f1 = interleaved_0[0:16, 16:32]  # Upper-right
            t1_f0 = interleaved_0[16:32, 0:16]  # Lower-left
            t1_f1 = interleaved_0[16:32, 16:32]  # Lower-right

            t0_f2 = interleaved_1[0:16, 0:16]  # Upper-left
            t0_f3 = interleaved_1[0:16, 16:32]  # Upper-right
            t1_f2 = interleaved_1[16:32, 0:16]  # Lower-left
            t1_f3 = interleaved_1[16:32, 16:32]  # Lower-right

            # Reconstruct standard tiles
            t0 = torch.cat(
                [
                    torch.cat([t0_f0, t0_f1], dim=1),  # Top: F0 | F1
                    torch.cat([t0_f2, t0_f3], dim=1),  # Bottom: F2 | F3
                ],
                dim=0,
            )

            t1 = torch.cat(
                [
                    torch.cat([t1_f0, t1_f1], dim=1),  # Top: F0 | F1
                    torch.cat([t1_f2, t1_f3], dim=1),  # Bottom: F2 | F3
                ],
                dim=0,
            )

            standard_tiles.append(t0)
            standard_tiles.append(t1)

        else:
            # Odd tile at the end, already in standard layout
            standard_tiles.append(interleaved_tiles[pair_idx])

    return standard_tiles


def validate_interleaved_conversion(tiles: List[torch.Tensor]) -> bool:
    """
    Validate that interleaved conversion is reversible (for testing).

    Args:
        tiles: List of tiles in standard layout

    Returns:
        True if conversion is reversible, False otherwise
    """
    interleaved = convert_to_interleaved_layout(tiles)
    recovered = convert_from_interleaved_layout(interleaved)

    if len(tiles) != len(recovered):
        return False

    for i, (orig, rec) in enumerate(zip(tiles, recovered)):
        if not torch.allclose(orig, rec):
            print(f"Tile {i} mismatch after round-trip conversion")
            return False

    return True
