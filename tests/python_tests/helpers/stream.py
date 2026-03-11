# SPDX-FileCopyrightText: © 2026 Tenstorrent AI ULC
#
# SPDX-License-Identifier: Apache-2.0

import time

from ttexalens.tt_exalens_lib import (
    read_from_device,
    read_word_from_device,
    write_to_device,
    write_words_to_device,
)


class Stream:
    """
    Memory layout at base_addr:
        [0:4]   write_idx  (std::uint32_t, producer-owned)
        [4:8]   read_idx   (std::uint32_t, consumer-owned)
        [8:8+N] buffer[N]  (char array, circular)

    A single stream_t is unidirectional.  For bidirectional host↔device
    traffic, use two Stream instances at different L1 addresses.
    """

    _WRITE_IDX_OFFSET = 0
    _READ_IDX_OFFSET = 4
    _BUFFER_OFFSET = 8

    def __init__(self, base_addr: int, buffer_size: int, location: str = "0,0"):
        self._base_addr = base_addr
        self._buffer_size = buffer_size
        self._location = location
        self._write_idx = read_word_from_device(
            location, base_addr + self._WRITE_IDX_OFFSET
        )
        self._read_idx = read_word_from_device(
            location, base_addr + self._READ_IDX_OFFSET
        )

    @property
    def _capacity(self) -> int:
        return self._buffer_size - 1

    def _poll_read_idx(self) -> int:
        self._read_idx = read_word_from_device(
            self._location, self._base_addr + self._READ_IDX_OFFSET
        )
        return self._read_idx

    def _poll_write_idx(self) -> int:
        self._write_idx = read_word_from_device(
            self._location, self._base_addr + self._WRITE_IDX_OFFSET
        )
        return self._write_idx

    def _commit_read_idx(self) -> None:
        write_words_to_device(
            location=self._location,
            addr=self._base_addr + self._READ_IDX_OFFSET,
            data=[self._read_idx],
        )

    def _commit_write_idx(self) -> None:
        write_words_to_device(
            location=self._location,
            addr=self._base_addr + self._WRITE_IDX_OFFSET,
            data=[self._write_idx],
        )

    def _wait_free(self, timeout: float = 0.1) -> int:

        def poll_free() -> int:
            self._poll_read_idx()
            return (
                self._capacity - self._write_idx + self._read_idx
            ) % self._buffer_size

        deadline = time.monotonic() + timeout
        while True:
            free = poll_free()
            if free > 0:
                return free

            if time.monotonic() > deadline:
                raise TimeoutError(
                    f"Stream: timed out after {timeout}s waiting for free space"
                )
            time.sleep(0.001)

    def _wait_avail(self, timeout: float = 0.1) -> int:

        def poll_avail() -> int:
            self._poll_write_idx()
            return (
                self._write_idx - self._read_idx + self._buffer_size
            ) % self._buffer_size

        deadline = time.monotonic() + timeout
        while True:
            avail = poll_avail()
            if avail > 0:
                return avail

            if time.monotonic() > deadline:
                raise TimeoutError(
                    f"Stream: timed out after {timeout}s waiting for data"
                )
            time.sleep(0.001)

    # ── public API ────────────────────────────────────────────────────

    def push(self, data: bytes, timeout: float = 2.0) -> None:
        """Push bytes into the stream, blocking until space is available."""
        remaining = len(data)
        offset = 0

        while remaining > 0:
            free = self._wait_free(timeout)
            chunk = min(remaining, free)

            # Handle wrap-around (mirrors C++ push logic)
            head = min(chunk, self._buffer_size - self._write_idx)
            if head > 0:
                write_to_device(
                    self._location,
                    self._base_addr + self._BUFFER_OFFSET + self._write_idx,
                    data[offset : offset + head],
                )

            tail = chunk - head
            if tail > 0:
                write_to_device(
                    self._location,
                    self._base_addr + self._BUFFER_OFFSET,
                    data[offset + head : offset + head + tail],
                )

            self._write_idx = (self._write_idx + chunk) % self._buffer_size
            self._commit_write_idx()

            offset += chunk
            remaining -= chunk

    def peek(self, timeout: float = 2.0) -> int:
        """Return the next byte without consuming it."""
        self._wait_avail(timeout)
        data = read_from_device(
            self._location,
            self._base_addr + self._BUFFER_OFFSET + self._read_idx,
            num_bytes=1,
        )
        return data[0]

    def pop(self, size: int, timeout: float = 2.0) -> bytes:
        """Pop *size* bytes from the stream, blocking until available."""
        result = bytearray()
        remaining = size

        while remaining > 0:
            avail = self._wait_avail(timeout)
            chunk = min(remaining, avail)

            # Handle wrap-around (mirrors C++ pop logic)
            head = min(chunk, self._buffer_size - self._read_idx)
            if head > 0:
                result.extend(
                    read_from_device(
                        self._location,
                        self._base_addr + self._BUFFER_OFFSET + self._read_idx,
                        num_bytes=head,
                    )
                )

            tail = chunk - head
            if tail > 0:
                result.extend(
                    read_from_device(
                        self._location,
                        self._base_addr + self._BUFFER_OFFSET,
                        num_bytes=tail,
                    )
                )

            self._read_idx = (self._read_idx + chunk) % self._buffer_size
            self._commit_read_idx()

            remaining -= chunk

        return bytes(result)
