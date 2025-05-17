// SPDX-FileCopyrightText: (c) 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

import {Octokit} from '@octokit/rest';
import {writeFileSync} from 'fs';

const token = process.env.GITHUB_TOKEN;
const repo  = process.env.GITHUB_REPOSITORY;

if (!token || !repo)
{
    console.error('❌ Missing environment variables GITHUB_TOKEN or GITHUB_REPOSITORY.');
    process.exit(1);
}

const [owner, repoName] = repo.split('/');
const octokit           = new Octokit({auth: token});

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

const getDaysOpen = (createdAt) => {
    const diffMs = new Date() - new Date(createdAt);
    return Math.floor(diffMs / (1000 * 60 * 60 * 24));
};

const getPriority = (issue) => {
    const label = issue.labels.find(l => typeof l.name === 'string' && /^P[0-2]$/.test(l.name));
    return label ? label.name : 'None';
};

const isOverdue = (priority, daysOpen) => {
    return daysOpen > (PRIORITY_DUE_DAYS[priority] || Infinity);
};

const generateIssueRow = (issue) => {
    const priority  = getPriority(issue);
    const daysOpen  = getDaysOpen(issue.created_at);
    const createdAt = new Date(issue.created_at).toLocaleDateString();
    const labels    = issue.labels.map(label => `<span class="label" style="background-color:#${label.color}">${label.name}</span>`).join(' ');

    const overdueClass = isOverdue(priority, daysOpen) ? 'overdue' : '';

    return `
        <tr class="${overdueClass}">
          <td><a href="${issue.html_url}" target="_blank">#${issue.number}</a></td>
          <td>${issue.title}</td>
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
        <h1>Open Issues Sorted by Priority & Age</h1>
        <table>
            <thead>
                <tr>
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
        const issues = await octokit.paginate(octokit.rest.issues.listForRepo, {
            owner,
            repo: repoName,
            state: 'open',
            per_page: 100,
        });

        const sortedIssues = issues.sort((a, b) => {
            const aPriority    = getPriority(a);
            const bPriority    = getPriority(b);
            const priorityDiff = PRIORITY_RANK[aPriority] - PRIORITY_RANK[bPriority];
            return priorityDiff !== 0 ? priorityDiff : new Date(a.created_at) - new Date(b.created_at);
        });

        const rows = sortedIssues.map(generateIssueRow).join('');
        const html = generateHTMLReport(rows);

        writeFileSync('sorted-issues.html', html);
        console.log('✅ HTML report generated: sorted-issues.html');
    }
    catch (err)
    {
        console.error('❌ Failed to generate issue report:', err);
        process.exit(1);
    }
})();
