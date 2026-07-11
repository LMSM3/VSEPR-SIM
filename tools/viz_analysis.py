#!/usr/bin/env python3
"""
viz_analysis.py - Port 10001 Detailed Analysis View
=====================================================
Historical graphs, property panels, candidate comparison, 3D crystal scene.
Connects to `vsepr viz <FORMULA>` streaming on port 10001.

Usage:
    vsepr viz Ar -T 300 -N 64        (in one terminal)
    python tools/viz_analysis.py      (in another terminal)

Panels:
    Row 1:  E(t) / T(t) timeline  |  Speed distribution f(v)  |  3D crystal view
    Row 2:  Property panel table  |  Candidate comparison bars |  KE distribution + coord

Requires: Python 3.6+, tkinter (standard library only)
"""

import socket, json, threading, math, time, tkinter as tk
from tkinter import ttk

HOST = "127.0.0.1"
PORT = 10001

BG       = "#080810"
PANEL_BG = "#0e0e18"
HDR_BG   = "#12121c"
FG       = "#e0e0e0"
ACCENT   = "#bb86fc"
ACCENT2  = "#03dac6"
ACCENT3  = "#cf6679"
GOLD     = "#FFD700"
DIM      = "#444455"
SUCCESS  = "#4CAF50"
GRID     = "#1a1a2a"

FONT_T  = ("Consolas", 12, "bold")
FONT_P  = ("Consolas", 10, "bold")
FONT_S  = ("Consolas", 9)
FONT_XS = ("Consolas", 7)

INFERNO = [(0,0,4),(20,4,54),(63,6,97),(110,15,96),(155,43,68),
           (194,78,38),(224,121,17),(244,174,8),(252,228,41),(252,255,252)]

def inferno_hex(t):
    t = max(0.0, min(1.0, t)); n = len(INFERNO)-1; idx = t*n
    lo=int(idx); hi=min(lo+1,n); f=idx-lo
    r0,g0,b0=INFERNO[lo]; r1,g1,b1=INFERNO[hi]
    return f"#{int(r0+f*(r1-r0)):02x}{int(g0+f*(g1-g0)):02x}{int(b0+f*(b1-b0)):02x}"

INFERNO_LUT = [inferno_hex(i/255) for i in range(256)]

class AnalysisViewer:
    def __init__(self, root):
        self.root = root
        self.root.title("VSEPR-SIM  |  Analysis View  |  Port 10001")
        self.root.configure(bg=BG)
        self.root.geometry("1600x940")
        self.root.minsize(1100, 700)

        self.frame_data  = None
        self.running     = True
        self.frame_count = 0
        self.crystal_rot = 0.0

        self._build_ui()
        self._connect()
        self._poll()

    # ------------------------------------------------------------------ UI
    def _panel(self, parent, title):
        return tk.LabelFrame(parent, text=title, font=FONT_P, fg=ACCENT,
                             bg=PANEL_BG, labelanchor="n", bd=1, relief=tk.GROOVE)

    def _build_ui(self):
        top = tk.Frame(self.root, bg=HDR_BG, height=42)
        top.pack(fill=tk.X); top.pack_propagate(False)
        tk.Label(top, text="  VSEPR-SIM  Analysis View  :10001",
                 font=FONT_T, fg=ACCENT, bg=HDR_BG).pack(side=tk.LEFT, padx=8)
        self.lbl_conn = tk.Label(top, text="Connecting...", font=FONT_S, fg=ACCENT3, bg=HDR_BG)
        self.lbl_conn.pack(side=tk.RIGHT, padx=8)
        self.lbl_info = tk.Label(top, text="", font=FONT_XS, fg=DIM, bg=HDR_BG)
        self.lbl_info.pack(side=tk.RIGHT, padx=8)

        body = tk.Frame(self.root, bg=BG)
        body.pack(fill=tk.BOTH, expand=True, padx=4, pady=4)
        for c in range(3): body.columnconfigure(c, weight=1)
        for r in range(2): body.rowconfigure(r, weight=1)

        # Row 0 Col 0: E/T timeline
        f_et = self._panel(body, " E(t) / T(t) / rho(t) Timeline ")
        f_et.grid(row=0, column=0, sticky="nsew", padx=3, pady=3)
        self.cvs_et = tk.Canvas(f_et, bg="#06060e", highlightthickness=0)
        self.cvs_et.pack(fill=tk.BOTH, expand=True, padx=2, pady=2)

        # Row 0 Col 1: Speed distribution
        f_sd = self._panel(body, " Speed Distribution  f(v)  MB-Theory ")
        f_sd.grid(row=0, column=1, sticky="nsew", padx=3, pady=3)
        self.cvs_sd = tk.Canvas(f_sd, bg="#06060e", highlightthickness=0)
        self.cvs_sd.pack(fill=tk.BOTH, expand=True, padx=2, pady=2)

        # Row 0 Col 2: 3D crystal / cluster view
        f_3d = self._panel(body, " 3D Crystal / Cluster Scene ")
        f_3d.grid(row=0, column=2, sticky="nsew", padx=3, pady=3)
        self.cvs_3d = tk.Canvas(f_3d, bg="#04040c", highlightthickness=0)
        self.cvs_3d.pack(fill=tk.BOTH, expand=True, padx=2, pady=2)
        self.cvs_3d.bind("<ButtonPress-1>",  self._3d_press)
        self.cvs_3d.bind("<B1-Motion>",      self._3d_drag)
        self._3d_drag_start = None; self._3d_rot_x = 0.3; self._3d_rot_y = 0.5

        # Row 1 Col 0: Property panel
        f_prop = self._panel(body, " Macro-Property Panel  P(t) ")
        f_prop.grid(row=1, column=0, sticky="nsew", padx=3, pady=3)
        self.cvs_prop = tk.Canvas(f_prop, bg="#06060e", highlightthickness=0)
        self.cvs_prop.pack(fill=tk.BOTH, expand=True, padx=2, pady=2)

        # Row 1 Col 1: Candidate comparison
        f_cand = self._panel(body, " Candidate Comparison  C = {C1, C2, C3} ")
        f_cand.grid(row=1, column=1, sticky="nsew", padx=3, pady=3)
        self.cvs_cand = tk.Canvas(f_cand, bg="#06060e", highlightthickness=0)
        self.cvs_cand.pack(fill=tk.BOTH, expand=True, padx=2, pady=2)

        # Row 1 Col 2: KE distribution + crystal metrics
        f_ke = self._panel(body, " KE Distribution + Crystal Metrics ")
        f_ke.grid(row=1, column=2, sticky="nsew", padx=3, pady=3)
        self.cvs_ke = tk.Canvas(f_ke, bg="#06060e", highlightthickness=0)
        self.cvs_ke.pack(fill=tk.BOTH, expand=True, padx=2, pady=2)

        bot = tk.Frame(self.root, bg=PANEL_BG, height=24)
        bot.pack(fill=tk.X); bot.pack_propagate(False)
        self.lbl_bot = tk.Label(bot, text="", font=FONT_XS, fg=DIM, bg=PANEL_BG)
        self.lbl_bot.pack(side=tk.LEFT, padx=8)
        self.lbl_fun = tk.Label(bot, text="From electron to dust — all properties are a must",
                                font=FONT_XS, fg=ACCENT, bg=PANEL_BG)
        self.lbl_fun.pack(side=tk.RIGHT, padx=12)

    # ------------------------------------------------------------------ Network
    def _connect(self):
        def reader():
            while self.running:
                try:
                    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                    s.settimeout(3.0)
                    s.connect((HOST, PORT))
                    self.lbl_conn.config(text="Connected :10001", fg=SUCCESS)
                    s.settimeout(10.0)
                    buf = ""
                    while self.running:
                        chunk = s.recv(131072).decode("utf-8", errors="replace")
                        if not chunk: break
                        buf += chunk
                        while "\n" in buf:
                            line, buf = buf.split("\n", 1)
                            line = line.strip()
                            if not line: continue
                            try: self.frame_data = json.loads(line)
                            except: pass
                    s.close()
                except Exception:
                    self.lbl_conn.config(text="Reconnecting...", fg=ACCENT3)
                    time.sleep(2.0)
        threading.Thread(target=reader, daemon=True).start()

    def _3d_press(self, e): self._3d_drag_start = (e.x, e.y)
    def _3d_drag(self, e):
        if self._3d_drag_start:
            dx = e.x-self._3d_drag_start[0]; dy = e.y-self._3d_drag_start[1]
            self._3d_rot_y += dx*0.01; self._3d_rot_x += dy*0.01
            self._3d_drag_start = (e.x, e.y)

    # ------------------------------------------------------------------ Poll
    def _poll(self):
        self.crystal_rot += 0.012
        if self.frame_data is not None:
            data = self.frame_data; self.frame_data = None
            self.frame_count += 1
            self._render(data)
        if self.running:
            self.root.after(200, self._poll)  # 5 fps for analysis view

    # ------------------------------------------------------------------ Graph helpers
    def _draw_graph(self, cvs, series_list, labels, colours, title="",
                    ml=52, mr=16, mt=24, mb=30):
        cw = cvs.winfo_width(); ch = cvs.winfo_height()
        if cw < 40 or ch < 40: return
        cvs.delete("all")
        pw = cw-ml-mr; ph = ch-mt-mb

        # Grid
        for i in range(5):
            y = mt+int(ph*i/4)
            cvs.create_line(ml, y, ml+pw, y, fill=GRID, dash=(2,4))
        for i in range(5):
            x = ml+int(pw*i/4)
            cvs.create_line(x, mt, x, mt+ph, fill=GRID, dash=(2,4))
        cvs.create_line(ml, mt, ml, mt+ph, fill=DIM)
        cvs.create_line(ml, mt+ph, ml+pw, mt+ph, fill=DIM)

        if title:
            cvs.create_text(ml+pw//2, 6, text=title, fill=DIM, font=FONT_XS, anchor="n")

        for series, lbl, col in zip(series_list, labels, colours):
            if len(series) < 2: continue
            vmin = min(series); vmax = max(series)
            if vmax <= vmin: vmax = vmin+1e-30
            n = len(series)
            pts = []
            for i, v in enumerate(series):
                x = ml + (i/(n-1))*pw
                y = mt + ph - ((v-vmin)/(vmax-vmin))*ph
                pts.extend([x, y])
            cvs.create_line(*pts, fill=col, width=2, smooth=True)
            # Latest value badge
            lv = series[-1]
            cvs.create_text(ml+pw-2, mt+4+labels.index(lbl)*14, anchor="ne",
                            text=f"{lbl}={lv:.3g}", fill=col, font=FONT_XS)
            # Y-axis ticks
            for i in range(5):
                frac = i/4.0
                val = vmin+frac*(vmax-vmin)
                y = mt+ph-frac*ph
                cvs.create_text(ml-3, y, text=f"{val:.2g}", anchor="e", fill=col, font=FONT_XS)

    # ------------------------------------------------------------------ Render
    def _render(self, data):
        formula = data.get("formula","?"); T_K = data.get("T_K",0.0)
        self.lbl_info.config(text=f"{formula} @ {T_K:.1f} K  |  frame {data.get('cycle',0)}")
        self._render_timeline(data)
        self._render_speed(data)
        self._render_3d(data)
        self._render_properties(data)
        self._render_candidates(data)
        self._render_ke_metrics(data)
        tags = [
            "From electron to dust — all properties are a must",
            "Compute scaled up, deterministic by design",
            "E(t), T(t), rho(t), phi(t) — all tracked, all visible",
            "Every candidate evaluated, every metric exposed",
        ]
        self.lbl_fun.config(text=tags[self.frame_count % len(tags)])
        self.lbl_bot.config(text=(
            f"lat: a={data.get('lattice_a',0):.3f}  b={data.get('lattice_b',0):.3f}  "
            f"c={data.get('lattice_c',0):.3f} A  |  "
            f"coord={data.get('coord_avg',0):.1f}  |  sg={data.get('space_group','?')}  |  "
            f"density_fit={data.get('density_fit',0):.3f}"))

    def _render_timeline(self, data):
        E = data.get("hist_E", [])
        T = data.get("hist_T", [])
        r = data.get("hist_rho", [])
        self._draw_graph(self.cvs_et,
                         [E, T, r],
                         ["E(Eh)", "T(K)", "rho(g/cc)"],
                         [ACCENT, ACCENT2, ACCENT3])

    def _render_speed(self, data):
        cvs = self.cvs_sd
        cw = cvs.winfo_width(); ch = cvs.winfo_height()
        if cw < 40 or ch < 40: return
        cvs.delete("all")
        sd = data.get("speed_dist", [])
        if not sd: return
        ml,mr,mt,mb = 40,16,20,30
        pw=cw-ml-mr; ph=ch-mt-mb
        for i in range(5):
            cvs.create_line(ml, mt+int(ph*i/4), ml+pw, mt+int(ph*i/4), fill=GRID, dash=(2,4))
        cvs.create_line(ml, mt, ml, mt+ph, fill=DIM)
        cvs.create_line(ml, mt+ph, ml+pw, mt+ph, fill=DIM)
        n = len(sd); vmax_val = max(sd) if sd else 1
        if vmax_val <= 0: vmax_val = 1
        # Gradient filled curve
        for i in range(n-1):
            x0 = ml+(i/(n-1))*pw; x1 = ml+((i+1)/(n-1))*pw
            y0 = mt+ph-(sd[i]/vmax_val)*ph; y1 = mt+ph-(sd[i+1]/vmax_val)*ph
            t = i/(n-1)
            rc=int(187*(1-t)+3*t); gc=int(134*(1-t)+218*t); bc=int(252*(1-t)+198*t)
            cvs.create_polygon(x0,y0,x1,y1,x1,mt+ph,x0,mt+ph,
                               fill=f"#{rc:02x}{gc:02x}{bc:02x}", outline="", stipple="gray50")
        pts = []
        for i,v in enumerate(sd):
            pts.extend([ml+(i/(n-1))*pw, mt+ph-(v/vmax_val)*ph])
        if len(pts) >= 4:
            cvs.create_line(*pts, fill=ACCENT, width=2, smooth=True)
        # Markers
        v_rms=data.get("v_rms",0); v_mp=data.get("v_mp",0); v_mean=data.get("v_mean",0)
        vmax_speed = v_rms * 2.5 if v_rms > 0 else 1
        for vv, col, lbl in [(v_mp,GOLD,"v_mp"),(v_mean,ACCENT2,"v_mean"),(v_rms,ACCENT3,"v_rms")]:
            if vv > 0 and vmax_speed > 0:
                xv = ml + (vv/vmax_speed)*pw
                if ml <= xv <= ml+pw:
                    cvs.create_line(xv, mt, xv, mt+ph, fill=col, width=1, dash=(4,2))
                    cvs.create_text(xv, mt-2, text=lbl, anchor="s", fill=col, font=FONT_XS)
        cvs.create_text(ml+pw//2, ch-4, text="speed (m/s) ->", anchor="s", fill=DIM, font=FONT_XS)
        cvs.create_text(8, mt+ph//2, text="f(v)", anchor="w", fill=DIM, font=FONT_XS)

    def _render_3d(self, data):
        cvs = self.cvs_3d
        cw = cvs.winfo_width(); ch = cvs.winfo_height()
        if cw < 40 or ch < 40: return
        cvs.delete("all")
        a_len = data.get("lattice_a", 4.0)
        b_len = data.get("lattice_b", 4.0)
        c_len = data.get("lattice_c", 4.0)
        rot_y_auto = self.crystal_rot + self._3d_rot_y
        rx_c = math.cos(self._3d_rot_x); rx_s = math.sin(self._3d_rot_x)
        ry_c = math.cos(rot_y_auto); ry_s = math.sin(rot_y_auto)
        cx = cw//2; cy = ch//2; fov = 400.0; scale = min(cw,ch)*0.04

        def tr(x,y,z):
            x*=scale; y*=scale; z*=scale
            ny = y*rx_c - z*rx_s; nz = y*rx_s + z*rx_c; y=ny; z=nz
            nx = x*ry_c + z*ry_s; nz2 = -x*ry_s + z*ry_c; x=nx; z=nz2
            zz = z + fov; zz = max(zz, 1)
            return cx + x*fov/zz, cy - y*fov/zz, zz

        # Draw FCC-like unit cell with ghost atoms
        verts_raw = [(0,0,0),(1,0,0),(0,1,0),(0,0,1),(1,1,0),(1,0,1),(0,1,1),(1,1,1),
                     (0.5,0.5,0),(0.5,0,0.5),(0,0.5,0.5),
                     (1,0.5,0.5),(0.5,1,0.5),(0.5,0.5,1)]
        edges = [(0,1),(0,2),(0,3),(1,4),(1,5),(2,4),(2,6),(3,5),(3,6),(4,7),(5,7),(6,7)]
        # Scale verts
        verts = [(a_len*x*0.3-a_len*0.15, b_len*y*0.3-b_len*0.15, c_len*z*0.3-c_len*0.15)
                 for x,y,z in verts_raw]
        projected = [tr(x,y,z) for x,y,z in verts]
        # Cell edges
        for i,j in edges:
            x0,y0,_ = projected[i]; x1,y1,_ = projected[j]
            cvs.create_line(x0,y0,x1,y1, fill="#335577", width=1, dash=(3,2))
        # Atoms
        atom_info = [(0,"corner"),(1,"corner"),(2,"corner"),(3,"corner"),
                     (4,"corner"),(5,"corner"),(6,"corner"),(7,"corner"),
                     (8,"face"),(9,"face"),(10,"face"),(11,"face"),(12,"face"),(13,"face")]
        coords = list(zip([p[2] for p in projected], range(len(projected))))
        coords.sort()
        for z_val, idx in coords:
            sx,sy,_ = projected[idx]
            atype = atom_info[idx][1] if idx < len(atom_info) else "corner"
            col = ACCENT2 if atype == "face" else ACCENT
            r = max(3, int(7*fov/max(z_val,1)))
            r = min(r, 14)
            cvs.create_oval(sx-r,sy-r,sx+r,sy+r, fill=col, outline="")
            # Highlight
            hr = max(1,r//3)
            cvs.create_oval(sx-r+1,sy-r+1,sx-r+hr+1,sy-r+hr+1,
                            fill="#ffffff", outline="", stipple="gray50")
        # Labels
        sg = data.get("space_group","?")
        cvs.create_text(8, 10, anchor="nw", text=f"sg: {sg}", fill=DIM, font=FONT_XS)
        cvs.create_text(8, 22, anchor="nw",
            text=f"a={a_len:.2f} b={b_len:.2f} c={c_len:.2f} A",
            fill=DIM, font=FONT_XS)
        # Axis arrows
        axis_len = 30
        origin = tr(0,0,0)
        for (dx,dy,dz), col, lbl in [((1,0,0),ACCENT3,"a"),((0,1,0),SUCCESS,"b"),((0,0,1),ACCENT2,"c")]:
            tip = tr(dx*a_len*0.12, dy*b_len*0.12, dz*c_len*0.12)
            cvs.create_line(origin[0],origin[1],tip[0],tip[1], fill=col, width=2, arrow=tk.LAST)
            cvs.create_text(tip[0]+4, tip[1]-4, text=lbl, fill=col, font=FONT_XS)

    def _render_properties(self, data):
        cvs = self.cvs_prop
        cw = cvs.winfo_width(); ch = cvs.winfo_height()
        if cw < 40 or ch < 40: return
        cvs.delete("all")
        props = data.get("props", {})
        rows = [
            ("density",      props.get("density",0),      "g/cm^3", 1),
            ("bulk mod",     props.get("bulk_mod",0),      "GPa",    1),
            ("shear mod",    props.get("shear_mod",0),     "GPa",    0),
            ("Youngs mod",   props.get("youngs_mod",0),    "GPa",    0),
            ("Poisson",      props.get("poisson",0),       "",       0),
            ("therm exp",    props.get("therm_exp",0),     "1e-6/K", 1),
            ("Debye T",      props.get("debye_T",0),       "K",      0),
            ("sound speed",  props.get("sound_speed",0),   "m/s",    1),
            ("melt T",       props.get("melt_T",0),        "K",      1),
        ]
        row_h = max(18, min(28, ch // max(len(rows)+2, 1)))
        col_w = cw // 3
        for i, (name, val, unit, active) in enumerate(rows):
            y = 14 + i*row_h
            col = ACCENT2 if active else FG
            # Bar visual
            max_val_map = {"density":20,"bulk mod":1000,"shear mod":500,"Youngs mod":1000,
                           "Poisson":1,"therm exp":100,"Debye T":2000,"sound speed":10000,"melt T":5000}
            mv = max_val_map.get(name, 1); frac = min(1.0, abs(val)/max(mv,1e-30))
            bar_w = int((cw - 16) * frac * 0.45)
            bar_col = inferno_hex(frac)
            cvs.create_rectangle(col_w+4, y+1, col_w+4+bar_w, y+row_h-3,
                                  fill=bar_col, outline="")
            cvs.create_text(8, y+row_h//2, anchor="w", text=name, fill=DIM, font=FONT_XS)
            cvs.create_text(col_w-4, y+row_h//2, anchor="e",
                            text=f"{val:.4g}", fill=col, font=FONT_XS)
            cvs.create_text(col_w*2+4, y+row_h//2, anchor="w",
                            text=unit, fill=DIM, font=FONT_XS)
        cvs.create_text(cw//2, 4, text="Macro Properties P(t)", fill=DIM, font=FONT_XS, anchor="n")

    def _render_candidates(self, data):
        cvs = self.cvs_cand
        cw = cvs.winfo_width(); ch = cvs.winfo_height()
        if cw < 40 or ch < 40: return
        cvs.delete("all")
        cands = data.get("candidates", [])
        if not cands: return
        n = len(cands)
        row_h = max(30, ch // (n + 2))
        e_vals = [c.get("E",0) for c in cands]
        r_vals = [c.get("rho",0) for c in cands]
        e_min = min(e_vals); e_max = max(e_vals)+1e-30
        r_min = min(r_vals); r_max = max(r_vals)+1e-30
        cols = [ACCENT, ACCENT2, ACCENT3, GOLD, SUCCESS]
        ml = 80; bar_max = cw - ml - 40
        cvs.create_text(cw//2, 6, text="Candidate Comparison  C={C1,C2,...}", fill=DIM, font=FONT_XS, anchor="n")
        for i, c in enumerate(cands):
            y = 22 + i * row_h
            col = cols[i % len(cols)]
            lbl = c.get("label","?")
            E   = c.get("E",0)
            rho = c.get("rho",0)
            sym = c.get("sym",0)
            ph  = c.get("phase","?")
            # Energy bar
            frac_e = (E-e_min)/(e_max-e_min+1e-30)
            bw_e = int(bar_max * 0.5 * frac_e)
            cvs.create_rectangle(ml, y+4, ml+bw_e, y+14, fill=col, outline="")
            cvs.create_text(8, y+9, anchor="w", text=lbl, fill=col, font=FONT_S)
            cvs.create_text(ml+bw_e+4, y+9, anchor="w",
                            text=f"E={E:.4g} Eh  rho={rho:.3g}  Z={sym:.4f}  {ph}",
                            fill=FG, font=FONT_XS)
            # Density bar (secondary, dimmer)
            frac_r = (rho-r_min)/(r_max-r_min+1e-30)
            bw_r = int(bar_max * 0.3 * frac_r)
            cvs.create_rectangle(ml, y+16, ml+bw_r, y+24,
                                  fill=inferno_hex(frac_r), outline="")

    def _render_ke_metrics(self, data):
        cvs = self.cvs_ke
        cw = cvs.winfo_width(); ch = cvs.winfo_height()
        if cw < 40 or ch < 40: return
        cvs.delete("all")
        ke = data.get("ke_dist", [])
        ml,mr,mt,mb = 40,16,20,ch//2+4
        pw=cw-ml-mr; ph=(ch//2)-mt-mb+ch//2-20
        if ph < 10: ph = 50
        # KE histogram bars
        if ke:
            vmax = max(ke) if ke else 1; n = len(ke)
            bar_w = max(1, pw//n)
            for i,v in enumerate(ke):
                x = ml + i*(pw/n)
                h = int((v/vmax)*ph) if vmax>0 else 0
                col = INFERNO_LUT[min(255,int(i/n*255))]
                cvs.create_rectangle(x, mt+ph-h, x+bar_w-1, mt+ph, fill=col, outline="")
            cvs.create_line(ml, mt, ml, mt+ph, fill=DIM)
            cvs.create_line(ml, mt+ph, ml+pw, mt+ph, fill=DIM)
            cvs.create_text(ml+pw//2, mt-4, text="KE Distribution", fill=DIM, font=FONT_XS, anchor="s")
            ke_avg = data.get("ke_avg_Eh", 0)
            cvs.create_text(ml+pw-4, mt+4, anchor="ne",
                            text=f"<KE>={ke_avg:.4e} Eh", fill=GOLD, font=FONT_XS)
        # Crystal metrics table (lower half)
        y0 = ch//2 + 4
        metrics = [
            ("a", f"{data.get('lattice_a',0):.4f} A"),
            ("b", f"{data.get('lattice_b',0):.4f} A"),
            ("c", f"{data.get('lattice_c',0):.4f} A"),
            ("coord", f"{data.get('coord_avg',0):.2f}"),
            ("space gp", data.get("space_group","?")),
            ("rho fit", f"{data.get('density_fit',0):.4f}"),
        ]
        for i,(k,v) in enumerate(metrics):
            y = y0 + i*16
            cvs.create_text(8, y+8, anchor="w", text=k, fill=DIM, font=FONT_XS)
            cvs.create_text(cw-8, y+8, anchor="e", text=v, fill=ACCENT2, font=FONT_XS)
        cvs.create_line(0, y0, cw, y0, fill="#222233")
        cvs.create_text(cw//2, y0-4, text="Crystal Metrics", fill=DIM, font=FONT_XS, anchor="s")


def main():
    root = tk.Tk()
    style = ttk.Style()
    try: style.theme_use("clam")
    except Exception: pass
    style.configure(".", background=BG, foreground=FG)
    app = AnalysisViewer(root)
    root.protocol("WM_DELETE_WINDOW", lambda: (setattr(app,"running",False), root.destroy()))
    root.mainloop()

if __name__ == "__main__":
    main()
