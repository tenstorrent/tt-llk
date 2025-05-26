# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
#
# SPDX-License-Identifier: Apache-2.0
from dataclasses import dataclass

from ttexalens.tt_exalens_lib import read_words_from_device


@dataclass()
class ProfilerTimestamp:
    full_marker: any
    timestamp: int
    data: int


@dataclass()
class ProfilerZoneScoped:
    full_marker: str
    start: int
    end: int
    duration: int


class ProfilerData:

    BUFFER_SIZE = 0x400
    THREAD_BUFFER = {
        "UNPACK": 0x16B000,
        "MATH": 0x16C000,
        "PACK": 0x16D000,
    }

    ENTRY_TYPE_SHAMT = 28
    ENTRY_ID_SHAMT = ENTRY_TYPE_SHAMT - 16

    ENTRY_TYPE_MASK = 0xF << ENTRY_TYPE_SHAMT
    ENTRY_ID_MASK = 0xFFFF << ENTRY_ID_SHAMT
    ENTRY_TIME_HIGH_MASK = 0xFFF

    ENTRY_EXISTS_BIT = 0b1000 << ENTRY_TYPE_SHAMT

    TIMESTAMP_ENTRY = 0b1000
    TIMESTAMP_DATA_ENTRY = 0b1001
    ZONE_START_ENTRY = 0b1010
    ZONE_END_ENTRY = 0b1011

    def get_data(self):
        return self._parse_buffers(self._load_buffers())

    def _load_buffers(self, core_loc="0,0", num_words=BUFFER_SIZE):
        return {
            thread: read_words_from_device(
                core_loc, ProfilerData.THREAD_BUFFER[thread], word_count=num_words
            )
            for thread in ProfilerData.THREAD_BUFFER.keys()
        }

    def _parse_buffers(self, buffers):
        return {thread: self._parse_thread(words) for thread, words in buffers.items()}

    def _parse_thread(self, words):
        thread = []
        zone_stack = []
        word_stream = iter(words)
        for word in word_stream:
            if not (word & self.ENTRY_EXISTS_BIT):
                break

            type = (word & self.ENTRY_TYPE_MASK) >> self.ENTRY_TYPE_SHAMT

            marker_id = (word & self.ENTRY_ID_MASK) >> self.ENTRY_ID_SHAMT

            timestamp_high = word & self.ENTRY_TIME_HIGH_MASK
            timestamp_low = next(word_stream)
            timestamp = (timestamp_high << 32) | timestamp_low

            match type:
                case self.TIMESTAMP_ENTRY:
                    thread.append(ProfilerTimestamp(marker_id, timestamp))

                case self.TIMESTAMP_DATA_ENTRY:
                    data_high = next(word_stream)
                    data_low = next(word_stream)
                    data = (data_high << 32) | data_low
                    thread.append(ProfilerTimestamp(marker_id, timestamp, data))

                case self.ZONE_START_ENTRY:
                    zone_stack.append(timestamp)

                case self.ZONE_END_ENTRY:
                    end = timestamp
                    start = zone_stack.pop()
                    duration = end - start
                    thread.append(ProfilerZoneScoped(marker_id, start, end, duration))

        return thread
