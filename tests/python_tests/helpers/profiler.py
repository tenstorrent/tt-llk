# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
#
# SPDX-License-Identifier: Apache-2.0

from ttexalens.tt_exalens_lib import read_words_from_device


class ProfilerTimestamp:
    def __init__(self, id, timestamp, data=None):
        self.id = id
        self.timestamp = timestamp
        self.data = data

    def __str__(self):
        return f"ProfilerTimestamp(id={self.id}, timestamp={self.timestamp}, duration={self.duration})"

    def __repr__(self):
        return self.__str__()


class ProfilerZoneScoped:
    def __init__(self, id, timestamp, duration):
        self.id = id
        self.timestamp = timestamp
        self.duration = duration

    def __str__(self):
        return f"ProfilerZoneScoped(id={self.id}, timestamp={self.timestamp}, duration={self.duration})"

    def __repr__(self):
        return self.__str__()


class ProfilerData:

    BUFFER_SIZE = 0x400
    BUFFER = {
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
        raw = self.load_raw()
        data = self.parse_raw(raw)
        return data

    def load_raw(self, core_loc="0,0", num_bytes=BUFFER_SIZE):
        raw = {}
        for thread in ["UNPACK", "MATH", "PACK"]:
            raw[thread] = self.load_buffer(thread, core_loc, num_bytes)
        return raw

    def load_buffer(self, thread, core_loc="0,0", num_words=BUFFER_SIZE):
        return read_words_from_device(
            core_loc, self.BUFFER[thread], word_count=num_words
        )

    def parse_raw(self, raw):
        data = {"UNPACK": [], "MATH": [], "PACK": []}
        for thread in data:
            raw_thread = raw[thread]
            data[thread] = self.parse_thread(raw_thread)
        return data

    def parse_thread(self, raw_thread):
        data_thread = []
        zone_stack = []
        raw_iter = iter(raw_thread)
        for word in raw_iter:
            if not (word & self.ENTRY_EXISTS_BIT):
                break
            entry = self.parse_entry(zone_stack, word, raw_iter)
            if entry is None:
                continue
            data_thread.append(entry)
        return data_thread

    def parse_entry(self, zone_stack, word, raw_iter):
        entry_type = (word & self.ENTRY_TYPE_MASK) >> self.ENTRY_TYPE_SHAMT
        entry_id = (word & self.ENTRY_ID_MASK) >> self.ENTRY_ID_SHAMT

        entry_time_high = word & self.ENTRY_TIME_HIGH_MASK
        entry_time_low = next(raw_iter)
        entry_time = (entry_time_high << 32) | entry_time_low

        match entry_type:
            case self.TIMESTAMP_ENTRY:
                return ProfilerTimestamp(entry_id, entry_time)
            case self.ZONE_START_ENTRY:
                zone_stack.append(entry_time)
                return None
            case self.ZONE_END_ENTRY:
                assert len(zone_stack) > 0
                timestamp = zone_stack.pop()
                duration = entry_time - timestamp
                return ProfilerZoneScoped(entry_id, timestamp, duration)
            case self.TIMESTAMP_DATA_ENTRY:
                entry_data_high = next(raw_iter)
                entry_data_low = next(raw_iter)
                entry_data = (entry_data_high << 32) | entry_data_low
                return ProfilerTimestamp(entry_id, entry_time, entry_data)
