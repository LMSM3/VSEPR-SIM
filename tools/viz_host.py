#!/usr/bin/env python3
"""
viz_host.py — Unified Visual Host for VSEPR-SIM
=================================================
Listens on ALL local 999X ports (9990-9999) + base port 8899.
Auto-detects frame types and spawns 2D/3D viewer windows.

Port Map:
    8899   Base control / peptide 2D analysis stream
    9990   Reserved (future: crystal viewer)
    9991   Reserved (future: coarse-grain viewer)
    9992   Reserved (future: EHD field viewer)
    9993   Reserved (future: thermal pipe viewer)
    9994   Peptide 2D analysis (alias)
    9995   Peptide 3D atomic (alias)
    9996   Reserved (future: phase diagram viewer)
    9997   Reserved (future: formation timeline)
    9998   Analysis deep view (gas2 analysis frames)
    9999   Atomic live view (gas2 atomic frames / peptide 3D)

Protocol:
    TCP NDJSON (newline-delimited JSON) — matches viz_server.cpp
    UDP JSON datagrams — matches peptide_stochastic_viz.cpp
    Both accepted on all ports simultaneously.

Usage:
    python tools/viz_host.py                    (listen on all ports)
    python tools/viz_host.py --ports 9999 8899  (specific ports only)
    python tools/viz_host.py --no-windows       (headless, log only)

Controls (any window):
    R             Reset view
    B             Toggle bonds
    C             Cycle colour mode
    N / P         Next / previous frame
    Space         Pause / resume
    Q / Escape    Quit all

Requires: Python 3.6+, tkinter (standard library only)
"""

import socket, json, threading, math, time, sys, os
import tkinter as tk
from collections import deque, defaultdict

# ============================================================================
# Port assignments
# ============================================================================

BASE_PORT        = 8899
ALL_999X         = list(range(9990, 10000))  # 9990-9999
ALL_PORTS        = [BASE_PORT] + ALL_999X

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
# Colour palette (shared across all windows)
# ============================================================================

BG       = "#080810"
PANEL_BG = "#10101a"
HDR_BG   = "#14141e"
FG       = "#e0e0e0"
ACCENT   = "#bb86fc"
ACCENT2  = "#03dac6"
ACCENT3  = "#cf6679"
DIM      = "#444455"
SUCCESS  = "#4CAF50"
GOLD     = "#FFD700"
WARN     = "#FF9800"

FONT_T  = ("Consolas", 12, "bold")
FONT_S  = ("Consolas", 9)
FONT_XS = ("Consolas", 7)

ELEMENT_COLORS = {
    1: "#FFFFFF", 6: "#808080", 7: "#2244FF", 8: "#FF2222",
    16: "#FFFF22", 15: "#FF8800", 17: "#44FF44", 9: "#88FF88",
}
ELEMENT_RADII = {1: 3, 6: 6, 7: 5, 8: 5, 16: 7, 15: 6, 17: 6, 9: 4}

ROLE_COLORS = {
    0: "#888888", 1: "#2266FF", 2: "#44AA44", 3: "#FF6644",
    4: "#FF2222", 5: "#6644FF", 6: "#AAAAAA", 7: "#44CCFF",
    8: "#FF44CC", 9: "#FFFF00",
}

SC_CLASS_COLORS = {
    0: "#888888", 1: "#228B22", 2: "#4488FF", 3: "#FF4444",
    4: "#4444FF", 5: "#FF8800", 6: "#FFFF00",
}

SC_CLASS_NAMES = {
    0: "none", 1: "hydrophobic", 2: "polar", 3: "acidic",
    4: "basic", 5: "aromatic", 6: "sulfur",
}

# ============================================================================
# Frame type detection
# ============================================================================

def detect_frame_type(frame):
    """Classify a JSON frame by its content."""
    if not isinstance(frame, dict):
        return "unknown"
    if "type" in frame:
        return frame["type"]  # "atomic" or "analysis"
    if "residues" in frame and "scores" in frame:
        return "peptide"
    if "atoms" in frame and "bonds" in frame and "lattice" in frame:
        return "atomic"
    if "hist_E" in frame or "history_energy" in frame:
        return "analysis"
    if "atoms" in frame:
        return "peptide"  # fallback for peptide with atoms
    return "unknown"

# ============================================================================
# 3D math helpers
# ============================================================================

def rot_x(x, y, z, c, s):
    return x, y*c - z*s, y*s + z*c

def rot_y(x, y, z, c, s):
    return x*c + z*s, y, -x*s + z*c

def project(x, y, z, cx, cy, fov=500.0):
    zz = z + fov
    if zz < 1: zz = 1
    return cx + x * fov / zz, cy - y * fov / zz, zz

# ============================================================================
# Frame Store — thread-safe ring buffer per source port
# ============================================================================

class FrameStore:
    """Thread-safe frame store for all ports."""
    def __init__(self, maxlen=128):
        self.lock = threading.Lock()
        self.buffers = defaultdict(lambda: deque(maxlen=maxlen))
        self.latest = {}
        self.counts = defaultdict(int)
        self.types = {}

    def push(self, port, frame):
        ftype = detect_frame_type(frame)
        with self.lock:
            self.buffers[port].append(frame)
            self.latest[port] = frame
            self.counts[port] += 1
            self.types[port] = ftype

    def get_latest(self, port):
        with self.lock:
            return self.latest.get(port)

    def get_all(self, port):
        with self.lock:
            return list(self.buffers[port])

    def get_all_latest(self):
        with self.lock:
            return dict(self.latest)

    def get_stats(self):
        with self.lock:
            return {p: (self.counts[p], self.types.get(p, "?")) for p in self.counts}

# ============================================================================
# Multi-protocol listener (TCP NDJSON + UDP datagrams)
# ============================================================================

class PortListener:
    """Listens on a single port for both TCP and UDP connections."""
    def __init__(self, port, store, verbose=False):
        self.port = port
        self.store = store
        self.verbose = verbose
        self.running = True

    def start(self):
        # UDP listener
        t_udp = threading.Thread(target=self._udp_loop, daemon=True)
        t_udp.start()
        # TCP listener
        t_tcp = threading.Thread(target=self._tcp_loop, daemon=True)
        t_tcp.start()

    def _udp_loop(self):
        try:
            sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            sock.bind(("127.0.0.1", self.port))
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
            except json.JSONDecodeError:
                pass
            except Exception:
                pass
        sock.close()

    def _tcp_loop(self):
        try:
            sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            sock.bind(("127.0.0.1", self.port))
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
                t = threading.Thread(target=self._tcp_client, args=(conn,), daemon=True)
                t.start()
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
# 3D Atomic Window
# ============================================================================

class AtomicWindow3D:
    """3D rotatable atomic viewer — works with both gas2 and peptide frames."""

    def __init__(self, parent, store, ports, title="3D Atomic View"):
        self.frame = tk.Toplevel(parent)
        self.frame.title(f"VSEPR-SIM | {title}")
        self.frame.configure(bg=BG)
        self.frame.geometry("1100x750")

        self.store = store
        self.ports = ports
        self.current_port = ports[0] if ports else 9999
        self.paused = False
        self.show_bonds = True
        self.color_mode = "role"
        self.rot_ax = 0.25
        self.rot_ay = 0.40
        self.zoom = 1.0
        self._drag_start = None

        self._build()
        self._poll()

    def _build(self):
        top = tk.Frame(self.frame, bg=HDR_BG, height=36)
        top.pack(fill=tk.X); top.pack_propagate(False)

        ports_str = ",".join(str(p) for p in self.ports)
        tk.Label(top, text=f"  3D Atomic View  :{ports_str}",
                 font=FONT_T, fg=ACCENT, bg=HDR_BG).pack(side=tk.LEFT, padx=6)
        self.lbl_status = tk.Label(top, text="Waiting...", font=FONT_S, fg=ACCENT3, bg=HDR_BG)
        self.lbl_status.pack(side=tk.RIGHT, padx=6)
        self.lbl_mode = tk.Label(top, text=f"[{self.color_mode}]", font=FONT_XS, fg=DIM, bg=HDR_BG)
        self.lbl_mode.pack(side=tk.RIGHT, padx=4)

        body = tk.Frame(self.frame, bg=BG)
        body.pack(fill=tk.BOTH, expand=True)
        body.columnconfigure(0, weight=1)
        body.columnconfigure(1, weight=0)
        body.rowconfigure(0, weight=1)

        self.cvs = tk.Canvas(body, bg=BG, highlightthickness=0, cursor="crosshair")
        self.cvs.grid(row=0, column=0, sticky="nsew")
        self.cvs.bind("<ButtonPress-1>", self._on_press)
        self.cvs.bind("<B1-Motion>", self._on_drag)
        self.cvs.bind("<MouseWheel>", self._on_scroll)

        side = tk.Frame(body, bg=PANEL_BG, width=200)
        side.grid(row=0, column=1, sticky="nsew")
        side.pack_propagate(False)
        self._build_side(side)

        self.frame.bind("<KeyPress-r>", lambda e: self._reset_view())
        self.frame.bind("<KeyPress-b>", lambda e: self._toggle_bonds())
        self.frame.bind("<KeyPress-c>", lambda e: self._cycle_color())
        self.frame.bind("<space>", lambda e: setattr(self, 'paused', not self.paused))

    def _build_side(self, parent):
        self.side_labels = {}
        fields = ["Port", "Type", "Frames", "Atoms", "Bonds",
                  "Formula/Seq", "State", "Energy", "Confidence"]
        for f in fields:
            row = tk.Frame(parent, bg=PANEL_BG)
            row.pack(fill=tk.X, padx=3, pady=1)
            tk.Label(row, text=f+":", font=FONT_XS, fg=DIM, bg=PANEL_BG,
                     width=12, anchor="w").pack(side=tk.LEFT)
            lbl = tk.Label(row, text="\u2014", font=FONT_XS, fg=FG, bg=PANEL_BG, anchor="w")
            lbl.pack(side=tk.LEFT, fill=tk.X, expand=True)
            self.side_labels[f] = lbl

    def _poll(self):
        if not self.paused:
            for p in self.ports:
                frame = self.store.get_latest(p)
                if frame:
                    self.current_port = p
                    self._render(frame)
                    self._update_side(frame, p)
                    break

        self.frame.after(50, self._poll)

    def _render(self, frame):
        self.cvs.delete("all")
        w = self.cvs.winfo_width()
        h = self.cvs.winfo_height()
        cx, cy = w / 2, h / 2

        atoms = frame.get("atoms", [])
        if not atoms:
            self.cvs.create_text(cx, cy, text="No atoms", fill=DIM, font=FONT_T)
            return

        avg_x = sum(a.get("x", 0) for a in atoms) / len(atoms)
        avg_y = sum(a.get("y", 0) for a in atoms) / len(atoms)
        avg_z = sum(a.get("z", 0) for a in atoms) / len(atoms)

        cos_x, sin_x = math.cos(self.rot_ax), math.sin(self.rot_ax)
        cos_y, sin_y = math.cos(self.rot_ay), math.sin(self.rot_ay)

        projected = []
        for a in atoms:
            x = (a.get("x", 0) - avg_x) * self.zoom * 40
            y = (a.get("y", 0) - avg_y) * self.zoom * 40
            z = (a.get("z", 0) - avg_z) * self.zoom * 40
            x, y, z = rot_x(x, y, z, cos_x, sin_x)
            x, y, z = rot_y(x, y, z, cos_y, sin_y)
            sx, sy, sz = project(x, y, z, cx, cy)
            projected.append((a, sx, sy, sz))

        projected.sort(key=lambda p: -p[3])

        # Backbone trace / bonds
        if self.show_bonds:
            bonds = frame.get("bonds", [])
            if bonds:
                for b in bonds:
                    i_id = b.get("i", b.get("a", -1))
                    j_id = b.get("j", b.get("b", -1))
                    p1 = p2 = None
                    for p in projected:
                        aid = p[0].get("id", -1)
                        if aid == i_id: p1 = p
                        if aid == j_id: p2 = p
                    if p1 and p2:
                        self.cvs.create_line(p1[1], p1[2], p2[1], p2[2],
                                             fill="#555577", width=1)
            else:
                # Peptide: draw backbone trace by sorted backbone atom roles
                backbone = [a for a in atoms if a.get("role", 0) in (1, 2, 3)]
                backbone.sort(key=lambda a: a.get("id", 0))
                for i in range(len(backbone) - 1):
                    p1 = p2 = None
                    for p in projected:
                        if p[0].get("id") == backbone[i].get("id"): p1 = p
                        if p[0].get("id") == backbone[i+1].get("id"): p2 = p
                    if p1 and p2:
                        self.cvs.create_line(p1[1], p1[2], p2[1], p2[2],
                                             fill="#555577", width=1)

        # Draw atoms
        for a, sx, sy, sz in projected:
            Z = a.get("Z", 6)
            role = a.get("role", 0)

            if self.color_mode == "element":
                color = ELEMENT_COLORS.get(Z, "#888888")
            elif self.color_mode == "role":
                color = ROLE_COLORS.get(role, "#888888")
            elif self.color_mode == "charge":
                q = a.get("q", 0)
                if q < -0.1: color = "#FF4444"
                elif q > 0.1: color = "#4444FF"
                else: color = "#888888"
            elif self.color_mode == "energy":
                chi = a.get("chi", 0)
                r_val = min(255, int(chi * 400))
                b_val = max(0, 255 - int(chi * 400))
                color = f"#{r_val:02x}44{b_val:02x}"
            else:
                color = ELEMENT_COLORS.get(Z, "#888888")

            r = ELEMENT_RADII.get(Z, 5) * self.zoom
            depth_fade = max(0.3, min(1.0, 800 / max(sz, 1)))
            r *= depth_fade

            self.cvs.create_oval(sx - r, sy - r, sx + r, sy + r,
                                 fill=color, outline="")

        # HUD
        seq = frame.get("sequence", frame.get("formula", ""))
        e_total = frame.get("energy", {}).get("total",
                  frame.get("energy_Eh", 0))
        conf = frame.get("scores", {}).get("confidence", 0)
        self.cvs.create_text(10, h - 30, text=f"{seq}", fill=ACCENT,
                             font=FONT_XS, anchor="w")
        self.cvs.create_text(10, h - 15,
                             text=f"E={e_total:+.1f}  conf={conf:.3f}  "
                                  f"[{self.color_mode}] bonds={'ON' if self.show_bonds else 'OFF'} "
                                  f"zoom={self.zoom:.1f}",
                             fill=DIM, font=FONT_XS, anchor="w")

    def _update_side(self, frame, port):
        s = self.side_labels
        stats = self.store.get_stats()
        count, ftype = stats.get(port, (0, "?"))

        s["Port"].config(text=f"{port} ({PORT_LABELS.get(port, '?')})")
        s["Type"].config(text=ftype)
        s["Frames"].config(text=str(count))
        s["Atoms"].config(text=str(frame.get("atom_count", frame.get("n_atoms", len(frame.get("atoms", []))))))
        s["Bonds"].config(text=str(frame.get("bond_count", len(frame.get("bonds", [])))))
        s["Formula/Seq"].config(text=str(frame.get("sequence", frame.get("formula", "?")))[:28])
        s["State"].config(text=str(frame.get("state", frame.get("phase", frame.get("phase_guess", "?")))))

        e = frame.get("energy", {})
        if isinstance(e, dict):
            s["Energy"].config(text=f"{e.get('total', 0):+.1f}")
        else:
            s["Energy"].config(text=f"{frame.get('energy_Eh', 0):+.4f}")

        s["Confidence"].config(text=f"{frame.get('scores', {}).get('confidence', frame.get('convergence', 0)):.4f}")

        self.lbl_status.config(text=f"Port {port} | {count} frames", fg=SUCCESS)

    def _on_press(self, e):
        self._drag_start = (e.x, e.y)

    def _on_drag(self, e):
        if self._drag_start:
            dx = e.x - self._drag_start[0]
            dy = e.y - self._drag_start[1]
            self.rot_ay += dx * 0.005
            self.rot_ax += dy * 0.005
            self._drag_start = (e.x, e.y)

    def _on_scroll(self, e):
        self.zoom *= 1.1 if e.delta > 0 else (1.0 / 1.1)
        self.zoom = max(0.1, min(10.0, self.zoom))

    def _reset_view(self):
        self.rot_ax = 0.25; self.rot_ay = 0.40; self.zoom = 1.0

    def _toggle_bonds(self):
        self.show_bonds = not self.show_bonds

    def _cycle_color(self):
        modes = ["role", "element", "charge", "energy"]
        idx = modes.index(self.color_mode) if self.color_mode in modes else 0
        self.color_mode = modes[(idx + 1) % len(modes)]
        self.lbl_mode.config(text=f"[{self.color_mode}]")


# ============================================================================
# 2D Analysis Window
# ============================================================================

class AnalysisWindow2D:
    """2D charts — energy timeline, scores, Ramachandran, residue strip."""

    def __init__(self, parent, store, ports, title="2D Analysis"):
        self.frame = tk.Toplevel(parent)
        self.frame.title(f"VSEPR-SIM | {title}")
        self.frame.configure(bg=BG)
        self.frame.geometry("1200x700")

        self.store = store
        self.ports = ports
        self.current_port = ports[0] if ports else 8899
        self.paused = False
        self.chart_mode = 0

        self._build()
        self._poll()

    def _build(self):
        top = tk.Frame(self.frame, bg=HDR_BG, height=36)
        top.pack(fill=tk.X); top.pack_propagate(False)

        ports_str = ",".join(str(p) for p in self.ports)
        tk.Label(top, text=f"  2D Analysis  :{ports_str}",
                 font=FONT_T, fg=ACCENT, bg=HDR_BG).pack(side=tk.LEFT, padx=6)
        self.lbl_status = tk.Label(top, text="Waiting...", font=FONT_S, fg=ACCENT3, bg=HDR_BG)
        self.lbl_status.pack(side=tk.RIGHT, padx=6)
        self.lbl_mode = tk.Label(top, text="[energy+score]", font=FONT_XS, fg=DIM, bg=HDR_BG)
        self.lbl_mode.pack(side=tk.RIGHT, padx=4)

        info = tk.Frame(self.frame, bg=PANEL_BG, height=26)
        info.pack(fill=tk.X); info.pack_propagate(False)
        self.lbl_info = tk.Label(info, text="", font=FONT_S, fg=ACCENT2, bg=PANEL_BG)
        self.lbl_info.pack(side=tk.LEFT, padx=6)

        self.cvs = tk.Canvas(self.frame, bg=BG, highlightthickness=0)
        self.cvs.pack(fill=tk.BOTH, expand=True)

        self.frame.bind("<KeyPress-c>", lambda e: self._cycle_chart())
        self.frame.bind("<space>", lambda e: setattr(self, 'paused', not self.paused))

    def _poll(self):
        if not self.paused:
            for p in self.ports:
                frame = self.store.get_latest(p)
                if frame:
                    self.current_port = p
                    self._render(frame, p)
                    break
        self.frame.after(60, self._poll)

    def _render(self, frame, port):
        self.cvs.delete("all")
        w = self.cvs.winfo_width()
        h = self.cvs.winfo_height()

        if not frame.get("success", True):
            self.cvs.create_text(w/2, h/2,
                                 text=f"Run {frame.get('run_id','?')} FAILED\n{frame.get('error','')}",
                                 fill=ACCENT3, font=FONT_T)
            return

        stats = self.store.get_stats()
        count, ftype = stats.get(port, (0, "?"))
        self.lbl_status.config(text=f"Port {port} | {count} frames | {ftype}", fg=SUCCESS)

        seq = frame.get("sequence", frame.get("formula", ""))
        env = frame.get("environment", "")
        state = frame.get("state", frame.get("phase_guess", ""))
        self.lbl_info.config(text=f"{seq}  |  {env}  |  {state}")

        if self.chart_mode == 0:
            self._draw_energy_bars(frame, 10, 10, w/2 - 20, h/2 - 20)
            self._draw_score_bars(frame, w/2 + 10, 10, w/2 - 20, h/2 - 20)
            self._draw_energy_timeline(port, 10, h/2, w - 20, h/2 - 20)
        elif self.chart_mode == 1:
            self._draw_ramachandran(port, 10, 10, w - 20, h - 30)
        elif self.chart_mode == 2:
            self._draw_residue_detail(frame, 10, 10, w - 20, h - 30)

    def _draw_energy_bars(self, frame, ox, oy, pw, ph):
        """Current frame energy component bars."""
        self.cvs.create_text(ox + pw/2, oy + 10, text="Energy Components",
                             fill=ACCENT, font=FONT_S)

        e = frame.get("energy", {})
        if not isinstance(e, dict):
            return

        components = [
            ("bond",      e.get("bond", 0),      "#44FF44"),
            ("vdw",       e.get("vdw", 0),        "#FF8844"),
            ("coulomb",   e.get("coulomb", 0),     "#4488FF"),
            ("solvation", e.get("solvation", 0),   "#FF44FF"),
            ("formation", e.get("formation", 0),   "#FFFF44"),
            ("total",     e.get("total", 0),        "#FFFFFF"),
        ]

        bar_y = oy + 30
        bar_h = 14
        gap = 5

        vals = [abs(c[1]) for c in components if c[1] != 0]
        max_val = max(vals) if vals else 1

        for name, val, color in components:
            norm = abs(val) / max_val if max_val > 0 else 0
            bar_w = max(0, norm * (pw - 120))

            self.cvs.create_text(ox + 2, bar_y + bar_h/2, text=name,
                                 fill=FG, font=FONT_XS, anchor="w")
            bar_color = color if val <= 0 else "#FF6666"
            self.cvs.create_rectangle(ox + 75, bar_y, ox + 75 + bar_w, bar_y + bar_h,
                                       fill=bar_color, outline="")
            self.cvs.create_text(ox + pw, bar_y + bar_h/2, text=f"{val:+.1f}",
                                 fill=DIM, font=FONT_XS, anchor="e")
            bar_y += bar_h + gap

    def _draw_score_bars(self, frame, ox, oy, pw, ph):
        """Score bar chart."""
        self.cvs.create_text(ox + pw/2, oy + 10, text="Quality Scores",
                             fill=ACCENT, font=FONT_S)

        scores = frame.get("scores", {})
        if not scores:
            return

        items = [
            ("steric",        scores.get("steric", 0),        "#44FF44"),
            ("electrostatic", scores.get("electrostatic", 0),  "#4488FF"),
            ("hydrophobic",   scores.get("hydrophobic", 0),    "#FF8844"),
            ("planarity",     scores.get("planarity", 0),      "#FF44FF"),
            ("hbond",         scores.get("hbond", 0),          "#44CCFF"),
            ("confidence",    scores.get("confidence", 0),     GOLD),
        ]

        bar_y = oy + 30
        bar_h = 14
        gap = 5

        for name, val, color in items:
            bar_w = max(0, val * (pw - 120))
            self.cvs.create_text(ox + 2, bar_y + bar_h/2, text=name,
                                 fill=FG, font=FONT_XS, anchor="w")
            self.cvs.create_rectangle(ox + 90, bar_y, ox + 90 + bar_w, bar_y + bar_h,
                                       fill=color, outline="")
            self.cvs.create_rectangle(ox + 90, bar_y, ox + pw - 30, bar_y + bar_h,
                                       outline=DIM)
            self.cvs.create_text(ox + pw, bar_y + bar_h/2, text=f"{val:.3f}",
                                 fill=DIM, font=FONT_XS, anchor="e")
            bar_y += bar_h + gap

    def _draw_energy_timeline(self, port, ox, oy, pw, ph):
        """Energy bar chart across all received frames on this port."""
        self.cvs.create_text(ox + pw/2, oy + 10, text="Energy Landscape (All Frames)",
                             fill=ACCENT, font=FONT_S)
        self.cvs.create_rectangle(ox, oy + 25, ox + pw, oy + ph, outline=DIM)

        all_frames = self.store.get_all(port)
        if not all_frames:
            return

        chart_y = oy + 30
        chart_h = ph - 40
        n = len(all_frames)

        energies = []
        for f in all_frames:
            e = f.get("energy", {})
            if isinstance(e, dict):
                energies.append(e.get("total", 0))
            else:
                energies.append(f.get("energy_Eh", 0))

        valid = [e for e in energies if e is not None and math.isfinite(e)]
        if not valid:
            return

        e_min = min(valid) - 10
        e_max = max(valid) + 10
        if abs(e_max - e_min) < 1: e_max = e_min + 1

        bar_w = max(2, (pw - 10) / n)
        for i, e in enumerate(energies):
            if e is None or not math.isfinite(e):
                continue
            norm = (e - e_min) / (e_max - e_min)
            bar_h = max(2, norm * chart_h)
            x = ox + 5 + i * bar_w
            y_top = chart_y + chart_h - bar_h

            color = "#44FF44" if e < 0 else "#FF4444"
            self.cvs.create_rectangle(x, y_top, x + bar_w - 1, chart_y + chart_h,
                                       fill=color, outline="")

        self.cvs.create_text(ox + 2, chart_y, text=f"{e_max:.0f}", fill=DIM,
                             font=FONT_XS, anchor="nw")
        self.cvs.create_text(ox + 2, chart_y + chart_h, text=f"{e_min:.0f}", fill=DIM,
                             font=FONT_XS, anchor="sw")

    def _draw_ramachandran(self, port, ox, oy, pw, ph):
        """Ramachandran scatter (all frames, all residues)."""
        self.cvs.create_text(ox + pw/2, oy + 10, text="Ramachandran Plot (All Frames)",
                             fill=ACCENT, font=FONT_S)

        plot_x = ox + 50
        plot_y = oy + 30
        plot_w = pw - 70
        plot_h = ph - 50

        self.cvs.create_rectangle(plot_x, plot_y, plot_x + plot_w, plot_y + plot_h, outline=DIM)
        self.cvs.create_text(plot_x + plot_w/2, plot_y + plot_h + 12, text="phi (deg)",
                             fill=FG, font=FONT_XS)
        self.cvs.create_text(plot_x - 25, plot_y + plot_h/2, text="psi",
                             fill=FG, font=FONT_XS)

        for val in [-180, -90, 0, 90, 180]:
            nx = (val + 180) / 360
            self.cvs.create_text(plot_x + nx * plot_w, plot_y + plot_h + 3,
                                 text=str(val), fill=DIM, font=FONT_XS, anchor="n")
            ny = 1 - (val + 180) / 360
            self.cvs.create_text(plot_x - 5, plot_y + ny * plot_h,
                                 text=str(val), fill=DIM, font=FONT_XS, anchor="e")

        all_frames = self.store.get_all(port)
        for f in all_frames:
            for res in f.get("residues", []):
                phi = res.get("phi", 0)
                psi = res.get("psi", 0)
                nx = (phi + 180) / 360
                ny = 1 - (psi + 180) / 360
                px = plot_x + nx * plot_w
                py = plot_y + ny * plot_h
                sc = res.get("sc_class", 0)
                color = SC_CLASS_COLORS.get(sc, "#666")
                r = 3
                self.cvs.create_oval(px - r, py - r, px + r, py + r,
                                     fill=color, outline="")

    def _draw_residue_detail(self, frame, ox, oy, pw, ph):
        """Per-residue table."""
        self.cvs.create_text(ox + pw/2, oy + 10,
                             text=f"Residue Detail \u2014 {frame.get('sequence', '?')}",
                             fill=ACCENT, font=FONT_S)

        residues = frame.get("residues", [])
        headers = ["#", "Name", "SC Class", "Phi", "Psi", "Omega"]
        col_w = pw / len(headers)
        for j, h in enumerate(headers):
            self.cvs.create_text(ox + j * col_w + col_w/2, oy + 30, text=h,
                                 fill=ACCENT2, font=FONT_XS)

        for i, res in enumerate(residues):
            y = oy + 48 + i * 16
            if y > oy + ph - 10: break
            vals = [
                str(res.get("id", i + 1)),
                res.get("name", "?"),
                SC_CLASS_NAMES.get(res.get("sc_class", 0), "?"),
                f"{res.get('phi', 0):+.1f}",
                f"{res.get('psi', 0):+.1f}",
                f"{res.get('omega', 0):+.1f}",
            ]
            for j, v in enumerate(vals):
                self.cvs.create_text(ox + j * col_w + col_w/2, y, text=v,
                                     fill=FG, font=FONT_XS)

    def _cycle_chart(self):
        self.chart_mode = (self.chart_mode + 1) % 3
        modes = ["energy+score", "ramachandran", "residue detail"]
        self.lbl_mode.config(text=f"[{modes[self.chart_mode]}]")


# ============================================================================
# Status dashboard window
# ============================================================================

class StatusDashboard:
    """Port status overview — shows all listening ports and frame counts."""

    def __init__(self, parent, store, listeners, ports):
        self.frame = tk.Toplevel(parent)
        self.frame.title("VSEPR-SIM | Viz Host Dashboard")
        self.frame.configure(bg=BG)
        self.frame.geometry("500x520")

        self.store = store
        self.listeners = listeners
        self.ports = ports
        self.start_time = time.time()

        self._build()
        self._poll()

    def _build(self):
        top = tk.Frame(self.frame, bg=HDR_BG, height=36)
        top.pack(fill=tk.X); top.pack_propagate(False)
        tk.Label(top, text="  VSEPR-SIM  Viz Host Dashboard",
                 font=FONT_T, fg=ACCENT, bg=HDR_BG).pack(side=tk.LEFT, padx=6)

        self.cvs = tk.Canvas(self.frame, bg=BG, highlightthickness=0)
        self.cvs.pack(fill=tk.BOTH, expand=True)

    def _poll(self):
        self.cvs.delete("all")
        w = self.cvs.winfo_width()
        elapsed = time.time() - self.start_time

        y = 15
        self.cvs.create_text(w/2, y, text="Port Status", fill=ACCENT, font=FONT_S)
        y += 25

        # Header
        cols = [40, 130, 220, 310, 400]
        headers = ["Port", "Label", "Proto", "Frames", "Type"]
        for j, h in enumerate(headers):
            self.cvs.create_text(cols[j], y, text=h, fill=ACCENT2, font=FONT_XS, anchor="w")
        y += 18

        stats = self.store.get_stats()

        for port in self.ports:
            count, ftype = stats.get(port, (0, "\u2014"))
            label = PORT_LABELS.get(port, "?")
            active = count > 0
            color = SUCCESS if active else DIM

            self.cvs.create_text(cols[0], y, text=str(port), fill=color, font=FONT_XS, anchor="w")
            self.cvs.create_text(cols[1], y, text=label[:14], fill=color, font=FONT_XS, anchor="w")
            self.cvs.create_text(cols[2], y, text="TCP+UDP", fill=DIM, font=FONT_XS, anchor="w")
            self.cvs.create_text(cols[3], y, text=str(count), fill=color, font=FONT_XS, anchor="w")
            self.cvs.create_text(cols[4], y, text=ftype, fill=color, font=FONT_XS, anchor="w")

            # Activity dot
            dot_x = 25
            dot_color = SUCCESS if active else "#333"
            self.cvs.create_oval(dot_x - 4, y - 4, dot_x + 4, y + 4,
                                 fill=dot_color, outline="")

            y += 18

        y += 15
        self.cvs.create_text(w/2, y, text=f"Uptime: {elapsed:.0f}s", fill=DIM, font=FONT_XS)
        y += 15

        total_frames = sum(stats.get(p, (0, ""))[0] for p in self.ports)
        active_ports = sum(1 for p in self.ports if stats.get(p, (0, ""))[0] > 0)
        self.cvs.create_text(w/2, y,
                             text=f"Total frames: {total_frames}  |  Active ports: {active_ports}/{len(self.ports)}",
                             fill=FG, font=FONT_XS)

        self.frame.after(500, self._poll)


# ============================================================================
# Main — launch everything
# ============================================================================

def main():
    ports = list(ALL_PORTS)
    no_windows = False
    verbose = False

    args = sys.argv[1:]
    i = 0
    while i < len(args):
        if args[i] == "--ports" and i + 1 < len(args):
            ports = [int(p) for p in args[i+1].split(",")]
            i += 2
        elif args[i] == "--no-windows":
            no_windows = True
            i += 1
        elif args[i] == "--verbose" or args[i] == "-v":
            verbose = True
            i += 1
        elif args[i] in ("--help", "-h"):
            print(__doc__)
            return
        else:
            i += 1

    print("\033[1;35m")
    print("\u2554" + "\u2550" * 60 + "\u2557")
    print("\u2551  VSEPR-SIM  Unified Visual Host" + " " * 27 + "\u2551")
    print("\u2551  Listening on ALL 999X + base 8899" + " " * 24 + "\u2551")
    print("\u255a" + "\u2550" * 60 + "\u255d")
    print("\033[0m")

    store = FrameStore()

    # Start listeners on all ports
    listeners = []
    for port in ports:
        listener = PortListener(port, store, verbose=verbose)
        listener.start()
        listeners.append(listener)
        label = PORT_LABELS.get(port, "")
        print(f"  \033[32m\u25cf\033[0m  Port {port:<5}  TCP+UDP  {label}")

    print(f"\n  Total: {len(ports)} ports active")
    print(f"  Protocol: TCP NDJSON + UDP JSON datagrams")
    print()

    print("  Connect sources:")
    print("    vsepr viz Ar -T 300 -N 64          (atomic -> 9999)")
    print("    peptide-stochastic-viz              (peptide -> 9999 + 8899)")
    print()

    if no_windows:
        print("  Running headless (--no-windows). Press Ctrl+C to stop.\n")
        try:
            while True:
                time.sleep(1)
                stats = store.get_stats()
                active = sum(1 for p in ports if stats.get(p, (0, ""))[0] > 0)
                total = sum(stats.get(p, (0, ""))[0] for p in ports)
                sys.stdout.write(f"\r  Ports: {active}/{len(ports)} active  Frames: {total}")
                sys.stdout.flush()
        except KeyboardInterrupt:
            pass
    else:
        root = tk.Tk()
        root.withdraw()  # hide root window

        # 3D window — watches 9999, 9995 (peptide 3D alias)
        ports_3d = [p for p in ports if p in (9999, 9995, 9994, 9993, 9992, 9991, 9990)]
        if not ports_3d:
            ports_3d = [9999]
        win_3d = AtomicWindow3D(root, store, ports_3d, "3D Atomic View :9999,999X")

        # 2D window — watches 8899, 9998, 9994 (analysis + peptide 2D)
        ports_2d = [p for p in ports if p in (8899, 9998, 9994, 9997, 9996)]
        if not ports_2d:
            ports_2d = [8899]
        win_2d = AnalysisWindow2D(root, store, ports_2d, "2D Analysis :8899,999X")

        # Dashboard
        dashboard = StatusDashboard(root, store, listeners, ports)

        def on_close():
            for l in listeners:
                l.stop()
            root.destroy()

        win_3d.frame.protocol("WM_DELETE_WINDOW", on_close)
        win_2d.frame.protocol("WM_DELETE_WINDOW", on_close)
        dashboard.frame.protocol("WM_DELETE_WINDOW", on_close)

        root.bind("<Escape>", lambda e: on_close())
        root.bind("<KeyPress-q>", lambda e: on_close())

        root.mainloop()

    # Cleanup
    for l in listeners:
        l.stop()
    print("\n  Viz host stopped.")


if __name__ == "__main__":
    main()
