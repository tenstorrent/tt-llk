# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0

"""
Domain-safe stimuli generation for fused operation testing.

This module provides functions to generate test inputs that avoid mathematical
domain issues when chaining elementwise binary and SFPU unary operations.
"""

from typing import Any, Dict, Optional, Tuple

import numpy as np
import torch

from .format_arg_mapping import MathOperation, format_dict
from .format_config import DataFormat


def get_format_safe_ranges(data_format: DataFormat) -> Dict[str, float]:
    """Get safe value ranges for different data formats to prevent overflow/underflow."""
    ranges = {
        DataFormat.Float16: {
            "min_positive": 6.1e-5,  # Smallest normal positive value
            "max_safe": 32.0,  # Well below overflow threshold
            "sqrt_max": 5.66,  # sqrt(32) to prevent square overflow
        },
        DataFormat.Float16_b: {
            "min_positive": 1.18e-38,  # bfloat16 smallest normal
            "max_safe": 128.0,  # Conservative for bfloat16
            "sqrt_max": 11.31,  # sqrt(128)
        },
        DataFormat.Float32: {
            "min_positive": 1.18e-38,  # Float32 smallest normal
            "max_safe": 1e10,  # Conservative upper bound
            "sqrt_max": 1e5,  # sqrt(1e10)
        },
        DataFormat.Bfp8_b: {
            "min_positive": 0.1,  # Even more conservative for BFP8
            "max_safe": 8.0,  # Tighter range for precision
            "sqrt_max": 2.8,  # sqrt(8)
        },
    }

    return ranges.get(data_format, ranges[DataFormat.Float16])


def generate_domain_safe_stimuli(
    eltwise_op: MathOperation,
    sfpu_op: MathOperation,
    input_format: DataFormat,
    dimensions: list = [32, 32],
    seed: Optional[int] = None,
) -> Tuple[torch.Tensor, torch.Tensor, int]:
    """
    Generate stimuli that ensure mathematically valid operation chains.

    Args:
        eltwise_op: Elementwise binary operation to be applied first
        sfpu_op: SFPU unary operation to be applied to the result
        input_format: Input data format
        dimensions: Tensor dimensions
        seed: Random seed for reproducibility

    Returns:
        Tuple of (src_A, src_B, tile_cnt) where src_A and src_B are flattened tensors
    """
    if seed is not None:
        torch.manual_seed(seed)
        np.random.seed(seed)

    ranges = get_format_safe_ranges(input_format)
    total_elements = np.prod(dimensions)

    # Calculate tile count (matches original generate_stimuli logic)
    tile_cnt = dimensions[0] // 32 * dimensions[1] // 32

    # Get constraints for this operation combination
    constraints = _get_operation_constraints(eltwise_op, sfpu_op, ranges)

    # Generate base tensors (flattened)
    src_A, src_B = _generate_constrained_tensors(
        total_elements, constraints, input_format
    )

    # Convert to proper data types (matching original generate_stimuli)
    dtype_A = (
        format_dict[input_format]
        if input_format != DataFormat.Bfp8_b
        else torch.bfloat16
    )
    dtype_B = (
        format_dict[input_format]
        if input_format != DataFormat.Bfp8_b
        else torch.bfloat16
    )

    src_A = src_A.to(dtype_A)
    src_B = src_B.to(dtype_B)

    return src_A, src_B, tile_cnt


def _get_operation_constraints(
    eltwise_op: MathOperation, sfpu_op: MathOperation, ranges: Dict[str, float]
) -> Dict[str, Any]:
    """Get constraints for specific operation combinations."""

    constraints = {
        "min_val": ranges["min_positive"],
        "max_val": ranges["max_safe"],
        "special_handling": None,
    }

    # Handle problematic combinations
    operation_pair = (eltwise_op, sfpu_op)

    if operation_pair == (MathOperation.Elwadd, MathOperation.Log):
        # For addition + log, keep both operands in safe range to ensure A + B stays in log domain
        # Use very conservative ranges for better precision, especially for BFP8
        constraints.update(
            {
                "min_val": max(
                    ranges["min_positive"] * 20, 0.5
                ),  # Higher minimum to avoid precision issues
                "max_val": min(
                    ranges["max_safe"] / 4, 4.0
                ),  # Tighter maximum for better precision
                "special_handling": "conservative_range",
            }
        )

    elif operation_pair == (MathOperation.Elwsub, MathOperation.Log):
        # Ensure A - B > min_positive to avoid log(0) or log(negative)
        constraints.update(
            {
                "special_handling": "positive_difference",
                "min_difference": ranges["min_positive"] * 10,
                "base_range": (ranges["min_positive"] * 100, ranges["max_safe"] / 2),
            }
        )

    elif operation_pair == (MathOperation.Elwsub, MathOperation.Sqrt):
        # Ensure A - B >= 0 for valid sqrt
        constraints.update(
            {
                "special_handling": "non_negative_difference",
                "min_difference": 0.0,
                "base_range": (0.1, ranges["max_safe"] / 2),
            }
        )

    elif operation_pair == (MathOperation.Elwsub, MathOperation.Reciprocal):
        # Ensure A - B is not too close to zero
        constraints.update(
            {
                "special_handling": "positive_difference",
                "min_difference": ranges["min_positive"] * 100,
                "base_range": (1.0, ranges["max_safe"] / 2),
            }
        )

    elif operation_pair == (MathOperation.Elwmul, MathOperation.Square):
        # Prevent overflow: (A * B)^2 should not overflow
        sqrt_max = ranges["sqrt_max"]
        constraints.update(
            {
                "special_handling": "bounded_product",
                "max_product": sqrt_max,
                "base_range": (ranges["min_positive"], sqrt_max / 2),
            }
        )

    elif operation_pair == (MathOperation.Elwmul, MathOperation.Log):
        # Ensure A * B > 0 and not too large for log
        constraints.update(
            {
                "special_handling": "positive_product",
                "min_product": ranges["min_positive"] * 10,
                "max_product": ranges["max_safe"],
                "base_range": (0.1, np.sqrt(ranges["max_safe"])),
            }
        )

    elif operation_pair == (MathOperation.Elwmul, MathOperation.Reciprocal):
        # Ensure A * B is not too small for reciprocal
        constraints.update(
            {
                "special_handling": "bounded_product",
                "min_product": ranges["min_positive"] * 1000,
                "max_product": ranges["max_safe"],
                "base_range": (0.1, np.sqrt(ranges["max_safe"])),
            }
        )

    elif sfpu_op == MathOperation.Square:
        # General square operation - prevent overflow
        constraints.update(
            {
                "max_val": ranges["sqrt_max"],
                "base_range": (ranges["min_positive"], ranges["sqrt_max"]),
            }
        )

    return constraints


def _generate_constrained_tensors(
    size: int, constraints: Dict[str, Any], input_format: DataFormat
) -> Tuple[torch.Tensor, torch.Tensor]:
    """Generate tensors that satisfy the given constraints."""

    special_handling = constraints.get("special_handling")

    if special_handling == "conservative_range":
        # Use conservative ranges for precision-sensitive operations
        min_val = constraints["min_val"]
        max_val = constraints["max_val"]

        src_A = torch.empty(size).uniform_(min_val, max_val)
        src_B = torch.empty(size).uniform_(min_val, max_val)

    elif special_handling == "positive_difference":
        # Generate A and B such that A - B > min_difference
        min_diff = constraints["min_difference"]
        base_min, base_max = constraints["base_range"]

        # Generate B first
        src_B = torch.empty(size).uniform_(base_min, base_max)
        # Generate A such that A > B + min_diff
        src_A = src_B + min_diff + torch.empty(size).uniform_(0, base_max - base_min)

    elif special_handling == "non_negative_difference":
        # Generate A and B such that A - B >= 0
        base_min, base_max = constraints["base_range"]

        src_B = torch.empty(size).uniform_(base_min, base_max)
        # Generate A such that A >= B
        src_A = src_B + torch.empty(size).uniform_(0, base_max - base_min)

    elif special_handling in ["bounded_product", "positive_product"]:
        # Generate A and B such that their product stays within bounds
        base_min, base_max = constraints["base_range"]
        max_product = constraints.get("max_product", base_max)
        min_product = constraints.get("min_product", base_min)

        # Generate A in base range
        src_A = torch.empty(size).uniform_(base_min, base_max)

        # Calculate B bounds based on A to satisfy product constraints
        b_max = torch.clamp(max_product / torch.abs(src_A), max=base_max)
        b_min = torch.clamp(min_product / torch.abs(src_A), min=base_min)

        # Generate B within calculated bounds
        src_B = torch.zeros(size)
        for i in range(size):
            src_B[i] = torch.empty(1).uniform_(b_min[i].item(), b_max[i].item())

    else:
        # Default generation within safe ranges
        min_val = constraints["min_val"]
        max_val = constraints["max_val"]

        src_A = torch.empty(size).uniform_(min_val, max_val)
        src_B = torch.empty(size).uniform_(min_val, max_val)

    return src_A, src_B


def is_combination_domain_safe(
    eltwise_op: MathOperation, sfpu_op: MathOperation
) -> bool:
    """Check if an operation combination can be made domain-safe with constrained stimuli."""

    # Combinations that can be made safe with proper stimuli generation
    safe_combinations = {
        (MathOperation.Elwsub, MathOperation.Sqrt),
        (MathOperation.Elwsub, MathOperation.Celu),
        (MathOperation.Elwmul, MathOperation.Square),  # with input constraints
        (MathOperation.Elwmul, MathOperation.Log),  # with input constraints
        (MathOperation.Elwadd, MathOperation.Square),  # generally safe
    }

    # Operations that are always problematic regardless of input
    unsafe_combinations = {
        (MathOperation.Elwsub, MathOperation.Log),  # Hard to ensure positive results
        (MathOperation.Elwsub, MathOperation.Reciprocal),  # Hard to avoid near-zero
    }

    return (eltwise_op, sfpu_op) in safe_combinations


# Convenience function for integration with existing tests
def should_use_domain_safe_stimuli(
    eltwise_op: MathOperation, sfpu_op: MathOperation, data_format: DataFormat
) -> bool:
    """Determine if domain-safe stimuli should be used for this combination."""

    # Always use for known problematic combinations that can be made safe
    if is_combination_domain_safe(eltwise_op, sfpu_op):
        return True

    # Use for precision-sensitive combinations with Log operation
    if sfpu_op == MathOperation.Log and eltwise_op in [
        MathOperation.Elwadd,
        MathOperation.Elwmul,
        MathOperation.Elwsub,
    ]:
        return True

    # Use for BFP8 with precision-sensitive operations
    if data_format == DataFormat.Bfp8_b and sfpu_op in [
        MathOperation.Square,
        MathOperation.Log,
        MathOperation.Reciprocal,
    ]:
        return True

    # Use for Float16 with overflow-prone operations
    if (
        data_format in [DataFormat.Float16, DataFormat.Float16_b]
        and sfpu_op == MathOperation.Square
    ):
        return True

    return False
