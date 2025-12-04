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
    elif mathop in FPU_BINARY_OPERATIONS:
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
        raise Exception(
            "Starting L1 address of A buffer not supplied. How will the kernel know where the stimuli is without it?"
        )

    if "buffer_B_address" not in test_config:
        raise Exception(
            "Starting L1 address of B buffer not supplied. How will the kernel know where the stimuli is without it?"
        )

    if "result_buffer_address" not in test_config:
        raise Exception(
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

    if loop_factor := test_config.get("loop_factor"):  # RT
        header_content.append(f"constexpr int LOOP_FACTOR = {loop_factor};")

    if unpack_transpose_faces := test_config.get("unpack_transpose_faces"):  # RT
        header_content.append(
            f"constexpr bool UNPACK_TRANSPOSE_FACES = {unpack_transpose_faces.value};"
        )

    # RT
    if unpack_transpose_within_face := test_config.get("unpack_transpose_within_face"):
        header_content.append(
            f"constexpr bool UNPACK_TRANSPOSE_WITHIN_FACE = {unpack_transpose_within_face.value};"
        )

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

    if throttle := test_config.get("throttle_level"):  # TP
        header_content.append(f"constexpr int THROTTLE_LEVEL = {throttle};")

    if math_transpose_faces := test_config.get("math_transpose_faces"):  # TP
        header_content.append(
            f"constexpr bool MATH_TRANSPOSE_FACES = {math_transpose_faces};"
        )

    if stochastic_rnd := test_config.get("stochastic_rnd"):  # TP
        header_content.append(
            f"constexpr auto STOCHASTIC_RND = ckernel::{stochastic_rnd.value};"
        )

    if data_copy_type := test_config.get("data_copy_type"):  # TP
        header_content.append(
            f"constexpr auto DATA_COPY_TYPE = ckernel::DataCopyType::{data_copy_type.value};"
        )

    if broadcast_type := test_config.get("broadcast_type"):  # TP
        header_content.append(
            f"constexpr auto BROADCAST_TYPE = ckernel::BroadcastType::{broadcast_type.value};"
        )

    if acc_to_dest := test_config.get("acc_to_dest"):  # TP
        header_content.append(
            f"constexpr bool ACC_TO_DEST = {str(acc_to_dest).lower()};"
        )

    if reuse_dest := test_config.get("reuse_dest"):  # TP
        header_content.append(
            f"constexpr auto REUSE_DEST_TYPE = ckernel::EltwiseBinaryReuseDestType::{reuse_dest.name};"
        )

    if disable_src_zero_flag := test_config.get("disable_src_zero_flag"):  # TP
        header_content.append(
            f"constexpr bool disable_src_zero_flag = {str(disable_src_zero_flag).lower()};"
        )

    if num_faces := test_config.get("num_faces"):  # RT
        header_content.append(f"constexpr std::uint32_t NUM_FACES = {num_faces};")

    if narrow_tile := test_config.get("narrow_tile"):  # RT
        header_content.append(f"constexpr bool NARROW_TILE = {narrow_tile};")

    if math_fidelity := test_config.get("math_fidelity"):  # TP
        header_content.append(
            f"constexpr std::uint32_t MATH_FIDELITY = {math_fidelity.value};"
        )
    else:  # default value
        header_content.append(
            f"constexpr std::uint32_t MATH_FIDELITY = {MathFidelity.LoFi.value};"
        )

    if approx_mode := test_config.get("approx_mode"):  # TP
        header_content.append(f"constexpr bool APPROX_MODE = {approx_mode.value};")
    else:  # default value
        header_content.append(
            f"constexpr bool APPROX_MODE = {ApproximationMode.No.value};"
        )

    # partial face - support separate configurations for A and B

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
    in0_tile_r_dim = test_config.get("in0_tile_r_dim", 32)
    header_content.append(f"constexpr int in0_tile_r_dim = {in0_tile_r_dim};")

    if in0_tile_c_dim := test_config.get("in0_tile_c_dim"):  # RT
        header_content.append(f"constexpr int in0_tile_c_dim = {in0_tile_c_dim};")

    if in1_tile_r_dim := test_config.get("in1_tile_r_dim"):  # RT
        header_content.append(f"constexpr int in1_tile_r_dim = {in1_tile_r_dim};")

    if in1_tile_c_dim := test_config.get("in1_tile_c_dim"):  # RT
        header_content.append(f"constexpr int in1_tile_c_dim = {in1_tile_c_dim};")

    # RT face dimensions - use TEST_ prefix to avoid namespace collision with ckernel::FACE_R_DIM
    if face_r_dim := test_config.get("face_r_dim"):
        header_content.append(f"constexpr int TEST_FACE_R_DIM = {face_r_dim};")

    # RT, Face column dimension, typically 16
    if face_c_dim := test_config.get("face_c_dim"):
        header_content.append(f"constexpr int TEST_FACE_C_DIM = {face_c_dim};")

    # Number of faces - support separate configurations for A and B
    num_faces = test_config.get("num_faces", 4)

    # Legacy TILE_SIZE for tests that still use it (e.g., tilize sweep)
    header_content.append(f"constexpr std::uint32_t TILE_SIZE = {16 * 16 * num_faces};")

    num_faces_A = test_config.get("num_faces_A", num_faces)
    num_faces_B = test_config.get("num_faces_B", num_faces)
    header_content.append(f"constexpr int num_faces = {num_faces};")
    header_content.append(f"constexpr int num_faces_A = {num_faces_A};")
    header_content.append(f"constexpr int num_faces_B = {num_faces_B};")

    # tile size
    formats = test_config["formats"]
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

    # All are RT, used in only few tests, but there wasn't any mechanism to include them
    header_content.extend(
        [
            f"constexpr std::uint32_t TILE_SIZE_PACK = {pack_size};",
            f"constexpr std::uint32_t TILE_SIZE_UNPACK_A = {unpack_size_a};",
            f"constexpr std::uint32_t TILE_SIZE_UNPACK_B = {unpack_size_b};",
        ]
    )

    # Dest synchronisation mode
    dest_sync = test_config.get("dest_sync", DestSync.Half)  # TP
    header_content.append(
        f"constexpr auto dest_sync = ckernel::DstSync::Sync{dest_sync.name};"
    )

    # Destination index configuration
    if dst_index := test_config("dst_index"):  # RT
        header_content.append(f"constexpr int DST_INDEX = {dst_index};")

    if tilize_en := test_config.get("tilize"):  # TP / constexpr
        header_content.append(f"constexpr bool tilize_en = {tilize_en.value};")

    # Reuse A times
    if srca_reuse_count := test_config.get("srca_reuse_count"):  # RT
        header_content.append(f"constexpr int SRCA_REUSE_COUNT = {srca_reuse_count};")

    # === DATA FORMAT INFERENCE & CONFIGURATION ===

    # Data Format Inference will now occur from the python-end, gives visibility on all formats for test case
    # DATA_FORMAT_INFERENCE_MODEL is no longer defined in build.h, thus inference is deactivated, only enabled from python-end
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

    # TP for fused tests in params.h, this one is weird cuz it's used to generate a lot of other variables that are used in just a few tests
    l1_to_l1_iterations = test_config.get("L1_to_L1_iterations", 1)
    header_content.append(
        f"constexpr std::uint32_t L1_to_L1_ITERATIONS = {l1_to_l1_iterations};"
    )

    # TP in blackhole and wormhole, RT in quasar
    unpack_to_dest = str(test_config.get("unpack_to_dest", False)).lower()
    header_content.append(f"constexpr bool unpack_to_dest = {unpack_to_dest};")

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
        header_content.append("// Format data for multiple L1-to-L1 iterations")
        header_content.append("#define FUSED_MULTIPLE_RUNS true")

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
            if "pool_type" in test_config:  # TP
                pool_type = test_config["pool_type"]
                header_content.append(
                    f"constexpr auto POOL_TYPE = ckernel::PoolType::{pool_type.value};"
                )

    # Optional extra unary operation (used when both a binary and unary op
    # need to be present in the same kernel, e.g. binary-eltwise followed by
    # SFPU unary).  If 'unary_op' exists, append its constant.
    if unary_extra := test_config.get("unary_op"):  # TP
        # Only add if we haven't already added a unary operation from the main mathop
        if mathop == "no_mathop" or mathop not in SFPU_UNARY_OPERATIONS:
            header_content.extend(["", "// Additional SFPU unary operation"])
            header_content.append(
                f"constexpr auto SFPU_UNARY_OPERATION = SfpuType::{unary_extra.cpp_enum_value};"
            )

    # Destination sync mode configuration
    if dst_sync := test_config.get("dst_sync"):  # TP
        header_content.extend(
            [
                "",
                "// Destination sync configuration",
                f"constexpr auto DST_SYNC = ckernel::DstSync::{dst_sync.value};",
            ]
        )

    # Multi-tile test configuration
    header_content.extend(["", "// Multi-tile test configuration"])

    if tile_cnt := test_config["tile_cnt"]:  # RT
        header_content.append(f"constexpr int TILE_CNT = {tile_cnt};")

    # Unpack + result buffer addresses arrays generations, all addresses are RT params
    header_content.extend(
        [
            f"constexpr Operand buffer_A({hex(test_config['buffer_A_address'])}, {format_tile_sizes[formats.input_format if formats is not None else DataFormat.Float16_b]});",
            f"constexpr Operand buffer_B({hex(test_config['buffer_B_address'])}, {format_tile_sizes[formats.input_format if formats is not None else DataFormat.Float16_b]});",
            f"constexpr Operand buffer_Res({hex(test_config['result_buffer_address'])}, {format_tile_sizes[formats.output_format if formats is not None else DataFormat.Float16_b]});",
        ]
    )

    # Add optional buffer_C if specified
    if buffer_C_address := test_config.get("buffer_C_address"):  # RT
        buffer_C_line = f"constexpr Operand buffer_C({hex(buffer_C_address)}, {format_tile_sizes[formats.input_format if formats != None else DataFormat.Float16_b]});"
        header_content.append(buffer_C_line)

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
            f"constexpr uint32_t FULL_RT_DIM = {full_rt_dim};",
            f"constexpr uint32_t FULL_CT_DIM = {full_ct_dim};",
            f"constexpr uint32_t BLOCK_CT_DIM = {block_ct_dim};",
            f"constexpr uint32_t BLOCK_RT_DIM = {block_rt_dim};",
        ]
    )

    # Matrix multiplication tile dimensions
    if rt_dim := test_config.get("rt_dim"):  # RT
        header_content.append(f"constexpr uint32_t RT_DIM = {rt_dim};")
    if ct_dim := test_config.get("ct_dim"):  # RT
        header_content.append(f"constexpr uint32_t CT_DIM = {ct_dim};")
    if kt_dim := test_config.get("kt_dim"):  # RT
        header_content.append(f"constexpr uint32_t KT_DIM = {kt_dim};")

    if "add_top_row" in test_config:  # Add top row flag
        header_content.append("constexpr bool ADD_TOP_ROW = true;")

    if perf_run_type := test_config.get("perf_run_type"):  # TP
        header_content.extend(
            ["", f"constexpr auto PERF_RUN_TYPE = PerfRunType::{perf_run_type.name};"]
        )

    header_content.append("")
    return "\n".join(header_content)
