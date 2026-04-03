# SPDX-FileCopyrightText: © 2026 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0
#
# SFPU Accuracy Tests – Unary Ops (Milestone 1)
#
# For each (data-format, op, range-profile) combination this test:
#   1. Generates controlled float inputs according to the op's range profile.
#   2. Quantises inputs to the target dtype (what the hardware actually receives).
#   3. Packs inputs into LLK tiles and writes them to L1.
#   4. Runs the generic unary SFPU kernel (sfpu_accuracy_unary_test.cpp).
#   5. Reads result tiles back from L1 and unpacks them to floats.
#   6. Computes PyTorch golden outputs and measures abs/rel/ULP errors.
#   7. Writes one CSV per (arch, dtype, op, range-profile) under:
#        accuracy_results/raw/{git_sha}/{arch}/{dtype}/{op}__{range_profile}.csv
#
# Run:
#   pytest test_sfpu_accuracy_unary.py -m sfpu_accuracy
#   pytest test_sfpu_accuracy_unary.py -m sfpu_accuracy -k "tanh"
#   pytest test_sfpu_accuracy_unary.py -m sfpu_accuracy -k "exp and train_core"
#
# CSV schema (one row per sample):
#   op_name, arch, dtype, range_profile, git_sha,
#   sample_id, input, output_ref, output_llk,
#   abs_err, rel_err, ulp_err, is_supported

import subprocess
from pathlib import Path

import numpy as np
import pandas as pd
import pytest
import torch
from helpers.format_config import DataFormat, InputOutputFormat
from helpers.llk_params import (
    DestAccumulation,
    format_dict,
)
from helpers.param_config import input_output_formats
from helpers.stimuli_config import StimuliConfig
from helpers.test_config import TestConfig
from helpers.test_variant_parameters import (
    APPROX_MODE,
    CLAMP_NEGATIVE,
    FAST_MODE,
    ITERATIONS,
    MATH_OP,
    NUM_BLOCKS,
    NUM_FACES,
    NUM_TILES_IN_BLOCK,
)
from sfpu_accuracy_config import OP_REGISTRY
from sfpu_accuracy_ranges import get_range_profiles, sample_inputs

# ─── Constants ────────────────────────────────────────────────────────────────

# Number of 32x32 tiles to process per test variant.
# Each tile holds 1024 samples → N_TILES * 1024 total samples per CSV row-group.
N_TILES = 2

TILE_ELEMENTS = 1024  # elements per 32x32 tile
N_SAMPLES = N_TILES * TILE_ELEMENTS

# Epsilon used as denominator guard for relative / ULP error
_EPSILON = 1e-30

# Supported formats for SFPU accuracy testing
_ACCURACY_FORMATS = [DataFormat.Float16_b, DataFormat.Float32]

# ─── Error-metric helpers ─────────────────────────────────────────────────────


def _ulp_size(x_f64: np.ndarray, torch_dtype: torch.dtype) -> np.ndarray:
    """
    Return the 1-ULP size (in float32) for each value in *x_f64* when that
    value is rounded to *torch_dtype*.

    For bfloat16 (7-bit mantissa), the ULP is 2^16 times a float32 ULP at
    the same magnitude.  For float32 (23-bit mantissa) the ULP is just
    numpy's spacing().
    """
    x_f32 = x_f64.astype(np.float32)
    spacing = np.abs(np.spacing(x_f32))  # 1 ULP in float32

    if torch_dtype == torch.bfloat16:
        # BF16 has 16 fewer mantissa bits than float32
        return spacing * 65536.0
    # float32 – spacing already gives the exact ULP
    return spacing


def _compute_errors(
    inputs_quantized: torch.Tensor,
    golden_f64: torch.Tensor,
    llk_f64: torch.Tensor,
    torch_dtype: torch.dtype,
) -> tuple[np.ndarray, np.ndarray, np.ndarray]:
    """Return (abs_err, rel_err, ulp_err) as float32 numpy arrays."""
    g = golden_f64.double().numpy()
    l = llk_f64.double().numpy()

    abs_err = np.abs(g - l).astype(np.float32)
    rel_err = (abs_err / np.maximum(np.abs(g), _EPSILON)).astype(np.float32)

    ulp = _ulp_size(g, torch_dtype)
    ulp_err = (abs_err / np.maximum(ulp, _EPSILON)).astype(np.float32)

    return abs_err, rel_err, ulp_err


# ─── CSV helpers ──────────────────────────────────────────────────────────────


def _get_git_sha() -> str:
    try:
        result = subprocess.run(
            ["git", "rev-parse", "--short", "HEAD"],
            capture_output=True,
            text=True,
            check=True,
            cwd=str(TestConfig.LLK_ROOT),
        )
        return result.stdout.strip()
    except Exception:
        return "unknown"


def _csv_path(
    git_sha: str, arch: str, dtype_name: str, op_name: str, range_profile: str
) -> Path:
    base = Path("accuracy_results") / "raw" / git_sha / arch / dtype_name
    base.mkdir(parents=True, exist_ok=True)
    return base / f"{op_name}__{range_profile}.csv"


def _write_csv(
    path: Path,
    *,
    op_name: str,
    arch: str,
    dtype_name: str,
    range_profile: str,
    git_sha: str,
    inputs_quantized: torch.Tensor,
    golden_f64: torch.Tensor,
    llk_output: torch.Tensor,
    abs_err: np.ndarray,
    rel_err: np.ndarray,
    ulp_err: np.ndarray,
) -> None:
    n = len(inputs_quantized)
    df = pd.DataFrame(
        {
            "op_name": op_name,
            "arch": arch,
            "dtype": dtype_name,
            "range_profile": range_profile,
            "git_sha": git_sha,
            "sample_id": np.arange(n, dtype=np.int64),
            "input": inputs_quantized.float().numpy(),
            "output_ref": golden_f64.float().numpy(),
            "output_llk": llk_output.float().numpy(),
            "abs_err": abs_err,
            "rel_err": rel_err,
            "ulp_err": ulp_err,
            "is_supported": True,  # domain scanner fills this in Milestone 4
        }
    )
    df.to_csv(path, index=False, na_rep="NaN")


# ─── Test parametrization ─────────────────────────────────────────────────────


def _build_test_params() -> list[tuple]:
    """Flat list of (fmt, op_name, range_profile) combos for parametrize."""
    params = []
    for fmt in input_output_formats(_ACCURACY_FORMATS, same=True):
        for op_name in OP_REGISTRY:
            for profile in get_range_profiles(op_name):
                params.append((fmt, op_name, profile))
    return params


_TEST_PARAMS = _build_test_params()


def _make_test_id(fmt: InputOutputFormat, op_name: str, range_profile: str) -> str:
    return f"{fmt.input_format.name}-{op_name}-{range_profile}"


# ─── Main test ────────────────────────────────────────────────────────────────


@pytest.mark.sfpu_accuracy
@pytest.mark.parametrize(
    "formats,op_name,range_profile",
    _TEST_PARAMS,
    ids=[_make_test_id(*p) for p in _TEST_PARAMS],
)
def test_sfpu_accuracy_unary(
    formats: InputOutputFormat,
    op_name: str,
    range_profile: str,
    workers_tensix_coordinates: str,
) -> None:
    """
    Accuracy test for a single (dtype, op, range-profile) triple.

    Generates *N_SAMPLES* controlled float inputs, runs the LLK SFPU kernel,
    and writes a CSV with per-sample error metrics.  No assertion is made –
    the test always passes; failures must be identified by inspecting the CSV
    or the dashboard (added in Milestone 3).
    """
    op_spec = OP_REGISTRY[op_name]
    torch_dtype = format_dict[formats.input_format]

    # ── 1. Generate inputs ──────────────────────────────────────────────────
    raw_inputs = sample_inputs(op_name, range_profile, N_SAMPLES)

    # Quantise to the hardware dtype so golden matches exactly what SFPU sees
    inputs_quantized = raw_inputs.to(torch_dtype)

    # src_A for StimuliConfig is a float32 view of the quantised values.
    # pack_bfp16 / pack_fp32 will re-quantise correctly from float32.
    src_A = inputs_quantized.float()  # (N_SAMPLES,)
    src_B = torch.zeros_like(src_A)  # unused by kernel

    # ── 2. Build StimuliConfig ──────────────────────────────────────────────
    # Tile layout: N_TILES tiles each with TILE_ELEMENTS elements.
    # StimuliConfig.write_matrix strides at MAX_TILE_ELEMENTS (1024) per tile.
    tile_count_A = N_TILES
    tile_count_B = N_TILES

    variant_stimuli = StimuliConfig(
        src_A,
        formats.input_format,
        src_B,
        formats.input_format,
        formats.output_format,
        tile_count_A=tile_count_A,
        tile_count_B=tile_count_B,
        tile_count_res=tile_count_A,
        num_faces=4,
        face_r_dim=16,
        sfpu=True,
    )

    # ── 3. Build and run TestConfig ─────────────────────────────────────────
    dest_acc = DestAccumulation.No
    unpack_to_dest = formats.input_format.is_32_bit()

    configuration = TestConfig(
        "sources/sfpu_accuracy_unary_test.cpp",
        formats,
        templates=[
            MATH_OP(mathop=op_spec.mathop),
            APPROX_MODE(approx_mode=op_spec.approx_mode),
            FAST_MODE(fast_mode=op_spec.fast_mode),
            CLAMP_NEGATIVE(clamp_negative=False),
            ITERATIONS(iterations=32),
        ],
        runtimes=[
            NUM_BLOCKS(num_blocks=tile_count_A),
            NUM_TILES_IN_BLOCK(num_tiles_in_block=1),
            NUM_FACES(num_faces=4),
        ],
        variant_stimuli=variant_stimuli,
        dest_acc=dest_acc,
        unpack_to_dest=unpack_to_dest,
    )

    outcome = configuration.run(workers_tensix_coordinates)
    res_from_l1: list[float] = outcome.result

    # ── 4. Compute golden on quantised inputs ───────────────────────────────
    # Golden is computed in float64 from the quantised (hardware-exact) inputs
    # so that the comparison is fair.
    inputs_f64 = inputs_quantized.double()
    golden_f64 = op_spec.golden_fn(inputs_f64).double()

    # ── 5. Collect LLK output ────────────────────────────────────────────────
    # res_from_l1 is a flat list of floats (length = N_TILES * TILE_ELEMENTS)
    llk_output = torch.tensor(res_from_l1, dtype=torch.float64)

    # Trim to N_SAMPLES in case the unpacker returned extra padding elements
    llk_output = llk_output[:N_SAMPLES]
    golden_f64 = golden_f64[:N_SAMPLES]
    inputs_quantized_trimmed = inputs_quantized[:N_SAMPLES]

    # ── 6. Compute error metrics ────────────────────────────────────────────
    abs_err, rel_err, ulp_err = _compute_errors(
        inputs_quantized_trimmed, golden_f64, llk_output, torch_dtype
    )

    # ── 7. Write CSV ────────────────────────────────────────────────────────
    git_sha = _get_git_sha()
    arch = TestConfig.ARCH.value
    dtype_name = formats.input_format.name

    csv_path = _csv_path(git_sha, arch, dtype_name, op_name, range_profile)
    _write_csv(
        csv_path,
        op_name=op_name,
        arch=arch,
        dtype_name=dtype_name,
        range_profile=range_profile,
        git_sha=git_sha,
        inputs_quantized=inputs_quantized_trimmed,
        golden_f64=golden_f64,
        llk_output=llk_output,
        abs_err=abs_err,
        rel_err=rel_err,
        ulp_err=ulp_err,
    )

    print(
        f"\n[sfpu_accuracy] {op_name:12s} {dtype_name:10s} {range_profile:12s} | "
        f"n={N_SAMPLES:5d} | "
        f"max_abs={abs_err.max():.4e} | "
        f"max_ulp={ulp_err.max():.2f} | "
        f"csv → {csv_path}"
    )
