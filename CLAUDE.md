# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## CRITICAL: No Git Commands

**NEVER use any git commands** in this repository. No `git diff`, `git status`, `git log`, `git show`, `git checkout`, `git restore`, etc. All file operations must use direct file reads/writes only.

## LLK CodeGen System

To generate kernels for a target architecture, start Claude from the `codegen/` folder:
```bash
cd codegen
claude
> Generate gelu for Quasar
```

The codegen system discovers architectural patterns dynamically from Confluence, DeepWiki, assembly.yaml, and existing implementations. See [codegen/CLAUDE.md](codegen/CLAUDE.md) for orchestrator details.

## Repository Structure

```
tt-llk/
├── tt_llk_quasar/           # Quasar LLK implementations
├── tt_llk_blackhole/        # Blackhole LLK (reference)
├── tt_llk_wormhole_b0/      # Wormhole LLK (reference)
├── codegen/                 # AI code generation system
│   ├── CLAUDE.md            # Orchestrator instructions
│   ├── agents/              # Agent playbooks
│   ├── references/          # Knowledge base
│   ├── artifacts/           # Generated artifacts
│   └── scripts/             # Tools
└── tests/                   # Test infrastructure
```

## Commands

### Environment Setup
```bash
cd tests
./setup_testing_env.sh
```

### MCP Servers
Pre-configured in `.mcp.json`. Atlassian requires authentication on first use.
- **Atlassian** — Search Confluence for target architecture specs (Quasar/NEO: page ID 84508873)
- **DeepWiki** — Query `tenstorrent/tt-isa-documentation` for reference architecture ISA docs

### Compilation Check
```bash
cd codegen
source ../tests/.venv/bin/activate
PYTHONPATH=.. python scripts/check_compile.py ../tt_llk_quasar/common/inc/sfpu/ckernel_sfpu_{op}.h -v
```

### Run Tests
```bash
cd codegen
source ../tests/.venv/bin/activate
python scripts/run_test.py --list
python scripts/run_test.py {test_name} -v
```

## Key Files

| File | Purpose |
|------|---------|
| `tt_llk_quasar/instructions/assembly.yaml` | Quasar ISA (277KB, use grep) |
| `tt_llk_blackhole/common/inc/sfpu/*.h` | Reference SFPU implementations |
| `tt_llk_quasar/common/inc/sfpu/*.h` | Quasar SFPU implementations |
