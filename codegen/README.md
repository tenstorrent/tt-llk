# LLK CodeGen

AI-powered kernel generation for Quasar.

## Quick Start

```bash
cd codegen
claude
> Generate gelu for Quasar
```

## Setup

### 1. Test Environment (required)
```bash
cd ../tests
./setup_testing_env.sh
```

### 2. Atlassian Access (optional)
On first use, Claude will prompt you to authenticate with Atlassian.
The MCP server is pre-configured in `.mcp.json`.

## Usage

| Command | Description |
|---------|-------------|
| `Generate sigmoid for Quasar` | Generate SFPU kernel |
| `Generate reduce for Quasar` | Generate math kernel |
| `Generate pack_untilize for Quasar` | Generate pack kernel |
