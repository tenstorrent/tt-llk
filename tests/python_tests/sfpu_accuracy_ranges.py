# SPDX-FileCopyrightText: © 2026 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0
#
# Per-op input range profiles for SFPU accuracy tests.
#
# Range profiles express "where we sample inputs from" for a given op.  Each
# profile is a list of RangeSegment objects; samples are drawn proportionally to
# segment weights.
#
# Named profiles per op:
#   "train_core"  – model-relevant activations (main mass of input distributions)
#   "tail_high"   – right/large-value tails that may saturate the approximation
#   "near_zero"   – inputs close to 0 where relative error can blow up (log/sqrt/rsqrt)
#
# The domain scanner (Milestone 4) will eventually tighten these ranges per
# architecture/dtype and annotate samples with is_supported flags.

import math
from dataclasses import dataclass

import torch

TILE_ELEMENTS = 1024  # elements per 32x32 tile


@dataclass
class RangeSegment:
    lo: float
    hi: float
    sampler: str = "uniform"  # "uniform" | "log_uniform"
    weight: float = 1.0


# ─── Per-op range profiles ────────────────────────────────────────────────────

RANGES: dict[str, dict[str, list[RangeSegment]]] = {
    "relu": {
        "train_core": [RangeSegment(-8.0, 8.0, "uniform")],
        "tail_high": [
            RangeSegment(8.0, 64.0, "uniform"),
            RangeSegment(-64.0, -8.0, "uniform"),
        ],
    },
    "gelu": {
        "train_core": [RangeSegment(-8.0, 8.0, "uniform")],
        "tail_high": [
            RangeSegment(8.0, 12.0, "uniform"),
            RangeSegment(-12.0, -8.0, "uniform"),
        ],
    },
    "tanh": {
        "train_core": [RangeSegment(-8.0, 8.0, "uniform")],
        "tail_high": [RangeSegment(8.0, 16.0, "uniform")],
    },
    "exp": {
        "train_core": [RangeSegment(-10.0, 10.0, "uniform")],
        "tail_high": [
            RangeSegment(10.0, 20.0, "uniform"),
            RangeSegment(-20.0, -10.0, "uniform"),
        ],
    },
    "log": {
        "train_core": [RangeSegment(1e-4, 1e4, "log_uniform")],
        "near_zero": [RangeSegment(1e-8, 1e-4, "log_uniform")],
        "tail_high": [RangeSegment(1e4, 1e8, "log_uniform")],
    },
    "sqrt": {
        "train_core": [RangeSegment(0.0, 1e4, "uniform")],
        "near_zero": [RangeSegment(1e-8, 1e-4, "log_uniform")],
        "tail_high": [RangeSegment(1e4, 1e8, "log_uniform")],
    },
    "rsqrt": {
        "train_core": [RangeSegment(1e-4, 1e4, "log_uniform")],
        "near_zero": [RangeSegment(1e-8, 1e-4, "log_uniform")],
        "tail_high": [RangeSegment(1e4, 1e8, "log_uniform")],
    },
    "reciprocal": {
        "train_core": [
            RangeSegment(1e-3, 10.0, "uniform", weight=1.0),
            RangeSegment(-10.0, -1e-3, "uniform", weight=1.0),
        ],
        "near_zero": [
            RangeSegment(1e-8, 1e-3, "log_uniform", weight=1.0),
            RangeSegment(-1e-3, -1e-8, "log_uniform", weight=1.0),
        ],
        "tail_high": [
            RangeSegment(10.0, 1e3, "uniform", weight=1.0),
            RangeSegment(-1e3, -10.0, "uniform", weight=1.0),
        ],
    },
}


# ─── Sampling helpers ─────────────────────────────────────────────────────────


def _sample_segment(seg: RangeSegment, k: int) -> torch.Tensor:
    """Draw k samples from a single segment."""
    if seg.sampler == "uniform":
        return torch.empty(k).uniform_(seg.lo, seg.hi)

    if seg.sampler == "log_uniform":
        # log-uniform over [|lo|, |hi|], then apply sign from lo
        abs_lo = abs(seg.lo)
        abs_hi = abs(seg.hi)
        if abs_lo == 0.0:
            abs_lo = 1e-38
        log_lo = math.log10(min(abs_lo, abs_hi))
        log_hi = math.log10(max(abs_lo, abs_hi))
        logs = torch.empty(k).uniform_(log_lo, log_hi)
        vals = 10.0**logs
        # Propagate sign from segment lo (negative segments have lo < 0)
        if seg.lo < 0:
            vals = -vals
        return vals

    raise ValueError(f"Unknown sampler: {seg.sampler!r}")


def sample_inputs(
    op_name: str,
    profile_name: str,
    n_samples: int,
) -> torch.Tensor:
    """
    Sample *n_samples* input values for *op_name* / *profile_name*.

    Returns a float32 tensor of shape (n_samples,).  Values are drawn from the
    segments in the profile proportionally to their weights, then trimmed / tiled
    to exactly n_samples.
    """
    if op_name not in RANGES:
        raise KeyError(f"No range profile registered for op {op_name!r}")
    profiles = RANGES[op_name]
    if profile_name not in profiles:
        raise KeyError(
            f"No profile {profile_name!r} for op {op_name!r}. "
            f"Available: {list(profiles)}"
        )

    segments = profiles[profile_name]
    total_weight = sum(s.weight for s in segments)

    parts: list[torch.Tensor] = []
    for seg in segments:
        k = max(1, round(n_samples * seg.weight / total_weight))
        parts.append(_sample_segment(seg, k))

    x = torch.cat(parts)

    # Trim to exactly n_samples
    if len(x) > n_samples:
        x = x[:n_samples]
    elif len(x) < n_samples:
        repeats = (n_samples + len(x) - 1) // len(x)
        x = x.repeat(repeats)[:n_samples]

    return x.float()


def get_range_profiles(op_name: str) -> list[str]:
    """Return the list of profile names registered for *op_name*."""
    return list(RANGES.get(op_name, {}).keys())
