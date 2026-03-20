# Blackhole Feature Gen

Add new features to existing Blackhole SFPU kernels with automated compile + test reinforcement loops.

## Usage

Start Claude from the `featuregen/` directory:

```bash
cd featuregen
claude
> Add approximation mode to gelu
> Add integer support to exp
> Add configurable threshold to relu
```

## What It Does

1. **Analyzes** the existing kernel and decomposes the feature request
2. **Plans** exact code changes with before/after diffs
3. **Implements** the changes surgically
4. **Compile loop** — fixes errors automatically, up to 5 attempts
5. **Test loop** — fixes logic errors automatically, up to 5 attempts
6. **Reports** — writes a summary artifact and prints to terminal

If either loop exhausts its attempts, the original kernel is restored automatically.

## Compile Command (standalone)

```bash
cd featuregen
source ../tests/.venv/bin/activate
PYTHONPATH=.. python ../codegen/scripts/check_compile.py ../tt_llk_blackhole/common/inc/sfpu/ckernel_sfpu_{kernel}.h --arch blackhole -v
```

## Structure

```
featuregen/
├── CLAUDE.md          # Orchestrator — read this for the full workflow
├── agents/            # Agent playbooks
│   ├── bh-feature-analyzer.md
│   ├── bh-feature-planner.md
│   ├── bh-feature-writer.md
│   ├── bh-compile-debugger.md
│   ├── bh-tester.md
│   └── bh-test-debugger.md
├── references/        # Knowledge base
│   ├── blackhole-sources.md
│   ├── feature-guide.md
│   └── common-errors.md
└── artifacts/         # Generated during workflow (gitignored)
```
