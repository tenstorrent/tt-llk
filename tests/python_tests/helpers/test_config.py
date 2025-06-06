# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0

import os
from enum import Enum

from .format_arg_mapping import (
    ApproximationMode,
    DestAccumulation,
    MathFidelity,
    MathOperation,
    ReduceDimension,
    math_dict,
    pack_dst_dict,
    pack_src_dict,
    unpack_A_dst_dict,
    unpack_A_src_dict,
    unpack_B_dst_dict,
    unpack_B_src_dict,
)
from .format_config import InputOutputFormat


class ProfilerBuild(Enum):
    Yes = "true"
    No = "false"


def generate_build_header(
    test_config, profiler_build: ProfilerBuild = ProfilerBuild.No
):
    """Generate build.h file with all configuration defines"""
    header_content = []
    header_content.append("// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC")
    header_content.append("// SPDX-License-Identifier: Apache-2.0")
    header_content.append("// Auto-generated build configuration header")
    header_content.append("")
    header_content.append("#pragma once")
    header_content.append("")

    # Basic configuration
    header_content.append("// Basic configuration")
    header_content.append("#define TILE_SIZE_CNT 0x1000")

    # Profiler configuration
    if profiler_build == ProfilerBuild.Yes:
        header_content.append("#define LLK_PROFILER")

    # Dest accumulation
    dest_acc = test_config.get("dest_acc", DestAccumulation.No)
    if dest_acc == DestAccumulation.Yes or dest_acc == "DEST_ACC":
        header_content.append("#define DEST_ACC")

    # Unpack to dest
    unpack_to_dest = test_config.get("unpack_to_dest", False)
    header_content.append(f"#define UNPACKING_TO_DEST {str(unpack_to_dest).lower()}")

    # Math fidelity
    math_fidelity = test_config.get("math_fidelity", MathFidelity.LoFi)
    header_content.append(f"#define MATH_FIDELITY {math_fidelity.value}")

    # Approximation mode
    approx_mode = test_config.get("approx_mode", ApproximationMode.No)
    header_content.append(f"#define APPROX_MODE {approx_mode.value}")

    # Format configuration
    header_content.append("")
    header_content.append("// Format configuration")
    formats = test_config.get("formats")

    if isinstance(formats, InputOutputFormat):
        # Simple input/output format case
        unpack_A_src_val = unpack_A_src_dict[formats.input_format]
        pack_dst_val = pack_dst_dict[formats.output_format]
        header_content.append(f"#define {unpack_A_src_val}")
        header_content.append(f"#define {pack_dst_val}")
    else:
        # Full format specification
        header_content.append(f"#define {unpack_A_src_dict[formats.unpack_A_src]}")
        header_content.append(f"#define {unpack_A_dst_dict[formats.unpack_A_dst]}")
        header_content.append(f"#define {unpack_B_src_dict[formats.unpack_B_src]}")
        header_content.append(f"#define {unpack_B_dst_dict[formats.unpack_B_dst]}")
        header_content.append(f"#define {math_dict[formats.math]}")
        header_content.append(f"#define {pack_src_dict[formats.pack_src]}")
        header_content.append(f"#define {pack_dst_dict[formats.pack_dst]}")

    # Math operation configuration
    mathop = test_config.get("mathop", "no_mathop")
    if mathop != "no_mathop":
        header_content.append("")
        header_content.append("// Math operation configuration")
        header_content.append(f"#define {mathop.value}")

        reduce_dim = test_config.get("reduce_dim", ReduceDimension.No)
        pool_type = test_config.get("pool_type", ReduceDimension.No)

        if mathop in [
            MathOperation.ReduceColumn,
            MathOperation.ReduceRow,
            MathOperation.ReduceScalar,
        ]:
            header_content.append(f"#define REDUCE_DIM {reduce_dim.value}")
            header_content.append(f"#define POOL_TYPE {pool_type.value}")

    # Multiple tiles test specific configuration
    testname = test_config.get("testname")
    if testname == "multiple_tiles_eltwise_test":
        header_content.append("")
        header_content.append("// Multiple tiles test configuration")
        header_content.append("#define MULTIPLE_OPS")

        kern_cnt = test_config.get("kern_cnt", 1)
        pack_addr_cnt = test_config.get("pack_addr_cnt")
        pack_addrs = test_config.get("pack_addrs")

        header_content.append(f"#define KERN_CNT {kern_cnt}")
        if pack_addr_cnt is not None:
            header_content.append(f"#define PACK_ADDR_CNT {pack_addr_cnt}")
        if pack_addrs is not None:
            header_content.append(f"#define PACK_ADDRS {pack_addrs}")

    header_content.append("")
    return "\n".join(header_content)


def write_build_header(
    test_config,
    profiler_build: ProfilerBuild = ProfilerBuild.No,
    output_path=None,
):
    """Write build.h file to the specified path"""
    if output_path is None:
        # Find the tests directory and construct the path to build.h
        current_dir = os.getcwd()
        if "python_tests" in current_dir:
            # Called from python_tests directory
            output_path = "../../helpers/include/build.h"
        elif current_dir.endswith("tests"):
            # Called from tests directory
            output_path = "helpers/include/build.h"
        else:
            raise FileNotFoundError("Could not locate tests directory with Makefile")

    header_content = generate_build_header(test_config, profiler_build)
    with open(output_path, "w") as f:
        f.write(header_content)

    print(f"Generated build.h at: {os.path.abspath(output_path)}")


def generate_make_command(
    test_config,
    profiler_build: ProfilerBuild = ProfilerBuild.No,
    generate_header: bool = True,
):
    """Generate make command. Optionally also generate build.h header file."""

    if generate_header:
        write_build_header(test_config, profiler_build=profiler_build)

    # Simplified make command - only basic build parameters
    make_cmd = f"make -j 6 --silent "
    testname = test_config.get("testname")
    make_cmd += f"testname={testname} "

    return make_cmd
