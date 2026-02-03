# SPDX-FileCopyrightText: © 2026 Tenstorrent AI ULC
#
# SPDX-License-Identifier: Apache-2.0

import io
from contextlib import redirect_stdout
from importlib import import_module
from typing import ClassVar

from ttexalens import init_ttexalens
from ttexalens.tt_exalens_init import GLOBAL_CONTEXT
from ttexalens.tt_exalens_lib import read_words_from_device, write_words_to_device
from ttexalens.uistate import UIState

# HACK! HACK! HACK! why would anyone name a python file with a dash ????????????????
t6_dump = import_module("ttexalens.cli_commands.dump-tensix-state").run


class TensixDump:

    TRISC_CFG_DUMP_MAILBOX_ADDRESS: ClassVar[int] = 0x16AFE4

    @classmethod
    def dump_tensix_config(cls, location: str) -> str:
        context = (
            GLOBAL_CONTEXT if GLOBAL_CONTEXT is not None else init_ttexalens()
        )  # HACK! HACK! HACK! use global context if it is available, otherwise initialize a new one
        ui_state = UIState(
            context
        )  # HACK! HACK! HACK! create new UIState because i can't get the one that is used by the CLI ????????????????
        stdout_capture = io.StringIO()
        with redirect_stdout(stdout_capture):
            t6_dump(f"tensix -d 0 -l {location}", context, ui_state)
        return stdout_capture.getvalue()

    @classmethod
    def signal_done(cls, location: str):
        write_words_to_device(location, cls.TRISC_CFG_DUMP_MAILBOX_ADDRESS, [0, 0, 0])

    @classmethod
    def try_poll(cls, location: str):
        values = read_words_from_device(
            location, cls.TRISC_CFG_DUMP_MAILBOX_ADDRESS, word_count=3
        )

        if values == [1, 1, 1]:
            result = cls.dump_tensix_config(location)
            cls.signal_done(location)

            return result

        return None
