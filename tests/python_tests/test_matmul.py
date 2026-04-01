# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0

from typing import List

import pytest
import torch
from helpers.device import BootMode
from helpers.format_config import DataFormat, FormatConfig, is_dest_acc_needed
from helpers.golden_generators import (
    MatmulGolden,
    _bfp4b_quantize_tilized,
    _bfp8b_quantize_tilized,
    get_golden_generator,
)
from helpers.llk_params import DestAccumulation, MathFidelity, format_dict
from helpers.matmul_sweep import (
    generate_matmul_dimension_combinations,
    generate_tile_dims,
)
from helpers.pack import pack_bfp4_b, pack_bfp8_b
from helpers.param_config import input_output_formats, parametrize
from helpers.stimuli_config import StimuliConfig
from helpers.stimuli_generator import generate_stimuli
from helpers.test_config import TestConfig
from helpers.test_variant_parameters import (
    CRK_TILE_DIMM,
    MATH_FIDELITY,
    NUM_FACES,
    TILE_COUNT,
)
from helpers.tilize_untilize import tilize_block, untilize_block
from helpers.unpack import unpack_bfp4_b, unpack_bfp8_b
from helpers.utils import passed_test, passed_test_bfp4_matmul


def generate_format_aware_matmul_combinations(
    formats_list: List[FormatConfig],
    dest_acc_modes: List[DestAccumulation],
):
    """
    Generate matmul dimension combinations for multiple tiles.

    Rules:
    1. Format outliers (Float16_b->Float16, Bfp8_b->Float16) MUST use dest_acc=Yes
    2. Running matmul tests on DestSync.Half, max tile count is 8
    3. When dest_acc=Yes: max 4 tiles (32-bit dest register)
    4. When dest_acc=No: max 8 tiles (16-bit dest register)

    Returns: List of (format, dest_acc, dimensions) tuples
    """
    combinations = []

    for fmt in formats_list:
        base_max_tiles = 4 if is_dest_acc_needed(fmt) else 8

        for dest_acc in dest_acc_modes:
            max_tiles = 4 if dest_acc == DestAccumulation.Yes else base_max_tiles
            dimensions_list = generate_matmul_dimension_combinations(max_tiles)
            combinations.extend([(fmt, dest_acc, dims) for dims in dimensions_list])

    return combinations


# Generate format-aware combinations
MATMUL_FORMATS = input_output_formats(
    [DataFormat.Float16_b, DataFormat.Float16, DataFormat.Float32, DataFormat.Bfp8_b]
)
DEST_ACC_MODES = [DestAccumulation.No, DestAccumulation.Yes]
ALL_MATMUL_COMBINATIONS = generate_format_aware_matmul_combinations(
    MATMUL_FORMATS, DEST_ACC_MODES
)


@parametrize(
    math_fidelity=[
        MathFidelity.LoFi,
        MathFidelity.HiFi2,
        MathFidelity.HiFi3,
        MathFidelity.HiFi4,
    ],
    format_dest_acc_and_dims=ALL_MATMUL_COMBINATIONS,
)
# Note: this test is used to test boot modes, that is why it has them piped as default arguments to the test itself
def test_matmul(
    math_fidelity,
    format_dest_acc_and_dims,
    workers_tensix_coordinates,
    boot_mode=BootMode.DEFAULT,
):
    torch_format = format_dict[format_dest_acc_and_dims[0].output_format]

    formats = format_dest_acc_and_dims[0]
    dest_acc = format_dest_acc_and_dims[1]
    input_A_dimensions = format_dest_acc_and_dims[2][0]
    input_B_dimensions = format_dest_acc_and_dims[2][1]

    src_A, tile_cnt_A, src_B, tile_cnt_B = generate_stimuli(
        stimuli_format_A=formats.input_format,
        input_dimensions_A=input_A_dimensions,
        stimuli_format_B=formats.input_format_B,
        input_dimensions_B=input_B_dimensions,
        sfpu=False,
    )

    # Calculate all matmul dimensions using helper function
    matmul_dims = generate_tile_dims((input_A_dimensions, input_B_dimensions))

    generate_golden = get_golden_generator(MatmulGolden)
    golden_tensor = generate_golden(
        src_A,
        src_B,
        formats.output_format,
        math_fidelity,
        input_A_dimensions=input_A_dimensions,
        input_B_dimensions=input_B_dimensions,
        # Golden cannot model FPU strided for tilized data computation, so we tilize output after computation
        tilize=True,
        input_A_format=formats.input_format,
        input_B_format=formats.input_format_B,
    )

    if formats.input_format != DataFormat.Bfp8_b:
        tilized_A = tilize_block(
            src_A, dimensions=input_A_dimensions, stimuli_format=formats.input_format
        )
        tilized_B = tilize_block(
            src_B, dimensions=input_B_dimensions, stimuli_format=formats.input_format_B
        )
    else:
        # BFP8 format requires special handling for tilization
        tilized_A = src_A
        tilized_B = src_B

    configuration = TestConfig(
        "sources/matmul_test.cpp",
        formats,
        templates=[MATH_FIDELITY(math_fidelity)],
        runtimes=[
            NUM_FACES(),
            TILE_COUNT(matmul_dims.output_tile_cnt),
            CRK_TILE_DIMM(matmul_dims.ct_dim, matmul_dims.rt_dim, matmul_dims.kt_dim),
        ],
        variant_stimuli=StimuliConfig(
            tilized_A.flatten(),
            formats.input_format,
            tilized_B.flatten(),
            formats.input_format_B,
            formats.output_format,
            tile_count_A=tile_cnt_A,
            tile_count_B=tile_cnt_B,
            tile_count_res=matmul_dims.output_tile_cnt,
        ),
        dest_acc=dest_acc,
        boot_mode=boot_mode,
    )

    res_from_L1 = configuration.run(workers_tensix_coordinates).result

    assert len(res_from_L1) == len(
        golden_tensor
    ), "Result tensor and golden tensor are not of the same length"

    res_tensor = torch.tensor(res_from_L1, dtype=torch_format)

    assert passed_test(
        golden_tensor, res_tensor, formats.output_format
    ), "Assert against golden failed"


BFP4_B_FORMATS = input_output_formats(
    [DataFormat.Bfp4_b, DataFormat.Bfp8_b, DataFormat.Float16_b]
)
BFP4_B_SMALL_DIMENSIONS = [
    ([32, 32], [32, 32]),
    # ([32, 64], [64, 32]),
    # ([64, 32], [32, 64]),
    # ([64, 64], [64, 64]),
]
BFP4_B_COMBINATIONS = [
    (fmt, dest_acc, dims)
    for fmt in BFP4_B_FORMATS
    for dest_acc in DEST_ACC_MODES
    for dims in BFP4_B_SMALL_DIMENSIONS
]


@parametrize(
    math_fidelity=[
        MathFidelity.LoFi,
        MathFidelity.HiFi2,
        MathFidelity.HiFi3,
        MathFidelity.HiFi4,
    ],
    format_dest_acc_and_dims=BFP4_B_COMBINATIONS,
)
def test_matmul_bfp4_b(
    math_fidelity,
    format_dest_acc_and_dims,
    workers_tensix_coordinates,
    boot_mode=BootMode.DEFAULT,
):
    torch_format = format_dict[format_dest_acc_and_dims[0].output_format]

    formats = format_dest_acc_and_dims[0]
    dest_acc = format_dest_acc_and_dims[1]
    input_A_dimensions = format_dest_acc_and_dims[2][0]
    input_B_dimensions = format_dest_acc_and_dims[2][1]

    if (
        formats.input_format != DataFormat.Bfp4_b
        and formats.output_format != DataFormat.Bfp4_b
    ):
        pytest.skip("not a bfp4_b test")

    DEBUG = True

    src_A, tile_cnt_A, src_B, tile_cnt_B = generate_stimuli(
        stimuli_format_A=formats.input_format,
        input_dimensions_A=input_A_dimensions,
        stimuli_format_B=formats.input_format_B,
        input_dimensions_B=input_B_dimensions,
        sfpu=False,
    )

    # # Create src_A as alternating blocks of 16 elements of 1 and 16 elements of 2
    # total_elements_A = input_A_dimensions[0] * input_A_dimensions[1]
    # pattern = torch.cat([torch.ones(16), torch.ones(16) * 2])
    # repeats = total_elements_A // 32
    # remaining = total_elements_A % 32

    # src_A_flat = pattern.repeat(repeats)
    # if remaining > 0:
    #     extra = torch.ones(min(16, remaining))
    #     src_A_flat = torch.cat([src_A_flat, extra])
    #     remaining -= len(extra)
    #     if remaining > 0:
    #         src_A_flat = torch.cat([src_A_flat, torch.ones(remaining) * 2])
    # src_A_flat = src_A_flat.to(torch.bfloat16)
    # src_A = src_A_flat
    # Create src_B as alternating blocks of 16 elements of 3 and 16 elements of 4 column-wise
    # rows_B, cols_B = input_B_dimensions
    # block_size = 16
    # full_blocks = cols_B // block_size
    # remainder_cols = cols_B % block_size

    # base_B = torch.empty((rows_B, cols_B), dtype=torch.bfloat16)

    # for block in range(full_blocks):
    #     val = 3 if block % 2 == 0 else 4
    #     base_B[:, block * block_size : (block + 1) * block_size] = val

    # if remainder_cols > 0:
    #     val = 3 if full_blocks % 2 == 0 else 4
    #     base_B[:, full_blocks * block_size :] = val

    # src_B = base_B.flatten()

    if DEBUG:
        print(f"\n{'='*70}")
        print(f"[DBG] src_A dtype={src_A.dtype} shape={src_A.shape}")
        print(f"[DBG] src_A = {src_A.view(input_A_dimensions)}")
        print(f"[DBG] src_B = {src_B.view(input_B_dimensions)}")

    matmul_dims = generate_tile_dims((input_A_dimensions, input_B_dimensions))

    generate_golden = get_golden_generator(MatmulGolden)
    golden_tensor = generate_golden(
        src_A,
        src_B,
        formats.output_format,
        math_fidelity,
        input_A_dimensions=input_A_dimensions,
        input_B_dimensions=input_B_dimensions,
        tilize=True,
        input_A_format=formats.input_format,
        input_B_format=formats.input_format_B,
    )

    M, K = input_A_dimensions[0], input_A_dimensions[1]
    Kb, N = input_B_dimensions[0], input_B_dimensions[1]
    assert K == Kb
    out_dims = (M, N)

    if DEBUG:
        print(
            f"[DBG] golden_tensor dtype={golden_tensor.dtype} shape={golden_tensor.shape}"
        )
        print(f"[DBG] golden (MxN) = {golden_tensor.view(M, N)}")

        def _pad_to_min_bfp_block(
            flat: torch.Tensor, min_len: int = 16
        ) -> torch.Tensor:
            flat = flat.flatten()
            if len(flat) >= min_len:
                return flat
            pad = torch.zeros(min_len - len(flat), dtype=flat.dtype, device=flat.device)
            return torch.cat([flat, pad])

        def _pack_unpack_row_major_a(flat_rm: torch.Tensor) -> torch.Tensor:
            flat_rm = flat_rm.flatten()
            fmt_a = formats.input_format
            if fmt_a not in (DataFormat.Bfp4_b, DataFormat.Bfp8_b):
                return flat_rm
            parts = []
            # Unpack must use the same num_faces / face_r_dim as pack, or mantissa bytes
            # are sliced away (exponents_in_packed defaults to 64) and the tensor is empty.
            for off in range(0, len(flat_rm), 16):
                chunk = _pad_to_min_bfp_block(flat_rm[off : off + 16])
                if fmt_a == DataFormat.Bfp4_b:
                    p = pack_bfp4_b(chunk, num_faces=1, face_r_dim=1)
                    parts.append(unpack_bfp4_b(bytes(p), num_faces=1, face_r_dim=1))
                else:
                    p = pack_bfp8_b(chunk, num_faces=1, face_r_dim=1)
                    parts.append(unpack_bfp8_b(bytes(p), num_faces=1, face_r_dim=1))
            sim = torch.cat([p.flatten() for p in parts])[: len(flat_rm)]
            return sim

        # Simulate pack→unpack on raw row-major src_A (what hardware sees after unpack).
        sim_A_from_hw = _pack_unpack_row_major_a(src_A.flatten())

        print(f"\n[DBG] --- simulate pack→unpack on raw row-major src_A ---")
        print(f"[DBG] sim_A_from_hw = {sim_A_from_hw.view(M, K)}")

        # Tilize→pack→unpack (matches MatmulGolden BFP operand path, 16-elem blocks).
        tilized_for_debug = tilize_block(
            src_A, dimensions=input_A_dimensions, stimuli_format=DataFormat.Float16_b
        ).flatten()
        sim_til = _pack_unpack_row_major_a(tilized_for_debug)
        untilized_sim = untilize_block(
            sim_til,
            stimuli_format=DataFormat.Float16_b,
            dimensions=input_A_dimensions,
        ).flatten()
        print(f"[DBG] sim_A tilize→pack→unpack→untilize = {untilized_sim.view(M, K)}")

    tilized_A = tilize_block(
        src_A, dimensions=input_A_dimensions, stimuli_format=formats.input_format
    )
    tilized_B = tilize_block(
        src_B, dimensions=input_B_dimensions, stimuli_format=formats.input_format_B
    )

    if DEBUG:
        print(f"\n[DBG] --- tilized_A (sent to hw) ---")
        print(f"[DBG] tilized_A dtype={tilized_A.dtype} shape={tilized_A.shape}")
        print(f"[DBG] tilized_A = {tilized_A.view(input_A_dimensions)}")

    configuration = TestConfig(
        "sources/matmul_test.cpp",
        formats,
        templates=[MATH_FIDELITY(math_fidelity)],
        runtimes=[
            NUM_FACES(),
            TILE_COUNT(matmul_dims.output_tile_cnt),
            CRK_TILE_DIMM(matmul_dims.ct_dim, matmul_dims.rt_dim, matmul_dims.kt_dim),
        ],
        variant_stimuli=StimuliConfig(
            tilized_A.flatten(),
            formats.input_format,
            tilized_B.flatten(),
            formats.input_format_B,
            formats.output_format,
            tile_count_A=tile_cnt_A,
            tile_count_B=tile_cnt_B,
            tile_count_res=matmul_dims.output_tile_cnt,
        ),
        dest_acc=dest_acc,
        boot_mode=boot_mode,
    )

    res_from_L1 = configuration.run(workers_tensix_coordinates).result

    assert len(res_from_L1) == len(
        golden_tensor
    ), "Result tensor and golden tensor are not of the same length"

    res_tensor = torch.tensor(res_from_L1, dtype=torch_format)

    if DEBUG:
        print(f"\n[DBG] --- hardware result vs golden ---")
        print(f"[DBG] res_tensor dtype={res_tensor.dtype} shape={res_tensor.shape}")
        print(f"[DBG] res (MxN row-major) = {res_tensor.view(M, N)}")
        print(f"[DBG] golden (MxN row-major via untilize) =")
        res_flat = res_tensor.float()
        gold_flat = golden_tensor.float()
        # Both tensors are in tilized face order; compare in row-major for stats and (r,c).
        gold_rm = untilize_block(
            gold_flat,
            stimuli_format=DataFormat.Float16_b,
            dimensions=out_dims,
        ).flatten()
        res_rm = untilize_block(
            res_flat,
            stimuli_format=DataFormat.Float16_b,
            dimensions=out_dims,
        ).flatten()
        print(f"[DBG] {gold_rm.view(M, N)}")
        abs_diff = (gold_rm - res_rm).abs()
        n_el = abs_diff.numel()
        med = abs_diff.median().item()
        p99 = torch.quantile(abs_diff.float(), 0.99).item()
        mx = abs_diff.max().item()
        n_gt8 = int((abs_diff > 8.0).sum().item())
        n_gt16 = int((abs_diff > 16.0).sum().item())
        iw = int(abs_diff.argmax().item())
        rw, cw = iw // N, iw % N
        print(
            f"[DBG] max_abs_diff={mx:.4f}  mean_abs_diff={abs_diff.mean().item():.4f}  "
            f"median={med:.4f}  p99={p99:.4f}"
        )
        print(
            f"[DBG] count(|diff|>8)={n_gt8}/{n_el}  count(|diff|>16)={n_gt16}/{n_el}  "
            f"worst @ rm_idx={iw} (r={rw},c={cw})  gold={gold_rm[iw].item():.4f}  hw={res_rm[iw].item():.4f}"
        )
        if formats.output_format == DataFormat.Bfp4_b:
            print(
                "[DBG] note: BFP4_b peak |diff| is often 8 or 16 when golden pack-sim and HW "
                "land on adjacent mantissa bins (same ~dot, different nibble); median/p99 show "
                "typical error. PCC is the pass criterion."
            )
        print(
            f"[DBG] res  min/max: {res_rm.min().item():.2f} / {res_rm.max().item():.2f}"
        )
        print(
            f"[DBG] gold min/max: {gold_rm.min().item():.2f} / {gold_rm.max().item():.2f}"
        )

        # Strict inequality counts every float32 representational difference; for BFP
        # outputs the golden and HW often differ by ~1.0 on the decoded bf16 grid while
        # PCC is still excellent. Use isclose for "actionable" mismatch lists only.
        def _rm_isclose(a: torch.Tensor, b: torch.Tensor) -> torch.Tensor:
            out = formats.output_format
            if out in (DataFormat.Bfp4_b, DataFormat.Bfp8_b):
                atol = 1.0 if out == DataFormat.Bfp4_b else 0.5
                return torch.isclose(a, b, rtol=0.02, atol=atol)
            return torch.isclose(a, b, rtol=1e-3, atol=1e-2)

        strict_rm_mismatches = int((abs_diff > 0).sum().item())
        rm_close = _rm_isclose(gold_rm, res_rm)
        actionable_rm_mismatches = int((~rm_close).sum().item())
        print(
            f"[DBG] row-major mismatches: strict (any |diff|>0)={strict_rm_mismatches}, "
            f"beyond isclose tol={actionable_rm_mismatches}"
        )

        def _quantize_tilized(til_tensor, fmt):
            if fmt == DataFormat.Bfp4_b:
                return _bfp4b_quantize_tilized(til_tensor)
            if fmt == DataFormat.Bfp8_b:
                return _bfp8b_quantize_tilized(til_tensor)
            return til_tensor

        # Quantized operands in row-major (matches matmul on unpacked L1 values).
        qA_rm = untilize_block(
            _quantize_tilized(tilized_A, formats.input_format),
            stimuli_format=DataFormat.Float16_b,
            dimensions=input_A_dimensions,
        ).flatten()
        qB_rm = untilize_block(
            _quantize_tilized(tilized_B, formats.input_format_B),
            stimuli_format=DataFormat.Float16_b,
            dimensions=input_B_dimensions,
        ).flatten()

        src_A_rm = src_A.flatten()
        src_B_rm = src_B.flatten()

        # Trace only elements outside isclose (avoids hundreds of ±1 BFP rounding drifts).
        mismatch_indices = (~rm_close).nonzero(as_tuple=True)[0]
        if len(mismatch_indices) > 0:

            print(
                f"\n[DBG] --- mismatch trace ({len(mismatch_indices)} mismatches, isclose-filtered) ---"
            )

            # For BFP4_b output format, dump the first mismatching 16-element
            # block in tilized order (packer layout): raw floats, packed bytes, reconstruct.
            if formats.output_format == DataFormat.Bfp4_b:
                til_close = _rm_isclose(gold_flat, res_flat)
                til_mismatch = (~til_close).nonzero(as_tuple=True)[0]
                if len(til_mismatch) > 0:
                    first_til_idx = til_mismatch[0].item()
                else:
                    first_til_idx = 0
                block_idx = first_til_idx // 16
                block_start = block_idx * 16
                block_end = block_start + 16
                gold_tilized = gold_flat
                res_tilized = res_flat
                gold_block = gold_tilized[block_start:block_end].tolist()
                res_block = res_tilized[block_start:block_end].tolist()
                # Pack the golden block and inspect bytes
                packed = pack_bfp4_b(
                    torch.tensor(gold_block, dtype=torch.bfloat16),
                    num_faces=1,
                    face_r_dim=1,
                )
                # pack_bfp4_b layout: [MIN_BFP_EXPONENTS exponent bytes][mantissa bytes]
                # With num_faces=1, face_r_dim=1 there is 1 block (16 elements),
                # so 1 real exponent padded to MIN_BFP_EXPONENTS=16, followed by 8 mantissa bytes.
                from helpers.tile_constants import MIN_BFP_EXPONENTS as _MIN_EXP

                shared_exp = packed[0]
                nibbles = []
                for byte in packed[_MIN_EXP:]:
                    nibbles.append(byte & 0xF)  # even element
                    nibbles.append((byte >> 4) & 0xF)  # odd element
                nibbles = nibbles[:16]
                print(
                    f"[DBG] --- first tilized mismatch block (til_idx={first_til_idx}, "
                    f"block_idx={block_idx}, elements [{block_start}:{block_end}]) ---"
                )
                print(f"[DBG]  golden  floats : {[f'{v:.3f}' for v in gold_block]}")
                print(f"[DBG]  hw      floats : {[f'{v:.3f}' for v in res_block]}")
                print(f"[DBG]  shared_exp={shared_exp} (2^{shared_exp-127:.0f})")
                print(f"[DBG]  packed nibbles : {nibbles}")
                # Reconstruct: value = sign * mag * 2^(shared_exp - 127) / 4
                # (3-bit mantissa encodes mag in [0..7], implicit scale /4)
                scale = 2.0 ** (shared_exp - 127)
                reconstructed = []
                for nib in nibbles:
                    sign = -1 if (nib >> 3) else 1
                    mag = nib & 0x7
                    reconstructed.append(sign * mag * scale / 4.0 if mag else 0.0)
                print(f"[DBG]  reconstructed  : {[f'{v:.3f}' for v in reconstructed]}")

            for flat_idx in mismatch_indices[:20].tolist():
                row = flat_idx // N
                col = flat_idx % N
                gold_val = gold_rm[flat_idx].item()
                hw_val = res_rm[flat_idx].item()
                a_row = qA_rm[row * K : (row + 1) * K].float()
                b_col = qB_rm.view(K, N)[:, col].float()
                dot_q = float(torch.dot(a_row, b_col))
                a_row_raw = src_A_rm[row * K : (row + 1) * K].float()
                b_col_raw = src_B_rm.view(K, N)[:, col].float()
                dot_raw = float(torch.dot(a_row_raw, b_col_raw))
                print(
                    f"[DBG]  rm_idx={flat_idx:4d} (r={row},c={col}): "
                    f"gold={gold_val:6.1f}  hw={hw_val:6.1f}  "
                    f"dot_q={dot_q:8.3f}  dot_raw={dot_raw:8.3f}"
                )
        elif strict_rm_mismatches > 0:
            print(
                f"\n[DBG] --- no isclose mismatches; {strict_rm_mismatches} strict "
                f"(|diff|>0) drifts (often ±1 on BFP grid; see PCC above) ---"
            )
        print(f"{'='*70}")

    pcc_threshold = 0.96

    assert passed_test_bfp4_matmul(
        golden_tensor,
        res_tensor,
        pcc_threshold=pcc_threshold,
        output_format=formats.output_format,
    ), f"Assert against golden failed (PCC < {pcc_threshold})"
