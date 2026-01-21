# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
#
# SPDX-License-Identifier: Apache-2.0

# from pathlib import Path

import pytest
from helpers.fused_golden import FusedGolden
from helpers.fused_pipeline import FUSER_CONFIG_DIR, create_fuse_pipeline

yaml_files = sorted(FUSER_CONFIG_DIR.glob("*.yaml"))
test_names = [f.stem for f in yaml_files]


@pytest.mark.parametrize("test_name", test_names, ids=test_names)
def test_fused(test_name, regenerate_cpp):
    config = create_fuse_pipeline(test_name)
    config.regenerate_cpp = regenerate_cpp
    config.run()

    if config.global_config.perf_run_type is None:
        golden = FusedGolden()
        assert golden.check_pipeline(config)
