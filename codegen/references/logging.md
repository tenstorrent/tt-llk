# Agent Execution Logging

Optional logging system for debugging agent execution.

## Enabling Logging

Before running the workflow, enable logging for an operation:

```bash
./scripts/logging/set_logging.sh {operation} --enable
```

Example:
```bash
./scripts/logging/set_logging.sh gelu --enable
```

## Disabling Logging

```bash
./scripts/logging/set_logging.sh {operation} --disable
```

## Log Location

Logs are written to: `codegen/artifacts/{operation}_log.md`

---

## For Agents: How to Log

### Step 1: Check if Logging is Enabled

At the start of execution, check if logging is enabled:

```bash
./scripts/logging/check_logging.sh {operation}
```

If exit code is 0 → logging enabled, follow logging steps below.
If exit code is 1 → logging disabled, skip all logging.

### Step 2: Initialize Log Section

If logging is enabled, initialize your agent's section:

```bash
./scripts/logging/init_log.sh {operation} {agent_name}
```

Example:
```bash
./scripts/logging/init_log.sh gelu llk-analyzer
```

### Step 3: Log Events During Execution

Use `append_log.sh` to log key events:

```bash
./scripts/logging/append_log.sh {operation} {event_type} "{message}"
```

#### Event Types

| Type | When to Use |
|------|-------------|
| `action` | Before performing an action |
| `result` | After action completes |
| `error` | When an error occurs |
| `hypothesis` | When forming a theory about an error |
| `recovery` | When attempting a fix |
| `complete` | At the end (with final status) |

#### Examples

```bash
# Log an action
./scripts/logging/append_log.sh gelu action "Reading BH reference implementation"

# Log a result
./scripts/logging/append_log.sh gelu result "Found sigmoid uses lut2 approach"

# Log an error
./scripts/logging/append_log.sh gelu error "Compilation failed: undefined TTI_SFPEXP"

# Log a hypothesis
./scripts/logging/append_log.sh gelu hypothesis "TTI_SFPEXP doesn't exist, need SFPNONLINEAR with EXP_MODE"

# Log recovery attempt
./scripts/logging/append_log.sh gelu recovery "Replaced TTI_SFPEXP with TTI_SFPNONLINEAR"

# Log completion
./scripts/logging/append_log.sh gelu complete "SUCCESS - kernel compiles"
```

### Step 4: Complete Log Section

At the end of execution, log completion status:

```bash
./scripts/logging/append_log.sh {operation} complete "SUCCESS|FAILED - {summary}"
```

---

## Example Log Output

After running the full workflow with logging enabled, `artifacts/gelu_log.md` might look like:

```markdown
---

## llk-analyzer
**Started**: 2024-03-04 15:30:00

### Events
- [15:30:05] **Action**: Reading BH reference at tt_llk_blackhole/common/inc/sfpu/ckernel_sfpu_gelu.h
- [15:30:10] **Result**: Found GELU uses approximation: 0.5 * x * (1 + tanh(...))
- [15:30:15] **Action**: Writing analysis to artifacts/gelu_analysis.md

**Completed**: 2024-03-04 15:30:20
**Status**: SUCCESS - Analysis complete

---

## llk-planner
**Started**: 2024-03-04 15:30:25

### Events
- [15:30:30] **Action**: Reading analysis and Quasar architecture docs
- [15:30:40] **Result**: Mapped to TTI_SFPNONLINEAR with TANH_MODE
- [15:30:45] **Action**: Writing spec to artifacts/gelu_spec.md

**Completed**: 2024-03-04 15:30:50
**Status**: SUCCESS - Spec complete

---

## llk-kernel-writer
**Started**: 2024-03-04 15:31:00

### Events
- [15:31:05] **Action**: Generating kernel from spec
- [15:31:15] **Action**: Running compilation check
- [15:31:20] **Error**: Compilation failed: 'LCONST_half' not defined
- [15:31:22] **Hypothesis**: Need to use TTI_SFPLOADI for 0.5 constant

**Completed**: 2024-03-04 15:31:25
**Status**: FAILED - Needs debugger

---

## llk-debugger
**Started**: 2024-03-04 15:31:30

### Events
- [15:31:35] **Action**: Reading error and common-errors.md
- [15:31:40] **Recovery**: Replaced LCONST_half with TTI_SFPLOADI(LREG5, 0, 0x3F00)
- [15:31:50] **Action**: Running compilation check
- [15:31:55] **Result**: Compilation successful

**Completed**: 2024-03-04 15:32:00
**Status**: SUCCESS - Fixed 1 error
```

---

## Orchestrator: Post-Workflow Log Review

If logging was enabled, after the workflow completes:

1. Read the log: `codegen/artifacts/{operation}_log.md`
2. Summarize for user:
   - Which agents ran
   - Any errors encountered
   - Recovery actions taken
   - Final status
