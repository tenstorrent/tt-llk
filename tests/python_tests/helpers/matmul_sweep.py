# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0

"""
Helper functions for dimension-related calculations in matrix operations and Matmul test configurations for matmul test sweeping.
"""
from dataclasses import dataclass
from typing import List, Tuple

from helpers.format_arg_mapping import (
    DestAccumulation,
    DestSync,
    StochasticRounding,
    Transpose,
)
from helpers.format_config import DataFormat, FormatConfig, is_dest_acc_needed
from helpers.param_config import get_max_dst_index


# =========================
# Configuration Classes
# =========================
@dataclass
class TileDims:
    input_A_dimensions: Tuple[int, int]
    input_B_dimensions: Tuple[int, int]
    output_dimensions: Tuple[int, int]
    rt_dim: int
    ct_dim: int
    kt_dim: int
    tile_cnt: int
    tile_cnt_A: int
    tile_cnt_B: int
    output_tile_cnt: int
    in0_tile_r_dim: int
    in0_tile_c_dim: int
    in1_tile_r_dim: int
    in1_tile_c_dim: int


@dataclass
class FaceLayoutConfig:
    unpack_transpose_faces: Transpose
    unpack_transpose_within_face: Transpose
    num_faces_A: int
    num_faces_B: int
    num_faces: int
    partial_face_A: bool
    partial_face_B: bool


@dataclass
class MatmulDims:
    tile_dimensions: TileDims
    face_layout_config: FaceLayoutConfig
    formats: FormatConfig
    stochastic_rnd: StochasticRounding
    dst_index: int
    dest_sync: DestSync
    dest_acc: DestAccumulation


# ======================================================================
# Helper Functions: Defining the Tile & Face Layout Dimensions for Matmul
# ======================================================================


def validate_tile_dimensions(dimension: int, row_col_dim: int):
    """Validate that dimension is divisible by row/col."""
    if dimension % row_col_dim != 0:
        raise AssertionError(f"{dimension} must be divisible by {row_col_dim}")


def calculate_matmul_dimensions(
    input_A_dimensions, input_B_dimensions, num_rows: int = 32, num_cols: int = 32
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
        input_A_dimensions: (rows, cols) for matrix A
        input_B_dimensions: (rows, cols) for matrix B

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
    output_dimensions = (M, N)

    validate_tile_dimensions(M, num_rows)
    validate_tile_dimensions(N, num_cols)
    validate_tile_dimensions(K1, num_cols)

    rt_dim = M // num_rows  # Row tiles in result
    ct_dim = N // num_cols  # Column tiles in result
    kt_dim = (
        K1 // num_cols
    )  # Inner dimension tiles rt_dim (matrix A) = kt_dim = ct_dim (matrix B) = 1

    # Calculate tile counts
    output_tile_cnt = rt_dim * ct_dim

    return {
        "rt_dim": rt_dim,
        "ct_dim": ct_dim,
        "kt_dim": kt_dim,
        "output_dimensions": output_dimensions,
        "output_tile_cnt": output_tile_cnt,
    }


def generate_matmul_dimension_combinations(max_tiles: int) -> List[tuple]:
    """
    Generate all valid matrix multiplication dimension combinations.

    Creates all possible combinations of (inputA_dimensions, inputB_dimensions) where:
    - The result matrix also has at most max_tiles tiles
    - Matrix multiplication is valid: inputA[1] == inputB[0] (K dimensions match)
    - Returns combinations that can be used for comprehensive matmul testing

    Args:
        max_tiles: Maximum number of tiles allowed per result matrix

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

    valid_combinations = []
    tile_rows = 32
    tile_cols = 32

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
                m_dim = m_tiles * tile_cols
                k_dim = k_tiles * tile_cols
                n_dim = n_tiles * tile_rows

                inputA_dims = [m_dim, k_dim]
                inputB_dims = [k_dim, n_dim]
                valid_combinations.append((inputA_dims, inputB_dims))

    return valid_combinations


def generate_matmul_tiny_tiles_combinations(max_tiles: int) -> List[tuple]:
    valid_combinations = []
    tile_A_rows = [1, 2, 4, 8, 16]
    tile_A_columns = 32
    tile_B_rows = 32
    tile_B_columns = list(range(32, (max_tiles + 1) * 32, 32))

    for tile_A_row in tile_A_rows:
        for tile_B_column in tile_B_columns:
            inputA_dims = (tile_A_row, tile_A_columns)
            inputB_dims = (tile_B_rows, tile_B_column)
            valid_combinations.append((inputA_dims, inputB_dims))

    return valid_combinations


def skip_matmul_combination(
    stochastic_rounding_mode: StochasticRounding,
    dest_acc: DestAccumulation,
    is_fpu_bfloat16: bool,
    kt_dim: int,
) -> bool:
    # Exposes a stochastic rounding bug in hw, leading to undeterministic failure due to accumulated precision loss in rounding across multiple tiles
    fpu_stochastic_modes = {StochasticRounding.Fpu, StochasticRounding.All}
    if (
        stochastic_rounding_mode in fpu_stochastic_modes
        and dest_acc == DestAccumulation.No
        and is_fpu_bfloat16
        and kt_dim >= 4
    ):
        return True
    return False


def generate_tile_dims(dimension: Tuple[list, list]) -> TileDims:
    inputA_dims, inputB_dims = dimension
    matmul_dimensions = calculate_matmul_dimensions(inputA_dims, inputB_dims)
    return TileDims(
        input_A_dimensions=inputA_dims,
        input_B_dimensions=inputB_dims,
        output_dimensions=matmul_dimensions["output_dimensions"],
        rt_dim=matmul_dimensions["rt_dim"],
        ct_dim=matmul_dimensions["ct_dim"],
        kt_dim=matmul_dimensions["kt_dim"],
        tile_cnt=matmul_dimensions["output_tile_cnt"],
        tile_cnt_A=(inputA_dims[0] * inputA_dims[1]) // (32 * 32),
        tile_cnt_B=(inputB_dims[0] * inputB_dims[1]) // (32 * 32),
        output_tile_cnt=matmul_dimensions["output_tile_cnt"],
        in0_tile_r_dim=32,
        in0_tile_c_dim=32,
        in1_tile_r_dim=32,
        in1_tile_c_dim=32,
    )


def generate_face_layout_config(num_faces: int) -> List[FaceLayoutConfig]:
    configs = []
    if num_faces == 1:
        for transpose in [Transpose.No, Transpose.Yes]:
            for partial_face in [True]:
                config = FaceLayoutConfig(
                    num_faces_A=1,
                    num_faces_B=1,
                    num_faces=1,
                    unpack_transpose_faces=transpose,
                    unpack_transpose_within_face=transpose,
                    partial_face_A=partial_face,
                    partial_face_B=partial_face,
                )
                configs.append(config)
    elif num_faces == 2:
        for partial_face in [True, False]:
            config = FaceLayoutConfig(
                num_faces_A=2,
                num_faces_B=2,
                num_faces=2,
                unpack_transpose_faces=Transpose.No,
                unpack_transpose_within_face=Transpose.No,
                partial_face_A=partial_face,
                partial_face_B=partial_face,
            )
            configs.append(config)
    else:  # num_faces == 4
        for transpose in [Transpose.No, Transpose.Yes]:
            config = FaceLayoutConfig(
                num_faces_A=4,
                num_faces_B=4,
                num_faces=4,
                unpack_transpose_faces=transpose,
                unpack_transpose_within_face=transpose,
                partial_face_A=False,
                partial_face_B=False,
            )
            configs.append(config)

    return configs


def generate_face_layout_config_sweep(math_matmul: bool) -> List[FaceLayoutConfig]:
    configs = []
    num_faces_list = [4] if math_matmul else [1, 2, 4]
    for num_faces in num_faces_list:
        configs.extend(generate_face_layout_config(num_faces))
    return configs


# ===========================================================
# Sweeping Functions: Generate All Matmul Test Configurations
# ===========================================================


def sweep_matmul(
    formats_list: List[FormatConfig],
    dest_acc_modes: List[DestAccumulation],
    all_stochastic_modes: List[StochasticRounding] = [StochasticRounding.No],
    dest_sync_modes: List[DestSync] = [DestSync.Half],
    math_matmul: bool = False,
) -> List[MatmulDims]:
    combinations = []

    # Cache dimensions to avoid redundant computation
    dimensions_cache = {}

    # Pre-computed sets for fast membership testing
    bfloat16_formats = {DataFormat.Float16_b, DataFormat.Float32}
    fpu_stochastic_modes = {StochasticRounding.Fpu, StochasticRounding.All}

    for fmt in formats_list:
        base_max_tiles = 4 if is_dest_acc_needed(fmt) else 8
        is_fpu_bfloat16 = (
            fmt.input_format in bfloat16_formats
            and fmt.output_format in bfloat16_formats
        )

        for dest_acc in dest_acc_modes:
            max_tiles = 4 if dest_acc == DestAccumulation.Yes else base_max_tiles

            # Use cached or newly generated dimensions
            dimensions_list = dimensions_cache.setdefault(
                max_tiles, generate_matmul_dimension_combinations(max_tiles)
            )

            for stochastic_mode in all_stochastic_modes:
                for dims in dimensions_list:
                    tile_dims = generate_tile_dims(dims)
                    if skip_matmul_combination(
                        stochastic_mode, dest_acc, is_fpu_bfloat16, tile_dims.kt_dim
                    ):
                        continue

                    for dest_sync in dest_sync_modes:
                        max_dst_index = get_max_dst_index(
                            dest_sync,
                            dest_acc == DestAccumulation.Yes,
                            tile_dims.tile_cnt,
                        )

                        face_layout_config_sweep = generate_face_layout_config_sweep(
                            math_matmul
                        )
                        for face_layout_config in face_layout_config_sweep:

                            base_matmul_dims = MatmulDims(
                                tile_dimensions=tile_dims,
                                face_layout_config=face_layout_config,
                                formats=fmt,
                                stochastic_rnd=stochastic_mode,
                                dst_index=0,
                                dest_sync=dest_sync,
                                dest_acc=dest_acc,
                            )

                            combinations.append(base_matmul_dims)

                            if max_dst_index != 0 and math_matmul:
                                # Create a new object with different dst_index since dataclass is immutable
                                edge_case_dims = MatmulDims(
                                    tile_dimensions=tile_dims,
                                    face_layout_config=face_layout_config,
                                    formats=fmt,
                                    stochastic_rnd=stochastic_mode,
                                    dst_index=max_dst_index,
                                    dest_sync=dest_sync,
                                    dest_acc=dest_acc,
                                )
                                combinations.append(edge_case_dims)

    return combinations


def sweep_tiny_tiles_matmul(
    formats_list: List[FormatConfig],
    dest_acc_modes: List[DestAccumulation],
    all_stochastic_modes: List[StochasticRounding] = [StochasticRounding.No],
    dest_sync_modes: List[DestSync] = [DestSync.Half],
    math_matmul: bool = False,
) -> List[MatmulDims]:
    combinations = []

    configs = []
    for dest_sync in dest_sync_modes:
        base_max_tiles = 8 if dest_sync == DestSync.Half else 16
        for fmt in formats_list:
            for dest_acc in dest_acc_modes:
                for stochastic_mode in all_stochastic_modes:
                    max_tiles = (
                        base_max_tiles // 2
                        if is_dest_acc_needed(fmt) or dest_acc == DestAccumulation.Yes
                        else base_max_tiles
                    )
                    configs.append(
                        {
                            "fmt": fmt,
                            "dest_acc": dest_acc,
                            "stochastic_mode": stochastic_mode,
                            "dest_sync": dest_sync,
                            "max_tiles": max_tiles,
                        }
                    )

    for config in configs:
        dimensions_list = generate_matmul_tiny_tiles_combinations(
            max_tiles=config["max_tiles"]
        )
        for dims in dimensions_list:
            # Generate tile dimensions for the tiny tiles
            inputA_dims, inputB_dims = dims
            matmul_dimensions = calculate_matmul_dimensions((32, 32), inputB_dims)
            tile_dims = TileDims(
                input_A_dimensions=(32, 32),
                input_B_dimensions=inputB_dims,
                output_dimensions=matmul_dimensions["output_dimensions"],
                rt_dim=1,
                ct_dim=inputB_dims[1] // 32,
                kt_dim=1,
                tile_cnt=matmul_dimensions["output_tile_cnt"],
                tile_cnt_A=1,  # For tiny tiles, matrix A is always 1 tile (32x32)
                tile_cnt_B=(inputB_dims[0] * inputB_dims[1]) // (32 * 32),
                output_tile_cnt=1,
                in0_tile_r_dim=inputA_dims[0],
                in0_tile_c_dim=inputA_dims[1],
                in1_tile_r_dim=inputB_dims[0],
                in1_tile_c_dim=inputB_dims[1],
            )

            # generate face layout for tiny tiles
            face = FaceLayoutConfig(
                num_faces_A=2,
                num_faces_B=4,
                num_faces=2,
                unpack_transpose_faces=Transpose.No,
                unpack_transpose_within_face=Transpose.No,
                partial_face_A=True,  # same for pack
                partial_face_B=False,  # same for math
            )

            max_dst_index = get_max_dst_index(
                config["dest_sync"],
                config["dest_acc"] == DestAccumulation.Yes,
                tile_dims.tile_cnt,
            )
            max_dst_indices = [0]
            if math_matmul and max_dst_index != 0:
                max_dst_indices.append(max_dst_index)

            for max_dst_idx in max_dst_indices:
                combinations.append(
                    MatmulDims(
                        tile_dimensions=tile_dims,
                        face_layout_config=face,
                        formats=config["fmt"],
                        stochastic_rnd=config["stochastic_mode"],
                        dst_index=max_dst_idx,
                        dest_sync=config["dest_sync"],
                        dest_acc=config["dest_acc"],
                    )
                )

    return combinations
