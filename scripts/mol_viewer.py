#!/usr/bin/env python3
"""
mol_viewer.py -- VSEPR-Sim live molecule viewer.

Controls:
  Left-drag          Rotate
  Scroll             Zoom
  Right-click atom   Context menu: Delete / Duplicate

Buttons (bottom bar):
  Export PNG   Export PDF   Save .dynx + .X

Usage:
  python scripts/mol_viewer.py [path/to/file]
  python scripts/mol_viewer.py path/to/file --export-png figure.png --frame last

Supported formats: .xyz .xyza .xyzf .xyzFull .xyzc .dynx .X

The headless export path uses Matplotlib Agg and draws no buttons or panels.
"""

from __future__ import annotations

import io
import math
import os
import sys
import tempfile
import argparse
import importlib.util
from pathlib import Path
from typing import List, Tuple, Optional, Dict

import numpy as np
import matplotlib
_HEADLESS_REQUESTED = "--export-png" in sys.argv or "--no-window" in sys.argv
matplotlib.use("Agg" if _HEADLESS_REQUESTED else "qtagg")
import matplotlib.pyplot as plt
from mpl_toolkits.mplot3d import Axes3D
from matplotlib.colors import LightSource
from matplotlib.widgets import Button, Slider

_SCRIPT_DIR = Path(__file__).resolve().parent
_ROOT       = _SCRIPT_DIR.parent
sys.path.insert(0, str(_SCRIPT_DIR))

_ELEMENT_DATA_PATH = _ROOT / "archive" / "pykernel" / "element_data.py"
_ELEMENT_DATA_SPEC = importlib.util.spec_from_file_location(
    "vsepr_element_data", _ELEMENT_DATA_PATH
)
if _ELEMENT_DATA_SPEC is None or _ELEMENT_DATA_SPEC.loader is None:
    raise ImportError("Cannot load authoritative element data from " + str(_ELEMENT_DATA_PATH))
_ELEMENT_DATA = importlib.util.module_from_spec(_ELEMENT_DATA_SPEC)
_ELEMENT_DATA_SPEC.loader.exec_module(_ELEMENT_DATA)

cpk_color = _ELEMENT_DATA.cpk_color
covalent_radius = _ELEMENT_DATA.covalent_radius
vdw_radius = _ELEMENT_DATA.vdw_radius
molecules_dir = _ELEMENT_DATA.molecules_dir
import mol_io
from mol_io import MolData, Frame, detect_bonds, write_xyz, write_dynx, write_x_bundle

BG          = "#0d1117"
BTN_COLOR   = "#21262d"
BTN_HOVER   = "#30363d"
SEL_COLOR   = "#f0883e"
SPHERE_N    = 28
SPHERE_RAD  = 0.65
BOND_RAD    = 0.07
LABEL_COLOR = "#c9d1d9"


def _sphere_surface(cx, cy, cz, r, n=SPHERE_N):
    u = np.linspace(0, 2 * np.pi, n)
    v = np.linspace(0, np.pi, n)
    x = cx + r * np.outer(np.cos(u), np.sin(v))
    y = cy + r * np.outer(np.sin(u), np.sin(v))
    z = cz + r * np.outer(np.ones_like(u), np.cos(v))
    return x, y, z


def _cylinder_between(p1, p2, rad=BOND_RAD, n=12):
    v = np.array(p2, dtype=float) - np.array(p1, dtype=float)
    mag = np.linalg.norm(v)
    if mag < 1e-8:
        return None
    v /= mag
    nv = np.array([1, 0, 0]) if abs(v[0]) < 0.9 else np.array([0, 1, 0])
    n1 = np.cross(v, nv); n1 /= np.linalg.norm(n1)
    n2 = np.cross(v, n1)
    t  = np.linspace(0, mag, 2)
    th = np.linspace(0, 2 * np.pi, n + 1)
    r  = np.array([
        p1[i] + v[i] * t[:, None]
        + rad * np.sin(th[None, :]) * n1[i]
        + rad * np.cos(th[None, :]) * n2[i]
        for i in range(3)
    ])
    return r[0], r[1], r[2]


def _setup_axes(atoms, ax, elev=20, azim=35, pad=2.0):
    if not atoms:
        return
    xs = [a[1] for a in atoms]
    ys = [a[2] for a in atoms]
    zs = [a[3] for a in atoms]
    cx, cy, cz = np.mean(xs), np.mean(ys), np.mean(zs)
    span = max(max(xs)-min(xs), max(ys)-min(ys), max(zs)-min(zs)) / 2 + pad
    ax.set_xlim(cx-span, cx+span)
    ax.set_ylim(cy-span, cy+span)
    ax.set_zlim(cz-span, cz+span)
    ax.set_box_aspect([1,1,1])
    ax.view_init(elev=elev, azim=azim)
    ax.axis("off")


def _style_btn(btn):
    btn.label.set_fontfamily("monospace")
    btn.label.set_fontsize(8)
    btn.label.set_color(LABEL_COLOR)


class MolViewer:
    def __init__(self, data, src_path, controls=True, initial_frame=0):
        self.data      = data
        self.src_path  = src_path
        self.controls  = controls
        self.frame_idx = initial_frame
        self.atoms     = list(data.frames[initial_frame])
        self.bonds     = detect_bonds(self.atoms, covalent_radius)
        self.selected  = None
        self._build_ui()
        self._redraw()

    def _build_ui(self):
        n_frames   = len(self.data.frames)
        has_slider = n_frames > 1

        self.fig = plt.figure(figsize=(10, 8), facecolor=BG)
        if not self.controls:
            self.ax = self.fig.add_axes([0.0, 0.0, 1.0, 1.0], projection="3d", facecolor=BG)
            self.ls = LightSource(azdeg=315, altdeg=45)
            self.slider = None
            return

        self.fig.canvas.manager.set_window_title(
            "VSEPR-Sim  -  " + self.data.title + "  [" + self.data.fmt + "]"
        )

        top = 0.10 if has_slider else 0.06
        self.ax = self.fig.add_axes([0.0, top, 1.0, 1.0 - top], projection="3d", facecolor=BG)
        self.ls = LightSource(azdeg=315, altdeg=45)

        by = 0.005
        ax_png  = self.fig.add_axes([0.01,  by, 0.12, 0.038])
        ax_pdf  = self.fig.add_axes([0.145, by, 0.12, 0.038])
        ax_save = self.fig.add_axes([0.27,  by, 0.20, 0.038])

        self.btn_png  = Button(ax_png,  "Export PNG",      color=BTN_COLOR, hovercolor=BTN_HOVER)
        self.btn_pdf  = Button(ax_pdf,  "Export PDF",      color=BTN_COLOR, hovercolor=BTN_HOVER)
        self.btn_save = Button(ax_save, "Save .dynx + .X", color=BTN_COLOR, hovercolor=BTN_HOVER)

        for b in (self.btn_png, self.btn_pdf, self.btn_save):
            _style_btn(b)

        self.btn_png.on_clicked( lambda _: self._export("png"))
        self.btn_pdf.on_clicked( lambda _: self._export("pdf"))
        self.btn_save.on_clicked(lambda _: self._save_bundle())

        self.slider = None
        if has_slider:
            ax_sl = self.fig.add_axes([0.52, by + 0.005, 0.45, 0.025])
            ax_sl.set_facecolor(BTN_COLOR)
            self.slider = Slider(ax_sl, "", 0, n_frames - 1, valinit=0, valstep=1, color="#58a6ff")
            self.slider.label.set_color(LABEL_COLOR)
            self.slider.valtext.set_color(LABEL_COLOR)
            self.slider.on_changed(self._on_slider)

        self.fig.canvas.mpl_connect("button_press_event", self._on_click)
        self.fig.canvas.mpl_connect("key_press_event",    self._on_key)

    def _redraw(self):
        self.ax.cla()
        self.ax.set_facecolor(BG)
        ls = self.ls

        for i, j in self.bonds:
            _, xi, yi, zi = self.atoms[i]
            _, xj, yj, zj = self.atoms[j]
            cyl = _cylinder_between((xi,yi,zi), (xj,yj,zj))
            if cyl is not None:
                self.ax.plot_surface(cyl[0], cyl[1], cyl[2],
                    color=(0.48,0.48,0.48), alpha=0.90,
                    shade=True, lightsource=ls, antialiased=True, linewidth=0)

        for idx, (elem, x, y, z) in enumerate(self.atoms):
            col = cpk_color(elem)
            r   = vdw_radius(elem) * SPHERE_RAD
            sx, sy, sz = _sphere_surface(x, y, z, r)
            is_sel = idx == self.selected
            self.ax.plot_surface(sx, sy, sz,
                color=(SEL_COLOR if is_sel else col),
                shade=True, lightsource=ls, antialiased=True, linewidth=0, alpha=0.90)

        _setup_axes(self.atoms, self.ax)
        self.fig.canvas.draw_idle()

    def _on_slider(self, val):
        idx = int(round(val))
        if idx != self.frame_idx:
            self.frame_idx = idx
            self.atoms     = list(self.data.frames[idx])
            self.bonds     = detect_bonds(self.atoms, covalent_radius)
            self.selected  = None
            self._redraw()

    def _on_click(self, event):
        if event.button != 3 or event.inaxes is not self.ax:
            return
        hit = self._pick_atom(event.x, event.y)
        if hit is None:
            return
        self.selected = hit
        self._redraw()
        self._show_context_menu(event, hit)

    def _pick_atom(self, mx, my):
        best_idx  = None
        best_dist = float("inf")
        tol_sq    = 32 ** 2
        from mpl_toolkits.mplot3d import proj3d
        for idx, (elem, x, y, z) in enumerate(self.atoms):
            try:
                x2d, y2d, _ = proj3d.proj_transform(x, y, z, self.ax.get_proj())
                disp = self.ax.transData.transform((x2d, y2d))
            except Exception:
                continue
            dx = disp[0] - mx
            dy = disp[1] - my
            d2 = dx*dx + dy*dy
            if d2 < tol_sq and d2 < best_dist:
                best_dist = d2
                best_idx  = idx
        return best_idx

    def _show_context_menu(self, event, atom_idx):
        try:
            from PyQt5.QtWidgets import QMenu
            from PyQt5.QtGui import QCursor

            menu = QMenu(self.fig.canvas)
            menu.setStyleSheet(
                "QMenu { background-color:#21262d; color:#c9d1d9; "
                "border:1px solid #30363d; font-family:monospace; font-size:12px; }"
                "QMenu::item:selected { background-color:#388bfd; }"
            )
            sym  = self.atoms[atom_idx][0]
            hdr  = menu.addAction(sym + "  (atom " + str(atom_idx) + ")")
            hdr.setEnabled(False)
            menu.addSeparator()
            act_del = menu.addAction("Delete")
            act_dup = menu.addAction("Duplicate")
            chosen  = menu.exec_(QCursor.pos())
            if chosen == act_del:
                self._atom_delete(atom_idx)
            elif chosen == act_dup:
                self._atom_duplicate(atom_idx)
        except ImportError:
            print("[mol_viewer] Right-click on atom " + str(atom_idx)
                  + " (" + self.atoms[atom_idx][0] + ")")
            print("  Press D to delete, U to duplicate, Esc to cancel")

    def _atom_delete(self, idx):
        if 0 <= idx < len(self.atoms):
            del self.atoms[idx]
            self.bonds    = detect_bonds(self.atoms, covalent_radius)
            self.selected = None
            self._redraw()

    def _atom_duplicate(self, idx):
        if 0 <= idx < len(self.atoms):
            sym, x, y, z = self.atoms[idx]
            off = vdw_radius(sym) * 0.5
            self.atoms.insert(idx + 1, (sym, x + off, y + off, z))
            self.bonds    = detect_bonds(self.atoms, covalent_radius)
            self.selected = idx + 1
            self._redraw()

    def _on_key(self, event):
        if self.selected is None:
            return
        k = (event.key or "").lower()
        if k == "d":
            self._atom_delete(self.selected)
        elif k == "u":
            self._atom_duplicate(self.selected)
        elif k == "escape":
            self.selected = None
            self._redraw()

    def _export(self, fmt):
        try:
            from tkinter import filedialog, Tk
            root = Tk(); root.withdraw()
            ext  = "." + fmt
            path = filedialog.asksaveasfilename(
                initialfile=self.data.title + ext,
                defaultextension=ext,
                filetypes=[(fmt.upper(), "*" + ext), ("All files", "*.*")],
                title="Export as " + fmt.upper(),
            )
            root.destroy()
        except Exception:
            path = str(Path(self.src_path).with_suffix("." + fmt))
        if path:
            self.fig.savefig(path, dpi=200, facecolor=self.fig.get_facecolor(),
                             bbox_inches="tight", pad_inches=0.08)
            print("  saved  " + path)

    def _save_bundle(self):
        try:
            from tkinter import filedialog, Tk
            root = Tk(); root.withdraw()
            out_dir = filedialog.askdirectory(title="Output directory for .dynx + .X")
            root.destroy()
        except Exception:
            out_dir = str(Path(self.src_path).parent)
        if not out_dir:
            return

        name = self.data.title
        out  = Path(out_dir)

        buf = io.StringIO()
        buf.write(str(len(self.atoms)) + "\n")
        buf.write("mol_viewer export -- " + name + "\n")
        for sym, x, y, z in self.atoms:
            buf.write(sym + "  " + str(round(x,8)) + "  " + str(round(y,8)) + "  " + str(round(z,8)) + "\n")
        xyz_content = buf.getvalue()

        xyz_path = str(out / (name + ".xyz"))
        with open(xyz_path, "w", encoding="utf-8") as f:
            f.write(xyz_content)

        dynx_path = str(out / (name + ".dynx"))
        write_dynx(dynx_path, self.atoms, source_path=self.src_path)
        with open(dynx_path, "r", encoding="utf-8") as f:
            dynx_content = f.read()

        x_path = str(out / (name + ".X"))
        write_x_bundle(x_path, self.atoms, name, xyz_content, dynx_content)

        print("  saved  " + xyz_path)
        print("  saved  " + dynx_path)
        print("  saved  " + x_path)


def main():
    parser = argparse.ArgumentParser(description="VSEPR-Sim Matplotlib molecular renderer")
    parser.add_argument("source", nargs="?", help="XYZ-family or VSEPR molecule artifact")
    parser.add_argument("--export-png", metavar="PATH", help="write a UI-free PNG and exit")
    parser.add_argument("--frame", default="first", help="first, last, or a zero-based frame index")
    parser.add_argument("--no-window", action="store_true", help="use the non-interactive Agg backend")
    args = parser.parse_args()

    src_path = args.source or str(molecules_dir() / "h2o_optimized.xyz")
    try:
        data = mol_io.load_file(src_path)
    except Exception as exc:
        print("[mol_viewer] Failed to load " + repr(src_path) + ": " + str(exc), file=sys.stderr)
        sys.exit(1)

    print("[mol_viewer] " + data.fmt + "  |  " + str(len(data.frames)) + " frame(s)"
          + "  |  " + str(len(data.atoms)) + " atoms  |  " + repr(data.title))

    if args.frame == "first":
        frame_idx = 0
    elif args.frame == "last":
        frame_idx = len(data.frames) - 1
    else:
        try:
            frame_idx = int(args.frame)
        except ValueError:
            parser.error("--frame must be first, last, or a zero-based integer")
        if frame_idx < 0 or frame_idx >= len(data.frames):
            parser.error("--frame index is outside the loaded trajectory")

    if args.export_png:
        output_path = Path(args.export_png)
        output_path.parent.mkdir(parents=True, exist_ok=True)
        viewer = MolViewer(data, src_path, controls=False, initial_frame=frame_idx)
        viewer.fig.savefig(output_path, dpi=240, facecolor=viewer.fig.get_facecolor(),
                           bbox_inches="tight", pad_inches=0.08)
        plt.close(viewer.fig)
        print("[matplotlib-png:written] " + str(output_path))
        return

    MolViewer(data, src_path, initial_frame=frame_idx)
    plt.show()


if __name__ == "__main__":
    main()
