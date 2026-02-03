# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0

"""
Helper functions for dimension-related calculations in matrix operations and Matmul test configurations for matmul test sweeping.
"""
from dataclasses import dataclass
from typing import Iterable, List, NamedTuple, Tuple

from helpers.format_config import DataFormat, FormatConfig, is_dest_acc_needed
from helpers.golden_generators import TILE_DIM
from helpers.llk_params import (
    DestAccumulation,
    DestSync,
    StochasticRounding,
    Transpose,
)
from helpers.param_config import get_max_dst_index


# =========================
# Configuration Classes
# =========================
@dataclass
class TileDimensions:
    in0_dimensions: Tuple[int, int]
    in1_dimensions: Tuple[int, int]
    output_dimensions: Tuple[int, int]
    rt_dim: int
    ct_dim: int
    kt_dim: int
    tile_cnt: int
    tile_cnt_in0: int
    tile_cnt_in1: int
    output_tile_cnt: int
    in0_tile_r_dim: int
    in0_tile_c_dim: int
    in1_tile_r_dim: int
    in1_tile_c_dim: int


@dataclass
class FaceLayoutConfig:
    unpack_transpose_faces: Transpose
    unpack_transpose_within_face: Transpose
    num_faces_in0: int
    num_faces_in1: int
    num_faces: int
    partial_face_in0: bool
    partial_face_in1: bool
    partial_face_math: bool
    partial_face_pack: bool


class FaceLayoutParameters(NamedTuple):
    """Parameters for face layout configuration."""

    transpose_faces: Transpose
    transpose_within: Transpose
    partial_face: bool


@dataclass
class MatmulConfig:
    tile_dimensions: TileDimensions
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
    if dimension < 0:
        raise ValueError(f"Dimension {dimension} must be positive!")
    elif row_col_dim < 0:
        raise ValueError(f"Row/col dimension {row_col_dim} must be positive!")
    elif dimension % row_col_dim != 0:
        raise ValueError(
            f"Dimension {dimension} must be divisible by Row/Column {row_col_dim}"
        )


def generate_matmul_dimension_combinations(
    max_tiles: int, kt_dims: Iterable[int] = range(1, 5)
) -> List[tuple]:
    """
    Generate valid matmul dimension pairs where result matrix size <= max_tiles.

    Args:
        max_tiles: Maximum size of result matrix in tiles (M×N tiles)
        kt_dims: K dimension sizes to test (in tiles)

    Returns:
        List[(in0_dims, in1_dims)] where in0_dims=[M, K], in1_dims=[K, N]

    Example:
        max_tiles=4
        - generates: ([32, 64], [64, 32]) → result 1×1 tiles
        - excludes: ([128, 32], [32, 128]) → result 4×4 = 16 tiles
    """

    return [
        ([32, 32], [32, 32])
    ]  # TODO: remove this once we have a proper matmul dimension generator

    return [
        ([mt_dim * TILE_DIM, kt_dim * TILE_DIM], [kt_dim * TILE_DIM, nt_dim * TILE_DIM])
        for mt_dim in range(1, max_tiles + 1)
        for nt_dim in range(1, max_tiles // mt_dim + 1)
        for kt_dim in kt_dims
    ]


def generate_matmul_tiny_tiles_combinations(max_tiles: int) -> List[tuple]:
    valid_combinations = []
    tile_in0_rows = [1, 2, 4, 8, 16]
    tile_in0_columns = 32
    tile_in1_rows = 32
    tile_in1_columns = [32]
    # tile_in1_columns = list(range(32, (max_tiles + 1) * 32, 32)) # TODO: uncomment this

    return [
        ((tile_in0_row, tile_in0_columns), (tile_in1_rows, tile_in1_column))
        for tile_in0_row in tile_in0_rows
        for tile_in1_column in tile_in1_columns
    ]


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


def generate_tile_dims(
    dimension: Tuple[list, list], tiny_tiles: bool = False, in0_tile_r_dim: int = 32
) -> TileDimensions:
    num_rows = 32
    num_cols = 32
    input0_dims, input1_dims = dimension
    M, K1 = input0_dims[0], input0_dims[1]
    K2, N = input1_dims[0], input1_dims[1]

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
    )  # Inner dimension tiles rt_dim (input 0) = kt_dim = ct_dim (input 1) = 1

    # Calculate tile counts
    output_tile_cnt = rt_dim * ct_dim

    return TileDimensions(
        in0_dimensions=input0_dims,
        in1_dimensions=input1_dims,
        output_dimensions=output_dimensions,
        rt_dim=rt_dim,
        ct_dim=ct_dim,
        kt_dim=kt_dim,
        tile_cnt=output_tile_cnt,
        tile_cnt_in0=(input0_dims[0] * input0_dims[1]) // (32 * 32),
        tile_cnt_in1=(input1_dims[0] * input1_dims[1]) // (32 * 32),
        output_tile_cnt=(
            output_tile_cnt if not tiny_tiles else 1
        ),  # matmul with input 0 tiny tile does not work on multiple tiles for input 1 https://github.com/tenstorrent/tt-llk/issues/697
        in0_tile_r_dim=in0_tile_r_dim,
        in0_tile_c_dim=32,
        in1_tile_r_dim=32,
        in1_tile_c_dim=32,
    )


def generate_face_layout_config(num_faces: int) -> List[FaceLayoutConfig]:
    """
    Generate face layout configurations for the specified number of faces.

    Raises:
        ValueError: If num_faces is not 1, 2, or 4
    """
    if num_faces not in [1, 2, 4]:
        raise ValueError(f"num_faces must be 1, 2, or 4, got {num_faces}")

    # Configuration parameters for each num_faces
    config_params = {
        1: [
            FaceLayoutParameters(
                transpose_faces=Transpose.No,
                transpose_within=Transpose.No,
                partial_face=True,
            ),
            FaceLayoutParameters(
                transpose_faces=Transpose.Yes,
                transpose_within=Transpose.Yes,
                partial_face=True,
            ),
        ],
        2: [
            FaceLayoutParameters(
                transpose_faces=Transpose.No,
                transpose_within=Transpose.No,
                partial_face=True,
            ),
            FaceLayoutParameters(
                transpose_faces=Transpose.No,
                transpose_within=Transpose.No,
                partial_face=False,
            ),
        ],
        4: [
            FaceLayoutParameters(
                transpose_faces=Transpose.No,
                transpose_within=Transpose.No,
                partial_face=False,
            ),
            # FaceLayoutParameters(
            #     transpose_faces=Transpose.Yes,
            #     transpose_within=Transpose.Yes,
            #     partial_face=False,
            # ),
        ],
    }

    return [
        FaceLayoutConfig(
            num_faces_in0=num_faces,
            num_faces_in1=num_faces,
            num_faces=num_faces,
            unpack_transpose_faces=params.transpose_faces,
            unpack_transpose_within_face=params.transpose_within,
            partial_face_in0=params.partial_face,
            partial_face_in1=params.partial_face,
            partial_face_math=params.partial_face,
            partial_face_pack=params.partial_face,
        )
        for params in config_params[num_faces]
    ]


def generate_face_layout_config_sweep(math_matmul: bool) -> List[FaceLayoutConfig]:
    num_faces_list = [4] if math_matmul else [1, 2, 4]
    return [
        config
        for num_faces in num_faces_list
        for config in generate_face_layout_config(num_faces)
    ]


# ===========================================================
# Sweeping Functions: Generate All Matmul Test Configurations
# ===========================================================


def sweep_matmul(
    formats_list: List[FormatConfig],
    dest_acc_modes: List[DestAccumulation],
    all_stochastic_modes: List[StochasticRounding] = [StochasticRounding.No],
    dest_sync_modes: List[DestSync] = [DestSync.Half],
    math_matmul: bool = False,
) -> List[MatmulConfig]:
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

                            base_matmul_dims = MatmulConfig(
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
                                edge_case_dims = MatmulConfig(
                                    tile_dimensions=tile_dims,
                                    face_layout_config=face_layout_config,
                                    formats=fmt,
                                    stochastic_rnd=stochastic_mode,
                                    dst_index=max_dst_index,
                                    dest_sync=dest_sync,
                                    dest_acc=dest_acc,
                                )
                                # combinations.append(edge_case_dims)

    return combinations


def sweep_tiny_tiles_matmul(
    formats_list: List[FormatConfig],
    dest_acc_modes: List[DestAccumulation],
    all_stochastic_modes: List[StochasticRounding] = [StochasticRounding.No],
    dest_sync_modes: List[DestSync] = [DestSync.Half],
    math_matmul: bool = False,
) -> List[MatmulConfig]:
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
            input0_dims, input1_dims = dims
            tile_dims = generate_tile_dims(
                ([32, 32], input1_dims), tiny_tiles=True, in0_tile_r_dim=input0_dims[0]
            )

            # generate face layout for tiny tiles
            face = FaceLayoutConfig(
                num_faces_in0=2,
                num_faces_in1=4,
                num_faces=2,
                unpack_transpose_faces=Transpose.No,
                unpack_transpose_within_face=Transpose.No,
                partial_face_in0=True,  # SrcB
                partial_face_in1=False,  # SrcA
                partial_face_math=input0_dims[0] < 16,
                partial_face_pack=input0_dims[0] < 16,
            )

            max_dst_index = get_max_dst_index(
                config["dest_sync"],
                config["dest_acc"] == DestAccumulation.Yes,
                tile_dims.tile_cnt,
            )
            max_dst_indices = [0]
            # if math_matmul and max_dst_index != 0:
            #     max_dst_indices.append(max_dst_index)

            for max_dst_idx in max_dst_indices:
                combinations.append(
                    MatmulConfig(
                        tile_dimensions=tile_dims,
                        face_layout_config=face,
                        formats=config["fmt"],
                        stochastic_rnd=config["stochastic_mode"],
                        dst_index=(
                            min(max_dst_idx, 3) if math_matmul else max_dst_idx
                        ),  # multi-matmul with input 0 tiny tile does not work when dst_index > 3 https://github.com/tenstorrent/tt-llk/issues/697
                        dest_sync=config["dest_sync"],
                        dest_acc=config["dest_acc"],
                    )
                )

    return combinations
