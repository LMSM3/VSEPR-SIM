"""
helium_tables.py -- Comprehensive Helium Steam Tables

Generates property tables for helium across:
  - Ambient temperatures (200 K - 400 K)
  - Reactor temperatures (600 K - 1500 K)
  - Multiple pressures (0.1 atm - 100 atm)
  - Multiple volumes (0.001 m^3 - 1000 m^3)

Properties computed per state point:
  P, T, V, rho, Z, Cp, Cv, gamma, a (sound speed), mu,
  h (specific enthalpy), s (specific entropy approx),
  VdW molar volume, ideal molar volume

Uses pykernel.gas He species data and EOS functions.

Anti-black-box: every value traceable to fundamental relations.

VSEPR-SIM 4.0.4 -- Day #50
"""

from __future__ import annotations

import math
import csv
import io
import logging
from dataclasses import dataclass, field
from typing import List, Dict, Optional, Tuple

_log = logging.getLogger(__name__)

# ── Import gas module (direct-load safe) ──────────────────────────────
def _gas():
    import sys, importlib.util, pathlib
    name = 'pykernel.gas'
    if name in sys.modules:
        return sys.modules[name]
    p = pathlib.Path(__file__).parent / 'gas.py'
    spec = importlib.util.spec_from_file_location(name, str(p))
    mod = importlib.util.module_from_spec(spec)
    sys.modules[name] = mod
    spec.loader.exec_module(mod)
    return mod

# =====================================================================
# Constants
# =====================================================================

R = 8.31446          # J/(mol K)
ATM = 101325.0       # Pa
HE_M = 4.003e-3      # kg/mol

# Helium thermal conductivity correlation (W/(m K))
# k_He ~ 0.1513 * (T/300)^0.67  (Chapman-Enskog approx for monatomic)
def he_thermal_conductivity(T_K: float) -> float:
    return 0.1513 * (T_K / 300.0) ** 0.67

# Helium viscosity correlation (Pa s)
# mu ~ 1.96e-5 * (T/298)^0.67
def he_viscosity(T_K: float) -> float:
    return 1.96e-5 * (T_K / 298.0) ** 0.67

# Helium Cp is essentially constant (monatomic ideal): 5/2 R per mol
HE_CP_MOLAR = 20.79   # J/(mol K)
HE_CV_MOLAR = 12.47   # J/(mol K)
HE_GAMMA    = 5.0 / 3.0


# =====================================================================
# Single state-point calculation
# =====================================================================

@dataclass
class HeliumStatePoint:
    """Complete thermodynamic state for helium at one (T, P) point."""
    T_K:       float
    P_Pa:      float
    # Derived
    rho_kg_m3:      float = 0.0
    Vm_ideal_m3:    float = 0.0
    Vm_vdw_m3:      float = 0.0
    Z:              float = 1.0
    cp_J_kgK:       float = 0.0
    cv_J_kgK:       float = 0.0
    gamma:          float = 0.0
    a_m_s:          float = 0.0    # speed of sound
    mu_Pa_s:        float = 0.0
    k_W_mK:         float = 0.0    # thermal conductivity
    h_kJ_kg:        float = 0.0    # specific enthalpy (ref 0 K)
    Pr:             float = 0.0    # Prandtl number

    def header(self) -> str:
        return (f"{'T(K)':>8} {'P(atm)':>9} {'P(Pa)':>12} "
                f"{'rho':>10} {'Z':>8} {'Vm_id':>12} {'Vm_vdw':>12} "
                f"{'Cp':>9} {'Cv':>9} {'gamma':>7} "
                f"{'a(m/s)':>9} {'mu(uPa.s)':>10} {'k(W/mK)':>9} "
                f"{'h(kJ/kg)':>10} {'Pr':>8}")

    def row(self) -> str:
        return (f"{self.T_K:8.1f} {self.P_Pa/ATM:9.3f} {self.P_Pa:12.0f} "
                f"{self.rho_kg_m3:10.4f} {self.Z:8.5f} "
                f"{self.Vm_ideal_m3:12.6e} {self.Vm_vdw_m3:12.6e} "
                f"{self.cp_J_kgK:9.1f} {self.cv_J_kgK:9.1f} {self.gamma:7.4f} "
                f"{self.a_m_s:9.1f} {self.mu_Pa_s*1e6:10.3f} {self.k_W_mK:9.5f} "
                f"{self.h_kJ_kg:10.2f} {self.Pr:8.4f}")

    def as_dict(self) -> Dict:
        return {
            'T_K': self.T_K, 'P_Pa': self.P_Pa, 'P_atm': self.P_Pa / ATM,
            'rho_kg_m3': self.rho_kg_m3, 'Z': self.Z,
            'Vm_ideal_m3_mol': self.Vm_ideal_m3, 'Vm_vdw_m3_mol': self.Vm_vdw_m3,
            'cp_J_kgK': self.cp_J_kgK, 'cv_J_kgK': self.cv_J_kgK,
            'gamma': self.gamma, 'a_m_s': self.a_m_s,
            'mu_Pa_s': self.mu_Pa_s, 'k_W_mK': self.k_W_mK,
            'h_kJ_kg': self.h_kJ_kg, 'Pr': self.Pr,
        }


def compute_state(T_K: float, P_Pa: float) -> HeliumStatePoint:
    """Compute all helium properties at (T, P)."""
    gas = _gas()
    He = gas.GAS_DB['He']

    sp = HeliumStatePoint(T_K=T_K, P_Pa=P_Pa)

    # Density
    sp.rho_kg_m3 = gas.ideal_density(He, T_K, P_Pa)

    # Molar volumes
    sp.Vm_ideal_m3 = R * T_K / P_Pa
    sp.Vm_vdw_m3 = gas.vdw_solve_volume(He, T_K, P_Pa)

    # Compressibility
    sp.Z = gas.compressibility_factor(He, T_K, P_Pa)

    # Heat capacities (constant for monatomic He)
    sp.cp_J_kgK = HE_CP_MOLAR / HE_M   # J/(kg K)
    sp.cv_J_kgK = HE_CV_MOLAR / HE_M
    sp.gamma = HE_GAMMA

    # Speed of sound
    sp.a_m_s = gas.speed_of_sound(He, T_K)

    # Transport
    sp.mu_Pa_s = he_viscosity(T_K)
    sp.k_W_mK = he_thermal_conductivity(T_K)

    # Specific enthalpy relative to 0 K (h = Cp * T for ideal gas)
    sp.h_kJ_kg = sp.cp_J_kgK * T_K / 1000.0

    # Prandtl number
    sp.Pr = sp.mu_Pa_s * sp.cp_J_kgK / sp.k_W_mK if sp.k_W_mK > 0 else 0.0

    return sp


# =====================================================================
# Table generators
# =====================================================================

def ambient_table(
    T_range: Tuple[float,...] = (200, 220, 240, 260, 280, 298, 300, 320, 340, 360, 380, 400),
    P_atm_range: Tuple[float,...] = (0.1, 0.5, 1.0, 2.0, 5.0, 10.0),
) -> List[HeliumStatePoint]:
    """Generate helium property table at ambient temperatures."""
    results = []
    for T in T_range:
        for P_atm in P_atm_range:
            results.append(compute_state(T, P_atm * ATM))
    return results


def reactor_table(
    T_range: Tuple[float,...] = (600, 700, 800, 900, 1000, 1100, 1200, 1300, 1400, 1500),
    P_atm_range: Tuple[float,...] = (1.0, 5.0, 10.0, 20.0, 50.0, 70.0, 100.0),
) -> List[HeliumStatePoint]:
    """Generate helium property table at reactor temperatures."""
    results = []
    for T in T_range:
        for P_atm in P_atm_range:
            results.append(compute_state(T, P_atm * ATM))
    return results


def volume_table(
    volumes_m3: Tuple[float,...] = (0.001, 0.01, 0.1, 1.0, 10.0, 100.0, 1000.0),
    n_mol: float = 1.0,
    T_range: Tuple[float,...] = (300, 600, 900, 1200, 1500),
) -> List[Dict]:
    """
    For a fixed amount of He (n_mol) in various container volumes,
    compute the resulting pressure and all properties.
    """
    results = []
    for T in T_range:
        for V in volumes_m3:
            P = n_mol * R * T / V
            sp = compute_state(T, P)
            rec = sp.as_dict()
            rec['V_m3'] = V
            rec['n_mol'] = n_mol
            results.append(rec)
    return results


# =====================================================================
# Printing + CSV export
# =====================================================================

def print_table(points: List[HeliumStatePoint], title: str = "Helium Properties"):
    """Pretty-print a table of state points."""
    if not points:
        return
    print(f"\n{'=' * 160}")
    print(f"  {title}")
    print(f"{'=' * 160}")
    print(points[0].header())
    print('-' * 160)

    prev_T = None
    for sp in points:
        if prev_T is not None and sp.T_K != prev_T:
            print()  # blank line between T blocks
        prev_T = sp.T_K
        print(sp.row())
    print(f"{'=' * 160}\n")


def print_volume_table(records: List[Dict], title: str = "Helium: Fixed-Amount Volume Sweep"):
    """Pretty-print volume sweep results."""
    if not records:
        return
    print(f"\n{'=' * 140}")
    print(f"  {title}")
    print(f"{'=' * 140}")
    print(f"{'T(K)':>8} {'V(m3)':>12} {'n(mol)':>8} {'P(Pa)':>14} {'P(atm)':>10} "
          f"{'rho':>10} {'Z':>8} {'a(m/s)':>9} {'k(W/mK)':>9} {'h(kJ/kg)':>10}")
    print('-' * 140)
    prev_T = None
    for r in records:
        if prev_T is not None and r['T_K'] != prev_T:
            print()
        prev_T = r['T_K']
        print(f"{r['T_K']:8.0f} {r['V_m3']:12.3e} {r['n_mol']:8.2f} "
              f"{r['P_Pa']:14.2f} {r['P_atm']:10.4f} "
              f"{r['rho_kg_m3']:10.4f} {r['Z']:8.5f} "
              f"{r['a_m_s']:9.1f} {r['k_W_mK']:9.5f} {r['h_kJ_kg']:10.2f}")
    print(f"{'=' * 140}\n")


def export_csv(points: List[HeliumStatePoint], filepath: str):
    """Export state points to CSV."""
    import os
    os.makedirs(os.path.dirname(filepath) or '.', exist_ok=True)
    with open(filepath, 'w', newline='') as f:
        if not points:
            return
        writer = csv.DictWriter(f, fieldnames=list(points[0].as_dict().keys()))
        writer.writeheader()
        for sp in points:
            writer.writerow({k: f"{v:.6g}" for k, v in sp.as_dict().items()})
    print(f"[helium_tables] Exported {len(points)} rows to {filepath}")


# =====================================================================
# Convenience: run all tables
# =====================================================================

def run_all(export_dir: str = "output/helium_tables"):
    """Generate and print all helium steam tables."""
    print("\n" + "#" * 80)
    print("#  HELIUM STEAM TABLES — VSEPR-SIM 4.0.4")
    print("#" * 80)

    # 1. Ambient
    amb = ambient_table()
    print_table(amb, "Table 1: Ambient Temperatures (200-400 K)")
    export_csv(amb, f"{export_dir}/he_ambient.csv")

    # 2. Reactor
    rx = reactor_table()
    print_table(rx, "Table 2: Reactor Temperatures (600-1500 K)")
    export_csv(rx, f"{export_dir}/he_reactor.csv")

    # 3. Volume sweep
    vol = volume_table()
    print_volume_table(vol, "Table 3: Volume Sweep (1 mol He, 0.001-1000 m^3)")
    # Export volume table as CSV too
    import os
    os.makedirs(export_dir, exist_ok=True)
    with open(f"{export_dir}/he_volume_sweep.csv", 'w', newline='') as f:
        if vol:
            writer = csv.DictWriter(f, fieldnames=list(vol[0].keys()))
            writer.writeheader()
            for r in vol:
                writer.writerow({k: f"{v:.6g}" for k, v in r.items()})
    print(f"[helium_tables] Exported {len(vol)} rows to {export_dir}/he_volume_sweep.csv")

    # 4. High-pressure reactor detail (70 atm, reactor range)
    hp = []
    for T in range(600, 1501, 25):
        hp.append(compute_state(float(T), 70.0 * ATM))
    print_table(hp, "Table 4: High-Pressure Reactor Detail (70 atm, 600-1500 K, 25K steps)")
    export_csv(hp, f"{export_dir}/he_highpressure_70atm.csv")

    print(f"\n[helium_tables] All tables complete. Files in {export_dir}/")
    return {'ambient': amb, 'reactor': rx, 'volume': vol, 'high_pressure': hp}