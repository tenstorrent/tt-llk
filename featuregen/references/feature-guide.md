# Feature Addition Guide

This guide describes how to think about adding features to Blackhole SFPU kernels. It covers methodology, not architecture specifics.

## What "Adding a Feature" Means

Features in SFPU kernels typically take one of these forms:

### 1. New Computation Mode (most common)
A new way to compute the same operation — faster but less accurate, or slower with more precision, or numerically equivalent but with different edge-case handling.

**Pattern:** New template parameter `bool FAST_MODE` or `bool APPROXIMATION_MODE`
**Structure:**
```cpp
template <bool APPROX>
void _calculate_foo_(const int iterations) {
    // ... existing setup ...
    for (int d = 0; d < iterations; d++) {
        // ... load ...
        if constexpr (APPROX) {
            // fast path
        } else {
            // accurate path
        }
        // ... store ...
    }
}
```

### 2. New Feature Flag via #ifdef
A compile-time opt-in that changes behavior for specific builds.

**Pattern:** `#ifdef SFPU_OP_PARAM_RELU` or similar
**Use case:** When the feature is so different it warrants a separate code path

### 3. New Parameter to Existing Computation
Add a runtime or compile-time parameter that modifies the operation (threshold value, clamp bounds, etc.).

**Pattern:** New template parameter carrying a value, or loaded via `SFPLOADI`

### 4. Extended Range / Data Type Support
Support a new data format (BF16, FP32, INT8) in a kernel that previously only handled one type.

**Pattern:** Template parameter `DTYPE` or `bool SFPU_RELU_INT8`

## Principles for Feature Additions

### Be Surgical
Modify only the code that needs to change. Do not reorganize, rename, or "improve" anything not directly related to the feature.

### Preserve Existing Behavior
The default behavior (existing template parameters at their default values) must be identical after the feature addition. All existing callers must continue to work.

### Match Existing Patterns
Find a BH kernel that already has a similar feature and replicate the exact pattern. Don't invent new patterns.

### Test Both Paths
If the feature adds a new code path, both the old path and the new path must be tested. Compilation testing verifies syntax; functional testing verifies correctness.

## Common Feature Mistakes

| Mistake | Consequence | Prevention |
|---------|-------------|------------|
| Changing default template argument | Breaks existing callers | Keep existing defaults; new params must default to old behavior |
| Wrong register in new SFPMAD | Silently wrong numerical output | Read working SFPMAD uses and match arg order |
| Missing SFPSETCC before predicated block | Wrong values computed | Always check: does the conditional block need a predicate setup? |
| Off-by-one in SFPLOADI constant | Wrong constant loaded | Verify constant encoding from working code that loads similar values |
| New `if constexpr` breaks loop structure | Compile error or logic error | Ensure loop variables and setup are still valid for both branches |

## Iteration Loop Structure

Most BH SFPU kernels follow this pattern:
```cpp
for (int d = 0; d < iterations; d++) {
    TTI_SFPLOAD(p0, 0, ADDR_MOD_7, 0);    // load
    // ... computation ...
    TTI_SFPSTORE(p0, 0, ADDR_MOD_7, 0);   // store
    TTI_INCRWC(0, 2, 0, 0);               // advance
}
```

When adding a feature that changes the computation, only the inner computation changes — the load/store/advance structure stays the same.

## Before Writing Code

1. Read the target kernel fully
2. Read 2 similar kernels that have a similar feature
3. Understand the pattern
4. Write the plan with exact before/after code
5. Only then implement

The quality of the plan determines the quality of the implementation.
