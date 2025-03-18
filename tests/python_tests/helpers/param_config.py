# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
from dataclasses import dataclass
from typing import List, Optional
from .format_arg_mapping import *
from .format_config import FormatConfig, DataFormat


def manage_included_params(func):
    def wrapper(*args, **kwargs):
        if not hasattr(wrapper, "included_params"):
            wrapper.included_params = []
        return func(wrapper.included_params, *args, **kwargs)

    return wrapper


@manage_included_params
def generate_format_combinations(
    included_params,
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
    >>> generate_format_combinations([DataFormat.Float16, DataFormat.Float32], True)
    [FormatConfig(unpack_src=DataFormat.Float16, unpack_dst=DataFormat.Float16, math=DataFormat.Float16, pack_src=DataFormat.Float16, pack_dst=DataFormat.Float16),
     FormatConfig(unpack_src=DataFormat.Float32, unpack_dst=DataFormat.Float32, math=DataFormat.Float32, pack_src=DataFormat.Float32, pack_dst=DataFormat.Float32)]

    >>> generate_format_combinations([DataFormat.Float16, "Float32"], False)
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


@manage_included_params
def generate_params(
    included_params,
    testnames: List[str],
    format_combos: List[FormatConfig],
    dest_acc: Optional[DestAccumulation] = None,
    approx_mode: Optional[List[str]] = None,
    mathop: Optional[List[str]] = None,
    math_fidelity: Optional[List[int]] = None,
    tile_cnt: Optional[List[int]] = None,
    reduce_dim: Optional[List[str]] = None,
    pool_type: Optional[List[str]] = None,
) -> List[tuple]:
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
        )
        for testname in testnames
        for format_config in format_combos
        for acc_mode in (dest_acc if dest_acc is not None else [None])
        for approx in (approx_mode if approx_mode is not None else [None])
        for math in (mathop if mathop is not None else [None])
        for fidelity in (math_fidelity if math_fidelity is not None else [None])
        for num_tiles in (tile_cnt if tile_cnt is not None else [None])
        for dim in (reduce_dim if reduce_dim is not None else [None])
        for pool in (pool_type if pool_type is not None else [None])
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
    ...     ("multiple_tiles_eltwise_test", FormatConfig(DataFormat.Float16, DataFormat.Float16, DataFormat.Float16, DataFormat.Float16, DataFormat.Float16), DestAccumulation.No, ApproximationMode.Yes, MathOperation.Elwadd, TileCount.One, MathFidelity.HiFi4, ReduceDimension.Column, ReducePoolArgs.Max),
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

        result = [
            f"unpack_A_src={format_config.unpack_A_src.value}",
            f"unpack_A_dst={format_config.unpack_A_dst.value}",
            *(
                f"unpack_B_src={format_config.unpack_B_src.value}"
                if format_config.unpack_B_src
                else ""
            ),
            *(
                f"unpack_B_dst={format_config.unpack_B_dst.value}"
                if format_config.unpack_B_dst
                else ""
            ),
            f"math={format_config.math.value}",
            f"pack_src={format_config.pack_src.value}",
            f"pack_dst={format_config.pack_dst.value}",
            *(f"dest_acc={params[0].name}" if params[0] else ""),
            *(f"approx_mode={params[1].value}" if params[1] else ""),
            *(f"mathop={params[2].name}" if params[2] else ""),
            *(f"math_fidelity={params[3].name}" if params[3] else ""),
            *(f"tile_cnt={params[4].value}" if params[4] else ""),
            *(f"reduce_dim={params[5].name}" if params[5] else ""),
            *(f"pool_type={params[6].name}" if params[6] else ""),
        ]

        return " | ".join(filter(None, result))
