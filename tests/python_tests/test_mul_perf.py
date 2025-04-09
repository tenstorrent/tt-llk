import pytest
import torch
import random
from ttexalens.tt_exalens_lib import (
    write_to_device,
    write_words_to_device,
    read_words_from_device,
    read_word_from_device,
    load_elf,
    run_elf,
    check_context,
)
from helpers import generate_make_command, run_shell_command, run_elf_files

def get_cycles(address):
    timestamps = read_words_from_device("0,0", address, word_count=4)
    start_time = timestamps[1] << 32 | timestamps[0]
    end_time   = timestamps[3] << 32 | timestamps[2]
    return start_time, end_time

def print_timing(st, et, buffer_size):
    cores = len(st)
    cyc = []
    for i in range(cores):
        c = et[i] - st[i]
        cyc.append(c) 
        print("core", i, "cycles", c)

def report_timing(address, buffer_size, cores):
    st = []
    et = []
    for i in range(cores):
        cyc = get_cycles(address + i * 16)
        st.append(cyc[0])
        et.append(cyc[1])
    print_timing(st, et, buffer_size)
    return st, et

TIMESTAMP_ADDRESS = 0x19000
START_ADDRESS     = 0x19FF0
BUFFER_ADDRESS    = 0x20000
BUCKET_ADDRESS    = 0x30000

@pytest.mark.parametrize("version", [0])
def test_histogram(version):
    write_words_to_device("0,0", TIMESTAMP_ADDRESS, [0] * 4 * 4)

    testname = "mul_perf_test"

    test_config = { 
        "testname": testname,
    }
    make_cmd = generate_make_command(test_config)
    run_shell_command(f"cd .. && {make_cmd}")
    run_elf_files(testname)

    st, et = report_timing(TIMESTAMP_ADDRESS, 32, 4)
    assert 1==2
