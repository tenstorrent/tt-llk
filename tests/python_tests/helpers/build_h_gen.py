# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
#
# SPDX-License-Identifier: Apache-2.0

from .chip_architecture import ChipArchitecture, get_chip_architecture
from .data_format_inference import data_formats, is_format_combination_outlier
from .format_config import DataFormat, FormatConfig
from .llk_params import (
    FPU_BINARY_OPERATIONS,
    REDUCE_OPERATIONS,
    SFPU_BINARY_OPERATIONS,
    SFPU_UNARY_OPERATIONS,
    ApproximationMode,
    DestAccumulation,
    DestSync,
    ImpliedMathFormat,
    MathFidelity,
    MathOperation,
    UnpackerEngine,
    format_tile_sizes,
)
from .matmul_sweep import validate_tile_dimensions


def _generate_operation_constants(mathop: MathOperation) -> list[str]:
    """Generate the appropriate operation constants based on the math operation type."""
    constants = []

    if mathop in SFPU_UNARY_OPERATIONS:  # TP
        constants.append(
            f"constexpr auto SFPU_UNARY_OPERATION = SfpuType::{mathop.cpp_enum_value};"
        )
    elif mathop in SFPU_BINARY_OPERATIONS:  # TP / RT it's complicated
        constants.append(
            f"constexpr auto SFPU_BINARY_OPERATION = ckernel::BinaryOp::{mathop.cpp_enum_value};"
        )
    elif mathop in FPU_BINARY_OPERATIONS:  # TP
        constants.append(
            f"constexpr auto ELTWISE_BINARY_OP = ckernel::EltwiseBinaryType::{mathop.cpp_enum_value};"
        )

    return constants


def generate_build_header(test_config) -> str:
    """
    Generate the contents of a C++ header file (build.h) with all configuration defines.

    This function creates a list of preprocessor #define statements based on the provided
    test configuration and profiler build option. The generated header is used to control
    build-time options for tests, such as data formats, math fidelity, accumulation modes,
    and other test-specific parameters.

    The resulting header content includes:
      - Basic configuration constants
      - Profiler and accumulation settings
      - Data format and math operation defines
      - Special configuration for multi-tile tests

    Data Format Inference:
      - Receive format configuration from test_config["formats"] and infers all formats for LLK APIs (unpack, math, pack) using the Python data format inference model.
      - A C++ FormatConfig struct is generated in build.h containing all format values,
        allowing C++ test files to access formats via `formats.unpack_src`, etc.

    Args:
        test_config (dict): Dictionary containing test configuration parameters.

    Returns:
        str: The complete contents of the build.h header file as a string.

    File location: <repository>/tests/helpers/include/build.h
    """

    # Checking if mandatory arguments are there
    if "buffer_A_address" not in test_config:
        raise ValueError(
            "Starting L1 address of A buffer not supplied. How will the kernel know where the stimuli is without it?"
        )

    if "buffer_B_address" not in test_config:
        raise ValueError(
            "Starting L1 address of B buffer not supplied. How will the kernel know where the stimuli is without it?"
        )

    if "result_buffer_address" not in test_config:
        raise ValueError(
            "Starting L1 address of result buffer not supplied. How will the kernel know where to write stimuli to?"
        )

    if "formats" not in test_config:
        raise ValueError("Format configuration not supplied")

    header_content = [
        "// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC",
        "//",
        "// SPDX-License-Identifier: Apache-2.0",
        "// AUTO-GENERATED CONFIGURATION HEADER. DO NOT EDIT MANUALLY!",
        "",
        "#pragma once",
        "",
        "#include <array>",
        "#include <type_traits>",
        "",
        '#include "operand.h"',
        '#include "llk_defs.h"',
        '#include "llk_sfpu_types.h"',
        # Conditionally include perf.h based on architecture
        (
            '#include "perf.h"'
            if get_chip_architecture() != ChipArchitecture.QUASAR
            else ""
        ),
        '#include "tensix_types.h"',
        "",
        "// Basic configuration",
        "constexpr std::uint32_t TILE_SIZE_CNT = 0x1000;",
    ]

    # ******** QUASAR specific ********
    if get_chip_architecture() == ChipArchitecture.QUASAR:
        # Implied math format
        implied_math_format = test_config.get(
            "implied_math_format", ImpliedMathFormat.No
        )
        header_content.append(  # TP
            f"constexpr bool IMPLIED_MATH_FORMAT = {implied_math_format.value};"
        )

        # Select unpacker
        unpacker_engine_sel = test_config.get(
            "unpacker_engine_sel", UnpackerEngine.UnpA
        )
        header_content.append(  # TP
            f"constexpr uint UNPACKER_ENGINE_SEL = p_unpacr::{unpacker_engine_sel.value};"
        )
    # *********************************

    formats = test_config["formats"]

    if "loop_factor" in test_config:  # RT ++
        header_content.append(
            f"constexpr int LOOP_FACTOR = {test_config['loop_factor']};"
        )

    if "unpack_transpose_faces" in test_config:  # RT ++
        header_content.append(
            f"constexpr bool UNPACK_TRANSPOSE_FACES = {test_config['unpack_transpose_faces'].value};"
        )

    if "unpack_transpose_within_face" in test_config:  # RT ++
        header_content.append(
            f"constexpr bool UNPACK_TRANSPOSE_WITHIN_FACE = {test_config['unpack_transpose_within_face'].value};"
        )

    # Matrix multiplication tile dimensions
    if "rt_dim" in test_config:  # RT ++
        header_content.append(f"constexpr uint32_t RT_DIM = {test_config['rt_dim']};")
    if "ct_dim" in test_config:  # RT ++
        header_content.append(f"constexpr uint32_t CT_DIM = {test_config['ct_dim']};")
    if "kt_dim" in test_config:  # RT ++
        header_content.append(f"constexpr uint32_t KT_DIM = {test_config['kt_dim']};")

    # partial face - support separate configurations for A and B ++
    partial_face_A = str(
        test_config.get("partial_face_A", test_config.get("partial_face", False))  # RT
    ).lower()
    header_content.append(f"constexpr bool PARTIAL_FACE_A = {partial_face_A};")
    header_content.append(f"constexpr bool PARTIAL_FACE_PACK = {partial_face_A};")

    partial_face_B = str(
        test_config.get("partial_face_B", test_config.get("partial_face", False))  # RT
    ).lower()
    header_content.append(f"constexpr bool PARTIAL_FACE_B = {partial_face_B};")
    header_content.append(f"constexpr bool PARTIAL_FACE_MATH = {partial_face_B};")

    # input tile dimensions
    in0_tile_r_dim = test_config.get("in0_tile_r_dim", 32)  # RT
    header_content.append(f"constexpr int in0_tile_r_dim = {in0_tile_r_dim};")

    if "in0_tile_c_dim" in test_config:  # RT
        header_content.append(
            f"constexpr int in0_tile_c_dim = {test_config['in0_tile_c_dim']};"
        )

    if "in1_tile_r_dim" in test_config:  # RT
        header_content.append(
            f"constexpr int in1_tile_r_dim = {test_config['in1_tile_r_dim']};"
        )

    if "in1_tile_c_dim" in test_config:  # RT
        header_content.append(
            f"constexpr int in1_tile_c_dim = {test_config['in1_tile_c_dim']};"
        )

    # RT face dimensions - use TEST_ prefix to avoid namespace collision with ckernel::FACE_R_DIM
    face_r_dim = test_config.get("face_r_dim", 16)  # TODO check if this is correct
    header_content.append(f"constexpr int TEST_FACE_R_DIM = {face_r_dim};")

    # RT, Face column dimension, typically 16
    if "face_c_dim" in test_config:
        header_content.append(
            f"constexpr int TEST_FACE_C_DIM = {test_config['face_c_dim']};"
        )

    # Number of faces - support separate configurations for A and B
    num_faces = test_config.get("num_faces", 4)
    header_content.append(f"constexpr std::uint32_t NUM_FACES = {num_faces};")  # RT

    # Legacy TILE_SIZE for tests that still use it (e.g., tilize sweep)
    # only one test as RT, ./sources/unpack_untilize_perf.cpp
    header_content.append(f"constexpr std::uint32_t TILE_SIZE = {16 * 16 * num_faces};")

    num_faces_A = test_config.get("num_faces_A", num_faces)
    num_faces_B = test_config.get("num_faces_B", num_faces)
    header_content.append(f"constexpr int num_faces = {num_faces};")
    header_content.append(f"constexpr int num_faces_A = {num_faces_A};")
    header_content.append(f"constexpr int num_faces_B = {num_faces_B};")

    # Tile byte size mapping
    TILE_SIZES = {
        DataFormat.Bfp8_b: 68,
        DataFormat.Float32: 256,
    }

    pack_size = TILE_SIZES.get(formats.output_format, 128)
    unpack_size_a = TILE_SIZES.get(formats.input_format, 128)
    unpack_size_b = TILE_SIZES.get(formats.input_format, 128)

    # Tiny tile flag, used to handle dimension
    if "tiny_tiles" in test_config:
        pack_size = (pack_size // num_faces) * (in0_tile_r_dim // face_r_dim)
        unpack_size_a = (unpack_size_a // num_faces_A) * (in0_tile_r_dim // face_r_dim)

    # All are RT, used in only few tests, but there wasn't any mechanism not to include them
    header_content.extend(
        [
            f"constexpr std::uint32_t TILE_SIZE_PACK = {pack_size};",
            f"constexpr std::uint32_t TILE_SIZE_UNPACK_A = {unpack_size_a};",
            f"constexpr std::uint32_t TILE_SIZE_UNPACK_B = {unpack_size_b};",
        ]
    )

    if "tile_cnt" in test_config:  # RT ++
        header_content.append(f"constexpr int TILE_CNT = {test_config['tile_cnt']};")

    if "narrow_tile" in test_config:  # RT ++
        header_content.append(
            f"constexpr bool NARROW_TILE = {test_config['narrow_tile']};"
        )

    # Unpack + result buffer addresses arrays generations, all addresses are RT params
    header_content.extend(
        [
            f"constexpr Operand buffer_A({hex(test_config['buffer_A_address'])}, {format_tile_sizes[formats.input_format if formats is not None else DataFormat.Float16_b]});",
            f"constexpr Operand buffer_B({hex(test_config['buffer_B_address'])}, {format_tile_sizes[formats.input_format if formats is not None else DataFormat.Float16_b]});",
            f"constexpr Operand buffer_Res({hex(test_config['result_buffer_address'])}, {format_tile_sizes[formats.output_format if formats is not None else DataFormat.Float16_b]});",
        ]
    )

    # Add optional buffer_C if specified
    if "buffer_C_address" in test_config:  # RT
        buffer_C_line = f"constexpr Operand buffer_C({hex(test_config['buffer_C_address'])}, {format_tile_sizes[formats.input_format if formats != None else DataFormat.Float16_b]});"
        header_content.append(buffer_C_line)

    if "dest_index" in test_config:  # RT ++
        header_content.append(f"constexpr int DST_INDEX = {test_config['dest_index']};")

    if "srca_reuse_count" in test_config:  # RT ++
        header_content.append(
            f"constexpr int SRCA_REUSE_COUNT = {test_config['srca_reuse_count']};"
        )

    # === TEMPLATE PARAMETERS ===

    if "throttle_level" in test_config:  # TP ++
        header_content.append(
            f"constexpr int THROTTLE_LEVEL = {test_config['throttle_level']};"
        )

    if "math_transpose_faces" in test_config:  # TP ++
        header_content.append(
            f"constexpr bool MATH_TRANSPOSE_FACES = {test_config['math_transpose_faces']};"
        )

    if "stochastic_rnd" in test_config:  # TP ++
        header_content.append(
            f"constexpr auto STOCHASTIC_RND = ckernel::{test_config['stochastic_rnd'].value};"
        )

    if "data_copy_type" in test_config:  # TP ++
        header_content.append(
            f"constexpr auto DATA_COPY_TYPE = ckernel::DataCopyType::{test_config['data_copy_type'].value};"
        )

    if "broadcast_type" in test_config:  # TP ++
        header_content.append(
            f"constexpr auto BROADCAST_TYPE = ckernel::BroadcastType::{test_config['broadcast_type'].value};"
        )

    if "acc_to_dest" in test_config:  # TP ++
        header_content.append(
            f"constexpr bool ACC_TO_DEST = {str(test_config['acc_to_dest']).lower()};"
        )

    if "reuse_dest" in test_config:  # TP ++
        header_content.append(
            f"constexpr auto REUSE_DEST_TYPE = ckernel::EltwiseBinaryReuseDestType::{test_config['reuse_dest'].name};"
        )

    if "disable_src_zero_flag" in test_config:  # TP ++
        header_content.append(
            f"constexpr bool disable_src_zero_flag = {str(test_config['disable_src_zero_flag']).lower()};"
        )

    if "math_fidelity" in test_config:  # TP ++
        header_content.append(
            f"constexpr std::uint32_t MATH_FIDELITY = {test_config['math_fidelity'].value};"
        )
    else:  # default value
        header_content.append(
            f"constexpr std::uint32_t MATH_FIDELITY = {MathFidelity.LoFi.value};"
        )

    if "approx_mode" in test_config:  # TP ++
        header_content.append(
            f"constexpr bool APPROX_MODE = {test_config['approx_mode'].value};"
        )
    else:  # default value
        header_content.append(
            f"constexpr bool APPROX_MODE = {ApproximationMode.No.value};"
        )

    # Dest synchronisation mode
    dest_sync = test_config.get("dest_sync", DestSync.Half)  # TP ++
    header_content.append(
        f"constexpr auto dest_sync = ckernel::DstSync::Sync{dest_sync.name};"
    )

    if "tilize" in test_config:  # TP / constexpr ++
        header_content.append(
            f"constexpr bool tilize_en = {test_config['tilize'].value};"
        )

        # Math operation configuration
    mathop = test_config.get("mathop", "no_mathop")
    if mathop != "no_mathop":  # check comments, RT/TP combo
        header_content.extend(["", "// Math operation configuration"])
        header_content.extend(_generate_operation_constants(mathop))

        # Handle reduce operations
        if mathop in REDUCE_OPERATIONS:  # RT
            header_content.append(
                f"constexpr auto REDUCE_DIM = ckernel::ReduceDim::{mathop.cpp_enum_value};"
            )

    if pool_type := test_config["pool_type"]:  # TP ++

        header_content.append(
            f"constexpr auto POOL_TYPE = ckernel::PoolType::{pool_type.value};"
        )

    # Optional extra unary operation (used when both a binary and unary op
    # need to be present in the same kernel, e.g. binary-eltwise followed by
    # SFPU unary).  If 'unary_op' exists, append its constant.

    if "unary_op" in test_config:  # TP
        # Only add if we haven't already added a unary operation from the main mathop
        if mathop == "no_mathop" or mathop not in SFPU_UNARY_OPERATIONS:
            header_content.extend(["", "// Additional SFPU unary operation"])
            header_content.append(
                f"constexpr auto SFPU_UNARY_OPERATION = SfpuType::{test_config['unary_op'].cpp_enum_value};"
            )

    if "add_top_row" in test_config:  # TP
        header_content.append("constexpr bool ADD_TOP_ROW = true;")

    if perf_run_type := test_config.get("perf_run_type"):  # TP
        header_content.extend(
            ["", f"constexpr auto PERF_RUN_TYPE = PerfRunType::{perf_run_type.name};"]
        )

    input_A_dimensions = test_config.get("input_A_dimensions", [32, 32])
    input_B_dimensions = test_config.get("input_B_dimensions", [32, 32])

    num_rows = 32
    num_cols = 32
    validate_tile_dimensions(input_A_dimensions[0], num_rows)
    validate_tile_dimensions(input_A_dimensions[1], num_cols)
    validate_tile_dimensions(input_B_dimensions[0], num_rows)
    validate_tile_dimensions(input_B_dimensions[1], num_cols)
    full_rt_dim = input_A_dimensions[0] // num_rows
    full_ct_dim = input_B_dimensions[1] // num_cols

    block_rt_dim = test_config.get("block_rt_dim", full_rt_dim)
    block_ct_dim = test_config.get("block_ct_dim", full_ct_dim)

    header_content.extend(
        [
            f"constexpr uint32_t FULL_RT_DIM = {full_rt_dim};",  # TP
            f"constexpr uint32_t FULL_CT_DIM = {full_ct_dim};",  # TP
            f"constexpr uint32_t BLOCK_CT_DIM = {block_ct_dim};",  # RT + TP
            f"constexpr uint32_t BLOCK_RT_DIM = {block_rt_dim};",  # RT + TP
        ]
    )

    # === DATA FORMAT INFERENCE & CONFIGURATION (always present, used in params.h) === TP

    header_content.append("// Data formats inferred by Python inference model")

    # Profiler Tests don't pass formats to the test config, so we need to set them here
    testname = test_config.get("testname", "")
    if "profiler" in testname:
        format = DataFormat.Float16
        formats = FormatConfig(format, format, format, format, format)

    # Dest accumulation
    dest_acc = test_config.get("dest_acc", DestAccumulation.No)
    header_content.append(f"constexpr bool is_fp32_dest_acc_en = {dest_acc.value};")

    # Check if this is an outlier format combination that requires dest_acc to be enabled
    if is_format_combination_outlier(
        formats.input_format, formats.output_format, dest_acc
    ):
        # Automatically enable dest_acc for outlier combinations
        dest_acc = DestAccumulation.Yes

    # Fused Test L1 to L1 : Input of first run is used as input for the second run ...
    # Not fusing: single L1-to-L1 iteration, so we retrieve one format configuration
    # L1_to_L1_iterations is the number of times we perform llk operations from L1 input tensor to L1 output tensor
    # If L1_to_L1_ITERATIONS is 1, we take input tensor from L1 -> unpack -> math -> pack -> L1
    # If L1_to_L1_ITERATIONS is greater than 1, we perform multiple iterations of unpack -> math -> pack, by taking results tensor in L1 to be input tensor of next iteration

    l1_to_l1_iterations = test_config.get("L1_to_L1_iterations", 1)

    # TP in blackhole and wormhole, RT in quasar
    unpack_to_dest = str(test_config.get("unpack_to_dest", False)).lower()
    header_content.append(f"constexpr bool unpack_to_dest = {unpack_to_dest};")
    header_content.append(f"constexpr bool UNPACKING_TO_DEST = {unpack_to_dest};")

    formats_config = data_formats(
        input_format=formats.input_format,
        output_format=formats.output_format,
        is_fp32_dest_acc_en=dest_acc,
        num_iterations=l1_to_l1_iterations,
        unpacking_to_dest=unpack_to_dest == "true",
        chip_arch=get_chip_architecture(),
        disable_format_inference=test_config.get("disable_format_inference", False),
    )

    # Check if we need to generate multiple format configurations

    if l1_to_l1_iterations > 1:
        # Generate format data as arrays that params.h can use to construct FormatConfig objects
        header_content.extend(
            [
                "// Format data for multiple L1-to-L1 iterations",
                f"constexpr std::uint32_t L1_to_L1_ITERATIONS = {l1_to_l1_iterations};",
                "#define FUSED_MULTIPLE_RUNS true",
            ]
        )

        # Create array of format configurations for multiple L1-to-L1 iterations
        unpack_a_in_values = [
            f"static_cast<std::underlying_type_t<DataFormat>>(DataFormat::{fmt.unpack_A_src.name})"
            for fmt in formats_config
        ]
        unpack_a_out_values = [
            f"static_cast<std::underlying_type_t<DataFormat>>(DataFormat::{fmt.unpack_A_dst.name})"
            for fmt in formats_config
        ]
        math_values = [
            f"static_cast<std::underlying_type_t<DataFormat>>(DataFormat::{fmt.math.name})"
            for fmt in formats_config
        ]
        pack_in_values = [
            f"static_cast<std::underlying_type_t<DataFormat>>(DataFormat::{fmt.pack_src.name})"
            for fmt in formats_config
        ]
        pack_out_values = [
            f"static_cast<std::underlying_type_t<DataFormat>>(DataFormat::{fmt.pack_dst.name})"
            for fmt in formats_config
        ]

        header_content.extend(
            [
                f"constexpr std::array<std::underlying_type_t<DataFormat>, L1_to_L1_ITERATIONS> UNPACK_A_IN_LIST = {{{', '.join(unpack_a_in_values)}}};",
                f"constexpr std::array<std::underlying_type_t<DataFormat>, L1_to_L1_ITERATIONS> UNPACK_A_OUT_LIST = {{{', '.join(unpack_a_out_values)}}};",
                f"constexpr std::array<std::underlying_type_t<DataFormat>, L1_to_L1_ITERATIONS> MATH_FORMAT_LIST = {{{', '.join(math_values)}}};",
                f"constexpr std::array<std::underlying_type_t<DataFormat>, L1_to_L1_ITERATIONS> PACK_IN_LIST = {{{', '.join(pack_in_values)}}};",
                f"constexpr std::array<std::underlying_type_t<DataFormat>, L1_to_L1_ITERATIONS> PACK_OUT_LIST = {{{', '.join(pack_out_values)}}};",
            ]
        )

    else:
        # Single iteration - use simple format inference
        # Generate format data as individual constants for single iteration
        formats_config = formats_config[0]
        header_content.extend(
            [
                "// Format data for single L1-to-L1 iteration",
                f"constexpr auto UNPACK_A_IN = static_cast<std::underlying_type_t<DataFormat>>(DataFormat::{formats_config.unpack_A_src.name});",
                f"constexpr auto UNPACK_A_OUT = static_cast<std::underlying_type_t<DataFormat>>(DataFormat::{formats_config.unpack_A_dst.name});",
                f"constexpr auto MATH_FORMAT = static_cast<std::underlying_type_t<DataFormat>>(DataFormat::{formats_config.math.name});",
                f"constexpr auto PACK_IN = static_cast<std::underlying_type_t<DataFormat>>(DataFormat::{formats_config.pack_src.name});",
                f"constexpr auto PACK_OUT = static_cast<std::underlying_type_t<DataFormat>>(DataFormat::{formats_config.pack_dst.name});",
            ]
        )

    header_content.append("")
    return "\n".join(header_content)
