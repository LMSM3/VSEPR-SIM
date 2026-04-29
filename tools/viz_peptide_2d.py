#!/usr/bin/env python3
"""
viz_peptide_2d.py — 2D Peptide Formation Analysis Viewer (Port 8899 UDP)
=========================================================================
Receives stochastic peptide formation JSON frames and renders:
  - Live energy timeline (total, bond, vdw, coulomb, solvation)
  - Ramachandran scatter (phi/psi per residue)
  - Score bar chart (steric, electrostatic, hydrophobic, planarity, confidence)
  - Sequence/environment/state per run

Usage:
    python tools/viz_peptide_2d.py           (listens on UDP 8899)

Then run:
    peptide-stochastic-viz                   (in another terminal)

Or use the unified host instead:
    python tools/viz_host.py                 (listens on ALL 999X + 8899)

Controls:
    N             Next run
    P             Previous run
    Space         Pause/resume
    C             Cycle chart mode
    Q / Escape    Quit

Requires: Python 3.6+, tkinter (standard library only)
"""

import socket, json, threading, math, time, tkinter as tk
from collections import deque

HOST = "127.0.0.1"
PORT = 8899

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

FONT_T  = ("Consolas", 12, "bold")
FONT_S  = ("Consolas", 9)
FONT_XS = ("Consolas", 7)

ENERGY_COLORS = {
    "total": "#FFFFFF",
    "bond": "#44FF44",
    "vdw": "#FF8844",
    "coulomb": "#4488FF",
    "solvation": "#FF44FF",
    "formation": "#FFFF44",
}

SCORE_COLORS = {
    "steric": "#44FF44",
    "electrostatic": "#4488FF",
    "hydrophobic": "#FF8844",
    "planarity": "#FF44FF",
    "confidence": "#FFD700",
}

SC_CLASS_NAMES = {
    0: "none", 1: "hydrophobic", 2: "polar", 3: "acidic",
    4: "basic", 5: "aromatic", 6: "sulfur"
}


class PeptideViewer2D:
    def __init__(self, root):
        self.root = root
        self.root.title("VSEPR-SIM | Peptide 2D Analysis | Port 8899")
        self.root.configure(bg=BG)
        self.root.geometry("1300x750")

        self.frames = deque(maxlen=64)
        self.current_idx = -1
        self.running = True
        self.paused = False
        self.chart_mode = 0  # 0=energy+score, 1=ramachandran, 2=residue breakdown

        self._build_ui()
        self._start_receiver()
        self._poll()

    def _build_ui(self):
        top = tk.Frame(self.root, bg=HDR_BG, height=40)
        top.pack(fill=tk.X); top.pack_propagate(False)
        tk.Label(top, text="  VSEPR-SIM  Peptide 2D Analysis  :8899",
                 font=FONT_T, fg=ACCENT, bg=HDR_BG).pack(side=tk.LEFT, padx=8)
        self.lbl_status = tk.Label(top, text="Waiting...", font=FONT_S, fg=ACCENT3, bg=HDR_BG)
        self.lbl_status.pack(side=tk.RIGHT, padx=8)
        self.lbl_mode = tk.Label(top, text="[energy+score]", font=FONT_XS, fg=DIM, bg=HDR_BG)
        self.lbl_mode.pack(side=tk.RIGHT, padx=4)

        # Info bar
        info = tk.Frame(self.root, bg=PANEL_BG, height=30)
        info.pack(fill=tk.X); info.pack_propagate(False)
        self.lbl_info = tk.Label(info, text="", font=FONT_S, fg=ACCENT2, bg=PANEL_BG)
        self.lbl_info.pack(side=tk.LEFT, padx=8)

        self.cvs = tk.Canvas(self.root, bg=BG, highlightthickness=0)
        self.cvs.pack(fill=tk.BOTH, expand=True)

        self.root.bind("<KeyPress-n>", lambda e: self._next())
        self.root.bind("<KeyPress-p>", lambda e: self._prev())
        self.root.bind("<space>", lambda e: self._toggle_pause())
        self.root.bind("<KeyPress-c>", lambda e: self._cycle_chart())
        self.root.bind("<Escape>", lambda e: self._quit())
        self.root.bind("<KeyPress-q>", lambda e: self._quit())

    def _start_receiver(self):
        def receiver():
            sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            sock.bind((HOST, PORT))
            sock.settimeout(0.5)
            while self.running:
                try:
                    data, _ = sock.recvfrom(65536)
                    frame = json.loads(data.decode("utf-8"))
                    self.frames.append(frame)
                    if not self.paused:
                        self.current_idx = len(self.frames) - 1
                except socket.timeout:
                    pass
                except Exception:
                    pass
            sock.close()
        t = threading.Thread(target=receiver, daemon=True)
        t.start()

    def _poll(self):
        if self.frames and 0 <= self.current_idx < len(self.frames):
            frame = self.frames[self.current_idx]
            self._render(frame)
            self.lbl_status.config(
                text=f"Run {frame.get('run_id','?')} | {len(self.frames)} total",
                fg=SUCCESS)
            seq = frame.get("sequence", "?")
            env = frame.get("environment", "?")
            state = frame.get("state", "?")
            self.lbl_info.config(
                text=f"Seq: {seq}  |  Env: {env}  |  State: {state}")
        self.root.after(60, self._poll)

    def _render(self, frame):
        self.cvs.delete("all")
        w = self.cvs.winfo_width()
        h = self.cvs.winfo_height()

        if not frame.get("success", False):
            self.cvs.create_text(w/2, h/2,
                                 text=f"Run {frame.get('run_id','?')} FAILED\n"
                                      f"{frame.get('error','')}",
                                 fill=ACCENT3, font=FONT_T)
            return

        if self.chart_mode == 0:
            self._draw_energy_timeline(frame, 10, 10, w/2 - 20, h - 30)
            self._draw_score_bars(frame, w/2 + 10, 10, w/2 - 20, h/2 - 20)
            self._draw_residue_strip(frame, w/2 + 10, h/2, w/2 - 20, h/2 - 20)
        elif self.chart_mode == 1:
            self._draw_ramachandran(frame, 10, 10, w - 20, h - 30)
        elif self.chart_mode == 2:
            self._draw_residue_breakdown(frame, 10, 10, w - 20, h - 30)

    def _draw_energy_timeline(self, frame, ox, oy, pw, ph):
        """Energy bar chart for all runs received so far."""
        self.cvs.create_text(ox + pw/2, oy + 10, text="Energy Landscape (All Runs)",
                             fill=ACCENT, font=FONT_S)
        self.cvs.create_rectangle(ox, oy + 25, ox + pw, oy + ph, outline=DIM)

        chart_y = oy + 35
        chart_h = ph - 45
        n = len(self.frames)
        if n == 0: return

        # Collect energy values
        energies = []
        for f in self.frames:
            if f.get("success"):
                energies.append(f.get("energy", {}).get("total", 0))
            else:
                energies.append(None)

        valid = [e for e in energies if e is not None]
        if not valid: return

        e_min = min(valid) - 10
        e_max = max(valid) + 10
        if abs(e_max - e_min) < 1: e_max = e_min + 1

        bar_w = max(2, (pw - 10) / n)
        for i, e in enumerate(energies):
            if e is None: continue
            norm = (e - e_min) / (e_max - e_min)
            bar_h = max(2, norm * chart_h)
            x = ox + 5 + i * bar_w
            y_top = chart_y + chart_h - bar_h

            color = "#44FF44" if e < 0 else "#FF4444"
            if i == self.current_idx:
                color = GOLD
            self.cvs.create_rectangle(x, y_top, x + bar_w - 1, chart_y + chart_h,
                                       fill=color, outline="")

        # Axis labels
        self.cvs.create_text(ox + 2, chart_y, text=f"{e_max:.0f}", fill=DIM,
                             font=FONT_XS, anchor="nw")
        self.cvs.create_text(ox + 2, chart_y + chart_h, text=f"{e_min:.0f}", fill=DIM,
                             font=FONT_XS, anchor="sw")

        # Legend
        lx = ox + pw - 100
        for j, (name, color) in enumerate(ENERGY_COLORS.items()):
            self.cvs.create_rectangle(lx, chart_y + j*12, lx + 8, chart_y + j*12 + 8,
                                       fill=color, outline="")
            self.cvs.create_text(lx + 12, chart_y + j*12, text=name, fill=DIM,
                                 font=FONT_XS, anchor="nw")

    def _draw_score_bars(self, frame, ox, oy, pw, ph):
        """Horizontal bar chart of scores for current run."""
        self.cvs.create_text(ox + pw/2, oy + 10, text="Quality Scores",
                             fill=ACCENT, font=FONT_S)

        scores = frame.get("scores", {})
        bar_y = oy + 30
        bar_h = 16
        gap = 6

        for name, color in SCORE_COLORS.items():
            val = scores.get(name, 0)
            bar_w = max(0, val * (pw - 100))

            self.cvs.create_text(ox + 2, bar_y + bar_h/2, text=name,
                                 fill=FG, font=FONT_XS, anchor="w")
            self.cvs.create_rectangle(ox + 90, bar_y, ox + 90 + bar_w, bar_y + bar_h,
                                       fill=color, outline="")
            self.cvs.create_rectangle(ox + 90, bar_y, ox + pw, bar_y + bar_h,
                                       outline=DIM)
            self.cvs.create_text(ox + pw + 4, bar_y + bar_h/2, text=f"{val:.3f}",
                                 fill=DIM, font=FONT_XS, anchor="w")
            bar_y += bar_h + gap

    def _draw_residue_strip(self, frame, ox, oy, pw, ph):
        """Colored strip showing residue sequence with sidechain class coloring."""
        self.cvs.create_text(ox + pw/2, oy + 10, text="Residue Sequence",
                             fill=ACCENT, font=FONT_S)

        residues = frame.get("residues", [])
        if not residues: return

        strip_y = oy + 30
        strip_h = 40
        n = len(residues)
        box_w = min(50, (pw - 10) / n)

        for i, res in enumerate(residues):
            x = ox + 5 + i * box_w
            sc = res.get("sc_class", 0)
            color = {0: "#444", 1: "#228B22", 2: "#4488FF", 3: "#FF4444",
                     4: "#4444FF", 5: "#FF8800", 6: "#FFFF00"}.get(sc, "#444")

            self.cvs.create_rectangle(x, strip_y, x + box_w - 2, strip_y + strip_h,
                                       fill=color, outline=DIM)
            self.cvs.create_text(x + box_w/2, strip_y + strip_h/2,
                                 text=res.get("name", "?")[:3],
                                 fill=FG, font=FONT_XS)

        # Legend
        ly = strip_y + strip_h + 10
        for sc_id, sc_name in SC_CLASS_NAMES.items():
            if sc_id == 0: continue
            color = {1: "#228B22", 2: "#4488FF", 3: "#FF4444",
                     4: "#4444FF", 5: "#FF8800", 6: "#FFFF00"}.get(sc_id, "#444")
            self.cvs.create_rectangle(ox + 5, ly, ox + 13, ly + 8, fill=color, outline="")
            self.cvs.create_text(ox + 17, ly, text=sc_name, fill=DIM, font=FONT_XS, anchor="nw")
            ly += 12

    def _draw_ramachandran(self, frame, ox, oy, pw, ph):
        """Ramachandran plot (phi/psi scatter) for all received runs."""
        self.cvs.create_text(ox + pw/2, oy + 10, text="Ramachandran Plot (All Runs)",
                             fill=ACCENT, font=FONT_S)

        plot_x = ox + 40
        plot_y = oy + 30
        plot_w = pw - 60
        plot_h = ph - 50

        # Axes
        self.cvs.create_rectangle(plot_x, plot_y, plot_x + plot_w, plot_y + plot_h,
                                   outline=DIM)
        self.cvs.create_text(plot_x + plot_w/2, plot_y + plot_h + 12, text="phi (deg)",
                             fill=FG, font=FONT_XS)
        self.cvs.create_text(plot_x - 20, plot_y + plot_h/2, text="psi",
                             fill=FG, font=FONT_XS)

        # Tick marks
        for val in [-180, -90, 0, 90, 180]:
            nx = (val + 180) / 360
            self.cvs.create_text(plot_x + nx * plot_w, plot_y + plot_h + 3,
                                 text=str(val), fill=DIM, font=FONT_XS, anchor="n")
            ny = 1 - (val + 180) / 360
            self.cvs.create_text(plot_x - 5, plot_y + ny * plot_h,
                                 text=str(val), fill=DIM, font=FONT_XS, anchor="e")

        # Plot points from all frames
        for fi, f in enumerate(self.frames):
            if not f.get("success"): continue
            is_current = (fi == self.current_idx)
            for res in f.get("residues", []):
                phi = res.get("phi", 0)
                psi = res.get("psi", 0)
                nx = (phi + 180) / 360
                ny = 1 - (psi + 180) / 360
                px = plot_x + nx * plot_w
                py = plot_y + ny * plot_h

                sc = res.get("sc_class", 0)
                if is_current:
                    color = GOLD
                    r = 4
                else:
                    color = {0: "#666", 1: "#228B22", 2: "#4488FF", 3: "#FF4444",
                             4: "#4444FF", 5: "#FF8800", 6: "#FFFF00"}.get(sc, "#666")
                    r = 2

                self.cvs.create_oval(px - r, py - r, px + r, py + r,
                                     fill=color, outline="")

    def _draw_residue_breakdown(self, frame, ox, oy, pw, ph):
        """Per-residue phi/psi/omega and sidechain class table."""
        self.cvs.create_text(ox + pw/2, oy + 10,
                             text=f"Residue Detail — Run {frame.get('run_id','?')}",
                             fill=ACCENT, font=FONT_S)

        residues = frame.get("residues", [])
        col_w = pw / 6
        headers = ["Residue", "SC Class", "Phi", "Psi", "Omega", "ID"]
        for j, h in enumerate(headers):
            self.cvs.create_text(ox + j * col_w + col_w/2, oy + 30, text=h,
                                 fill=ACCENT2, font=FONT_XS)

        for i, res in enumerate(residues):
            y = oy + 48 + i * 16
            if y > oy + ph - 10: break
            vals = [
                res.get("name", "?"),
                SC_CLASS_NAMES.get(res.get("sc_class", 0), "?"),
                f"{res.get('phi', 0):+.1f}",
                f"{res.get('psi', 0):+.1f}",
                f"{res.get('omega', 0):+.1f}",
                str(res.get("id", "?")),
            ]
            for j, v in enumerate(vals):
                self.cvs.create_text(ox + j * col_w + col_w/2, y, text=v,
                                     fill=FG, font=FONT_XS)

    def _next(self):
        if self.frames:
            self.current_idx = (self.current_idx + 1) % len(self.frames)

    def _prev(self):
        if self.frames:
            self.current_idx = (self.current_idx - 1) % len(self.frames)

    def _toggle_pause(self):
        self.paused = not self.paused

    def _cycle_chart(self):
        self.chart_mode = (self.chart_mode + 1) % 3
        modes = ["energy+score", "ramachandran", "residue detail"]
        self.lbl_mode.config(text=f"[{modes[self.chart_mode]}]")

    def _quit(self):
        self.running = False
        self.root.destroy()


if __name__ == "__main__":
    root = tk.Tk()
    app = PeptideViewer2D(root)
    root.protocol("WM_DELETE_WINDOW", app._quit)
    root.mainloop()
