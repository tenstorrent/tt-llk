#!/usr/bin/env python3
# SPDX-FileCopyrightText: © 2026 Tenstorrent AI ULC
#
# SPDX-License-Identifier: Apache-2.0

"""LLK CodeGen Dashboard — local web UI for monitoring codegen runs."""

import json
from datetime import datetime
from pathlib import Path

from flask import Flask, jsonify

app = Flask(__name__)

REPO_ROOT = Path(__file__).resolve().parent.parent.parent
BASE_DIR = Path("/proj_sw/user_dev/llk_code_gen")

PROJECTS = {
    "quasar": {
        "name": "Quasar CodeGen",
        "desc": "Generate kernels for Quasar architecture",
        "logs_root": BASE_DIR / "quasar",
        "runs_jsonl": BASE_DIR / "quasar" / "runs.jsonl",
    },
    "blackhole": {
        "name": "Blackhole Issue Solver",
        "desc": "Debug and fix Blackhole kernel issues",
        "logs_root": BASE_DIR / "blackhole",
        "runs_jsonl": BASE_DIR / "blackhole" / "runs.jsonl",
    },
}


def get_project(project_id):
    return PROJECTS.get(project_id, PROJECTS["quasar"])


def load_runs(project_id="quasar"):
    """Load runs from runs.jsonl + scan run.json files in logs_root as fallback."""
    proj = get_project(project_id)
    runs_file = proj["runs_jsonl"]
    logs_root = proj["logs_root"]
    seen_ids = set()
    runs = []

    # 1. Load from runs.jsonl (primary source)
    if runs_file.exists():
        for line in runs_file.read_text().strip().splitlines():
            line = line.strip()
            if not line:
                continue
            try:
                run = json.loads(line)
                seen_ids.add(run.get("run_id"))
                _enrich_run(run)
                runs.append(run)
            except (json.JSONDecodeError, ValueError):
                continue

    # 2. Scan run.json files in log directories (catch any not in JSONL)
    if logs_root.exists():
        for run_dir in sorted(logs_root.iterdir()):
            run_json = run_dir / "run.json"
            if not run_json.exists():
                continue
            try:
                run = json.loads(run_json.read_text())
                if run.get("run_id") in seen_ids:
                    continue
                seen_ids.add(run.get("run_id"))
                _enrich_run(run)
                runs.append(run)
            except (json.JSONDecodeError, ValueError):
                continue

    return runs


def _enrich_run(run):
    """Add derived display fields to a run dict."""
    try:
        start = datetime.fromisoformat(run.get("start_time", "").replace("Z", "+00:00"))
        end = datetime.fromisoformat(run.get("end_time", "").replace("Z", "+00:00"))
        run["duration_seconds"] = int((end - start).total_seconds())
        run["duration_display"] = format_duration(run["duration_seconds"])
    except (ValueError, TypeError):
        run["duration_seconds"] = 0
        run["duration_display"] = "—"
    run["display_status"] = compute_display_status(run)


def compute_display_status(run):
    """Compute three-way status: success / compiled / failed."""
    status = run.get("status", "failed")
    if status == "failed":
        return "failed"
    # It at least compiled — check tests
    tests_total = run.get("tests_total", 0)
    tests_passed = run.get("tests_passed", 0)
    if tests_total > 0 and tests_passed == tests_total:
        return "success"
    if status == "success" and tests_total > 0 and tests_passed == tests_total:
        return "success"
    return "compiled"


def format_duration(seconds):
    """Format seconds into human readable string."""
    if seconds < 60:
        return f"{seconds}s"
    minutes = seconds // 60
    secs = seconds % 60
    if minutes < 60:
        return f"{minutes}m {secs}s"
    hours = minutes // 60
    mins = minutes % 60
    return f"{hours}h {mins}m"


@app.route("/")
def index():
    return HTML_PAGE


@app.route("/api/projects")
def api_projects():
    return jsonify(
        [{"id": k, "name": v["name"], "desc": v["desc"]} for k, v in PROJECTS.items()]
    )


@app.route("/api/runs")
def api_runs():
    from flask import request

    project_id = request.args.get("project", "quasar")
    runs = load_runs(project_id)
    return jsonify(runs)


@app.route("/api/run/<run_id>")
def api_run_detail(run_id):
    from flask import request

    project_id = request.args.get("project", "quasar")
    proj = get_project(project_id)
    runs = load_runs(project_id)
    run = next((r for r in runs if r.get("run_id") == run_id), None)
    if not run:
        return jsonify({"error": "not found"}), 404

    # Load agent logs from LOG_DIR
    log_dir = proj["logs_root"] / run_id
    agent_logs = {}
    if log_dir.exists():
        for f in sorted(log_dir.iterdir()):
            if f.name.startswith("agent_") and f.suffix == ".md":
                agent_logs[f.stem] = f.read_text()

    # Load report
    report = None
    report_files = list(log_dir.glob("*_report.md")) if log_dir.exists() else []
    if report_files:
        report = report_files[0].read_text()

    # Load generated kernel source (try LOG_DIR snapshot first, fall back to repo)
    generated_code = None
    gen_file = run.get("generated_file", "")
    if gen_file:
        gen_filename = Path(gen_file).name
        snapshot_path = log_dir / gen_filename if log_dir.exists() else None
        if snapshot_path and snapshot_path.exists():
            generated_code = snapshot_path.read_text()
        else:
            gen_path = REPO_ROOT / gen_file
            if gen_path.exists():
                generated_code = gen_path.read_text()

    # Load reference kernel source
    reference_code = None
    ref_file = run.get("reference_file", "")
    if ref_file:
        ref_path = REPO_ROOT / ref_file
        if ref_path.exists():
            reference_code = ref_path.read_text()

    return jsonify(
        {
            "run": run,
            "agent_logs": agent_logs,
            "report": report,
            "generated_code": generated_code,
            "reference_code": reference_code,
            "log_dir": str(log_dir),
        }
    )


@app.route("/api/batches")
def api_batches():
    from flask import request

    project_id = request.args.get("project", "quasar")
    runs = load_runs(project_id)
    batches = {}
    for run in runs:
        bid = run.get("batch_id") or "manual"
        if bid not in batches:
            batches[bid] = {
                "batch_id": bid,
                "runs": [],
                "success": 0,
                "compiled": 0,
                "failed": 0,
                "total": 0,
            }
        batches[bid]["runs"].append(run["run_id"])
        batches[bid]["total"] += 1
        ds = run["display_status"]
        if ds in batches[bid]:
            batches[bid][ds] += 1
    return jsonify(list(batches.values()))


HTML_PAGE = """<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>LLK CodeGen Dashboard</title>
<style>
  :root {
    --bg: #0d1117;
    --surface: #161b22;
    --border: #30363d;
    --text: #e6edf3;
    --text-muted: #8b949e;
    --green: #3fb950;
    --blue: #58a6ff;
    --red: #f85149;
    --yellow: #d29922;
    --green-bg: rgba(63,185,80,0.12);
    --blue-bg: rgba(88,166,255,0.12);
    --red-bg: rgba(248,81,73,0.12);
  }
  * { margin: 0; padding: 0; box-sizing: border-box; }
  body { background: var(--bg); color: var(--text); font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Helvetica, Arial, sans-serif; font-size: 14px; }

  .header { background: var(--surface); border-bottom: 1px solid var(--border); padding: 16px 24px; display: flex; align-items: center; justify-content: space-between; }
  .header h1 { font-size: 20px; font-weight: 600; }
  .header .stats { display: flex; gap: 16px; }

  .tabs { display: flex; gap: 0; border-bottom: 1px solid var(--border); background: var(--surface); padding: 0 24px; }
  .tab { padding: 10px 16px; cursor: pointer; color: var(--text-muted); border-bottom: 2px solid transparent; font-size: 14px; }
  .tab:hover { color: var(--text); }
  .tab.active { color: var(--text); border-bottom-color: var(--blue); }

  .content { padding: 24px; max-width: 1400px; margin: 0 auto; }

  /* Summary cards */
  .summary-cards { display: grid; grid-template-columns: repeat(auto-fit, minmax(180px, 1fr)); gap: 16px; margin-bottom: 24px; }
  .card { background: var(--surface); border: 1px solid var(--border); border-radius: 8px; padding: 16px; }
  .card .label { color: var(--text-muted); font-size: 12px; text-transform: uppercase; letter-spacing: 0.5px; margin-bottom: 4px; }
  .card .value { font-size: 28px; font-weight: 600; }
  .card .value.green { color: var(--green); }
  .card .value.blue { color: var(--blue); }
  .card .value.red { color: var(--red); }

  /* Filter bar */
  .filter-bar { display: flex; gap: 12px; margin-bottom: 16px; align-items: center; }
  .filter-bar select, .filter-bar input { background: var(--surface); color: var(--text); border: 1px solid var(--border); border-radius: 6px; padding: 6px 12px; font-size: 13px; }
  .filter-bar label { color: var(--text-muted); font-size: 13px; }

  /* Runs table */
  .runs-table { width: 100%; border-collapse: collapse; }
  .runs-table th { text-align: left; padding: 8px 12px; color: var(--text-muted); font-size: 12px; text-transform: uppercase; letter-spacing: 0.5px; border-bottom: 1px solid var(--border); }
  .runs-table td { padding: 10px 12px; border-bottom: 1px solid var(--border); vertical-align: top; }
  .runs-table tr:hover { background: rgba(88,166,255,0.04); cursor: pointer; }
  .runs-table tr:last-child td { border-bottom: none; }

  .status-badge { display: inline-flex; align-items: center; gap: 6px; padding: 2px 10px; border-radius: 12px; font-size: 12px; font-weight: 600; text-transform: uppercase; }
  .status-badge.success { background: var(--green-bg); color: var(--green); }
  .status-badge.compiled { background: var(--blue-bg); color: var(--blue); }
  .status-badge.failed { background: var(--red-bg); color: var(--red); }
  .status-dot { width: 8px; height: 8px; border-radius: 50%; display: inline-block; }
  .status-dot.success { background: var(--green); }
  .status-dot.compiled { background: var(--blue); }
  .status-dot.failed { background: var(--red); }

  .obstacle-text { color: var(--text-muted); font-size: 12px; margin-top: 4px; }
  .kernel-type { color: var(--text-muted); font-size: 11px; }
  .run-type-badge { display: inline-block; padding: 1px 8px; border-radius: 10px; font-size: 11px; font-weight: 600; text-transform: uppercase; }
  .run-type-badge.ci { background: #1a3a4a; color: #4fc3f7; }
  .run-type-badge.manual { background: #3a3a2a; color: #c0b060; }

  /* Detail panel */
  .detail-panel { background: var(--surface); border: 1px solid var(--border); border-radius: 8px; padding: 24px; }
  .detail-panel h2 { margin-bottom: 16px; font-size: 18px; }
  .detail-grid { display: grid; grid-template-columns: 1fr 1fr; gap: 16px; margin-bottom: 24px; }
  .detail-field .label { color: var(--text-muted); font-size: 12px; margin-bottom: 2px; }
  .detail-field .value { font-size: 14px; }

  .phase-card { background: var(--bg); border: 1px solid var(--border); border-radius: 6px; padding: 12px 16px; margin-bottom: 8px; }
  .phase-header { display: flex; justify-content: space-between; align-items: center; margin-bottom: 8px; }
  .phase-name { font-weight: 600; }
  .phase-stats { display: flex; gap: 16px; font-size: 13px; color: var(--text-muted); }
  .phase-error { color: var(--red); font-size: 12px; margin-top: 4px; font-family: monospace; }
  .phase-test-detail { color: var(--text-muted); font-size: 12px; margin-top: 4px; }

  .agent-logs { margin-top: 16px; }
  .agent-log-btn { background: var(--bg); border: 1px solid var(--border); border-radius: 6px; padding: 6px 12px; color: var(--blue); cursor: pointer; font-size: 13px; margin-right: 8px; margin-bottom: 8px; display: inline-block; }
  .agent-log-btn:hover { border-color: var(--blue); }
  .agent-log-btn.active { border-color: var(--blue); background: var(--blue-bg); }
  .log-content { background: var(--bg); border: 1px solid var(--border); border-radius: 6px; padding: 16px; margin-top: 12px; white-space: pre-wrap; font-family: monospace; font-size: 12px; max-height: 500px; overflow-y: auto; line-height: 1.6; }

  .back-btn { background: none; border: 1px solid var(--border); border-radius: 6px; padding: 6px 16px; color: var(--text); cursor: pointer; margin-bottom: 16px; font-size: 13px; }
  .back-btn:hover { border-color: var(--blue); color: var(--blue); }

  /* Pipeline */
  .pipeline { display: flex; align-items: flex-start; gap: 0; margin-bottom: 24px; overflow-x: auto; padding: 8px 0; }
  .pipeline-step { display: flex; align-items: center; }
  .pipeline-node { background: var(--surface); border: 2px solid var(--border); border-radius: 8px; padding: 10px 14px; min-width: 120px; text-align: center; cursor: default; transition: border-color 0.2s; }
  .pipeline-node:hover { border-color: var(--blue); }
  .pipeline-node.ran { border-color: var(--green); }
  .pipeline-node.skipped { border-color: var(--border); opacity: 0.45; }
  .pipeline-node .step-num { font-size: 10px; color: var(--text-muted); text-transform: uppercase; letter-spacing: 0.5px; }
  .pipeline-node .step-name { font-size: 13px; font-weight: 600; margin: 4px 0 2px; }
  .pipeline-node .step-desc { font-size: 11px; color: var(--text-muted); line-height: 1.3; }
  .pipeline-node .step-status { font-size: 11px; margin-top: 4px; }
  .pipeline-arrow { color: var(--border); font-size: 18px; padding: 0 6px; margin-top: 14px; flex-shrink: 0; }
  .pipeline-node.ran .step-status { color: var(--green); }
  .pipeline-loop { display: flex; flex-direction: column; align-items: center; }
  .pipeline-loop-label { font-size: 10px; color: var(--yellow); margin-bottom: 4px; }

  /* Trends */
  .trend-section { margin-bottom: 32px; }
  .trend-section h3 { margin-bottom: 12px; color: var(--text-muted); font-size: 13px; text-transform: uppercase; letter-spacing: 0.5px; }

  .heatmap { width: 100%; border-collapse: collapse; }
  .heatmap th { text-align: left; padding: 6px 10px; color: var(--text-muted); font-size: 12px; }
  .heatmap td { padding: 6px 10px; text-align: center; }
  .heatmap-cell { width: 24px; height: 24px; border-radius: 4px; display: inline-block; }
  .heatmap-cell.success { background: var(--green); }
  .heatmap-cell.compiled { background: var(--blue); }
  .heatmap-cell.failed { background: var(--red); }
  .heatmap-cell.empty { background: var(--border); opacity: 0.3; }

  .chart-container { background: var(--surface); border: 1px solid var(--border); border-radius: 8px; padding: 16px; margin-bottom: 16px; }
  .bar-chart { display: flex; align-items: flex-end; gap: 4px; height: 120px; padding: 0 8px; }
  .bar-group { display: flex; flex-direction: column; align-items: center; flex: 1; height: 100%; justify-content: flex-end; }
  .bar { width: 100%; max-width: 40px; border-radius: 3px 3px 0 0; min-height: 2px; transition: height 0.3s; }
  .bar.success { background: var(--green); }
  .bar.compiled { background: var(--blue); }
  .bar.failed { background: var(--red); }
  .bar-label { font-size: 10px; color: var(--text-muted); margin-top: 4px; text-align: center; white-space: nowrap; overflow: hidden; text-overflow: ellipsis; max-width: 60px; }
  .bar-value { font-size: 10px; color: var(--text-muted); margin-bottom: 2px; }

  .empty-state { text-align: center; padding: 60px 20px; color: var(--text-muted); }
  .empty-state h2 { margin-bottom: 8px; color: var(--text); }

  /* Responsive */
  @media (max-width: 768px) {
    .detail-grid { grid-template-columns: 1fr; }
    .summary-cards { grid-template-columns: repeat(2, 1fr); }
  }
</style>
</head>
<body>

<div class="header">
  <div style="display:flex;align-items:center;gap:16px">
    <h1>LLK CodeGen Dashboard</h1>
    <select id="project-select" onchange="switchProject(this.value)" style="background:var(--bg);color:var(--text);border:1px solid var(--border);border-radius:6px;padding:6px 12px;font-size:13px;cursor:pointer"></select>
  </div>
  <div class="stats" id="header-stats"></div>
</div>

<div class="tabs">
  <div class="tab active" data-tab="overview" onclick="switchTab('overview')">Overview</div>
  <div class="tab" data-tab="trends" onclick="switchTab('trends')">Trends</div>
  <div class="tab" data-tab="detail" id="detail-tab" onclick="switchTab('detail')" style="display:none">Run Detail</div>
</div>

<div class="content" id="content"></div>

<script>
let allRuns = [];
let currentTab = 'overview';
let currentRunId = null;
let currentDetail = null;
let currentProject = 'quasar';
let projects = [];
let filters = { batch: 'all', type: 'all', status: 'all', date: '', model: 'all', runtype: 'all' };

async function init() {
  // Load projects
  const projResp = await fetch('/api/projects');
  projects = await projResp.json();
  const projSelect = document.getElementById('project-select');
  projSelect.innerHTML = projects.map(p =>
    `<option value="${p.id}" ${p.id === currentProject ? 'selected' : ''}>${p.name}</option>`
  ).join('');

  await loadRuns();
}

async function loadRuns() {
  const resp = await fetch('/api/runs?project=' + currentProject);
  allRuns = await resp.json();
  // Sort newest first
  allRuns.sort((a, b) => (b.start_time || '').localeCompare(a.start_time || ''));
  updateHeaderStats();
  if (currentTab === 'detail' && currentRunId) renderDetail();
  else if (currentTab === 'trends') renderTrends();
  else renderOverview();
}

async function switchProject(projectId) {
  currentProject = projectId;
  currentRunId = null;
  currentDetail = null;
  document.getElementById('detail-tab').style.display = 'none';
  filters = { batch: 'all', type: 'all', status: 'all', date: '' };
  switchTab('overview');
  await loadRuns();
}

function updateHeaderStats() {
  const el = document.getElementById('header-stats');
  const total = allRuns.length;
  const success = allRuns.filter(r => r.display_status === 'success').length;
  const compiled = allRuns.filter(r => r.display_status === 'compiled').length;
  const failed = allRuns.filter(r => r.display_status === 'failed').length;
  el.innerHTML = `
    <span style="color:var(--green)">${success} passed</span>
    <span style="color:var(--blue)">${compiled} compiled</span>
    <span style="color:var(--red)">${failed} failed</span>
    <span style="color:var(--text-muted)">${total} total</span>
  `;
}

function switchTab(tab) {
  currentTab = tab;
  document.querySelectorAll('.tab').forEach(t => t.classList.remove('active'));
  document.querySelector(`.tab[data-tab="${tab}"]`).classList.add('active');
  if (tab === 'overview') renderOverview();
  else if (tab === 'trends') renderTrends();
  else if (tab === 'detail') renderDetail();
}

// ==================== OVERVIEW ====================

function saveFilters() {
  const b = document.getElementById('filter-batch');
  const t = document.getElementById('filter-type');
  const s = document.getElementById('filter-status');
  const d = document.getElementById('filter-date');
  const m = document.getElementById('filter-model');
  const rt = document.getElementById('filter-runtype');
  if (b) filters.batch = b.value;
  if (t) filters.type = t.value;
  if (s) filters.status = s.value;
  if (d) filters.date = d.value;
  if (m) filters.model = m.value;
  if (rt) filters.runtype = rt.value;
}

function sel(current, value) { return current === value ? 'selected' : ''; }

function renderOverview() {
  saveFilters();
  const content = document.getElementById('content');
  const runs = getFilteredRuns();

  const success = runs.filter(r => r.display_status === 'success').length;
  const compiled = runs.filter(r => r.display_status === 'compiled').length;
  const failed = runs.filter(r => r.display_status === 'failed').length;
  const totalCompiles = runs.reduce((s, r) => s + (r.compilation_attempts || 0), 0);
  const totalPhases = runs.reduce((s, r) => s + (r.phases_total || 0), 0);
  const avgCompiles = totalPhases > 0 ? (totalCompiles / totalPhases).toFixed(1) : '—';
  const totalTokens = runs.reduce((s, r) => s + ((r.tokens && r.tokens.total) || 0), 0);
  const totalDuration = runs.reduce((s, r) => s + (r.duration_seconds || 0), 0);

  // Get unique batches for filter
  const batches = [...new Set(allRuns.map(r => r.batch_id || 'manual'))];

  content.innerHTML = `
    <div class="filter-bar">
      <label>Batch:</label>
      <select id="filter-batch" onchange="renderOverview()">
        <option value="all" ${sel(filters.batch,'all')}>All runs</option>
        ${batches.map(b => `<option value="${b}" ${sel(filters.batch,b)}>${b}</option>`).join('')}
      </select>
      <label>Kernel type:</label>
      <select id="filter-type" onchange="renderOverview()">
        <option value="all" ${sel(filters.type,'all')}>All</option>
        <option value="sfpu" ${sel(filters.type,'sfpu')}>sfpu</option>
        <option value="math" ${sel(filters.type,'math')}>math</option>
        <option value="pack" ${sel(filters.type,'pack')}>pack</option>
        <option value="unpack" ${sel(filters.type,'unpack')}>unpack</option>
      </select>
      <label>Status:</label>
      <select id="filter-status" onchange="renderOverview()">
        <option value="all" ${sel(filters.status,'all')}>All</option>
        <option value="success" ${sel(filters.status,'success')}>Success</option>
        <option value="compiled" ${sel(filters.status,'compiled')}>Compiled</option>
        <option value="failed" ${sel(filters.status,'failed')}>Failed</option>
      </select>
      <label>Model:</label>
      <select id="filter-model" onchange="renderOverview()">
        <option value="all" ${sel(filters.model,'all')}>All</option>
        ${[...new Set(allRuns.map(r => r.model || 'unknown').filter(m => m !== 'unknown'))].sort().map(m => `<option value="${m}" ${sel(filters.model,m)}>${m}</option>`).join('')}
      </select>
      <label>Run type:</label>
      <select id="filter-runtype" onchange="renderOverview()">
        <option value="all" ${sel(filters.runtype,'all')}>All</option>
        <option value="ci" ${sel(filters.runtype,'ci')}>CI</option>
        <option value="manual" ${sel(filters.runtype,'manual')}>Manual</option>
      </select>
      <label>Date:</label>
      <input type="date" id="filter-date" value="${filters.date}" onchange="renderOverview()">
    </div>

    <div class="summary-cards">
      <div class="card">
        <div class="label">Success</div>
        <div class="value green">${success}</div>
      </div>
      <div class="card">
        <div class="label">Compiled</div>
        <div class="value blue">${compiled}</div>
      </div>
      <div class="card">
        <div class="label">Failed</div>
        <div class="value red">${failed}</div>
      </div>
      <div class="card">
        <div class="label">Avg Compiles/Phase</div>
        <div class="value">${avgCompiles}</div>
      </div>
      <div class="card">
        <div class="label">Total Tokens</div>
        <div class="value">${formatTokens(totalTokens)}</div>
      </div>
      <div class="card">
        <div class="label">Total Time</div>
        <div class="value">${formatDuration(totalDuration)}</div>
      </div>
    </div>

    ${runs.length === 0 ? `
      <div class="empty-state">
        <h2>No runs yet</h2>
        <p>Run a codegen prompt and results will appear here.</p>
      </div>
    ` : `
      <table class="runs-table">
        <thead>
          <tr>
            <th>Status</th>
            <th>Kernel</th>
            <th>Prompt</th>
            <th>Model</th>
            <th>Type</th>
            <th>Phases</th>
            <th>Compiles</th>
            <th>Debug</th>
            <th>Tests</th>
            <th>Duration</th>
            <th>Date</th>
          </tr>
        </thead>
        <tbody>
          ${runs.map(r => renderRunRow(r)).join('')}
        </tbody>
      </table>
    `}
  `;
}

function getFilteredRuns() {
  let runs = [...allRuns];
  if (filters.batch !== 'all') {
    runs = runs.filter(r => (r.batch_id || 'manual') === filters.batch);
  }
  if (filters.type !== 'all') {
    runs = runs.filter(r => r.kernel_type === filters.type);
  }
  if (filters.status !== 'all') {
    runs = runs.filter(r => r.display_status === filters.status);
  }
  if (filters.model !== 'all') {
    runs = runs.filter(r => (r.model || 'unknown') === filters.model);
  }
  if (filters.runtype !== 'all') {
    runs = runs.filter(r => (r.run_type || 'manual') === filters.runtype);
  }
  if (filters.date) {
    runs = runs.filter(r => (r.start_time || '').slice(0, 10) === filters.date);
  }
  return runs;
}

function renderRunRow(r) {
  const ds = r.display_status;
  const date = r.start_time ? r.start_time.slice(0, 10) : '—';
  const tests = r.tests_total > 0 ? `${r.tests_passed}/${r.tests_total}` : '—';
  return `
    <tr onclick="openDetail('${r.run_id}')">
      <td><span class="status-badge ${ds}"><span class="status-dot ${ds}"></span>${ds}</span></td>
      <td><strong>${r.kernel}</strong><br><span class="kernel-type">${r.kernel_type} · ${r.arch}</span></td>
      <td style="max-width:250px;overflow:hidden;text-overflow:ellipsis;white-space:nowrap">${r.prompt || '—'}</td>
      <td>${r.model || '—'}</td>
      <td><span class="run-type-badge ${r.run_type || 'manual'}">${r.run_type || 'manual'}</span></td>
      <td>${r.phases_completed || 0}/${r.phases_total || 0}</td>
      <td>${r.compilation_attempts || 0}</td>
      <td>${r.debug_cycles || 0}</td>
      <td>${tests}</td>
      <td>${r.duration_display || '—'}</td>
      <td>${date}</td>
    </tr>
    ${r.obstacle ? `<tr onclick="openDetail('${r.run_id}')"><td></td><td colspan="10" class="obstacle-text">⚠ ${r.obstacle}</td></tr>` : ''}
  `;
}

// ==================== DETAIL ====================

async function openDetail(runId) {
  currentRunId = runId;
  document.getElementById('detail-tab').style.display = '';
  switchTab('detail');
}

async function renderDetail() {
  const content = document.getElementById('content');
  if (!currentRunId) {
    content.innerHTML = '<div class="empty-state"><p>Select a run from Overview.</p></div>';
    return;
  }

  content.innerHTML = '<div class="empty-state"><p>Loading...</p></div>';

  const resp = await fetch(`/api/run/${currentRunId}?project=${currentProject}`);
  const data = await resp.json();
  currentDetail = data;
  const r = data.run;
  const ds = r.display_status;
  const tests = r.tests_total > 0 ? `${r.tests_passed}/${r.tests_total}` : 'N/A';

  content.innerHTML = `
    <button class="back-btn" onclick="switchTab('overview')">← Back to Overview</button>

    <div class="detail-panel">
      <div style="display:flex;justify-content:space-between;align-items:center;margin-bottom:16px">
        <h2>${r.kernel} <span class="kernel-type" style="font-size:14px;font-weight:normal">${r.kernel_type} · ${r.arch}</span></h2>
        <span class="status-badge ${ds}"><span class="status-dot ${ds}"></span>${ds}</span>
      </div>

      <div class="detail-grid">
        <div class="detail-field"><div class="label">Prompt</div><div class="value">${r.prompt || '—'}</div></div>
        <div class="detail-field"><div class="label">Run ID</div><div class="value" style="font-family:monospace;font-size:12px">${r.run_id}</div></div>
        <div class="detail-field"><div class="label">Duration</div><div class="value">${r.duration_display}</div></div>
        <div class="detail-field"><div class="label">Lines Generated</div><div class="value">${r.lines_generated}</div></div>
        <div class="detail-field"><div class="label">Reference</div><div class="value" style="font-size:12px">${r.reference_file}</div></div>
        <div class="detail-field"><div class="label">Generated</div><div class="value" style="font-size:12px">${r.generated_file}</div></div>
        <div class="detail-field"><div class="label">Compile Attempts</div><div class="value">${r.compilation_attempts}</div></div>
        <div class="detail-field"><div class="label">Debug Cycles</div><div class="value">${r.debug_cycles}</div></div>
        <div class="detail-field"><div class="label">Tests</div><div class="value">${tests}</div></div>
        <div class="detail-field"><div class="label">Prettified</div><div class="value">${r.prettified ? 'Yes' : 'No'}</div></div>
        <div class="detail-field"><div class="label">Tokens</div><div class="value">${r.tokens && r.tokens.total > 0 ? formatTokens(r.tokens.total) + ` (in: ${formatTokens(r.tokens.input)}, out: ${formatTokens(r.tokens.output)}, cache: ${formatTokens(r.tokens.cache_read)})` : 'Not captured'}</div></div>
        <div class="detail-field"><div class="label">Model</div><div class="value">${r.model || 'Unknown'}</div></div>
        <div class="detail-field"><div class="label">Run Type</div><div class="value"><span class="run-type-badge ${r.run_type || 'manual'}">${r.run_type === 'ci' ? 'CI (scheduled)' : 'Manual'}</span></div></div>
        <div class="detail-field"><div class="label">Batch</div><div class="value">${r.batch_id || 'Manual run'}</div></div>
      </div>

      ${r.obstacle ? `<div style="background:var(--red-bg);border:1px solid var(--red);border-radius:6px;padding:12px;margin-bottom:16px;color:var(--red)">⚠ Obstacle: ${r.obstacle}</div>` : ''}

      <h3 style="margin-bottom:12px">Pipeline</h3>
      <div class="pipeline">
        ${renderPipeline(r, data.agent_logs)}
      </div>

      <h3 style="margin-bottom:12px">Phases</h3>
      ${(r.per_phase || []).map(p => `
        <div class="phase-card">
          <div class="phase-header">
            <span class="phase-name">Phase ${p.phase}: ${p.name}</span>
          </div>
          <div class="phase-stats">
            <span style="color: ${p.compilation_attempts <= 1 ? 'var(--green)' : 'var(--yellow)'}">
              ${p.compilation_attempts <= 1 ? 'Compiled on first try' : 'Compiled after ' + p.compilation_attempts + ' attempts'}
            </span>
            ${p.debug_cycles > 0 ? `<span style="color:var(--yellow)">${p.debug_cycles} debug cycle${p.debug_cycles > 1 ? 's' : ''}</span>` : ''}
          </div>
          ${p.first_compile_error ? `<div class="phase-error">Initial error (fixed): ${p.first_compile_error}</div>` : ''}
          <div style="margin-top:8px;font-size:13px">
            ${p.test_result === 'passed' ? '<span style="color:var(--green)">Tests passed</span>' + (p.test_details ? ' — ' + p.test_details : '') :
              p.test_result === 'failed' ? '<span style="color:var(--red)">Tests failed</span>' + (p.test_details ? ' — ' + p.test_details : '') :
              '<span style="color:var(--text-muted)">Tests not run</span>' + (p.test_details ? ' — ' + p.test_details : '')}
          </div>
        </div>
      `).join('')}

      <div class="agent-logs">
        <h3 style="margin-bottom:12px">Code</h3>
        <div id="code-btns">
          ${data.generated_code ? '<span class="agent-log-btn" onclick="showCode(&quot;generated&quot;, this)">Generated Kernel</span>' : ''}
          ${data.reference_code ? '<span class="agent-log-btn" onclick="showCode(&quot;reference&quot;, this)">Reference Kernel</span>' : ''}
          ${!data.generated_code && !data.reference_code ? '<span style="color:var(--text-muted)">No kernel files available</span>' : ''}
        </div>
        <div id="code-content"></div>
      </div>

      <div class="agent-logs" style="margin-top:24px">
        <h3 style="margin-bottom:12px">Agent Logs</h3>
        <div id="agent-btns">
          ${orderAgentLogs(Object.keys(data.agent_logs || {})).map(name =>
            `<span class="agent-log-btn" onclick="showAgentLog('${name}', this)">${name.replace('agent_', '')}</span>`
          ).join('')}
          ${Object.keys(data.agent_logs || {}).length === 0 ? '<span style="color:var(--text-muted)">No agent logs available</span>' : ''}
        </div>
        <div id="agent-log-content"></div>
      </div>
    </div>
  `;
}

function showCode(which, btn) {
  document.querySelectorAll('#code-btns .agent-log-btn').forEach(b => b.classList.remove('active'));
  btn.classList.add('active');
  const el = document.getElementById('code-content');
  const code = which === 'generated' ? currentDetail.generated_code : currentDetail.reference_code;
  const path = which === 'generated' ? currentDetail.run.generated_file : currentDetail.run.reference_file;
  if (!code) { el.innerHTML = '<p style="color:var(--text-muted);margin-top:12px">File not found.</p>'; return; }
  const lines = code.split('\\n');
  const numbered = lines.map((line, i) => `<span style="color:var(--text-muted);user-select:none">${String(i+1).padStart(4)} </span>${escapeHtml(line)}`).join('\\n');
  el.innerHTML = `<div style="color:var(--text-muted);font-size:11px;margin-top:12px;margin-bottom:4px">${path}</div><div class="log-content" style="font-size:13px;line-height:1.5">${numbered}</div>`;
}

function showAgentLog(name, btn) {
  document.querySelectorAll('#agent-btns .agent-log-btn').forEach(b => b.classList.remove('active'));
  btn.classList.add('active');
  const logs = currentDetail.agent_logs || {};
  const logEl = document.getElementById('agent-log-content');
  logEl.innerHTML = logs[name] ? `<div class="log-content">${escapeHtml(logs[name])}</div>` : '<p style="color:var(--text-muted);margin-top:12px">No log content.</p>';
}

// ==================== TRENDS ====================

function renderTrends() {
  const content = document.getElementById('content');
  if (allRuns.length === 0) {
    content.innerHTML = '<div class="empty-state"><h2>No data yet</h2><p>Run some codegen prompts to see trends.</p></div>';
    return;
  }

  // Group by week
  const weeks = {};
  allRuns.forEach(r => {
    const d = r.start_time ? r.start_time.slice(0, 10) : 'unknown';
    // Group by ISO week
    const dt = new Date(r.start_time);
    const weekStart = new Date(dt);
    weekStart.setDate(dt.getDate() - dt.getDay());
    const wk = weekStart.toISOString().slice(0, 10);
    if (!weeks[wk]) weeks[wk] = [];
    weeks[wk].push(r);
  });
  const sortedWeeks = Object.keys(weeks).sort();

  // Get all unique kernels
  const allKernels = [...new Set(allRuns.map(r => r.kernel))].sort();

  // Compile attempts bar chart data
  const compileData = sortedWeeks.map(w => {
    const runs = weeks[w];
    const totalCompiles = runs.reduce((s, r) => s + (r.compilation_attempts || 0), 0);
    const totalPhases = runs.reduce((s, r) => s + (r.phases_total || 0), 0);
    return { week: w, avg: totalPhases > 0 ? totalCompiles / totalPhases : 0 };
  });
  const maxAvg = Math.max(...compileData.map(d => d.avg), 1);

  content.innerHTML = `
    <div class="trend-section">
      <h3>Status Distribution by Run</h3>
      <div class="chart-container">
        <div class="bar-chart">
          ${allRuns.slice().reverse().map(r => `
            <div class="bar-group">
              <div class="bar-value">${r.compilation_attempts || 0}c</div>
              <div class="bar ${r.display_status}" style="height:${Math.max(8, (r.compilation_attempts || 1) / Math.max(...allRuns.map(x => x.compilation_attempts || 1)) * 100)}%"></div>
              <div class="bar-label" title="${r.kernel}">${r.kernel}</div>
            </div>
          `).join('')}
        </div>
      </div>
    </div>

    <div class="trend-section">
      <h3>Avg Compile Attempts per Phase (by Week)</h3>
      <div class="chart-container">
        <div class="bar-chart">
          ${compileData.map(d => `
            <div class="bar-group">
              <div class="bar-value">${d.avg.toFixed(1)}</div>
              <div class="bar compiled" style="height:${Math.max(8, d.avg / maxAvg * 100)}%"></div>
              <div class="bar-label">${d.week.slice(5)}</div>
            </div>
          `).join('')}
        </div>
      </div>
    </div>

    <div class="trend-section">
      <h3>Kernel Heatmap</h3>
      <div class="chart-container">
        <table class="heatmap">
          <thead>
            <tr>
              <th>Kernel</th>
              ${sortedWeeks.map(w => `<th>${w.slice(5)}</th>`).join('')}
            </tr>
          </thead>
          <tbody>
            ${allKernels.map(k => `
              <tr>
                <td style="font-weight:600">${k}</td>
                ${sortedWeeks.map(w => {
                  const run = weeks[w].find(r => r.kernel === k);
                  if (!run) return '<td><span class="heatmap-cell empty"></span></td>';
                  return `<td><span class="heatmap-cell ${run.display_status}" title="${run.display_status}: ${run.compilation_attempts} compiles"></span></td>`;
                }).join('')}
              </tr>
            `).join('')}
          </tbody>
        </table>
      </div>
    </div>
  `;
}

// ==================== PIPELINE ====================

const AGENT_ORDER = ['agent_arch_lookup', 'agent_analyzer', 'agent_planner', 'agent_writer', 'agent_debugger', 'agent_test_writer', 'agent_tester', 'agent_prettifier'];

function orderAgentLogs(names) {
  const ordered = AGENT_ORDER.filter(a => names.includes(a));
  const extra = names.filter(a => !AGENT_ORDER.includes(a)).sort();
  return ordered.concat(extra);
}

function renderPipeline(run, agentLogs) {
  const agents = run.agents || [];
  const logs = agentLogs || {};
  const phases = run.per_phase || [];

  const ca = run.compilation_attempts || 0;
  const debugCycles = run.debug_cycles || 0;
  const phasesTotal = run.phases_total || 1;
  const tr = run.tests_total || 0;
  const tp = run.tests_passed || 0;
  const hasCompileDebug = ca > phasesTotal;
  const hasTestFailure = tr > 0 && tp < tr;

  const steps = [
    { id: 'arch_lookup', name: 'Research', desc: 'Architecture discovery via Confluence & DeepWiki', log: 'agent_arch_lookup' },
    { id: 'analyzer', name: 'Analyze', desc: 'Study reference implementation, identify phases', log: 'agent_analyzer' },
    { id: 'planner', name: 'Plan', desc: 'Map instructions, design target implementation', log: 'agent_planner' },
    { id: 'writer', name: 'Write', desc: 'Generate kernel code from spec', log: 'agent_writer' },
    { id: 'fix_compile', name: 'Fix Compile', desc: 'Fix compilation errors', log: 'agent_debugger' },
    { id: 'test_writer', name: 'Create Tests', desc: 'Create test suite if none exists', log: 'agent_test_writer' },
    { id: 'tester', name: 'Test', desc: 'Run functional tests on simulator', log: 'agent_tester' },
    { id: 'fix_tests', name: 'Fix Tests', desc: 'Fix runtime/test failures', log: 'agent_debugger' },
    { id: 'prettifier', name: 'Prettify', desc: 'Refactor for readability & style', log: 'agent_prettifier' },
  ];

  return steps.map((step, i) => {
    let ran = false;
    let statusText = '';

    if (step.id === 'fix_compile') {
      ran = hasCompileDebug;
      if (!ran) { statusText = 'not needed'; }
      else { statusText = '⚠ ' + (ca - phasesTotal) + ' extra compile' + (ca - phasesTotal > 1 ? 's' : ''); }
    } else if (step.id === 'test_writer') {
      ran = agents.includes('test_writer') || !!logs['agent_test_writer'];
      if (!ran) { statusText = 'tests existed'; }
      else { statusText = '✓ tests created'; }
    } else if (step.id === 'fix_tests') {
      ran = hasTestFailure && debugCycles > 0;
      if (!ran) { statusText = tr === 0 ? 'no tests to fix' : 'not needed'; }
      else { statusText = '⚠ fixing test failures'; }
    } else if (step.id === 'writer') {
      ran = agents.includes('writer') || !!logs['agent_writer'];
      statusText = ran ? (ca <= phasesTotal ? '✓ clean compile' : '✓ compiled (' + ca + ' attempts)') : 'skipped';
    } else if (step.id === 'tester') {
      ran = agents.includes('tester') || !!logs['agent_tester'];
      if (!ran) { statusText = 'skipped'; }
      else { statusText = tr === 0 ? '— no tests available' : tp === tr ? '✓ ' + tp + '/' + tr + ' passed' : '✗ ' + tp + '/' + tr + ' passed'; }
    } else if (step.id === 'prettifier') {
      ran = agents.includes('prettifier') || !!logs['agent_prettifier'];
      statusText = ran ? (run.prettified ? '✓ done' : '— skipped') : 'skipped';
    } else {
      ran = agents.includes(step.id) || !!logs[step.log];
      statusText = ran ? '✓ done' : 'skipped';
    }

    const nodeClass = ran ? 'ran' : 'skipped';
    const arrow = i < steps.length - 1 ? '<span class="pipeline-arrow">→</span>' : '';

    let node = `<div class="pipeline-node ${nodeClass}">
        <div class="step-num">Step ${i + 1}</div>
        <div class="step-name">${step.name}</div>
        <div class="step-desc">${step.desc}</div>
        <div class="step-status">${statusText}</div>
      </div>`;

    return `<div class="pipeline-step">${node}${arrow}</div>`;
  }).join('');
}

// ==================== UTILS ====================

function formatTokens(n) {
  if (!n || n === 0) return '0';
  if (n >= 1000000) return (n / 1000000).toFixed(1) + 'M';
  if (n >= 1000) return (n / 1000).toFixed(1) + 'k';
  return n.toString();
}

function formatDuration(seconds) {
  if (!seconds) return '—';
  if (seconds < 60) return seconds + 's';
  const m = Math.floor(seconds / 60);
  const s = seconds % 60;
  if (m < 60) return m + 'm ' + s + 's';
  const h = Math.floor(m / 60);
  return h + 'h ' + (m % 60) + 'm';
}

function escapeHtml(str) {
  return str.replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;');
}

// Init
init();
</script>
</body>
</html>
"""


if __name__ == "__main__":
    import argparse

    parser = argparse.ArgumentParser(description="LLK CodeGen Dashboard")
    parser.add_argument("--port", type=int, default=8080, help="Port to serve on")
    parser.add_argument("--host", type=str, default="0.0.0.0", help="Host to bind to")
    args = parser.parse_args()

    print(f"Dashboard: http://localhost:{args.port}")
    print(f"  Network: http://0.0.0.0:{args.port}")
    print(f"  Projects:")
    for pid, proj in PROJECTS.items():
        print(f"    {pid}: logs={proj['logs_root']}, data={proj['runs_jsonl']}")
    app.run(host=args.host, port=args.port, debug=False)
