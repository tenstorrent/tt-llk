# LLK Architecture Reference

Low-Level Kernels (LLK) for Tenstorrent architectures.

## Kernel Categories

| Category | Purpose | Path Pattern | Naming Pattern |
|----------|---------|--------------|----------------|
| **SFPU** | Vector operations (sigmoid, exp, relu, etc.) | `common/inc/sfpu/` | `ckernel_sfpu_{op}.h` |
| **Math** | Matrix/tensor operations (matmul, reduce) | `llk_lib/` | `llk_math_{op}.h` |
| **Pack** | Pack data from dest to L1 | `llk_lib/` | `llk_pack_{op}.h` |
| **Unpack** | Unpack data from L1 to src registers | `llk_lib/` | `llk_unpack_{op}.h` |

## How to Learn a Kernel Type

For any kernel type on any architecture, the best approach is:

### 1. Read existing implementations
```bash
# List all SFPU kernels for an architecture
ls tt_llk_{arch}/common/inc/sfpu/

# List all math/pack/unpack kernels
ls tt_llk_{arch}/llk_lib/

# Read golden verified examples
ls codegen/references/golden/
```

### 2. Identify common patterns
Read 2-3 implementations and look for:
- **Include patterns** — what headers are common across all kernels of this type?
- **Namespace patterns** — what namespace structure wraps the code?
- **Function signature patterns** — what template params, naming, parameter lists?
- **Core loop pattern** — how does data flow through the kernel?
- **Register/resource patterns** — what's allocated and how?

### 3. Consult architecture docs
- **Confluence** for target architecture specifications
- **DeepWiki** (`tenstorrent/tt-isa-documentation`) for ISA details
- **assembly.yaml** for instruction definitions

## Architecture Directories

| Architecture | Directory |
|--------------|-----------|
| Blackhole | `tt_llk_blackhole/` |
| Quasar | `tt_llk_quasar/` |
| Wormhole | `tt_llk_wormhole_b0/` |

Each directory follows the same structure:
```
tt_llk_{arch}/
  common/inc/sfpu/    # SFPU kernel headers
  llk_lib/            # Math, pack, unpack kernel headers
  instructions/       # ISA definition (assembly.yaml)
```

## Golden Examples

Verified correct implementations are stored in `codegen/references/golden/`. These are the most trustworthy reference for how target architecture code should look. Always read them before writing new kernels.
