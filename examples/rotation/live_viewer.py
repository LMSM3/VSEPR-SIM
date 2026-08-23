#!/usr/bin/env python3
"""
examples/rotation/live_viewer.py
=================================
Live 800x800 pixel viewer window for xsim rotation examples.

Watches a simulation output directory and repaints whenever new files
are written.  Designed to run alongside the GL simulation window launched
by run_rotation.sh.

Features
--------
  !new    Wipe the watch directory and restart via run_rotation.sh !new.
  !live   Force a manual refresh right now.
  Auto-refresh every 800 ms whenever the output directory changes.

Layout (800x800)
----------------
  Top half  : particle scatter (XY projection, last XYZ frame)
  Bottom L  : total_energy trace from metrics.tsv
  Bottom R  : temperature_K trace from metrics.tsv
  Footer    : command bar  ( !live | !new )  + status line

Dependencies
------------
  pip install matplotlib watchdog
  (matplotlib is mandatory; watchdog adds filesystem events, falls back to polling)

Usage (direct)
--------------
  python3 examples/rotation/live_viewer.py \\
      --watch out/sphere_shaded_sim --title sphere_shaded --live

Usage via wrapper
-----------------
  bash examples/rotation/run_rotation.sh sphere_shaded
"""

from __future__ import annotations

import argparse
import os
import shutil
import subprocess
import sys
import json
import threading
import time
from datetime import datetime
from pathlib import Path

# ── mandatory dep: matplotlib ─────────────────────────────────────────────────
try:
    import matplotlib
    matplotlib.use("TkAgg")
    import matplotlib.pyplot as plt
    import matplotlib.gridspec as gridspec
    from matplotlib.widgets import TextBox
    from matplotlib.animation import FuncAnimation
except ImportError:
    sys.exit("[live_viewer] matplotlib is required:  pip install matplotlib")

# ── optional dep: watchdog ────────────────────────────────────────────────────
try:
    from watchdog.observers import Observer
    from watchdog.events import FileSystemEventHandler
    _HAVE_WATCHDOG = True
except ImportError:
    _HAVE_WATCHDOG = False
    print("[live_viewer] watchdog not installed — polling fallback active")
    print("              pip install watchdog  for filesystem-event refresh")

# ── constants ─────────────────────────────────────────────────────────────────

WIN_PX    = 800
WIN_DPI   = 100
POLL_MS   = 800
MAX_ATOMS = 4000

BG_DARK   = "#0d0d0d"
BG_PANEL  = "#111111"
FG_DIM    = "#555555"
FG_MID    = "#888888"
FG_BRIGHT = "#cccccc"
ACCENT    = "#5588ff"

ATOM_COLORS: dict[str, str] = {
    "Fe": "#c0903a", "Ar": "#00d4ff", "Si": "#c060ff",
    "O":  "#ff3333", "C":  "#66ff66", "H":  "#eeeeee",
    "N":  "#3355ff", "S":  "#ffee00", "P":  "#ff8800",
    "Na": "#ffaa22", "Cl": "#88ff44", "Ca": "#eecc88",
    "Mg": "#44ee88", "Al": "#aaaaff", "Cu": "#ff8833",
    "Au": "#ffd700", "Ag": "#c8c8c8", "Zn": "#88ccaa",
}
_DEFAULT_COLOR = "#aaaaaa"

# Van-der-Waals radii (Angstrom) -- used to scale scatter point sizes.
ATOM_RADII: dict[str, float] = {
    "Fe": 1.26, "Ar": 1.88, "Si": 1.17, "O": 0.73, "C": 0.77,
    "H":  0.31, "N":  0.75, "S":  1.04, "P": 1.07,
    "Na": 1.86, "Cl": 1.02, "Ca": 1.74, "Mg": 1.45,
    "Al": 1.21, "Cu": 1.28, "Au": 1.44, "Ag": 1.44, "Zn": 1.22,
}
_DEFAULT_RADIUS = 1.0
_PT_BASE        = 10.0   # scatter area = (_PT_BASE * radius)^2 / 2

KNOWN_COMMANDS = (
    "!live", "!new", "!pause", "!resume",
    "!frame <N>", "!frame +", "!frame -",
    "!scene cube", "!scene sphere", "!scene sphere_shaded",
    "!help",
)

# ── XYZ parser ────────────────────────────────────────────────────────────────

def parse_xyz_last_frame(
    path: Path,
) -> tuple[list[str], list[float], list[float], list[float]]:
    """Return (symbols, xs, ys, zs) from the last frame of an .xyz file."""
    try:
        text = path.read_text(errors="replace")
    except OSError:
        return [], [], [], []

    lines = text.splitlines()
    last_atoms: list[str] = []
    i = 0
    while i < len(lines):
        s = lines[i].strip()
        if not s:
            i += 1
            continue
        try:
            n = int(s)
        except ValueError:
            i += 1
            continue
        end = i + 2 + n
        if end > len(lines):
            break
        last_atoms = lines[i + 2 : end]
        i = end

    syms: list[str] = []
    xs: list[float] = []
    ys: list[float] = []
    zs: list[float] = []
    for line in last_atoms[:MAX_ATOMS]:
        parts = line.split()
        if len(parts) < 4:
            continue
        try:
            syms.append(parts[0])
            xs.append(float(parts[1]))
            ys.append(float(parts[2]))
            zs.append(float(parts[3]))
        except ValueError:
            continue
    return syms, xs, ys, zs


def atom_colors(syms: list[str]) -> list[str]:
    return [ATOM_COLORS.get(s, _DEFAULT_COLOR) for s in syms]


def atom_sizes(syms: list[str]) -> list[float]:
    """Scatter point areas scaled to Van-der-Waals radii."""
    return [(_PT_BASE * ATOM_RADII.get(s, _DEFAULT_RADIUS)) ** 2 / 2 for s in syms]


# ── frame-aware XYZ parser ────────────────────────────────────────────────────────

def _read_xyz_frames_raw(path: Path, max_frames: int = 1000) -> list[list[str]]:
    """Scan an XYZ file; return raw atom-line lists, one entry per frame."""
    try:
        text = path.read_text(errors="replace")
    except OSError:
        return []
    lines = text.splitlines()
    frames: list[list[str]] = []
    i = 0
    while i < len(lines) and len(frames) < max_frames:
        s = lines[i].strip()
        if not s:
            i += 1
            continue
        try:
            n = int(s)
        except ValueError:
            i += 1
            continue
        end = i + 2 + n
        if end > len(lines):
            break
        frames.append(lines[i + 2 : end])
        i = end
    return frames


def parse_xyz_frame(
    path: Path,
    frame_idx: int = -1,
) -> tuple[list[str], list[float], list[float], list[float], int]:
    """
    Parse one frame from an XYZ file.

    frame_idx = -1  last frame (default).
    frame_idx >= 0  that frame (clamped to valid range).

    Returns (syms, xs, ys, zs, total_frame_count).
    """
    raw = _read_xyz_frames_raw(path)
    total = len(raw)
    if not raw:
        return [], [], [], [], 0

    idx = max(0, min(frame_idx, total - 1)) if frame_idx >= 0 else total - 1
    atom_lines = raw[idx][:MAX_ATOMS]

    syms: list[str] = []
    xs:   list[float] = []
    ys:   list[float] = []
    zs:   list[float] = []
    for line in atom_lines:
        parts = line.split()
        if len(parts) < 4:
            continue
        try:
            syms.append(parts[0])
            xs.append(float(parts[1]))
            ys.append(float(parts[2]))
            zs.append(float(parts[3]))
        except ValueError:
            continue
    return syms, xs, ys, zs, total


# ── analysis.json parser ────────────────────────────────────────────────────────

def parse_analysis_json(path: Path) -> dict[str, str]:
    """
    Read analysis.json and return a flat string-value display dict.

    Keys with nested structure are flattened to the two innermost levels
    and shortened to fit the info panel.  Capped at 12 entries.
    """
    display: dict[str, str] = {}
    try:
        raw = json.loads(path.read_text(errors="replace"))
    except (OSError, json.JSONDecodeError):
        return display

    def _flatten(obj: object, prefix: str = "") -> None:
        if isinstance(obj, dict):
            for k, v in obj.items():
                _flatten(v, f"{prefix}{k}.")
        elif isinstance(obj, (int, float)):
            key = prefix.rstrip(".")
            parts = key.split(".")
            short = ".".join(parts[-2:]) if len(parts) > 2 else key
            display[short] = f"{obj:.4g}" if isinstance(obj, float) else str(obj)
        elif isinstance(obj, str) and len(obj) < 40:
            key = prefix.rstrip(".")
            parts = key.split(".")
            short = ".".join(parts[-2:]) if len(parts) > 2 else key
            display[short] = obj

    _flatten(raw)
    return dict(list(display.items())[:12])


# ── scene helper ────────────────────────────────────────────────────────────────

def _scene_watch_subdir(scene: str) -> str:
    """Map a scene name to its output subdirectory under out/."""
    return {
        "cube":          "cube_rotation",
        "sphere":        "sphere_rotation",
        "sphere_shaded": "sphere_shaded_sim",
    }.get(scene, scene)


# ── metrics TSV parser ────────────────────────────────────────────────────────

def parse_metrics(path: Path) -> dict[str, list[float]]:
    data: dict[str, list[float]] = {}
    try:
        rows = path.read_text(errors="replace").splitlines()
    except OSError:
        return data
    if not rows:
        return data
    headers = rows[0].split("\t")
    cols: dict[str, list[float]] = {h: [] for h in headers}
    for row in rows[1:]:
        parts = row.split("\t")
        for h, v in zip(headers, parts):
            try:
                cols[h].append(float(v))
            except ValueError:
                pass
    return cols


# ── directory scanner ─────────────────────────────────────────────────────────

def scan_dir(watch_dir: Path) -> dict[str, Path]:
    hits: dict[str, Path] = {}
    for name in (
        "trajectory.xyz", "frames.xyz",
        "metrics.tsv",
        "analysis.json", "report.md", "manifest.json",
    ):
        p = watch_dir / name
        if p.exists():
            hits[name] = p
    return hits


# ── watchdog handler ──────────────────────────────────────────────────────────

class _FSHandler(FileSystemEventHandler):
    def __init__(self, flag: threading.Event) -> None:
        self._flag = flag

    def on_any_event(self, _event) -> None:  # type: ignore[override]
        self._flag.set()


# ── viewer ────────────────────────────────────────────────────────────────────

class LiveViewer:
    def __init__(self, watch_dir: Path, title: str, live: bool) -> None:
        self.watch_dir      = watch_dir
        self.title          = title
        self.live           = live
        self._dirty         = threading.Event()
        self._dirty.set()
        self._last_mtime    = 0.0
        self._observer      = None
        # playback / frame state
        self._paused        = False
        self._frame_idx     = -1     # -1 = always show last frame
        self._frame_total   = 0
        self._elapsed_start = time.monotonic()

        if _HAVE_WATCHDOG and live:
            self._start_watchdog()

        self._build_figure()

    # ── watchdog ──────────────────────────────────────────────────────────────

    def _start_watchdog(self) -> None:
        self.watch_dir.mkdir(parents=True, exist_ok=True)
        handler = _FSHandler(self._dirty)
        self._observer = Observer()
        self._observer.schedule(handler, str(self.watch_dir), recursive=True)
        self._observer.start()

    # ── figure construction ───────────────────────────────────────────────────

    def _build_figure(self) -> None:
        size_in = WIN_PX / WIN_DPI
        self.fig = plt.figure(
            figsize=(size_in, size_in),
            dpi=WIN_DPI,
            facecolor=BG_DARK,
        )
        self.fig.canvas.manager.set_window_title(
            f"XSIM live  |  {self.title}  |  {WIN_PX}x{WIN_PX}"
        )

        # ── layout: 3 rows × 2 cols ───────────────────────────────────────
        #   row 0 [0,:]  — atom scatter        tall (spans both cols)
        #   row 1 [1,0]  — total energy trace
        #   row 1 [1,1]  — temperature trace
        #   row 2 [2,:]  — analysis.json info  short (spans both cols)
        # Status line, command bar, hints are outside the gridspec.
        gs = gridspec.GridSpec(
            3, 2,
            figure=self.fig,
            hspace=0.55,
            wspace=0.38,
            height_ratios=[2.2, 1.1, 0.55],
            left=0.08, right=0.97,
            top=0.91, bottom=0.20,
        )

        def _ax(spec):
            ax = self.fig.add_subplot(spec, facecolor=BG_PANEL)
            ax.tick_params(colors=FG_DIM, labelsize=6)
            for sp in ax.spines.values():
                sp.set_edgecolor("#2a2a2a")
            return ax

        self.ax_xyz  = _ax(gs[0, :])
        self.ax_ene  = _ax(gs[1, 0])
        self.ax_temp = _ax(gs[1, 1])
        self.ax_info = _ax(gs[2, :])
        self.ax_info.axis("off")   # text-only panel

        # frame counter + elapsed text inside the scatter panel title area
        self._lbl_frame = self.fig.text(
            0.97, 0.915, "",
            ha="right", va="top",
            color="#446688", fontsize=6.5,
        )

        # window title text
        self.fig.text(
            0.5, 0.965,
            f"XSIM  live viewer  —  {self.title}",
            ha="center", va="top",
            color="#ffffff", fontsize=9, fontweight="bold",
        )

        # status line
        self._lbl_status = self.fig.text(
            0.5, 0.155,
            "waiting for output...",
            ha="center", va="top",
            color=FG_DIM, fontsize=7,
        )

        # command bar
        ax_cmd = self.fig.add_axes(
            [0.08, 0.04, 0.84, 0.075],
            facecolor="#12122a",
        )
        self.cmd_box = TextBox(
            ax_cmd, " cmd > ", initial="",
            color="#12122a", hovercolor="#1e1e40",
            label_pad=0.01,
        )
        self.cmd_box.label.set_color(ACCENT)
        self.cmd_box.label.set_fontsize(8)
        self.cmd_box.text_disp.set_color(FG_BRIGHT)
        self.cmd_box.on_submit(self._on_cmd)

        # hint line
        self.fig.text(
            0.5, 0.013,
            "!live  !new  !pause  !resume  !frame N/+/-  !scene cube|sphere|sphere_shaded  !help",
            ha="center", va="bottom",
            color=FG_DIM, fontsize=6.0,
        )

        self._anim = FuncAnimation(
            self.fig,
            self._tick,
            interval=POLL_MS,
            cache_frame_data=False,
        )

    # ── command bar ───────────────────────────────────────────────────────────

    def _on_cmd(self, text: str) -> None:
        text = text.strip()
        self.cmd_box.set_val("")
        if not text:
            return

        if text == "!live":
            self._dirty.set()
            self._set_status("!live — forced refresh")

        elif text == "!new":
            self._do_new()

        elif text == "!pause":
            self._paused = True
            self._set_status("!pause — auto-refresh suspended  (type !resume to continue)")

        elif text == "!resume":
            self._paused = False
            self._dirty.set()
            self._set_status("!resume — auto-refresh active")

        elif text in ("!frame +", "!frame+"):
            if self._frame_total > 0:
                cur = self._frame_idx if self._frame_idx >= 0 else self._frame_total - 1
                self._frame_idx = min(cur + 1, self._frame_total - 1)
                self._dirty.set()
                self._set_status(f"!frame + — frame {self._frame_idx + 1}/{self._frame_total}")
            else:
                self._set_status("!frame + — no trajectory loaded yet")

        elif text in ("!frame -", "!frame-"):
            if self._frame_total > 0:
                cur = self._frame_idx if self._frame_idx >= 0 else self._frame_total - 1
                self._frame_idx = max(cur - 1, 0)
                self._dirty.set()
                self._set_status(f"!frame - — frame {self._frame_idx + 1}/{self._frame_total}")
            else:
                self._set_status("!frame - — no trajectory loaded yet")

        elif text.startswith("!frame "):
            rest = text[7:].strip()
            if rest == "last" or rest == "end":
                self._frame_idx = -1
                self._dirty.set()
                self._set_status("!frame last — tracking live tail")
            else:
                try:
                    n = int(rest)
                    if self._frame_total > 0:
                        self._frame_idx = max(0, min(n - 1, self._frame_total - 1))
                        self._dirty.set()
                        self._set_status(
                            f"!frame {n} — showing frame {self._frame_idx + 1}/{self._frame_total}"
                        )
                    else:
                        self._set_status(f"!frame {n} — no trajectory yet")
                except ValueError:
                    self._set_status(f"!frame: expected integer, got '{rest}'")

        elif text.startswith("!scene "):
            scene = text[7:].strip()
            valid_scenes = ("cube", "sphere", "sphere_shaded")
            if scene not in valid_scenes:
                self._set_status(f"!scene: unknown '{scene}'  valid: {' | '.join(valid_scenes)}")
                return
            sub = _scene_watch_subdir(scene)
            root = Path(__file__).parent.parent.parent   # repo root
            new_dir = root / "out" / sub
            new_dir.mkdir(parents=True, exist_ok=True)
            self.watch_dir = new_dir
            self.title = scene
            self._frame_idx = -1
            self._frame_total = 0
            self._elapsed_start = time.monotonic()
            # restart watchdog on the new directory
            if self._observer is not None:
                self._observer.stop()
                self._observer.join()
                self._observer = None
            if _HAVE_WATCHDOG and self.live:
                self._start_watchdog()
            self._dirty.set()
            self._set_status(f"!scene {scene} — watching {new_dir}")

        elif text == "!help":
            help_lines = [
                "!live               force repaint now",
                "!new                wipe outputs + restart run_rotation.sh",
                "!pause              suspend auto-refresh",
                "!resume             resume auto-refresh",
                "!frame N            jump to frame N (1-based)",
                "!frame +/-          step forward / backward one frame",
                "!frame last         return to live tail tracking",
                "!scene cube         switch watch dir to cube_rotation",
                "!scene sphere       switch watch dir to sphere_rotation",
                "!scene sphere_shaded switch watch dir to sphere_shaded_sim",
                "!help               show this list",
            ]
            self._set_status("  |  ".join(help_lines[:4]))
            print("\n[live_viewer] commands:")
            for ln in help_lines:
                print(f"  {ln}")

        else:
            self._set_status(
                f"unknown: '{text}'   type  !help  for command list"
            )

    def _do_new(self) -> None:
        self._set_status("!new — wiping outputs and restarting...")
        sh = Path(__file__).parent / "run_rotation.sh"
        try:
            subprocess.Popen(
                ["bash", str(sh), self.title, "!new"],
                start_new_session=True,
            )
            self._set_status(f"!new — relaunched  run_rotation.sh {self.title} !new")
        except FileNotFoundError:
            # fallback: just delete the watch dir manually
            if self.watch_dir.exists():
                shutil.rmtree(self.watch_dir)
            self.watch_dir.mkdir(parents=True, exist_ok=True)
            self._set_status("!new — watch dir cleared  (manual fallback; restart sim manually)")
        self._dirty.set()

    # ── status ────────────────────────────────────────────────────────────────

    def _set_status(self, msg: str) -> None:
        ts = datetime.now().strftime("%H:%M:%S")
        self._lbl_status.set_text(f"{ts}  |  {msg}")
        self.fig.canvas.draw_idle()

    # ── animation tick ────────────────────────────────────────────────────────

    def _dir_mtime(self) -> float:
        try:
            return max(
                (p.stat().st_mtime for p in self.watch_dir.rglob("*") if p.is_file()),
                default=0.0,
            )
        except OSError:
            return 0.0

    def _tick(self, _frame: int) -> None:
        # pause guard
        if self._paused:
            return

        # polling fallback when watchdog is absent
        if not _HAVE_WATCHDOG:
            mt = self._dir_mtime()
            if mt > self._last_mtime:
                self._last_mtime = mt
                self._dirty.set()

        if not self._dirty.is_set():
            return
        self._dirty.clear()
        self._repaint()

    # ── repaint ───────────────────────────────────────────────────────────────

    def _repaint(self) -> None:
        arts = scan_dir(self.watch_dir)
        elapsed = time.monotonic() - self._elapsed_start
        paused_tag = "  [PAUSED]" if self._paused else ""

        # ── atom scatter ──────────────────────────────────────────────────
        ax = self.ax_xyz
        ax.cla()
        ax.set_facecolor(BG_PANEL)
        ax.tick_params(colors=FG_DIM, labelsize=6)
        for sp in ax.spines.values():
            sp.set_edgecolor("#2a2a2a")

        n_atoms = 0
        xyz_path = arts.get("trajectory.xyz") or arts.get("frames.xyz")
        frame_label = ""
        if xyz_path:
            syms, xs, ys, _zs, total = parse_xyz_frame(xyz_path, self._frame_idx)
            self._frame_total = total
            n_atoms = len(syms)
            # if tracking live tail, keep _frame_idx at -1
            shown = (self._frame_idx + 1) if self._frame_idx >= 0 else total
            frame_label = f"frame {shown}/{total}" if total else ""
            if xs:
                sizes = atom_sizes(syms)
                ax.scatter(
                    xs, ys,
                    c=atom_colors(syms),
                    s=sizes,
                    alpha=0.88,
                    linewidths=0,
                    zorder=3,
                )
                pad_x = (max(xs) - min(xs)) * 0.08 + 1
                pad_y = (max(ys) - min(ys)) * 0.08 + 1
                ax.set_xlim(min(xs) - pad_x, max(xs) + pad_x)
                ax.set_ylim(min(ys) - pad_y, max(ys) + pad_y)
        if not n_atoms:
            ax.text(0.5, 0.5, "no trajectory yet",
                    ha="center", va="center",
                    transform=ax.transAxes,
                    color="#3a3a3a", fontsize=10)

        track_tag = "  (live tail)" if self._frame_idx == -1 and n_atoms else ""
        ax.set_title(
            f"particle positions  —  XY projection{track_tag}{paused_tag}",
            color=FG_MID, fontsize=7, pad=4,
        )

        # frame counter + elapsed in top-right corner
        mins, secs = divmod(int(elapsed), 60)
        self._lbl_frame.set_text(
            f"{frame_label}   {mins:02d}:{secs:02d}  elapsed"
        )

        # ── metrics panels ────────────────────────────────────────────────
        metrics: dict[str, list[float]] = {}
        if "metrics.tsv" in arts:
            metrics = parse_metrics(arts["metrics.tsv"])

        def _metric_panel(panel_ax, col: str, label: str, color: str) -> None:
            panel_ax.cla()
            panel_ax.set_facecolor(BG_PANEL)
            panel_ax.set_title(label, color=FG_MID, fontsize=7, pad=3)
            panel_ax.tick_params(colors=FG_DIM, labelsize=6)
            for sp in panel_ax.spines.values():
                sp.set_edgecolor("#2a2a2a")
            vals = metrics.get(col, [])
            if vals:
                # colour the line with a subtle glow: plot background shadow first
                if len(vals) > 1:
                    panel_ax.fill_between(
                        range(len(vals)), vals,
                        min(vals),
                        alpha=0.12, color=color, linewidth=0,
                    )
                panel_ax.plot(vals, color=color, linewidth=1.3)
                panel_ax.set_xlabel("step", color=FG_DIM, fontsize=6)
                # annotate last value
                panel_ax.annotate(
                    f"{vals[-1]:.4g}",
                    xy=(len(vals) - 1, vals[-1]),
                    xytext=(4, 0), textcoords="offset points",
                    color=color, fontsize=6, va="center",
                )
            else:
                panel_ax.text(0.5, 0.5, "no data",
                              ha="center", va="center",
                              transform=panel_ax.transAxes,
                              color="#3a3a3a", fontsize=7)

        _metric_panel(self.ax_ene,  "total_energy",  "total energy",     "#ff7040")
        _metric_panel(self.ax_temp, "temperature_K", "temperature  (K)", "#00b4ff")

        # ── analysis.json info panel ──────────────────────────────────────
        self.ax_info.cla()
        self.ax_info.axis("off")
        self.ax_info.set_facecolor(BG_PANEL)
        info: dict[str, str] = {}
        if "analysis.json" in arts:
            info = parse_analysis_json(arts["analysis.json"])
        if info:
            keys   = list(info.keys())
            vals_s = list(info.values())
            # two rows of up to 6 entries each
            cols   = min(len(keys), 6)
            for ci, (k, v) in enumerate(zip(keys[:cols], vals_s[:cols])):
                x = 0.01 + ci * (0.98 / cols)
                self.ax_info.text(
                    x, 0.72, k,
                    transform=self.ax_info.transAxes,
                    color="#557799", fontsize=6, va="top",
                )
                self.ax_info.text(
                    x, 0.28, v,
                    transform=self.ax_info.transAxes,
                    color=FG_BRIGHT, fontsize=6.5, va="top", fontweight="bold",
                )
            # second row if >6 entries
            for ci, (k, v) in enumerate(zip(keys[6:12], vals_s[6:12])):
                x = 0.01 + ci * (0.98 / min(len(keys) - 6, 6))
                self.ax_info.text(
                    x, 0.72 - 0.5, k,
                    transform=self.ax_info.transAxes,
                    color="#446688", fontsize=5.5, va="top",
                )
        elif arts.get("report.md"):
            self.ax_info.text(
                0.5, 0.5, "analysis.json not yet written  (report.md present)",
                transform=self.ax_info.transAxes,
                ha="center", va="center", color="#333355", fontsize=6,
            )
        else:
            self.ax_info.text(
                0.5, 0.5, "simulation output not yet available",
                transform=self.ax_info.transAxes,
                ha="center", va="center", color="#2a2a2a", fontsize=6,
            )

        # ── status line ───────────────────────────────────────────────────
        parts: list[str] = []
        if n_atoms:
            parts.append(f"{n_atoms} atoms")
        if arts:
            parts.append(f"{len(arts)} file(s)")
        if self._paused:
            parts.append("PAUSED")
        ts = datetime.now().strftime("%H:%M:%S")
        self._lbl_status.set_text(
            f"{ts}  |  {self.watch_dir.name}  |  "
            + ("  ".join(parts) if parts else "waiting for output...")
        )

        self.fig.canvas.draw_idle()

    # ── run ───────────────────────────────────────────────────────────────────

    def run(self) -> None:
        plt.show()
        if self._observer is not None:
            self._observer.stop()
            self._observer.join()


# ── entry point ───────────────────────────────────────────────────────────────

def main() -> None:
    ap = argparse.ArgumentParser(description="XSIM live 800x800 rotation viewer")
    ap.add_argument("--watch",  required=True,  help="Output directory to watch")
    ap.add_argument("--title",  default="scene", help="Scene name for window title")
    ap.add_argument("--live",   action="store_true",
                    help="Enable watchdog filesystem-event refresh")
    args = ap.parse_args()

    watch_dir = Path(args.watch).resolve()
    watch_dir.mkdir(parents=True, exist_ok=True)

    viewer = LiveViewer(watch_dir=watch_dir, title=args.title, live=args.live)
    viewer.run()


if __name__ == "__main__":
    main()
