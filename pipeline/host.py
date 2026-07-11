"""
pipeline/host.py — VSEPR-SIM FastAPI result host.

Serves the post-processed dashboard and provides a JSON API for live polling.

Routes:
  GET /                     → redirect to /dashboard
  GET /dashboard            → full HTML dashboard (index.html)
  GET /api/status           → JSON: overall pass/fail, test counts
  GET /api/results          → JSON: full merged results record
  GET /api/figures          → JSON: figure catalogue
  GET /api/pipeline         → JSON: pipeline stage summary
  POST /api/rerun           → trigger a fresh pipeline run (non-blocking)
  GET /api/rerun/status     → status of the last triggered rerun
  GET /figures/{filename}   → serve a PNG figure
  GET /static/{filepath}    → serve CSS / JS from web/static/

VSEPR-SIM 3.0.0
"""

from __future__ import annotations

import asyncio
import json
import os
import subprocess
import sys
from datetime import datetime, timezone
from pathlib import Path
from typing import Any

from fastapi import FastAPI, HTTPException, BackgroundTasks
from fastapi.responses import (
    FileResponse, HTMLResponse, JSONResponse, RedirectResponse
)
from fastapi.staticfiles import StaticFiles
from starlette.middleware.base import BaseHTTPMiddleware
from starlette.requests import Request
from starlette.responses import Response

# ── Paths ────────────────────────────────────────────────────────────────────
ROOT_DIR     = Path(__file__).resolve().parent.parent
OUT_DIR      = ROOT_DIR / "out" / "pipeline"
RESULTS_DIR  = OUT_DIR / "results"
REPORTS_DIR  = OUT_DIR / "reports"
FIG_DIR      = REPORTS_DIR / "figures"
STATIC_DIR   = ROOT_DIR / "web" / "static"
PIPELINE_SH  = ROOT_DIR / "pipeline" / "run_pipeline.sh"
LOG_FILE     = OUT_DIR / "pipeline.log"

from contextlib import asynccontextmanager

@asynccontextmanager
async def _lifespan(app):
    print(f"\n  VSEPR-SIM Result Host  v4.0.4.01-beta")
    print(f"  Dashboard : http://localhost:8765/dashboard")
    print(f"  API docs  : http://localhost:8765/docs")
    print(f"  Reactor   : http://localhost:8800/stream")
    print(f"  Reports   : {REPORTS_DIR}")
    print(f"  Figures   : {len(_figure_list())} available")
    print(f"  Ports     : 8000-9000 reserved (webapp branch)\n")
    yield

app = FastAPI(
    title="VSEPR-SIM Result Host",
    description="Live test dashboard and result API for the VSEPR-SIM platform",
    version="4.0.4.01-beta",
    lifespan=_lifespan,
)

# ── Static files (CSS, JS) ───────────────────────────────────────────────────
if STATIC_DIR.exists():
    app.mount("/static", StaticFiles(directory=str(STATIC_DIR)), name="static")

# ── Rerun state ──────────────────────────────────────────────────────────────
_rerun_state: dict[str, Any] = {
    "running":   False,
    "last_start": None,
    "last_end":   None,
    "exit_code":  None,
    "stages":    "",
}


# =============================================================================
# Middleware — no-cache headers on API responses
# =============================================================================

class NoCacheMiddleware(BaseHTTPMiddleware):
    async def dispatch(self, request: Request, call_next):
        response = await call_next(request)
        if request.url.path.startswith("/api/"):
            response.headers["Cache-Control"] = "no-store"
        return response

app.add_middleware(NoCacheMiddleware)


# =============================================================================
# Helpers
# =============================================================================

def _load_json_safe(path: Path) -> dict:
    try:
        with open(path) as f:
            return json.load(f)
    except Exception:
        return {}


def _results() -> dict:
    return _load_json_safe(REPORTS_DIR / "results.json")


def _pipeline_summary() -> dict:
    return _load_json_safe(RESULTS_DIR / "pipeline_summary.json")


def _figure_list() -> list[dict]:
    if not FIG_DIR.exists():
        return []
    return [
        {"filename": p.name, "name": p.stem,
         "url": f"/figures/{p.name}",
         "size_kb": round(p.stat().st_size / 1024, 1)}
        for p in sorted(FIG_DIR.glob("*.png"))
    ]


# =============================================================================
# Routes — Dashboard
# =============================================================================

@app.get("/", include_in_schema=False)
async def root():
    return RedirectResponse(url="/dashboard")


@app.get("/dashboard", response_class=HTMLResponse)
async def dashboard():
    """Serve the generated dashboard HTML."""
    index = REPORTS_DIR / "index.html"
    if not index.exists():
        return HTMLResponse(
            content=_fallback_html(),
            status_code=200,
        )
    return HTMLResponse(content=index.read_text(encoding="utf-8"))


@app.get("/figures/{filename}")
async def serve_figure(filename: str):
    """Serve a PNG figure from the reports/figures directory."""
    # Sanitise – no path traversal
    filename = Path(filename).name
    path = FIG_DIR / filename
    if not path.exists() or path.suffix.lower() != ".png":
        raise HTTPException(status_code=404, detail=f"Figure not found: {filename}")
    return FileResponse(str(path), media_type="image/png")


# =============================================================================
# Routes — JSON API
# =============================================================================

@app.get("/api/status")
async def api_status():
    """Lightweight status poll — overall pass/fail + test counts."""
    data = _results()
    if not data:
        return JSONResponse({
            "overall": "unknown",
            "total_tests": 0,
            "total_pass": 0,
            "total_fail": 0,
            "pass_rate": 0,
            "rerun_running": _rerun_state["running"],
        })
    return JSONResponse({
        "overall":      data.get("overall", "unknown"),
        "total_tests":  data.get("total_tests", 0),
        "total_pass":   data.get("total_pass", 0),
        "total_fail":   data.get("total_fail", 0),
        "pass_rate":    data.get("pass_rate", 0),
        "timestamp":    data.get("timestamp", ""),
        "rerun_running": _rerun_state["running"],
    })


@app.get("/api/results")
async def api_results():
    """Full merged result record."""
    data = _results()
    if not data:
        raise HTTPException(status_code=404, detail="No results available yet")
    return JSONResponse(data)


@app.get("/api/figures")
async def api_figures():
    """Figure catalogue with download URLs."""
    return JSONResponse({
        "count":   len(_figure_list()),
        "figures": _figure_list(),
    })


@app.get("/api/pipeline")
async def api_pipeline():
    """Pipeline stage summary."""
    data = _pipeline_summary()
    if not data:
        raise HTTPException(status_code=404, detail="No pipeline summary available")
    return JSONResponse(data)


@app.get("/api/log")
async def api_log(lines: int = 200):
    """Last N lines of the pipeline log."""
    if not LOG_FILE.exists():
        return JSONResponse({"lines": [], "path": str(LOG_FILE)})
    text = LOG_FILE.read_text(errors="replace")
    all_lines = text.splitlines()
    tail = all_lines[-lines:] if len(all_lines) > lines else all_lines
    return JSONResponse({"lines": tail, "total_lines": len(all_lines)})


@app.get("/api/csv")
async def api_csv():
    """Serve the test_log.csv as downloadable file."""
    path = REPORTS_DIR / "test_log.csv"
    if not path.exists():
        raise HTTPException(status_code=404, detail="test_log.csv not found")
    return FileResponse(str(path), media_type="text/csv",
                        filename="vsepr_test_log.csv")


# =============================================================================
# Routes — Rerun
# =============================================================================

def _run_pipeline_background(stages: str):
    """Run the shell pipeline in a subprocess. Called in a background task."""
    _rerun_state["running"]    = True
    _rerun_state["last_start"] = datetime.now(timezone.utc).isoformat()
    _rerun_state["exit_code"]  = None
    _rerun_state["stages"]     = stages

    shell = "bash"
    cmd = [shell, str(PIPELINE_SH), f"--stage={stages}"]

    try:
        result = subprocess.run(
            cmd, cwd=str(ROOT_DIR),
            capture_output=False,          # logs go to pipeline.log via tee
            timeout=900,                   # 15 min hard limit
        )
        _rerun_state["exit_code"] = result.returncode
    except subprocess.TimeoutExpired:
        _rerun_state["exit_code"] = -1
    except Exception as e:
        _rerun_state["exit_code"] = -2
    finally:
        _rerun_state["running"]  = False
        _rerun_state["last_end"] = datetime.now(timezone.utc).isoformat()


@app.post("/api/rerun")
async def api_rerun(background_tasks: BackgroundTasks,
                    stages: str = "2,3,4,5"):
    """
    Trigger a pipeline rerun.

    Query params:
      stages  — comma-separated stage numbers (default: 2,3,4,5  = skip build+host)

    Returns immediately; poll /api/rerun/status for progress.
    """
    if _rerun_state["running"]:
        return JSONResponse(
            {"started": False, "reason": "A rerun is already in progress"},
            status_code=409,
        )

    background_tasks.add_task(_run_pipeline_background, stages)
    return JSONResponse({
        "started":   True,
        "stages":    stages,
        "timestamp": datetime.now(timezone.utc).isoformat(),
    })


@app.get("/api/rerun/status")
async def api_rerun_status():
    """Current rerun status."""
    return JSONResponse({
        "running":    _rerun_state["running"],
        "last_start": _rerun_state["last_start"],
        "last_end":   _rerun_state["last_end"],
        "exit_code":  _rerun_state["exit_code"],
        "stages":     _rerun_state["stages"],
    })


# =============================================================================
# Routes — Chemistry Shell (chem_bridge)
# =============================================================================

try:
    from chem.chem_shell.chem_bridge import (
        handle_command as _chem_handle,
        current_state as _chem_state,
        history_entries as _chem_history,
        validate as _chem_validate,
        HELP_SHORT as _CHEM_HELP,
    )
    _CHEM_AVAILABLE = True
except Exception:
    _CHEM_AVAILABLE = False


@app.get("/api/chem/status")
async def api_chem_status():
    """Check whether the chem subsystem is loaded."""
    if not _CHEM_AVAILABLE:
        return JSONResponse({"available": False}, status_code=503)
    return JSONResponse({"available": True, **_chem_validate()})


@app.get("/api/chem/state")
async def api_chem_state():
    """Current reaction state (polled by the web viewer)."""
    if not _CHEM_AVAILABLE:
        raise HTTPException(503, "chem subsystem unavailable")
    return JSONResponse(_chem_state())


@app.get("/api/chem/history")
async def api_chem_history(limit: int = 50):
    """Last N reaction history entries."""
    if not _CHEM_AVAILABLE:
        raise HTTPException(503, "chem subsystem unavailable")
    return JSONResponse({"entries": _chem_history(limit)})


@app.post("/api/chem/command")
async def api_chem_command(cmd: str = "help"):
    """
    Dispatch a command to the chem controller.

    Query params:
      cmd -- the command string (reaction, help, 998, 999, etc.)

    Returns the controller response text and updated state.
    """
    if not _CHEM_AVAILABLE:
        raise HTTPException(503, "chem subsystem unavailable")
    response_text = _chem_handle(cmd)
    return JSONResponse({
        "command":  cmd,
        "response": response_text,
        "state":    _chem_state(),
    })


@app.get("/api/chem/help")
async def api_chem_help():
    """Return the chem shell help text."""
    if not _CHEM_AVAILABLE:
        raise HTTPException(503, "chem subsystem unavailable")
    return JSONResponse({"help": _CHEM_HELP})


# =============================================================================
# Routes — Plant Alpha2 (Nuclear & Traditional Power Plant Modelling)
# =============================================================================

try:
    from chem.chem_shell.plant_bridge import (
        validate as _plant_validate,
        list_materials as _plant_list,
        material_summary as _plant_summary,
        generate_report_data as _plant_report,
        fit_from_data as _plant_fit,
    )
    _PLANT_AVAILABLE = True
except Exception:
    _PLANT_AVAILABLE = False


@app.get("/api/plant/status")
async def api_plant_status():
    """Check whether the plant modelling subsystem is loaded."""
    if not _PLANT_AVAILABLE:
        return JSONResponse({"available": False}, status_code=503)
    return JSONResponse({"available": True, **_plant_validate()})


@app.get("/api/plant/materials")
async def api_plant_materials(category: str = ""):
    """List available materials, optionally filtered by category."""
    if not _PLANT_AVAILABLE:
        raise HTTPException(503, "plant subsystem unavailable")
    return JSONResponse({"materials": _plant_list(category or None)})


@app.get("/api/plant/material/{formula}")
async def api_plant_material(formula: str, T: float = 1000.0):
    """Get property summary for a material at temperature T."""
    if not _PLANT_AVAILABLE:
        raise HTTPException(503, "plant subsystem unavailable")
    return JSONResponse(_plant_summary(formula, T))


@app.get("/api/plant/report")
async def api_plant_report():
    """Generate selective output table for automatic reports."""
    if not _PLANT_AVAILABLE:
        raise HTTPException(503, "plant subsystem unavailable")
    return JSONResponse({"rows": _plant_report()})


@app.post("/api/plant/fit")
async def api_plant_fit(T_data: list[float], y_data: list[float],
                        degree: int = 3, form: str = "poly"):
    """Run curve fitting on provided data."""
    if not _PLANT_AVAILABLE:
        raise HTTPException(503, "plant subsystem unavailable")
    return JSONResponse(_plant_fit(T_data, y_data, degree, form))


# =============================================================================
# Routes -- Continual Reactor proxy/status
# =============================================================================

@app.get("/api/continual")
async def api_continual_proxy():
    """Check if the continual reactor is reachable and return its status."""
    import httpx
    try:
        async with httpx.AsyncClient(timeout=2.0) as client:
            r = await client.get("http://127.0.0.1:8800/api/continual/status")
            return JSONResponse({"available": True, **r.json()})
    except Exception:
        return JSONResponse({"available": False, "note": "Start with: python -m uvicorn pipeline.continual:app --port 8800"})


@app.get("/api/ports")
async def api_ports():
    """Return the port registry."""
    try:
        from pipeline.ports import PORTS, summary
        return JSONResponse({
            "ports": {str(p): {"service": e.service, "module": e.module, "description": e.description}
                      for p, e in PORTS.items()},
            "summary": summary(),
        })
    except Exception as exc:
        return JSONResponse({"error": str(exc)})


# =============================================================================
# Fallback HTML (before first pipeline run)
# =============================================================================

def _fallback_html() -> str:
    return """<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<title>VSEPR-SIM Host</title>
<style>
  body { background:#0f1117; color:#e0e0e0; font-family:system-ui,sans-serif;
         display:flex; align-items:center; justify-content:center;
         min-height:100vh; flex-direction:column; gap:1rem; }
  h1 { color:#2e86c1; font-size:2rem; }
  p  { color:#7f8c8d; }
  code { background:#161b27; padding:0.3rem 0.7rem; border-radius:4px;
         color:#f39c12; font-family:monospace; }
  .btn {
    margin-top:1rem;
    background:#2e86c1; color:white; border:none; padding:0.6rem 1.5rem;
    border-radius:6px; font-size:1rem; cursor:pointer;
  }
</style>
</head>
<body>
  <h1>VSEPR-SIM Result Host</h1>
  <p>No results found yet. Run the pipeline first:</p>
  <code>bash pipeline/run_pipeline.sh --stage=2,3,4,5</code>
  <p>Or trigger a rerun via the API:</p>
  <code>POST /api/rerun</code>
  <button class="btn" onclick="triggerRerun()">Trigger Rerun (stages 2–5)</button>
  <p id="status" style="color:#f39c12"></p>
  <script>
    function triggerRerun() {
      fetch('/api/rerun', {method:'POST'})
        .then(r => r.json())
        .then(d => {
          document.getElementById('status').textContent =
            d.started ? 'Rerun started! Refresh in ~30 seconds.' : d.reason;
        });
    }
  </script>
</body>
</html>"""


# =============================================================================
# Startup / Shutdown events
# =============================================================================

# (startup handled by lifespan context above)
