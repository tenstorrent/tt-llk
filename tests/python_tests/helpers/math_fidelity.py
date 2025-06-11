import torch
from .format_arg_mapping import DestAccumulation, MathFidelity, format_dict
from .format_config import DataFormat


def apply_fidelity(operand1, operand2, data_format, math_fidelity_phase):

    print("\n\n\nAPPLYING MATH FIDELITY PHASE: ", math_fidelity_phase,"\n\n\n")

    # Extract exponents from all operands based on data format
    if data_format == DataFormat.Float16:
        # Convert operands to uint16 for bitwise operations
        operand1_uint = operand1.to(torch.float16).view(torch.uint16)
        operand2_uint = operand2.to(torch.float16).view(torch.uint16)

        # Mask 5 bits starting from 2nd MSB (bits 10 to 14)
        exponent_mask = 0x7C00  # 0111 1100 0000 0000

        exponents_1 = (operand1_uint & exponent_mask)
        exponents_2 = (operand2_uint & exponent_mask)
    elif data_format == DataFormat.Float16_b:
        # Convert operands to uint16 for bitwise operations
        operand1_uint = operand1.to(torch.bfloat16).view(torch.uint16)
        operand2_uint = operand2.to(torch.bfloat16).view(torch.uint16)

        # Mask 8 bits starting from 2nd MSB (bits 7 to 14)
        exponent_mask = 0x7F80  # 0111 1111 1000 0000

        exponents_1 = operand1_uint & exponent_mask
        exponents_2 = operand2_uint & exponent_mask
    elif data_format == DataFormat.Float32:
        # Convert operands to uint32 for bitwise operations
        operand1_uint = operand1.to(torch.float32).view(torch.uint32)
        operand2_uint = operand2.to(torch.float32).view(torch.uint32)

        # Mask 8 bits starting from 2nd MSB (bits 23 to 30)
        exponent_mask = 0x7F800000  # 0111 1111 1000 0000 0000 0000 0000 0000

        exponents_1 = operand1_uint & exponent_mask
        exponents_2 = operand2_uint & exponent_mask

    # extract mantissas from all operands based on data format
    if data_format == DataFormat.Float16:
        mantissa_mask = 0x03FF  # 0000 0011 1111 1111
        mantissas_1 = operand1_uint & mantissa_mask
        mantissas_2 = operand2_uint & mantissa_mask
    elif data_format == DataFormat.Float16_b:
        mantissa_mask = 0x007F  # 0000 0000 0111 1111
        mantissas_1 = operand1_uint & mantissa_mask
        mantissas_2 = operand2_uint & mantissa_mask
    elif data_format == DataFormat.Float32:
        mantissa_mask = 0x007FFFFF  # 0000 0000 0111 1111 1111 1111 1111 1111
        mantissas_1 = operand1_uint & mantissa_mask
        mantissas_2 = operand2_uint & mantissa_mask

    
    #extract sign bits from all operands based on data format
    if data_format == DataFormat.Float16:
        sign_mask = 0x8000  # 1000 0000 0000 0000
        sign_1 = operand1_uint & sign_mask
        sign_2 = operand2_uint & sign_mask
    elif data_format == DataFormat.Float16_b:
        sign_mask = 0x8000  # 1000 0000 0000 0000
        sign_1 = operand1_uint & sign_mask
        sign_2 = operand2_uint & sign_mask
    elif data_format == DataFormat.Float32:
        sign_mask = 0x80000000  # 1000 0000 0000 0000 0000 0000 0000 0000
        sign_1 = operand1_uint & sign_mask
        sign_2 = operand2_uint & sign_mask

    # set masks based on math fidelity phase

    if(math_fidelity_phase == 0):
        a_mask = 0x7C0
        b_mask = 0x7C0
    elif(math_fidelity_phase == 1):
        a_mask = 0x3E
        b_mask = 0x7C0
    elif(math_fidelity_phase == 2):
        a_mask = 0x7C00
        b_mask = 0xF
    elif(math_fidelity_phase == 3):
        a_mask = 0x3E
        b_mask = 0xF


    # apply masks to mantissas 
    mantissas_1 = mantissas_1 & a_mask
    mantissas_2 = mantissas_2 & b_mask

    # print("\n" * 5)
    # print(exponents_1.view(32,32))
    # print(mantissas_1.view(32,32))
    # print("\n" * 5)
    # print(exponents_2.view(32,32))
    # print(mantissas_2.view(32,32))
    # print("\n" * 5)

    # Recombine the sign, exponent, and mantissa bits
    if data_format == DataFormat.Float16:
        operand1 = (sign_1 | exponents_1 | mantissas_1).view(torch.float16)
        operand2 = (sign_2 | exponents_2 | mantissas_2).view(torch.float16)
    elif data_format == DataFormat.Float16_b:
        operand1 = (sign_1 | exponents_1 | mantissas_1).view(torch.bfloat16)
        operand2 = (sign_2 | exponents_2 | mantissas_2).view(torch.bfloat16)
    elif data_format == DataFormat.Float32:
        operand1 = (sign_1 | exponents_1 | mantissas_1).view(torch.float32)
        operand2 = (sign_2 | exponents_2 | mantissas_2).view(torch.float32)
    
    torch_format = format_dict.get(data_format, format_dict[DataFormat.Float16_b])

    return operand1.to(torch_format), operand2.to(torch_format)
