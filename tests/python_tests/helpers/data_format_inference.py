# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0

"""
Data format inference for LLK pipeline stages.

This module provides functionality to automatically infer data formats across
the unpacking, math, and packing stages of compute pipelines, handling
architecture-specific differences between Wormhole and Blackhole.
"""

from typing import List

from .chip_architecture import ChipArchitecture, get_chip_architecture
from .format_config import DataFormat, FormatConfig
from .llk_params import DestAccumulation


def is_format_combination_outlier(
    input_format: DataFormat,
    output_format: DataFormat,
    is_fp32_dest_acc_en: DestAccumulation,
) -> bool:
    """
    Checks if the given input/output format combination is an outlier case
    that is unsupported by hardware and requires a workaround.

    This outlier case occurs when converting an 8-bit exponent format datum
    directly to Float16 without using an intermediate Float32 representation
    in the dest register.

    To handle this hardware limitation, the destination register stores 32-bit datums,
    and the packer input format is converted to Float32.

    Args:
        input_format: The input data format in L1
        output_format: The output data format in L1
        is_fp32_dest_acc_en: Flag indicating if 32-bit destination accumulation is enabled (dest_acc)

    Returns:
        True if the format combination is an unsupported hardware outlier; False otherwise
    """
    return (
        input_format.is_exponent_B()
        and output_format == DataFormat.Float16
        and is_fp32_dest_acc_en == DestAccumulation.No
    )


def infer_unpack_out(
    input_format: DataFormat,
    output_format: DataFormat,
    is_fp32_dest_acc_en: DestAccumulation,
    unpacking_to_dest: bool = False,
) -> DataFormat:
    """
    Returns the output format for the unpacker (data format config for registers)
    based on the input format in L1 and whether unpacking targets the source or destination register.

    Args:
        input_format: The data format currently stored in L1 cache
        output_format: The final desired output format
        is_fp32_dest_acc_en: Whether FP32 accumulation is enabled
        unpacking_to_dest: Indicates whether unpacking targets the destination register

    Returns:
        The inferred output data format for unpacking to registers
    """
    if input_format == DataFormat.Float32 and not unpacking_to_dest:
        # When input format in L1 is Float32 + unpacking to src registers (instead of directly to dest register)
        # Source registers can store 19-bit values, so we truncate Float32 to Tf32 if we know dest will be 32-bit format
        # which preserves the 8-bit exponent and as much mantissa precision as fits. If our dst register is 16-bit we directly truncate to 16-bit format
        if is_fp32_dest_acc_en == DestAccumulation.Yes:
            return DataFormat.Tf32
        elif output_format.is_exponent_B() or output_format == DataFormat.Float32:
            return DataFormat.Float16_b  # If output Float32 or Float16_b
        return DataFormat.Float16  # Tilize to Float16

    # For all other cases, we can keep the format the same in L1 and src register or dest register
    return input_format


def infer_pack_in(
    input_format: DataFormat,
    output_format: DataFormat,
    unpack_out: DataFormat,
    is_fp32_dest_acc_en: DestAccumulation,
    unpacking_to_dest: bool = False,
    chip_arch: ChipArchitecture = None,
) -> DataFormat:
    """
    Infers the packer input format based on input/output formats and architecture.

    Args:
        input_format: Input data format in L1 (unpacker input)
        output_format: Final output data format after packing
        unpack_out: The unpacker output format
        is_fp32_dest_acc_en: Flag indicating if FP32 accumulation is enabled
        unpacking_to_dest: Whether unpacking targets the destination register
        chip_arch: The chip architecture (Wormhole or Blackhole). If None, will be detected automatically.

    Returns:
        The inferred packer input format
    """
    if chip_arch is None:
        chip_arch = get_chip_architecture()

    is_wormhole = chip_arch == ChipArchitecture.WORMHOLE

    if (
        is_wormhole
        and is_fp32_dest_acc_en == DestAccumulation.Yes
        and output_format == DataFormat.Float16
    ):
        # On wormhole architecture, data stored as Float32 in dest register,
        # gasket cannot convert Float32 ->Float16_A, so it leaves the data as Float32,
        # allowing the packer to handle the conversion successfully.
        return DataFormat.Float32
    elif input_format == DataFormat.Float32 and not unpacking_to_dest:
        # When input is Float32 in L1 and we are unpacking the input tensor to source registers (not directly to dest registers)
        if is_fp32_dest_acc_en == DestAccumulation.Yes or output_format.is_exponent_B():
            # If FP32 dest accumulation is enabled and the output format has an 8-bit exponent,
            # the packer input format can directly be the output format since packer can convert Float32 to another 8-bit exponent format
            return output_format
        else:
            # Otherwise, we truncate Float32 to Tf32 or 16-bit format
            # because the packer cannot convert Float32 directly to output formats with less than 8-bit exponent (e.g., 5-bit exponent formats).
            return unpack_out
    elif (
        input_format == DataFormat.Float16
        and output_format == DataFormat.Bfp8_b
        and is_fp32_dest_acc_en == DestAccumulation.No
    ):
        # When storing Float16 input in destination registers without FP32 accumulation,
        # the packer cannot convert Float16_A directly to Block Float format (in this case Bfp8_B).
        # The gasket will convert Float16_A to Bfp8_A before passing it to the packer,
        # which then converts Bfp8_A to Bfp8_B.
        return DataFormat.Bfp8
    elif is_format_combination_outlier(
        input_format, output_format, is_fp32_dest_acc_en
    ):
        # Handling a hardware limitation: cannot convert 8-bit exponent datums to Float16 without storing them as intermediate Float32 in dest register.
        # In this case, we set dest registers store 32-bit datums (in params.h).
        # For wormhole architecture, gasket cannot perform this conversion and packer takes input Float32 (from dest register) converting to Float16_A.
        # For blackhole architecture, gasket able to convert Float32 to Float16_A before packing (reduces work on packer).
        return DataFormat.Float32 if is_wormhole else output_format

    # For all other cases:
    # - If dest register stores 32-bit data (is_fp32_dest_acc_en = true), packer input format can be set to desired output format,
    #   as gasket can convert Float32 to any format (except Float16_A).
    # - If destination registers do not store 32-bit data, gasket cannot convert,
    #   so the packer input format will be same as dest register format.
    return (
        output_format if is_fp32_dest_acc_en == DestAccumulation.Yes else input_format
    )


def infer_data_formats(
    input_format: DataFormat,
    output_format: DataFormat,
    is_fp32_dest_acc_en: DestAccumulation,
    unpacking_to_dest: bool = False,
    chip_arch: ChipArchitecture = None,
) -> FormatConfig:
    """
    Infers all data formats needed for unpacking, math, and packing stages in a pipeline.

    Args:
        input_format: Input data format in L1 (unpacker input)
        output_format: Final output data format after packing
        is_fp32_dest_acc_en: Flag indicating if FP32 accumulation is enabled
        unpacking_to_dest: Whether unpacking targets the destination register (default: False)
        chip_arch: The chip architecture (Wormhole or Blackhole). If None, will be detected automatically.

    Returns:
        FormatConfig struct containing all formats (with same_src_format=True, so A and B formats match)
    """
    if chip_arch is None:
        chip_arch = get_chip_architecture()

    # The following two formats are hard-coded for this test case
    unpack_in = input_format  # The input format for Unpacker (data format in L1)
    pack_out = output_format  # The final desired output format after packing (format in L1 after leaving the pipeline)

    # Determine the intermediate formats
    unpack_out = infer_unpack_out(
        input_format, output_format, is_fp32_dest_acc_en, unpacking_to_dest
    )  # output format for Unpacker, desired format in src register(s)
    math = unpack_out  # The data format used for mathematical computations, desired format in dest register (typically matches unpack_out)
    pack_in = infer_pack_in(
        input_format,
        output_format,
        unpack_out,
        is_fp32_dest_acc_en,
        unpacking_to_dest,
        chip_arch,
    )  # input to the packing stage, determines what gasket can convert from dest register
    # potentially different from unpack_out and pack_out depending on FP32 accumulation

    # Return a FormatConfig struct capturing all the inferred formats needed for this stage
    # same_src_format=True by default, so unpack_B_src/dst will match unpack_A_src/dst
    return FormatConfig(
        unpack_A_src=unpack_in,
        unpack_A_dst=unpack_out,
        math=math,
        pack_src=pack_in,
        pack_dst=pack_out,
    )


def build_data_formats(
    n: int,
    intermediate_config: FormatConfig,
    final_config: FormatConfig,
) -> List[FormatConfig]:
    """
    Helper function to build a list of FormatConfig objects.

    This function generates a list of FormatConfig, simulating
    multiple pipeline iterations where all but the last use intermediate_config,
    and the last uses final_config.

    Args:
        n: Number of L1-to-L1 pipeline iterations (list size)
        intermediate_config: Configuration for all iterations except the last
        final_config: Configuration for the final iteration

    Returns:
        List of FormatConfig for each iteration
    """
    configs = []
    for i in range(n):
        if i < n - 1:
            configs.append(intermediate_config)
        else:
            configs.append(final_config)
    return configs


def data_formats(
    input_format: DataFormat,
    output_format: DataFormat,
    is_fp32_dest_acc_en: DestAccumulation,
    n: int,
    unpacking_to_dest: bool = False,
    chip_arch: ChipArchitecture = None,
) -> List[FormatConfig]:
    """
    Entry point for computing a list of FormatConfig objects.

    Each FormatConfig object contains all the data formats necessary to execute
    a specific L1-to-L1 compute run across all 3 cores: unpack, math, and pack.

    Args:
        input_format: The input data format for all pipeline runs
        output_format: The output data format for the final pipeline run
        is_fp32_dest_acc_en: Whether FP32 accumulation is enabled
        n: The number of pipeline runs (iterations), determines list length
        unpacking_to_dest: Whether unpacking targets the destination register (default: False)
        chip_arch: The chip architecture (Wormhole or Blackhole). If None, will be detected automatically.

    Returns:
        A list of FormatConfig objects of length n
    """
    if chip_arch is None:
        chip_arch = get_chip_architecture()

    if chip_arch == ChipArchitecture.QUASAR:
        # Data Format Inference is not supported for Quasar architecture, so we return a single FormatConfig where all formats are the same.
        return [
            FormatConfig(
                unpack_A_src=input_format,
                unpack_A_dst=input_format,
                math=input_format,
                pack_src=input_format,
                pack_dst=output_format,
            )
        ]

    intermediate_config = infer_data_formats(
        input_format, input_format, is_fp32_dest_acc_en, unpacking_to_dest, chip_arch
    )
    final_config = infer_data_formats(
        input_format, output_format, is_fp32_dest_acc_en, unpacking_to_dest, chip_arch
    )

    return build_data_formats(n, intermediate_config, final_config)
