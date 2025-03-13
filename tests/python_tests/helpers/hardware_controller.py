# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC

from .utils import get_chip_architecture, run_shell_command


class HardwareController:
    def __init__(self):
        self.chip_architecture = get_chip_architecture()

    def reset(self):
        if self.chip_architecture == "blackhole":
            print("Resetting BH card")
            run_shell_command("/home/software/syseng/bh/tt-smi -r 0")
        elif self.chip_architecture == "wormhole":
            print("Resetting WH card")
            run_shell_command("/home/software/syseng/wh/tt-smi -wr 0")
        else:
            raise ValueError("Unknown chip architecture")
