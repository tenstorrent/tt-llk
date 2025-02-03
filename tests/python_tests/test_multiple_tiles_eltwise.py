import pytest
import torch
import os
from helpers import *

def generate_golden(op, operand1, operand2, data_format):
    if( data_format == "Float16" or data_format == "Float16_b"):
        tensor1_float = operand1.clone().detach().to(format_dict[data_format])
        tensor2_float = operand2.clone().detach().to(format_dict[data_format])
    else:
        tensor1_float = operand1.clone().detach().to(format_dict["Float16_b"])
        tensor2_float = operand2.clone().detach().to(format_dict["Float16_b"])

    if(op==1):
        res = tensor1_float + tensor2_float
    elif(op==2):
        res = tensor1_float - tensor2_float
    elif(op==3):
        res = tensor1_float * tensor2_float
    else:
        raise ValueError("Unsupported operation!")
    
    return res.tolist()

@pytest.mark.parametrize("mathop", range(1,4))
@pytest.mark.parametrize("tile_cnt", range(1,4))
@pytest.mark.parametrize("format", ["Bfp8_b", "Float16_b", "Float16"])
@pytest.mark.parametrize("dest_acc", ["","DEST_ACC"])
@pytest.mark.parametrize("testname", ["multiple_tiles_eltwise_test"])
def test_multiple_kernels(format, testname, tile_cnt, mathop, dest_acc):

    # prepare setup for running kernels

    pack_start_address = 0x1a000 + 2*4096*tile_cnt
    pack_addresses = [pack_start_address + 0x1000 * i for i in range(tile_cnt)]
    pack_addresses_formatted = format_kernel_list(pack_addresses, as_hex=True)

    #context = init_debuda()
    src_A, src_B = generate_stimuli(format,tile_cnt = tile_cnt)
    golden = generate_golden(mathop,src_A,src_B,format)
    write_stimuli_to_l1(src_A,src_B,format,tile_cnt)

    test_config = {
        "input_format": format,
        "output_format": format,
        "testname": testname,
        "dest_acc": dest_acc,
        "mathop" : mathop,
        "kern_cnt" : tile_cnt,
        "pack_addr_cnt" : len(pack_addresses),
        "pack_addrs" : pack_addresses_formatted,
        "unp_a_addr_cnt": tile_cnt
    }
    
    make_cmd = generate_make_command(test_config)
    os.system(f"cd .. && {make_cmd}")

    run_elf_files(testname)

    os.system("cd .. && make clean")

    assert read_words_from_device("0,0", 0x19FF4, word_count=1)[0].to_bytes(4, 'big') == b'\x00\x00\x00\x01'
    assert read_words_from_device("0,0", 0x19FF8, word_count=1)[0].to_bytes(4, 'big') == b'\x00\x00\x00\x01'
    assert read_words_from_device("0,0", 0x19FFC, word_count=1)[0].to_bytes(4, 'big') == b'\x00\x00\x00\x01'

    #check resluts from multiple tiles
    res_from_L1 = []

    for address in pack_addresses:
        res_from_L1.append(collect_results(format,src_A,address))
        

    res_from_L1 = flatten_list(res_from_L1)

    golden_tensor = torch.tensor(golden, dtype=format_dict[format] if format in ["Float16", "Float16_b"] else torch.bfloat16)
    res_tensor = torch.tensor(res_from_L1, dtype=format_dict[format] if format in ["Float16", "Float16_b"] else torch.bfloat16)

    if(format == "Float16_b" or format == "Float16"):
        atol = 0.05
        rtol = 0.1
    elif(format == "Bfp8_b"):
        atol = 0.1
        rtol = 0.2
  
    _ , pcc = comp_pcc(golden_tensor, res_tensor, pcc=0.99) 
    assert pcc > 0.99