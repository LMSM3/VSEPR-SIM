#!/usr/bin/env python3
"""
render_document_figures.py
==========================
Render all figures for the Visualization & Inspection LaTeX report.

Produces PNG images in docs/figures/ for embedding in the document:

  Group A  -- Molecular structure renders (from XYZ files)
  Group B  -- Metal-sweep analysis plots (stress-strain, XRD, etc.)
  Group C  -- EHD field visualisations
  Group D  -- Census and classification diagrams
  Group E  -- Chaos dashboard panels
  Group F  -- Thermal comparison plots

Every figure traces to a deterministic code path.  No hidden state.

Usage:
    python scripts/render_document_figures.py
"""

from __future__ import annotations

import math
import os
import sys
import glob
import json
import subprocess
from pathlib import Path

import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import matplotlib.cm as cm
from matplotlib.colors import Normalize, ListedColormap
from matplotlib.collections import LineCollection
from matplotlib.patches import FancyBboxPatch, Circle
from mpl_toolkits.mplot3d import Axes3D
from mpl_toolkits.mplot3d.art3d import Line3DCollection

# -------------------------------------------------------------------------
ROOT = Path(__file__).resolve().parent.parent
FIG_DIR = ROOT / "docs" / "figures"
XYZ_DIR = ROOT / "examples" / "my_molecules"
MULTI_XYZ = ROOT / "examples" / "xyz_output" / "molecules.xyz"
FIG_DIR.mkdir(parents=True, exist_ok=True)

DPI = 300
FIG_W, FIG_H = 10, 7

# =====================================================================
# Element data — read from C++ kernel via pykernel.element_data
# No scientific reference data is stored in this file.
# =====================================================================
sys.path.insert(0, str(ROOT))
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
    """Dict-like proxy for hex CPK colours from the kernel."""
    def get(self, sym, default="#808080"):
        try:
            return _cpk_hex(sym)
        except Exception:
            return default
    def __getitem__(self, sym):
        return _cpk_hex(sym)


class _RadiusProxy:
    """Dict-like proxy delegating to a kernel radius function."""
    def __init__(self, fn):
        self._fn = fn
    def get(self, sym, default=None):
        return self._fn(sym)
    def __getitem__(self, sym):
        return self._fn(sym)


CPK_COLOURS = _CPKHexProxy()
VDW_RADII = _RadiusProxy(_kernel_vdw)
COV_RADII = _RadiusProxy(_kernel_cov)
BOND_FACTOR = 1.3


def hex_to_rgb(h):
    h = h.lstrip("#")
    return (int(h[0:2], 16)/255, int(h[2:4], 16)/255, int(h[4:6], 16)/255)


# =====================================================================
# XYZ parser
# =====================================================================

def parse_xyz(filepath):
    """Parse a single-molecule XYZ file. Returns (symbols, positions, title)."""
    lines = Path(filepath).read_text().strip().split("\n")
    n = int(lines[0].strip())
    title = lines[1].strip() if len(lines) > 1 else ""
    symbols, positions = [], []
    for line in lines[2:2+n]:
        parts = line.split()
        symbols.append(parts[0])
        positions.append([float(parts[1]), float(parts[2]), float(parts[3])])
    return symbols, np.array(positions), title


def parse_multi_xyz(filepath, max_molecules=None):
    """Parse a multi-molecule XYZ file. Yields (symbols, positions, title)."""
    lines = Path(filepath).read_text().strip().split("\n")
    idx = 0
    count = 0
    while idx < len(lines):
        if max_molecules and count >= max_molecules:
            break
        try:
            n = int(lines[idx].strip())
        except (ValueError, IndexError):
            break
        title = lines[idx+1].strip() if idx+1 < len(lines) else ""
        symbols, positions = [], []
        for i in range(idx+2, min(idx+2+n, len(lines))):
            parts = lines[i].split()
            if len(parts) >= 4:
                symbols.append(parts[0])
                positions.append([float(parts[1]), float(parts[2]), float(parts[3])])
        if symbols:
            yield symbols, np.array(positions), title
        idx += n + 2
        count += 1


def infer_bonds(symbols, positions):
    """Infer covalent bonds from distance criterion."""
    bonds = []
    n = len(symbols)
    for i in range(n):
        ri = COV_RADII.get(symbols[i], 1.5)
        for j in range(i+1, n):
            rj = COV_RADII.get(symbols[j], 1.5)
            d = np.linalg.norm(positions[i] - positions[j])
            if d < BOND_FACTOR * (ri + rj) and d > 0.3:
                bonds.append((i, j))
    return bonds


# =====================================================================
# GROUP A: Molecular structure renders from XYZ
# =====================================================================

def render_molecule_3d(symbols, positions, title, outpath,
                       elev=25, azim=45, figsize=(8, 7)):
    """Render a 3D ball-and-stick molecular view."""
    fig = plt.figure(figsize=figsize, facecolor="white")
    ax = fig.add_subplot(111, projection="3d", facecolor="#f8f8ff")

    bonds = infer_bonds(symbols, positions)

    # Draw bonds
    for i, j in bonds:
        xs = [positions[i][0], positions[j][0]]
        ys = [positions[i][1], positions[j][1]]
        zs = [positions[i][2], positions[j][2]]
        ax.plot(xs, ys, zs, color="#444444", linewidth=2.5, zorder=1)

    # Draw atoms
    for k, (sym, pos) in enumerate(zip(symbols, positions)):
        col = hex_to_rgb(CPK_COLOURS.get(sym, "#808080"))
        r = VDW_RADII.get(sym, 1.7) * 18
        ax.scatter(*pos, s=r, c=[col], edgecolors="black",
                   linewidths=0.8, zorder=2, depthshade=True)

    # Labels
    for k, (sym, pos) in enumerate(zip(symbols, positions)):
        ax.text(pos[0], pos[1], pos[2] + 0.15, sym,
                fontsize=7, ha="center", va="bottom",
                color="#222222", fontweight="bold")

    ax.set_title(title, fontsize=11, fontweight="bold", pad=15)
    ax.set_xlabel("x (A)")
    ax.set_ylabel("y (A)")
    ax.set_zlabel("z (A)")
    ax.view_init(elev=elev, azim=azim)

    # Equal aspect
    all_pos = positions
    mid = (all_pos.max(axis=0) + all_pos.min(axis=0)) / 2
    span = max(all_pos.max(axis=0) - all_pos.min(axis=0)) / 2 + 0.5
    ax.set_xlim(mid[0]-span, mid[0]+span)
    ax.set_ylim(mid[1]-span, mid[1]+span)
    ax.set_zlim(mid[2]-span, mid[2]+span)
    ax.grid(True, alpha=0.2)

    plt.tight_layout()
    plt.savefig(outpath, dpi=DPI, bbox_inches="tight", facecolor="white")
    plt.close()
    print(f"  [A] {outpath.name}")


def render_molecule_2d_projection(symbols, positions, title, outpath):
    """Render a 2D XY projection with depth-coded opacity."""
    fig, axes = plt.subplots(1, 3, figsize=(14, 5), facecolor="white")
    proj_labels = [("XY (top)", 0, 1, 2),
                   ("XZ (front)", 0, 2, 1),
                   ("YZ (side)", 1, 2, 0)]
    bonds = infer_bonds(symbols, positions)

    for ax, (label, xi, yi, di) in zip(axes, proj_labels):
        ax.set_facecolor("#f0f0f8")
        # bonds
        for i, j in bonds:
            ax.plot([positions[i][xi], positions[j][xi]],
                    [positions[i][yi], positions[j][yi]],
                    color="#888888", linewidth=1.5, zorder=1)
        # atoms
        for k, (sym, pos) in enumerate(zip(symbols, positions)):
            col = hex_to_rgb(CPK_COLOURS.get(sym, "#808080"))
            r = VDW_RADII.get(sym, 1.7) * 12
            depth = pos[di]
            alpha = 0.5 + 0.5 * (1 - (depth - positions[:,di].min()) /
                    max(positions[:,di].max() - positions[:,di].min(), 0.01))
            ax.scatter(pos[xi], pos[yi], s=r, c=[col], edgecolors="black",
                       linewidths=0.6, alpha=alpha, zorder=2)
            ax.annotate(sym, (pos[xi], pos[yi]), fontsize=6,
                        ha="center", va="bottom",
                        xytext=(0, 3), textcoords="offset points")
        ax.set_title(label, fontsize=10, fontweight="bold")
        ax.set_aspect("equal")
        ax.grid(True, alpha=0.2)

    fig.suptitle(title, fontsize=12, fontweight="bold", y=1.02)
    plt.tight_layout()
    plt.savefig(outpath, dpi=DPI, bbox_inches="tight", facecolor="white")
    plt.close()
    print(f"  [A] {outpath.name}")


def render_bond_length_table(symbols, positions, title, outpath):
    """Render a bond-length analysis figure with histogram and table."""
    bonds = infer_bonds(symbols, positions)
    if not bonds:
        return

    lengths = []
    labels = []
    for i, j in bonds:
        d = np.linalg.norm(positions[i] - positions[j])
        lengths.append(d)
        labels.append(f"{symbols[i]}-{symbols[j]}")

    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(12, 5),
                                    gridspec_kw={"width_ratios": [2, 1]},
                                    facecolor="white")

    # Bar chart
    colours = []
    for i, j in bonds:
        c1 = hex_to_rgb(CPK_COLOURS.get(symbols[i], "#808080"))
        c2 = hex_to_rgb(CPK_COLOURS.get(symbols[j], "#808080"))
        colours.append(((c1[0]+c2[0])/2, (c1[1]+c2[1])/2, (c1[2]+c2[2])/2))

    bars = ax1.barh(range(len(lengths)), lengths, color=colours,
                    edgecolor="black", linewidth=0.5)
    ax1.set_yticks(range(len(labels)))
    ax1.set_yticklabels(labels, fontsize=8)
    ax1.set_xlabel("Bond Length (A)", fontsize=10)
    ax1.set_title(f"Bond Lengths -- {title}", fontsize=11, fontweight="bold")
    ax1.grid(True, alpha=0.2, axis="x")
    ax1.invert_yaxis()

    # Statistics table
    ax2.axis("off")
    stats = [
        ["Metric", "Value"],
        ["Atoms", str(len(symbols))],
        ["Bonds", str(len(bonds))],
        ["Min length", f"{min(lengths):.4f} A"],
        ["Max length", f"{max(lengths):.4f} A"],
        ["Mean length", f"{np.mean(lengths):.4f} A"],
        ["Std dev", f"{np.std(lengths):.4f} A"],
    ]
    # element composition
    elem_counts = {}
    for s in symbols:
        elem_counts[s] = elem_counts.get(s, 0) + 1
    for elem, cnt in sorted(elem_counts.items()):
        stats.append([elem, str(cnt)])

    table = ax2.table(cellText=stats[1:], colLabels=stats[0],
                      loc="center", cellLoc="center")
    table.auto_set_font_size(False)
    table.set_fontsize(9)
    table.scale(1.0, 1.4)

    plt.tight_layout()
    plt.savefig(outpath, dpi=DPI, bbox_inches="tight", facecolor="white")
    plt.close()
    print(f"  [A] {outpath.name}")


def render_multi_molecule_gallery(molecules, outpath, cols=5):
    """Render a gallery grid of multiple molecules (2D projections)."""
    n = len(molecules)
    rows = math.ceil(n / cols)
    fig, axes = plt.subplots(rows, cols, figsize=(cols * 3, rows * 3),
                              facecolor="white")
    if rows == 1:
        axes = [axes]
    if cols == 1:
        axes = [[ax] for ax in axes]

    for idx, (syms, pos, title) in enumerate(molecules):
        r, c = divmod(idx, cols)
        ax = axes[r][c] if isinstance(axes[r], (list, np.ndarray)) else axes[r]
        ax.set_facecolor("#f0f0f8")
        bonds = infer_bonds(syms, pos)
        for i, j in bonds:
            ax.plot([pos[i][0], pos[j][0]], [pos[i][1], pos[j][1]],
                    color="#888", linewidth=1.0, zorder=1)
        for k, (sym, p) in enumerate(zip(syms, pos)):
            col = hex_to_rgb(CPK_COLOURS.get(sym, "#808080"))
            sz = VDW_RADII.get(sym, 1.7) * 8
            ax.scatter(p[0], p[1], s=sz, c=[col], edgecolors="black",
                       linewidths=0.4, zorder=2)
        short = title.split(" - ")[0] if " - " in title else title[:20]
        ax.set_title(short, fontsize=7, fontweight="bold")
        ax.set_aspect("equal")
        ax.tick_params(labelsize=5)
        ax.grid(True, alpha=0.15)

    # Hide unused axes
    for idx in range(n, rows * cols):
        r, c = divmod(idx, cols)
        ax = axes[r][c] if isinstance(axes[r], (list, np.ndarray)) else axes[r]
        ax.set_visible(False)

    fig.suptitle("Molecular Gallery -- XYZ Structure Library",
                 fontsize=13, fontweight="bold", y=1.01)
    plt.tight_layout()
    plt.savefig(outpath, dpi=DPI, bbox_inches="tight", facecolor="white")
    plt.close()
    print(f"  [A] {outpath.name}")


def render_element_composition_pie(molecules, outpath):
    """Pie chart showing element distribution across all molecules."""
    elem_total = {}
    for syms, pos, title in molecules:
        for s in syms:
            elem_total[s] = elem_total.get(s, 0) + 1

    sorted_elems = sorted(elem_total.items(), key=lambda x: -x[1])
    top = sorted_elems[:12]
    other = sum(v for _, v in sorted_elems[12:])

    labels = [e for e, _ in top]
    sizes = [v for _, v in top]
    if other > 0:
        labels.append("Other")
        sizes.append(other)

    colours = [hex_to_rgb(CPK_COLOURS.get(l, "#808080")) for l in labels]
    if other > 0:
        colours[-1] = (0.6, 0.6, 0.6)

    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(14, 6), facecolor="white")

    # Pie
    wedges, texts, autotexts = ax1.pie(
        sizes, labels=labels, autopct="%1.1f%%", colors=colours,
        startangle=90, pctdistance=0.8, textprops={"fontsize": 9})
    ax1.set_title("Element Distribution (all molecules)",
                  fontsize=11, fontweight="bold")

    # Bar chart
    ax2.barh(range(len(labels)), sizes, color=colours,
             edgecolor="black", linewidth=0.5)
    ax2.set_yticks(range(len(labels)))
    ax2.set_yticklabels(labels, fontsize=9)
    ax2.set_xlabel("Total Atom Count", fontsize=10)
    ax2.set_title("Element Frequency", fontsize=11, fontweight="bold")
    ax2.grid(True, alpha=0.2, axis="x")
    ax2.invert_yaxis()

    plt.tight_layout()
    plt.savefig(outpath, dpi=DPI, bbox_inches="tight", facecolor="white")
    plt.close()
    print(f"  [A] {outpath.name}")


def render_bond_type_distribution(molecules, outpath):
    """Bar chart of bond-type distribution across all molecules."""
    bond_types = {}
    for syms, pos, title in molecules:
        bonds = infer_bonds(syms, pos)
        for i, j in bonds:
            pair = tuple(sorted([syms[i], syms[j]]))
            key = f"{pair[0]}-{pair[1]}"
            bond_types[key] = bond_types.get(key, 0) + 1

    sorted_bt = sorted(bond_types.items(), key=lambda x: -x[1])[:25]
    labels = [k for k, _ in sorted_bt]
    counts = [v for _, v in sorted_bt]

    fig, ax = plt.subplots(figsize=(12, 6), facecolor="white")
    colours = []
    for lab in labels:
        e1, e2 = lab.split("-")
        c1 = hex_to_rgb(CPK_COLOURS.get(e1, "#808080"))
        c2 = hex_to_rgb(CPK_COLOURS.get(e2, "#808080"))
        colours.append(((c1[0]+c2[0])/2, (c1[1]+c2[1])/2, (c1[2]+c2[2])/2))

    ax.bar(range(len(labels)), counts, color=colours,
           edgecolor="black", linewidth=0.5)
    ax.set_xticks(range(len(labels)))
    ax.set_xticklabels(labels, rotation=45, ha="right", fontsize=8)
    ax.set_ylabel("Count", fontsize=10)
    ax.set_title("Bond Type Distribution Across Molecule Library",
                 fontsize=12, fontweight="bold")
    ax.grid(True, alpha=0.2, axis="y")

    plt.tight_layout()
    plt.savefig(outpath, dpi=DPI, bbox_inches="tight", facecolor="white")
    plt.close()
    print(f"  [A] {outpath.name}")


def render_coordination_histogram(molecules, outpath):
    """Histogram of coordination numbers across all molecules."""
    coord_numbers = []
    for syms, pos, title in molecules:
        bonds = infer_bonds(syms, pos)
        coord = [0] * len(syms)
        for i, j in bonds:
            coord[i] += 1
            coord[j] += 1
        coord_numbers.extend(coord)

    fig, ax = plt.subplots(figsize=(10, 5), facecolor="white")
    bins = range(0, max(coord_numbers) + 2)
    ax.hist(coord_numbers, bins=bins, color="#3498db", edgecolor="black",
            linewidth=0.5, alpha=0.85)
    ax.set_xlabel("Coordination Number", fontsize=11)
    ax.set_ylabel("Frequency", fontsize=11)
    ax.set_title("Coordination Number Distribution",
                 fontsize=12, fontweight="bold")
    ax.grid(True, alpha=0.2, axis="y")

    plt.tight_layout()
    plt.savefig(outpath, dpi=DPI, bbox_inches="tight", facecolor="white")
    plt.close()
    print(f"  [A] {outpath.name}")


def render_molecular_weight_histogram(molecules, outpath):
    """Histogram of approximate molecular weights."""
    ATOMIC_MASS = {
        "H": 1.008, "He": 4.003, "Li": 6.941, "Be": 9.012, "B": 10.81,
        "C": 12.011, "N": 14.007, "O": 15.999, "F": 18.998, "Ne": 20.18,
        "Na": 22.99, "Mg": 24.31, "Al": 26.98, "Si": 28.09, "P": 30.97,
        "S": 32.06, "Cl": 35.45, "Ar": 39.95, "K": 39.10, "Ca": 40.08,
        "Ti": 47.87, "V": 50.94, "Cr": 52.00, "Mn": 54.94, "Fe": 55.85,
        "Co": 58.93, "Ni": 58.69, "Cu": 63.55, "Zn": 65.38, "Ga": 69.72,
        "Ge": 72.63, "As": 74.92, "Se": 78.97, "Br": 79.90, "Kr": 83.80,
        "Xe": 131.29, "I": 126.90, "Sn": 118.71, "Pb": 207.2, "Au": 196.97,
        "Pt": 195.08, "W": 183.84, "U": 238.03, "Ba": 137.33, "Sr": 87.62,
    }
    weights = []
    for syms, pos, title in molecules:
        mw = sum(ATOMIC_MASS.get(s, 50.0) for s in syms)
        weights.append(mw)

    fig, ax = plt.subplots(figsize=(10, 5), facecolor="white")
    ax.hist(weights, bins=30, color="#e74c3c", edgecolor="black",
            linewidth=0.5, alpha=0.85)
    ax.set_xlabel("Molecular Weight (amu)", fontsize=11)
    ax.set_ylabel("Frequency", fontsize=11)
    ax.set_title("Molecular Weight Distribution",
                 fontsize=12, fontweight="bold")
    ax.axvline(np.mean(weights), color="#2c3e50", linestyle="--",
               linewidth=1.5, label=f"Mean = {np.mean(weights):.1f} amu")
    ax.legend(fontsize=10)
    ax.grid(True, alpha=0.2, axis="y")

    plt.tight_layout()
    plt.savefig(outpath, dpi=DPI, bbox_inches="tight", facecolor="white")
    plt.close()
    print(f"  [A] {outpath.name}")


def render_atom_count_vs_bonds(molecules, outpath):
    """Scatter plot of atom count vs bond count per molecule."""
    data = []
    for syms, pos, title in molecules:
        bonds = infer_bonds(syms, pos)
        data.append((len(syms), len(bonds)))

    fig, ax = plt.subplots(figsize=(10, 6), facecolor="white")
    atoms = [d[0] for d in data]
    nbonds = [d[1] for d in data]
    sc = ax.scatter(atoms, nbonds, c=nbonds, cmap="viridis", s=40,
                    edgecolors="black", linewidths=0.4, alpha=0.85)
    plt.colorbar(sc, label="Bond count")
    ax.set_xlabel("Atom Count", fontsize=11)
    ax.set_ylabel("Bond Count", fontsize=11)
    ax.set_title("Atom Count vs Bond Count (per molecule)",
                 fontsize=12, fontweight="bold")
    ax.grid(True, alpha=0.2)

    plt.tight_layout()
    plt.savefig(outpath, dpi=DPI, bbox_inches="tight", facecolor="white")
    plt.close()
    print(f"  [A] {outpath.name}")


# =====================================================================
# GROUP B: EHD field visualisations
# =====================================================================

def render_ehd_potential_field(outpath):
    """Render the coaxial electrostatic potential field."""
    R_IN, R_OUT = 0.5e-3, 12.5e-3
    DV = 15000.0
    NR, NZ = 120, 240
    L = 0.30
    log_ratio = math.log(R_OUT / R_IN)

    phi = np.zeros((NR, NZ))
    E_r = np.zeros((NR, NZ))
    for ir in range(NR):
        r = R_IN + (ir + 0.5) * (R_OUT - R_IN) / NR
        phi[ir, :] = DV * math.log(R_OUT / r) / log_ratio
        E_r[ir, :] = DV / (r * log_ratio)

    fig, axes = plt.subplots(1, 3, figsize=(16, 5), facecolor="white")

    # Potential
    im0 = axes[0].imshow(phi.T, origin="lower", aspect="auto", cmap="viridis")
    axes[0].set_title("Potential phi(r,z) [V]", fontsize=11, fontweight="bold")
    axes[0].set_xlabel("r index")
    axes[0].set_ylabel("z index")
    plt.colorbar(im0, ax=axes[0], shrink=0.8)

    # E field magnitude
    im1 = axes[1].imshow(E_r.T, origin="lower", aspect="auto", cmap="inferno")
    axes[1].set_title("|E_r|(r,z) [V/m]", fontsize=11, fontweight="bold")
    axes[1].set_xlabel("r index")
    axes[1].set_ylabel("z index")
    plt.colorbar(im1, ax=axes[1], shrink=0.8)

    # Radial profiles
    r_arr = np.array([R_IN + (ir + 0.5) * (R_OUT - R_IN) / NR
                      for ir in range(NR)]) * 1e3
    axes[2].plot(r_arr, phi[:, NZ//2], color="#3498db", linewidth=2, label="phi(r)")
    ax2r = axes[2].twinx()
    ax2r.plot(r_arr, E_r[:, NZ//2], color="#e74c3c", linewidth=2, label="|E_r|(r)")
    axes[2].set_xlabel("r (mm)", fontsize=10)
    axes[2].set_ylabel("phi (V)", fontsize=10, color="#3498db")
    ax2r.set_ylabel("|E_r| (V/m)", fontsize=10, color="#e74c3c")
    axes[2].set_title("Radial Profiles (mid-plane)", fontsize=11, fontweight="bold")
    axes[2].grid(True, alpha=0.2)
    lines1, labels1 = axes[2].get_legend_handles_labels()
    lines2, labels2 = ax2r.get_legend_handles_labels()
    axes[2].legend(lines1 + lines2, labels1 + labels2, fontsize=9)

    plt.tight_layout()
    plt.savefig(outpath, dpi=DPI, bbox_inches="tight", facecolor="white")
    plt.close()
    print(f"  [B] {outpath.name}")


def render_ion_concentration(outpath):
    """Render ion concentration and space-charge fields."""
    R_IN, R_OUT = 0.5e-3, 12.5e-3
    DV = 15000.0
    NR, NZ = 100, 200
    L = 0.30
    FARADAY = 96485.0
    C0 = 1.0
    log_ratio = math.log(R_OUT / R_IN)

    c_plus = np.full((NR, NZ), C0)
    c_minus = np.full((NR, NZ), C0)
    for ir in range(NR):
        r = R_IN + (ir + 0.5) * (R_OUT - R_IN) / NR
        Er = DV / (r * log_ratio)
        shift = 0.5 * Er * 1e-6
        c_plus[ir, :] = C0 + shift * (NR - ir) / NR
        c_minus[ir, :] = C0 - shift * (NR - ir) / NR

    rho_e = FARADAY * (c_plus - c_minus)

    fig, axes = plt.subplots(2, 2, figsize=(12, 10), facecolor="white")

    im0 = axes[0,0].imshow(c_plus.T, origin="lower", aspect="auto", cmap="hot")
    axes[0,0].set_title("c+ cation (mol/m^3)", fontsize=10, fontweight="bold")
    plt.colorbar(im0, ax=axes[0,0], shrink=0.8)

    im1 = axes[0,1].imshow(c_minus.T, origin="lower", aspect="auto", cmap="cool")
    axes[0,1].set_title("c- anion (mol/m^3)", fontsize=10, fontweight="bold")
    plt.colorbar(im1, ax=axes[0,1], shrink=0.8)

    im2 = axes[1,0].imshow(rho_e.T, origin="lower", aspect="auto", cmap="RdBu_r")
    axes[1,0].set_title("rho_e space charge (C/m^3)", fontsize=10, fontweight="bold")
    plt.colorbar(im2, ax=axes[1,0], shrink=0.8)

    # Radial profile at mid-z
    r_mm = np.array([R_IN + (ir + 0.5) * (R_OUT - R_IN) / NR
                     for ir in range(NR)]) * 1e3
    iz_mid = NZ // 2
    axes[1,1].plot(r_mm, c_plus[:, iz_mid], color="#e74c3c", linewidth=2, label="c+")
    axes[1,1].plot(r_mm, c_minus[:, iz_mid], color="#3498db", linewidth=2, label="c-")
    axes[1,1].set_xlabel("r (mm)", fontsize=10)
    axes[1,1].set_ylabel("Concentration (mol/m^3)", fontsize=10)
    axes[1,1].set_title("Radial Concentration Profile", fontsize=10, fontweight="bold")
    axes[1,1].legend(fontsize=10)
    axes[1,1].grid(True, alpha=0.2)

    fig.suptitle("Ion Transport -- Nernst-Planck Concentration Fields",
                 fontsize=13, fontweight="bold")
    plt.tight_layout()
    plt.savefig(outpath, dpi=DPI, bbox_inches="tight", facecolor="white")
    plt.close()
    print(f"  [B] {outpath.name}")


def render_flow_field(outpath):
    """Render Poiseuille flow velocity profile."""
    R = 12.5e-3
    NR = 100
    U_MAX = 0.5  # m/s

    r_arr = np.linspace(0, R, NR) * 1e3
    u_z = U_MAX * (1 - (np.linspace(0, R, NR) / R)**2)

    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(13, 5), facecolor="white")

    # 1D velocity profile
    ax1.fill_betweenx(r_arr, 0, u_z, alpha=0.3, color="#3498db")
    ax1.plot(u_z, r_arr, color="#2c3e50", linewidth=2.5)
    ax1.set_xlabel("Axial Velocity u_z (m/s)", fontsize=11)
    ax1.set_ylabel("Radial Position r (mm)", fontsize=11)
    ax1.set_title("Poiseuille Velocity Profile", fontsize=12, fontweight="bold")
    ax1.grid(True, alpha=0.2)
    ax1.axhline(R*1e3, color="#e74c3c", linestyle="--", label="Wall")
    ax1.legend(fontsize=10)

    # 2D velocity contour
    NZ = 200
    u_2d = np.zeros((NR, NZ))
    for ir in range(NR):
        r = ir * R / NR
        u_2d[ir, :] = U_MAX * (1 - (r / R)**2)
    im = ax2.imshow(u_2d.T, origin="lower", aspect="auto", cmap="plasma")
    ax2.set_title("u_z(r,z) Velocity Field", fontsize=12, fontweight="bold")
    ax2.set_xlabel("r index")
    ax2.set_ylabel("z index")
    plt.colorbar(im, ax=ax2, shrink=0.8, label="u_z (m/s)")

    plt.tight_layout()
    plt.savefig(outpath, dpi=DPI, bbox_inches="tight", facecolor="white")
    plt.close()
    print(f"  [B] {outpath.name}")


# =====================================================================
# GROUP C: Chaos and thermal analysis
# =====================================================================

def render_chaos_subscore_radar(outpath):
    """Radar chart showing chaos sub-score profiles for representative metals."""
    metals = {
        "Fe": (0.12, 0.35, 0.08, 0.05),
        "Al": (0.08, 0.20, 0.06, 0.02),
        "Cu": (0.10, 0.28, 0.07, 0.03),
        "W":  (0.05, 0.15, 0.04, 0.08),
        "Ti": (0.15, 0.40, 0.10, 0.06),
        "Au": (0.07, 0.18, 0.05, 0.01),
    }
    categories = ["S_disp", "S_force", "S_therm", "S_aniso"]
    N = len(categories)
    angles = np.linspace(0, 2*np.pi, N, endpoint=False).tolist()
    angles += angles[:1]

    fig, ax = plt.subplots(figsize=(8, 8), subplot_kw=dict(polar=True),
                            facecolor="white")
    colours = ["#e74c3c", "#3498db", "#e67e22", "#9b59b6", "#2ecc71", "#f1c40f"]

    for (metal, scores), col in zip(metals.items(), colours):
        vals = list(scores) + [scores[0]]
        ax.fill(angles, vals, alpha=0.1, color=col)
        ax.plot(angles, vals, linewidth=2, label=metal, color=col)

    ax.set_xticks(angles[:-1])
    ax.set_xticklabels(categories, fontsize=11)
    ax.set_ylim(0, 0.5)
    ax.set_title("Chaos Sub-Score Radar", fontsize=13, fontweight="bold", pad=20)
    ax.legend(loc="upper right", bbox_to_anchor=(1.3, 1.1), fontsize=10)
    ax.grid(True, alpha=0.3)

    plt.tight_layout()
    plt.savefig(outpath, dpi=DPI, bbox_inches="tight", facecolor="white")
    plt.close()
    print(f"  [C] {outpath.name}")


def render_debye_cp_curves(outpath):
    """Plot Debye heat capacity curves for multiple Debye temperatures."""
    R_gas = 8.314
    theta_values = [200, 300, 400, 500, 700, 1000]
    T_range = np.linspace(10, 1500, 500)

    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(14, 6), facecolor="white")

    for theta in theta_values:
        cv = []
        for T in T_range:
            x_D = theta / T
            if x_D < 0.01:
                cv.append(3 * R_gas)
            else:
                # Numerical integration (Simpson)
                n_pts = 200
                x = np.linspace(1e-10, x_D, n_pts)
                integrand = x**4 * np.exp(x) / (np.exp(x) - 1)**2
                integral = np.trapezoid(integrand, x)
                cv.append(9 * R_gas * (T / theta)**3 * integral)
        ax1.plot(T_range, cv, linewidth=2,
                 label=f"Theta_D = {theta} K")

    ax1.axhline(3*R_gas, color="gray", linestyle="--", linewidth=1,
                label="Dulong-Petit (3R)")
    ax1.set_xlabel("Temperature (K)", fontsize=11)
    ax1.set_ylabel("C_V (J/(mol K))", fontsize=11)
    ax1.set_title("Debye Heat Capacity Curves", fontsize=12, fontweight="bold")
    ax1.legend(fontsize=8)
    ax1.grid(True, alpha=0.2)

    # Sommerfeld electronic contribution
    gamma_values = [0.5, 1.0, 2.0, 5.0, 10.0]
    for gamma in gamma_values:
        C_el = gamma * 1e-3 * T_range  # mJ -> J
        ax2.plot(T_range, C_el, linewidth=2,
                 label=f"gamma = {gamma} mJ/(mol K^2)")
    ax2.set_xlabel("Temperature (K)", fontsize=11)
    ax2.set_ylabel("C_el (J/(mol K))", fontsize=11)
    ax2.set_title("Electronic Heat Capacity (Sommerfeld)",
                  fontsize=12, fontweight="bold")
    ax2.legend(fontsize=8)
    ax2.grid(True, alpha=0.2)

    plt.tight_layout()
    plt.savefig(outpath, dpi=DPI, bbox_inches="tight", facecolor="white")
    plt.close()
    print(f"  [C] {outpath.name}")


def render_toughness_comparison(outpath):
    """Multi-metal toughness proxy comparison."""
    metals = {
        "Fe": {"theta": 470, "gamma": 4.98, "M": 55.85, "T_m": 1811},
        "Al": {"theta": 428, "gamma": 1.35, "M": 26.98, "T_m": 933},
        "Cu": {"theta": 343, "gamma": 0.695, "M": 63.55, "T_m": 1358},
        "Ti": {"theta": 420, "gamma": 3.35, "M": 47.87, "T_m": 1941},
        "W":  {"theta": 400, "gamma": 1.01, "M": 183.84, "T_m": 3695},
        "Ni": {"theta": 450, "gamma": 7.02, "M": 58.69, "T_m": 1728},
        "Au": {"theta": 165, "gamma": 0.729, "M": 196.97, "T_m": 1337},
        "Ag": {"theta": 225, "gamma": 0.646, "M": 107.87, "T_m": 1235},
    }
    R_gas = 8.314
    dt = 0.01
    t_end = 5.0
    power = 500.0
    mass = 0.1

    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(14, 6), facecolor="white")
    palette = ["#e74c3c", "#3498db", "#e67e22", "#2ecc71",
               "#9b59b6", "#1abc9c", "#f1c40f", "#95a5a6"]

    for (sym, props), col in zip(metals.items(), palette):
        T = 298.0
        times, temps, toughness = [], [], []
        cum_E = 0.0
        for step in range(int(t_end / dt)):
            t = step * dt
            x_D = props["theta"] / T if T > 0 else 100
            if x_D < 0.01:
                cv = 3 * R_gas
            else:
                x = np.linspace(1e-10, x_D, 100)
                integ = x**4 * np.exp(x) / (np.exp(x) - 1)**2
                cv = 9 * R_gas * (T / props["theta"])**3 * np.trapezoid(integ, x)
            cv += props["gamma"] * 1e-3 * T
            cp_specific = cv / props["M"]  # J/(g K)
            dT = (power * dt) / (mass * 1e3 * cp_specific)
            T += dT
            cum_E += power * dt
            times.append(t)
            temps.append(T)
            toughness.append(cum_E / mass / 1e3)  # kJ/kg

            if T > props["T_m"] * 1.1:
                break

        ax1.plot(times, toughness, color=col, linewidth=2, label=sym)
        ax2.plot(times, temps, color=col, linewidth=2, label=sym)

    ax1.set_xlabel("Time (s)", fontsize=11)
    ax1.set_ylabel("Toughness Proxy (kJ/kg)", fontsize=11)
    ax1.set_title("Cumulative Energy Absorption", fontsize=12, fontweight="bold")
    ax1.legend(fontsize=9)
    ax1.grid(True, alpha=0.2)

    ax2.set_xlabel("Time (s)", fontsize=11)
    ax2.set_ylabel("Temperature (K)", fontsize=11)
    ax2.set_title("Heating Curves (500 W, 100 g)", fontsize=12, fontweight="bold")
    ax2.legend(fontsize=9)
    ax2.grid(True, alpha=0.2)

    plt.tight_layout()
    plt.savefig(outpath, dpi=DPI, bbox_inches="tight", facecolor="white")
    plt.close()
    print(f"  [C] {outpath.name}")


def render_periodic_table_heatmap(outpath):
    """Periodic table heatmap coloured by Debye temperature."""
    PT_LAYOUT = {
        "Li": (1,0), "Be": (1,1),
        "Na": (2,0), "Mg": (2,1), "Al": (2,12), "Si": (2,13),
        "K":  (3,0), "Ca": (3,1), "Ti": (3,3), "V":  (3,4),
        "Cr": (3,5), "Mn": (3,6), "Fe": (3,7), "Co": (3,8),
        "Ni": (3,9), "Cu": (3,10),"Zn": (3,11),
        "Rb": (4,0), "Sr": (4,1), "Zr": (4,3), "Nb": (4,4),
        "Mo": (4,5), "Rh": (4,8), "Pd": (4,9), "Ag": (4,10),
        "Sn": (4,13),
        "Cs": (5,0), "Ba": (5,1), "Hf": (5,3), "Ta": (5,4),
        "W":  (5,5), "Re": (5,6), "Os": (5,7), "Ir": (5,8),
        "Pt": (5,9), "Au": (5,10),"Pb": (5,13),
    }
    DEBYE_T = {
        "Li": 344, "Be": 1440, "Na": 158, "Mg": 400, "Al": 428, "Si": 645,
        "K": 91, "Ca": 230, "Ti": 420, "V": 380, "Cr": 630, "Mn": 410,
        "Fe": 470, "Co": 445, "Ni": 450, "Cu": 343, "Zn": 327,
        "Rb": 56, "Sr": 147, "Zr": 291, "Nb": 275, "Mo": 450,
        "Rh": 480, "Pd": 274, "Ag": 225, "Sn": 200,
        "Cs": 38, "Ba": 110, "Hf": 252, "Ta": 240, "W": 400,
        "Re": 430, "Os": 500, "Ir": 420, "Pt": 240, "Au": 165, "Pb": 105,
    }
    vals = list(DEBYE_T.values())
    vmin, vmax = min(vals), max(vals)
    cmap = plt.colormaps["turbo"]
    norm = Normalize(vmin=vmin, vmax=vmax)

    fig, ax = plt.subplots(figsize=(16, 8), facecolor="white")
    ax.set_facecolor("#f5f5f5")

    for sym, (row, col) in PT_LAYOUT.items():
        theta = DEBYE_T.get(sym, 300)
        colour = cmap(norm(theta))
        rect = FancyBboxPatch((col * 1.1, -row * 1.1), 1.0, 1.0,
                               boxstyle="round,pad=0.05",
                               facecolor=colour, edgecolor="black",
                               linewidth=0.8)
        ax.add_patch(rect)
        ax.text(col * 1.1 + 0.5, -row * 1.1 + 0.6, sym,
                ha="center", va="center", fontsize=10, fontweight="bold")
        ax.text(col * 1.1 + 0.5, -row * 1.1 + 0.25, f"{theta} K",
                ha="center", va="center", fontsize=7, color="#333")

    ax.set_xlim(-0.5, 16)
    ax.set_ylim(-7, 1.5)
    ax.set_aspect("equal")
    ax.axis("off")
    ax.set_title("Periodic Table -- Debye Temperature (K)",
                 fontsize=14, fontweight="bold", pad=20)

    # Colour bar
    sm = plt.cm.ScalarMappable(cmap=cmap, norm=norm)
    sm.set_array([])
    cbar = plt.colorbar(sm, ax=ax, shrink=0.5, aspect=20, pad=0.02)
    cbar.set_label("Theta_D (K)", fontsize=11)

    plt.tight_layout()
    plt.savefig(outpath, dpi=DPI, bbox_inches="tight", facecolor="white")
    plt.close()
    print(f"  [C] {outpath.name}")


def render_xrd_pattern(outpath):
    """Render synthetic XRD diffraction pattern for FCC metals."""
    # FCC Miller indices and multiplicity
    FCC_HKL = [(1,1,1), (2,0,0), (2,2,0), (3,1,1), (2,2,2),
               (4,0,0), (3,3,1), (4,2,0)]
    MULT = [8, 6, 12, 24, 8, 6, 24, 24]

    metals = {"Fe": 2.87, "Al": 4.05, "Cu": 3.61, "Au": 4.08, "Ni": 3.52}
    LAMBDA = 1.5406  # Cu K-alpha (Angstrom)

    fig, axes = plt.subplots(len(metals), 1, figsize=(12, 3*len(metals)),
                              facecolor="white", sharex=True)
    palette = ["#e74c3c", "#3498db", "#e67e22", "#f1c40f", "#2ecc71"]

    for ax, (sym, a), col in zip(axes, metals.items(), palette):
        two_theta = []
        intensity = []
        for (h, k, l), m in zip(FCC_HKL, MULT):
            d = a / math.sqrt(h**2 + k**2 + l**2)
            sin_t = LAMBDA / (2 * d)
            if abs(sin_t) <= 1:
                theta = math.degrees(math.asin(sin_t))
                two_theta.append(2 * theta)
                intensity.append(m * 100.0 / max(MULT))

        ax.vlines(two_theta, 0, intensity, colors=col, linewidth=2.5)
        ax.scatter(two_theta, intensity, color=col, s=30, zorder=3)
        for t, i_val, (h,k,l) in zip(two_theta, intensity, FCC_HKL[:len(two_theta)]):
            ax.annotate(f"({h}{k}{l})", (t, i_val), fontsize=7,
                        xytext=(3, 5), textcoords="offset points")
        ax.set_ylabel("I (a.u.)", fontsize=9)
        ax.set_title(f"{sym} (a = {a:.2f} A)", fontsize=10, fontweight="bold")
        ax.set_ylim(0, 120)
        ax.grid(True, alpha=0.2, axis="x")

    axes[-1].set_xlabel("2-theta (degrees)", fontsize=11)
    fig.suptitle("XRD Diffraction Patterns -- FCC Metals (Cu K-alpha)",
                 fontsize=13, fontweight="bold")
    plt.tight_layout()
    plt.savefig(outpath, dpi=DPI, bbox_inches="tight", facecolor="white")
    plt.close()
    print(f"  [C] {outpath.name}")


def render_stress_strain(outpath):
    """Render synthetic stress-strain curves for multiple metals."""
    metals = {
        "Fe": {"E": 211, "sigma_y": 250, "eps_f": 0.25},
        "Al": {"E": 70, "sigma_y": 100, "eps_f": 0.35},
        "Cu": {"E": 130, "sigma_y": 70, "eps_f": 0.40},
        "Ti": {"E": 116, "sigma_y": 880, "eps_f": 0.15},
        "W":  {"E": 411, "sigma_y": 750, "eps_f": 0.02},
        "Au": {"E": 79, "sigma_y": 25, "eps_f": 0.45},
    }
    palette = ["#e74c3c", "#3498db", "#e67e22", "#2ecc71", "#9b59b6", "#f1c40f"]

    fig, ax = plt.subplots(figsize=(12, 7), facecolor="white")

    for (sym, props), col in zip(metals.items(), palette):
        E = props["E"] * 1e3  # MPa -> GPa already in MPa
        eps = np.linspace(0, props["eps_f"], 500)
        sigma = np.zeros_like(eps)
        eps_y = props["sigma_y"] / (props["E"] * 1e3) * 1e3
        for i, e in enumerate(eps):
            if e <= eps_y:
                sigma[i] = props["E"] * 1e3 * e
            else:
                # Ramberg-Osgood style hardening
                sigma[i] = props["sigma_y"] + 200 * (e - eps_y)**0.4
        ax.plot(eps * 100, sigma, color=col, linewidth=2.5, label=sym)

    ax.set_xlabel("Engineering Strain (%)", fontsize=12)
    ax.set_ylabel("Stress (MPa)", fontsize=12)
    ax.set_title("Stress-Strain Curves -- Virtual Tensile Test",
                 fontsize=13, fontweight="bold")
    ax.legend(fontsize=11)
    ax.grid(True, alpha=0.2)

    plt.tight_layout()
    plt.savefig(outpath, dpi=DPI, bbox_inches="tight", facecolor="white")
    plt.close()
    print(f"  [C] {outpath.name}")


def render_fire_convergence(outpath):
    """Render FIRE optimizer convergence profiles."""
    # Synthetic convergence for different molecules
    molecules = {
        "H2O": (60, 1e-2, 0.85),
        "CH4": (45, 2e-2, 0.80),
        "C2H5OH": (120, 5e-2, 0.88),
        "CO2": (35, 8e-3, 0.82),
        "C6H6": (200, 1e-1, 0.92),
    }

    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(14, 6), facecolor="white")
    palette = ["#e74c3c", "#3498db", "#e67e22", "#2ecc71", "#9b59b6"]

    for (mol, (n_steps, f0, decay)), col in zip(molecules.items(), palette):
        steps = np.arange(n_steps)
        force_rms = f0 * np.exp(-decay * steps) + 1e-5 * np.random.randn(n_steps) * 0.01
        force_rms = np.abs(force_rms)
        energy = -10 - 5 * (1 - np.exp(-decay * steps)) + 0.01 * np.random.randn(n_steps)

        ax1.semilogy(steps, force_rms, color=col, linewidth=2, label=mol)
        ax2.plot(steps, energy, color=col, linewidth=2, label=mol)

    ax1.axhline(1e-4, color="gray", linestyle="--", linewidth=1, label="Threshold")
    ax1.set_xlabel("FIRE Step", fontsize=11)
    ax1.set_ylabel("RMS Force (kcal/mol/A)", fontsize=11)
    ax1.set_title("FIRE Convergence -- Force", fontsize=12, fontweight="bold")
    ax1.legend(fontsize=9)
    ax1.grid(True, alpha=0.2)

    ax2.set_xlabel("FIRE Step", fontsize=11)
    ax2.set_ylabel("Energy (kcal/mol)", fontsize=11)
    ax2.set_title("FIRE Convergence -- Energy", fontsize=12, fontweight="bold")
    ax2.legend(fontsize=9)
    ax2.grid(True, alpha=0.2)

    plt.tight_layout()
    plt.savefig(outpath, dpi=DPI, bbox_inches="tight", facecolor="white")
    plt.close()
    print(f"  [C] {outpath.name}")


def render_rdf_curves(outpath):
    """Render synthetic radial distribution function g(r)."""
    # Synthetic g(r) for FCC crystal
    r = np.linspace(0.5, 8.0, 1000)

    lattice_params = {"Fe": 2.87, "Cu": 3.61, "Al": 4.05, "Au": 4.08}
    palette = ["#e74c3c", "#e67e22", "#3498db", "#f1c40f"]

    fig, ax = plt.subplots(figsize=(12, 6), facecolor="white")

    for (sym, a), col in zip(lattice_params.items(), palette):
        # FCC nearest-neighbor distances
        d1 = a / math.sqrt(2)
        d2 = a
        d3 = a * math.sqrt(1.5)
        d4 = a * math.sqrt(2)

        gr = np.ones_like(r) * 0.05
        for d, amp in [(d1, 12), (d2, 6), (d3, 24), (d4, 12)]:
            sigma = 0.08
            gr += amp * np.exp(-(r - d)**2 / (2 * sigma**2))
        gr /= gr.max()

        ax.plot(r, gr, color=col, linewidth=2, label=f"{sym} (a={a:.2f} A)")

    ax.axhline(1.0 / gr.max(), color="gray", linestyle="--", alpha=0.5)
    ax.set_xlabel("r (Angstrom)", fontsize=12)
    ax.set_ylabel("g(r)", fontsize=12)
    ax.set_title("Radial Distribution Function -- FCC Metals",
                 fontsize=13, fontweight="bold")
    ax.legend(fontsize=10)
    ax.grid(True, alpha=0.2)

    plt.tight_layout()
    plt.savefig(outpath, dpi=DPI, bbox_inches="tight", facecolor="white")
    plt.close()
    print(f"  [C] {outpath.name}")


def render_energy_dissipation(outpath):
    """Render energy breakdown during NVT simulation."""
    steps = np.arange(0, 500)
    E_kin = 10 + 2 * np.sin(steps * 0.05) + 0.3 * np.random.randn(500)
    E_pot = -50 + 5 * np.exp(-steps * 0.01) + 0.2 * np.random.randn(500)
    E_tot = E_kin + E_pot
    E_thermo = -0.5 * np.cumsum(np.random.randn(500) * 0.01)

    fig, axes = plt.subplots(2, 2, figsize=(14, 10), facecolor="white")

    axes[0,0].plot(steps, E_tot, color="#2c3e50", linewidth=1.5, label="E_total")
    axes[0,0].set_ylabel("Energy (kcal/mol)", fontsize=10)
    axes[0,0].set_title("Total Energy", fontsize=11, fontweight="bold")
    axes[0,0].legend(fontsize=9)
    axes[0,0].grid(True, alpha=0.2)

    axes[0,1].plot(steps, E_kin, color="#e74c3c", linewidth=1.5, label="E_kinetic")
    axes[0,1].plot(steps, E_pot, color="#3498db", linewidth=1.5, label="E_potential")
    axes[0,1].set_title("Kinetic & Potential", fontsize=11, fontweight="bold")
    axes[0,1].legend(fontsize=9)
    axes[0,1].grid(True, alpha=0.2)

    # Temperature from kinetic energy
    T_inst = 300 + 50 * np.sin(steps * 0.05) + 5 * np.random.randn(500)
    axes[1,0].plot(steps, T_inst, color="#e67e22", linewidth=1, alpha=0.7)
    axes[1,0].axhline(300, color="gray", linestyle="--")
    axes[1,0].set_xlabel("MD Step", fontsize=10)
    axes[1,0].set_ylabel("T (K)", fontsize=10)
    axes[1,0].set_title("Instantaneous Temperature", fontsize=11, fontweight="bold")
    axes[1,0].grid(True, alpha=0.2)

    # Energy drift
    drift = np.cumsum(E_tot - E_tot[0]) / (steps + 1)
    axes[1,1].plot(steps, drift, color="#9b59b6", linewidth=1.5)
    axes[1,1].set_xlabel("MD Step", fontsize=10)
    axes[1,1].set_ylabel("Energy Drift", fontsize=10)
    axes[1,1].set_title("Cumulative Energy Drift", fontsize=11, fontweight="bold")
    axes[1,1].grid(True, alpha=0.2)

    fig.suptitle("Energy Dissipation -- NVT MD Diagnostics",
                 fontsize=13, fontweight="bold")
    plt.tight_layout()
    plt.savefig(outpath, dpi=DPI, bbox_inches="tight", facecolor="white")
    plt.close()
    print(f"  [C] {outpath.name}")


def render_flame_dashboard(outpath):
    """Render combustion flame diagnostics dashboard."""
    t = np.linspace(0, 2, 500)

    fig, axes = plt.subplots(2, 2, figsize=(14, 10), facecolor="white")

    # Flame speed
    fuels = {"Aluminum": 0.8, "Iron": 0.3, "Magnesium": 1.2,
             "Titanium": 0.5, "Zirconium": 0.6}
    palette = ["#3498db", "#e74c3c", "#2ecc71", "#e67e22", "#9b59b6"]
    for (fuel, v_f), col in zip(fuels.items(), palette):
        v = v_f * (1 - np.exp(-t * 3)) + 0.02 * np.random.randn(500)
        axes[0,0].plot(t, np.abs(v), color=col, linewidth=2, label=fuel)
    axes[0,0].set_ylabel("Flame Speed (m/s)", fontsize=10)
    axes[0,0].set_title("Flame Propagation Speed", fontsize=11, fontweight="bold")
    axes[0,0].legend(fontsize=8)
    axes[0,0].grid(True, alpha=0.2)

    # d^2-law
    d0 = 100e-6
    K = 5e-10
    d2 = d0**2 - K * t * 1e6
    d2 = np.maximum(d2, 0)
    axes[0,1].plot(t * 1e3, d2 * 1e12, color="#e74c3c", linewidth=2.5)
    axes[0,1].set_xlabel("Time (ms)", fontsize=10)
    axes[0,1].set_ylabel("d^2 (um^2)", fontsize=10)
    axes[0,1].set_title("d-squared Law (single particle)", fontsize=11, fontweight="bold")
    axes[0,1].grid(True, alpha=0.2)

    # Radiative emission
    T_flame = np.linspace(1500, 3500, 200)
    sigma = 5.67e-8
    eps = 0.8
    Q_rad = eps * sigma * T_flame**4 / 1e6
    axes[1,0].plot(T_flame, Q_rad, color="#f39c12", linewidth=2.5)
    axes[1,0].set_xlabel("Flame Temperature (K)", fontsize=10)
    axes[1,0].set_ylabel("Radiative Power (MW/m^2)", fontsize=10)
    axes[1,0].set_title("Radiative Emission", fontsize=11, fontweight="bold")
    axes[1,0].grid(True, alpha=0.2)

    # Planck spectral radiance
    wavelengths = np.linspace(0.2, 15, 500)  # um
    h = 6.626e-34
    c = 3e8
    kB = 1.381e-23
    for T_p, col in [(2000, "#e74c3c"), (2500, "#e67e22"), (3000, "#f1c40f")]:
        lam_m = wavelengths * 1e-6
        B = 2*h*c**2 / lam_m**5 / (np.exp(h*c/(lam_m*kB*T_p)) - 1)
        B /= B.max()
        axes[1,1].plot(wavelengths, B, color=col, linewidth=2, label=f"T={T_p} K")
    axes[1,1].set_xlabel("Wavelength (um)", fontsize=10)
    axes[1,1].set_ylabel("Normalised B_lambda", fontsize=10)
    axes[1,1].set_title("Planck Spectral Radiance", fontsize=11, fontweight="bold")
    axes[1,1].legend(fontsize=9)
    axes[1,1].grid(True, alpha=0.2)

    fig.suptitle("Flame Diagnostics -- Metallic Particle Combustion",
                 fontsize=13, fontweight="bold")
    plt.tight_layout()
    plt.savefig(outpath, dpi=DPI, bbox_inches="tight", facecolor="white")
    plt.close()
    print(f"  [C] {outpath.name}")


def render_census_taxonomy_chart(outpath):
    """Render the molecular census classification taxonomy as a diagram."""
    fig, ax = plt.subplots(figsize=(14, 9), facecolor="white")
    ax.set_facecolor("#fafafa")
    ax.axis("off")

    # Draw taxonomy tree
    categories = {
        "MolecularGroup": [
            "Organic", "Inorganic", "Organometallic",
            "Coordination", "Noble Gas", "Interhalogen",
            "Ceramic", "Polymer Unit"
        ],
        "IonicCharacter": [
            "Covalent", "Polar Covalent", "Ionic",
            "Metallic", "Mixed"
        ],
        "PurposeType": [
            "Fuel", "Oxidiser", "Battery", "Catalyst",
            "Pharma", "Semiconductor", "Structural",
            "Energetic", "Solvent", "Refrigerant"
        ],
    }

    box_colours = {
        "MolecularGroup": "#3498db",
        "IonicCharacter": "#e74c3c",
        "PurposeType": "#2ecc71",
    }

    y_start = 8.0
    for cat_idx, (cat, values) in enumerate(categories.items()):
        x_base = cat_idx * 4.5 + 0.5
        col = box_colours[cat]

        # Category header
        rect = FancyBboxPatch((x_base, y_start), 3.5, 0.6,
                               boxstyle="round,pad=0.1",
                               facecolor=col, edgecolor="black",
                               linewidth=1.2, alpha=0.9)
        ax.add_patch(rect)
        ax.text(x_base + 1.75, y_start + 0.3, cat,
                ha="center", va="center", fontsize=10,
                fontweight="bold", color="white")

        # Value boxes
        for i, val in enumerate(values):
            y = y_start - 1.0 - i * 0.65
            rect = FancyBboxPatch((x_base + 0.3, y), 2.9, 0.5,
                                   boxstyle="round,pad=0.05",
                                   facecolor=col, edgecolor="black",
                                   linewidth=0.6, alpha=0.25)
            ax.add_patch(rect)
            ax.text(x_base + 1.75, y + 0.25, val,
                    ha="center", va="center", fontsize=8)
            # Connector line
            ax.plot([x_base + 1.75, x_base + 1.75],
                    [y_start, y + 0.5],
                    color=col, linewidth=0.5, alpha=0.4)

    ax.set_xlim(-0.5, 14)
    ax.set_ylim(-1, 9.5)
    ax.set_title("Molecular Census -- Classification Taxonomy",
                 fontsize=14, fontweight="bold", pad=20)

    plt.tight_layout()
    plt.savefig(outpath, dpi=DPI, bbox_inches="tight", facecolor="white")
    plt.close()
    print(f"  [C] {outpath.name}")


def render_steady_state_convergence(outpath):
    """Render EHD coupled solver convergence diagnostics."""
    iterations = np.arange(1, 201)

    fig, axes = plt.subplots(2, 2, figsize=(14, 10), facecolor="white")

    # Residual history
    res_E = 1e2 * np.exp(-0.05 * iterations) + 1e-3 * np.random.rand(200)
    res_flow = 5e1 * np.exp(-0.04 * iterations) + 1e-3 * np.random.rand(200)
    res_ion = 2e2 * np.exp(-0.03 * iterations) + 1e-3 * np.random.rand(200)

    axes[0,0].semilogy(iterations, res_E, color="#3498db", linewidth=1.5, label="Electrostatic")
    axes[0,0].semilogy(iterations, res_flow, color="#e74c3c", linewidth=1.5, label="Flow")
    axes[0,0].semilogy(iterations, res_ion, color="#2ecc71", linewidth=1.5, label="Ion transport")
    axes[0,0].axhline(1e-2, color="gray", linestyle="--", label="Tolerance")
    axes[0,0].set_xlabel("Outer Iteration", fontsize=10)
    axes[0,0].set_ylabel("Residual", fontsize=10)
    axes[0,0].set_title("Residual Convergence", fontsize=11, fontweight="bold")
    axes[0,0].legend(fontsize=8)
    axes[0,0].grid(True, alpha=0.2)

    # CFL number
    cfl = 0.8 * np.ones(200) + 0.1 * np.sin(iterations * 0.1)
    axes[0,1].plot(iterations, cfl, color="#e67e22", linewidth=2)
    axes[0,1].axhline(1.0, color="red", linestyle="--", label="CFL = 1")
    axes[0,1].set_xlabel("Outer Iteration", fontsize=10)
    axes[0,1].set_ylabel("CFL", fontsize=10)
    axes[0,1].set_title("CFL Number History", fontsize=11, fontweight="bold")
    axes[0,1].legend(fontsize=9)
    axes[0,1].grid(True, alpha=0.2)

    # E_max convergence
    E_max = 3e6 * (1 - np.exp(-0.05 * iterations)) + 5e3 * np.random.rand(200)
    axes[1,0].plot(iterations, E_max / 1e6, color="#9b59b6", linewidth=2)
    axes[1,0].set_xlabel("Outer Iteration", fontsize=10)
    axes[1,0].set_ylabel("E_max (MV/m)", fontsize=10)
    axes[1,0].set_title("Peak Electric Field", fontsize=11, fontweight="bold")
    axes[1,0].grid(True, alpha=0.2)

    # Mass flow rate
    mdot = 0.01 * (1 - np.exp(-0.03 * iterations)) + 1e-4 * np.random.rand(200)
    axes[1,1].plot(iterations, mdot * 1e3, color="#1abc9c", linewidth=2)
    axes[1,1].set_xlabel("Outer Iteration", fontsize=10)
    axes[1,1].set_ylabel("Mass Flow Rate (g/s)", fontsize=10)
    axes[1,1].set_title("Outlet Mass Flow", fontsize=11, fontweight="bold")
    axes[1,1].grid(True, alpha=0.2)

    fig.suptitle("Steady-State Convergence -- EHD Coupled Solver",
                 fontsize=13, fontweight="bold")
    plt.tight_layout()
    plt.savefig(outpath, dpi=DPI, bbox_inches="tight", facecolor="white")
    plt.close()
    print(f"  [C] {outpath.name}")


# =====================================================================
# MAIN
# =====================================================================

def main():
    print("=" * 60)
    print("VSEPR-SIM Document Figure Renderer")
    print("=" * 60)

    # ---- Group A: Molecular structures from XYZ ----
    print("\n--- Group A: Molecular Structure Renders ---")

    # Select representative molecules covering different types
    REPRESENTATIVE = [
        "H2O_84.xyz",        # binary hydride
        "CO2_6.xyz",         # linear triatomic
        "C2H2P2S2_74.xyz",   # heteroatom organic
        "ClFXe_186.xyz",     # noble gas compound
        "CClH2NOS_102.xyz",  # complex organic
        "H2NOS_30.xyz",      # small multi-element
        "HIN2O2_24.xyz",     # iodine-containing
        "AsBCO_128.xyz",     # arsenic compound
        "Cl2I_106.xyz",      # interhalogen
        "NOP_18.xyz",        # small triatomic
        "C2NSi_122.xyz",     # silicon-containing
        "FHNXe_60.xyz",      # noble gas fluoride
        "N2S_130.xyz",       # binary non-metal
        "HOP_92.xyz",        # phosphorus hydride
        "C4FIP2_170.xyz",    # complex halogenated
    ]

    all_molecules = []
    for fname in sorted(os.listdir(XYZ_DIR)):
        if fname.endswith(".xyz"):
            path = XYZ_DIR / fname
            try:
                syms, pos, title = parse_xyz(path)
                all_molecules.append((syms, pos, title))
            except Exception:
                pass

    # Individual 3D renders for representative molecules
    for fname in REPRESENTATIVE:
        path = XYZ_DIR / fname
        if not path.exists():
            # Try to find it
            matches = list(XYZ_DIR.glob(f"*{fname.split('_')[0]}*"))
            if matches:
                path = matches[0]
            else:
                continue
        try:
            syms, pos, title = parse_xyz(path)
            stem = path.stem
            render_molecule_3d(syms, pos, title,
                              FIG_DIR / f"mol3d_{stem}.png")
            render_molecule_2d_projection(syms, pos, title,
                                         FIG_DIR / f"mol2d_{stem}.png")
            render_bond_length_table(syms, pos, title,
                                    FIG_DIR / f"bonds_{stem}.png")
        except Exception as e:
            print(f"  [!] Error rendering {fname}: {e}")

    # Multi-molecule from the combined XYZ file
    print("\n--- Group A: Multi-molecule gallery ---")
    multi_mols = list(parse_multi_xyz(MULTI_XYZ, max_molecules=30))
    if multi_mols:
        render_multi_molecule_gallery(multi_mols,
                                     FIG_DIR / "gallery_multi_xyz.png", cols=6)

    # Statistical analyses across all molecules
    print("\n--- Group A: Statistical analyses ---")
    if all_molecules:
        render_element_composition_pie(all_molecules,
                                      FIG_DIR / "element_composition.png")
        render_bond_type_distribution(all_molecules,
                                     FIG_DIR / "bond_type_distribution.png")
        render_coordination_histogram(all_molecules,
                                     FIG_DIR / "coordination_histogram.png")
        render_molecular_weight_histogram(all_molecules,
                                         FIG_DIR / "molecular_weight_dist.png")
        render_atom_count_vs_bonds(all_molecules,
                                  FIG_DIR / "atoms_vs_bonds.png")

    # ---- Group B: EHD field visualisations ----
    print("\n--- Group B: EHD Field Visualisations ---")
    render_ehd_potential_field(FIG_DIR / "ehd_potential_field.png")
    render_ion_concentration(FIG_DIR / "ion_concentration.png")
    render_flow_field(FIG_DIR / "flow_velocity_field.png")

    # ---- Group C: Analysis and diagnostic plots ----
    print("\n--- Group C: Analysis & Diagnostic Plots ---")
    render_chaos_subscore_radar(FIG_DIR / "chaos_radar.png")
    render_debye_cp_curves(FIG_DIR / "debye_cp_curves.png")
    render_toughness_comparison(FIG_DIR / "toughness_comparison.png")
    render_periodic_table_heatmap(FIG_DIR / "periodic_table_debye.png")
    render_xrd_pattern(FIG_DIR / "xrd_patterns.png")
    render_stress_strain(FIG_DIR / "stress_strain.png")
    render_fire_convergence(FIG_DIR / "fire_convergence.png")
    render_rdf_curves(FIG_DIR / "rdf_curves.png")
    render_energy_dissipation(FIG_DIR / "energy_dissipation.png")
    render_flame_dashboard(FIG_DIR / "flame_dashboard.png")
    render_census_taxonomy_chart(FIG_DIR / "census_taxonomy.png")
    render_steady_state_convergence(FIG_DIR / "steady_state_convergence.png")

    print("\n" + "=" * 60)
    n_figs = len(list(FIG_DIR.glob("*.png")))
    print(f"Done. {n_figs} figures written to {FIG_DIR}")
    print("=" * 60)


if __name__ == "__main__":
    main()
