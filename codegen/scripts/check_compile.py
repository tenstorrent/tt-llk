#!/usr/bin/env python3
# SPDX-FileCopyrightText: (c) 2026 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0

"""
Check LLK kernel compilation.

Usage:
    # Check a specific file (from codegen/ directory)
    PYTHONPATH=.. python scripts/check_compile.py ../tt_llk_quasar/common/inc/sfpu/ckernel_sfpu_sigmoid.h

    # Check with custom function names
    PYTHONPATH=.. python scripts/check_compile.py my_kernel.h --func _calculate_foo_ --init _init_foo_
"""

import argparse
import sys
from pathlib import Path

# Add parent to path
sys.path.insert(0, str(Path(__file__).parent.parent.parent))

from codegen.agents.compile_agent import CompileAgent, CompileResult


def create_wrapper(filename: str, func_name: str, init_name: str | None) -> str:
    """Create a test wrapper for the given function."""
    init_calls = ""
    if init_name:
        init_calls = f"""
    void force_compile_init() {{
        {init_name}<true>();
        {init_name}<false>();
    }}"""

    return f"""// Auto-generated compile test wrapper
#include "ckernel_trisc_common.h"
#include "cmath_common.h"
#include "{filename}"

using namespace ckernel;
using namespace ckernel::sfpu;

namespace {{
    void force_compile() {{
        {func_name}<true>(16);
        {func_name}<false>(16);
    }}
{init_calls}
}}
"""


def check_file(
    filepath: Path,
    arch: str = "quasar",
    func_name: str | None = None,
    init_name: str | None = None,
) -> CompileResult:
    """Check that an LLK kernel file compiles."""
    agent = CompileAgent(arch=arch)

    # Read the file
    code = filepath.read_text()

    # Infer function names from filename if not provided
    if func_name is None:
        # ckernel_sfpu_sigmoid.h -> _calculate_sigmoid_
        stem = filepath.stem  # ckernel_sfpu_sigmoid
        op_name = stem.replace("ckernel_sfpu_", "")  # sigmoid
        func_name = f"_calculate_{op_name}_"
        init_name = f"_init_{op_name}_"

    # Override the wrapper creation
    original_wrapper = agent._create_wrapper
    agent._create_wrapper = lambda code, filename: create_wrapper(
        filename, func_name, init_name
    )

    # Compile
    result = agent.compile(code, filepath.name)

    return result


def main():
    parser = argparse.ArgumentParser(description="Check LLK kernel compilation")
    parser.add_argument("file", type=Path, help="Path to LLK header file")
    parser.add_argument(
        "--arch", default="quasar", choices=["quasar", "blackhole", "wormhole"]
    )
    parser.add_argument("--func", help="Main function name (e.g., _calculate_sigmoid_)")
    parser.add_argument("--init", help="Init function name (e.g., _init_sigmoid_)")
    parser.add_argument(
        "--verbose", "-v", action="store_true", help="Show full error output"
    )
    args = parser.parse_args()

    if not args.file.exists():
        print(f"Error: File not found: {args.file}")
        sys.exit(1)

    print(f"Checking: {args.file}")
    print(f"Architecture: {args.arch}")

    result = check_file(args.file, args.arch, args.func, args.init)

    if result.success:
        print("✓ Compilation successful!")
        sys.exit(0)
    else:
        print(f"✗ Compilation failed ({len(result.errors)} errors)")
        print()

        if args.verbose:
            print("Full compiler output:")
            print("-" * 60)
            print(result.stderr)
        else:
            print("Errors:")
            for err in result.errors[:10]:
                if err.line:
                    print(f"  Line {err.line}: {err.message}")
                else:
                    print(f"  {err.message}")

            if len(result.errors) > 10:
                print(f"  ... and {len(result.errors) - 10} more")
                print()
                print("Use --verbose for full output")

        sys.exit(1)


if __name__ == "__main__":
    main()
