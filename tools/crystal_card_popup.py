#!/usr/bin/env python3
"""
crystal_card_popup.py — V4 Visual Card: Cartoon Crystal + Score Graphs
═══════════════════════════════════════════════════════════════════════
Pops up at random (triggered by the kernel) 0.08s after classification
options are collected, before thermal analysis runs.

Shows:
  - Cartoon 2D crystal structure for the host atom (projected FCC/BCC/HCP)
  - CPK-coloured atom spheres with neighbour bonds
  - V4 score bar charts (gamma, Q_data, C_compact) if available
  - Element info card (name, atomic number, lattice, Tmelt, Ecoh)

Usage (direct):
    python3 tools/crystal_card_popup.py <symbol> [lattice] [gamma] [q_data] [compact]

Usage (from kernel via subprocess, non-blocking):
    python3 tools/crystal_card_popup.py Au FCC 0.25 0.56 0.34 &

Auto-closes after 8 seconds (non-blocking to the kernel).
"""

import sys
import os
import json
import math
import time
import random
import tempfile
import threading
import tkinter as tk
from tkinter import font as tkfont

# ── Reference data ─────────────────────────────────────────────────────────────
ELEMENT_DATA = {
    "Au": {"name": "Gold",       "Z": 79,  "lattice": "FCC", "a0": 4.078, "Tmelt": 1337.3, "Ecoh": -3.81, "r": 1.44, "color": "#FFD700"},
    "Ag": {"name": "Silver",     "Z": 47,  "lattice": "FCC", "a0": 4.086, "Tmelt": 1234.9, "Ecoh": -2.95, "r": 1.44, "color": "#C0C0C0"},
    "Cu": {"name": "Copper",     "Z": 29,  "lattice": "FCC", "a0": 3.615, "Tmelt": 1357.8, "Ecoh": -3.49, "r": 1.28, "color": "#B87333"},
    "Pt": {"name": "Platinum",   "Z": 78,  "lattice": "FCC", "a0": 3.924, "Tmelt": 2041.4, "Ecoh": -5.84, "r": 1.39, "color": "#E8E8E8"},
    "Ni": {"name": "Nickel",     "Z": 28,  "lattice": "FCC", "a0": 3.524, "Tmelt": 1728.0, "Ecoh": -4.44, "r": 1.25, "color": "#8FAF8F"},
    "Al": {"name": "Aluminium",  "Z": 13,  "lattice": "FCC", "a0": 4.050, "Tmelt":  933.5, "Ecoh": -3.39, "r": 1.43, "color": "#A8A9AD"},
    "Fe": {"name": "Iron",       "Z": 26,  "lattice": "BCC", "a0": 2.870, "Tmelt": 1811.0, "Ecoh": -4.28, "r": 1.26, "color": "#CC6633"},
    "W":  {"name": "Tungsten",   "Z": 74,  "lattice": "BCC", "a0": 3.165, "Tmelt": 3695.0, "Ecoh": -8.90, "r": 1.41, "color": "#6B7FAB"},
    "Mo": {"name": "Molybdenum", "Z": 42,  "lattice": "BCC", "a0": 3.147, "Tmelt": 2896.0, "Ecoh": -6.82, "r": 1.39, "color": "#7D7D8C"},
    "Cr": {"name": "Chromium",   "Z": 24,  "lattice": "BCC", "a0": 2.885, "Tmelt": 2180.0, "Ecoh": -4.10, "r": 1.28, "color": "#8A8ABA"},
    "Ti": {"name": "Titanium",   "Z": 22,  "lattice": "HCP", "a0": 2.950, "Tmelt": 1941.0, "Ecoh": -4.85, "r": 1.47, "color": "#BFC2C7"},
    "Co": {"name": "Cobalt",     "Z": 27,  "lattice": "HCP", "a0": 2.507, "Tmelt": 1768.0, "Ecoh": -4.39, "r": 1.25, "color": "#4A6FA5"},
}

# ── 3D view constants ──────────────────────────────────────────────────────────
VIEW_AZIMUTH   = math.radians(30.0)
VIEW_ELEVATION = math.radians(22.0)

# Complexity tiers: (label, (nx, ny, nz), px_per_cell)
COMPLEXITY = [
    ("3\u00d73",  (3, 3, 2), 44),
    ("3\u00d74",  (3, 4, 2), 37),
    ("4\u00d74",  (4, 4, 3), 29),
]

# Bond distance thresholds in lattice units (a = 1)
_BOND_T3D = {"FCC": 0.78, "BCC": 0.92, "HCP": 1.05}

# ── Card limit lockfile infrastructure ─────────────────────────────────────────
CARD_LOCK_DIR = os.path.join(tempfile.gettempdir(), "vsepr_cards")
MAX_3D_CARDS  = 12


def _ensure_lock_dir():
    os.makedirs(CARD_LOCK_DIR, exist_ok=True)


def _count_active_3d_cards():
    """Count live 3D card processes; clean up stale locks."""
    _ensure_lock_dir()
    count = 0
    for fname in os.listdir(CARD_LOCK_DIR):
        if not fname.startswith("card_") or not fname.endswith(".lock"):
            continue
        fpath = os.path.join(CARD_LOCK_DIR, fname)
        try:
            pid = int(fname[5:-5])
            os.kill(pid, 0)   # raises OSError if process is gone
            count += 1
        except (OSError, ValueError):
            try:
                os.remove(fpath)
            except OSError:
                pass
    return count


def _register_3d_card():
    """Write a lock file for this PID; return its path."""
    _ensure_lock_dir()
    path = os.path.join(CARD_LOCK_DIR, f"card_{os.getpid()}.lock")
    with open(path, "w") as f:
        f.write(str(os.getpid()))
    return path


def _unregister_card(lock_path):
    """Remove lock file on close."""
    if lock_path and os.path.exists(lock_path):
        try:
            os.remove(lock_path)
        except OSError:
            pass


# ── Data snapshot exporter ──────────────────────────────────────────────────────
_SNAPSHOT_DIR = os.path.join(
    os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
    "data", "card_snapshots"
)


def _export_card_snapshot(symbol, lattice, gamma, q_data, compact, mode, cidx):
    """Write a JSON data snapshot for this card to data/card_snapshots/."""
    os.makedirs(_SNAPSHOT_DIR, exist_ok=True)
    ts_ms = int(time.time() * 1000)
    cplx  = COMPLEXITY[cidx]
    info  = ELEMENT_DATA.get(symbol, {})
    snap  = {
        "timestamp_ms":       ts_ms,
        "symbol":             symbol,
        "name":               info.get("name", symbol),
        "Z":                  info.get("Z", 0),
        "lattice":            lattice,
        "a0_angstrom":        info.get("a0", None),
        "Tmelt_K":            info.get("Tmelt", None),
        "Ecoh_eV":            info.get("Ecoh", None),
        "gamma":              gamma,
        "q_data":             q_data,
        "compact":            compact,
        "render_mode":        mode,
        "complexity_label":   cplx[0],
        "supercell_dims":     list(cplx[1]),
        "view_azimuth_deg":   math.degrees(VIEW_AZIMUTH),
        "view_elevation_deg": math.degrees(VIEW_ELEVATION),
        "pid":                os.getpid(),
    }
    fname = f"{symbol}_{lattice}_{ts_ms}.json"
    fpath = os.path.join(_SNAPSHOT_DIR, fname)
    with open(fpath, "w") as f:
        json.dump(snap, f, indent=2)
    return fpath


# ── 3D orthographic projection ──────────────────────────────────────────────────

def project_ortho(pos3, az, el, scale, cx, cy):
    """Return (screen_x, screen_y, depth) for painter's algorithm."""
    x, y, z   = pos3
    ca, sa    = math.cos(az), math.sin(az)
    ce, se    = math.cos(el), math.sin(el)
    sx        = (-sa * x + ca * y) * scale + cx
    sy        = (ca * se * x + sa * se * y - ce * z) * scale + cy
    depth     = ca * ce * x + sa * ce * y + se * z
    return sx, sy, depth


# ── 3D site generators ──────────────────────────────────────────────────────────

def fcc_3d_sites(nx, ny, nz):
    """FCC basis sites in a nx×ny×nz supercell (lattice units, a=1)."""
    basis = [(0,0,0), (0.5,0.5,0), (0.5,0,0.5), (0,0.5,0.5)]
    sites = []
    for ix in range(nx):
        for iy in range(ny):
            for iz in range(nz):
                for bx, by_, bz in basis:
                    sites.append((ix + bx, iy + by_, iz + bz))
    return sites


def bcc_3d_sites(nx, ny, nz):
    """BCC basis sites in a nx×ny×nz supercell (lattice units, a=1)."""
    basis = [(0,0,0), (0.5,0.5,0.5)]
    sites = []
    for ix in range(nx):
        for iy in range(ny):
            for iz in range(nz):
                for bx, by_, bz in basis:
                    sites.append((ix + bx, iy + by_, iz + bz))
    return sites


def hcp_3d_sites(nx, ny, nz):
    """HCP orthogonalised basis (c/a = 1.633) in a nx×ny×nz supercell."""
    ca_ratio = 1.6330
    basis = [(0, 0, 0), (0.5, 1.0/3.0, 0.5 * ca_ratio)]
    sites = []
    for ix in range(nx):
        for iy in range(ny):
            for iz in range(nz):
                for bx, by_, bz in basis:
                    sites.append((ix + bx, iy + by_, iz + bz))
    return sites


def _l_segment_trim(sites, nx, ny, cidx):
    """Remove top-right rectangular corner for cidx >= 1 (L-shaped view)."""
    if cidx == 0:
        return sites
    cx_cut = nx * 2 // 3
    cy_cut = ny * 2 // 3
    return [(x, y, z) for x, y, z in sites
            if not (x >= cx_cut and y >= cy_cut)]


def bond_pairs_3d(sites, bond_thresh):
    """Return index pairs within bond_thresh (world units)."""
    pairs = []
    n = len(sites)
    for i in range(n):
        x1, y1, z1 = sites[i]
        for j in range(i + 1, n):
            x2, y2, z2 = sites[j]
            d = math.sqrt((x2-x1)**2 + (y2-y1)**2 + (z2-z1)**2)
            if d < bond_thresh:
                pairs.append((i, j))
    return pairs


# ── Crystal geometry generators (2D) ───────────────────────────────────────────

def fcc_2d_sites(cx, cy, scale):
    """FCC [001] projection: face-centred square grid, 2 unit cells."""
    sites = []
    a = scale
    for i in range(-1, 3):
        for j in range(-1, 3):
            sites.append((cx + i * a, cy + j * a, False))          # corner
            sites.append((cx + (i + 0.5) * a, cy + (j + 0.5) * a, True))  # face centre
    return sites

def bcc_2d_sites(cx, cy, scale):
    """BCC [001] projection: square grid + body centres."""
    sites = []
    a = scale
    for i in range(-1, 3):
        for j in range(-1, 3):
            sites.append((cx + i * a, cy + j * a, False))          # corner
            sites.append((cx + (i + 0.5) * a, cy + (j + 0.5) * a, True))  # body centre
    return sites

def hcp_2d_sites(cx, cy, scale):
    """HCP [0001] projection: hexagonal close-packed."""
    sites = []
    a = scale
    h = a * math.sqrt(3) / 2
    for row in range(-1, 4):
        offset = 0 if row % 2 == 0 else a / 2
        for col in range(-1, 4):
            sites.append((cx + col * a + offset, cy + row * h, row % 2 == 1))
    return sites

def bond_pairs_2d(sites, bond_thresh):
    """Return pairs of site indices within bond_thresh distance (2D)."""
    pairs = []
    for i, (x1, y1, _) in enumerate(sites):
        for j, (x2, y2, _) in enumerate(sites):
            if j <= i:
                continue
            d = math.hypot(x2 - x1, y2 - y1)
            if d < bond_thresh:
                pairs.append((i, j))
    return pairs

# ── Score bar renderer ──────────────────────────────────────────────────────────

def draw_score_bar(canvas, x, y, w, h, value, label, color, bg="#1a1a2e"):
    """Draw a horizontal score bar [0,1] with label."""
    canvas.create_rectangle(x, y, x + w, y + h, fill="#2a2a3e", outline="#444", width=1)
    fill_w = int(w * max(0.0, min(1.0, value)))
    if fill_w > 0:
        canvas.create_rectangle(x, y, x + fill_w, y + h, fill=color, outline="")
    canvas.create_text(x + w + 6, y + h // 2, text=f"{value:.3f}",
                       fill="#e0e0e0", anchor="w", font=("Consolas", 9))
    canvas.create_text(x - 4, y + h // 2, text=label,
                       fill="#aaaaaa", anchor="e", font=("Consolas", 9))

# ── Main popup window ───────────────────────────────────────────────────────────

def build_popup(symbol, lattice_override=None, gamma=None, q_data=None, compact=None):
    info = ELEMENT_DATA.get(symbol, {
        "name": symbol, "Z": 0, "lattice": lattice_override or "FCC",
        "a0": 3.5, "Tmelt": 0.0, "Ecoh": 0.0, "r": 1.3, "color": "#888888"
    })
    lattice    = lattice_override or info["lattice"]
    elem_color  = info["color"]
    elem_color2 = "#ffffff"

    # ── Mutable card state ───────────────────────────────────────────────────
    state = {
        "mode":      "2D",    # "2D" | "3D"
        "cidx":      0,       # COMPLEXITY tier index (0/1/2)
        "lock_path": None,    # lockfile path when 3D is active
    }

    W, H = 680, 480
    root = tk.Tk()
    root.title(f"VSEPR-SIM  |  {symbol} \u2014 {info['name']}  [{lattice}]")
    root.configure(bg="#0d0d1a")
    root.resizable(False, False)

    sw = root.winfo_screenwidth()
    sh = root.winfo_screenheight()
    rx = random.randint(40, max(41, sw - W - 40))
    ry = random.randint(40, max(41, sh - H - 80))
    root.geometry(f"{W}x{H}+{rx}+{ry}")

    canvas = tk.Canvas(root, width=W, height=H, bg="#0d0d1a", highlightthickness=0)
    canvas.pack(fill="both", expand=True)

    # ── Limit-warning overlay ────────────────────────────────────────────────
    def _show_limit_warning():
        canvas.create_rectangle(180, 170, 500, 270, fill="#1a0010",
                                 outline="#ff2244", width=2)
        canvas.create_text(340, 200, text="\u26a0  3D card limit reached",
                           fill="#ff2244", font=("Consolas", 13, "bold"))
        canvas.create_text(340, 225,
                           text=f"Max {MAX_3D_CARDS} simultaneous 3D cards.",
                           fill="#cc8888", font=("Consolas", 9))
        canvas.create_text(340, 245,
                           text="Close another card to enable 3D here.",
                           fill="#886666", font=("Consolas", 8))

    # ── Core redraw ──────────────────────────────────────────────────────────
    def _redraw():
        canvas.delete("all")
        _draw_header()
        _draw_element_card()
        _draw_score_section()
        _draw_lattice_descriptor()
        _draw_footer()
        if state["mode"] == "3D":
            _draw_3d_crystal()
        else:
            _draw_2d_crystal()
        _draw_toggles()

    # ── Header ───────────────────────────────────────────────────────────────
    def _draw_header():
        canvas.create_rectangle(0, 0, W, 44, fill="#111130", outline="")
        canvas.create_text(16, 22, text="VSEPR-SIM  V4 Beta", fill="#00d4ff",
                           anchor="w", font=("Consolas", 11, "bold"))
        mode_tag = f"[{state['mode']}]"
        if state["mode"] == "3D":
            cl = COMPLEXITY[state["cidx"]]
            mode_tag = f"[3D \u2022 {cl[0]}]"
        canvas.create_text(W - 16, 22,
                           text=f"Pre-Thermal Classification  |  Host: {symbol}  {mode_tag}",
                           fill="#888888", anchor="e", font=("Consolas", 9))
        canvas.create_line(0, 44, W, 44, fill="#00d4ff", width=1)

    # ── Element info card ─────────────────────────────────────────────────────
    def _draw_element_card():
        card_x, card_y = 16, 56
        canvas.create_rectangle(card_x, card_y, card_x + 140, card_y + 180,
                                 fill="#16213e", outline="#334", width=1)
        canvas.create_text(card_x + 10, card_y + 12, text=str(info["Z"]),
                           fill="#555577", anchor="w", font=("Consolas", 9))
        canvas.create_text(card_x + 70, card_y + 62, text=symbol,
                           fill=elem_color, font=("Consolas", 40, "bold"))
        canvas.create_text(card_x + 70, card_y + 100, text=info["name"],
                           fill="#cccccc", font=("Consolas", 10))
        badge_colors = {"FCC": "#1a4a1a", "BCC": "#1a1a4a", "HCP": "#4a1a2a"}
        canvas.create_rectangle(card_x + 30, card_y + 112, card_x + 112, card_y + 128,
                                 fill=badge_colors.get(lattice, "#333"), outline="#555")
        canvas.create_text(card_x + 71, card_y + 120, text=lattice,
                           fill="#00d4ff", font=("Consolas", 9, "bold"))
        canvas.create_text(card_x + 10, card_y + 142, anchor="w",
                           text=f"a\u2080  = {info['a0']:.3f} \u00c5",
                           fill="#aaaaaa", font=("Consolas", 8))
        canvas.create_text(card_x + 10, card_y + 156, anchor="w",
                           text=f"Tm  = {info['Tmelt']:.0f} K",
                           fill="#aaaaaa", font=("Consolas", 8))
        canvas.create_text(card_x + 10, card_y + 170, anchor="w",
                           text=f"Ec  = {info['Ecoh']:.2f} eV",
                           fill="#aaaaaa", font=("Consolas", 8))

    # ── Score bars + radar ────────────────────────────────────────────────────
    def _draw_score_section():
        if gamma is None and q_data is None and compact is None:
            return
        bx, by = 16, 260
        bw, bh = 140, 12
        canvas.create_text(bx, by - 14, text="V4 Meta-Scores",
                           fill="#00d4ff", anchor="w", font=("Consolas", 9, "bold"))
        gv = gamma   if gamma   is not None else 0.0
        qv = q_data  if q_data  is not None else 0.0
        cv = compact if compact is not None else 0.0
        draw_score_bar(canvas, bx + 50, by,      bw, bh, gv, "\u03b3",       "#44aaff")
        draw_score_bar(canvas, bx + 50, by + 20, bw, bh, qv, "Q_data",  "#44ffaa")
        draw_score_bar(canvas, bx + 50, by + 40, bw, bh, cv, "C_cmpct", "#ffaa44")
        if all(v is not None for v in [gamma, q_data, compact]):
            radar_cx, radar_cy, radar_r = 105, by + 110, 38
            canvas.create_text(radar_cx, radar_cy - radar_r - 10,
                               text="Profile", fill="#555577", font=("Consolas", 7))
            angles = [math.radians(-90), math.radians(30), math.radians(150)]
            vals   = [gv, qv, cv]
            labels = ["\u03b3", "Q", "C"]
            pts = []
            for ang, v in zip(angles, vals):
                px = radar_cx + radar_r * v * math.cos(ang)
                py = radar_cy + radar_r * v * math.sin(ang)
                pts.append(px); pts.append(py)
            for ang, lbl in zip(angles, labels):
                ex = radar_cx + radar_r * math.cos(ang)
                ey = radar_cy + radar_r * math.sin(ang)
                canvas.create_line(radar_cx, radar_cy, ex, ey, fill="#333355", width=1)
                canvas.create_text(ex + 10 * math.cos(ang), ey + 10 * math.sin(ang),
                                   text=lbl, fill="#667799", font=("Consolas", 7))
            if len(pts) >= 6:
                canvas.create_polygon(pts, fill="", outline="#00d4ff", width=1)

    # ── Lattice descriptor (right) ────────────────────────────────────────────
    def _draw_lattice_descriptor():
        desc_x, desc_y = W - 170, 60
        canvas.create_text(desc_x, desc_y, text="Lattice Descriptor",
                           fill="#00d4ff", anchor="w", font=("Consolas", 9, "bold"))
        lattice_notes = {
            "FCC": ["12-fold coordination", "ABCABC stacking", "Packing \u03b7 = 0.740",
                    "Space group: Fm-3m", "Slip: {111}<110>"],
            "BCC": ["8-fold coordination",  "ABAB stacking",  "Packing \u03b7 = 0.680",
                    "Space group: Im-3m",   "Slip: {110}<111>"],
            "HCP": ["12-fold coordination", "ABABAB stacking","Packing \u03b7 = 0.740",
                    "Space group: P6\u2083/mmc","Slip: {0001}<1120>"],
        }
        for i, note in enumerate(lattice_notes.get(lattice, [])):
            canvas.create_text(desc_x, desc_y + 20 + i * 16, text=f"\u2022 {note}",
                               fill="#888899", anchor="w", font=("Consolas", 8))

    # ── Footer ────────────────────────────────────────────────────────────────
    def _draw_footer():
        canvas.create_line(0, H - 30, W, H - 30, fill="#333355", width=1)
        canvas.create_text(16, H - 15, anchor="w",
                           text=f"VSEPR-SIM  |  {lattice} host: {symbol}  |  Pre-thermal snapshot",
                           fill="#444466", font=("Consolas", 7))
        canvas.create_text(W - 16, H - 15, anchor="e",
                           text="auto-close 8s",
                           fill="#333355", font=("Consolas", 7))

    # ── 2D crystal cartoon ────────────────────────────────────────────────────
    def _draw_2d_crystal():
        cryst_cx, cryst_cy, scale = 340, 230, 46
        r_atom = max(6, int(scale * 0.32))
        r_face = max(4, int(scale * 0.22))
        if lattice == "FCC":
            sites = fcc_2d_sites(cryst_cx, cryst_cy, scale)
            bond_t = scale * 0.78
        elif lattice == "BCC":
            sites = bcc_2d_sites(cryst_cx, cryst_cy, scale)
            bond_t = scale * 0.78
        else:
            sites = hcp_2d_sites(cryst_cx, cryst_cy, scale)
            bond_t = scale * 0.72
        vis = [(x, y, fc) for x, y, fc in sites
               if 180 <= x <= W - 40 and 60 <= y <= H - 100]
        for i, j in bond_pairs_2d(vis, bond_t):
            x1, y1, _ = vis[i]; x2, y2, _ = vis[j]
            canvas.create_line(x1, y1, x2, y2, fill="#334455", width=1)
        canvas.create_text(cryst_cx, 66, text=f"{lattice} \u2014 [{symbol}]",
                           fill="#00d4ff", font=("Consolas", 10, "bold"))
        for x, y, is_face in vis:
            r   = r_face if is_face else r_atom
            col = elem_color2 if is_face else elem_color
            canvas.create_oval(x-r-3, y-r-3, x+r+3, y+r+3,
                               fill="", outline=elem_color, width=1, stipple="gray25")
            canvas.create_oval(x-r, y-r, x+r, y+r,
                               fill=col, outline="#cccccc", width=1)
        cx0, cy0, _ = vis[len(vis) // 2]
        canvas.create_oval(cx0-r_atom-5, cy0-r_atom-5,
                           cx0+r_atom+5, cy0+r_atom+5,
                           fill="", outline="#00ffff", width=2)
        canvas.create_text(cx0, cy0 + r_atom + 10, text="host",
                           fill="#00ffff", font=("Consolas", 7))
        uc_x = cryst_cx - scale; uc_y = cryst_cy - scale
        canvas.create_rectangle(uc_x, uc_y, uc_x + scale, uc_y + scale,
                                 fill="", outline="#0077bb", width=1, dash=(4, 4))

    # ── 3D orthographic crystal ───────────────────────────────────────────────
    def _draw_3d_crystal():
        cidx  = state["cidx"]
        _, dims, px_cell = COMPLEXITY[cidx]
        nx, ny, nz = dims

        if lattice == "FCC":
            raw = fcc_3d_sites(nx, ny, nz)
            bt  = _BOND_T3D["FCC"]
        elif lattice == "BCC":
            raw = bcc_3d_sites(nx, ny, nz)
            bt  = _BOND_T3D["BCC"]
        else:
            raw = hcp_3d_sites(nx, ny, nz)
            bt  = _BOND_T3D["HCP"]

        raw = _l_segment_trim(raw, nx, ny, cidx)

        # Centre supercell at origin
        mx = sum(p[0] for p in raw) / len(raw) if raw else 0
        my = sum(p[1] for p in raw) / len(raw) if raw else 0
        mz = sum(p[2] for p in raw) / len(raw) if raw else 0
        sites3 = [(x - mx, y - my, z - mz) for x, y, z in raw]

        cryst_cx, cryst_cy = 340, 235
        projected = [project_ortho(p, VIEW_AZIMUTH, VIEW_ELEVATION,
                                   px_cell, cryst_cx, cryst_cy)
                     for p in sites3]

        pairs = bond_pairs_3d(sites3, bt)

        # Sort atoms back-to-front (ascending depth)
        order = sorted(range(len(projected)), key=lambda i: projected[i][2])

        r_atom = max(4, int(px_cell * 0.28))

        # Draw bonds
        for i, j in pairs:
            sx1, sy1, _ = projected[i]
            sx2, sy2, _ = projected[j]
            if (180 <= sx1 <= W - 40 and 60 <= sy1 <= H - 100 and
                    180 <= sx2 <= W - 40 and 60 <= sy2 <= H - 100):
                canvas.create_line(sx1, sy1, sx2, sy2, fill="#334455", width=1)

        # Draw atoms back-to-front
        for idx in order:
            sx, sy, depth = projected[idx]
            if not (180 <= sx <= W - 40 and 60 <= sy <= H - 100):
                continue
            # Depth-shade: farther = darker
            d_norm = max(0.0, min(1.0, (depth + 4) / 8.0))
            shade  = int(80 + 140 * d_norm)
            shade  = max(0, min(255, shade))
            base   = elem_color.lstrip("#")
            br, bg_, bb = int(base[0:2], 16), int(base[2:4], 16), int(base[4:6], 16)
            sr = int(br * shade / 255)
            sg = int(bg_ * shade / 255)
            sb = int(bb * shade / 255)
            col = f"#{sr:02x}{sg:02x}{sb:02x}"
            canvas.create_oval(sx-r_atom-2, sy-r_atom-2, sx+r_atom+2, sy+r_atom+2,
                               fill="", outline=elem_color, width=1, stipple="gray25")
            canvas.create_oval(sx-r_atom, sy-r_atom, sx+r_atom, sy+r_atom,
                               fill=col, outline="#aaaaaa", width=1)

        cl = COMPLEXITY[cidx]
        canvas.create_text(cryst_cx, 66,
                           text=f"{lattice} \u2014 [{symbol}]  3D {cl[0]}",
                           fill="#00d4ff", font=("Consolas", 10, "bold"))
        canvas.create_text(cryst_cx, 82,
                           text=f"az={math.degrees(VIEW_AZIMUTH):.0f}\u00b0  "
                                f"el={math.degrees(VIEW_ELEVATION):.0f}\u00b0  "
                                f"{len(sites3)} atoms",
                           fill="#555577", font=("Consolas", 7))

    # ── Toggle buttons ────────────────────────────────────────────────────────
    def _draw_toggles():
        # [2D] / [3D] toggle
        mode_col_2d = "#00d4ff" if state["mode"] == "2D" else "#334466"
        mode_col_3d = "#00d4ff" if state["mode"] == "3D" else "#334466"
        canvas.create_rectangle(176, 52, 222, 68, fill="#111130",
                                 outline=mode_col_2d, width=1, tags="btn_2d")
        canvas.create_text(199, 60, text="2D", fill=mode_col_2d,
                           font=("Consolas", 8, "bold"), tags="btn_2d")
        canvas.create_rectangle(226, 52, 272, 68, fill="#111130",
                                 outline=mode_col_3d, width=1, tags="btn_3d")
        canvas.create_text(249, 60, text="3D", fill=mode_col_3d,
                           font=("Consolas", 8, "bold"), tags="btn_3d")

        # Complexity buttons (only visible in 3D)
        if state["mode"] == "3D":
            for i, (lbl, _, _) in enumerate(COMPLEXITY):
                bx0 = 276 + i * 46
                active = (i == state["cidx"])
                oc = "#00ffaa" if active else "#334455"
                canvas.create_rectangle(bx0, 52, bx0 + 42, 68, fill="#111130",
                                         outline=oc, width=1,
                                         tags=f"btn_cplx_{i}")
                canvas.create_text(bx0 + 21, 60, text=lbl, fill=oc,
                                   font=("Consolas", 8), tags=f"btn_cplx_{i}")

        canvas.tag_bind("btn_2d",  "<Button-1>", lambda e: _on_set_mode("2D"))
        canvas.tag_bind("btn_3d",  "<Button-1>", lambda e: _on_set_mode("3D"))
        for i in range(len(COMPLEXITY)):
            canvas.tag_bind(f"btn_cplx_{i}", "<Button-1>",
                            lambda e, ci=i: _on_set_cidx(ci))

    # ── Mode-switch handlers ──────────────────────────────────────────────────
    def _on_set_mode(mode):
        if mode == state["mode"]:
            return
        if mode == "3D":
            active = _count_active_3d_cards()
            if active >= MAX_3D_CARDS:
                canvas.delete("all")
                _draw_header()
                _show_limit_warning()
                _draw_toggles()
                return
            state["lock_path"] = _register_3d_card()
            _export_card_snapshot(symbol, lattice, gamma, q_data, compact,
                                   "3D", state["cidx"])
        else:
            _unregister_card(state["lock_path"])
            state["lock_path"] = None
        state["mode"] = mode
        _redraw()

    def _on_set_cidx(cidx):
        if state["mode"] != "3D":
            return
        state["cidx"] = cidx
        _export_card_snapshot(symbol, lattice, gamma, q_data, compact,
                               "3D", cidx)
        _redraw()

    # ── Close + auto-close ────────────────────────────────────────────────────
    def _close():
        _unregister_card(state["lock_path"])
        try:
            root.destroy()
        except Exception:
            pass

    root.protocol("WM_DELETE_WINDOW", _close)

    # Export initial 2D snapshot
    _export_card_snapshot(symbol, lattice, gamma, q_data, compact, "2D", 0)

    _redraw()
    root.after(8000, _close)
    root.mainloop()

# ── Entry point ─────────────────────────────────────────────────────────────────

if __name__ == "__main__":
    sym     = sys.argv[1] if len(sys.argv) > 1 else "Fe"
    lat     = sys.argv[2] if len(sys.argv) > 2 else None
    gamma   = float(sys.argv[3]) if len(sys.argv) > 3 else None
    q_data  = float(sys.argv[4]) if len(sys.argv) > 4 else None
    compact = float(sys.argv[5]) if len(sys.argv) > 5 else None

    # 0.08 s delay after classification options collected
    time.sleep(0.08)
    build_popup(sym, lat, gamma, q_data, compact)
