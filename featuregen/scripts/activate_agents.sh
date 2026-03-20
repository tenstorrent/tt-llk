#!/usr/bin/env bash
# Copies featuregen agent playbooks to ~/.claude/agents/ so they appear
# in the /agents dialog and can be invoked by name.

set -euo pipefail

AGENTS_SRC="$(cd "$(dirname "$0")/../agents" && pwd)"
AGENTS_DST="$HOME/.claude/agents"

mkdir -p "$AGENTS_DST"

echo "Copying featuregen agents to $AGENTS_DST ..."

for src in "$AGENTS_SRC"/*.md; do
    name="$(basename "$src")"
    dst="$AGENTS_DST/$name"
    cp "$src" "$dst"
    echo "  ✓ $name"
done

echo ""
echo "Done. $(ls "$AGENTS_SRC"/*.md | wc -l) agents activated."
echo "Restart Claude Code or run /agents to verify."
