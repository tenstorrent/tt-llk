# Blackhole Architecture Sources

This file lists where to find authoritative information about the Blackhole architecture. Do NOT hardcode architectural knowledge — always read from these sources.

## Source Priority Order

1. **Existing Blackhole implementations** — `tt_llk_blackhole/common/inc/sfpu/*.h`
   - Primary source of truth for code patterns, instruction usage, and conventions
   - If a working kernel uses an instruction a certain way, that's correct

2. **Wormhole reference implementations** — `tt_llk_wormhole_b0/common/inc/sfpu/*.h`
   - Older architecture, useful for understanding algorithmic intent
   - Code patterns differ, but the math and feature structure are similar

3. **DeepWiki** — `tenstorrent/tt-isa-documentation`
   - ISA documentation including Blackhole instruction set
   - Use `mcp__deepwiki__ask_question` for specific questions

4. **Blackhole ISA file** (if present):
   ```bash
   find tt_llk_blackhole/ -name "assembly.yaml" 2>/dev/null
   ```
   Use grep to search — do NOT read the whole file.

## Key Locations

| What you need | Where to look |
|---------------|---------------|
| SFPU kernel patterns | `tt_llk_blackhole/common/inc/sfpu/ckernel_sfpu_*.h` |
| Common includes / types | `tt_llk_blackhole/common/inc/` |
| LLK library functions | `tt_llk_blackhole/llk_lib/` |
| Architecture-specific | `tt_llk_blackhole/common/inc/hw_specific/` (if exists) |
| Template parameter patterns | Read any 2-3 SFPU kernels |
| Mode/variant patterns | `ckernel_sfpu_relu.h`, `ckernel_sfpu_gelu.h`, `ckernel_sfpu_clamp.h` |
| Conditional computation | `ckernel_sfpu_comp.h`, `ckernel_sfpu_threshold.h` |
| Multiple iteration patterns | `ckernel_sfpu_exp.h`, `ckernel_sfpu_tanh.h` |

## Searching Effectively

```bash
# Find kernels using a specific instruction
grep -l "SFPNONLINEAR\|SFPMAD\|SFPIADD" tt_llk_blackhole/common/inc/sfpu/*.h

# Find how a specific function is called
grep -rn "function_name(" tt_llk_blackhole/common/inc/sfpu/*.h | head -1000

# Find all template parameter patterns
grep -n "template" tt_llk_blackhole/common/inc/sfpu/ckernel_sfpu_relu.h

# Find #define flags used in BH SFPU
grep -rn "#ifdef\|#ifndef\|#define" tt_llk_blackhole/common/inc/sfpu/*.h | grep -v "SPDX" | head -1000
```
