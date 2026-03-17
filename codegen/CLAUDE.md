# LLK CodeGen Orchestrator

## CRITICAL: No Git Commands

**NEVER use any git commands** (git diff, git status, git log, git show, git checkout, git restore, etc.) anywhere in the codegen workflow — not in the orchestrator, not in any subagent. All file operations must use direct file reads/writes only. This rule is absolute and applies to all agents spawned by this orchestrator.

---

When a user asks to **"generate {kernel} for {target_arch}"**, follow this workflow using the **Agent tool** to spawn subagents. Each agent runs in its own context, keeping the main conversation clean.

The system is designed to work across architectures. Agents must **discover** architectural patterns from authoritative sources — not rely on hardcoded knowledge.

---

## Step -1: Validate Environment

Before starting, verify prerequisites:

```bash
cd codegen
PYTHONPATH=.. python -c "from codegen.config.settings import settings; issues = settings.validate(); [print(f'ISSUE: {i}') for i in issues]; exit(1) if issues else print('Environment OK')"
```

If any issues are reported, **stop and tell the user** what needs to be fixed before codegen can work.

---

## Step 0: Identify Kernel Type and Architecture

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

    ## Sources to query (use ALL that are available):

    ### 1. Confluence (for target architecture docs)
    Use Atlassian MCP tools to search for architecture specifications:
    - Search for "{target_arch}" architecture, ISA, SFPU, instruction set
    - Look for the Tensix NEO High Level Specification (page ID 84508873) if targeting Quasar/NEO
    - Search for any {kernel_type}-related architecture docs

    ### 2. DeepWiki (for reference architecture ISA docs)
    Use DeepWiki MCP tools with repo: tenstorrent/tt-isa-documentation
    - Ask about {kernel_type} instructions, register files, execution model
    - Ask about the reference architecture (blackhole) instruction set for comparison

    ### 3. Target architecture ISA definition
    Read/grep the target ISA file for relevant instructions:
      grep -i "{relevant_keyword}" tt_llk_{target_arch}/instructions/assembly.yaml
    For SFPU kernels, search for: SFPLOAD, SFPSTORE, SFPMAD, SFPNONLINEAR, etc.
    For math kernels, search for: MATMUL, ELWADD, GMPOOL, GAPOOL, etc.
    For pack/unpack, search for: PACR, UNPACR, TILE_INC, etc.

    ## Output
    Write a concise architecture brief to: codegen/artifacts/{op}_arch_research.md
    Include:
    - Relevant instructions available on {target_arch} for this kernel type
    - Register file layout and constraints
    - Key differences from reference architecture (if found)
    - Any architecture-specific patterns or constraints
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
    Reference path: tt_llk_{ref_arch}/{kernel_path}
    Architecture research: codegen/artifacts/{op}_arch_research.md
    Output your analysis to: codegen/artifacts/{op}_analysis.md
```

Wait for completion.

**Verify**: Check that `codegen/artifacts/{op}_analysis.md` exists and contains: kernel_type, functions list, dependencies, and complexity classification. If missing, **stop and report the error to the user**.

### Step 3: Plan Implementation

Spawn an agent:
```
Agent tool:
  subagent_type: "general-purpose"
  description: "Plan {op} implementation"
  prompt: |
    Read and follow codegen/agents/llk-planner.md to plan the "{op}" kernel.
    Kernel type: {kernel_type}
    Target architecture: {target_arch}
    Analysis: codegen/artifacts/{op}_analysis.md
    Architecture research: codegen/artifacts/{op}_arch_research.md
    Output your spec to: codegen/artifacts/{op}_spec.md
```

Wait for completion.

**Verify**: Check that `codegen/artifacts/{op}_spec.md` exists and contains: target_file_path, instruction_sequence, and resource_allocation. If missing, **stop and report the error to the user**.

### Step 4: Generate Code

Spawn an agent:
```
Agent tool:
  subagent_type: "general-purpose"
  description: "Generate {op} kernel"
  prompt: |
    Read and follow codegen/agents/llk-kernel-writer.md to generate the "{op}" kernel.
    Kernel type: {kernel_type}
    Target architecture: {target_arch}
    Spec: codegen/artifacts/{op}_spec.md
    Output to: tt_llk_{target_arch}/{kernel_path}
    Run compilation check after writing.
```

Wait for completion. Agent returns compilation result (PASSED or FAILED).

**Verify**: Check that `tt_llk_{target_arch}/{kernel_path}` exists. If the kernel file was not created, re-run Step 4 (once). If it fails again, **stop and report to the user**.

### Step 5: Debug (if needed)

If Step 4 reports compilation failure, spawn debugger:
```
Agent tool:
  subagent_type: "general-purpose"
  description: "Debug {op} kernel"
  prompt: |
    Read and follow codegen/agents/llk-debugger.md to fix compilation errors.
    Kernel: {op}
    Kernel type: {kernel_type}
    Target architecture: {target_arch}
    Kernel path: tt_llk_{target_arch}/{kernel_path}
    Architecture research: codegen/artifacts/{op}_arch_research.md
    Max 5 fix attempts. Report when compilation passes or if stuck.
```

**If debugger reports STUCK** after 5 attempts: **stop and report to the user** with the blocking error details. Do NOT proceed to testing.

### Step 6: Run Functional Tests (if compilation passed)

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
    CHIP_ARCH: {target_arch}
    Run quick mode first, then full tests if quick passes.

    IMPORTANT: You MUST set CHIP_ARCH={target_arch} when running tests. Example:
      cd codegen
      source ../tests/.venv/bin/activate
      CHIP_ARCH={target_arch} python scripts/run_functional_test.py {op} --quick -v

    If quick passes, run full tests:
      CHIP_ARCH={target_arch} python scripts/run_functional_test.py {op} -v

    If tests FAIL, report the failure details so the debugger can fix the kernel.
```

Wait for test results. Agent returns PASSED, FAILED, or NOT_AVAILABLE.

**If FAILED**: Go back to Step 5 (debugger) with the test failure details, then re-run Step 6.

### Step 7: Report Completion

After all agents complete:
```
Kernel Type: {kernel_type}
Target Architecture: {target_arch}
Generated: tt_llk_{target_arch}/{kernel_path}
Compilation: PASSED/FAILED
Functional Tests: PASSED/FAILED/NOT_AVAILABLE
Artifacts:
  - codegen/artifacts/{op}_arch_research.md
  - codegen/artifacts/{op}_analysis.md
  - codegen/artifacts/{op}_spec.md
```

---

## Inter-Agent Contracts

Each stage produces artifacts that the next stage consumes:

| From -> To | Artifact | Required Contents |
|-----------|----------|-------------------|
| Researcher -> Analyzer, Planner | `artifacts/{op}_arch_research.md` | Available instructions, register layout, arch constraints |
| Analyzer -> Planner | `artifacts/{op}_analysis.md` | kernel_type, function signatures, dependencies, complexity_class, key constructs |
| Planner -> Writer | `artifacts/{op}_spec.md` | target_file_path, instruction_sequence (pseudocode), resource_allocation, includes |
| Writer -> Debugger | kernel file + error output | Full compiler stderr passed in prompt |
| Writer/Debugger -> Tester | compiled kernel file | File must exist and compile successfully |

---

## Authoritative Sources

Agents must discover architectural details from these sources — **not from hardcoded knowledge in prompts**:

| Source | What it provides | How to access |
|--------|-----------------|---------------|
| **Confluence** | Target architecture specs (Quasar/NEO ISA, SFPU, register files) | Atlassian MCP tools — search or fetch page ID 84508873 |
| **DeepWiki** | Reference architecture ISA docs (Blackhole instruction set) | DeepWiki MCP tools with repo `tenstorrent/tt-isa-documentation` |
| **assembly.yaml** | Target ISA instruction definitions | `grep` in `tt_llk_{arch}/instructions/assembly.yaml` |
| **Existing target implementations** | Working code patterns for the target arch | Read files in `tt_llk_{target_arch}/` |
| **Golden examples** | Verified correct implementations | Read files in `codegen/references/golden/` |
| **Reference implementations** | Source code being ported | Read files in `tt_llk_{ref_arch}/` |

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

## Key Paths

| Path | Purpose |
|------|---------|
| `tt_llk_blackhole/` | Blackhole LLK (reference architecture) |
| `tt_llk_quasar/` | Quasar LLK (target architecture) |
| `tt_llk_{arch}/instructions/assembly.yaml` | ISA definition (use grep — large file) |
| `codegen/references/golden/` | Verified correct target implementations |
| `codegen/references/` | Methodology guides (porting process, error patterns) |

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
