from ttlens.tt_lens_init import init_ttlens
from ttlens.tt_lens_lib import write_to_device, read_words_from_device, run_elf
from helpers import *


def collect_results(format,address=0x1c000,core_loc = "0,0", sfpu=False):
    read_words_cnt = calculate_read_words_count(format,sfpu)
    read_data = read_words_from_device(core_loc, address, word_count=read_words_cnt)
    read_data_bytes = flatten_list([int_to_bytes_list(data) for data in read_data])
    res_from_L1 = get_result_from_device(format,read_data_bytes,sfpu)
    return res_from_L1

def run_elf_files(testname, core_loc = "0,0", run_brisc=True):
    
    ELF_LOCATION = "../build/elf/"

    if run_brisc:
        run_elf(f"{ELF_LOCATION}brisc.elf", core_loc, risc_id=0)

    # for i in range(3):
    #     run_elf(f"{ELF_LOCATION}{testname}_trisc{i}.elf", core_loc, risc_id=i + 1)

    # Added because there was a race that caused failure in test_eltwise_unary_datacopy,
    # and now cores are run in revese order PACK, MATH, UNOPACK
    # Once that issue is reolved with tt-exalens code will be returned to normal for loop

    run_elf(f"{ELF_LOCATION}{testname}_trisc2.elf", core_loc, risc_id=3)
    run_elf(f"{ELF_LOCATION}{testname}_trisc1.elf", core_loc, risc_id=2)
    run_elf(f"{ELF_LOCATION}{testname}_trisc0.elf", core_loc, risc_id=1)

def write_stimuli_to_l1(buffer_A, buffer_B, stimuli_format, core_loc = "0,0", tile_cnt = 1):

    BUFFER_SIZE = 4096
    TILE_SIZE = 1024

    buffer_A_address = 0x1a000
    buffer_B_address = 0x1a000 + BUFFER_SIZE*tile_cnt

    for i in range(tile_cnt):

        start_index = TILE_SIZE * i
        end_index = start_index + TILE_SIZE

        # if end_index > len(buffer_A) or end_index > len(buffer_B):
        #     raise IndexError("Buffer access out of bounds")

        buffer_A_tile = buffer_A[start_index:end_index]
        buffer_B_tile = buffer_B[start_index:end_index]

        packers = {
            "Bfp8_b": pack_bfp8_b,
            "Float16": pack_fp16,
            "Float16_b": pack_bfp16,
            "Float32": pack_fp32,
            "Int32": pack_int32
        }

        pack_function = packers.get(stimuli_format)

        write_to_device(core_loc, buffer_A_address, pack_function(buffer_A_tile))
        write_to_device(core_loc, buffer_B_address, pack_function(buffer_B_tile))
        
        buffer_A_address += BUFFER_SIZE
        buffer_B_address += BUFFER_SIZE
        
        
def get_result_from_device(format: str, read_data_bytes: bytes, core_loc : str = "0,0", sfpu: bool =False):
    # Dictionary of format to unpacking function mappings
    unpackers = {
        "Float16": unpack_fp16,
        "Float16_b": unpack_bfp16,
        "Float32": unpack_float32,
        "Int32": unpack_int32
    }
    
    # Handling "Bfp8_b" format separately with sfpu condition
    if format == "Bfp8_b":
        unpack_func = unpack_bfp16 if sfpu else unpack_bfp8_b
    else:
        unpack_func = unpackers.get(format)
    
    if unpack_func:
        return unpack_func(read_data_bytes)
    else:
        raise ValueError(f"Unsupported format: {format}")
    
def read_mailboxes(core_loc : str= "0,0"):
    mailbox_addresses = [0x19FF4, 0x19FF8, 0x19FFC] # L1 Mailbox addresses
    mailbox_values = [read_words_from_device(core_loc, address, word_count=1)[0] for address in mailbox_addresses]
    return all(value == 1 for value in mailbox_values)