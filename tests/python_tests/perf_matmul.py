# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0

# from turtle import Shape
from typing import List

import pytest
from helpers.format_config import DataFormat, FormatConfig, is_dest_acc_needed
from helpers.llk_params import DestAccumulation, DestSync, MathFidelity
from helpers.matmul_sweep import (
    generate_matmul_dimension_combinations,
    generate_tile_dims,
)
from helpers.param_config import parametrize
from helpers.perf import PerfRunType, perf_benchmark, update_report

# Important K dimensions to test
KT_DIMS = [1]

# Parameter set definitions
PARAM_SET_1 = {
    "max_tiles": 8,
    "kt_dims": [8],
    "loop_factor": 16,
    "formats": [
        FormatConfig(
            unpack_A_src=DataFormat.Float16_b,
            unpack_A_dst=DataFormat.Float16_b,
            unpack_B_src=DataFormat.Bfp4_b,
            unpack_B_dst=DataFormat.Bfp8_b,
            pack_src=DataFormat.Float16_b,
            pack_dst=DataFormat.Float16_b,
            math=DataFormat.Float16_b,
            same_src_format=False,
        )
    ],
}

PARAM_SET_2 = {
    "max_tiles": 1,
    "kt_dims": [1],
    "loop_factor": 1024,
    "formats": [
        FormatConfig(
            unpack_A_src=fmt,
            unpack_A_dst=fmt,
            unpack_B_src=fmt,
            unpack_B_dst=fmt,
            pack_src=fmt,
            pack_dst=fmt,
            math=fmt,
            same_src_format=True,
        )
        for fmt in [DataFormat.Float16_b, DataFormat.Bfp8]
    ],
}

# Select which parameter set to use
ACTIVE_PARAM_SET = PARAM_SET_1  # Change to PARAM_SET_2 as needed


def calculate_unpacker_config(
    in0_tile_r_dim: int, in0_tile_c_dim: int, in1_tile_r_dim: int, in1_tile_c_dim: int
):
    """
    Calculate unpacker configuration based on tile dimensions.

    Uses the tile configuration lookup table (lines 114-133) to determine:
    - Face dimensions for A and B
    - Number of faces for A and B
    - Partial face flags for A and B
    - Tiny tiles flag

    Args:
        in0_tile_r_dim: Tile A row dimension (height)
        in0_tile_c_dim: Tile A column dimension (width)
        in1_tile_r_dim: Tile B row dimension (height)
        in1_tile_c_dim: Tile B column dimension (width)

    Returns:
        Dictionary with unpacker configuration parameters
    """
    TILE_HEIGHT = 32
    TILE_WIDTH = 32
    FACE_R_DIM = 16
    FACE_C_DIM = 16

    # Calculate flags for tile A
    partial_face_a = in0_tile_r_dim < TILE_HEIGHT
    narrow_tile_a = in0_tile_c_dim < TILE_WIDTH

    # Calculate flags for tile B
    partial_face_b = in1_tile_r_dim < TILE_HEIGHT
    narrow_tile_b = in1_tile_c_dim < TILE_WIDTH

    # Calculate face dimensions for A
    if partial_face_a:
        unpack_A_face_r_dim = (
            in0_tile_r_dim if in0_tile_r_dim < FACE_R_DIM else FACE_R_DIM
        )
    else:
        unpack_A_face_r_dim = FACE_R_DIM

    unpack_A_face_c_dim = in0_tile_c_dim if narrow_tile_a else FACE_C_DIM

    # Calculate face dimensions for B
    if partial_face_b:
        unpack_B_face_r_dim = (
            in1_tile_r_dim if in1_tile_r_dim < FACE_R_DIM else FACE_R_DIM
        )
    else:
        unpack_B_face_r_dim = FACE_R_DIM

    unpack_B_face_c_dim = in1_tile_c_dim if narrow_tile_b else FACE_C_DIM

    # Calculate num_faces with partial_face override
    # From metal: num_faces = partial_face ? 1 : get_operand_num_faces(operand_id)
    if partial_face_a:
        num_faces_A = 1
    else:
        tile_hw_a = in0_tile_r_dim * in0_tile_c_dim
        face_hw_a = unpack_A_face_r_dim * unpack_A_face_c_dim
        num_faces_A = tile_hw_a // face_hw_a

    if partial_face_b:
        num_faces_B = 1
    else:
        tile_hw_b = in1_tile_r_dim * in1_tile_c_dim
        face_hw_b = unpack_B_face_r_dim * unpack_B_face_c_dim
        num_faces_B = tile_hw_b // face_hw_b

    # Tiny tiles flag: True if any tile is not 32×32
    tiny_tiles = (
        in0_tile_r_dim != TILE_HEIGHT
        or in0_tile_c_dim != TILE_WIDTH
        or in1_tile_r_dim != TILE_HEIGHT
        or in1_tile_c_dim != TILE_WIDTH
    )

    return {
        "tiny_tiles": tiny_tiles,
        "unpack_A_face_r_dim": unpack_A_face_r_dim,
        "unpack_B_face_r_dim": unpack_B_face_r_dim,
        "num_faces_A": num_faces_A,
        "num_faces_B": num_faces_B,
        "partial_face_a": partial_face_a,
        "partial_face_b": partial_face_b,
    }


def calculate_math_config(
    in0_tile_r_dim: int, in0_tile_c_dim: int, in1_tile_r_dim: int, in1_tile_c_dim: int
):
    """
    Calculate math kernel configuration based on tile dimensions.

    The math kernel uses partial_face_math flag which is based on
    input A tile row dimension compared to FACE_R_DIM.

    Args:
        in0_tile_r_dim: Tile A row dimension (height)
        in0_tile_c_dim: Tile A column dimension (width)
        in1_tile_r_dim: Tile B row dimension (height)
        in1_tile_c_dim: Tile B column dimension (width)

    Returns:
        Dictionary with math configuration parameters
    """
    FACE_R_DIM = 16

    # partial_face_math is based on input A tile row dimension vs FACE_R_DIM
    partial_face_math = in0_tile_r_dim < FACE_R_DIM

    return {
        "partial_face_math": partial_face_math,
    }


def calculate_packer_config(
    in0_tile_r_dim: int, in0_tile_c_dim: int, in1_tile_r_dim: int, in1_tile_c_dim: int
):
    """
    Calculate packer configuration based on tile dimensions.

    Uses the packer configuration lookup table (lines 135-175) to determine:
    - Output tile dimensions (same as matmul result dimensions)
    - Output face dimensions
    - Number of output faces
    - Partial face and narrow tile flags for output

    For matmul: output tile dimensions = (in0_tile_r_dim, in1_tile_c_dim)

    Args:
        in0_tile_r_dim: Tile A row dimension (height)
        in0_tile_c_dim: Tile A column dimension (width)
        in1_tile_r_dim: Tile B row dimension (height)
        in1_tile_c_dim: Tile B column dimension (width)

    Returns:
        Dictionary with packer configuration parameters
    """
    TILE_HEIGHT = 32
    TILE_WIDTH = 32
    FACE_R_DIM = 16
    FACE_C_DIM = 16

    # Output tile dimensions for matmul: (A_rows, B_cols)
    out_tile_r_dim = in0_tile_r_dim
    out_tile_c_dim = in1_tile_c_dim

    # Calculate flags for output tile
    partial_face_out = out_tile_r_dim < TILE_HEIGHT
    narrow_tile_out = out_tile_c_dim < TILE_WIDTH

    # Calculate output face dimensions
    if partial_face_out:
        out_face_r_dim = out_tile_r_dim if out_tile_r_dim < FACE_R_DIM else FACE_R_DIM
    else:
        out_face_r_dim = FACE_R_DIM

    out_face_c_dim = out_tile_c_dim if narrow_tile_out else FACE_C_DIM

    # Calculate num_faces for output: tile_hw / face_hw
    tile_hw_out = out_tile_r_dim * out_tile_c_dim
    face_hw_out = out_face_r_dim * out_face_c_dim
    num_faces_out = tile_hw_out // face_hw_out

    return {
        "out_tile_r_dim": out_tile_r_dim,
        "out_tile_c_dim": out_tile_c_dim,
        "out_face_r_dim": out_face_r_dim,
        "num_faces_out": num_faces_out,
        "partial_face_out": partial_face_out,
        "narrow_tile_out": narrow_tile_out,
    }


def generate_matmul_dimension_combinations_simple(max_tiles: int, kt_dims):
    """
    Generate simplified matmul dimension pairs with fixed rt_dim=1 (mt_dim=1).
    Sweeps over kt_dim and all possible ct_dim (nt_dim) values.

    Args:
        max_tiles: Maximum size of result matrix in tiles (M×N tiles)
        kt_dims: K dimension sizes to test (in tiles)

    Returns:
        List[(A_dims, B_dims)] where A_dims=[32, K], B_dims=[K, N]
        with M fixed at 32 (1 tile) and N varying from 32 to max_tiles*32
    """
    TILE_DIM = 32  # Standard tile dimension for row and column
    mt_dim = 1  # Fixed row dimension

    return [
        ([mt_dim * TILE_DIM, kt_dim * TILE_DIM], [kt_dim * TILE_DIM, nt_dim * TILE_DIM])
        for kt_dim in kt_dims
        for nt_dim in range(1, max_tiles + 1)
    ]


def matmul_combos(
    formats: List[FormatConfig],
    dest_acc: List[DestAccumulation],
):
    def _dest_bank_max_tiles(format: FormatConfig, dest_acc: DestAccumulation):
        if is_dest_acc_needed(format) or dest_acc == DestAccumulation.Yes:
            return 4
        return 8

    unique_max_tiles = set(
        _dest_bank_max_tiles(fmt, acc) for fmt in formats for acc in dest_acc
    )
    dimensions = {
        max_tiles: generate_matmul_dimension_combinations(max_tiles, kt_dims=KT_DIMS)
        for max_tiles in unique_max_tiles
    }

    return [
        (format, accumulation, dims)
        for format in formats
        for accumulation in dest_acc
        for dims in dimensions[_dest_bank_max_tiles(format, accumulation)]
    ]


@pytest.mark.perf
@parametrize(
    test_name="matmul_perf",
    in0_tile_r_dim=[1, 2, 4, 8, 16, 32],
    kt_dim=ACTIVE_PARAM_SET["kt_dims"],
    formats=ACTIVE_PARAM_SET["formats"],
    dimension_combo=lambda kt_dim: generate_matmul_dimension_combinations_simple(
        max_tiles=ACTIVE_PARAM_SET["max_tiles"], kt_dims=[kt_dim]
    ),
)
def test_perf_matmul_simple(
    perf_report, test_name, in0_tile_r_dim, kt_dim, formats, dimension_combo
):
    """
    Simplified matmul performance test with configurable tile dimensions and formats.
    Sweeps over in0_tile_r_dim, formats, and all dimension combinations from the active parameter set.
    """
    dest_acc = DestAccumulation.No  # Fixed for both parameter sets
    math_fidelity = MathFidelity.LoFi

    matrix_a, matrix_b = dimension_combo

    run_types = [
        PerfRunType.L1_TO_L1,
        PerfRunType.UNPACK_ISOLATE,
        PerfRunType.MATH_ISOLATE,
        PerfRunType.PACK_ISOLATE,
        PerfRunType.L1_CONGESTION,
    ]

    # Calculate all matmul dimensions using helper function
    dims = generate_tile_dims((matrix_a, matrix_b))

    # Define tile dimensions (in0_tile_r_dim is parameterized)
    # in0_tile_r_dim is passed as parameter from @parametrize
    in0_tile_c_dim = 32  # Tile A col dimension
    in1_tile_r_dim = 32  # Tile B row dimension
    in1_tile_c_dim = 32  # Tile B col dimension

    # Calculate all tile configurations using helper functions
    unpacker_config = calculate_unpacker_config(
        in0_tile_r_dim, in0_tile_c_dim, in1_tile_r_dim, in1_tile_c_dim
    )
    math_config = calculate_math_config(
        in0_tile_r_dim, in0_tile_c_dim, in1_tile_r_dim, in1_tile_c_dim
    )
    packer_config = calculate_packer_config(
        in0_tile_r_dim, in0_tile_c_dim, in1_tile_r_dim, in1_tile_c_dim
    )

    # TODO add logic for calculating tiny tile flags for different configurations

    # UNPACK AND MATH configuration From metal runtime
    # // KEY LINE: partial_face is set based on tile height
    # partial_face = static_cast<uint32_t>(this->tile_shape[0] < constants::TILE_HEIGHT);
    # narrow_tile = static_cast<uint32_t>(this->tile_shape[1] < constants::TILE_WIDTH);
    # Tile_Shape	face_shape	tile_hw	face_hw	num_faces	partial_face	narrow_tile
    # 32×32	        16×16	    1024	256	    4	        0	            0
    # 16×32	        16×16	    512	    256	    2	        1	            0
    # 32×16	        16×16	    512	    256	    2	        0	            1
    # 16×16	        16×16	    256	    256	    1	        1	            1
    # 8×32	        8×16	    256	    128	    2	        1	            0
    # 4×32	        4×16	    128	    64	    2	        1	            0
    # 2×32	        2×16	    64	    32	    2	        1	            0
    # 1×32	        1×16	    32	    16	    2	        1	            0
    # 8×16	        8×16	    128	    64	    2	        1	            1
    # 4×16	        4×16	    64	    16	    4	        1	            1
    # 2×16	        2×16	    32	    4	    8	        1	            1
    # 1×16	        1×16	    16	    1	    16	        1	            1

    # unpack_A_face_r_dim = face_shaepe[0]
    # partial_face_math = in0_tile_r_dim < FACE_R_DIM (FACE_R_DIM = 16)
    # partial_face_a/b uses table above based on in0/in1_tile_r_dim
    # num_faces_A/B = unpA_num_faces = partial_face_a ? 1 : get_operand_num_faces(operandA_id); get_operand_num_faces --> read num_faces from table above

    # PACKER configuration from metal runtime
    # =====================================================================================================================
    # TILE CONFIGURATION LOOKUP TABLE (from TILE_FACE_HW_CHOICES)
    # =====================================================================================================================
    #
    # | # | Tile Shape | Face Shape | tile_hw | face_hw | num_faces | partial_face | narrow_tile | Notes              |
    # |---|------------|------------|---------|---------|-----------|--------------|-------------|--------------------|
    # | 1 | 32×32      | 16×16      | 1024    | 256     | 4         | 0            | 0           | Standard full tile |
    # | 2 | 16×32      | 16×16      | 512     | 256     | 2         | 1            | 0           | Half-height tile   |
    # | 3 | 32×16      | 16×16      | 512     | 256     | 2         | 0            | 1           | Half-width tile    |
    # | 4 | 16×16      | 16×16      | 256     | 256     | 1         | 1            | 1           | Quarter tile       |
    # | 5 | 8×32       | 8×16       | 256     | 128     | 2         | 1            | 0           | Host loopback only |
    # | 6 | 4×32       | 4×16       | 128     | 64      | 2         | 1            | 0           | Host loopback only |
    # | 7 | 2×32       | 2×16       | 64      | 32      | 2         | 1            | 0           | Host loopback only |
    # | 8 | 1×32       | 1×16       | 32      | 16      | 2         | 1            | 0           | Host loopback only |
    # | 9 | 8×16       | 8×16       | 128     | 128     | 1         | 1            | 1           | Host loopback only |
    # |10 | 4×16       | 4×16       | 64      | 64      | 1         | 1            | 1           | Host loopback only |
    # |11 | 2×16       | 2×16       | 32      | 32      | 1         | 1            | 1           | Host loopback only |
    # |12 | 1×16       | 1×16       | 16      | 16      | 1         | 1            | 1           | Host loopback only |
    #
    # Computed Values:
    # ---------------
    #   tile_hw      = tile_shape[0] × tile_shape[1]
    #   face_hw      = face_shape[0] × face_shape[1]
    #   num_faces    = tile_hw / face_hw
    #   partial_face = (tile_shape[0] < 32) ? 1 : 0
    #   narrow_tile  = (tile_shape[1] < 32) ? 1 : 0
    #
    # Hardware Constraints:
    # --------------------
    #   - num_faces must be 1, 2, or 4 (validated in llk_unpack_common.h)
    #   - Configurations #1-#4 are LLK-supported (compute kernels)
    #   - Configurations #5-#12 are for HOST LOOPBACK ONLY (not supported in LLK)
    #
    # Generated Arrays (example for CB ID 0):
    # ---------------------------------------
    #   Config #1 (32×32): face_r_dim=16, num_faces=4, partial_face=0, narrow_tile=0
    #   Config #2 (16×32): face_r_dim=16, num_faces=2, partial_face=1, narrow_tile=0
    #   Config #3 (32×16): face_r_dim=16, num_faces=2, partial_face=0, narrow_tile=1
    #   Config #4 (16×16): face_r_dim=16, num_faces=1, partial_face=1, narrow_tile=1
    #
    # =====================================================================================================================
    test_config = {
        "formats": formats,
        "testname": test_name,
        "loop_factor": ACTIVE_PARAM_SET["loop_factor"],
        "tile_cnt": dims.rt_dim * dims.ct_dim * dims.kt_dim,
        "input_A_dimensions": matrix_a,
        "input_B_dimensions": matrix_b,
        "output_dimensions": dims.output_dimensions,
        "rt_dim": dims.rt_dim,
        "ct_dim": dims.ct_dim,
        "kt_dim": dims.kt_dim,
        "dest_acc": dest_acc,
        "math_fidelity": math_fidelity,
        "dest_sync": DestSync.Half,
        # Tile dimensions
        "in0_tile_r_dim": in0_tile_r_dim,
        "in0_tile_c_dim": in0_tile_c_dim,
        "in1_tile_r_dim": in1_tile_r_dim,
        "in1_tile_c_dim": in1_tile_c_dim,
        # Unpacker configuration (calculated)
        **unpacker_config,
        # Math configuration (calculated)
        **math_config,
        # Packer configuration (calculated)
        **packer_config,
    }

    results = perf_benchmark(test_config, run_types)
    update_report(perf_report, test_config, results)


# @parametrize(
#     test_name="matmul_perf",
#     combos=matmul_combos(
#         formats=input_output_formats(
#             [
#                 DataFormat.Float16_b,
#                 DataFormat.Float16,
#                 DataFormat.Float32,
#                 DataFormat.Bfp8_b,
#             ]
#         ),
#         dest_acc=[DestAccumulation.No, DestAccumulation.Yes],
#     ),
#     math_fidelity=[
#         MathFidelity.LoFi,
#         MathFidelity.HiFi2,
#         MathFidelity.HiFi3,
#         MathFidelity.HiFi4,
#     ],
# )
# def test_perf_matmul(perf_report, test_name, combos, math_fidelity):

#     formats, dest_acc, (matrix_a, matrix_b) = combos

#     print (formats)
#     print (type(formats))

#     if is_dest_acc_needed(formats) and dest_acc == DestAccumulation.No:
#         pytest.skip("Dest accumulation must be enabled for this format")

#     run_types = [
#         PerfRunType.L1_TO_L1,
#         PerfRunType.UNPACK_ISOLATE,
#         PerfRunType.MATH_ISOLATE,
#         PerfRunType.PACK_ISOLATE,
#         PerfRunType.L1_CONGESTION,
#     ]

#     # Calculate all matmul dimensions using helper function
#     dims = generate_tile_dims((matrix_a, matrix_b))

#     test_config = {
#         "formats": formats,
#         "testname": test_name,
#         "loop_factor": 16,
#         "tile_cnt": dims.rt_dim * dims.ct_dim * dims.kt_dim,
#         "input_A_dimensions": matrix_a,
#         "input_B_dimensions": matrix_b,
#         "output_dimensions": dims.output_dimensions,
#         "rt_dim": dims.rt_dim,
#         "ct_dim": dims.ct_dim,
#         "kt_dim": dims.kt_dim,
#         "dest_acc": dest_acc,
#         "math_fidelity": math_fidelity,
#     }

#     results = perf_benchmark(test_config, run_types)
#     update_report(perf_report, test_config, results)
