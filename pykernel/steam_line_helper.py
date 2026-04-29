"""
steam_line_helper.py -- Steam Line with Flashing, Wall Heat Loss, Oxide Particulate
================================================================================

Analysis system: Case 5 -- RS-0001 framework applied to a two-phase steam pipe.

Physical regime
  - Pressurized water / steam with partial flashing through a pressure drop
  - Wall cooling (heat loss to environment)
  - Suspended / depositing oxide particles (carryover from boiler or corrosion)

Dominant composite terms (from system analysis)
  STEP_r : central -- enthalpy / phase / transport step
  H_r    : enthalpy-reactivity, huge near phase change
  P_r    : phase compatibility, governs flashing and condensation threshold
  V_r    : flow velocity and shear transport
  M_r    : fluid-particle mobility and carryover fraction
  I_r    : wall interaction, wetting, oxide adhesion
  D_r    : suspended particle dispersion
  C_r    : particulate and dissolved species fraction
  S_r    : entropy production (most significant near phase change)
  Y_r    : optional -- prior wall fouling history

Every result is individually traceable.  Streams via pykernel.pipe.

VSEPR-SIM 4.0.4
"""

from __future__ import annotations

import math
import time
import logging
from dataclasses import dataclass, field
from typing import Dict, List, Optional
from enum import Enum, auto

_log = logging.getLogger(__name__)


# ── Late imports (same pattern as pipe3_plant_helper) ──────────────────────────
def _import_gas():
    import sys, importlib.util, pathlib as _pl
    name = "pykernel.gas"
    if name in sys.modules:
        return sys.modules[name]
    p = _pl.Path(__file__).parent / "gas.py"
    spec = importlib.util.spec_from_file_location(name, str(p))
    mod = importlib.util.module_from_spec(spec)
    sys.modules[name] = mod
    spec.loader.exec_module(mod)
    return mod


def _import_pipe():
    import sys, importlib.util, pathlib as _pl
    name = "pykernel.pipe"
    if name in sys.modules:
        return sys.modules[name]
    p = _pl.Path(__file__).parent / "pipe.py"
    spec = importlib.util.spec_from_file_location(name, str(p))
    mod = importlib.util.module_from_spec(spec)
    sys.modules[name] = mod
    spec.loader.exec_module(mod)
    return mod


# ─────────────────────────────────────────────────────────────────────────────
# Constants
# ─────────────────────────────────────────────────────────────────────────────

R_UNIV       = 8.31446     # J/(mol K)
ATM_PA       = 101_325.0   # Pa
MISSING      = -9999.0

MW_H2O       = 0.018015    # kg/mol
CP_STEAM_kJ  = 2.08        # kJ/(kg K) -- superheated steam approx
CP_WATER_kJ  = 4.18        # kJ/(kg K)
H_VAP_kJ     = 2257.0      # kJ/kg at 100 C / 1 atm (reference)
T_SAT_1ATM   = 373.15      # K


# ─────────────────────────────────────────────────────────────────────────────
# Thermodynamic helpers
# ─────────────────────────────────────────────────────────────────────────────

def saturation_T_K(P_Pa: float) -> float:
    """Clausius-Clapeyron saturation temperature at P_Pa."""
    T_ref = T_SAT_1ATM
    P_ref = ATM_PA
    dH    = H_VAP_kJ * 1000.0 * MW_H2O   # J/mol
    if P_Pa <= 0:
        return T_ref
    inv_T = 1.0 / T_ref - (R_UNIV / dH) * math.log(P_Pa / P_ref)
    return 1.0 / max(inv_T, 1e-6)


def saturation_P_Pa(T_K: float) -> float:
    """Clausius-Clapeyron saturation pressure at T_K."""
    T_ref = T_SAT_1ATM
    P_ref = ATM_PA
    dH    = H_VAP_kJ * 1000.0 * MW_H2O
    return P_ref * math.exp(dH / R_UNIV * (1.0 / T_ref - 1.0 / T_K))


def steam_density_kg_m3(T_K: float, P_Pa: float) -> float:
    """Superheated steam density via ideal gas."""
    return (P_Pa * MW_H2O) / (R_UNIV * max(T_K, 1.0))


def liquid_water_density_kg_m3(T_K: float) -> float:
    """Liquid water density fit (273-373 K)."""
    return 1000.0 - 0.003 * (T_K - 273.15) ** 2


def steam_viscosity_Pa_s(T_K: float) -> float:
    """Power-law dynamic viscosity of steam."""
    return 8.5e-6 * (T_K / 373.15) ** 0.65


def quality_after_flash(h_in_kJ_kg: float, P_out_Pa: float) -> float:
    """
    Steam quality x after isenthalpic flash to P_out_Pa.
    Returns x clamped to [0, 1].
    """
    T_sat = saturation_T_K(P_out_Pa)
    h_f   = CP_WATER_kJ * (T_sat - 273.15)
    h_g   = h_f + H_VAP_kJ
    if h_in_kJ_kg >= h_g:
        return 1.0
    if h_in_kJ_kg <= h_f:
        return 0.0
    return (h_in_kJ_kg - h_f) / H_VAP_kJ


def entropy_gen_kJ_kgK(T_fluid_K: float, T_ambient_K: float,
                        Q_loss_kJ_kg: float) -> float:
    """Irreversible entropy generation from heat loss across a temperature gap."""
    if T_fluid_K <= 0 or T_ambient_K <= 0:
        return 0.0
    return Q_loss_kJ_kg * (1.0 / T_ambient_K - 1.0 / T_fluid_K)


# ─────────────────────────────────────────────────────────────────────────────
# RS-0001 Term Evaluators  (dimensionless 0-1 severity scores)
# ─────────────────────────────────────────────────────────────────────────────

def term_STEP_r(dP_bar: float, dh_kJ_kg: float, x_out: float) -> float:
    p = min(abs(dP_bar) / 50.0, 1.0)
    h = min(abs(dh_kJ_kg) / 500.0, 1.0)
    return (p + h + x_out) / 3.0


def term_H_r(dh_kJ_kg: float, x_out: float) -> float:
    base  = min(abs(dh_kJ_kg) / 500.0, 1.0)
    boost = 0.5 * x_out if 0.0 < x_out < 1.0 else 0.0
    return min(base + boost, 1.0)


def term_P_r(T_K: float, P_Pa: float, x_out: float) -> float:
    T_sat   = saturation_T_K(P_Pa)
    approach = 1.0 - abs(T_K - T_sat) / max(T_sat, 1.0)
    return max(0.0, min(approach + 0.3 * x_out, 1.0))


def term_V_r(v_ms: float, v_ref: float = 30.0) -> float:
    return min(v_ms / v_ref, 1.0)


def term_M_r(particle_load_kg_m3: float, fluid_density_kg_m3: float) -> float:
    if fluid_density_kg_m3 <= 0:
        return 0.0
    return min(particle_load_kg_m3 / (0.01 * fluid_density_kg_m3), 1.0)


def term_I_r(fouling_mm: float, deposition_rate: float) -> float:
    return (min(fouling_mm / 2.0, 1.0) + min(deposition_rate, 1.0)) / 2.0


def term_D_r(particle_um: float, v_ms: float) -> float:
    size  = 1.0 - min(particle_um / 100.0, 1.0)
    vel   = min(v_ms / 30.0, 1.0)
    return (size + vel) / 2.0


def term_C_r(mass_fraction: float, dissolved_ppm: float) -> float:
    return (min(mass_fraction / 0.01, 1.0) + min(dissolved_ppm / 100.0, 1.0)) / 2.0


def term_S_r(ds_kJ_kgK: float) -> float:
    return min(abs(ds_kJ_kgK) / 0.5, 1.0)


def term_Y_r(prior_fouling_mm: float, cycles: int) -> float:
    return (min(prior_fouling_mm / 5.0, 1.0) + min(cycles / 1000.0, 1.0)) / 2.0


# ─────────────────────────────────────────────────────────────────────────────
# Data classes
# ─────────────────────────────────────────────────────────────────────────────

@dataclass
class OxideParticulate:
    """Suspended oxide particle characteristics."""
    species: str             = "Fe3O4"
    diameter_um: float       = 10.0
    mass_fraction: float     = 1e-4
    dissolved_ppm: float     = 5.0
    stickiness: float        = 0.3

    def load_kg_m3(self, fluid_density: float) -> float:
        return self.mass_fraction * fluid_density


@dataclass
class WallCondition:
    """Pipe wall thermal and fouling state."""
    inner_diameter_m: float   = 0.05
    wall_thickness_m: float   = 0.006
    material: str             = "carbon_steel"
    k_wall_W_mK: float        = 50.0
    ambient_T_K: float        = 298.15
    h_outer_W_m2K: float      = 10.0
    fouling_mm: float         = 0.1
    prior_fouling_mm: float   = 0.0
    thermal_cycles: int       = 0

    def U_overall(self, h_inner: float) -> float:
        """Overall heat-transfer coefficient W/(m2 K) -- simplified cylindrical."""
        R_i = 1.0 / max(h_inner, 1.0)
        R_w = self.wall_thickness_m / max(self.k_wall_W_mK, 0.1)
        R_f = (self.fouling_mm * 1e-3) / 0.1
        R_o = 1.0 / max(self.h_outer_W_m2K, 1.0)
        return 1.0 / (R_i + R_w + R_f + R_o)


@dataclass
class SteamSegment:
    """One segment of the steam line."""
    name: str
    length_m: float            = 10.0
    inlet_T_K: float           = 500.0
    inlet_P_Pa: float          = 5e6
    inlet_quality: float       = 1.0
    mass_flow_kg_s: float      = 5.0
    wall: WallCondition        = field(default_factory=WallCondition)
    oxide: OxideParticulate    = field(default_factory=OxideParticulate)
    has_flash_valve: bool      = False
    flash_P_out_Pa: float      = 1e6
    label: str                 = ""


@dataclass
class SteamSegmentResult:
    """Computed outcome for one segment."""
    name: str
    inlet_T_K: float;  inlet_P_Pa: float;  inlet_quality: float
    outlet_T_K: float; outlet_P_Pa: float; outlet_quality: float
    density_kg_m3: float
    velocity_m_s: float
    heat_loss_kW: float
    dh_kJ_kg: float
    entropy_gen_kJ_kgK: float
    oxide_load_kg_m3: float
    deposition_tendency: float
    STEP_r: float; H_r: float; P_r: float; V_r: float; M_r: float
    I_r: float;   D_r: float; C_r: float; S_r: float; Y_r: float
    dominant_terms: List[str]
    interpretation: str

    def as_dict(self) -> Dict:
        d = self.__dict__.copy()
        d["dominant_terms"] = ", ".join(self.dominant_terms)
        return d


# ─────────────────────────────────────────────────────────────────────────────
# Analyzer
# ─────────────────────────────────────────────────────────────────────────────

class SteamLineAnalyzer:
    """Analyze a sequence of SteamSegments; compute RS-0001 term scores per segment."""

    def __init__(self, segments: List[SteamSegment]):
        self.segments = segments

    # ------------------------------------------------------------------
    def _calc(self, seg: SteamSegment) -> SteamSegmentResult:
        T_in = seg.inlet_T_K
        P_in = seg.inlet_P_Pa
        x_in = seg.inlet_quality

        # inlet enthalpy (kJ/kg relative to 0 C liquid)
        T_sat_in = saturation_T_K(P_in)
        h_f_in   = CP_WATER_kJ * (T_sat_in - 273.15)
        h_in     = h_f_in + x_in * H_VAP_kJ
        if x_in >= 1.0:
            h_in += CP_STEAM_kJ * (T_in - T_sat_in)

        # flash valve (isenthalpic)
        if seg.has_flash_valve:
            P_out    = seg.flash_P_out_Pa
            x_mid    = quality_after_flash(h_in, P_out)
            T_sat_o  = saturation_T_K(P_out)
            if x_mid >= 1.0:
                T_out = T_sat_o + (h_in - (CP_WATER_kJ * (T_sat_o - 273.15) + H_VAP_kJ)) / CP_STEAM_kJ
            else:
                T_out = T_sat_o
        else:
            P_out = P_in
            x_mid = x_in
            T_out = T_in

        # fluid properties
        if x_mid >= 1.0:
            rho   = steam_density_kg_m3(T_out, P_out)
            mu    = steam_viscosity_Pa_s(T_out)
            cp_kJ = CP_STEAM_kJ
        else:
            T_sat_o  = saturation_T_K(P_out)
            rho_v    = steam_density_kg_m3(T_sat_o, P_out)
            rho_l    = liquid_water_density_kg_m3(T_sat_o)
            rho      = 1.0 / (x_mid / max(rho_v, 1e-9) + (1 - x_mid) / max(rho_l, 1e-9))
            mu       = steam_viscosity_Pa_s(T_sat_o)
            cp_kJ    = x_mid * CP_STEAM_kJ + (1 - x_mid) * CP_WATER_kJ

        A   = math.pi / 4.0 * seg.wall.inner_diameter_m ** 2
        v   = seg.mass_flow_kg_s / max(rho * A, 1e-9)
        Re  = rho * v * seg.wall.inner_diameter_m / max(mu, 1e-12)
        f_D = (0.316 / Re ** 0.25) if Re > 2300 else 64.0 / max(Re, 1.0)
        dP_fric = f_D * (seg.length_m / seg.wall.inner_diameter_m) * 0.5 * rho * v ** 2

        if not seg.has_flash_valve:
            P_out = max(P_in - dP_fric, 1e4)

        # wall heat loss
        Nu       = 0.023 * Re ** 0.8 * 0.6  # Pr~0.6 steam approx
        k_fluid  = 0.025                    # W/(m K) steam
        h_inner  = max(Nu * k_fluid / seg.wall.inner_diameter_m, 1.0)
        if x_mid < 1.0:
            h_inner  = 6000.0               # enhanced two-phase convection

        U    = seg.wall.U_overall(h_inner)
        Ac   = math.pi * seg.wall.inner_diameter_m * seg.length_m
        Q_kW = U * Ac * max(T_out - seg.wall.ambient_T_K, 0.0) / 1000.0
        dh   = -Q_kW / max(seg.mass_flow_kg_s, 1e-9)

        # outlet after heat loss
        h_out  = h_in + dh
        x_out  = quality_after_flash(h_out, P_out)
        T_sat2 = saturation_T_K(P_out)
        if x_out >= 1.0:
            T_out2 = T_sat2 + (h_out - (CP_WATER_kJ * (T_sat2 - 273.15) + H_VAP_kJ)) / CP_STEAM_kJ
        else:
            T_out2 = T_sat2

        ds   = entropy_gen_kJ_kgK(T_out, seg.wall.ambient_T_K, abs(dh))
        load = seg.oxide.load_kg_m3(rho)
        dep  = min(seg.oxide.stickiness * (1.0 - min(v / 10.0, 1.0)) + (0.3 if 0 < x_out < 1 else 0.0), 1.0)
        dP_b = (P_in - P_out) / 1e5

        STEP = term_STEP_r(dP_b, dh, x_out)
        H    = term_H_r(dh, x_out)
        P    = term_P_r(T_out2, P_out, x_out)
        V    = term_V_r(v)
        M    = term_M_r(load, rho)
        I    = term_I_r(seg.wall.fouling_mm, dep)
        D    = term_D_r(seg.oxide.diameter_um, v)
        C    = term_C_r(seg.oxide.mass_fraction, seg.oxide.dissolved_ppm)
        S    = term_S_r(ds)
        Y    = term_Y_r(seg.wall.prior_fouling_mm, seg.wall.thermal_cycles)

        scores   = {"STEP_r": STEP, "H_r": H, "P_r": P, "V_r": V, "M_r": M,
                    "I_r": I, "D_r": D, "C_r": C, "S_r": S, "Y_r": Y}
        dominant = [k for k, v_ in sorted(scores.items(), key=lambda kv: -kv[1]) if v_ >= 0.4][:4]

        phase = ("superheated vapour" if x_out >= 1.0
                 else "saturated two-phase" if x_out > 0.01 else "subcooled liquid")
        interp = (f"Outlet: {phase} (x={x_out:.3f}). "
                  f"Heat loss: {Q_kW:.2f} kW. dP: {dP_b:.2f} bar. "
                  f"Oxide: {load*1e6:.1f} mg/m3. "
                  f"Dominant: {', '.join(dominant) if dominant else 'none'}.")

        return SteamSegmentResult(
            name=seg.name,
            inlet_T_K=T_in, inlet_P_Pa=P_in, inlet_quality=x_in,
            outlet_T_K=T_out2, outlet_P_Pa=P_out, outlet_quality=x_out,
            density_kg_m3=rho, velocity_m_s=v, heat_loss_kW=Q_kW,
            dh_kJ_kg=dh, entropy_gen_kJ_kgK=ds,
            oxide_load_kg_m3=load, deposition_tendency=dep,
            STEP_r=STEP, H_r=H, P_r=P, V_r=V, M_r=M,
            I_r=I, D_r=D, C_r=C, S_r=S, Y_r=Y,
            dominant_terms=dominant, interpretation=interp,
        )

    # ------------------------------------------------------------------
    def analyze(self) -> List[SteamSegmentResult]:
        return [self._calc(s) for s in self.segments]

    # ------------------------------------------------------------------
    def print_report(self, results: Optional[List[SteamSegmentResult]] = None) -> None:
        if results is None:
            results = self.analyze()
        print()
        print("=" * 72)
        print("  STEAM LINE ANALYSIS -- RS-0001 Case 5")
        print("  Flashing / Wall Heat Loss / Oxide Particulate Carryover")
        print("=" * 72)
        for r in results:
            print()
            print(f"  Segment : {r.name}")
            print(f"    Inlet   {r.inlet_T_K:.1f} K  {r.inlet_P_Pa/1e5:.2f} bar  x={r.inlet_quality:.3f}")
            print(f"    Outlet  {r.outlet_T_K:.1f} K  {r.outlet_P_Pa/1e5:.2f} bar  x={r.outlet_quality:.3f}")
            print(f"    v={r.velocity_m_s:.2f} m/s   rho={r.density_kg_m3:.4f} kg/m3")
            print(f"    Heat loss : {r.heat_loss_kW:.3f} kW   dh={r.dh_kJ_kg:.3f} kJ/kg")
            print(f"    Entropy gen : {r.entropy_gen_kJ_kgK:.5f} kJ/(kg K)")
            print(f"    Oxide load  : {r.oxide_load_kg_m3*1e6:.2f} mg/m3   dep={r.deposition_tendency:.3f}")
            print()
            print("    RS-0001 Term Scores:")
            for nm, val in [("STEP_r", r.STEP_r), ("H_r", r.H_r), ("P_r", r.P_r),
                             ("V_r", r.V_r), ("M_r", r.M_r), ("I_r", r.I_r),
                             ("D_r", r.D_r), ("C_r", r.C_r), ("S_r", r.S_r), ("Y_r", r.Y_r)]:
                bar = "#" * int(val * 20)
                print(f"      {nm:8s}  {val:.3f}  |{bar:<20}|")
            print()
            print(f"    Dominant      : {', '.join(r.dominant_terms) if r.dominant_terms else 'none'}")
            print(f"    Interpretation: {r.interpretation}")
        print()
        print("=" * 72)

    # ------------------------------------------------------------------
    def stream_to_pipe(self, results: Optional[List[SteamSegmentResult]] = None,
                       csv_path: Optional[str] = None,
                       json_path: Optional[str] = None) -> List[SteamSegmentResult]:
        """Stream results through pykernel.pipe infrastructure."""
        if results is None:
            results = self.analyze()
        pipe_mod = _import_pipe()
        pipe = pipe_mod.Pipe()
        if csv_path:
            pipe.subscribe(pipe_mod.CSVSink(csv_path))
        if json_path:
            pipe.subscribe(pipe_mod.JSONSink(json_path))
        for r in results:
            pipe.push(r.as_dict())
        return results


# ─────────────────────────────────────────────────────────────────────────────
# Preset scenarios
# ─────────────────────────────────────────────────────────────────────────────

def preset_hp_superheated() -> List[SteamSegment]:
    """High-pressure superheated outlet line, moderate oxide carryover."""
    wall  = WallCondition(inner_diameter_m=0.08, wall_thickness_m=0.01,
                          material="alloy_steel", k_wall_W_mK=40.0,
                          ambient_T_K=298.15, h_outer_W_m2K=8.0,
                          fouling_mm=0.05, prior_fouling_mm=0.0, thermal_cycles=50)
    oxide = OxideParticulate(species="Fe3O4", diameter_um=5.0,
                             mass_fraction=5e-5, dissolved_ppm=3.0, stickiness=0.25)
    return [
        SteamSegment("HP-A", length_m=20.0, inlet_T_K=823.15, inlet_P_Pa=12e6,
                     inlet_quality=1.0, mass_flow_kg_s=8.0, wall=wall, oxide=oxide,
                     label="HP turbine inlet header"),
        SteamSegment("HP-B", length_m=15.0, inlet_T_K=810.0, inlet_P_Pa=11.5e6,
                     inlet_quality=1.0, mass_flow_kg_s=8.0, wall=wall, oxide=oxide,
                     label="Crossover to HP turbine"),
    ]


def preset_flash_valve() -> List[SteamSegment]:
    """Saturated line with isenthalpic flash valve; significant quality change."""
    wall  = WallCondition(inner_diameter_m=0.06, wall_thickness_m=0.008,
                          material="carbon_steel", k_wall_W_mK=50.0,
                          ambient_T_K=293.15, h_outer_W_m2K=12.0,
                          fouling_mm=0.3, prior_fouling_mm=1.2, thermal_cycles=800)
    oxide = OxideParticulate(species="Fe2O3", diameter_um=20.0,
                             mass_fraction=2e-4, dissolved_ppm=15.0, stickiness=0.55)
    return [
        SteamSegment("Pre-flash", length_m=8.0, inlet_T_K=453.0, inlet_P_Pa=1e6,
                     inlet_quality=0.98, mass_flow_kg_s=3.0, wall=wall, oxide=oxide,
                     label="Before flash valve"),
        SteamSegment("Flash-valve", length_m=0.5, inlet_T_K=453.0, inlet_P_Pa=1e6,
                     inlet_quality=0.98, mass_flow_kg_s=3.0, wall=wall, oxide=oxide,
                     has_flash_valve=True, flash_P_out_Pa=200_000,
                     label="Isenthalpic flash to 2 bar"),
        SteamSegment("Post-flash", length_m=12.0, inlet_T_K=393.15, inlet_P_Pa=200_000,
                     inlet_quality=0.85, mass_flow_kg_s=3.0, wall=wall, oxide=oxide,
                     label="Two-phase carryover after flash"),
    ]


def preset_fouled_historical() -> List[SteamSegment]:
    """Heavily fouled aged line -- Y_r and I_r dominated."""
    wall  = WallCondition(inner_diameter_m=0.05, wall_thickness_m=0.007,
                          material="carbon_steel", k_wall_W_mK=50.0,
                          ambient_T_K=288.15, h_outer_W_m2K=9.0,
                          fouling_mm=1.8, prior_fouling_mm=4.5, thermal_cycles=3000)
    oxide = OxideParticulate(species="mixed_oxide", diameter_um=50.0,
                             mass_fraction=8e-4, dissolved_ppm=60.0, stickiness=0.75)
    return [
        SteamSegment("Fouled-run", length_m=30.0, inlet_T_K=523.15, inlet_P_Pa=4e6,
                     inlet_quality=1.0, mass_flow_kg_s=2.0, wall=wall, oxide=oxide,
                     label="Aged service line"),
    ]


def run_presets() -> None:
    """Run all built-in preset scenarios and print reports."""
    for title, segs in [
        ("HP Superheated Line",     preset_hp_superheated()),
        ("Flash Valve Scenario",    preset_flash_valve()),
        ("Fouled Historical Line",  preset_fouled_historical()),
    ]:
        print()
        print("/" * 72)
        print(f"  PRESET: {title}")
        print("/" * 72)
        SteamLineAnalyzer(segs).print_report()


# ─────────────────────────────────────────────────────────────────────────────

if __name__ == "__main__":
    run_presets()
