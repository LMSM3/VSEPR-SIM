#!/usr/bin/env python3
"""
gas2_heatmap.py - MB Sample Heat Map, Atom Grid & Live Graph Visualisation
===========================================================================

Reads JSON frames from vsepr gas2 heatmap via stdin pipe and displays in a
steady-state Tkinter window:

  Row 1:  Velocity heat map  |  Speed-KE heat map  |  Speed distribution curve
  Row 2:  Atom grid           |  Shell matrix        |  Temperature timeline

Resolution:  48x48 / 64x64 / 256x256  (selectable via toolbar)
Refresh:     ~30 fps target

Usage:
    vsepr gas2 heatmap Ar -T 300 -N 10000 --grid 64 | python tools/gas2_heatmap.py

Requires: Python 3.6+, tkinter (standard library)
"""

import sys
import json
import threading
import math
import tkinter as tk
from tkinter import ttk

BG_COLOR    = "#0a0a0f"
PANEL_BG    = "#12121a"
HEADER_BG   = "#16161e"
FG_COLOR    = "#e0e0e0"
ACCENT      = "#bb86fc"
ACCENT2     = "#03dac6"
ACCENT3     = "#cf6679"
GRID_LINE   = "#222233"
DIM         = "#555566"
SUCCESS     = "#4CAF50"

FONT_TITLE  = ("Consolas", 13, "bold")
FONT_PANEL  = ("Consolas", 10, "bold")
FONT_SMALL  = ("Consolas", 9)
FONT_TINY   = ("Consolas", 7)

INFERNO_RAMP = [
    (0, 0, 4),    (7, 2, 25),   (20, 4, 54),   (40, 8, 78),
    (63, 6, 97),  (87, 7, 102), (110, 15, 96),  (133, 28, 83),
    (155, 43, 68),(175, 60, 52), (194, 78, 38),  (210, 99, 26),
    (224, 121, 17),(235, 147, 10),(244, 174, 8),  (250, 202, 16),
    (252, 228, 41),(250, 251, 81),(252, 254, 164),(252, 255, 252),
]

def inferno(t):
    t = max(0.0, min(1.0, t))
    n = len(INFERNO_RAMP) - 1
    idx = t * n;  lo = int(idx);  hi = min(lo + 1, n);  frac = idx - lo
    r0, g0, b0 = INFERNO_RAMP[lo];  r1, g1, b1 = INFERNO_RAMP[hi]
    return f"#{int(r0+frac*(r1-r0)):02x}{int(g0+frac*(g1-g0)):02x}{int(b0+frac*(b1-b0)):02x}"

def viridis(t):
    t = max(0.0, min(1.0, t))
    r = int(68 + t * (253 - 68));  g = int(1 + t * (231 - 1))
    b = int(84 + (0.5 - abs(t - 0.5)) * 2 * (170 - 84))
    return f"#{min(r,255):02x}{min(g,255):02x}{min(b,255):02x}"

INFERNO_LUT = [inferno(i / 255.0) for i in range(256)]
VIRIDIS_LUT = [viridis(i / 255.0) for i in range(256)]

SPARKLE = ["*", "+", "o", "~", ":", "^"]

def sparkle_title(text, idx=0):
    c = SPARKLE[idx % len(SPARKLE)]
    return f" {c}  {text}  {c} "

class HeatmapViewer:
    def __init__(self, root):
        self.root = root
        self.root.title("VSEPR-SIM gas2 - Heat Maps, Graphs & Atom Grid")
        self.root.configure(bg=BG_COLOR)
        self.root.geometry("1680x920")
        self.root.minsize(1200, 750)
        self.latest = None
        self.running = True
        self.frame_count = 0
        self.grid_res = 64
        self.slowdown = 48
        self.target_fps = 30
        self.poll_ms = max(1, 1000 // self.target_fps)
        self._vv_photo = None
        self._se_photo = None
        self._grid_photo = None
        self._shell_photo = None
        self._temp_history = []
        self._vrms_history = []
        self._ke_history = []
        self._max_history = 200
        self._build_ui()
        self._start_reader()
        self._poll()

    def _build_ui(self):
        top = tk.Frame(self.root, bg=HEADER_BG, height=46)
        top.pack(fill=tk.X);  top.pack_propagate(False)
        tk.Label(top, text="  gas2 Heat Map & Graph Viewer",
                 font=FONT_TITLE, fg=ACCENT, bg=HEADER_BG).pack(side=tk.LEFT, padx=8)
        self.lbl_status = tk.Label(top, text="Waiting for data...",
                                    font=FONT_SMALL, fg=DIM, bg=HEADER_BG)
        self.lbl_status.pack(side=tk.RIGHT, padx=8)
        res_frame = tk.Frame(top, bg=HEADER_BG)
        res_frame.pack(side=tk.RIGHT, padx=12)
        tk.Label(res_frame, text="Grid:", font=FONT_TINY, fg="#888", bg=HEADER_BG).pack(side=tk.LEFT)
        self._res_var = tk.StringVar(value="64")
        for sz in ("48", "64", "256"):
            rb = tk.Radiobutton(res_frame, text=sz, variable=self._res_var, value=sz,
                                font=FONT_TINY, fg=FG_COLOR, bg=HEADER_BG,
                                selectcolor="#333", activebackground=HEADER_BG,
                                command=self._on_res_change)
            rb.pack(side=tk.LEFT, padx=3)
        self.lbl_fps = tk.Label(top, text="", font=FONT_TINY, fg=DIM, bg=HEADER_BG)
        self.lbl_fps.pack(side=tk.RIGHT, padx=8)

        body = tk.Frame(self.root, bg=BG_COLOR)
        body.pack(fill=tk.BOTH, expand=True, padx=6, pady=4)
        for c in range(3): body.columnconfigure(c, weight=1)
        for r in range(2): body.rowconfigure(r, weight=1)

        f_vv = self._panel(body, " vx - vy  Velocity Heat Map ")
        f_vv.grid(row=0, column=0, sticky="nsew", padx=3, pady=3)
        self.cvs_vv = tk.Canvas(f_vv, bg="#000000", highlightthickness=0)
        self.cvs_vv.pack(fill=tk.BOTH, expand=True, padx=2, pady=2)

        f_se = self._panel(body, " Speed - KE  Heat Map ")
        f_se.grid(row=0, column=1, sticky="nsew", padx=3, pady=3)
        self.cvs_se = tk.Canvas(f_se, bg="#000000", highlightthickness=0)
        self.cvs_se.pack(fill=tk.BOTH, expand=True, padx=2, pady=2)

        f_sdist = self._panel(body, " Speed Distribution  f(v) ")
        f_sdist.grid(row=0, column=2, sticky="nsew", padx=3, pady=3)
        self.cvs_sdist = tk.Canvas(f_sdist, bg="#080810", highlightthickness=0)
        self.cvs_sdist.pack(fill=tk.BOTH, expand=True, padx=2, pady=2)

        f_grid = self._panel(body, " Atom Grid - KE Coloured ")
        f_grid.grid(row=1, column=0, sticky="nsew", padx=3, pady=3)
        self.cvs_grid = tk.Canvas(f_grid, bg="#000000", highlightthickness=0)
        self.cvs_grid.pack(fill=tk.BOTH, expand=True, padx=2, pady=2)

        f_shell = self._panel(body, " Shell Matrix - Radial Speed Bins ")
        f_shell.grid(row=1, column=1, sticky="nsew", padx=3, pady=3)
        self.cvs_shell = tk.Canvas(f_shell, bg="#000000", highlightthickness=0)
        self.cvs_shell.pack(fill=tk.BOTH, expand=True, padx=2, pady=2)

        f_timeline = self._panel(body, " Temperature & v_rms Timeline ")
        f_timeline.grid(row=1, column=2, sticky="nsew", padx=3, pady=3)
        self.cvs_timeline = tk.Canvas(f_timeline, bg="#080810", highlightthickness=0)
        self.cvs_timeline.pack(fill=tk.BOTH, expand=True, padx=2, pady=2)

        info = tk.Frame(self.root, bg=PANEL_BG, height=32)
        info.pack(fill=tk.X);  info.pack_propagate(False)
        self.lbl_info = tk.Label(info, text="", font=FONT_SMALL, fg="#888", bg=PANEL_BG)
        self.lbl_info.pack(side=tk.LEFT, padx=10)
        self.cvs_legend = tk.Canvas(info, bg=PANEL_BG, highlightthickness=0, width=260, height=20)
        self.cvs_legend.pack(side=tk.RIGHT, padx=10, pady=2)
        self._draw_legend()
        self.lbl_fun = tk.Label(info, text="Science is beautiful", font=FONT_TINY, fg=ACCENT, bg=PANEL_BG)
        self.lbl_fun.pack(side=tk.RIGHT, padx=16)

    def _panel(self, parent, title):
        return tk.LabelFrame(parent, text=title, font=FONT_PANEL, fg=ACCENT, bg=PANEL_BG,
                             labelanchor="n", bd=1, relief=tk.GROOVE,
                             highlightbackground="#333344", highlightthickness=1)

    def _draw_legend(self):
        c = self.cvs_legend
        for i in range(256):
            c.create_line(2+i, 3, 2+i, 17, fill=INFERNO_LUT[i])
        c.create_text(2, 10, text="0", anchor="e", fill="#666", font=FONT_TINY)
        c.create_text(260, 10, text="max", anchor="w", fill="#666", font=FONT_TINY)

    def _on_res_change(self):
        self.grid_res = int(self._res_var.get())

    def _start_reader(self):
        def reader():
            try:
                for line in sys.stdin:
                    line = line.strip()
                    if not line: continue
                    try:
                        data = json.loads(line)
                        self.latest = data
                    except json.JSONDecodeError: pass
            except Exception: pass
            finally: self.running = False
        t = threading.Thread(target=reader, daemon=True);  t.start()

    def _poll(self):
        if self.latest is not None:
            data = self.latest;  self.latest = None;  self.frame_count += 1
            self._render(data)
        if self.running:
            self.root.after(self.poll_ms, self._poll)
        else:
            self.lbl_status.config(text="Stream ended", fg=ACCENT3)

    def _render(self, data):
        formula = data.get("formula", "?")
        T_K = data.get("T_K", 0.0);  cycle = data.get("cycle", 0)
        n_samp = data.get("n_samples", 0);  v_rms = data.get("v_rms", 1.0)
        grid_sz = data.get("grid_size", self.grid_res)
        ke_avg = data.get("ke_avg_Eh", 0.0)

        sp = sparkle_title(f"Frame {self.frame_count}  |  {formula} @ {T_K:.1f} K  |  "
                           f"{n_samp} samples  |  cycle {cycle}", self.frame_count)
        self.lbl_status.config(text=sp, fg=SUCCESS)

        self._temp_history.append((self.frame_count, T_K))
        self._vrms_history.append((self.frame_count, v_rms))
        self._ke_history.append((self.frame_count, ke_avg))
        if len(self._temp_history) > self._max_history:
            self._temp_history.pop(0);  self._vrms_history.pop(0);  self._ke_history.pop(0)

        vv_bins = data.get("vv_heatmap", []);  n_vv = data.get("vv_size", 0)
        if vv_bins and n_vv > 0:
            self._render_heatmap(self.cvs_vv, vv_bins, n_vv, "_vv_photo")

        se_bins = data.get("se_heatmap", []);  n_se = data.get("se_size", 0)
        if se_bins and n_se > 0:
            self._render_heatmap(self.cvs_se, se_bins, n_se, "_se_photo")

        grid_ke = data.get("atom_grid", []);  n_g = data.get("grid_size", 0)
        if grid_ke and n_g > 0:
            self._render_atom_grid(self.cvs_grid, grid_ke, n_g)

        shell = data.get("shell_matrix", []);  n_sh = data.get("shell_size", 0)
        if shell and n_sh > 0:
            self._render_heatmap(self.cvs_shell, shell, n_sh, "_shell_photo", lut=VIRIDIS_LUT)

        if vv_bins and n_vv > 0:
            self._render_speed_distribution(data, v_rms)
        self._render_timeline()

        self.lbl_info.config(
            text=f"<KE> = {ke_avg:.4e} Eh  |  v_rms = {v_rms:.1f} m/s  |  "
                 f"grid {grid_sz}x{grid_sz}  |  slowdown {self.slowdown}x  |  frames: {self.frame_count}")
        tags = ["Science is beautiful", "Maxwell-Boltzmann in action",
                "Deterministic | Inspectable | Fun", "Every atom tells a story",
                "Kinetic theory, visualised", "Research-grade simulation"]
        self.lbl_fun.config(text=tags[self.frame_count % len(tags)])

    def _render_heatmap(self, canvas, flat_bins, n, photo_attr, lut=None):
        if lut is None: lut = INFERNO_LUT
        cw = canvas.winfo_width();  ch = canvas.winfo_height()
        if cw < 20 or ch < 20: return
        max_val = max(flat_bins) if flat_bins else 1.0
        if max_val <= 0: max_val = 1.0
        inv_max = 1.0 / max_val
        photo = tk.PhotoImage(width=n, height=n)
        row_data = []
        for row in range(n):
            base = row * n;  pixels = []
            for col in range(n):
                idx = int(flat_bins[base + col] * inv_max * 255.0)
                if idx < 0: idx = 0
                elif idx > 255: idx = 255
                pixels.append(lut[idx])
            row_data.append("{" + " ".join(pixels) + "}")
        photo.put(" ".join(row_data))
        zoom = max(1, min(cw // n, ch // n))
        if zoom > 1: photo = photo.zoom(zoom, zoom)
        setattr(self, photo_attr, photo)
        canvas.delete("all")
        img_w = n * zoom;  img_h = n * zoom
        ox = (cw - img_w) // 2;  oy = (ch - img_h) // 2
        canvas.create_image(ox, oy, anchor=tk.NW, image=photo)

    def _render_atom_grid(self, canvas, grid_ke, n):
        cw = canvas.winfo_width();  ch = canvas.winfo_height()
        if cw < 20 or ch < 20: return
        canvas.delete("all")
        max_ke = max(grid_ke) if grid_ke else 1.0
        if max_ke <= 0: max_ke = 1.0
        inv_max = 1.0 / max_ke
        if n > 100:
            photo = tk.PhotoImage(width=n, height=n)
            row_data = []
            for row in range(n):
                base = row * n;  pixels = []
                for col in range(n):
                    idx = int(grid_ke[base + col] * inv_max * 255.0)
                    if idx < 0: idx = 0
                    elif idx > 255: idx = 255
                    pixels.append(VIRIDIS_LUT[idx])
                row_data.append("{" + " ".join(pixels) + "}")
            photo.put(" ".join(row_data))
            zoom = max(1, min(cw // n, ch // n))
            if zoom > 1: photo = photo.zoom(zoom, zoom)
            self._grid_photo = photo
            img_w = n * zoom;  img_h = n * zoom
            ox = (cw - img_w) // 2;  oy = (ch - img_h) // 2
            canvas.create_image(ox, oy, anchor=tk.NW, image=photo)
        else:
            cell_w = cw / n;  cell_h = ch / n;  cell = min(cell_w, cell_h)
            ox = (cw - cell * n) / 2.0;  oy = (ch - cell * n) / 2.0
            rad = cell * 0.38
            for row in range(n):
                base = row * n
                for col in range(n):
                    idx = int(grid_ke[base + col] * inv_max * 255.0)
                    if idx < 0: idx = 0
                    elif idx > 255: idx = 255
                    colour = VIRIDIS_LUT[idx]
                    cx = ox + (col + 0.5) * cell;  cy = oy + (row + 0.5) * cell
                    canvas.create_oval(cx-rad, cy-rad, cx+rad, cy+rad, fill=colour, outline="")

    def _render_speed_distribution(self, data, v_rms):
        cvs = self.cvs_sdist
        cw = cvs.winfo_width();  ch = cvs.winfo_height()
        if cw < 40 or ch < 40: return
        cvs.delete("all")
        vv = data.get("vv_heatmap", []);  n = data.get("vv_size", 0)
        if not vv or n < 4: return
        cx_bin = n / 2.0;  cy_bin = n / 2.0;  n_radial = n // 2
        radial = [0.0] * n_radial
        for row in range(n):
            for col in range(n):
                dx = col - cx_bin + 0.5;  dy = row - cy_bin + 0.5
                r = math.sqrt(dx*dx + dy*dy);  ri = int(r)
                if 0 <= ri < n_radial:
                    radial[ri] += vv[row * n + col]
        max_r = max(radial) if radial else 1.0
        if max_r <= 0: max_r = 1.0
        ml, mr, mt, mb = 45, 15, 20, 30
        pw = cw - ml - mr;  ph = ch - mt - mb
        for i in range(5):
            y = mt + int(ph * i / 4)
            cvs.create_line(ml, y, ml+pw, y, fill=GRID_LINE, dash=(2,4))
        for i in range(5):
            x = ml + int(pw * i / 4)
            cvs.create_line(x, mt, x, mt+ph, fill=GRID_LINE, dash=(2,4))
        cvs.create_line(ml, mt, ml, mt+ph, fill=DIM, width=1)
        cvs.create_line(ml, mt+ph, ml+pw, mt+ph, fill=DIM, width=1)
        points = []
        for i in range(n_radial):
            x = ml + (i / max(n_radial-1, 1)) * pw
            y = mt + ph - (radial[i] / max_r) * ph
            points.append((x, y))
        for i in range(len(points)-1):
            x0, y0 = points[i];  x1, y1 = points[i+1]
            t = i / max(len(points)-1, 1)
            r_c = int(187*(1-t) + 3*t);  g_c = int(134*(1-t) + 218*t);  b_c = int(252*(1-t) + 198*t)
            fill_c = f"#{r_c:02x}{g_c:02x}{b_c:02x}"
            cvs.create_polygon(x0, y0, x1, y1, x1, mt+ph, x0, mt+ph,
                               fill=fill_c, outline="", stipple="gray50")
        if len(points) >= 2:
            flat = []
            for x, y in points: flat.extend([x, y])
            cvs.create_line(*flat, fill=ACCENT, width=2, smooth=True)
        peak_bin = radial.index(max(radial))
        if peak_bin > 0:
            peak_x = ml + (peak_bin / max(n_radial-1, 1)) * pw
            cvs.create_line(peak_x, mt, peak_x, mt+ph, fill="#FFD700", width=1, dash=(4,2))
            cvs.create_text(peak_x, mt-2, text="v_mp", anchor="s", fill="#FFD700", font=FONT_TINY)
            rms_bin = min(int(peak_bin * 1.22), n_radial-1)
            rms_x = ml + (rms_bin / max(n_radial-1, 1)) * pw
            cvs.create_line(rms_x, mt, rms_x, mt+ph, fill=ACCENT3, width=1, dash=(4,2))
            cvs.create_text(rms_x, mt-2, text="v_rms", anchor="s", fill=ACCENT3, font=FONT_TINY)
        cvs.create_text(ml+pw//2, ch-4, text="speed ->", anchor="s", fill=DIM, font=FONT_TINY)
        cvs.create_text(8, mt+ph//2, text="f(v)", anchor="w", fill=DIM, font=FONT_TINY)
        cvs.create_text(ml+pw-4, mt+4, anchor="ne", text=f"#{self.frame_count}", fill=DIM, font=FONT_TINY)

    def _render_timeline(self):
        cvs = self.cvs_timeline
        cw = cvs.winfo_width();  ch = cvs.winfo_height()
        if cw < 40 or ch < 40: return
        cvs.delete("all")
        ml, mr, mt, mb = 50, 50, 20, 30
        pw = cw - ml - mr;  ph = ch - mt - mb
        if len(self._temp_history) < 2:
            cvs.create_text(cw//2, ch//2, text="Collecting data...", fill=DIM, font=FONT_SMALL)
            return
        for i in range(5):
            y = mt + int(ph * i / 4)
            cvs.create_line(ml, y, ml+pw, y, fill=GRID_LINE, dash=(2,4))
        cvs.create_line(ml, mt, ml, mt+ph, fill=ACCENT2, width=1)
        cvs.create_line(ml, mt+ph, ml+pw, mt+ph, fill=DIM, width=1)
        cvs.create_line(ml+pw, mt, ml+pw, mt+ph, fill=ACCENT3, width=1)

        temps = [t[1] for t in self._temp_history]
        t_min = min(temps) - 0.5;  t_max = max(temps) + 0.5
        if t_max <= t_min: t_max = t_min + 1.0
        n_pts = len(self._temp_history)
        temp_pts = []
        for i, (_, T) in enumerate(self._temp_history):
            x = ml + (i / max(n_pts-1, 1)) * pw
            y = mt + ph - ((T - t_min) / (t_max - t_min)) * ph
            temp_pts.extend([x, y])
        if len(temp_pts) >= 4:
            cvs.create_line(*temp_pts, fill=ACCENT2, width=2, smooth=True)

        vrms_vals = [v[1] for v in self._vrms_history]
        v_min = min(vrms_vals) * 0.95;  v_max = max(vrms_vals) * 1.05
        if v_max <= v_min: v_max = v_min + 1.0
        vrms_pts = []
        for i, (_, v) in enumerate(self._vrms_history):
            x = ml + (i / max(n_pts-1, 1)) * pw
            y = mt + ph - ((v - v_min) / (v_max - v_min)) * ph
            vrms_pts.extend([x, y])
        if len(vrms_pts) >= 4:
            cvs.create_line(*vrms_pts, fill=ACCENT3, width=2, smooth=True)

        if len(self._ke_history) >= 2:
            ke_vals = [k[1] for k in self._ke_history]
            ke_min_v = min(ke_vals);  ke_max_v = max(ke_vals)
            if ke_max_v <= ke_min_v: ke_max_v = ke_min_v + 1e-20
            ke_pts = []
            for i, (_, ke) in enumerate(self._ke_history):
                x = ml + (i / max(n_pts-1, 1)) * pw
                y = mt + ph - ((ke - ke_min_v) / (ke_max_v - ke_min_v)) * ph * 0.5
                ke_pts.extend([x, y])
            if len(ke_pts) >= 4:
                cvs.create_line(*ke_pts, fill="#FFD700", width=1, smooth=True, dash=(2,3))

        cvs.create_text(8, mt+ph//2, text="T (K)", anchor="w", fill=ACCENT2, font=FONT_TINY)
        cvs.create_text(cw-8, mt+ph//2, text="v_rms", anchor="e", fill=ACCENT3, font=FONT_TINY)
        cvs.create_text(ml+pw//2, ch-4, text="frame ->", anchor="s", fill=DIM, font=FONT_TINY)
        last_T = self._temp_history[-1][1];  last_v = self._vrms_history[-1][1]
        cvs.create_text(ml+4, mt+4, anchor="nw", text=f"T = {last_T:.2f} K", fill=ACCENT2, font=FONT_SMALL)
        cvs.create_text(ml+pw-4, mt+4, anchor="ne", text=f"v = {last_v:.1f} m/s", fill=ACCENT3, font=FONT_SMALL)
        for i in range(5):
            frac = i / 4.0
            val = t_min + frac * (t_max - t_min);  y = mt + ph - frac * ph
            cvs.create_text(ml-4, y, text=f"{val:.1f}", anchor="e", fill=ACCENT2, font=FONT_TINY)
        for i in range(5):
            frac = i / 4.0
            val = v_min + frac * (v_max - v_min);  y = mt + ph - frac * ph
            cvs.create_text(ml+pw+4, y, text=f"{val:.0f}", anchor="w", fill=ACCENT3, font=FONT_TINY)


def main():
    root = tk.Tk()
    root.option_add("*Font", "Consolas 9")
    style = ttk.Style()
    try: style.theme_use("clam")
    except Exception: pass
    style.configure(".", background=BG_COLOR, foreground=FG_COLOR)
    _app = HeatmapViewer(root)
    root.mainloop()

if __name__ == "__main__":
    main()
