from typing import List

packer_gasket_out = {
    "Float32": ["Float32", "Tf32", "Float16", "Float16_b", "Bfp8_b"],
    "Float16": ["Float16", "Bfp8", "Lf8"],
    "Float16_b": ["Float16", "Float16_b", "Bfp8_b"],
    "Bfp8": []
}

packer_out = {
    "Float32": ["Float32", "Float16", "Float16_b"],
    "Float16": ["Float32", "Float16", "Float16_b", "Bfp8", "Bfp4", "Bfp2", "Lf8"],
    "Float16_b": ["Float32", "Float16_b", "Bfp8_b", "Bfp4_b", "Bfp2_b"],
    "Bfp8_b": ["Float32", "Float16_b", "Bfp8_b", "Bfp8", "Bfp4_b", "Bfp2_b", "Lf8", "Bfp4", "Bfp2"]
}

def check_unpack(src : str, dst: str) -> bool:
    if src == "Float32":
        return dst in ["Float32", "Float16", "Float16_b"]
    else:
        return src == dst
    
def get_register_format(unpack_dst : str) -> str:
    if unpack_dst in ["Float32", "Float16", "Float16_b"]:
        return unpack_dst
    elif unpack_dst == "Bfp8_b":
        return "Float16_b"
    
    return ""

def get_pack_src(register_format : str) -> List[str]:
    return packer_gasket_out[register_format]

def check_pack_src(pack_src : str, unpack_dst: str) -> bool:
    return pack_src in get_pack_src(get_register_format(unpack_dst))

def check_pack(pack_src: str, pack_dst: str) -> bool:
    if check_pack_src:
        return pack_dst in packer_out[pack_src]
    return False

def hw_support(unpack_src: str, unpack_dst: str, pack_src: str, pack_dst: str) -> str:
    if check_unpack(unpack_src, unpack_dst) and check_pack(pack_src, pack_dst):
        return "PASS"
    return "FAIL"