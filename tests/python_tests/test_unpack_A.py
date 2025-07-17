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
    generate_param_ids,
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
broadcast_types = [BroadcastType.NONE, BroadcastType.COL, BroadcastType.ROW, BroadcastType.SCALAR]
disable_src_zero_flags = [False, True]
is_fp32_dest_acc_flags = [False, True] #not needed? 
acc_to_dest_flags = [False, True]
stoch_rounding_types = [StochRndType.NONE, StochRndType.Fpu, StochRndType.Pack, StochRndType.All]
reuse_dest_types = [EltwiseBinaryReuseDestType.NONE, EltwiseBinaryReuseDestType.DEST_TO_SRCA, EltwiseBinaryReuseDestType.DEST_TO_SRCB]
#unpack_to_dest_flags = [False, True]
transpose_of_faces_values = [0, 1]
within_face_16x16_transpose_values = [0, 1]
num_faces_values = [1, 2, 4]

# Generate format combinations
supported_formats = [DataFormat.Float16_b, DataFormat.Float32, DataFormat.Bfp8_b, DataFormat.Float16]
same_test_formats = input_output_formats(supported_formats, True)
cross_test_formats = input_output_formats(supported_formats, False)    
test_formats = same_test_formats + cross_test_formats

unpack_A_param_combinations = generate_unpack_A_params(
    broadcast_types=broadcast_types,
    disable_src_zero_flags=disable_src_zero_flags,
    is_fp32_dest_acc_flags=is_fp32_dest_acc_flags,
    acc_to_dest_flags=acc_to_dest_flags,
    stoch_rounding_types=stoch_rounding_types,
    reuse_dest_types=reuse_dest_types,
    #unpack_to_dest_flags=unpack_to_dest_flags,
    transpose_of_faces_values=transpose_of_faces_values,
    within_face_16x16_transpose_values=within_face_16x16_transpose_values,
    num_faces_values=num_faces_values,
)


# Combine testname + formats + unpack_A parameters
all_params = []
for format_combo in test_formats:
    for unpack_params in unpack_A_param_combinations:
        # Combine testname, format, and unpack_A parameters
        combined_params = ("unpack_A_test", format_combo) + unpack_params
        all_params.append(combined_params)


def create_simple_ids(all_params):
    """Create comprehensive but readable IDs for unpack_A tests"""
    ids = []
    for i, params in enumerate(all_params):
        # params = (testname, formats, broadcast_type, disable_src_zero, is_fp32_dest_acc, 
        #           acc_to_dest, stoch_rnd_type, reuse_dest, unpack_to_dest, transpose_of_faces, 
        #           within_face_16x16_transpose, num_faces)
        
        testname = params[0]
        formats = params[1]
        broadcast_type = params[2]
        disable_src_zero = params[3]
        is_fp32_dest_acc = params[4]
        acc_to_dest = params[5]
        stoch_rnd_type = params[6]
        reuse_dest = params[7]
        #unpack_to_dest = params[8]
        transpose_of_faces = params[8]
        within_face_16x16_transpose = params[9]
        num_faces = params[10]

        # Create a comprehensive but readable ID
        id_parts = [
            f"in_{formats.input_format.name}",
            f"out_{formats.output_format.name}",
            f"bcast_{broadcast_type.name}",
            f"disable_src_zero_{disable_src_zero}",
            f"fp32_dest_acc_{is_fp32_dest_acc}",
            f"acc_to_dest_{acc_to_dest}",
            f"stoch_rnd_{stoch_rnd_type.name}",
            f"reuse_dest_{reuse_dest.name}",
            #"unpack_to_dest_{unpack_to_dest}",f
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
    "testname, formats, broadcast_type, disable_src_zero, is_fp32_dest_acc, acc_to_dest, "
    "stoch_rnd_type, reuse_dest, transpose_of_faces, "                                          #unpack_to_dest,
    "within_face_16x16_transpose, num_faces",
    clean_params(all_params),
    ids=param_ids
)
def test_unpack_comprehensive(
    testname, formats, broadcast_type, disable_src_zero, is_fp32_dest_acc, acc_to_dest,
    stoch_rnd_type, reuse_dest, transpose_of_faces,                                         #unpack_to_dest,
    within_face_16x16_transpose, num_faces
):
    
    # Check if the format is supported
    arch = get_chip_architecture()
    unpack_to_dest = formats.input_format.is_32_bit()

    print(f"DEBUG: broadcast_type={broadcast_type}, acc_to_dest={acc_to_dest}, reuse_dest={reuse_dest}, unpack_to_dest={unpack_to_dest}")

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

    print("DEBUG: Test would continue...")
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
        "dest_acc": DestAccumulation.Yes if is_fp32_dest_acc else DestAccumulation.No,
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