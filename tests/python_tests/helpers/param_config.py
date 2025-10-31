# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC

import inspect
from itertools import product
from typing import Dict, List, Optional, Set, Tuple, TypedDict

import pytest
from helpers.chip_architecture import ChipArchitecture, get_chip_architecture
from typing_extensions import deprecated

from .format_config import (
    DataFormat,
    FormatConfig,
    InputOutputFormat,
    is_dest_acc_needed,
)
from .llk_params import DestAccumulation, DestSync, Tilize

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


# When reading this file, keep in mind that parameter means FORMAL PARAMETER and argument means ACTUAL PARAMETER


def _param_dependencies(parameter: str, argument: any) -> Set[str]:
    """Extract parameter names from a callable using introspection."""
    if callable(argument):
        return set(inspect.signature(argument).parameters.keys())
    return set()


def _resolution_map(**kwargs: any) -> Dict[int, Set[int]]:
    """
    Builds a map of parameters used to resolve the constrained cartesian product

    The ordering of the keys in the map is the order in which the parameters are resolved.

    The key (int) is the index of the parameter in the result tuple.
    The values (set[int]) are the indices of the parameters on which the current parameter depends.

    """
    param_idx = {param: idx for idx, param in enumerate(kwargs.keys())}

    # Build dependency graph: param -> set of dependencies
    dependency_matrix = {
        param_idx[param]: set(
            [param_idx[dependency] for dependency in _param_dependencies(param, value)]
        )
        for param, value in kwargs.items()
    }

    topological = {}
    resolved = set()

    while dependency_matrix:
        resolved_next = {
            param: deps
            for param, deps in dependency_matrix.items()
            if not (deps - resolved)
        }

        if not resolved_next:
            # convert indices back to parameter names
            circular_dependencies = [
                kwargs.keys()[idx] for idx in dependency_matrix.keys()
            ]

            raise ValueError(
                f"Circular dependency detected among: {circular_dependencies}"
            )

        resolved_params = resolved_next.keys()

        # Add newly resolved parameters to topological order and resolved set
        resolved.update(resolved_params)
        topological.update(resolved_next)

        # Remove resolved parameters from dependency graph
        for param in resolved_params:
            del dependency_matrix[param]

    return topological


def _params_solve_dependencies(**kwargs: any) -> List[Tuple]:
    """
    Compute constrained cartesian product by resolving parameter dependencies.

    Uses recursive backtracking to generate all valid combinations where
    callable parameters can depend on previously resolved parameters.

    Returns:
        List of tuples representing all valid parameter combinations
    """
    param_order = tuple[str, ...](kwargs.keys())
    resolution_order = _topsort_dependencies(**kwargs)

    def _resolution_to_param_order(resolution: List[Tuple]) -> List[Tuple]:
        # Create mapping from param name to index in param_order for fast lookup
        param_to_idx = {param: idx for idx, param in enumerate(param_order)}
        # Create mapping from topological index to param_order index
        topo_to_order_idx = [param_to_idx[param] for param in topological]

    def _resolve_param_values(param: str, resolved_list: list) -> list:
        """Get possible values for a parameter given resolved dependencies."""
        value = kwargs[param]

        if param in callable_params:
            deps = callable_params[param]
            # Build constraint kwargs only for resolved dependencies
            constraint_kwargs = {}
            for dep in deps:
                dep_idx = param_to_idx.get(dep)
                if dep_idx is not None and resolved_list[dep_idx] is not None:
                    constraint_kwargs[dep] = resolved_list[dep_idx]
            result = value(**constraint_kwargs)
            return result if isinstance(result, list) else [result]

        if isinstance(value, list):
            return value

        return [value]

    def _solve_recursive(parameter_idx: int, resolved: list) -> List[Tuple]:
        """Recursively build combinations starting from parameter at idx."""
        if parameter_idx >= len(topological):
            # Return tuple of resolved parameters in the original order
            return [tuple(resolved)]

        # Get current parameter and its possible values
        parameter = topological[parameter_idx]
        param_idx_in_order = topo_to_order_idx[parameter_idx]
        arguments = _resolve_param_values(parameter, resolved)

        if not arguments:
            return []

        # For each possible value, recurse with next parameter
        combinations = []
        for argument in arguments:
            # Create new resolved list with updated value
            resolved_next = list(resolved)
            resolved_next[param_idx_in_order] = argument
            combinations.extend(_solve_recursive(parameter_idx + 1, resolved_next))

        return combinations

    # Initialize resolved list with None values
    initial_resolved = [None] * len(param_order)
    return _solve_recursive(0, initial_resolved)


def parametrize(**kwargs: any):
    parameters = tuple(kwargs.keys())
    parameters_string = ",".join(parameters)
    parameter_values = _params_solve_dependencies(**kwargs)

    def decorator(test_function):
        return pytest.mark.parametrize(parameters_string, parameter_values)(
            test_function
        )

    return decorator


@deprecated("Try using parametrize or python inbuilt product function")
def generate_params(**kwargs: any) -> List[tuple]:
    wrap_list = lambda x: [x] if not isinstance(x, list) else x
    arguments = [wrap_list(value) for value in kwargs.values() if value is not None]

    return product(*arguments)


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

                    dest_sync_list = [DestSync.Half]
                    # Generate all dest sync and index combinations
                    for _, dest_idx in calculate_edgecase_dest_indices(
                        is_fp32_dest_acc_en, result_tiles, dest_sync_list
                    ):
                        combinations.append(
                            (
                                fmt,
                                dest_acc,
                                num_faces,
                                tilize_en,
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

        # Add both combinations: lowest possible index = 0 and at max possible index
        # If max_index = 0 add only (dest_sync, 0) to avoid duplicates
        combinations.extend([(dest_sync, 0)])
        if max_index != 0:
            combinations.extend([(dest_sync, max_index)])

    return combinations


def get_max_dst_index(dest_sync: DestSync, dest_acc: bool, result_tiles: int) -> int:
    DEST_SYNC_TILE_LIMITS = {
        DestSync.Half: 8 if not dest_acc else 4,
        DestSync.Full: 16 if not dest_acc else 8,
    }
    return max(DEST_SYNC_TILE_LIMITS[dest_sync] - result_tiles, 0)


def generate_unary_input_dimensions(dest_acc, dest_sync=DestSync.Half):
    """Generate all possible input dimensions for unary operations.
    These dimensions are determined by the number of tiles that can fit into dest, which is determined by dest_sync and dest_acc.
    The generated input dimensions should ensure that all of the data fits into dest without any overflow when running unary operations.

    Key rules:
    1. When DestSync.Half:  max_tiles_in_dest=8 (if dest is 16bit) or max_tiles_in_dest=4 (if dest is 32bit)
    2. When DestSync.Full:  max_tiles_in_dest=16 (if dest is 16bit) or max_tiles_in_dest=8 (if dest is 32bit)

    Args:
        dest_acc: Dest 16/32 bit mode
        dest_sync: DestSync mode. Defaults to DestSync.Half

    Returns:
        List of input dimensions
    """

    DEST_SYNC_TILE_LIMITS = {
        DestSync.Half: 8,
        DestSync.Full: 16,
    }
    capacity_divisor = 2 if dest_acc == DestAccumulation.Yes else 1
    max_tiles_in_dest = DEST_SYNC_TILE_LIMITS[dest_sync] // capacity_divisor

    num_tile_rows = 32
    num_tile_cols = 32

    return [
        [row * num_tile_rows, column * num_tile_cols]
        for row in range(1, max_tiles_in_dest + 1)
        for column in range(1, (max_tiles_in_dest // row) + 1)
    ]
