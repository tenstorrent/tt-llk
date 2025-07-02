# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC


class TestConfig:
    run_simulator: bool = False
    simulator_port: int = 5555
    device_id: int = 0
    log_level: str = "INFO"

    @classmethod
    def from_pytest_config(cls, config):
        port = config.getoption("--port")
        cls.run_simulator = config.getoption("--run_simulator")
        cls.simulator_port = port if port is not None else 5555
        return cls


# Global instance
test_config = TestConfig()
