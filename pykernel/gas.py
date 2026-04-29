"""
gas.py -- Gas thermodynamics and pipe-flow calculations.

Ideal and real gas properties, mixture rules, compressibility,
and Darcy-Weisbach pipe-flow pressure drop integrated with the
Pipe[T] infrastructure from pykernel.pipe.

Covers:
  - Ideal gas law  (PV = nRT)
  - Van der Waals  (P + a/V^2)(V - b) = RT
  - Gas mixture averaging (Kay's rule)
  - Compressibility factor Z (Pitzer correlation)
  - Isentropic relations
  - Darcy-Weisbach pressure drop
  - Moody friction factor (Colebrook-White)
  - GasPipe streaming class

Anti-black-box: every intermediate calculation is traceable.

VSEPR-SIM 4.0.4
"""

from __future__ import annotations

import math
import time
import logging
from dataclasses import dataclass, field
from typing import Dict, List, Optional

_log = logging.getLogger(__name__)

# =====================================================================
# Constants
# =====================================================================

R_UNIVERSAL = 8.31446          # J/(mol K)
ATM_PA      = 101325.0         # Pa per atm
GRAVITY     = 9.80665          # m/s^2


# =====================================================================
# Gas species database
# =====================================================================

@dataclass(frozen=True)
class GasSpecies:
    """Thermophysical properties of a pure gas species."""
    name: str
    symbol: str
    molar_mass: float          # g/mol
    cp_molar: float            # J/(mol K) at 298 K, 1 atm
    cv_molar: float            # J/(mol K)
    gamma: float               # cp/cv
    mu_Pa_s: float             # dynamic viscosity Pa s at 298 K
    Tc_K: float                # critical temperature K
    Pc_Pa: float               # critical pressure Pa
    omega: float               # acentric factor (Pitzer)
    vdw_a: float               # van der Waals a  (Pa m^6/mol^2)
    vdw_b: float               # van der Waals b  (m^3/mol)

    @property
    def cp_specific(self) -> float:
        """J/(g K)"""
        return self.cp_molar / self.molar_mass

    @property
    def cv_specific(self) -> float:
        """J/(g K)"""
        return self.cv_molar / self.molar_mass

    @property
    def R_specific(self) -> float:
        """J/(g K) -- specific gas constant."""
        return R_UNIVERSAL / self.molar_mass


GAS_DB: Dict[str, GasSpecies] = {
    "N2": GasSpecies(
        name="Nitrogen", symbol="N2", molar_mass=28.014,
        cp_molar=29.12, cv_molar=20.81, gamma=1.400,
        mu_Pa_s=1.76e-5, Tc_K=126.2, Pc_Pa=3.394e6,
        omega=0.040, vdw_a=0.1370, vdw_b=3.87e-5,
    ),
    "O2": GasSpecies(
        name="Oxygen", symbol="O2", molar_mass=32.00,
        cp_molar=29.38, cv_molar=21.07, gamma=1.395,
        mu_Pa_s=2.04e-5, Tc_K=154.6, Pc_Pa=5.043e6,
        omega=0.022, vdw_a=0.1378, vdw_b=3.18e-5,
    ),
    "H2": GasSpecies(
        name="Hydrogen", symbol="H2", molar_mass=2.016,
        cp_molar=28.84, cv_molar=20.53, gamma=1.405,
        mu_Pa_s=0.88e-5, Tc_K=33.2, Pc_Pa=1.297e6,
        omega=-0.220, vdw_a=0.0245, vdw_b=2.65e-5,
    ),
    "He": GasSpecies(
        name="Helium", symbol="He", molar_mass=4.003,
        cp_molar=20.79, cv_molar=12.47, gamma=1.667,
        mu_Pa_s=1.96e-5, Tc_K=5.19, Pc_Pa=0.227e6,
        omega=-0.390, vdw_a=0.00346, vdw_b=2.38e-5,
    ),
    "CO2": GasSpecies(
        name="Carbon dioxide", symbol="CO2", molar_mass=44.01,
        cp_molar=37.13, cv_molar=28.82, gamma=1.289,
        mu_Pa_s=1.47e-5, Tc_K=304.2, Pc_Pa=7.382e6,
        omega=0.228, vdw_a=0.3658, vdw_b=4.29e-5,
    ),
    "H2O": GasSpecies(
        name="Water vapour", symbol="H2O", molar_mass=18.015,
        cp_molar=33.58, cv_molar=25.27, gamma=1.329,
        mu_Pa_s=0.97e-5, Tc_K=647.1, Pc_Pa=22.064e6,
        omega=0.344, vdw_a=0.5536, vdw_b=3.05e-5,
    ),
    "Ar": GasSpecies(
        name="Argon", symbol="Ar", molar_mass=39.948,
        cp_molar=20.79, cv_molar=12.47, gamma=1.667,
        mu_Pa_s=2.23e-5, Tc_K=150.9, Pc_Pa=4.898e6,
        omega=0.000, vdw_a=0.1355, vdw_b=3.22e-5,
    ),
    "CH4": GasSpecies(
        name="Methane", symbol="CH4", molar_mass=16.043,
        cp_molar=35.69, cv_molar=27.38, gamma=1.303,
        mu_Pa_s=1.10e-5, Tc_K=190.6, Pc_Pa=4.604e6,
        omega=0.011, vdw_a=0.2303, vdw_b=4.31e-5,
    ),
    "NH3": GasSpecies(
        name="Ammonia", symbol="NH3", molar_mass=17.031,
        cp_molar=35.06, cv_molar=26.75, gamma=1.310,
        mu_Pa_s=0.98e-5, Tc_K=405.5, Pc_Pa=11.353e6,
        omega=0.256, vdw_a=0.4233, vdw_b=3.73e-5,
    ),
    "UF6": GasSpecies(
        name="Uranium hexafluoride", symbol="UF6", molar_mass=352.02,
        cp_molar=129.6, cv_molar=121.3, gamma=1.068,
        mu_Pa_s=1.70e-5, Tc_K=505.8, Pc_Pa=4.66e6,
        omega=0.648, vdw_a=2.050, vdw_b=1.12e-4,
    ),
}


def lookup_gas(symbol: str) -> Optional[GasSpecies]:
    return GAS_DB.get(symbol)


# =====================================================================
# Ideal gas
# =====================================================================

def ideal_pressure(n_mol: float, T_K: float, V_m3: float) -> float:
    """P = nRT/V  [Pa]"""
    return n_mol * R_UNIVERSAL * T_K / V_m3


def ideal_volume(n_mol: float, T_K: float, P_Pa: float) -> float:
    """V = nRT/P  [m^3]"""
    return n_mol * R_UNIVERSAL * T_K / P_Pa


def ideal_density(gas: GasSpecies, T_K: float, P_Pa: float) -> float:
    """rho = P M / (R T)  [kg/m^3]"""
    return P_Pa * (gas.molar_mass / 1000.0) / (R_UNIVERSAL * T_K)


# =====================================================================
# Van der Waals
# =====================================================================

def vdw_pressure(gas: GasSpecies, T_K: float, Vm_m3: float) -> float:
    """Van der Waals pressure for molar volume Vm [Pa]."""
    return R_UNIVERSAL * T_K / (Vm_m3 - gas.vdw_b) - gas.vdw_a / (Vm_m3 ** 2)


def vdw_solve_volume(gas: GasSpecies, T_K: float, P_Pa: float,
                     tol: float = 1e-10, max_iter: int = 200) -> float:
    """Solve VdW for molar volume via Newton-Raphson [m^3/mol]."""
    Vm = R_UNIVERSAL * T_K / P_Pa  # ideal guess
    for _ in range(max_iter):
        f = (P_Pa + gas.vdw_a / Vm**2) * (Vm - gas.vdw_b) - R_UNIVERSAL * T_K
        dfdVm = P_Pa - gas.vdw_a / Vm**2 + 2.0 * gas.vdw_a * (Vm - gas.vdw_b) / Vm**3
        dVm = -f / dfdVm
        Vm += dVm
        if abs(dVm) < tol:
            break
    return Vm


# =====================================================================
# Compressibility factor Z  (Pitzer correlation)
# =====================================================================

def compressibility_factor(gas: GasSpecies, T_K: float, P_Pa: float) -> float:
    """Z = PV/(nRT) via the Pitzer correlation Z = Z0 + omega*Z1."""
    Tr = T_K / gas.Tc_K
    Pr = P_Pa / gas.Pc_Pa
    # Simple truncated virial: B0, B1 from Pitzer-Curl
    B0 = 0.083 - 0.422 / Tr**1.6
    B1 = 0.139 - 0.172 / Tr**4.2
    Z = 1.0 + (B0 + gas.omega * B1) * Pr / Tr
    return Z


# =====================================================================
# Gas mixtures (Kay's rule)
# =====================================================================

@dataclass
class GasMixture:
    """Gas mixture with mole fractions."""
    components: Dict[str, float]   # symbol -> mole fraction

    def __post_init__(self):
        total = sum(self.components.values())
        if abs(total - 1.0) > 1e-6:
            _log.warning("GasMixture fractions sum to %.6f, normalising", total)
            self.components = {k: v / total for k, v in self.components.items()}

    @property
    def molar_mass(self) -> float:
        return sum(y * GAS_DB[s].molar_mass for s, y in self.components.items())

    @property
    def cp_molar(self) -> float:
        return sum(y * GAS_DB[s].cp_molar for s, y in self.components.items())

    @property
    def gamma(self) -> float:
        cp = self.cp_molar
        cv = sum(y * GAS_DB[s].cv_molar for s, y in self.components.items())
        return cp / cv if cv > 0 else 1.4

    @property
    def pseudo_Tc(self) -> float:
        """Kay's rule pseudo-critical temperature [K]."""
        return sum(y * GAS_DB[s].Tc_K for s, y in self.components.items())

    @property
    def pseudo_Pc(self) -> float:
        """Kay's rule pseudo-critical pressure [Pa]."""
        return sum(y * GAS_DB[s].Pc_Pa for s, y in self.components.items())

    def density(self, T_K: float, P_Pa: float) -> float:
        """Mixture density [kg/m^3] via ideal gas."""
        return P_Pa * (self.molar_mass / 1000.0) / (R_UNIVERSAL * T_K)

    def viscosity(self) -> float:
        """Wilke-approximate mixture viscosity [Pa s]."""
        return sum(y * GAS_DB[s].mu_Pa_s for s, y in self.components.items())


# =====================================================================
# Isentropic relations
# =====================================================================

def isentropic_T_ratio(gamma: float, M: float) -> float:
    """T0/T = 1 + (gamma-1)/2 * M^2"""
    return 1.0 + (gamma - 1.0) / 2.0 * M**2


def isentropic_P_ratio(gamma: float, M: float) -> float:
    """P0/P = (T0/T)^(gamma/(gamma-1))"""
    return isentropic_T_ratio(gamma, M) ** (gamma / (gamma - 1.0))


def speed_of_sound(gas: GasSpecies, T_K: float) -> float:
    """a = sqrt(gamma R T / M)  [m/s]"""
    return math.sqrt(gas.gamma * R_UNIVERSAL * T_K / (gas.molar_mass / 1000.0))


def mach_number(velocity_m_s: float, gas: GasSpecies, T_K: float) -> float:
    return velocity_m_s / speed_of_sound(gas, T_K)


# =====================================================================
# Pipe flow: Darcy-Weisbach + Colebrook-White
# =====================================================================

def reynolds_number(rho: float, v: float, D: float, mu: float) -> float:
    """Re = rho v D / mu"""
    return rho * v * D / mu


def colebrook_white(Re: float, eps_D: float, tol: float = 1e-8,
                    max_iter: int = 50) -> float:
    """Solve 1/sqrt(f) = -2 log10(eps_D/3.7 + 2.51/(Re sqrt(f))) iteratively."""
    if Re < 2300:
        return 64.0 / Re if Re > 0 else 0.0
    # Swamee-Jain initial guess
    f = 0.25 / (math.log10(eps_D / 3.7 + 5.74 / Re**0.9))**2
    for _ in range(max_iter):
        rhs = -2.0 * math.log10(eps_D / 3.7 + 2.51 / (Re * math.sqrt(f)))
        f_new = 1.0 / rhs**2
        if abs(f_new - f) < tol:
            return f_new
        f = f_new
    return f


def darcy_weisbach_dp(f: float, L: float, D: float,
                      rho: float, v: float) -> float:
    """Pressure drop dP = f (L/D) (rho v^2 / 2)  [Pa]."""
    return f * (L / D) * rho * v**2 / 2.0


@dataclass
class PipeFlowResult:
    """Result of a single pipe-flow calculation."""
    gas_symbol: str
    T_K: float
    P_in_Pa: float
    velocity_m_s: float
    pipe_D_m: float
    pipe_L_m: float
    roughness_m: float
    Re: float
    friction_f: float
    dP_Pa: float
    rho_kg_m3: float
    mach: float
    mass_flow_kg_s: float

    @property
    def dP_kPa(self) -> float:
        return self.dP_Pa / 1000.0

    @property
    def dP_atm(self) -> float:
        return self.dP_Pa / ATM_PA

    def as_dict(self) -> Dict:
        return {
            "gas": self.gas_symbol,
            "T_K": round(self.T_K, 2),
            "P_in_Pa": round(self.P_in_Pa, 0),
            "v_m_s": round(self.velocity_m_s, 3),
            "D_m": self.pipe_D_m,
            "L_m": self.pipe_L_m,
            "Re": round(self.Re, 0),
            "f": round(self.friction_f, 6),
            "dP_Pa": round(self.dP_Pa, 2),
            "dP_kPa": round(self.dP_kPa, 4),
            "rho_kg_m3": round(self.rho_kg_m3, 4),
            "Mach": round(self.mach, 4),
            "mdot_kg_s": round(self.mass_flow_kg_s, 6),
        }


def compute_pipe_flow(
    gas_symbol: str,
    T_K: float = 298.0,
    P_Pa: float = ATM_PA,
    velocity_m_s: float = 10.0,
    pipe_D_m: float = 0.1,
    pipe_L_m: float = 100.0,
    roughness_m: float = 4.5e-5,
) -> PipeFlowResult:
    """Full pipe-flow calculation for a single gas species."""
    gas = GAS_DB[gas_symbol]
    rho = ideal_density(gas, T_K, P_Pa)
    Re = reynolds_number(rho, velocity_m_s, pipe_D_m, gas.mu_Pa_s)
    eps_D = roughness_m / pipe_D_m
    f = colebrook_white(Re, eps_D)
    dP = darcy_weisbach_dp(f, pipe_L_m, pipe_D_m, rho, velocity_m_s)
    Ma = mach_number(velocity_m_s, gas, T_K)
    A = math.pi * (pipe_D_m / 2.0)**2
    mdot = rho * velocity_m_s * A
    return PipeFlowResult(
        gas_symbol=gas_symbol, T_K=T_K, P_in_Pa=P_Pa,
        velocity_m_s=velocity_m_s, pipe_D_m=pipe_D_m, pipe_L_m=pipe_L_m,
        roughness_m=roughness_m, Re=Re, friction_f=f, dP_Pa=dP,
        rho_kg_m3=rho, mach=Ma, mass_flow_kg_s=mdot,
    )


# =====================================================================
# GasPipe: streaming pipe-flow integrated with Pipe[T]
# =====================================================================

class GasPipe:
    """
    Runs a parametric sweep of pipe-flow conditions and streams results
    through a Pipe[dict] for downstream consumption (CSV, JSON, etc.).

    Usage:
        from pykernel.pipe import Pipe, CSVSink
        from pykernel.gas import GasPipe

        gp = GasPipe("N2", pipe_D_m=0.05, pipe_L_m=50.0)
        results = gp.sweep_velocity(v_range=[1, 5, 10, 20, 50])
    """

    def __init__(self, gas_symbol: str, T_K: float = 298.0,
                 P_Pa: float = ATM_PA, pipe_D_m: float = 0.1,
                 pipe_L_m: float = 100.0, roughness_m: float = 4.5e-5):
        self.gas_symbol = gas_symbol
        self.T_K = T_K
        self.P_Pa = P_Pa
        self.pipe_D_m = pipe_D_m
        self.pipe_L_m = pipe_L_m
        self.roughness_m = roughness_m
        self._results: List[PipeFlowResult] = []

    def sweep_velocity(self, v_range: List[float]) -> List[PipeFlowResult]:
        """Run flow calc for each velocity in the list."""
        results = []
        for v in v_range:
            r = compute_pipe_flow(
                self.gas_symbol, self.T_K, self.P_Pa, v,
                self.pipe_D_m, self.pipe_L_m, self.roughness_m,
            )
            results.append(r)
        self._results.extend(results)
        return results

    def sweep_temperature(self, T_range: List[float],
                          velocity: float = 10.0) -> List[PipeFlowResult]:
        results = []
        for T in T_range:
            r = compute_pipe_flow(
                self.gas_symbol, T, self.P_Pa, velocity,
                self.pipe_D_m, self.pipe_L_m, self.roughness_m,
            )
            results.append(r)
        self._results.extend(results)
        return results

    def sweep_gases(self, symbols: List[str],
                    velocity: float = 10.0) -> List[PipeFlowResult]:
        results = []
        for sym in symbols:
            r = compute_pipe_flow(
                sym, self.T_K, self.P_Pa, velocity,
                self.pipe_D_m, self.pipe_L_m, self.roughness_m,
            )
            results.append(r)
        self._results.extend(results)
        return results

    @property
    def results(self) -> List[PipeFlowResult]:
        return self._results

    def summary_table(self) -> str:
        """Format results as a text table."""
        if not self._results:
            return "(no results)"
        hdr = f"{'Gas':>5} {'v m/s':>8} {'Re':>12} {'f':>10} {'dP kPa':>10} {'Mach':>8}"
        lines = [hdr, "-" * len(hdr)]
        for r in self._results:
            lines.append(
                f"{r.gas_symbol:>5} {r.velocity_m_s:>8.2f} {r.Re:>12.0f} "
                f"{r.friction_f:>10.6f} {r.dP_kPa:>10.4f} {r.mach:>8.4f}"
            )
        return "\n".join(lines)
