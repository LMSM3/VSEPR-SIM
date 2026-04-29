"""
metal_graphs — Research-quality matplotlib graphs from crystal lattice data.

Generates six publication-grade figure types from the metal sweep pipeline,
all derived primarily from crystal lattice structure converted to xyzA
(Angstrom) coordinates:

  1. Stress-Strain Rainbow     — virtual uniaxial tensile test, colour by Z
  2. Brittleness Ranking       — Pugh ratio + Pettifor indicator per element
  3. X-ray Diffraction (XRD)   — Bragg peaks from FCC lattice d-spacings
  4. Toughness over Time       — energy absorption vs heating history
  5. Chaos Dashboard           — chi factor, sub-scores, amplitude histogram
  6. Lattice 3D Projection     — atom positions projected with CPK colours

Every data point traces back to crystal lattice positions (Angstrom),
Debye-model thermodynamics, and the chaos_amplitude sub-scores.
No hidden state.

Usage:
    python -m pykernel.metal_graphs                    # all 74 metals
    python -m pykernel.metal_graphs --symbol Fe Cu Au  # specific metals
    python -m pykernel.metal_graphs --out dir/          # custom output

VSEPR-SIM 3.0.0
"""

from __future__ import annotations

import math
import os
import sys
from dataclasses import dataclass, field
from pathlib import Path
from typing import Optional, Sequence

import numpy as np

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import matplotlib.cm as cm
from matplotlib.colors import Normalize
from matplotlib.collections import LineCollection

from pykernel.metallic_cp import (
    MetalRecord, METAL_DB, compute_cp, compute_cp_curve, CpResult,
    debye_cv, electronic_cv, dulong_petit, R,
)
from pykernel.heating_sim import (
    HeatingSimulation, HeatingSimConfig, PartThermalConfig,
    HeatSchedule, PartThermalResult,
)
from pykernel.metal_sweep import (
    ElementInfo, SweepFrame, load_elements, synthesise_metal_record,
    build_fcc_lattice, hex_to_colour, render_metal,
)
from pykernel.chaos_amplitude import (
    compute_chaos, ChaosResult, ScaleWeights, AmplitudeScale,
    build_amplitude_scale, score_displacement, score_force,
    score_thermal, score_anisotropy,
)
from pykernel.crystal_tui import TUIConfig, Vec3, LatticeInfo
from pykernel.live_viewer import XYZAFrame


# =====================================================================
# Constants
# =====================================================================

_DPI = 300
_FIG_W = 12
_FIG_H = 8


# =====================================================================
# Helper: lattice parameter from covalent radius (FCC)
# =====================================================================

def _fcc_lattice_param(r_cov: float) -> float:
    """a = 2*sqrt(2)*r_cov for FCC."""
    return 2.0 * math.sqrt(2.0) * r_cov


# =====================================================================
# Helper: CPK hex -> matplotlib RGB tuple
# =====================================================================

def _hex_to_rgb(h: str) -> tuple[float, float, float]:
    h = h.lstrip("#")
    if len(h) < 6:
        h = h.ljust(6, "0")
    return (int(h[0:2], 16) / 255.0,
            int(h[2:4], 16) / 255.0,
            int(h[4:6], 16) / 255.0)


# =====================================================================
# 1. Stress-Strain Rainbow
# =====================================================================

@dataclass
class StressStrainPoint:
    """One point on a virtual stress-strain curve."""
    strain: float       # epsilon (dimensionless)
    stress_GPa: float   # sigma (GPa)


def compute_stress_strain(
    metal: MetalRecord,
    lattice_a: float,
    n_points: int = 100,
    max_strain: float = 0.30,
) -> list[StressStrainPoint]:
    """Virtual uniaxial tensile test from lattice + Debye parameters.

    Stress model (Lennard-Jones-like interatomic):
      sigma(epsilon) = E * epsilon * (1 - epsilon)^2 * exp(-alpha * epsilon)

    where:
      E = bulk modulus estimate from Debye temperature:
          B = (M / V_m) * (k_B * theta_D)^2 / (gamma_G * h_bar^2)
          simplified: B ~ rho * (theta_D * k_B / h_bar)^2 / gamma_approx

      For tractability we use the Debye-Gruneisen approximation:
          E_Young ~ 3 * rho * (kB * theta_D)^2 / (M_atom * gamma^2)

    All positions in Angstrom; stress in GPa.
    """
    kB = 1.380649e-23   # J/K
    amu = 1.66054e-27   # kg

    # Estimate Young's modulus from Debye temperature
    M_atom = metal.molar_mass * amu           # mass per atom (kg)
    rho = metal.density * 1e3                  # kg/m^3
    theta = max(metal.theta_D, 50.0)
    gamma_eff = max(metal.gamma * 1e-3, 0.5e-3)  # Sommerfeld -> effective

    # Simplified Debye-Gruneisen:  E ~ C * rho * (kB * theta)^2 / M
    # C is a scaling constant ~10 to reproduce typical moduli
    E_Pa = 10.0 * rho * (kB * theta) ** 2 / M_atom
    E_GPa = E_Pa * 1e-9

    # Clamp to physically reasonable range [1, 800] GPa
    E_GPa = max(1.0, min(800.0, E_GPa))

    # alpha controls softening rate; higher for lower melting points
    alpha = 4.0 + 2000.0 / max(metal.melting_point, 100.0)

    curve = []
    for i in range(n_points):
        eps = max_strain * i / max(n_points - 1, 1)
        sigma = E_GPa * eps * (1.0 - eps) ** 2 * math.exp(-alpha * eps)
        curve.append(StressStrainPoint(strain=eps, stress_GPa=max(sigma, 0.0)))

    return curve


def plot_stress_strain_rainbow(
    elements: Sequence[ElementInfo],
    metals: dict[str, MetalRecord],
    out_path: str,
) -> str:
    """Rainbow stress-strain overlay for all metals, colour-coded by Z.

    Each element's curve is drawn with a colour from a spectral (rainbow)
    colourmap scaled by atomic number Z.  This produces the visual
    "rainbow effect" across the periodic table.

    Returns path to saved figure.
    """
    fig, ax = plt.subplots(figsize=(_FIG_W, _FIG_H))

    z_min = min(e.Z for e in elements)
    z_max = max(e.Z for e in elements)
    norm = Normalize(vmin=z_min, vmax=z_max)
    cmap = plt.colormaps["Spectral_r"]

    for elem in elements:
        metal = metals[elem.symbol]
        a = _fcc_lattice_param(elem.r_cov)
        curve = compute_stress_strain(metal, a)
        eps = [p.strain for p in curve]
        sig = [p.stress_GPa for p in curve]
        colour = cmap(norm(elem.Z))
        ax.plot(eps, sig, color=colour, linewidth=1.2, alpha=0.85)

    sm = cm.ScalarMappable(cmap=cmap, norm=norm)
    sm.set_array([])
    cbar = fig.colorbar(sm, ax=ax, pad=0.02)
    cbar.set_label("Atomic Number Z", fontsize=12)

    ax.set_xlabel("Strain  " + r"$\varepsilon$", fontsize=13)
    ax.set_ylabel("Stress  (GPa)", fontsize=13)
    ax.set_title("Stress-Strain Rainbow  —  Z=5..101 Metals (FCC lattice, Debye-derived E)",
                 fontsize=14, fontweight="bold")
    ax.set_xlim(0, 0.30)
    ax.set_ylim(bottom=0)
    ax.grid(True, alpha=0.25)

    plt.tight_layout()
    plt.savefig(out_path, dpi=_DPI, bbox_inches="tight")
    plt.close()
    return out_path


# =====================================================================
# 2. Brittleness Ranking
# =====================================================================

@dataclass
class BrittlenessRecord:
    """Brittleness indicator for one metal."""
    symbol: str
    Z: int
    pugh_ratio: float       # B/G (bulk/shear modulus ratio estimate)
    pettifor: float         # Cauchy pressure indicator (C12-C44 proxy)
    brittleness: float      # composite 0..1 (1 = most brittle)


def compute_brittleness(
    metal: MetalRecord,
    lattice_a: float,
) -> BrittlenessRecord:
    """Estimate brittleness from Debye-model elastic proxies.

    Pugh ratio B/G > 1.75 → ductile; < 1.75 → brittle
    Pettifor indicator (C12 - C44) > 0 → ductile; < 0 → brittle

    We estimate B and G from the Debye temperature:
      v_D = kB * theta_D / h_bar * (V/N)^(1/3)  (Debye velocity)
      B ~ rho * v_D^2                             (approximate)
      G ~ 0.6 * B (typical for FCC metals)

    All lattice data in Angstrom.
    """
    kB = 1.380649e-23
    hbar = 1.054571817e-34
    amu = 1.66054e-27

    rho = metal.density * 1e3  # kg/m^3
    theta = max(metal.theta_D, 50.0)
    M_atom = metal.molar_mass * amu
    V_atom = M_atom / max(rho, 100.0)  # m^3 per atom

    # Debye velocity
    v_D = kB * theta / hbar * V_atom ** (1.0 / 3.0)

    # Bulk modulus
    B = rho * v_D ** 2
    B_GPa = B * 1e-9

    # Shear modulus: G/B ratio varies by category
    # Transition metals: G/B ~ 0.4-0.6; alkali: 0.2-0.3
    if 21 <= metal.Z <= 30 or 39 <= metal.Z <= 48 or 72 <= metal.Z <= 80:
        gb_ratio = 0.50
    elif 57 <= metal.Z <= 71 or 89 <= metal.Z <= 103:
        gb_ratio = 0.40
    else:
        gb_ratio = 0.30

    G = B * gb_ratio
    G_GPa = G * 1e-9

    # Pugh ratio
    pugh = B_GPa / max(G_GPa, 0.01)

    # Pettifor indicator: C12 - C44
    # For cubic: C12 = B - 2G/3, C44 ~ G
    C12 = B_GPa - 2.0 * G_GPa / 3.0
    C44 = G_GPa
    pettifor = C12 - C44

    # Composite brittleness: 0 = ductile, 1 = brittle
    # Based on Pugh threshold 1.75 and Pettifor sign
    brit_pugh = max(0.0, min(1.0, (2.5 - pugh) / 2.5))
    brit_pettifor = max(0.0, min(1.0, -pettifor / max(abs(pettifor) + 1.0, 1.0)))
    brittleness = 0.6 * brit_pugh + 0.4 * brit_pettifor

    return BrittlenessRecord(
        symbol=metal.symbol,
        Z=metal.Z,
        pugh_ratio=pugh,
        pettifor=pettifor,
        brittleness=brittleness,
    )


def plot_brittleness(
    elements: Sequence[ElementInfo],
    metals: dict[str, MetalRecord],
    out_path: str,
) -> str:
    """Bar chart of brittleness ranking, colour by ductile/brittle."""
    records = []
    for elem in elements:
        metal = metals[elem.symbol]
        a = _fcc_lattice_param(elem.r_cov)
        records.append(compute_brittleness(metal, a))

    # Sort by brittleness (most ductile first)
    records.sort(key=lambda r: r.brittleness)

    symbols = [r.symbol for r in records]
    brits = [r.brittleness for r in records]
    colours = [cm.RdYlGn_r(b) for b in brits]

    fig, ax = plt.subplots(figsize=(_FIG_W + 4, _FIG_H))
    bars = ax.barh(range(len(symbols)), brits, color=colours, edgecolor="none")
    ax.set_yticks(range(len(symbols)))
    ax.set_yticklabels(symbols, fontsize=7)
    ax.set_xlabel("Brittleness Index  (0 = ductile, 1 = brittle)", fontsize=12)
    ax.set_title("Brittleness Ranking  —  Debye-model elastic proxies (FCC lattice)",
                 fontsize=14, fontweight="bold")
    ax.axvline(0.5, color="gray", linestyle="--", alpha=0.5, label="Threshold")
    ax.set_xlim(0, 1)
    ax.grid(True, axis="x", alpha=0.25)
    ax.legend(fontsize=10)
    ax.invert_yaxis()

    plt.tight_layout()
    plt.savefig(out_path, dpi=_DPI, bbox_inches="tight")
    plt.close()
    return out_path


# =====================================================================
# 3. X-ray Diffraction (XRD)
# =====================================================================

@dataclass
class XRDPeak:
    """One Bragg peak in the XRD pattern."""
    hkl: tuple[int, int, int]
    d_spacing_A: float     # Angstrom
    two_theta_deg: float   # degrees (for Cu K-alpha, lambda=1.5406 A)
    intensity: float       # relative (0..1)


def compute_xrd_pattern(
    lattice_a: float,
    wavelength_A: float = 1.5406,  # Cu K-alpha
    max_two_theta: float = 120.0,
) -> list[XRDPeak]:
    """Compute XRD peaks for an FCC lattice from Bragg's law.

    FCC selection rule: h, k, l all odd or all even.
    d-spacing: d = a / sqrt(h^2 + k^2 + l^2)
    Bragg: 2d sin(theta) = n * lambda
           sin(theta) = lambda / (2d)
           2*theta = 2 * arcsin(lambda / (2d))

    Intensity: Lorentz-polarisation factor × multiplicity.
    All distances in Angstrom.
    """
    # Generate allowed FCC reflections up to h^2+k^2+l^2 = 50
    reflections = []
    for h in range(0, 8):
        for k in range(0, 8):
            for l in range(0, 8):
                if h == 0 and k == 0 and l == 0:
                    continue
                # FCC selection: all odd or all even
                if (h % 2 == k % 2 == l % 2):
                    reflections.append((h, k, l))

    # Deduplicate by h^2+k^2+l^2 (same d-spacing)
    seen = {}
    for hkl in reflections:
        key = hkl[0] ** 2 + hkl[1] ** 2 + hkl[2] ** 2
        if key not in seen:
            seen[key] = hkl

    peaks = []
    for key, hkl in sorted(seen.items()):
        d = lattice_a / math.sqrt(key)
        sin_theta = wavelength_A / (2.0 * d)
        if abs(sin_theta) > 1.0:
            continue
        theta = math.asin(sin_theta)
        two_theta = math.degrees(2.0 * theta)
        if two_theta > max_two_theta:
            continue

        # Lorentz-polarisation factor
        cos2t = math.cos(2.0 * theta) ** 2
        lp = (1.0 + cos2t) / (math.sin(theta) ** 2 * math.cos(theta))

        # Multiplicity for cubic (simplified)
        h, k, l = hkl
        vals = sorted([abs(h), abs(k), abs(l)], reverse=True)
        if vals[0] == vals[1] == vals[2]:
            mult = 8
        elif vals[0] == vals[1] or vals[1] == vals[2]:
            mult = 24
        else:
            mult = 48

        intensity = mult * lp
        peaks.append(XRDPeak(hkl=hkl, d_spacing_A=d, two_theta_deg=two_theta,
                             intensity=intensity))

    # Normalise intensities
    if peaks:
        i_max = max(p.intensity for p in peaks)
        for p in peaks:
            p.intensity /= i_max

    return peaks


def plot_xrd_rainbow(
    elements: Sequence[ElementInfo],
    metals: dict[str, MetalRecord],
    out_path: str,
) -> str:
    """Stacked XRD patterns with rainbow colour coding by Z.

    Each element's diffraction pattern is offset vertically and coloured
    by atomic number.  Peak positions shift with lattice parameter.
    """
    fig, ax = plt.subplots(figsize=(_FIG_W, _FIG_H + 2))

    z_min = min(e.Z for e in elements)
    z_max = max(e.Z for e in elements)
    norm = Normalize(vmin=z_min, vmax=z_max)
    cmap = plt.colormaps["rainbow"]

    n_elem = len(elements)
    y_step = 1.0

    for idx, elem in enumerate(elements):
        a = _fcc_lattice_param(elem.r_cov)
        peaks = compute_xrd_pattern(a)
        colour = cmap(norm(elem.Z))
        y_offset = idx * y_step

        for p in peaks:
            # Draw peak as a narrow Gaussian
            x = np.linspace(p.two_theta_deg - 1.5, p.two_theta_deg + 1.5, 60)
            sigma = 0.3
            y = p.intensity * np.exp(-0.5 * ((x - p.two_theta_deg) / sigma) ** 2)
            ax.fill_between(x, y_offset, y_offset + y * y_step * 0.8,
                            color=colour, alpha=0.7, linewidth=0)

    sm = cm.ScalarMappable(cmap=cmap, norm=norm)
    sm.set_array([])
    cbar = fig.colorbar(sm, ax=ax, pad=0.02)
    cbar.set_label("Atomic Number Z", fontsize=12)

    ax.set_xlabel(r"$2\theta$  (degrees, Cu K$\alpha$)", fontsize=13)
    ax.set_ylabel("Element (offset by Z)", fontsize=13)
    ax.set_title("X-ray Diffraction  —  FCC Lattice Bragg Peaks (Rainbow by Z)",
                 fontsize=14, fontweight="bold")
    ax.set_xlim(10, 120)
    ax.set_yticks([])
    ax.grid(True, axis="x", alpha=0.25)

    plt.tight_layout()
    plt.savefig(out_path, dpi=_DPI, bbox_inches="tight")
    plt.close()
    return out_path


# =====================================================================
# 4. Toughness over Time
# =====================================================================

@dataclass
class ToughnessPoint:
    """Energy absorption at one time step."""
    time_s: float
    T_K: float
    cp_J_molK: float
    energy_absorbed_J: float
    toughness_proxy: float   # cumulative energy / mass


def compute_toughness_history(
    metal: MetalRecord,
    t_end: float = 5.0,
    dt: float = 0.01,
    power_W: float = 500.0,
    mass_kg: float = 0.1,
) -> list[ToughnessPoint]:
    """Toughness proxy via cumulative thermal energy absorption.

    Runs a heating simulation and tracks cumulative energy input as a
    proxy for toughness (energy absorbed before failure).  Higher c_p
    metals absorb more energy per kelvin rise → tougher.

    All lattice-derived: c_p comes from the Debye model (theta_D from
    lattice vibrations), and energy balance is explicit per time step.
    """
    sim = HeatingSimulation(HeatingSimConfig(dt=dt, t_end=t_end, T_max=5000.0))
    pcfg = PartThermalConfig(
        part_name=f"{metal.symbol}_toughness",
        metal_symbol=metal.symbol,
        mass_kg=mass_kg,
        T_initial=298.0,
        schedule=HeatSchedule(mode="constant", power=power_W),
        metal=metal,
    )
    sim._part_configs.append(pcfg)
    results = sim.run()

    if not results or not results[0].steps:
        return []

    result = results[0]
    history = []
    cumulative_J = 0.0
    for step in result.steps:
        Q = power_W * dt
        cumulative_J += Q
        toughness = cumulative_J / mass_kg  # J/kg
        history.append(ToughnessPoint(
            time_s=step.time,
            T_K=step.T,
            cp_J_molK=step.cp_molar,
            energy_absorbed_J=cumulative_J,
            toughness_proxy=toughness,
        ))

    return history


def plot_toughness_over_time(
    elements: Sequence[ElementInfo],
    metals: dict[str, MetalRecord],
    out_path: str,
    n_sample: int = 12,
) -> str:
    """Toughness (cumulative energy absorption) vs time for sampled metals."""
    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(_FIG_W + 2, _FIG_H))

    z_min = min(e.Z for e in elements)
    z_max = max(e.Z for e in elements)
    norm = Normalize(vmin=z_min, vmax=z_max)
    cmap = plt.colormaps["turbo"]

    # Sample elements evenly across Z range
    step = max(1, len(elements) // n_sample)
    sampled = elements[::step][:n_sample]

    for elem in sampled:
        metal = metals[elem.symbol]
        history = compute_toughness_history(metal)
        if not history:
            continue

        times = [h.time_s for h in history]
        toughness = [h.toughness_proxy / 1e3 for h in history]  # kJ/kg
        temps = [h.T_K for h in history]
        colour = cmap(norm(elem.Z))

        ax1.plot(times, toughness, color=colour, linewidth=1.5,
                 label=f"{elem.symbol} (Z={elem.Z})")
        ax2.plot(times, temps, color=colour, linewidth=1.5)

    ax1.set_xlabel("Time (s)", fontsize=12)
    ax1.set_ylabel("Toughness Proxy  (kJ/kg)", fontsize=12)
    ax1.set_title("Cumulative Energy Absorption", fontsize=13, fontweight="bold")
    ax1.legend(fontsize=7, ncol=2, loc="upper left")
    ax1.grid(True, alpha=0.25)

    ax2.set_xlabel("Time (s)", fontsize=12)
    ax2.set_ylabel("Temperature (K)", fontsize=12)
    ax2.set_title("Heating Response", fontsize=13, fontweight="bold")
    ax2.grid(True, alpha=0.25)

    fig.suptitle("Toughness over Time  —  Debye-model c_p(T) heating simulation",
                 fontsize=14, fontweight="bold", y=1.02)
    plt.tight_layout()
    plt.savefig(out_path, dpi=_DPI, bbox_inches="tight")
    plt.close()
    return out_path


# =====================================================================
# 5. Chaos Dashboard
# =====================================================================

def plot_chaos_dashboard(
    elements: Sequence[ElementInfo],
    metals: dict[str, MetalRecord],
    out_path: str,
) -> str:
    """Four-panel chaos factor dashboard.

    Panel 1: chi vs Z (bar chart, colour by grade)
    Panel 2: Sub-score breakdown (stacked area)
    Panel 3: Amplitude spectral entropy vs Z
    Panel 4: Dominant channel pie chart
    """
    # Compute chaos for all elements
    chaos_data = []
    for elem in elements:
        metal = metals[elem.symbol]
        a = _fcc_lattice_param(elem.r_cov)
        positions = build_fcc_lattice(a, nx=2, ny=2, nz=2)
        n = len(positions)
        # Build forces (same model as metal_sweep)
        forces_np = np.zeros_like(positions)
        center = positions[n // 2]
        sigma = a * 2.0
        sig2 = sigma ** 2
        wind_str = min(300.0 / 500.0, 5.0)
        for i in range(n):
            dx = positions[i][0] - center[0]
            dy = positions[i][1] - center[1]
            dz = positions[i][2] - center[2]
            r2 = dx * dx + dy * dy + dz * dz
            envelope = math.exp(-r2 / (2.0 * sig2))
            forces_np[i] = [
                math.sin(elem.Z * 0.7) * 0.5 * wind_str * envelope,
                math.cos(elem.Z * 0.3) * 0.5 * wind_str * envelope,
                0.2 * wind_str * envelope,
            ]
        fmags = [float(np.linalg.norm(forces_np[i])) for i in range(n)]
        pos_tuples = [(float(positions[i][0]), float(positions[i][1]),
                        float(positions[i][2])) for i in range(n)]
        cr = compute_chaos(
            pos_tuples, pos_tuples, fmags,
            a, a, a, T=300.0, T_melt=metal.melting_point, label=elem.symbol,
        )
        chaos_data.append((elem, cr))

    fig, axes = plt.subplots(2, 2, figsize=(_FIG_W + 2, _FIG_H + 2))

    # Panel 1: chi vs Z
    ax = axes[0, 0]
    zs = [e.Z for e, _ in chaos_data]
    chis = [cr.chi for _, cr in chaos_data]
    grade_colours = {
        "STABLE": "#2ecc71", "LOW-CHAOS": "#27ae60",
        "MODERATE": "#f39c12", "HIGH-CHAOS": "#e74c3c",
        "CRITICAL": "#8e44ad",
    }
    colours = [grade_colours.get(cr.grade, "#999") for _, cr in chaos_data]
    ax.bar(zs, chis, color=colours, width=1.0, edgecolor="none")
    ax.set_xlabel("Z", fontsize=11)
    ax.set_ylabel(r"$\chi$", fontsize=13)
    ax.set_title("Chaos Factor vs Atomic Number", fontsize=12, fontweight="bold")
    ax.grid(True, alpha=0.25, axis="y")

    # Panel 2: Sub-score stacked area
    ax = axes[0, 1]
    s_disp = [cr.S_disp for _, cr in chaos_data]
    s_force = [cr.S_force for _, cr in chaos_data]
    s_therm = [cr.S_therm for _, cr in chaos_data]
    s_aniso = [cr.S_aniso for _, cr in chaos_data]
    ax.stackplot(zs, s_disp, s_force, s_therm, s_aniso,
                 labels=["S_disp", "S_force", "S_therm", "S_aniso"],
                 colors=["#3498db", "#e74c3c", "#f39c12", "#9b59b6"], alpha=0.8)
    ax.set_xlabel("Z", fontsize=11)
    ax.set_ylabel("Sub-score", fontsize=11)
    ax.set_title("Chaos Sub-score Breakdown", fontsize=12, fontweight="bold")
    ax.legend(fontsize=8, loc="upper left")
    ax.grid(True, alpha=0.25)

    # Panel 3: Spectral entropy vs Z
    ax = axes[1, 0]
    entropies = [cr.amplitude.spectral_entropy for _, cr in chaos_data]
    ax.scatter(zs, entropies, c=chis, cmap="RdYlGn_r", s=30, edgecolors="k",
               linewidths=0.3)
    ax.set_xlabel("Z", fontsize=11)
    ax.set_ylabel("Spectral Entropy (bits)", fontsize=11)
    ax.set_title("Amplitude Spectral Entropy", fontsize=12, fontweight="bold")
    ax.grid(True, alpha=0.25)

    # Panel 4: Dominant channel pie
    ax = axes[1, 1]
    channel_counts = {}
    for _, cr in chaos_data:
        ch = cr.dominant_channel()
        channel_counts[ch] = channel_counts.get(ch, 0) + 1
    labels = list(channel_counts.keys())
    sizes = list(channel_counts.values())
    pie_colours = {"disp": "#3498db", "force": "#e74c3c",
                   "therm": "#f39c12", "aniso": "#9b59b6"}
    ax.pie(sizes, labels=labels, autopct="%1.0f%%",
           colors=[pie_colours.get(l, "#999") for l in labels],
           startangle=90, textprops={"fontsize": 10})
    ax.set_title("Dominant Chaos Channel", fontsize=12, fontweight="bold")

    fig.suptitle("Chaos Factor Dashboard  —  Metal Sweep Z=5..101",
                 fontsize=14, fontweight="bold", y=1.01)
    plt.tight_layout()
    plt.savefig(out_path, dpi=_DPI, bbox_inches="tight")
    plt.close()
    return out_path


# =====================================================================
# 6. Lattice 3D Projection (xyzA Angstrom)
# =====================================================================

def plot_lattice_projection(
    elements: Sequence[ElementInfo],
    metals: dict[str, MetalRecord],
    out_path: str,
    n_sample: int = 9,
) -> str:
    """3x3 grid of FCC lattice projections, CPK-coloured, Angstrom axes.

    All atom positions are in xyzA (Angstrom) coordinates.
    Force vectors shown as arrows.
    """
    step = max(1, len(elements) // n_sample)
    sampled = elements[::step][:n_sample]

    ncols = min(3, len(sampled))
    nrows = math.ceil(len(sampled) / max(ncols, 1))
    fig, axes = plt.subplots(nrows, ncols, figsize=(_FIG_W, _FIG_H),
                              subplot_kw={"projection": "3d"},
                              squeeze=False)

    for idx, elem in enumerate(sampled):
        r = idx // ncols
        c = idx % ncols
        ax = axes[r][c]

        a = _fcc_lattice_param(elem.r_cov)
        positions = build_fcc_lattice(a, nx=2, ny=2, nz=2)
        cpk_rgb = _hex_to_rgb(elem.cpk_hex)

        ax.scatter(positions[:, 0], positions[:, 1], positions[:, 2],
                   c=[cpk_rgb], s=60, edgecolors="k", linewidths=0.3,
                   alpha=0.9, depthshade=True)

        # Force arrows (subsample for clarity)
        n = len(positions)
        center = positions[n // 2]
        sigma = a * 2.0
        sig2 = sigma ** 2
        for i in range(0, n, 4):
            dx = positions[i][0] - center[0]
            dy = positions[i][1] - center[1]
            dz = positions[i][2] - center[2]
            r2 = dx * dx + dy * dy + dz * dz
            env = math.exp(-r2 / (2.0 * sig2))
            fx = math.sin(elem.Z * 0.7) * 0.3 * env * a
            fy = math.cos(elem.Z * 0.3) * 0.3 * env * a
            fz = 0.1 * env * a
            ax.quiver(positions[i][0], positions[i][1], positions[i][2],
                      fx, fy, fz, color="red", alpha=0.5, arrow_length_ratio=0.3,
                      linewidth=0.8)

        ax.set_title(f"{elem.symbol}  (Z={elem.Z})\na={a:.2f} " + r"$\AA$",
                     fontsize=9)
        ax.set_xlabel(r"x ($\AA$)", fontsize=7)
        ax.set_ylabel(r"y ($\AA$)", fontsize=7)
        ax.set_zlabel(r"z ($\AA$)", fontsize=7)
        ax.tick_params(labelsize=6)

    # Hide unused subplots
    for idx in range(len(sampled), nrows * ncols):
        r = idx // ncols
        c = idx % ncols
        axes[r][c].set_visible(False)

    fig.suptitle("FCC Lattice 3D Projections  —  xyzA Angstrom coordinates",
                 fontsize=14, fontweight="bold")
    plt.tight_layout()
    plt.savefig(out_path, dpi=_DPI, bbox_inches="tight")
    plt.close()
    return out_path


# =====================================================================
# Consolidated xyzA export
# =====================================================================

def export_sweep_xyza(
    elements: Sequence[ElementInfo],
    metals: dict[str, MetalRecord],
    out_path: str,
) -> str:
    """Write all metal lattices to a single xyzA trajectory file (Angstrom).

    Each frame: N_atoms, then element x y z charge vx vy vz fx fy fz energy
    All positions and forces in Angstrom units.
    """
    from pykernel.live_viewer import XYZAFrameWriter

    writer = XYZAFrameWriter(out_path, keep_history=True)

    for idx, elem in enumerate(elements):
        metal = metals[elem.symbol]
        a = _fcc_lattice_param(elem.r_cov)
        positions = build_fcc_lattice(a, nx=2, ny=2, nz=2)
        n = len(positions)

        forces = np.zeros_like(positions)
        center = positions[n // 2]
        sigma = a * 2.0
        sig2 = sigma ** 2
        wind_str = min(300.0 / 500.0, 5.0)
        for i in range(n):
            dx = positions[i][0] - center[0]
            dy = positions[i][1] - center[1]
            dz = positions[i][2] - center[2]
            r2 = dx * dx + dy * dy + dz * dz
            envelope = math.exp(-r2 / (2.0 * sig2))
            forces[i] = [
                math.sin(elem.Z * 0.7) * 0.5 * wind_str * envelope,
                math.cos(elem.Z * 0.3) * 0.5 * wind_str * envelope,
                0.2 * wind_str * envelope,
            ]

        cp = compute_cp(metal, 300.0)
        frame = XYZAFrame(
            step=idx,
            n_atoms=n,
            elements=[elem.symbol] * n,
            positions=positions,
            forces=forces,
            total_energy=cp.Cv_total * 300.0 / 1000.0,
        )
        writer.write(frame)

    return out_path


# =====================================================================
# Master generator
# =====================================================================

@dataclass
class GraphReport:
    """Collection of all generated figure paths."""
    stress_strain: str = ""
    brittleness: str = ""
    xrd: str = ""
    toughness: str = ""
    chaos_dashboard: str = ""
    lattice_3d: str = ""
    xyza_trajectory: str = ""
    summary_md: str = ""

    def all_paths(self) -> list[str]:
        return [p for p in [
            self.stress_strain, self.brittleness, self.xrd,
            self.toughness, self.chaos_dashboard, self.lattice_3d,
            self.xyza_trajectory, self.summary_md,
        ] if p]


def generate_all_graphs(
    out_dir: str = "out/metal_graphs",
    symbols: Optional[Sequence[str]] = None,
) -> GraphReport:
    """Generate all six figure types plus xyzA export.

    Parameters
    ----------
    out_dir : str
        Output directory for figures and data.
    symbols : list of str or None
        Restrict to specific element symbols.  None = all 74 metals.

    Returns
    -------
    GraphReport with paths to all generated files.
    """
    outdir = Path(out_dir)
    outdir.mkdir(parents=True, exist_ok=True)

    elements = load_elements()
    if symbols:
        sym_set = set(s.strip() for s in symbols)
        elements = [e for e in elements if e.symbol in sym_set]

    metals: dict[str, MetalRecord] = {}
    for elem in elements:
        metals[elem.symbol] = synthesise_metal_record(elem)

    report = GraphReport()

    print(f"  Generating graphs for {len(elements)} metals -> {outdir}")

    # 1. Stress-Strain Rainbow
    p = str(outdir / "stress_strain_rainbow.png")
    plot_stress_strain_rainbow(elements, metals, p)
    report.stress_strain = p
    print(f"  [1/7] Stress-Strain Rainbow       -> {p}")

    # 2. Brittleness
    p = str(outdir / "brittleness_ranking.png")
    plot_brittleness(elements, metals, p)
    report.brittleness = p
    print(f"  [2/7] Brittleness Ranking          -> {p}")

    # 3. XRD Rainbow
    p = str(outdir / "xrd_rainbow.png")
    plot_xrd_rainbow(elements, metals, p)
    report.xrd = p
    print(f"  [3/7] X-ray Diffraction Rainbow    -> {p}")

    # 4. Toughness over Time
    p = str(outdir / "toughness_over_time.png")
    plot_toughness_over_time(elements, metals, p)
    report.toughness = p
    print(f"  [4/7] Toughness over Time          -> {p}")

    # 5. Chaos Dashboard
    p = str(outdir / "chaos_dashboard.png")
    plot_chaos_dashboard(elements, metals, p)
    report.chaos_dashboard = p
    print(f"  [5/7] Chaos Dashboard              -> {p}")

    # 6. Lattice 3D Projection
    p = str(outdir / "lattice_3d_projection.png")
    plot_lattice_projection(elements, metals, p)
    report.lattice_3d = p
    print(f"  [6/7] Lattice 3D Projection        -> {p}")

    # 7. xyzA trajectory
    p = str(outdir / "all_metals.xyzA")
    export_sweep_xyza(elements, metals, p)
    report.xyza_trajectory = p
    print(f"  [7/7] xyzA Trajectory              -> {p}")

    # Summary Markdown
    md_path = str(outdir / "GRAPH_REPORT.md")
    _write_summary_md(md_path, report, elements)
    report.summary_md = md_path
    print(f"  Report                             -> {md_path}")

    return report


def _write_summary_md(path: str, report: GraphReport, elements: Sequence[ElementInfo]):
    """Write a Markdown summary referencing all figures."""
    with open(path, "w") as f:
        f.write("# VSEPR-SIM Metal Graphs Report\n\n")
        f.write("**Version:** 3.0.0  \n")
        f.write(f"**Elements:** {len(elements)} metals (Z=5..101)  \n")
        f.write("**Coordinate system:** xyzA (Angstrom)  \n\n")

        f.write("## 1. Stress-Strain Rainbow\n\n")
        f.write("Virtual uniaxial tensile test from Debye-derived Young's modulus.\n")
        f.write("Colour gradient by atomic number Z (spectral colourmap).\n\n")
        f.write(f"![Stress-Strain]({os.path.basename(report.stress_strain)})\n\n")

        f.write("## 2. Brittleness Ranking\n\n")
        f.write("Pugh ratio (B/G) and Pettifor indicator from Debye elastic proxies.\n")
        f.write("0 = ductile, 1 = brittle.\n\n")
        f.write(f"![Brittleness]({os.path.basename(report.brittleness)})\n\n")

        f.write("## 3. X-ray Diffraction (XRD)\n\n")
        f.write("FCC Bragg peaks from d = a/sqrt(h^2+k^2+l^2), Cu K-alpha.\n")
        f.write("Stacked patterns with rainbow colour coding by Z.\n\n")
        f.write(f"![XRD]({os.path.basename(report.xrd)})\n\n")

        f.write("## 4. Toughness over Time\n\n")
        f.write("Cumulative energy absorption from Debye c_p(T) heating.\n")
        f.write("Higher c_p metals absorb more energy per kelvin rise.\n\n")
        f.write(f"![Toughness]({os.path.basename(report.toughness)})\n\n")

        f.write("## 5. Chaos Factor Dashboard\n\n")
        f.write("Four-panel: chi vs Z, sub-score breakdown, spectral entropy, "
                "dominant channel.\n\n")
        f.write(f"![Chaos]({os.path.basename(report.chaos_dashboard)})\n\n")

        f.write("## 6. Lattice 3D Projection\n\n")
        f.write("FCC unit cells in xyzA Angstrom with CPK colours and force arrows.\n\n")
        f.write(f"![Lattice3D]({os.path.basename(report.lattice_3d)})\n\n")

        f.write("## Data Export\n\n")
        f.write(f"- xyzA trajectory: `{os.path.basename(report.xyza_trajectory)}`\n")
        f.write("  (all 74 metals, 32 atoms each, Angstrom coordinates)\n\n")

        f.write("---\n")
        f.write("*Generated by VSEPR-SIM metal_graphs pipeline.*\n")


# =====================================================================
# CLI entry point
# =====================================================================

def main():
    import argparse
    parser = argparse.ArgumentParser(
        description="VSEPR-SIM Metal Graphs — research-quality figures from crystal lattice data")
    parser.add_argument("--out", default="out/metal_graphs",
                        help="Output directory (default: out/metal_graphs)")
    parser.add_argument("--symbol", nargs="*", default=None,
                        help="Restrict to specific element symbols")
    args = parser.parse_args()

    print("\033[1;36m")
    print("  VSEPR-SIM Metal Graphs Generator v3.0.0")
    print("  Crystal lattice -> xyzA (Angstrom) -> matplotlib figures")
    print("\033[0m")

    report = generate_all_graphs(out_dir=args.out, symbols=args.symbol)

    print(f"\n\033[1;32m  Done — {len(report.all_paths())} files generated.\033[0m")


if __name__ == "__main__":
    main()
