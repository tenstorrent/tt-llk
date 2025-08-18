/**
 * LLK AI Issue Consultant - Comment Poster
 *
 * This script posts the AI-generated response as a comment on the GitHub issue.
 */

import * as core from '@actions/core';
import * as github from '@actions/github';
import {existsSync, readFileSync} from 'fs';
import path from 'path';

// Environment variables
const token       = process.env.GITHUB_TOKEN;
const issueNumber = process.env.ISSUE_NUMBER;

if (!token)
{
    console.error('❌ Missing GITHUB_TOKEN.');
    process.exit(1);
}

async function main()
{
    const context = github.context;
    const octokit = github.getOctokit(token);
    try
    {
        if (!issueNumber)
        {
            core.setFailed('Issue number not provided');
            return;
        }

        // Read the AI response from file
        const aiResponsePath = path.join(process.cwd(), 'ai_response.md');

        if (!existsSync(aiResponsePath))
        {
            core.setFailed('AI response file not found');
            return;
        }

        const aiResponse = readFileSync(aiResponsePath, 'utf8');

        // Post comment on the issue
        await octokit.rest.issues.createComment({owner: context.repo.owner, repo: context.repo.repo, issue_number: parseInt(issueNumber), body: aiResponse});

        console.log(`✅ AI consultant response posted successfully to issue #${issueNumber}`);
    }
    catch (error)
    {
        core.setFailed(`Failed to post AI response: ${error.message}`);
        throw error;
    }
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
