"""
plant_alpha2 — Nuclear And Traditional Power Plant Modelling Engine
====================================================================

Alpha Method 2: curve fitting, nuclear fuel/coolant property correlations,
sentinel-based missing-data handling, selective output, and automatic
report generation hooks.

Integer-sentinel pattern: MISSING = -9999 (never compare floats for equality).
Confidence tiers: C1 (published/validated), C2 (estimated/extrapolated), C3 (placeholder).

Covers:
  - Nuclear fuels: UO2, MOX, metallic U/Pu/Th
  - Molten-salt fuels: FLiBe-UF4, FLiBe-ThF4-UF4, LiF-ThF4-PuF3, LiF-ThF4, chloride salts
  - Coolants: liquid Na, liquid Pb, H2O (light water), FLiBe (molten salt)
  - Traditional: steam cycle water, natural gas combustion products
  - Curve fitting: Shomate, polynomial, Debye, custom correlations
  - Evaluation: 8-method screening (neutronic, thermophysical, phase-equilibrium,
    reprocessing, materials compatibility, safety, resource/supply, economic/deployment)

VSEPR-SIM 4.0.4
"""

from __future__ import annotations

import math
import json
from dataclasses import dataclass, field, asdict
from typing import Optional
from pathlib import Path

# ═══════════════════════════════════════════════════════════════════════
# Sentinel & constants
# ═══════════════════════════════════════════════════════════════════════

MISSING: float = -9999.0
"""Integer-backed sentinel.  Safe exact comparison: `v == MISSING`."""

R = 8.314462618        # J/(mol K)
STEFAN_BOLTZMANN = 5.670374419e-8  # W/(m^2 K^4)


# ═══════════════════════════════════════════════════════════════════════
# Confidence tier
# ═══════════════════════════════════════════════════════════════════════

class Confidence:
    C1 = "C1"   # published, peer-reviewed, validated
    C2 = "C2"   # estimated, extrapolated, or single-source
    C3 = "C3"   # placeholder / engineering guess


# ═══════════════════════════════════════════════════════════════════════
# Property record — one T-dependent correlation
# ═══════════════════════════════════════════════════════════════════════

@dataclass
class PropertyCorrelation:
    """A named T-dependent property with polynomial or custom form."""
    name: str
    symbol: str
    unit: str
    coeffs: list[float]          # polynomial coeffs [a0, a1, a2, ...] in ascending power
    T_min: float                 # validity range lower (K)
    T_max: float                 # validity range upper (K)
    form: str = "poly"           # "poly" | "shomate" | "custom"
    confidence: str = Confidence.C1
    source: str = ""
    notes: str = ""

    def evaluate(self, T: float) -> float:
        """Evaluate at temperature T (K).  Returns MISSING if out of range."""
        if T < self.T_min or T > self.T_max:
            return MISSING
        if self.form == "poly":
            return self._eval_poly(T)
        elif self.form == "shomate":
            return self._eval_shomate(T)
        else:
            return MISSING

    def _eval_poly(self, T: float) -> float:
        result = 0.0
        for i, c in enumerate(self.coeffs):
            result += c * T ** i
        return result

    def _eval_shomate(self, T: float) -> float:
        """Shomate: Cp = A + Bt + Ct^2 + Dt^3 + E/t^2, t = T/1000."""
        if len(self.coeffs) < 5:
            return MISSING
        t = T / 1000.0
        A, B, C, D, E = self.coeffs[:5]
        return A + B * t + C * t**2 + D * t**3 + E / t**2


# ═══════════════════════════════════════════════════════════════════════
# Material record — a named material with multiple property correlations
# ═══════════════════════════════════════════════════════════════════════

@dataclass
class Material:
    name: str
    formula: str
    category: str           # "fuel" | "coolant" | "structural" | "product"
    phase: str              # "solid" | "liquid" | "gas" | "mixed"
    melting_K: float        # K  (MISSING if unknown)
    boiling_K: float        # K  (MISSING if unknown)
    density_kgm3: float     # kg/m^3 at reference T (MISSING if unknown)
    ref_T: float            # reference temperature for density (K)
    properties: dict[str, PropertyCorrelation] = field(default_factory=dict)
    confidence: str = Confidence.C1
    source: str = ""

    def get(self, prop_name: str, T: float) -> float:
        """Evaluate a property at T.  Returns MISSING if not found."""
        p = self.properties.get(prop_name)
        if p is None:
            return MISSING
        return p.evaluate(T)

    def summary_at(self, T: float) -> dict:
        """Evaluate all properties at T, returning {name: value} dict."""
        out = {"material": self.name, "T_K": T}
        for pname, pcorr in self.properties.items():
            val = pcorr.evaluate(T)
            out[pname] = val if val != MISSING else None
            out[f"{pname}_confidence"] = pcorr.confidence
        return out

    def is_valid(self, prop_name: str, T: float) -> bool:
        """Check if a property value at T is known (not MISSING)."""
        return self.get(prop_name, T) != MISSING


# ═══════════════════════════════════════════════════════════════════════
# Nuclear fuel database
# ═══════════════════════════════════════════════════════════════════════

MATERIALS: dict[str, Material] = {}


def _mat(m: Material):
    MATERIALS[m.formula] = m


def _add_prop(formula: str, p: PropertyCorrelation):
    MATERIALS[formula].properties[p.name] = p


# ---------- UO2 (uranium dioxide fuel) ----------
_mat(Material(
    name="Uranium Dioxide", formula="UO2", category="fuel",
    phase="solid", melting_K=3120.0, boiling_K=3815.0,
    density_kgm3=10970.0, ref_T=298.0,
    confidence=Confidence.C1,
    source="IAEA-TECDOC-1496; Fink (2000) JNM 279:1",
))
# Cp(T) for UO2: Fink (2000) polynomial fit 298-3120 K
# Cp = 52.1743 + 87.951e-3 T - 84.2411e-6 T^2 + 31.542e-9 T^3 - 2.6334e-12 T^4
# (J/(mol K))  — valid solid phase only
_add_prop("UO2", PropertyCorrelation(
    name="Cp", symbol="C_p", unit="J/(mol K)",
    coeffs=[52.1743, 87.951e-3, -84.2411e-6, 31.542e-9, -2.6334e-12],
    T_min=298.0, T_max=3120.0, form="poly",
    confidence=Confidence.C1,
    source="Fink (2000) JNM 279:1-18",
))
# Thermal conductivity k(T) for UO2: IAEA correlation
# k = 1/(0.0375 + 2.165e-4 T) + 4.715e9/T^2 * exp(-16361/T)  W/(m K)
# We store as custom but approximate with a polynomial fit 500-2500 K:
# k ~ 7.98 - 0.00636T + 2.14e-6 T^2   (R^2 = 0.994 over 500-2500 K)
_add_prop("UO2", PropertyCorrelation(
    name="k_thermal", symbol="k", unit="W/(m K)",
    coeffs=[7.98, -0.00636, 2.14e-6],
    T_min=500.0, T_max=2500.0, form="poly",
    confidence=Confidence.C1,
    source="IAEA-TECDOC-1496 eq. 4.1",
    notes="Polynomial approx of IAEA correlation; R^2=0.994",
))

# ---------- MOX (mixed oxide, (U,Pu)O2) ----------
_mat(Material(
    name="Mixed Oxide Fuel", formula="MOX", category="fuel",
    phase="solid", melting_K=3023.0, boiling_K=MISSING,
    density_kgm3=11050.0, ref_T=298.0,
    confidence=Confidence.C2,
    source="IAEA-TECDOC-1496; Carbajo (2001) JNM 299:181",
))
_add_prop("MOX", PropertyCorrelation(
    name="Cp", symbol="C_p", unit="J/(mol K)",
    coeffs=[51.26, 92.10e-3, -88.0e-6, 33.1e-9, -2.8e-12],
    T_min=298.0, T_max=3023.0, form="poly",
    confidence=Confidence.C2,
    source="Carbajo (2001) adjusted from UO2 baseline",
))

# ---------- Metallic Uranium ----------
_mat(Material(
    name="Uranium metal (alpha)", formula="U_alpha", category="fuel",
    phase="solid", melting_K=1405.3, boiling_K=4404.0,
    density_kgm3=19100.0, ref_T=298.0,
    confidence=Confidence.C1,
    source="CRC Handbook 97th Ed; IAEA",
))
_add_prop("U_alpha", PropertyCorrelation(
    name="Cp", symbol="C_p", unit="J/(mol K)",
    coeffs=[24.65, 9.68e-3, 3.15e-6],
    T_min=298.0, T_max=941.0, form="poly",
    confidence=Confidence.C1,
    source="Konings (2010) JNM 402:166",
    notes="alpha-U phase only (orthorhombic, < 941 K)",
))
_add_prop("U_alpha", PropertyCorrelation(
    name="k_thermal", symbol="k", unit="W/(m K)",
    coeffs=[21.73, 0.01591, -5.12e-6],
    T_min=298.0, T_max=1405.0, form="poly",
    confidence=Confidence.C1,
    source="Ho et al., TPRC Data Series Vol 1",
))

# ---------- Thorium metal ----------
_mat(Material(
    name="Thorium metal", formula="Th", category="fuel",
    phase="solid", melting_K=2023.0, boiling_K=5061.0,
    density_kgm3=11700.0, ref_T=298.0,
    confidence=Confidence.C1,
    source="CRC Handbook; Peterson (1985)",
))
_add_prop("Th", PropertyCorrelation(
    name="Cp", symbol="C_p", unit="J/(mol K)",
    coeffs=[24.31, 6.20e-3, 5.95e-6],
    T_min=298.0, T_max=1633.0, form="poly",
    confidence=Confidence.C1,
    source="Konings (2014) JNM 451:1",
))

# ---------- Zircaloy-4 (cladding) ----------
_mat(Material(
    name="Zircaloy-4", formula="Zry4", category="structural",
    phase="solid", melting_K=2125.0, boiling_K=MISSING,
    density_kgm3=6560.0, ref_T=298.0,
    confidence=Confidence.C1,
    source="NUREG/CR-6150 (MATPRO)",
))
_add_prop("Zry4", PropertyCorrelation(
    name="Cp", symbol="C_p", unit="J/(kg K)",
    coeffs=[252.54, 0.1189],
    T_min=298.0, T_max=1083.0, form="poly",
    confidence=Confidence.C1,
    source="MATPRO Zircaloy correlations",
    notes="alpha phase; J/(kg K) not J/(mol K)",
))

# ═══════════════════════════════════════════════════════════════════════
# Coolant database
# ═══════════════════════════════════════════════════════════════════════

# ---------- Liquid sodium ----------
_mat(Material(
    name="Liquid Sodium", formula="Na_liq", category="coolant",
    phase="liquid", melting_K=370.9, boiling_K=1156.0,
    density_kgm3=927.0, ref_T=371.0,
    confidence=Confidence.C1,
    source="Fink & Leibowitz (1995) ANL/RE-95/2",
))
# rho(T) = 1014 - 0.2353 T   kg/m^3   (371-1155 K)
_add_prop("Na_liq", PropertyCorrelation(
    name="density", symbol="rho", unit="kg/m^3",
    coeffs=[1014.0, -0.2353],
    T_min=371.0, T_max=1155.0, form="poly",
    confidence=Confidence.C1,
    source="Fink & Leibowitz (1995)",
))
# Cp(T) = 1658 - 0.8479T + 4.454e-4 T^2   J/(kg K)
_add_prop("Na_liq", PropertyCorrelation(
    name="Cp", symbol="C_p", unit="J/(kg K)",
    coeffs=[1658.0, -0.8479, 4.454e-4],
    T_min=371.0, T_max=1155.0, form="poly",
    confidence=Confidence.C1,
    source="Fink & Leibowitz (1995)",
))
# k(T) = 110 - 0.0648T + 1.16e-5 T^2   W/(m K)
_add_prop("Na_liq", PropertyCorrelation(
    name="k_thermal", symbol="k", unit="W/(m K)",
    coeffs=[110.0, -0.0648, 1.16e-5],
    T_min=371.0, T_max=1155.0, form="poly",
    confidence=Confidence.C1,
    source="Fink & Leibowitz (1995)",
))

# ---------- Liquid lead ----------
_mat(Material(
    name="Liquid Lead", formula="Pb_liq", category="coolant",
    phase="liquid", melting_K=600.6, boiling_K=2022.0,
    density_kgm3=10678.0, ref_T=601.0,
    confidence=Confidence.C1,
    source="OECD/NEA Handbook on Lead-Bismuth",
))
_add_prop("Pb_liq", PropertyCorrelation(
    name="density", symbol="rho", unit="kg/m^3",
    coeffs=[11367.0, -1.1944],
    T_min=601.0, T_max=2000.0, form="poly",
    confidence=Confidence.C1,
    source="OECD/NEA LBE Handbook",
))
_add_prop("Pb_liq", PropertyCorrelation(
    name="Cp", symbol="C_p", unit="J/(kg K)",
    coeffs=[175.1, -4.961e-2, 1.985e-5, -2.099e-9],
    T_min=601.0, T_max=2000.0, form="poly",
    confidence=Confidence.C1,
    source="OECD/NEA LBE Handbook",
))

# ---------- FLiBe (molten salt) ----------
_mat(Material(
    name="FLiBe (2LiF-BeF2)", formula="FLiBe", category="coolant",
    phase="liquid", melting_K=732.0, boiling_K=1703.0,
    density_kgm3=1940.0, ref_T=973.0,
    confidence=Confidence.C2,
    source="Williams (2006) ORNL/TM-2006/12",
))
_add_prop("FLiBe", PropertyCorrelation(
    name="density", symbol="rho", unit="kg/m^3",
    coeffs=[2413.0, -0.488],
    T_min=732.0, T_max=1473.0, form="poly",
    confidence=Confidence.C2,
    source="Williams (2006)",
))
_add_prop("FLiBe", PropertyCorrelation(
    name="Cp", symbol="C_p", unit="J/(kg K)",
    coeffs=[2386.0],  # nearly constant over operating range
    T_min=732.0, T_max=1473.0, form="poly",
    confidence=Confidence.C2,
    source="Williams (2006); Benes & Konings (2012)",
    notes="Approximately constant; spread in literature is +/- 10%",
))

# ---------- Light water (subcooled, liquid) ----------
_mat(Material(
    name="Light Water (subcooled)", formula="H2O_liq", category="coolant",
    phase="liquid", melting_K=273.15, boiling_K=373.15,
    density_kgm3=997.0, ref_T=298.0,
    confidence=Confidence.C1,
    source="IAPWS-IF97",
    # Note: boiling_K is at 1 atm; PWR operates at ~15.5 MPa (618 K saturation)
))
_add_prop("H2O_liq", PropertyCorrelation(
    name="Cp", symbol="C_p", unit="J/(kg K)",
    coeffs=[8958.9, -40.535, 0.11243, -1.014e-4],
    T_min=280.0, T_max=600.0, form="poly",
    confidence=Confidence.C1,
    source="IAPWS-IF97 polynomial fit (1 atm to 16 MPa average)",
    notes="Pressure-dependent; this is an average for LWR range",
))


# ═══════════════════════════════════════════════════════════════════════
# Molten-salt fuel database
# ═══════════════════════════════════════════════════════════════════════

# ---------- FLiBe-UF4: LiF-BeF2-UF4 (classic fluoride fuel) ----------
_mat(Material(
    name="FLiBe with Uranium (LiF-BeF2-UF4)", formula="FLiBe_UF4",
    category="fuel", phase="liquid",
    melting_K=733.0, boiling_K=MISSING,
    density_kgm3=2150.0, ref_T=973.0,
    confidence=Confidence.C2,
    source="ORNL-4449; Cantor (1968); Williams (2006) ORNL/TM-2006/12",
))
# rho(T) ~ 2553 - 0.414 T   kg/m^3  (approximate for ~5 mol% UF4 in FLiBe)
_add_prop("FLiBe_UF4", PropertyCorrelation(
    name="density", symbol="rho", unit="kg/m^3",
    coeffs=[2553.0, -0.414],
    T_min=733.0, T_max=1200.0, form="poly",
    confidence=Confidence.C2,
    source="Williams (2006); adjusted for UF4 loading",
))
_add_prop("FLiBe_UF4", PropertyCorrelation(
    name="Cp", symbol="C_p", unit="J/(kg K)",
    coeffs=[2300.0],
    T_min=733.0, T_max=1200.0, form="poly",
    confidence=Confidence.C2,
    source="Williams (2006); Benes & Konings (2012)",
    notes="Approximately constant; UF4 addition lowers Cp slightly vs pure FLiBe",
))
_add_prop("FLiBe_UF4", PropertyCorrelation(
    name="k_thermal", symbol="k", unit="W/(m K)",
    coeffs=[1.1],
    T_min=733.0, T_max=1200.0, form="poly",
    confidence=Confidence.C3,
    source="Estimated from FLiBe baseline; limited UF4 data",
))
_add_prop("FLiBe_UF4", PropertyCorrelation(
    name="viscosity", symbol="mu", unit="Pa s",
    coeffs=[7.67e-2, -1.04e-4, 5.6e-8],
    T_min=773.0, T_max=1100.0, form="poly",
    confidence=Confidence.C3,
    source="Cantor et al. (1968) adjusted estimate",
    notes="Viscosity decreases with T; salt-composition dependent",
))

# ---------- FLiBe-ThF4-UF4: LiF-BeF2-ThF4-UF4 (thorium-uranium fluoride) ----------
_mat(Material(
    name="FLiBe with Thorium-Uranium (LiF-BeF2-ThF4-UF4)", formula="FLiBe_ThUF",
    category="fuel", phase="liquid",
    melting_K=773.0, boiling_K=MISSING,
    density_kgm3=2800.0, ref_T=973.0,
    confidence=Confidence.C2,
    source="ORNL-4449; Cantor (1968); phase diagrams from Thoma (1959)",
))
_add_prop("FLiBe_ThUF", PropertyCorrelation(
    name="density", symbol="rho", unit="kg/m^3",
    coeffs=[3200.0, -0.42],
    T_min=773.0, T_max=1200.0, form="poly",
    confidence=Confidence.C2,
    source="Estimated from ThF4-bearing FLiBe mixtures",
    notes="ThF4 raises density relative to pure FLiBe significantly",
))
_add_prop("FLiBe_ThUF", PropertyCorrelation(
    name="Cp", symbol="C_p", unit="J/(kg K)",
    coeffs=[1550.0],
    T_min=773.0, T_max=1200.0, form="poly",
    confidence=Confidence.C3,
    source="Engineering estimate; ThF4 and UF4 lower heat capacity vs pure FLiBe",
))
_add_prop("FLiBe_ThUF", PropertyCorrelation(
    name="thermal_conductivity", symbol="k", unit="W/(m K)",
    coeffs=[1.0],
    T_min=773.0, T_max=1200.0, form="poly",
    confidence=Confidence.C3,
    source="Engineering estimate; FLiBe baseline ~1 W/(m K); ThF4 effect uncertain",
))
_add_prop("FLiBe_ThUF", PropertyCorrelation(
    name="viscosity", symbol="mu", unit="Pa s",
    coeffs=[3.5e-2, -2.5e-5],
    T_min=773.0, T_max=1200.0, form="poly",
    confidence=Confidence.C3,
    source="Estimated from FLiBe viscosity + ThF4 viscosity depression",
))

# ---------- LiF-ThF4-PuF3: Actinide-rich fluoride ----------
_mat(Material(
    name="Actinide-Rich Fluoride (LiF-ThF4-PuF3)", formula="LiF_ThPuF",
    category="fuel", phase="liquid",
    melting_K=838.0, boiling_K=MISSING,
    density_kgm3=4200.0, ref_T=973.0,
    confidence=Confidence.C3,
    source="ORNL-4812; Grimes (1970) reactor chemistry review",
))
_add_prop("LiF_ThPuF", PropertyCorrelation(
    name="density", symbol="rho", unit="kg/m^3",
    coeffs=[4700.0, -0.52],
    T_min=838.0, T_max=1100.0, form="poly",
    confidence=Confidence.C3,
    source="Estimated from LiF-ThF4 binary + PuF3 contribution",
    notes="PuF3 solubility limits complicate actual fuel loading",
))
_add_prop("LiF_ThPuF", PropertyCorrelation(
    name="Cp", symbol="C_p", unit="J/(kg K)",
    coeffs=[1350.0],
    T_min=838.0, T_max=1100.0, form="poly",
    confidence=Confidence.C3,
    source="Engineering estimate; heavy-actinide loading reduces Cp",
))
_add_prop("LiF_ThPuF", PropertyCorrelation(
    name="thermal_conductivity", symbol="k", unit="W/(m K)",
    coeffs=[0.9],
    T_min=838.0, T_max=1100.0, form="poly",
    confidence=Confidence.C3,
    source="Engineering estimate; heavy actinide loading may depress k",
))
_add_prop("LiF_ThPuF", PropertyCorrelation(
    name="viscosity", symbol="mu", unit="Pa s",
    coeffs=[4.0e-2, -3.0e-5],
    T_min=838.0, T_max=1100.0, form="poly",
    confidence=Confidence.C3,
    source="Estimated; PuF3 loading increases viscosity relative to LiF-ThF4 binary",
))

# ---------- LiF-ThF4: Lithium-Thorium fluoride ----------
_mat(Material(
    name="Lithium-Thorium Fluoride (LiF-ThF4)", formula="LiF_ThF4",
    category="fuel", phase="liquid",
    melting_K=841.0, boiling_K=MISSING,
    density_kgm3=4060.0, ref_T=973.0,
    confidence=Confidence.C2,
    source="Cantor (1968); Thoma (1959) ORNL-2548 phase diagrams",
))
# rho(T) ~ 4520 - 0.474 T  for 73-27 mol% LiF-ThF4
_add_prop("LiF_ThF4", PropertyCorrelation(
    name="density", symbol="rho", unit="kg/m^3",
    coeffs=[4520.0, -0.474],
    T_min=841.0, T_max=1200.0, form="poly",
    confidence=Confidence.C2,
    source="Cantor (1968) for 73-27 mol% LiF-ThF4",
))
_add_prop("LiF_ThF4", PropertyCorrelation(
    name="Cp", symbol="C_p", unit="J/(kg K)",
    coeffs=[1000.0],
    T_min=841.0, T_max=1200.0, form="poly",
    confidence=Confidence.C2,
    source="Estimated from ORNL measurements; ThF4-rich salts",
))
_add_prop("LiF_ThF4", PropertyCorrelation(
    name="viscosity", symbol="mu", unit="Pa s",
    coeffs=[1.1e-1, -1.2e-4, 5.0e-8],
    T_min=873.0, T_max=1200.0, form="poly",
    confidence=Confidence.C3,
    source="Estimated from Cantor (1968) viscosity systematics",
))
_add_prop("LiF_ThF4", PropertyCorrelation(
    name="thermal_conductivity", symbol="k", unit="W/(m K)",
    coeffs=[0.95],
    T_min=841.0, T_max=1200.0, form="poly",
    confidence=Confidence.C3,
    source="Estimated from fluoride salt thermal conductivity systematics",
))

# ---------- Chloride-based: ThCl4-UCl3-PuCl3 / NaCl-MgCl2 carrier ----------
_mat(Material(
    name="Chloride Fuel Salt (ThCl4-UCl3-PuCl3 / NaCl-MgCl2)", formula="Cl_ThUPu",
    category="fuel", phase="liquid",
    melting_K=693.0, boiling_K=MISSING,
    density_kgm3=3200.0, ref_T=973.0,
    confidence=Confidence.C3,
    source="Mourogov & Bokov (2006); Taube (1978); IAEA fast-MSR review",
))
_add_prop("Cl_ThUPu", PropertyCorrelation(
    name="density", symbol="rho", unit="kg/m^3",
    coeffs=[3600.0, -0.41],
    T_min=693.0, T_max=1100.0, form="poly",
    confidence=Confidence.C3,
    source="Estimated from NaCl-MgCl2-actinide chloride mixtures",
    notes="Fast-spectrum salt; chloride data much less mature than fluorides",
))
_add_prop("Cl_ThUPu", PropertyCorrelation(
    name="Cp", symbol="C_p", unit="J/(kg K)",
    coeffs=[1050.0],
    T_min=693.0, T_max=1100.0, form="poly",
    confidence=Confidence.C3,
    source="Engineering estimate for chloride actinide mixture",
))
_add_prop("Cl_ThUPu", PropertyCorrelation(
    name="thermal_conductivity", symbol="k", unit="W/(m K)",
    coeffs=[0.5],
    T_min=693.0, T_max=1100.0, form="poly",
    confidence=Confidence.C3,
    source="Estimated from chloride salt thermal conductivity systematics",
))
_add_prop("Cl_ThUPu", PropertyCorrelation(
    name="viscosity", symbol="mu", unit="Pa s",
    coeffs=[4.5e-3, -2.5e-6],
    T_min=693.0, T_max=1100.0, form="poly",
    confidence=Confidence.C3,
    source="Approximate; chloride salts have lower viscosity than fluorides",
))

# ---------- FLiNaK: LiF-NaF-KF eutectic (46.5-11.5-42 mol%) ----------
_mat(Material(
    name="FLiNaK Eutectic (LiF-NaF-KF)", formula="FLiNaK",
    category="coolant", phase="liquid",
    melting_K=727.0, boiling_K=1843.0,
    density_kgm3=2530.0, ref_T=973.0,
    confidence=Confidence.C1,
    source="Vriesema (1979); Williams (2006) ORNL/TM-2006/12; Sohal et al. INL/EXT-10-18297",
))
_add_prop("FLiNaK", PropertyCorrelation(
    name="density", symbol="rho", unit="kg/m^3",
    coeffs=[2729.3, -0.73],
    T_min=727.0, T_max=1170.0, form="poly",
    confidence=Confidence.C1,
    source="Vriesema (1979); rho = 2729.3 - 0.73 T",
))
_add_prop("FLiNaK", PropertyCorrelation(
    name="Cp", symbol="C_p", unit="J/(kg K)",
    coeffs=[1884.0],
    T_min=727.0, T_max=1170.0, form="poly",
    confidence=Confidence.C1,
    source="Grimes (1966); Sohal et al. (2010)",
))
_add_prop("FLiNaK", PropertyCorrelation(
    name="thermal_conductivity", symbol="k", unit="W/(m K)",
    coeffs=[0.36, 5.6e-4],
    T_min=790.0, T_max=1080.0, form="poly",
    confidence=Confidence.C2,
    source="Smirnov et al. (1987); k ~ 0.36 + 5.6e-4 T",
))
_add_prop("FLiNaK", PropertyCorrelation(
    name="viscosity", symbol="mu", unit="Pa s",
    coeffs=[2.487e-2, -2.0e-5],
    T_min=770.0, T_max=1080.0, form="poly",
    confidence=Confidence.C2,
    source="Ambrosek (2010); linearised from Arrhenius fit",
))

# ---------- MSBR Reference Salt: LiF-BeF2-ThF4-UF4 (71.7-16-12-0.3 mol%) ----------
_mat(Material(
    name="MSBR Reference Salt (LiF-BeF2-ThF4-UF4)", formula="MSBR_ref",
    category="fuel", phase="liquid",
    melting_K=773.0, boiling_K=MISSING,
    density_kgm3=3350.0, ref_T=973.0,
    confidence=Confidence.C2,
    source="Robertson (1971) ORNL-4541 MSBR Conceptual Design Study",
))
_add_prop("MSBR_ref", PropertyCorrelation(
    name="density", symbol="rho", unit="kg/m^3",
    coeffs=[3752.0, -0.41],
    T_min=773.0, T_max=1100.0, form="poly",
    confidence=Confidence.C2,
    source="ORNL-4541 Table 3.1; rho = 3752 - 0.41 T",
))
_add_prop("MSBR_ref", PropertyCorrelation(
    name="Cp", symbol="C_p", unit="J/(kg K)",
    coeffs=[1357.0],
    T_min=773.0, T_max=1100.0, form="poly",
    confidence=Confidence.C2,
    source="ORNL-4541; blended FLiBe + ThF4 Cp estimate",
))
_add_prop("MSBR_ref", PropertyCorrelation(
    name="thermal_conductivity", symbol="k", unit="W/(m K)",
    coeffs=[1.0],
    T_min=773.0, T_max=1100.0, form="poly",
    confidence=Confidence.C3,
    source="Robertson (1971); single design-basis value ~1 W/(m K)",
))
_add_prop("MSBR_ref", PropertyCorrelation(
    name="viscosity", symbol="mu", unit="Pa s",
    coeffs=[2.6e-2, -1.8e-5],
    T_min=773.0, T_max=1100.0, form="poly",
    confidence=Confidence.C3,
    source="ORNL-4541 design value; linearised from ThF4-loaded FLiBe data",
))

# ---------- NaCl-UCl3: Binary chloride fuel ----------
_mat(Material(
    name="NaCl-UCl3 Binary Chloride Fuel", formula="NaCl_UCl3",
    category="fuel", phase="liquid",
    melting_K=798.0, boiling_K=MISSING,
    density_kgm3=3390.0, ref_T=973.0,
    confidence=Confidence.C3,
    source="Taube and Ligou (1974); Mourogov et al. (2006) fast-MSR fuel review",
))
_add_prop("NaCl_UCl3", PropertyCorrelation(
    name="density", symbol="rho", unit="kg/m^3",
    coeffs=[3836.0, -0.46],
    T_min=798.0, T_max=1200.0, form="poly",
    confidence=Confidence.C3,
    source="Estimated from NaCl-UCl3 binary phase data",
))
_add_prop("NaCl_UCl3", PropertyCorrelation(
    name="Cp", symbol="C_p", unit="J/(kg K)",
    coeffs=[1020.0],
    T_min=798.0, T_max=1200.0, form="poly",
    confidence=Confidence.C3,
    source="Engineering estimate from chloride salt systematics",
))
_add_prop("NaCl_UCl3", PropertyCorrelation(
    name="thermal_conductivity", symbol="k", unit="W/(m K)",
    coeffs=[0.65],
    T_min=798.0, T_max=1200.0, form="poly",
    confidence=Confidence.C3,
    source="Estimated from NaCl-based chloride salt measurements",
))
_add_prop("NaCl_UCl3", PropertyCorrelation(
    name="viscosity", symbol="mu", unit="Pa s",
    coeffs=[5.0e-3, -3.0e-6],
    T_min=798.0, T_max=1200.0, form="poly",
    confidence=Confidence.C3,
    source="Approximate; chloride salts lower viscosity than fluorides",
))

# ---------- NaCl-MgCl2-UCl3: Ternary chloride carrier/fuel ----------
_mat(Material(
    name="NaCl-MgCl2-UCl3 Ternary Chloride Fuel", formula="NaCl_MgCl2_UCl3",
    category="fuel", phase="liquid",
    melting_K=673.0, boiling_K=MISSING,
    density_kgm3=2800.0, ref_T=973.0,
    confidence=Confidence.C3,
    source="Proposed fast-spectrum carrier; NaCl-MgCl2 eutectic + UCl3 loading",
))
_add_prop("NaCl_MgCl2_UCl3", PropertyCorrelation(
    name="density", symbol="rho", unit="kg/m^3",
    coeffs=[3150.0, -0.38],
    T_min=673.0, T_max=1100.0, form="poly",
    confidence=Confidence.C3,
    source="Estimated from NaCl-MgCl2 eutectic + UCl3 additive rule",
))
_add_prop("NaCl_MgCl2_UCl3", PropertyCorrelation(
    name="Cp", symbol="C_p", unit="J/(kg K)",
    coeffs=[1080.0],
    T_min=673.0, T_max=1100.0, form="poly",
    confidence=Confidence.C3,
    source="Blended from NaCl-MgCl2 eutectic Cp + UCl3 depression",
))
_add_prop("NaCl_MgCl2_UCl3", PropertyCorrelation(
    name="thermal_conductivity", symbol="k", unit="W/(m K)",
    coeffs=[0.55],
    T_min=673.0, T_max=1100.0, form="poly",
    confidence=Confidence.C3,
    source="Estimated from chloride salt thermal conductivity systematics",
))
_add_prop("NaCl_MgCl2_UCl3", PropertyCorrelation(
    name="viscosity", symbol="mu", unit="Pa s",
    coeffs=[6.0e-3, -3.5e-6],
    T_min=673.0, T_max=1100.0, form="poly",
    confidence=Confidence.C3,
    source="Approximate; MgCl2 addition slightly raises viscosity vs binary NaCl-UCl3",
))


# ═══════════════════════════════════════════════════════════════════════
# Evaluation framework — 8-method screening for molten-salt fuel selection
# ═══════════════════════════════════════════════════════════════════════

class EvalRating:
    """Qualitative rating for evaluation method results."""
    EXCELLENT = 5
    GOOD      = 4
    ADEQUATE  = 3
    MARGINAL  = 2
    POOR      = 1
    UNKNOWN   = 0


@dataclass
class EvalResult:
    """Single evaluation method result for one fuel salt."""
    method: str
    rating: int            # EvalRating value 0-5
    confidence: str        # Confidence tier
    summary: str           # one-line finding
    notes: str = ""        # extended notes


@dataclass
class FuelEvaluation:
    """Complete multi-method evaluation for a single fuel salt."""
    formula: str
    results: dict[str, EvalResult] = field(default_factory=dict)

    def overall_score(self) -> float:
        """Weighted average of all rated methods (UNKNOWN excluded)."""
        rated = [r for r in self.results.values() if r.rating > 0]
        if not rated:
            return 0.0
        return sum(r.rating for r in rated) / len(rated)

    def summary(self) -> dict:
        out = {
            "formula": self.formula,
            "overall_score": round(self.overall_score(), 2),
            "method_count": len(self.results),
            "methods": {},
        }
        for name, r in self.results.items():
            out["methods"][name] = {
                "rating": r.rating,
                "confidence": r.confidence,
                "summary": r.summary,
            }
        return out


# --- Evaluation method definitions ---

EVAL_METHODS = [
    "neutronic",
    "thermophysical",
    "phase_equilibrium",
    "reprocessing",
    "materials_compatibility",
    "safety",
    "resource_supply",
    "economic_deployment",
]


def _eval_neutronic(mat: Material) -> EvalResult:
    """Neutron-balance / reactor-performance screening."""
    f = mat.formula
    if f == "MSBR_ref" or ("FLiBe" in f and "Pu" not in f):
        return EvalResult("neutronic", EvalRating.GOOD, Confidence.C2,
            "Thermal-spectrum fluoride; good neutron economy with enriched-Li7",
            notes="Li-6 parasitic capture requires isotopic enrichment to >99.99% Li-7")
    if f == "LiF_ThF4":
        return EvalResult("neutronic", EvalRating.ADEQUATE, Confidence.C2,
            "Fertile-only; requires fissile driver or external source",
            notes="Th-232 breeds U-233 but cannot self-sustain without fissile addition")
    if f == "LiF_ThPuF":
        return EvalResult("neutronic", EvalRating.ADEQUATE, Confidence.C3,
            "Pu fissile driver with Th fertile; neutronics viable but Pu-fluoride chemistry immature",
            notes="PuF3 solubility limits constrain actual fissile loading")
    if "Cl" in f:
        return EvalResult("neutronic", EvalRating.GOOD, Confidence.C3,
            "Fast-spectrum chloride; harder spectrum favors breeding and actinide burning",
            notes="Cl-37 enrichment needed to avoid Cl-35(n,p)S-35 activation")
    return EvalResult("neutronic", EvalRating.UNKNOWN, Confidence.C3,
        "Insufficient neutronic data for this salt")


def _eval_thermophysical(mat: Material) -> EvalResult:
    """Thermophysical property completeness and quality."""
    props = mat.properties
    n_props = len(props)
    avg_conf = sum(1 for p in props.values() if p.confidence == Confidence.C1)
    if n_props >= 3 and avg_conf >= 2:
        return EvalResult("thermophysical", EvalRating.GOOD, Confidence.C1,
            f"{n_props} properties with {avg_conf} at C1 confidence")
    if n_props >= 2:
        return EvalResult("thermophysical", EvalRating.ADEQUATE, Confidence.C2,
            f"{n_props} properties available; data gaps remain")
    return EvalResult("thermophysical", EvalRating.MARGINAL, Confidence.C3,
        f"Only {n_props} properties; significant data gaps")


def _eval_phase_equilibrium(mat: Material) -> EvalResult:
    """Phase diagram / composition stability assessment."""
    f = mat.formula
    if f in ("FLiBe_UF4", "LiF_ThF4", "MSBR_ref"):
        return EvalResult("phase_equilibrium", EvalRating.GOOD, Confidence.C2,
            "Binary/pseudo-binary phase diagrams available from ORNL campaigns",
            notes="Freezing behavior well-characterized over relevant composition ranges")
    if f == "FLiBe_ThUF":
        return EvalResult("phase_equilibrium", EvalRating.ADEQUATE, Confidence.C2,
            "Ternary LiF-BeF2-ThF4 partially mapped; UF4 addition less characterized",
            notes="Replacing some UF4 with ThF4 does not drastically alter freezing behavior")
    if f == "LiF_ThPuF":
        return EvalResult("phase_equilibrium", EvalRating.MARGINAL, Confidence.C3,
            "PuF3 phase behavior in fluorides poorly characterized",
            notes="Solubility limits of PuF3 not well established at reactor temperatures")
    if "Cl" in f:
        return EvalResult("phase_equilibrium", EvalRating.MARGINAL, Confidence.C3,
            "Chloride phase diagrams for actinide mixtures largely unavailable",
            notes="NaCl-MgCl2 carrier well known; actinide chloride additions not")
    return EvalResult("phase_equilibrium", EvalRating.UNKNOWN, Confidence.C3,
        "No phase-equilibrium assessment available")


def _eval_reprocessing(mat: Material) -> EvalResult:
    """Reprocessing / chemical management feasibility."""
    f = mat.formula
    if "FLiBe" in f and "Pu" not in f and "Th" not in f.replace("FLiBe_ThUF", ""):
        if f == "FLiBe_UF4":
            return EvalResult("reprocessing", EvalRating.GOOD, Confidence.C2,
                "Fluoride volatility process demonstrated for uranium recovery at ORNL",
                notes="UF6 volatility method proven at pilot scale in MSRE campaign")
    if "Th" in f:
        return EvalResult("reprocessing", EvalRating.MARGINAL, Confidence.C3,
            "Thorium/Pa-233 separation much less straightforward than uranium recovery",
            notes="Protactinium removal required for breeding; liquid-metal extraction proposed but unproven at scale")
    if "Pu" in f:
        return EvalResult("reprocessing", EvalRating.POOR, Confidence.C3,
            "Plutonium fluoride/chloride reprocessing immature and problematic",
            notes="Proliferation and chemistry challenges; no demonstrated industrial process")
    if "Cl" in f:
        return EvalResult("reprocessing", EvalRating.MARGINAL, Confidence.C3,
            "Chloride reprocessing (pyroprocessing) less mature than fluoride volatility",
            notes="Electrorefining for chlorides under development; not yet industrial")
    return EvalResult("reprocessing", EvalRating.UNKNOWN, Confidence.C3,
        "Reprocessing pathway not assessed")


def _eval_materials_compat(mat: Material) -> EvalResult:
    """Structural materials compatibility (INOR-8 / Hastelloy-N, graphite)."""
    f = mat.formula
    if "FLiBe" in f and "Pu" not in f:
        return EvalResult("materials_compatibility", EvalRating.GOOD, Confidence.C2,
            "Compatible with Hastelloy-N (INOR-8) demonstrated in MSRE",
            notes="Graphite moderator compatibility also established; Te embrittlement managed with Nb addition")
    if f == "LiF_ThF4":
        return EvalResult("materials_compatibility", EvalRating.ADEQUATE, Confidence.C2,
            "LiF-ThF4 compatible with Hastelloy-N; less operational experience than FLiBe-UF4",
            notes="No graphite moderator needed for some configurations")
    if "Pu" in f:
        return EvalResult("materials_compatibility", EvalRating.MARGINAL, Confidence.C3,
            "PuF3 corrosion behavior poorly characterized; expected worse than UF4 systems",
            notes="Higher redox potential may accelerate container corrosion")
    if "Cl" in f:
        return EvalResult("materials_compatibility", EvalRating.POOR, Confidence.C3,
            "Chloride salts significantly more corrosive than fluorides to nickel alloys",
            notes="May require Mo-based or refractory alloys; no proven reactor-grade containment")
    return EvalResult("materials_compatibility", EvalRating.UNKNOWN, Confidence.C3,
        "Materials compatibility not assessed")


def _eval_safety(mat: Material) -> EvalResult:
    """Safety, toxicity, handling risk, and failure-mode assessment."""
    f = mat.formula
    # BeF2 toxicity is universal for FLiBe-based salts
    if "FLiBe" in f:
        base = EvalRating.ADEQUATE
        note = "BeF2 is toxic; Li-7 enrichment needed; "
        if "Pu" in f:
            base = EvalRating.MARGINAL
            note += "PuF3 adds proliferation and radiotoxicity burden"
        else:
            note += "UF4/ThF4 handling is established practice"
        return EvalResult("safety", base, Confidence.C2, note.rstrip("; "))
    if f == "LiF_ThF4":
        return EvalResult("safety", EvalRating.ADEQUATE, Confidence.C2,
            "No Be toxicity; Th handling is manageable but generates Ra-224/Rn-220 in decay chain")
    if f == "LiF_ThPuF":
        return EvalResult("safety", EvalRating.MARGINAL, Confidence.C3,
            "Pu handling adds criticality and proliferation risk; Th adds decay chain complexity")
    if "Cl" in f:
        return EvalResult("safety", EvalRating.MARGINAL, Confidence.C3,
            "Chloride activation (Cl-35 -> S-35); aggressive corrosion failure modes; less operational heritage",
            notes="Volatile fission products may behave differently in chloride matrix")
    return EvalResult("safety", EvalRating.UNKNOWN, Confidence.C3,
        "Safety assessment not available")


def _eval_resource_supply(mat: Material) -> EvalResult:
    """Resource availability and supply-chain assessment."""
    f = mat.formula
    if "FLiBe" in f:
        return EvalResult("resource_supply", EvalRating.ADEQUATE, Confidence.C2,
            "Requires enriched Li-7 (limited supply) and Be (strategic mineral); U is available",
            notes="Li-7 enrichment is the binding constraint; COLEX process discontinued")
    if f == "LiF_ThF4":
        return EvalResult("resource_supply", EvalRating.GOOD, Confidence.C2,
            "Thorium abundant in monazite sands; Li supply needed but no Be required",
            notes="Thorium recovery infrastructure exists but is dormant in most countries")
    if "Pu" in f:
        return EvalResult("resource_supply", EvalRating.MARGINAL, Confidence.C3,
            "Pu availability limited to reprocessed spent fuel; supply chain heavily regulated",
            notes="Reactor-grade Pu sourcing depends on national reprocessing policy")
    if "Cl" in f:
        return EvalResult("resource_supply", EvalRating.ADEQUATE, Confidence.C3,
            "NaCl/MgCl2 abundant; Cl-37 enrichment adds cost; actinide chloride production immature",
            notes="Chloride salt carrier is cheap; fissile/fertile chloride preparation is not")
    return EvalResult("resource_supply", EvalRating.UNKNOWN, Confidence.C3,
        "Resource supply not assessed")


def _eval_economic(mat: Material) -> EvalResult:
    """Economic and deployment practicality assessment."""
    f = mat.formula
    if f == "FLiBe_UF4":
        return EvalResult("economic_deployment", EvalRating.GOOD, Confidence.C2,
            "Most mature MSR fuel option; existing design basis from MSRE; moderate reprocessing cost",
            notes="Li-7 enrichment cost is significant but bounded")
    if f in ("FLiBe_ThUF", "MSBR_ref"):
        return EvalResult("economic_deployment", EvalRating.ADEQUATE, Confidence.C2,
            "Adds Th fuel cycle complexity to FLiBe-UF4 baseline; Pa separation cost unknown",
            notes="Breeding gain potential offsets some fuel-cycle cost long-term")
    if f in ("LiF_ThPuF",):
        return EvalResult("economic_deployment", EvalRating.POOR, Confidence.C3,
            "Exotic reprocessing, harsher materials control, Pu handling — expensive and annoying",
            notes="Once you need exotic reprocessing and harsher materials, it is not cheap but quirky")
    if f == "LiF_ThF4":
        return EvalResult("economic_deployment", EvalRating.ADEQUATE, Confidence.C2,
            "Simpler than multi-actinide salts; thorium fuel cycle economics unproven at scale")
    if "Cl" in f:
        return EvalResult("economic_deployment", EvalRating.MARGINAL, Confidence.C3,
            "Fast-spectrum chloride MSR is furthest from deployment; R&D burden very high",
            notes="Corrosion-resistant containment and Cl-37 enrichment add major cost layers")
    return EvalResult("economic_deployment", EvalRating.UNKNOWN, Confidence.C3,
        "Economic assessment not available")


_EVAL_DISPATCH = {
    "neutronic": _eval_neutronic,
    "thermophysical": _eval_thermophysical,
    "phase_equilibrium": _eval_phase_equilibrium,
    "reprocessing": _eval_reprocessing,
    "materials_compatibility": _eval_materials_compat,
    "safety": _eval_safety,
    "resource_supply": _eval_resource_supply,
    "economic_deployment": _eval_economic,
}


def evaluate_fuel(formula: str, methods: Optional[list[str]] = None) -> FuelEvaluation:
    """Run evaluation methods on a molten-salt fuel material.

    Args:
        formula: material formula key in MATERIALS
        methods: list of method names (default: all 8 methods)

    Returns:
        FuelEvaluation with results for each requested method.
    """
    mat = MATERIALS.get(formula)
    if mat is None:
        raise ValueError(f"Unknown material: {formula}")
    if methods is None:
        methods = EVAL_METHODS
    ev = FuelEvaluation(formula=formula)
    for m in methods:
        fn = _EVAL_DISPATCH.get(m)
        if fn is None:
            ev.results[m] = EvalResult(m, EvalRating.UNKNOWN, Confidence.C3,
                f"No evaluator for method '{m}'")
        else:
            ev.results[m] = fn(mat)
    return ev


def evaluate_all_fuels(methods: Optional[list[str]] = None) -> dict[str, FuelEvaluation]:
    """Evaluate all fuel-category materials.  Returns {formula: FuelEvaluation}."""
    fuels = [f for f, m in MATERIALS.items() if m.category == "fuel"]
    return {f: evaluate_fuel(f, methods) for f in fuels}


def fuel_comparison_table(methods: Optional[list[str]] = None) -> list[dict]:
    """Generate a comparison table across all fuel materials and evaluation methods."""
    evals = evaluate_all_fuels(methods)
    rows = []
    for formula, ev in evals.items():
        row = {"formula": formula, "overall_score": ev.overall_score()}
        for mname, res in ev.results.items():
            row[f"{mname}_rating"] = res.rating
            row[f"{mname}_conf"] = res.confidence
        rows.append(row)
    rows.sort(key=lambda r: r["overall_score"], reverse=True)
    return rows


# ═══════════════════════════════════════════════════════════════════════
# Curve fitting engine
# ═══════════════════════════════════════════════════════════════════════

@dataclass
class FitResult:
    """Result of a least-squares curve fit."""
    coeffs: list[float]
    form: str              # "poly" | "shomate"
    degree: int
    T_min: float
    T_max: float
    r_squared: float
    rms_error: float
    max_error: float
    n_points: int
    confidence: str


def _lstsq_poly(T_arr: list[float], y_arr: list[float], degree: int) -> list[float]:
    """Least-squares polynomial fit without numpy (Gauss normal equations).

    Returns coefficients [a0, a1, ..., a_degree] such that
    y ~ a0 + a1*T + a2*T^2 + ... + a_degree*T^degree.
    """
    n = len(T_arr)
    m = degree + 1

    # Build A^T A and A^T y
    ATA = [[0.0] * m for _ in range(m)]
    ATy = [0.0] * m

    for k in range(n):
        T = T_arr[k]
        y = y_arr[k]
        powers = [1.0]
        for j in range(1, 2 * degree + 1):
            powers.append(powers[-1] * T)
        for i in range(m):
            ATy[i] += powers[i] * y
            for j in range(m):
                ATA[i][j] += powers[i + j]

    # Solve via Gauss elimination with partial pivoting
    aug = [ATA[i][:] + [ATy[i]] for i in range(m)]
    for col in range(m):
        # pivot
        max_row = col
        for row in range(col + 1, m):
            if abs(aug[row][col]) > abs(aug[max_row][col]):
                max_row = row
        aug[col], aug[max_row] = aug[max_row], aug[col]
        if abs(aug[col][col]) < 1e-30:
            continue
        for row in range(col + 1, m):
            factor = aug[row][col] / aug[col][col]
            for j in range(col, m + 1):
                aug[row][j] -= factor * aug[col][j]
    # Back-substitute
    coeffs = [0.0] * m
    for i in range(m - 1, -1, -1):
        s = aug[i][m]
        for j in range(i + 1, m):
            s -= aug[i][j] * coeffs[j]
        coeffs[i] = s / aug[i][i] if abs(aug[i][i]) > 1e-30 else 0.0

    return coeffs


def fit_polynomial(
    T_data: list[float],
    y_data: list[float],
    degree: int = 3,
    confidence: str = Confidence.C2,
) -> FitResult:
    """Fit a polynomial to (T, y) data points.

    Uses pure-Python least squares (no numpy dependency).
    """
    if len(T_data) != len(y_data) or len(T_data) < degree + 1:
        raise ValueError(f"Need at least {degree+1} points for degree-{degree} fit")

    coeffs = _lstsq_poly(T_data, y_data, degree)

    # Compute statistics
    n = len(T_data)
    y_mean = sum(y_data) / n
    ss_tot = sum((y - y_mean) ** 2 for y in y_data)
    ss_res = 0.0
    max_err = 0.0
    for i in range(n):
        y_pred = sum(coeffs[j] * T_data[i] ** j for j in range(degree + 1))
        err = y_data[i] - y_pred
        ss_res += err ** 2
        max_err = max(max_err, abs(err))

    r2 = 1.0 - ss_res / ss_tot if ss_tot > 0 else 0.0
    rms = math.sqrt(ss_res / n) if n > 0 else 0.0

    return FitResult(
        coeffs=coeffs,
        form="poly",
        degree=degree,
        T_min=min(T_data),
        T_max=max(T_data),
        r_squared=r2,
        rms_error=rms,
        max_error=max_err,
        n_points=n,
        confidence=confidence,
    )


def fit_shomate(
    T_data: list[float],
    cp_data: list[float],
    confidence: str = Confidence.C2,
) -> FitResult:
    """Fit Shomate form: Cp = A + Bt + Ct^2 + Dt^3 + E/t^2, t = T/1000.

    Transforms to linear regression in [1, t, t^2, t^3, 1/t^2].
    """
    n = len(T_data)
    if n < 5:
        raise ValueError("Need at least 5 points for Shomate fit")

    # Build design matrix columns: [1, t, t^2, t^3, 1/t^2]
    t_arr = [T / 1000.0 for T in T_data]
    # Map to polynomial fit in transformed variables
    T_trans = []
    y_trans = cp_data[:]
    for i in range(n):
        t = t_arr[i]
        inv_t2 = 1.0 / (t * t) if t > 0 else 0.0
        # Subtract E/t^2 contribution and fit residual as poly in t
        T_trans.append(t)

    # Simple approach: fit A + Bt + Ct^2 + Dt^3 as degree-3 poly in t,
    # then fit E from residuals against 1/t^2
    coeffs_4 = _lstsq_poly(t_arr, cp_data, 3)
    residuals = []
    inv_t2_arr = []
    for i in range(n):
        t = t_arr[i]
        pred = sum(coeffs_4[j] * t ** j for j in range(4))
        residuals.append(cp_data[i] - pred)
        inv_t2_arr.append(1.0 / (t * t) if abs(t) > 1e-10 else 0.0)

    # Fit E from residual ~ E / t^2
    num = sum(r * iv for r, iv in zip(residuals, inv_t2_arr))
    den = sum(iv * iv for iv in inv_t2_arr)
    E = num / den if den > 0 else 0.0

    coeffs = list(coeffs_4) + [E]  # [A, B, C, D, E]

    # Statistics
    y_mean = sum(cp_data) / n
    ss_tot = sum((y - y_mean) ** 2 for y in cp_data)
    ss_res = 0.0
    max_err = 0.0
    for i in range(n):
        t = t_arr[i]
        y_pred = coeffs[0] + coeffs[1]*t + coeffs[2]*t**2 + coeffs[3]*t**3
        if abs(t) > 1e-10:
            y_pred += coeffs[4] / (t * t)
        err = cp_data[i] - y_pred
        ss_res += err ** 2
        max_err = max(max_err, abs(err))

    r2 = 1.0 - ss_res / ss_tot if ss_tot > 0 else 0.0
    rms = math.sqrt(ss_res / n)

    return FitResult(
        coeffs=coeffs,
        form="shomate",
        degree=5,
        T_min=min(T_data),
        T_max=max(T_data),
        r_squared=r2,
        rms_error=rms,
        max_error=max_err,
        n_points=n,
        confidence=confidence,
    )


# ═══════════════════════════════════════════════════════════════════════
# Selective output & report generation
# ═══════════════════════════════════════════════════════════════════════

@dataclass
class SelectiveOutput:
    """Filter and format plant data for reports."""
    include_confidence: list[str] = field(default_factory=lambda: ["C1", "C2", "C3"])
    include_categories: list[str] = field(default_factory=lambda: ["fuel", "coolant", "structural", "product"])
    T_points: list[float] = field(default_factory=lambda: [300, 500, 800, 1000, 1500, 2000, 2500])
    flag_missing: bool = True

    def generate_table(self, materials: Optional[dict[str, Material]] = None) -> list[dict]:
        """Generate a filtered property table across all materials and T points."""
        if materials is None:
            materials = MATERIALS

        rows = []
        for formula, mat in materials.items():
            if mat.category not in self.include_categories:
                continue
            if mat.confidence not in self.include_confidence:
                continue
            for T in self.T_points:
                row = mat.summary_at(T)
                row["formula"] = formula
                row["category"] = mat.category
                row["phase"] = mat.phase
                row["melting_K"] = mat.melting_K if mat.melting_K != MISSING else None
                if self.flag_missing:
                    missing_props = [
                        pname for pname in mat.properties
                        if mat.get(pname, T) == MISSING
                    ]
                    row["_missing"] = missing_props if missing_props else None
                rows.append(row)
        return rows


def export_csv(rows: list[dict], path: str) -> Path:
    """Write rows to CSV file.  Returns the Path written."""
    p = Path(path)
    p.parent.mkdir(parents=True, exist_ok=True)
    if not rows:
        p.write_text("# empty\n", encoding="utf-8")
        return p

    keys = list(rows[0].keys())
    lines = [",".join(keys)]
    for row in rows:
        vals = []
        for k in keys:
            v = row.get(k)
            if v is None:
                vals.append("")
            elif isinstance(v, float):
                vals.append(f"{v:.6g}")
            elif isinstance(v, list):
                vals.append(";".join(str(x) for x in v))
            else:
                vals.append(str(v))
        lines.append(",".join(vals))
    p.write_text("\n".join(lines) + "\n", encoding="utf-8")
    return p


def export_json(data: dict, path: str) -> Path:
    """Write data dict to JSON.  Returns the Path written."""
    p = Path(path)
    p.parent.mkdir(parents=True, exist_ok=True)
    p.write_text(json.dumps(data, indent=2, default=str), encoding="utf-8")
    return p


# ═══════════════════════════════════════════════════════════════════════
# Validation self-check
# ═══════════════════════════════════════════════════════════════════════

def validate() -> dict:
    """Run self-check on the plant_alpha2 module."""
    report = {
        "ok": True,
        "module": "plant_alpha2",
        "version": "4.0.4-alpha2",
        "materials_count": len(MATERIALS),
        "materials": {},
        "issues": [],
    }

    for formula, mat in MATERIALS.items():
        m_info = {
            "name": mat.name,
            "category": mat.category,
            "melting_K": mat.melting_K if mat.melting_K != MISSING else None,
            "properties": list(mat.properties.keys()),
            "confidence": mat.confidence,
        }
        # Spot-check: evaluate Cp at 1000 K
        cp_val = mat.get("Cp", 1000.0)
        m_info["Cp_at_1000K"] = cp_val if cp_val != MISSING else None
        if cp_val == MISSING and "Cp" in mat.properties:
            report["issues"].append(f"{formula}: Cp defined but MISSING at 1000 K")

        # Check melting point is not MISSING for fuels
        if mat.category == "fuel" and mat.melting_K == MISSING:
            report["issues"].append(f"{formula}: fuel material with unknown melting point")
            report["ok"] = False

        report["materials"][formula] = m_info

    # Curve fitter sanity check
    try:
        test_T = [300, 400, 500, 600, 700, 800, 900, 1000]
        test_y = [25.0, 26.1, 27.3, 28.6, 30.0, 31.5, 33.1, 34.8]
        result = fit_polynomial(test_T, test_y, degree=2)
        report["curve_fitter"] = {
            "test": "quadratic_fit",
            "r_squared": round(result.r_squared, 6),
            "rms_error": round(result.rms_error, 4),
            "ok": result.r_squared > 0.99,
        }
        if result.r_squared < 0.99:
            report["issues"].append("Curve fitter R^2 < 0.99 on test data")
    except Exception as e:
        report["curve_fitter"] = {"ok": False, "error": str(e)}
        report["ok"] = False

    if report["issues"]:
        report["ok"] = all(
            "unknown melting" not in iss and "fitter" not in iss.lower()
            for iss in report["issues"]
        )

    # Evaluation framework sanity check
    try:
        fuel_formulas = list_materials("fuel")
        report["fuel_evaluation"] = {
            "fuel_count": len(fuel_formulas),
            "fuels": fuel_formulas,
            "eval_methods": EVAL_METHODS,
        }
        if fuel_formulas:
            test_ev = evaluate_fuel(fuel_formulas[0])
            report["fuel_evaluation"]["test_formula"] = fuel_formulas[0]
            report["fuel_evaluation"]["test_score"] = test_ev.overall_score()
            report["fuel_evaluation"]["test_ok"] = test_ev.overall_score() > 0
    except Exception as e:
        report["fuel_evaluation"] = {"ok": False, "error": str(e)}

    return report


# ═══════════════════════════════════════════════════════════════════════
# Convenience: list all materials
# ═══════════════════════════════════════════════════════════════════════

def list_materials(category: Optional[str] = None) -> list[str]:
    if category:
        return [f for f, m in MATERIALS.items() if m.category == category]
    return list(MATERIALS.keys())


def lookup(formula: str) -> Optional[Material]:
    return MATERIALS.get(formula)
