# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0

"""
Minimal test to set RWC (Register Word Counters) to known values
for verification via debug bus.
"""

import pytest
from helpers.test_config import run_test


@pytest.mark.parametrize("arch", ["wormhole"])
def test_rwc_debug(arch):
    """
    Simple test that sets RWCs to deterministic values:
    - rwc_a (srcA counter)
    - rwc_b (srcB counter)
    - rwc_d (dest counter)

    After running this test, use debug bus to verify these values.
    """

    test_config = {"testname": "rwc_debug_test"}

    run_test(test_config)
