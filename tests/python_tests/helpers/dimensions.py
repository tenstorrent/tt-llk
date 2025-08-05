# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0

"""
Helper functions for dimension-related calculations in matrix operations.
"""

from typing import List


def calculate_dimension_properties(
    input_dimensions: List[int], format_size: int
) -> dict:
    """
    Calculate all dimension-related properties for a given input.

    Args:
        input_dimensions: List containing [rows, cols] dimensions
        format_size: Size of the data format in bytes

    Returns:
        Dictionary containing all calculated properties:
        - num_rows: Hardware-specific row count for LLK functions
        - faces: Number of unpack faces (each face is 16x16)
        - face_r_dim: Row dimension for face calculations
        - partial_face: Whether this represents a partial face (row dimension < 16)
    """
    return {
        "num_rows": calculate_num_rows(input_dimensions, format_size),
        "faces": calculate_unpack_faces(input_dimensions),
        "face_r_dim": get_face_r_dimension(input_dimensions),
        "partial_face": is_partial_face(input_dimensions),
    }


def calculate_matmul_dimensions(
    input_A_dimensions: List[int], input_B_dimensions: List[int]
) -> dict:
    """
    Calculate matrix multiplication tile dimensions.

    For matrix multiplication A[M,K] × B[K,N] = C[M,N], calculate:
    - rt_dim: Row tiles (M dimension)
    - ct_dim: Column tiles (N dimension)
    - kt_dim: Inner dimension tiles (K dimension)
    - output_dimensions: Result matrix dimensions
    - output_tile_cnt: Number of tiles in result

    Args:
        input_A_dimensions: List containing [rows, cols] for matrix A
        input_B_dimensions: List containing [rows, cols] for matrix B

    Returns:
        dict: Dictionary containing all calculated dimensions

    Raises:
        AssertionError: If matrix dimensions are incompatible for multiplication
    """
    M, K1 = input_A_dimensions[0], input_A_dimensions[1]
    K2, N = input_B_dimensions[0], input_B_dimensions[1]

    # Verify K dimensions match for valid matmul
    assert (
        K1 == K2
    ), f"Matrix dimensions incompatible for multiplication: A[{M},{K1}] × B[{K2},{N}]"

    # Calculate output dimensions: A[M,K] × B[K,N] = C[M,N]
    output_dimensions = [M, N]

    # Calculate tile dimensions (each tile is 32×32)
    rt_dim = M // 32  # Row tiles in result
    ct_dim = N // 32  # Column tiles in result
    kt_dim = K1 // 32  # Inner dimension tiles

    # Calculate tile counts
    tile_cnt_A = (M // 32) * (K1 // 32)
    tile_cnt_B = (K2 // 32) * (N // 32)
    output_tile_cnt = rt_dim * ct_dim

    return {
        "rt_dim": rt_dim,
        "ct_dim": ct_dim,
        "kt_dim": kt_dim,
        "output_dimensions": output_dimensions,
        "output_tile_cnt": output_tile_cnt,
        "tile_cnt_A": tile_cnt_A,
        "tile_cnt_B": tile_cnt_B,
        "M": M,
        "K": K1,
        "N": N,
    }


def generate_matmul_dimension_combinations(max_tiles: int) -> List[tuple]:
    """
    Generate all valid matrix multiplication dimension combinations.

    Creates all possible combinations of (inputA_dimensions, inputB_dimensions) where:
    - Each input matrix has at most max_tiles tiles (each tile is 32×32)
    - The result matrix also has at most max_tiles tiles
    - Matrix multiplication is valid: inputA[1] == inputB[0] (K dimensions match)
    - Returns combinations that can be used for comprehensive matmul testing

    Args:
        max_tiles: Maximum number of tiles allowed per matrix (inputs AND result)

    Returns:
        List of tuples: Each tuple contains (inputA_dimensions, inputB_dimensions)
        where inputA_dimensions and inputB_dimensions are [rows, cols] lists

    Note: When 16-bit datums in dest can fit max 8 tiles and 4 tiles for 32-bit datums
    Example:
        For max_tiles=4:
        Returns combinations like:
        ([32, 32], [32, 32])    # 1×1 tiles each, result: 1×1 = 1 tile
        ([32, 64], [64, 32])    # 1×2 and 2×1 tiles, result: 1×1 = 1 tile
        ([64, 128], [128, 128])    # result: 2×4 = 8 tiles, works for 16-bit datums
        ([32, 32], [32, 128])  # 1×1 and 1×4 tiles, result: 1×4 = 4 tiles, works for 16-bit and 32-bit datums

        But NOT ([256, 32], [32, 256]) because result would be 8×8 = 64 tiles > 4 for 32-bit datums and >8 for 16-bit datums
    """


def generate_matmul_dimension_combinations(max_tiles: int) -> List[tuple]:
    valid_combinations = []

    for m_tiles in range(1, max_tiles + 1):
        for k_tiles in range(1, max_tiles + 1):
            # Check if matrix A is valid: m_tiles * k_tiles <= max_tiles
            if m_tiles * k_tiles > max_tiles:
                break  # Early termination - larger k_tiles will also be invalid

            # Calculate maximum valid n_tiles based on constraints
            max_n_from_B = (
                max_tiles // k_tiles
            )  # From B constraint: k_tiles * n_tiles <= max_tiles
            max_n_from_C = (
                max_tiles // m_tiles
            )  # From C constraint: m_tiles * n_tiles <= max_tiles
            max_n_tiles = min(max_n_from_B, max_n_from_C)

            # Generate all valid n_tiles values
            for n_tiles in range(1, max_n_tiles + 1):
                # Convert tile counts to actual dimensions
                m_dim = m_tiles * 32
                k_dim = k_tiles * 32
                n_dim = n_tiles * 32

                inputA_dims = [m_dim, k_dim]
                inputB_dims = [k_dim, n_dim]
                valid_combinations.append((inputA_dims, inputB_dims))

    return valid_combinations
