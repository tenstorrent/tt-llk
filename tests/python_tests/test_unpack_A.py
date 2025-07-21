import pytest
import torch

from helpers.device import (
    collect_results,
    write_stimuli_to_l1,
)
from helpers.format_arg_mapping import format_dict, DestAccumulation
from helpers.format_config import DataFormat, BroadcastType, StochRndType, EltwiseBinaryReuseDestType
from helpers.golden_generators import DataCopyGolden, get_golden_generator
from helpers.chip_architecture import ChipArchitecture, get_chip_architecture
from helpers.stimuli_generator import generate_stimuli
from helpers.test_config import run_test
from helpers.utils import passed_test

from helpers.param_config import (
    clean_params,
    format_combination_sweep,
    generate_param_ids,
    generate_params,
    generate_unpack_A_params,
    input_output_formats,
)
# SUPPORTED FORMATS FOR TEST
supported_formats = [
    DataFormat.Float32,
    DataFormat.Float16,
    DataFormat.Float16_b,
    DataFormat.Bfp8_b,
]

# Define your parameter lists
broadcast_types = [BroadcastType.NONE] #, BroadcastType.COL, BroadcastType.ROW, BroadcastType.SCALAR
dest_acc = [DestAccumulation.Yes, DestAccumulation.No]
disable_src_zero_flags = [False] #, True
# is_fp32_dest_acc_flags removed - it's automatically handled in params.h
acc_to_dest_flags = [False] #, True
stoch_rounding_types = [StochRndType.NONE, StochRndType.Fpu, StochRndType.Pack, StochRndType.All]
reuse_dest_types = [EltwiseBinaryReuseDestType.NONE] #, EltwiseBinaryReuseDestType.DEST_TO_SRCA, EltwiseBinaryReuseDestType.DEST_TO_SRCB
#unpack_to_dest_flags = [False, True]
transpose_of_faces_values = [0, 1]
within_face_16x16_transpose_values = [0, 1]
num_faces_values = [4] #1, 2,

# Generate format combinations using both input_output_formats and FormatConfig approach
supported_formats = [DataFormat.Float16_b, DataFormat.Float32, DataFormat.Bfp8_b, DataFormat.Float16]

# Create InputOutputFormat combinations for your test (this is what your test expects)
same_test_formats = input_output_formats(supported_formats, True)
cross_test_formats = input_output_formats(supported_formats, False)    
test_formats = same_test_formats + cross_test_formats

# Generate unpack_A specific parameter combinations
unpack_A_param_combinations = generate_unpack_A_params(
    broadcast_types=broadcast_types,
    disable_src_zero_flags=disable_src_zero_flags,
    # is_fp32_dest_acc_flags removed - handled automatically in params.h
    acc_to_dest_flags=acc_to_dest_flags,
    stoch_rounding_types=stoch_rounding_types,
    reuse_dest_types=reuse_dest_types,
    transpose_of_faces_values=transpose_of_faces_values,
    within_face_16x16_transpose_values=within_face_16x16_transpose_values,
    num_faces_values=num_faces_values,
)

# Create unified parameter combinations
# This combines the power of generate_params with unpack_A specific parameters
all_params = []
testname = ["unpack_A_test"]

# Method 1: Use generate_params for base parameter structure (like datacopy test)
base_params = generate_params(testname, test_formats, dest_acc)

# Method 2: Extend base params with unpack_A specific parameters
for base_param in base_params:
    # base_param = (testname, format_config, acc_mode, approx, math, fidelity, num_tiles, dim, pool)
    # We only care about the first 3: testname, formats, dest_acc
    base_testname = base_param[0]
    formats = base_param[1] 
    base_dest_acc = base_param[2]
    
    for unpack_params in unpack_A_param_combinations:
        # unpack_params = (broadcast_type, disable_src_zero, 
        #                  acc_to_dest, stoch_rnd_type, reuse_dest, transpose_of_faces, 
        #                  within_face_16x16_transpose, num_faces)
        
        # Extract individual unpack parameters (is_fp32_dest_acc removed)
        broadcast_type = unpack_params[0]
        disable_src_zero = unpack_params[1]
        acc_to_dest = unpack_params[2]
        stoch_rnd_type = unpack_params[3]
        reuse_dest = unpack_params[4]
        transpose_of_faces = unpack_params[5]
        within_face_16x16_transpose = unpack_params[6]
        num_faces = unpack_params[7]
        
        # Create complete parameter tuple matching test signature
        # Use acc_to_dest from unpack_params instead of base_dest_acc for more control
        combined_params = (
            base_testname,  # testname
            formats,        # formats
            broadcast_type, # broadcast_type
            disable_src_zero, # disable_src_zero
            acc_to_dest,    # acc_to_dest (from unpack_params, not base_dest_acc)
            stoch_rnd_type, # stoch_rnd_type
            reuse_dest,     # reuse_dest
            transpose_of_faces, # transpose_of_faces
            within_face_16x16_transpose, # within_face_16x16_transpose
            num_faces       # num_faces
        )
        all_params.append(combined_params)

# Optional: If you want to use generate_params for additional control, 
# you can create FormatConfig objects and use generate_params for those:
# 
# # Create FormatConfig combinations for generate_params compatibility
# from helpers.param_config import format_combination_sweep
# format_configs = format_combination_sweep(supported_formats, all_same=False)
# 
# # Use generate_params for additional parameter control
# base_params = generate_params(
#     testnames=[testname],
#     format_combos=format_configs,
#     dest_acc=dest_acc,
#     tile_cnt=4,
# )
# 
# # Then extend base_params with unpack_A specific parameters similar to above


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
        stoch_rnd_type = params[5]
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
            f"stoch_rnd_{stoch_rnd_type.name}",
            f"reuse_dest_{reuse_dest.name}",
            f"transpose_faces_{transpose_of_faces}",
            f"within_face_transpose_{within_face_16x16_transpose}",
            f"num_faces_{num_faces}"
        ]
        
        id_str = "-".join(id_parts)
        ids.append(id_str)
    
    return ids

param_ids = create_simple_ids(all_params)
#param_ids = generate_param_ids(all_params)

@pytest.mark.parametrize(
    "testname, formats, broadcast_type, disable_src_zero, acc_to_dest, "
    "stoch_rnd_type, reuse_dest, transpose_of_faces, "
    "within_face_16x16_transpose, num_faces",
    clean_params(all_params),
    ids=param_ids
)
def test_unpack_comprehensive(
    testname, formats, broadcast_type, disable_src_zero, acc_to_dest,
    stoch_rnd_type, reuse_dest, transpose_of_faces,
    within_face_16x16_transpose, num_faces
):
    
    # Check if the format is supported
    arch = get_chip_architecture()
    unpack_to_dest = formats.input_format.is_32_bit()


    # Static assertion 1: broadcast + acc_to_dest + DEST_TO_SRCB
    if (broadcast_type != BroadcastType.NONE and 
        acc_to_dest and 
        reuse_dest == EltwiseBinaryReuseDestType.DEST_TO_SRCB):
        pytest.skip("Static assertion: broadcast + acc_to_dest + DEST_TO_SRCB not supported")

    # Static assertion 2: unpack_to_dest configuration restrictions  
    if not (((broadcast_type == BroadcastType.NONE and 
        not acc_to_dest and 
        reuse_dest == EltwiseBinaryReuseDestType.NONE) or 
        not unpack_to_dest)):
        pytest.skip("Static assertion: invalid configuration when unpacking to dest")

    # Static assertion 3: SCALAR broadcast + acc_to_dest
    if broadcast_type == BroadcastType.SCALAR and acc_to_dest:
        pytest.skip("Static assertion: broadcast scalar with acc_to_dest not supported")

    # Static assertion 4: DEST_TO_SRCA reuse mode not supported in current test framework
    if reuse_dest == EltwiseBinaryReuseDestType.DEST_TO_SRCA:
        pytest.skip("Static assertion: DEST_TO_SRCA reuse mode not supported in current test framework")

    if unpack_to_dest:
        # unpack_to_dest requires 32-bit input format
        if not (formats.input_format in [DataFormat.Float32]):
            pytest.skip("unpack_to_dest requires 32-bit input format")
        
        # unpack_to_dest + transpose_of_faces requires exactly 4 faces
        if transpose_of_faces and num_faces != 4:
            pytest.skip("unpack_to_dest + transpose_of_faces requires exactly 4 faces")
        
        # When unpack_to_dest=True, broadcast limitations don't apply (different code path)
        
    else:
        
        # COL broadcast limitations
        if broadcast_type == BroadcastType.COL:
            # DEST_TO_SRCB not supported (both architectures)
            if reuse_dest == EltwiseBinaryReuseDestType.DEST_TO_SRCB:
                pytest.skip("COL broadcast: DEST_TO_SRCB not supported")
            
            # Face count limitations differ by architecture
            if arch == ChipArchitecture.BLACKHOLE:
                if acc_to_dest:
                    if num_faces > 2:
                        pytest.skip("Blackhole COL broadcast with acc_to_dest: num_faces > 2 not supported")
                else:
                    if num_faces > 1:
                        pytest.skip("Blackhole COL broadcast without acc_to_dest: num_faces > 1 not supported")
            
            elif arch == ChipArchitecture.WORMHOLE:
                if num_faces > 1:
                    pytest.skip("Wormhole COL broadcast: num_faces > 1 not supported")
        
        # ROW broadcast limitations
        if broadcast_type == BroadcastType.ROW:
            if arch == ChipArchitecture.BLACKHOLE:
                if num_faces > 2:
                    pytest.skip("Blackhole ROW broadcast: num_faces > 2 not supported")
            
            elif arch == ChipArchitecture.WORMHOLE:
                if num_faces > 1:
                    pytest.skip("Wormhole ROW broadcast: num_faces > 1 not supported")

 
    # transpose_of_faces with 1 face makes no sense
    if transpose_of_faces and num_faces == 1:
       pytest.skip("Cannot transpose faces with only 1 face")

   
    input_dimensions = [64, 64]

    src_A, src_B, tile_cnt = generate_stimuli(
        formats.input_format, 
        formats.input_format, 
        input_dimensions=input_dimensions,
    )

    # generate golden generator    
    generate_golden = get_golden_generator(DataCopyGolden) 
    golden_tensor = generate_golden(src_A, formats.output_format)

    res_address = write_stimuli_to_l1(
        src_A, src_B, formats.input_format, formats.input_format, tile_count=tile_cnt
    )

    
    # BUILD THE COMPLETE TEST CONFIG
    test_config = {
        "formats": formats,
        "testname": testname,
        "tile_cnt": tile_cnt,
        "input_dimensions": input_dimensions,
        # NEW TEMPLATE PARAMETERS:
        "broadcast_type": broadcast_type,
        "acc_to_dest": acc_to_dest,
        "reuse_dest": reuse_dest,
        "unpack_to_dest": unpack_to_dest,
        "stoch_rnd_type": stoch_rnd_type,
        "dest_acc": DestAccumulation.Yes if acc_to_dest else DestAccumulation.No,
        #"is_fp32_dest_acc_en": is_fp32_dest_acc, 
        "disable_src_zero_flag": disable_src_zero,
        "transpose_of_faces": transpose_of_faces,
        "within_face_16x16_transpose": within_face_16x16_transpose,
        "num_faces": num_faces,
    }

    run_test(test_config)
    
    # Collect and validate results
    res_from_L1 = collect_results(formats, tile_count=tile_cnt, address=res_address)
    assert len(res_from_L1) == len(golden_tensor)

    res_tensor = torch.tensor(res_from_L1, dtype=format_dict[formats.output_format])
    assert passed_test(golden_tensor, res_tensor, formats.output_format)