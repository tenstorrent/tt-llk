# Quasar Architecture Reference

This document points to authoritative sources for Quasar architecture information. **Do not hardcode architecture details here** — always consult the sources directly so information stays current.

## Authoritative Sources

### 1. Confluence — Tensix NEO High Level Specification
- **Page ID**: 84508873
- **URL**: https://tenstorrent.atlassian.net/wiki/spaces/TA/pages/84508873
- **Access**: Atlassian MCP tools (`mcp__atlassian__get_page` with page_id `84508873`)
- **Contains**: Full Quasar/NEO architecture spec — SFPU, register files, instruction encoding, execution model, NoC

### 2. DeepWiki — tt-isa-documentation
- **Repo**: `tenstorrent/tt-isa-documentation`
- **Access**: DeepWiki MCP tools (`mcp__deepwiki__ask_question` with repo `tenstorrent/tt-isa-documentation`)
- **Contains**: ISA documentation, instruction details, architecture comparisons

### 3. assembly.yaml — Quasar ISA Definition
- **Path**: `tt_llk_quasar/instructions/assembly.yaml`
- **Size**: ~277KB — always use grep, never read the whole file
- **Contains**: Every instruction available on Quasar with parameters and encoding

```bash
# Find all SFPU instructions
grep "^SFP" tt_llk_quasar/instructions/assembly.yaml

# Get details for a specific instruction
grep -A 30 "^SFPLOAD:" tt_llk_quasar/instructions/assembly.yaml

# Search for instructions by keyword
grep -i "nonlinear" tt_llk_quasar/instructions/assembly.yaml
```

### 4. Existing Quasar Implementations
- **SFPU kernels**: `tt_llk_quasar/common/inc/sfpu/*.h`
- **Math/pack/unpack**: `tt_llk_quasar/llk_lib/*.h`
- **Golden examples**: `codegen/references/golden/*.h`

These are the most reliable source for understanding how Quasar code should be structured.

## Key Areas to Research

When working on a kernel, query the sources above for:

| Topic | What to look for |
|-------|-----------------|
| **SFPU** | Vector engine architecture, LREG registers, SFPU instructions, nonlinear modes |
| **Math** | FPU operations, tile processing, fidelity modes, face handling |
| **Pack** | Packer units, buffer descriptors, tile increment instructions |
| **Unpack** | Unpacker units, source registers, tile processing modes |
| **General** | Register file layout, data formats, addressing modes |
