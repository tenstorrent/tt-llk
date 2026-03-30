---
name: bh-test
description: Run LLK tests for Blackhole kernels. Creates phase tests, runs via run_test.sh, classifies failures.
user_invocable: true
---

# /bh-test — Test a Blackhole LLK Kernel

Spawn the `bh-tester` agent to run tests for the given kernel.

## Usage

```
/bh-test <kernel_name> [--phase N] [--quick]
/bh-test sigmoid
/bh-test pack_untilize --phase 1
/bh-test unpack_tilize --quick
```

## What to Do

1. Parse the kernel name and options from the user's arguments
2. Determine the kernel type and path:
   - SFPU: `tt_llk_blackhole/common/inc/sfpu/ckernel_sfpu_{op}.h`
   - Math: `tt_llk_blackhole/llk_lib/llk_math_{op}.h`
   - Pack: `tt_llk_blackhole/llk_lib/llk_pack_{op}.h`
   - Unpack: `tt_llk_blackhole/llk_lib/llk_unpack_{op}.h`
3. Spawn the **bh-tester** agent:

```
Agent tool:
  subagent_type: "bh-tester"
  description: "Test {kernel_name}"
  prompt: |
    Kernel: {kernel_name}
    Kernel type: {kernel_type}
    Kernel path: {kernel_path}
    Architecture: blackhole
    Phase: {N or "all"}
```

4. Report the agent's results to the user
5. If tests failed, suggest `/bh-debug {kernel_path}` as next step
