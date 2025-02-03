import torch
from .dictionaries import *
import numpy as np

def untilize(original_tensor,stimuli_format = "Float16_b"):

    if original_tensor.size(0) != 1024:
        raise ValueError("Input tensor must have 1024 elements.")
    
    submatrices = original_tensor.reshape(4, 16, 16)

    new_tensor = torch.zeros((32, 32))#, dtype=format_dict[stimuli_format])

    new_tensor[0:16, 0:16] = submatrices[0]  # Top-left
    new_tensor[0:16, 16:32] = submatrices[1]  # Top-right
    new_tensor[16:32, 0:16] = submatrices[2]  # Bottom-left
    new_tensor[16:32, 16:32] = submatrices[3]  # Bottom-right

    return new_tensor

import torch

def calculate_read_words_cnt(format,src_A, sfpu=False):

    if sfpu == True: # for now just for 16 bit formats
        return 128

    if(format == "Float16" or format == "Float16_b"):
        return 512
    elif( format == "Bfp8_b"):
        return 282
    elif( format == "Float32" or format == "Int32"):
        return 1024

def tilize(original_tensor, stimuli_format="Float16_b"):

    if original_tensor.size(0) != 32 or original_tensor.size(1) != 32:
        raise ValueError("Input tensor must have size 32x32.")
    
    submatrix_1 = original_tensor[0:16, 0:16]  # Top-left
    submatrix_2 = original_tensor[0:16, 16:32]  # Top-right
    submatrix_3 = original_tensor[16:32, 0:16]  # Bottom-left
    submatrix_4 = original_tensor[16:32, 16:32]  # Bottom-right
    
    # Stack the submatrices into a single tensor
    result = torch.cat([submatrix_1.flatten(), submatrix_2.flatten(),
                        submatrix_3.flatten(), submatrix_4.flatten()])
    
    # Return the tensor in the requested format (if applicable)
    return result.to(dtype=torch.bfloat16 if stimuli_format == "Float16_b" else torch.float32)

def revese_endian_chunk(input_list, chunk_size = 4):

    output_list = []
    
    for j in range(0, len(input_list), chunk_size):
        chunk = input_list[j:j+chunk_size]
        reversed_chunk = chunk[::-1]
        output_list.extend(reversed_chunk)
    
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

def comp_pcc(golden, calculated, pcc=0.99):
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
