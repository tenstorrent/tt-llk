from typing import Optional
from helpers.format_arg_mapping import *
from tabulate import tabulate

def results(formats, dest_acc: Optional[DestAccumulation] = None, approx_mode: Optional[ApproximationMode] = None, mathop: Optional[MathOperation]=None, math_fidelity: Optional[MathFidelity]=None, tile_cnt: Optional[TileCount]=None, reduce_dim: Optional[ReduceDimension]=None, pool_type: Optional[ReducePool]=None):
    hw = "PASS"
    res = [
        "FAIL",  # Result
        hw, # is format combination supported by HW
        (formats.unpack_A_src, formats.unpack_B_src),  # Input Formats
        (formats.unpack_A_dst, formats.unpack_B_dst),
        (formats. pack_src, formats.pack_dst),  # Output Format
        0,  # PCC placeholder until calculated
        "deadbeef",  # Placeholder for error (replace if failure)
    ]
    includes = [False, False, False, False, False, False, False]
    if dest_acc : 
        includes[0] = True
        res.append(dest_acc.name)
    if approx_mode:
        includes[1] = True
        res.append(approx_mode.value)
    if mathop:
        includes[2] = True
        res.append(mathop.name)
    if math_fidelity:
        includes[3] = True
        res.append(math_fidelity.name)
    if tile_cnt:
        includes[4] = True
        res.append(tile_cnt.value)
    if reduce_dim:
        includes[5] = True
        res.append(reduce_dim.name)
    if pool_type:
        includes[6] = True
        res.append(pool_type.name)
    return [res, includes]

def update_passed_result(results, pcc):
    results[0][-1][0] = "PASS"
    results[0][-1][4] = pcc
    return results

def update_failed_test(results, error):
    results[0][-1][6] = error
    return results