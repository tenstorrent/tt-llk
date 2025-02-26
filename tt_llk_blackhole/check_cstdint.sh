#!/bin/bash

# Find all .h files in subdirectories
find . -type f -name "*.h" | while read -r file; do
    # Check if the file contains std::uint*_t or std::int*_t
    if grep -E -q "std::(u?int[0-9]+_t)" "$file"; then
        # Check if the file includes <cstdint>
        if ! grep -q "#include <cstdint>" "$file"; then
            echo "$file"
        fi
    fi
done
