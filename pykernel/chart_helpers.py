"""
chart_helpers — Reusable chart rendering and data-format library for VSEPR-SIM.

Provides publication-quality chart functions and data I/O that can be
imported by any script that needs to produce figures or structured data.

Layers:
  1. Data I/O          — load/save CSV, JSON, manifest files (C++ ↔ Python)
  2. Style Presets      — project colour palette, font config, DPI defaults
  3. Chart Primitives   — line, scatter, bar, heatmap, radar, box, histogram
  4. Molecular Renders  — 3D ball-and-stick, 2D topology, bond diagrams
  5. LaTeX Helpers      — figure, subfigure, table snippet generators
  6. Manifest Consumer  — read a C++ FigureManifest and render all charts

Every function is deterministic: same data → identical PNG output.
No hidden state.

Usage:
    from pykernel.chart_helpers import (
        load_csv, load_manifest, save_figure,
        chart_line, chart_bar, chart_scatter, chart_heatmap,
        render_molecule_3d, render_molecule_2d,
        latex_figure, latex_table,
        PALETTE, configure_style,
    )

VSEPR-SIM 3.0.0
"""

from __future__ import annotations

import csv
import json
import math
import os
import sys
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any, Optional, Sequence

import numpy as np

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import matplotlib.cm as cm
import matplotlib.patheffects as pe
from matplotlib.colors import Normalize, ListedColormap, to_rgba
from matplotlib.collections import LineCollection
from matplotlib.patches import FancyBboxPatch, Circle, FancyArrowPatch
from mpl_toolkits.mplot3d import Axes3D
from mpl_toolkits.mplot3d.art3d import Line3DCollection


# ═══════════════════════════════════════════════════════════════════════
# 1. CONSTANTS AND PALETTES
# ═══════════════════════════════════════════════════════════════════════

# Project default output
ROOT_DIR = Path(__file__).resolve().parent.parent
FIG_DIR  = ROOT_DIR / "docs" / "figures"
DEFAULT_DPI = 300

# ── Project colour palette ──
PALETTE = {
    "blue":      "#2e86c1",
    "red":       "#e74c3c",
    "green":     "#27ae60",
    "purple":    "#8e44ad",
    "orange":    "#f39c12",
    "teal":      "#1abc9c",
    "navy":      "#1a5276",
    "pink":      "#e91e8e",
    "gray":      "#7f8c8d",
    "dark":      "#2c3e50",
    "light":     "#ecf0f1",
    "gold":      "#d4ac0d",
}

# Ordered list for multi-series charts
PALETTE_CYCLE = [
    "#e74c3c", "#2e86c1", "#27ae60", "#f39c12", "#8e44ad",
    "#1abc9c", "#e67e22", "#3498db", "#9b59b6", "#1a5276",
    "#d4ac0d", "#c0392b", "#16a085", "#2980b9", "#8e44ad",
]

# ── Element data — read from C++ kernel via pykernel.element_data ──
# CPK colours, VdW radii, and covalent radii are parsed from the kernel
# source files at import time.  No scientific reference data is stored here.
from pykernel.element_data import (
    cpk_color as _kernel_cpk,
    covalent_radius as _kernel_cov,
    vdw_radius as _kernel_vdw,
)


def _cpk_hex(sym: str) -> str:
    """Convert kernel CPK float-RGB to hex string."""
    r, g, b = _kernel_cpk(sym)
    return f"#{int(r*255):02X}{int(g*255):02X}{int(b*255):02X}"


class _CPKHexProxy:
    """Dict-like proxy that generates hex CPK colours from the kernel."""
    def get(self, sym, default="#808080"):
        try:
            return _cpk_hex(sym)
        except Exception:
            return default
    def __getitem__(self, sym):
        return _cpk_hex(sym)


class _RadiusProxy:
    """Dict-like proxy that delegates to a kernel radius function."""
    def __init__(self, fn, default):
        self._fn = fn
        self._default = default
    def get(self, sym, default=None):
        return self._fn(sym)
    def __getitem__(self, sym):
        return self._fn(sym)


CPK_COLOURS = _CPKHexProxy()
VDW_RADII = _RadiusProxy(_kernel_vdw, 2.0)
COV_RADII = _RadiusProxy(_kernel_cov, 0.8)

BOND_FACTOR = 1.3


# ═══════════════════════════════════════════════════════════════════════
# 2. STYLE CONFIGURATION
# ═══════════════════════════════════════════════════════════════════════

def configure_style(font_size: float = 11, font_family: str = "serif",
                    usetex: bool = False):
    """Apply project-wide matplotlib style defaults."""
    plt.rcParams.update({
        "figure.facecolor":   "white",
        "axes.facecolor":     "white",
        "axes.edgecolor":     "#2c3e50",
        "axes.labelcolor":    "#2c3e50",
        "axes.grid":          True,
        "grid.alpha":         0.3,
        "grid.linestyle":     "--",
        "font.size":          font_size,
        "font.family":        font_family,
        "text.usetex":        usetex,
        "figure.dpi":         DEFAULT_DPI,
        "savefig.dpi":        DEFAULT_DPI,
        "savefig.bbox":       "tight",
        "savefig.facecolor":  "white",
        "legend.framealpha":  0.85,
        "legend.edgecolor":   "#bdc3c7",
    })


# ═══════════════════════════════════════════════════════════════════════
# 3. DATA I/O
# ═══════════════════════════════════════════════════════════════════════

@dataclass
class TableData:
    """In-memory representation of a named CSV table."""
    name: str = ""
    columns: list[str] = field(default_factory=list)
    units: list[str] = field(default_factory=list)
    rows: list[list[str]] = field(default_factory=list)

    @property
    def num_cols(self) -> int:
        return len(self.columns)

    @property
    def num_rows(self) -> int:
        return len(self.rows)

    def column(self, name_or_idx) -> np.ndarray:
        """Extract a column as a float array by name or index."""
        if isinstance(name_or_idx, str):
            idx = self.columns.index(name_or_idx)
        else:
            idx = name_or_idx
        return np.array([float(row[idx]) for row in self.rows])

    def column_str(self, name_or_idx) -> list[str]:
        """Extract a column as a string list."""
        if isinstance(name_or_idx, str):
            idx = self.columns.index(name_or_idx)
        else:
            idx = name_or_idx
        return [row[idx] for row in self.rows]


def load_csv(path: str | Path, name: str = "") -> TableData:
    """Load a CSV file into a TableData. Skips lines starting with #."""
    path = Path(path)
    td = TableData(name=name or path.stem)
    with open(path, "r", newline="") as f:
        # Skip comment lines
        lines = [ln for ln in f if not ln.startswith("#")]
    reader = csv.reader(lines)
    td.columns = next(reader)
    # Strip whitespace from headers
    td.columns = [c.strip() for c in td.columns]
    for row in reader:
        if row:
            td.rows.append([c.strip() for c in row])
    return td


def save_csv(td: TableData, path: str | Path):
    """Write a TableData to CSV."""
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    with open(path, "w", newline="") as f:
        w = csv.writer(f)
        w.writerow(td.columns)
        w.writerows(td.rows)


def load_json(path: str | Path) -> dict:
    """Load a JSON file."""
    with open(path, "r") as f:
        return json.load(f)


def save_json(data: dict, path: str | Path, indent: int = 2):
    """Write a dict to JSON."""
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    with open(path, "w") as f:
        json.dump(data, f, indent=indent)


def load_manifest(path: str | Path) -> dict:
    """Load a FigureManifest JSON produced by C++ chart_data.hpp."""
    return load_json(path)


# ═══════════════════════════════════════════════════════════════════════
# 4. SAVE HELPER
# ═══════════════════════════════════════════════════════════════════════

def save_figure(fig: plt.Figure, name: str,
                outdir: str | Path | None = None,
                dpi: int = DEFAULT_DPI,
                close: bool = True) -> Path:
    """Save a figure to PNG and optionally close it."""
    d = Path(outdir) if outdir else FIG_DIR
    d.mkdir(parents=True, exist_ok=True)
    path = d / name
    fig.savefig(str(path), dpi=dpi, bbox_inches="tight", facecolor="white")
    if close:
        plt.close(fig)
    return path


# ═══════════════════════════════════════════════════════════════════════
# 5. CHART PRIMITIVES
# ═══════════════════════════════════════════════════════════════════════

def _apply_common(ax, title: str = "", xlabel: str = "", ylabel: str = "",
                  grid: bool = True, legend: bool = True,
                  x_scale: str = "", y_scale: str = ""):
    """Apply shared axis formatting."""
    if title:
        ax.set_title(title, fontsize=13, fontweight="bold", color="#2c3e50")
    if xlabel:
        ax.set_xlabel(xlabel, fontsize=11)
    if ylabel:
        ax.set_ylabel(ylabel, fontsize=11)
    if x_scale:
        ax.set_xscale(x_scale)
    if y_scale:
        ax.set_yscale(y_scale)
    if grid:
        ax.grid(True, alpha=0.3, linestyle="--")
    ax.spines["top"].set_visible(False)
    ax.spines["right"].set_visible(False)
    if legend and ax.get_legend_handles_labels()[1]:
        ax.legend(framealpha=0.85, edgecolor="#bdc3c7")


def chart_line(x_data: list[np.ndarray] | np.ndarray,
               y_data: list[np.ndarray] | np.ndarray,
               labels: list[str] | None = None,
               colors: list[str] | None = None,
               title: str = "", xlabel: str = "", ylabel: str = "",
               figsize: tuple = (10, 7),
               linewidths: list[float] | None = None,
               linestyles: list[str] | None = None,
               **kwargs) -> plt.Figure:
    """Multi-series line chart."""
    fig, ax = plt.subplots(figsize=figsize)

    # Normalize to lists
    if isinstance(x_data, np.ndarray) and x_data.ndim == 1:
        x_data = [x_data]
    if isinstance(y_data, np.ndarray) and y_data.ndim == 1:
        y_data = [y_data]

    n = len(y_data)
    if labels is None:
        labels = [None] * n
    if colors is None:
        colors = PALETTE_CYCLE[:n]
    if linewidths is None:
        linewidths = [2.0] * n
    if linestyles is None:
        linestyles = ["-"] * n

    for i in range(n):
        xi = x_data[i] if i < len(x_data) else x_data[0]
        ax.plot(xi, y_data[i], label=labels[i], color=colors[i],
                linewidth=linewidths[i], linestyle=linestyles[i])

    _apply_common(ax, title, xlabel, ylabel, **kwargs)
    return fig


def chart_scatter(x: np.ndarray, y: np.ndarray,
                  colors: np.ndarray | str = "#2e86c1",
                  sizes: np.ndarray | float = 30,
                  labels: list[str] | None = None,
                  title: str = "", xlabel: str = "", ylabel: str = "",
                  cmap: str = "viridis", colorbar_label: str = "",
                  figsize: tuple = (10, 7), **kwargs) -> plt.Figure:
    """Scatter plot with optional colour mapping."""
    fig, ax = plt.subplots(figsize=figsize)
    sc = ax.scatter(x, y, c=colors, s=sizes, cmap=cmap,
                    edgecolors="white", linewidths=0.5, alpha=0.85)
    if isinstance(colors, np.ndarray) and colors.dtype != object:
        cb = plt.colorbar(sc, ax=ax, shrink=0.8)
        if colorbar_label:
            cb.set_label(colorbar_label, fontsize=10)
    if labels is not None:
        for i, lbl in enumerate(labels):
            ax.annotate(lbl, (x[i], y[i]), fontsize=7, ha="center",
                        va="bottom", textcoords="offset points",
                        xytext=(0, 5))
    _apply_common(ax, title, xlabel, ylabel, **kwargs)
    return fig


def chart_bar(names: list[str], values: list[float],
              colors: list[str] | None = None,
              title: str = "", xlabel: str = "", ylabel: str = "",
              figsize: tuple = (10, 6),
              bar_labels: bool = True, **kwargs) -> plt.Figure:
    """Vertical bar chart."""
    fig, ax = plt.subplots(figsize=figsize)
    n = len(names)
    if colors is None:
        colors = [PALETTE_CYCLE[i % len(PALETTE_CYCLE)] for i in range(n)]
    bars = ax.bar(range(n), values, color=colors, edgecolor="white",
                  linewidth=2, alpha=0.85)
    ax.set_xticks(range(n))
    ax.set_xticklabels(names, fontsize=8, rotation=0)
    if bar_labels:
        for bar, val in zip(bars, values):
            ax.text(bar.get_x() + bar.get_width() / 2, bar.get_height(),
                    f"{val:.4g}", ha="center", va="bottom",
                    fontsize=9, fontweight="bold")
    _apply_common(ax, title, xlabel, ylabel, legend=False, **kwargs)
    return fig


def chart_barh(names: list[str], values: list[float],
               colors: list[str] | None = None,
               title: str = "", xlabel: str = "", ylabel: str = "",
               figsize: tuple = (10, 6), **kwargs) -> plt.Figure:
    """Horizontal bar chart."""
    fig, ax = plt.subplots(figsize=figsize)
    n = len(names)
    if colors is None:
        colors = [PALETTE_CYCLE[i % len(PALETTE_CYCLE)] for i in range(n)]
    bars = ax.barh(range(n), values, color=colors, edgecolor="white",
                   linewidth=2, alpha=0.85)
    ax.set_yticks(range(n))
    ax.set_yticklabels(names, fontsize=9)
    for bar, val in zip(bars, values):
        ax.text(bar.get_width(), bar.get_y() + bar.get_height() / 2,
                f" {val:.4g}", ha="left", va="center",
                fontsize=9, fontweight="bold")
    _apply_common(ax, title, xlabel, ylabel, legend=False, **kwargs)
    return fig


def chart_histogram(data: np.ndarray, bins: int = 30,
                    color: str = "#2e86c1",
                    title: str = "", xlabel: str = "", ylabel: str = "Count",
                    figsize: tuple = (10, 6), **kwargs) -> plt.Figure:
    """Histogram."""
    fig, ax = plt.subplots(figsize=figsize)
    ax.hist(data, bins=bins, color=color, edgecolor="white",
            linewidth=1, alpha=0.85)
    _apply_common(ax, title, xlabel, ylabel, legend=False, **kwargs)
    return fig


def chart_heatmap(matrix: np.ndarray,
                  x_labels: list[str] | None = None,
                  y_labels: list[str] | None = None,
                  title: str = "", cmap: str = "viridis",
                  colorbar_label: str = "", annotate: bool = False,
                  figsize: tuple = (10, 8), fmt: str = ".2f",
                  **kwargs) -> plt.Figure:
    """2-D heatmap."""
    fig, ax = plt.subplots(figsize=figsize)
    im = ax.imshow(matrix, cmap=cmap, aspect="auto", origin="lower")
    cb = plt.colorbar(im, ax=ax, shrink=0.8)
    if colorbar_label:
        cb.set_label(colorbar_label, fontsize=10)
    if x_labels:
        ax.set_xticks(range(len(x_labels)))
        ax.set_xticklabels(x_labels, fontsize=8, rotation=45, ha="right")
    if y_labels:
        ax.set_yticks(range(len(y_labels)))
        ax.set_yticklabels(y_labels, fontsize=8)
    if annotate:
        for i in range(matrix.shape[0]):
            for j in range(matrix.shape[1]):
                ax.text(j, i, f"{matrix[i, j]:{fmt}}", ha="center",
                        va="center", fontsize=7, color="white")
    _apply_common(ax, title, grid=False, legend=False, **kwargs)
    return fig


def chart_radar(categories: list[str], values: list[float],
                title: str = "", color: str = "#2e86c1",
                fill_alpha: float = 0.25,
                figsize: tuple = (7, 7)) -> plt.Figure:
    """Radar / spider chart."""
    n = len(categories)
    angles = np.linspace(0, 2 * np.pi, n, endpoint=False).tolist()
    vals = values + [values[0]]
    angles += [angles[0]]

    fig, ax = plt.subplots(figsize=figsize, subplot_kw=dict(polar=True))
    ax.plot(angles, vals, color=color, linewidth=2)
    ax.fill(angles, vals, color=color, alpha=fill_alpha)
    ax.set_xticks(angles[:-1])
    ax.set_xticklabels(categories, fontsize=9)
    if title:
        ax.set_title(title, fontsize=13, fontweight="bold", pad=20)
    return fig


def chart_box(data_groups: list[np.ndarray],
              group_names: list[str] | None = None,
              title: str = "", ylabel: str = "",
              figsize: tuple = (10, 6), **kwargs) -> plt.Figure:
    """Box-and-whisker chart."""
    fig, ax = plt.subplots(figsize=figsize)
    bp = ax.boxplot(data_groups, patch_artist=True, notch=False,
                    medianprops=dict(color="#2c3e50", linewidth=2))
    for i, patch in enumerate(bp["boxes"]):
        patch.set_facecolor(PALETTE_CYCLE[i % len(PALETTE_CYCLE)])
        patch.set_alpha(0.7)
    if group_names:
        ax.set_xticks(range(1, len(group_names) + 1))
        ax.set_xticklabels(group_names, fontsize=9)
    _apply_common(ax, title, ylabel=ylabel, legend=False, **kwargs)
    return fig


def chart_stacked_area(x: np.ndarray, y_stack: list[np.ndarray],
                       labels: list[str] | None = None,
                       colors: list[str] | None = None,
                       title: str = "", xlabel: str = "", ylabel: str = "",
                       figsize: tuple = (10, 6), **kwargs) -> plt.Figure:
    """Stacked area chart."""
    fig, ax = plt.subplots(figsize=figsize)
    n = len(y_stack)
    if colors is None:
        colors = PALETTE_CYCLE[:n]
    if labels is None:
        labels = [None] * n
    ax.stackplot(x, *y_stack, labels=labels, colors=colors, alpha=0.8)
    _apply_common(ax, title, xlabel, ylabel, **kwargs)
    return fig


def chart_pie(values: list[float], labels: list[str],
              colors: list[str] | None = None,
              title: str = "",
              figsize: tuple = (8, 8), **kwargs) -> plt.Figure:
    """Pie chart."""
    fig, ax = plt.subplots(figsize=figsize)
    n = len(values)
    if colors is None:
        colors = PALETTE_CYCLE[:n]
    ax.pie(values, labels=labels, colors=colors, autopct="%1.1f%%",
           startangle=90, wedgeprops=dict(edgecolor="white", linewidth=2))
    if title:
        ax.set_title(title, fontsize=13, fontweight="bold")
    return fig


# ═══════════════════════════════════════════════════════════════════════
# 6. ARCHITECTURE / DIAGRAM HELPERS
# ═══════════════════════════════════════════════════════════════════════

def draw_box(ax, x: float, y: float, w: float, h: float,
             label: str, color: str = "#2e86c1",
             detail: str = "", alpha: float = 0.85,
             fontsize_label: int = 11, fontsize_detail: int = 7):
    """Draw a rounded labeled box on an axes (for architecture diagrams)."""
    box = FancyBboxPatch((x, y), w, h, boxstyle="round,pad=0.1",
                         facecolor=color, edgecolor="white",
                         linewidth=2, alpha=alpha)
    ax.add_patch(box)
    ax.text(x + w / 2, y + h * 0.65, label, ha="center", va="center",
            fontsize=fontsize_label, fontweight="bold", color="white",
            path_effects=[pe.withStroke(linewidth=2, foreground="black")])
    if detail:
        ax.text(x + w / 2, y + h * 0.25, detail, ha="center", va="center",
                fontsize=fontsize_detail, color="white", alpha=0.9)


def draw_arrow(ax, x1: float, y1: float, x2: float, y2: float,
               color: str = "#2c3e50", lw: float = 2):
    """Draw an arrow between two points."""
    ax.annotate("", xy=(x2, y2), xytext=(x1, y1),
                arrowprops=dict(arrowstyle="-|>", color=color, lw=lw))


def create_diagram_axes(figsize: tuple = (14, 9),
                        xlim: tuple = (0, 14),
                        ylim: tuple = (0, 9),
                        title: str = ""):
    """Create a clean axes for architecture / flow diagrams."""
    fig, ax = plt.subplots(figsize=figsize)
    ax.set_xlim(*xlim)
    ax.set_ylim(*ylim)
    ax.set_aspect("equal")
    ax.axis("off")
    if title:
        fig.suptitle(title, fontsize=16, fontweight="bold", y=0.97)
    return fig, ax


# ═══════════════════════════════════════════════════════════════════════
# 7. MOLECULAR RENDERING
# ═══════════════════════════════════════════════════════════════════════

def hex_to_rgb(h: str) -> tuple[float, float, float]:
    h = h.lstrip("#")
    return (int(h[0:2], 16) / 255, int(h[2:4], 16) / 255,
            int(h[4:6], 16) / 255)


def parse_xyz(filepath: str | Path):
    """Parse a single-molecule XYZ file. Returns (symbols, positions, title)."""
    lines = Path(filepath).read_text().strip().split("\n")
    n = int(lines[0].strip())
    title = lines[1].strip() if len(lines) > 1 else ""
    symbols, positions = [], []
    for line in lines[2:2 + n]:
        parts = line.split()
        symbols.append(parts[0])
        positions.append([float(parts[1]), float(parts[2]), float(parts[3])])
    return symbols, np.array(positions), title


def parse_multi_xyz(filepath: str | Path, max_molecules: int | None = None):
    """Parse a multi-molecule XYZ file. Yields (symbols, positions, title)."""
    lines = Path(filepath).read_text().strip().split("\n")
    idx, count = 0, 0
    while idx < len(lines):
        if max_molecules and count >= max_molecules:
            break
        try:
            n = int(lines[idx].strip())
        except (ValueError, IndexError):
            break
        title = lines[idx + 1].strip() if idx + 1 < len(lines) else ""
        symbols, positions = [], []
        for i in range(idx + 2, min(idx + 2 + n, len(lines))):
            parts = lines[i].split()
            if len(parts) >= 4:
                symbols.append(parts[0])
                positions.append(
                    [float(parts[1]), float(parts[2]), float(parts[3])])
        if symbols:
            yield symbols, np.array(positions), title
        idx += n + 2
        count += 1


def infer_bonds(symbols: list[str], positions: np.ndarray) -> list[tuple]:
    """Infer covalent bonds from distance criterion."""
    bonds = []
    n = len(symbols)
    for i in range(n):
        ri = COV_RADII.get(symbols[i], 1.5)
        for j in range(i + 1, n):
            rj = COV_RADII.get(symbols[j], 1.5)
            d = np.linalg.norm(positions[i] - positions[j])
            if d < BOND_FACTOR * (ri + rj) and d > 0.3:
                bonds.append((i, j))
    return bonds


def render_molecule_3d(symbols: list[str], positions: np.ndarray,
                       title: str = "",
                       outpath: str | Path | None = None,
                       elev: float = 25, azim: float = 45,
                       figsize: tuple = (8, 7),
                       dpi: int = DEFAULT_DPI) -> plt.Figure:
    """Render a 3D ball-and-stick molecular view."""
    fig = plt.figure(figsize=figsize, facecolor="white")
    ax = fig.add_subplot(111, projection="3d", facecolor="#f8f8ff")

    bonds = infer_bonds(symbols, positions)

    # Bonds
    for i, j in bonds:
        xs = [positions[i][0], positions[j][0]]
        ys = [positions[i][1], positions[j][1]]
        zs = [positions[i][2], positions[j][2]]
        ax.plot(xs, ys, zs, color="#444444", linewidth=2.5, zorder=1)

    # Atoms
    for sym, pos in zip(symbols, positions):
        col = hex_to_rgb(CPK_COLOURS.get(sym, "#808080"))
        r = VDW_RADII.get(sym, 1.7) * 18
        ax.scatter(*pos, s=r, c=[col], edgecolors="black",
                   linewidths=0.8, zorder=2, depthshade=True)

    # Labels
    for sym, pos in zip(symbols, positions):
        ax.text(pos[0], pos[1], pos[2] + 0.15, sym, fontsize=7,
                ha="center", va="bottom", color="#222222", fontweight="bold")

    ax.set_title(title, fontsize=11, fontweight="bold", pad=15)
    ax.set_xlabel("x (Å)")
    ax.set_ylabel("y (Å)")
    ax.set_zlabel("z (Å)")
    ax.view_init(elev=elev, azim=azim)

    # Equal aspect
    mid = (positions.max(axis=0) + positions.min(axis=0)) / 2
    span = max(positions.max(axis=0) - positions.min(axis=0)) / 2 + 0.5
    ax.set_xlim(mid[0] - span, mid[0] + span)
    ax.set_ylim(mid[1] - span, mid[1] + span)
    ax.set_zlim(mid[2] - span, mid[2] + span)

    if outpath:
        fig.savefig(str(outpath), dpi=dpi, bbox_inches="tight",
                    facecolor="white")
    return fig


def render_molecule_2d(symbols: list[str], positions: np.ndarray,
                       title: str = "",
                       outpath: str | Path | None = None,
                       figsize: tuple = (8, 6),
                       dpi: int = DEFAULT_DPI) -> plt.Figure:
    """Render a 2D topology view (XY projection) with bonds."""
    fig, ax = plt.subplots(figsize=figsize, facecolor="white")

    bonds = infer_bonds(symbols, positions)
    pos2d = positions[:, :2]

    # Bonds
    for i, j in bonds:
        ax.plot([pos2d[i][0], pos2d[j][0]],
                [pos2d[i][1], pos2d[j][1]],
                color="#666666", linewidth=2.5, zorder=1)

    # Atoms
    for sym, pos in zip(symbols, pos2d):
        col = hex_to_rgb(CPK_COLOURS.get(sym, "#808080"))
        r = VDW_RADII.get(sym, 1.7) * 50
        ax.scatter(pos[0], pos[1], s=r, c=[col], edgecolors="black",
                   linewidths=1.0, zorder=2)
        ax.text(pos[0], pos[1] + 0.12, sym, fontsize=8,
                ha="center", va="bottom", fontweight="bold")

    ax.set_title(title, fontsize=11, fontweight="bold")
    ax.set_xlabel("x (Å)")
    ax.set_ylabel("y (Å)")
    ax.set_aspect("equal")
    ax.spines["top"].set_visible(False)
    ax.spines["right"].set_visible(False)
    ax.grid(True, alpha=0.2)

    if outpath:
        fig.savefig(str(outpath), dpi=dpi, bbox_inches="tight",
                    facecolor="white")
    return fig


def render_bond_diagram(symbols: list[str], positions: np.ndarray,
                        title: str = "",
                        outpath: str | Path | None = None,
                        figsize: tuple = (8, 6),
                        dpi: int = DEFAULT_DPI) -> plt.Figure:
    """Render a 2D bond-topology diagram (circles + lines, no scatter)."""
    fig, ax = plt.subplots(figsize=figsize, facecolor="white")

    bonds = infer_bonds(symbols, positions)
    pos2d = positions[:, :2]

    for i, j in bonds:
        ax.plot([pos2d[i][0], pos2d[j][0]],
                [pos2d[i][1], pos2d[j][1]],
                color="#2c3e50", linewidth=3, zorder=1)

    for idx, (sym, pos) in enumerate(zip(symbols, pos2d)):
        col = hex_to_rgb(CPK_COLOURS.get(sym, "#808080"))
        r = COV_RADII.get(sym, 0.8) * 0.4
        circ = Circle((pos[0], pos[1]), r, facecolor=col,
                       edgecolor="black", linewidth=1.5, zorder=2)
        ax.add_patch(circ)
        ax.text(pos[0], pos[1], sym, fontsize=9, ha="center",
                va="center", fontweight="bold", color="white", zorder=3)

    ax.set_title(title, fontsize=11, fontweight="bold")
    ax.set_aspect("equal")
    ax.autoscale()
    margin = 0.5
    xl, xr = ax.get_xlim()
    yl, yr = ax.get_ylim()
    ax.set_xlim(xl - margin, xr + margin)
    ax.set_ylim(yl - margin, yr + margin)
    ax.axis("off")

    if outpath:
        fig.savefig(str(outpath), dpi=dpi, bbox_inches="tight",
                    facecolor="white")
    return fig


# ═══════════════════════════════════════════════════════════════════════
# 8. LATEX SNIPPET GENERATORS
# ═══════════════════════════════════════════════════════════════════════

def _latex_escape(s: str) -> str:
    for old, new in [("_", r"\_"), ("%", r"\%"), ("&", r"\&"),
                     ("#", r"\#"), ("{", r"\{"), ("}", r"\}")]:
        s = s.replace(old, new)
    return s


def latex_figure(filename: str, caption: str, label: str,
                 width: float = 0.95) -> str:
    """Generate a LaTeX \\includegraphics figure block."""
    return (
        f"\\begin{{figure}}[H]\n"
        f"\\centering\n"
        f"\\includegraphics[width={width}\\textwidth]{{{filename}}}\n"
        f"\\caption{{{caption}}}\n"
        f"\\label{{{label}}}\n"
        f"\\end{{figure}}\n"
    )


def latex_subfigure_pair(fig1: str, cap1: str,
                         fig2: str, cap2: str,
                         overall_caption: str, label: str,
                         sub_width: float = 0.48) -> str:
    """Generate a side-by-side subfigure pair."""
    return (
        f"\\begin{{figure}}[H]\n\\centering\n"
        f"\\begin{{subfigure}}[b]{{{sub_width}\\textwidth}}\n"
        f"\\includegraphics[width=\\textwidth]{{{fig1}}}\n"
        f"\\caption{{{cap1}}}\n\\end{{subfigure}}\n\\hfill\n"
        f"\\begin{{subfigure}}[b]{{{sub_width}\\textwidth}}\n"
        f"\\includegraphics[width=\\textwidth]{{{fig2}}}\n"
        f"\\caption{{{cap2}}}\n\\end{{subfigure}}\n"
        f"\\caption{{{overall_caption}}}\n"
        f"\\label{{{label}}}\n"
        f"\\end{{figure}}\n"
    )


def latex_subfigure_grid(figs: list[str], caps: list[str],
                         overall_caption: str, label: str,
                         cols: int = 2,
                         sub_width: float = 0.48) -> str:
    """Generate an N-up subfigure grid."""
    lines = ["\\begin{figure}[H]\n\\centering"]
    for i, fig in enumerate(figs):
        lines.append(f"\\begin{{subfigure}}[b]{{{sub_width}\\textwidth}}")
        lines.append(f"\\includegraphics[width=\\textwidth]{{{fig}}}")
        if i < len(caps):
            lines.append(f"\\caption{{{caps[i]}}}")
        lines.append("\\end{subfigure}")
        if (i + 1) % cols == 0 and i + 1 < len(figs):
            lines.append("\\\\[0.5cm]")
        elif (i + 1) % cols != 0:
            lines.append("\\hfill")
    lines.append(f"\\caption{{{overall_caption}}}")
    lines.append(f"\\label{{{label}}}")
    lines.append("\\end{figure}")
    return "\n".join(lines) + "\n"


def latex_table(headers: list[str], rows: list[list[str]],
                caption: str = "", label: str = "",
                col_align: str | None = None) -> str:
    """Generate a LaTeX booktabs table."""
    if col_align is None:
        col_align = "l" + "r" * (len(headers) - 1)
    lines = ["\\begin{table}[H]", "\\centering"]
    if caption:
        lines.append(f"\\caption{{{caption}}}")
    if label:
        lines.append(f"\\label{{{label}}}")
    lines.append(f"\\begin{{tabular}}{{{col_align}}}")
    lines.append("\\toprule")
    lines.append(
        " & ".join(f"\\textbf{{{_latex_escape(h)}}}" for h in headers)
        + " \\\\"
    )
    lines.append("\\midrule")
    for row in rows:
        lines.append(" & ".join(_latex_escape(c) for c in row) + " \\\\")
    lines.append("\\bottomrule")
    lines.append("\\end{tabular}")
    lines.append("\\end{table}")
    return "\n".join(lines) + "\n"


# ═══════════════════════════════════════════════════════════════════════
# 9. MANIFEST CONSUMER — Render all charts from a C++ manifest
# ═══════════════════════════════════════════════════════════════════════

_CHART_DISPATCH = {
    "line":         "chart_line",
    "scatter":      "chart_scatter",
    "bar":          "chart_bar",
    "barh":         "chart_barh",
    "histogram":    "chart_histogram",
    "heatmap":      "chart_heatmap",
    "radar":        "chart_radar",
    "box":          "chart_box",
    "stacked_area": "chart_stacked_area",
    "pie":          "chart_pie",
}


def render_manifest(manifest_path: str | Path,
                    outdir: str | Path | None = None,
                    dpi: int = DEFAULT_DPI) -> list[Path]:
    """
    Read a C++ FigureManifest JSON and render every chart.

    The manifest JSON has:
      {
        "name": "...",
        "data_file": "..._data.csv",
        "charts": [ { ChartSpec }, ... ]
      }

    Returns list of saved figure paths.
    """
    manifest_path = Path(manifest_path)
    manifest = load_json(manifest_path)
    base_dir = manifest_path.parent

    # Load companion data CSV
    data_file = base_dir / manifest["data_file"]
    td = load_csv(data_file, manifest.get("name", ""))

    output_dir = Path(outdir) if outdir else base_dir / "figures"
    output_dir.mkdir(parents=True, exist_ok=True)

    saved = []
    for spec in manifest.get("charts", []):
        chart_type = spec.get("type", "line")
        chart_id = spec.get("id", "chart")
        title = spec.get("title", "")
        xlabel = spec.get("xlabel", "")
        ylabel = spec.get("ylabel", "")
        figw = spec.get("fig_width", 10)
        figh = spec.get("fig_height", 7)

        x_col = spec.get("x_col", 0)
        y_cols = spec.get("y_cols", [1])
        x_data = td.column(x_col)

        if chart_type in ("line", "scatter"):
            y_data = [td.column(c) for c in y_cols]
            labels = spec.get("series_labels",
                              [td.columns[c] for c in y_cols])
            colors = spec.get("series_colors", None) or None
            if chart_type == "line":
                fig = chart_line(
                    [x_data] * len(y_data), y_data,
                    labels=labels, colors=colors,
                    title=title, xlabel=xlabel, ylabel=ylabel,
                    figsize=(figw, figh),
                )
            else:
                fig = chart_scatter(
                    x_data, y_data[0],
                    title=title, xlabel=xlabel, ylabel=ylabel,
                    figsize=(figw, figh),
                )
        elif chart_type == "bar":
            names = td.column_str(x_col)
            values = td.column(y_cols[0]).tolist()
            fig = chart_bar(names, values, title=title, xlabel=xlabel,
                            ylabel=ylabel, figsize=(figw, figh))
        elif chart_type == "barh":
            names = td.column_str(x_col)
            values = td.column(y_cols[0]).tolist()
            fig = chart_barh(names, values, title=title, xlabel=xlabel,
                             ylabel=ylabel, figsize=(figw, figh))
        elif chart_type == "histogram":
            data = td.column(y_cols[0])
            fig = chart_histogram(data, title=title, xlabel=xlabel,
                                  ylabel=ylabel, figsize=(figw, figh))
        else:
            # Fallback: line chart
            y_data = [td.column(c) for c in y_cols]
            fig = chart_line([x_data] * len(y_data), y_data,
                             title=title, xlabel=xlabel, ylabel=ylabel,
                             figsize=(figw, figh))

        path = save_figure(fig, f"{chart_id}.png", outdir=output_dir,
                           dpi=spec.get("dpi", dpi))
        saved.append(path)
        print(f"  [OK] {chart_id}.png ({chart_type})")

    return saved


# ═══════════════════════════════════════════════════════════════════════
# 10. MODULE SELF-TEST
# ═══════════════════════════════════════════════════════════════════════

def _selftest():
    """Generate demo charts to verify all chart types work."""
    import tempfile
    outdir = Path(tempfile.mkdtemp(prefix="chart_helpers_"))
    print(f"Self-test output: {outdir}")

    configure_style()

    # Line
    x = np.linspace(0, 10, 100)
    fig = chart_line([x, x], [np.sin(x), np.cos(x)],
                     labels=["sin(x)", "cos(x)"],
                     title="Line Chart Demo", xlabel="x", ylabel="y")
    save_figure(fig, "demo_line.png", outdir)
    print("  [OK] demo_line.png")

    # Scatter
    fig = chart_scatter(np.random.randn(50), np.random.randn(50),
                        colors=np.random.randn(50),
                        title="Scatter Demo", xlabel="x", ylabel="y")
    save_figure(fig, "demo_scatter.png", outdir)
    print("  [OK] demo_scatter.png")

    # Bar
    fig = chart_bar(["Fe", "Al", "Cu", "Ti", "W"],
                    [210, 70, 120, 110, 400],
                    title="Young's Modulus", ylabel="E (GPa)")
    save_figure(fig, "demo_bar.png", outdir)
    print("  [OK] demo_bar.png")

    # Horizontal bar
    fig = chart_barh(["Fe", "Al", "Cu", "Ti", "W"],
                     [210, 70, 120, 110, 400],
                     title="Young's Modulus", xlabel="E (GPa)")
    save_figure(fig, "demo_barh.png", outdir)
    print("  [OK] demo_barh.png")

    # Histogram
    fig = chart_histogram(np.random.randn(1000),
                          title="Normal Distribution", xlabel="Value")
    save_figure(fig, "demo_histogram.png", outdir)
    print("  [OK] demo_histogram.png")

    # Heatmap
    fig = chart_heatmap(np.random.rand(8, 8), annotate=True,
                        title="Random Heatmap")
    save_figure(fig, "demo_heatmap.png", outdir)
    print("  [OK] demo_heatmap.png")

    # Radar
    fig = chart_radar(["Strength", "Ductility", "Hardness",
                        "Conductivity", "Corrosion"],
                       [0.9, 0.4, 0.7, 0.8, 0.5],
                       title="Material Radar")
    save_figure(fig, "demo_radar.png", outdir)
    print("  [OK] demo_radar.png")

    # Box
    fig = chart_box([np.random.randn(50), np.random.randn(50) + 1,
                     np.random.randn(50) - 0.5],
                    group_names=["A", "B", "C"],
                    title="Box Plot Demo", ylabel="Value")
    save_figure(fig, "demo_box.png", outdir)
    print("  [OK] demo_box.png")

    # Pie
    fig = chart_pie([45, 25, 20, 10],
                    ["Theory", "Code", "Tests", "Docs"],
                    title="Project Breakdown")
    save_figure(fig, "demo_pie.png", outdir)
    print("  [OK] demo_pie.png")

    # LaTeX snippets
    print("\n--- LaTeX figure snippet ---")
    print(latex_figure("demo_line.png", "Line demo", "fig:linedemo"))

    print("--- LaTeX table snippet ---")
    print(latex_table(
        ["Metal", "E (GPa)", "Density"],
        [["Fe", "210", "7.87"], ["Al", "70", "2.70"], ["Cu", "120", "8.96"]],
        caption="Material properties", label="tab:props"))

    print(f"\nAll demos saved to {outdir}")


if __name__ == "__main__":
    _selftest()
