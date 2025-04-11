# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0

import pytest
import torch
import math
import sys
from helpers import *

all_params = generate_params(
    ["eltwise_unary_sfpu_test"],
    [
        FormatConfig(
            DataFormat.Float32,
            DataFormat.Float32,
            DataFormat.Int32,
            DataFormat.Int32,
            DataFormat.Float32,
            same_src_format=True,
        )
    ],
    dest_acc=[DestAccumulation.Yes],
    approx_mode=[ApproximationMode.No],
    mathop=[MathOperation.SetSgn],
)
param_ids = generate_param_ids(all_params)


@pytest.mark.parametrize(
    "testname, formats, dest_acc, approx_mode, mathop",
    clean_params(all_params),
    ids=param_ids,
)
def test_eltwise_unary_sfpu(testname, formats, dest_acc, approx_mode, mathop):  #

    src_A, src_B = generate_stimuli(
        formats.unpack_A_src, formats.unpack_B_src, sfpu=True
    )
    write_stimuli_to_l1(src_A, src_B, formats.unpack_A_src, formats.unpack_B_src)

    test_config = {
        "formats": formats,
        "testname": testname,
        "dest_acc": dest_acc,
        "mathop": mathop,
        "approx_mode": approx_mode,
    }
    make_cmd = generate_make_command(test_config)
    run_shell_command(f"cd .. && {make_cmd}")
    run_elf_files(testname)

    wait_for_tensix_operations_finished()
    res_from_L1 = collect_results(
        formats, sfpu=True
    )

    # sys.stdout.write(f"\nResult from L1:\n")
    # for i in range(0, len(res_from_L1), 8):  # 16 items per row
    #     sys.stdout.write(" ".join(f"{res_from_L1[j]}" for j in range(i, i + 8)) + "\n")

    assert res_from_L1[0] == -4, f"First element should be -4, but got {res_from_L1[0]}"
    
    
