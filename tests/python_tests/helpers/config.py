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


# Global instance
test_config = TestConfig()
