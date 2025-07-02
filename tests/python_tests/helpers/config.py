# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
from dataclasses import dataclass
from typing import ClassVar, Optional


@dataclass
class TestConfig:
    run_simulator: ClassVar[bool] = False
    simulator_port: ClassVar[Optional[int]] = None
    device_id: int = 0
    log_level: str = "INFO"

    @classmethod
    def from_pytest_config(cls, config):
        cls.run_simulator = config.getoption("--run_simulator")
        cls.simulator_port = config.getoption("--port")
        return cls


# Global instance
test_config = TestConfig()
