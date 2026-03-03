#!/usr/bin/env python3
# SPDX-FileCopyrightText: (c) 2026 Tenstorrent AI ULC
#
# SPDX-License-Identifier: Apache-2.0

"""
Sigmoid generation experiment - E2E test of the codegen pipeline.

This script:
1. Provides hardcoded context (BH sigmoid + Quasar patterns)
2. Calls Claude to generate Quasar sigmoid
3. Attempts to compile
4. Loops with error feedback if compilation fails

Usage:
    cd tt-llk/codegen
    python -m experiments.sigmoid_test
"""

import sys
from pathlib import Path

# Add parent to path for imports
sys.path.insert(0, str(Path(__file__).parent.parent.parent))

from codegen.agents.compile_agent import CompileAgent
from codegen.config.settings import settings
from codegen.generator.claude_client import ClaudeClient

# Reference: Blackhole sigmoid implementation
BLACKHOLE_SIGMOID = """
// SPDX-FileCopyrightText: (c) 2025 Tenstorrent AI ULC
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "ckernel_sfpu_load_config.h"
#include "sfpi.h"

namespace ckernel
{
namespace sfpu
{

template <bool APPROXIMATION_MODE, int ITERATIONS>
inline void _calculate_sigmoid_(const int iterations)
{
    constexpr int lut_mode = 0; // SFPLUTFP32_MOD0_FP16_6ENTRY_TABLE1
    sfpi::vUInt l0         = sfpi::l_reg[sfpi::LRegs::LReg0];
    sfpi::vUInt l1         = sfpi::l_reg[sfpi::LRegs::LReg1];
    sfpi::vUInt l2         = sfpi::l_reg[sfpi::LRegs::LReg2];
    sfpi::vUInt l4         = sfpi::l_reg[sfpi::LRegs::LReg4];
    sfpi::vUInt l5         = sfpi::l_reg[sfpi::LRegs::LReg5];
    sfpi::vUInt l6         = sfpi::l_reg[sfpi::LRegs::LReg6];

#pragma GCC unroll 8
    for (int d = 0; d < iterations; d++)
    {
        sfpi::vFloat val = sfpi::dst_reg[0];
        sfpi::dst_reg[0] = lut2(val, l0, l1, l2, l4, l5, l6, lut_mode) + 0.5f;
        sfpi::dst_reg++;
    }

    sfpi::l_reg[sfpi::LRegs::LReg0] = l0;
    sfpi::l_reg[sfpi::LRegs::LReg1] = l1;
    sfpi::l_reg[sfpi::LRegs::LReg2] = l2;
    sfpi::l_reg[sfpi::LRegs::LReg4] = l4;
    sfpi::l_reg[sfpi::LRegs::LReg5] = l5;
    sfpi::l_reg[sfpi::LRegs::LReg6] = l6;
}

template <bool APPROXIMATION_MODE>
inline void _init_sigmoid_()
{
    // Using a 6 piece LUT to calculate and model sigmoid directly
    // x <= 0.5 --> 0.2452x + (-0.0004997)
    // x <= 1.0 --> 0.2173x + 0.0152
    // x <= 1.5 --> 0.1731x + 0.05988
    // x <= 2.0 --> 0.1262x + 0.1298
    // x <= 4.0 --> 0.0485x + 0.2998
    // x >  4.0 --> 0.4998

    // imm0[15:0] = A0=0.2452 = 0x33D9 -- imm0[31:16] = A1=0.2173 = 0x32F4
    _sfpu_load_imm32_(0, 0x32F433D9);
    // imm4[15:0] = B0= -0.0004997  = 0x9018 -- imm4[31:16] = B1= 0.0152 = 0x23c8
    _sfpu_load_imm32_(4, 0x23C89018);

    // imm1[15:0] = A2=0.1731 = 0x318a -- imm1[31:16] = A3=0.1262 = 0x300a
    _sfpu_load_imm32_(1, 0x300A318A);
    // imm5[15:0] = B2=0.05988 = 0x2BAA -- imm5[31:16] = B3=0.1298 = 0x3027
    _sfpu_load_imm32_(5, 0x30272BAA);

    // imm2[15:0] = A4=0.0485 = 0x2A35 -- imm2[31:16] = A5=0.0 = 0x7C00
    _sfpu_load_imm32_(2, 0x7C002A35);
    // imm6[15:0] = B4=0.2998 = 0x34CC -- imm6[31:16] = B5=0.4998 = 0x37ff
    _sfpu_load_imm32_(6, 0x37ff34CC);
}

} // namespace sfpu
} // namespace ckernel
"""

# Reference: Quasar relu implementation (shows the pattern to follow)
QUASAR_RELU = """
// SPDX-FileCopyrightText: (c) 2025 Tenstorrent AI ULC
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "ckernel_trisc_common.h"
#include "cmath_common.h"

namespace ckernel
{
namespace sfpu
{
// Calculates RELU for number of rows of output SFPU ops (Quasar = 2 rows)
inline void _calculate_relu_sfp_rows_()
{
    TTI_SFPLOAD(p_sfpu::LREG0, p_sfpu::sfpmem::DEFAULT, ADDR_MOD_7, 0, 0);
    TTI_SFPNONLINEAR(p_sfpu::LREG0, p_sfpu::LREG1, p_sfpnonlinear::RELU_MODE);
    TTI_SFPSTORE(p_sfpu::LREG1, 0, ADDR_MOD_7, 0, 0);
}

inline void _calculate_relu_(const int iterations)
{
#pragma GCC unroll 8
    for (int d = 0; d < iterations; d++)
    {
        _calculate_relu_sfp_rows_();
        ckernel::math::_incr_counters_<0x0, 0x0, ckernel::math::SFP_ROWS, 0x0>();
    }
}

} // namespace sfpu
} // namespace ckernel
"""

# Reference: Quasar exp implementation (shows APPROXIMATION_MODE pattern)
QUASAR_EXP = """
// SPDX-FileCopyrightText: (c) 2025 Tenstorrent AI ULC
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "ckernel_trisc_common.h"
#include "cmath_common.h"

namespace ckernel
{
namespace sfpu
{
template <bool APPROXIMATION_MODE>
inline void _calculate_exp_sfp_rows_()
{
    TTI_SFPLOAD(p_sfpu::LREG0, p_sfpu::sfpmem::DEFAULT, ADDR_MOD_7, 0, 0);

    if constexpr (APPROXIMATION_MODE)
    {
        TTI_SFPNONLINEAR(p_sfpu::LREG0, p_sfpu::LREG1, p_sfpnonlinear::EXP_MODE);
    }

    TTI_SFPSTORE(p_sfpu::LREG1, 0, ADDR_MOD_7, 0, 0);
}

template <bool APPROXIMATION_MODE>
inline void _calculate_exp_(const int iterations)
{
#pragma GCC unroll 8
    for (int d = 0; d < iterations; d++)
    {
        _calculate_exp_sfp_rows_<APPROXIMATION_MODE>();
        ckernel::math::_incr_counters_<0x0, 0x0, ckernel::math::SFP_ROWS, 0x0>();
    }
}

} // namespace sfpu
} // namespace ckernel
"""

SYSTEM_PROMPT = """You are an expert in Tenstorrent Low-Level Kernel (LLK) development.

You are porting SFPU kernels from Blackhole architecture to Quasar architecture.

KEY DIFFERENCES between Blackhole and Quasar:
1. Blackhole uses sfpi:: namespace with high-level abstractions (dst_reg, l_reg, lut2)
2. Quasar uses TTI_* macros for direct instruction emission
3. Quasar processes 2 rows at a time (SFP_ROWS)
4. Quasar uses ckernel::math::_incr_counters_ for dest_reg advancement

QUASAR PATTERNS TO FOLLOW:
- Include: ckernel_trisc_common.h, cmath_common.h
- Use TTI_SFPLOAD to load from dest register
- Use TTI_SFPNONLINEAR for nonlinear operations (check available modes)
- Use TTI_SFPSTORE to store back to dest register
- Use p_sfpu:: constants for register references
- Use ADDR_MOD_7 for address modes

IMPORTANT: For sigmoid on Quasar, you may need to:
- Check if p_sfpnonlinear::SIGMOID_MODE exists
- Or implement using available primitives (may need multiple instructions)
- Or use a LUT-based approach if hardware supports it

Generate complete, compilable code with proper headers and namespace."""


def create_generation_prompt() -> str:
    """Create the prompt for sigmoid generation."""
    return f"""
## Task

Port the sigmoid SFPU kernel from Blackhole to Quasar architecture.

## Blackhole Reference Implementation

```cpp
{BLACKHOLE_SIGMOID}
```

## Quasar Pattern Examples

### Relu (simple nonlinear)
```cpp
{QUASAR_RELU}
```

### Exp (with APPROXIMATION_MODE template)
```cpp
{QUASAR_EXP}
```

## Requirements

1. Generate a Quasar sigmoid implementation following the Quasar patterns
2. Include proper SPDX license header
3. Use the same namespace structure: ckernel::sfpu
4. Implement both _calculate_sigmoid_ and _init_sigmoid_ functions
5. Use TTI_* macros appropriate for Quasar
6. Handle the APPROXIMATION_MODE template parameter

## Output

Generate the complete ckernel_sfpu_sigmoid.h file for Quasar.
"""


def run_experiment(max_iterations: int = 5, verbose: bool = True) -> bool:
    """
    Run the sigmoid generation experiment.

    Returns:
        True if generation succeeded, False otherwise
    """
    print("=" * 60)
    print("LLK CodeGen Experiment: Sigmoid for Quasar")
    print("=" * 60)

    # Check API key
    if not settings.anthropic_api_key:
        print("ERROR: ANTHROPIC_API_KEY not set")
        return False

    # Initialize components
    client = ClaudeClient()
    compiler = CompileAgent(arch="quasar")

    prompt = create_generation_prompt()

    print(f"\nUsing model: {client.model}")
    print(f"Max iterations: {max_iterations}")
    print(f"Build dir: {compiler.build_dir}")

    code = None
    for iteration in range(max_iterations):
        print(f"\n--- Iteration {iteration + 1}/{max_iterations} ---")

        # Generate or regenerate code
        if code is None:
            print("Generating initial code...")
            result = client.generate_code(prompt, system_prompt=SYSTEM_PROMPT)
        else:
            print("Regenerating with error feedback...")
            result = client.generate_with_feedback(
                prompt=prompt,
                previous_code=code,
                error_message=compile_result.stderr,
                system_prompt=SYSTEM_PROMPT,
            )

        code = result.code
        print(f"Tokens used: {result.usage}")

        if verbose:
            print("\nGenerated code preview (first 30 lines):")
            print("-" * 40)
            for i, line in enumerate(code.split("\n")[:30]):
                print(f"{i+1:3}: {line}")
            print("-" * 40)

        # Try to compile
        print("\nCompiling...")
        compile_result = compiler.compile(code, "ckernel_sfpu_sigmoid.h")

        if compile_result.success:
            print("SUCCESS! Code compiled successfully.")
            print(f"Object file: {compile_result.object_path}")

            # Save the successful code
            output_path = (
                settings.build_dir / "quasar" / "ckernel_sfpu_sigmoid_generated.h"
            )
            output_path.write_text(code)
            print(f"Saved to: {output_path}")

            return True
        else:
            print(f"FAILED: {len(compile_result.errors)} errors")
            if verbose:
                print("\nCompiler output:")
                print(compile_result.stderr[:2000])  # Limit output

            if compile_result.errors:
                print("\nParsed errors:")
                for err in compile_result.errors[:5]:
                    print(f"  Line {err.line}: {err.message}")

    print(f"\nFailed to generate compilable code after {max_iterations} iterations")
    return False


if __name__ == "__main__":
    import argparse

    parser = argparse.ArgumentParser(description="Run sigmoid generation experiment")
    parser.add_argument(
        "--max-iterations", type=int, default=5, help="Max feedback loop iterations"
    )
    parser.add_argument("--quiet", action="store_true", help="Less verbose output")
    args = parser.parse_args()

    success = run_experiment(
        max_iterations=args.max_iterations,
        verbose=not args.quiet,
    )

    sys.exit(0 if success else 1)
