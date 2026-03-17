# LLK Porting Guide

This guide teaches the **methodology** for porting kernels between architectures. It does NOT provide specific instruction mappings — those must be discovered from authoritative sources for each architecture pair.

## Porting Philosophy

Porting a kernel is NOT line-by-line translation. It's understanding what the reference does algorithmically, then implementing that algorithm using the target architecture's capabilities. The target may be simpler (hardware support for what the reference does in software) or more complex (missing features that need workarounds).

---

## Step 1: Understand What the Reference Does

Before looking at the target architecture at all:
1. Read the reference implementation
2. Identify the **algorithm** (mathematical operations, data flow)
3. Identify **architecture-specific constructs** (these need translation)
4. Separate the algorithm from the implementation

Example thought process:
- "This kernel computes sigmoid(x) = 1/(1+exp(-x))"
- "The reference uses a LUT-based approximation with `lut2()` calls"
- "The LUT approach is architecture-specific — the algorithm (sigmoid) is what matters"

---

## Step 2: Research the Target Architecture

Use these authoritative sources (in priority order):

### 1. Existing target implementations (highest priority)
```
tt_llk_{target_arch}/common/inc/sfpu/*.h
tt_llk_{target_arch}/llk_lib/*.h
codegen/references/golden/*.h
```
Read 2-3 existing kernels of the same type. They show you:
- What includes are needed
- What namespaces are used
- What instruction patterns work
- What register conventions exist
- What loop/iteration patterns are standard

### 2. Architecture documentation
- **Confluence**: Search for target architecture specs (ISA, register files, execution model)
- **DeepWiki**: Query `tenstorrent/tt-isa-documentation` for ISA details

### 3. ISA definition
```bash
# List all instructions matching a keyword
grep -i "{keyword}" tt_llk_{target_arch}/instructions/assembly.yaml

# Get details for a specific instruction
grep -A 30 "^{INSTRUCTION}:" tt_llk_{target_arch}/instructions/assembly.yaml

# Verify an instruction exists
grep -c "^{INSTRUCTION}:" tt_llk_{target_arch}/instructions/assembly.yaml
```

---

## Step 3: Map Constructs

For each architecture-specific construct in the reference:

1. **Does the target have a direct equivalent?**
   - Search existing target code for similar patterns
   - Check assembly.yaml for matching instructions

2. **Can it be composed from available primitives?**
   - Look at how existing target kernels handle similar operations
   - Check if multiple target instructions can achieve the same result

3. **Does it need an entirely different approach?**
   - If the reference relies on a feature the target doesn't have (e.g., LUTs, specific vector ops), redesign the algorithm using what's available
   - Check architecture docs for recommended alternatives

### Discovery method for each construct:

```bash
# Find how the target arch handles a specific concept
grep -r "{concept}" tt_llk_{target_arch}/ --include="*.h" | head -20

# Find what instructions are available for a category
grep -i "SFP" tt_llk_{target_arch}/instructions/assembly.yaml | head -30

# Check how golden examples handle it
grep -r "{pattern}" codegen/references/golden/ --include="*.h"
```

---

## Step 4: Plan Register/Resource Allocation

Study existing target implementations to discover:
- How many registers are typically used
- What naming conventions exist for register purposes
- What resources (packers, unpackers, buffer descriptors) are used and how
- Any allocation constraints

Follow the same conventions as existing code.

---

## Step 5: Write the Implementation

Match existing target code style exactly:
- Same includes
- Same namespace structure
- Same function naming pattern
- Same loop/iteration pattern
- Same comment style

---

## Common Porting Patterns

These patterns apply across many architecture pairs, but **always verify against actual code**:

### Pattern: Hardware acceleration
The target may have single-instruction support for what the reference implements in software (e.g., hardware sqrt vs. Newton-Raphson). Always check for built-in operations before porting complex algorithms.

### Pattern: API level differences
Different architectures may use different abstraction levels:
- High-level C++ operator overloading vs. explicit instruction calls
- Implicit register management vs. explicit register allocation
- Library functions vs. direct hardware instructions

### Pattern: Conditional execution
Different architectures handle per-element conditionals differently:
- SIMD-style predication
- Condition code registers
- Compile-time constexpr branching
- Hardware-supported operations (e.g., ReLU as max(0,x) in one instruction)

### Pattern: Iteration and data movement
Different architectures process data in different chunk sizes and have different mechanisms for advancing through memory.

---

## Verification

After implementing, verify:

1. **Compilation**: `python scripts/check_compile.py {kernel_file} -v`
2. **Instruction validity**: Every instruction used exists in assembly.yaml
3. **Pattern conformance**: The code matches existing target implementation patterns
4. **Functional correctness**: Run available tests

---

## Key Paths

| Path | Purpose |
|------|---------|
| `tt_llk_{arch}/common/inc/sfpu/*.h` | SFPU kernel implementations |
| `tt_llk_{arch}/llk_lib/*.h` | Math/pack/unpack kernel implementations |
| `tt_llk_{arch}/instructions/assembly.yaml` | ISA definition (use grep) |
| `codegen/references/golden/*.h` | Verified correct implementations |
| `codegen/references/common-errors.md` | Known error patterns and fixes |
