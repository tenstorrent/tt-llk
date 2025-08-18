/**
 * LLK AI Issue Consultant - Intelligent Prompt Generator
 *
 * This script uses TT-Chat to generate a more specific and descriptive prompt
 * based on the GitHub issue content, falling back to a default prompt if needed.
 */

import * as core from '@actions/core';
import {writeFileSync} from 'fs';
import {OpenAI} from 'openai';

// Environment variables
const apiKey      = process.env.TT_CHAT_API_KEY;
const baseUrl     = process.env.TT_CHAT_BASE_URL;
const model       = process.env.TT_CHAT_MODEL;
const issueTitle  = process.env.ISSUE_TITLE;
const issueBody   = process.env.ISSUE_BODY;
const issueAuthor = process.env.ISSUE_AUTHOR;
const issueLabels = process.env.ISSUE_LABELS;

// Default fallback prompt
const DEFAULT_PROMPT =
    `Given the following issue description do the following: 1. Determine and list which LLK APIs are relevant; 2. Within those APIs, list the Tensix instructions that are called; 3. Within those APIs, list the Tensix configuration registers that are programmed.`;

async function main()
{
    console.log('🎯 Generating intelligent prompt for LLK analysis...');

    try
    {
        // Validate required environment variables for prompt generation
        if (!apiKey || !baseUrl || !model)
        {
            console.log('⚠️  TT-Chat API not configured, using default prompt');
            await setDefaultPrompt();
            return;
        }

        if (!issueTitle || !issueBody)
        {
            console.log('⚠️  Issue details missing, using default prompt');
            await setDefaultPrompt();
            return;
        }

        console.log(`🔧 Configuration:`);
        console.log(`   Base URL: ${baseUrl}`);
        console.log(`   Model: ${model}`);
        console.log(`   Issue: ${issueTitle}`);
        console.log(`   Author: ${issueAuthor}`);
        console.log(`   Labels: ${issueLabels}`);

        // Initialize OpenAI client with TT-Chat configuration
        const client = new OpenAI({apiKey: apiKey, baseURL: baseUrl});

        // Create a meta-prompt to generate a better analysis prompt
        const metaPrompt = `You are an expert in Tenstorrent's Low Level Kernel (LLK) APIs and Tensix processor architecture.

The repo you are working on is https://github.com/tenstorrent/tt-llk.
You also know about all the information in the tt-metal repo https://github.com/tenstorrent/tt-metal, and the Tensix ISA repo https://github.com/tenstorrent/tt-isa-documentation.

Given the following GitHub issue, create a specific, detailed prompt that would help analyze the issue in terms of:
1. Relevant LLK APIs
2. Tensix instructions that are called
3. Tensix configuration registers that are programmed
4. Tensix data formats that are relevant to the issue

The prompt should be tailored to the specific issue content and ask for precise, technical details.

Issue Title: ${issueTitle}
Issue Body: ${issueBody}
Issue Author: ${issueAuthor}
Issue Labels: ${issueLabels}

Generate a prompt that is more specific and insightful than this default prompt:
"${DEFAULT_PROMPT}"

The prompt should explicitly tell the LLK to not halucinate any information, and only answer what it knows.

Return ONLY the generated prompt text, nothing else.`;

        console.log('🧠 Asking TT-Chat to generate a specialized prompt...');

        // Generate the specialized prompt
        const response = await client.chat.completions.create({
            messages: [
                {
                    role: 'system',
                    content:
                        'You are an expert prompt engineer specializing in Tenstorrent LLK and Tensix analysis. Create specific, technical prompts that will yield detailed, actionable insights.'
                },
                {role: 'user', content: metaPrompt}
            ],
            model: model,
            stream: false,
            temperature: 0.3, // Lower temperature for more consistent prompt generation
            max_tokens: 1000  // Sufficient for a detailed prompt
        });

        const generatedPrompt = response.choices[0]?.message?.content?.trim();

        if (!generatedPrompt || generatedPrompt.length < 50)
        {
            console.log('⚠️  Generated prompt too short, using default prompt');
            await setDefaultPrompt();
            return;
        }

        console.log(`✅ Generated specialized prompt (${generatedPrompt.length} characters)`);
        console.log(`📝 Prompt: ${generatedPrompt}`);

        // Set the generated prompt as output
        core.setOutput('ai_prompt', generatedPrompt);
        core.setOutput('prompt_source', 'generated');

        // Also write to file for debugging
        writeFileSync('generated_prompt.txt', generatedPrompt, 'utf8');

        console.log('🎯 Specialized prompt ready for analysis');

        return {success: true, prompt: generatedPrompt, source: 'generated'};
    }
    catch (error)
    {
        console.error('❌ Prompt generation failed:', error.message);
        console.log('🔄 Falling back to default prompt');

        await setDefaultPrompt();

        return {success: false, error: error.message, prompt: DEFAULT_PROMPT, source: 'fallback'};
    }
}

async function setDefaultPrompt()
{
    console.log('📝 Using default LLK analysis prompt');
    core.setOutput('ai_prompt', DEFAULT_PROMPT);
    core.setOutput('prompt_source', 'default');

    // Write default prompt to file
    writeFileSync('generated_prompt.txt', DEFAULT_PROMPT, 'utf8');

    console.log(`✅ Default prompt ready: ${DEFAULT_PROMPT}`);
}

// Run the main function when script is executed directly
if (import.meta.url === `file://${process.argv[1]}`)
{
    main().catch(error => {
        core.setFailed(`Script failed: ${error.message}`);
        process.exit(1);
    });
}

export default main;
