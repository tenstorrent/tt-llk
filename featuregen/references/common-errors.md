# Common Errors in Blackhole Feature Additions

## Compilation Errors

### "error: 'SFPXXX' was not declared in this scope"
**Cause:** Wrong instruction name.
**Fix:** Search for the correct name in working BH code:
```bash
grep -rn "SFPXXX\|TTI_SFPXXX" tt_llk_blackhole/common/inc/sfpu/*.h | head -10
```
BH instructions may use `TTI_SFPXXX` prefix or just `SFPXXX` — check which form is used consistently.

### "error: no matching function for call to 'function_name'"
**Cause:** Wrong number/type of arguments, or calling with wrong template arguments.
**Fix:** Find the actual function signature in working code:
```bash
grep -rn "void function_name\|function_name<" tt_llk_blackhole/common/inc/ | head -10
```

### "error: template argument N is invalid"
**Cause:** New template parameter added in wrong position, or wrong type.
**Fix:** Check how the template is instantiated in callers. New params must go at the END of the param list so existing callers (which don't pass the new param) still work via defaults.

### "error: expected primary-expression before '>' token"
**Cause:** Template syntax error — usually a missing `template` keyword when calling a templated method from a template context.
**Fix:** Add `template` before the method call: `obj.template method<T>()` or restructure.

### "error: 'constexpr if' is not supported in this language standard"
**Cause:** Wrong C++ standard. Shouldn't happen (code uses C++17) but check if accidentally using C-style syntax.

### Missing braces after `if constexpr`
**Cause:** Added a new `if constexpr` block without proper braces — if the new block is only one statement, it might "steal" the else of an existing block.
**Fix:** Always use `{}` for all `if constexpr` branches, even single-statement ones.

---

## Functional Test Failures

### All outputs are zero
**Cause:** SFPSTORE not reached, or SFPLOAD loading from wrong address, or register overwritten before store.
**Investigation:** Check if the new code path conditionally skips the store.

### Wrong numerical result (consistent offset)
**Cause:** Wrong constant in SFPLOADI — loaded constant has wrong value or wrong encoding.
**Fix:** Find a working kernel that loads a similar constant and compare the encoding.

### NaN output
**Cause:** Division by zero, log of negative, or overflow in new computation path.
**Fix:** Add bounds checking matching what similar kernels do (e.g., max with small epsilon before log/recip).

### Wrong result only for specific inputs
**Cause:** Conditional branch bug — the condition for entering the new path is wrong.
**Fix:** Check the SFPSETCC usage. Compare with working kernels that use the same condition.

### Test passes in quick mode but fails in full mode
**Cause:** Edge case inputs (very small, very large, denormals, ±0, ±inf) hit the new code path differently.
**Fix:** Read the test input values from the test output and trace the computation manually.

---

## Investigation Commands

```bash
# Find all uses of a specific instruction
grep -rn "TTI_SFPLOAD\|SFPLOAD" tt_llk_blackhole/common/inc/sfpu/*.h | head -20

# Find all kernels with a specific template parameter pattern
grep -rn "APPROXIMATION_MODE\|FAST_MODE\|bool.*MODE" tt_llk_blackhole/common/inc/sfpu/*.h | head -20

# Find constant loading patterns
grep -rn "SFPLOADI" tt_llk_blackhole/common/inc/sfpu/ckernel_sfpu_gelu.h

# Compare similar implementations
grep -n "SFPMAD\|SFPNOP\|SFPIADD" tt_llk_blackhole/common/inc/sfpu/ckernel_sfpu_{kernel}.h
```
