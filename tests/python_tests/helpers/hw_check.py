from typing import List


packer_outputs = {
    "Float32": ["Float32", "Float16", "Float16_b"],
    "Float16": ["Float32", "Float16", "Float16_b"],
    "Float16_b": ["Float32", "Float16_b", "Bfp8_b"],
    "Bfp8_b" : ["Float32", "Float16_b", "Bfp8_b"]
}

def check_unpacking(src: str, dst: str) -> bool:
    if src in ["Float16", "Float16_b"]:
        return dst == src or dst =="Tf_32"
    elif src == "Bfp8_b":
        return dst in ["Float16_b", "Tf32"]
    elif src == "Float32":
        return dst in ["Tf32", "Float16", "Float16_b", "Float32"]
    else:
        return False
    
def packer_gasket_outputs(src: str) -> List[str]:
    if src == "Float32":
        return ["Float32", "Tf32", "Float16", "Float16_b", "Bfp8_b"]
    elif src == "Float16":
        return ["Float16", "Bfp8", "Lf8"]
    elif src == "Float16_b":
        return ["Float16_b", "Bfp8_b", "Float16"]
    elif src in ["Bfp8_b", "Tf32"]:
        return []
    
def packer_support(gasket_outputs: List[str], dst: str) -> bool:
    if len(gasket_outputs) == 0:
        return False
    for gasket_out in gasket_outputs:
        if dst in packer_outputs[gasket_out]:
            return True
    
    return False

def check_packing(src: str, dst: str) -> bool:
    return packer_support(packer_gasket_outputs(src), dst)

def hw_support(unpack_src: str, unpack_dst: str, pack_src: str, pack_dst: str) -> str:
    if check_unpacking(unpack_src, unpack_dst) and check_packing(pack_src, pack_dst):
        return "PASS"
    return "FAIL"