# LLK CodeGen Orchestrator

## CRITICAL: No Git Commands

**NEVER use any git commands** (git diff, git status, git log, git show, git checkout, git restore, etc.) anywhere in the codegen workflow — not in the orchestrator, not in any subagent. All file operations must use direct file reads/writes only. This rule is absolute and applies to all agents spawned by this orchestrator.

---

When a user asks to **"generate {kernel} for {target_arch}"**, follow this workflow using the **Agent tool** to spawn subagents. Each agent runs in its own context, keeping the main conversation clean.

The system is designed to work across architectures. Agents must **discover** architectural patterns from authoritative sources — not rely on hardcoded knowledge.

---

## CRITICAL: Incremental Phase-Based Generation

**Kernels MUST be generated incrementally, one sub-kernel at a time.**

Most kernel files contain multiple sub-kernels (e.g., a basic variant, a dual-input variant, an optimized variant). Each sub-kernel is a group of related functions (init, main, uninit, mop_config) that form a logical unit.

**The rule**: Write one sub-kernel → compile → test → only proceed to the next sub-kernel when the current one passes. Never write the entire file at once.

**Why**: A single wrong architectural assumption in a monolithic write poisons 400+ lines with no working baseline. Incremental phases give test feedback early, keep blast radius small, and give agents a working foundation to build on.

---

## Step -1: Validate Environment

Before starting, verify prerequisites:

```bash
cd codegen
PYTHONPATH=.. python -c "from codegen.config.settings import settings; issues = settings.validate(); [print(f'ISSUE: {i}') for i in issues]; exit(1) if issues else print('Environment OK')"
```

If any issues are reported, **stop and tell the user** what needs to be fixed before codegen can work.

---

## Step 0: Setup Metrics Logging

Record the start time and create a unique log directory for this run:

```bash
START_TIME=$(date -u +%Y-%m-%dT%H:%M:%SZ)
RUN_ID=$(date +%Y-%m-%d)_{kernel}_{arch}_$(head -c 4 /dev/urandom | xxd -p)
LOG_DIR=/proj_sw/user_dev/llk_code_gen/quasar/$RUN_ID
mkdir -p $LOG_DIR/instructions
```

Track these variables throughout the run for metrics:
- `START_TIME` — captured above
- `PROMPT` — the original user prompt verbatim (e.g., "Generate gelu for Quasar")
- `BATCH_ID` — if provided via environment variable `CODEGEN_BATCH_ID`, use it; otherwise `null`
- `MODEL` — if provided via environment variable `CODEGEN_MODEL`, use it; otherwise detect from the current Claude CLI model (e.g., "opus", "sonnet", "haiku")
- `RUN_TYPE` — if `CODEGEN_BATCH_ID` is set, use `"ci"`; otherwise `"manual"`
- `COMPILATION_ATTEMPTS=0` — increment each time `check_compile.py` is run
- `PER_PHASE=[]` — build up per-phase results as phases complete

Copy the agent playbooks used (snapshot for reproducibility):
```bash
cp codegen/agents/llk-analyzer.md $LOG_DIR/instructions/
cp codegen/agents/llk-planner.md $LOG_DIR/instructions/
cp codegen/agents/llk-kernel-writer.md $LOG_DIR/instructions/
cp codegen/agents/llk-debugger.md $LOG_DIR/instructions/
cp codegen/agents/llk-tester.md $LOG_DIR/instructions/
cp codegen/agents/llk-test-writer.md $LOG_DIR/instructions/
cp codegen/agents/llk-prettifier.md $LOG_DIR/instructions/
```

Pass `LOG_DIR` to every agent prompt so they can self-log their reasoning.

---

## Step 0b: Identify Kernel Type and Architecture

Determine the kernel category and architecture from the request:

| Category | Keywords | Path Pattern |
|----------|----------|--------------|
| **SFPU** | sigmoid, relu, exp, gelu, tanh, sqrt, recip | `common/inc/sfpu/ckernel_sfpu_{op}.h` |
| **Math** | matmul, reduce, eltwise, binary, unary | `llk_lib/llk_math_{op}.h` |
| **Pack** | pack, untilize | `llk_lib/llk_pack_{op}.h` |
| **Unpack** | unpack, tilize | `llk_lib/llk_unpack_{op}.h` |

Determine:
- **Reference architecture** (default: blackhole) — the existing implementation to port from
- **Target architecture** (default: quasar) — where the kernel needs to run

---

## Workflow: Spawn Agents Sequentially

### Step 1: Research Target Architecture

**This step is mandatory.** Before analyzing any code, gather architecture knowledge from authoritative sources.

Spawn an agent:
```
Agent tool:
  subagent_type: "general-purpose"
  description: "Research {target_arch} architecture for {op} kernel"
  prompt: |
    Research the {target_arch} architecture to understand what's needed for implementing a {kernel_type} kernel.

    ## Sources to query (use ALL — Confluence is PRIMARY):

    ### 1. Confluence — Target Architecture Specs (PRIMARY SOURCE)
    Use Atlassian MCP tools. You MUST fetch BOTH of these pages:

    **Page 1: Tensix NEO High Level Specification (page ID 84508873)**
    - Use mcp__atlassian__getConfluencePage with page ID 84508873
    - Contains: General architecture — SFPU overview, register files, execution model, NoC, data formats, tile structure
    - Extract sections relevant to {kernel_type} kernels

    **Page 2: Tensix Instruction Set Architecture (page ID 1613201604)**
    - Use mcp__atlassian__getConfluencePage with page ID 1613201604
    - Contains: Per-instruction details — every instruction with parameters, encoding, behavior, constraints
    - This is the AUTHORITATIVE source for instruction behavior
    - Extract all instructions relevant to {kernel_type} kernels (parameters, encoding, behavior)

    Also search for other relevant pages using mcp__atlassian__searchConfluenceUsingCql.

    ### 2. DeepWiki (for reference architecture ISA docs)
    Use DeepWiki MCP tools with repo: tenstorrent/tt-isa-documentation
    - Ask about {kernel_type} instructions, register files, execution model
    - Ask about the reference architecture (blackhole) instruction set for comparison

    ### 3. assembly.yaml (for cross-checking instruction existence)
    Verify instructions found in Confluence exist in the local ISA definition:
      grep -i "{relevant_keyword}" tt_llk_{target_arch}/instructions/assembly.yaml
    For SFPU kernels, search for: SFPLOAD, SFPSTORE, SFPMAD, SFPNONLINEAR, etc.
    For math kernels, search for: MATMUL, ELWADD, GMPOOL, GAPOOL, etc.
    For pack/unpack, search for: PACR, UNPACR, TILE_INC, etc.

    ## Output
    Write a concise architecture brief to: codegen/artifacts/{op}_arch_research.md
    Include:
    - Relevant instructions available on {target_arch} for this kernel type (FROM CONFLUENCE)
    - Per-instruction details: parameters, encoding, behavior (FROM CONFLUENCE ISA page)
    - Register file layout and constraints
    - Key differences from reference architecture (if found)
    - Any architecture-specific patterns or constraints

    LOG_DIR: {LOG_DIR}
```

Wait for completion. **Verify** that `codegen/artifacts/{op}_arch_research.md` exists.

### Step 2: Analyze Reference Implementation

Spawn an agent:
```
Agent tool:
  subagent_type: "general-purpose"
  description: "Analyze {op} kernel"
  prompt: |
    Read and follow codegen/agents/llk-analyzer.md to analyze the "{op}" kernel.
    Kernel type: {kernel_type}
    Reference architecture: {ref_arch}
    Target architecture: {target_arch}
    Reference path: tt_llk_{ref_arch}/{kernel_path}
    Architecture research: codegen/artifacts/{op}_arch_research.md
    Output your analysis to: codegen/artifacts/{op}_analysis.md

    CRITICAL: Before reading the reference, you MUST read the target integration points:
    1. Test harness: Find and read tests/sources/*{op}*.cpp (look for #ifdef ARCH_{TARGET_UPPER})
    2. Parent file: Read the target file that #includes this kernel
    3. Closest existing target kernel: Read the most similar existing target kernel of this type line-by-line
    Document the target-expected API (function signatures, template params, target-only features, reference-only features to drop).

    CRITICAL: You MUST identify sub-kernel phases in your analysis. See the
    "Sub-Kernel Phases" section in llk-analyzer.md for the required output format.

    LOG_DIR: {LOG_DIR}
```

Wait for completion. Agent returns summary of analysis **including the phase plan**.

**Verify**: Check that `codegen/artifacts/{op}_analysis.md` exists and contains: kernel_type, functions list, dependencies, complexity classification, and sub-kernel phases. If missing, **stop and report the error to the user**.

### Step 2b: Extract Phase Plan

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
    Read and follow codegen/agents/llk-planner.md to plan the "{op}" kernel.
    Kernel type: {kernel_type}
    Target architecture: {target_arch}
    Analysis: codegen/artifacts/{op}_analysis.md
    Architecture research: codegen/artifacts/{op}_arch_research.md
    Output your spec to: codegen/artifacts/{op}_phase{N}_spec.md

    IMPORTANT - INCREMENTAL PHASE:
    You are planning ONLY phase {N}: "{phase_name}"
    Functions to plan: {phase_functions}
    Previously completed phases: {list of completed phase names, or "none"}

    Plan ONLY the functions listed above. Do not plan functions from other phases.
    If prior phases exist, their functions are already written and tested — your
    phase must be compatible with them but do not redesign them.

    CRITICAL: Design from target patterns, not reference patterns. The analysis contains
    target-expected API from the test harness and parent file — template params and
    function signatures MUST match those, not the reference.
    Verify init/uninit symmetry: uninit must restore what init changes.

    LOG_DIR: {LOG_DIR}
```

Wait for completion. **Verify** that `codegen/artifacts/{op}_phase{N}_spec.md` exists.

#### Step 3b: Generate Phase Code

Spawn an agent:
```
Agent tool:
  subagent_type: "general-purpose"
  description: "Generate {op} phase {N}"
  prompt: |
    Read and follow codegen/agents/llk-kernel-writer.md to generate the "{op}" kernel.
    Kernel type: {kernel_type}
    Target architecture: {target_arch}
    Spec: codegen/artifacts/{op}_phase{N}_spec.md
    Output to: tt_llk_{target_arch}/{kernel_path}
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
    1. The target test harness (tests/sources/*{op}*.cpp, #ifdef ARCH_{TARGET_UPPER} branch)
    2. The target parent file (tt_llk_{target_arch}/llk_lib/llk_{type}.h)
    3. The closest existing target kernel of this type
    If the spec conflicts with target sources, target sources WIN.
    Do NOT port reference features that the target test/parent don't reference.

    LOG_DIR: {LOG_DIR}
```

Wait for completion. Agent returns compilation result (PASSED or FAILED).

**Metrics**: Increment `COMPILATION_ATTEMPTS` by 1 (the writer always runs one compile check).

#### Step 3c: Debug Phase (if needed)

If Step 3b reports compilation failure, spawn debugger:
```
Agent tool:
  subagent_type: "general-purpose"
  description: "Debug {op} phase {N}"
  prompt: |
    Read and follow codegen/agents/llk-debugger.md to fix compilation errors.
    Kernel: {op}
    Kernel type: {kernel_type}
    Target architecture: {target_arch}
    Kernel path: tt_llk_{target_arch}/{kernel_path}
    Architecture research: codegen/artifacts/{op}_arch_research.md
    Max 5 fix attempts. Report when compilation passes or if stuck.

    IMPORTANT - INCREMENTAL PHASE:
    You are debugging ONLY phase {N}: "{phase_name}"
    Functions in this phase: {phase_functions}
    Do NOT modify functions from previously completed phases — they are tested and working.

    LOG_DIR: {LOG_DIR}
```

**Metrics**: Increment `COMPILATION_ATTEMPTS` by the number of compile attempts the debugger made (up to 5). Increment `debug_cycles` by 1.

**If debugger reports STUCK** after 5 attempts: **stop and report to the user** with the blocking error details. Do NOT proceed to the next phase.

#### Step 3d: Create Tests (if needed)

After compilation passes, check if a test exists for this kernel:
```bash
ls tests/python_tests/{target_arch}/test_{op}_{target_arch}.py 2>/dev/null
ls tests/python_tests/{target_arch}/test_sfpu_*{op}*_{target_arch}.py 2>/dev/null
```

If a test file exists, skip to Step 3e.

If NO test exists, spawn the test-writer agent:
```
Agent tool:
  subagent_type: "general-purpose"
  description: "Create tests for {op}"
  prompt: |
    Read and follow codegen/agents/llk-test-writer.md to create functional tests.
    Kernel: {op}
    Kernel type: {kernel_type}
    Target architecture: {target_arch}
    Kernel path: tt_llk_{target_arch}/{kernel_path}

    LOG_DIR: {LOG_DIR}
```

Wait for completion. The agent will create:
- `tests/sources/{target_arch}/sfpu_{op}_{target_arch}_test.cpp`
- `tests/python_tests/{target_arch}/test_{op}_{target_arch}.py`
- Any required infrastructure changes (SfpuType enum entries, etc.)

If the agent reports BLOCKED, record `test_result: "skipped"` for this phase and continue.

#### Step 3e: Test Phase

After compilation passes (and tests exist), spawn the tester:
```
Agent tool:
  subagent_type: "general-purpose"
  model: "haiku"
  description: "Test {op} phase {N}"
  prompt: |
    Read and follow codegen/agents/llk-tester.md to test the "{op}" kernel.
    Kernel: {op}
    Kernel type: {kernel_type}
    CHIP_ARCH: {target_arch}
    Architecture: {target_arch}

    IMPORTANT - INCREMENTAL PHASE:
    This is phase {N}: "{phase_name}"
    Functions in this phase: {phase_functions}
    Previously completed phases: {list of completed phase names, or "none"}

    You MUST CREATE a phase-specific test (C++ source + Python test) that exercises
    ONLY the functions from this phase. Do NOT use existing tests — they expect the
    complete kernel. See the "Phase Test Creation" section in llk-tester.md for instructions.

    After your phase test passes, also re-run phase tests from previous phases to
    confirm no regressions.

    LOG_DIR: {LOG_DIR}
```

Wait for test results. If PASSED, mark this phase complete and continue to the next phase.

If FAILED, spawn the debugger again but this time with test failure details instead of compilation errors:
```
Agent tool:
  subagent_type: "general-purpose"
  description: "Fix {op} test failure phase {N}"
  prompt: |
    Read and follow codegen/agents/llk-debugger.md to fix RUNTIME/TEST failures.
    Kernel: {op}
    Kernel type: {kernel_type}
    Target architecture: {target_arch}
    Kernel path: tt_llk_{target_arch}/{kernel_path}
    Architecture research: codegen/artifacts/{op}_arch_research.md

    THIS IS A TEST FAILURE, NOT A COMPILATION ERROR.
    Follow the "Runtime/Functional Debugging" section in llk-debugger.md.

    Test failure details:
    {paste the test output/error from the tester agent}

    IMPORTANT - INCREMENTAL PHASE:
    You are debugging ONLY phase {N}: "{phase_name}"
    Functions in this phase: {phase_functions}
    Do NOT modify functions from previously completed phases.

    After fixing, recompile to ensure compilation still passes:
      cd codegen && source ../tests/.venv/bin/activate
      PYTHONPATH=.. python scripts/check_compile.py ../{kernel_path} -v

    Max 5 fix attempts.

    LOG_DIR: {LOG_DIR}
```
After the debugger fixes the code, return to Step 3e (test) to verify.
Max 2 debug→test cycles per phase before escalating to the user.

**Metrics**: When a phase completes (pass or fail), record its per-phase result:
```
{
  "phase": {N},
  "name": "{phase_name}",
  "compilation_attempts": {N},  // compile attempts for THIS phase only
  "debug_cycles": {N},          // debug rounds for THIS phase only
  "test_result": "passed|failed|skipped"
}
```
Append to the `PER_PHASE` list.

#### Step 3f: Cleanup Phase Tests

After **all phases pass**, delete the temporary phase test files:
```bash
rm -f tests/sources/{op}_phase*_test.cpp
rm -f tests/python_tests/test_{op}_phase*.py
```

### Step 4: Final Regression

After all phases complete and phase tests are cleaned up, run the existing repo tests that exercise this kernel to confirm the complete kernel works end-to-end:

```bash
source ../tests/.venv/bin/activate
cd ../tests/python_tests/quasar
TT_UMD_SIMULATOR_PATH=/proj_sw/user_dev/vvukomanovic/tt-umd-simulators/build/vcs-quasar-1x3 CHIP_ARCH=quasar pytest -x --run-simulator --port=5556 test_{op}_quasar.py
```

If no existing test covers this kernel, report NOT_AVAILABLE and move to Step 5.
If tests FAIL, return to the debug→test loop (Step 3c/3e) for the failing phase.

### Step 5: Backup Pre-Prettification

Before prettifying, **save a copy** of the working kernel so we can revert if needed:

```bash
cp tt_llk_{target_arch}/{kernel_path} codegen/artifacts/{op}_pre_prettify_backup.h
```

### Step 6: Prettify Kernel

Spawn an agent:
```
Agent tool:
  subagent_type: "general-purpose"
  description: "Prettify {op} kernel"
  prompt: |
    Read and follow codegen/agents/llk-prettifier.md to refactor the "{op}" kernel.
    Kernel: {op}
    Kernel type: {kernel_type}
    Target architecture: {target_arch}
    Kernel path: tt_llk_{target_arch}/{kernel_path}
    Refactor the code for maintainability and reuse while preserving behavior.

    LOG_DIR: {LOG_DIR}
```

Wait for completion. Agent returns compilation result (PASSED or FAILED).

### Step 7: Compile Prettified Kernel

If the prettifier did NOT already report PASSED compilation, run compilation check:
```
Agent tool:
  subagent_type: "general-purpose"
  description: "Compile prettified {op}"
  prompt: |
    Run compilation check on the prettified kernel:
      cd codegen
      source ../tests/.venv/bin/activate
      PYTHONPATH=.. python scripts/check_compile.py tt_llk_{target_arch}/{kernel_path} -v
    Report PASSED or FAILED with full error output.
```

**If FAILED**: Go to **Step 8** (debug). Set `prettify_debug_attempts = 0`.

**If PASSED**: Go to **Step 9** (test).

### Step 8: Debug Prettified Kernel (if compilation failed)

Spawn the debugger:
```
Agent tool:
  subagent_type: "general-purpose"
  description: "Debug prettified {op} kernel"
  prompt: |
    Read and follow codegen/agents/llk-debugger.md to fix compilation errors.
    Kernel: {op}
    Kernel type: {kernel_type}
    Target architecture: {target_arch}
    Kernel path: tt_llk_{target_arch}/{kernel_path}
    Architecture research: codegen/artifacts/{op}_arch_research.md
    This kernel was recently refactored (prettified) and compilation broke.
    Max 3 fix attempts. Report when compilation passes or if stuck.

    LOG_DIR: {LOG_DIR}
```

Increment `prettify_debug_attempts`.

**If PASSED**: Go to **Step 9** (test).

**If STUCK** and `prettify_debug_attempts < 2`: Go to **Step 8** again.

**If STUCK** and `prettify_debug_attempts >= 2`: **Revert** — restore the backup:
```bash
cp codegen/artifacts/{op}_pre_prettify_backup.h tt_llk_{target_arch}/{kernel_path}
```
Report `Prettified: SKIPPED (compilation could not be fixed)`. Go to **Step 10** (report).

### Step 9: Test Prettified Kernel

Run functional tests on the prettified kernel:
```
Agent tool:
  subagent_type: "general-purpose"
  model: "haiku"
  description: "Test prettified {op} kernel"
  prompt: |
    Read and follow codegen/agents/llk-tester.md to run functional tests.
    Kernel: {op}
    Kernel type: {kernel_type}
    CHIP_ARCH: {target_arch}
    This is a re-validation after prettification/refactoring.

    Run tests directly with pytest:
      source ../tests/.venv/bin/activate
      cd ../tests/python_tests/quasar
      TT_UMD_SIMULATOR_PATH=/proj_sw/user_dev/vvukomanovic/tt-umd-simulators/build/vcs-quasar-1x3 CHIP_ARCH=quasar pytest -x --run-simulator --port=5556 test_{op}_quasar.py

    If tests FAIL, report the failure details.

    LOG_DIR: {LOG_DIR}
```

**If PASSED**: Prettification is complete. Go to **Step 10** (report).

**If FAILED**: Go to **Step 8** (debug) with the test failure details appended. Loop back to **Step 9**.

**If FAILED after 2 full debug→test cycles**: **Revert** — restore the backup:
```bash
cp codegen/artifacts/{op}_pre_prettify_backup.h tt_llk_{target_arch}/{kernel_path}
```
Report `Prettified: SKIPPED (tests failed after refactoring)`. Go to **Step 10** (report).

### Step 10: Report Completion and Log Metrics

After all agents complete:

1. **Record end time**:
```bash
END_TIME=$(date -u +%Y-%m-%dT%H:%M:%SZ)
```

2. **Count lines generated**:
```bash
LINES_GENERATED=$(wc -l < tt_llk_{target_arch}/{kernel_path})
```

3. **Append a run entry** to `/proj_sw/user_dev/llk_code_gen/quasar/runs.jsonl`:
```json
{
  "kernel": "{op}",
  "kernel_type": "{sfpu|math|pack|unpack}",
  "arch": "{target_arch}",
  "reference_arch": "{ref_arch}",
  "reference_file": "tt_llk_{ref_arch}/{kernel_path}",
  "generated_file": "tt_llk_{target_arch}/{kernel_path}",
  "start_time": "{START_TIME}",
  "end_time": "{END_TIME}",
  "phases_total": 0,
  "phases_completed": 0,
  "compilation_attempts": 0,
  "debug_cycles": 0,
  "tests_total": 0,
  "tests_passed": 0,
  "lines_generated": 0,
  "prettified": true,
  "status": "success",
  "obstacle": null,
  "per_phase": [
    {
      "phase": 1,
      "name": "{phase_name}",
      "compilation_attempts": 0,
      "debug_cycles": 0,
      "test_result": "passed",
      "first_compile_error": null,
      "test_details": null
    }
  ],
  "prompt": "{original_prompt}",
  "batch_id": null,
  "tokens": {
    "input": 0,
    "output": 0,
    "cache_read": 0,
    "total": 0
  },
  "model": "{MODEL}",
  "run_type": "{RUN_TYPE}",
  "agents": ["analyzer", "planner", "writer", "tester", "debugger"],
  "run_id": "{RUN_ID}",
  "log_dir": "logs/{RUN_ID}"
}
```
**Write as a single line** (the above is expanded for readability). Use actual values, not placeholders.

**Notes on special fields**:
- `status`: Use three-way classification:
  - `"success"` — compiles AND all tests pass (`tests_passed == tests_total` and `tests_total > 0`)
  - `"compiled"` — compiles but tests failed, were skipped, or unavailable (`tests_total == 0` or `tests_passed < tests_total`)
  - `"failed"` — does not compile
- `prompt`: Store the exact user prompt that initiated this run (e.g., "Generate gelu for Quasar")
- `batch_id`: Use `$CODEGEN_BATCH_ID` environment variable if set, otherwise `null`. The batch runner script sets this to group runs from a single session.
- `model`: Use `$CODEGEN_MODEL` environment variable if set (e.g., "opus", "sonnet", "haiku"). Otherwise, detect from the current Claude CLI model. The batch runner script sets this to track which model was used.
- `run_type`: `"ci"` if `$CODEGEN_BATCH_ID` is set (indicates a scheduled/automated batch run), `"manual"` otherwise (interactive session). This lets the dashboard distinguish Friday CI runs from ad-hoc manual runs.
- `tokens`: If token usage is not available from the CLI output, set all values to `0`. When running via `claude -p "..." --output-format json`, the response includes `usage.input_tokens`, `usage.output_tokens`, and `usage.cache_read_input_tokens` — the batch runner script will pass these via `$CODEGEN_TOKENS_INPUT`, `$CODEGEN_TOKENS_OUTPUT`, `$CODEGEN_TOKENS_CACHE_READ` environment variables. `total = input + output`.

4. **Write the report** to `codegen/artifacts/{op}_report.md` AND print it directly to the terminal:

```
========================================
  LLK CodeGen — Generation Complete
========================================
Prompt:           {PROMPT}
Kernel:           {op}
Kernel Type:      {kernel_type}
Target Arch:      {target_arch}
Reference:        tt_llk_{ref_arch}/{kernel_path}
Generated File:   tt_llk_{target_arch}/{kernel_path}
Lines Generated:  {N}
----------------------------------------
Timing:
  Start:          {START_TIME}
  End:            {END_TIME}
----------------------------------------
Tokens:
  Input:          {N}
  Output:         {N}
  Cache Read:     {N}
  Total:          {N}
----------------------------------------
Quality:
  Phases:           {completed}/{total}
  Compile Attempts: {N} (across all phases)
  Debug Cycles:     {N}
  Compilation:      PASSED/FAILED
  Functional Tests: PASSED/FAILED/NOT_AVAILABLE ({passed}/{total})
  Prettified:       YES/SKIPPED
----------------------------------------
Per Phase:
  Phase 1 ({name}): compile_attempts={N}, debug={N}, test={passed/failed}
  Phase 2 ({name}): compile_attempts={N}, debug={N}, test={passed/failed}
  ...
----------------------------------------
Artifacts:
  - codegen/artifacts/{op}_arch_research.md
  - codegen/artifacts/{op}_analysis.md
  - codegen/artifacts/{op}_phase{N}_spec.md (one per phase)
Metrics:
  - /proj_sw/user_dev/llk_code_gen/quasar/runs.jsonl
  - {LOG_DIR}/
========================================
```

This conclusion MUST be both:
1. **Written to file**: `codegen/artifacts/{op}_report.md`
2. **Output as text** in your response so the user sees it directly in the terminal

5. **Copy all artifacts to LOG_DIR** so each run is self-contained:
```bash
cp codegen/artifacts/{op}_report.md {LOG_DIR}/
cp codegen/artifacts/{op}_arch_research.md {LOG_DIR}/
cp codegen/artifacts/{op}_analysis.md {LOG_DIR}/
cp codegen/artifacts/{op}_phase*_spec.md {LOG_DIR}/
cp tt_llk_{target_arch}/{kernel_path} {LOG_DIR}/
# Copy the runs.jsonl entry as a standalone run.json for this run
```
Also write `{LOG_DIR}/run.json` containing **just this run's** JSONL entry (same content appended to runs.jsonl, but pretty-printed JSON). This makes each LOG_DIR a complete, self-contained record.

6. **Verify all agent logs exist** in LOG_DIR. The following files MUST be present:
   - `agent_analyzer.md`
   - `agent_planner.md`
   - `agent_writer.md`
   - `agent_tester.md`
   - `agent_test_writer.md` (only if tests were created)
   - `agent_prettifier.md`
   - `agent_arch_lookup.md` (from the arch research agent)
   - `agent_debugger.md` (only if the debugger was invoked)

If any expected file is missing, write a placeholder noting `"Agent ran but did not produce a log"` so missing logs are visible rather than silently absent.

---

## Inter-Agent Contracts

Each stage produces artifacts that the next stage consumes:

| From → To | Artifact | Required Contents |
|-----------|----------|-------------------|
| Researcher → Analyzer, Planner | `artifacts/{op}_arch_research.md` | Available instructions, register layout, arch constraints |
| Analyzer → Planner | `artifacts/{op}_analysis.md` | kernel_type, function signatures, dependencies, complexity_class, key constructs, sub-kernel phases |
| Planner → Writer | `artifacts/{op}_phase{N}_spec.md` | target_file_path, instruction_sequence (pseudocode), resource_allocation, includes |
| Writer → Debugger | kernel file + error output | Full compiler stderr passed in prompt |
| Writer/Debugger → Tester | compiled kernel file | File must exist and compile successfully |
| Tester → next phase or Prettifier | tested kernel file | Phase tests pass before proceeding |
| Prettifier → Compiler → Debugger (loop) | prettified kernel file | Must compile; if fails, debug up to 2 cycles or revert |
| Compiler/Debugger → Tester (loop) | compiling kernel file | Must pass functional tests; if fails, debug up to 2 cycles or revert |

---

## Authoritative Sources

Agents must discover architectural details from these sources — **not from hardcoded knowledge in prompts**:

| Priority | Source | What it provides | How to access |
|----------|--------|-----------------|---------------|
| 1 | **Confluence: Tensix NEO Spec** | General architecture — SFPU, register files, execution model, data formats | `mcp__atlassian__getConfluencePage` page ID `84508873` |
| 1 | **Confluence: Tensix ISA** | Per-instruction details — parameters, encoding, behavior, constraints | `mcp__atlassian__getConfluencePage` page ID `1613201604` |
| 2 | **Existing target implementations** | Working code patterns for the target arch | Read files in `tt_llk_{target_arch}/` |
| 3 | **DeepWiki** | Reference architecture ISA docs (Blackhole instruction set) | DeepWiki MCP tools with repo `tenstorrent/tt-isa-documentation` |
| 4 | **assembly.yaml** | Target ISA instruction definitions (cross-check) | `grep` in `tt_llk_{arch}/instructions/assembly.yaml` |
| 5 | **Reference implementations** | Source code being ported | Read files in `tt_llk_{ref_arch}/` |

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

## Key Paths

| Path | Purpose |
|------|---------|
| `tt_llk_blackhole/` | Blackhole LLK (reference architecture) |
| `tt_llk_quasar/` | Quasar LLK (target architecture) |
| `tt_llk_{arch}/instructions/assembly.yaml` | ISA definition (cross-check, use grep — large file) |
| `codegen/references/common-errors.md` | Known error patterns for debugging |

## Key Confluence Pages

| Page ID | Title | Purpose |
|---------|-------|---------|
| `84508873` | Tensix NEO High Level Specification | General architecture — SFPU, registers, execution model |
| `1613201604` | Tensix Instruction Set Architecture | Per-instruction details — parameters, encoding, behavior |

## Commands

```bash
# Compilation check (syntax/type errors)
cd codegen
source ../tests/.venv/bin/activate
PYTHONPATH=.. python scripts/check_compile.py {path_to_kernel} -v

# Functional tests (correctness validation)
source ../tests/.venv/bin/activate
cd ../tests/python_tests/quasar
TT_UMD_SIMULATOR_PATH=/proj_sw/user_dev/vvukomanovic/tt-umd-simulators/build/vcs-quasar-1x3 CHIP_ARCH=quasar pytest -x --run-simulator --port=5556 test_{kernel_name}_quasar.py

# List available tests
ls ../tests/python_tests/quasar/test_*_quasar.py
```
