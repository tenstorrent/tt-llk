# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
from dataclasses import dataclass
from typing import Optional


@dataclass
class TestConfig:
    run_simulator: bool = False
    simulator_port: Optional[int] = None
    device_id: int = 0
    log_level: str = "INFO"

    @classmethod
    def from_pytest_config(cls, config):
        cls.run_simulator = (config.getoption("--run_simulator"),)
        cls.simulator_port = config.getoption("--port")
        return cls
