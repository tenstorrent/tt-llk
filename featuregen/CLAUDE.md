# Blackhole Feature Gen Orchestrator

## CRITICAL: No Git Commands

**NEVER use any git commands** (git diff, git status, git log, git show, git checkout, git restore, etc.) anywhere in this workflow — not in the orchestrator, not in any subagent. All file operations must use direct file reads/writes only. This rule is absolute and applies to all agents spawned by this orchestrator.

---

When a user asks to **"add {feature} to {kernel}"** or describes a feature to implement for Blackhole, follow this workflow using the **Agent tool** to spawn subagents. Each agent runs in its own context.

The system is built around two reinforcement loops: a **compile loop** and a **test loop**, each iterating until the code passes or a maximum attempt count is reached.

A **watchdog** runs in the background (via cron or manually) to detect if the orchestrator is stuck and writes nudges that the orchestrator reads at each loop checkpoint.

---

## Progress Logging

At the start of **every step** and at the **top of every loop iteration**, write a progress update:

```python
import json, datetime
from pathlib import Path
progress = {
    "kernel": "{kernel}",
    "current_step": "{step name}",
    "step_started_at": datetime.datetime.utcnow().isoformat(),
    "last_heartbeat_at": datetime.datetime.utcnow().isoformat(),
    "compile_attempts": {compile_attempts},
    "test_attempts": {test_attempts},
    "last_action": "{one-line description of what just happened}",
    "last_compile_error": "{first line of last compiler error, or empty}",
    "prev_compile_error": "{first line of compiler error from previous attempt, or empty}",
}
Path("featuregen/artifacts/{kernel}_progress.json").write_text(json.dumps(progress, indent=2))
```

This is the watchdog's only window into your state. Keep it accurate.

---

## Watchdog Nudge Check

At the **top of every loop iteration** (both compile and test loops), before doing anything else:

```python
nudge_path = Path("featuregen/artifacts/WATCHDOG_NUDGE.md")
if nudge_path.exists():
    nudge = nudge_path.read_text()
    # READ THE NUDGE CAREFULLY
    # Act on its instructions
    # Then delete it:
    nudge_path.unlink()
```

If a nudge exists: **stop what you were about to do**, read the nudge, and follow its instructions. The watchdog has diagnosed a stall pattern and the nudge contains a concrete corrective action.

---

## Step -1: Validate Environment

Before starting, verify prerequisites:

```bash
cd featuregen
python -c "
import sys; sys.path.insert(0, '..')
from codegen.config.settings import settings
issues = settings.validate()
[print(f'ISSUE: {i}') for i in issues]
exit(1) if issues else print('Environment OK')
"
```

If any issues are reported, **stop and tell the user** what needs to be fixed.

---

## Step 0: Parse Request

From the user request, extract:
- **`kernel`** — the kernel name (e.g., `exp`, `gelu`, `relu`)
- **`kernel_path`** — `tt_llk_blackhole/common/inc/sfpu/ckernel_sfpu_{kernel}.h`
- **`feature_description`** — the feature to add (full description from user)

Confirm the kernel file exists:
```bash
ls tt_llk_blackhole/common/inc/sfpu/ckernel_sfpu_{kernel}.h
```

If the file does not exist, **stop and tell the user** — list available kernels:
```bash
ls tt_llk_blackhole/common/inc/sfpu/ckernel_sfpu_*.h
```

---

## Step 1: Backup Existing Kernel

Before any modifications, save the original:

```bash
cp tt_llk_blackhole/common/inc/sfpu/ckernel_sfpu_{kernel}.h featuregen/artifacts/{kernel}_original_backup.h
```

This backup is used for revert if all loops fail.

---

## Step 2: Analyze Existing Kernel and Feature Request

Spawn an agent:
```
Agent tool:
  subagent_type: "general-purpose"
  description: "Analyze {kernel} kernel and feature request"
  prompt: |
    Read and follow featuregen/agents/bh-feature-analyzer.md to analyze the "{kernel}" kernel.
    Kernel path: tt_llk_blackhole/common/inc/sfpu/ckernel_sfpu_{kernel}.h
    Feature request: {feature_description}
    Output your analysis to: featuregen/artifacts/{kernel}_feature_analysis.md
```

Wait for completion. **Verify** `featuregen/artifacts/{kernel}_feature_analysis.md` exists.

---

## Step 3: Plan the Changes

Spawn an agent:
```
Agent tool:
  subagent_type: "general-purpose"
  description: "Plan {feature_description} changes for {kernel}"
  prompt: |
    Read and follow featuregen/agents/bh-feature-planner.md to plan the feature changes.
    Kernel: {kernel}
    Kernel path: tt_llk_blackhole/common/inc/sfpu/ckernel_sfpu_{kernel}.h
    Feature request: {feature_description}
    Analysis: featuregen/artifacts/{kernel}_feature_analysis.md
    Output your plan to: featuregen/artifacts/{kernel}_feature_plan.md
```

Wait for completion. **Verify** `featuregen/artifacts/{kernel}_feature_plan.md` exists and contains: `target_file_path`, `changes_summary`, and `implementation_steps`.

---

## Step 4: Implement the Changes

Spawn an agent:
```
Agent tool:
  subagent_type: "general-purpose"
  description: "Implement {feature_description} in {kernel}"
  prompt: |
    Read and follow featuregen/agents/bh-feature-writer.md to implement the feature changes.
    Kernel: {kernel}
    Kernel path: tt_llk_blackhole/common/inc/sfpu/ckernel_sfpu_{kernel}.h
    Feature request: {feature_description}
    Plan: featuregen/artifacts/{kernel}_feature_plan.md
    Analysis: featuregen/artifacts/{kernel}_feature_analysis.md
    Edit the kernel file in place. Do NOT run compilation — the orchestrator handles that.
    Report what changes were made.
```

Wait for completion.

---

## Step 5: Compile Reinforcement Loop

Initialize: `compile_attempts = 0`, `max_compile_attempts = 5`

### COMPILE_LOOP:

Run compilation check:
```bash
cd featuregen
source ../tests/.venv/bin/activate
PYTHONPATH=.. python ../codegen/scripts/check_compile.py ../tt_llk_blackhole/common/inc/sfpu/ckernel_sfpu_{kernel}.h --arch blackhole -v
```

**If PASSED:** Go to **Step 5.5** (sync to tt-metal).

**If FAILED:**
- `compile_attempts += 1`
- If `compile_attempts > max_compile_attempts`: **COMPILE STUCK** — go to **Failure Recovery** below.
- Otherwise, spawn the compile debugger:

```
Agent tool:
  subagent_type: "general-purpose"
  description: "Fix compilation errors in {kernel} (attempt {compile_attempts}/{max_compile_attempts})"
  prompt: |
    Read and follow featuregen/agents/bh-compile-debugger.md to fix compilation errors.
    Kernel: {kernel}
    Kernel path: tt_llk_blackhole/common/inc/sfpu/ckernel_sfpu_{kernel}.h
    Feature being added: {feature_description}
    Plan: featuregen/artifacts/{kernel}_feature_plan.md
    Compiler errors:
    {paste full compiler error output here}
    Attempt: {compile_attempts} of {max_compile_attempts}
    Fix ONE root cause per iteration. Edit the file. Report what you changed.
```

After the debugger returns, go back to **COMPILE_LOOP**.

### Step 5.5: Sync to tt-metal

After compilation passes, copy the modified kernel to the tt-metal mirror before running tests:

```bash
cp tt_llk_blackhole/common/inc/sfpu/ckernel_sfpu_{kernel}.h \
   /proj_sw/user_dev/vvukomanovic/tt-metal/third-party/tt-llk/tt_llk_blackhole/common/inc/sfpu/ckernel_sfpu_{kernel}.h
```

Verify the copy succeeded (file exists at destination). Then go to **Step 6**.

### Failure Recovery (compile stuck):
Restore backup (tt-metal was not yet modified at this point, so only local restore needed):
```bash
cp featuregen/artifacts/{kernel}_original_backup.h tt_llk_blackhole/common/inc/sfpu/ckernel_sfpu_{kernel}.h
```
Report to user: "Compilation could not be fixed after {max_compile_attempts} attempts. Original kernel restored. Last errors: {errors}". **STOP**.

---

## Step 6: Test Reinforcement Loop

Initialize: `test_attempts = 0`, `max_test_attempts = 5`

### TEST_LOOP:

Spawn the tester agent:
```
Agent tool:
  subagent_type: "general-purpose"
  description: "Test {kernel} kernel (attempt {test_attempts+1})"
  prompt: |
    Read and follow featuregen/agents/bh-tester.md to run tests for the "{kernel}" kernel.
    Kernel: {kernel}
    Kernel path: tt_llk_blackhole/common/inc/sfpu/ckernel_sfpu_{kernel}.h
    Run quick mode first, then full tests if quick passes.
    Report: PASSED, FAILED (with details), or NOT_AVAILABLE (no tests for this kernel).
```

**If PASSED or NOT_AVAILABLE:** Go to **Step 7**.

**If FAILED:**
- `test_attempts += 1`
- If `test_attempts > max_test_attempts`: **TEST STUCK** — go to **Failure Recovery** below.
- Otherwise, spawn the test debugger:

```
Agent tool:
  subagent_type: "general-purpose"
  description: "Fix test failures in {kernel} (attempt {test_attempts}/{max_test_attempts})"
  prompt: |
    Read and follow featuregen/agents/bh-test-debugger.md to fix test failures.
    Kernel: {kernel}
    Kernel path: tt_llk_blackhole/common/inc/sfpu/ckernel_sfpu_{kernel}.h
    Feature being added: {feature_description}
    Plan: featuregen/artifacts/{kernel}_feature_plan.md
    Test failure output:
    {paste full test failure output here}
    Attempt: {test_attempts} of {max_test_attempts}
    Fix the logic error. Edit the file. Report what you changed.
```

After the test debugger returns:

1. **Recompile** to make sure the fix didn't break compilation:
```bash
cd featuregen
source ../tests/.venv/bin/activate
PYTHONPATH=.. python ../codegen/scripts/check_compile.py ../tt_llk_blackhole/common/inc/sfpu/ckernel_sfpu_{kernel}.h --arch blackhole -v
```
If compilation broke, run a quick compile debug cycle (max 2 attempts) before continuing.

2. **Re-sync to tt-metal** so the test picks up the fixed file:
```bash
cp tt_llk_blackhole/common/inc/sfpu/ckernel_sfpu_{kernel}.h \
   /proj_sw/user_dev/vvukomanovic/tt-metal/third-party/tt-llk/tt_llk_blackhole/common/inc/sfpu/ckernel_sfpu_{kernel}.h
```

Go back to **TEST_LOOP**.

### Failure Recovery (test stuck):
Restore backup in both locations:
```bash
cp featuregen/artifacts/{kernel}_original_backup.h tt_llk_blackhole/common/inc/sfpu/ckernel_sfpu_{kernel}.h
cp featuregen/artifacts/{kernel}_original_backup.h \
   /proj_sw/user_dev/vvukomanovic/tt-metal/third-party/tt-llk/tt_llk_blackhole/common/inc/sfpu/ckernel_sfpu_{kernel}.h
```
Report to user: "Tests failed after {max_test_attempts} attempts. Original kernel restored. Last failures: {failures}". **STOP**.

---

## Step 7: Report Completion

Write the summary to `featuregen/artifacts/{kernel}_feature_report.md` AND print it to the terminal:

```
========================================
  Blackhole Feature Gen — Complete
========================================
Kernel:           tt_llk_blackhole/common/inc/sfpu/ckernel_sfpu_{kernel}.h
tt-metal mirror:  /proj_sw/user_dev/vvukomanovic/tt-metal/third-party/tt-llk/tt_llk_blackhole/common/inc/sfpu/ckernel_sfpu_{kernel}.h
Feature:          {feature_description}
Compilation:      PASSED
Functional Tests: PASSED / NOT_AVAILABLE
Compile Attempts: {compile_attempts}
Test Attempts:    {test_attempts}
----------------------------------------
Artifacts:
  - featuregen/artifacts/{kernel}_original_backup.h
  - featuregen/artifacts/{kernel}_feature_analysis.md
  - featuregen/artifacts/{kernel}_feature_plan.md
  - featuregen/artifacts/{kernel}_feature_report.md
========================================
```

---

## Reinforcement Loop Summary

```
User request
    │
    ▼
Backup original ──────────────────────────────────────────────┐
    │                                                          │
    ▼                                                          │ REVERT
Analyze ──► Plan ──► Implement                                 │
                        │                                      │
                        ▼                                      │
              ┌──── COMPILE LOOP ─────┐                        │
              │                       │                        │
              │  compile ──► PASSED ──┼──► sync-to-tt-metal ──►┐  │
              │      │                │   │                    │
              │   FAILED              │   │                    │
              │      │                │   │                    │
              │  debug-compile        │   │                    │
              │      │ (max 5)        │   │                    │
              │      └──► STUCK ──────┼───┼──────────────────► │
              └───────────────────────┘   │                    │
                                          ▼                    │
                               ┌──── TEST LOOP ───────┐        │
                               │                       │       │
                               │  test ──► PASSED ─────┼──►┐  │
                               │     │                  │   │  │
                               │  FAILED                │   │  │
                               │     │                  │   │  │
                               │  debug-test            │   │  │
                               │     │ (max 5)          │   │  │
                               │     └──► STUCK ────────┼───┼─►│
                               └───────────────────────┘   │
                                                            ▼
                                                         REPORT
```

---

## Compile Command (Blackhole)

```bash
cd featuregen
source ../tests/.venv/bin/activate
PYTHONPATH=.. python ../codegen/scripts/check_compile.py ../tt_llk_blackhole/common/inc/sfpu/ckernel_sfpu_{kernel}.h --arch blackhole -v
```

---

## Watchdog Setup

Start the watchdog in a separate terminal before running featuregen. It polls every 5 minutes and writes a nudge file if the orchestrator appears stuck.

```bash
# Run once manually to check current state
python featuregen/scripts/watchdog.py -v

# Set up to run every 5 minutes via cron (add to crontab):
# */5 * * * * cd /proj_sw/user_dev/vvukomanovic/tt-llk && python featuregen/scripts/watchdog.py >> /tmp/featuregen_watchdog.log 2>&1

# Or run in a loop in a separate terminal:
watch -n 300 python featuregen/scripts/watchdog.py -v
```

If the watchdog detects a stall, it writes `featuregen/artifacts/WATCHDOG_NUDGE.md`. The orchestrator reads and acts on this at the top of each loop iteration.

For **deep diagnosis** (when the Python script flags a stall), spawn the watchdog agent manually:
```
Agent tool:
  subagent_type: "general-purpose"
  description: "Diagnose featuregen stall"
  prompt: |
    Read and follow featuregen/agents/bh-watchdog.md.
    The orchestrator appears stuck. Diagnose exactly what it's doing wrong
    and write a concrete nudge to featuregen/artifacts/WATCHDOG_NUDGE.md.
```

---

## Authoritative Sources

| Source | What it provides | How to access |
|--------|-----------------|---------------|
| **Existing BH implementations** | Working code patterns | Read `tt_llk_blackhole/common/inc/sfpu/*.h` |
| **BH ISA** | Instruction definitions | `grep` in `tt_llk_blackhole/instructions/assembly.yaml` (if exists) |
| **DeepWiki** | Blackhole ISA docs | DeepWiki MCP with repo `tenstorrent/tt-isa-documentation` |
| **Wormhole reference** | Older reference patterns | Read `tt_llk_wormhole_b0/common/inc/sfpu/*.h` |
