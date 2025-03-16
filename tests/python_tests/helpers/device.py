# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0

from ttexalens.tt_exalens_lib import (
    write_to_device,
    read_words_from_device,
    read_word_from_device,
    run_elf,
)
from helpers import *
import inspect
import time
from helpers.param_config import *


def collect_results(
    formats: FormatConfig,
    address: int = 0x1C000,
    core_loc: str = "0,0",
    sfpu: bool = False,
):
    read_words_cnt = calculate_read_words_count(formats.pack_dst, sfpu)
    read_data = read_words_from_device(core_loc, address, word_count=read_words_cnt)
    read_data_bytes = flatten_list([int_to_bytes_list(data) for data in read_data])
    res_from_L1 = get_result_from_device(formats, read_data_bytes, sfpu)
    return res_from_L1


def run_elf_files(testname, core_loc="0,0", run_brisc=True):

    ELF_LOCATION = "../build/elf/"

    if run_brisc:
        run_elf(f"{ELF_LOCATION}brisc.elf", core_loc, risc_id=0)

    # for i in range(3):
    #     run_elf(f"{ELF_LOCATION}{testname}_trisc{i}.elf", core_loc, risc_id=i + 1)

    # Added because there was a race that caused failure in test_eltwise_unary_datacopy,
    # and now cores are run in revese order PACK, MATH, UNOPACK
    # Once that issue is reolved with tt-exalens code will be returned to normal for loop

    for i in reversed(range(3)):
        run_elf(f"{ELF_LOCATION}{testname}_trisc{i}.elf", core_loc, risc_id=i + 1)


def write_stimuli_to_l1(buffer_A, buffer_B, stimuli_A_format, stimuli_B_format, core_loc="0,0", tile_cnt=1):

    BUFFER_SIZE = 4096
    TILE_SIZE = 1024

    buffer_A_address = 0x1A000
    buffer_B_address = 0x1A000 + BUFFER_SIZE * tile_cnt

    for i in range(tile_cnt):

        start_index = TILE_SIZE * i
        end_index = start_index + TILE_SIZE

        # if end_index > len(buffer_A) or end_index > len(buffer_B):
        #     raise IndexError("Buffer access out of bounds")

        buffer_A_tile = buffer_A[start_index:end_index]
        buffer_B_tile = buffer_B[start_index:end_index]

        packers = {
            DataFormat.Bfp8_b: pack_bfp8_b,
            DataFormat.Float16: pack_fp16,
            DataFormat.Float16_b: pack_bfp16,
            DataFormat.Float32: pack_fp32,
            DataFormat.Int32: pack_int32,
        }

        pack_function_A = packers.get(stimuli_A_format)
        pack_function_B = packers.get(stimuli_B_format)

        write_to_device(core_loc, buffer_A_address, pack_function_A(buffer_A_tile))
        write_to_device(core_loc, buffer_B_address, pack_function_B(buffer_B_tile))

        buffer_A_address += BUFFER_SIZE
        buffer_B_address += BUFFER_SIZE


def get_result_from_device(
    formats: FormatConfig,
    read_data_bytes: bytes,
    core_loc: str = "0,0",
    sfpu: bool = False,
):
    # Dictionary of format to unpacking function mappings
    unpackers = {
        DataFormat.Float16: unpack_fp16,
        DataFormat.Float16_b: unpack_bfp16,
        DataFormat.Float32: unpack_float32,
        DataFormat.Int32: unpack_int32,
    }

    # Handling "Bfp8_b" format separately with sfpu condition
    if formats.pack_dst == DataFormat.Bfp8_b:
        unpack_func = unpack_bfp16 if sfpu else unpack_bfp8_b
    else:
        unpack_func = unpackers.get(formats.pack_dst)

    if unpack_func:
        num_args = len(inspect.signature(unpack_func).parameters)
        if num_args > 1:
            return unpack_func(
                read_data_bytes, formats.unpack_A_src, formats.pack_dst
            )  # Bug patchup in (unpack.py): in case unpack_src is Bfp8_b != pack_dst, L1 must be read in different order to extract correct results
        else:
            return unpack_func(read_data_bytes)
    else:
        raise ValueError(f"Unsupported format: {formats.pack_dst}")


def assert_value_with_timeout(core_loc, mailbox_addr, timeout=0, poll_interval=0.1):
    end_time = time.time() + timeout

    while time.time() < end_time:
        if read_word_from_device(core_loc, mailbox_addr) == 1:
            return True
        time.sleep(poll_interval)

    # If the loop finishes without breaking, that means the condition was never true
    assert (
        read_word_from_device(core_loc, mailbox_addr) == 1
    ), f"Value at mailbox {hex(mailbox_addr)} was not 1 after waiting {timeout} seconds."


def assert_tensix_operations_finished(core_loc: str = "0,0"):

    tensix_L1_mailboxes = [0x19FF4, 0x19FF8, 0x19FFC]  # L1 Mailbox addresses
    assert_value_with_timeout(core_loc, tensix_L1_mailboxes[0])
    assert_value_with_timeout(core_loc, tensix_L1_mailboxes[1])
    assert_value_with_timeout(core_loc, tensix_L1_mailboxes[2])
