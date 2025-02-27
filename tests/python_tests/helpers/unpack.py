# unpack.py

import struct
import torch
import struct
from .utils import *

unpacked_bfp8 = {}

def int_to_bytes_list(n):
    binary_str = bin(n)[2:].zfill(32)
    return [int(binary_str[i:i + 8], 2) for i in range(0, 32, 8)]

def bytes_to_float16(byte_list):
    bytes_data = bytes(byte_list[:2])
    unpacked_value = struct.unpack('>e', bytes_data)[0]
    return torch.tensor(unpacked_value, dtype=torch.float16)

def bytes_to_bfloat16(byte_list):
    bytes_data = bytes(byte_list[:2] + [0, 0])  # Ensure we include padding
    unpacked_value = struct.unpack('>f', bytes_data)[0]
    return torch.tensor(unpacked_value, dtype=torch.float32)

def bytes_to_float32(byte_list):
    bytes_data = bytes(byte_list)
    unpacked_value = struct.unpack('>f', bytes_data)[0]
    return torch.tensor(unpacked_value, dtype=torch.float32)

def bytes_to_int32(byte_list):
    bytes_data = bytes(byte_list)
    unpacked_value = struct.unpack('>I', bytes_data)[0]
    return torch.tensor(unpacked_value, dtype=torch.int32)

def unpack_fp16(packed_list):
    limited_packed_list = packed_list[:2048]
    return [bytes_to_float16(limited_packed_list[i:i + 2]).item() for i in range(0, len(limited_packed_list), 2)]

def unpack_bfp16(packed_list, unpack_src, pack_dst):
    limited_packed_list = packed_list[:2048]
    ret = []
    for i in range(0, len(limited_packed_list), 2):
        ret.append(bytes_to_bfloat16(limited_packed_list[i:i + 2]).item())
    if unpack_src == "Bfp8_b" and pack_dst != unpack_src:    
        for i in range(0, len(ret), 2):
            tmp = ret[i]
            ret[i] = ret[i+1]
            ret[i+1] = tmp
    return ret
    # return [bytes_to_bfloat16(limited_packed_list[i:i + 2]).item() for i in range(0, len(limited_packed_list), 2)]

def unpack_float32(packed_list):
    return [bytes_to_float32(packed_list[i:i + 4]).item() for i in range(0, len(packed_list), 4)]

def unpack_int32(packed_list):
    return [bytes_to_int32(packed_list[i:i + 4]).item() for i in range(0, len(packed_list), 4)]

def bfp8_to_float_block(exponent, bfp8_mantissas):
    bfloat16_values = []
    exponent = exponent - 127
    
    for mantissa in bfp8_mantissas:
        if (exponent, mantissa) in unpacked_bfp8:
            bfloat16_values.append(unpacked_bfp8[(exponent, mantissa)])
            continue
        sign_mantissa = str(format(mantissa, '08b'))
        sign = int(sign_mantissa[0],2)
        mantissa_value = sign_mantissa[1:]

        fract_value = 0.0
        for i in range(len(mantissa_value)):
            if(mantissa_value[i] == '1'):
                fract_value += 1/(2**(i))

        bfloat16_values.append(((-1.0)**sign)*(2**exponent)*(fract_value))

        unpacked_bfp8[(exponent, mantissa)] = (((-1.0)**sign)*(2**exponent)*(fract_value))

    return bfloat16_values

def unpack_bfp8_b(bfp8_block,unpack_src, pack_dst,sfpu=False):

    if sfpu == False:
        exponents = bfp8_block[:64]
        reversed_exponents = revese_endian_chunk(exponents)
        mantissas = bfp8_block[64:]
    else:
        exponents = bfp8_block[:16]
        reversed_exponents = revese_endian_chunk(exponents)
        mantissas = bfp8_block[16:272]

    bfloat16_values = []
    for i in range(len(reversed_exponents)):
        exponent = reversed_exponents[i]
        bfp8_mantissas = mantissas[i * 16:(i + 1) * 16]        
        reversed_sign_mantissa = revese_endian_chunk(bfp8_mantissas)

        block_bfloat16_values = bfp8_to_float_block(exponent, reversed_sign_mantissa)
        bfloat16_values.extend(block_bfloat16_values)
    if (unpack_src != pack_dst):
        for i in range(0, len(bfloat16_values), 2):
            tmp = bfloat16_values[i]
            bfloat16_values[i] = bfloat16_values[i+1]
            bfloat16_values[i+1] = tmp
    
    return torch.tensor(bfloat16_values, dtype=torch.bfloat16)
