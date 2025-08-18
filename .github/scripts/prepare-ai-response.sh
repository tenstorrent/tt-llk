#!/bin/bash

# LLK AI Issue Consultant - AI Prompt Preparation Script
#
# This script prepares the AI prompt based on issue details and generated prompts
# for TT-Chat analysis.

set -e

# Check required environment variables
if [ -z "$ISSUE_NUMBER" ] || [ -z "$ISSUE_TITLE" ] || [ -z "$ISSUE_AUTHOR" ] || [ -z "$ISSUE_LABELS" ]; then
  echo "❌ Error: Missing required environment variables"
  echo "Required: ISSUE_NUMBER, ISSUE_TITLE, ISSUE_AUTHOR, ISSUE_LABELS"
  exit 1
fi

# Create a summary of the issue for logging
echo "📋 Issue Summary:"
echo "  Number: $ISSUE_NUMBER"
echo "  Title: $ISSUE_TITLE"
echo "  Author: $ISSUE_AUTHOR"
echo "  Labels: $ISSUE_LABELS"
echo "  URL: $ISSUE_URL"
echo ""
echo "📝 Issue Description:"
echo "$ISSUE_BODY"
echo ""
echo "🤖 Ready for AI analysis"

# Use generated prompt if available, otherwise fallback to default
if [ -n "$GENERATED_PROMPT" ] && [ "$PROMPT_SOURCE" = "generated" ]; then
  export AI_PROMPT="$GENERATED_PROMPT

Issue Description:
Title: $ISSUE_TITLE
Body: $ISSUE_BODY"
  echo "🧠 Using AI-generated specialized prompt"
  echo "📝 Prompt source: $PROMPT_SOURCE"
elif [ -n "$GENERATED_PROMPT" ] && [ "$PROMPT_SOURCE" = "default" ]; then
  export AI_PROMPT="$GENERATED_PROMPT

Issue Description:
Title: $ISSUE_TITLE
Body: $ISSUE_BODY"
  echo "🔄 Using default prompt (AI generation failed)"
  echo "📝 Prompt source: $PROMPT_SOURCE"
else
  # Ultimate fallback if no prompt is available
  export AI_PROMPT="Given the following issue description do the following: 1. Determine and list which LLK APIs are relevant; 2. Within those APIs, list the Tensix instructions that are called; 3. Within those APIs, list the Tensix configuration registers that are programmed.

Issue Description:
Title: $ISSUE_TITLE
Body: $ISSUE_BODY"
  echo "⚙️  Using hardcoded default prompt (no AI generation attempted)"
  echo "📝 Prompt source: hardcoded"
fi

# Export issue details for the TT-Chat script
export ISSUE_TITLE
export ISSUE_AUTHOR
export ISSUE_LABELS

echo ""
echo "🤖 AI Prompt prepared for TT-Chat:"
echo "$AI_PROMPT"
echo ""

echo "🚀 AI prompt prepared for TT-Chat analysis"
echo "✅ Proceeding to TT-Chat API call"
