# LLK CodeGen - Claude Code Instructions

## Project Overview

This is an **AI-powered LLK (Low-Level Kernel) code generation system** for Tenstorrent hardware. It uses Claude API to generate LLK code for Quasar (and future architectures) based on existing Wormhole/Blackhole implementations.

## Key Documents

- [PLAN.md](./PLAN.md) - Full implementation plan with architecture diagrams
- [llk-codegen-architecture.excalidraw](./llk-codegen-architecture.excalidraw) - Visual architecture diagram (import into excalidraw.com)

## Architecture Summary

```
User Request → Context Assembly → Claude API (with tools) → N Candidates
                                       ↓
                              Compile All N (parallel)
                                       ↓
                    ✅ Success → Functional Test → ✅ Validated LLK
                    ❌ Errors → Regenerate (loop)
```

### What Uses Claude API (LLM)

| Component | Uses LLM? |
|-----------|-----------|
| Context Assembly | No (Python) |
| Code Generator | **Yes** |
| Compile Agent | No (SFPI compiler) |
| Functional Test Agent | No (Quasar emulator) |
| Improvement Suggester | **Yes** |

## Project Structure

```
codegen/
├── cli.py                      # CLI entry point
├── config/                     # Configuration
│   └── settings.py
├── context/                    # Context assembly (no LLM)
│   ├── assembly_parser.py      # Parse assembly.yaml
│   ├── llk_examples.py         # Load WH/BH LLKs
│   └── context_builder.py      # Build context string
├── generator/                  # Code generation (uses Claude API)
│   ├── claude_client.py        # Claude API wrapper
│   ├── prompt_templates.py     # Prompts + tool definitions
│   └── code_generator.py       # Main generation logic
├── agents/                     # Validation agents (no LLM)
│   ├── compile_agent.py        # SFPI compilation
│   └── functional_agent.py     # Quasar emulator tests
├── feedback/                   # Feedback loop
│   ├── feedback_loop.py        # Orchestrator
│   └── error_analyzer.py       # Parse errors
└── library/                    # Validated LLK storage
```

## Key Files to Reference

| File | Purpose |
|------|---------|
| `tt_llk_quasar/instructions/assembly.yaml` | Quasar instruction set (277KB) |
| `tt_llk_blackhole/common/inc/sfpu/*.h` | Reference SFPU implementations |
| `tt_llk_wormhole_b0/llk_lib/*.h` | Reference LLK implementations |
| `tests/python_tests/helpers/` | Test infrastructure to reuse |

## Claude API Tools (Agentic Context)

The Claude LLM can request additional context via tools:

```python
TOOLS = [
    {
        "name": "get_instruction_details",
        "description": "Get details about a Quasar instruction from assembly.yaml",
        "input_schema": {"instruction_name": "string"}
    },
    {
        "name": "get_similar_impl",
        "description": "Get similar LLK implementation from WH/BH",
        "input_schema": {"operation": "string", "architecture": "string"}
    }
]
```

## Running the System

```bash
# Set API key
export ANTHROPIC_API_KEY="your-key"

# Generate a kernel
python -m codegen.cli generate \
    --arch quasar \
    --op sfpu_sigmoid \
    --reference blackhole \
    --validate
```

## Development Guidelines

1. **Modularity** - All components have clear interfaces, easy to swap
2. **Configuration** - Use `config/settings.py` for all tunable parameters
3. **Testing** - Add tests in `tests/` for each new component
4. **Extensibility** - New context sources/validators can be registered

## Phase 0 Target

- **Goal**: E2E pipeline for simple SFPU operation (sigmoid/relu)
- **Duration**: 2 weeks
- **Success**: Code compiles + passes functional test in ≤5 iterations

## Dependencies

```
anthropic>=0.20.0
pyyaml>=6.0
pytest>=7.0
```
