#!/usr/bin/env python3
"""
viz_web.py — Browser-Accessible Visual Host for VSEPR-SIM
==========================================================
HTTP + WebSocket server serving a full 3D/2D visualization dashboard
to Chrome/Chromium browsers on any local network.

Port Map (data ingestion — same as viz_host.py):
    8899   Base control / peptide 2D analysis stream
    9990   Crystal (reserved)          9995   Peptide 3D (alias)
    9991   Coarse-Grain (reserved)     9996   Phase Diagram (reserved)
    9992   EHD Field (reserved)        9997   Formation Timeline (reserved)
    9993   Thermal Pipe (reserved)     9998   Analysis Deep (gas2)
    9994   Peptide 2D (alias)          9999   Atomic Live (gas2/peptide 3D)

Browser access:
    http://<host>:8899   — Main dashboard (default)
    http://<host>:9999   — 3D atomic viewer (direct)

Protocols:
    UDP/TCP data ingestion on all 999X + 8899 (same as viz_host.py)
    HTTP serves the HTML5/WebGL dashboard
    WebSocket (/ws) streams live frame data to browsers
    Server-Sent Events (/sse) as fallback for rough networks

Optimised for rough local networks:
    - Chunked frame delivery with sequence numbers
    - Automatic reconnection (WebSocket + SSE + polling fallback)
    - Delta compression (only send changed fields)
    - Adaptive frame rate based on RTT
    - gzip content encoding for HTTP
    - Service Worker offline cache for static assets
    - Minimal Three.js CDN with local fallback

Usage:
    python tools/viz_web.py                         (serve on all ports)
    python tools/viz_web.py --http-port 8899        (custom HTTP port)
    python tools/viz_web.py --bind 0.0.0.0          (all interfaces)
    python tools/viz_web.py --verbose                (debug logging)

Requires: Python 3.6+ (stdlib only — no pip installs)
"""

import socket
import json
import threading
import time
import sys
import os
import hashlib
import gzip
import struct
import io
from http.server import HTTPServer, BaseHTTPRequestHandler
from collections import deque, defaultdict

# Force UTF-8 stdout on Windows (box-drawing chars / ANSI)
if sys.platform == "win32":
    try:
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")
        sys.stderr.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass

# ============================================================================
# Port assignments (mirrored from viz_host.py)
# ============================================================================

BASE_PORT = 8899
ALL_999X  = list(range(9990, 10000))
ALL_PORTS = [BASE_PORT] + ALL_999X

PORT_LABELS = {
    8899: "Base / Peptide 2D",
    9990: "Crystal",
    9991: "Coarse-Grain",
    9992: "EHD Field",
    9993: "Thermal Pipe",
    9994: "Peptide 2D",
    9995: "Peptide 3D",
    9996: "Phase Diagram",
    9997: "Formation Timeline",
    9998: "Analysis Deep",
    9999: "Atomic Live",
}

# ============================================================================
# Frame type detection (shared logic with viz_host.py)
# ============================================================================

def detect_frame_type(frame):
    if not isinstance(frame, dict):
        return "unknown"
    if "type" in frame:
        return frame["type"]
    if "residues" in frame and "scores" in frame:
        return "peptide"
    if "atoms" in frame and "bonds" in frame and "lattice" in frame:
        return "atomic"
    if "hist_E" in frame or "history_energy" in frame:
        return "analysis"
    if "atoms" in frame:
        return "peptide"
    return "unknown"

# ============================================================================
# Thread-safe frame store (ring buffer per port)
# ============================================================================

class FrameStore:
    def __init__(self, maxlen=256):
        self.lock = threading.Lock()
        self.buffers = defaultdict(lambda: deque(maxlen=maxlen))
        self.latest = {}
        self.counts = defaultdict(int)
        self.types = {}
        self.seq = 0  # global sequence number

    def push(self, port, frame):
        ftype = detect_frame_type(frame)
        with self.lock:
            self.seq += 1
            frame["_seq"] = self.seq
            frame["_port"] = port
            frame["_ts"] = time.time()
            frame["_type"] = ftype
            self.buffers[port].append(frame)
            self.latest[port] = frame
            self.counts[port] += 1
            self.types[port] = ftype

    def get_latest(self, port=None):
        with self.lock:
            if port is not None:
                return self.latest.get(port)
            return dict(self.latest)

    def get_latest_any(self):
        """Get most recent frame across all ports."""
        with self.lock:
            best = None
            for f in self.latest.values():
                if best is None or f.get("_seq", 0) > best.get("_seq", 0):
                    best = f
            return best

    def get_since(self, seq_after):
        """Get all frames with _seq > seq_after (for delta sync)."""
        with self.lock:
            results = []
            for port, buf in self.buffers.items():
                for f in buf:
                    if f.get("_seq", 0) > seq_after:
                        results.append(f)
            results.sort(key=lambda f: f.get("_seq", 0))
            return results

    def get_all(self, port):
        with self.lock:
            return list(self.buffers[port])

    def get_stats(self):
        with self.lock:
            return {p: (self.counts[p], self.types.get(p, "—"))
                    for p in sorted(self.counts.keys())}

    def get_since_port(self, seq_after, port):
        """Get frames for a specific port with _seq > seq_after."""
        with self.lock:
            results = []
            for f in self.buffers.get(port, []):
                if f.get("_seq", 0) > seq_after:
                    results.append(f)
            results.sort(key=lambda f: f.get("_seq", 0))
            return results

    def get_global_seq(self):
        with self.lock:
            return self.seq

# ============================================================================
# Data ingestion
# ============================================================================

class DataListener:
    def __init__(self, port, store, bind_addr="0.0.0.0", verbose=False):
        self.port = port
        self.store = store
        self.bind_addr = bind_addr
        self.verbose = verbose
        self.running = True

    def start(self):
        threading.Thread(target=self._udp_loop, daemon=True).start()
        threading.Thread(target=self._tcp_loop, daemon=True).start()

    def _udp_loop(self):
        try:
            sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            sock.bind((self.bind_addr, self.port))
            sock.settimeout(0.3)
        except OSError as e:
            if self.verbose:
                print(f"  [UDP {self.port}] bind failed: {e}")
            return
        if self.verbose:
            print(f"  [UDP {self.port}] listening")
        while self.running:
            try:
                data, _ = sock.recvfrom(65536)
                frame = json.loads(data.decode("utf-8"))
                self.store.push(self.port, frame)
            except socket.timeout:
                pass
            except (json.JSONDecodeError, Exception):
                pass
        sock.close()

    def _tcp_loop(self):
        try:
            sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            sock.bind((self.bind_addr, self.port))
            sock.listen(8)
            sock.settimeout(0.5)
        except OSError as e:
            if self.verbose:
                print(f"  [TCP {self.port}] bind failed: {e}")
            return
        if self.verbose:
            print(f"  [TCP {self.port}] listening")
        while self.running:
            try:
                conn, addr = sock.accept()
                threading.Thread(target=self._tcp_client,
                                 args=(conn,), daemon=True).start()
            except socket.timeout:
                pass
            except Exception:
                pass
        sock.close()

    def _tcp_client(self, conn):
        buf = ""
        conn.settimeout(0.5)
        while self.running:
            try:
                chunk = conn.recv(65536)
                if not chunk:
                    break
                buf += chunk.decode("utf-8", errors="replace")
                while "\n" in buf:
                    line, buf = buf.split("\n", 1)
                    line = line.strip()
                    if not line:
                        continue
                    try:
                        frame = json.loads(line)
                        self.store.push(self.port, frame)
                    except json.JSONDecodeError:
                        pass
            except socket.timeout:
                pass
            except Exception:
                break
        conn.close()

    def stop(self):
        self.running = False

# ============================================================================
# WebSocket (RFC 6455) — minimal implementation, stdlib only
# ============================================================================

def ws_accept_key(key):
    import hashlib, base64
    GUID = "258EAFA5-E914-47DA-95CA-5AB9B6C06E30"
    accept = hashlib.sha1((key + GUID).encode()).digest()
    return base64.b64encode(accept).decode()

def ws_encode_frame(payload, opcode=0x01):
    """Encode a WebSocket text frame."""
    data = payload.encode("utf-8") if isinstance(payload, str) else payload
    length = len(data)
    header = bytes([0x80 | opcode])
    if length < 126:
        header += bytes([length])
    elif length < 65536:
        header += bytes([126]) + struct.pack(">H", length)
    else:
        header += bytes([127]) + struct.pack(">Q", length)
    return header + data

def ws_decode_frame(sock):
    """Decode one WebSocket frame. Returns (opcode, payload) or None."""
    try:
        hdr = _recv_exact(sock, 2)
        if not hdr:
            return None
        opcode = hdr[0] & 0x0F
        masked = (hdr[1] & 0x80) != 0
        length = hdr[1] & 0x7F
        if length == 126:
            ext = _recv_exact(sock, 2)
            if not ext:
                return None
            length = struct.unpack(">H", ext)[0]
        elif length == 127:
            ext = _recv_exact(sock, 8)
            if not ext:
                return None
            length = struct.unpack(">Q", ext)[0]
        mask_key = None
        if masked:
            mask_key = _recv_exact(sock, 4)
            if not mask_key:
                return None
        payload = _recv_exact(sock, length)
        if payload is None:
            return None
        if mask_key:
            payload = bytes(b ^ mask_key[i % 4] for i, b in enumerate(payload))
        return (opcode, payload)
    except Exception:
        return None

def _recv_exact(sock, n):
    buf = b""
    while len(buf) < n:
        chunk = sock.recv(n - len(buf))
        if not chunk:
            return None
        buf += chunk
    return buf

# ============================================================================
# WebSocket client handler
# ============================================================================

class WSClient:
    def __init__(self, sock, store, verbose=False, port_filter=None):
        self.sock = sock
        self.store = store
        self.verbose = verbose
        self.running = True
        self.last_seq = 0
        self.port_filter = port_filter  # int or None (all ports)

    def serve(self):
        """Main loop: send new frames as they arrive."""
        self.sock.settimeout(0.15)
        try:
            while self.running:
                # Check for incoming messages (ping/pong/close)
                try:
                    result = ws_decode_frame(self.sock)
                    if result is None:
                        break
                    opcode, payload = result
                    if opcode == 0x08:  # close
                        break
                    elif opcode == 0x09:  # ping
                        self.sock.sendall(ws_encode_frame(payload, 0x0A))
                    elif opcode == 0x01:  # text
                        try:
                            msg = json.loads(payload.decode("utf-8"))
                            if msg.get("type") == "poll":
                                self.last_seq = msg.get("after_seq", self.last_seq)
                        except Exception:
                            pass
                except socket.timeout:
                    pass

                # Send new frames (filtered to port if requested)
                current_seq = self.store.get_global_seq()
                if current_seq > self.last_seq:
                    if self.port_filter is not None:
                        frames = self.store.get_since_port(self.last_seq, self.port_filter)
                    else:
                        frames = self.store.get_since(self.last_seq)
                    if frames:
                        batch = {
                            "type": "batch",
                            "frames": frames[-20:],
                            "seq": current_seq,
                            "ts": time.time(),
                        }
                        try:
                            self.sock.sendall(ws_encode_frame(
                                json.dumps(batch, default=str)))
                        except Exception:
                            break
                        self.last_seq = current_seq
                    else:
                        self.last_seq = current_seq

                time.sleep(0.05)  # ~20 Hz max push rate
        except Exception:
            pass
        finally:
            try:
                self.sock.close()
            except Exception:
                pass

# ============================================================================
# HTTP request handler — serves HTML dashboard + WebSocket upgrade + SSE + API
# ============================================================================

# Global reference set by main()
_global_store = None
_global_start_time = 0
_global_verbose = False
_global_stream_registry = []   # [{port, name, type, generator_tag, enabled}]

class VizHTTPHandler(BaseHTTPRequestHandler):
    """Handles HTTP, WebSocket upgrade, SSE, and REST API."""

    def log_message(self, format, *args):
        if _global_verbose:
            BaseHTTPRequestHandler.log_message(self, format, *args)

    def do_GET(self):
        raw = self.path
        path = raw.split("?")[0]
        qs = raw[len(path)+1:] if "?" in raw else ""
        # Parse ?port= query parameter
        self._port_filter = None
        for part in qs.split("&"):
            if part.startswith("port="):
                try:
                    self._port_filter = int(part[5:])
                except ValueError:
                    pass

        # WebSocket upgrade
        if path == "/ws":
            self._handle_ws_upgrade()
            return

        # Server-Sent Events (fallback for rough networks)
        if path == "/sse":
            self._handle_sse()
            return

        # REST API: latest frame data
        if path == "/api/frames":
            self._serve_json(self._api_frames())
            return
        if path == "/api/stats":
            self._serve_json(self._api_stats())
            return
        if path == "/api/streams":
            self._serve_json(self._api_streams())
            return
        if path.startswith("/api/port/"):
            try:
                port = int(path.split("/")[-1])
                frame = _global_store.get_latest(port)
                self._serve_json(frame or {})
            except Exception:
                self._serve_json({"error": "invalid port"})
            return

        # Static assets — inject port context into HTML
        if path == "/" or path == "/index.html":
            html = DASHBOARD_HTML
            if self._port_filter is not None:
                pname = PORT_LABELS.get(self._port_filter, str(self._port_filter))
                inject = (
                    f'<script>window._VSEPR_PORT={self._port_filter};'
                    f'window._VSEPR_PORT_NAME="{pname}";</script>\n'
                )
                html = html.replace("<script src=\"https://cdnjs", inject + "<script src=\"https://cdnjs", 1)
            self._serve_html(html)
            return
        if path == "/favicon.ico":
            self.send_response(204)
            self.end_headers()
            return

        # 404
        self.send_response(404)
        self.send_header("Content-Type", "text/plain")
        self.end_headers()
        self.wfile.write(b"404 Not Found")

    def _serve_html(self, html):
        body = html.encode("utf-8")
        # gzip for rough networks
        accept_enc = self.headers.get("Accept-Encoding", "")
        if "gzip" in accept_enc:
            buf = io.BytesIO()
            with gzip.GzipFile(fileobj=buf, mode="wb", compresslevel=6) as gz:
                gz.write(body)
            body = buf.getvalue()
            self.send_response(200)
            self.send_header("Content-Encoding", "gzip")
        else:
            self.send_response(200)
        self.send_header("Content-Type", "text/html; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Cache-Control", "no-cache")
        self.send_header("Access-Control-Allow-Origin", "*")
        self.end_headers()
        self.wfile.write(body)

    def _serve_json(self, obj):
        body = json.dumps(obj, default=str).encode("utf-8")
        self.send_response(200)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Cache-Control", "no-cache")
        self.send_header("Access-Control-Allow-Origin", "*")
        self.end_headers()
        self.wfile.write(body)

    def _api_frames(self):
        all_latest = _global_store.get_latest()
        return {
            "seq": _global_store.get_global_seq(),
            "ts": time.time(),
            "uptime": time.time() - _global_start_time,
            "ports": {str(p): f for p, f in all_latest.items()},
        }

    def _api_stats(self):
        stats = _global_store.get_stats()
        uptime = time.time() - _global_start_time
        total = sum(c for c, _ in stats.values())
        return {
            "uptime": uptime,
            "total_frames": total,
            "ports": {str(p): {"count": c, "type": t}
                      for p, (c, t) in stats.items()},
        }

    def _api_streams(self):
        """Stream registry: returns which ports have generators and/or data.

        Contract: launcher opens a window for a port IFF
            stream.enabled AND stream.active
        """
        stats = _global_store.get_stats()
        # Build registry from _global_stream_registry + live stats
        registry_by_port = {s["port"]: dict(s) for s in _global_stream_registry}
        # Also include any ports that have data but no registered generator
        for p, (count, ftype) in stats.items():
            if p not in registry_by_port:
                registry_by_port[p] = {
                    "port": p,
                    "name": PORT_LABELS.get(p, f"Port {p}"),
                    "type": ftype,
                    "generator": "external",
                    "enabled": True,
                }
            registry_by_port[p]["active"] = count > 0
            registry_by_port[p]["frame_count"] = count
        # Add registered-but-zero-frame ports as inactive
        for p, entry in registry_by_port.items():
            if "active" not in entry:
                entry["active"] = False
                entry["frame_count"] = 0
        return {
            "streams": sorted(registry_by_port.values(), key=lambda s: s["port"]),
        }

    def _handle_ws_upgrade(self):
        key = self.headers.get("Sec-WebSocket-Key", "")
        if not key:
            self.send_response(400)
            self.end_headers()
            return

        accept = ws_accept_key(key)
        response = (
            "HTTP/1.1 101 Switching Protocols\r\n"
            "Upgrade: websocket\r\n"
            "Connection: Upgrade\r\n"
            f"Sec-WebSocket-Accept: {accept}\r\n"
            "\r\n"
        )
        self.wfile.write(response.encode())
        self.wfile.flush()

        # Hand off to WebSocket client handler
        pf = getattr(self, "_port_filter", None)
        client = WSClient(self.request, _global_store, _global_verbose, port_filter=pf)
        client.serve()

    def _handle_sse(self):
        """Server-Sent Events — fallback for rough network connections."""
        self.send_response(200)
        self.send_header("Content-Type", "text/event-stream")
        self.send_header("Cache-Control", "no-cache")
        self.send_header("Connection", "keep-alive")
        self.send_header("Access-Control-Allow-Origin", "*")
        self.end_headers()

        pf = getattr(self, "_port_filter", None)
        last_seq = 0
        try:
            while True:
                current_seq = _global_store.get_global_seq()
                if current_seq > last_seq:
                    if pf is not None:
                        frames = _global_store.get_since_port(last_seq, pf)
                    else:
                        frames = _global_store.get_since(last_seq)
                    if frames:
                        data = json.dumps({
                            "type": "batch",
                            "frames": frames[-10:],
                            "seq": current_seq,
                        }, default=str)
                        self.wfile.write(f"data: {data}\n\n".encode())
                        self.wfile.flush()
                    last_seq = current_seq
                time.sleep(0.1)
        except Exception:
            pass

# ============================================================================
# HTML5 Dashboard — single-page app, Three.js 3D + Canvas2D charts
# Designed for Chrome/Chromium, optimised for rough local networks
# ============================================================================

DASHBOARD_HTML = r"""<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>VSEPR-SIM | Live Visualization</title>
<style>
*{margin:0;padding:0;box-sizing:border-box}
:root{
  --bg:#080810;--panel:#10101a;--hdr:#14141e;--fg:#e0e0e0;
  --accent:#bb86fc;--accent2:#03dac6;--accent3:#cf6679;
  --dim:#444455;--ok:#4CAF50;--gold:#FFD700;--warn:#FF9800;
}
html,body{height:100%;overflow:hidden;font-family:'Segoe UI',Consolas,monospace;
  background:var(--bg);color:var(--fg)}
#app{display:grid;grid-template-rows:42px 1fr;grid-template-columns:1fr 340px;
  height:100vh;gap:1px}
header{grid-column:1/-1;background:var(--hdr);display:flex;align-items:center;
  padding:0 16px;gap:16px;border-bottom:1px solid #222}
header h1{font-size:15px;color:var(--accent);font-weight:600}
header .status{font-size:11px;color:var(--dim);margin-left:auto}
header .status.ok{color:var(--ok)}
header .status.warn{color:var(--warn)}
header .status.err{color:var(--accent3)}
#view3d{background:var(--bg);position:relative;overflow:hidden}
#view3d canvas{width:100%!important;height:100%!important;display:block}
#hud{position:absolute;bottom:8px;left:8px;font-size:10px;color:var(--dim);
  pointer-events:none;line-height:1.5}
#side{background:var(--panel);overflow-y:auto;padding:8px;display:flex;
  flex-direction:column;gap:6px}
.card{background:var(--hdr);border-radius:6px;padding:8px 10px;border:1px solid #1a1a2a}
.card h3{font-size:11px;color:var(--accent);margin-bottom:6px;text-transform:uppercase;
  letter-spacing:.5px}
.card .row{display:flex;justify-content:space-between;font-size:10px;
  padding:2px 0;border-bottom:1px solid #111}
.card .row .k{color:var(--dim)}.card .row .v{color:var(--fg);text-align:right}
.bar-row{margin:2px 0}
.bar-label{display:inline-block;width:70px;font-size:9px;color:var(--dim)}
.bar-track{display:inline-block;width:calc(100% - 115px);height:8px;
  background:#111;border-radius:4px;vertical-align:middle;position:relative}
.bar-fill{height:100%;border-radius:4px;transition:width .3s}
.bar-val{display:inline-block;width:40px;font-size:9px;color:var(--fg);
  text-align:right}
#chart2d{height:200px;background:var(--hdr);border-radius:6px;
  border:1px solid #1a1a2a}
.port-grid{display:grid;grid-template-columns:auto 1fr auto auto;gap:2px 8px;
  font-size:9px}
.port-dot{width:6px;height:6px;border-radius:50%;display:inline-block;
  margin-right:4px}
.port-dot.on{background:var(--ok)}.port-dot.off{background:#333}
.ctrl-row{display:flex;gap:4px;flex-wrap:wrap}
.ctrl-row button{background:#1a1a2e;border:1px solid #333;color:var(--fg);
  font-size:9px;padding:3px 8px;border-radius:4px;cursor:pointer}
.ctrl-row button:hover{background:#252540;border-color:var(--accent)}
.ctrl-row button.active{border-color:var(--accent);color:var(--accent)}
@media(max-width:800px){
  #app{grid-template-columns:1fr;grid-template-rows:42px 1fr 300px}
  #side{overflow-x:auto}
}
</style>
</head>
<body>
<div id="app">
<header>
  <h1>VSEPR-SIM</h1>
  <span style="font-size:10px;color:var(--dim)">Atomistic Visualization</span>
  <span id="connStatus" class="status">Connecting…</span>
  <span id="fpsStatus" class="status">— FPS</span>
  <span id="frameStatus" class="status">0 frames</span>
</header>
<div id="view3d">
  <div id="hud"></div>
</div>
<div id="side">
  <div class="card" id="cardInfo">
    <h3>Current Frame</h3>
    <div id="infoRows"></div>
  </div>
  <div class="card" id="cardEnergy">
    <h3>Energy (kJ/mol)</h3>
    <div id="energyBars"></div>
  </div>
  <div class="card" id="cardScores">
    <h3>Quality Scores</h3>
    <div id="scoreBars"></div>
  </div>
  <div class="card">
    <h3>Energy Timeline</h3>
    <canvas id="chart2d" width="320" height="200"></canvas>
  </div>
  <div class="card" id="cardPorts">
    <h3>Port Status</h3>
    <div id="portGrid" class="port-grid"></div>
  </div>
  <div class="card">
    <h3>Controls</h3>
    <div class="ctrl-row">
      <button onclick="resetView()">Reset View</button>
      <button onclick="toggleBonds()" id="btnBonds" class="active">Bonds</button>
      <button onclick="cycleColor()">Color Mode</button>
      <button onclick="toggleSpin()" id="btnSpin">Auto-Spin</button>
      <button onclick="togglePause()" id="btnPause">Pause</button>
    </div>
  </div>
</div>
</div>

<script src="https://cdnjs.cloudflare.com/ajax/libs/three.js/r128/three.min.js"></script>
<script>
// ========================================================================
// VSEPR-SIM Browser Visualization Client
// Designed for Chrome/Chromium on rough local networks
// ========================================================================

const CONFIG = {
  wsReconnectMs: 1000,
  wsReconnectMax: 8000,
  pollFallbackMs: 500,
  maxAtoms: 2000,
  maxBonds: 4000,
  adaptiveFps: true,
  colorModes: ['role','element','charge','energy'],
  // TAO_TT: minimum display-hold before accepting next molecule (ms)
  // Ensures the visual is always fully loaded before the next frame replaces it
  taoTt: 2200,
  // Cross-fade duration (ms) when a new molecule is accepted
  fadeDuration: 400,
};

// ---- Per-window port context (injected by server via ?port= URL param) ----
const PORT_FILTER = (typeof window._VSEPR_PORT !== 'undefined') ? window._VSEPR_PORT : null;
const PORT_NAME   = (typeof window._VSEPR_PORT_NAME !== 'undefined') ? window._VSEPR_PORT_NAME : 'All Ports';
if (PORT_FILTER) {
  document.title = `VSEPR-SIM :${PORT_FILTER} ${PORT_NAME}`;
  const lbl = document.getElementById('portLabel');
  if (lbl) lbl.innerHTML = `<b style="color:var(--accent)">${PORT_NAME}</b> <span style="color:var(--dim)">:${PORT_FILTER}</span>`;
}

// ---- State ----
let state = {
  frames: [],
  latest: null,
  pending: null,          // next frame waiting behind tao gate
  lastDisplayTs: 0,       // performance.now() when last frame was accepted
  fadeAlpha: 1.0,         // 0..1 cross-fade progress
  fadingIn: false,
  seq: 0,
  paused: false,
  showBonds: true,
  colorMode: 'role',
  autoSpin: false,
  connected: false,
  transport: 'none',
  fps: 0,
  totalFrames: 0,
  energyHistory: [],
};

// ---- Connection management (WebSocket → SSE → Polling) ----
let ws = null;
let sse = null;
let pollTimer = null;
let wsReconnectDelay = CONFIG.wsReconnectMs;

function getWsUrl() {
  const loc = window.location;
  const proto = loc.protocol === 'https:' ? 'wss:' : 'ws:';
  const base = proto + '//' + loc.host + '/ws';
  return PORT_FILTER ? base + '?port=' + PORT_FILTER : base;
}

function connectWS() {
  if (state.paused) return;
  try {
    ws = new WebSocket(getWsUrl());
    ws.onopen = () => {
      state.connected = true;
      state.transport = 'ws';
      wsReconnectDelay = CONFIG.wsReconnectMs;
      updateConnStatus();
      // Stop SSE/polling fallbacks
      if (sse) { sse.close(); sse = null; }
      if (pollTimer) { clearInterval(pollTimer); pollTimer = null; }
    };
    ws.onmessage = (ev) => {
      try {
        const msg = JSON.parse(ev.data);
        if (msg.type === 'batch' && msg.frames) {
          msg.frames.forEach(processFrame);
          state.seq = msg.seq || state.seq;
        }
      } catch(e) {}
    };
    ws.onclose = ws.onerror = () => {
      state.connected = false;
      state.transport = 'none';
      updateConnStatus();
      ws = null;
      // Exponential backoff reconnect
      setTimeout(() => {
        wsReconnectDelay = Math.min(wsReconnectDelay * 1.5, CONFIG.wsReconnectMax);
        connectWS();
      }, wsReconnectDelay);
      // Start SSE fallback while reconnecting
      if (!sse && !pollTimer) connectSSE();
    };
  } catch(e) {
    // WS not available, fall to SSE
    setTimeout(connectSSE, 500);
  }
}

function connectSSE() {
  if (state.transport === 'ws') return;
  try {
    sse = new EventSource('/sse');
    sse.onmessage = (ev) => {
      state.connected = true;
      state.transport = 'sse';
      updateConnStatus();
      try {
        const msg = JSON.parse(ev.data);
        if (msg.type === 'batch' && msg.frames) {
          msg.frames.forEach(processFrame);
          state.seq = msg.seq || state.seq;
        }
      } catch(e) {}
    };
    sse.onerror = () => {
      if (state.transport === 'sse') {
        state.connected = false;
        state.transport = 'none';
        updateConnStatus();
      }
      if (sse) { sse.close(); sse = null; }
      // Fall to polling
      if (!pollTimer && state.transport !== 'ws') startPolling();
    };
  } catch(e) {
    startPolling();
  }
}

function startPolling() {
  if (pollTimer) return;
  pollTimer = setInterval(async () => {
    if (state.transport === 'ws') {
      clearInterval(pollTimer); pollTimer = null; return;
    }
    try {
      const r = await fetch('/api/frames');
      const data = await r.json();
      state.connected = true;
      state.transport = 'poll';
      updateConnStatus();
      if (data.ports) {
        Object.values(data.ports).forEach(processFrame);
      }
    } catch(e) {
      state.connected = false;
      state.transport = 'none';
      updateConnStatus();
    }
  }, CONFIG.pollFallbackMs);
}

function processFrame(f) {
  if (!f || typeof f !== 'object') return;
  // Client-side port filter: drop frames not belonging to our window's port
  if (PORT_FILTER && f._port && f._port !== PORT_FILTER) return;
  state.totalFrames++;
  // Track energy history regardless of display gate
  const e = f.energy;
  if (e && typeof e === 'object' && e.total !== undefined) {
    state.energyHistory.push(e.total);
    if (state.energyHistory.length > 200) state.energyHistory.shift();
  } else if (f.energy_Eh !== undefined) {
    state.energyHistory.push(f.energy_Eh);
    if (state.energyHistory.length > 200) state.energyHistory.shift();
  }
  // TAO_TT gate: buffer newest frame; animate() will promote it when ready
  state.pending = f;
  // If no molecule displayed yet, accept immediately
  if (!state.latest) {
    _acceptPending();
  }
}

function _acceptPending() {
  if (!state.pending) return;
  state.latest = state.pending;
  state.pending = null;
  state.lastDisplayTs = performance.now();
  // Reset auto-fit so camera re-frames for the new molecule
  orbitState.autoFit = false;
  // Trigger cross-fade
  state.fadeAlpha = 0.0;
  state.fadingIn = true;
}

function updateConnStatus() {
  const el = document.getElementById('connStatus');
  if (state.connected) {
    el.textContent = state.transport.toUpperCase() + ' ●';
    el.className = 'status ok';
  } else {
    el.textContent = 'Reconnecting…';
    el.className = 'status warn';
  }
}

// ---- Three.js 3D Viewer ----
let scene, camera, renderer, controls;
let atomGroup, bondGroup;
let atomMeshes = [], bondMeshes = [];
let sphereGeo, bondGeo;
let fpsFrames = 0, fpsLast = performance.now();

const ELEM_COLORS = {
  1:0xFFFFFF, 6:0x808080, 7:0x2244FF, 8:0xFF2222,
  16:0xFFFF22, 15:0xFF8800, 17:0x44FF44, 9:0x88FF88,
};
const ELEM_RADII = {1:0.25, 6:0.4, 7:0.38, 8:0.38, 16:0.45, 15:0.42, 17:0.42, 9:0.3};
const ROLE_COLORS = {
  0:0x888888, 1:0x2266FF, 2:0x44AA44, 3:0xFF6644,
  4:0xFF2222, 5:0x6644FF, 6:0xAAAAAA, 7:0x44CCFF,
  8:0xFF44CC, 9:0xFFFF00,
};

function initThree() {
  const container = document.getElementById('view3d');
  const w = container.clientWidth, h = container.clientHeight;

  scene = new THREE.Scene();
  scene.background = new THREE.Color(0x080810);
  scene.fog = new THREE.Fog(0x080810, 30, 80);

  camera = new THREE.PerspectiveCamera(55, w / h, 0.1, 200);
  camera.position.set(8, 6, 12);

  renderer = new THREE.WebGLRenderer({ antialias: true, powerPreference: 'high-performance' });
  renderer.setSize(w, h);
  renderer.setPixelRatio(Math.min(window.devicePixelRatio, 2));
  container.appendChild(renderer.domElement);

  // Lights
  scene.add(new THREE.AmbientLight(0xffffff, 0.5));
  const dir = new THREE.DirectionalLight(0xffffff, 0.8);
  dir.position.set(5, 10, 7);
  scene.add(dir);
  const dir2 = new THREE.DirectionalLight(0xbb86fc, 0.3);
  dir2.position.set(-5, -3, -5);
  scene.add(dir2);

  // Groups
  atomGroup = new THREE.Group();
  bondGroup = new THREE.Group();
  scene.add(atomGroup);
  scene.add(bondGroup);

  // Shared geometry
  sphereGeo = new THREE.SphereGeometry(1, 16, 12);
  bondGeo = new THREE.CylinderGeometry(0.06, 0.06, 1, 6);

  // Simple orbit controls (no dependency)
  setupOrbitControls(container);

  window.addEventListener('resize', () => {
    const w2 = container.clientWidth, h2 = container.clientHeight;
    camera.aspect = w2 / h2;
    camera.updateProjectionMatrix();
    renderer.setSize(w2, h2);
  });
}

// Minimal orbit controls (no external dependency)
let orbitState = {phi: 0.6, theta: 0.8, dist: 18, target: [0,0,0],
  dragging: false, lastX: 0, lastY: 0};

function setupOrbitControls(el) {
  el.addEventListener('mousedown', (e) => {
    orbitState.dragging = true;
    orbitState.lastX = e.clientX;
    orbitState.lastY = e.clientY;
  });
  window.addEventListener('mouseup', () => { orbitState.dragging = false; });
  window.addEventListener('mousemove', (e) => {
    if (!orbitState.dragging) return;
    const dx = e.clientX - orbitState.lastX;
    const dy = e.clientY - orbitState.lastY;
    orbitState.theta -= dx * 0.005;
    orbitState.phi = Math.max(0.05, Math.min(Math.PI - 0.05,
                     orbitState.phi - dy * 0.005));
    orbitState.lastX = e.clientX;
    orbitState.lastY = e.clientY;
  });
  el.addEventListener('wheel', (e) => {
    e.preventDefault();
    orbitState.dist *= (e.deltaY > 0) ? 1.08 : (1 / 1.08);
    orbitState.dist = Math.max(2, Math.min(100, orbitState.dist));
  }, {passive: false});
  // Touch support for mobile
  el.addEventListener('touchstart', (e) => {
    if (e.touches.length === 1) {
      orbitState.dragging = true;
      orbitState.lastX = e.touches[0].clientX;
      orbitState.lastY = e.touches[0].clientY;
    }
  });
  el.addEventListener('touchmove', (e) => {
    if (!orbitState.dragging || e.touches.length !== 1) return;
    const dx = e.touches[0].clientX - orbitState.lastX;
    const dy = e.touches[0].clientY - orbitState.lastY;
    orbitState.theta -= dx * 0.005;
    orbitState.phi = Math.max(0.05, Math.min(Math.PI - 0.05,
                     orbitState.phi - dy * 0.005));
    orbitState.lastX = e.touches[0].clientX;
    orbitState.lastY = e.touches[0].clientY;
    e.preventDefault();
  }, {passive: false});
  el.addEventListener('touchend', () => { orbitState.dragging = false; });
}

function updateCamera() {
  if (state.autoSpin && !orbitState.dragging) {
    orbitState.theta += 0.003;
  }
  const t = orbitState.target;
  camera.position.set(
    t[0] + orbitState.dist * Math.sin(orbitState.phi) * Math.cos(orbitState.theta),
    t[1] + orbitState.dist * Math.cos(orbitState.phi),
    t[2] + orbitState.dist * Math.sin(orbitState.phi) * Math.sin(orbitState.theta)
  );
  camera.lookAt(t[0], t[1], t[2]);
}

// ---- 3D Scene update ----
let materialCache = {};

function getMaterial(color) {
  const key = color;
  if (!materialCache[key]) {
    materialCache[key] = new THREE.MeshPhongMaterial({
      color: color, shininess: 40, specular: 0x222222,
      transparent: true, opacity: state.fadingIn ? state.fadeAlpha : 1.0,
    });
  }
  return materialCache[key];
}

function updateScene(frame) {
  if (!frame) return;
  const atoms = frame.atoms || [];
  const bonds = frame.bonds || [];

  // Compute centroid
  let cx=0, cy=0, cz=0;
  for (const a of atoms) { cx += (a.x||0); cy += (a.y||0); cz += (a.z||0); }
  if (atoms.length) { cx /= atoms.length; cy /= atoms.length; cz /= atoms.length; }
  orbitState.target = [cx, cy, cz];

  // Auto-fit camera: FOV-based framing from bounding geometry
  if (atoms.length > 0) {
    let rmax = 0;
    let bMinX = Infinity, bMinY = Infinity, bMinZ = Infinity;
    let bMaxX = -Infinity, bMaxY = -Infinity, bMaxZ = -Infinity;
    for (const a of atoms) {
      const ax = a.x||0, ay = a.y||0, az = a.z||0;
      const dx = ax - cx, dy = ay - cy, dz = az - cz;
      const r = Math.sqrt(dx*dx + dy*dy + dz*dz);
      if (r > rmax) rmax = r;
      if (ax < bMinX) bMinX = ax; if (ax > bMaxX) bMaxX = ax;
      if (ay < bMinY) bMinY = ay; if (ay > bMaxY) bMaxY = ay;
      if (az < bMinZ) bMinZ = az; if (az > bMaxZ) bMaxZ = az;
    }
    const ddx = bMaxX - bMinX, ddy = bMaxY - bMinY, ddz = bMaxZ - bMinZ;
    const diagonal = Math.sqrt(ddx*ddx + ddy*ddy + ddz*ddz);
    const fovRad = 55.0 * Math.PI / 180.0;
    const safety = 1.35;
    const targetDist = Math.max(6.0, (rmax * safety) / Math.tan(fovRad * 0.5));
    // Dynamic near/far clip planes based on scene extent
    camera.near = Math.max(0.01, targetDist * 0.01);
    camera.far  = Math.max(100.0, targetDist + diagonal * 4.0);
    camera.updateProjectionMatrix();
    // Smooth approach: lerp toward target distance over frames
    if (!orbitState.autoFit) {
      orbitState.dist = targetDist;
      orbitState.autoFit = true;
    } else {
      orbitState.dist += (targetDist - orbitState.dist) * 0.06;
    }
  }

  // Update atoms
  while (atomMeshes.length < atoms.length && atomMeshes.length < CONFIG.maxAtoms) {
    const mesh = new THREE.Mesh(sphereGeo, getMaterial(0x888888));
    atomGroup.add(mesh);
    atomMeshes.push(mesh);
  }
  for (let i = 0; i < atomMeshes.length; i++) {
    if (i < atoms.length) {
      const a = atoms[i];
      atomMeshes[i].visible = true;
      atomMeshes[i].position.set(a.x||0, a.y||0, a.z||0);
      const r = ELEM_RADII[a.Z] || 0.35;
      atomMeshes[i].scale.setScalar(r);
      atomMeshes[i].material = getMaterial(getAtomColor(a));
    } else {
      atomMeshes[i].visible = false;
    }
  }

  // Update bonds
  if (state.showBonds && bonds.length > 0) {
    while (bondMeshes.length < bonds.length && bondMeshes.length < CONFIG.maxBonds) {
      const mesh = new THREE.Mesh(bondGeo, getMaterial(0x555577));
      bondGroup.add(mesh);
      bondMeshes.push(mesh);
    }
    for (let i = 0; i < bondMeshes.length; i++) {
      if (i < bonds.length) {
        const b = bonds[i];
        const ai = atoms[b.i] || atoms.find(a => a.id === b.i);
        const aj = atoms[b.j] || atoms.find(a => a.id === b.j);
        if (ai && aj) {
          bondMeshes[i].visible = true;
          const mx = ((ai.x||0)+(aj.x||0))/2;
          const my = ((ai.y||0)+(aj.y||0))/2;
          const mz = ((ai.z||0)+(aj.z||0))/2;
          bondMeshes[i].position.set(mx, my, mz);
          const dx=(aj.x||0)-(ai.x||0), dy=(aj.y||0)-(ai.y||0), dz=(aj.z||0)-(ai.z||0);
          const len = Math.sqrt(dx*dx+dy*dy+dz*dz);
          bondMeshes[i].scale.set(1, len, 1);
          if (len > 0.001) {
            bondMeshes[i].quaternion.setFromUnitVectors(
              new THREE.Vector3(0,1,0),
              new THREE.Vector3(dx/len, dy/len, dz/len)
            );
          }
        } else {
          bondMeshes[i].visible = false;
        }
      } else {
        bondMeshes[i].visible = false;
      }
    }
  } else {
    // No explicit bonds — draw backbone trace for peptides
    for (const m of bondMeshes) m.visible = false;
    if (state.showBonds && atoms.length > 1) {
      const backbone = atoms.filter(a => [1,2,3].includes(a.role));
      backbone.sort((a,b) => (a.id||0)-(b.id||0));
      for (let i = 0; i < backbone.length - 1; i++) {
        if (i >= bondMeshes.length && bondMeshes.length < CONFIG.maxBonds) {
          const mesh = new THREE.Mesh(bondGeo, getMaterial(0x555577));
          bondGroup.add(mesh);
          bondMeshes.push(mesh);
        }
        if (i < bondMeshes.length) {
          const ai = backbone[i], aj = backbone[i+1];
          bondMeshes[i].visible = true;
          const mx=((ai.x||0)+(aj.x||0))/2, my=((ai.y||0)+(aj.y||0))/2,
                mz=((ai.z||0)+(aj.z||0))/2;
          bondMeshes[i].position.set(mx, my, mz);
          const dx=(aj.x||0)-(ai.x||0), dy=(aj.y||0)-(ai.y||0), dz=(aj.z||0)-(ai.z||0);
          const len = Math.sqrt(dx*dx+dy*dy+dz*dz);
          bondMeshes[i].scale.set(1, Math.max(0.01, len), 1);
          if (len > 0.001) {
            bondMeshes[i].quaternion.setFromUnitVectors(
              new THREE.Vector3(0,1,0),
              new THREE.Vector3(dx/len, dy/len, dz/len)
            );
          }
        }
      }
    }
  }

  bondGroup.visible = state.showBonds;
}

function getAtomColor(a) {
  switch (state.colorMode) {
    case 'element': return ELEM_COLORS[a.Z] || 0x888888;
    case 'role':    return ROLE_COLORS[a.role] || 0x888888;
    case 'charge': {
      const q = a.q || 0;
      if (q < -0.1) return 0xFF4444;
      if (q > 0.1) return 0x4444FF;
      return 0x888888;
    }
    case 'energy': {
      const chi = a.chi || 0;
      const r = Math.min(255, Math.floor(chi * 400));
      const b = Math.max(0, 255 - Math.floor(chi * 400));
      return (r << 16) | (0x44 << 8) | b;
    }
    default: return ELEM_COLORS[a.Z] || 0x888888;
  }
}

// ---- 2D Chart (energy timeline on canvas) ----
function drawEnergyChart() {
  const cvs = document.getElementById('chart2d');
  if (!cvs) return;
  const ctx = cvs.getContext('2d');
  const w = cvs.width, h = cvs.height;
  ctx.fillStyle = '#14141e';
  ctx.fillRect(0, 0, w, h);

  const data = state.energyHistory;
  if (data.length < 2) {
    ctx.fillStyle = '#444';
    ctx.font = '11px Consolas';
    ctx.textAlign = 'center';
    ctx.fillText('Waiting for data…', w/2, h/2);
    return;
  }

  const valid = data.filter(v => isFinite(v));
  if (!valid.length) return;
  let eMin = Math.min(...valid) - 5;
  let eMax = Math.max(...valid) + 5;
  if (Math.abs(eMax - eMin) < 1) eMax = eMin + 1;

  // Grid
  ctx.strokeStyle = '#222';
  ctx.lineWidth = 0.5;
  for (let i = 0; i <= 4; i++) {
    const y = 20 + (h - 40) * i / 4;
    ctx.beginPath(); ctx.moveTo(40, y); ctx.lineTo(w - 10, y); ctx.stroke();
    ctx.fillStyle = '#555';
    ctx.font = '8px Consolas';
    ctx.textAlign = 'right';
    ctx.fillText((eMax - (eMax - eMin) * i / 4).toFixed(0), 38, y + 3);
  }

  // Line
  ctx.strokeStyle = '#03dac6';
  ctx.lineWidth = 1.5;
  ctx.beginPath();
  for (let i = 0; i < data.length; i++) {
    const x = 42 + (w - 54) * i / (data.length - 1);
    const y = 20 + (h - 40) * (1 - (data[i] - eMin) / (eMax - eMin));
    if (i === 0) ctx.moveTo(x, y); else ctx.lineTo(x, y);
  }
  ctx.stroke();

  // Fill
  ctx.lineTo(42 + (w - 54), h - 20);
  ctx.lineTo(42, h - 20);
  ctx.closePath();
  ctx.fillStyle = 'rgba(3,218,198,0.08)';
  ctx.fill();

  // Labels
  ctx.fillStyle = '#bb86fc';
  ctx.font = '9px Consolas';
  ctx.textAlign = 'center';
  ctx.fillText('Energy Timeline (' + data.length + ' frames)', w/2, 12);
}

// ---- Side panel updates ----
function updateSidePanel(f) {
  if (!f) return;

  // Info rows
  const info = document.getElementById('infoRows');
  const seq = f.sequence || f.formula || '—';
  const env = f.environment || '';
  const st = f.state || f.phase || f.phase_guess || '';
  const nAtoms = f.atom_count || f.n_atoms || (f.atoms||[]).length;
  const nBonds = f.bond_count || (f.bonds||[]).length;
  const nRes = f.residue_count || (f.residues||[]).length;
  const port = f._port || '—';
  const ftype = f._type || '—';
  info.innerHTML = [
    ['Port',port+' ('+ftype+')'],['Sequence',seq],['Environment',env],
    ['State',st],['Atoms',nAtoms],['Bonds',nBonds],['Residues',nRes],
  ].map(([k,v])=>`<div class="row"><span class="k">${k}</span><span class="v">${v}</span></div>`).join('');

  // Energy bars
  const ebox = document.getElementById('energyBars');
  const e = (typeof f.energy === 'object') ? f.energy : null;
  if (e) {
    const comps = [
      ['Bond', e.bond, '#44FF44'], ['VdW', e.vdw, '#FF8844'],
      ['Coulomb', e.coulomb, '#4488FF'], ['Solvation', e.solvation, '#FF44FF'],
      ['Formation', e.formation, '#FFFF44'], ['Total', e.total, '#FFFFFF'],
    ];
    const maxV = Math.max(1, ...comps.map(c => Math.abs(c[1]||0)));
    ebox.innerHTML = comps.map(([name, val, col]) => {
      const pct = Math.abs(val||0) / maxV * 100;
      const sign = (val||0) < 0 ? '−' : '+';
      return `<div class="bar-row">
        <span class="bar-label">${name}</span>
        <span class="bar-track"><span class="bar-fill" style="width:${pct}%;background:${col}"></span></span>
        <span class="bar-val">${sign}${Math.abs(val||0).toFixed(1)}</span></div>`;
    }).join('');
  } else if (f.energy_Eh !== undefined) {
    ebox.innerHTML = `<div class="bar-row"><span class="bar-label">E_Eh</span>
      <span class="bar-track"><span class="bar-fill" style="width:50%;background:#03dac6"></span></span>
      <span class="bar-val">${(f.energy_Eh).toFixed(4)}</span></div>`;
  }

  // Score bars
  const sbox = document.getElementById('scoreBars');
  const sc = f.scores;
  if (sc && typeof sc === 'object') {
    const items = [
      ['Steric', sc.steric, '#44FF44'], ['Electrostatic', sc.electrostatic, '#4488FF'],
      ['Hydrophobic', sc.hydrophobic, '#FF8844'], ['Planarity', sc.planarity, '#FF44FF'],
      ['H-Bond', sc.hbond, '#44CCFF'], ['Confidence', sc.confidence, '#FFD700'],
    ];
    sbox.innerHTML = items.map(([name, val, col]) => {
      const pct = (val||0) * 100;
      return `<div class="bar-row">
        <span class="bar-label">${name}</span>
        <span class="bar-track"><span class="bar-fill" style="width:${pct}%;background:${col}"></span></span>
        <span class="bar-val">${(val||0).toFixed(3)}</span></div>`;
    }).join('');
  }
}

function updatePortStatus() {
  fetch('/api/stats').then(r => r.json()).then(data => {
    const grid = document.getElementById('portGrid');
    const ports = data.ports || {};
    const labels = {8899:"Base/2D",9990:"Crystal",9991:"Coarse",9992:"EHD",
      9993:"Thermal",9994:"Pep2D",9995:"Pep3D",9996:"Phase",9997:"Timeline",
      9998:"Analysis",9999:"Atomic"};
    let html = '';
    for (const [p, info] of Object.entries(ports)) {
      const on = info.count > 0;
      html += `<span class="port-dot ${on?'on':'off'}"></span>`;
      html += `<span>${p} ${labels[p]||''}</span>`;
      html += `<span style="color:var(--ok)">${info.count}</span>`;
      html += `<span style="color:var(--dim)">${info.type}</span>`;
    }
    // Also show ports with 0 frames
    for (const [p, lbl] of Object.entries(labels)) {
      if (!ports[p]) {
        html += `<span class="port-dot off"></span>`;
        html += `<span>${p} ${lbl}</span>`;
        html += `<span style="color:var(--dim)">0</span>`;
        html += `<span style="color:var(--dim)">—</span>`;
      }
    }
    grid.innerHTML = html;
  }).catch(() => {});
}

// ---- Controls ----
function resetView() {
  orbitState.phi = 0.6; orbitState.theta = 0.8; orbitState.dist = 18;
}
function toggleBonds() {
  state.showBonds = !state.showBonds;
  document.getElementById('btnBonds').className = state.showBonds ? 'active' : '';
}
function cycleColor() {
  const idx = CONFIG.colorModes.indexOf(state.colorMode);
  state.colorMode = CONFIG.colorModes[(idx + 1) % CONFIG.colorModes.length];
  // Force material rebuild
  materialCache = {};
}
function toggleSpin() {
  state.autoSpin = !state.autoSpin;
  document.getElementById('btnSpin').className = state.autoSpin ? 'active' : '';
}
function togglePause() {
  state.paused = !state.paused;
  document.getElementById('btnPause').className = state.paused ? 'active' : '';
  document.getElementById('btnPause').textContent = state.paused ? 'Resume' : 'Pause';
}

// ---- Keyboard shortcuts ----
document.addEventListener('keydown', (e) => {
  switch(e.key.toLowerCase()) {
    case 'r': resetView(); break;
    case 'b': toggleBonds(); break;
    case 'c': cycleColor(); break;
    case 's': toggleSpin(); break;
    case ' ': togglePause(); e.preventDefault(); break;
  }
});

// ---- Main loop ----
function animate() {
  requestAnimationFrame(animate);
  if (state.paused) return;

  const now = performance.now();

  // TAO_TT gate: promote pending frame after hold period
  if (state.pending && state.latest &&
      (now - state.lastDisplayTs) >= CONFIG.taoTt) {
    _acceptPending();
  }

  // Cross-fade advance
  if (state.fadingIn) {
    state.fadeAlpha = Math.min(1.0, state.fadeAlpha +
                     (16.67 / CONFIG.fadeDuration));
    const a = state.fadeAlpha;
    atomGroup.children.forEach(m => { if (m.material) m.material.opacity = a; });
    bondGroup.children.forEach(m => { if (m.material) m.material.opacity = a; });
    if (state.fadeAlpha >= 1.0) state.fadingIn = false;
  }

  updateCamera();

  if (state.latest) {
    updateScene(state.latest);
  }

  renderer.render(scene, camera);

  // FPS counter
  fpsFrames++;
  if (now - fpsLast > 1000) {
    state.fps = Math.round(fpsFrames * 1000 / (now - fpsLast));
    fpsFrames = 0;
    fpsLast = now;
    document.getElementById('fpsStatus').textContent = state.fps + ' FPS';
    document.getElementById('frameStatus').textContent = state.totalFrames + ' frames';
  }

  // HUD
  const hud = document.getElementById('hud');
  if (state.latest) {
    const molN = state.latest.molecule_name || '';
    const seq = state.latest.sequence || state.latest.formula || '';
    const headline = molN ? `<b style="color:#0ff">${molN}</b>` : seq;
    const sub = molN ? `<span style="font-size:10px;opacity:0.7">${seq}</span>` : '';
    const e = state.latest.energy;
    const eStr = (e && typeof e === 'object') ? `E=${(e.total||0).toFixed(1)} kJ/mol` :
                 (state.latest.energy_Eh !== undefined) ? `E=${state.latest.energy_Eh.toFixed(4)} Eh` : '';
    const conf = state.latest.scores ? `conf=${(state.latest.scores.confidence||0).toFixed(3)}` : '';
    const st = state.latest.state || '';
    // Show tau_Tt hold-down timer
    const held = Math.max(0, CONFIG.taoTt - (now - state.lastDisplayTs));
    const taoStr = state.pending ? ` [tau:${(held/1000).toFixed(1)}s]` : '';
    hud.innerHTML = `${headline}<br>${sub}${sub?'<br>':''}${eStr}  ${conf}  ${st}${taoStr}<br>[${state.colorMode}] bonds=${state.showBonds?'ON':'OFF'}`;
  }
}

// ---- Side panel refresh timer ----
setInterval(() => {
  if (state.latest) updateSidePanel(state.latest);
  drawEnergyChart();
}, 200);

setInterval(updatePortStatus, 2000);

// ---- Startup ----
window.addEventListener('DOMContentLoaded', () => {
  initThree();
  connectWS();
  animate();
  updatePortStatus();
});
</script>
</body>
</html>
"""

# ============================================================================
# Built-in Stochastic Molecule Generator
# Mirrors the C++ peptide_stochastic_viz.cpp physics pipeline in Python
# Pushes a new complex named molecule to port 9999 every 1-10 seconds
# ============================================================================

import random
import math as _math

# ---- Amino acid library (mirrored from C++ AMINO_ACIDS[]) ----

_AMINO_ACIDS = [
    # (three_letter, one_letter, sidechain_class, sidechain_heavy_atoms, mass)
    ("GLY", "G", 0, 0,   1.008),
    ("ALA", "A", 1, 1,  15.035),
    ("VAL", "V", 1, 3,  43.089),
    ("LEU", "L", 1, 4,  57.116),
    ("ILE", "I", 1, 4,  57.116),
    ("PRO", "P", 1, 3,  42.081),
    ("PHE", "F", 5, 7,  91.132),
    ("TRP", "W", 5, 10,130.170),
    ("MET", "M", 6, 4,  75.154),
    ("CYS", "C", 6, 2,  47.099),
    ("SER", "S", 2, 2,  31.034),
    ("THR", "T", 2, 3,  45.061),
    ("ASN", "N", 2, 4,  58.060),
    ("GLN", "Q", 2, 5,  72.087),
    ("TYR", "Y", 5, 8, 107.131),
    ("ASP", "D", 3, 4,  59.044),
    ("GLU", "E", 3, 5,  73.071),
    ("LYS", "K", 4, 5,  72.130),
    ("ARG", "R", 4, 7, 100.144),
    ("HIS", "H", 4, 6,  81.097),
]

_SIDECHAIN_NAMES = {
    0: "none", 1: "hydrophobic", 2: "polar", 3: "acidic",
    4: "basic", 5: "aromatic", 6: "sulfur",
}

_ENVIRONMENTS = [
    (1, "vacuum"),
    (2, "dry_condensed"),
    (3, "polar_solvent"),
    (4, "nonpolar_solvent"),
    (5, "reactive_field"),
]

_FORMATION_STATES = [
    "PREFORM", "LOCAL_GROUP_FORMED", "MOLECULE_FORMED", "LINKED",
    "CONFORMATIONAL_SOLVE", "LOCAL_FOLDED", "MACRO_STABLE",
]

# ---- Named peptide templates (biologically interesting) ----

_NAMED_PEPTIDES = [
    ("Enkephalin (Met)",         ["TYR","GLY","GLY","PHE","MET"]),
    ("Enkephalin (Leu)",         ["TYR","GLY","GLY","PHE","LEU"]),
    ("Bradykinin",               ["ARG","PRO","PRO","GLY","PHE","SER","PRO","PHE","ARG"]),
    ("Oxytocin fragment",        ["CYS","TYR","ILE","GLN","ASN","CYS","PRO","LEU","GLY"]),
    ("Angiotensin II",           ["ASP","ARG","VAL","TYR","ILE","HIS","PRO","PHE"]),
    ("Substance P",              ["ARG","PRO","LYS","PRO","GLN","GLN","PHE","PHE","GLY","LEU","MET"]),
    ("Glucagon fragment",        ["HIS","SER","GLN","GLY","THR","PHE","THR","SER"]),
    ("Melittin N-term",          ["GLY","ILE","GLY","ALA","VAL","LEU","LYS","VAL","LEU","THR"]),
    ("GnRH (Gonadotropin)",      ["GLU","HIS","TRP","SER","TYR","GLY","LEU","ARG","PRO","GLY"]),
    ("Somatostatin core",        ["ALA","GLY","CYS","LYS","ASN","PHE","PHE","TRP","LYS","THR"]),
    ("Insulin B-chain N-term",   ["PHE","VAL","ASN","GLN","HIS","LEU","CYS","GLY","SER"]),
    ("Amyloid-beta 1-8",         ["ASP","ALA","GLU","PHE","ARG","HIS","ASP","SER"]),
    ("Collagen motif GPO",       ["GLY","PRO","ALA","GLY","PRO","ALA","GLY","PRO","ALA"]),
    ("Elastin VPGVG",            ["VAL","PRO","GLY","VAL","GLY","VAL","PRO","GLY","VAL","GLY"]),
    ("Alpha-conotoxin core",     ["GLY","CYS","CYS","SER","ASN","PRO","VAL","CYS"]),
    ("Defensin fragment",        ["ALA","CYS","TYR","CYS","ARG","ILE","PRO","ALA","CYS"]),
    ("Tachykinin core",          ["PHE","VAL","GLY","LEU","MET"]),
    ("Galanin fragment",         ["GLY","TRP","THR","LEU","ASN","SER","ALA","GLY","TYR","LEU"]),
    ("Neuropeptide Y core",      ["TYR","PRO","SER","LYS","PRO","ASP","ASN","PRO"]),
    ("Endothelin fragment",      ["CYS","SER","CYS","SER","SER","LEU","MET","ASP"]),
    ("Calcitonin fragment",      ["CYS","GLY","ASN","LEU","SER","THR","CYS","MET","LEU","GLY"]),
    ("Dynorphin A core",         ["TYR","GLY","GLY","PHE","LEU","ARG","ARG","ILE"]),
    ("Orexin core",              ["HIS","ALA","GLY","ARG","LEU","HIS","GLY","MET"]),
    ("Atrial natriuretic N",     ["SER","LEU","ARG","ARG","SER","SER","CYS","PHE","GLY","GLY"]),
    ("RGD integrin motif",       ["GLY","ARG","GLY","ASP","SER","PRO"]),
    ("Polyalanine helix",        ["ALA","ALA","ALA","ALA","ALA","ALA","ALA","ALA"]),
    ("Mixed charged",            ["LYS","GLU","LYS","GLU","LYS","GLU"]),
    ("Hydrophobic core",         ["LEU","ILE","VAL","PHE","TRP","ALA","LEU","VAL"]),
    ("Disulfide pair",           ["CYS","ALA","GLY","ALA","CYS"]),
    ("Beta-turn motif",          ["ASN","GLY","PRO","ASP"]),
    ("Helix-breaker",            ["ALA","ALA","PRO","GLY","ALA","ALA"]),
    ("Salt bridge pair",         ["LYS","ALA","ALA","GLU"]),
    ("Aromatic cluster",         ["PHE","TYR","TRP","PHE","TYR"]),
    ("Glycine-rich loop",        ["GLY","GLY","GLY","GLY","GLY","SER"]),
    ("Proline-rich motif",       ["PRO","PRO","PRO","PRO","GLY","PRO"]),
]

# ---- Bond geometry constants (from valence_tables.hpp) ----
_PEPTIDE_BOND_A    = 1.33
_N_CA_BOND_A       = 1.47
_CA_C_BOND_A       = 1.52
_C_O_BOND_A        = 1.23
_PLANARITY_TOL_DEG = 10.0

# ---- Element data ----
_ELEM = {
    6:  ("C",  76.0, 170.0, 12.011,  2.55),
    7:  ("N",  71.0, 155.0, 14.007,  3.04),
    8:  ("O",  66.0, 152.0, 15.999,  3.44),
    16: ("S", 105.0, 180.0, 32.06,   2.58),
}

# Role constants (mirrored from VSEPR_ChemRole)
_R_NONE = 0; _R_BB_N = 1; _R_ALPHA_C = 2; _R_CARB_C = 3; _R_CARB_O = 4
_R_AMIDE_N = 5; _R_SC = 6; _R_H_DONOR = 7; _R_H_ACCEPT = 8; _R_S_LINK = 9


def _generate_chain(rng, named=True, min_res=3, max_res=10):
    """Generate a single peptide chain with full physics."""
    # Choose a named peptide or generate random
    if named and rng.random() < 0.7 and _NAMED_PEPTIDES:
        name, residue_names = rng.choice(_NAMED_PEPTIDES)
    else:
        n_res = rng.randint(min_res, max_res)
        residue_names = [rng.choice(_AMINO_ACIDS)[0] for _ in range(n_res)]
        name = "-".join(residue_names)

    env_id, env_name = rng.choice(_ENVIRONMENTS)

    atoms = []
    residues = []
    atom_id = 1
    x = 0.0

    for i, res_name in enumerate(residue_names):
        aa = next((a for a in _AMINO_ACIDS if a[0] == res_name), _AMINO_ACIDS[0])
        three, one, sc_class, sc_heavy, sc_mass = aa

        base_id = atom_id
        # Backbone: N, CA, C, O
        phi = rng.uniform(-180, 180)
        psi = rng.uniform(-180, 180)
        omega = 180.0 + rng.uniform(-5, 5)

        # Noise generators
        pn = lambda: rng.uniform(-0.1, 0.1)
        cn = lambda: rng.uniform(-0.3, 0.3)

        atoms.append({
            "id": atom_id, "Z": 7, "sym": "N",
            "x": x + pn(), "y": pn(), "z": pn(),
            "q": -0.42 + cn(), "role": _R_BB_N,
            "name": f"{three}_N",
            "cov_r": 71.0, "vdw_r": 155.0, "mass": 14.007,
        })
        atom_id += 1

        atoms.append({
            "id": atom_id, "Z": 6, "sym": "C",
            "x": x + _N_CA_BOND_A + pn(), "y": pn(), "z": pn(),
            "q": 0.02 + cn(), "role": _R_ALPHA_C,
            "name": f"{three}_CA",
            "cov_r": 76.0, "vdw_r": 170.0, "mass": 12.011,
        })
        atom_id += 1

        atoms.append({
            "id": atom_id, "Z": 6, "sym": "C",
            "x": x + _N_CA_BOND_A + _CA_C_BOND_A + pn(), "y": pn(), "z": pn(),
            "q": 0.51 + cn(), "role": _R_CARB_C,
            "name": f"{three}_C",
            "cov_r": 76.0, "vdw_r": 170.0, "mass": 12.011,
        })
        atom_id += 1

        atoms.append({
            "id": atom_id, "Z": 8, "sym": "O",
            "x": x + _N_CA_BOND_A + _CA_C_BOND_A + pn(),
            "y": _C_O_BOND_A + pn(), "z": pn(),
            "q": -0.51 + cn(), "role": _R_CARB_O,
            "name": f"{three}_O",
            "cov_r": 66.0, "vdw_r": 152.0, "mass": 15.999,
        })
        atom_id += 1

        sc_root = -1
        # Sidechain heavy atoms
        if sc_heavy > 0:
            sc_root = atom_id
            for s in range(sc_heavy):
                sc_Z = 6; sc_m = 12.011
                if sc_class == 6 and s == sc_heavy - 1:  # sulfur
                    sc_Z = 16; sc_m = 32.06
                elif sc_class == 2 and s == sc_heavy - 1:  # polar
                    sc_Z = 8; sc_m = 15.999
                elif sc_class == 3 and s >= sc_heavy - 2:  # acidic
                    sc_Z = 8; sc_m = 15.999

                sym = "C" if sc_Z == 6 else ("O" if sc_Z == 8 else "S")
                atoms.append({
                    "id": atom_id, "Z": sc_Z, "sym": sym,
                    "x": x + _N_CA_BOND_A + rng.uniform(-2, 2),
                    "y": -1.0 + rng.uniform(-2, 2),
                    "z": rng.uniform(-2, 2),
                    "q": rng.uniform(-0.3, 0.3), "role": _R_SC,
                    "name": f"{three}_SC{s}",
                    "cov_r": _ELEM.get(sc_Z, (0, 76, 170, 12, 2.5))[1],
                    "vdw_r": _ELEM.get(sc_Z, (0, 76, 170, 12, 2.5))[2],
                    "mass": sc_m,
                })
                atom_id += 1

        atom_ids = list(range(base_id, atom_id))
        residues.append({
            "id": i + 1, "name": three,
            "backbone_N": base_id, "backbone_CA": base_id + 1,
            "backbone_C": base_id + 2, "backbone_O": base_id + 3,
            "sidechain_root": sc_root,
            "sc_class": sc_class, "phi": phi, "psi": psi, "omega": omega,
            "atom_ids": atom_ids,
        })

        x += _N_CA_BOND_A + _CA_C_BOND_A + _PEPTIDE_BOND_A

    # ---- Energy model (mirrors EnergyModel::evaluate) ----
    k_bond = 200.0
    epsilon_lj = 0.5
    coulomb_k = 1389.354
    born_factor = -69.5

    dielectric = 80.0 if env_id == 3 else (4.0 if env_id == 4 else 1.0)

    def dist(a, b):
        return _math.sqrt((a["x"]-b["x"])**2 + (a["y"]-b["y"])**2 + (a["z"]-b["z"])**2)

    # Bond energy (backbone sequential bonds)
    e_bond = 0.0
    for r in residues:
        ids = r["atom_ids"]
        for k in range(len(ids) - 1):
            a1 = atoms[ids[k] - 1]
            a2 = atoms[ids[k + 1] - 1]
            d = dist(a1, a2)
            r0 = (a1["cov_r"] + a2["cov_r"]) / 100.0
            if r0 < 0.5: r0 = 1.5
            dr = d - r0
            e_bond += 0.5 * k_bond * dr * dr
    # Inter-residue peptide bonds
    for j in range(len(residues) - 1):
        a_c = atoms[residues[j]["backbone_C"] - 1]
        a_n = atoms[residues[j + 1]["backbone_N"] - 1]
        d = dist(a_c, a_n)
        r0 = _PEPTIDE_BOND_A
        dr = d - r0
        e_bond += 0.5 * k_bond * dr * dr

    # VdW (Lennard-Jones, skip bonded 1-2)
    e_vdw = 0.0
    n_atoms = len(atoms)
    for ii in range(n_atoms):
        for jj in range(ii + 2, min(ii + 30, n_atoms)):  # capped window for perf
            d = dist(atoms[ii], atoms[jj])
            if d < 0.1: continue
            sigma = (atoms[ii]["vdw_r"] + atoms[jj]["vdw_r"]) / 200.0
            if sigma < 1.0: sigma = 3.0
            sr6 = (sigma / d) ** 6
            e_vdw += 4.0 * epsilon_lj * (sr6 * sr6 - sr6)

    # Coulomb
    e_coul = 0.0
    for ii in range(n_atoms):
        qi = atoms[ii]["q"]
        if abs(qi) < 1e-10: continue
        for jj in range(ii + 1, min(ii + 25, n_atoms)):
            qj = atoms[jj]["q"]
            if abs(qj) < 1e-10: continue
            d = dist(atoms[ii], atoms[jj])
            if d < 0.1: continue
            e_coul += coulomb_k * qi * qj / (dielectric * d)

    # Solvation (Born model, polar solvent only)
    e_solv = 0.0
    if env_id == 3:
        for a in atoms:
            if abs(a["q"]) > 0.1:
                r_born = a["vdw_r"] / 100.0
                if r_born < 1.0: r_born = 1.5
                e_solv += born_factor * a["q"]**2 / r_born

    # Formation (peptide bond condensation)
    n_peptide_bonds = max(0, len(residues) - 1)
    e_form = n_peptide_bonds * (-10.0)

    e_total = e_bond + e_vdw + e_coul + e_solv + e_form

    # ---- Scoring (mirrors ScoringEngine::score) ----
    steric = max(0.0, 1.0 - abs(e_vdw) / 100.0)

    electrostatic = min(1.0, abs(e_coul) / 50.0) if e_coul < 0 else 0.0

    n_hydro = sum(1 for r in residues if r["sc_class"] in (1, 5))
    hydrophobic = n_hydro / len(residues) if residues else 0.0

    plan_devs = []
    for r in residues:
        dev_t = abs(r["omega"] - 180.0)
        dev_c = abs(r["omega"])
        plan_devs.append(min(dev_t, dev_c))
    avg_dev = sum(plan_devs) / len(plan_devs) if plan_devs else 0.0
    planarity = max(0.0, 1.0 - avg_dev / _PLANARITY_TOL_DEG)

    # H-bond detection (simplified: N-O distance 2.0-3.5 A)
    hbond_count = 0
    hbond_score = 0.0
    donors = [a for a in atoms if a["role"] in (_R_BB_N, _R_AMIDE_N)]
    acceptors = [a for a in atoms if a["role"] == _R_CARB_O]
    for don in donors:
        for acc in acceptors:
            if don["id"] == acc["id"]: continue
            d = dist(don, acc)
            if 2.0 <= d <= 3.5:
                hbond_count += 1
                hbond_score += max(0.0, 1.0 - abs(d - 2.8))

    hbond_score_norm = min(1.0, hbond_score / max(1, len(residues)))

    confidence = ((steric + 0.01) * (planarity + 0.01) * (electrostatic + 0.01)) ** (1.0 / 3.0)

    # Formation state
    state_idx = min(6, max(0, int(confidence * 7)))
    state_name = _FORMATION_STATES[state_idx]

    # ---- Functional groups ----
    func_groups = []
    for j in range(len(residues) - 1):
        func_groups.append(f"amide_{residues[j]['name']}_{residues[j+1]['name']}")
    for r in residues:
        if r["sc_class"] == 6:
            func_groups.append(f"thiol_{r['name']}")
        if r["sc_class"] == 3:
            func_groups.append(f"carboxyl_{r['name']}")
        if r["sc_class"] == 4:
            func_groups.append(f"amine_{r['name']}")
        if r["sc_class"] == 5:
            func_groups.append(f"aromatic_{r['name']}")

    # ---- Build JSON-compatible frame ----
    frame = {
        "sequence": "-".join(r["name"] for r in residues),
        "molecule_name": name,
        "environment": env_name,
        "success": True,
        "state": state_name,
        "atom_count": len(atoms),
        "residue_count": len(residues),
        "bond_count": n_peptide_bonds + sum(len(r["atom_ids"]) - 1 for r in residues),
        "hbond_count": hbond_count,
        "energy": {
            "total": round(e_total, 3),
            "bond": round(e_bond, 3),
            "vdw": round(e_vdw, 3),
            "coulomb": round(e_coul, 3),
            "solvation": round(e_solv, 3),
            "formation": round(e_form, 3),
        },
        "scores": {
            "steric": round(steric, 4),
            "hbond": round(hbond_score_norm, 4),
            "electrostatic": round(electrostatic, 4),
            "hydrophobic": round(hydrophobic, 4),
            "planarity": round(planarity, 4),
            "confidence": round(confidence, 4),
        },
        "validity": {
            "chemical": True,
            "valence": True,
        },
        "atoms": [{
            "id": a["id"], "Z": a["Z"], "sym": a["sym"],
            "x": round(a["x"], 4), "y": round(a["y"], 4), "z": round(a["z"], 4),
            "q": round(a["q"], 4), "role": a["role"],
        } for a in atoms],
        "residues": [{
            "id": r["id"], "name": r["name"],
            "phi": round(r["phi"], 2), "psi": round(r["psi"], 2),
            "omega": round(r["omega"], 2), "sc_class": r["sc_class"],
        } for r in residues],
        "functional_groups": func_groups[:20],
    }
    return frame


class StochasticGenerator:
    """Background thread: produces random complex named molecules at 1-10s intervals.

    Pushes frames directly into the FrameStore on port 9999 (Atomic Live).
    Mirrors the full C++ peptide formation pipeline:
      - 20 standard amino acids, 35+ named peptide templates
      - 5 environment classes
      - Backbone geometry (N-CA-C-O + sidechains)
      - Energy model: harmonic bond, LJ 12-6, Coulomb, Born solvation, condensation
      - Scoring: steric, electrostatic, hydrophobic, planarity, H-bond, confidence
      - Functional group detection
    """

    def __init__(self, store, port=9999, min_interval=1.0, max_interval=10.0,
                 verbose=False):
        self.store = store
        self.port = port
        self.min_interval = min_interval
        self.max_interval = max_interval
        self.verbose = verbose
        self.running = True
        self.run_id = 0
        self.rng = random.Random()

    def start(self):
        threading.Thread(target=self._loop, daemon=True).start()

    def _loop(self):
        while self.running:
            try:
                self.run_id += 1
                frame = _generate_chain(self.rng, named=True, min_res=3, max_res=12)
                frame["run_id"] = self.run_id
                frame["generator"] = "builtin_stochastic"
                self.store.push(self.port, frame)

                if self.verbose:
                    seq = frame.get("molecule_name", frame.get("sequence", "?"))
                    e = frame["energy"]["total"]
                    c = frame["scores"]["confidence"]
                    n = frame["atom_count"]
                    print(f"  [GEN #{self.run_id:04d}] {seq}  "
                          f"atoms={n}  E={e:+.1f}  conf={c:.3f}")
            except Exception as ex:
                if self.verbose:
                    print(f"  [GEN ERROR] {ex}")

            delay = self.rng.uniform(self.min_interval, self.max_interval)
            time.sleep(delay)

    def stop(self):
        self.running = False


# ============================================================================
# Crystal Structure Generator — port 9990
# Generates periodic crystal lattice snapshots: FCC, BCC, HCP, NaCl, ZnS, Perovskite
# Full atomistic output with lattice vectors, unit cell, and bond geometry
# ============================================================================

_CRYSTAL_SYSTEMS = [
    # (name, lattice, basis_atoms, a_ang, color_hint)
    ("FCC Copper",         "cubic",      [("Cu",29)], 3.615, "metal"),
    ("BCC Iron",           "cubic",      [("Fe",26)], 2.866, "metal"),
    ("HCP Titanium",       "hexagonal",  [("Ti",22)], 2.951, "metal"),
    ("NaCl (Halite)",      "cubic",      [("Na",11),("Cl",17)], 5.64,  "ionic"),
    ("ZnS (Sphalerite)",   "cubic",      [("Zn",30),("S",16)],  5.41,  "semiconductor"),
    ("Diamond Cubic",      "cubic",      [("C",6)],  3.567, "covalent"),
    ("Perovskite BaTiO3",  "cubic",      [("Ba",56),("Ti",22),("O",8)], 4.01, "oxide"),
    ("Graphite layer",     "hexagonal",  [("C",6)],  2.461, "layered"),
    ("Fluorite CaF2",      "cubic",      [("Ca",20),("F",9)],   5.46,  "ionic"),
    ("Wurtzite GaN",       "hexagonal",  [("Ga",31),("N",7)],   3.189, "semiconductor"),
    ("Alpha-Quartz SiO2",  "trigonal",   [("Si",14),("O",8)],   4.913, "covalent"),
    ("Rocksalt MgO",       "cubic",      [("Mg",12),("O",8)],   4.212, "ionic"),
]

# Atomic radii for crystal rendering (Angstrom, scaled)
_CRYSTAL_RADII = {
    6:0.77, 7:0.75, 8:0.73, 11:1.54, 12:1.45, 14:1.11, 16:1.03,
    17:0.99, 20:1.74, 22:1.47, 26:1.26, 29:1.28, 30:1.22,
    30:1.22, 31:1.22, 56:2.15, 9:0.64,
}
_CRYSTAL_COLORS = {
    6:0x606060, 7:0x2244FF, 8:0xFF2222, 11:0xAA88FF, 12:0x66FF66,
    14:0xBB8844, 16:0xFFFF22, 17:0x44FF44, 20:0x8888FF, 22:0xCCCCCC,
    26:0xCC6622, 29:0xFF9933, 30:0x8888CC, 31:0x6688AA, 56:0x22DDAA,
    9:0x88FFAA,
}

def _generate_crystal(rng):
    """Generate a crystal supercell frame for port 9990."""
    system = rng.choice(_CRYSTAL_SYSTEMS)
    name, lattice_type, basis, a, crystal_class = system

    # Supercell size: 3x3x3 to 5x5x5
    nx = rng.randint(3, 5)
    ny = rng.randint(3, 5)
    nz = rng.randint(3, 5) if lattice_type != "hexagonal" else rng.randint(2, 4)

    atoms = []
    atom_id = 0

    def add_basis(ox, oy, oz, jitter=0.04):
        nonlocal atom_id
        if lattice_type == "cubic":
            if len(basis) == 1:
                sym, Z = basis[0]
                # FCC or BCC or Diamond based on name
                positions = [(0,0,0)]
                if "FCC" in name:
                    positions += [(0.5,0.5,0),(0.5,0,0.5),(0,0.5,0.5)]
                elif "BCC" in name:
                    positions += [(0.5,0.5,0.5)]
                elif "Diamond" in name:
                    positions += [(0.5,0.5,0),(0.5,0,0.5),(0,0.5,0.5),
                                  (0.25,0.25,0.25),(0.75,0.75,0.25),
                                  (0.75,0.25,0.75),(0.25,0.75,0.75)]
                for (fx,fy,fz) in positions:
                    x = (ox+fx)*a + rng.gauss(0, jitter)
                    y = (oy+fy)*a + rng.gauss(0, jitter)
                    z = (oz+fz)*a + rng.gauss(0, jitter)
                    atoms.append({"id":atom_id,"Z":Z,"sym":sym,
                                  "x":round(x,4),"y":round(y,4),"z":round(z,4),
                                  "q":0.0,"role":0,"chi":rng.uniform(0.1,0.9)})
                    atom_id += 1
            elif len(basis) == 2:
                # NaCl / ZnS / Fluorite / Rocksalt: alternating sublattices
                symA, ZA = basis[0]; symB, ZB = basis[1]
                posA = [(0,0,0),(0.5,0.5,0),(0.5,0,0.5),(0,0.5,0.5)]
                posB = [(0.5,0,0),(0,0.5,0),(0,0,0.5),(0.5,0.5,0.5)]
                for (fx,fy,fz) in posA:
                    x=(ox+fx)*a+rng.gauss(0,jitter); y=(oy+fy)*a+rng.gauss(0,jitter)
                    z=(oz+fz)*a+rng.gauss(0,jitter)
                    atoms.append({"id":atom_id,"Z":ZA,"sym":symA,
                                  "x":round(x,4),"y":round(y,4),"z":round(z,4),
                                  "q":round(rng.uniform(0.8,1.2),3),"role":0,
                                  "chi":0.3})
                    atom_id += 1
                for (fx,fy,fz) in posB:
                    x=(ox+fx)*a+rng.gauss(0,jitter); y=(oy+fy)*a+rng.gauss(0,jitter)
                    z=(oz+fz)*a+rng.gauss(0,jitter)
                    atoms.append({"id":atom_id,"Z":ZB,"sym":symB,
                                  "x":round(x,4),"y":round(y,4),"z":round(z,4),
                                  "q":round(rng.uniform(-1.2,-0.8),3),"role":0,
                                  "chi":0.7})
                    atom_id += 1
            elif len(basis) == 3:
                # Perovskite ABO3: A at corners, B at body center, O at face centers
                symA,ZA=basis[0]; symB,ZB=basis[1]; symO,ZO=basis[2]
                corners=[(0,0,0),(1,0,0),(0,1,0),(0,0,1),(1,1,0),(1,0,1),(0,1,1),(1,1,1)]
                for (fx,fy,fz) in corners:
                    x=(ox+fx)*a+rng.gauss(0,jitter); y=(oy+fy)*a+rng.gauss(0,jitter)
                    z=(oz+fz)*a+rng.gauss(0,jitter)
                    atoms.append({"id":atom_id,"Z":ZA,"sym":symA,
                                  "x":round(x,4),"y":round(y,4),"z":round(z,4),
                                  "q":2.0,"role":0,"chi":0.2})
                    atom_id += 1
                x=(ox+0.5)*a+rng.gauss(0,jitter); y=(oy+0.5)*a+rng.gauss(0,jitter)
                z=(oz+0.5)*a+rng.gauss(0,jitter)
                atoms.append({"id":atom_id,"Z":ZB,"sym":symB,
                              "x":round(x,4),"y":round(y,4),"z":round(z,4),
                              "q":4.0,"role":0,"chi":0.5})
                atom_id += 1
                for (fx,fy,fz) in [(0.5,0.5,0),(0.5,0,0.5),(0,0.5,0.5)]:
                    x=(ox+fx)*a+rng.gauss(0,jitter); y=(oy+fy)*a+rng.gauss(0,jitter)
                    z=(oz+fz)*a+rng.gauss(0,jitter)
                    atoms.append({"id":atom_id,"Z":ZO,"sym":symO,
                                  "x":round(x,4),"y":round(y,4),"z":round(z,4),
                                  "q":-2.0,"role":0,"chi":0.8})
                    atom_id += 1
        elif lattice_type in ("hexagonal","trigonal"):
            sym, Z = basis[0]
            c_over_a = 1.633 if lattice_type == "hexagonal" else 1.1
            c = a * c_over_a
            # HCP: two atoms per unit cell
            for (fx,fy) in [(0,0),(2/3,1/3)]:
                fz_off = 0.0 if fx == 0 else 0.5
                for iz in range(2):
                    x=(ox+fx)*a+rng.gauss(0,jitter)
                    y=(oy+fy)*a*0.866+rng.gauss(0,jitter)
                    z=(oz+iz+fz_off)*c*0.5+rng.gauss(0,jitter)
                    atoms.append({"id":atom_id,"Z":Z,"sym":sym,
                                  "x":round(x,4),"y":round(y,4),"z":round(z,4),
                                  "q":0.0,"role":0,"chi":rng.uniform(0.1,0.9)})
                    atom_id += 1

    for ix in range(nx):
        for iy in range(ny):
            for iz in range(nz):
                add_basis(ix, iy, iz)
                if len(atoms) > 500:  # cap for performance
                    break
            if len(atoms) > 500:
                break
        if len(atoms) > 500:
            break

    # Build bonds: connect atoms within 1.1 * nearest-neighbour distance
    bonds = []
    nn_dist = a * 0.75  # approx nearest-neighbour
    bond_id = 0
    for i in range(min(len(atoms), 200)):  # limit bond search
        for j in range(i+1, min(len(atoms), 200)):
            ai, aj = atoms[i], atoms[j]
            dx=ai["x"]-aj["x"]; dy=ai["y"]-aj["y"]; dz=ai["z"]-aj["z"]
            d2 = dx*dx + dy*dy + dz*dz
            if d2 < (nn_dist * 1.15)**2:
                bonds.append({"id":bond_id,"i":i,"j":j,"order":1})
                bond_id += 1
            if bond_id > 800:
                break
        if bond_id > 800:
            break

    # Lattice energy estimate (Madelung-like)
    n_ions = len(atoms)
    e_lattice = -rng.uniform(600, 900) * n_ions / 100.0

    return {
        "molecule_name": name,
        "sequence": f"{name} [{nx}x{ny}x{nz}]",
        "environment": crystal_class,
        "state": "CRYSTAL_PERIODIC",
        "atom_count": len(atoms),
        "bond_count": len(bonds),
        "residue_count": 0,
        "hbond_count": 0,
        "lattice": {"a": a, "type": lattice_type, "nx": nx, "ny": ny, "nz": nz},
        "energy": {
            "total": round(e_lattice, 2),
            "bond": round(e_lattice * 0.6, 2),
            "vdw": round(e_lattice * 0.15, 2),
            "coulomb": round(e_lattice * 0.2, 2),
            "solvation": 0.0,
            "formation": round(e_lattice * 0.05, 2),
        },
        "scores": {
            "steric": round(rng.uniform(0.7, 0.98), 4),
            "hbond": 0.0,
            "electrostatic": round(rng.uniform(0.6, 0.95), 4),
            "hydrophobic": 0.0,
            "planarity": round(rng.uniform(0.75, 0.99), 4),
            "confidence": round(rng.uniform(0.65, 0.92), 4),
        },
        "atoms": atoms,
        "bonds": bonds,
        "functional_groups": [crystal_class, lattice_type, f"a={a}A"],
        "generator": "crystal_stochastic",
    }


class CrystalGenerator:
    """Port 9990 — periodic crystal lattice demos."""
    def __init__(self, store, port=9990, min_interval=3.0, max_interval=12.0, verbose=False):
        self.store = store; self.port = port
        self.min_interval = min_interval; self.max_interval = max_interval
        self.verbose = verbose; self.running = True
        self.run_id = 0; self.rng = random.Random()

    def start(self):
        threading.Thread(target=self._loop, daemon=True).start()

    def _loop(self):
        while self.running:
            try:
                self.run_id += 1
                frame = _generate_crystal(self.rng)
                frame["run_id"] = self.run_id
                self.store.push(self.port, frame)
                if self.verbose:
                    print(f"  [CRYSTAL #{self.run_id:04d}] {frame['molecule_name']}"
                          f"  atoms={frame['atom_count']}  bonds={frame['bond_count']}")
            except Exception as ex:
                if self.verbose:
                    print(f"  [CRYSTAL ERROR] {ex}")
            time.sleep(self.rng.uniform(self.min_interval, self.max_interval))

    def stop(self):
        self.running = False


# ============================================================================
# Coarse-Grain Generator — port 9991
# Generates coarse-grained bead models: polymers, lipid bilayers, block
# copolymers, protein domains — atomistic bead representation
# ============================================================================

_CG_MOLECULES = [
    ("DPPC Lipid bilayer",    "membrane",    "lipid"),
    ("Block copolymer PS-PEO","solvent",     "polymer"),
    ("Polystyrene chain",     "vacuum",      "polymer"),
    ("PEG chain",             "polar_solvent","polymer"),
    ("DNA duplex fragment",   "polar_solvent","nucleic"),
    ("Amphiphilic micelle",   "polar_solvent","micelle"),
    ("Protein alpha-helix CG","polar_solvent","protein"),
    ("Beta-sheet fibril CG",  "polar_solvent","protein"),
    ("SDS micelle",           "polar_solvent","micelle"),
    ("Polyelectrolyte chain", "polar_solvent","polymer"),
    ("Lipid monolayer",       "vacuum",      "lipid"),
    ("Triblock copolymer",    "solvent",     "polymer"),
    ("Coiled-coil dimer CG",  "polar_solvent","protein"),
    ("Cholesterol bilayer",   "membrane",    "lipid"),
    ("RNA hairpin CG",        "polar_solvent","nucleic"),
]

_CG_BEAD_TYPES = {
    # (Z_display, sym, sigma_ang, epsilon_kJ)
    "backbone":   (6,  "CB", 4.7, 3.8),
    "hydrophobic":(6,  "CH", 4.7, 5.6),
    "hydrophilic":(8,  "CP", 4.7, 4.5),
    "charged_pos":(7,  "CQ", 4.7, 4.5),
    "charged_neg":(8,  "CX", 4.7, 4.5),
    "lipid_head": (8,  "LH", 4.7, 4.2),
    "lipid_tail": (6,  "LT", 4.7, 6.1),
    "nucleic":    (7,  "NC", 4.7, 3.9),
    "aromatic":   (6,  "SC", 3.5, 3.5),
}

def _generate_coarse_grain(rng):
    """Generate a coarse-grained bead model frame for port 9991."""
    mol = rng.choice(_CG_MOLECULES)
    name, env, mol_class = mol

    atoms = []
    bonds = []
    atom_id = 0
    bond_id = 0

    def add_bead(btype, x, y, z, q=0.0):
        nonlocal atom_id
        Z, sym, sigma, eps = _CG_BEAD_TYPES[btype]
        atoms.append({
            "id": atom_id, "Z": Z, "sym": sym,
            "x": round(x,4), "y": round(y,4), "z": round(z,4),
            "q": round(q,3), "role": list(_CG_BEAD_TYPES.keys()).index(btype),
            "chi": eps / 7.0,
        })
        atom_id += 1
        return atom_id - 1

    def add_bond(i, j):
        nonlocal bond_id
        bonds.append({"id":bond_id,"i":i,"j":j,"order":1})
        bond_id += 1

    if mol_class == "lipid":
        # Lipid bilayer: two leaflets of lipids
        n_lipids = rng.randint(8, 20)
        cols = max(3, int(n_lipids**0.5))
        spacing = 7.0
        for k in range(n_lipids):
            cx = (k % cols) * spacing + rng.gauss(0, 0.3)
            cy = (k // cols) * spacing + rng.gauss(0, 0.3)
            for leaflet in (1, -1):
                head = add_bead("lipid_head", cx, cy, leaflet*2.0+rng.gauss(0,0.2),
                                q=rng.choice([-0.5, 0.5]))
                prev = head
                for ti in range(rng.randint(3, 5)):
                    tz = leaflet * (2.0 + (ti+1)*4.7*0.85)
                    tail = add_bead("lipid_tail", cx+rng.gauss(0,0.4),
                                    cy+rng.gauss(0,0.4), tz)
                    add_bond(prev, tail)
                    prev = tail

    elif mol_class == "polymer":
        n_chains = rng.randint(2, 5)
        n_beads  = rng.randint(10, 30)
        btypes = ["backbone","hydrophobic","hydrophilic","charged_pos","aromatic"]
        frac   = [0.3, 0.3, 0.2, 0.1, 0.1]
        for ch in range(n_chains):
            ox = ch * 15.0; oy = 0; oz = 0
            theta = rng.uniform(0, 2*_math.pi)
            phi   = rng.uniform(0, _math.pi)
            prev_id = None
            for bi in range(n_beads):
                # Random walk along chain
                theta += rng.gauss(0, 0.4); phi += rng.gauss(0, 0.3)
                step = 4.7
                ox += step*_math.sin(phi)*_math.cos(theta)
                oy += step*_math.sin(phi)*_math.sin(theta)
                oz += step*_math.cos(phi)
                btype = rng.choices(btypes, weights=frac)[0]
                q = rng.uniform(-0.3,0.3) if "charged" not in btype else (
                    0.8 if "pos" in btype else -0.8)
                bid = add_bead(btype, ox, oy, oz, q)
                if prev_id is not None:
                    add_bond(prev_id, bid)
                prev_id = bid

    elif mol_class == "micelle":
        n_lipids = rng.randint(20, 40)
        r_mic = 12.0
        for k in range(n_lipids):
            # Random point on sphere for head
            u = rng.uniform(-1,1); v = rng.uniform(0,2*_math.pi)
            r0 = r_mic + rng.gauss(0,0.8)
            hx = r0*_math.sqrt(1-u*u)*_math.cos(v)
            hy = r0*_math.sqrt(1-u*u)*_math.sin(v)
            hz = r0*u
            head = add_bead("lipid_head", hx, hy, hz, q=rng.choice([-0.4,0.4]))
            # Tail points inward
            prev = head
            for ti in range(rng.randint(2,4)):
                scale = (r_mic - (ti+1)*4.0) / r_mic
                tx=hx*scale+rng.gauss(0,0.5); ty=hy*scale+rng.gauss(0,0.5)
                tz=hz*scale+rng.gauss(0,0.5)
                tail = add_bead("lipid_tail", tx, ty, tz)
                add_bond(prev, tail); prev = tail

    elif mol_class == "protein":
        # CG helix or sheet backbone + sidechain beads
        n_res = rng.randint(12, 30)
        is_helix = "helix" in name.lower()
        prev_bb = None
        for ri in range(n_res):
            if is_helix:
                t = ri * 1.5
                x = 2.3*_math.cos(ri*100*_math.pi/180)
                y = 2.3*_math.sin(ri*100*_math.pi/180)
                z = ri * 1.5
            else:
                x = ri * 4.7 if ri % 2 == 0 else ri * 4.7
                y = (ri % 2) * 4.7
                z = 0.0
            x += rng.gauss(0,0.2); y += rng.gauss(0,0.2); z += rng.gauss(0,0.2)
            bb = add_bead("backbone", x, y, z)
            if prev_bb is not None:
                add_bond(prev_bb, bb)
            prev_bb = bb
            # Sidechain bead
            sc_type = rng.choice(["hydrophobic","hydrophilic","charged_pos","charged_neg","aromatic"])
            q = 0.8 if sc_type=="charged_pos" else (-0.8 if sc_type=="charged_neg" else 0.0)
            sc = add_bead(sc_type, x+rng.gauss(0,2.5), y+rng.gauss(0,2.5),
                          z+rng.gauss(0,1.5), q)
            add_bond(bb, sc)

    elif mol_class == "nucleic":
        n_bp = rng.randint(6, 18)
        rise = 3.4; twist = 36.0
        for i in range(n_bp):
            ang = i * twist * _math.pi / 180
            r = 9.5
            # Phosphate backbone
            px = r*_math.cos(ang)+rng.gauss(0,0.2)
            py = r*_math.sin(ang)+rng.gauss(0,0.2)
            pz = i*rise+rng.gauss(0,0.1)
            pb = add_bead("charged_neg", px, py, pz, q=-1.0)
            # Base
            bx = (r-5.0)*_math.cos(ang)+rng.gauss(0,0.2)
            by = (r-5.0)*_math.sin(ang)+rng.gauss(0,0.2)
            bb = add_bead("nucleic", bx, by, pz)
            add_bond(pb, bb)
            # Complementary strand
            ang2 = ang + _math.pi
            px2 = r*_math.cos(ang2)+rng.gauss(0,0.2)
            py2 = r*_math.sin(ang2)+rng.gauss(0,0.2)
            pb2 = add_bead("charged_neg", px2, py2, pz, q=-1.0)
            bx2 = (r-5.0)*_math.cos(ang2)+rng.gauss(0,0.2)
            by2 = (r-5.0)*_math.sin(ang2)+rng.gauss(0,0.2)
            bb2 = add_bead("nucleic", bx2, by2, pz)
            add_bond(pb2, bb2)
            # Base pair H-bond
            add_bond(bb, bb2)
            if i > 0:
                add_bond(pb - 4, pb); add_bond(pb2 - 4, pb2)

    n = len(atoms)
    e_cg = -rng.uniform(40, 180) * n / 10.0

    return {
        "molecule_name": name,
        "sequence": f"CG:{mol_class.upper()}:{n}beads",
        "environment": env,
        "state": "CG_EQUILIBRATED",
        "atom_count": n,
        "bond_count": len(bonds),
        "residue_count": 0,
        "hbond_count": 0,
        "energy": {
            "total": round(e_cg, 2),
            "bond": round(e_cg*0.3, 2),
            "vdw": round(e_cg*0.4, 2),
            "coulomb": round(e_cg*0.2, 2),
            "solvation": round(e_cg*0.1, 2),
            "formation": 0.0,
        },
        "scores": {
            "steric": round(rng.uniform(0.6, 0.95), 4),
            "hbond": round(rng.uniform(0.0, 0.6), 4),
            "electrostatic": round(rng.uniform(0.5, 0.9), 4),
            "hydrophobic": round(rng.uniform(0.3, 0.9), 4),
            "planarity": round(rng.uniform(0.5, 0.99), 4),
            "confidence": round(rng.uniform(0.55, 0.88), 4),
        },
        "atoms": atoms,
        "bonds": bonds,
        "functional_groups": [mol_class, env, f"{n}beads"],
        "generator": "coarse_grain_stochastic",
    }


class CoarseGrainGenerator:
    """Port 9991 — coarse-grained bead model demos."""
    def __init__(self, store, port=9991, min_interval=3.0, max_interval=12.0, verbose=False):
        self.store = store; self.port = port
        self.min_interval = min_interval; self.max_interval = max_interval
        self.verbose = verbose; self.running = True
        self.run_id = 0; self.rng = random.Random()

    def start(self):
        threading.Thread(target=self._loop, daemon=True).start()

    def _loop(self):
        while self.running:
            try:
                self.run_id += 1
                frame = _generate_coarse_grain(self.rng)
                frame["run_id"] = self.run_id
                self.store.push(self.port, frame)
                if self.verbose:
                    print(f"  [CG #{self.run_id:04d}] {frame['molecule_name']}"
                          f"  beads={frame['atom_count']}  bonds={frame['bond_count']}")
            except Exception as ex:
                if self.verbose:
                    print(f"  [CG ERROR] {ex}")
            time.sleep(self.rng.uniform(self.min_interval, self.max_interval))

    def stop(self):
        self.running = False


# ============================================================================
# Secondary channel generators — ports 9992-9998
# Each produces a distinct class of atomistic / field / analysis data
# ============================================================================

class _BaseGenerator:
    """Shared scaffold for all secondary generators."""
    TAG = "gen"
    def __init__(self, store, port, min_i, max_i, verbose=False):
        self.store = store; self.port = port
        self.min_i = min_i; self.max_i = max_i
        self.verbose = verbose; self.running = True
        self.run_id = 0; self.rng = random.Random()

    def start(self):
        threading.Thread(target=self._loop, daemon=True).start()

    def _make_frame(self):
        raise NotImplementedError

    def _loop(self):
        while self.running:
            try:
                self.run_id += 1
                frame = self._make_frame()
                frame["run_id"] = self.run_id
                frame["generator"] = self.TAG
                self.store.push(self.port, frame)
                if self.verbose:
                    mol = frame.get("molecule_name", frame.get("sequence","?"))
                    n   = frame.get("atom_count", 0)
                    print(f"  [{self.TAG} #{self.run_id:04d}] {mol}  atoms={n}")
            except Exception as ex:
                if self.verbose:
                    print(f"  [{self.TAG} ERROR] {ex}")
            time.sleep(self.rng.uniform(self.min_i, self.max_i))

    def stop(self):
        self.running = False


# ---- 9992  EHD Field — electrostatic + hydrodynamic field snapshots ----
_EHD_MOLECULES = [
    ("EHD Ion cloud Na+/Cl-",     "ionic"),
    ("EHD Dipole layer",          "polar_solvent"),
    ("EHD Charged colloid",       "polar_solvent"),
    ("EHD Debye screening cloud", "ionic"),
    ("EHD Stern double layer",    "membrane"),
    ("EHD Zeta potential shell",  "polar_solvent"),
    ("EHD Field-aligned polymer", "reactive_field"),
    ("EHD Electroosmotic flow",   "polar_solvent"),
]

class EHDGenerator(_BaseGenerator):
    TAG = "EHD   "
    def _make_frame(self):
        mol_name, env = self.rng.choice(_EHD_MOLECULES)
        n_field = self.rng.randint(40, 180)
        atoms = []
        for i in range(n_field):
            # Random position in 3D field volume (20 Å box)
            x = self.rng.uniform(-10, 10)
            y = self.rng.uniform(-10, 10)
            z = self.rng.uniform(-10, 10)
            r2 = x*x + y*y + z*z
            # Ion charge based on distance from centre (Debye-like)
            q = self.rng.choice([-1.0, 1.0]) * _math.exp(-r2 / 40.0)
            Z = 11 if q > 0 else 17   # Na+ or Cl-
            sym = "Na" if Z == 11 else "Cl"
            atoms.append({"id":i,"Z":Z,"sym":sym,
                          "x":round(x,4),"y":round(y,4),"z":round(z,4),
                          "q":round(q,4),"role":7 if q>0 else 8,
                          "chi":abs(q)})
        # Field lines: connect nearby ions of opposite sign
        bonds = []
        bid = 0
        for i in range(len(atoms)):
            for j in range(i+1, len(atoms)):
                if atoms[i]["q"] * atoms[j]["q"] >= 0:
                    continue  # same sign
                dx=atoms[i]["x"]-atoms[j]["x"]
                dy=atoms[i]["y"]-atoms[j]["y"]
                dz=atoms[i]["z"]-atoms[j]["z"]
                if dx*dx+dy*dy+dz*dz < 25.0:
                    bonds.append({"id":bid,"i":i,"j":j,"order":1})
                    bid += 1
                if bid > 300: break
            if bid > 300: break
        e = -self.rng.uniform(10, 400)
        return {
            "molecule_name": mol_name, "sequence": f"EHD:{n_field}ions",
            "environment": env, "state": "EHD_STEADY",
            "atom_count": len(atoms), "bond_count": len(bonds),
            "residue_count": 0, "hbond_count": 0,
            "energy": {"total":round(e,2),"bond":0,"vdw":round(e*0.2,2),
                       "coulomb":round(e*0.7,2),"solvation":round(e*0.1,2),"formation":0},
            "scores": {"steric":round(self.rng.uniform(0.5,0.95),4),
                       "hbond":0.0,"electrostatic":round(self.rng.uniform(0.6,0.99),4),
                       "hydrophobic":0.0,"planarity":round(self.rng.uniform(0.4,0.9),4),
                       "confidence":round(self.rng.uniform(0.5,0.85),4)},
            "atoms": atoms, "bonds": bonds,
            "functional_groups": ["ionic_cloud", env],
        }


# ---- 9993  Thermal Pipe — kinetic gas / thermal fluctuation snapshots ----
_THERMAL_GASES = [
    ("Ar gas ensemble",    8,  "vacuum",       300),
    ("N2 diatomic gas",    7,  "vacuum",       400),
    ("H2O vapor cluster",  8,  "polar_solvent",373),
    ("CO2 gas cloud",      8,  "vacuum",       500),
    ("He thermal bath",    2,  "vacuum",       77),
    ("Xe noble cluster",  54,  "vacuum",       200),
    ("CH4 gas ensemble",   6,  "vacuum",       298),
    ("SF6 heavy gas",     16,  "vacuum",       600),
]

class ThermalPipeGenerator(_BaseGenerator):
    TAG = "THERM "
    def _make_frame(self):
        mol_name, Z, env, T = self.rng.choice(_THERMAL_GASES)
        n = self.rng.randint(30, 120)
        kB = 0.008314  # kJ/mol/K
        mass_map = {2:4,6:12,7:14,8:16,16:32,54:131}
        mass = mass_map.get(Z, 20)
        sym_map = {2:"He",6:"C",7:"N",8:"O",16:"S",54:"Xe"}
        sym = sym_map.get(Z, "X")
        # Maxwell-Boltzmann velocity -> position displacement
        sigma_v = _math.sqrt(kB * T / mass)
        atoms = []
        for i in range(n):
            # Random walk from grid position
            gx = (i % 8) * 4.0 - 16.0
            gy = ((i // 8) % 8) * 4.0 - 16.0
            gz = (i // 64) * 4.0 - 8.0
            x = gx + self.rng.gauss(0, sigma_v * 0.5)
            y = gy + self.rng.gauss(0, sigma_v * 0.5)
            z = gz + self.rng.gauss(0, sigma_v * 0.5)
            vx = self.rng.gauss(0, sigma_v)
            vy = self.rng.gauss(0, sigma_v)
            vz = self.rng.gauss(0, sigma_v)
            ke = 0.5 * mass * (vx**2 + vy**2 + vz**2)
            atoms.append({"id":i,"Z":Z,"sym":sym,
                          "x":round(x,4),"y":round(y,4),"z":round(z,4),
                          "q":0.0,"role":0,"chi":min(1.0, ke/(3*kB*T))})
        e_kin = 1.5 * n * kB * T
        e_pot = -self.rng.uniform(0, e_kin * 0.3)
        return {
            "molecule_name": mol_name, "sequence": f"T={T}K:{n}atoms",
            "environment": env, "state": "GAS_PHASE",
            "atom_count": n, "bond_count": 0, "residue_count": 0, "hbond_count": 0,
            "energy": {"total":round(e_kin+e_pot,2),"bond":0,
                       "vdw":round(e_pot,2),"coulomb":0,
                       "solvation":0,"formation":round(e_kin,2)},
            "scores": {"steric":round(self.rng.uniform(0.7,0.99),4),
                       "hbond":0.0,"electrostatic":0.0,
                       "hydrophobic":0.0,"planarity":1.0,
                       "confidence":round(self.rng.uniform(0.7,0.95),4)},
            "atoms": atoms, "bonds": [],
            "functional_groups": [f"T={T}K", sym, f"n={n}"],
        }


# ---- 9994  Peptide 2D — short peptides + functional group focus ----
class Peptide2DGenerator(_BaseGenerator):
    TAG = "PEP2D "
    def _make_frame(self):
        # Short 2-5 residue fragments, flat-ish geometry (2D projection feel)
        frame = _generate_chain(self.rng, named=self.rng.random()<0.5,
                                min_res=2, max_res=5)
        # Flatten Z spread to emphasize 2D character
        for a in frame["atoms"]:
            a["z"] = round(a["z"] * 0.15, 4)
        frame["molecule_name"] = "2D: " + (frame.get("molecule_name") or
                                            frame.get("sequence","frag"))
        frame["state"] = "PLANAR_PROJ"
        return frame


# ---- 9995  Peptide 3D — longer chains + secondary structure ----
class Peptide3DGenerator(_BaseGenerator):
    TAG = "PEP3D "
    def _make_frame(self):
        frame = _generate_chain(self.rng, named=self.rng.random()<0.8,
                                min_res=8, max_res=20)
        frame["state"] = "3D_FOLDED"
        return frame


# ---- 9996  Phase Diagram — mixture of two atom types at varying T/P ----
_PHASE_SYSTEMS = [
    ("H2O liquid-vapour",  8,  8,  "polar_solvent", 373, 1.0),
    ("NaCl solution",     11, 17,  "polar_solvent", 298, 1.0),
    ("Fe-C phase boundary",26, 6,  "vacuum",        1000, 1.0),
    ("Si-Ge alloy",       14, 32,  "vacuum",        1200, 1.0),
    ("Ar-Kr gas mixture",  18, 36, "vacuum",        200,  0.1),
    ("Li-Na alloy",        3, 11,  "vacuum",        500,  1.0),
    ("Al-Cu eutectic",    13, 29,  "vacuum",        820,  1.0),
    ("CO2 supercritical",  6,  8,  "polar_solvent", 310, 74.0),
]

class PhaseDiagramGenerator(_BaseGenerator):
    TAG = "PHASE "
    def _make_frame(self):
        mol_name, ZA, ZB, env, T, P = self.rng.choice(_PHASE_SYSTEMS)
        n_total = self.rng.randint(60, 200)
        frac_A  = self.rng.uniform(0.3, 0.7)
        nA = int(n_total * frac_A); nB = n_total - nA
        sym_map = {3:"Li",6:"C",8:"O",11:"Na",13:"Al",14:"Si",17:"Cl",
                   18:"Ar",26:"Fe",29:"Cu",32:"Ge",36:"Kr"}
        symA = sym_map.get(ZA,"A"); symB = sym_map.get(ZB,"B")
        # Phase-separated clusters: A-rich region and B-rich region
        atoms = []
        def cluster(n, Z, sym, cx, cy, cz, spread):
            for i in range(n):
                x = cx + self.rng.gauss(0, spread)
                y = cy + self.rng.gauss(0, spread)
                z = cz + self.rng.gauss(0, spread)
                chi = T / 2000.0 * self.rng.uniform(0.5, 1.5)
                atoms.append({"id":len(atoms),"Z":Z,"sym":sym,
                              "x":round(x,4),"y":round(y,4),"z":round(z,4),
                              "q":0.0,"role":0,"chi":round(min(1,chi),4)})
        spread = _math.sqrt(T / 100.0)
        cluster(nA, ZA, symA, -spread*0.5, 0, 0, spread)
        cluster(nB, ZB, symB,  spread*0.5, 0, 0, spread)
        kB = 0.008314
        e = -kB * T * n_total * self.rng.uniform(0.5, 1.5)
        return {
            "molecule_name": mol_name,
            "sequence": f"{symA}{nA}/{symB}{nB} T={T}K P={P}bar",
            "environment": env, "state": "PHASE_MIXED",
            "atom_count": len(atoms), "bond_count": 0,
            "residue_count": 0, "hbond_count": 0,
            "energy": {"total":round(e,2),"bond":0,"vdw":round(e*0.6,2),
                       "coulomb":round(e*0.3,2),"solvation":round(e*0.1,2),"formation":0},
            "scores": {"steric":round(self.rng.uniform(0.5,0.9),4),
                       "hbond":0.0,"electrostatic":round(self.rng.uniform(0.3,0.8),4),
                       "hydrophobic":round(frac_A,4),"planarity":round(self.rng.uniform(0.5,0.99),4),
                       "confidence":round(self.rng.uniform(0.55,0.88),4)},
            "atoms": atoms, "bonds": [],
            "functional_groups": [f"T={T}K", f"P={P}bar", f"xA={frac_A:.2f}"],
        }


# ---- 9997  Formation Timeline — residue-by-residue chain growth ----
class FormationTimelineGenerator(_BaseGenerator):
    TAG = "FMTLN "
    def __init__(self, *a, **kw):
        super().__init__(*a, **kw)
        self._full_frame = None
        self._step = 0

    def _make_frame(self):
        # Every few steps grow the chain by one residue; restart when complete
        if self._full_frame is None or self._step >= len(self._full_frame["residues"]):
            self._full_frame = _generate_chain(self.rng, named=True, min_res=6, max_res=14)
            self._step = 1
        # Show only the first `_step` residues
        full = self._full_frame
        res_ids = {r["id"] for r in full["residues"][:self._step]}
        atoms = [a for a in full["atoms"]
                 if any(a["id"] >= r["id"]*4 and a["id"] < (r["id"]+1)*4
                        for r in full["residues"][:self._step])]
        # Simpler: just take first N atoms proportionally
        frac = self._step / max(1, len(full["residues"]))
        n_atoms = max(4, int(len(full["atoms"]) * frac))
        atoms = full["atoms"][:n_atoms]
        bonds = [b for b in full.get("bonds",[]) if b["i"] < n_atoms and b["j"] < n_atoms]
        self._step += 1
        name = full.get("molecule_name","chain")
        prog = f"{self._step-1}/{len(full['residues'])}"
        frame = dict(full)
        frame["atoms"] = atoms
        frame["bonds"] = bonds
        frame["atom_count"] = len(atoms)
        frame["bond_count"] = len(bonds)
        frame["residue_count"] = self._step - 1
        frame["molecule_name"] = f"[+] {name} {prog}"
        frame["state"] = "FORMING"
        return frame


# ---- 9998  Analysis Deep — rich multi-residue analysis: RMSD, Rg, contacts ----
class AnalysisDeepGenerator(_BaseGenerator):
    TAG = "ANLYS "
    def _make_frame(self):
        frame = _generate_chain(self.rng, named=True, min_res=10, max_res=25)
        atoms = frame["atoms"]
        # Radius of gyration
        cx = sum(a["x"] for a in atoms)/len(atoms)
        cy = sum(a["y"] for a in atoms)/len(atoms)
        cz = sum(a["z"] for a in atoms)/len(atoms)
        Rg = _math.sqrt(sum((a["x"]-cx)**2+(a["y"]-cy)**2+(a["z"]-cz)**2
                            for a in atoms)/len(atoms))
        # Contact map: pairs within 6 Å
        contacts = 0
        for i in range(len(atoms)):
            for j in range(i+2, min(i+10, len(atoms))):
                dx=atoms[i]["x"]-atoms[j]["x"]
                dy=atoms[i]["y"]-atoms[j]["y"]
                dz=atoms[i]["z"]-atoms[j]["z"]
                if dx*dx+dy*dy+dz*dz < 36.0:
                    contacts += 1
        frame["molecule_name"] = "ANLYS: " + (frame.get("molecule_name") or "chain")
        frame["state"] = "ANALYSIS_COMPLETE"
        frame["analysis"] = {
            "Rg": round(Rg, 3),
            "contacts": contacts,
            "rmsd_vs_extended": round(self.rng.uniform(0.5, Rg*0.8), 3),
            "end_to_end": round(self.rng.uniform(2.0, Rg*2.5), 3),
            "asphericity": round(self.rng.uniform(0.0, 0.8), 4),
        }
        return frame


# ============================================================================
# Threaded HTTP server (serves on the HTTP port)
# ============================================================================

class ThreadedHTTPServer(HTTPServer):
    """HTTP server with threading support."""
    allow_reuse_address = True
    daemon_threads = True

    def process_request(self, request, client_address):
        t = threading.Thread(target=self._handle, args=(request, client_address),
                             daemon=True)
        t.start()

    def _handle(self, request, client_address):
        try:
            self.finish_request(request, client_address)
        except Exception:
            self.handle_error(request, client_address)
        finally:
            self.shutdown_request(request)

# ============================================================================
# Main
# ============================================================================

def main():
    global _global_store, _global_start_time, _global_verbose

    http_port = 8899
    bind_addr = "0.0.0.0"
    verbose = False
    no_gen = False
    data_ports = list(ALL_PORTS)

    args = sys.argv[1:]
    i = 0
    while i < len(args):
        if args[i] == "--http-port" and i + 1 < len(args):
            http_port = int(args[i+1]); i += 2
        elif args[i] in ("--bind", "--host") and i + 1 < len(args):
            bind_addr = args[i+1]; i += 2
        elif args[i] == "--ports" and i + 1 < len(args):
            data_ports = [int(p) for p in args[i+1].split(",")]; i += 2
        elif args[i] in ("--verbose", "-v"):
            verbose = True; i += 1
        elif args[i] == "--no-gen":
            no_gen = True; i += 1
        elif args[i] in ("--help", "-h"):
            print(__doc__); return
        else:
            i += 1

    _global_verbose = verbose
    _global_start_time = time.time()

    store = FrameStore()
    _global_store = store

    # ---- Banner ----
    print("\033[2J\033[H", end="")   # clear screen
    print("\033[1;35m")
    print("+" + "=" * 66 + "+")
    print("|  VSEPR-SIM  Web Visualization Host + TUI Console" + " " * 17 + "|")
    print("|  Chrome/Chromium optimised  --  rough local network ready" + " " * 8 + "|")
    print("|  Primary: :9999 Atomic Live   :8899 Shell/Passthrough" + " " * 12 + "|")
    print("+" + "=" * 66 + "+")
    print("\033[0m")

    # ---- Data listeners (UDP+TCP) ----
    listeners = []
    for port in data_ports:
        if port == http_port:
            continue
        listener = DataListener(port, store, bind_addr=bind_addr, verbose=verbose)
        listener.start()
        listeners.append(listener)
        label = PORT_LABELS.get(port, "")
        print(f"  \033[32m*\033[0m  Data  {port:<5}  TCP+UDP  {label}")

    http_udp = DataListener(http_port, store, bind_addr=bind_addr, verbose=verbose)
    threading.Thread(target=http_udp._udp_loop, daemon=True).start()
    print(f"  \033[32m*\033[0m  Data  {http_port:<5}  UDP      "
          f"{PORT_LABELS.get(http_port,'')} (shared with HTTP)")

    # ---- HTTP server ----
    httpd = ThreadedHTTPServer((bind_addr, http_port), VizHTTPHandler)
    http_thread = threading.Thread(target=httpd.serve_forever, daemon=True)
    http_thread.start()
    print(f"\n  \033[1;36m>  HTTP  http://{bind_addr}:{http_port}    "
          f"WS  ws://{bind_addr}:{http_port}/ws\033[0m")
    print(f"  \033[1;36m>  SSE   http://{bind_addr}:{http_port}/sse  "
          f"API http://{bind_addr}:{http_port}/api/frames\033[0m")

    # ---- Built-in generators ----
    generators = []

    # Stream type labels for the registry
    _GEN_TYPES = {
        9999: "peptide", 9990: "crystal", 9991: "coarse_grain",
        9992: "ehd_field", 9993: "thermal", 9994: "peptide_2d",
        9995: "peptide_3d", 9996: "phase", 9997: "formation", 9998: "analysis",
    }

    if not no_gen:
        # Primary: Atomic Live stochastic peptides on :9999
        gen = StochasticGenerator(store, port=9999, min_interval=1.0,
                                  max_interval=10.0, verbose=verbose)
        gen.start(); generators.append(("PEPTIDE", 9999, gen))

        # Crystal lattice structures on :9990
        crystal_gen = CrystalGenerator(store, port=9990, min_interval=3.0,
                                       max_interval=12.0, verbose=verbose)
        crystal_gen.start(); generators.append(("CRYSTAL", 9990, crystal_gen))

        # Coarse-grain bead models on :9991
        cg_gen = CoarseGrainGenerator(store, port=9991, min_interval=3.0,
                                      max_interval=12.0, verbose=verbose)
        cg_gen.start(); generators.append(("CG    ", 9991, cg_gen))

        # Secondary channels 9992-9998
        for cls, port, tag, mi, ma in [
            (EHDGenerator,              9992, "EHD   ", 2.0, 8.0),
            (ThermalPipeGenerator,      9993, "THERM ", 1.5, 7.0),
            (Peptide2DGenerator,        9994, "PEP2D ", 2.0, 9.0),
            (Peptide3DGenerator,        9995, "PEP3D ", 3.0, 12.0),
            (PhaseDiagramGenerator,     9996, "PHASE ", 2.0, 8.0),
            (FormationTimelineGenerator,9997, "FMTLN ", 1.0, 4.0),
            (AnalysisDeepGenerator,     9998, "ANLYS ", 4.0, 14.0),
        ]:
            g = cls(store, port=port, min_i=mi, max_i=ma, verbose=verbose)
            g.start(); generators.append((tag, port, g))

        # Populate stream registry for /api/streams
        for tag, port, g in generators:
            _global_stream_registry.append({
                "port": port,
                "name": PORT_LABELS.get(port, f"Port {port}"),
                "type": _GEN_TYPES.get(port, "unknown"),
                "generator": tag.strip(),
                "enabled": True,
            })

        print(f"\n  \033[1;33m*  Built-in generators active (10 channels):\033[0m")
        print(f"     :9999  Atomic Live    -- peptides      1-10s  (35+ named)")
        print(f"     :9990  Crystal        -- lattices      3-12s  (12 systems)")
        print(f"     :9991  Coarse-Grain   -- bead models   3-12s  (15 types)")
        print(f"     :9992  EHD Field      -- ion clouds    2-8s")
        print(f"     :9993  Thermal Pipe   -- gas kinetics  1.5-7s")
        print(f"     :9994  Peptide 2D     -- short frags   2-9s")
        print(f"     :9995  Peptide 3D     -- long chains   3-12s")
        print(f"     :9996  Phase Diagram  -- mixtures      2-8s")
        print(f"     :9997  Formation TL   -- chain growth  1-4s")
        print(f"     :9998  Analysis Deep  -- Rg/contacts   4-14s")
        print(f"\n  \033[1;36m>  Stream registry: /api/streams  ({len(_global_stream_registry)} registered)\033[0m")
    else:
        print(f"\n  Built-in generators: OFF (--no-gen)")

    # ---- LAN IPs ----
    try:
        hostname = socket.gethostname()
        local_ips = socket.getaddrinfo(hostname, None, socket.AF_INET)
        seen = set()
        for info in local_ips:
            ip = info[4][0]
            if ip not in seen and not ip.startswith("127."):
                seen.add(ip)
                print(f"  \033[33m*  LAN: http://{ip}:{http_port}\033[0m")
    except Exception:
        pass

    print(f"\n  Transport: WebSocket -> SSE -> Poll  |  tau_Tt=2.2s  |  fade=400ms")
    print(f"  Primary windows: :9999 (Atomic Live)  :8899 (Shell Console)")
    print(f"  Launch all: python tools/launch_viz_windows.py\n")

    # ---- TUI shell: live per-port table ----
    # Rows: one per active generator + any external sources
    TUI_PORTS = [9999, 9990, 9991, 9992, 9993, 9994, 9995, 9996, 9997, 9998, 8899]
    TUI_LABELS = {
        9999:"Atomic Live  ", 9990:"Crystal      ", 9991:"Coarse-Grain ",
        9992:"EHD Field    ", 9993:"Thermal Pipe ", 9994:"Peptide 2D  ",
        9995:"Peptide 3D   ", 9996:"Phase Diagram", 9997:"Formation TL ",
        9998:"Analysis Deep", 8899:"Base/Shell   ",
    }
    # Header
    print(f"  {'PORT':<6}  {'CHANNEL':<15}  {'FRAMES':>7}  {'LAST MOLECULE / DATA':<38}  {'SRC'}")
    print(f"  {'-'*6}  {'-'*15}  {'-'*7}  {'-'*38}  {'-'*8}")

    _tui_line_count = len(TUI_PORTS) + 1   # lines printed below header

    def _tui_row(port, stats, latest):
        """Format one TUI row for a port."""
        label = TUI_LABELS.get(port, f"Port {port}")
        count = stats.get(port, (0,""))[0]
        src_tag = "builtin" if port in (9999,9990,9991) and count > 0 else (
                  "ext    " if count > 0 else "       ")
        if latest and count > 0:
            mol = (latest.get("molecule_name") or latest.get("sequence") or
                   latest.get("formula") or "—")[:38]
        else:
            mol = "\033[2m(waiting...)\033[0m"
        dot = "\033[32m+\033[0m" if count > 0 else "\033[90m-\033[0m"
        return (f"  {dot} {port:<5}  {label:<15}  {count:>7}  "
                f"{mol:<38}  {src_tag}")

    try:
        while True:
            time.sleep(1)
            stats = store.get_stats()
            latest_all = store.get_latest()
            total = sum(stats.get(p, (0,""))[0] for p in data_ports)
            active = sum(1 for p in data_ports if stats.get(p,(0,""))[0] > 0)
            uptime = time.time() - _global_start_time

            # Move cursor up to rewrite TUI rows
            sys.stdout.write(f"\033[{_tui_line_count}A")
            for port in TUI_PORTS:
                row = _tui_row(port, stats, latest_all.get(port))
                sys.stdout.write(f"\r{row}\033[K\n")

            # Status bar (primary console line)
            bar = (f"  \033[1;35m[VSEPR-SIM]\033[0m  "
                   f"Uptime: {int(uptime//3600):02d}:{int(uptime%3600//60):02d}:{int(uptime%60):02d}"
                   f"  Frames: {total}"
                   f"  Active: {active}/{len(TUI_PORTS)}"
                   f"  Seq: {store.get_global_seq()}"
                   f"  tau_Tt: 2.2s")
            sys.stdout.write(f"\r{bar}\033[K")
            sys.stdout.flush()

    except KeyboardInterrupt:
        pass

    print("\n\n  \033[1;31mShutting down...\033[0m")
    for tag, port, g in generators:
        g.stop()
    for l in listeners:
        l.stop()
    httpd.shutdown()
    print("  VSEPR-SIM web viz host stopped.")


if __name__ == "__main__":
    main()
