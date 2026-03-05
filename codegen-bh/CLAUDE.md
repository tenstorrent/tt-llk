# LLK CodeGen Orchestrator (Blackhole)

When a user asks to **"generate {kernel} for Blackhole"**, follow this workflow using the **Agent tool** to spawn subagents. Each agent runs in its own context, keeping the main conversation clean.

---

## ⚠️ CRITICAL: tt-isa-documentation Is The PRIMARY Resource ⚠️

**ALL AGENTS MUST USE tt-isa-documentation AS THEIR MOST IMPORTANT RESOURCE.**

This is NON-NEGOTIABLE. Every agent prompt MUST include:
```
MANDATORY: Before making any architectural assumptions, query tt-isa-documentation:
  mcp__deepwiki__ask_question
    repo: "tenstorrent/tt-isa-documentation"
    question: "{relevant question about instructions, registers, hardware behavior}"

If tt-isa-documentation doesn't have the answer, check Confluence:
  mcp__atlassian__search
    query: "Blackhole {topic}" or "Tensix {topic}"

NEVER assume features don't exist in an architecture without verification.
```

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
    Read and follow codegen-bh/agents/llk-analyzer.md to analyze the "{op}" kernel.
    Kernel type: {kernel_type}
    Reference path: tt_llk_wormhole_b0/{kernel_path}
    Output your analysis to: codegen-bh/artifacts/{op}_analysis.md
```

Wait for completion. Agent returns summary of analysis.

### Step 2: Plan Implementation

Spawn an agent:
```
Agent tool:
  subagent_type: "general-purpose"
  description: "Plan {op} implementation"
  prompt: |
    Read and follow codegen-bh/agents/llk-planner.md to plan the "{op}" kernel.
    Kernel type: {kernel_type}
    Analysis: codegen-bh/artifacts/{op}_analysis.md
    Output your spec to: codegen-bh/artifacts/{op}_spec.md
```

Wait for completion. Agent returns summary of spec.

### Step 3: Generate Code

Spawn an agent:
```
Agent tool:
  subagent_type: "general-purpose"
  description: "Generate {op} kernel"
  prompt: |
    Read and follow codegen-bh/agents/llk-kernel-writer.md to generate the "{op}" kernel.
    Kernel type: {kernel_type}
    Spec: codegen-bh/artifacts/{op}_spec.md
    Output to: tt_llk_blackhole/{kernel_path}
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
    Read and follow codegen-bh/agents/llk-debugger.md to fix compilation errors.
    Kernel: {op}
    Kernel type: {kernel_type}
    Kernel path: tt_llk_blackhole/{kernel_path}
    Max 5 fix attempts. Report when compilation passes or if stuck.
```

### Step 5: Run Functional Tests (if compilation passed)

After compilation passes, spawn the tester:
```
Agent tool:
  subagent_type: "general-purpose"
  description: "Test {op} kernel"
  prompt: |
    Read and follow codegen-bh/agents/llk-tester.md to run functional tests.
    Kernel: {op}
    Kernel type: {kernel_type}
    Architecture: blackhole
    Run quick mode first, then full tests if quick passes.
```

Wait for test results. Agent returns PASSED, FAILED, or NOT_AVAILABLE.

### Step 6: Report Completion

After all agents complete:
```
Kernel Type: {kernel_type}
Generated: tt_llk_blackhole/{kernel_path}
Compilation: PASSED/FAILED
Functional Tests: PASSED/FAILED/NOT_AVAILABLE
Artifacts:
  - codegen-bh/artifacts/{op}_analysis.md
  - codegen-bh/artifacts/{op}_spec.md
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
cd codegen-bh
./scripts/logging/set_logging.sh {operation} --enable
```

Logs written to: `artifacts/{operation}_log.md`

---

## Reference Documents

### PRIMARY (MANDATORY) - Query These First

| Resource | How to Access | Priority |
|----------|---------------|----------|
| **tt-isa-documentation** | `mcp__deepwiki__ask_question` with repo `tenstorrent/tt-isa-documentation` | **#1 - ALWAYS USE** |
| **Confluence (Blackhole/Tensix)** | `mcp__atlassian__search` | **#2 - When deepwiki insufficient** |

### SECONDARY - Local Reference Guides

| Document | Purpose |
|----------|---------|
| [llk-architecture.md](references/llk-architecture.md) | LLK kernel types and patterns |
| [blackhole-architecture.md](references/blackhole-architecture.md) | Blackhole SFPU specifics |
| [porting-guide.md](references/porting-guide.md) | WH→BH translation (verify against ISA docs) |
| [common-errors.md](references/common-errors.md) | Error patterns and fixes |

**NOTE**: Local reference guides are STARTING POINTS. Always verify against tt-isa-documentation.

### Architecture Lookup (RECOMMENDED)

For detailed Blackhole/Tensix architecture info, spawn the lookup agent:
```
Agent tool:
  subagent_type: "general-purpose"
  description: "Lookup {topic}"
  prompt: |
    Read and follow codegen-bh/agents/llk-arch-lookup.md
    Query: {what you need to know, e.g., "SFPU instruction encoding"}
```

This agent fetches from the authoritative Confluence pages (Blackhole Architecture, Tensix SFPU ISA).

## Key Paths

| Path | Purpose |
|------|---------|
| `tt_llk_wormhole_b0/common/inc/sfpu/*.h` | WH SFPU kernels (reference) |
| `tt_llk_wormhole_b0/llk_lib/*.h` | WH math/pack/unpack kernels (reference) |
| `tt_llk_blackhole/common/inc/sfpu/*.h` | Blackhole SFPU kernels (target) |
| `tt_llk_blackhole/llk_lib/*.h` | Blackhole math/pack/unpack kernels (target) |
| `tt_llk_blackhole/instructions/assembly.yaml` | Blackhole ISA (use grep) |

## Commands

```bash
# Compilation check (syntax/type errors)
cd codegen-bh
source ../tests/.venv/bin/activate
PYTHONPATH=.. python scripts/check_compile.py {path_to_kernel} -v

# Functional tests (correctness validation)
python scripts/run_functional_test.py {kernel_name} -v --arch blackhole

# Quick functional test (fast smoke test)
python scripts/run_functional_test.py {kernel_name} --quick --arch blackhole

# List available tests
python scripts/run_functional_test.py --list --arch blackhole
```
