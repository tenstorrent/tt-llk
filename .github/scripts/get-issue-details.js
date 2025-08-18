/**
 * LLK AI Issue Consultant - Issue Details Extractor
 *
 * This script extracts issue details from GitHub API and sets workflow outputs
 * for further processing by the AI consultant workflow.
 */

import * as core from '@actions/core';
import * as github from '@actions/github';

// Environment variables
const token            = process.env.GITHUB_TOKEN;
const inputIssueNumber = process.env.GITHUB_EVENT_INPUTS_ISSUE_NUMBER;

if (!token)
{
    console.error('❌ Missing GITHUB_TOKEN.');
    process.exit(1);
}

async function main()
{
    const context = github.context;
    const octokit = github.getOctokit(token);
    let issueNumber;

    // Determine issue number from event or input
    if (context.eventName === 'issues')
    {
        issueNumber = context.issue.number;
    }
    else if (context.eventName === 'workflow_dispatch')
    {
        issueNumber = inputIssueNumber || '598'; // Default to issue 598 if not provided
        console.log(`📋 Using issue number: ${issueNumber} ${!inputIssueNumber ? '(default)' : '(provided)'}`);
    }
    else if (context.eventName === 'push')
    {
        issueNumber = '598'; // Default to issue 598 for push events (testing)
        console.log(`📋 Using default issue number for push event: ${issueNumber}`);
    }
    else
    {
        issueNumber = '598'; // Ultimate fallback
        console.log(`⚠️  Unknown event type '${context.eventName}', using default issue number: ${issueNumber}`);
    }

    console.log(`Processing issue #${issueNumber}`);

    try
    {
        // Fetch issue details
        const {data: issue} = await octokit.rest.issues.get({owner: context.repo.owner, repo: context.repo.repo, issue_number: parseInt(issueNumber)});

        // Validate that the issue has the required label
        const hasRequiredLabel = issue.labels.some(label => typeof label === 'object' ? label.name === 'llk-ai-consultant' : label === 'llk-ai-consultant');

        if (!hasRequiredLabel)
        {
            core.setFailed('Issue must have "llk-ai-consultant" label');
            return;
        }

        // Extract relevant information
        const issueData = {
            number: issue.number,
            title: issue.title,
            body: issue.body || '',
            author: issue.user.login,
            labels: issue.labels.map(label => typeof label === 'object' ? label.name : label),
            created_at: issue.created_at,
            html_url: issue.html_url
        };

        console.log('Issue data extracted successfully');
        console.log(`Title: ${issueData.title}`);
        console.log(`Author: ${issueData.author}`);
        console.log(`Labels: ${issueData.labels.join(', ')}`);

        // Set outputs for next steps
        core.setOutput('issue_number', issueData.number);
        core.setOutput('issue_title', issueData.title);
        core.setOutput('issue_body', issueData.body);
        core.setOutput('issue_author', issueData.author);
        core.setOutput('issue_labels', issueData.labels.join(','));
        core.setOutput('issue_url', issueData.html_url);

        return issueData;
    }
    catch (error)
    {
        core.setFailed(`Failed to fetch issue details: ${error.message}`);
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
