#!/usr/bin/env python3
"""
viz_peptide_3d.py — 3D Peptide Formation Viewer (Port 9999 UDP)
================================================================
Receives stochastic peptide formation JSON frames and renders
3D atomic views with backbone tracing, sidechain coloring, and
live energy HUD.

Usage:
    python tools/viz_peptide_3d.py           (listens on UDP 9999)

Then run:
    peptide-stochastic-viz                   (in another terminal)

Or use the unified host instead:
    python tools/viz_host.py                 (listens on ALL 999X + 8899)

Controls:
    Mouse drag    Rotate
    Scroll        Zoom
    R             Reset view
    B             Toggle bonds
    N             Next run
    Space         Pause/resume
    Q / Escape    Quit

Requires: Python 3.6+, tkinter (standard library only)
"""

import socket, json, threading, math, time, tkinter as tk
from collections import deque

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

ELEMENT_COLORS = {
    1: "#FFFFFF",  # H
    6: "#808080",  # C
    7: "#2244FF",  # N
    8: "#FF2222",  # O
    16: "#FFFF22", # S
    15: "#FF8800", # P
}

ELEMENT_RADII = {
    1: 3, 6: 6, 7: 5, 8: 5, 16: 7, 15: 6
}

ROLE_COLORS = {
    1: "#2266FF",  # BACKBONE_N
    2: "#44AA44",  # ALPHA_C
    3: "#FF6644",  # CARBONYL_C
    4: "#FF2222",  # CARBONYL_O
    5: "#6644FF",  # AMIDE_N
    6: "#AAAAAA",  # SIDECHAIN
    7: "#44CCFF",  # H_DONOR
    8: "#FF44CC",  # H_ACCEPTOR
    9: "#FFFF00",  # SULFUR_LINKABLE
}

SC_CLASS_COLORS = {
    0: "#888888",  # NONE
    1: "#44AA44",  # HYDROPHOBIC
    2: "#44AAFF",  # POLAR
    3: "#FF4444",  # ACIDIC
    4: "#4444FF",  # BASIC
    5: "#FF8800",  # AROMATIC
    6: "#FFFF00",  # SULFUR
}


def rot_x(x, y, z, c, s):
    return x, y*c - z*s, y*s + z*c

def rot_y(x, y, z, c, s):
    return x*c + z*s, y, -x*s + z*c

def project(x, y, z, cx, cy, fov=500.0):
    zz = z + fov
    if zz < 1: zz = 1
    return cx + x * fov / zz, cy - y * fov / zz, zz


class PeptideViewer3D:
    def __init__(self, root):
        self.root = root
        self.root.title("VSEPR-SIM | Peptide 3D | Port 9999")
        self.root.configure(bg=BG)
        self.root.geometry("1200x800")

        self.frames = deque(maxlen=64)
        self.current_frame = None
        self.frame_idx = 0
        self.running = True
        self.paused = False
        self.show_bonds = True
        self.color_mode = "role"  # role | element | charge | sc_class

        self.rot_ax = 0.25
        self.rot_ay = 0.40
        self.zoom = 1.0
        self._drag_start = None

        self._build_ui()
        self._start_receiver()
        self._poll()

    def _build_ui(self):
        top = tk.Frame(self.root, bg=HDR_BG, height=40)
        top.pack(fill=tk.X); top.pack_propagate(False)
        tk.Label(top, text="  VSEPR-SIM  Peptide 3D  :9999",
                 font=FONT_T, fg=ACCENT, bg=HDR_BG).pack(side=tk.LEFT, padx=8)
        self.lbl_status = tk.Label(top, text="Waiting...", font=FONT_S, fg=ACCENT3, bg=HDR_BG)
        self.lbl_status.pack(side=tk.RIGHT, padx=8)
        self.lbl_mode = tk.Label(top, text="[role]", font=FONT_XS, fg=DIM, bg=HDR_BG)
        self.lbl_mode.pack(side=tk.RIGHT, padx=4)

        body = tk.Frame(self.root, bg=BG)
        body.pack(fill=tk.BOTH, expand=True)
        body.columnconfigure(0, weight=1)
        body.columnconfigure(1, weight=0)
        body.rowconfigure(0, weight=1)

        self.cvs = tk.Canvas(body, bg=BG, highlightthickness=0, cursor="crosshair")
        self.cvs.grid(row=0, column=0, sticky="nsew")
        self.cvs.bind("<ButtonPress-1>", self._on_press)
        self.cvs.bind("<B1-Motion>", self._on_drag)
        self.cvs.bind("<MouseWheel>", self._on_scroll)

        side = tk.Frame(body, bg=PANEL_BG, width=220)
        side.grid(row=0, column=1, sticky="nsew")
        side.pack_propagate(False)
        self._build_side(side)

        self.root.bind("<KeyPress-r>", lambda e: self._reset_view())
        self.root.bind("<KeyPress-b>", lambda e: self._toggle_bonds())
        self.root.bind("<KeyPress-c>", lambda e: self._cycle_color())
        self.root.bind("<KeyPress-n>", lambda e: self._next_frame())
        self.root.bind("<space>", lambda e: self._toggle_pause())
        self.root.bind("<Escape>", lambda e: self._quit())
        self.root.bind("<KeyPress-q>", lambda e: self._quit())

    def _build_side(self, parent):
        self.side_labels = {}
        fields = [
            "Sequence", "Environment", "State",
            "Atoms", "Residues", "Bonds", "H-bonds",
            "E total", "E bond", "E vdw", "E coulomb", "E solv", "E form",
            "Steric", "Electrostatic", "Hydrophobic", "Planarity", "Confidence",
            "Chem valid", "Val valid"
        ]
        for f in fields:
            row = tk.Frame(parent, bg=PANEL_BG)
            row.pack(fill=tk.X, padx=4, pady=1)
            tk.Label(row, text=f+":", font=FONT_XS, fg=DIM, bg=PANEL_BG,
                     width=14, anchor="w").pack(side=tk.LEFT)
            lbl = tk.Label(row, text="—", font=FONT_XS, fg=FG, bg=PANEL_BG, anchor="w")
            lbl.pack(side=tk.LEFT, fill=tk.X, expand=True)
            self.side_labels[f] = lbl

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
                        self.current_frame = frame
                        self.frame_idx = len(self.frames) - 1
                except socket.timeout:
                    pass
                except Exception:
                    pass
            sock.close()
        t = threading.Thread(target=receiver, daemon=True)
        t.start()

    def _poll(self):
        if self.current_frame:
            self._render(self.current_frame)
            self._update_side(self.current_frame)
            self.lbl_status.config(
                text=f"Run {self.current_frame.get('run_id','?')} | "
                     f"{len(self.frames)} frames",
                fg=SUCCESS)
        self.root.after(50, self._poll)

    def _render(self, frame):
        self.cvs.delete("all")
        w = self.cvs.winfo_width()
        h = self.cvs.winfo_height()
        cx, cy = w / 2, h / 2

        atoms = frame.get("atoms", [])
        if not atoms:
            self.cvs.create_text(cx, cy, text="No atoms", fill=DIM, font=FONT_T)
            return

        # Center the molecule
        avg_x = sum(a["x"] for a in atoms) / len(atoms)
        avg_y = sum(a["y"] for a in atoms) / len(atoms)
        avg_z = sum(a["z"] for a in atoms) / len(atoms)

        cos_x, sin_x = math.cos(self.rot_ax), math.sin(self.rot_ax)
        cos_y, sin_y = math.cos(self.rot_ay), math.sin(self.rot_ay)

        projected = []
        for a in atoms:
            x = (a["x"] - avg_x) * self.zoom * 40
            y = (a["y"] - avg_y) * self.zoom * 40
            z = (a["z"] - avg_z) * self.zoom * 40
            x, y, z = rot_x(x, y, z, cos_x, sin_x)
            x, y, z = rot_y(x, y, z, cos_y, sin_y)
            sx, sy, sz = project(x, y, z, cx, cy)
            projected.append((a, sx, sy, sz))

        # Sort by depth for painter's algorithm
        projected.sort(key=lambda p: -p[3])

        # Draw backbone trace
        if self.show_bonds:
            residues = frame.get("residues", [])
            backbone_atoms = [a for a in atoms if a.get("role", 0) in (1, 2, 3)]
            backbone_atoms.sort(key=lambda a: a["id"])
            for i in range(len(backbone_atoms) - 1):
                a1 = backbone_atoms[i]
                a2 = backbone_atoms[i + 1]
                # Find their projected positions
                p1 = p2 = None
                for p in projected:
                    if p[0]["id"] == a1["id"]: p1 = p
                    if p[0]["id"] == a2["id"]: p2 = p
                if p1 and p2:
                    self.cvs.create_line(p1[1], p1[2], p2[1], p2[2],
                                         fill="#555577", width=1)

        # Draw atoms
        for a, sx, sy, sz in projected:
            Z = a.get("Z", 6)
            role = a.get("role", 0)
            sc = a.get("sc_class", 0) if "sc_class" in a else 0

            if self.color_mode == "element":
                color = ELEMENT_COLORS.get(Z, "#888888")
            elif self.color_mode == "role":
                color = ROLE_COLORS.get(role, "#888888")
            elif self.color_mode == "charge":
                q = a.get("q", 0)
                if q < -0.1: color = "#FF4444"
                elif q > 0.1: color = "#4444FF"
                else: color = "#888888"
            elif self.color_mode == "sc_class":
                color = SC_CLASS_COLORS.get(sc, "#888888")
            else:
                color = "#888888"

            r = ELEMENT_RADII.get(Z, 5) * self.zoom
            depth_fade = max(0.3, min(1.0, 800 / max(sz, 1)))
            r *= depth_fade

            self.cvs.create_oval(sx - r, sy - r, sx + r, sy + r,
                                 fill=color, outline="", tags="atom")

        # HUD
        seq = frame.get("sequence", "?")
        e_total = frame.get("energy", {}).get("total", 0)
        conf = frame.get("scores", {}).get("confidence", 0)
        self.cvs.create_text(10, h - 40, text=f"Seq: {seq}", fill=ACCENT,
                             font=FONT_XS, anchor="w")
        self.cvs.create_text(10, h - 25, text=f"E={e_total:+.1f} kJ/mol  conf={conf:.3f}",
                             fill=GOLD, font=FONT_XS, anchor="w")
        self.cvs.create_text(10, h - 10,
                             text=f"[{self.color_mode}] bonds={'ON' if self.show_bonds else 'OFF'} "
                                  f"zoom={self.zoom:.1f}",
                             fill=DIM, font=FONT_XS, anchor="w")

    def _update_side(self, frame):
        s = self.side_labels
        s["Sequence"].config(text=frame.get("sequence", "?")[:24])
        s["Environment"].config(text=frame.get("environment", "?"))
        s["State"].config(text=frame.get("state", "?"))
        s["Atoms"].config(text=str(frame.get("atom_count", 0)))
        s["Residues"].config(text=str(frame.get("residue_count", 0)))
        s["Bonds"].config(text=str(frame.get("bond_count", 0)))
        s["H-bonds"].config(text=str(frame.get("hbond_count", 0)))

        e = frame.get("energy", {})
        s["E total"].config(text=f"{e.get('total', 0):+.2f}")
        s["E bond"].config(text=f"{e.get('bond', 0):+.2f}")
        s["E vdw"].config(text=f"{e.get('vdw', 0):+.2f}")
        s["E coulomb"].config(text=f"{e.get('coulomb', 0):+.2f}")
        s["E solv"].config(text=f"{e.get('solvation', 0):+.2f}")
        s["E form"].config(text=f"{e.get('formation', 0):+.2f}")

        sc = frame.get("scores", {})
        s["Steric"].config(text=f"{sc.get('steric', 0):.3f}")
        s["Electrostatic"].config(text=f"{sc.get('electrostatic', 0):.3f}")
        s["Hydrophobic"].config(text=f"{sc.get('hydrophobic', 0):.3f}")
        s["Planarity"].config(text=f"{sc.get('planarity', 0):.3f}")
        s["Confidence"].config(text=f"{sc.get('confidence', 0):.4f}")

        val = frame.get("validity", {})
        s["Chem valid"].config(text="YES" if val.get("chemical") else "NO",
                               fg=SUCCESS if val.get("chemical") else ACCENT3)
        s["Val valid"].config(text="YES" if val.get("valence") else "NO",
                              fg=SUCCESS if val.get("valence") else ACCENT3)

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
        if e.delta > 0:
            self.zoom *= 1.1
        else:
            self.zoom /= 1.1
        self.zoom = max(0.1, min(10.0, self.zoom))

    def _reset_view(self):
        self.rot_ax = 0.25; self.rot_ay = 0.40; self.zoom = 1.0

    def _toggle_bonds(self):
        self.show_bonds = not self.show_bonds

    def _cycle_color(self):
        modes = ["role", "element", "charge", "sc_class"]
        idx = modes.index(self.color_mode)
        self.color_mode = modes[(idx + 1) % len(modes)]
        self.lbl_mode.config(text=f"[{self.color_mode}]")

    def _next_frame(self):
        if self.frames:
            self.frame_idx = (self.frame_idx + 1) % len(self.frames)
            self.current_frame = self.frames[self.frame_idx]

    def _toggle_pause(self):
        self.paused = not self.paused

    def _quit(self):
        self.running = False
        self.root.destroy()


if __name__ == "__main__":
    root = tk.Tk()
    app = PeptideViewer3D(root)
    root.protocol("WM_DELETE_WINDOW", app._quit)
    root.mainloop()
