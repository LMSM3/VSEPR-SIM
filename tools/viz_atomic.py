#!/usr/bin/env python3
"""
viz_atomic.py - Port 9999 Live Atomic Viewer
=============================================
3D atomistic scene: positions, bonds, lattice wireframe, live HUD.
Connects to `vsepr viz <FORMULA>` streaming on port 9999.

Usage:
    vsepr viz Ar -T 300 -N 64        (in one terminal)
    python tools/viz_atomic.py        (in another terminal)

Controls:
    Mouse drag    Rotate
    Scroll        Zoom
    R             Reset view
    B             Toggle bonds
    L             Toggle lattice
    C             Cycle colour mode
    Space         Pause/resume
    Q / Escape    Quit

Requires: Python 3.6+, tkinter (standard library only)
"""

import socket, json, threading, math, time, tkinter as tk
from tkinter import ttk

HOST = "127.0.0.1"
PORT = 9999

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

COLOUR_MODES = ["element", "energy", "velocity", "defect", "charge"]

PHASE_COLOURS = {
    "gas": "#64B5F6", "vapor": "#81C784", "liquid": "#FFB74D",
    "supercritical": "#CE93D8", "solid": "#FF8A65", "unknown": "#888"
}

RUN_MODE_ICONS = {"sample": "~", "relax": "v", "explore": "*", "crystal": "#"}

class Vec3:
    __slots__ = ("x","y","z")
    def __init__(self,x=0,y=0,z=0): self.x=x; self.y=y; self.z=z
    def __sub__(self,o): return Vec3(self.x-o.x,self.y-o.y,self.z-o.z)
    def __add__(self,o): return Vec3(self.x+o.x,self.y+o.y,self.z+o.z)
    def __mul__(self,s): return Vec3(self.x*s,self.y*s,self.z*s)
    def dot(self,o): return self.x*o.x+self.y*o.y+self.z*o.z
    def length(self): return math.sqrt(self.x**2+self.y**2+self.z**2)
    def norm(self):
        l = self.length(); return Vec3(self.x/l,self.y/l,self.z/l) if l>1e-12 else Vec3(0,0,1)

def rot_x(v,c,s): return Vec3(v.x, v.y*c-v.z*s, v.y*s+v.z*c)
def rot_y(v,c,s): return Vec3(v.x*c+v.z*s, v.y, -v.x*s+v.z*c)

def project(v, cx, cy, fov=600.0):
    z = v.z + fov
    if z < 1: z = 1
    sx = cx + v.x * fov / z
    sy = cy - v.y * fov / z
    return sx, sy, z

def hsv_to_hex(h,s,v):
    h=h%1.0; i=int(h*6); f=h*6-i; p=v*(1-s); q=v*(1-f*s); t=v*(1-(1-f)*s)
    sectors=[(v,t,p),(q,v,p),(p,v,t),(p,q,v),(t,p,v),(v,p,q)]
    r,g,b=sectors[i%6]; return f"#{int(r*255):02x}{int(g*255):02x}{int(b*255):02x}"

def interp_colour(t, c0, c1):
    def parse(c): return (int(c[1:3],16)/255, int(c[3:5],16)/255, int(c[5:7],16)/255)
    r0,g0,b0=parse(c0); r1,g1,b1=parse(c1)
    return f"#{int((r0+(r1-r0)*t)*255):02x}{int((g0+(g1-g0)*t)*255):02x}{int((b0+(b1-b0)*t)*255):02x}"

class AtomicViewer:
    def __init__(self, root):
        self.root = root
        self.root.title("VSEPR-SIM  |  Atomic View  |  Port 9999")
        self.root.configure(bg=BG)
        self.root.geometry("1400x860")
        self.root.minsize(900,600)

        self.frame_data  = None
        self.running     = True
        self.paused      = False
        self.frame_count = 0
        self.fps_last    = time.time()
        self.fps_val     = 0.0

        # View state
        self.rot_x   = 0.20
        self.rot_y   = 0.35
        self.zoom    = 1.0
        self.pan_x   = 0.0
        self.pan_y   = 0.0
        self._drag_start = None

        # Render toggles
        self.show_bonds   = True
        self.show_lattice = True
        self.show_hud     = True
        self.colour_mode  = 0
        self.atom_scale   = 1.0
        self.follow_focus = False

        self._build_ui()
        self._connect()
        self._poll()

    # ------------------------------------------------------------------ UI
    def _build_ui(self):
        top = tk.Frame(self.root, bg=HDR_BG, height=42)
        top.pack(fill=tk.X); top.pack_propagate(False)
        tk.Label(top, text="  VSEPR-SIM  Atomic View  :9999",
                 font=FONT_T, fg=ACCENT, bg=HDR_BG).pack(side=tk.LEFT, padx=8)
        self.lbl_conn = tk.Label(top, text="Connecting...", font=FONT_S, fg=ACCENT3, bg=HDR_BG)
        self.lbl_conn.pack(side=tk.RIGHT, padx=8)
        self.lbl_fps = tk.Label(top, text="", font=FONT_XS, fg=DIM, bg=HDR_BG)
        self.lbl_fps.pack(side=tk.RIGHT, padx=6)

        body = tk.Frame(self.root, bg=BG)
        body.pack(fill=tk.BOTH, expand=True)
        body.columnconfigure(0, weight=1)
        body.columnconfigure(1, weight=0)
        body.rowconfigure(0, weight=1)

        # 3D canvas
        self.cvs = tk.Canvas(body, bg=BG, highlightthickness=0, cursor="crosshair")
        self.cvs.grid(row=0, column=0, sticky="nsew")
        self.cvs.bind("<ButtonPress-1>",   self._on_press)
        self.cvs.bind("<B1-Motion>",       self._on_drag)
        self.cvs.bind("<MouseWheel>",      self._on_scroll)
        self.cvs.bind("<Button-4>",        lambda e: self._on_scroll_up())
        self.cvs.bind("<Button-5>",        lambda e: self._on_scroll_down())

        # Side panel
        side = tk.Frame(body, bg=PANEL_BG, width=230)
        side.grid(row=0, column=1, sticky="nsew")
        side.pack_propagate(False)
        self._build_side(side)

        # Key bindings
        self.root.bind("<KeyPress-r>",     lambda e: self._reset_view())
        self.root.bind("<KeyPress-R>",     lambda e: self._reset_view())
        self.root.bind("<KeyPress-b>",     lambda e: self._toggle("bonds"))
        self.root.bind("<KeyPress-B>",     lambda e: self._toggle("bonds"))
        self.root.bind("<KeyPress-l>",     lambda e: self._toggle("lattice"))
        self.root.bind("<KeyPress-L>",     lambda e: self._toggle("lattice"))
        self.root.bind("<KeyPress-c>",     lambda e: self._cycle_colour())
        self.root.bind("<KeyPress-C>",     lambda e: self._cycle_colour())
        self.root.bind("<space>",          lambda e: self._toggle("paused"))
        self.root.bind("<KeyPress-q>",     lambda e: self.root.destroy())
        self.root.bind("<Escape>",         lambda e: self.root.destroy())

        # Bottom bar
        bot = tk.Frame(self.root, bg=PANEL_BG, height=26)
        bot.pack(fill=tk.X); bot.pack_propagate(False)
        self.lbl_hint = tk.Label(bot,
            text="  Drag:rotate  Scroll:zoom  R:reset  B:bonds  L:lattice  C:colour  Space:pause  Q:quit",
            font=FONT_XS, fg=DIM, bg=PANEL_BG)
        self.lbl_hint.pack(side=tk.LEFT, padx=6)

    def _build_side(self, parent):
        tk.Label(parent, text=" Controls", font=FONT_S, fg=ACCENT, bg=PANEL_BG).pack(anchor="w", padx=8, pady=(8,2))
        sep = tk.Frame(parent, bg="#333344", height=1); sep.pack(fill=tk.X, padx=8)

        def btn(text, cmd):
            b = tk.Button(parent, text=text, command=cmd, font=FONT_XS,
                          bg="#1e1e2e", fg=FG, activebackground="#333",
                          relief=tk.FLAT, bd=0, padx=6, pady=3)
            b.pack(fill=tk.X, padx=8, pady=1)

        btn("[ B ] Toggle Bonds",       lambda: self._toggle("bonds"))
        btn("[ L ] Toggle Lattice",     lambda: self._toggle("lattice"))
        btn("[ C ] Cycle Colour Mode",  self._cycle_colour)
        btn("[ R ] Reset View",         self._reset_view)
        btn("[ Space ] Pause",          lambda: self._toggle("paused"))

        tk.Label(parent, text=" Colour Mode", font=FONT_XS, fg=DIM, bg=PANEL_BG).pack(anchor="w", padx=8, pady=(10,2))
        self.lbl_cmode = tk.Label(parent, text="element", font=FONT_S, fg=ACCENT2, bg=PANEL_BG)
        self.lbl_cmode.pack(anchor="w", padx=8)

        tk.Label(parent, text=" Atom Scale", font=FONT_XS, fg=DIM, bg=PANEL_BG).pack(anchor="w", padx=8, pady=(10,2))
        self.scale_var = tk.DoubleVar(value=1.0)
        tk.Scale(parent, variable=self.scale_var, from_=0.2, to=3.0, resolution=0.1,
                 orient=tk.HORIZONTAL, bg=PANEL_BG, fg=FG, troughcolor="#222",
                 highlightthickness=0, length=180, command=lambda v: setattr(self,"atom_scale",float(v))
                 ).pack(padx=8)

        sep2 = tk.Frame(parent, bg="#333344", height=1); sep2.pack(fill=tk.X, padx=8, pady=6)
        tk.Label(parent, text=" Live HUD", font=FONT_XS, fg=DIM, bg=PANEL_BG).pack(anchor="w", padx=8)

        self.hud_vars = {}
        for key in ["formula","T_K","energy_Eh","phase","n_atoms","n_defects","convergence","run_mode"]:
            row = tk.Frame(parent, bg=PANEL_BG)
            row.pack(fill=tk.X, padx=8, pady=1)
            tk.Label(row, text=f"{key}:", font=FONT_XS, fg=DIM, bg=PANEL_BG, width=12, anchor="w").pack(side=tk.LEFT)
            lbl = tk.Label(row, text="-", font=FONT_XS, fg=FG, bg=PANEL_BG, anchor="w")
            lbl.pack(side=tk.LEFT, fill=tk.X)
            self.hud_vars[key] = lbl

    # ------------------------------------------------------------------ Network
    def _connect(self):
        def reader():
            while self.running:
                try:
                    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                    s.settimeout(3.0)
                    s.connect((HOST, PORT))
                    self.lbl_conn.config(text="Connected :9999", fg=SUCCESS)
                    s.settimeout(5.0)
                    buf = ""
                    while self.running:
                        chunk = s.recv(65536).decode("utf-8", errors="replace")
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
                    time.sleep(1.5)
        threading.Thread(target=reader, daemon=True).start()

    # ------------------------------------------------------------------ Input
    def _on_press(self, e): self._drag_start = (e.x, e.y)
    def _on_drag(self, e):
        if self._drag_start:
            dx = e.x - self._drag_start[0]; dy = e.y - self._drag_start[1]
            self.rot_y += dx * 0.007; self.rot_x += dy * 0.007
            self._drag_start = (e.x, e.y)
    def _on_scroll(self, e):
        self.zoom *= (1.1 if e.delta > 0 else 0.9)
        self.zoom = max(0.1, min(10.0, self.zoom))
    def _on_scroll_up(self): self.zoom = min(10.0, self.zoom * 1.1)
    def _on_scroll_down(self): self.zoom = max(0.1, self.zoom * 0.9)
    def _reset_view(self): self.rot_x=0.2; self.rot_y=0.35; self.zoom=1.0
    def _toggle(self, attr):
        if attr == "bonds":   self.show_bonds   = not self.show_bonds
        elif attr == "lattice": self.show_lattice = not self.show_lattice
        elif attr == "paused":  self.paused       = not self.paused
    def _cycle_colour(self):
        self.colour_mode = (self.colour_mode + 1) % len(COLOUR_MODES)
        self.lbl_cmode.config(text=COLOUR_MODES[self.colour_mode])

    # ------------------------------------------------------------------ Poll
    def _poll(self):
        if not self.paused and self.frame_data is not None:
            data = self.frame_data; self.frame_data = None
            self._draw(data)
            self.frame_count += 1
            now = time.time()
            dt = now - self.fps_last
            if dt > 0.5:
                self.fps_val = self.frame_count / dt
                self.frame_count = 0; self.fps_last = now
                self.lbl_fps.config(text=f"{self.fps_val:.1f} fps")
        if self.running:
            self.root.after(33, self._poll)  # ~30 fps target

    # ------------------------------------------------------------------ 3D Render
    def _atom_colour(self, atom, mode):
        if mode == 0:  # element
            r,g,b = atom["cr"], atom["cg"], atom["cb"]
            return f"#{int(r*255):02x}{int(g*255):02x}{int(b*255):02x}"
        elif mode == 1:  # energy (KE)
            t = min(1.0, atom["ke"] / (1e-3 + 1e-30))
            return interp_colour(t, "#2244aa", "#ff3300")
        elif mode == 2:  # velocity magnitude
            v = math.sqrt(atom["vx"]**2 + atom["vy"]**2 + atom["vz"]**2)
            t = min(1.0, v / 1000.0)
            return hsv_to_hex(0.65 - t*0.65, 0.8, 0.9)
        elif mode == 3:  # defect
            return ACCENT3 if atom["st"] == 2 else (ACCENT if atom["st"] == 1 else "#666688")
        else:  # chi / confidence
            t = max(0.0, min(1.0, atom.get("chi", 0.5)))
            return interp_colour(t, "#224488", "#ffaa00")

    def _draw(self, data):
        cvs = self.cvs
        cw = cvs.winfo_width(); ch = cvs.winfo_height()
        if cw < 50 or ch < 50: return
        cvs.delete("all")
        cx = cw // 2; cy = ch // 2

        # Rotation matrices
        rx_c = math.cos(self.rot_x); rx_s = math.sin(self.rot_x)
        ry_c = math.cos(self.rot_y); ry_s = math.sin(self.rot_y)
        scale = self.zoom * min(cw, ch) * 0.008

        def transform(x, y, z):
            v = Vec3(x * scale, y * scale, z * scale)
            v = rot_x(v, rx_c, rx_s)
            v = rot_y(v, ry_c, ry_s)
            return project(v, cx + self.pan_x, cy + self.pan_y)

        atoms = data.get("atoms", [])
        bonds = data.get("bonds", [])
        lat   = data.get("lattice", {})
        focus = data.get("focus_atom", -1)
        mode  = self.colour_mode

        # ---- Lattice wireframe ----
        if self.show_lattice and lat.get("active"):
            corners_3d = []
            a = (lat["ax"],lat["ay"],lat["az"])
            b = (lat["bx"],lat["by"],lat["bz"])
            c = (lat["cx"],lat["cy"],lat["cz"])
            o = (0,0,0)
            verts = [o,
                     a, b, c,
                     (a[0]+b[0],a[1]+b[1],a[2]+b[2]),
                     (a[0]+c[0],a[1]+c[1],a[2]+c[2]),
                     (b[0]+c[0],b[1]+c[1],b[2]+c[2]),
                     (a[0]+b[0]+c[0],a[1]+b[1]+c[1],a[2]+b[2]+c[2])]
            edges = [(0,1),(0,2),(0,3),(1,4),(1,5),(2,4),(2,6),(3,5),(3,6),(4,7),(5,7),(6,7)]
            pts = [transform(*v) for v in verts]
            for i,j in edges:
                x0,y0,_ = pts[i]; x1,y1,_ = pts[j]
                cvs.create_line(x0,y0,x1,y1, fill="#446688", width=1, dash=(4,3))

        # ---- Bonds (drawn before atoms so atoms are on top) ----
        if self.show_bonds and atoms:
            for bond in bonds:
                i,j = bond["i"], bond["j"]
                if i >= len(atoms) or j >= len(atoms): continue
                ai, aj = atoms[i], atoms[j]
                x0,y0,z0 = transform(ai["x"],ai["y"],ai["z"])
                x1,y1,z1 = transform(aj["x"],aj["y"],aj["z"])
                w = max(0.5, bond["w"] * 2.0)
                btype = bond.get("t", 1)
                bcol = {"0":"#88aacc","1":DIM,"2":"#aa66cc","3":"#cc8844"}.get(str(btype), DIM)
                cvs.create_line(x0,y0,x1,y1, fill=bcol, width=w)

        # ---- Atoms (depth sorted) ----
        if atoms:
            projected = []
            for a in atoms:
                sx,sy,z = transform(a["x"],a["y"],a["z"])
                projected.append((z, sx, sy, a))
            projected.sort(key=lambda p: p[0])  # back to front

            ke_max = max((a["ke"] for a in atoms), default=1e-30)

            for z, sx, sy, atom in projected:
                col  = self._atom_colour(atom, mode)
                r_px = max(3, int(8 * self.atom_scale * 600 / max(z, 1)))
                r_px = min(r_px, 30)

                is_focus = (atom["id"] == focus)
                # Glow ring for focus atom
                if is_focus:
                    cvs.create_oval(sx-r_px-4, sy-r_px-4, sx+r_px+4, sy+r_px+4,
                                    outline=ACCENT2, width=2)

                # Main sphere (filled oval with highlight)
                cvs.create_oval(sx-r_px, sy-r_px, sx+r_px, sy+r_px,
                                fill=col, outline="")
                # Specular highlight dot
                hr = max(1, r_px//3)
                cvs.create_oval(sx-r_px+hr//2, sy-r_px+hr//2,
                                sx-r_px+hr//2+hr, sy-r_px+hr//2+hr,
                                fill="#ffffff", outline="", stipple="gray50")

                # Label focus atom
                if is_focus and r_px > 6:
                    cvs.create_text(sx, sy+r_px+10, text=atom["sym"],
                                    fill=ACCENT2, font=FONT_XS)

        # ---- Onscreen HUD ----
        if self.show_hud:
            hud = [
                f"job: {data.get('formula','?')}",
                f"mode: {RUN_MODE_ICONS.get(data.get('run_mode','?'),'?')} {data.get('run_mode','?')}",
                f"frame: {data.get('cycle',0)}",
                f"T: {data.get('T_K',0):.2f} K",
                f"E: {data.get('energy_Eh',0):.4e} Eh",
                f"phase: {data.get('phase','?')}",
                f"N: {data.get('n_atoms',0)}  def: {data.get('n_defects',0)}",
                f"conv: {data.get('convergence',0):.4f}",
            ]
            ph = data.get("phase","?")
            ph_col = PHASE_COLOURS.get(ph, "#888")
            for i, line in enumerate(hud):
                cvs.create_text(12, 16 + i*16, text=line, anchor="w",
                                fill=(ACCENT2 if i==0 else (ph_col if i==5 else FG)),
                                font=FONT_XS)

            # Colour mode badge
            cvs.create_text(cw-8, 16, anchor="ne",
                            text=f"colour: {COLOUR_MODES[mode]}", fill=DIM, font=FONT_XS)
            # Pause indicator
            if self.paused:
                cvs.create_text(cw//2, ch//2, text="PAUSED",
                                fill=ACCENT3, font=FONT_T)

        # ---- Update side-panel HUD ----
        vals = {
            "formula":   data.get("formula","?"),
            "T_K":       f"{data.get('T_K',0):.2f} K",
            "energy_Eh": f"{data.get('energy_Eh',0):.4e} Eh",
            "phase":     data.get("phase","?"),
            "n_atoms":   str(data.get("n_atoms",0)),
            "n_defects": str(data.get("n_defects",0)),
            "convergence": f"{data.get('convergence',0):.5f}",
            "run_mode":  data.get("run_mode","?"),
        }
        for k, v in vals.items():
            if k in self.hud_vars: self.hud_vars[k].config(text=v)

def main():
    root = tk.Tk()
    style = ttk.Style()
    try: style.theme_use("clam")
    except Exception: pass
    style.configure(".", background=BG, foreground=FG)
    app = AtomicViewer(root)
    root.protocol("WM_DELETE_WINDOW", lambda: (setattr(app,"running",False), root.destroy()))
    root.mainloop()

if __name__ == "__main__":
    main()
