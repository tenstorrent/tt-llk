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

def generate_image(size, buckets, const = False):
    image = []
    for i in range(size):
        if const:
            image.append(15)
        else:
            image.append(random.randint(0, buckets - 1))
    return image

def golden(img, buckets):
    res = [0] * buckets
    for i in img:
        res[i] = res[i] + 1
    return res

def split_words(words):
    res = []
    for word in words:
        res.append(word & 0xFFFF)
        res.append(word >> 16)
    return res

def merge_words(words):
    res = []
    for i in range(len(words) // 4):
        res.append(words[i * 4 + 1])
    return res

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
        print("core", i, "cycles", c, "clk/px", c / buffer_size, "cyc/px/core", c / (buffer_size / cores))
    if cores > 1:
        min_start = min(st)
        max_start = max(st)
        min_end   = min(et)
        max_end   = max(et)
        min_cyc   = min(cyc)
        max_cyc   = max(cyc)
        e2e_cyc   = max_end - min_start 
        print("cross core timing")
        print("best  core", "cycles", min_cyc, "clk/px", min_cyc / buffer_size, "cyc/px/core", min_cyc / (buffer_size / cores))
        print("worst core", "cycles", max_cyc, "clk/px", max_cyc / buffer_size, "cyc/px/core", max_cyc / (buffer_size / cores))
        print("start deviation", max_start - min_start)
        print("end   deviation", max_end   - min_end)
        print("end to end", "cycles", e2e_cyc, "clk/px", e2e_cyc / buffer_size, "cyc/px/core", e2e_cyc / (buffer_size / cores))

def report_timing(address, buffer_size, cores):
    st = []
    et = []
    for i in range(cores):
        cyc = get_cycles(address + i * 16)
        st.append(cyc[0])
        et.append(cyc[1])
    print_timing(st, et, buffer_size)
    return st, et

def print_summary(sts, ets, buffer_size):
    e2es = []
    tests = len(sts)
    for i in range(tests):
        st = sts[i]
        et = ets[i]
        e2es.append(max(et) - min(st))
    min_e2e = min(e2es)
    max_e2e = max(e2es)
    avg_e2e = sum(e2es) / tests
    dev_e2e = max_e2e - min_e2e
    cores = len(st)
    print("e2e min", "cycles", min_e2e, "clk/px", min_e2e / buffer_size, "cyc/px/core", min_e2e / (buffer_size / cores))
    print("e2e max", "cycles", max_e2e, "clk/px", max_e2e / buffer_size, "cyc/px/core", max_e2e / (buffer_size / cores))
    print("e2e avg", "cycles", avg_e2e, "clk/px", avg_e2e / buffer_size, "cyc/px/core", avg_e2e / (buffer_size / cores))
    print("e2e dev", "cycles", dev_e2e, "clk/px", dev_e2e / buffer_size, "cyc/px/core", dev_e2e / (buffer_size / cores))

def print_end_of_test(res, short = False):
    for result in res:
        sts = []
        ets = []
        t0s = []
        t1s = []
        for instance in result:
            version, cores, pipeline_factor, buffer_size, buckets, t0, t1, st, et = instance
            sts.append(st)
            ets.append(et)
            t0s.append(t0)
            t1s.append(t1)
            if not short:
                print("version", version, "pipeline_factor", pipeline_factor, "cores", cores, "buffer_size", buffer_size, "buckets", buckets, "t0", t0, "t1", t1)
                print_timing(st, et, buffer_size)
                print()
        version, cores, pipeline_factor, buffer_size, buckets, t0, t1, st, et = result[0]
        print("version", version, "pipeline_factor", pipeline_factor, "cores", cores, "buffer_size", buffer_size, "buckets", buckets, "t0", all(t0s), "t1", all(t1s))
        print("******************")
        print_summary(sts, ets, buffer_size)
        print("==================")

TIMESTAMP_ADDRESS = 0x19000
START_ADDRESS     = 0x19FF0
BUFFER_ADDRESS    = 0x20000
BUCKET_ADDRESS    = 0x30000

@pytest.mark.parametrize("version", [0])
def test_histogram(version):
    write_words_to_device("0,0", TIMESTAMP_ADDRESS, [0] * 4 * 4)

    testname = "histogram_test"

    test_config = { 
        "testname": testname,
    }
    make_cmd = generate_make_command(test_config)
    run_shell_command(f"cd .. && {make_cmd}")
    run_elf_files(testname)

    st, et = report_timing(TIMESTAMP_ADDRESS, 32, 4)
    print(st, et)

    assert 1==2
