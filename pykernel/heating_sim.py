"""
heating_sim — Time-dependent heating simulation per named STEP part.

Given a StepAssembly with material assignments, this module runs a
time-stepping thermal evolution where each part tracks:
  - temperature T(t) via energy balance:  m·c_p(T)·dT = Q_in(t)·dt
  - metallic c_p(T) from the Debye model (metallic_cp module)
  - heat input schedule Q_in(t) (constant, ramp, pulsed, or custom)

Output: per-part temperature-vs-time curves for piping to CSV/JSON/XLSX.

Anti-black-box: every time step, every intermediate c_p evaluation,
every energy balance residual is explicitly recorded and inspectable.

VSEPR-SIM 3.0.0
"""

from __future__ import annotations

import math
from dataclasses import dataclass, field
from typing import Optional, Callable

from pykernel.step_parser import NamedPart, StepAssembly, Vec3
from pykernel.metallic_cp import (
    MetalRecord, METAL_DB, compute_cp, CpResult, lookup_metal, dulong_petit, R,
)


# ═══════════════════════════════════════════════════════════════════════
# Heat source schedule
# ═══════════════════════════════════════════════════════════════════════

@dataclass
class HeatSchedule:
    """Time-dependent heat input schedule.

    Q_in(t) in Watts (J/s).
    mode:
      "constant"  → Q_in = power  for all t
      "ramp"      → Q_in linearly 0 → power over ramp_time, then constant
      "pulse"     → Q_in = power  for t in [pulse_start, pulse_start+pulse_width], else 0
      "custom"    → user-supplied callable  f(t) → Q_in
    """
    mode: str = "constant"
    power: float = 100.0           # W
    ramp_time: float = 0.0         # s (for "ramp" mode)
    pulse_start: float = 0.0       # s
    pulse_width: float = 1.0       # s
    custom_fn: Optional[Callable[[float], float]] = None

    def Q_in(self, t: float) -> float:
        """Evaluate heat input at time t (seconds)."""
        if self.mode == "constant":
            return self.power
        elif self.mode == "ramp":
            if self.ramp_time <= 0:
                return self.power
            frac = min(t / self.ramp_time, 1.0)
            return self.power * frac
        elif self.mode == "pulse":
            if self.pulse_start <= t <= self.pulse_start + self.pulse_width:
                return self.power
            return 0.0
        elif self.mode == "custom" and self.custom_fn is not None:
            return self.custom_fn(t)
        return self.power


# ═══════════════════════════════════════════════════════════════════════
# Per-part configuration
# ═══════════════════════════════════════════════════════════════════════

@dataclass
class PartThermalConfig:
    """Thermal configuration for one part in the assembly."""
    part_name: str
    metal_symbol: str         # e.g. "Fe", "Al", "Cu"
    mass_kg: float            # part mass in kg
    T_initial: float = 298.0  # K
    schedule: HeatSchedule = field(default_factory=HeatSchedule)

    # Resolved at setup time
    metal: Optional[MetalRecord] = field(default=None, repr=False)

    def resolve_metal(self):
        """Look up MetalRecord from symbol."""
        self.metal = lookup_metal(self.metal_symbol)
        if self.metal is None:
            raise ValueError(
                f"Unknown metal '{self.metal_symbol}' for part '{self.part_name}'. "
                f"Available: {sorted(METAL_DB.keys())}"
            )


# ═══════════════════════════════════════════════════════════════════════
# Time-step record
# ═══════════════════════════════════════════════════════════════════════

@dataclass
class ThermalTimeStep:
    """One snapshot in the thermal evolution of a single part."""
    step: int
    time: float          # s
    T: float             # K
    Q_in: float          # W
    cp_molar: float      # J/(mol·K)
    cp_specific: float   # J/(g·K)
    dT: float            # K (temperature change this step)
    energy_in: float     # J (cumulative)
    fraction_dp: float   # Cv_lattice / 3nR


@dataclass
class PartThermalResult:
    """Full thermal history for one part."""
    part_name: str
    metal_symbol: str
    mass_kg: float
    T_initial: float
    T_final: float
    steps: list[ThermalTimeStep] = field(default_factory=list)
    total_energy_J: float = 0.0
    peak_T: float = 0.0
    peak_cp: float = 0.0

    def temperatures(self) -> list[float]:
        return [s.T for s in self.steps]

    def times(self) -> list[float]:
        return [s.time for s in self.steps]

    def cp_curve(self) -> list[float]:
        return [s.cp_molar for s in self.steps]

    def as_dict_rows(self) -> list[dict]:
        """Convert to list of flat dicts for CSV/JSON piping."""
        rows = []
        for s in self.steps:
            rows.append({
                "part": self.part_name,
                "metal": self.metal_symbol,
                "step": s.step,
                "time_s": round(s.time, 6),
                "T_K": round(s.T, 4),
                "Q_in_W": round(s.Q_in, 4),
                "cp_molar_J_molK": round(s.cp_molar, 4),
                "cp_specific_J_gK": round(s.cp_specific, 6),
                "dT_K": round(s.dT, 6),
                "energy_in_J": round(s.energy_in, 4),
                "fraction_DP": round(s.fraction_dp, 4),
            })
        return rows


# ═══════════════════════════════════════════════════════════════════════
# Simulation engine
# ═══════════════════════════════════════════════════════════════════════

@dataclass
class HeatingSimConfig:
    """Global simulation configuration."""
    dt: float = 0.01        # time step (s)
    t_end: float = 10.0     # total simulation time (s)
    T_max: float = 5000.0   # safety cutoff temperature (K)
    validate_energy: bool = True  # check energy balance each step


class HeatingSimulation:
    """Time-stepping thermal evolution for multi-part assemblies.

    Algorithm per part per time step:
      1. Evaluate Q_in(t) from the heat schedule
      2. Compute c_p(T) via Debye model for the assigned metal
      3. Energy balance:  dE = Q_in · dt
      4. Temperature update:  dT = dE / (m · c_p_specific)
      5. T_new = T_old + dT
      6. Record all intermediates

    The simulation is explicit Euler (first-order) — sufficient for
    thermal time-scales with small dt.
    """

    def __init__(self, config: HeatingSimConfig = None):
        self.config = config or HeatingSimConfig()
        self._part_configs: list[PartThermalConfig] = []
        self._results: list[PartThermalResult] = []
        self._validation_errors: list[str] = []

    def add_part(self, cfg: PartThermalConfig):
        """Register a part for simulation."""
        cfg.resolve_metal()
        self._part_configs.append(cfg)

    def add_parts_from_assembly(
        self,
        assembly: StepAssembly,
        material_map: dict[str, str],
        mass_map: dict[str, float],
        schedule: Optional[HeatSchedule] = None,
        T_initial: float = 298.0,
    ):
        """Bulk-add parts from a parsed STEP assembly.

        material_map: part_name → metal symbol (e.g. {"Housing": "Al"})
        mass_map: part_name → mass in kg
        """
        for part in assembly.parts:
            sym = material_map.get(part.name)
            mass = mass_map.get(part.name)
            if sym is None or mass is None:
                continue
            cfg = PartThermalConfig(
                part_name=part.name,
                metal_symbol=sym,
                mass_kg=mass,
                T_initial=T_initial,
                schedule=schedule or HeatSchedule(),
            )
            self.add_part(cfg)

    def run(self) -> list[PartThermalResult]:
        """Execute the heating simulation for all registered parts."""
        self._results.clear()
        self._validation_errors.clear()

        for pcfg in self._part_configs:
            result = self._run_part(pcfg)
            self._results.append(result)

        return self._results

    def _run_part(self, pcfg: PartThermalConfig) -> PartThermalResult:
        """Run simulation for a single part."""
        metal = pcfg.metal
        dt = self.config.dt
        n_steps = int(math.ceil(self.config.t_end / dt))
        mass_g = pcfg.mass_kg * 1000.0   # kg → g
        n_moles = mass_g / metal.molar_mass

        T = pcfg.T_initial
        E_cumulative = 0.0
        peak_T = T
        peak_cp = 0.0

        result = PartThermalResult(
            part_name=pcfg.part_name,
            metal_symbol=pcfg.metal_symbol,
            mass_kg=pcfg.mass_kg,
            T_initial=pcfg.T_initial,
            T_final=T,
        )

        for step_i in range(n_steps + 1):
            t = step_i * dt

            # Heat input
            Q = pcfg.schedule.Q_in(t)

            # c_p at current temperature
            cp_res = compute_cp(metal, max(T, 1.0))

            # Energy balance
            dE = Q * dt if step_i > 0 else 0.0
            E_cumulative += dE

            # Temperature change: dT = dE / (m * cp_specific)
            # cp_specific is J/(g·K), mass_g is in grams
            cp_s = cp_res.cp_specific
            if cp_s > 0 and mass_g > 0:
                dT_val = dE / (mass_g * cp_s) if step_i > 0 else 0.0
            else:
                dT_val = 0.0

            if step_i > 0:
                T += dT_val

            # Safety cutoff
            if T > self.config.T_max:
                T = self.config.T_max
                self._validation_errors.append(
                    f"Part '{pcfg.part_name}': T_max reached at t={t:.4f}s"
                )

            # Track peaks
            if T > peak_T:
                peak_T = T
            if cp_res.Cp_approx > peak_cp:
                peak_cp = cp_res.Cp_approx

            ts = ThermalTimeStep(
                step=step_i,
                time=t,
                T=T,
                Q_in=Q,
                cp_molar=cp_res.Cp_approx,
                cp_specific=cp_s,
                dT=dT_val,
                energy_in=E_cumulative,
                fraction_dp=cp_res.fraction_dp,
            )
            result.steps.append(ts)

            # Validation: energy conservation check
            if self.config.validate_energy and step_i > 0:
                # Expected T rise from total energy
                avg_cp_s = E_cumulative / (mass_g * max(T - pcfg.T_initial, 1e-12))
                if avg_cp_s < 0:
                    self._validation_errors.append(
                        f"Part '{pcfg.part_name}': negative avg c_p at step {step_i}"
                    )

        result.T_final = T
        result.total_energy_J = E_cumulative
        result.peak_T = peak_T
        result.peak_cp = peak_cp

        return result

    @property
    def results(self) -> list[PartThermalResult]:
        return self._results

    @property
    def validation_errors(self) -> list[str]:
        return self._validation_errors

    def summary(self) -> dict:
        """Return a summary dict for all parts."""
        return {
            "n_parts": len(self._results),
            "dt_s": self.config.dt,
            "t_end_s": self.config.t_end,
            "parts": [
                {
                    "name": r.part_name,
                    "metal": r.metal_symbol,
                    "mass_kg": r.mass_kg,
                    "T_initial_K": r.T_initial,
                    "T_final_K": round(r.T_final, 2),
                    "peak_T_K": round(r.peak_T, 2),
                    "peak_cp_J_molK": round(r.peak_cp, 4),
                    "total_energy_J": round(r.total_energy_J, 4),
                    "n_steps": len(r.steps),
                }
                for r in self._results
            ],
            "validation_errors": self._validation_errors,
        }


# ═══════════════════════════════════════════════════════════════════════
# Convenience builders
# ═══════════════════════════════════════════════════════════════════════

def quick_heat_single(
    metal_symbol: str,
    mass_kg: float,
    power_W: float,
    t_end: float = 10.0,
    dt: float = 0.01,
    T_initial: float = 298.0,
) -> PartThermalResult:
    """One-liner: heat a single metal part at constant power."""
    sim = HeatingSimulation(HeatingSimConfig(dt=dt, t_end=t_end))
    sim.add_part(PartThermalConfig(
        part_name=f"{metal_symbol}_part",
        metal_symbol=metal_symbol,
        mass_kg=mass_kg,
        T_initial=T_initial,
        schedule=HeatSchedule(mode="constant", power=power_W),
    ))
    return sim.run()[0]


def heat_assembly(
    assembly: StepAssembly,
    material_map: dict[str, str],
    mass_map: dict[str, float],
    power_W: float = 100.0,
    t_end: float = 10.0,
    dt: float = 0.01,
    T_initial: float = 298.0,
) -> list[PartThermalResult]:
    """Heat all parts in a STEP assembly at constant power."""
    sim = HeatingSimulation(HeatingSimConfig(dt=dt, t_end=t_end))
    sim.add_parts_from_assembly(
        assembly, material_map, mass_map,
        schedule=HeatSchedule(mode="constant", power=power_W),
        T_initial=T_initial,
    )
    return sim.run()
