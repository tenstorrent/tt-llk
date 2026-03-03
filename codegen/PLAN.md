# LLK Code Generation System - Implementation Plan

## Context

**Problem**: Generating Low-Level Kernels (LLKs) for new Tenstorrent architectures (Quasar) is labor-intensive and requires deep hardware expertise. Existing WH/BH LLKs can serve as templates, but significant manual effort is needed.

**Solution**: Build an AI-powered LLK code generation system with:
- LLM-based code generation using architecture context
- Automated compilation → functional test → performance test feedback loop
- Iterative refinement until kernels pass validation

**Outcome**: Accelerate LLK development for Quasar and future architectures by 10x.

---

## Architecture Overview

### What is What?

| Component | Type | Description |
|-----------|------|-------------|
| **Claude Code** | AI Assistant | ME - helping you design/build this system (not part of final tool) |
| **CodeGen CLI** | Python Tool | The standalone tool we're building |
| **Claude API + Tools** | LLM via API | Claude can call tools to get more context dynamically |
| **Compile Agent** | Python Code | Runs SFPI compiler (no LLM) |
| **Functional Test Agent** | Python Code | Runs tests on emulator (no LLM) |

### Key Architecture Concepts

**1. Agentic Context Gathering (Claude with Tools)**

The Claude LLM can *request* additional context during generation using tools:
- `get_instruction_details(name)` - Fetch detailed instruction info from assembly.yaml
- `get_similar_impl(op, arch)` - Fetch similar LLK implementation from WH/BH
- `explore_rtl(unit)` - [Phase 1] Query RTL for hardware details

This makes context gathering dynamic, not just a fixed input.

**2. Multi-Candidate Generation (Beam Search)**

Instead of generating 1 kernel at a time:
- Generate N candidates (e.g., 3-5 variants)
- Compile all N in parallel
- Successful compiles → proceed to functional test
- Failed compiles → loop back with error context for regeneration
- This increases success rate and explores different approaches

### System Diagram (Excalidraw-ready)

```
┌─────────────────────────────────────────────────────────────────────────────────┐
│                           LLK CodeGen CLI (Python Tool)                          │
│                                                                                 │
│  ┌───────────────────────────────────────────────────────────────────────────┐  │
│  │                    1. INITIAL CONTEXT (Python)                            │  │
│  │  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐                    │  │
│  │  │ assembly.yaml│  │  WH/BH LLKs  │  │  Arch TLDR   │                    │  │
│  │  │   (parsed)   │  │  (loaded)    │  │   Docs       │                    │  │
│  │  └──────────────┘  └──────────────┘  └──────────────┘                    │  │
│  │                           │                                              │  │
│  │                           ▼                                              │  │
│  │                  ┌─────────────────┐                                     │  │
│  │                  │ Initial Context │                                     │  │
│  │                  └────────┬────────┘                                     │  │
│  └───────────────────────────┼───────────────────────────────────────────────┘  │
│                              │                                                  │
│                              ▼                                                  │
│  ┌───────────────────────────────────────────────────────────────────────────┐  │
│  │        2. 🤖 CLAUDE API + TOOLS (Agentic Code Generation)                 │  │
│  │                                                                           │  │
│  │  ┌────────────────────────────────────────────────────────────────────┐  │  │
│  │  │   Claude can REQUEST more context via tools:                       │  │  │
│  │  │                                                                    │  │  │
│  │  │   ┌─────────────────────┐  ┌─────────────────────┐                │  │  │
│  │  │   │ get_instruction()   │  │ get_similar_impl()  │                │  │  │
│  │  │   │ → assembly.yaml     │  │ → WH/BH LLK code    │                │  │  │
│  │  │   └─────────────────────┘  └─────────────────────┘                │  │  │
│  │  │                                                                    │  │  │
│  │  │   ┌─────────────────────┐  ┌─────────────────────┐                │  │  │
│  │  │   │ explore_rtl()       │  │ search_docs()       │                │  │  │
│  │  │   │ → RTL details [P1]  │  │ → architecture docs │                │  │  │
│  │  │   └─────────────────────┘  └─────────────────────┘                │  │  │
│  │  └────────────────────────────────────────────────────────────────────┘  │  │
│  │                              │                                           │  │
│  │                              ▼                                           │  │
│  │                    ┌──────────────────┐                                  │  │
│  │                    │ Generate N       │  (N = 3-5 candidates)            │  │
│  │                    │ Code Variants    │                                  │  │
│  │                    └────────┬─────────┘                                  │  │
│  └─────────────────────────────┼─────────────────────────────────────────────┘  │
│                                │                                                │
│                                ▼                                                │
│  ┌───────────────────────────────────────────────────────────────────────────┐  │
│  │              3. COMPILE AGENT (Python - NO LLM) - Parallel                │  │
│  │                                                                           │  │
│  │  ┌────────────┐  ┌────────────┐  ┌────────────┐       ┌────────────┐     │  │
│  │  │ Candidate 1│  │ Candidate 2│  │ Candidate 3│  ...  │ Candidate N│     │  │
│  │  └─────┬──────┘  └─────┬──────┘  └─────┬──────┘       └─────┬──────┘     │  │
│  │        │               │               │                    │            │  │
│  │        ▼               ▼               ▼                    ▼            │  │
│  │  ┌──────────────────────────────────────────────────────────────────┐    │  │
│  │  │                    SFPI Compiler (parallel)                      │    │  │
│  │  └──────────────────────────────────────────────────────────────────┘    │  │
│  │        │               │               │                    │            │  │
│  │        ▼               ▼               ▼                    ▼            │  │
│  │     ✅ Binary      ❌ Error        ✅ Binary            ❌ Error        │  │
│  └───────┬────────────────┬───────────────┬─────────────────────┬───────────┘  │
│          │                │               │                     │               │
│          │      ┌─────────┴───────────────┴─────────────────────┘               │
│          │      │                                                               │
│          │      ▼                                                               │
│          │   ┌──────────────────────────────────────────────────────────────┐  │
│          │   │  FAILED CANDIDATES → 🤖 Claude API (analyze + regenerate)   │  │
│          │   │                                                              │  │
│          │   │  • Parse compiler errors                                     │  │
│          │   │  • Send errors + context to Claude                          │  │
│          │   │  • Generate improved variants                               │  │
│          │   │  • Loop back to Compile                                     │  │
│          │   └──────────────────────────────────────────────────────────────┘  │
│          │                                                                      │
│          ▼                                                                      │
│  ┌───────────────────────────────────────────────────────────────────────────┐  │
│  │           4. FUNCTIONAL TEST (Python - NO LLM) - Parallel                 │  │
│  │                                                                           │  │
│  │  ┌─────────────────────────────────────────────────────────────────┐     │  │
│  │  │   Run all successful binaries on Quasar emulator                │     │  │
│  │  │                                                                 │     │  │
│  │  │   Binary 1 ──▶ Emulator ──▶ Compare vs Golden ──▶ ✅ Pass      │     │  │
│  │  │   Binary 2 ──▶ Emulator ──▶ Compare vs Golden ──▶ ❌ Fail      │     │  │
│  │  │   ...                                                           │     │  │
│  │  └─────────────────────────────────────────────────────────────────┘     │  │
│  │          │                                                               │  │
│  │          ▼                                                               │  │
│  │     ┌────────────────────────────────────────────────────────────┐       │  │
│  │     │  FAILED TESTS → 🤖 Claude API (analyze + regenerate)      │       │  │
│  │     │  Loop back to Generation                                  │       │  │
│  │     └────────────────────────────────────────────────────────────┘       │  │
│  └───────────────────────────────────────────────────────────────────────────┘  │
│                    │                                                            │
│                    ▼                                                            │
│  ┌───────────────────────────────────────────────────────────────────────────┐  │
│  │                      5. ✅ VALIDATED LLK LIBRARY                          │  │
│  │                                                                           │  │
│  │   Store successful implementations for:                                   │  │
│  │   • Future reference examples                                            │  │
│  │   • Training data for prompt improvement                                 │  │
│  │   • Baseline comparisons                                                 │  │
│  └───────────────────────────────────────────────────────────────────────────┘  │
│                                                                                 │
└─────────────────────────────────────────────────────────────────────────────────┘

Legend:
  🤖 = Uses Claude API (LLM)
  Python = Regular Python code (no LLM)
  [P1] = Phase 1 feature
```

### Simplified Flow (Phase 0)

```
                                    ┌─────────────────────────────────────────────┐
                                    │                                             │
User Request ──▶ Initial   ──▶ 🤖 Claude API  ──▶  N Candidates                  │
                 Context       (with tools)        │                              │
                                                   │                              │
                                                   ▼                              │
                                            ┌─────────────┐                       │
                                            │  Compile    │                       │
                                            │  All N      │◀──────────────────────┤
                                            └──────┬──────┘                       │
                                                   │                              │
                                    ┌──────────────┴──────────────┐               │
                                    │                             │               │
                                    ▼                             ▼               │
                              ✅ Binaries                   ❌ Errors ────────────┤
                                    │                       (regenerate)          │
                                    ▼                                             │
                             ┌─────────────┐                                      │
                             │ Functional  │                                      │
                             │ Test        │◀─────────────────────────────────────┤
                             └──────┬──────┘                                      │
                                    │                                             │
                     ┌──────────────┴──────────────┐                              │
                     │                             │                              │
                     ▼                             ▼                              │
               ✅ Passed                     ❌ Failed ───────────────────────────┘
                     │                       (regenerate)
                     ▼
            ┌────────────────┐
            │ Validated LLK  │
            │ (stored)       │
            └────────────────┘
```

---

## Project Structure

**Location**: `tt_metal/third_party/tt_llk/codegen/` (inside tt_llk submodule for tight integration)

```
tt_metal/third_party/tt_llk/
├── codegen/                            # NEW: LLK Code Generation System
│   ├── __init__.py
│   ├── cli.py                          # CLI entry point
│   ├── config/
│   │   ├── __init__.py
│   │   ├── settings.py                 # Global settings, Claude API key
│   │   └── architectures.yaml          # Architecture-specific configs
│   │
│   ├── context/                        # Context assembly module
│   │   ├── __init__.py
│   │   ├── assembly_parser.py          # Parse assembly.yaml files
│   │   ├── llk_examples.py             # Load WH/BH LLK examples
│   │   ├── doc_loader.py               # Load architecture docs
│   │   ├── context_builder.py          # Build LLM context from sources
│   │   └── architecture_tldr/          # Curated architecture summaries
│   │       ├── quasar.md
│   │       ├── blackhole.md
│   │       └── wormhole.md
│   │
│   ├── generator/                      # Code generation module (🤖 uses Claude API)
│   │   ├── __init__.py
│   │   ├── claude_client.py            # Claude API client wrapper
│   │   ├── prompt_templates.py         # Prompt engineering templates
│   │   ├── code_generator.py           # Main generation logic (calls Claude)
│   │   └── output_parser.py            # Parse LLM output to code (no LLM)
│   │
│   ├── agents/                         # Validation agents
│   │   ├── __init__.py
│   │   ├── base_agent.py               # Base agent interface
│   │   ├── compile_agent.py            # SFPI compilation agent (no LLM)
│   │   ├── functional_agent.py         # Functional test - Quasar emulator (no LLM)
│   │   ├── performance_agent.py        # [Phase 1] Performance test agent
│   │   └── rtl_explorer.py             # [Phase 1] RTL exploration
│   │
│   ├── feedback/                       # Feedback loop system
│   │   ├── __init__.py
│   │   ├── feedback_loop.py            # Orchestrate iterations (Python orchestrator)
│   │   ├── error_analyzer.py           # Parse compile/test errors (no LLM)
│   │   └── improvement_suggester.py    # Suggest fixes (🤖 uses Claude API)
│   │
│   ├── library/                        # Validated LLK library
│   │   ├── __init__.py
│   │   ├── library_manager.py          # Manage validated LLKs
│   │   └── examples/                   # Stored validated examples
│   │
│   ├── tests/                          # Unit and integration tests
│   │   ├── __init__.py
│   │   ├── test_context_builder.py
│   │   ├── test_code_generator.py
│   │   ├── test_agents.py
│   │   └── test_feedback_loop.py
│   │
│   └── scripts/                        # Utility scripts
│       ├── setup_context.sh            # Initial context setup
│       └── run_generation.py           # Run generation pipeline
│
├── tt_llk_quasar/                      # (existing) Quasar LLK target
├── tt_llk_blackhole/                   # (existing) Blackhole LLK target
├── tt_llk_wormhole_b0/                 # (existing) Wormhole LLK target
└── tests/                              # (existing) Test infrastructure
```

---

## Phased Implementation

### Pre-Phase: Documentation Setup

**Copy plan documents to shareable location:**
```bash
# Copy to /proj_sw/user_dev/vvukomanovic/ for sharing with team
cp ~/.claude/plans/linked-singing-meadow.md /proj_sw/user_dev/vvukomanovic/llk-codegen-plan.md
cp ~/.claude/plans/llk-codegen-architecture.excalidraw /proj_sw/user_dev/vvukomanovic/
```

---

### Phase 0: Foundation (2 weeks, 1 person)

**Goal**: End-to-end pipeline working for a simple SFPU operation (sigmoid or relu)

**First Target Kernel**: Port sigmoid/relu from Blackhole to Quasar as proof-of-concept

#### Week 1: Context & Generation Setup

| Task | Description | Files |
|------|-------------|-------|
| 0.1 | Create project structure | `tools/llk_codegen/` |
| 0.2 | Implement assembly.yaml parser | `context/assembly_parser.py` |
| 0.3 | Implement WH/BH LLK example loader | `context/llk_examples.py` |
| 0.4 | Write Quasar architecture TLDR | `context/architecture_tldr/quasar.md` |
| 0.5 | Create initial prompt templates | `generator/prompt_templates.py` |
| 0.6 | Implement Claude API client | `generator/claude_client.py` |
| 0.7 | Basic code generator | `generator/code_generator.py` |

**Critical Files to Reference**:
- `tt_metal/third_party/tt_llk/tt_llk_quasar/instructions/assembly.yaml` (277KB instruction set)
- `tt_metal/third_party/tt_llk/tt_llk_blackhole/common/inc/sfpu/*.h` (example LLKs)
- `tt_metal/third_party/tt_llk/tt_llk_wormhole_b0/llk_lib/*.h` (example LLKs)

#### Week 2: Compilation & Feedback Loop

| Task | Description | Files |
|------|-------------|-------|
| 0.8 | Implement SFPI compilation agent | `agents/compile_agent.py` |
| 0.9 | Implement functional test agent (Quasar emulator) | `agents/functional_agent.py` |
| 0.10 | Create feedback loop (compile + functional only) | `feedback/feedback_loop.py` |
| 0.11 | Error analyzer for compile/test errors | `feedback/error_analyzer.py` |
| 0.12 | CLI for running pipeline | `cli.py` |
| 0.13 | Integration test with sigmoid SFPU | `tests/test_feedback_loop.py` |

**Note**: Performance testing deferred to Phase 1.

**Critical Files to Reference**:
- `runtime/sfpi/compiler/bin/riscv-tt-elf-gcc` (SFPI compiler)
- `tt_metal/third_party/tt_llk/tests/python_tests/` (test patterns)
- `tt_metal/third_party/tt_llk/tests/Makefile` (build patterns)

---

### Phase 1: Enhancement (1 month, 2-3 people)

**Goal**: Production-quality system with RTL exploration and INT8 support

#### Week 1-2: RTL Explorer & Context Enhancement

| Task | Description | Owner |
|------|-------------|-------|
| 1.1 | RTL explorer agent | Person 1 |
| 1.2 | Integrate SharePoint docs | Person 1 |
| 1.3 | Create comprehensive instruction TLDR | Person 2 |
| 1.4 | Improve context relevance scoring | Person 2 |

#### Week 3-4: RL Prompt Refinement & INT8

| Task | Description | Owner |
|------|-------------|-------|
| 1.5 | Implement prompt refinement with RL | Person 1 |
| 1.6 | INT8 datatype support | Person 2 |
| 1.7 | P1 kernels for Quasar | Person 3 |
| 1.8 | Performance benchmarking suite | Person 3 |
| 1.9 | Validated LLK library management | All |

---

## Detailed Component Specifications

### 1. Context Builder (`context/context_builder.py`)

```python
class ContextBuilder:
    """Assembles context for LLM from multiple sources"""

    def build_context(
        self,
        target_arch: str,           # "quasar", "blackhole", "wormhole"
        operation_type: str,        # "sfpu_unary", "sfpu_binary", "matmul"
        operation_name: str,        # "sigmoid", "relu", "add"
        include_similar_ops: bool = True,
        max_context_tokens: int = 100000
    ) -> str:
        """
        Returns assembled context string including:
        - Relevant instructions from assembly.yaml
        - Similar LLK implementations from WH/BH
        - Architecture-specific documentation
        """
```

**Key Data Sources**:
| Source | Location | Usage |
|--------|----------|-------|
| assembly.yaml | `tt_llk_*/instructions/assembly.yaml` | Instruction definitions |
| WH LLKs | `tt_llk_wormhole_b0/llk_lib/*.h` | Reference implementations |
| BH LLKs | `tt_llk_blackhole/llk_lib/*.h` | Reference implementations |
| SFPU impls | `tt_llk_*/common/inc/sfpu/*.h` | SFPU kernel examples |
| Arch TLDR | `context/architecture_tldr/*.md` | Curated summaries |

### 2. Code Generator (`generator/code_generator.py`)

```python
class LLKCodeGenerator:
    """Generates LLK code using LLM"""

    def generate(
        self,
        task_description: str,      # "Add math fidelity to eltwise binary broadcast"
        target_arch: str,
        base_implementation: str = None,  # Optional starting point
        constraints: list[str] = None     # Performance/correctness requirements
    ) -> GeneratedCode:
        """
        Returns:
        - header_code: str (the .h file content)
        - implementation_notes: str
        - expected_test_cases: list[str]
        """
```

**Prompt Template Structure**:
```
<architecture_context>
{assembly_yaml_relevant_instructions}
{architecture_tldr}
</architecture_context>

<reference_implementations>
{wormhole_implementation}
{blackhole_implementation}
</reference_implementations>

<task>
{task_description}
</task>

<constraints>
{performance_constraints}
{correctness_requirements}
</constraints>

Generate a Quasar LLK implementation following the patterns shown...
```

### 3. Compilation Agent (`agents/compile_agent.py`)

```python
class CompileAgent:
    """Compiles generated LLK code using SFPI compiler"""

    def compile(
        self,
        code: str,
        target_arch: str,
        output_path: str
    ) -> CompileResult:
        """
        Returns:
        - success: bool
        - binary_path: str (if success)
        - errors: list[CompileError] (if failed)
        - warnings: list[str]
        """
```

**Compiler Integration**:
- Path: `runtime/sfpi/compiler/bin/riscv-tt-elf-gcc`
- Include paths: `tt_llk_*/common/inc/`, `runtime/sfpi/include/`

### 4. Functional Test Agent (`agents/functional_agent.py`)

```python
class FunctionalTestAgent:
    """Runs functional tests on compiled LLK using Quasar emulator"""

    def run_test(
        self,
        binary_path: str,
        test_config: TestConfig,
        golden_generator: Callable
    ) -> TestResult:
        """
        Executes on Quasar emulator/simulator.

        Returns:
        - passed: bool
        - output_values: ndarray
        - golden_values: ndarray
        - error_metrics: dict (MSE, max_error, etc.)
        """
```

**Test Environment**: Quasar emulator/simulator (confirmed available)

**Test Infrastructure Reuse**:
- Golden generators: `tests/python_tests/helpers/golden_generators.py`
- Stimuli generators: `tests/python_tests/helpers/stimuli_generator.py`
- Hardware controller: `tests/python_tests/helpers/hardware_controller.py`

### 5. Feedback Loop (`feedback/feedback_loop.py`)

```python
class FeedbackLoop:
    """Orchestrates iterative refinement"""

    def run(
        self,
        initial_code: str,
        target_arch: str,
        max_iterations: int = 5
    ) -> FeedbackResult:
        """
        1. Compile code
        2. If compile fails: analyze errors, regenerate
        3. Run functional tests
        4. If tests fail: analyze failures, regenerate
        5. Run performance tests
        6. If perf insufficient: analyze, regenerate
        7. Store validated LLK in library
        """
```

---

## Key Files to Modify/Create

### New Files (Phase 0)

| File | Priority | Description |
|------|----------|-------------|
| `tt_llk/codegen/__init__.py` | P0 | Package init |
| `tt_llk/codegen/cli.py` | P0 | CLI entry point |
| `tt_llk/codegen/config/settings.py` | P0 | Configuration |
| `tt_llk/codegen/context/assembly_parser.py` | P0 | Parse assembly.yaml |
| `tt_llk/codegen/context/llk_examples.py` | P0 | Load LLK examples |
| `tt_llk/codegen/context/context_builder.py` | P0 | Build LLM context |
| `tt_llk/codegen/context/architecture_tldr/quasar.md` | P0 | Arch summary |
| `tt_llk/codegen/generator/claude_client.py` | P0 | Claude API client |
| `tt_llk/codegen/generator/prompt_templates.py` | P0 | Prompts |
| `tt_llk/codegen/generator/code_generator.py` | P0 | Main generator |
| `tt_llk/codegen/agents/compile_agent.py` | P0 | Compilation |
| `tt_llk/codegen/agents/functional_agent.py` | P0 | Functional tests (Quasar emulator) |
| `tt_llk/codegen/feedback/feedback_loop.py` | P0 | Feedback orchestration |

*Note: `tt_llk/` refers to `tt_metal/third_party/tt_llk/`*

### Files to Reference (Read-Only)

| File | Purpose |
|------|---------|
| `tt_metal/third_party/tt_llk/tt_llk_quasar/instructions/assembly.yaml` | Quasar instruction set |
| `tt_metal/third_party/tt_llk/tt_llk_blackhole/common/inc/sfpu/ckernel_sfpu_sigmoid.h` | Example SFPU impl |
| `tt_metal/third_party/tt_llk/tests/python_tests/helpers/golden_generators.py` | Golden generation |
| `tt_metal/third_party/tt_llk/tests/Makefile` | Build patterns |

---

## Verification Plan

### Phase 0 Verification

1. **Unit Tests**:
   ```bash
   pytest tools/llk_codegen/tests/ -v
   ```

2. **Integration Test** (Sigmoid SFPU):
   ```bash
   cd tt_metal/third_party/tt_llk
   python -m codegen.cli generate \
       --arch quasar \
       --op sfpu_sigmoid \
       --reference blackhole \
       --validate
   ```

3. **Success Criteria**:
   - Generated code compiles without errors
   - Functional tests pass with <1% error vs golden
   - Feedback loop converges in ≤5 iterations

### Phase 1 Verification

1. **INT8 Kernel Generation**:
   ```bash
   python -m llk_codegen.cli generate \
       --arch quasar \
       --datatype int8 \
       --op reduce_sum
   ```

2. **Performance Benchmark**:
   - Generated kernels within 10% of hand-written performance
   - P1 kernel coverage: ≥80%

---

## Extensibility & Iteration Strategy

### Modular Architecture (Easy to Swap Components)

The system is designed with **clear interfaces** between components, making it easy to replace or improve individual parts:

```
┌─────────────────────────────────────────────────────────────────────────┐
│                        PLUGGABLE COMPONENTS                              │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│  ┌─────────────────┐     Interface: ContextSource                       │
│  │ Context Sources │     • add_source(source) - add new context type    │
│  │ (Pluggable)     │     • Easy to add: RTL explorer, SharePoint, etc.  │
│  └─────────────────┘                                                    │
│          │                                                              │
│          ▼                                                              │
│  ┌─────────────────┐     Interface: LLMClient                           │
│  │ LLM Backend     │     • Can swap Claude → GPT-4 → local model        │
│  │ (Swappable)     │     • Just implement: generate(prompt) → code      │
│  └─────────────────┘                                                    │
│          │                                                              │
│  ┌─────────────────┐     Interface: Validator                           │
│  │ Validators      │     • add_validator(validator)                     │
│  │ (Chainable)     │     • Easy to add: static analysis, perf, etc.     │
│  └─────────────────┘                                                    │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

### What If Approach X Doesn't Work?

| Component | If It Fails | How to Improve |
|-----------|-------------|----------------|
| **Context is not relevant** | LLM generates wrong code | Add relevance scoring, use embeddings to find similar code |
| **Single prompt not enough** | Code always has errors | Switch to multi-turn conversation with tool-use |
| **N=1 candidate fails often** | Low success rate | Increase N, try different temperatures |
| **Claude API too slow** | Iteration takes too long | Cache context, use streaming, try smaller model for drafts |
| **Compiler errors are confusing** | Feedback loop stuck | Build error pattern database, add domain-specific hints |
| **Functional tests fail** | Subtle correctness issues | Add more test cases, use differential testing vs WH/BH |

### Adding New Features (Extension Points)

**1. Add a New Context Source** (e.g., RTL Explorer):
```python
# Just implement the ContextSource interface
class RTLExplorer(ContextSource):
    def get_context(self, query: str) -> str:
        # Query RTL for hardware details
        return rtl_info

# Register it
context_builder.add_source(RTLExplorer())
```

**2. Add a New Validator** (e.g., Static Analysis):
```python
# Just implement the Validator interface
class StaticAnalysisValidator(Validator):
    def validate(self, code: str) -> ValidationResult:
        # Run static analysis
        return result

# Add to the pipeline
pipeline.add_validator(StaticAnalysisValidator())
```

**3. Add a New Tool for Claude** (Agentic Context):
```python
# Define tool in prompt_templates.py
TOOLS = [
    {
        "name": "get_instruction_details",
        "description": "Get details about a Quasar instruction",
        "input_schema": {"instruction_name": "string"}
    },
    # Add new tool here:
    {
        "name": "explore_rtl",
        "description": "Query RTL for hardware implementation details",
        "input_schema": {"unit_name": "string"}
    }
]
```

**4. Switch LLM Backend**:
```python
# Just implement LLMClient interface
class GPT4Client(LLMClient):
    def generate(self, prompt: str, tools: list) -> str:
        return openai.chat.completions.create(...)

# Swap in config
settings.llm_client = GPT4Client()  # Instead of ClaudeClient()
```

### Iteration Strategy (If Initial Approach Is Bad)

**Week 1-2 Check-in Points:**

| Checkpoint | Metric | If Bad | Action |
|------------|--------|--------|--------|
| Day 3 | Context assembly works | Can't parse assembly.yaml | Simplify parser, focus on subset |
| Day 5 | Claude generates valid syntax | Syntax errors | Add more examples to prompt |
| Day 7 | Compile succeeds sometimes | Always fails | Add error examples to prompt |
| Day 10 | Functional test passes sometimes | Always fails | Check golden generation, simplify test case |
| Day 14 | E2E works for sigmoid | Still failing | Step back, try simpler op (identity) |

**Pivot Options:**

1. **If agentic tools don't help**: Fall back to larger static context
2. **If N candidates doesn't improve rate**: Focus on better single prompt
3. **If Claude API is too expensive**: Try local model for drafts, Claude for refinement
4. **If Quasar emulator is broken**: Use RTL sim or mock tests first

### Configuration-Driven Behavior

Everything is configurable in `config/settings.py`:

```python
class Settings:
    # LLM settings
    llm_model: str = "claude-sonnet-4-20250514"  # Can change
    llm_temperature: float = 0.7
    num_candidates: int = 3  # N for beam search
    max_iterations: int = 5

    # Context settings
    max_context_tokens: int = 100000
    include_similar_ops: bool = True
    context_sources: list = ["assembly", "llk_examples", "docs"]

    # Validation settings
    validators: list = ["compile", "functional"]  # Add "performance" later
    error_tolerance: float = 0.01  # 1% for functional tests
```

This makes it easy to experiment with different configurations without code changes.

---

## Risk Mitigation

| Risk | Mitigation |
|------|------------|
| LLM generates incorrect instructions | Validate against assembly.yaml schema |
| Compilation errors are cryptic | Build error classifier with common patterns |
| Quasar emulator limitations | Have RTL simulation as fallback |
| Context too large for LLM | Implement relevance scoring, chunking |
| Performance regression | Baseline all generated kernels |

---

## Dependencies

- Python 3.10+
- `anthropic` Python package (Claude API)
- SFPI compiler (already in repo at `runtime/sfpi/compiler/`)
- Quasar emulator/simulator (confirmed available)
- PyYAML for assembly.yaml parsing
- pytest for testing

**Environment Variable Required**:
```bash
export ANTHROPIC_API_KEY="your-api-key"
```

---

## Timeline Summary

| Phase | Duration | Deliverable |
|-------|----------|-------------|
| Phase 0 | 2 weeks | E2E pipeline for single kernel type |
| Phase 1 | 4 weeks | Production system with INT8, RTL explorer |

**Total**: 6 weeks to MVP, ongoing iteration for kernel coverage
