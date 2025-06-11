# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
import torch

from .format_arg_mapping import format_dict
from .format_config import DataFormat


def apply_fidelity(operand1, operand2, data_format, math_fidelity_phase):

    # Extract exponents from all operands based on data format
    if data_format == DataFormat.Float16:
        # Convert operands to uint16 for bitwise operations
        operand1_uint = operand1.to(torch.float16).view(torch.uint16)
        operand2_uint = operand2.to(torch.float16).view(torch.uint16)

        # Mask 5 bits starting from 2nd MSB (bits 10 to 14)
        exponent_mask = 0x7C00  # 0111 1100 0000 0000

        exponents_1 = operand1_uint & exponent_mask
        exponents_2 = operand2_uint & exponent_mask
    elif data_format == DataFormat.Float16_b:
        # Convert operands to uint16 for bitwise operations
        operand1_uint = operand1.to(torch.bfloat16).view(torch.uint16)
        operand2_uint = operand2.to(torch.bfloat16).view(torch.uint16)

        # Print operands as hex numbers in 32x32 matrix
        print("Operand1 (bfloat16) as hex:")
        print(
            "\n".join(
                [
                    " ".join([f"{x.item():04x}" for x in row])
                    for row in operand1_uint.view(32, 32)
                ]
            )
        )
        print("Operand2 (bfloat16) as hex:")
        print(
            "\n".join(
                [
                    " ".join([f"{x.item():04x}" for x in row])
                    for row in operand2_uint.view(32, 32)
                ]
            )
        )

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

    # extract sign bits from all operands based on data format
    if data_format in [DataFormat.Float16, DataFormat.Float16_b]:
        sign_mask = 0x8000  # 1000 0000 0000 0000
        sign_1 = operand1_uint & sign_mask
        sign_2 = operand2_uint & sign_mask
    elif data_format == DataFormat.Float32:
        sign_mask = 0x80000000  # 1000 0000 0000 0000 0000 0000 0000 0000
        sign_1 = operand1_uint & sign_mask
        sign_2 = operand2_uint & sign_mask

    # extract mantissas from all operands based on data format
    if data_format == DataFormat.Float16:
        mantissa_mask = 0x3FF  # 0000 0011 1111 1111
        mantissas_1 = operand1_uint & mantissa_mask
        mantissas_2 = operand2_uint & mantissa_mask
    elif data_format == DataFormat.Float16_b:
        mantissa_mask = 0x7F  # 0000 0000 0111 1111
        mantissas_1 = operand1_uint & mantissa_mask
        mantissas_2 = operand2_uint & mantissa_mask
    elif data_format == DataFormat.Float32:
        mantissa_mask = 0x007FFFFF  # 0000 0000 0111 1111 1111 1111 1111 1111
        mantissas_1 = operand1_uint & mantissa_mask
        mantissas_2 = operand2_uint & mantissa_mask

    # Shift mantissas 3 times to the left
    # TODO: DO IT FOR ALL DATA FORMATS DIFFERENTLY 11 - mantissa length
    mantissas_1 = (mantissas_1.to(torch.int32) << 3).to(mantissas_1.dtype)
    mantissas_2 = (mantissas_2.to(torch.int32) << 3).to(mantissas_2.dtype)

    # Append 1 as MSB to each mantissa (for normalized numbers - implied leading 1)
    if data_format == DataFormat.Float16:
        mantissa_msb = 0x400  # 1 << 10, MSB of an 11-bit number
    elif data_format == DataFormat.Float16_b:
        mantissa_msb = 0x400  # 1 << 10, MSB of an 11-bit number
    elif data_format == DataFormat.Float32:
        mantissa_msb = 0x800000  # 1 << 23

    mantissas_1 = mantissas_1 | mantissa_msb
    mantissas_2 = mantissas_2 | mantissa_msb

    print("Mantissas 1 (binary):")
    print(
        "\n".join(
            [
                " ".join([f"{x.item():011b}" for x in row])
                for row in mantissas_1.view(32, 32)
            ]
        )
    )
    print("Mantissas 2 (binary):")
    print(
        "\n".join(
            [
                " ".join([f"{x.item():011b}" for x in row])
                for row in mantissas_2.view(32, 32)
            ]
        )
    )

    # print("\n" * 5)
    # print("Exponents 1:")
    # print('\n'.join([' '.join([f"{x.item():04x}" for x in row]) for row in exponents_1.view(32, 32)]))
    # print("Mantissas 1:")
    # print('\n'.join([' '.join([f"{x.item():04x}" for x in row]) for row in mantissas_1.view(32, 32)]))
    # print("\n" * 2)
    # print("Exponents 2:")
    # print('\n'.join([' '.join([f"{x.item():04x}" for x in row]) for row in exponents_2.view(32, 32)]))
    # print("Mantissas 2:")
    # print('\n'.join([' '.join([f"{x.item():04x}" for x in row]) for row in mantissas_2.view(32, 32)]))
    # print("\n" * 5)

    # set masks based on math fidelity phase
    if math_fidelity_phase == 0:
        a_mask = 0x7C0
        b_mask = 0x7F0
    elif math_fidelity_phase == 1:
        print()
    elif math_fidelity_phase == 2:
        print()
    elif math_fidelity_phase == 3:
        print()

    # apply masks to mantissas
    mantissas_1 = mantissas_1 & a_mask
    mantissas_2 = mantissas_2 & b_mask

    print("Mantissas 1 AFTER:")
    print(
        "\n".join(
            [
                " ".join([f"{x.item():011b}" for x in row])
                for row in mantissas_1.view(32, 32)
            ]
        )
    )
    print("Mantissas 2 AFTER:")
    print(
        "\n".join(
            [
                " ".join([f"{x.item():011b}" for x in row])
                for row in mantissas_2.view(32, 32)
            ]
        )
    )

    # Remove the MSB from mantissas (for normalized numbers)
    mantissas_1 = mantissas_1 & (~mantissa_msb)
    mantissas_2 = mantissas_2 & (~mantissa_msb)

    print("Mantissas 1 REMOVED SIGN:")
    print(
        "\n".join(
            [
                " ".join([f"{x.item():011b}" for x in row])
                for row in mantissas_1.view(32, 32)
            ]
        )
    )
    print("Mantissas 2 REMOVED SIGN:")
    print(
        "\n".join(
            [
                " ".join([f"{x.item():011b}" for x in row])
                for row in mantissas_2.view(32, 32)
            ]
        )
    )

    # Now when masks are applied shift mantissas back to their original positions
    mantissas_1 = (mantissas_1.to(torch.int32) >> 3).to(mantissas_1.dtype)
    mantissas_2 = (mantissas_2.to(torch.int32) >> 3).to(mantissas_2.dtype)

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

    print("\n\n", torch_format)

    return operand1.to(torch_format), operand2.to(torch_format)
