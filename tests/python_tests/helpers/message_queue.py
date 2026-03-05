# SPDX-FileCopyrightText: © 2026 Tenstorrent AI ULC
#
# SPDX-License-Identifier: Apache-2.0

import time
from enum import IntEnum

from ttexalens.tt_exalens_lib import (
    read_from_device,
    write_to_device,
)


class MessageQueue:
    """
    Host-side counterpart to the device-side llk::message_queue.

    Memory layout of queue_store_t at STORAGE_BASE:
        host_push[5]: 5 × 32 bytes  (host writes, device reads)
        host_poll[5]: 5 × 32 bytes  (host reads,  device writes)

    Each entry struct is 32 bytes:
        [0]    req  — write pointer (owned by the queue's producer)
        [1]    ack  — read pointer  (owned by the queue's consumer)
        [2:32] queue data (QUEUE_DEPTH = 30 slots)
    """

    STORAGE_BASE = 0xACAFACA
    QUEUE_DEPTH = 30
    ENTRY_SIZE = 32  # 2 + QUEUE_DEPTH
    NUM_ENTRIES = 5

    _HOST_PUSH_BASE = STORAGE_BASE
    _HOST_POLL_BASE = STORAGE_BASE + NUM_ENTRIES * ENTRY_SIZE

    class HostMessage(IntEnum):
        """Messages the host pushes to the device (mirrors llk::message_queue::host_message)."""

        H2D_EXECUTE_KERNEL_REQ = 0
        D2H_DUMP_TENSIX_ACK = 1

    class DeviceMessage(IntEnum):
        """Messages the device pushes to the host (mirrors llk::message_queue::device_message)."""

        D2H_DUMP_TENSIX_REQ = 0
        H2D_EXECUTE_KERNEL_ACK = 1

    def __init__(self, device_idx: int, location: str = "0,0"):
        self._device_idx = device_idx
        self._location = location

        # Pre-compute base addresses for this device index
        self._push_base = self._HOST_PUSH_BASE + device_idx * self.ENTRY_SIZE
        self._poll_base = self._HOST_POLL_BASE + device_idx * self.ENTRY_SIZE

    def _read_byte(self, addr: int) -> int:
        data = read_from_device(self._location, addr, num_bytes=1)
        return data[0]

    def _write_byte(self, addr: int, value: int) -> None:
        write_to_device(self._location, addr, [value & 0xFF])

    # ── host_to_device queue (host pushes, device pops) ──────────────
    #
    #   write pointer : host_push[idx].host_to_device_req   (offset 0, host-owned)
    #   read  pointer : host_poll[idx].host_to_device_ack   (offset 1, device-owned)
    #   queue data    : host_push[idx].host_to_device_queue (offset 2, host-owned)

    @property
    def _h2d_write_idx_addr(self) -> int:
        return self._push_base + 0

    @property
    def _h2d_read_idx_addr(self) -> int:
        return self._poll_base + 1

    @property
    def _h2d_queue_addr(self) -> int:
        return self._push_base + 2

    # ── device_to_host queue (device pushes, host pops) ──────────────
    #
    #   write pointer : host_poll[idx].device_to_host_req   (offset 0, device-owned)
    #   read  pointer : host_push[idx].device_to_host_ack   (offset 1, host-owned)
    #   queue data    : host_poll[idx].device_to_host_queue (offset 2, device-owned)

    @property
    def _d2h_write_idx_addr(self) -> int:
        return self._poll_base + 0

    @property
    def _d2h_read_idx_addr(self) -> int:
        return self._push_base + 1

    @property
    def _d2h_queue_addr(self) -> int:
        return self._poll_base + 2

    # ── public API ───────────────────────────────────────────────────

    def initialize(self) -> None:
        """Zero out both queue entries for this device index (call before starting the device)."""
        write_to_device(self._location, self._push_base, bytes(self.ENTRY_SIZE))
        write_to_device(self._location, self._poll_base, bytes(self.ENTRY_SIZE))

    def push(self, message: "MessageQueue.HostMessage", timeout: float = 30.0) -> None:
        """Push a host_message into the host-to-device queue."""
        write_idx = self._read_byte(self._h2d_write_idx_addr)
        next_write = (write_idx + 1) % self.QUEUE_DEPTH

        # Spin until there is space in the queue
        deadline = time.monotonic() + timeout
        while next_write == self._read_byte(self._h2d_read_idx_addr):
            if time.monotonic() > deadline:
                raise TimeoutError(
                    "Timed out waiting for space in host-to-device queue"
                )
            time.sleep(0.001)

        # Write the message and advance the write pointer
        self._write_byte(self._h2d_queue_addr + write_idx, int(message))
        self._write_byte(self._h2d_write_idx_addr, next_write)

    def pop(self, timeout: float = 30.0) -> "MessageQueue.DeviceMessage":
        """Pop a device_message from the device-to-host queue."""
        read_idx = self._read_byte(self._d2h_read_idx_addr)

        # Spin until there is a message in the queue
        deadline = time.monotonic() + timeout
        while self._read_byte(self._d2h_write_idx_addr) == read_idx:
            if time.monotonic() > deadline:
                raise TimeoutError("Timed out waiting for device message")
            time.sleep(0.001)

        # Read the message and advance the read pointer
        raw = self._read_byte(self._d2h_queue_addr + read_idx)
        next_read = (read_idx + 1) % self.QUEUE_DEPTH
        self._write_byte(self._d2h_read_idx_addr, next_read)

        return self.DeviceMessage(raw)
