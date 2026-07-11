"""
formation.py -- Formation & Degradation tracking for multi-scale simulation.

Mirrors the C++ v4::FormationRecord and atomistic::kinetic patterns:
  - FormationRecord: per-case formation data (symbol, lattice, convergence, energy)
  - DegradationTracker: time-evolving material degradation (creep, corrosion, fatigue)
  - HeatingRegime: classifies operating temperature into salt / helium / radiation
  - RegimeTransition: detects when a system crosses regime boundaries

Physical basis:
  - Heating loops: primary 600-750C, secondary 700-900C, tertiary 1000-1200C
  - Salt limit ~1100C (corrosion spike, containment alloy failure)
  - Helium regime 1100-1350C (HTGR-class, ceramic heat exchangers)
  - Radiation/plasma >1350C (direct radiative, no fluid medium)

Degradation mechanisms:
  - Creep: Arrhenius-type exponential acceleration with temperature
  - Corrosion: salt-driven, peaks near regime boundary
  - Thermal fatigue: cycle-count driven crack initiation + propagation
  - Diffusion: boundary degradation across joints (Fick's law scaling)

Anti-black-box: every rate and threshold is inspectable.

VSEPR-SIM 4.0.4 — Day #50
"""

from __future__ import annotations

import math
import time
import logging
from dataclasses import dataclass, field
from typing import List, Optional, Dict
from enum import Enum, auto

_log = logging.getLogger(__name__)

# =====================================================================
# Constants
# =====================================================================

KELVIN_OFFSET = 273.15
R_GAS = 8.31446                # J/(mol K)

# Regime boundaries (Celsius)
SALT_MAX_C      = 1100.0       # molten salt practical ceiling
HELIUM_MAX_C    = 1350.0       # helium gas enclosure ceiling
# Above HELIUM_MAX_C -> radiation/plasma

# Loop temperature ranges (Celsius)
PRIMARY_RANGE   = (600.0, 750.0)
SECONDARY_RANGE = (700.0, 900.0)
TERTIARY_RANGE  = (1000.0, 1200.0)


# =====================================================================
# Heating Regime
# =====================================================================

class HeatingRegime(Enum):
    """Operating regime based on peak process temperature."""
    MOLTEN_SALT = auto()       # <= 1100 C
    HELIUM_GAS  = auto()       # 1100 - 1350 C
    RADIATION   = auto()       # > 1350 C

    @staticmethod
    def classify(T_celsius: float) -> 'HeatingRegime':
        if T_celsius <= SALT_MAX_C:
            return HeatingRegime.MOLTEN_SALT
        elif T_celsius <= HELIUM_MAX_C:
            return HeatingRegime.HELIUM_GAS
        else:
            return HeatingRegime.RADIATION


class LoopPosition(Enum):
    """Which heating loop a temperature falls into."""
    PRIMARY   = auto()         # 600 - 750 C (reactor)
    SECONDARY = auto()         # 700 - 900 C (isolation)
    TERTIARY  = auto()         # 1000 - 1200 C (process heat)
    BEYOND    = auto()         # > 1200 C

    @staticmethod
    def classify(T_celsius: float) -> 'LoopPosition':
        if T_celsius <= PRIMARY_RANGE[1]:
            return LoopPosition.PRIMARY
        elif T_celsius <= SECONDARY_RANGE[1]:
            return LoopPosition.SECONDARY
        elif T_celsius <= TERTIARY_RANGE[1]:
            return LoopPosition.TERTIARY
        else:
            return LoopPosition.BEYOND


# =====================================================================
# Lattice Class (mirrors v4::LatticeClass)
# =====================================================================

class LatticeClass(Enum):
    FCC     = 0
    BCC     = 1
    HCP     = 2
    UNKNOWN = 255


# =====================================================================
# Formation Record (mirrors v4::FormationRecord)
# =====================================================================

@dataclass
class FormationRecord:
    """One complete formation case — mirrors Day #47 spreadsheet + V4 meta."""
    symbol:       str   = ""
    name:         str   = ""
    structure:    LatticeClass = LatticeClass.UNKNOWN
    n_beads:      int   = 64

    # Convergence
    converged:    bool  = False
    steps:        int   = 0
    rms_force:    float = float('nan')

    # Formation averages
    avg_eta:      float = float('nan')
    avg_rho:      float = float('nan')
    avg_C:        float = float('nan')
    final_energy: float = float('nan')

    # Timing / domain
    elapsed_ms:   float = float('nan')
    n_l3_domains: int   = 0

    # Macro precursors
    macro_rigidity:  float = float('nan')
    macro_ductility: float = float('nan')
    macro_color:     float = float('nan')

    # Experimental reference
    Ecoh_eV:   float = float('nan')
    Tmelt_K:   float = float('nan')
    a0_ang:    float = float('nan')
    B_GPa:     float = float('nan')
    G_GPa:     float = float('nan')
    E_GPa:     float = float('nan')

    # V4 meta-scores
    gamma:       float = float('nan')
    data_quality: float = float('nan')
    compactness: float = float('nan')

    FIELD_COUNT: int = field(default=22, init=False, repr=False)

    def populated_fields(self) -> int:
        count = 0
        for v in [self.rms_force, self.avg_eta, self.avg_rho, self.avg_C,
                  self.final_energy, self.elapsed_ms,
                  self.macro_rigidity, self.macro_ductility, self.macro_color,
                  self.Ecoh_eV, self.Tmelt_K, self.a0_ang,
                  self.B_GPa, self.G_GPa, self.E_GPa]:
            if math.isfinite(v) and v != 0.0:
                count += 1
        if self.steps > 0:        count += 1
        if self.n_beads > 0:      count += 1
        if self.n_l3_domains > 0: count += 1
        count += 1  # converged is always populated
        if self.symbol: count += 1
        if self.name:   count += 1
        return count

    def is_scoreable(self) -> bool:
        return (bool(self.symbol)
                and self.structure != LatticeClass.UNKNOWN
                and self.n_beads > 0
                and self.steps > 0)


# =====================================================================
# Reference Dataset — 12 elements (mirrors v4::reference_dataset)
# =====================================================================

def reference_dataset() -> List[FormationRecord]:
    """Day #47 reference: 12 elements with experimental data."""
    def mk(sym, nm, lc, nb, conv, st, rms, eta, rho, C, energy, ms,
           l3, mr, md, mc, ecoh, tmelt, a0, B, G, E):
        return FormationRecord(
            symbol=sym, name=nm, structure=lc, n_beads=nb,
            converged=conv, steps=st, rms_force=rms,
            avg_eta=eta, avg_rho=rho, avg_C=C, final_energy=energy,
            elapsed_ms=ms, n_l3_domains=l3,
            macro_rigidity=mr, macro_ductility=md, macro_color=mc,
            Ecoh_eV=ecoh, Tmelt_K=tmelt, a0_ang=a0,
            B_GPa=B, G_GPa=G, E_GPa=E)

    LC = LatticeClass
    return [
        mk("Au","Gold",      LC.FCC,64,True, 1698,0.00745,0.279298,5.200771,25.65625,-10367.9,104.7, 1,0.165,0.4542,0.4737,-3.81,1337.3,4.078,180,27,79),
        mk("Ag","Silver",    LC.FCC,64,True,    6,0.0,0.000701,0.0,0.0,0.0,0.26,           0,0.0,0.0,0.0,-2.95,1234.9,4.086,103,30,83),
        mk("Cu","Copper",    LC.FCC,64,True,    5,0.0,0.000931,0.0,0.0,0.0,0.23,           0,0.0,0.0,0.0,-3.49,1357.8,3.615,142,48,130),
        mk("Pt","Platinum",  LC.FCC,64,False,6000,0.0,0.0,0.0,0.0,0.0,263.32,              7,0.1779,0.4327,0.3967,-5.84,2041.4,3.924,278,61,168),
        mk("Ni","Nickel",    LC.FCC,64,False,5000,0.0,0.0,0.0,0.0,0.0,216.6,               7,0.1776,0.3981,0.4516,-4.44,1728.0,3.524,180,76,200),
        mk("Al","Aluminium", LC.FCC,64,True,    5,0.000017,0.000637,0.0,0.0,-0.00077,0.21,  0,0.0,0.0,0.0,-3.39,933.5,4.05,76,26,70),
        mk("Fe","Iron",      LC.BCC,64,True, 2668,0.011844,0.469197,11.80109,45.34375,-15735.7,179.15,1,0.238,0.5048,0.6086,-4.28,1811.0,2.87,170,82,211),
        mk("W", "Tungsten",  LC.BCC,64,True, 2954,0.011784,0.494255,9.393497,40.09375,-33206.8,198.85,1,0.246,0.504,0.6159,-8.9,3695.0,3.165,310,161,411),
        mk("Mo","Molybdenum",LC.BCC,64,True, 2811,0.011542,0.454102,9.521529,40.3125,-25012.7,195.27,1,0.2292,0.4949,0.59,-6.82,2896.0,3.147,230,120,329),
        mk("Cr","Chromium",  LC.BCC,64,True, 2684,0.011993,0.467368,11.66662,45.0,-15065.4,178.89,  1,0.237,0.504,0.6069,-4.1,2180.0,2.885,160,115,279),
        mk("Ti","Titanium",  LC.HCP,64,True,   62,0.000013,0.000697,0.0,0.0,-0.00012,2.45,  0,0.0,0.0,0.0,-4.85,1941.0,2.95,110,44,116),
        mk("Co","Cobalt",    LC.HCP,64,False,6000,0.0,0.0,0.0,0.0,0.0,253.53,              5,0.0914,0.3787,0.3609,-4.39,1768.0,2.507,180,75,211),
    ]


# =====================================================================
# Degradation Mechanisms
# =====================================================================

@dataclass
class CreepState:
    """Arrhenius creep accumulation."""
    strain:       float = 0.0
    rate_per_s:   float = 0.0
    Q_activation: float = 250000.0   # J/mol (typical Ni-alloy)
    A_prefactor:  float = 1e10       # 1/s
    stress_MPa:   float = 100.0
    n_exponent:   float = 3.0        # Norton power-law exponent

    def update_rate(self, T_kelvin: float) -> float:
        """Compute current creep rate (1/s) at temperature T."""
        self.rate_per_s = (self.A_prefactor
                          * (self.stress_MPa ** self.n_exponent)
                          * math.exp(-self.Q_activation / (R_GAS * T_kelvin)))
        return self.rate_per_s

    def advance(self, T_kelvin: float, dt_s: float) -> float:
        """Accumulate creep strain over dt seconds."""
        self.update_rate(T_kelvin)
        d_strain = self.rate_per_s * dt_s
        self.strain += d_strain
        return d_strain


@dataclass
class CorrosionState:
    """Salt-driven corrosion rate model."""
    depth_um:        float = 0.0
    base_rate_um_yr: float = 25.0    # um/year at reference T
    T_ref_K:         float = 973.15  # 700 C reference
    E_corr:          float = 80000.0 # J/mol activation energy

    def rate_um_per_s(self, T_kelvin: float) -> float:
        """Instantaneous corrosion rate at T."""
        yr_s = 365.25 * 86400.0
        base_per_s = self.base_rate_um_yr / yr_s
        return base_per_s * math.exp(self.E_corr / R_GAS * (1.0/self.T_ref_K - 1.0/T_kelvin))

    def advance(self, T_kelvin: float, dt_s: float) -> float:
        """Accumulate corrosion depth over dt seconds."""
        rate = self.rate_um_per_s(T_kelvin)
        d_depth = rate * dt_s
        self.depth_um += d_depth
        return d_depth


@dataclass
class FatigueState:
    """Thermal cycling fatigue — crack initiation and propagation."""
    cycles:        int   = 0
    crack_length_m: float = 0.0
    delta_K:       float = 10.0     # MPa*sqrt(m) stress intensity range
    C_paris:       float = 1e-11    # Paris law coefficient
    m_paris:       float = 3.0      # Paris law exponent
    N_initiation:  int   = 1000     # cycles to crack initiation

    def advance_cycle(self) -> float:
        """Advance one thermal cycle, return crack growth (m)."""
        self.cycles += 1
        if self.cycles < self.N_initiation:
            return 0.0
        da = self.C_paris * (self.delta_K ** self.m_paris)
        self.crack_length_m += da
        return da

    @property
    def initiated(self) -> bool:
        return self.cycles >= self.N_initiation


@dataclass
class DiffusionState:
    """Inter-diffusion across joints (Fick's law scaling)."""
    penetration_um: float = 0.0
    D0:            float = 1e-4     # m^2/s pre-exponential
    Q_diff:        float = 200000.0 # J/mol activation energy

    def diffusivity(self, T_kelvin: float) -> float:
        """Effective diffusion coefficient at T (m^2/s)."""
        return self.D0 * math.exp(-self.Q_diff / (R_GAS * T_kelvin))

    def advance(self, T_kelvin: float, dt_s: float) -> float:
        """Advance diffusion penetration (um) over dt seconds."""
        D = self.diffusivity(T_kelvin)
        # sqrt(2*D*dt) penetration depth
        d_pen = math.sqrt(2.0 * D * dt_s) * 1e6  # m -> um
        self.penetration_um += d_pen
        return d_pen


# =====================================================================
# Degradation Tracker — unified multi-mechanism
# =====================================================================

@dataclass
class DegradationTracker:
    """Tracks all degradation mechanisms for a single component."""
    component_id:  str = "default"
    T_operating_K: float = 973.15    # current operating temperature

    creep:     CreepState     = field(default_factory=CreepState)
    corrosion: CorrosionState = field(default_factory=CorrosionState)
    fatigue:   FatigueState   = field(default_factory=FatigueState)
    diffusion: DiffusionState = field(default_factory=DiffusionState)

    total_time_s:  float = 0.0
    total_cycles:  int   = 0

    def advance_time(self, dt_s: float, T_kelvin: Optional[float] = None) -> Dict[str, float]:
        """Advance all time-dependent mechanisms by dt seconds."""
        T = T_kelvin if T_kelvin is not None else self.T_operating_K
        self.total_time_s += dt_s
        return {
            'creep_strain':     self.creep.advance(T, dt_s),
            'corrosion_um':     self.corrosion.advance(T, dt_s),
            'diffusion_um':     self.diffusion.advance(T, dt_s),
        }

    def advance_cycle(self) -> Dict[str, float]:
        """Advance one thermal cycle."""
        self.total_cycles += 1
        return {
            'crack_growth_m': self.fatigue.advance_cycle(),
        }

    def severity_score(self) -> float:
        """0-1 composite severity: higher = worse."""
        s = 0.0
        # Creep: 1% strain is serious
        s += min(self.creep.strain / 0.01, 1.0) * 0.3
        # Corrosion: 500 um is wall-thinning limit
        s += min(self.corrosion.depth_um / 500.0, 1.0) * 0.3
        # Fatigue: 1mm crack is critical
        s += min(self.fatigue.crack_length_m / 0.001, 1.0) * 0.25
        # Diffusion: 100 um penetration is concerning
        s += min(self.diffusion.penetration_um / 100.0, 1.0) * 0.15
        return min(s, 1.0)

    def summary(self) -> str:
        regime = HeatingRegime.classify(self.T_operating_K - KELVIN_OFFSET)
        lines = [
            f"Degradation: {self.component_id}  regime={regime.name}",
            f"  T_op = {self.T_operating_K:.1f} K ({self.T_operating_K - KELVIN_OFFSET:.0f} C)",
            f"  time = {self.total_time_s:.1f} s  cycles = {self.total_cycles}",
            f"  creep strain    = {self.creep.strain:.6e}  rate = {self.creep.rate_per_s:.4e} /s",
            f"  corrosion depth = {self.corrosion.depth_um:.4f} um",
            f"  fatigue cycles  = {self.fatigue.cycles}  crack = {self.fatigue.crack_length_m:.4e} m",
            f"  diffusion pen   = {self.diffusion.penetration_um:.4f} um",
            f"  severity        = {self.severity_score():.4f}",
        ]
        return "\n".join(lines)


# =====================================================================
# Regime Transition Detector
# =====================================================================

@dataclass
class RegimeTransition:
    """Detects and logs regime boundary crossings."""
    current_regime: HeatingRegime = HeatingRegime.MOLTEN_SALT
    history: List[Dict] = field(default_factory=list)

    def update(self, T_celsius: float, t_seconds: float = 0.0) -> Optional[HeatingRegime]:
        """Check for regime change. Returns new regime if transition occurred."""
        new_regime = HeatingRegime.classify(T_celsius)
        if new_regime != self.current_regime:
            self.history.append({
                'from': self.current_regime.name,
                'to':   new_regime.name,
                'T_C':  T_celsius,
                't_s':  t_seconds,
            })
            _log.info("Regime transition: %s -> %s at %.0f C",
                      self.current_regime.name, new_regime.name, T_celsius)
            self.current_regime = new_regime
            return new_regime
        return None