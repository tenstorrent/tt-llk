# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC


class TestConfig:
    """TestConfig class represents the test configuration set by command line options."""

    def __init__(
        self, run_simulator=False, simulator_port=5555, device_id=0, log_level="INFO"
    ):
        """
            Initializes the test configuration.
        Args:
            run_simulator (bool): True if the test is run on simulator, False if on silicon
            simulator_port (int): Simulator server port number
            device_id (int): ID number of the device to send message to.
            log_level (str): Log level
        """
        self.run_simulator: bool = run_simulator
        self.simulator_port: int = simulator_port
        self.device_id: int = device_id
        self.log_level: str = log_level

    def from_pytest_config(self, config):
        """
            Fetches the options set in the command line.
        Args:
            config: An instance of pytest.Config. It represents the configuration state for a test session.
        """
        port = config.getoption("--port")
        self.run_simulator = config.getoption("--run_simulator")
        self.simulator_port = port if port is not None else 5555


# Global instance
test_config = TestConfig()
