# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC

from itertools import product
from typing import List, Optional, Tuple, TypedDict

import pytest

from helpers.chip_architecture import ChipArchitecture, get_chip_architecture
from helpers.dimensions import (
    calculate_matmul_dimensions,
    generate_matmul_dimension_combinations,
)
from helpers.log_utils import add_to_format_log

from .format_arg_mapping import DestAccumulation, DestSync, StochasticRounding, Tilize
from .format_config import (
    DataFormat,
    FormatConfig,
    InputOutputFormat,
    is_dest_acc_needed,
)

checked_formats_and_dest_acc = {}


def format_combination_sweep(
    formats: List[DataFormat],
    all_same: bool,
    same_src_reg_format: bool = True,
) -> List[FormatConfig]:
    """
    Generates a list of FormatConfig instances based on the given formats and the 'all_same' flag.
    This is used for pytesting in order to utilize pytest.mark.parametrize to test different format combinations.

    If the 'all_same' flag is set to True, the function returns combinations where all format attributes are the same.
    If the 'all_same' flag is set to False, the function returns all possible combinations of formats for each attribute.
        This is good to use when looking to test on full format flush.

    Parameters:
    formats (List[DataFormat]): A list of formats that are supported for this test. Combinations are generated based on these formats.
    all_same (bool): A flag indicating whether to return combinations with all formats being the same
                     (True) or all possible combinations (False).

    Returns:
    List[FormatConfig]: A list of FormatConfig instances representing the generated format combinations.

    Example:
    >>> format_combination_sweep([DataFormat.Float16, DataFormat.Float32], True)
    [FormatConfig(unpack_src=DataFormat.Float16, unpack_dst=DataFormat.Float16, math=DataFormat.Float16, pack_src=DataFormat.Float16, pack_dst=DataFormat.Float16),
     FormatConfig(unpack_src=DataFormat.Float32, unpack_dst=DataFormat.Float32, math=DataFormat.Float32, pack_src=DataFormat.Float32, pack_dst=DataFormat.Float32)]

    >>> format_combination_sweep([DataFormat.Float16, "Float32"], False)
    [FormatConfig(unpack_src=DataFormat.Float16, unpack_dst=DataFormat.Float16, math=DataFormat.Float16, pack_src=DataFormat.Float16, pack_dst=DataFormat.Float16),
     FormatConfig(unpack_src=DataFormat.Float16, unpack_dst=DataFormat.Float16, math=DataFormat.Float16, pack_src=DataFormat.Float16, pack_dst=DataFormat.Float32),
     ...
     FormatConfig(unpack_src=DataFormat.Float32, unpack_dst=DataFormat.Float32, math=DataFormat.Float32, pack_src=DataFormat.Float32, pack_dst=DataFormat.Float32)]
    """
    if all_same:
        return [
            FormatConfig(
                unpack_A_src=fmt,
                unpack_A_dst=fmt,
                math=fmt,
                pack_src=fmt,
                pack_dst=fmt,
                same_src_format=same_src_reg_format,
            )
            for fmt in formats
        ]
    return [
        FormatConfig(
            unpack_A_src=unpack_src,
            unpack_A_dst=unpack_dst,
            math=math,
            pack_src=pack_src,
            pack_dst=pack_dst,
            same_src_format=same_src_reg_format,
        )
        for unpack_src in formats
        for unpack_dst in formats
        for math in formats
        for pack_src in formats
        for pack_dst in formats
    ]


class TestParamsConfig(TypedDict):
    test_name: str
    formats: Optional[List[FormatConfig]] = None
    dest_acc: Optional[DestAccumulation] = None
    approx_mode: Optional[List[str]] = None
    mathop: Optional[List[str]] = None
    math_fidelity: Optional[List[int]] = None
    tile_count: Optional[int] = None
    reduce_dim: Optional[List[str]] = None
    pool_type: Optional[List[str]] = None
    num_faces: Optional[List[int]] = None
    dest_sync: Optional[DestSync] = None
    tilize_en: Optional[Tilize] = None
    dst_idx: Optional[List[int]] = None


def generate_params(**kwargs: any) -> List[tuple]:
    """
    Generates a list of parameter combinations for test configurations.

    This function creates all possible combinations of the provided test parameters, including optional ones,
    while filtering out any None values. The function returns these combinations as tuples, which can be used
    for setting up tests or experiments.

    Returns:
    List[tuple]: A list of tuples, where each tuple represents a combination of parameters with any `None` values filtered out.

    Example:
    >>> testnames = ["multiple_tiles_eltwise_test", "matmul_test"]
    >>> format_combos = [FormatConfig(DataFormat.Float16, DataFormat.Float16, DataFormat.Float16, DataFormat.Float16, DataFormat.Float16)]
    >>> generate_params(testnames, format_combos, dest_acc=[DestAccumulation.Yes], approx_mode=[ApproximationMode.Yes])
    [
        ("multiple_tiles_eltwise_test", FormatConfig(DataFormat.Float16, DataFormat.Float16, DataFormat.Float16, DataFormat.Float16, DataFormat.Float16), DestAccumulation.Yes, ApproximationMode.Yes, None, None, None, None, None),
        ("matmul_test", FormatConfig(DataFormat.Float16, DataFormat.Float16, DataFormat.Float16, DataFormat.Float16, DataFormat.Float16), DestAccumulation.Yes, ApproximationMode.No, None, None, None, None, None)
    ]
    """

    format_combos = kwargs.get("formats", [])
    dest_acc = kwargs.get("dest_acc", [])

    for combo in format_combos:
        if not isinstance(combo, InputOutputFormat):
            continue

        for acc in dest_acc:
            if acc == DestAccumulation.No and is_dest_acc_needed(combo):
                key = (combo.input, combo.output)
                if key not in checked_formats_and_dest_acc:
                    add_to_format_log(combo.input_format, combo.output_format)
                    checked_formats_and_dest_acc[key] = True

    wrap_list = lambda x: [x] if not isinstance(x, list) else x
    arguments = [wrap_list(value) for value in kwargs.values() if value is not None]

    return product(*arguments)


def parametrize(**kwargs: any):
    parameters = kwargs.keys()
    parameters_string = ",".join(parameters)
    parameter_values = generate_params(**kwargs)

    def decorator(test_function):
        return pytest.mark.parametrize(parameters_string, parameter_values)(
            test_function
        )

    return decorator


def input_output_formats(
    formats: List[DataFormat], same: bool = False
) -> List[InputOutputFormat]:
    """
    Generates a list of InputOutputFormat instances based on the given formats.
    This function is used to create input-output format combinations for testing.
    Parameters:
    formats (List[DataFormat]): A list of formats that are supported for this test.
    Returns:
    List[InputOutputFormat]: A list of InputOutputFormat instances representing the generated format combinations.
    """
    if same:
        return [InputOutputFormat(input, input) for input in formats]
    return [InputOutputFormat(input, output) for input in formats for output in formats]


def generate_combination(formats: List[Tuple[DataFormat]]) -> List[FormatConfig]:
    """
    A function that creates a list of FormatConfig objects from a list of DataFormat objects that client wants to test.
    This function is useful for creating a list of FormatConfig objects for testing multiple formats combinations
    and cases which the user has specifically defined and wants to particularly test instead of a full format flush.
    Args:
    formats (List[Tuple[DataFormat]]): A list of tuples of DataFormat objects for which FormatConfig objects need to be created.
    Returns:
    List[FormatConfig]: A list of FormatConfig objects created from the list of DataFormat objects passed as input.
    Example:
    >>> formats = [(DataFormat.Float16, DataFormat.Float32, DataFormat.Float16, DataFormat.Float32, DataFormat.Float32)]
    >>> format_configs = generate_combination(formats)
    >>> print(format_configs[0].unpack_A_src)
    DataFormat.Float16
    >>> print(format_configs[0].unpack_B_src)
    DataFormat.Float16
    """
    return [
        (
            FormatConfig(
                unpack_A_src=tuple[0],
                unpack_A_dst=tuple[1],
                pack_src=tuple[2],
                pack_dst=tuple[3],
                math=tuple[4],
            )
            if len(tuple) == 5
            else FormatConfig(
                unpack_A_src=tuple[0],
                unpack_A_dst=tuple[1],
                unpack_B_src=tuple[2],
                unpack_B_dst=tuple[3],
                pack_src=tuple[4],
                pack_dst=tuple[5],
                math=tuple[6],
                same_src_format=False,
            )
        )
        for tuple in formats
    ]


def generate_format_aware_matmul_combinations(
    formats_list: List[FormatConfig],
    dest_acc_modes: List[DestAccumulation],
    all_stochastic_modes: List[StochasticRounding] = None,
):
    """
    Generate matmul dimension combinations with stochastic rounding support.

    Rules:
    1. Format outliers (Float16_b->Float16, Bfp8_b->Float16) MUST use dest_acc=Yes
    2. When dest_acc=Yes: max 4 tiles (32-bit dest register)
    3. When dest_acc=No: max 8 tiles (16-bit dest register)
    4. Exclude cases when: stochastic_rounding enabled for Pack (Pack, All) AND dest_acc == DestAccumulation.No
       AND k_tiles >= 4 AND fmt.input_format in [DataFormat.Float16_b, DataFormat.Float32]
       AND fmt.output_format in [DataFormat.Float16_b, DataFormat.Float32]
        - This specific case models when we have bfloat16 in dst register, when stochastic rounding is enabled the datum lacks mantissa bits
        to absorb the accumulated precision loss from multiple matmul ops acrossmultiple tiles. And due to existence of bug, this specific case sometimes fails,
        for now we will exclude this case.

    Returns: List of (format, dest_acc, stochastic_rounding, dimensions) tuples
    """
    if all_stochastic_modes is None:
        all_stochastic_modes = [StochasticRounding.No]

    combinations = []

    for fmt in formats_list:
        # Pre-compute format-specific values
        base_max_tiles = 4 if is_dest_acc_needed(fmt) else 8
        is_fpu_bfloat16 = fmt.input_format in [
            DataFormat.Float16_b,
            DataFormat.Float32,
        ] and fmt.output_format in [DataFormat.Float16_b, DataFormat.Float32]

        for dest_acc in dest_acc_modes:
            max_tiles = 4 if dest_acc == DestAccumulation.Yes else base_max_tiles
            dimensions_list = generate_matmul_dimension_combinations(max_tiles)

            for stochastic_mode in all_stochastic_modes:
                # Pre-compute stochastic condition
                is_fpu_stochastic = stochastic_mode in [
                    StochasticRounding.Fpu,
                    StochasticRounding.All,
                ]

                # Skip early if we know all combinations will be excluded
                if (
                    is_fpu_stochastic
                    and dest_acc == DestAccumulation.No
                    and is_fpu_bfloat16
                ):
                    # Check if any dimensions would have k_tiles < 4
                    valid_dimensions = []
                    for dims in dimensions_list:
                        inputA_dims, inputB_dims = dims
                        matmul_info = calculate_matmul_dimensions(
                            tuple(inputA_dims), tuple(inputB_dims)
                        )
                        if matmul_info["kt_dim"] < 4:
                            valid_dimensions.append(dims)

                    # Add only the valid ones
                    combinations.extend(
                        [
                            (fmt, dest_acc, dims, stochastic_mode)
                            for dims in valid_dimensions
                        ]
                    )
                else:
                    # No exclusion needed for this stochastic mode, add all
                    combinations.extend(
                        [
                            (fmt, dest_acc, dims, stochastic_mode)
                            for dims in dimensions_list
                        ]
                    )

    return combinations


def generate_tilize_aware_datacopy_combinations(formats_list, result_tiles: int = 1):
    """
    Generate possible (format, num_faces, tilize, dest_sync, dest_index) combinations that respect chip_architecture and tilize constraints.

    Key rules:
    1. When chip_architecture=WH: tilize_en=Tilize.No
        When testing on WH, tilize is always False because DataCopy does not have tilize argument for WH.
    2. When tilize_en=Tilize.Yes: num_faces=4
        Pack does not support less than 4 faces when tilize=True.
    3. When tilize_en=Tilize.Yes: input_format!=Bfp8_b
        Unpack tilize does not support input_format=Bfp8_b.

    Args:
        formats_list: List of InputOutputFormat combinations
        result_tiles: Number of tiles in the result matrix

    Returns:
        List of tuples: (format, num_faces, tilize_en, dest_sync, dest_index)
    """

    combinations = []

    # Determine tilize options based on chip architecture
    chip_arch = get_chip_architecture()
    tilize_list = (
        [Tilize.No]
        if chip_arch == ChipArchitecture.WORMHOLE
        else [Tilize.No, Tilize.Yes]
    )

    for tilize_en in tilize_list:
        num_faces_list = [4] if tilize_en == Tilize.Yes else [1, 2, 4]

        for num_faces in num_faces_list:
            for fmt in formats_list:
                # Skip invalid combination: tilize with Bfp8_b format
                if tilize_en == Tilize.Yes and fmt.input_format == DataFormat.Bfp8_b:
                    continue

                for dest_acc in [DestAccumulation.No, DestAccumulation.Yes]:
                    # Calculate dest acc setting for edgecase indices calculation
                    is_fp32_dest_acc_en = (
                        dest_acc == DestAccumulation.Yes or is_dest_acc_needed(fmt)
                    )

                    dest_sync_list = [DestSync.Half, DestSync.Full]
                    # Generate all dest sync and index combinations
                    for dest_sync, dest_idx in calculate_edgecase_dest_indices(
                        is_fp32_dest_acc_en, result_tiles, dest_sync_list
                    ):
                        combinations.append(
                            (
                                fmt,
                                dest_acc,
                                num_faces,
                                tilize_en,
                                dest_sync,
                                dest_idx,
                            )
                        )

    return combinations


def calculate_edgecase_dest_indices(
    dest_acc: bool, result_tiles: int, dest_sync_modes: List[DestSync] = [DestSync.Half]
):
    """
    Generate the lowest and highest possible dest index depending on the DestSync mode and whether dest is 32bit or not.

    Key rules:
    1. The lowest possible dest index is always 0.
    2. When DestSync.Half:  max_dst_tiles=8 (if dest is 16bit) or max_dst_tiles=4 (if dest is 32bit)
    3. When DestSync.Full:  max_dst_tiles=16 (if dest is 16bit) or max_dst_tiles=8 (if dest is 32bit)

    Args:
        dest_acc: Dest 16/32 bit mode, has to match is_fp32_dest_acc_en from C++
        result_tiles: Number of tiles in the result matrix
        dest_sync_modes: List of DestSync modes to generate indices for. If None, uses [DestSync.Half]

    Returns:
        List of tuples: (dest_sync, dst_index)
    """

    combinations = []

    DEST_SYNC_TILE_LIMITS = {
        DestSync.Half: 8,
        DestSync.Full: 16,
    }

    capacity_divisor = 2 if dest_acc else 1

    for dest_sync in dest_sync_modes:
        base_tile_limit = DEST_SYNC_TILE_LIMITS[dest_sync]
        max_tiles = base_tile_limit // capacity_divisor
        max_index = max_tiles - result_tiles

        if max_index < 0:
            raise ValueError(
                f"Too many result tiles ({result_tiles}) for destination capacity ({max_tiles}) with {dest_sync.name}"
            )

        # Add both combinations: starting at index 0 and at max allowed index
        # If max_index = 0 add only (dest_sync, 0) to avoid duplicates
        (
            combinations.extend([(dest_sync, 0), (dest_sync, max_index)])
            if max_index != 0
            else combinations.extend([(dest_sync, 0)])
        )

    return combinations
