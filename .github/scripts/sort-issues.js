// SPDX-FileCopyrightText: (c) 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

// ============================================================================
// IMPORTS & CONFIGURATION
// ============================================================================

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

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

const getPriority = (issue) => {
    const label = issue.labels.find((l) => typeof l.name === 'string' && /^P[0-2]$/.test(l.name));
    return label ? label.name : 'None';
};

const getDaysOpen = (createdAt) => {
    const createdDate = new Date(createdAt);
    const currentDate = new Date();
    return Math.floor((currentDate - createdDate) / (1000 * 60 * 60 * 24));
};

const isOverdue = (priority, daysOpen) => {
    const threshold = PRIORITY_DUE_DAYS[priority];
    return threshold && daysOpen > threshold;
};

const getStyledLabels = (labels) => {
    if (!labels || labels.length === 0)
        return 'None';

    return labels
        .map((label) => {
            const color = label.color || 'cccccc';
            const name  = label.name || 'Unknown';
            return `<span class="label" style="background-color: #${color}; color: ${getTextColor(color)};">${name}</span>`;
        })
        .join(' ');
};

const getTextColor = (backgroundColor) => {
    const r         = parseInt(backgroundColor.substring(0, 2), 16);
    const g         = parseInt(backgroundColor.substring(2, 4), 16);
    const b         = parseInt(backgroundColor.substring(4, 6), 16);
    const luminance = (0.299 * r + 0.587 * g + 0.114 * b) / 255;
    return luminance > 0.5 ? '#000000' : '#ffffff';
};

const formattedDate = new Intl.DateTimeFormat('en-GB', {day: '2-digit', month: 'long', year: 'numeric'}).format(new Date());

// ============================================================================
// GITHUB API - ISSUES & PULL REQUESTS
// ============================================================================

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

// ============================================================================
// DATA PROCESSING & STATISTICS
// ============================================================================

const countIssuesByPriority = (issues) => {
    const counts = {P0: 0, P1: 0, P2: 0, None: 0}; // Ensure all keys are initialized to 0
    issues.forEach((issue) => {
        const priority   = getPriority(issue);
        counts[priority] = (counts[priority] || 0) + 1;
    });
    return counts;
};

const calculateClosedIssuesStats = (issues) => {
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

    const stats = {};
    for (const priority in closingTimes)
    {
        const times     = closingTimes[priority];
        const count     = times.length;
        const avg       = count > 0 ? (times.reduce((sum, time) => sum + time, 0) / count).toFixed(1) : 'N/A';
        stats[priority] = {count, avg};
    }

    return stats;
};

const countBounties = (openIssues, closedIssues) => {
    const hasBountyLabel = (issue) => issue.labels.some((label) => typeof label.name === 'string' && label.name.toLowerCase() === 'bounty');

    const openBounties   = openIssues.filter(hasBountyLabel).length;
    const closedBounties = closedIssues.filter(hasBountyLabel).length;

    return {open: openBounties, closed: closedBounties, total: openBounties + closedBounties};
};

// ============================================================================
// HTML REPORT GENERATION
// ============================================================================

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

const generateHTMLReport = (rows, priorityCounts, closedIssuesStats, bountyCounts) => `
<!DOCTYPE html>
<html>
<head>
<meta charset="UTF-8">
<title>LLK Open Issues Report</title>
<style>
  body { font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; margin: 0; padding: 20px; background-color: #f8f9fa; color: #333; }
  h1 { color: #2c3e50; text-align: center; margin-bottom: 30px; }
  h2 { color: #34495e; border-bottom: 2px solid #3498db; padding-bottom: 10px; }
  .report-container { max-width: 1400px; margin: 0 auto; background: white; border-radius: 10px; box-shadow: 0 4px 6px rgba(0, 0, 0, 0.1); padding: 30px; }
  .stats-container { margin-bottom: 30px; padding: 20px; background-color: #ecf0f1; border-radius: 8px; }
  .priority-stats { display: flex; gap: 20px; margin-bottom: 20px; flex-wrap: wrap; }
  .priority-box { background: white; padding: 15px; border-radius: 8px; box-shadow: 0 2px 4px rgba(0, 0, 0, 0.1); flex: 1; min-width: 150px; text-align: center; }
  .priority-box h3 { margin: 0 0 10px 0; font-size: 18px; }
  .priority-box .count { font-size: 32px; font-weight: bold; margin: 10px 0; }
  .P0 { border-left: 4px solid #e74c3c; } .P0 .count { color: #e74c3c; }
  .P1 { border-left: 4px solid #f39c12; } .P1 .count { color: #f39c12; }
  .P2 { border-left: 4px solid #f1c40f; } .P2 .count { color: #f1c40f; }
  .None { border-left: 4px solid #95a5a6; } .None .count { color: #95a5a6; }
  table { width: 100%; border-collapse: collapse; margin-top: 20px; background: white; }
  th, td { padding: 12px; text-align: left; border-bottom: 1px solid #ddd; }
  th { background-color: #3498db; color: white; font-weight: bold; position: sticky; top: 0; }
  tr:hover { background-color: #f5f5f5; }
  .overdue { background-color: #ffeaa7 !important; }
  .overdue:hover { background-color: #fdcb6e !important; }
  a { color: #3498db; text-decoration: none; } a:hover { text-decoration: underline; }
  .label { padding: 2px 8px; border-radius: 12px; font-size: 12px; margin-right: 4px; display: inline-block; }
  footer { text-align: center; margin-top: 30px; color: #7f8c8d; font-size: 14px; }
  .chart-container { width: 100%; max-width: 600px; margin: 20px auto; }
  .bounty-info { background: #e8f5e8; padding: 15px; border-radius: 8px; margin-bottom: 20px; }
  .bounty-info h3 { margin: 0 0 10px 0; color: #27ae60; }
</style>
<script src="https://cdn.jsdelivr.net/npm/chart.js"></script>
</head>
<body>
<h1>LLK Open Issues Report — ${formattedDate}</h1>
<div class="report-container">
<div class="stats-container">
  <h2>Closed Issues Statistics</h2>
  <div class="priority-stats">
    <div class="priority-box P0"><h3>P0</h3><div class="count">${closedIssuesStats.P0?.count || 0}</div><p>Avg: ${
    closedIssuesStats.P0?.avg || 'N/A'} days</p></div>
    <div class="priority-box P1"><h3>P1</h3><div class="count">${closedIssuesStats.P1?.count || 0}</div><p>Avg: ${
    closedIssuesStats.P1?.avg || 'N/A'} days</p></div>
    <div class="priority-box P2"><h3>P2</h3><div class="count">${closedIssuesStats.P2?.count || 0}</div><p>Avg: ${
    closedIssuesStats.P2?.avg || 'N/A'} days</p></div>
    <div class="priority-box None"><h3>None</h3><div class="count">${closedIssuesStats.None?.count || 0}</div><p>Avg: ${
    closedIssuesStats.None?.avg || 'N/A'} days</p></div>
  </div>

  <div class="bounty-info">
    <h3>🎯 Bounty Tracker</h3>
    <p><strong>Open Bounties:</strong> ${bountyCounts.open} | <strong>Closed Bounties:</strong> ${bountyCounts.closed} | <strong>Total:</strong> ${
    bountyCounts.total}</p>
  </div>

  <h2>Open Issues Distribution</h2>
  <div class="chart-container">
    <canvas id="priorityChart" width="400" height="200"></canvas>
  </div>
  <div class="priority-stats">
    <div class="priority-box P0"><h3>P0</h3><div class="count">${priorityCounts.P0}</div><p>Critical (${PRIORITY_DUE_DAYS.P0} days)</p></div>
    <div class="priority-box P1"><h3>P1</h3><div class="count">${priorityCounts.P1}</div><p>High (${PRIORITY_DUE_DAYS.P1} days)</p></div>
    <div class="priority-box P2"><h3>P2</h3><div class="count">${priorityCounts.P2}</div><p>Medium (${PRIORITY_DUE_DAYS.P2} days)</p></div>
    <div class="priority-box None"><h3>None</h3><div class="count">${priorityCounts.None}</div><p>No Priority</p></div>
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
    <th>Reporter</th>
    <th>Assignees</th>
  </tr>
</thead>
<tbody>
  ${rows}
</tbody>
</table>
<footer>
  Generated by LLK Issue Tracker © 2025
</footer>
<script>
const ctx = document.getElementById('priorityChart').getContext('2d');
new Chart(ctx, {
  type: 'pie',
  data: {
    labels: ['P0 (Critical)', 'P1 (High)', 'P2 (Medium)', 'None'],
    datasets: [{
      data: [${priorityCounts.P0}, ${priorityCounts.P1}, ${priorityCounts.P2}, ${priorityCounts.None}],
      backgroundColor: ['#e74c3c', '#f39c12', '#f1c40f', '#95a5a6'],
      borderWidth: 2,
      borderColor: '#fff'
    }]
  },
  options: {
    responsive: true,
    maintainAspectRatio: false,
    plugins: {
      legend: { position: 'bottom' },
      title: { display: true, text: 'Issues by Priority' }
    }
  }
});
</script>
</body>
</html>
`;

// ============================================================================
// MAIN EXECUTION
// ============================================================================

(async () => {
    try
    {
        console.log('🔄 Fetching issues and pull requests...');

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
        const closedIssues      = [...localClosedIssues, ...ttMetalClosedIssues];
        const closedIssuesStats = calculateClosedIssuesStats(closedIssues);
        const bountyCounts      = countBounties(combinedIssues, closedIssues);

        // Generate HTML report
        const rows = sortedIssues.map(generateIssueRow).join('');
        const html = generateHTMLReport(rows, priorityCounts, closedIssuesStats, bountyCounts);

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
