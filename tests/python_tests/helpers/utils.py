# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0

import torch
import numpy as np
import subprocess
from .format_arg_mapping import  format_dict,format_sizes

torch.set_printoptions(linewidth=500,sci_mode = False, precision=2,threshold=10000)

def print_faces(operand1):
    f0 = operand1[:256].view(16, 16)
    f1 = operand1[256:512].view(16, 16)
    f2 = operand1[512:768].view(16, 16)
    f3 = operand1[768:].view(16, 16)

    # Print the first set with proper alignment
    for i in range(16):
        print(' '.join(f"{x:6.2f}" for x in f0[i].tolist()), " | ", 
            ' '.join(f"{x:6.2f}" for x in f1[i].tolist()))

    print("-" * 250)

    # Print the second set with proper alignment
    for i in range(16):
        print(' '.join(f"{x:6.2f}" for x in f2[i].tolist()), " | ", 
            ' '.join(f"{x:6.2f}" for x in f3[i].tolist()))

    print("\n"*3)

def run_shell_command(command: str):
    result = subprocess.run(command, shell=True, text=True, capture_output=False,stdout=subprocess.DEVNULL)
    if result.returncode != 0:
        raise RuntimeError(f"Command failed: {command}\n{result.stderr}")
    return result

def calculate_read_words_count(format, sfpu=False):

    if format not in format_sizes:
        raise ValueError(f"Unsupported format: {format}")

    if sfpu: # for now just for 16 bit formats
        return 256 if format in ["Float32", "Int32"] else 128
    
    return format_sizes[format]

def tilize(original_tensor, stimuli_format="Float16_b"):

    if original_tensor.size(0) != 1024:
        raise ValueError("Input tensor must have 1024 elements.")
    
    matrix = original_tensor.view(32, 32)

    f0 = matrix[:16, :16]
    f1 = matrix[:16, 16:32]
    f2 = matrix[16:32, :16]
    f3 = matrix[16:32, 16:32]

    result = torch.cat((f0.reshape(-1), f1.reshape(-1), f2.reshape(-1), f3.reshape(-1)))

    return result.to(dtype=format_dict[stimuli_format] if stimuli_format in ["Float16_b","Float16"] else torch.float32)

def untilize(tilized_tensor, stimuli_format="Float16_b"):

    tilized_tensor = tilized_tensor.view(-1)

    f0 = tilized_tensor[:256].view(16, 16)
    f1 = tilized_tensor[256:512].view(16, 16)
    f2 = tilized_tensor[512:768].view(16, 16)
    f3 = tilized_tensor[768:].view(16, 16)

    top = torch.cat((f0, f1), dim=1)
    bottom = torch.cat((f2, f3), dim=1)

    original_tensor = torch.cat((top, bottom), dim=0).view(1024)

    return original_tensor.to(dtype=format_dict[stimuli_format] if stimuli_format in ["Float16_b","Float16"] else torch.float32)

def reverse_endian_chunk(input_list, chunk_size = 4):
    output_list = []
    for j in range(0, len(input_list), chunk_size):
        output_list.extend(input_list[j:j+chunk_size][::-1])
    return output_list

def format_kernel_list(kernels, as_hex=False):
    formatted_str = ""
    for i in kernels:
        # Use hex formatting if the flag is set, otherwise use decimal
        if as_hex:
            formatted_str += str(hex(i)) + ","
        else:
            formatted_str += str(i) + ","
    return formatted_str[:-1]  # Remove the trailing comma

def compare_pcc(golden, calculated, pcc=0.99):
    golden = torch.Tensor(golden)
    calculated = torch.Tensor(calculated)

    if golden.dtype != calculated.dtype:
        calculated = calculated.type(golden.dtype)

    if torch.all(torch.isnan(golden)) and torch.all(torch.isnan(calculated)):
        #logger.warning("Both tensors are 'nan'")
        return True, 1.0

    if torch.all(torch.isnan(golden)) or torch.all(torch.isnan(calculated)):
        #logger.error("One tensor is all nan, the other is not.")
        return False, 0.0

    # Test if either is completely zero
    if torch.any(golden.bool()) != torch.any(calculated.bool()):
        #logger.error("One tensor is all zero")
        return False, 0.0

    # For now, mask all infs and nans so that we check the rest... TODO
    golden = golden.clone()
    golden[
        torch.logical_or(
            torch.isnan(golden),
            torch.logical_or(torch.isinf(golden), torch.isneginf(golden)),
        )
    ] = 0
    calculated = calculated.clone()
    calculated[
        torch.logical_or(
            torch.isnan(calculated),
            torch.logical_or(torch.isinf(calculated), torch.isneginf(calculated)),
        )
    ] = 0

    if torch.equal(golden, calculated):
        return True, 1.0

    if golden.dtype == torch.bfloat16:
        golden = golden.type(torch.float32)
        calculated = calculated.type(torch.float32)
    cal_pcc = np.min(
        np.ma.corrcoef(
            np.ma.masked_invalid(torch.squeeze(golden).detach().numpy()).flatten(),
            np.ma.masked_invalid(torch.squeeze(calculated).detach().numpy()).flatten(),
        )
    )

    if isinstance(cal_pcc, np.ma.core.MaskedConstant):
        return True, 1.0

    return cal_pcc >= pcc, cal_pcc
