# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC

from itertools import product
from typing import List, Optional, Tuple, TypedDict

import pytest

from helpers.dimensions import generate_matmul_dimension_combinations
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


<<<<<<< HEAD
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
=======
@manage_included_params
def generate_params(
    included_params,
    testnames: List[str],
    format_combos: List[FormatConfig],
    dest_acc: Optional[DestAccumulation] = None,
    approx_mode: Optional[List[str]] = None,
    mathop: Optional[List[str]] = None,
    math_fidelity: Optional[List[int]] = None,
    tile_cnt: Optional[int] = None,
    reduce_dim: Optional[List[str]] = None,
    pool_type: Optional[List[str]] = None,
    num_faces: Optional[List[int]] = None,
) -> List[tuple]:
>>>>>>> f618ed4 (Add num faces as test argument)
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
<<<<<<< HEAD

    format_combos = kwargs.get("formats", [])
    dest_acc = kwargs.get("dest_acc", [])

=======
    
>>>>>>> f618ed4 (Add num faces as test argument)
    for combo in format_combos:
        if not isinstance(combo, InputOutputFormat):
            continue

        for acc in dest_acc:
            if acc == DestAccumulation.No and is_dest_acc_needed(combo):
                key = (combo.input, combo.output)
                if key not in checked_formats_and_dest_acc:
                    add_to_format_log(combo.input_format, combo.output_format)
                    checked_formats_and_dest_acc[key] = True

<<<<<<< HEAD
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
=======
    # Build a list of parameter names (`included_params`) that are non-None.
    # This allows later code in generate_param_ids(...) to conditionally include
    # only the parameters that were actually provided (not None) when generating the ID.
    included_params.extend(
        [
            param
            for param, value in [
                ("dest_acc", dest_acc),
                ("approx_mode", approx_mode),
                ("mathop", mathop),
                ("math_fidelity", math_fidelity),
                ("tile_cnt", tile_cnt),
                ("reduce_dim", reduce_dim),
                ("pool_type", pool_type),
                ("num_faces", num_faces),
            ]
            if value is not None
        ]  
    )

    return [
        (
            testname,
            format_config,
            acc_mode,
            approx,
            math,
            fidelity,
            num_tiles,
            dim,
            pool,
            faces,
        )
        for testname in testnames
        for format_config in format_combos
        for acc_mode in (dest_acc if dest_acc is not None else [None])
        for approx in (approx_mode if approx_mode is not None else [None])
        for math in (mathop if mathop is not None else [None])
        for fidelity in (math_fidelity if math_fidelity is not None else [None])
        for num_tiles in [tile_cnt]
        for dim in (reduce_dim if reduce_dim is not None else [None])
        for pool in (pool_type if pool_type is not None else [None])
        for faces in (num_faces if num_faces is not None else 7)
    ]


@manage_included_params
def clean_params(included_params, all_params: List[tuple]) -> List[tuple]:
    """
    Cleans up the list of parameter combinations by removing any `None` values.

    This function filters out any `None` values from the provided list of parameter combinations.
    It is used to clean up the list of parameters before generating parameter IDs for test cases.

    Returns:
    List[tuple]: A list of tuples, where each tuple represents a combination of parameters with any `None` values filtered out.

    Example:
    >>> all_params = [
    ...     ("matmul_test", FormatConfig(DataFormat.Float16, DataFormat.Float16, DataFormat.Float16, DataFormat.Float16, DataFormat.Float16), DestAccumulation.No, ApproximationMode.Yes, None, None, None, None, None),
    ...     ("fill_dest_test", FormatConfig(DataFormat.Float16, DataFormat.Float16, DataFormat.Float16, DataFormat.Float16, DataFormat.Float16), DestAccumulation.No, ApproximationMode.Yes, None, None, None, None, None)
    ... ]
    >>> clean_params(all_params)
    [
        ("matmul_test", FormatConfig(DataFormat.Float16, DataFormat.Float16, DataFormat.Float16, DataFormat.Float16, DataFormat.Float16), DestAccumulation.No, ApproximationMode.Yes),
        ("fill_dest_test", FormatConfig(DataFormat.Float16, DataFormat.Float16, DataFormat.Float16, DataFormat.Float16, DataFormat.Float16), DestAccumulation.No, ApproximationMode.Yes)
    ]
    """
    return [tuple(param for param in comb if param is not None) for comb in all_params]


@manage_included_params
def generate_param_ids(included_params, all_params: List[tuple]) -> List[str]:
    """
    Generates a list of parameter IDs based on the provided parameter combinations.

    Creates a string ID for each combination, including only those parameters that are present in the `included_params` list.
    If a parameter is not included (i.e., it is `None`), it will be excluded from the final ID.

    Used to format output of our test cases in a more readable way. Function return is passed into `ids` parameter of `@pytest.mark.parametrize`.

    Parameters:
    all_params (List[tuple]): A list of tuples, where each tuple contains a combination of parameters.
                               The second element in the tuple is expected to be a `FormatConfig` object,
                               and the rest are the parameter values (like `dest_acc`, `approx_mode`, etc.).

    Returns:
    List[str]: A list of formatted strings representing each combination of parameters.

    Example:
    >>> all_params = [
    ...     ("multiple_tiles_eltwise_test", FormatConfig(DataFormat.Float16, DataFormat.Float16, DataFormat.Float16, DataFormat.Float16, DataFormat.Float16), DestAccumulation.No, ApproximationMode.Yes, MathOperation.Elwadd, 1, MathFidelity.HiFi4, ReduceDimension.Column, ReducePoolArgs.Max),
    ...     ("reduce_test", FormatConfig(DataFormat.Float32, DataFormat.Float32, DataFormat.Float32, DataFormat.Float32, DataFormat.Float32), DestAccumulation.No, None, MathOperation.Elwmul, None, None, ReduceDimension.Column, ReducePollArgs.Avg)
    ... ]
    >>> generate_param_ids(all_params)
    [
        'unpack_src=Float16 | unpack_dst=Float16 | math=Add | pack_src=Float16 | pack_dst=Float16 | dest_acc=No | approx_mode=true | mathop=ElwAdd | tile_cnt=1 | math_fidelity=HiFi4 | reduce_dim=Column | pool_type=Max',
        'unpack_src=Float32 | unpack_dst=Float32 | math=Mul | pack_src=Float32 | pack_dst=Float32 | dest_acc=No | mathop=ElwMul | reduce_dim=Column | pool_type=Average'
    ]
    """

    def format_combination(comb: tuple) -> str:
        """
        Helper function to format a single combination of parameters into a readable string.

        Args:
        comb (tuple): A tuple containing a combination of parameters, including the FormatConfig object.

        Returns:
        str: A formatted string of parameter names and values.
        """
        # Extract the FormatConfig and other parameters
        testname, format_config, *params = comb

        # Start with the FormatConfig information
        if isinstance(format_config, InputOutputFormat):
            result = [
                f"unpack_src={format_config.input.name}",
                f"pack_dst={format_config.output.name}",
            ]
        else:
            result = [
                f"unpack_src={format_config.unpack_A_src.name}, {format_config.unpack_B_src.name}",
                f"unpack_dst={format_config.unpack_A_dst.name}, {format_config.unpack_B_dst.name}",
                f"math={format_config.math.name}",
                f"pack_src={format_config.pack_src.name}",
                f"pack_dst={format_config.pack_dst.name}",
            ]
        if params[0]:
            dest_acc_value = (
                "Turned On"
                if isinstance(format_config, InputOutputFormat)
                and params[0] == DestAccumulation.No
                and is_dest_acc_needed(format_config)
                else params[0].name
            )
            result.append(f"dest_acc={dest_acc_value}")
        if params[1]:
            result.append(f"approx_mode={params[1].value}")
        if params[2]:
            result.append(f"mathop={params[2].name}")
        if params[3]:
            result.append(f"math_fidelity={params[3].name}")
        if params[4]:
            result.append(f"tile_cnt={params[4]}")
        if params[5]:
            result.append(f"reduce_dim={params[5].name}")
        if params[6]:
            result.append(f"pool_type={params[6].name}")
        if params[7]:
            result.append(f"num_tiles={params[7].value}")

        # Join the result list into a single string with appropriate spacing
        return " | ".join(result)

    # Generate and return formatted strings for all parameter combinations
    return [format_combination(comb) for comb in all_params if comb[0] is not None]
>>>>>>> f618ed4 (Add num faces as test argument)


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


def generate_format_aware_matmul_combinations(formats_list):
    """
    Generate matmul dimension combinations that respect format specific / datum size and dest register constraints.

    Key rules:
    1. Format outliers (Float16_b->Float16, Bfp8_b->Float16) MUST use dest_acc=Yes
    2. When dest_acc=Yes: max 4 tiles (32-bit dest register)
    3. When dest_acc=No: max 8 tiles (16-bit dest register)

    Args:
        formats_list: List of InputOutputFormat combinations

    Returns:
        List of tuples: (format, dest_acc, dimensions)
    """

    combinations = []

    for fmt in formats_list:
        if is_dest_acc_needed(fmt):
            # Format outliers: C++ will force dest_acc --> Yes, so test both cases
            max_tiles = 4
            for dest_acc in [DestAccumulation.No, DestAccumulation.Yes]:
                dimensions = generate_matmul_dimension_combinations(max_tiles)
                for dims in dimensions:
                    combinations.append((fmt, dest_acc, dims))
        else:
            # Normal formats: test both dest accumulation modes
            for dest_acc in [DestAccumulation.No, DestAccumulation.Yes]:
                max_tiles = 8 if dest_acc == DestAccumulation.No else 4
                dimensions = generate_matmul_dimension_combinations(max_tiles)
                for dims in dimensions:
                    combinations.append((fmt, dest_acc, dims))

    return combinations
