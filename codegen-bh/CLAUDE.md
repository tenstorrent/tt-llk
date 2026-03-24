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

## ⚠️ CRITICAL: Incremental Phase-Based Generation ⚠️

**Kernels MUST be generated incrementally, one sub-kernel at a time.**

Most kernel files contain multiple sub-kernels (e.g., a basic variant, a dual-input variant, a fast/optimized variant). Each sub-kernel is a group of related functions (init, main, uninit, mop_config) that form a logical unit.

**The rule**: Write one sub-kernel → compile → test → only proceed to the next sub-kernel when the current one passes. Never write the entire file at once.

**Why**: A single wrong architectural assumption in a monolithic write poisons 400+ lines with no working baseline. Incremental phases give test feedback early, keep blast radius small, and give agents a working foundation to build on.

---

## Step 0: Setup Metrics Logging

Before starting, create a unique log directory for this run:

```bash
RUN_ID=$(date +%Y-%m-%d)_{kernel}_{arch}_$(head -c 4 /dev/urandom | xxd -p)
LOG_DIR=/proj_sw/user_dev/$(whoami)/codegen-metrics/logs/$RUN_ID
mkdir -p $LOG_DIR/instructions
```

Copy the agent playbooks used (snapshot of instructions):
```bash
cp codegen-bh/agents/llk-analyzer.md $LOG_DIR/instructions/
cp codegen-bh/agents/llk-planner.md $LOG_DIR/instructions/
cp codegen-bh/agents/llk-kernel-writer.md $LOG_DIR/instructions/
cp codegen-bh/agents/llk-tester.md $LOG_DIR/instructions/
cp codegen-bh/agents/llk-debugger.md $LOG_DIR/instructions/
```

Pass `LOG_DIR` to every agent prompt so they can self-log their reasoning.

After completion (Step 5), append a line to `/proj_sw/user_dev/$USER/codegen-metrics/runs.jsonl` with the run summary (kernel, arch, date, duration, cost, agents used, debug cycles, test results, status, obstacle, log path).

---

## Step 0b: Identify Kernel Type

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
    Target path: tt_llk_blackhole/{kernel_path}
    Output your analysis to: codegen-bh/artifacts/{op}_analysis.md

    CRITICAL: Before reading the WH reference, you MUST read the BH integration points:
    1. Test harness: Find and read tests/sources/*{op}*.cpp (look for #ifdef ARCH_BLACKHOLE)
    2. Parent file: Read the BH file that #includes this kernel (e.g., tt_llk_blackhole/llk_lib/llk_{type}.h)
    3. Closest existing BH kernel: Read the most similar existing BH kernel of this type line-by-line
    Document the BH-expected API (function signatures, template params, BH-only features, WH-only features to drop).

    CRITICAL: You MUST identify sub-kernel phases in your analysis. See the
    "Sub-Kernel Phases" section in llk-analyzer.md for the required output format.

    LOG_DIR: {LOG_DIR}
```

Wait for completion. Agent returns summary of analysis **including the phase plan**.

### Step 2: Extract Phase Plan

From the analyzer's output, extract the ordered list of phases. Each phase has:
- **Phase name** (a short label for the sub-kernel group)
- **Functions** (the functions belonging to this phase)
- **Test file(s)** (which test file validates this phase, if any)
- **Dependencies** (any prior phases that must pass first)

If the analysis identifies only a single sub-kernel (e.g., simple SFPU ops), there is one phase and the workflow is identical to a single-pass generation.

### Step 3: Loop Over Phases

For each phase **in order**, run Steps 3a–3d. Only proceed to the next phase when the current phase passes tests (or has no applicable test).

#### Step 3a: Plan Phase

Spawn an agent:
```
Agent tool:
  subagent_type: "general-purpose"
  description: "Plan {op} phase {N}"
  prompt: |
    Read and follow codegen-bh/agents/llk-planner.md to plan the "{op}" kernel.
    Kernel type: {kernel_type}
    Analysis: codegen-bh/artifacts/{op}_analysis.md
    Output your spec to: codegen-bh/artifacts/{op}_phase{N}_spec.md

    IMPORTANT - INCREMENTAL PHASE:
    You are planning ONLY phase {N}: "{phase_name}"
    Functions to plan: {phase_functions}
    Previously completed phases: {list of completed phase names, or "none"}

    Plan ONLY the functions listed above. Do not plan functions from other phases.
    If prior phases exist, their functions are already written and tested — your
    phase must be compatible with them but do not redesign them.

    CRITICAL: Design from BH patterns, not WH patterns. The analysis contains
    BH-expected API from the test harness and parent file - template params and
    function signatures MUST match those, not the WH reference.
    Verify init/uninit symmetry: uninit must restore what init changes.

    LOG_DIR: {LOG_DIR}
```

#### Step 3b: Generate Phase Code

Spawn an agent:
```
Agent tool:
  subagent_type: "general-purpose"
  description: "Generate {op} phase {N}"
  prompt: |
    Read and follow codegen-bh/agents/llk-kernel-writer.md to generate the "{op}" kernel.
    Kernel type: {kernel_type}
    Spec: codegen-bh/artifacts/{op}_phase{N}_spec.md
    Output to: tt_llk_blackhole/{kernel_path}
    Run compilation check after writing.

    IMPORTANT - INCREMENTAL PHASE:
    You are implementing ONLY phase {N}: "{phase_name}"
    Functions to write: {phase_functions}
    Previously completed phases: {list of completed phase names, or "none"}

    If prior phases exist, READ the current file first. Their functions are already
    written and tested — APPEND your new functions after them. Do NOT modify
    previously written functions.

    If this is phase 1, create the file with includes/headers and write your functions.

    CRITICAL: Before writing code, verify EVERY function signature against:
    1. The BH test harness (tests/sources/*{op}*.cpp, #ifdef ARCH_BLACKHOLE branch)
    2. The BH parent file (tt_llk_blackhole/llk_lib/llk_{type}.h)
    3. The closest existing BH kernel of this type
    If the spec conflicts with BH sources, BH sources WIN.
    Do NOT port WH features that BH test/parent don't reference.

    LOG_DIR: {LOG_DIR}
```

#### Step 3c: Debug Phase (if needed)

If Step 3b reports compilation failure, spawn debugger:
```
Agent tool:
  subagent_type: "general-purpose"
  description: "Debug {op} phase {N}"
  prompt: |
    Read and follow codegen-bh/agents/llk-debugger.md to fix compilation errors.
    Kernel: {op}
    Kernel type: {kernel_type}
    Kernel path: tt_llk_blackhole/{kernel_path}
    Max 5 fix attempts. Report when compilation passes or if stuck.

    IMPORTANT - INCREMENTAL PHASE:
    You are debugging ONLY phase {N}: "{phase_name}"
    Functions in this phase: {phase_functions}
    Do NOT modify functions from previously completed phases — they are tested and working.

    LOG_DIR: {LOG_DIR}
```

#### Step 3d: Test Phase

After compilation passes, spawn the tester:
```
Agent tool:
  subagent_type: "general-purpose"
  description: "Test {op} phase {N}"
  prompt: |
    Read and follow codegen-bh/agents/llk-tester.md to test the "{op}" kernel.
    Kernel: {op}
    Kernel type: {kernel_type}
    Kernel path: tt_llk_blackhole/{kernel_path}
    Architecture: blackhole

    IMPORTANT - INCREMENTAL PHASE:
    This is phase {N}: "{phase_name}"
    Functions in this phase: {phase_functions}
    Previously completed phases: {list of completed phase names, or "none"}

    You MUST CREATE a phase-specific test (C++ source + Python test) that exercises
    ONLY the functions from this phase. Do NOT use existing tests — they expect the
    complete kernel. See Phase 2 in llk-tester.md for instructions.

    After your phase test passes, also re-run phase tests from previous phases to
    confirm no regressions.

    LOG_DIR: {LOG_DIR}
```

Wait for test results. If PASSED, mark this phase complete and continue to the next phase.
If FAILED, return to Step 3c (debug) for this phase. Max 2 debug→test cycles per phase before escalating to the user.

#### Step 3e: Cleanup Phase Tests

After all phases pass, delete the phase test files:
```bash
rm -f tests/sources/{op}_phase*_test.cpp
rm -f tests/python_tests/test_{op}_phase*.py
```

### Step 4: Final Regression

After all phases complete and phase tests are cleaned up, run the existing repo tests that exercise this kernel to confirm the complete kernel works end-to-end:

```bash
# Find existing tests for this kernel
grep -rl "{op}" tests/python_tests/test_*.py tests/sources/*.cpp 2>/dev/null
```

Run any matching existing tests. If they fail, return to the debug→test loop.

### Step 5: Report Completion and Log Metrics

After all phases complete and regression passes:

1. **Copy the orchestration log** to `{LOG_DIR}/orchestration.md`

2. **Append a run entry** to `/proj_sw/user_dev/$USER/codegen-metrics/runs.jsonl`:
```json
{"kernel": "{op}", "arch": "blackhole", "date": "{YYYY-MM-DD}", "duration_min": {N}, "cost_usd": null, "agents": ["analyzer", "planner", "writer", "tester", "debugger"], "debug_cycles": {N}, "tests_total": {N}, "tests_passed": {N}, "status": "success|failed", "obstacle": "{main obstacle or null}", "log_file": "logs/{RUN_ID}"}
```

3. **Report**:
```
Kernel Type: {kernel_type}
Generated: tt_llk_blackhole/{kernel_path}
Phases completed: {N}/{total}
Compilation: PASSED/FAILED
Functional Tests: PASSED/FAILED/NOT_AVAILABLE (per phase)
Metrics logged: /proj_sw/user_dev/$USER/codegen-metrics/runs.jsonl
Agent logs: {LOG_DIR}/
Artifacts:
  - codegen-bh/artifacts/{op}_analysis.md
  - codegen-bh/artifacts/{op}_phase{N}_spec.md (one per phase)
```

---

## Why Agents?

Each agent runs in a **separate context**:
- Main conversation stays clean
- Only summaries come back
- No context pollution from file reads, searches, iterations

## Why Phases?

Each sub-kernel goes through a **full write→compile→test cycle** before the next:
- Smaller blast radius — 50-100 lines per phase, not 400+
- Real feedback — agents learn from test results before attempting the next phase
- Working baseline — when phase N breaks, phase N-1 is known good
- Different complexity — sub-kernels often use fundamentally different patterns (e.g., ckernel_template vs replay buffers). Getting both right simultaneously with zero feedback is unrealistic

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
