# LLK CodeGen Orchestrator

When a user asks to **"generate {kernel} for Quasar"**, follow this workflow using the **Agent tool** to spawn subagents. Each agent runs in its own context, keeping the main conversation clean.

---

## Step 0: Identify Kernel Type

Determine the kernel category from the request:

| Category | Keywords | Reference Path | Target Path |
|----------|----------|----------------|-------------|
| **SFPU** | sigmoid, relu, exp, gelu, tanh, sqrt, recip | `common/inc/sfpu/ckernel_sfpu_{op}.h` | `common/inc/sfpu/ckernel_sfpu_{op}.h` |
| **Math** | matmul, reduce, eltwise, binary, unary | `llk_lib/llk_math_{op}.h` | `llk_lib/llk_math_{op}.h` |
| **Pack** | pack, untilize | `llk_lib/llk_pack_{op}.h` | `llk_lib/llk_pack_{op}.h` |
| **Unpack** | unpack, tilize | `llk_lib/llk_unpack_{op}.h` | `llk_lib/llk_unpack_{op}.h` |

---

## Workflow: Spawn Agents Sequentially

### Step 1: Analyze Reference

Spawn an agent:
```
Agent tool:
  subagent_type: "general-purpose"
  description: "Analyze {op} kernel"
  prompt: |
    Read and follow codegen/agents/llk-analyzer.md to analyze the "{op}" kernel.
    Kernel type: {kernel_type}
    Reference path: tt_llk_blackhole/{kernel_path}
    Output your analysis to: codegen/artifacts/{op}_analysis.md
```

Wait for completion. Agent returns summary of analysis.

### Step 2: Plan Implementation

Spawn an agent:
```
Agent tool:
  subagent_type: "general-purpose"
  description: "Plan {op} implementation"
  prompt: |
    Read and follow codegen/agents/llk-planner.md to plan the "{op}" kernel.
    Kernel type: {kernel_type}
    Analysis: codegen/artifacts/{op}_analysis.md
    Output your spec to: codegen/artifacts/{op}_spec.md
```

Wait for completion. Agent returns summary of spec.

### Step 3: Generate Code

Spawn an agent:
```
Agent tool:
  subagent_type: "general-purpose"
  description: "Generate {op} kernel"
  prompt: |
    Read and follow codegen/agents/llk-kernel-writer.md to generate the "{op}" kernel.
    Kernel type: {kernel_type}
    Spec: codegen/artifacts/{op}_spec.md
    Output to: tt_llk_quasar/{kernel_path}
    Run compilation check after writing.
```

Wait for completion. Agent returns compilation result (PASSED or FAILED).

### Step 4: Debug (if needed)

If Step 3 reports compilation failure, spawn debugger:
```
Agent tool:
  subagent_type: "general-purpose"
  description: "Debug {op} kernel"
  prompt: |
    Read and follow codegen/agents/llk-debugger.md to fix compilation errors.
    Kernel: {op}
    Kernel type: {kernel_type}
    Kernel path: tt_llk_quasar/{kernel_path}
    Max 5 fix attempts. Report when compilation passes or if stuck.
```

### Step 5: Run Functional Tests (if compilation passed)

After compilation passes, spawn the tester:
```
Agent tool:
  subagent_type: "general-purpose"
  model: "haiku"
  description: "Test {op} kernel"
  prompt: |
    Read and follow codegen/agents/llk-tester.md to run functional tests.
    Kernel: {op}
    Kernel type: {kernel_type}
    Run quick mode first, then full tests if quick passes.
```

Wait for test results. Agent returns PASSED, FAILED, or NOT_AVAILABLE.

### Step 6: Report Completion

After all agents complete:
```
Kernel Type: {kernel_type}
Generated: tt_llk_quasar/{kernel_path}
Compilation: PASSED/FAILED
Functional Tests: PASSED/FAILED/NOT_AVAILABLE
Artifacts:
  - codegen/artifacts/{op}_analysis.md
  - codegen/artifacts/{op}_spec.md
```

---

## Why Agents?

Each agent runs in a **separate context**:
- Main conversation stays clean
- Only summaries come back
- No context pollution from file reads, searches, iterations

---

## Logging (Optional)

To enable execution logging for debugging:
```bash
cd codegen
./scripts/logging/set_logging.sh {operation} --enable
```

Logs written to: `artifacts/{operation}_log.md`

---

## Reference Documents

| Document | Purpose |
|----------|---------|
| [llk-architecture.md](references/llk-architecture.md) | LLK kernel types and patterns |
| [quasar-architecture.md](references/quasar-architecture.md) | Quasar ISA, TTI instructions |
| [porting-guide.md](references/porting-guide.md) | BH→Quasar translation |
| [common-errors.md](references/common-errors.md) | Error patterns and fixes |

### Architecture Lookup (Optional)

For detailed Quasar/NEO architecture info, spawn the lookup agent:
```
Agent tool:
  subagent_type: "general-purpose"
  description: "Lookup {topic}"
  prompt: |
    Read and follow codegen/agents/llk-arch-lookup.md
    Query: {what you need to know, e.g., "SFPU instruction encoding"}
```

This agent fetches from the authoritative Confluence page (Tensix NEO High Level Specification).

## Key Paths

| Path | Purpose |
|------|---------|
| `tt_llk_blackhole/common/inc/sfpu/*.h` | BH SFPU kernels |
| `tt_llk_blackhole/llk_lib/*.h` | BH math/pack/unpack kernels |
| `tt_llk_quasar/common/inc/sfpu/*.h` | Quasar SFPU kernels |
| `tt_llk_quasar/llk_lib/*.h` | Quasar math/pack/unpack kernels |
| `tt_llk_quasar/instructions/assembly.yaml` | Quasar ISA (use grep) |

## Commands

```bash
# Compilation check (syntax/type errors)
cd codegen
source ../tests/.venv/bin/activate
PYTHONPATH=.. python scripts/check_compile.py {path_to_kernel} -v

# Functional tests (correctness validation)
python scripts/run_functional_test.py {kernel_name} -v

# Quick functional test (fast smoke test)
python scripts/run_functional_test.py {kernel_name} --quick

# List available tests
python scripts/run_functional_test.py --list
```
