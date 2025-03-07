from dataclasses import dataclass
from typing import List, Optional

@dataclass
class FormatConfig:
    unpack_src: str
    unpack_dst: str
    math: str
    pack_src: str
    pack_dst: str
    
included_params = []   
def generate_format_combinations(formats: List[str], all_same: bool) -> List[FormatConfig]:
    if all_same: #This flag returns format combinations where all formats are the same
        return [
            FormatConfig(fmt, fmt, fmt, fmt, fmt)
            for fmt in formats
        ]
    return [
        FormatConfig(unpack_src, unpack_dst, math, pack_src, pack_dst)
        for unpack_src in formats
        for unpack_dst in formats
        for math in formats
        for pack_src in formats
        for pack_dst in formats
    ]

def generate_params(testnames: List[str], format_combos: List[FormatConfig], 
                    dest_acc: Optional[List[str]]= None, 
                    approx_mode: Optional[List[str]]= None, 
                    mathop: Optional[List[str]]= None, 
                    math_fidelity: Optional[List[int]]= None, 
                    tile_cnt: Optional[List[int]]= None,
                    reduce_dim: Optional[List[str]]= None,
                    pool_type: Optional[List[str]]= None) -> List[tuple]:
    
    included_params.extend([
        param for param, value in [
            ("dest_acc", dest_acc),
            ("approx_mode", approx_mode),
            ("mathop", mathop),
            ("math_fidelity", math_fidelity),
            ("tile_cnt", tile_cnt),
            ("reduce_dim", reduce_dim),
            ("pool_type", pool_type)
        ] if value is not None
    ])
    
    return [
        tuple(filter(lambda x: x is not None, (testname, format_config, acc_mode, approx, math, fidelity, num_tiles, dim, pool)))
        for testname in testnames
        for format_config in format_combos
        for acc_mode in (dest_acc if dest_acc is not None else [None])
        for approx in (approx_mode if approx_mode is not None else [None])
        for math in (mathop if mathop is not None else [None])
        for fidelity in (math_fidelity if math_fidelity is not None else [None])
        for num_tiles in (tile_cnt if tile_cnt is not None else [None])
        for dim in (reduce_dim if reduce_dim is not None else [None])
        for pool in (pool_type if pool_type is not None else [None])
    ]
    
def generate_param_ids(all_params: List[tuple]) -> List[str]:
    return [
        f"unpack_src={comb[1].unpack_src} | unpack_dst={comb[1].unpack_dst} | math={comb[1].math} | "
        f"pack_src={comb[1].pack_src} | pack_dst={comb[1].pack_dst} "
        f"| dest_acc={comb[2]} " if "dest_acc" in included_params else ""
        f"| approx_mode={comb[3]} " if "approx_mode" in included_params else ""
        f"| mathop={comb[4]} " if "mathop" in included_params else ""
        f"| math_fidelity={comb[5]}" if "math_fidelity" in included_params else ""
        f"| tile_cnt={comb[6]}" if "tile_cnt" in included_params else ""
        f"| reduce_dim={comb[7]}" if "reduce_dim" in included_params else ""
        f"| pool_type={comb[8]}" if "pool_type" in included_params else ""
        for comb in all_params
    ]