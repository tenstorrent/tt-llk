import pytest
import csv
from tabulate import tabulate

@pytest.fixture(scope='session')
def test_results():
    results = []
    yield results  # This makes the fixture available to tests
    
    pass_results = [] 
    pass_fail_results = []
    fail_pass_results = []
    fail_fail_results = []
    
    for entry in results:
        # If the test is marked as "PASS", organize the result accordingly
        if entry[0] == "PASS" and entry[1] == "PASS":
            pass_results.append(entry[0:5] + entry[6:])
        elif entry[0] == "PASS" and entry[1] == "FAIL":
            pass_fail_results.append(entry[0:5] + entry[6:])
        elif entry[0] == "FAIL" and entry[1] == "PASS":
            fail_pass_results.append(entry[0:4] + entry[5:])
        else:
            fail_fail_results.append(entry[0:4] + entry[5:])
    
    # Define headers for pass and fail results
    headers_pass = ["RESULT", "HW", "INPUT FORMAT", "OUTPUT FORMAT", "PCC", "Unpack Src", "Unpack Dst", "FPU", "Pack Src", "Pack Dst", "Math", "Dest Acc"]
    headers_pass_fail = ["RESULT", "HW", "INPUT FORMAT", "OUTPUT FORMAT", "PCC", "Unpack Src", "Unpack Dst", "FPU", "Pack Src", "Pack Dst", "Math", "Dest Acc"]
    headers_fail_pass = ["RESULT", "HW", "INPUT FORMAT", "OUTPUT FORMAT", "ERROR", "Unpack Src", "Unpack Dst", "FPU", "Pack Src", "Pack Dst", "Math", "Dest Acc"]
    headers_fail_fail = ["RESULT", "HW", "INPUT FORMAT", "OUTPUT FORMAT", "ERROR", "Unpack Src", "Unpack Dst", "FPU", "Pack Src", "Pack Dst", "Math", "Dest Acc"]

    
    def sort_key(entry):
        # Sorting by mathop first, then by dest_acc ("" for off comes before "DEST_ACC" for on)
        if entry[0] == "PASS":  
            return (entry[2], entry[3], entry[10], entry[11], -entry[4])
        else:
             return (entry[2], entry[3], entry[10]) 
    
    sorted_pass_results = sorted(pass_results, key=sort_key)
    sorted_pass_fail_results = sorted(pass_fail_results, key=sort_key)
    sorted_fail_pass_results = sorted(fail_pass_results, key=sort_key)
    sorted_fail_fail_results = sorted(fail_fail_results, key=sort_key)
    
    # Save results to a text file
    with open('test_results.txt', 'w') as file:
        file.write("\n--- Collected Test Info (PASS) ---\n")
        file.write(tabulate(sorted_pass_results, headers=headers_pass, tablefmt="grid"))
        file.write("\n--- Collected Test Info (PASS-FAIL) ---\n")
        file.write(tabulate(sorted_pass_fail_results, headers=headers_pass_fail, tablefmt="grid"))
        file.write("\n\n--- Collected Test Info (FAIL-PASS) ---\n")
        file.write(tabulate(sorted_fail_pass_results, headers=headers_fail_pass, tablefmt="grid"))
        file.write("\n\n--- Collected Test Info (FAIL-FAIL) ---\n")
        file.write(tabulate(sorted_fail_fail_results, headers=headers_fail_fail, tablefmt="grid"))
        
    # Save results to a CSV file
    with open('test_results.csv', 'w', newline='') as file:
        writer = csv.writer(file)
        
        # Write the headers for the pass results
        writer.writerow(headers_pass)
        
        # Write the data for pass results
        writer.writerows(sorted_pass_results)
        
        # Add a separator between pass and fail results
        writer.writerow([])  # Empty row
        
        # Write the headers for the fail results
        writer.writerow(headers_fail_pass)
        
        # Write the data for fail results
        writer.writerows(sorted_fail_pass_results)
        
        # Add a separator between pass and fail results
        writer.writerow([])  # Empty row
        
        # Write the headers for the fail results
        writer.writerow(headers_fail_fail)
        
        # Write the data for fail results
        writer.writerows(sorted_fail_fail_results)
