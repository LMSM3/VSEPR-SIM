"""
metal_sweep — Infinite Z=5..101 metals sweep with forced 3D rendering.

Iterates every metallic element from Boron (Z=5) to Mendelevium (Z=101),
builds an FCC crystal lattice, runs a Debye-model heating simulation,
and renders each frame through the Crystal TUI with CPK colour mapping.

Usage:
    python -m pykernel.metal_sweep            # run infinite loop
    python -m pykernel.metal_sweep --once     # single pass Z=5..101

VSEPR-SIM 3.0.0
"""

from __future__ import annotations

import json
import math
import os
import sys
import time
from dataclasses import dataclass, field
from pathlib import Path
from typing import Optional

import numpy as np

from pykernel.metallic_cp import (
    METAL_DB, MetalRecord, compute_cp, debye_cv, dulong_petit, R,
)
from pykernel.heating_sim import (
    HeatingSimulation, HeatingSimConfig, PartThermalConfig,
    HeatSchedule, PartThermalResult,
)
from pykernel.crystal_tui import (
    CrystalTUI, TUIConfig, TUISnapshot, LatticeInfo, WindState,
    Vec3, Colour,
)
from pykernel.live_viewer import XYZAFrame, XYZAFrameWriter
from pykernel.pipe import Pipe, CSVSink, JSONSink
from pykernel.chaos_amplitude import (
    compute_chaos, ChaosResult, ScaleWeights, AmplitudeScale,
    summarise_chaos, ChaosSweepSummary,
)


# ═══════════════════════════════════════════════════════════════════════
# Element database loader
# ═══════════════════════════════════════════════════════════════════════

@dataclass(frozen=True)
class ElementInfo:
    """Minimal element record for the sweep."""
    Z: int
    symbol: str
    name: str
    weight: float
    r_cov: float
    r_vdw: float
    cpk_hex: str
    category: str


# Categories that count as metals for this sweep
_METAL_CATEGORIES = {
    "Alkali metal", "Alkaline earth metal",
    "Transition metal", "Post-transition metal",
    "Lanthanide", "Actinide",
}

# Some entries in the JSON lack a category field — classify by Z
_METAL_Z_RANGES = (
    range(3, 5),     # Li, Be
    range(11, 14),   # Na, Mg, Al
    range(19, 32),   # K..Ga
    range(37, 51),   # Rb..Sn
    range(55, 84),   # Cs..Bi
    range(87, 104),  # Fr..Lr
)


def _is_metal_by_Z(Z: int) -> bool:
    for r in _METAL_Z_RANGES:
        if Z in r:
            return True
    return False


def load_elements(json_path: Optional[str] = None) -> list[ElementInfo]:
    """Load periodic table JSON and filter to metals Z=5..101."""
    if json_path is None:
        json_path = str(Path(__file__).parent.parent / "data" / "periodic_table_102.json")

    with open(json_path, "r") as f:
        data = json.load(f)

    elements = []
    for e in data["elements"]:
        Z = e["Z"]
        if Z < 5 or Z > 101:
            continue
        cat = e.get("category", "")
        if cat in _METAL_CATEGORIES or _is_metal_by_Z(Z):
            elements.append(ElementInfo(
                Z=Z,
                symbol=e["symbol"],
                name=e["name"],
                weight=e["weight"],
                r_cov=e.get("r_cov", 1.5),
                r_vdw=e.get("r_vdw", 2.0),
                cpk_hex=e.get("cpk", "#C0C0C0"),
                category=cat if cat else "Metal",
            ))
    return elements


# ═══════════════════════════════════════════════════════════════════════
# Metal record synthesis (for elements not in METAL_DB)
# ═══════════════════════════════════════════════════════════════════════

def _estimate_theta_D(weight: float) -> float:
    """Rough Debye temperature from Lindemann scaling: Θ_D ~ 300 * (28/M)^(1/2)."""
    return 300.0 * math.sqrt(28.0 / max(weight, 1.0))


def _estimate_gamma(Z: int) -> float:
    """Rough Sommerfeld coefficient in mJ/(mol·K²)."""
    # Transition metals tend higher, s-block lower
    if 21 <= Z <= 30 or 39 <= Z <= 48 or 72 <= Z <= 80:
        return 4.0  # transition metals
    if 57 <= Z <= 71:
        return 8.0  # lanthanides (often high)
    if 89 <= Z <= 103:
        return 9.0  # actinides
    return 2.0


def _estimate_melting(weight: float) -> float:
    """Very rough melting point estimate."""
    return 500.0 + 5.0 * weight


def synthesise_metal_record(elem: ElementInfo) -> MetalRecord:
    """Build a MetalRecord from element info + estimates."""
    if elem.symbol in METAL_DB:
        return METAL_DB[elem.symbol]
    theta = _estimate_theta_D(elem.weight)
    gamma = _estimate_gamma(elem.Z)
    Tm = _estimate_melting(elem.weight)
    cp_est = dulong_petit(1)  # ~24.9 J/(mol·K)
    k_est = 50.0  # generic thermal conductivity
    return MetalRecord(
        symbol=elem.symbol,
        Z=elem.Z,
        name=elem.name,
        molar_mass=elem.weight,
        density=7.0,  # generic
        theta_D=theta,
        gamma=gamma,
        melting_point=Tm,
        cp_298=cp_est,
        thermal_cond=k_est,
    )


# ═══════════════════════════════════════════════════════════════════════
# FCC lattice builder
# ═══════════════════════════════════════════════════════════════════════

def build_fcc_lattice(a: float, nx: int = 2, ny: int = 2, nz: int = 2) -> np.ndarray:
    """Build FCC lattice positions. Returns (N, 3) array in Angstroms.

    a: lattice parameter in Angstroms
    nx, ny, nz: number of unit cells in each direction
    """
    basis = np.array([
        [0.0, 0.0, 0.0],
        [0.5, 0.5, 0.0],
        [0.5, 0.0, 0.5],
        [0.0, 0.5, 0.5],
    ])
    positions = []
    for ix in range(nx):
        for iy in range(ny):
            for iz in range(nz):
                offset = np.array([ix, iy, iz], dtype=float)
                for b in basis:
                    positions.append((b + offset) * a)
    return np.array(positions)


# ═══════════════════════════════════════════════════════════════════════
# CPK hex → Colour
# ═══════════════════════════════════════════════════════════════════════

def hex_to_colour(h: str) -> Colour:
    """Convert '#RRGGBB' to Colour."""
    h = h.lstrip("#")
    if len(h) < 6:
        h = h.ljust(6, "0")
    return Colour(int(h[0:2], 16), int(h[2:4], 16), int(h[4:6], 16))


# ═══════════════════════════════════════════════════════════════════════
# Per-element rendering pass
# ═══════════════════════════════════════════════════════════════════════

@dataclass
class SweepFrame:
    """One rendered frame in the metal sweep."""
    cycle: int
    Z: int
    symbol: str
    name: str
    n_atoms: int
    T_final: float
    cp_final: float
    tui_frame: str
    elapsed_ms: float
    chaos: ChaosResult = field(default_factory=ChaosResult)


def render_metal(
    elem: ElementInfo,
    metal: MetalRecord,
    cycle: int,
    config: TUIConfig,
    writer: Optional[XYZAFrameWriter] = None,
) -> SweepFrame:
    """Build lattice, run heating, render TUI frame for one metal."""
    t0 = time.perf_counter()

    # Build FCC lattice: a = 2*sqrt(2) * r_cov
    a = 2.0 * math.sqrt(2.0) * elem.r_cov
    positions = build_fcc_lattice(a, nx=2, ny=2, nz=2)
    n_atoms = len(positions)

    # Run short heating simulation (2 s at 500 W on 0.1 kg sample)
    try:
        sim = HeatingSimulation(HeatingSimConfig(dt=0.01, t_end=2.0, T_max=5000.0))
        pcfg = PartThermalConfig(
            part_name=f"{elem.symbol}_crystal",
            metal_symbol=elem.symbol,
            mass_kg=0.1,
            T_initial=298.0,
            schedule=HeatSchedule(mode="ramp", power=500.0, ramp_time=1.0),
            metal=metal,
        )
        # Bypass resolve_metal() — it would overwrite our synthesised record
        sim._part_configs.append(pcfg)
        results = sim.run()
        result = results[0]
        T_final = result.T_final
        cp_final = result.peak_cp
    except Exception:
        # Fallback for elements with extreme parameters
        T_final = 298.0 + 500.0 * 2.0 / (0.1 * 1000.0 * metal.molar_mass / 1000.0 * 0.5)
        cp_final = dulong_petit(1)

    # Compute c_p at final temperature for display
    cp_at_T = compute_cp(metal, T_final)

    # Wind direction: thermal agitation proportional to T
    wind_dir = Vec3(
        math.sin(elem.Z * 0.7) * 0.5,
        math.cos(elem.Z * 0.3) * 0.5,
        0.2,
    )
    wind_strength = min(T_final / 500.0, 5.0)
    sigma = a * 2.0

    # Build forces array: wind-like envelope on each atom
    forces_np = np.zeros_like(positions)
    center = positions[n_atoms // 2] if n_atoms > 0 else np.zeros(3)
    sig2 = sigma ** 2 if sigma > 1e-10 else 1.0
    for i in range(n_atoms):
        dx = positions[i][0] - center[0]
        dy = positions[i][1] - center[1]
        dz = positions[i][2] - center[2]
        r2 = dx * dx + dy * dy + dz * dz
        envelope = math.exp(-r2 / (2.0 * sig2))
        forces_np[i][0] = wind_dir.x * wind_strength * envelope
        forces_np[i][1] = wind_dir.y * wind_strength * envelope
        forces_np[i][2] = wind_dir.z * wind_strength * envelope

    # Convert numpy arrays → list[Vec3] for TUISnapshot
    pos_vec = [Vec3(float(positions[i][0]), float(positions[i][1]), float(positions[i][2])) for i in range(n_atoms)]
    frc_vec = [Vec3(float(forces_np[i][0]), float(forces_np[i][1]), float(forces_np[i][2])) for i in range(n_atoms)]

    # Compute force statistics
    fmags = [frc_vec[i].norm() for i in range(n_atoms)]
    Fmax = max(fmags) if fmags else 0.0
    Frms = math.sqrt(sum(f * f for f in fmags) / max(n_atoms, 1))

    # Build TUISnapshot with correct API
    snap = TUISnapshot(
        positions=pos_vec,
        types=[elem.Z] * n_atoms,
        forces=frc_vec,
        Frms=Frms,
        Fmax=Fmax,
        U_total=cp_at_T.Cv_total * T_final / 1000.0,
        KE=cp_at_T.Cv_total * T_final / 2000.0,
        T=T_final,
        step=cycle * 1000 + elem.Z,
        N_atoms=n_atoms,
        lattice=LatticeInfo(
            a=a, b=a, c=a,
            alpha_deg=90.0, beta_deg=90.0, gamma_deg=90.0,
            volume=a ** 3,
        ),
        wind=WindState(
            direction=wind_dir,
            strength=wind_strength,
            ramp=1.0,
            peak_force=Fmax,
            headroom=1.5,
            dt_factor=1.5,
        ),
    )

    # Render the frame through CrystalTUI → returns ANSI string
    tui = CrystalTUI(config)
    frame_str = tui.render(snap)

    # Chaos factor evaluation
    pos_tuples = [(float(positions[i][0]), float(positions[i][1]), float(positions[i][2])) for i in range(n_atoms)]
    ideal_tuples = list(pos_tuples)  # FCC lattice IS the ideal reference
    chaos_result = compute_chaos(
        positions=pos_tuples,
        ideal_positions=ideal_tuples,
        force_magnitudes=fmags,
        lattice_a=a,
        lattice_b=a,
        lattice_c=a,
        T=T_final,
        T_ref=298.0,
        T_melt=metal.melting_point,
        label=elem.symbol,
    )

    # Write xyzA frame
    if writer is not None:
        xyza = XYZAFrame(
            step=cycle * 1000 + elem.Z,
            n_atoms=n_atoms,
            elements=[elem.symbol] * n_atoms,
            positions=positions,
            forces=forces_np,
            total_energy=cp_at_T.Cv_total * T_final / 1000.0,
        )
        writer.write(xyza)

    elapsed = (time.perf_counter() - t0) * 1000.0

    return SweepFrame(
        cycle=cycle,
        Z=elem.Z,
        symbol=elem.symbol,
        name=elem.name,
        n_atoms=n_atoms,
        T_final=T_final,
        cp_final=cp_at_T.Cp_approx,
        tui_frame=frame_str,
        elapsed_ms=elapsed,
        chaos=chaos_result,
    )


# ═══════════════════════════════════════════════════════════════════════
# Banner
# ═══════════════════════════════════════════════════════════════════════

_BANNER = """\033[1;36m
╔══════════════════════════════════════════════════════════════════════╗
║  VSEPR-SIM  METAL SWEEP  Z=5..101  •  Infinite 3D Rendering Loop  ║
╚══════════════════════════════════════════════════════════════════════╝\033[0m"""

_ELEMENT_HEADER = """\033[1;33m
┌──────────────────────────────────────────────────────────────────────┐
│  Cycle {cycle:>4d}  │  Z={Z:<3d}  {symbol:<3s}  {name:<16s}  │  {category:<24s} │
└──────────────────────────────────────────────────────────────────────┘\033[0m"""

_RESULT_LINE = (
    "\033[32m  T_final = {T:.1f} K  │  "
    "c_p = {cp:.2f} J/(mol·K)  │  "
    "{n} atoms  │  "
    "{ms:.1f} ms\033[0m\n"
    "\033[36m  chi = {chi:.4f} [{grade}]  │  "
    "S_disp={Sd:.3f}  S_force={Sf:.3f}  S_therm={St:.3f}  S_aniso={Sa:.3f}  │  "
    "dominant: {dom}\033[0m"
)


# ═══════════════════════════════════════════════════════════════════════
# Main sweep runner
# ═══════════════════════════════════════════════════════════════════════

class MetalSweepRunner:
    """Infinite sweep of Z=5..101 metals with forced 3D TUI rendering."""

    def __init__(
        self,
        output_dir: str = "out/metal_sweep",
        tui_width: int = 80,
        tui_height: int = 36,
        render_delay: float = 0.3,
    ):
        self._outdir = Path(output_dir)
        self._outdir.mkdir(parents=True, exist_ok=True)

        self._elements = load_elements()
        self._metals: dict[str, MetalRecord] = {}
        for elem in self._elements:
            self._metals[elem.symbol] = synthesise_metal_record(elem)

        self._config = TUIConfig(width=tui_width, height=tui_height)
        self._writer = XYZAFrameWriter(
            str(self._outdir / "sweep_trajectory.xyzA"),
            keep_history=True,
        )

        # Pipe infrastructure
        self._pipe = Pipe[dict]("metal_sweep")
        self._csv = CSVSink(
            self._pipe,
            str(self._outdir / "sweep_log.csv"),
            ["cycle", "Z", "symbol", "name", "n_atoms",
             "T_final_K", "cp_J_molK",
             "chi", "grade", "S_disp", "S_force", "S_therm", "S_aniso",
             "dominant", "spectral_entropy", "elapsed_ms"],
        )
        self._json = JSONSink(
            self._pipe,
            str(self._outdir / "sweep_log.jsonl"),
        )

        self._delay = render_delay
        self._cycle = 0
        self._total_frames = 0
        self._start_time = 0.0

    @property
    def element_count(self) -> int:
        return len(self._elements)

    def run_once(self) -> list[SweepFrame]:
        """Run one full pass over all metals Z=5..101."""
        self._cycle += 1
        frames = []

        for elem in self._elements:
            metal = self._metals[elem.symbol]
            frame = render_metal(
                elem, metal, self._cycle, self._config, self._writer,
            )

            # Print to terminal: forced 3D rendering
            print(_ELEMENT_HEADER.format(
                cycle=self._cycle, Z=elem.Z,
                symbol=elem.symbol, name=elem.name,
                category=elem.category,
            ))
            print(frame.tui_frame)
            print(_RESULT_LINE.format(
                T=frame.T_final, cp=frame.cp_final,
                n=frame.n_atoms, ms=frame.elapsed_ms,
                chi=frame.chaos.chi, grade=frame.chaos.grade,
                Sd=frame.chaos.S_disp, Sf=frame.chaos.S_force,
                St=frame.chaos.S_therm, Sa=frame.chaos.S_aniso,
                dom=frame.chaos.dominant_channel(),
            ))
            print()

            # Push through pipe
            self._pipe.push({
                "cycle": self._cycle,
                "Z": elem.Z,
                "symbol": elem.symbol,
                "name": elem.name,
                "n_atoms": frame.n_atoms,
                "T_final_K": round(frame.T_final, 2),
                "cp_J_molK": round(frame.cp_final, 4),
                "chi": round(frame.chaos.chi, 6),
                "grade": frame.chaos.grade,
                "S_disp": round(frame.chaos.S_disp, 6),
                "S_force": round(frame.chaos.S_force, 6),
                "S_therm": round(frame.chaos.S_therm, 6),
                "S_aniso": round(frame.chaos.S_aniso, 6),
                "dominant": frame.chaos.dominant_channel(),
                "spectral_entropy": round(frame.chaos.amplitude.spectral_entropy, 4),
                "elapsed_ms": round(frame.elapsed_ms, 2),
            }, source="metal_sweep")

            self._total_frames += 1
            frames.append(frame)

            # Render delay — let the user see each frame
            time.sleep(self._delay)

        return frames

    def run_forever(self):
        """Infinite loop: sweep Z=5..101, repeat."""
        print(_BANNER)
        print(f"\033[1m  {self.element_count} metals loaded  │  "
              f"Output: {self._outdir}\033[0m\n")

        self._start_time = time.time()

        try:
            while True:
                frames = self.run_once()
                elapsed = time.time() - self._start_time
                fps = self._total_frames / max(elapsed, 1e-9)

                # Chaos summary for this cycle
                csummary = summarise_chaos([f.chaos for f in frames])

                print(f"\033[1;35m{'='*70}")
                print(f"  CYCLE {self._cycle} COMPLETE  │  "
                      f"{self._total_frames} total frames  │  "
                      f"{fps:.1f} frames/s  │  "
                      f"{elapsed:.0f}s elapsed")
                print(f"  chi: min={csummary.chi_min:.4f} "
                      f"mean={csummary.chi_mean:.4f} "
                      f"max={csummary.chi_max:.4f}  │  "
                      f"stable={csummary.most_stable}  "
                      f"chaotic={csummary.most_chaotic}")
                gdist = "  ".join(f"{k}:{v}" for k, v in sorted(csummary.grade_distribution.items()))
                print(f"  grades: {gdist}")
                print(f"{'='*70}\033[0m\n")

        except KeyboardInterrupt:
            elapsed = time.time() - self._start_time
            print(f"\n\033[1;31m  STOPPED after {self._cycle} cycles, "
                  f"{self._total_frames} frames, {elapsed:.1f}s\033[0m")


# ═══════════════════════════════════════════════════════════════════════
# __main__
# ═══════════════════════════════════════════════════════════════════════

def main():
    once = "--once" in sys.argv
    runner = MetalSweepRunner()
    if once:
        runner.run_once()
    else:
        runner.run_forever()


if __name__ == "__main__":
    main()
