# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
#
# SPDX-License-Identifier: Apache-2.0


from helpers.device import (
    collect_pipeline_results,
    write_pipeline_operands_to_l1,
)
from helpers.fuse_golden import FuseGolden
from helpers.fuse_pipeline import create_fuse_pipeline
from helpers.param_config import parametrize
from helpers.test_config import run_fuse_test


@parametrize(
    test_name="fuse_test",
)
def test_fused(test_name):
    # TODO: this argument should be used for specific yml/json config
    pipeline = create_fuse_pipeline()

    write_pipeline_operands_to_l1(pipeline)

    run_fuse_test(pipeline)

    collect_pipeline_results(pipeline)

    golden = FuseGolden()
    assert golden.check_pipeline(pipeline)
