---
name: bh-arch-lookup
description: Fetch Blackhole/Tensix architecture info using external documentation sources (tt-isa-documentation, Confluence, Glean). Use for ISA, SFPU, NoC, or register questions.
user_invocable: true
---

# /bh-arch-lookup — Architecture Documentation Lookup

Answer architecture questions about Blackhole/Tensix hardware directly in the conversation using external documentation sources.

## Usage

```
/bh-arch-lookup "SFPU instruction set"
/bh-arch-lookup "How does the replay buffer work?"
/bh-arch-lookup "LREG usage"
```

## What to Do

1. First, check local reference files by spawning the **bh-arch-lookup** agent:

```
Agent tool:
  subagent_type: "bh-arch-lookup"
  description: "Arch lookup: {topic}"
  prompt: "{user's query}"
```

2. If the agent's local results are insufficient, query external sources in this order:

### tt-isa-documentation (PRIMARY)

```
mcp__deepwiki__ask_question
  repo: "tenstorrent/tt-isa-documentation"
  question: "{user's query}"
```

### Confluence (SECONDARY)

```
mcp__atlassian__search
  query: "Blackhole {topic}" or "Tensix {topic}"
```

Key pages: `1201504351` (Blackhole Architecture), `1170505767` (Tensix SFPU ISA), `1224999338` (System/chip design).

For full page content: `mcp__atlassian__getConfluencePage` with the page ID.

### Glean (TERTIARY — architecture ONLY, never kernel code)

```
mcp__glean_default__search
  query: "hardware concept or architecture question"
  app: "confluence"
```

3. Combine local and external findings into a concise summary for the user, citing sources.
