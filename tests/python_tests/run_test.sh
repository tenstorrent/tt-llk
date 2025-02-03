#!/bin/bash

# Check if CHIP_ARCH is set to wormhole or blackhole
if [[ "$CHIP_ARCH" = "wormhole" ]]; then
    /home/software/syseng/wh/tt-smi -wr 0
elif [[ "$CHIP_ARCH" = "blackhole" ]]; then
    tt-smi -r 0
else
    echo "No architecture detected"
fi

cd .. && make clean && cd python_tests
rm -rf *.log

# Function to display usage instructions
usage() {
    echo "Usage: $0 --repeat <number_of_repeats> --test <test_name> [--log <log_file>]"
    echo "       $0 --all "
    exit 1
}

# Parse command-line arguments
log_file="test_results.log"  # Default log file
while [[ $# -gt 0 ]]; do
    case "$1" in
        --repeat)
            repeat_count="$2"
            shift 2
            ;;
        --test)
            test_name="$2"
            shift 2
            ;;
        --all)
            all_tests=true
            shift
            ;;
        --log)
            log_file="$2"
            shift 2
            ;;
        *)
            usage
            ;;
    esac
done

if [[ "$all_tests" = true ]]; then
    echo "Running all tests in current directory."
    test_files=$(ls test_*.py)

    for test_file in $test_files; do
        echo "Running $test_file"
        pytest --color=yes -rA "$test_file"
        tt-smi -r 0
    done
    
    exit 0
fi


# Ensure both parameters (--repeat and --test) are provided if not running all tests
if [[ -z "$repeat_count" || -z "$test_name" ]]; then
    usage
fi

pass_count=0
fail_count=0

# Run the test for the specified number of iterations
for i in $(seq 1 "$repeat_count"); do
    if [[ -n "$log_file" ]]; then
        echo "Running test: $test_name (Iteration $i)" | tee -a "$log_file"
        pytest "$test_name.py" | tee -a "$log_file" 2>&1
    else
        echo "Running test: $test_name (Iteration $i)"
        pytest "$test_name.py"
    fi

    result=$?
    
    if [ $result -eq 0 ]; then
        ((pass_count++))
    else
        ((fail_count++))
    fi
done

# Report results
if [[ -n "$log_file" ]]; then
    echo "Passed: $pass_count" | tee -a "$log_file"
    echo "Failed: $fail_count" | tee -a "$log_file"
    echo "Test results saved to: $log_file"
else
    echo "Passed: $pass_count"
    echo "Failed: $fail_count"
fi