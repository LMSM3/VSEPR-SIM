#!/usr/bin/env python3
"""
pipeline/postprocess.py — VSEPR-SIM result post-processor.

Reads:
  out/pipeline/results/ctest_results.json
  out/pipeline/results/pytest_results.json
  out/pipeline/results/pipeline_summary.json
  docs/figures/*.png

Writes:
  out/pipeline/reports/index.html       — full dashboard
  out/pipeline/reports/results.json     — merged, normalised result record
  out/pipeline/reports/test_log.csv     — flat per-test CSV for downstream use
  out/pipeline/reports/figures/         — PNG copies (already staged by shell)

Design: deterministic, no hidden state.  Re-running produces the same output
for the same input.

Usage (called by run_pipeline.sh stage 5, or directly):
  python pipeline/postprocess.py \\
      --results-dir out/pipeline/results \\
      --reports-dir out/pipeline/reports \\
      --fig-dir     out/pipeline/reports/figures \\
      --root        .
"""

from __future__ import annotations

import argparse
import csv
import json
import os
import shutil
import sys
from datetime import datetime, timezone
from pathlib import Path

# ── Jinja2 for HTML rendering ────────────────────────────────────────────────
try:
    from jinja2 import Environment, FileSystemLoader, select_autoescape
    HAS_JINJA = True
except ImportError:
    HAS_JINJA = False

ROOT_DIR  = Path(__file__).resolve().parent.parent
TMPL_DIR  = ROOT_DIR / "web" / "templates"
STATIC_DIR = ROOT_DIR / "web" / "static"


# =============================================================================
# Data loading
# =============================================================================

def _load_json_safe(path: Path) -> dict:
    try:
        with open(path) as f:
            return json.load(f)
    except Exception:
        return {}


def load_results(results_dir: Path) -> dict:
    """Merge all JSON result files into one normalised record."""
    ctest  = _load_json_safe(results_dir / "ctest_results.json")
    pytest = _load_json_safe(results_dir / "pytest_results.json")
    summary = _load_json_safe(results_dir / "pipeline_summary.json")

    # Normalise pytest-json-report vs our fallback format
    if "summary" in pytest:
        ps = pytest["summary"]
        pytest_pass  = ps.get("passed", 0)
        pytest_fail  = ps.get("failed", 0)
        pytest_total = ps.get("total",  0)
        pytest_status = "pass" if pytest_fail == 0 else "fail"
    else:
        pytest_pass  = int(pytest.get("passed", 0))
        pytest_fail  = int(pytest.get("failed", 0))
        pytest_total = int(pytest.get("total",  pytest_pass + pytest_fail))
        pytest_status = pytest.get("status", "pass" if pytest_fail == 0 else "fail")

    ctest_pass  = int(ctest.get("passed", 0))
    ctest_fail  = int(ctest.get("failed", 0))
    ctest_total = int(ctest.get("total",  ctest_pass + ctest_fail))
    ctest_status = ctest.get("status", "pass")

    total_pass  = ctest_pass  + pytest_pass
    total_fail  = ctest_fail  + pytest_fail
    total_tests = ctest_total + pytest_total
    overall     = "pass" if total_fail == 0 else "fail"

    stages = summary.get("stages", [])

    return {
        "timestamp":    datetime.now(timezone.utc).isoformat(),
        "version":      summary.get("version", "3.0.0"),
        "overall":      overall,
        "total_tests":  total_tests,
        "total_pass":   total_pass,
        "total_fail":   total_fail,
        "pass_rate":    round(100.0 * total_pass / max(total_tests, 1), 1),
        "ctest": {
            "total": ctest_total, "passed": ctest_pass,
            "failed": ctest_fail, "status": ctest_status,
        },
        "pytest": {
            "total": pytest_total, "passed": pytest_pass,
            "failed": pytest_fail, "status": pytest_status,
        },
        "stages": stages,
        "elapsed_s": summary.get("total_elapsed_s", 0),
        "raw": {
            "ctest": ctest,
            "pytest": pytest,
            "pipeline": summary,
        },
    }


# =============================================================================
# CSV export
# =============================================================================

def write_test_csv(data: dict, path: Path):
    """Write a flat per-suite CSV row for each test suite."""
    path.parent.mkdir(parents=True, exist_ok=True)
    rows = [
        {
            "suite":   "ctest",
            "total":   data["ctest"]["total"],
            "passed":  data["ctest"]["passed"],
            "failed":  data["ctest"]["failed"],
            "status":  data["ctest"]["status"],
            "timestamp": data["timestamp"],
        },
        {
            "suite":   "pytest",
            "total":   data["pytest"]["total"],
            "passed":  data["pytest"]["passed"],
            "failed":  data["pytest"]["failed"],
            "status":  data["pytest"]["status"],
            "timestamp": data["timestamp"],
        },
    ]
    with open(path, "w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=["suite","total","passed","failed","status","timestamp"])
        w.writeheader()
        w.writerows(rows)
    print(f"  [CSV] {path}")


# =============================================================================
# Figure catalogue
# =============================================================================

def collect_figures(fig_dir: Path) -> list[dict]:
    """Return sorted list of {name, path, group} for each PNG figure."""
    if not fig_dir.exists():
        return []
    figures = []
    for p in sorted(fig_dir.glob("*.png")):
        name = p.stem
        # Derive group from prefix
        if name.startswith("overview_"):
            group = "Overview"
        elif name.startswith("stress") or name.startswith("metal"):
            group = "Materials"
        elif name.startswith("mol_") or name.startswith("molecule"):
            group = "Molecular"
        elif name.startswith("ehd"):
            group = "EHD"
        elif name.startswith("debye") or name.startswith("fire") or \
             name.startswith("flow") or name.startswith("ion"):
            group = "Thermal / Flow"
        else:
            group = "Other"
        figures.append({
            "name":     name,
            "filename": p.name,
            "path":     str(p),
            "group":    group,
        })
    return figures


# =============================================================================
# HTML generation
# =============================================================================

HTML_TEMPLATE = """\
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>VSEPR-SIM — Test Results</title>
  <style>
    /* ── Reset + base ── */
    *, *::before, *::after { box-sizing: border-box; margin: 0; padding: 0; }
    html { scroll-behavior: smooth; }
    body {
      font-family: 'Segoe UI', system-ui, sans-serif;
      background: #0f1117;
      color: #e0e0e0;
      min-height: 100vh;
    }

    /* ── Header ── */
    header {
      background: linear-gradient(135deg, #1a2744 0%, #0f1117 100%);
      border-bottom: 2px solid #2e86c1;
      padding: 1.5rem 2rem;
      display: flex;
      align-items: center;
      gap: 1.5rem;
    }
    .logo-text { font-size: 1.6rem; font-weight: 700; color: #2e86c1; letter-spacing: 0.04em; }
    .logo-sub  { font-size: 0.85rem; color: #7f8c8d; margin-top: 0.2rem; }
    .header-status {
      margin-left: auto;
      padding: 0.4rem 1.2rem;
      border-radius: 2rem;
      font-weight: 600;
      font-size: 0.9rem;
      letter-spacing: 0.05em;
    }
    .status-pass { background: #145a32; color: #2ecc71; border: 1px solid #27ae60; }
    .status-fail { background: #641e16; color: #e74c3c; border: 1px solid #c0392b; }

    /* ── Nav ── */
    nav {
      background: #161b27;
      padding: 0.6rem 2rem;
      display: flex;
      gap: 1.5rem;
      border-bottom: 1px solid #2c3e50;
      position: sticky;
      top: 0;
      z-index: 100;
    }
    nav a {
      color: #7f8c8d;
      text-decoration: none;
      font-size: 0.88rem;
      font-weight: 500;
      padding: 0.25rem 0;
      border-bottom: 2px solid transparent;
      transition: all 0.2s;
    }
    nav a:hover { color: #2e86c1; border-bottom-color: #2e86c1; }

    /* ── Main ── */
    main { max-width: 1300px; margin: 0 auto; padding: 2rem; }

    /* ── Section headers ── */
    .section-title {
      font-size: 1.1rem;
      font-weight: 600;
      color: #2e86c1;
      letter-spacing: 0.08em;
      text-transform: uppercase;
      margin: 2.5rem 0 1rem;
      padding-bottom: 0.4rem;
      border-bottom: 1px solid #2c3e50;
    }

    /* ── Metric grid ── */
    .metric-grid {
      display: grid;
      grid-template-columns: repeat(auto-fit, minmax(180px, 1fr));
      gap: 1rem;
      margin-bottom: 1.5rem;
    }
    .metric-card {
      background: #161b27;
      border: 1px solid #2c3e50;
      border-radius: 8px;
      padding: 1.2rem 1.5rem;
      text-align: center;
    }
    .metric-card .val {
      font-size: 2.4rem;
      font-weight: 700;
      line-height: 1;
    }
    .metric-card .lbl {
      font-size: 0.78rem;
      color: #7f8c8d;
      text-transform: uppercase;
      letter-spacing: 0.08em;
      margin-top: 0.4rem;
    }
    .val-pass  { color: #2ecc71; }
    .val-fail  { color: #e74c3c; }
    .val-total { color: #2e86c1; }
    .val-rate  { color: #f39c12; }
    .val-time  { color: #9b59b6; }

    /* ── Suite table ── */
    table {
      width: 100%;
      border-collapse: collapse;
      font-size: 0.88rem;
      background: #161b27;
      border-radius: 8px;
      overflow: hidden;
      margin-bottom: 1.5rem;
    }
    thead { background: #1e2d3e; }
    th { padding: 0.75rem 1rem; text-align: left; font-weight: 600;
         color: #7f8c8d; text-transform: uppercase; font-size: 0.78rem;
         letter-spacing: 0.06em; }
    td { padding: 0.7rem 1rem; border-top: 1px solid #2c3e50; }
    tr:hover td { background: #1a2035; }
    .badge {
      display: inline-block;
      padding: 0.2rem 0.7rem;
      border-radius: 1rem;
      font-size: 0.78rem;
      font-weight: 600;
    }
    .badge-pass { background: #145a32; color: #2ecc71; }
    .badge-fail { background: #641e16; color: #e74c3c; }

    /* ── Stage pipeline ── */
    .stage-row {
      display: flex;
      gap: 0.5rem;
      flex-wrap: wrap;
      margin-bottom: 1.5rem;
    }
    .stage-box {
      padding: 0.5rem 1rem;
      border-radius: 6px;
      font-size: 0.82rem;
      font-weight: 600;
      border: 1px solid transparent;
    }
    .stage-pass { background: #145a32; border-color: #27ae60; color: #2ecc71; }
    .stage-fail { background: #641e16; border-color: #c0392b; color: #e74c3c; }
    .stage-skip { background: #1e2d3e; border-color: #2c3e50; color: #7f8c8d; }

    /* ── Figure gallery ── */
    .gallery-controls {
      display: flex;
      gap: 0.5rem;
      flex-wrap: wrap;
      margin-bottom: 1rem;
    }
    .filter-btn {
      padding: 0.3rem 0.9rem;
      border-radius: 1.5rem;
      background: #1e2d3e;
      border: 1px solid #2c3e50;
      color: #7f8c8d;
      font-size: 0.8rem;
      cursor: pointer;
      transition: all 0.15s;
    }
    .filter-btn.active, .filter-btn:hover {
      background: #2e86c1;
      border-color: #2e86c1;
      color: white;
    }
    .figure-grid {
      display: grid;
      grid-template-columns: repeat(auto-fill, minmax(280px, 1fr));
      gap: 1rem;
    }
    .fig-card {
      background: #161b27;
      border: 1px solid #2c3e50;
      border-radius: 8px;
      overflow: hidden;
      transition: transform 0.15s, border-color 0.15s;
      cursor: pointer;
    }
    .fig-card:hover {
      transform: translateY(-2px);
      border-color: #2e86c1;
    }
    .fig-card img {
      width: 100%;
      height: 180px;
      object-fit: cover;
      display: block;
      background: #1e2d3e;
    }
    .fig-card .fig-label {
      padding: 0.5rem 0.75rem;
      font-size: 0.78rem;
      color: #7f8c8d;
      white-space: nowrap;
      overflow: hidden;
      text-overflow: ellipsis;
    }

    /* ── Lightbox ── */
    #lightbox {
      display: none;
      position: fixed;
      inset: 0;
      background: rgba(0,0,0,0.9);
      z-index: 1000;
      align-items: center;
      justify-content: center;
      flex-direction: column;
    }
    #lightbox.open { display: flex; }
    #lightbox img { max-width: 90vw; max-height: 85vh; border-radius: 4px; }
    #lightbox-label { color: #ccc; margin-top: 0.75rem; font-size: 0.85rem; }
    #lightbox-close {
      position: absolute;
      top: 1rem; right: 1.5rem;
      font-size: 2rem;
      color: white;
      cursor: pointer;
      line-height: 1;
    }

    /* ── Footer ── */
    footer {
      text-align: center;
      padding: 2rem;
      color: #4a5568;
      font-size: 0.8rem;
      border-top: 1px solid #2c3e50;
      margin-top: 3rem;
    }

    /* ── Log section ── */
    .log-box {
      background: #0d1117;
      border: 1px solid #2c3e50;
      border-radius: 8px;
      padding: 1rem 1.2rem;
      font-family: 'Consolas', 'Cascadia Code', monospace;
      font-size: 0.8rem;
      color: #7ec8e3;
      max-height: 420px;
      overflow-y: auto;
      white-space: pre-wrap;
      word-break: break-all;
    }
  </style>
</head>
<body>

<header>
  <div>
    <div class="logo-text">VSEPR-SIM</div>
    <div class="logo-sub">Research-Oriented Atomistic Simulation Platform — v{{ version }}</div>
  </div>
  <div class="header-status {{ 'status-pass' if overall == 'pass' else 'status-fail' }}">
    {{ '✔ ALL PASS' if overall == 'pass' else '✘ FAILURES DETECTED' }}
  </div>
</header>

<nav>
  <a href="#summary">Summary</a>
  <a href="#suites">Test Suites</a>
  <a href="#stages">Pipeline</a>
  <a href="#figures">Figures</a>
  <a href="#log">Log</a>
</nav>

<main>

  <!-- ── Summary ── -->
  <div id="summary">
    <div class="section-title">Run Summary</div>
    <div class="metric-grid">
      <div class="metric-card">
        <div class="val val-total">{{ total_tests }}</div>
        <div class="lbl">Total Tests</div>
      </div>
      <div class="metric-card">
        <div class="val val-pass">{{ total_pass }}</div>
        <div class="lbl">Passed</div>
      </div>
      <div class="metric-card">
        <div class="val val-fail">{{ total_fail }}</div>
        <div class="lbl">Failed</div>
      </div>
      <div class="metric-card">
        <div class="val val-rate">{{ pass_rate }}%</div>
        <div class="lbl">Pass Rate</div>
      </div>
      <div class="metric-card">
        <div class="val val-time">{{ elapsed_s }}s</div>
        <div class="lbl">Elapsed</div>
      </div>
      <div class="metric-card">
        <div class="val" style="font-size:1rem; color:#7f8c8d">{{ timestamp[:19].replace('T',' ') }}</div>
        <div class="lbl">Generated</div>
      </div>
    </div>
  </div>

  <!-- ── Test Suites ── -->
  <div id="suites">
    <div class="section-title">Test Suites</div>
    <table>
      <thead>
        <tr>
          <th>Suite</th>
          <th>Total</th>
          <th>Passed</th>
          <th>Failed</th>
          <th>Pass Rate</th>
          <th>Status</th>
        </tr>
      </thead>
      <tbody>
        {% for suite_name, suite in suites.items() %}
        <tr>
          <td><strong>{{ suite_name }}</strong></td>
          <td>{{ suite.total }}</td>
          <td style="color:#2ecc71">{{ suite.passed }}</td>
          <td style="color:{{ '#e74c3c' if suite.failed > 0 else '#7f8c8d' }}">{{ suite.failed }}</td>
          <td>{{ (100 * suite.passed // suite.total) if suite.total > 0 else 100 }}%</td>
          <td><span class="badge {{ 'badge-pass' if suite.status == 'pass' else 'badge-fail' }}">
            {{ suite.status.upper() }}
          </span></td>
        </tr>
        {% endfor %}
      </tbody>
    </table>
  </div>

  <!-- ── Pipeline Stages ── -->
  <div id="stages">
    <div class="section-title">Pipeline Stages</div>
    <div class="stage-row">
      {% for stage in stages %}
      <div class="stage-box {{ 'stage-pass' if stage.status == 'pass' else ('stage-fail' if stage.status == 'fail' else 'stage-skip') }}">
        {{ stage.id }} · {{ stage.name }}
        {% if stage.elapsed %}
          <span style="opacity:0.7; font-weight:400"> · {{ stage.elapsed }}</span>
        {% endif %}
      </div>
      {% endfor %}
    </div>
  </div>

  <!-- ── Figures ── -->
  <div id="figures">
    <div class="section-title">Generated Figures ({{ figures | length }})</div>
    <div class="gallery-controls">
      <button class="filter-btn active" onclick="filterFigs('all', this)">All</button>
      {% for group in fig_groups %}
      <button class="filter-btn" onclick="filterFigs('{{ group }}', this)">{{ group }}</button>
      {% endfor %}
    </div>
    <div class="figure-grid" id="fig-grid">
      {% for fig in figures %}
      <div class="fig-card" data-group="{{ fig.group }}" onclick="openLightbox('figures/{{ fig.filename }}', '{{ fig.name }}')">
        <img src="figures/{{ fig.filename }}" alt="{{ fig.name }}" loading="lazy">
        <div class="fig-label">{{ fig.name }}</div>
      </div>
      {% endfor %}
    </div>
  </div>

  <!-- ── Log ── -->
  <div id="log">
    <div class="section-title">Pipeline Log</div>
    <div class="log-box">{{ log_content }}</div>
  </div>

</main>

<!-- ── Lightbox ── -->
<div id="lightbox" onclick="closeLightbox()">
  <span id="lightbox-close" onclick="closeLightbox()">×</span>
  <img id="lightbox-img" src="" alt="">
  <div id="lightbox-label"></div>
</div>

<footer>
  VSEPR-SIM {{ version }} &nbsp;·&nbsp; Generated {{ timestamp[:19].replace('T', ' ') }} UTC
  &nbsp;·&nbsp; Atomistic simulation research platform
</footer>

<script>
  function filterFigs(group, btn) {
    document.querySelectorAll('.filter-btn').forEach(b => b.classList.remove('active'));
    btn.classList.add('active');
    document.querySelectorAll('.fig-card').forEach(card => {
      card.style.display = (group === 'all' || card.dataset.group === group) ? '' : 'none';
    });
  }

  function openLightbox(src, label) {
    document.getElementById('lightbox-img').src = src;
    document.getElementById('lightbox-label').textContent = label;
    document.getElementById('lightbox').classList.add('open');
  }

  function closeLightbox() {
    document.getElementById('lightbox').classList.remove('open');
  }

  document.addEventListener('keydown', e => { if (e.key === 'Escape') closeLightbox(); });

  // Auto-refresh badge if ?live=1
  if (new URLSearchParams(location.search).get('live')) {
    setInterval(() => {
      fetch('/api/status').then(r => r.json()).then(d => {
        const el = document.querySelector('.header-status');
        if (d.overall === 'pass') {
          el.className = 'header-status status-pass';
          el.textContent = '✔ ALL PASS';
        } else {
          el.className = 'header-status status-fail';
          el.textContent = '✘ FAILURES DETECTED';
        }
      }).catch(() => {});
    }, 5000);
  }
</script>
</body>
</html>
"""


def render_html(data: dict, figures: list[dict], log_content: str,
                reports_dir: Path):
    """Render the dashboard HTML using inline or Jinja2 template."""
    fig_groups = sorted(set(f["group"] for f in figures))
    suites = {
        "ctest  (C++ / CTest)":  data["ctest"],
        "pytest (pykernel)":     data["pytest"],
    }

    ctx = {
        "version":    data["version"],
        "overall":    data["overall"],
        "timestamp":  data["timestamp"],
        "total_tests": data["total_tests"],
        "total_pass":  data["total_pass"],
        "total_fail":  data["total_fail"],
        "pass_rate":   data["pass_rate"],
        "elapsed_s":   data["elapsed_s"],
        "suites":      suites,
        "stages":      data["stages"],
        "figures":     figures,
        "fig_groups":  fig_groups,
        "log_content": log_content,
    }

    if HAS_JINJA and TMPL_DIR.exists():
        env = Environment(
            loader=FileSystemLoader(str(TMPL_DIR)),
            autoescape=select_autoescape(["html"]),
        )
        try:
            tmpl = env.get_template("dashboard.html")
            html = tmpl.render(**ctx)
        except Exception:
            # Fall back to inline
            from jinja2 import Template
            tmpl = Template(HTML_TEMPLATE)
            html = tmpl.render(**ctx)
    else:
        from jinja2 import Template
        tmpl = Template(HTML_TEMPLATE)
        html = tmpl.render(**ctx)

    out_path = reports_dir / "index.html"
    out_path.write_text(html, encoding="utf-8")
    print(f"  [HTML] {out_path}")
    return out_path


# =============================================================================
# Main
# =============================================================================

def main():
    parser = argparse.ArgumentParser(
        description="VSEPR-SIM result post-processor")
    parser.add_argument("--results-dir", required=True)
    parser.add_argument("--reports-dir", required=True)
    parser.add_argument("--fig-dir",     required=True)
    parser.add_argument("--root",        default=".")
    args = parser.parse_args()

    results_dir = Path(args.results_dir)
    reports_dir = Path(args.reports_dir)
    fig_dir     = Path(args.fig_dir)
    root        = Path(args.root)

    reports_dir.mkdir(parents=True, exist_ok=True)

    # ── Load and merge results ──────────────────────────────────────────────
    data = load_results(results_dir)
    print(f"  [RESULTS] overall={data['overall']}  "
          f"pass={data['total_pass']}/{data['total_tests']}  "
          f"({data['pass_rate']}%)")

    # ── Write merged results.json ───────────────────────────────────────────
    results_out = reports_dir / "results.json"
    with open(results_out, "w") as f:
        json.dump(data, f, indent=2)
    print(f"  [JSON]  {results_out}")

    # ── Write test_log.csv ──────────────────────────────────────────────────
    write_test_csv(data, reports_dir / "test_log.csv")

    # ── Collect figures ─────────────────────────────────────────────────────
    figures = collect_figures(fig_dir)
    print(f"  [FIGS]  {len(figures)} figures found in {fig_dir}")

    # ── Read pipeline log ────────────────────────────────────────────────────
    log_path = root / "out" / "pipeline" / "pipeline.log"
    log_content = ""
    if log_path.exists():
        log_content = log_path.read_text(errors="replace")[-8000:]  # last 8 KB

    # ── Render HTML ─────────────────────────────────────────────────────────
    render_html(data, figures, log_content, reports_dir)

    # ── Print summary ────────────────────────────────────────────────────────
    status_sym = "✔" if data["overall"] == "pass" else "✘"
    print(f"\n  {status_sym}  {data['total_pass']}/{data['total_tests']} tests pass "
          f"({data['pass_rate']}%)  →  {reports_dir / 'index.html'}")


if __name__ == "__main__":
    main()
