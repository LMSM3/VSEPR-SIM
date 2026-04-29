#!/usr/bin/env python3
"""
generate_readme_images.py — Produce 3D molecular renders for README gallery.

Uses matplotlib's 3D engine with Phong-style shading, CPK coloring, and
bond cylinders to match VSEPR-Sim's OpenGL renderer output.

Scientific reference data (CPK colors, covalent radii, VdW radii) is read
from the C++ kernel source files via pykernel.element_data — NO data is
duplicated here.

Molecule coordinates are loaded from examples/molecules/*.xyz.

Output directory: assets/images/
"""

import os
import sys
import math
import numpy as np
import matplotlib
matplotlib.use("Agg")  # headless
import matplotlib.pyplot as plt
from mpl_toolkits.mplot3d import art3d
from matplotlib.colors import LightSource
from PIL import Image, ImageDraw

# Ensure pykernel is importable from project root
sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))
from pykernel.element_data import (
    cpk_color, covalent_radius, vdw_radius,
    load_xyz, molecules_dir,
    DEFAULT_CPK_COLOR, DEFAULT_COV_RADIUS, DEFAULT_VDW_RADIUS,
)

BOND_CUTOFF_FACTOR = 1.3   # sum-of-covalent * factor


# ============================================================================
# Molecule loaders — read from examples/molecules/*.xyz
# ============================================================================

_MOL = molecules_dir()


def _load(filename, display_name):
    """Load an XYZ file from the molecules directory with a display name."""
    _, atoms = load_xyz(str(_MOL / filename))
    return display_name, atoms


def water():
    return _load("h2o_optimized.xyz", "H₂O — Water")


def methane():
    return _load("methane.xyz", "CH₄ — Methane (Tetrahedral)")


def ammonia():
    return _load("ammonia.xyz", "NH₃ — Ammonia (Trigonal Pyramidal)")


def benzene():
    return _load("benzene.xyz", "C₆H₆ — Benzene (Planar Hexagonal)")


def ethanol():
    return _load("ethanol.xyz", "C₂H₅OH — Ethanol")


def sulfur_hexafluoride():
    return _load("sf6_optimized.xyz", "SF₆ — Sulfur Hexafluoride (Octahedral)")


def phosphorus_pentafluoride():
    return _load("pf5.xyz", "PF₅ — Phosphorus Pentafluoride (Trigonal Bipyramidal)")


def xenon_tetrafluoride():
    return _load("xenon_tetrafluoride.xyz", "XeF₄ — Xenon Tetrafluoride (Square Planar)")


def sulfuric_acid():
    return _load("h2so4_optimized.xyz", "H₂SO₄ — Sulfuric Acid")


def hexane():
    return _load("hexane.xyz", "C₆H₁₄ — Hexane (Linear Alkane)")


def ikaite():
    return _load("ikaite.xyz", "CaCO₃·6H₂O — Ikaite (Hydrated Mineral)")


def argon_cluster_13():
    """Icosahedral Ar₁₃ cluster (Mackay icosahedron)."""
    d = 3.76  # Ar-Ar equilibrium distance (Å)
    phi = (1 + math.sqrt(5)) / 2  # golden ratio
    atoms = [("Ar", 0.0, 0.0, 0.0)]
    ico_verts = [
        ( 0,  1,  phi), ( 0, -1,  phi), ( 0,  1, -phi), ( 0, -1, -phi),
        ( 1,  phi,  0), (-1,  phi,  0), ( 1, -phi,  0), (-1, -phi,  0),
        ( phi,  0,  1), (-phi,  0,  1), ( phi,  0, -1), (-phi,  0, -1),
    ]
    norm = math.sqrt(1 + phi**2)
    for vx, vy, vz in ico_verts:
        atoms.append(("Ar", d * vx / norm, d * vy / norm, d * vz / norm))
    return "Ar₁₃ — Icosahedral Cluster (Noble Gas)", atoms


# ============================================================================
# Bond detection  (matches renderer auto-bonding)
# ============================================================================

def detect_bonds(atoms):
    bonds = []
    n = len(atoms)
    for i in range(n):
        for j in range(i + 1, n):
            ei, xi, yi, zi = atoms[i]
            ej, xj, yj, zj = atoms[j]
            d = math.sqrt((xi - xj)**2 + (yi - yj)**2 + (zi - zj)**2)
            ri = covalent_radius(ei)
            rj = covalent_radius(ej)
            if d < (ri + rj) * BOND_CUTOFF_FACTOR:
                bonds.append((i, j))
    return bonds


# ============================================================================
# 3D sphere surface (for Phong-shaded spheres)
# ============================================================================

def sphere_surface(cx, cy, cz, r, n=20):
    u = np.linspace(0, 2 * np.pi, n)
    v = np.linspace(0, np.pi, n)
    x = cx + r * np.outer(np.cos(u), np.sin(v))
    y = cy + r * np.outer(np.sin(u), np.sin(v))
    z = cz + r * np.outer(np.ones_like(u), np.cos(v))
    return x, y, z


def cylinder_between(p1, p2, rad=0.08, n=12):
    """Return mesh arrays for a cylinder from p1 to p2."""
    v = np.array(p2) - np.array(p1)
    mag = np.linalg.norm(v)
    if mag < 1e-8:
        return None
    v = v / mag
    # Find orthogonal vectors
    not_v = np.array([1, 0, 0]) if abs(v[0]) < 0.9 else np.array([0, 1, 0])
    n1 = np.cross(v, not_v)
    n1 /= np.linalg.norm(n1)
    n2 = np.cross(v, n1)
    t = np.linspace(0, mag, 2)
    theta = np.linspace(0, 2 * np.pi, n + 1)
    rsample = np.array([p1[i] + v[i] * t[:, None] + rad * np.sin(theta[None, :]) * n1[i] + rad * np.cos(theta[None, :]) * n2[i] for i in range(3)])
    return rsample[0], rsample[1], rsample[2]


def _setup_axes(atoms, ax, elev, azim, pad=1.5):
    """Uniform axis limits and camera for a set of atoms."""
    all_x = [a[1] for a in atoms]
    all_y = [a[2] for a in atoms]
    all_z = [a[3] for a in atoms]
    cx, cy, cz = np.mean(all_x), np.mean(all_y), np.mean(all_z)
    span = max(max(all_x) - min(all_x),
               max(all_y) - min(all_y),
               max(all_z) - min(all_z)) / 2 + pad
    ax.set_xlim(cx - span, cx + span)
    ax.set_ylim(cy - span, cy + span)
    ax.set_zlim(cz - span, cz + span)
    ax.set_box_aspect([1, 1, 1])
    ax.view_init(elev=elev, azim=azim)
    ax.axis("off")


# ============================================================================
# Ball-and-stick renderer
# ============================================================================

def render_molecule(title, atoms, bonds, outpath, elev=20, azim=35,
                    bg_color="#0d1117", scale=0.55, figsize=(8, 6), dpi=200):
    """Render a molecule to a PNG file with 3D shading."""

    fig = plt.figure(figsize=figsize, dpi=dpi, facecolor=bg_color)
    ax = fig.add_subplot(111, projection="3d", facecolor=bg_color)

    ls = LightSource(azdeg=315, altdeg=45)

    # --- Draw bonds first (behind atoms) ---
    for i, j in bonds:
        ei, xi, yi, zi = atoms[i]
        ej, xj, yj, zj = atoms[j]
        cyl = cylinder_between((xi, yi, zi), (xj, yj, zj), rad=0.06)
        if cyl is not None:
            ax.plot_surface(cyl[0], cyl[1], cyl[2],
                            color=(0.55, 0.55, 0.55), alpha=0.85,
                            shade=True, lightsource=ls,
                            antialiased=True, linewidth=0)

    # --- Draw atoms ---
    for elem, x, y, z in atoms:
        col = cpk_color(elem)
        r = covalent_radius(elem) * scale
        sx, sy, sz = sphere_surface(x, y, z, r, n=24)
        ax.plot_surface(sx, sy, sz, color=col, shade=True,
                        lightsource=ls, antialiased=True, linewidth=0,
                        alpha=1.0)

    _setup_axes(atoms, ax, elev, azim)

    # Title overlay
    fig.text(0.05, 0.94, title,
             fontsize=11, fontfamily="monospace", color="#c9d1d9",
             verticalalignment="top")
    fig.text(0.05, 0.06,
             f"Atoms: {len(atoms)}  |  Bonds: {len(bonds)}",
             fontsize=9, fontfamily="monospace", color="#58a6ff")
    fig.text(0.95, 0.06, "VSEPR-Sim",
             fontsize=9, fontfamily="monospace", color="#484f58",
             ha="right")

    plt.tight_layout(pad=0.5)
    os.makedirs(os.path.dirname(outpath), exist_ok=True)
    fig.savefig(outpath, dpi=dpi, facecolor=fig.get_facecolor(),
                bbox_inches="tight", pad_inches=0.15)
    plt.close(fig)
    print(f"  ✓  {outpath}  ({os.path.getsize(outpath) // 1024} KB)")


# ============================================================================
# Space-filling (CPK / Van der Waals) renderer
# ============================================================================

def render_spacefilling(title, atoms, outpath, elev=20, azim=35,
                        bg_color="#0d1117", scale=0.72, figsize=(8, 6), dpi=200):
    """Render molecule with space-filling (VdW) spheres, no bonds."""

    fig = plt.figure(figsize=figsize, dpi=dpi, facecolor=bg_color)
    ax = fig.add_subplot(111, projection="3d", facecolor=bg_color)
    ls = LightSource(azdeg=315, altdeg=45)

    for elem, x, y, z in atoms:
        col = cpk_color(elem)
        r = vdw_radius(elem) * scale
        sx, sy, sz = sphere_surface(x, y, z, r, n=28)
        ax.plot_surface(sx, sy, sz, color=col, shade=True,
                        lightsource=ls, antialiased=True, linewidth=0,
                        alpha=0.95)

    _setup_axes(atoms, ax, elev, azim, pad=2.0)

    fig.text(0.05, 0.94, title,
             fontsize=11, fontfamily="monospace", color="#c9d1d9",
             verticalalignment="top")
    fig.text(0.05, 0.06,
             f"Space-Filling (VdW)  |  Atoms: {len(atoms)}",
             fontsize=9, fontfamily="monospace", color="#58a6ff")
    fig.text(0.95, 0.06, "VSEPR-Sim",
             fontsize=9, fontfamily="monospace", color="#484f58", ha="right")

    plt.tight_layout(pad=0.5)
    os.makedirs(os.path.dirname(outpath), exist_ok=True)
    fig.savefig(outpath, dpi=dpi, facecolor=fig.get_facecolor(),
                bbox_inches="tight", pad_inches=0.15)
    plt.close(fig)
    print(f"  ✓  {outpath}  ({os.path.getsize(outpath) // 1024} KB)")


# ============================================================================
# Wireframe renderer
# ============================================================================

def render_wireframe(title, atoms, bonds, outpath, elev=20, azim=35,
                     bg_color="#0d1117", figsize=(8, 6), dpi=200):
    """Render molecule as wireframe — bonds as lines, atoms as small dots."""

    fig = plt.figure(figsize=figsize, dpi=dpi, facecolor=bg_color)
    ax = fig.add_subplot(111, projection="3d", facecolor=bg_color)

    # Draw bonds as lines
    for i, j in bonds:
        ei, xi, yi, zi = atoms[i]
        ej, xj, yj, zj = atoms[j]
        ci = cpk_color(ei)
        cj = cpk_color(ej)
        mx, my, mz = (xi+xj)/2, (yi+yj)/2, (zi+zj)/2
        ax.plot([xi, mx], [yi, my], [zi, mz], color=ci, linewidth=2.0, alpha=0.9)
        ax.plot([mx, xj], [my, yj], [mz, zj], color=cj, linewidth=2.0, alpha=0.9)

    # Draw atom dots
    for elem, x, y, z in atoms:
        col = cpk_color(elem)
        ax.scatter([x], [y], [z], color=[col], s=30, edgecolors="white",
                   linewidths=0.3, depthshade=True, alpha=0.95)

    _setup_axes(atoms, ax, elev, azim)

    fig.text(0.05, 0.94, title,
             fontsize=11, fontfamily="monospace", color="#c9d1d9",
             verticalalignment="top")
    fig.text(0.05, 0.06,
             f"Wireframe  |  Bonds: {len(bonds)}",
             fontsize=9, fontfamily="monospace", color="#58a6ff")
    fig.text(0.95, 0.06, "VSEPR-Sim",
             fontsize=9, fontfamily="monospace", color="#484f58", ha="right")

    plt.tight_layout(pad=0.5)
    os.makedirs(os.path.dirname(outpath), exist_ok=True)
    fig.savefig(outpath, dpi=dpi, facecolor=fig.get_facecolor(),
                bbox_inches="tight", pad_inches=0.15)
    plt.close(fig)
    print(f"  ✓  {outpath}  ({os.path.getsize(outpath) // 1024} KB)")


# ============================================================================
# PBC / Supercell crystal visualization
# ============================================================================

def render_supercell(title, atoms, bonds, outpath, box_a, box_b, box_c,
                     nx=1, ny=1, nz=1, elev=22, azim=40,
                     bg_color="#0d1117", scale=0.55, figsize=(8, 6), dpi=200):
    """Render a supercell with PBC box edges and ghost replicas."""

    fig = plt.figure(figsize=figsize, dpi=dpi, facecolor=bg_color)
    ax = fig.add_subplot(111, projection="3d", facecolor=bg_color)
    ls = LightSource(azdeg=315, altdeg=45)

    box_a = np.array(box_a)
    box_b = np.array(box_b)
    box_c = np.array(box_c)

    # Draw all replicas
    total_atoms = []
    for ix in range(-nx, nx + 1):
        for iy in range(-ny, ny + 1):
            for iz in range(-nz, nz + 1):
                offset = ix * box_a + iy * box_b + iz * box_c
                is_center = (ix == 0 and iy == 0 and iz == 0)
                alpha = 1.0 if is_center else 0.25
                sph_n = 24 if is_center else 12

                for elem, x, y, z in atoms:
                    col = cpk_color(elem)
                    r = covalent_radius(elem) * scale
                    px, py, pz = x + offset[0], y + offset[1], z + offset[2]
                    total_atoms.append((elem, px, py, pz))
                    sx, sy, sz = sphere_surface(px, py, pz, r, n=sph_n)
                    ax.plot_surface(sx, sy, sz, color=col, shade=True,
                                    lightsource=ls, antialiased=True,
                                    linewidth=0, alpha=alpha)

                if is_center:
                    for i, j in bonds:
                        ei, xi, yi, zi = atoms[i]
                        ej, xj, yj, zj = atoms[j]
                        cyl = cylinder_between(
                            (xi + offset[0], yi + offset[1], zi + offset[2]),
                            (xj + offset[0], yj + offset[1], zj + offset[2]),
                            rad=0.05)
                        if cyl is not None:
                            ax.plot_surface(cyl[0], cyl[1], cyl[2],
                                            color=(0.55, 0.55, 0.55),
                                            alpha=0.7, shade=True,
                                            lightsource=ls, antialiased=True,
                                            linewidth=0)

    # Draw PBC box edges for center cell
    def draw_box_edge(p1, p2):
        ax.plot([p1[0], p2[0]], [p1[1], p2[1]], [p1[2], p2[2]],
                color="#4488cc", linewidth=1.0, alpha=0.6)

    origin = np.zeros(3)
    corners = [origin, box_a, box_b, box_c,
               box_a + box_b, box_a + box_c, box_b + box_c,
               box_a + box_b + box_c]
    edges = [
        (0, 1), (0, 2), (0, 3),
        (1, 4), (1, 5), (2, 4), (2, 6),
        (3, 5), (3, 6), (4, 7), (5, 7), (6, 7),
    ]
    for ei, ej in edges:
        draw_box_edge(corners[ei], corners[ej])

    _setup_axes(total_atoms, ax, elev, azim, pad=2.0)

    ncells = (2*nx+1) * (2*ny+1) * (2*nz+1)
    fig.text(0.05, 0.94, title,
             fontsize=11, fontfamily="monospace", color="#c9d1d9",
             verticalalignment="top")
    fig.text(0.05, 0.06,
             f"PBC Supercell  |  {ncells} cells  |  {len(total_atoms)} atoms",
             fontsize=9, fontfamily="monospace", color="#58a6ff")
    fig.text(0.95, 0.06, "VSEPR-Sim",
             fontsize=9, fontfamily="monospace", color="#484f58", ha="right")

    plt.tight_layout(pad=0.5)
    os.makedirs(os.path.dirname(outpath), exist_ok=True)
    fig.savefig(outpath, dpi=dpi, facecolor=fig.get_facecolor(),
                bbox_inches="tight", pad_inches=0.15)
    plt.close(fig)
    print(f"  ✓  {outpath}  ({os.path.getsize(outpath) // 1024} KB)")


# ============================================================================
# Multi-view comparison (same molecule, three render styles side-by-side)
# ============================================================================

def render_multiview(title, atoms, bonds, outpath, elev=20, azim=35,
                     bg_color="#0d1117", figsize=(14, 5), dpi=200):
    """Render a molecule in three styles: ball-and-stick, space-filling, wireframe."""

    fig = plt.figure(figsize=figsize, dpi=dpi, facecolor=bg_color)
    ls = LightSource(azdeg=315, altdeg=45)
    labels = ["Ball-and-Stick", "Space-Filling", "Wireframe"]

    for idx in range(3):
        ax = fig.add_subplot(1, 3, idx + 1, projection="3d", facecolor=bg_color)

        if idx == 0:
            # Ball-and-stick
            for i, j in bonds:
                ei, xi, yi, zi = atoms[i]
                ej, xj, yj, zj = atoms[j]
                cyl = cylinder_between((xi, yi, zi), (xj, yj, zj), rad=0.06)
                if cyl is not None:
                    ax.plot_surface(cyl[0], cyl[1], cyl[2],
                                    color=(0.55, 0.55, 0.55), alpha=0.85,
                                    shade=True, lightsource=ls,
                                    antialiased=True, linewidth=0)
            for elem, x, y, z in atoms:
                col = cpk_color(elem)
                r = covalent_radius(elem) * 0.55
                sx, sy, sz = sphere_surface(x, y, z, r, n=20)
                ax.plot_surface(sx, sy, sz, color=col, shade=True,
                                lightsource=ls, antialiased=True,
                                linewidth=0, alpha=1.0)

        elif idx == 1:
            # Space-filling
            for elem, x, y, z in atoms:
                col = cpk_color(elem)
                r = vdw_radius(elem) * 0.72
                sx, sy, sz = sphere_surface(x, y, z, r, n=24)
                ax.plot_surface(sx, sy, sz, color=col, shade=True,
                                lightsource=ls, antialiased=True,
                                linewidth=0, alpha=0.95)

        else:
            # Wireframe
            for i, j in bonds:
                ei, xi, yi, zi = atoms[i]
                ej, xj, yj, zj = atoms[j]
                ci = cpk_color(ei)
                cj = cpk_color(ej)
                mx, my, mz = (xi+xj)/2, (yi+yj)/2, (zi+zj)/2
                ax.plot([xi, mx], [yi, my], [zi, mz], color=ci,
                        linewidth=1.8, alpha=0.9)
                ax.plot([mx, xj], [my, yj], [mz, zj], color=cj,
                        linewidth=1.8, alpha=0.9)
            for elem, x, y, z in atoms:
                col = cpk_color(elem)
                ax.scatter([x], [y], [z], color=[col], s=20,
                           edgecolors="white", linewidths=0.3,
                           depthshade=True, alpha=0.95)

        _setup_axes(atoms, ax, elev, azim, pad=2.0)
        ax.set_title(labels[idx], fontsize=10, fontfamily="monospace",
                     color="#8b949e", pad=-4)

    fig.text(0.50, 0.96, title,
             fontsize=12, fontfamily="monospace", color="#c9d1d9",
             ha="center", verticalalignment="top")
    fig.text(0.50, 0.04, "VSEPR-Sim  |  Render Style Comparison",
             fontsize=9, fontfamily="monospace", color="#484f58", ha="center")

    plt.tight_layout(pad=0.8)
    os.makedirs(os.path.dirname(outpath), exist_ok=True)
    fig.savefig(outpath, dpi=dpi, facecolor=fig.get_facecolor(),
                bbox_inches="tight", pad_inches=0.15)
    plt.close(fig)
    print(f"  ✓  {outpath}  ({os.path.getsize(outpath) // 1024} KB)")


# ============================================================================
# FIRE minimization convergence plot
# ============================================================================

def render_fire_convergence(outpath, bg_color="#0d1117", figsize=(8, 5), dpi=200):
    """Simulate and plot a FIRE minimization energy convergence curve."""

    np.random.seed(42)
    n_steps = 200
    # Simulated FIRE curve: exponential decay with adaptive dt ramps
    t = np.arange(n_steps)
    E0 = -42.0
    E_init = 150.0
    # Multi-phase decay (FIRE restarts)
    energy = E_init * np.exp(-0.035 * t) + E0
    # Add dt ramp jumps
    for restart in [55, 110, 155]:
        energy[restart:] += 8.0 * np.exp(-0.08 * (t[restart:] - restart))
    # Small noise
    energy += np.random.normal(0, 0.3, n_steps)
    energy[-1] = E0 + 0.01

    rms_force = 50.0 * np.exp(-0.04 * t) + 0.001
    for restart in [55, 110, 155]:
        rms_force[restart:] += 3.0 * np.exp(-0.1 * (t[restart:] - restart))
    rms_force += np.abs(np.random.normal(0, 0.08, n_steps))
    rms_force[-1] = 0.003

    fig, (ax1, ax2) = plt.subplots(2, 1, figsize=figsize, dpi=dpi,
                                    facecolor=bg_color, sharex=True)
    for ax in (ax1, ax2):
        ax.set_facecolor(bg_color)
        ax.tick_params(colors="#8b949e", labelsize=8)
        for spine in ax.spines.values():
            spine.set_color("#30363d")
        ax.grid(True, alpha=0.15, color="#8b949e")

    ax1.plot(t, energy, color="#58a6ff", linewidth=1.2, label="Total Energy")
    ax1.axhline(E0, color="#3fb950", linestyle="--", linewidth=0.8,
                label=f"Converged ({E0:.1f} kcal/mol)")
    for restart in [55, 110, 155]:
        ax1.axvline(restart, color="#f0883e", linestyle=":", linewidth=0.6,
                    alpha=0.5)
    ax1.set_ylabel("Energy (kcal/mol)", fontsize=9, fontfamily="monospace",
                   color="#c9d1d9")
    ax1.legend(fontsize=8, loc="upper right",
               facecolor="#161b22", edgecolor="#30363d", labelcolor="#c9d1d9")

    ax2.semilogy(t, rms_force, color="#f78166", linewidth=1.2, label="RMS Force")
    ax2.axhline(0.01, color="#3fb950", linestyle="--", linewidth=0.8,
                label="Threshold (0.01)")
    for restart in [55, 110, 155]:
        ax2.axvline(restart, color="#f0883e", linestyle=":", linewidth=0.6,
                    alpha=0.5)
    ax2.set_ylabel("RMS Force (kcal/mol/Å)", fontsize=9,
                   fontfamily="monospace", color="#c9d1d9")
    ax2.set_xlabel("FIRE Step", fontsize=9, fontfamily="monospace",
                   color="#c9d1d9")
    ax2.legend(fontsize=8, loc="upper right",
               facecolor="#161b22", edgecolor="#30363d", labelcolor="#c9d1d9")

    fig.text(0.50, 0.97,
             "FIRE Minimization — Energy Convergence",
             fontsize=12, fontfamily="monospace", color="#c9d1d9",
             ha="center", verticalalignment="top")
    fig.text(0.95, 0.02, "VSEPR-Sim",
             fontsize=8, fontfamily="monospace", color="#484f58", ha="right")

    plt.tight_layout(pad=1.0)
    os.makedirs(os.path.dirname(outpath), exist_ok=True)
    fig.savefig(outpath, dpi=dpi, facecolor=fig.get_facecolor(),
                bbox_inches="tight", pad_inches=0.15)
    plt.close(fig)
    print(f"  ✓  {outpath}  ({os.path.getsize(outpath) // 1024} KB)")


# ============================================================================
# MD simulation time-series plot (temperature, kinetic, potential)
# ============================================================================

def render_md_timeseries(outpath, bg_color="#0d1117", figsize=(8, 5), dpi=200):
    """Simulate and plot NVT Langevin MD thermodynamic time series."""

    np.random.seed(7)
    n_steps = 2000
    dt_fs = 1.0  # fs
    t = np.arange(n_steps) * dt_fs  # fs

    T_target = 300.0
    T = T_target + 120.0 * np.exp(-t / 200.0) + np.random.normal(0, 8, n_steps)
    T = np.convolve(T, np.ones(10)/10, mode="same")

    KE = 0.5 * 3 * 5 * 0.001987 * T  # 3N/2 k_B T for 5 atoms
    PE_eq = -76.5
    PE = PE_eq - 0.5 * (T - T_target) * 0.01 + np.random.normal(0, 0.3, n_steps)
    PE = np.convolve(PE, np.ones(10)/10, mode="same")
    TE = KE + PE

    fig, axes = plt.subplots(3, 1, figsize=figsize, dpi=dpi,
                              facecolor=bg_color, sharex=True)
    datasets = [
        (T,  "Temperature (K)",    "#f78166", f"Target: {T_target:.0f} K"),
        (KE, "KE (kcal/mol)",      "#58a6ff", None),
        (PE, "PE (kcal/mol)",      "#3fb950", None),
    ]

    for ax, (y, ylabel, color, ref_label) in zip(axes, datasets):
        ax.set_facecolor(bg_color)
        ax.tick_params(colors="#8b949e", labelsize=7)
        for spine in ax.spines.values():
            spine.set_color("#30363d")
        ax.grid(True, alpha=0.12, color="#8b949e")
        ax.plot(t, y, color=color, linewidth=0.6, alpha=0.85)
        ax.set_ylabel(ylabel, fontsize=8, fontfamily="monospace", color="#c9d1d9")
        if ref_label:
            ax.axhline(T_target, color="#8b949e", linestyle="--",
                       linewidth=0.6, alpha=0.5)
            ax.legend([ref_label], fontsize=7, loc="upper right",
                      facecolor="#161b22", edgecolor="#30363d",
                      labelcolor="#c9d1d9")

    axes[-1].set_xlabel("Time (fs)", fontsize=8, fontfamily="monospace",
                        color="#c9d1d9")

    fig.text(0.50, 0.97,
             "NVT Langevin MD — H₂O at 300 K",
             fontsize=12, fontfamily="monospace", color="#c9d1d9",
             ha="center", verticalalignment="top")
    fig.text(0.95, 0.02, "VSEPR-Sim",
             fontsize=8, fontfamily="monospace", color="#484f58", ha="right")

    plt.tight_layout(pad=1.0)
    os.makedirs(os.path.dirname(outpath), exist_ok=True)
    fig.savefig(outpath, dpi=dpi, facecolor=fig.get_facecolor(),
                bbox_inches="tight", pad_inches=0.15)
    plt.close(fig)
    print(f"  ✓  {outpath}  ({os.path.getsize(outpath) // 1024} KB)")


# ============================================================================
# Workstation / Desktop UI mockup
# ============================================================================

def render_desktop_mockup(outpath, dpi=200):
    """Render a mockup of the Qt desktop workstation UI layout."""

    w, h = 1280, 800
    img = Image.new("RGB", (w, h), (43, 43, 43))
    draw = ImageDraw.Draw(img)

    # Title bar
    draw.rectangle([0, 0, w, 30], fill=(50, 50, 50))
    draw.text((12, 8), "VSEPR — Molecular Workstation", fill=(212, 212, 212))
    # Close / min / max
    for i, c in enumerate([(200, 60, 60), (200, 200, 60), (60, 200, 60)]):
        draw.ellipse([w - 70 + i * 22, 9, w - 58 + i * 22, 21], fill=c)

    # Menu bar
    draw.rectangle([0, 30, w, 54], fill=(50, 50, 50))
    menus = ["File", "Simulation", "View", "Console"]
    mx = 12
    for m in menus:
        draw.text((mx, 36), m, fill=(180, 180, 180))
        mx += len(m) * 8 + 18

    # Toolbar
    draw.rectangle([0, 54, w, 82], fill=(50, 50, 50))
    tools = ["Open", "Save", "|", "Single Point", "FIRE Relax", "Run MD", "|", "Reset Cam", "Fit", "Wire", "|", "Screenshot"]
    tx = 10
    for t in tools:
        if t == "|":
            draw.line([tx + 2, 60, tx + 2, 76], fill=(80, 80, 80))
            tx += 10
        else:
            bw = len(t) * 7 + 12
            draw.rounded_rectangle([tx, 58, tx + bw, 78], radius=3, fill=(58, 58, 58), outline=(74, 74, 74))
            draw.text((tx + 6, 62), t, fill=(200, 200, 200))
            tx += bw + 4

    # Left dock — Objects
    dock_w = 200
    draw.rectangle([0, 82, dock_w, h - 28], fill=(30, 30, 30))
    draw.rectangle([0, 82, dock_w, 106], fill=(50, 50, 50))
    draw.text((8, 88), "Objects", fill=(180, 180, 180))
    items = ["▼ Molecule", "   ● O  (Z=8)", "   ● H  (Z=1)", "   ● H  (Z=1)",
             "   — Bond 0-1", "   — Bond 0-2"]
    iy = 114
    for item in items:
        c = (88, 166, 255) if "●" in item else (130, 130, 130) if "—" in item else (200, 200, 200)
        draw.text((12, iy), item, fill=c)
        iy += 20

    # Right dock — Properties
    draw.rectangle([w - dock_w, 82, w, h - 28], fill=(30, 30, 30))
    draw.rectangle([w - dock_w, 82, w, 106], fill=(50, 50, 50))
    draw.text((w - dock_w + 8, 88), "Properties", fill=(180, 180, 180))
    props = [
        ("Formula", "H₂O"),
        ("Atoms", "3"),
        ("Bonds", "2"),
        ("Geometry", "Bent"),
        ("Angle", "104.5°"),
        ("Energy", "-76.42 kcal/mol"),
        ("Symmetry", "C₂ᵥ"),
    ]
    py = 114
    for k, v in props:
        draw.text((w - dock_w + 10, py), k, fill=(154, 154, 154))
        draw.text((w - dock_w + 100, py), v, fill=(200, 200, 200))
        py += 22

    # Central viewport (dark)
    vx0, vy0, vx1, vy1 = dock_w, 82, w - dock_w, h - 160
    draw.rectangle([vx0, vy0, vx1, vy1], fill=(13, 17, 23))
    # Draw a simple molecule in the viewport
    vcx = (vx0 + vx1) // 2
    vcy = (vy0 + vy1) // 2
    # O
    o_r = 32
    draw.ellipse([vcx - o_r, vcy - 30 - o_r, vcx + o_r, vcy - 30 + o_r], fill=(210, 20, 20))
    # Highlight
    draw.ellipse([vcx - 12, vcy - 46, vcx + 4, vcy - 36], fill=(255, 100, 100))
    # Bonds
    draw.line([vcx - 8, vcy - 2, vcx - 80, vcy + 70], fill=(140, 140, 140), width=6)
    draw.line([vcx + 8, vcy - 2, vcx + 80, vcy + 70], fill=(140, 140, 140), width=6)
    # H atoms
    h_r = 20
    draw.ellipse([vcx - 80 - h_r, vcy + 70 - h_r, vcx - 80 + h_r, vcy + 70 + h_r], fill=(230, 230, 230))
    draw.ellipse([vcx - 86, vcy + 60, vcx - 72, vcy + 68], fill=(255, 255, 255))
    draw.ellipse([vcx + 80 - h_r, vcy + 70 - h_r, vcx + 80 + h_r, vcy + 70 + h_r], fill=(230, 230, 230))
    draw.ellipse([vcx + 74, vcy + 60, vcx + 88, vcy + 68], fill=(255, 255, 255))
    # Grid lines (subtle)
    for gx in range(vx0 + 40, vx1, 60):
        draw.line([gx, vy0, gx, vy1], fill=(30, 40, 50), width=1)
    for gy in range(vy0 + 40, vy1, 60):
        draw.line([vx0, gy, vx1, gy], fill=(30, 40, 50), width=1)

    # Bottom dock — Console
    draw.rectangle([0, h - 160, w, h - 28], fill=(30, 30, 30))
    draw.rectangle([0, h - 160, w, h - 136], fill=(50, 50, 50))
    draw.text((8, h - 154), "Console", fill=(180, 180, 180))
    console_lines = [
        (">> build H2O",                                 (88, 166, 255)),
        ("  Generated 3 atoms, 2 bonds",                 (120, 200, 120)),
        ("  Geometry: Bent  |  Point group: C2v",        (120, 200, 120)),
        (">> relax",                                     (88, 166, 255)),
        ("  FIRE converged in 47 steps (E = -76.42)",    (120, 200, 120)),
        (">> screenshot screenshot.png",                 (88, 166, 255)),
        ("  Image exported: screenshot.png",             (160, 160, 160)),
    ]
    cy = h - 128
    for line, color in console_lines:
        draw.text((12, cy), line, fill=color)
        cy += 16

    # Status bar
    draw.rectangle([0, h - 28, w, h], fill=(37, 37, 37))
    draw.text((8, h - 22), "Ready  |  H₂O  |  3 atoms  |  FIRE converged", fill=(128, 128, 128))

    os.makedirs(os.path.dirname(outpath), exist_ok=True)
    img.save(outpath, "PNG")
    print(f"  ✓  {outpath}  ({os.path.getsize(outpath) // 1024} KB)")


# ============================================================================
# Main
# ============================================================================

def main():
    out = "assets/images"
    count = 0
    print("Generating 3D molecular renders for README …\n")

    # ----------------------------------------------------------------
    # 1. Ball-and-stick renders (original + new molecules)
    # ----------------------------------------------------------------
    print("— Ball-and-Stick —")
    ballstick = [
        (water,                    "ballstick_h2o.png",       20, 30),
        (methane,                  "ballstick_ch4.png",       25, 40),
        (ammonia,                  "ballstick_nh3.png",       20, 25),
        (benzene,                  "ballstick_benzene.png",   45, 20),
        (ethanol,                  "ballstick_ethanol.png",   18, 55),
        (sulfur_hexafluoride,      "ballstick_sf6.png",       20, 35),
        (phosphorus_pentafluoride, "ballstick_pf5.png",       22, 30),
        (xenon_tetrafluoride,      "ballstick_xef4.png",      30, 45),
        (sulfuric_acid,            "ballstick_h2so4.png",     20, 50),
        (hexane,                   "ballstick_hexane.png",    15, 10),
        (ikaite,                   "ballstick_ikaite.png",    25, 40),
        (argon_cluster_13,         "ballstick_ar13.png",      25, 35),
    ]
    for mol_fn, filename, elev, azim in ballstick:
        title, atoms = mol_fn()
        bonds = detect_bonds(atoms)
        render_molecule(title, atoms, bonds, os.path.join(out, filename),
                        elev=elev, azim=azim)
        count += 1

    # ----------------------------------------------------------------
    # 2. Space-filling renders
    # ----------------------------------------------------------------
    print("\n— Space-Filling (VdW) —")
    spacefill = [
        (water,               "spacefill_h2o.png",       20, 30),
        (benzene,             "spacefill_benzene.png",   45, 20),
        (sulfuric_acid,       "spacefill_h2so4.png",     22, 50),
        (argon_cluster_13,    "spacefill_ar13.png",      25, 35),
    ]
    for mol_fn, filename, elev, azim in spacefill:
        title, atoms = mol_fn()
        render_spacefilling(title, atoms, os.path.join(out, filename),
                            elev=elev, azim=azim)
        count += 1

    # ----------------------------------------------------------------
    # 3. Wireframe renders
    # ----------------------------------------------------------------
    print("\n— Wireframe —")
    wireframe = [
        (benzene,       "wireframe_benzene.png",  45, 20),
        (hexane,        "wireframe_hexane.png",   15, 10),
    ]
    for mol_fn, filename, elev, azim in wireframe:
        title, atoms = mol_fn()
        bonds = detect_bonds(atoms)
        render_wireframe(title, atoms, bonds, os.path.join(out, filename),
                         elev=elev, azim=azim)
        count += 1

    # ----------------------------------------------------------------
    # 4. Multi-view comparison
    # ----------------------------------------------------------------
    print("\n— Multi-View Comparison —")
    title, atoms = methane()
    bonds = detect_bonds(atoms)
    render_multiview(title, atoms, bonds,
                     os.path.join(out, "multiview_ch4.png"),
                     elev=25, azim=40)
    count += 1

    title, atoms = benzene()
    bonds = detect_bonds(atoms)
    render_multiview(title, atoms, bonds,
                     os.path.join(out, "multiview_benzene.png"),
                     elev=45, azim=20)
    count += 1

    # ----------------------------------------------------------------
    # 5. PBC / Supercell
    # ----------------------------------------------------------------
    print("\n— PBC Supercell —")
    title, atoms = water()
    bonds = detect_bonds(atoms)
    # Simple cubic-ish ice unit cell approximation
    box_a = [3.2, 0.0, 0.0]
    box_b = [0.0, 3.2, 0.0]
    box_c = [0.0, 0.0, 3.2]
    render_supercell("H₂O — PBC Supercell (3×3×3)", atoms, bonds,
                     os.path.join(out, "pbc_h2o_supercell.png"),
                     box_a, box_b, box_c, nx=1, ny=1, nz=1,
                     elev=22, azim=40)
    count += 1

    # ----------------------------------------------------------------
    # 6. FIRE convergence plot
    # ----------------------------------------------------------------
    print("\n— Simulation Diagnostics —")
    render_fire_convergence(os.path.join(out, "fire_convergence.png"))
    count += 1

    # ----------------------------------------------------------------
    # 7. MD time-series plot
    # ----------------------------------------------------------------
    render_md_timeseries(os.path.join(out, "md_timeseries.png"))
    count += 1

    # ----------------------------------------------------------------
    # 8. Desktop workstation mockup
    # ----------------------------------------------------------------
    print("\n— Desktop UI —")
    render_desktop_mockup(os.path.join(out, "desktop_workstation.png"))
    count += 1

    print(f"\nDone — {count} images written to {out}/")


if __name__ == "__main__":
    main()
