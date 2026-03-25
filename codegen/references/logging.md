# Metrics and Logging System

The codegen system includes a structured metrics and logging system for tracking runs, debugging failures, and benchmarking quality over time.

---

## LOG_DIR: Per-Run Log Directory

Each codegen run creates a unique log directory:

```
/proj_sw/user_dev/$USER/codegen-metrics/logs/{RUN_ID}/
├── instructions/          # Snapshot of agent playbooks used
│   ├── llk-analyzer.md
│   ├── llk-planner.md
│   ├── llk-kernel-writer.md
│   ├── llk-debugger.md
│   ├── llk-tester.md
│   └── llk-prettifier.md
├── agent_analyzer.md      # Analyzer's reasoning log
├── agent_planner.md       # Planner's reasoning log
├── agent_writer.md        # Writer's reasoning log
├── agent_debugger.md      # Debugger's reasoning log (if invoked)
├── agent_tester.md        # Tester's reasoning log
└── orchestration.md       # Orchestrator's high-level log
```

**RUN_ID format**: `{YYYY-MM-DD}_{kernel}_{arch}_{random_hex}`

The `LOG_DIR` is passed to every agent prompt. Agents write their reasoning to `{LOG_DIR}/agent_{name}.md`.

---

## runs.jsonl: Cumulative Run Metrics

After each run completes, the orchestrator appends a JSONL entry to:
```
codegen/artifacts/runs.jsonl
```

### Entry Format

```json
{
  "kernel": "gelu",
  "kernel_type": "sfpu",
  "arch": "quasar",
  "reference_arch": "blackhole",
  "reference_file": "tt_llk_blackhole/common/inc/sfpu/ckernel_sfpu_gelu.h",
  "generated_file": "tt_llk_quasar/common/inc/sfpu/ckernel_sfpu_gelu.h",
  "start_time": "2026-03-24T14:30:00Z",
  "end_time": "2026-03-24T14:45:12Z",
  "phases_total": 2,
  "phases_completed": 2,
  "compilation_attempts": 4,
  "debug_cycles": 1,
  "tests_total": 12,
  "tests_passed": 12,
  "lines_generated": 187,
  "prettified": true,
  "status": "success",
  "obstacle": null,
  "per_phase": [
    {"phase": 1, "name": "basic", "compilation_attempts": 1, "debug_cycles": 0, "test_result": "passed"},
    {"phase": 2, "name": "derivative", "compilation_attempts": 3, "debug_cycles": 1, "test_result": "passed"}
  ],
  "agents": ["analyzer", "planner", "writer", "tester", "debugger"],
  "run_id": "2026-03-24_gelu_quasar_a1b2c3d4",
  "log_dir": "logs/2026-03-24_gelu_quasar_a1b2c3d4"
}
```

**Written as a single JSONL line** (expanded above for readability).

### Fields

| Field | Type | Description |
|-------|------|-------------|
| `kernel` | string | Kernel name (e.g., "gelu", "reduce") |
| `kernel_type` | string | Category: "sfpu", "math", "pack", or "unpack" |
| `arch` | string | Target architecture |
| `reference_arch` | string | Reference architecture ported from |
| `reference_file` | string | Path to source reference file |
| `generated_file` | string | Path to generated output file |
| `start_time` | string | ISO 8601 UTC timestamp when run started |
| `end_time` | string | ISO 8601 UTC timestamp when run finished |
| `phases_total` | number | Number of sub-kernel phases identified |
| `phases_completed` | number | Number of phases that passed |
| `compilation_attempts` | number | Total compile invocations across all phases |
| `debug_cycles` | number | Total debug agent invocations across all phases |
| `tests_total` | number | Total test cases run |
| `tests_passed` | number | Total test cases passed |
| `lines_generated` | number | Line count of generated file |
| `prettified` | boolean | Whether prettification succeeded |
| `status` | string | "success" or "failed" |
| `obstacle` | string/null | Main blocker if failed, null if success |
| `per_phase` | array | Per-phase breakdown (see below) |
| `agents` | array | List of agents that were invoked |
| `run_id` | string | Unique run identifier |
| `log_dir` | string | Relative path to LOG_DIR |

### Per-Phase Entry

| Field | Type | Description |
|-------|------|-------------|
| `phase` | number | Phase number (1-indexed) |
| `name` | string | Short label for the sub-kernel group |
| `compilation_attempts` | number | Compile invocations for this phase only |
| `debug_cycles` | number | Debug agent invocations for this phase only |
| `test_result` | string | "passed", "failed", or "skipped" |

---

## Key Metrics for Benchmarking

When building a dashboard from `runs.jsonl`, these are the most useful metrics:

### Quality Metrics
- **Success rate**: `status == "success"` / total runs — per kernel_type and overall
- **First-try compilation rate**: runs where `compilation_attempts == phases_total` (writer got it right every time)
- **Debug overhead**: `compilation_attempts - phases_total` — extra compiles needed beyond the minimum
- **Test pass rate**: `tests_passed / tests_total`

### Efficiency Metrics
- **Duration**: `end_time - start_time` — wall-clock time per run
- **Compile attempts per phase**: `compilation_attempts / phases_total` — lower is better
- **Debug cycles per phase**: `debug_cycles / phases_total`

### Trending Over Time
- Group by `start_time` (weekly) to see quality trends
- Compare same kernel across runs to measure improvement
- Track `lines_generated` for code bloat trends

### Example Queries
```bash
# Success rate
jq -s '[.[] | .status] | group_by(.) | map({(.[0]): length})' codegen/artifacts/runs.jsonl

# Average compile attempts by kernel type
jq -s 'group_by(.kernel_type) | map({type: .[0].kernel_type, avg_compiles: ([.[].compilation_attempts] | add / length)})' codegen/artifacts/runs.jsonl

# Failed runs with obstacles
jq 'select(.status == "failed") | {kernel, obstacle}' codegen/artifacts/runs.jsonl

# Worst phases (most debug cycles)
jq '.per_phase[] | select(.debug_cycles > 0) | {kernel: input_filename, phase: .name, debug_cycles}' codegen/artifacts/runs.jsonl
```

---

## Agent Self-Logging

Every agent writes a reasoning log to `{LOG_DIR}/agent_{name}.md` during execution. The log includes:

- **Files read** — which files were read and why
- **Key findings** — important patterns, issues, or decisions discovered
- **Decisions made** — what approach was chosen and why
- **Surprises** — anything unexpected or non-obvious

If no `LOG_DIR` is provided, agents skip logging (backward compatible).

---

## Reviewing Logs After a Run

### Quick status check
```bash
# See all runs
cat codegen/artifacts/runs.jsonl

# See failed runs
jq 'select(.status == "failed")' codegen/artifacts/runs.jsonl

# See runs for a specific kernel
jq 'select(.kernel == "gelu")' codegen/artifacts/runs.jsonl

# See runs for a specific kernel type
jq 'select(.kernel_type == "sfpu")' codegen/artifacts/runs.jsonl
```

### Deep dive into a run
```bash
# List log files for a run
ls /proj_sw/user_dev/$USER/codegen-metrics/logs/{RUN_ID}/

# Read a specific agent's reasoning
cat /proj_sw/user_dev/$USER/codegen-metrics/logs/{RUN_ID}/agent_analyzer.md

# Compare agent playbooks used vs current
diff /proj_sw/user_dev/$USER/codegen-metrics/logs/{RUN_ID}/instructions/llk-planner.md codegen/agents/llk-planner.md
```
