# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
import random

import ttexalens
from ttexalens.tt_exalens_init import init_ttexalens
from ttexalens.tt_exalens_lib import (
    read_words_from_device,
    write_words_to_device,
)

from helpers.device import RiscCore, run_cores, run_elf_files
from helpers.test_config import ProfilerBuild, build_test


def generate_image(size, buckets, const=False):
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
    end_time = timestamps[3] << 32 | timestamps[2]
    return start_time, end_time


def print_timing(st, et, buffer_size):
    cores = len(st)
    cyc = []
    for i in range(cores):
        c = et[i] - st[i]
        cyc.append(c)
        print(
            "core",
            i,
            "cycles",
            c,
            "clk/px",
            c / buffer_size,
            "cyc/px/core",
            c / (buffer_size / cores),
        )
    if cores > 1:
        min_start = min(st)
        max_start = max(st)
        min_end = min(et)
        max_end = max(et)
        min_cyc = min(cyc)
        max_cyc = max(cyc)
        e2e_cyc = max_end - min_start
        print("cross core timing")
        print(
            "best  core",
            "cycles",
            min_cyc,
            "clk/px",
            min_cyc / buffer_size,
            "cyc/px/core",
            min_cyc / (buffer_size / cores),
        )
        print(
            "worst core",
            "cycles",
            max_cyc,
            "clk/px",
            max_cyc / buffer_size,
            "cyc/px/core",
            max_cyc / (buffer_size / cores),
        )
        print("start deviation", max_start - min_start)
        print("end   deviation", max_end - min_end)
        print(
            "end to end",
            "cycles",
            e2e_cyc,
            "clk/px",
            e2e_cyc / buffer_size,
            "cyc/px/core",
            e2e_cyc / (buffer_size / cores),
        )


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
    print(
        "e2e min",
        "cycles",
        min_e2e,
        "clk/px",
        min_e2e / buffer_size,
        "cyc/px/core",
        min_e2e / (buffer_size / cores),
    )
    print(
        "e2e max",
        "cycles",
        max_e2e,
        "clk/px",
        max_e2e / buffer_size,
        "cyc/px/core",
        max_e2e / (buffer_size / cores),
    )
    print(
        "e2e avg",
        "cycles",
        avg_e2e,
        "clk/px",
        avg_e2e / buffer_size,
        "cyc/px/core",
        avg_e2e / (buffer_size / cores),
    )
    print(
        "e2e dev",
        "cycles",
        dev_e2e,
        "clk/px",
        dev_e2e / buffer_size,
        "cyc/px/core",
        dev_e2e / (buffer_size / cores),
    )


def print_end_of_test(res, short=False):
    for result in res:
        sts = []
        ets = []
        t0s = []
        t1s = []
        for instance in result:
            version, cores, pipeline_factor, buffer_size, buckets, t0, t1, st, et = (
                instance
            )
            sts.append(st)
            ets.append(et)
            t0s.append(t0)
            t1s.append(t1)
            if not short:
                print(
                    "version",
                    version,
                    "pipeline_factor",
                    pipeline_factor,
                    "cores",
                    cores,
                    "buffer_size",
                    buffer_size,
                    "buckets",
                    buckets,
                    "t0",
                    t0,
                    "t1",
                    t1,
                )
                print_timing(st, et, buffer_size)
                print()
        version, cores, pipeline_factor, buffer_size, buckets, t0, t1, st, et = result[
            0
        ]
        print(
            "LLK version",
            version,
            "pipeline_factor",
            pipeline_factor,
            "cores",
            cores,
            "buffer_size",
            buffer_size,
            "buckets",
            buckets,
            "buffer_correct",
            all(t0s),
            "histogram_correct",
            all(t1s),
        )
        print("******************")
        print_summary(sts, ets, buffer_size)
        print("==================")


TIMESTAMP_ADDRESS = 0x19000
START_ADDRESS = 0x19FF0
BUFFER_ADDRESS = 0x20000
BUCKET_ADDRESS = 0x30000


def run_histogram_llk(version, cores, pipeline_factor, buffer_size, buckets):
    """
    Run histogram test using the LLK histogram interface.
    Only version 18 is supported by the LLK implementation.
    """
    print("Running LLK Histogram Test")
    print(ttexalens.__file__)
    context = init_ttexalens()

    # LLK histogram uses 32-bit buckets (version 18)
    bucket_bpp = 16

    # Validate that only version 18 is used with LLK
    if version != 18:
        raise ValueError(
            f"LLK histogram only supports version 18, got version {version}"
        )

    img = generate_image(buffer_size, buckets)
    res = golden(img, buckets)

    # Initialize device memory
    write_words_to_device("0,0", TIMESTAMP_ADDRESS, [0] * 4 * cores)
    write_words_to_device("0,0", START_ADDRESS, [0])
    write_words_to_device("0,0", BUFFER_ADDRESS, img)
    write_words_to_device("0,0", BUCKET_ADDRESS, [0] * ((buckets * bucket_bpp) // 4))

    # Build and run the LLK histogram test
    testname = "histogram_llk_test"  # This corresponds to histogram_llk_test.cpp
    def_dict = {
        "HISTOGRAM_BUFFER_SIZE": buffer_size,
        "HISTOGRAM_NUM_CORES": cores,
        "HISTOGRAM_PIPELINE_FACTOR": pipeline_factor,
        "HISTOGRAM_VERSION": version,
    }
    test_config = {"testname": testname, "def_dict": def_dict}
    build_test(test_config, ProfilerBuild.No)
    run_elf_files(testname)

    # Start all TRISC cores
    run_cores([RiscCore.TRISC0, RiscCore.TRISC1, RiscCore.TRISC2])

    # Trigger histogram computation
    write_words_to_device("0,0", START_ADDRESS, [1])

    # Read back results
    buffer = read_words_from_device("0,0", BUFFER_ADDRESS, word_count=buffer_size)
    bucket = read_words_from_device(
        "0,0", BUCKET_ADDRESS, word_count=(buckets * bucket_bpp) // 4
    )

    # Process bucket data (32-bit buckets for version 18)
    bucket = merge_words(bucket)

    print("Input preserved:", img == buffer)
    if img != buffer:
        print("Original input:")
        print(img[:20], "..." if len(img) > 20 else "")
        print("Device buffer:")
        print(buffer[:20], "..." if len(buffer) > 20 else "")

    print("Histogram correct:", res == bucket)
    if res != bucket:
        print("Expected histogram:")
        print(res[:20], "..." if len(res) > 20 else "")
        print("Device histogram:")
        print(bucket[:20], "..." if len(bucket) > 20 else "")
        print("Expected sum:", sum(res))
        print("Device sum:  ", sum(bucket))

        print("Differences:")
        diff_count = 0
        for i in range(len(res)):
            if res[i] != bucket[i]:
                print(f"bucket[{i}]: expected {res[i]}, got {bucket[i]}")
                diff_count += 1
                if diff_count >= 10:  # Limit output
                    print("... (more differences)")
                    break

    st, et = report_timing(TIMESTAMP_ADDRESS, buffer_size, cores)
    return (
        version,
        cores,
        pipeline_factor,
        buffer_size,
        buckets,
        img == buffer,
        res == bucket,
        st,
        et,
    )


def loop_histogram_llk(version, cores, pipeline_factor, buffer_size, buckets, loops):
    """Run multiple iterations of the LLK histogram test."""
    res = []
    for i in range(loops):
        print(f"LLK Test iteration {i+1}/{loops}")
        res.append(
            run_histogram_llk(version, cores, pipeline_factor, buffer_size, buckets)
        )
    return res


def sweep_histogram_llk(version, cores, pipeline_factor, buffer_size, buckets, loops):
    """Run comprehensive sweep of LLK histogram test parameters."""
    res = []
    for v in version:
        for c in cores:
            for p in pipeline_factor:
                for b in buffer_size:
                    for bck in buckets:
                        for l in loops:
                            print(
                                f"LLK Test: version={v}, cores={c}, pipeline={p}, buffer={b}, buckets={bck}, loops={l}"
                            )
                            res.append(loop_histogram_llk(v, c, p, b, bck, l))
    return res


def test_histogram_llk():
    """
    Main test function for LLK histogram.
    Tests version 18 (the only version supported by LLK) with various configurations.
    """
    print("=== LLK Histogram Test Suite ===")
    res = []

    # Test single core configurations
    print("\n--- Single Core Tests ---")
    res += sweep_histogram_llk([18], [1], [1, 2, 4], [3072], [256], [3])

    # Test multi-core configurations
    print("\n--- Multi-Core Tests ---")
    res += sweep_histogram_llk([18], [2, 4], [1, 2, 4], [4096], [256], [2])

    # Performance test with larger buffer
    print("\n--- Performance Tests ---")
    res += sweep_histogram_llk([18], [1, 4], [4], [8192], [256], [5])

    print("\n=== LLK Histogram Test Results ===")
    print_end_of_test(res, True)


def test_histogram_llk_quick():
    """Quick test for development and CI."""
    print("=== Quick LLK Histogram Test ===")
    res = []

    # Just test basic functionality
    res += sweep_histogram_llk([18], [1], [1, 4], [1024], [256], [1])

    print("\n=== Quick Test Results ===")
    print_end_of_test(res, False)


if __name__ == "__main__":
    # Run quick test by default
    test_histogram_llk_quick()

    # Uncomment for full test suite
    # test_histogram_llk()
