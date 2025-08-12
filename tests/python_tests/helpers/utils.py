# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0

import math
import subprocess
from collections import namedtuple

import numpy as np
import torch

from .format_arg_mapping import Transpose, format_dict
from .format_config import DataFormat, FormatConfig

torch.set_printoptions(linewidth=500, sci_mode=False, precision=2, threshold=10000)


def print_faces(operand1):
    f0 = operand1[:256].view(16, 16)
    f1 = operand1[256:512].view(16, 16)
    f2 = operand1[512:768].view(16, 16)
    f3 = operand1[768:].view(16, 16)

    # Print the first set with proper alignment
    for i in range(16):
        print(
            " ".join(f"{x:6.2f}" for x in f0[i].tolist()),
            " | ",
            " ".join(f"{x:6.2f}" for x in f1[i].tolist()),
        )

    print("-" * 250)

    # Print the second set with proper alignment
    for i in range(16):
        print(
            " ".join(f"{x:6.2f}" for x in f2[i].tolist()),
            " | ",
            " ".join(f"{x:6.2f}" for x in f3[i].tolist()),
        )

    print("\n" * 3)


def run_shell_command(command: str, cwd: str | None = None):
    result = subprocess.run(
        command,
        cwd=cwd,
        shell=True,
        text=True,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.PIPE,
    )
    if result.returncode != 0:
        raise RuntimeError(f"Build failed: {command}\n{result.stderr}")
    return result


def calculate_read_byte_count(format: FormatConfig, array_size: int, sfpu=False) -> int:
    total_bytes = array_size * format.output_format.size
    if format.output_format == DataFormat.Bfp8_b:
        total_bytes += total_bytes // 16
    return total_bytes


def reverse_endian_chunk(input_list, chunk_size=4):
    output_list = []
    for j in range(0, len(input_list), chunk_size):
        output_list.extend(input_list[j : j + chunk_size][::-1])
    return output_list


def format_kernel_list(kernels, as_hex=False):
    formatter = hex if as_hex else str
    return ",".join(formatter(i) for i in kernels)


def calculate_pcc(golden, input):
    """Calculate Pearson Correlation Coefficient between two tensors.

    Handles special cases like NaN/Inf values and returns correlation coefficient.
    1.0 indicates perfect correlation, 0.0 indicates no correlation.

    Args:
        golden: Reference tensor
        input: Tensor to compare against golden

    Returns:
        float: Pearson correlation coefficient
    """
    golden = torch.as_tensor(golden)
    input = torch.as_tensor(input)

    if golden.dtype != input.dtype:
        input = input.type(golden.dtype)

    # Handle special cases
    if torch.all(torch.isnan(golden)) and torch.all(torch.isnan(input)):
        return 1.0

    if torch.all(torch.isnan(golden)) or torch.all(torch.isnan(input)):
        return 0.0

    # Test if either is completely zero
    if torch.any(golden.bool()) != torch.any(input.bool()):
        return 0.0

    # Handle exact equality case
    if torch.equal(golden, input):
        return 1.0

    # Mask invalid values (nan/inf)
    def mask_invalid(x):
        x = x.clone()
        mask = torch.logical_or(
            torch.isnan(x), torch.logical_or(torch.isinf(x), torch.isneginf(x))
        )
        x[mask] = 0
        return x

    golden = mask_invalid(golden)
    input = mask_invalid(input)

    # Convert bfloat16 to float32 for correlation calculation
    if golden.dtype == torch.bfloat16:
        golden = golden.type(torch.float32)
        input = input.type(torch.float32)

    # Calculate correlation
    pcc_result = np.min(
        np.ma.corrcoef(
            np.ma.masked_invalid(torch.squeeze(golden).detach().numpy()).flatten(),
            np.ma.masked_invalid(torch.squeeze(input).detach().numpy()).flatten(),
        )
    )

    if isinstance(pcc_result, np.ma.core.MaskedConstant):
        return 1.0

    return pcc_result


def tolerance_stoch_rounding(kt_dim: int) -> tuple[float, float]:
    """
    Compute stochastic-rounding tolerances (atol, rtol) for bfloat16 (Float16_b)
    using a 3σ random-walk model.

    Model:
    - Each op adds independent ±0.5·ULP noise (stochastic rounding).
    - Effective ops per output element ≈ 32 * kt_dim (32-wide K per tile).
    - Total stddev ≈ sqrt(ops) * (0.5 · ULP), and we return a 3σ bound.
    - For bfloat16 near value ≈ 1.0: ULP = 2^-7 = 1/128 ≈ 0.0078125.
    - We set rtol = atol (appropriate for values ~1.0).

    Args:
    kt_dim (int): Number of 32-wide K tiles (1..8). Effective ops = 32 * kt_dim.

    Returns:
    Tolerance: namedtuple('Tolerance', ['atol', 'rtol']) with both set to the
                3σ bound: 3 * sqrt(32 * kt_dim) * 0.5 * (2^-7).

    Examples:
    kt_dim=1  → ops=32  → atol≈3 * sqrt(32) * 0.5 * 1/128 ≈ 0.066
    kt_dim=8  → ops=256 → atol≈3 * 16       * 0.5 * 1/128 ≈ 0.1875
    """
    Tolerance = namedtuple("Tolerance", ["atol", "rtol"])
    # Effective ops per element: 32 values per K-tile × kt_dim tiles
    ops = max(1, 32 * kt_dim)

    # Stochastic rounding affects precision during Fpu rounding with DestAccumulation.No (16bit register) with float16_b since it has less mantissa bits
    # ULP for bfloat16 around value ~1.0
    ulp = 2.0**-7

    # Random-walk 3σ bound: 3 * sqrt(ops) * 0.5 * ULP
    atol = 3.0 * math.sqrt(ops) * 0.5 * ulp
    atol = math.ceil(atol * 10) / 10

    # Enforce a floor if your system needs 0.2 even for small kt_dim
    return Tolerance(atol=atol, rtol=atol)


def get_tolerance(output_data_format):
    if output_data_format in [
        DataFormat.Float16,
        DataFormat.Float16_b,
        DataFormat.Float32,
        DataFormat.Int32,
    ]:
        atol = 0.05
        rtol = 0.05
    elif output_data_format == DataFormat.Bfp8_b:
        atol = 0.1
        rtol = 0.2
    else:
        raise ValueError(f"Unsupported output data format: {output_data_format}")

    return atol, rtol


def passed_test(
    golden_tensor,
    res_tensor,
    output_data_format: DataFormat = DataFormat.Float16_b,
    L1_to_L1_iterations: int = 1,
    stochastic_rounding_enabled: bool = False,
    kt_dim: int = 1,
    transpose: Transpose = Transpose.No,
):
    transpose = 1 if transpose == Transpose.No else 2
    Tolerance = namedtuple("Tolerance", ["atol", "rtol"])

    def get_tolerance(output_data_format, stochastic_rounding_enabled=False):
        if stochastic_rounding_enabled:
            # Much more relaxed tolerances for stochastic rounding
            tolerances = {
                DataFormat.Float16_b: tolerance_stoch_rounding(kt_dim),
                DataFormat.Float32: tolerance_stoch_rounding(kt_dim),
            }
        else:
            # Standard tolerances for deterministic operations
            tolerances = {
                DataFormat.Float16: Tolerance(atol=0.05, rtol=0.05),
                DataFormat.Float16_b: Tolerance(atol=0.05, rtol=0.05),
                DataFormat.Float32: Tolerance(atol=0.05, rtol=0.05),
                DataFormat.Int32: Tolerance(atol=0, rtol=0),
                DataFormat.UInt32: Tolerance(atol=0, rtol=0),
                DataFormat.UInt16: Tolerance(atol=0, rtol=0),
                DataFormat.Int8: Tolerance(atol=0, rtol=0),
                DataFormat.UInt8: Tolerance(atol=0, rtol=0),
                DataFormat.Bfp8_b: Tolerance(atol=0.1, rtol=0.2),
            }

        try:
            return tolerances[output_data_format]
        except KeyError:
            raise ValueError(f"Unsupported output data format: {output_data_format}")

    tolerance = get_tolerance(output_data_format, stochastic_rounding_enabled)

    golden_tensor = golden_tensor.type(format_dict[output_data_format])
    res_tensor = res_tensor.type(format_dict[output_data_format])

    is_close = torch.isclose(
        golden_tensor, res_tensor, rtol=tolerance.rtol, atol=tolerance.atol
    )
    is_nan = torch.isnan(golden_tensor) & torch.isnan(res_tensor)

    is_valid = is_close | is_nan
    is_within_tolerance = torch.all(is_valid)

    if not is_within_tolerance:
        # Find all indices where values differ
        diff_indices = torch.where(~is_valid)[0]
        print(f"Found {len(diff_indices)} differences:")
        for idx in diff_indices:
            print(
                f"Failed at index {idx} with result={res_tensor[idx]}, golden={golden_tensor[idx]}"
            )

    pcc = calculate_pcc(res_tensor, golden_tensor)

    target_pcc = 0.99
    if output_data_format == DataFormat.Bfp8_b:
        target_pcc = pow(0.99, L1_to_L1_iterations)
    elif stochastic_rounding_enabled:
        target_pcc = 0.97  # In metal they set target_pcc to 0.97 for these cases
        target_pcc = pow(
            target_pcc, transpose
        )  # we transpose with stochastic rounding, so precision is accumulated here as well

    print(f"PCC: {pcc}")
    return is_within_tolerance and (pcc > target_pcc)
