"""
calibration_shell.py -- Reusable Run Calibration System

Initializes simulation characteristics and variable defaults for the
multi-scale heating-loop simulation.  Links gas3 (pykernel.gas) and
pipe infrastructure (pykernel.pipe) with formation/degradation tracking
(pykernel.formation).

Design:
  RunConfig       ~30 variables covering geometry, thermal loops, gas,
                  formation, degradation thresholds, and solver tuning.
  CalibrationShell  Validates config, selects heating regime, builds
                    gas pipe + degradation tracker, returns SimSession.
  SimSession      Ready-to-run session with initialized state arrays,
                  step() method for time-advance, and summary export.

Regime logic:
  T_peak <= 1100 C  ->  MOLTEN_SALT   (FLiNaK / FLiBe)
  T_peak <= 1350 C  ->  HELIUM_GAS    (He enclosure)
  T_peak >  1350 C  ->  RADIATION     (direct radiative / plasma)

Variable count:  RunConfig has exactly 30 user-facing fields.
                 SimSession adds ~2 internal trackers.
                 Total state footprint: 32 variables.

Anti-black-box: every default is physically motivated and documented.

VSEPR-SIM 4.0.4 -- Day #50
"""

from __future__ import annotations

import math
import time
import logging
from dataclasses import dataclass, field
from typing import List, Dict, Optional, Tuple

_log = logging.getLogger(__name__)

# Late imports to avoid circular / heavy init issues
def _import_gas():
    import importlib, sys
    mod_name = 'pykernel.gas'
    if mod_name not in sys.modules:
        import importlib.util, pathlib
        p = pathlib.Path(__file__).parent / 'gas.py'
        spec = importlib.util.spec_from_file_location(mod_name, str(p))
        mod = importlib.util.module_from_spec(spec)
        sys.modules[mod_name] = mod
        spec.loader.exec_module(mod)
    return sys.modules[mod_name]

def _import_formation():
    import importlib, sys
    mod_name = 'pykernel.formation'
    if mod_name not in sys.modules:
        import importlib.util, pathlib
        p = pathlib.Path(__file__).parent / 'formation.py'
        spec = importlib.util.spec_from_file_location(mod_name, str(p))
        mod = importlib.util.module_from_spec(spec)
        sys.modules[mod_name] = mod
        spec.loader.exec_module(mod)
    return sys.modules[mod_name]


# =====================================================================
# RunConfig -- 30 user-facing initialization variables
# =====================================================================

@dataclass
class RunConfig:
    """Simulation initialization parameters (30 variables)."""

    # ── Identity ──────────────────────────────────────────────────── (2)
    run_name:       str   = ""            # auto-generated if empty
    description:    str   = ""

    # ── Geometry (pipe) ───────────────────────────────────────────── (3)
    pipe_length_m:    float = 1.0
    pipe_diameter_m:  float = 0.025
    pipe_roughness_m: float = 4.6e-5      # steel default

    # ── Primary loop (reactor) ────────────────────────────────────── (2)
    T_primary_in_C:   float = 600.0
    T_primary_out_C:  float = 750.0

    # ── Secondary loop (isolation) ────────────────────────────────── (2)
    T_secondary_in_C: float = 700.0
    T_secondary_out_C: float = 900.0

    # ── Tertiary loop (process heat) ──────────────────────────────── (2)
    T_tertiary_in_C:  float = 1000.0
    T_tertiary_out_C: float = 1200.0

    # ── Gas species + flow ────────────────────────────────────────── (4)
    gas_species:      str   = "He"        # default to helium
    gas_pressure_Pa:  float = 200000.0    # 2 bar
    gas_velocity_m_s: float = 15.0
    mass_flow_kg_s:   float = 0.01

    # ── Formation ─────────────────────────────────────────────────── (3)
    target_symbol:    str   = "Fe"
    target_lattice:   str   = "BCC"
    n_beads:          int   = 64

    # ── Degradation thresholds ────────────────────────────────────── (4)
    creep_strain_limit:    float = 0.01   # 1% strain -> failure
    corrosion_depth_limit: float = 500.0  # um
    crack_length_limit:    float = 0.001  # 1 mm
    diffusion_pen_limit:   float = 100.0  # um

    # ── Solver tuning ─────────────────────────────────────────────── (4)
    dt_s:             float = 1.0         # time step (seconds)
    max_steps:        int   = 1000
    T_peak_C:         float = 1200.0      # peak process temperature
    n_thermal_cycles: int   = 100

    # ── Export ────────────────────────────────────────────────────── (4)
    output_dir:       str   = "output/calibration_runs"
    csv_output:       bool  = True
    verbose:          bool  = False
    seed:             int   = 42

    def validate(self) -> List[str]:
        """Return list of validation errors (empty = OK)."""
        errs = []
        if self.pipe_length_m <= 0:   errs.append("pipe_length_m must be > 0")
        if self.pipe_diameter_m <= 0: errs.append("pipe_diameter_m must be > 0")
        if self.T_primary_in_C >= self.T_primary_out_C:
            errs.append("T_primary_in_C must be < T_primary_out_C")
        if self.dt_s <= 0: errs.append("dt_s must be > 0")
        if self.max_steps < 1: errs.append("max_steps must be >= 1")
        if self.n_beads < 1: errs.append("n_beads must be >= 1")
        gas = _import_gas()
        if self.gas_species not in gas.GAS_DB:
            errs.append(f"Unknown gas species: {self.gas_species}")
        return errs

    @property
    def field_count(self) -> int:
        """Number of user-facing config fields."""
        return 30


# =====================================================================
# SimSession -- initialized, ready-to-run state
# =====================================================================

@dataclass
class SimSession:
    """An initialized simulation session with state + stepping."""
    config:      RunConfig
    regime:      object = None           # HeatingRegime enum
    gas_pipe:    object = None           # gas.GasPipe instance
    formation:   object = None           # FormationRecord
    degradation: object = None           # DegradationTracker
    regime_watch: object = None          # RegimeTransition

    step_count:  int   = 0
    t_elapsed_s: float = 0.0
    history:     List[Dict] = field(default_factory=list)

    def step(self, n: int = 1) -> List[Dict]:
        """Advance n time steps. Returns list of per-step records."""
        form = _import_formation()
        gas  = _import_gas()
        records = []

        for _ in range(n):
            if self.step_count >= self.config.max_steps:
                break

            # Current temperature ramps linearly from primary_in to T_peak
            frac = self.step_count / max(self.config.max_steps - 1, 1)
            T_C = self.config.T_primary_in_C + frac * (self.config.T_peak_C - self.config.T_primary_in_C)
            T_K = T_C + form.KELVIN_OFFSET

            # Degradation advance
            deg_delta = self.degradation.advance_time(self.config.dt_s, T_K)

            # Thermal cycle every 10 steps
            cyc_delta = {}
            if self.step_count > 0 and self.step_count % 10 == 0:
                cyc_delta = self.degradation.advance_cycle()

            # Regime check
            new_regime = self.regime_watch.update(T_C, self.t_elapsed_s)

            # Gas pipe flow at current temperature
            flow = gas.compute_pipe_flow(
                self.config.gas_species,
                T_K=T_K,
                P_Pa=self.config.gas_pressure_Pa,
                velocity_m_s=self.config.gas_velocity_m_s,
                pipe_D_m=self.config.pipe_diameter_m,
                pipe_L_m=self.config.pipe_length_m,
                roughness_m=self.config.pipe_roughness_m,
            )

            rec = {
                'step':       self.step_count,
                't_s':        self.t_elapsed_s,
                'T_C':        T_C,
                'T_K':        T_K,
                'regime':     self.regime_watch.current_regime.name,
                'loop':       form.LoopPosition.classify(T_C).name,
                'dP_Pa':      flow.dP_Pa,
                'Re':         flow.Re,
                'mach':       flow.mach,
                'creep':      self.degradation.creep.strain,
                'corrosion':  self.degradation.corrosion.depth_um,
                'crack_m':    self.degradation.fatigue.crack_length_m,
                'diffusion':  self.degradation.diffusion.penetration_um,
                'severity':   self.degradation.severity_score(),
            }
            if new_regime:
                rec['regime_transition'] = new_regime.name

            records.append(rec)
            self.history.append(rec)

            self.step_count += 1
            self.t_elapsed_s += self.config.dt_s

        return records

    def is_failed(self) -> Tuple[bool, str]:
        """Check if any degradation limit is exceeded."""
        cfg = self.config
        d = self.degradation
        if d.creep.strain >= cfg.creep_strain_limit:
            return True, f"Creep strain {d.creep.strain:.4e} >= {cfg.creep_strain_limit}"
        if d.corrosion.depth_um >= cfg.corrosion_depth_limit:
            return True, f"Corrosion {d.corrosion.depth_um:.2f} um >= {cfg.corrosion_depth_limit}"
        if d.fatigue.crack_length_m >= cfg.crack_length_limit:
            return True, f"Crack {d.fatigue.crack_length_m:.4e} m >= {cfg.crack_length_limit}"
        if d.diffusion.penetration_um >= cfg.diffusion_pen_limit:
            return True, f"Diffusion {d.diffusion.penetration_um:.2f} um >= {cfg.diffusion_pen_limit}"
        return False, "OK"

    def summary(self) -> str:
        failed, reason = self.is_failed()
        lines = [
            f"=== SimSession: {self.config.run_name or '(unnamed)'} ===",
            f"Steps: {self.step_count}/{self.config.max_steps}  t={self.t_elapsed_s:.1f}s",
            f"Regime: {self.regime_watch.current_regime.name}  T_peak={self.config.T_peak_C:.0f}C",
            f"Gas: {self.config.gas_species}  v={self.config.gas_velocity_m_s} m/s",
            f"Formation: {self.config.target_symbol} ({self.config.target_lattice})",
            self.degradation.summary(),
            f"Failed: {failed}  ({reason})",
        ]
        if self.regime_watch.history:
            lines.append(f"Regime transitions: {len(self.regime_watch.history)}")
            for tr in self.regime_watch.history:
                lines.append(f"  {tr['from']} -> {tr['to']} at {tr['T_C']:.0f}C t={tr['t_s']:.1f}s")
        return "\n".join(lines)


# =====================================================================
# CalibrationShell -- builds SimSession from RunConfig
# =====================================================================

class CalibrationShell:
    """Reusable calibration system: validates, initializes, returns session."""

    @staticmethod
    def initialize(config: Optional[RunConfig] = None) -> SimSession:
        """Build a fully initialized SimSession from config."""
        if config is None:
            config = RunConfig()

        # Auto-name
        if not config.run_name:
            config.run_name = f"cal_{int(time.time())}"

        # Validate
        errs = config.validate()
        if errs:
            raise ValueError(f"RunConfig validation failed: {'; '.join(errs)}")

        gas  = _import_gas()
        form = _import_formation()

        # Regime classification
        regime = form.HeatingRegime.classify(config.T_peak_C)
        _log.info("Calibration: regime=%s  T_peak=%.0fC  gas=%s",
                  regime.name, config.T_peak_C, config.gas_species)

        # Gas pipe
        gp = gas.GasPipe(
            config.gas_species,
            P_Pa=config.gas_pressure_Pa,
            pipe_D_m=config.pipe_diameter_m,
            pipe_L_m=config.pipe_length_m,
            roughness_m=config.pipe_roughness_m,
        )

        # Formation record from reference dataset (if target exists)
        ref = {r.symbol: r for r in form.reference_dataset()}
        if config.target_symbol in ref:
            fr = ref[config.target_symbol]
        else:
            fr = form.FormationRecord(
                symbol=config.target_symbol,
                structure=form.LatticeClass[config.target_lattice]
                          if config.target_lattice in form.LatticeClass.__members__
                          else form.LatticeClass.UNKNOWN,
                n_beads=config.n_beads,
            )

        # Degradation tracker
        T_op_K = config.T_peak_C + form.KELVIN_OFFSET
        dt = form.DegradationTracker(
            component_id=f"{config.target_symbol}_pipe",
            T_operating_K=T_op_K,
        )

        # Regime watcher
        rw = form.RegimeTransition(current_regime=regime)

        return SimSession(
            config=config,
            regime=regime,
            gas_pipe=gp,
            formation=fr,
            degradation=dt,
            regime_watch=rw,
        )

    @staticmethod
    def quick_run(config: Optional[RunConfig] = None) -> SimSession:
        """Initialize + run all steps + return completed session."""
        session = CalibrationShell.initialize(config)
        session.step(session.config.max_steps)
        return session

    @staticmethod
    def preset_salt_loop() -> RunConfig:
        """Preset: molten salt loop at 900C peak."""
        return RunConfig(
            run_name="salt_loop_900C",
            T_peak_C=900.0,
            gas_species="N2",
            description="Molten salt primary loop, N2 cover gas",
        )

    @staticmethod
    def preset_helium_htgr() -> RunConfig:
        """Preset: helium HTGR loop at 1200C peak."""
        return RunConfig(
            run_name="helium_htgr_1200C",
            T_peak_C=1200.0,
            gas_species="He",
            description="Helium gas cooled reactor, ceramic HX",
        )

    @staticmethod
    def preset_radiation() -> RunConfig:
        """Preset: radiation regime at 1500C peak."""
        return RunConfig(
            run_name="radiation_1500C",
            T_peak_C=1500.0,
            gas_species="He",
            description="Direct radiative heating, plasma-facing",
        )