---
name: bh-debug
description: Fix compilation and runtime errors in Blackhole LLK kernels. Use when a kernel fails to compile or tests fail.
user_invocable: true
---

# /bh-debug — Debug a Blackhole LLK Kernel

Spawn the `bh-debugger` agent to fix compilation or runtime errors in the given kernel.

## Usage

```
/bh-debug <kernel_path>
/bh-debug tt_llk_blackhole/llk_lib/llk_pack_untilize.h
/bh-debug tt_llk_blackhole/common/inc/sfpu/ckernel_sfpu_gelu.h
```

## What to Do

1. Parse the kernel path from the user's arguments (or ask if not provided)
2. Determine the kernel type from the path:
   - `common/inc/sfpu/` → sfpu
   - `llk_lib/llk_math_` → math
   - `llk_lib/llk_pack_` → pack
   - `llk_lib/llk_unpack_` → unpack
3. Spawn the **bh-debugger** agent:

```
Agent tool:
  subagent_type: "bh-debugger"
  description: "Debug {kernel_name}"
  prompt: |
    Kernel path: {kernel_path}
    Kernel type: {kernel_type}
    Max 5 fix attempts.
```

4. Report the agent's results to the user
