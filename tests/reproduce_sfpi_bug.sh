#!/bin/bash

rm ./*_math_elf_dump.txt
rm -rf ./tmp/tt-llk-build/ || true
echo "Running Functional test variant"
pytest ./python_tests/test_eltwise_unary_sfpu.py 1>/dev/null
./sfpi/compiler/bin/riscv-tt-elf-objdump -t -d /tmp/tt-llk-build/eltwise_unary_sfpu_test/*/elf/math.elf > ./functional_math_elf_dump.txt

rm -rf ./tmp/tt-llk-build/ || true
echo "Running Coverage test variant"
pytest --coverage ./python_tests/test_eltwise_unary_sfpu.py 1>/dev/null || true
./sfpi/compiler/bin/riscv-tt-elf-objdump -t -d /tmp/tt-llk-build/eltwise_unary_sfpu_test/*/elf/math.elf > ./coverage_math_elf_dump.txt
