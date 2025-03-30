from typing import List
from helpers.format_arg_mapping import DestAccumulation

packer_gasket_out = {
    "Float32": ["Float32", "Tf32", "Float16", "Float16_b", "Bfp8_b"],
    "Float16": ["Float16", "Bfp8", "Lf8"],
    "Float16_b": ["Float16_b", "Bfp8_b"], 
    "Int32": ["Int32"], # Even though hw team says Float16 is supported output for Float16_b, our research shows otherwise. It is left out for now, and an issue will be opened for further investigation.
    "Bfp8": []
}

packer_out = {
    "Float32": ["Float32", "Float16", "Float16_b"],
    "Float16": ["Float32", "Float16", "Float16_b", "Bfp8", "Bfp4", "Bfp2", "Lf8"], # if dest register not in fp32 mode, then packer does not support convertion from exponent B type to A type
    "Float16_b": ["Float32", "Float16_b", "Bfp8_b", "Bfp4_b", "Bfp2_b"], # if dest register not in fp32 mode, then packer does not support convertion from exponent B type to A type
    "Bfp8_b": ["Float32", "Float16_b", "Bfp8_b", "Bfp8", "Bfp4_b", "Bfp2_b", "Lf8", "Bfp4", "Bfp2"],
    "Int32": ["Int32"]
}

def check_exponent_b(format: str)-> bool:
    if format[-1] == "b":
        return True
    return False

def check_unpack(src : str, dst: str, unpack_to_dest: bool) -> bool:
    if dst == src == "Int32" and DestAccumulation.Yes and unpack_to_dest:
        return True
    if src == "Float32":
        return dst in ["Float32", "Float16", "Float16_b"]
    else:
        return src == dst
    
def get_register_format(unpack_dst : str, fp32_mode: DestAccumulation) -> str:
    if fp32_mode.Yes:
        return "Float32"
    if unpack_dst in ["Float32", "Float16", "Float16_b"]:
        return unpack_dst
    elif unpack_dst == "Bfp8_b":
        return "Float16_b"
    
    return ""

def get_pack_src(register_format : str) -> List[str]:
    return packer_gasket_out[register_format]

def check_pack_src(pack_src : str, unpack_dst: str, fp32_mode: DestAccumulation, unpack_to_dest: bool) -> bool:
    if fp32_mode == DestAccumulation.Yes and pack_src != "Int32":
        return pack_src in packer_gasket_out["Float32"]
    if unpack_dst == pack_src == "Int32" and fp32_mode == DestAccumulation.Yes and unpack_to_dest:
        return True
    return pack_src in get_pack_src(get_register_format(unpack_dst, fp32_mode))

def check_pack(unpack_dst: str, pack_src: str, pack_dst: str, fp32_mode : DestAccumulation, unpack_to_dest: bool) -> bool:
    if check_pack_src(pack_src, unpack_dst, fp32_mode, unpack_to_dest):
        return pack_dst in packer_out[pack_src]
    return False

def hw_support(unpack_src: str, unpack_dst: str, pack_src: str, pack_dst: str, fp32_mode: DestAccumulation, unpack_to_dest: bool) -> str:
    if unpack_src == "Bfp8_b" and (not check_exponent_b(pack_dst)) and fp32_mode == DestAccumulation.No:
        return "FAIL"
    if check_unpack(unpack_src, unpack_dst, unpack_to_dest) and check_pack(unpack_dst, pack_src, pack_dst, fp32_mode, unpack_to_dest):
        return "PASS"
    return "FAIL"