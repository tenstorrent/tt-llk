# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC

from itertools import product
from typing import List, Optional, Tuple, TypedDict

import pytest

from helpers.dimensions import (
    calculate_matmul_dimensions,
    generate_matmul_dimension_combinations,
)
from helpers.log_utils import add_to_format_log

from .format_arg_mapping import DestAccumulation
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


def _calculate_dst_index_range(result_tiles: int, max_dst_tiles: int = 8) -> range:
    """
    Calculate the valid dst_index range for a given number of result tiles.

    Args:
        result_tiles: Number of tiles in the result matrix
        max_dst_tiles: Maximum tiles that can fit in dst register (default: 8)

    Returns:
        range: Valid dst_index values (0 to max_valid_index)
    """
    max_dst_index = max_dst_tiles - result_tiles
    return range(max_dst_index + 1)


def _generate_combinations_for_format(
    fmt: InputOutputFormat, max_tiles: int, dest_acc_modes: List[DestAccumulation]
) -> List[Tuple]:
    """
    Generate all combinations for a single format with given constraints.

    Args:
        fmt: InputOutputFormat to generate combinations for
        max_tiles: Maximum tiles allowed for this format
        dest_acc_modes: List of DestAccumulation modes to test

    Returns:
        List of tuples: (format, dest_acc, dimensions, dst_index)
    """
    combinations = []
    dimensions = generate_matmul_dimension_combinations(max_tiles)

    for dest_acc in dest_acc_modes:
        for dims in dimensions:
            # Calculate result matrix tiles
            matmul_dims = calculate_matmul_dimensions(dims[0], dims[1])
            result_tiles = matmul_dims["output_tile_cnt"]

            # Generate dst_index sweep
            for dst_index in _calculate_dst_index_range(result_tiles):
                combinations.append((fmt, dest_acc, dims, dst_index))

    return combinations


def generate_format_aware_matmul_combinations(
    formats_list: List[InputOutputFormat],
) -> List[Tuple]:
    """
    Generate matmul dimension combinations that respect format specific / datum size and dest register constraints.
    Also includes dst_index sweeping based on result matrix tile count.

    Key rules:
    1. Format outliers (Float16_b->Float16, Bfp8_b->Float16) MUST use dest_acc=Yes
    2. When dest_acc=Yes: max 4 tiles (32-bit dest register)
    3. When dest_acc=No: max 8 tiles (16-bit dest register)
    4. dst_index sweep: if result has N tiles, sweep dst_index from 0 to (8-N)
       - 1 tile: dst_index 0->7 (8 possible positions)
       - 2 tiles: dst_index 0->6 (7 possible positions)
       - etc.

    Args:
        formats_list: List of InputOutputFormat combinations

    Returns:
        List of tuples: (format, dest_acc, dimensions, dst_index)
    """
    combinations = []

    for fmt in formats_list:
        dest_acc_modes = [DestAccumulation.No, DestAccumulation.Yes]

        if is_dest_acc_needed(fmt):
            # Format outliers use conservative 4-tile limit for all dest_acc modes
            combinations.extend(
                _generate_combinations_for_format(fmt, 4, dest_acc_modes)
            )
        else:
            # Normal formats: different limits per dest_acc mode
            for dest_acc in dest_acc_modes:
                tiles_limit = 8 if dest_acc == DestAccumulation.No else 4
                combinations.extend(
                    _generate_combinations_for_format(fmt, tiles_limit, [dest_acc])
                )

    return combinations
