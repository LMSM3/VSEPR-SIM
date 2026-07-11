"""
pipeline/continual.py -- Continual Reaction Generation Service
===============================================================

V4.0.4.01 beta -- webapp functionality branch

Runs on port 8800 (registered in ports.py).  Continuously generates
random chemical reactions at computation speed, streams them via
Server-Sent Events (SSE), and logs to history.

Architecture:
  - Async background task generates reactions in a tight loop
  - SSE endpoint /stream/events pushes each reaction to all connected clients
  - Browser viewer at /stream renders reactions in real-time
  - /api/continual/status reports generation rate and totals
  - /api/continual/control allows pause/resume/set-delay

Designed for future GPU acceleration: the generation loop is isolated
behind an async queue so the compute backend can be swapped.

Usage:
  python -m uvicorn pipeline.continual:app --host 127.0.0.1 --port 8800
"""

from __future__ import annotations

import asyncio
import json
import math
import random
import time
from collections import deque
from contextlib import asynccontextmanager
from dataclasses import dataclass, field
from datetime import datetime, timezone
from pathlib import Path
from typing import AsyncGenerator

from fastapi import FastAPI
from fastapi.responses import HTMLResponse, JSONResponse
from starlette.responses import StreamingResponse

# ── Paths ─────────────────────────────────────────────────────────────────────
ROOT_DIR = Path(__file__).resolve().parent.parent
STATE_DIR = ROOT_DIR / "chem" / "chem_shell" / "state"
STATE_DIR.mkdir(parents=True, exist_ok=True)
CONTINUAL_LOG = STATE_DIR / "continual_history.jsonl"

# ── Species pool for random generation ────────────────────────────────────────
# Drawn from real chemistry: elements, common molecules, ionic species

ELEMENTS = [
    "H2", "O2", "N2", "Cl2", "F2", "Br2", "I2", "S", "C", "P", "P4",
    "Na", "K", "Ca", "Mg", "Al", "Fe", "Cu", "Zn", "Ag", "Pb", "Sn",
    "Li", "Ba", "Mn", "Cr", "Co", "Ni", "Ti", "W", "Mo",
]

COMPOUNDS = [
    "H2O", "CO2", "CO", "NO", "NO2", "N2O", "SO2", "SO3",
    "NH3", "HCl", "HBr", "HF", "HI", "H2S", "HNO3", "H2SO4", "H3PO4",
    "NaOH", "KOH", "Ca(OH)2", "Mg(OH)2", "Al(OH)3", "Fe(OH)3",
    "NaCl", "KCl", "CaCl2", "MgCl2", "AlCl3", "FeCl3", "FeCl2",
    "ZnCl2", "CuCl2", "AgCl", "PbCl2", "BaCl2",
    "Na2SO4", "K2SO4", "CaSO4", "MgSO4", "FeSO4", "CuSO4", "ZnSO4",
    "Na2CO3", "K2CO3", "CaCO3", "MgCO3", "BaCO3",
    "NaNO3", "KNO3", "Ca(NO3)2", "AgNO3", "Pb(NO3)2",
    "Na2O", "K2O", "CaO", "MgO", "Al2O3", "Fe2O3", "Fe3O4",
    "CuO", "ZnO", "PbO", "TiO2", "SiO2", "MnO2", "Cr2O3",
    "CH4", "C2H6", "C3H8", "C2H4", "C2H2", "C6H6", "CH3OH", "C2H5OH",
    "HCOOH", "CH3COOH", "C6H12O6",
    "UO2", "UF6", "UF4", "ThF4", "PuF3", "LiF", "BeF2",
]

REACTION_TEMPLATES = [
    # combustion
    ("{fuel} + {n}O2 -> {n2}CO2 + {n3}H2O", "combustion"),
    # acid-base
    ("{acid} + {base} -> {salt} + H2O", "acid-base"),
    # decomposition
    ("{compound} -> {prod1} + {prod2}", "decomposition"),
    # synthesis
    ("{a} + {b} -> {product}", "synthesis"),
    # single replacement
    ("{metal} + {compound} -> {prod1} + {prod2}", "single-replacement"),
    # double replacement
    ("{compound1} + {compound2} -> {prod1} + {prod2}", "double-replacement"),
    # nuclear-relevant
    ("{salt1} + {salt2} -> {prod1} + {prod2}", "nuclear-salt"),
]

# Pre-built reaction pool (balanced, real chemistry)
REACTION_POOL = [
    ("2H2 + O2 -> 2H2O + 572 kJ", "combustion"),
    ("CH4 + 2O2 -> CO2 + 2H2O + 891 kJ", "combustion"),
    ("C2H6 + 3.5O2 -> 2CO2 + 3H2O + 1561 kJ", "combustion"),
    ("C3H8 + 5O2 -> 3CO2 + 4H2O + 2219 kJ", "combustion"),
    ("2C2H2 + 5O2 -> 4CO2 + 2H2O + 2600 kJ", "combustion"),
    ("C6H6 + 7.5O2 -> 6CO2 + 3H2O + 3268 kJ", "combustion"),
    ("2CH3OH + 3O2 -> 2CO2 + 4H2O + 1452 kJ", "combustion"),
    ("C2H5OH + 3O2 -> 2CO2 + 3H2O + 1367 kJ", "combustion"),
    ("2CO + O2 -> 2CO2 + 566 kJ", "combustion"),
    ("S + O2 -> SO2 + 297 kJ", "combustion"),
    ("2Mg + O2 -> 2MgO + 1204 kJ", "combustion"),
    ("4Fe + 3O2 -> 2Fe2O3 + 1648 kJ", "combustion"),
    ("4Al + 3O2 -> 2Al2O3 + 3352 kJ", "combustion"),
    ("2Na + Cl2 -> 2NaCl + 822 kJ", "synthesis"),
    ("N2 + 3H2 -> 2NH3 + 92 kJ", "synthesis"),
    ("H2 + Cl2 -> 2HCl + 185 kJ", "synthesis"),
    ("CaO + H2O -> Ca(OH)2 + 65 kJ", "synthesis"),
    ("SO3 + H2O -> H2SO4 + 130 kJ", "synthesis"),
    ("P2O5 + 3H2O -> 2H3PO4 + 177 kJ", "synthesis"),
    ("Na2O + H2O -> 2NaOH + 146 kJ", "synthesis"),
    ("2CaCO3 -> 2CaO + 2CO2 - 178 kJ", "decomposition"),
    ("2KClO3 -> 2KCl + 3O2", "decomposition"),
    ("2H2O2 -> 2H2O + O2", "decomposition"),
    ("2AgCl -> 2Ag + Cl2", "decomposition"),
    ("CuCO3 -> CuO + CO2", "decomposition"),
    ("2NaHCO3 -> Na2CO3 + H2O + CO2", "decomposition"),
    ("NH4NO3 -> N2O + 2H2O", "decomposition"),
    ("HCl + NaOH -> NaCl + H2O + 57 kJ", "acid-base"),
    ("H2SO4 + 2NaOH -> Na2SO4 + 2H2O + 115 kJ", "acid-base"),
    ("HNO3 + KOH -> KNO3 + H2O + 56 kJ", "acid-base"),
    ("2HCl + Ca(OH)2 -> CaCl2 + 2H2O", "acid-base"),
    ("3HCl + Al(OH)3 -> AlCl3 + 3H2O", "acid-base"),
    ("H2SO4 + 2KOH -> K2SO4 + 2H2O", "acid-base"),
    ("Zn + 2HCl -> ZnCl2 + H2", "single-replacement"),
    ("Fe + CuSO4 -> FeSO4 + Cu", "single-replacement"),
    ("Cu + 2AgNO3 -> Cu(NO3)2 + 2Ag", "single-replacement"),
    ("Mg + 2HCl -> MgCl2 + H2 + 462 kJ", "single-replacement"),
    ("Zn + CuSO4 -> ZnSO4 + Cu", "single-replacement"),
    ("2Al + 3CuCl2 -> 2AlCl3 + 3Cu", "single-replacement"),
    ("Fe + 2HCl -> FeCl2 + H2", "single-replacement"),
    ("NaCl + AgNO3 -> AgCl + NaNO3", "double-replacement"),
    ("BaCl2 + Na2SO4 -> BaSO4 + 2NaCl", "double-replacement"),
    ("Pb(NO3)2 + 2KI -> PbI2 + 2KNO3", "double-replacement"),
    ("CaCl2 + Na2CO3 -> CaCO3 + 2NaCl", "double-replacement"),
    ("3Mg + N2 -> Mg3N2", "synthesis"),
    ("2Fe + 3Cl2 -> 2FeCl3", "synthesis"),
    ("Ti + 2Cl2 -> TiCl4", "synthesis"),
    ("4Li + O2 -> 2Li2O", "synthesis"),
    ("UO2 + 4HF -> UF4 + 2H2O", "nuclear-salt"),
    ("UF4 + F2 -> UF6", "nuclear-salt"),
    ("ThF4 + 2Li -> Th + 2LiF", "nuclear-salt"),
    ("PuF3 + 3Na -> Pu + 3NaF", "nuclear-salt"),
    ("2LiF + BeF2 -> Li2BeF4", "nuclear-salt"),
    ("C + O2 -> CO2 + 394 kJ", "combustion"),
    ("4Na + O2 -> 2Na2O + 828 kJ", "combustion"),
    ("2Ca + O2 -> 2CaO + 1270 kJ", "combustion"),
]


# ── Reaction classification (reuse logic from controller) ─────────────────────

_ENERGY_RE_PATTERN = r'([+-]?\s*[\d.]+)\s*kJ'
import re
_ENERGY_RE = re.compile(_ENERGY_RE_PATTERN, re.IGNORECASE)


def _parse_energy(text: str):
    m = _ENERGY_RE.search(text)
    if m:
        val = float(m.group(1).replace(" ", ""))
        return val, ("exothermic" if val > 0 else "endothermic")
    return None, None


def _classify(text: str) -> str:
    t = text.lower()
    lhs = t.split("->")[0] if "->" in t else t
    rhs = t.split("->")[1] if "->" in t else ""
    if "o2" in lhs and ("co2" in rhs or "oxide" in rhs.replace(" ", "")):
        return "combustion"
    if "+" not in lhs.strip() and "+" in rhs:
        return "decomposition"
    if any(a in t for a in ("hcl", "h2so4", "hno3", "hf", "hbr")) and \
       any(b in t for b in ("naoh", "koh", "ca(oh)2", "mg(oh)2", "al(oh)3")):
        return "acid-base"
    return "general"


# ── Generation engine ─────────────────────────────────────────────────────────

@dataclass
class ReactorState:
    """Mutable state for the continual reactor."""
    running: bool = True
    delay_ms: float = 50.0          # ms between reactions (20/sec default)
    total_generated: int = 0
    total_streamed: int = 0
    start_time: float = 0.0
    last_reaction: str = ""
    last_class: str = ""
    last_energy: float | None = None
    reactions_per_sec: float = 0.0
    # ring buffer of recent reactions for the viewer
    recent: deque = field(default_factory=lambda: deque(maxlen=200))
    # SSE subscriber queues
    subscribers: list[asyncio.Queue] = field(default_factory=list)


_reactor = ReactorState()


def _generate_one() -> dict:
    """Pick a random reaction from the pool and build state."""
    rxn_text, rxn_class = random.choice(REACTION_POOL)
    energy, mode = _parse_energy(rxn_text)
    state = {
        "id": _reactor.total_generated,
        "timestamp": time.time(),
        "iso_time": datetime.now(timezone.utc).isoformat(),
        "reaction": rxn_text,
        "class": rxn_class,
        "energy_kj": energy,
        "mode": mode or "unknown",
    }
    return state


async def _generation_loop():
    """Core async loop: generate reactions and push to all subscribers."""
    _reactor.start_time = time.time()
    count_window = 0
    window_start = time.time()

    while True:
        if not _reactor.running:
            await asyncio.sleep(0.1)
            continue

        state = _generate_one()
        _reactor.total_generated += 1
        _reactor.last_reaction = state["reaction"]
        _reactor.last_class = state["class"]
        _reactor.last_energy = state["energy_kj"]
        _reactor.recent.append(state)

        # Push to SSE subscribers
        payload = json.dumps(state, default=str)
        dead = []
        for i, q in enumerate(_reactor.subscribers):
            try:
                q.put_nowait(payload)
            except asyncio.QueueFull:
                dead.append(i)
        for i in reversed(dead):
            _reactor.subscribers.pop(i)

        _reactor.total_streamed += len(_reactor.subscribers)

        # Rate tracking (1-second window)
        count_window += 1
        elapsed = time.time() - window_start
        if elapsed >= 1.0:
            _reactor.reactions_per_sec = count_window / elapsed
            count_window = 0
            window_start = time.time()

        # Log every 100th reaction
        if _reactor.total_generated % 100 == 0:
            try:
                with open(CONTINUAL_LOG, "a", encoding="utf-8") as f:
                    f.write(payload + "\n")
            except Exception:
                pass

        # Delay control
        if _reactor.delay_ms > 0:
            await asyncio.sleep(_reactor.delay_ms / 1000.0)
        else:
            await asyncio.sleep(0)  # yield to event loop


# ── App ───────────────────────────────────────────────────────────────────────

@asynccontextmanager
async def _lifespan(app):
    print("\n  VSEPR-SIM Continual Reactor  V4.0.4.01-beta")
    print("  Stream    : http://localhost:8800/stream")
    print("  SSE feed  : http://localhost:8800/stream/events")
    print("  API       : http://localhost:8800/api/continual/status")
    print("  Control   : http://localhost:8800/api/continual/control")
    print()
    task = asyncio.create_task(_generation_loop())
    yield
    task.cancel()
    try:
        await task
    except asyncio.CancelledError:
        pass


app = FastAPI(
    title="VSEPR-SIM Continual Reactor",
    description="Continuous chemical reaction generation and streaming service",
    version="4.0.4.01-beta",
    lifespan=_lifespan,
)


# ── SSE stream endpoint ──────────────────────────────────────────────────────

async def _sse_generator(q: asyncio.Queue) -> AsyncGenerator[str, None]:
    """Yield SSE-formatted events from a subscriber queue."""
    try:
        while True:
            payload = await q.get()
            yield f"data: {payload}\n\n"
    except asyncio.CancelledError:
        return


@app.get("/stream/events")
async def sse_stream():
    """Server-Sent Events stream of generated reactions."""
    q: asyncio.Queue = asyncio.Queue(maxsize=500)
    _reactor.subscribers.append(q)
    return StreamingResponse(
        _sse_generator(q),
        media_type="text/event-stream",
        headers={
            "Cache-Control": "no-cache",
            "Connection": "keep-alive",
            "X-Accel-Buffering": "no",
        },
    )


# ── API ───────────────────────────────────────────────────────────────────────

@app.get("/api/continual/status")
async def api_status():
    uptime = time.time() - _reactor.start_time if _reactor.start_time else 0
    return JSONResponse({
        "running": _reactor.running,
        "total_generated": _reactor.total_generated,
        "total_streamed": _reactor.total_streamed,
        "reactions_per_sec": round(_reactor.reactions_per_sec, 1),
        "delay_ms": _reactor.delay_ms,
        "uptime_sec": round(uptime, 1),
        "subscribers": len(_reactor.subscribers),
        "last_reaction": _reactor.last_reaction,
        "last_class": _reactor.last_class,
        "pool_size": len(REACTION_POOL),
        "version": "4.0.4.01-beta",
    })


@app.post("/api/continual/control")
async def api_control(action: str = "status", delay_ms: float = -1):
    """Control the reactor: pause, resume, set delay."""
    if action == "pause":
        _reactor.running = False
        return JSONResponse({"action": "paused", "running": False})
    if action == "resume":
        _reactor.running = True
        return JSONResponse({"action": "resumed", "running": True})
    if action == "set-delay" and delay_ms >= 0:
        _reactor.delay_ms = delay_ms
        return JSONResponse({"action": "delay-set", "delay_ms": delay_ms})
    if action == "turbo":
        _reactor.delay_ms = 0
        return JSONResponse({"action": "turbo", "delay_ms": 0, "note": "full computation speed"})
    return JSONResponse({
        "running": _reactor.running,
        "delay_ms": _reactor.delay_ms,
        "actions": ["pause", "resume", "set-delay", "turbo"],
    })


@app.post("/api/continual/inject")
async def api_inject(equation: str = "", source: str = "burn_engine"):
    """
    Live equation pass-in: external code pushes equation lines into the
    reactor stream.  Used by pykernel/burn_inorganic.py and any future
    engine that needs to broadcast computation steps.
    """
    if not equation:
        return JSONResponse({"error": "equation parameter required"}, status_code=400)

    state = {
        "id": _reactor.total_generated,
        "timestamp": time.time(),
        "iso_time": datetime.now(timezone.utc).isoformat(),
        "reaction": equation,
        "class": "injected",
        "energy_kj": None,
        "mode": "injected",
        "source": source,
    }
    _reactor.total_generated += 1
    _reactor.last_reaction = equation
    _reactor.last_class = "injected"
    _reactor.recent.append(state)

    payload = json.dumps(state, default=str)
    dead = []
    for i, q in enumerate(_reactor.subscribers):
        try:
            q.put_nowait(payload)
        except asyncio.QueueFull:
            dead.append(i)
    for i in reversed(dead):
        _reactor.subscribers.pop(i)
    _reactor.total_streamed += len(_reactor.subscribers)

    return JSONResponse({"ok": True, "id": state["id"], "equation": equation})


@app.get("/api/continual/recent")
async def api_recent(limit: int = 50):
    """Return recent reactions from the ring buffer."""
    items = list(_reactor.recent)[-limit:]
    return JSONResponse({"count": len(items), "reactions": items})


# ── Browser viewer ────────────────────────────────────────────────────────────

@app.get("/stream", response_class=HTMLResponse)
async def stream_viewer():
    return HTMLResponse(_VIEWER_HTML)


@app.get("/", response_class=HTMLResponse)
async def root():
    return HTMLResponse(_VIEWER_HTML)


_VIEWER_HTML = """\
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<title>VSEPR-SIM Continual Reactor</title>
<style>
  * { margin:0; padding:0; box-sizing:border-box; }
  body {
    background:#0a0e17; color:#c8d6e5; font-family:'Fira Code',monospace;
    display:flex; flex-direction:column; height:100vh; overflow:hidden;
  }
  header {
    background:#111827; padding:12px 24px; display:flex;
    align-items:center; justify-content:space-between; border-bottom:1px solid #1e293b;
  }
  header h1 { font-size:1.1rem; color:#60a5fa; letter-spacing:1px; }
  .stats { display:flex; gap:24px; font-size:0.85rem; color:#94a3b8; }
  .stats span { color:#60a5fa; font-weight:bold; }
  .controls { display:flex; gap:8px; }
  .controls button {
    background:#1e293b; color:#e2e8f0; border:1px solid #334155;
    padding:6px 14px; border-radius:4px; cursor:pointer; font-size:0.8rem;
  }
  .controls button:hover { background:#334155; }
  .controls button.active { background:#2563eb; border-color:#3b82f6; }
  #feed {
    flex:1; overflow-y:auto; padding:8px 16px;
    display:flex; flex-direction:column-reverse;
  }
  .rxn {
    padding:4px 0; border-bottom:1px solid #111827;
    display:flex; gap:16px; align-items:baseline; font-size:0.88rem;
    animation:fadeIn 0.15s ease-out;
  }
  @keyframes fadeIn { from{opacity:0;transform:translateY(-4px)} to{opacity:1;transform:none} }
  .rxn-id   { color:#475569; min-width:60px; text-align:right; font-size:0.75rem; }
  .rxn-text { color:#e2e8f0; flex:1; }
  .rxn-class {
    font-size:0.75rem; padding:2px 8px; border-radius:3px;
    text-transform:uppercase; letter-spacing:0.5px; min-width:100px; text-align:center;
  }
  .cls-combustion       { background:#7f1d1d; color:#fca5a5; }
  .cls-synthesis        { background:#14532d; color:#86efac; }
  .cls-decomposition    { background:#713f12; color:#fde68a; }
  .cls-acid-base        { background:#312e81; color:#a5b4fc; }
  .cls-single-replacement { background:#164e63; color:#67e8f9; }
  .cls-double-replacement { background:#4a1d96; color:#c4b5fd; }
  .cls-nuclear-salt     { background:#831843; color:#f9a8d4; }
  .cls-general          { background:#1e293b; color:#94a3b8; }
  .rxn-energy { color:#fbbf24; min-width:80px; text-align:right; font-size:0.8rem; }
  footer {
    background:#111827; padding:8px 24px; border-top:1px solid #1e293b;
    font-size:0.75rem; color:#475569; display:flex; justify-content:space-between;
  }
</style>
</head>
<body>
<header>
  <h1>&#9883; VSEPR-SIM CONTINUAL REACTOR</h1>
  <div class="stats">
    Generated: <span id="total">0</span> &nbsp;|&nbsp;
    Rate: <span id="rate">0</span>/sec &nbsp;|&nbsp;
    Uptime: <span id="uptime">0s</span>
  </div>
  <div class="controls">
    <button id="btnPause" onclick="control('pause')">&#9724; Pause</button>
    <button id="btnResume" onclick="control('resume')">&#9654; Resume</button>
    <button id="btnTurbo" onclick="control('turbo')">&#9889; Turbo</button>
    <button onclick="control('set-delay',200)">Slow</button>
    <button onclick="control('set-delay',50)">Normal</button>
  </div>
</header>
<div id="feed"></div>
<footer>
  <span>VSEPR-SIM 4.0.4.01-beta | Continual Generation Core</span>
  <span>SSE: /stream/events | API: /api/continual/status</span>
</footer>
<script>
const feed = document.getElementById('feed');
const MAX_ITEMS = 500;
let count = 0;

const evtSource = new EventSource('/stream/events');
evtSource.onmessage = (e) => {
  const d = JSON.parse(e.data);
  count++;
  const div = document.createElement('div');
  div.className = 'rxn';
  const cls = d.class.replace(/\\s+/g, '-');
  const energy = d.energy_kj ? d.energy_kj + ' kJ' : '';
  div.innerHTML =
    '<span class="rxn-id">#' + d.id + '</span>' +
    '<span class="rxn-text">' + escHtml(d.reaction) + '</span>' +
    '<span class="rxn-class cls-' + cls + '">' + d.class + '</span>' +
    '<span class="rxn-energy">' + energy + '</span>';
  feed.prepend(div);
  if (feed.children.length > MAX_ITEMS) feed.lastChild.remove();
  document.getElementById('total').textContent = d.id;
};

function escHtml(s) {
  return s.replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;');
}

function control(action, delay) {
  let url = '/api/continual/control?action=' + action;
  if (delay !== undefined) url += '&delay_ms=' + delay;
  fetch(url, {method:'POST'});
}

// Poll status for rate/uptime
setInterval(async () => {
  try {
    const r = await fetch('/api/continual/status');
    const d = await r.json();
    document.getElementById('rate').textContent = d.reactions_per_sec;
    const m = Math.floor(d.uptime_sec/60);
    const s = Math.floor(d.uptime_sec%60);
    document.getElementById('uptime').textContent = m + 'm ' + s + 's';
  } catch(e) {}
}, 1000);
</script>
</body>
</html>
"""
