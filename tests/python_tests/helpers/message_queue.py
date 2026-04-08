# SPDX-FileCopyrightText: © 2026 Tenstorrent AI ULC
#
# SPDX-License-Identifier: Apache-2.0

import struct
from dataclasses import dataclass
from enum import IntEnum
from typing import Optional

from .stream import Stream


class MessageType(IntEnum):
    EXEC_REQUEST = 0
    EXEC_DONE = 1
    MEMCPY_REQUEST = 2
    MEMCPY_DONE = 3


class MessageStreamId(IntEnum):
    # TRISC -> RUNTIME streams [0,1,2]
    TRISC_RUNTIME = 0
    # RUNTIME -> TRISC streams [3,4,5]
    RUNTIME_TRISC = 3
    # BRISC -> HOST stream [6]
    BRISC_HOST = 6
    # HOST -> BRISC stream [7]
    HOST_BRISC = 7
    # TRISC -> HOST streams [8,9,10]
    TRISC_HOST = 8
    # HOST -> TRISC streams [11,12,13]
    HOST_TRISC = 11


@dataclass
class MemcpyRequestData:
    """
    Mirrors the device-side MemcpyRequestData struct.

    On 32-bit RISC-V:
        destination: void*  (4 bytes)
        source:      void*  (4 bytes)
        length:      size_t (4 bytes)
    """

    destination: int
    source: int
    length: int

    SIZE = 12

    def to_bytes(self) -> bytes:
        return struct.pack("<III", self.destination, self.source, self.length)

    @classmethod
    def from_bytes(cls, data: bytes) -> "MemcpyRequestData":
        destination, source, length = struct.unpack("<III", data)
        return cls(destination=destination, source=source, length=length)


_QUEUE_COUNT = 14
_MESSAGE_STREAM_BUFFER_SIZE = 24


class MessageQueue:
    """
    Python-side mirror of the device-side llk::MessageQueue.

    Memory layout: 14 consecutive Stream<24> objects.
    Stream IDs are used as direct indices — the host applies no TRISC offset.
    TRISC cores T0/T1/T2 offset by their core index (0/1/2) on the device side.
    """

    def __init__(self, address: int, location: str = "0,0"):
        """Attach to an existing MessageQueue at *address* on the device.

        Args:
            address: L1 device address of the MessageQueue.
            location: Tensix core coordinate.
        """
        self._address = address
        self._location = location

        first = Stream(address, _MESSAGE_STREAM_BUFFER_SIZE, location)
        stream_size = first.sizeof
        self._streams = [first] + [
            Stream(address + i * stream_size, _MESSAGE_STREAM_BUFFER_SIZE, location)
            for i in range(1, _QUEUE_COUNT)
        ]

    def _get_stream(self, id: MessageStreamId) -> Stream:
        return self._streams[int(id)]

    def init(self) -> None:
        """Initialize all streams. Should be called once before any operations."""
        for stream in self._streams:
            stream.init()

    def send(self, id: MessageStreamId, type: MessageType, data: bytes = b"") -> None:
        """Send a message to the given stream, blocking until space is available.

        Args:
            id: Target stream.
            type: Message type byte.
            data: Optional payload (trivially copyable bytes).
        """
        stream = self._get_stream(id)
        stream.push(bytes([int(type)]))
        if data:
            stream.push(data)

    def receive(self, id: MessageStreamId, expected_type: MessageType, size: int = 0) -> bytes:
        """Receive a message from the given stream, blocking until available.

        Args:
            id: Source stream.
            expected_type: The message type to assert against.
            size: Number of payload bytes to pop after the type byte (0 if no payload).

        Returns:
            Payload bytes (empty if size == 0).

        Raises:
            ValueError: If the received type does not match expected_type.
        """
        stream = self._get_stream(id)
        type_byte = stream.pop(1)
        actual_type = type_byte[0]
        if actual_type != int(expected_type):
            raise ValueError(
                f"MessageQueue::receive: unexpected message type "
                f"(expected {expected_type.name}={int(expected_type)}, got {actual_type})"
            )
        if size > 0:
            return stream.pop(size)
        return b""

    def peek(self, id: MessageStreamId) -> Optional[MessageType]:
        """Peek at the next message type without consuming it.

        Returns:
            The next MessageType if data is available, otherwise None.
        """
        stream = self._get_stream(id)
        byte = stream.peek()
        if byte is None:
            return None
        return MessageType(byte)
