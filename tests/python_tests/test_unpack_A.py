# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
#
# SPDX-License-Identifier: Apache-2.0

import pytest
from z3 import And, BoolVal, If, Implies, IntVal, Not, Or, Solver, sat

from helpers.chip_architecture import ChipArchitecture, get_chip_architecture
from helpers.device import (
    collect_results,
    write_stimuli_to_l1,
)
from helpers.format_arg_mapping import DestAccumulation, StochasticRounding, format_dict
from helpers.format_config import (
    BroadcastType,
    DataFormat,
    EltwiseBinaryReuseDestType,
)
from helpers.golden_generators import (
    DataCopyGolden,
    ScalarBroadcastGolden,
    TransposeGolden,
    get_golden_generator,
)
from helpers.param_config import (
    clean_params,
    generate_params,
    generate_unpack_A_params,
    input_output_formats,
)
from helpers.stimuli_generator import generate_stimuli
from helpers.test_config import run_test
from helpers.utils import passed_test

# SUPPORTED FORMATS FOR TEST
supported_formats = [
    DataFormat.Float32,
    DataFormat.Float16,
    DataFormat.Float16_b,
    DataFormat.Bfp8_b,
]

# Define parameter lists
broadcast_types = [
    BroadcastType.None_,
    BroadcastType.Column,
    BroadcastType.Row,
    BroadcastType.Scalar,
]
dest_acc = [DestAccumulation.Yes, DestAccumulation.No]
disable_src_zero_flags = [False, True]
acc_to_dest_flags = [False, True]
stochastic_rnd = [
    StochasticRounding.No,
    StochasticRounding.Fpu,
    StochasticRounding.Pack,
    StochasticRounding.All,
]
reuse_dest_types = [
    EltwiseBinaryReuseDestType.NONE,
    EltwiseBinaryReuseDestType.DEST_TO_SRCA,
    EltwiseBinaryReuseDestType.DEST_TO_SRCB,
]
transpose_of_faces_values = [0, 1]
within_face_16x16_transpose_values = [0, 1]
num_faces_values = [1, 2, 4]


# Use only cross_test_formats as it already includes same-format combinations
test_formats = input_output_formats(supported_formats, False)


# Generate unpack_A specific parameter combinations
unpack_A_param_combinations = generate_unpack_A_params(
    broadcast_types=broadcast_types,
    disable_src_zero_flags=disable_src_zero_flags,
    # is_fp32_dest_acc_flags removed - handled automatically in params.h
    acc_to_dest_flags=acc_to_dest_flags,
    stoch_rounding_types=stochastic_rnd,
    reuse_dest_types=reuse_dest_types,
    transpose_of_faces_values=transpose_of_faces_values,
    within_face_16x16_transpose_values=within_face_16x16_transpose_values,
    num_faces_values=num_faces_values,
)

# Create unified parameter combinations
# This combines the power of generate_params with unpack_A specific parameters
all_params = []
testname = ["unpack_A_test"]

# Use generate_params for base parameter structure (like datacopy test)
base_params = list(
    generate_params(testnames=testname, formats=test_formats)
)  # Convert itertools.product to list

# Extend base params with unpack_A specific parameters
for base_param in base_params:
    # base_param = (testname, format_config) - new format from main branch
    base_testname = base_param[0]
    formats = base_param[1]

    for unpack_params in unpack_A_param_combinations:
        # unpack_params = (broadcast_type, disable_src_zero,
        #                  acc_to_dest, stoch_rnd_type, reuse_dest, transpose_of_faces,
        #                  within_face_16x16_transpose, num_faces)

        broadcast_type = unpack_params[0]
        disable_src_zero = unpack_params[1]
        acc_to_dest = unpack_params[2]
        stochastic_rnd = unpack_params[3]
        reuse_dest = unpack_params[4]
        transpose_of_faces = unpack_params[5]
        within_face_16x16_transpose = unpack_params[6]
        num_faces = unpack_params[7]

        # Create complete parameter tuple matching test signature
        combined_params = (
            base_testname,  # testname
            formats,  # formats
            broadcast_type,  # broadcast_type
            disable_src_zero,  # disable_src_zero
            acc_to_dest,  # acc_to_dest
            stochastic_rnd,  # stochastic_rnd
            reuse_dest,  # reuse_dest
            transpose_of_faces,  # transpose_of_faces
            within_face_16x16_transpose,  # within_face_16x16_transpose
            num_faces,  # num_faces
        )
        all_params.append(combined_params)


def filter_params_with_z3(all_params):
    """Use Z3 to filter valid parameter combinations based on hardware constraints"""

    arch = get_chip_architecture()

    valid_params = []

    for params in all_params:
        # Extract parameters from tuple
        (
            testname,
            formats,
            broadcast_type,
            disable_src_zero,
            acc_to_dest,
            stochastic_rnd,
            reuse_dest,
            transpose_of_faces,
            within_face_16x16_transpose,
            num_faces,
        ) = params

        # Create Z3 solver
        s = Solver()

        # Convert enum values to integers for Z3
        # Map BroadcastType string values to integers
        broadcast_mapping = {"NONE": 0, "COL": 1, "ROW": 2, "SCALAR": 3}
        if hasattr(broadcast_type, "value") and isinstance(broadcast_type.value, str):
            broadcast_val = broadcast_mapping.get(broadcast_type.value, 0)
        else:
            broadcast_val = (
                broadcast_type.value if hasattr(broadcast_type, "value") else 0
            )

        reuse_dest_val = reuse_dest.value if hasattr(reuse_dest, "value") else 0
        stoch_rnd_val = stochastic_rnd.value if hasattr(stochastic_rnd, "value") else 0

        # Z3 variables representing our parameters
        broadcast = IntVal(broadcast_val)  # 0=NONE, 1=COL, 2=ROW, 3=SCALAR
        acc_to_dest_z3 = BoolVal(acc_to_dest)
        reuse_dest_z3 = IntVal(reuse_dest_val)  # 0=NONE, 1=DEST_TO_SRCA, 2=DEST_TO_SRCB
        transpose_faces = BoolVal(transpose_of_faces == 1)
        num_faces_z3 = IntVal(num_faces)
        unpack_to_dest = BoolVal(formats.input_format.is_32_bit() and acc_to_dest)
        is_blackhole = BoolVal(arch == ChipArchitecture.BLACKHOLE)
        is_wormhole = BoolVal(arch == ChipArchitecture.WORMHOLE)

        # Define constraint predicates using Z3
        broadcast_none = broadcast == 0
        broadcast_col = broadcast == 1
        broadcast_row = broadcast == 2
        broadcast_scalar = broadcast == 3

        reuse_none = reuse_dest_z3 == 0
        reuse_srca = reuse_dest_z3 == 1
        reuse_srcb = reuse_dest_z3 == 2

        # Static assertion 1: broadcast + acc_to_dest + DEST_TO_SRCB
        constraint1 = Not(And(Not(broadcast_none), acc_to_dest_z3, reuse_srcb))

        # Static assertion 2: unpack_to_dest configuration restrictions
        valid_unpack_config = Or(
            And(broadcast_none, Not(acc_to_dest_z3), reuse_none), Not(unpack_to_dest)
        )
        constraint2 = valid_unpack_config

        # Static assertion 3: SCALAR broadcast + acc_to_dest
        constraint3 = Not(And(broadcast_scalar, acc_to_dest_z3))

        # Static assertion 4: DEST_TO_SRCA only supported when broadcast = NONE
        constraint4 = Implies(reuse_srca, broadcast_none)

        # unpack_to_dest specific constraints
        unpack_constraints = If(
            unpack_to_dest,
            And(
                # unpack_to_dest + transpose_of_faces requires exactly 4 faces
                Implies(transpose_faces, num_faces_z3 == 4)
            ),
            True,
        )

        # Architecture-specific broadcast constraints (when not unpack_to_dest)
        broadcast_constraints = If(
            Not(unpack_to_dest),
            And(
                # COL broadcast limitations
                Implies(
                    broadcast_col,
                    And(
                        # DEST_TO_SRCB not supported
                        Not(reuse_srcb),
                        # Architecture-specific face count limits
                        If(
                            is_blackhole,
                            If(
                                acc_to_dest_z3,
                                num_faces_z3
                                <= 2,  # Blackhole COL + acc_to_dest: <= 2 faces
                                num_faces_z3
                                <= 1,  # Blackhole COL no acc_to_dest: <= 1 face
                            ),
                            # Wormhole
                            num_faces_z3 <= 1,  # Wormhole COL: <= 1 face
                        ),
                    ),
                ),
                # ROW broadcast limitations
                Implies(
                    broadcast_row,
                    And(
                        # DEST_TO_SRCB not supported with ROW broadcast (hardware doesn't implement it)
                        Not(reuse_srcb),
                        If(
                            is_blackhole,
                            num_faces_z3 <= 2,  # Blackhole ROW: <= 2 faces
                            num_faces_z3 <= 1,  # Wormhole ROW: <= 1 face
                        ),
                    ),
                ),
            ),
            True,
        )

        # transpose_constraint removed - transpose operations work with num_faces=1

        # User constraint: transpose_of_faces and within_face_16x16_transpose are mutually inclusive
        within_face_transpose = BoolVal(within_face_16x16_transpose == 1)
        transpose_mutual_constraint = transpose_faces == within_face_transpose

        # specific_exclusion_constraint removed - allow all format conversions for testing

        # Exclude acc_to_dest=True for simple datacopy operations
        # Hardware produces 2x scaling when both srcA and srcB load same data
        datacopy_acc_to_dest_constraint = Not(
            And(acc_to_dest_z3, Not(transpose_faces), broadcast_none, reuse_none)
        )

        # Block num_faces != 4 for Bfp8_b format due to hardware bug
        # Hardware block indexing logic hardcoded for 4-face operation
        bfp8_num_faces_constraint = If(
            Or(
                BoolVal(formats.input_format == DataFormat.Bfp8_b),
                BoolVal(formats.output_format == DataFormat.Bfp8_b),
            ),
            num_faces_z3 == 4,
            True,
        )

        # Block num_faces != 4 for COL/ROW broadcast operations (LLK limitation)
        # SCALAR broadcast supports num_faces < 4
        broadcast_num_faces_constraint = If(
            Or(broadcast_col, broadcast_row), num_faces_z3 == 4, True
        )

        # Add all constraints to solver
        s.add(
            constraint1,
            constraint2,
            constraint3,
            constraint4,
            unpack_constraints,
            broadcast_constraints,
            transpose_mutual_constraint,
            datacopy_acc_to_dest_constraint,
            bfp8_num_faces_constraint,
            broadcast_num_faces_constraint,
            # float32_transpose_constraint,
        )

        # Check if this parameter combination is valid
        if s.check() == sat:
            valid_params.append(params)

    return valid_params


# Apply Z3 constraint filtering
all_params = filter_params_with_z3(all_params)


def create_simple_ids(all_params):
    """Create comprehensive but readable IDs for unpack_A tests"""
    ids = []
    for i, params in enumerate(all_params):
        # params = (testname, formats, broadcast_type, disable_src_zero,
        #           acc_to_dest, stoch_rnd_type, reuse_dest, transpose_of_faces,
        #           within_face_16x16_transpose, num_faces)

        testname = params[0]
        formats = params[1]
        broadcast_type = params[2]
        disable_src_zero = params[3]
        acc_to_dest = params[4]
        stochastic_rnd = params[5]
        reuse_dest = params[6]
        transpose_of_faces = params[7]
        within_face_16x16_transpose = params[8]
        num_faces = params[9]

        # Create a comprehensive but readable ID
        id_parts = [
            f"in_{formats.input_format.name}",
            f"out_{formats.output_format.name}",
            f"bcast_{broadcast_type.name}",
            f"disable_src_zero_{disable_src_zero}",
            f"acc_to_dest_{acc_to_dest}",
            f"stoch_rnd_{stochastic_rnd.name}",
            f"reuse_dest_{reuse_dest.name}",
            f"transpose_faces_{transpose_of_faces}",
            f"within_face_transpose_{within_face_16x16_transpose}",
            f"num_faces_{num_faces}",
        ]

        id_str = "-".join(id_parts)
        ids.append(id_str)

    return ids


param_ids = create_simple_ids(all_params)
# param_ids = generate_param_ids(all_params)


@pytest.mark.parametrize(
    "testname, formats, broadcast_type, disable_src_zero, acc_to_dest, "
    "stochastic_rnd, reuse_dest, transpose_of_faces, "
    "within_face_16x16_transpose, num_faces",
    clean_params(all_params),
    ids=param_ids,
)
def test_unpack_comprehensive(
    testname,
    formats,
    broadcast_type,
    disable_src_zero,
    acc_to_dest,
    stochastic_rnd,
    reuse_dest,
    transpose_of_faces,
    within_face_16x16_transpose,
    num_faces,
):
    import torch

    # Get architecture and compute unpack_to_dest
    arch = get_chip_architecture()
    unpack_to_dest = formats.input_format.is_32_bit() and acc_to_dest

    # Note: All constraint validation has been done by Z3 during parameter generation
    # No need for pytest.skip() calls - invalid combinations have been filtered out

    input_dimensions = [32, 32]

    src_A, src_B, tile_cnt = generate_stimuli(
        formats.input_format,
        formats.input_format,
        input_dimensions=input_dimensions,
    )

    # generate golden tensor with proper broadcast and transpose handling
    # PRIORITY: Scalar broadcast takes precedence over transpose operations
    # (transposing a uniform scalar value still yields a uniform scalar value)
    if broadcast_type == BroadcastType.Scalar:
        # Scalar broadcast: replicate first element across entire tile
        # Transpose operations don't change uniform data
        generate_golden = get_golden_generator(ScalarBroadcastGolden)
        golden_tensor = generate_golden(
            src_A, formats.output_format, num_faces, input_dimensions
        )
    elif transpose_of_faces == 1 and within_face_16x16_transpose == 1:
        # Apply both transpose operations (for non-scalar broadcasts)
        transpose_golden = get_golden_generator(TransposeGolden)
        # First apply within-face transpose, then face transpose
        temp_tensor = transpose_golden.transpose_within_faces(
            src_A, formats.output_format, input_dimensions, num_faces
        )
        golden_tensor = transpose_golden.transpose_faces(
            temp_tensor, formats.output_format, input_dimensions, num_faces
        )
    elif transpose_of_faces == 1:
        # Only face transpose (should not happen due to mutual inclusivity constraint)
        transpose_golden = get_golden_generator(TransposeGolden)
        golden_tensor = transpose_golden.transpose_faces(
            src_A, formats.output_format, input_dimensions, num_faces
        )
    elif within_face_16x16_transpose == 1:
        # Only within-face transpose (should not happen due to mutual inclusivity constraint)
        transpose_golden = get_golden_generator(TransposeGolden)
        golden_tensor = transpose_golden.transpose_within_faces(
            src_A, formats.output_format, input_dimensions, num_faces
        )
    else:
        # No transpose - handle based on reuse_dest behavior
        if reuse_dest == EltwiseBinaryReuseDestType.DEST_TO_SRCA and acc_to_dest:
            # DEST_TO_SRCA: destination registers get moved to srcA for reuse
            # This creates a feedback loop where processed data gets reused as source

            input_tensor = torch.tensor(src_A, dtype=format_dict[formats.input_format])
            face_size = 256  # 16x16 face

            if num_faces == 1:
                # Single face with DEST_TO_SRCA + acc_to_dest:
                # Hardware processes first half of face (128 elements), then reuses/duplicates for second half
                # DEST_TO_SRCA causes the first 128 elements to be processed, then repeated for elements 128-255
                input_face = input_tensor[:face_size].to(
                    format_dict[formats.output_format]
                )
                first_half = input_face[:128]  # First 128 elements
                # Duplicate first half for second half due to DEST_TO_SRCA register reuse
                golden_tensor = torch.cat([first_half, first_half])
            else:
                # Multiple faces: DEST_TO_SRCA applies duplication pattern within each face
                # Each face behaves like single face - first 128 elements duplicated for second 128 elements
                result = torch.zeros(
                    face_size * num_faces, dtype=format_dict[formats.output_format]
                )

                for face_idx in range(num_faces):
                    face_start = face_idx * face_size
                    face_end = face_start + face_size
                    input_face = input_tensor[face_start:face_end].to(
                        format_dict[formats.output_format]
                    )

                    # Apply same duplication pattern as single face within each face
                    first_half = input_face[:128]  # First 128 elements of this face
                    face_output = torch.cat(
                        [first_half, first_half]
                    )  # Duplicate first half
                    result[face_start:face_end] = face_output

                golden_tensor = result
        else:
            # Regular data copy for other reuse types or no acc_to_dest
            generate_golden = get_golden_generator(DataCopyGolden)
            golden_tensor = generate_golden(
                src_A, formats.output_format, num_faces, input_dimensions
            )

    # BUILD THE COMPLETE TEST CONFIG
    test_config = {
        "formats": formats,
        "testname": testname,
        "tile_cnt": tile_cnt,
        "input_dimensions": input_dimensions,
        "broadcast_type": broadcast_type,
        "acc_to_dest": acc_to_dest,
        "reuse_dest": reuse_dest,
        "unpack_to_dest": unpack_to_dest,
        "stochastic_rnd": stochastic_rnd,
        "dest_acc": DestAccumulation.Yes if acc_to_dest else DestAccumulation.No,
        # "is_fp32_dest_acc_en": is_fp32_dest_acc,
        "disable_src_zero_flag": disable_src_zero,
        "unpack_transpose_faces": transpose_of_faces,
        "unpack_transpose_within_face": within_face_16x16_transpose,
        "num_faces": num_faces,
    }

    res_address = write_stimuli_to_l1(
        test_config,
        src_A,
        src_B,
        formats.input_format,
        formats.input_format,
        tile_count_A=tile_cnt,
        tile_count_B=tile_cnt,
    )

    run_test(test_config)

    # Collect and validate results
    res_from_L1 = collect_results(
        formats, tile_count=tile_cnt, address=res_address, num_faces=num_faces
    )
    assert len(res_from_L1) == len(golden_tensor)

    res_tensor = torch.tensor(res_from_L1, dtype=format_dict[formats.output_format])
    assert passed_test(golden_tensor, res_tensor, formats.output_format)
