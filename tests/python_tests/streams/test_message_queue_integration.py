# SPDX-FileCopyrightText: © 2026 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0

import pytest
from helpers.device import wait_for_tensix_operations_finished
from helpers.format_config import DataFormat
from helpers.message_queue import MemcpyRequestData, MessageQueue, MessageStreamId, MessageType
from helpers.param_config import input_output_formats
from helpers.test_config import TestConfig, TestMode
from ttexalens.tt_exalens_lib import read_word_from_device


MQUEUE_SOURCE = "sources/streams/message_queue_integration_test.cpp"
MQUEUE_ADDRESS = 0x70000


def test_message_queue_handshake(workers_tensix_coordinates):
    worker = workers_tensix_coordinates

    formats = input_output_formats([DataFormat.Float16_b])[0]

    configuration = TestConfig(MQUEUE_SOURCE, formats)
    configuration.generate_variant_hash()

    if TestConfig.MODE in [TestMode.PRODUCE, TestMode.DEFAULT]:
        configuration.build_elfs()
    if TestConfig.MODE == TestMode.PRODUCE:
        pytest.skip(TestConfig.SKIP_JUST_FOR_COMPILE_MARKER)

    mqueue = MessageQueue(MQUEUE_ADDRESS, worker)
    mqueue.init()

    configuration.write_runtimes_to_L1(worker)
    elfs = configuration.run_elf_files(worker)

    # --- step 1 & 2: EXEC handshake (no payload) ---

    # peek() may return None if device hasn't sent yet; receive() blocks until available
    peek_result = mqueue.peek(MessageStreamId.TRISC_HOST)
    assert peek_result is None or peek_result == MessageType.EXEC_REQUEST

    mqueue.receive(MessageStreamId.TRISC_HOST, MessageType.EXEC_REQUEST)

    # stream is empty after consuming
    assert mqueue.peek(MessageStreamId.TRISC_HOST) is None

    mqueue.send(MessageStreamId.HOST_TRISC, MessageType.EXEC_DONE)

    # --- step 3 & 4: MEMCPY handshake (with payload) ---

    data = mqueue.receive(MessageStreamId.TRISC_HOST, MessageType.MEMCPY_REQUEST, size=MemcpyRequestData.SIZE)
    req = MemcpyRequestData.from_bytes(data)

    assert req.destination == 0x10000
    assert req.source == 0x20000
    assert req.length == 64

    mqueue.send(MessageStreamId.HOST_TRISC, MessageType.MEMCPY_DONE)

    wait_for_tensix_operations_finished(elfs, worker)

    # Both streams involved should be empty (read_idx == write_idx)
    stream_size = mqueue._streams[0].sizeof
    for stream_id in [MessageStreamId.TRISC_HOST, MessageStreamId.HOST_TRISC]:
        stream_addr = MQUEUE_ADDRESS + int(stream_id) * stream_size
        write_idx = read_word_from_device(worker, stream_addr + 0)
        read_idx = read_word_from_device(worker, stream_addr + 4)
        assert read_idx == write_idx, f"Stream {stream_id.name} is not empty after test"
