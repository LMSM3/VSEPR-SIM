#!/usr/bin/env python3
"""
cfe_web.py -- Web Dashboard: All Daemon Layers Combined
Formation Engine v0.5.1

Lightweight, zero-dependency web dashboard that serves a browser-based
view combining all daemon layers:
  2.1  Common Name Finder
  2.2  Live Translation / Character-Cast
  2.3  Thermo Correctness Estimation
  2.4  High-N Audit & Unknown Detector

Uses only Python stdlib (http.server). No Flask, Django, or npm needed.

Usage:
  python scripts/cfe_web.py continual_results              # serve on :8051
  python scripts/cfe_web.py continual_results --port 9000   # custom port
  python scripts/cfe_web.py continual_results --open        # auto-open browser
"""

import sys, os, io, json, argparse, csv, re, math, time
import http.server
import urllib.parse
from pathlib import Path
from collections import Counter, defaultdict
from typing import List, Dict, Optional
from functools import partial

# Fix Windows encoding
if sys.stdout.encoding and sys.stdout.encoding.lower().replace('-', '') not in ('utf8', 'utf16'):
    sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')
    sys.stderr = io.TextIOWrapper(sys.stderr.buffer, encoding='utf-8', errors='replace')

_SCRIPT_DIR = Path(__file__).parent.resolve()
sys.path.insert(0, str(_SCRIPT_DIR))
from cfe_names import lookup, lookup_name, lookup_category, all_entries, database_size, search
from cfe_thermo import _read_ledger, analyse_thermo, _boltzmann_entropy, ThermoFlag
from cfe_audit import run_audit, audit_formula, AuditLevel, parse_formula, atom_count

# ── ANSI to console ──────────────────────────────────────────────────────────
_TTY = sys.stdout.isatty()
def _c(code, t): return f"\033[{code}m{t}\033[0m" if _TTY else t
GRN  = lambda t: _c("32", t)
CYN  = lambda t: _c("36", t)
YEL  = lambda t: _c("33", t)
RED  = lambda t: _c("31", t)
BOLD = lambda t: _c("1",  t)
DIM  = lambda t: _c("2",  t)

# ═══════════════════════════════════════════════════════════════════════════════
#  HTML TEMPLATES
# ═══════════════════════════════════════════════════════════════════════════════

_CSS = """
:root {
    --bg:      #0d1117;
    --bg2:     #161b22;
    --bg3:     #21262d;
    --fg:      #c9d1d9;
    --fg2:     #8b949e;
    --green:   #3fb950;
    --yellow:  #d29922;
    --orange:  #db6d28;
    --red:     #f85149;
    --cyan:    #58a6ff;
    --magenta: #bc8cff;
    --border:  #30363d;
    --mono:    'Cascadia Code', 'Fira Code', 'JetBrains Mono', 'Consolas', monospace;
}
* { box-sizing: border-box; margin: 0; padding: 0; }
body {
    background: var(--bg); color: var(--fg); font-family: var(--mono);
    font-size: 13px; line-height: 1.6; padding: 20px;
}
h1 { color: var(--cyan); font-size: 18px; margin-bottom: 4px; }
h2 { color: var(--fg2); font-size: 14px; margin: 20px 0 8px 0; border-bottom: 1px solid var(--border); padding-bottom: 4px; }
.subtitle { color: var(--fg2); font-size: 11px; margin-bottom: 16px; }
.grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(280px, 1fr)); gap: 12px; margin-bottom: 16px; }
.card {
    background: var(--bg2); border: 1px solid var(--border); border-radius: 6px;
    padding: 12px; }
.card-title { color: var(--fg2); font-size: 11px; text-transform: uppercase; margin-bottom: 6px; }
.card-value { font-size: 22px; font-weight: bold; }
.green  { color: var(--green); }
.yellow { color: var(--yellow); }
.orange { color: var(--orange); }
.red    { color: var(--red); }
.cyan   { color: var(--cyan); }
.magenta { color: var(--magenta); }
.dim    { color: var(--fg2); }
table { width: 100%; border-collapse: collapse; margin-bottom: 12px; }
th { text-align: left; color: var(--fg2); font-size: 11px; padding: 4px 8px;
     border-bottom: 1px solid var(--border); text-transform: uppercase; }
td { padding: 4px 8px; border-bottom: 1px solid var(--bg3); font-size: 12px; }
tr:hover td { background: var(--bg3); }
.bar-container { display: flex; height: 18px; border-radius: 3px; overflow: hidden; margin: 4px 0; }
.bar-ok   { background: var(--green); }
.bar-warn { background: var(--orange); }
.bar-err  { background: var(--red); }
.bar-named   { background: var(--green); }
.bar-unnamed { background: var(--yellow); }
.bar-exotic  { background: var(--orange); }
.bar-highn   { background: var(--red); }
input[type=text] {
    background: var(--bg3); border: 1px solid var(--border); color: var(--fg);
    padding: 6px 10px; border-radius: 4px; font-family: var(--mono);
    font-size: 13px; width: 300px; margin-bottom: 12px;
}
.tag {
    display: inline-block; padding: 1px 6px; border-radius: 3px;
    font-size: 10px; margin-right: 4px;
}
.tag-organic   { background: #d2992233; color: var(--yellow); }
.tag-inorganic { background: #58a6ff22; color: var(--cyan); }
.tag-mineral   { background: #bc8cff22; color: var(--magenta); }
.tag-gas       { background: #8b949e22; color: var(--fg2); }
.tag-acid      { background: #f8514922; color: var(--red); }
.tag-salt      { background: #3fb95022; color: var(--green); }
.tag-solvent   { background: #58a6ff22; color: var(--cyan); }
.tag-unknown   { background: #30363d; color: var(--fg2); }
.refresh-info { color: var(--fg2); font-size: 11px; margin-top: 20px; }
#search-results { margin-top: 8px; }
"""

def _tag(cat: str) -> str:
    return f'<span class="tag tag-{cat}">{cat}</span>'

def _cls_color(cls: str) -> str:
    m = {"stable": "green", "metastable": "yellow", "unstable": "red",
         "collapsed": "red", "timeout": "dim", "fragment": "dim"}
    return m.get(cls, "dim")

def _audit_color(level: str) -> str:
    return {"green": "green", "yellow": "yellow", "orange": "orange", "red": "red"}.get(level, "dim")


def _build_dashboard(ledger_path: Path) -> str:
    """Build the full HTML dashboard string."""
    rows = _read_ledger(ledger_path)
    thermo = analyse_thermo(rows) if rows else {}
    audit  = run_audit(rows) if rows else {"total_formulas": 0, "counts": {},
                                           "coverage_pct": 0, "results": [],
                                           "db_size": database_size()}

    total = len(rows)
    formulas_set = set(r.get('formula', '') for r in rows if r.get('formula'))

    # Count names
    named   = sum(1 for f in formulas_set if lookup_name(f))
    unnamed = len(formulas_set) - named

    # Classification counts
    cls_counts = Counter(r.get('classification', 'unknown') for r in rows)

    # Top stable formulas by best energy/atom
    stable_rows = [r for r in rows if r.get('classification') == 'stable']
    stable_best = {}
    for r in sorted(stable_rows, key=lambda x: x.get('energy_per_atom', 1e9)):
        f = r.get('formula', '')
        if f and f not in stable_best:
            stable_best[f] = r.get('energy_per_atom', 0)
    top_stable = list(stable_best.items())[:15]

    # Thermo summary
    n_ok   = thermo.get("n_ok", 0)
    n_warn = thermo.get("n_warn", 0)
    n_err  = thermo.get("n_err", 0)

    # Audit summary
    aud_cts = audit.get("counts", {})

    ts = time.strftime("%Y-%m-%d %H:%M:%S")

    html = f"""<!DOCTYPE html>
<html lang="en"><head><meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>CFE Dashboard — Formation Engine v0.5.1</title>
<style>{_CSS}</style>
</head><body>

<h1>⚗ Formation Engine — Dashboard</h1>
<div class="subtitle">v0.5.1 — Daemons 2.1 / 2.2 / 2.3 / 2.4  |  {ts}  |  Ledger: {ledger_path}</div>

<!-- ── Summary Cards ──────────────────────────────────────────────── -->
<div class="grid">
  <div class="card">
    <div class="card-title">Total Formations</div>
    <div class="card-value cyan">{total}</div>
  </div>
  <div class="card">
    <div class="card-title">Unique Formulas</div>
    <div class="card-value">{len(formulas_set)}</div>
  </div>
  <div class="card">
    <div class="card-title">Named / DB Size</div>
    <div class="card-value green">{named} <span class="dim">/ {database_size()}</span></div>
  </div>
  <div class="card">
    <div class="card-title">Stable</div>
    <div class="card-value green">{cls_counts.get('stable', 0)}</div>
  </div>
  <div class="card">
    <div class="card-title">Metastable</div>
    <div class="card-value yellow">{cls_counts.get('metastable', 0)}</div>
  </div>
  <div class="card">
    <div class="card-title">Collapsed / Timeout</div>
    <div class="card-value red">{cls_counts.get('collapsed', 0)} / {cls_counts.get('timeout', 0)}</div>
  </div>
</div>

<!-- ── Thermo Plausibility [2.3] ──────────────────────────────────── -->
<h2>Layer 2.3 — Thermo Plausibility</h2>
<div class="bar-container" style="width:100%;max-width:600px">
  <div class="bar-ok"   style="width:{n_ok/max(total,1)*100:.1f}%"></div>
  <div class="bar-warn" style="width:{n_warn/max(total,1)*100:.1f}%"></div>
  <div class="bar-err"  style="width:{n_err/max(total,1)*100:.1f}%"></div>
</div>
<div class="dim" style="margin-bottom:8px">
  <span class="green">{n_ok} ok</span> &nbsp;
  <span class="orange">{n_warn} warn</span> &nbsp;
  <span class="red">{n_err} err</span> &nbsp;
  E/atom: μ={thermo.get('epa_mean',0):+.2f} σ={thermo.get('epa_stddev',0):.2f}
</div>
"""

    # Thermo flagged rows (top 20 worst)
    rr_list = thermo.get("row_reports", [])
    flagged = [rr for rr in rr_list if rr["worst"] != "ok"]
    flagged_show = flagged[:20]
    if flagged_show:
        html += """<table><tr><th>Formula</th><th>Name</th><th>Class</th><th>E/atom</th><th>Flags</th></tr>\n"""
        for rr in flagged_show:
            f = rr["formula"]
            n = lookup_name(f)
            c = _cls_color(rr["classification"])
            epa = rr["energy_per_atom"]
            epa_s = f"{epa:+.4f}" if isinstance(epa, (int, float)) else "N/A"
            flags_s = ", ".join(f'{fl.code}' for fl in rr["flags"] if fl.level != "ok")
            lvl = "red" if rr["worst"] == "error" else "orange"
            html += f'<tr><td class="{lvl}">{f}</td><td>{n or "<span class=dim>--</span>"}</td>'
            html += f'<td class="{c}">{rr["classification"]}</td>'
            html += f'<td>{epa_s}</td><td class="{lvl}">{flags_s}</td></tr>\n'
        html += "</table>\n"

    # ── Audit [2.4] ────────────────────────────────────────────────────────
    html += """<h2>Layer 2.4 — Name Audit</h2>\n"""
    cov = audit.get("coverage_pct", 0)
    html += f"""<div class="bar-container" style="width:100%;max-width:600px">
  <div class="bar-named"   style="width:{aud_cts.get('green',0)/max(len(formulas_set),1)*100:.1f}%"></div>
  <div class="bar-unnamed" style="width:{aud_cts.get('yellow',0)/max(len(formulas_set),1)*100:.1f}%"></div>
  <div class="bar-exotic"  style="width:{aud_cts.get('orange',0)/max(len(formulas_set),1)*100:.1f}%"></div>
  <div class="bar-highn"   style="width:{aud_cts.get('red',0)/max(len(formulas_set),1)*100:.1f}%"></div>
</div>
<div class="dim" style="margin-bottom:8px">
  Coverage: {cov:.0f}% &nbsp;
  <span class="green">●{aud_cts.get('green',0)}</span> &nbsp;
  <span class="yellow">◐{aud_cts.get('yellow',0)}</span> &nbsp;
  <span class="orange">◑{aud_cts.get('orange',0)}</span> &nbsp;
  <span class="red">○{aud_cts.get('red',0)}</span>
</div>\n"""

    # RED audit rows
    red_results = [r for r in audit.get("results", []) if r.level == AuditLevel.RED]
    if red_results:
        html += '<table><tr><th>⚠ Formula</th><th>Atoms</th><th>Elements</th><th>Reasons</th></tr>\n'
        for ar in red_results[:15]:
            reasons = "; ".join(ar.reasons)
            html += (f'<tr><td class="red">{ar.formula}</td>'
                     f'<td>{ar.n_atoms}</td><td>{ar.n_elements}</td>'
                     f'<td class="red">{reasons}</td></tr>\n')
        html += '</table>\n'

    # ── Top Stable [2.2 Translation] ──────────────────────────────────────
    html += """<h2>Layer 2.2 — Top Stable (Translated)</h2>\n"""
    if top_stable:
        html += '<table><tr><th>Formula</th><th>Common Name</th><th>Category</th><th>E/atom</th></tr>\n'
        for f, epa in top_stable:
            n = lookup_name(f)
            cat = lookup_category(f)
            html += f'<tr><td class="green">{f}</td>'
            html += f'<td>{n or "<span class=dim>--</span>"}</td>'
            html += f'<td>{_tag(cat)}</td>'
            html += f'<td>{epa:+.4f}</td></tr>\n'
        html += '</table>\n'

    # ── Name Search [2.1] ─────────────────────────────────────────────────
    html += """<h2>Layer 2.1 — Common Name Finder</h2>
<input type="text" id="search-box" placeholder="Type a formula or name..." oninput="doSearch()">
<div id="search-results"></div>
<script>
async function doSearch() {
    const q = document.getElementById('search-box').value.trim();
    if (q.length < 1) { document.getElementById('search-results').innerHTML = ''; return; }
    const resp = await fetch('/api/search?q=' + encodeURIComponent(q));
    const data = await resp.json();
    let html = '<table><tr><th>Formula</th><th>Name</th><th>Category</th></tr>';
    for (const r of data) {
        html += '<tr><td>' + r.formula + '</td><td class="yellow">' + r.name + '</td>';
        html += '<td>' + r.category + '</td></tr>';
    }
    html += '</table>';
    document.getElementById('search-results').innerHTML = html;
}
</script>\n"""

    # ── Recent rows [2.2 live cast] ───────────────────────────────────────
    html += """<h2>Layer 2.2 — Recent Formations (Last 30)</h2>\n"""
    recent = rows[-30:] if len(rows) > 30 else rows
    if recent:
        html += '<table><tr><th>#</th><th>Formula</th><th>Name</th><th>Class</th><th>E/atom</th><th>Tier</th><th>ms</th></tr>\n'
        base = len(rows) - len(recent)
        for i, r in enumerate(recent):
            f   = r.get('formula', '')
            n   = lookup_name(f)
            cls = r.get('classification', '')
            epa = r.get('energy_per_atom', 0)
            epa_s = f"{epa:+.4f}" if isinstance(epa, (int, float)) else ""
            tier = r.get('tier', '')
            ms  = r.get('wall_ms', 0)
            ms_s = f"{ms:.1f}" if isinstance(ms, (int, float)) else ""
            cc  = _cls_color(cls)
            html += (f'<tr><td class="dim">{base+i+1}</td>'
                     f'<td>{f}</td>'
                     f'<td class="yellow">{n or "--"}</td>'
                     f'<td class="{cc}">{cls}</td>'
                     f'<td>{epa_s}</td><td>{tier}</td><td>{ms_s}</td></tr>\n')
        html += '</table>\n'

    html += f"""
<div class="refresh-info">Auto-refresh: <a href="javascript:location.reload()" style="color:var(--cyan)">Reload</a>
 | <a href="/api/thermo" style="color:var(--cyan)">JSON Thermo</a>
 | <a href="/api/audit" style="color:var(--cyan)">JSON Audit</a>
 | <a href="/api/names" style="color:var(--cyan)">JSON Names DB</a>
</div>
<script>setTimeout(()=>location.reload(), 15000);</script>
</body></html>"""
    return html


# ═══════════════════════════════════════════════════════════════════════════════
#  HTTP HANDLER
# ═══════════════════════════════════════════════════════════════════════════════

class CFEHandler(http.server.BaseHTTPRequestHandler):
    """Minimal request handler for the dashboard."""

    def __init__(self, ledger_path: Path, *args, **kwargs):
        self.ledger_path = ledger_path
        super().__init__(*args, **kwargs)

    def log_message(self, fmt, *args):
        # Quieter logging
        msg = fmt % args
        sys.stderr.write(DIM(f"  [{self.log_date_time_string()}] {msg}\n"))

    def _send(self, body: str, content_type: str = "text/html", status: int = 200):
        data = body.encode('utf-8')
        self.send_response(status)
        self.send_header("Content-Type", f"{content_type}; charset=utf-8")
        self.send_header("Content-Length", str(len(data)))
        self.send_header("Access-Control-Allow-Origin", "*")
        self.end_headers()
        self.wfile.write(data)

    def do_GET(self):
        parsed = urllib.parse.urlparse(self.path)
        path   = parsed.path
        qs     = urllib.parse.parse_qs(parsed.query)

        if path == "/" or path == "/index.html":
            html = _build_dashboard(self.ledger_path)
            self._send(html)

        elif path == "/api/search":
            q = qs.get("q", [""])[0]
            results = search(q) if q else []
            data = [{"formula": f, "name": n, "category": c} for f, n, c in results]
            self._send(json.dumps(data), "application/json")

        elif path == "/api/lookup":
            formula = qs.get("f", [""])[0]
            hit = lookup(formula)
            if hit:
                data = {"formula": formula, "name": hit[0], "category": hit[1]}
            else:
                data = {"formula": formula, "name": None, "category": "unknown"}
            self._send(json.dumps(data), "application/json")

        elif path == "/api/names":
            entries = all_entries()
            data = [{"formula": f, "name": n, "category": c} for f, n, c in entries]
            self._send(json.dumps(data), "application/json")

        elif path == "/api/thermo":
            rows = _read_ledger(self.ledger_path)
            info = analyse_thermo(rows)
            for rr in info.get("row_reports", []):
                rr["flags"] = [f.to_dict() for f in rr["flags"]]
            self._send(json.dumps(info, indent=2, default=str), "application/json")

        elif path == "/api/audit":
            rows = _read_ledger(self.ledger_path)
            info = run_audit(rows)
            out = dict(info)
            out["results"] = [r.to_dict() for r in info["results"]]
            self._send(json.dumps(out, indent=2), "application/json")

        else:
            self._send("<h1>404</h1>", status=404)


# ═══════════════════════════════════════════════════════════════════════════════
#  CLI
# ═══════════════════════════════════════════════════════════════════════════════

def _find_ledger(d: Path) -> Optional[Path]:
    if (d / 'ledger.csv').exists():
        return d / 'ledger.csv'
    if d.is_file():
        return d
    for sub in ('fast', 'medium', 'highn'):
        p = d / sub / 'ledger.csv'
        if p.exists():
            return p
    return None


def main():
    ap = argparse.ArgumentParser(
        prog="cfe_web",
        description="Formation Engine v0.5.1 — Web Dashboard (All Layers)")
    ap.add_argument("dir", nargs="?", default="continual_results",
                    help="Results directory containing ledger.csv")
    ap.add_argument("--port", type=int, default=8051,
                    help="HTTP port (default: 8051)")
    ap.add_argument("--open", action="store_true",
                    help="Auto-open browser")
    args = ap.parse_args()

    d = Path(args.dir)
    ledger = _find_ledger(d)
    if not ledger:
        print(RED(f"  No ledger.csv found in {d}"))
        print(YEL(f"  Hint: run the continual_runner first, or point to a directory with ledger.csv"))
        sys.exit(1)

    print()
    print(BOLD(CYN("  ┌─────────────────────────────────────────────────────────────┐")))
    print(BOLD(CYN("  │  CFE Web Dashboard — Formation Engine v0.5.1               │")))
    print(BOLD(CYN("  └─────────────────────────────────────────────────────────────┘")))
    print()
    print(f"  Ledger : {DIM(str(ledger))}")
    print(f"  URL    : {BOLD(CYN(f'http://localhost:{args.port}'))}")
    print(f"  APIs   : /api/search?q=  /api/lookup?f=  /api/names  /api/thermo  /api/audit")
    print()

    handler = partial(CFEHandler, ledger)
    server  = http.server.HTTPServer(("0.0.0.0", args.port), handler)

    if args.open:
        import webbrowser
        webbrowser.open(f"http://localhost:{args.port}")

    try:
        print(GRN(f"  Serving on port {args.port} — Ctrl+C to stop"))
        server.serve_forever()
    except KeyboardInterrupt:
        print(YEL("\n  Shutting down..."))
        server.shutdown()


if __name__ == "__main__":
    main()
