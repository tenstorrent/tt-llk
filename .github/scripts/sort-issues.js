// SPDX-FileCopyrightText: (c) 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

import {Octokit} from '@octokit/rest';
import {writeFileSync} from 'fs';

// Environment variables
const token       = process.env.GITHUB_TOKEN;
const currentRepo = process.env.GITHUB_REPOSITORY;

if (!token || !currentRepo)
{
    console.error('❌ Missing GITHUB_TOKEN or GITHUB_REPOSITORY.');
    process.exit(1);
}

const [defaultOwner, defaultRepoName] = currentRepo.split('/');
const octokit                         = new Octokit({auth: token});

// Constants for priority and overdue thresholds
const PRIORITY_RANK = {
    P0: 0,
    P1: 1,
    P2: 2,
    None: 3
};
const PRIORITY_DUE_DAYS = {
    P0: 20,
    P1: 30,
    P2: 60
};

// Helper functions
const getDaysOpen = (createdAt) => Math.floor((new Date() - new Date(createdAt)) / (1000 * 60 * 60 * 24));

const getPriority = (issue) => {
    const label = issue.labels.find((l) => typeof l.name === 'string' && /^P[0-2]$/.test(l.name));
    return label ? label.name : 'None';
};

const isOverdue = (priority, daysOpen) => daysOpen > (PRIORITY_DUE_DAYS[priority] || Infinity);

const getStyledLabels = (labels) => labels.map((label) => `<span class="label" style="background-color:#${label.color}">${label.name}</span>`).join(' ');

const fetchIssues = async ({owner, repo, filterLabel = null, tag}) => {
    const allIssues = await octokit.paginate(octokit.rest.issues.listForRepo, {
        owner,
        repo,
        state: 'open',
        per_page: 100,
    });

    return allIssues.filter((issue) => !filterLabel || issue.labels.some((l) => typeof l.name === 'string' && l.name === filterLabel))
        .map((issue) => ({...issue, _sourceRepo: tag}));
};

const generateIssueRow = (issue) => {
    const priority     = getPriority(issue);
    const daysOpen     = getDaysOpen(issue.created_at);
    const createdAt    = new Date(issue.created_at).toLocaleDateString();
    const labels       = getStyledLabels(issue.labels);
    const overdueClass = isOverdue(priority, daysOpen) ? 'overdue' : '';

    return `
    <tr class="${overdueClass}">
      <td>${issue._sourceRepo}</td>
      <td><a href="${issue.html_url}" target="_blank">#${issue.number}</a></td>
      <td><a href="${issue.html_url}" target="_blank">${issue.title}</a></td>
      <td>${priority}</td>
      <td>${createdAt}</td>
      <td>${daysOpen}</td>
      <td>${labels}</td>
    </tr>
  `;
};

const generateHTMLReport = (rows) => `
  <!DOCTYPE html>
  <html>
  <head>
    <meta charset="UTF-8">
    <title>Open Issues Report</title>
    <style>
      body { font-family: "Segoe UI", Roboto, Helvetica, Arial, sans-serif; background-color: #f8f9fa; color: #333; padding: 20px; }
      h1 { text-align: center; color: #444; }
      table { border-collapse: collapse; width: 100%; margin-top: 20px; box-shadow: 0 2px 8px rgba(0,0,0,0.05); }
      th, td { padding: 12px; border: 1px solid #ddd; text-align: left; vertical-align: top; }
      th { background-color: #e9ecef; color: #212529; }
      tr:nth-child(even) { background-color: #f1f3f5; }
      tr:hover { background-color: #dee2e6; }
      tr.overdue { background-color: #ffe6e6 !important; }
      a { color: #007bff; text-decoration: none; }
      a:hover { text-decoration: underline; }
      .label { display: inline-block; padding: 2px 6px; margin: 2px 4px 2px 0; font-size: 0.85em; font-weight: 500; color: #fff; border-radius: 12px; }
    </style>
  </head>
  <body>
    <h1>Open Issues Report (with LLK from tt-metal)</h1>
    <table>
      <thead>
        <tr>
          <th>Repository</th>
          <th>Issue</th>
          <th>Title</th>
          <th>Priority</th>
          <th>Created At</th>
          <th>Days Open</th>
          <th>Labels</th>
        </tr>
      </thead>
      <tbody>
        ${rows}
      </tbody>
    </table>
  </body>
  </html>
`;

(async () => {
    try
    {
        // Fetch issues from both repositories
        const localIssues = await fetchIssues({
            owner: defaultOwner,
            repo: defaultRepoName,
            tag: 'tt-llk',
        });

        const ttMetalIssues = await fetchIssues({
            owner: 'tenstorrent',
            repo: 'tt-metal',
            filterLabel: 'LLK',
            tag: 'tt-metal',
        });

        // Combine and sort issues
        const combinedIssues = [...localIssues, ...ttMetalIssues];
        const sortedIssues   = combinedIssues.sort((a, b) => {
            const aPriority    = getPriority(a);
            const bPriority    = getPriority(b);
            const priorityDiff = PRIORITY_RANK[aPriority] - PRIORITY_RANK[bPriority];
            return priorityDiff !== 0 ? priorityDiff : new Date(a.created_at) - new Date(b.created_at);
        });

        // Generate HTML report
        const rows = sortedIssues.map(generateIssueRow).join('');
        const html = generateHTMLReport(rows);

        // Write to file
        writeFileSync('sorted-issues.html', html);
        console.log('✅ HTML report generated: sorted-issues.html');
    }
    catch (err)
    {
        console.error('❌ Failed to generate report:', err);
        process.exit(1);
    }
})();
