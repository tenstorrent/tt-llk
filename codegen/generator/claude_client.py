# SPDX-FileCopyrightText: (c) 2026 Tenstorrent AI ULC
#
# SPDX-License-Identifier: Apache-2.0

"""Claude API client wrapper for LLK code generation."""

import re
from dataclasses import dataclass
from typing import Optional

import anthropic

from codegen.config.settings import settings


@dataclass
class GenerationResult:
    """Result of a code generation request."""

    code: str
    explanation: str
    raw_response: str
    model: str
    usage: dict


class ClaudeClient:
    """Wrapper for Claude API calls."""

    def __init__(
        self,
        model: Optional[str] = None,
        temperature: Optional[float] = None,
        max_tokens: Optional[int] = None,
    ):
        if not settings.anthropic_api_key:
            raise ValueError("ANTHROPIC_API_KEY environment variable not set")

        self.client = anthropic.Anthropic(api_key=settings.anthropic_api_key)
        self.model = model or settings.llm_model
        self.temperature = (
            temperature if temperature is not None else settings.llm_temperature
        )
        self.max_tokens = max_tokens or settings.llm_max_tokens

    def generate_code(
        self,
        prompt: str,
        system_prompt: Optional[str] = None,
    ) -> GenerationResult:
        """
        Generate LLK code from a prompt.

        Args:
            prompt: The user prompt describing what to generate
            system_prompt: Optional system prompt for context

        Returns:
            GenerationResult with generated code and metadata
        """
        messages = [{"role": "user", "content": prompt}]

        response = self.client.messages.create(
            model=self.model,
            max_tokens=self.max_tokens,
            temperature=self.temperature,
            system=system_prompt or self._default_system_prompt(),
            messages=messages,
        )

        raw_text = response.content[0].text
        code, explanation = self._parse_response(raw_text)

        return GenerationResult(
            code=code,
            explanation=explanation,
            raw_response=raw_text,
            model=response.model,
            usage={
                "input_tokens": response.usage.input_tokens,
                "output_tokens": response.usage.output_tokens,
            },
        )

    def generate_with_feedback(
        self,
        prompt: str,
        previous_code: str,
        error_message: str,
        system_prompt: Optional[str] = None,
    ) -> GenerationResult:
        """
        Generate improved code based on compilation/test errors.

        Args:
            prompt: Original generation prompt
            previous_code: Code that failed
            error_message: Compiler or test error output
            system_prompt: Optional system prompt

        Returns:
            GenerationResult with improved code
        """
        feedback_prompt = f"""
{prompt}

## Previous Attempt

The following code was generated but failed:

```cpp
{previous_code}
```

## Error Output

```
{error_message}
```

## Instructions

Analyze the error and generate a corrected version. Focus on fixing the specific issue indicated by the error message.
"""
        return self.generate_code(feedback_prompt, system_prompt)

    def _default_system_prompt(self) -> str:
        """Default system prompt for LLK generation."""
        return """You are an expert in Tenstorrent Low-Level Kernel (LLK) development for the Tensix architecture.

You generate C++ code for SFPU (Scalar Floating Point Unit) kernels that run on Tenstorrent hardware.

Key guidelines:
- Follow the exact patterns shown in reference implementations
- Use the correct namespace: ckernel::sfpu
- Include proper headers (ckernel_sfpu_load_config.h, sfpi.h, etc.)
- Use TTI_* macros for Quasar architecture
- Use sfpi:: namespace for Blackhole/Wormhole architectures
- Always include SPDX license header

When generating code, wrap it in ```cpp code blocks.
"""

    def _parse_response(self, raw_text: str) -> tuple[str, str]:
        """
        Parse raw LLM response to extract code and explanation.

        Returns:
            Tuple of (code, explanation)
        """
        # Find code blocks
        code_pattern = r"```(?:cpp|c\+\+)?\s*\n(.*?)```"
        matches = re.findall(code_pattern, raw_text, re.DOTALL)

        if matches:
            # Take the largest code block (usually the main implementation)
            code = max(matches, key=len).strip()
            # Everything outside code blocks is explanation
            explanation = re.sub(code_pattern, "", raw_text, flags=re.DOTALL).strip()
        else:
            # No code blocks found, treat entire response as code
            code = raw_text.strip()
            explanation = ""

        return code, explanation
