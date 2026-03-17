# Common Compilation Errors and Fixes

This document catalogs error patterns observed during kernel development. It is a **starting point** for debugging — always verify fixes against the actual codebase and architecture docs.

## How to Use This Document

1. Match your error against the patterns below
2. Try the suggested fix
3. If the fix doesn't work, **investigate dynamically**:
   ```bash
   # Search existing working code for correct usage of a symbol
   grep -r "{symbol}" tt_llk_{target_arch}/ --include="*.h" | head -20

   # Check golden examples
   grep -r "{symbol}" codegen/references/golden/ --include="*.h"

   # Look up instruction details
   grep -A 20 "^{INSTRUCTION}:" tt_llk_{target_arch}/instructions/assembly.yaml
   ```

---

## Error Categories

### 1. Undeclared Symbol

**Pattern**: `'X' was not declared in this scope`

**Common causes**:
- Wrong instruction/function name
- Missing include file
- Wrong namespace

**Investigation**:
```bash
# Find where the symbol IS used correctly
grep -r "X" tt_llk_{target_arch}/ --include="*.h" -l
# Check what includes that file uses
head -20 {file_that_uses_X}
```

---

### 2. Not a Member of Namespace

**Pattern**: `'X' is not a member of 'Y'`

**Common causes**:
- Wrong namespace prefix for the symbol
- Symbol exists in a different namespace

**Investigation**:
```bash
# Find the correct namespace for the symbol
grep -r "X" tt_llk_{target_arch}/ --include="*.h" | grep -v "^Binary"
```

---

### 3. Wrong Argument Count

**Pattern**: `too many arguments to function 'X'` or `too few arguments`

**Investigation**:
```bash
# Check the function/macro signature
grep -A 5 "X" tt_llk_{target_arch}/instructions/assembly.yaml
# Or find usage in existing code
grep -r "X(" tt_llk_{target_arch}/ --include="*.h" | head -10
```

---

### 4. Missing Include

**Pattern**: Various symbols undeclared that should be available

**Investigation**:
```bash
# Check what includes similar kernels use
head -10 tt_llk_{target_arch}/common/inc/sfpu/ckernel_sfpu_*.h
# Or for non-SFPU
head -10 tt_llk_{target_arch}/llk_lib/llk_math_*.h
```

---

### 5. Deprecated/Renamed Instruction

**Pattern**: Instruction exists on reference arch but not on target

**Investigation**:
```bash
# Verify the instruction doesn't exist
grep -c "^{INSTRUCTION}:" tt_llk_{target_arch}/instructions/assembly.yaml

# Search for similar instruction names
grep -i "{partial_name}" tt_llk_{target_arch}/instructions/assembly.yaml

# Check how existing code handles this operation
grep -r "{operation_concept}" tt_llk_{target_arch}/ --include="*.h"
```

---

## Debugging Tips

1. **Start with the compiler suggestion** — "did you mean X?" is usually correct
2. **Compare with working code** — the most reliable fix source is existing implementations
3. **One fix at a time** — change one thing, recompile, check
4. **Structural vs. superficial** — if individual fixes keep failing, the problem may be structural (wrong includes, wrong namespace setup, wrong function pattern). Compare the full file against a similar working kernel.
5. **Check the spec** — the error may trace back to a wrong instruction mapping in the spec
