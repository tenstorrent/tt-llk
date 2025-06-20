# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0

import time

from ttexalens.tt_exalens_lib import (
    check_context,
    load_elf,
    read_from_device,
    read_word_from_device,
    run_elf,
    write_to_device,
    write_words_to_device,
)

from .format_arg_mapping import L1_buffer_locations, Mailbox, format_tile_sizes
from .format_config import DataFormat, FormatConfig
from .pack import (
    pack_bfp8_b,
    pack_bfp16,
    pack_fp16,
    pack_fp32,
    pack_int8,
    pack_int32,
    pack_uint8,
    pack_uint16,
    pack_uint32,
)
from .unpack import (
    unpack_res_tiles,
)

MAX_READ_BYTE_SIZE_16BIT = 2048


def collect_results(
    formats: FormatConfig,
    tile_cnt: int,
    address: int = 0x1C000,
    core_loc: str = "0,0",
    sfpu: bool = False,
):

    read_bytes_cnt = format_tile_sizes[formats.output_format] * tile_cnt
    read_data = read_from_device(core_loc, address, num_bytes=read_bytes_cnt)
    res_from_L1 = unpack_res_tiles(read_data, formats, tile_cnt=tile_cnt, sfpu=sfpu)
    return res_from_L1


def run_elf_files(testname, core_loc="0,0", run_brisc=True):
    ELF_LOCATION = "../build/elf/"

    if run_brisc:
        run_elf(f"{ELF_LOCATION}brisc.elf", core_loc, risc_id=0)

    context = check_context()
    device = context.devices[0]
    RISC_DBG_SOFT_RESET0 = device.get_tensix_register_address(
        "RISCV_DEBUG_REG_SOFT_RESET_0"
    )

    # Perform soft reset
    soft_reset = read_word_from_device(core_loc, RISC_DBG_SOFT_RESET0)
    soft_reset |= 0x7800
    write_words_to_device(core_loc, RISC_DBG_SOFT_RESET0, soft_reset)

    # Load ELF files
    for i in range(3):
        load_elf(f"{ELF_LOCATION}{testname}_trisc{i}.elf", core_loc, risc_id=i + 1)

    # Reset the profiler barrier
    TRISC_PROFILER_BARRIER = 0x16AFF4
    write_words_to_device(core_loc, TRISC_PROFILER_BARRIER, [0, 0, 0])

    # Clear soft reset
    soft_reset &= ~0x7800
    write_words_to_device(core_loc, RISC_DBG_SOFT_RESET0, soft_reset)


def write_stimuli_to_l1(
    buffer_A,
    buffer_B,
    stimuli_A_format,
    stimuli_B_format,
    core_loc="0,0",
    tile_cnt=1,
):

    TILE_ELEMENTS = 1024

    TILE_SIZE_A = format_tile_sizes.get(stimuli_A_format, 2048)
    TILE_SIZE_B = format_tile_sizes.get(stimuli_A_format, 2048)

    # beginning addresses of srcA, srcB and result buffers in L1
    buffer_A_address = 0x1A000
    buffer_B_address = 0x1A000 + TILE_SIZE_A * tile_cnt
    res_buffer_address = buffer_B_address + TILE_SIZE_B * tile_cnt

    write_to_device(
        core_loc, L1_buffer_locations.srcA.value, buffer_A_address.to_bytes(4, "little")
    )
    write_to_device(
        core_loc, L1_buffer_locations.srcB.value, buffer_B_address.to_bytes(4, "little")
    )
    write_to_device(
        core_loc,
        L1_buffer_locations.Res.value,
        res_buffer_address.to_bytes(4, "little"),
    )

    for i in range(tile_cnt):

        start_index = TILE_ELEMENTS * i
        end_index = start_index + TILE_ELEMENTS

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
            DataFormat.UInt32: pack_uint32,
            DataFormat.UInt16: pack_uint16,
            DataFormat.Int8: pack_int8,
            DataFormat.UInt8: pack_uint8,
        }

        pack_function_A = packers.get(stimuli_A_format)
        pack_function_B = packers.get(stimuli_B_format)

        write_to_device(core_loc, buffer_A_address, pack_function_A(buffer_A_tile))
        write_to_device(core_loc, buffer_B_address, pack_function_B(buffer_B_tile))

        buffer_A_address += TILE_SIZE_A
        buffer_B_address += TILE_SIZE_B

    return res_buffer_address  # return address where result will be stored


def wait_until_tensix_complete(core_loc, mailbox_addr, timeout=30, max_backoff=5):
    """
    Polls a value from the device with an exponential backoff timer and fails if it doesn't read 1 within the timeout.

    Args:
        core_loc: The location of the core to poll.
        mailbox_addr: The mailbox address to read from.
        timeout: Maximum time to wait (in seconds) before timing out. Default is 30 seconds.
        max_backoff: Maximum backoff time (in seconds) between polls. Default is 5 seconds.
    """
    start_time = time.time()
    backoff = 0.1  # Initial backoff time in seconds

    while time.time() - start_time < timeout:
        if read_word_from_device(core_loc, mailbox_addr.value) == 1:
            return

        time.sleep(backoff)
        backoff = min(backoff * 2, max_backoff)  # Exponential backoff with a cap

    assert False, f"Timeout reached: waited {timeout} seconds for {mailbox_addr.name}"


def wait_for_tensix_operations_finished(core_loc: str = "0,0"):

    wait_until_tensix_complete(core_loc, Mailbox.Packer)
    wait_until_tensix_complete(core_loc, Mailbox.Math)
    wait_until_tensix_complete(core_loc, Mailbox.Unpacker)
