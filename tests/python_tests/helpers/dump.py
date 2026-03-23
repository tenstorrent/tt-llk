# SPDX-FileCopyrightText: © 2026 Tenstorrent AI ULC
#
# SPDX-License-Identifier: Apache-2.0

import difflib
import json
from dataclasses import asdict
from enum import IntEnum
from typing import Any, ClassVar

from helpers.device import commit_brisc_command
from helpers.llk_params import BriscCmd
from helpers.logger import logger
from ttexalens.tt_exalens_lib import (
    check_context,
    convert_coordinate,
    get_tensix_state,
    read_words_from_device,
    write_words_to_device,
)


class TensixDump:

    TENSIX_DUMP_MAILBOX_ADDRESS: ClassVar[int] = 0x16AFE4

    class MailboxState(IntEnum):
        DONE = 0
        REQUESTED = 1

    @classmethod
    def initialize(cls, location: str) -> None:
        from helpers.test_config import TestConfig

        count = len(TestConfig.KERNEL_COMPONENTS)
        initial = [cls.MailboxState.DONE] * count
        write_words_to_device(location, cls.TENSIX_DUMP_MAILBOX_ADDRESS, initial)

    @classmethod
    def try_process_request(cls, dumps: list[Any], location: str = "0,0"):
        is_requested = cls._try_receive_request(location)

        if not is_requested:
            return False

        commit_brisc_command(location, BriscCmd.DO_EBREAK)

        dumps.append(cls._fetch_state(location))
        cls._release_brisc_from_ebreak(location)
        cls._send_done(location)

        return True

    @classmethod
    def _release_brisc_from_ebreak(cls, core_loc: str = "0,0"):
        context = check_context()
        device = context.devices[0]
        coordinate = convert_coordinate(core_loc, 0, context)
        block = device.get_block(coordinate)
        risc_debug = block.get_risc_debug("brisc")
        risc_debug.cont()
        risc_debug.cont()

        logger.info("ASKJDHASKJDHAKSJ")

    @classmethod
    def _try_receive_request(cls, location: str):
        from helpers.test_config import TestConfig

        count = len(TestConfig.KERNEL_COMPONENTS)
        all_requested = [cls.MailboxState.REQUESTED] * count
        mailbox = read_words_from_device(
            location, cls.TENSIX_DUMP_MAILBOX_ADDRESS, word_count=count
        )
        return mailbox == all_requested

    @classmethod
    def _fetch_state(cls, location: str) -> dict[str, Any]:
        return asdict(get_tensix_state(location, device_id=0))

    @classmethod
    def _send_done(cls, location: str):
        from helpers.test_config import TestConfig

        count = len(TestConfig.KERNEL_COMPONENTS)
        done = [cls.MailboxState.DONE] * count
        write_words_to_device(location, cls.TENSIX_DUMP_MAILBOX_ADDRESS, done)

    @classmethod
    def format_state(cls, state: dict) -> str:
        return json.dumps(state, indent=4)

    @classmethod
    def assert_equal(cls, left: dict, right: dict) -> None:
        if left == right:
            return

        left_lines = cls.format_state(left).splitlines(keepends=True)
        right_lines = cls.format_state(right).splitlines(keepends=True)
        diff = difflib.unified_diff(
            left_lines,
            right_lines,
            fromfile="left",
            tofile="right",
            n=max(
                len(left_lines), len(right_lines)
            ),  # sstanisic todo: better way to force full diff ?
        )
        msg = f"Assertion FAILED: Tensix dump mismatch:\n{''.join(diff)}"
        logger.opt(exception=True).error(msg)
        raise AssertionError(msg)
