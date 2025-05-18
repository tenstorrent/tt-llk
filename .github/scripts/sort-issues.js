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

const countIssuesByPriority = (issues) => {
    const counts = {P0: 0, P1: 0, P2: 0, None: 0}; // Ensure all keys are initialized to 0
    issues.forEach((issue) => {
        const priority   = getPriority(issue);
        counts[priority] = (counts[priority] || 0) + 1;
    });
    return counts;
};

const fetchIssues = async ({owner, repo, filterLabel = null, tag, state = 'open'}) => {
    const allIssues = await octokit.paginate(octokit.rest.issues.listForRepo, {
        owner,
        repo,
        state,
        per_page: 100,
    });

    return allIssues
        .filter((issue) => !issue.pull_request) // Exclude pull requests
        .filter((issue) => !filterLabel || issue.labels.some((l) => typeof l.name === 'string' && l.name === filterLabel))
        .map((issue) => ({...issue, _sourceRepo: tag}));
};

const calculateAverageClosingTimes = (issues) => {
    const closingTimes = {P0: [], P1: [], P2: [], None: []};

    issues.forEach((issue) => {
        const priority = getPriority(issue);
        if (issue.closed_at)
        {
            const createdAt   = new Date(issue.created_at);
            const closedAt    = new Date(issue.closed_at);
            const daysToClose = Math.floor((closedAt - createdAt) / (1000 * 60 * 60 * 24));
            closingTimes[priority].push(daysToClose);
        }
    });

    const averages = {};
    for (const priority in closingTimes)
    {
        const times        = closingTimes[priority];
        averages[priority] = times.length > 0 ? (times.reduce((sum, time) => sum + time, 0) / times.length).toFixed(1) : 'N/A';
    }

    return averages;
};

const generateIssueRow = (issue) => {
    const priority     = getPriority(issue);
    const daysOpen     = getDaysOpen(issue.created_at);
    const createdAt    = new Intl.DateTimeFormat('en-US', {day: '2-digit', month: 'long', year: 'numeric'}).format(new Date(issue.created_at)); // Format date
    const labels       = getStyledLabels(issue.labels);
    const overdueClass = isOverdue(priority, daysOpen) ? 'overdue' : '';
    const reporter     = issue.user.login; // Reporter is the issue creator
    const assignees    = issue.assignees.length > 0 ? issue.assignees.map((assignee) => assignee.login).join(', ') : 'None';

    return `
  <tr class="${overdueClass}">
    <td>${issue._sourceRepo}</td>
    <td><a href="${issue.html_url}" target="_blank">#${issue.number}</a></td>
    <td><a href="${issue.html_url}" target="_blank">${issue.title}</a></td>
    <td>${priority}</td>
    <td>${createdAt}</td>
    <td>${daysOpen}</td>
    <td>${labels}</td>
    <td>${reporter}</td> <!-- New Reporter column -->
    <td>${assignees}</td>
  </tr>
`;
};

const generateHTMLReport = (rows, priorityCounts, averageClosingTimes) => `
<!DOCTYPE html>
<html>
<head>
<meta charset="UTF-8">
<title>LLK Open Issues Report</title>
<script src="https://cdn.jsdelivr.net/npm/chart.js"></script>
<style>
body { font-family: "Segoe UI", Roboto, Helvetica, Arial, sans-serif; background-color: #f8f9fa; color: #333; padding: 20px; }
h1 { text-align: center; color: #444; }
.report-container { display: flex; justify-content: space-between; margin-top: 20px; }
.chart-container { width: 25%; } /* Reduced size for the chart */
.stats-container { width: 25%; } /* Adjusted width for the table */
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
<h1>LLK Open Issues Report</h1>
<div class="report-container">
<div class="stats-container">
  <h2>Average Closing Time</h2>
  <table>
    <thead>
      <tr>
        <th>Priority</th>
        <th>Average Closing Time (in Days)</th>
      </tr>
    </thead>
    <tbody>
      <tr><td>P0</td><td>${averageClosingTimes.P0}</td></tr>
      <tr><td>P1</td><td>${averageClosingTimes.P1}</td></tr>
      <tr><td>P2</td><td>${averageClosingTimes.P2}</td></tr>
      <tr><td>None</td><td>${averageClosingTimes.None}</td></tr>
    </tbody>
  </table>
</div>
<div class="chart-container">
  <canvas id="priorityChart"></canvas>
</div>
</div>
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
    <th>Reporter</th> <!-- New Reporter column -->
    <th>Assignees</th>
  </tr>
</thead>
<tbody>
  ${rows}
</tbody>
</table>
<script>
const ctx = document.getElementById('priorityChart').getContext('2d');
new Chart(ctx, {
  type: 'pie',
  data: {
    labels: ['P0', 'P1', 'P2', 'None'],
    datasets: [{
      data: [${priorityCounts.P0}, ${priorityCounts.P1}, ${priorityCounts.P2}, ${priorityCounts.None}],
      backgroundColor: ['#ff6384', '#36a2eb', '#ffce56', '#cccccc'],
    }]
  },
  options: {
    responsive: true,
    plugins: {
      legend: {
        position: 'bottom',
      },
      tooltip: {
        callbacks: {
          label: function (context) {
            const value = context.raw;
            const total = context.dataset.data.reduce((sum, val) => sum + val, 0);
            const percentage = ((value / total) * 100).toFixed(1) + '%';
            return context.label + ': ' + value + ' (' + percentage + ')';
          }
        }
      }
    }
  }
});
</script>
</body>
</html>
`;

(async () => {
    try
    {
        // Fetch open issues
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

        // Fetch closed issues
        const localClosedIssues = await fetchIssues({
            owner: defaultOwner,
            repo: defaultRepoName,
            tag: 'tt-llk',
            state: 'closed',
        });

        const ttMetalClosedIssues = await fetchIssues({
            owner: 'tenstorrent',
            repo: 'tt-metal',
            filterLabel: 'LLK',
            tag: 'tt-metal',
            state: 'closed',
        });

        // Combine and sort open issues
        const combinedIssues = [...localIssues, ...ttMetalIssues];
        const sortedIssues   = combinedIssues.sort((a, b) => {
            const aPriority    = getPriority(a);
            const bPriority    = getPriority(b);
            const priorityDiff = PRIORITY_RANK[aPriority] - PRIORITY_RANK[bPriority];
            return priorityDiff !== 0 ? priorityDiff : new Date(a.created_at) - new Date(b.created_at);
        });

        // Count issues by priority
        const priorityCounts = countIssuesByPriority(combinedIssues);

        // Calculate average closing times
        const closedIssues        = [...localClosedIssues, ...ttMetalClosedIssues];
        const averageClosingTimes = calculateAverageClosingTimes(closedIssues);

        // Generate HTML report
        const rows = sortedIssues.map(generateIssueRow).join('');
        const html = generateHTMLReport(rows, priorityCounts, averageClosingTimes);

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
