from typing import Optional
from .format_arg_mapping import *
from tabulate import tabulate
# from helpers.format_hw_support import hw_support
import csv

def pass_fail_results(testname, formats, dest_acc: Optional[DestAccumulation] = None, approx_mode: Optional[ApproximationMode] = None, mathop: Optional[MathOperation]=None, math_fidelity: Optional[MathFidelity]=None, tile_cnt: Optional[TileCount]=None, reduce_dim: Optional[ReduceDimension]=None, pool_type: Optional[ReducePool]=None):
    unpack_to_dest = False
    if testname in ["eltwise_unary_datacopy_test", "eltiwise_unary_sft_test"]:
        unpack_to_dest = True
    # hw = hw_support(
    #     formats.unpack_A_src.name,
    #     formats.unpack_A_dst.name,
    #     formats.pack_src.name,
    #     formats.pack_dst.name,
    #     dest_acc,
    #     unpack_to_dest
    # )
    res = [
        "FAIL",  # Result
        "PASS",# hw, # is format combination supported by HW
        formats.unpack_A_src.name,
        formats.pack_dst.name,
        0,
        "deadbeef",  # Placeholder for error (replace if failure)
        (formats.unpack_A_src.name), #, formats.unpack_B_src.name),  # Input Formats
        (formats.unpack_A_dst.name), #, formats.unpack_B_dst.name),
        (formats.math.name),
        (formats. pack_src.name), 
        (formats.pack_dst.name),  # Output Formats
    ]
    # add_included_paramaters(res, dest_acc, approx_mode, mathop, math_fidelity, tile_cnt, reduce_dim, pool_type)
    return res

def add_included_paramaters(res: list, dest_acc: Optional[DestAccumulation] = None, approx_mode: Optional[ApproximationMode] = None, mathop: Optional[MathOperation]=None, math_fidelity: Optional[MathFidelity]=None, tile_cnt: Optional[TileCount]=None, reduce_dim: Optional[ReduceDimension]=None, pool_type: Optional[ReducePool]=None):
    
    if dest_acc : 
        res.append(dest_acc)
    if approx_mode:
        res.append(approx_mode)
    if mathop:
        res.append(mathop)
    if math_fidelity:
        res.append(math_fidelity)
    if tile_cnt:
        res.append(tile_cnt)
    if reduce_dim:
        res.append(reduce_dim)
    if pool_type:
        res.append(pool_type)

def update_passed_test(results, pcc):
    results[-1][0] = "PASS"
    results[-1][4] = pcc
    return results

def update_failed_test(results, error):
    results[-1][5] = error
    return results

def format_included_params(results, pass_pass, fail_fail):
    for x in results[9:]:
        if isinstance(x, DestAccumulation):
            pass_pass.append("Dest Accumulation")
            # pass_fail.append("Dest Accumulation")
            # fail_pass.append("Dest Accumulation")
            fail_fail.append("Dest Accumulation")
        elif isinstance(x, ApproximationMode):
            pass_pass.append("Approx Mode")
            # pass_fail.append("Approx Mode")
            # fail_pass.append("Approx Mode")
            fail_fail.append("Approx Mode")
        elif isinstance(x, MathOperation):
            pass_pass.append("Math Op")
            # pass_fail.append("Math Op")
            # fail_pass.append("Math Op")
            fail_fail.append("Math Op")
        elif isinstance(x, MathFidelity):
            pass_pass.append("Fidelity")
            # pass_fail.append("Fidelity")
            # fail_pass.append("Fidelity")
            fail_fail.append("Fidelity")
        elif isinstance(x, TileCount):
            pass_pass.append("Tile(s)")
            # pass_fail.append("Tile(s)")
            # fail_pass.append("Tile(s)")
            fail_fail.append("Tile(s)")
        elif isinstance(x, ReduceDimension):       
            pass_pass.append("Reduce Dim")
            # pass_fail.append("Reduce Dim")
            # fail_pass.append("Reduce Dim")
            fail_fail.append("Reduce Dim")
        elif isinstance(x, ReducePool):
            pass_pass.append("Pool")
            # pass_fail.append("Pool")
            # fail_pass.append("Pool")
            fail_fail.append("Pool")
        
    # return pass_pass, pass_fail, fail_pass, fail_fail
    return pass_pass, fail_fail

def formatted_results(results):
    for i in range(len(results)):
        for j in range(len(results[i])):
            if j not in range(10):
                print(results[i])
                results[i][j] = results[i][j].name
    

def format_results(results):
    pass_results = [] 
    # pass_fail_results = []
    # fail_pass_results = []
    fail_fail_results = []
    
    # formatted_results(results)
    for entry in results:
        # If the test is marked as "PASS", organize the result accordingly
        if entry[0] == "PASS": # and entry[1] == "PASS":
            pass_results.append([entry[0]] + entry[2:4] + entry[6:])
        # elif entry[0] == "PASS" and entry[1] == "FAIL":
        #     pass_fail_results.append(entry[0:5] + entry[6:])
        # elif entry[0] == "FAIL" and entry[1] == "PASS":
        #     fail_pass_results.append(entry[0:4] + entry[5:])
        else:
            fail_fail_results.append([entry[0]] + entry[2:4] + entry[6:]) #(entry[0:4] + entry[5:])
    
    # Define headers for pass and fail results
    headers_pass = ["RESULT", "INPUT FORMAT", "OUTPUT FORMAT", "Unpack Src", "Unpack Dst", "FPU", "Pack Src", "Pack Dst"]
    # headers_pass_fail = ["RESULT", "HW", "INPUT FORMAT", "OUTPUT FORMAT", "PCC", "Unpack Src", "Unpack Dst", "FPU", "Pack Src", "Pack Dst"]
    # headers_fail_pass = ["RESULT", "HW", "INPUT FORMAT", "OUTPUT FORMAT", "ERROR", "Unpack Src", "Unpack Dst", "FPU", "Pack Src", "Pack Dst"]
    headers_fail_fail = ["RESULT", "INPUT FORMAT", "OUTPUT FORMAT", "Unpack Src", "Unpack Dst", "FPU", "Pack Src", "Pack Dst"]
    
    # headers_pass, headers_pass_fail, headers_fail_pass, headers_fail_fail = format_included_params(results, headers_pass, headers_pass_fail, headers_fail_pass, headers_fail_fail)
    # headers_pass, headers_fail_fail = format_included_params(results, headers_pass, headers_fail_fail)
    def sort_key(entry):
        if entry[0] == "PASS":  
            return (entry[1], entry[2]) #(entry[2], entry[3], -entry[4])
        else:
             return (entry[1], entry[2]) #(entry[2], entry[3]) 
    
    sorted_pass_results = sorted(pass_results, key=sort_key)
    # sorted_pass_fail_results = sorted(pass_fail_results, key=sort_key)
    # sorted_fail_pass_results = sorted(fail_pass_results, key=sort_key)
    sorted_fail_fail_results = sorted(fail_fail_results, key=sort_key)
    
    # Save results to a text file
    with open('test_results.txt', 'w') as file:
        file.write("\n--- Collected Test Info (PASS) ---\n")
        file.write(tabulate(sorted_pass_results, headers=headers_pass, tablefmt="grid"))
        # file.write("\n--- Collected Test Info (PASS-FAIL) ---\n")
        # file.write(tabulate(sorted_pass_fail_results, headers=headers_pass_fail, tablefmt="grid"))
        # file.write("\n\n--- Collected Test Info (FAIL-PASS) ---\n")
        # file.write(tabulate(sorted_fail_pass_results, headers=headers_fail_pass, tablefmt="grid"))
        file.write("\n\n--- Collected Test Info (FAIL) ---\n") #-FAIL) ---\n")
        file.write(tabulate(sorted_fail_fail_results, headers=headers_fail_fail, tablefmt="grid"))
        
    # Save results to a CSV file
    with open('test_results.csv', 'w', newline='') as file:
        writer = csv.writer(file)
        writer.writerow(headers_pass)
        writer.writerows(sorted_pass_results)
        writer.writerow([])
        # writer.writerow(headers_fail_pass)
        # writer.writerows(sorted_fail_pass_results)
        writer.writerow([])
        writer.writerow(headers_fail_fail)
        writer.writerows(sorted_fail_fail_results)