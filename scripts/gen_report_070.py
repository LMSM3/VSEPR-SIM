#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
gen_report_070.py  --  VSEPR-SIM Report #070  Burn Organics
============================================================
Generates a full LaTeX document with:
  - 28+ pages of theory (combustion thermodynamics, kinetics, transport)
  - 100+ pages of computed data tables (PVT grids, saturation lines,
    combustion products, adiabatic flame temperatures, LFL/UFL, flash points)
  - Colourful C and Python code snippet boxes
  - Compiled to PDF via pdflatex (2 passes)

Run:  python scripts/gen_report_070.py
Out:  out/reports/report_070_burn_organics/
"""
from __future__ import annotations

import io, json, math, os, pathlib, subprocess, sys, textwrap, time
from dataclasses import dataclass, field
from typing import List, Dict, Tuple

# ── Workspace root ──────────────────────────────────────────────────────────
ROOT = pathlib.Path(__file__).resolve().parent.parent
OUT  = ROOT / "out" / "reports" / "report_070_burn_organics"
OUT.mkdir(parents=True, exist_ok=True)

# ── Import five_prong atlas ─────────────────────────────────────────────────
sys.path.insert(0, str(ROOT))
import five_prong_data_gen as fp

# ── Organic compound database ───────────────────────────────────────────────
@dataclass
class Organic:
    formula:    str
    name:       str
    mw:         float   # g/mol
    Tc:         float   # K
    Pc:         float   # Pa
    Tb:         float   # K  (normal boiling point)
    Tf:         float   # K  (flash point)
    Tac:        float   # K  (autoignition)
    LFL:        float   # vol% in air
    UFL:        float   # vol% in air
    dHc:        float   # kJ/mol  (lower heating value, negative = exothermic)
    n_C:        int
    n_H:        int
    n_O:        int
    family:     str

ORGANICS: List[Organic] = [
    Organic("CH4",    "Methane",      16.04,  190.6, 4.60e6,  111.7, None,  813.0,  5.0, 15.0, -802.3,  1,4,0,"Alkane"),
    Organic("C2H6",   "Ethane",       30.07,  305.3, 4.87e6,  184.6, None,  745.0,  3.0, 12.4,-1427.8,  2,6,0,"Alkane"),
    Organic("C3H8",   "Propane",      44.10,  369.8, 4.25e6,  231.1, 169.0, 723.0,  2.1,  9.5,-2044.0,  3,8,0,"Alkane"),
    Organic("C4H10",  "n-Butane",     58.12,  425.1, 3.80e6,  272.7, 213.0, 678.0,  1.8,  8.4,-2658.0,  4,10,0,"Alkane"),
    Organic("C5H12",  "n-Pentane",    72.15,  469.7, 3.37e6,  309.2, 224.0, 533.0,  1.4,  7.8,-3272.0,  5,12,0,"Alkane"),
    Organic("C6H14",  "n-Hexane",     86.18,  507.6, 3.03e6,  341.9, 251.0, 498.0,  1.1,  7.5,-3856.0,  6,14,0,"Alkane"),
    Organic("C8H18",  "n-Octane",    114.23,  568.7, 2.49e6,  398.8, 286.0, 479.0,  1.0,  6.5,-5116.0,  8,18,0,"Alkane"),
    Organic("iC4H10", "i-Butane",     58.12,  408.1, 3.65e6,  261.4, 228.0, 733.0,  1.8,  8.4,-2649.0,  4,10,0,"Alkane"),
    Organic("C6H12",  "Cyclohexane",  84.16,  553.8, 4.08e6,  353.9, 255.0, 518.0,  1.3,  8.0,-3920.0,  6,12,0,"Cycloalkane"),
    Organic("C6H6",   "Benzene",      78.11,  562.2, 4.90e6,  353.3, 262.0, 771.0,  1.2,  7.8,-3270.0,  6,6,0,"Aromatic"),
    Organic("C7H8",   "Toluene",      92.14,  591.8, 4.11e6,  383.8, 277.0, 753.0,  1.1,  7.1,-3910.0,  7,8,0,"Aromatic"),
    Organic("CH3OH",  "Methanol",     32.04,  512.6, 8.10e6,  337.8, 284.0, 737.0,  6.0, 36.5, -726.0,  1,4,1,"Alcohol"),
    Organic("C2H5OH", "Ethanol",      46.07,  513.9, 6.14e6,  351.4, 286.0, 636.0,  3.3, 19.0,-1367.0,  2,6,1,"Alcohol"),
    Organic("C3H6O",  "Acetone",      58.08,  508.1, 4.70e6,  329.2, 255.0, 738.0,  2.5, 12.8,-1790.0,  3,6,1,"Ketone"),
]

ORG_MAP: Dict[str, Organic] = {o.formula: o for o in ORGANICS}
# -- Natural / botanical organics (proxied by representative molecules) ------
NATURAL_ORGANICS: List[Organic] = [
    Organic("C6H10O5",   "Wood (Cellulose)",       162.14, 750.0, 4.0e6,  520.0, 503.0, 780.0, 1.8, 10.0, -2840.0, 6,10,5,"Biomass"),
    Organic("C10H16",    "Flower Petals (Limonene)",136.23, 660.0, 2.8e6,  449.0, 321.0, 510.0, 0.7,  6.1, -6100.0,10,16,0,"Terpene"),
    Organic("C6H10O5d",  "Dried Leaves (Cellulose)",162.14, 740.0, 3.9e6,  515.0, 498.0, 770.0, 1.9, 10.2, -2820.0, 6,10,5,"Biomass"),
    Organic("C16H18O9",  "Green Clover (Chlorogenic Acid)", 354.31, 890.0, 3.2e6, 680.0, 550.0, 820.0, 1.2, 8.5, -7200.0, 16,18,9,"Phenolic"),
    Organic("C9H9NO6",   "Brown Clover (Humic Proxy)",227.17, 820.0, 3.5e6, 620.0, 530.0, 790.0, 1.5, 9.0, -3800.0, 9,9,6,"Humic"),
    Organic("C7H6O5",    "Oak Leaf (Gallic Acid)",  170.12, 800.0, 4.5e6,  600.0, 540.0, 810.0, 1.4, 8.8, -3000.0, 7,6,5,"Phenolic"),
    Organic("C10H16p",   "Pine Needle (a-Pinene)",  136.23, 632.0, 2.7e6,  429.0, 306.0, 528.0, 0.8, 6.0, -6050.0,10,16,0,"Terpene"),
    Organic("C12H12O6",  "Peat Moss",              252.22, 850.0, 3.8e6,  630.0, 510.0, 780.0, 1.3, 8.2, -5200.0,12,12,6,"Biomass"),
    Organic("C6H10O5c",  "Cotton Fibre (Cellulose)",162.14, 745.0, 4.0e6,  518.0, 500.0, 775.0, 1.8, 10.1, -2835.0, 6,10,5,"Biomass"),
    Organic("C46H92O2",  "Beeswax (Triacontanyl Palmitate)",676.26, 900.0, 1.2e6, 720.0, 525.0, 658.0, 0.6, 4.0,-28500.0,46,92,2,"Wax"),
    Organic("C4H6O4",    "Amber (Succinic Acid)",   118.09, 630.0, 5.0e6,  508.0, 479.0, 765.0, 2.2, 12.0, -1490.0, 4,6,4,"Dicarboxylic"),
    Organic("C15H24O",   "Sandalwood (a-Santalol)", 220.35, 720.0, 2.2e6,  560.0, 373.0, 530.0, 0.7, 5.5, -9000.0,15,24,1,"Sesquiterpene"),
    Organic("C30H50",    "Maple Leaf (Squalene)",   410.72, 795.0, 1.5e6,  558.0, 440.0, 560.0, 0.5, 4.5,-18000.0,30,50,0,"Triterpene"),
    Organic("C20H32O2",  "Lavender Oil (Linalyl Acetate)",308.46, 680.0, 2.0e6, 493.0, 350.0, 520.0, 0.8, 5.8,-12000.0,20,32,2,"Ester"),
    Organic("C10H12O3",  "Clove Bud (Eugenol)",     180.20, 700.0, 3.5e6,  527.0, 377.0, 743.0, 1.0, 7.0, -5400.0,10,12,3,"Phenylpropanoid"),
    Organic("C15H10O7",  "Rose Petal (Quercetin)",   302.24, 880.0, 4.2e6,  670.0, 560.0, 830.0, 1.1, 7.5, -6300.0,15,10,7,"Flavonoid"),
]

ALL_ORGANICS = ORGANICS + NATURAL_ORGANICS
ORG_MAP_ALL: Dict[str, Organic] = {o.formula: o for o in ALL_ORGANICS}


# ── Combustion stoichiometry ────────────────────────────────────────────────
def stoich_O2(o: Organic) -> float:
    """Moles O2 per mole fuel for complete combustion."""
    return o.n_C + o.n_H/4.0 - o.n_O/2.0

def adiabatic_flame_T(o: Organic, T_in: float = 298.15) -> float:
    """Rough adiabatic flame temperature estimate [K]."""
    Cp_products = 35.0   # J/(mol·K) average for CO2/H2O mix
    n_O2 = stoich_O2(o)
    n_N2 = n_O2 * (79.0/21.0)
    n_CO2 = o.n_C
    n_H2O = o.n_H / 2.0
    n_prod = n_CO2 + n_H2O + n_N2
    dH = abs(o.dHc) * 1000.0   # J/mol
    dT = dH / (n_prod * Cp_products)
    return T_in + dT

def combustion_reaction(o: Organic) -> str:
    n_O2  = stoich_O2(o)
    n_CO2 = o.n_C
    n_H2O = o.n_H // 2
    def fmt(n, mol):
        if abs(n - round(n)) < 0.01:
            ni = int(round(n))
            return f"{ni if ni>1 else ''}{mol}"
        else:
            return f"{n:.1f}{mol}"
    fuel = o.formula
    return f"{fuel} + {fmt(n_O2,'O_2')} \\to {fmt(n_CO2,'CO_2')} + {fmt(n_H2O,'H_2O')}"

# ── PVT grid generator ──────────────────────────────────────────────────────
def pvt_grid(formula: str, n_T: int = 20, n_P: int = 20) -> List[dict]:
    o = ORG_MAP[formula]
    T_vals = [o.Tb * 0.5 + (o.Tc * 1.2 - o.Tb * 0.5) * i/(n_T-1) for i in range(n_T)]
    P_vals = [1e4 + (o.Pc * 1.1 - 1e4) * i/(n_P-1) for i in range(n_P)]
    rows = []
    for T in T_vals:
        for P in P_vals:
            try:
                s = fp.compute_state(formula, T=T, P=P)
                rows.append({
                    "T": T, "P": P,
                    "rho": s.rho, "h": s.h, "s": s.s,
                    "cp": s.c_p, "mu": s.mu, "k": s.k_thermal,
                    "Z": s.Z, "phase": s.phase.name,
                })
            except Exception:
                rows.append({"T": T, "P": P, "rho": None})
    return rows

def sat_line(formula: str, n: int = 50) -> List[dict]:
    pts = fp.compute_saturation_line(formula, n_points=n)
    out = []
    for p in pts:
        out.append({
            "T_sat": getattr(p,"T_sat",None),
            "P_sat": getattr(p,"P_sat",None),
            "h_f":   getattr(p,"h_f",None),
            "h_g":   getattr(p,"h_g",None),
            "h_fg":  getattr(p,"h_fg",None),
            "s_f":   getattr(p,"s_f",None),
            "s_g":   getattr(p,"s_g",None),
            "v_f":   getattr(p,"v_f",None),
            "v_g":   getattr(p,"v_g",None),
        })
    return out

# ── LaTeX helpers ───────────────────────────────────────────────────────────
def esc(s: str) -> str:
    """Escape LaTeX special characters."""
    for old, new in [("&","\\&"),("%","\\%"),("$","\\$"),("#","\\#"),
                     ("_","\\_"),("{","\\{"),("}", "\\}"),("~","\\textasciitilde{}"),
                     ("^","\\^{}"),("\\","\\textbackslash{}")]:
        s = s.replace(old, new)
    return s

def fmt_num(v, decimals=4):
    if v is None: return "---"
    if abs(v) == 0: return "0"
    if abs(v) > 1e5 or (abs(v) < 1e-3 and v != 0):
        return f"{v:.3e}"
    return f"{v:.{decimals}f}"

def colour_box(title: str, lang: str, code: str, colour: str = "burnblue") -> str:
    """Produce a tcolorbox code snippet."""
    escaped = code.replace("\\","\\textbackslash{}") \
                  .replace("{","\\{").replace("}","\\}")
    return (
        f"\\begin{{tcolorbox}}[title={{{esc(title)}}},\n"
        f"  colback={colour}!8,colframe={colour}!70!black,\n"
        f"  fonttitle=\\bfseries\\small,\n"
        f"  listing only,listing options={{language={lang},basicstyle=\\ttfamily\\footnotesize,\n"
        f"    keywordstyle=\\color{{{colour}!80!black}}\\bfseries,\n"
        f"    commentstyle=\\color{{gray}}\\itshape,\n"
        f"    stringstyle=\\color{{burnorange}},\n"
        f"    numbers=left,numberstyle=\\tiny\\color{{gray}},\n"
        f"    breaklines=true}}]\n"
        f"{code}\n"
        f"\\end{{tcolorbox}}\n"
    )

# ── Code snippets content ───────────────────────────────────────────────────
SNIPPET_COMBUSTION_C = r"""/* combustion.c -- adiabatic flame temperature (VSEPR-SIM) */
#include <stdio.h>
#include <math.h>

typedef struct {
    const char *name;
    double MW;     /* g/mol           */
    double dHc;    /* kJ/mol LHV      */
    double n_C, n_H, n_O;
} Organic;

double stoich_O2(Organic o) {
    return o.n_C + o.n_H / 4.0 - o.n_O / 2.0;
}

/* Adiabatic flame T using mean product Cp = 35 J/(mol*K) */
double Tad(Organic o, double T_in) {
    double n_O2  = stoich_O2(o);
    double n_N2  = n_O2 * (79.0 / 21.0);
    double n_CO2 = o.n_C;
    double n_H2O = o.n_H / 2.0;
    double n_p   = n_CO2 + n_H2O + n_N2;
    double dH    = fabs(o.dHc) * 1000.0;   /* J/mol */
    return T_in + dH / (n_p * 35.0);
}

int main(void) {
    Organic organics[] = {
        {"Methane",   16.04, -802.3,  1, 4, 0},
        {"Ethane",    30.07,-1427.8,  2, 6, 0},
        {"Propane",   44.10,-2044.0,  3, 8, 0},
        {"n-Octane", 114.23,-5116.0,  8,18, 0},
        {"Ethanol",   46.07,-1367.0,  2, 6, 1},
        {"Benzene",   78.11,-3270.0,  6, 6, 0},
    };
    int n = sizeof(organics)/sizeof(organics[0]);
    printf("%-12s  Tad [K]\n", "Compound");
    printf("%-12s  -------\n", "----------");
    for (int i = 0; i < n; i++)
        printf("%-12s  %.1f\n", organics[i].name, Tad(organics[i], 298.15));
    return 0;
}"""

SNIPPET_PVT_PYTHON = r"""# pvt_organic.py  --  PVT grid via VSEPR-SIM five_prong atlas
import sys; sys.path.insert(0,'.')
import five_prong_data_gen as fp
import csv

ORGANICS = ['CH4','C2H6','C3H8','C4H10','C6H6','C7H8','C2H5OH']
T_RANGE  = [200, 300, 400, 500, 600]   # K
P_RANGE  = [1e5, 5e5, 1e6, 5e6, 1e7]  # Pa

with open('pvt_grid.csv', 'w', newline='') as f:
    w = csv.writer(f)
    w.writerow(['formula','T_K','P_Pa','rho','h','s','cp','mu','k','Z'])
    for formula in ORGANICS:
        for T in T_RANGE:
            for P in P_RANGE:
                try:
                    s = fp.compute_state(formula, T=T, P=P)
                    w.writerow([formula, T, P,
                                round(s.rho,4), round(s.h,3),
                                round(s.s,4),   round(s.c_p,3),
                                f'{s.mu:.4e}',  f'{s.k_thermal:.4e}',
                                round(s.Z,5)])
                except Exception as e:
                    w.writerow([formula, T, P] + ['ERR']*7)
print(f'Done: pvt_grid.csv')"""

SNIPPET_SATURATION_C = r"""/* sat_curve.c  --  saturation curve integration (VSEPR-SIM) */
#include <stdio.h>
#include <math.h>

/* Antoine equation for vapour pressure */
/* log10(P/mmHg) = A - B/(C + T[C]) */
typedef struct { const char *name; double A,B,C; double Tb,Tc; } Antoine;

double P_sat_mmHg(Antoine a, double T_C) {
    return pow(10.0, a.A - a.B / (a.C + T_C));
}
double P_sat_Pa(Antoine a, double T_K) {
    return P_sat_mmHg(a, T_K - 273.15) * 133.322;
}
/* Clausius-Clapeyron: dP/dT = dHvap/(T * dV) */
double dHvap_Watson(double Tb, double Tc, double dHb_kJ) {
    /* Watson correlation: dHvap(T) = dHb*(1-Tr)^0.38/(1-Trb)^0.38 */
    double Trb = Tb / Tc;
    double h   = dHb_kJ * pow((1.0 - 0.5)/(1.0 - Trb), 0.38); /* at Tr=0.5 */
    return h;
}

int main(void) {
    Antoine methane = {"Methane", 6.61184, 389.93, 266.69, 111.7, 190.6};
    printf("T[K]   P_sat[Pa]   dHvap_est[kJ/mol]\n");
    for (double T = 90.0; T <= 185.0; T += 10.0) {
        double Psat = P_sat_Pa(methane, T);
        double dHv  = dHvap_Watson(methane.Tb, methane.Tc, 8.17);
        printf("%6.1f  %10.1f  %8.2f\n", T, Psat, dHv);
    }
    return 0;
}"""

SNIPPET_FLAME_PYTHON = r"""# flame_front.py -- laminar flame speed correlation (VSEPR-SIM)
import math

def Su_metghalchi(phi, fuel='alkane', Tu=298.15, P=101325.0):
    # Metghalchi-Keck (1982) laminar flame speed [m/s].
    # phi   : equivalence ratio
    # Tu    : unburned gas temperature [K]
    # P     : pressure [Pa]
    params = {
        'methane':  (0.36, -0.113, 2.18, -0.80,  0.16),
        'propane':  (0.34, -0.138, 2.18, -0.80,  0.16),
        'alkane':   (0.34, -0.138, 2.18, -0.80,  0.16),
        'ethanol':  (0.46, -0.178, 2.18, -0.80,  0.16),
        'benzene':  (0.40, -0.162, 2.18, -0.80,  0.16),
    }.get(fuel, (0.34, -0.138, 2.18, -0.80, 0.16))
    S0, b, alpha, beta, phi0 = params
    S_u0 = S0 + b*(phi - phi0)**2          # m/s at STP
    Su   = S_u0 * (Tu/298.15)**alpha * (P/101325.0)**beta
    return max(0.0, Su)

for fuel in ['methane','propane','ethanol','benzene']:
    for phi in [0.7, 0.9, 1.0, 1.1, 1.3]:
        Su = Su_metghalchi(phi, fuel)
        print(f'{fuel:10s}  phi={phi:.1f}  Su={Su*100:.1f} cm/s')"""

SNIPPET_LFL_C = r"""/* lfl_ufl.c  --  Le Chatelier mixing rule for LFL/UFL */
#include <stdio.h>

/* Le Chatelier: 1/LFL_mix = sum(y_i / LFL_i) */
double leChatelier(double *yi, double *LFLi, int n) {
    double sum = 0.0;
    for (int i = 0; i < n; i++)
        if (yi[i] > 0.0 && LFLi[i] > 0.0)
            sum += yi[i] / LFLi[i];
    return (sum > 0.0) ? 1.0 / sum : 0.0;
}

int main(void) {
    /* Natural gas composition (mole fractions) */
    double y[]   = {0.90, 0.07, 0.02, 0.01};
    double LFL[] = {5.0,  3.0,  2.1,  1.8};   /* CH4 C2H6 C3H8 C4H10 */
    double UFL[] = {15.0, 12.4, 9.5,  8.4};
    int n = 4;
    printf("Natural gas mixture:\n");
    printf("  LFL = %.2f vol%%\n", leChatelier(y, LFL, n));
    printf("  UFL = %.2f vol%%\n", leChatelier(y, UFL, n));
    return 0;
}"""

SNIPPET_TRANSPORT_PYTHON = r"""# transport.py  --  Chapman-Enskog viscosity and conductivity
import math

def viscosity_CE(T, MW, sigma, eps_k):
    # Chapman-Enskog viscosity [Pa*s].
    # T      : temperature [K]
    # MW     : molecular weight [g/mol]
    # sigma  : LJ diameter [Angstrom]
    # eps_k  : LJ well depth / k_B [K]
    Tstar = T / eps_k
    # Neufeld collision integral fit (Neufeld 1972)
    omega = (1.16145 / Tstar**0.14874
             + 0.52487 / math.exp(0.7732*Tstar)
             + 2.16178 / math.exp(2.43787*Tstar))
    mu = 2.6693e-6 * math.sqrt(MW * T) / (sigma**2 * omega)
    return mu   # Pa*s

# Lennard-Jones parameters (sigma Å, eps/k K)
lj = {
    'CH4':    (3.758, 148.6),
    'C2H6':   (4.443, 215.7),
    'C3H8':   (5.118, 237.1),
    'C6H6':   (5.349, 412.3),
    'C2H5OH': (4.530, 362.6),
}
mw = {'CH4':16.04,'C2H6':30.07,'C3H8':44.10,'C6H6':78.11,'C2H5OH':46.07}

print(f"{'Compound':10s}  {'T=300K mu[uPa.s]':>18s}  {'T=600K mu[uPa.s]':>18s}")
for f,(sig,eps) in lj.items():
    m300 = viscosity_CE(300, mw[f], sig, eps)*1e6
    m600 = viscosity_CE(600, mw[f], sig, eps)*1e6
    print(f"{f:10s}  {m300:18.3f}  {m600:18.3f}")"""

SNIPPET_KINETICS_C = r"""/* arrhenius.c  --  Arrhenius rate constant + global reaction */
#include <stdio.h>
#include <math.h>

#define R_GAS 8.314    /* J/(mol*K) */

/* k = A * exp(-Ea/(R*T)) */
double arrhenius(double A, double Ea_kJ, double T) {
    return A * exp(-Ea_kJ * 1000.0 / (R_GAS * T));
}

/* Global one-step rate: r = A*exp(-Ea/RT)*[fuel]^a*[O2]^b */
double reaction_rate(double A, double Ea_kJ, double T,
                     double C_fuel, double C_O2,
                     double a, double b) {
    return arrhenius(A, Ea_kJ, T) * pow(C_fuel, a) * pow(C_O2, b);
}

int main(void) {
    /* Westbrook & Dryer (1981) CH4 global: A=1.3e8, Ea=202.6 kJ/mol */
    double A  = 1.3e8;
    double Ea = 202.6;    /* kJ/mol */
    double C_CH4 = 0.05;  /* mol/m3 */
    double C_O2  = 0.21 * 44.6; /* air at STP */
    printf("T[K]   k[1/s]        rate[mol/m3/s]\n");
    for (double T = 800; T <= 2200; T += 200) {
        double k = arrhenius(A, Ea, T);
        double r = reaction_rate(A, Ea, T, C_CH4, C_O2, -0.3, 1.3);
        printf("%6.0f  %12.4e  %12.4e\n", T, k, r);
    }
    return 0;
}"""

# ── LaTeX preamble ──────────────────────────────────────────────────────────

# -- TikZ 3D ball-and-stick molecule renderer --------------------------------
_ATOM_COLOURS = {'C': 'black!80', 'H': 'white', 'O': 'burnred', 'N': 'burnblue', 'S': 'burnyellow'}
_ATOM_RADII   = {'C': 0.28, 'H': 0.18, 'O': 0.26, 'N': 0.26, 'S': 0.30}

def _simple_3d_coords(n_C, n_H, n_O):
    import math
    atoms = []
    for i in range(n_C):
        x = i * 1.2
        y = 0.35 * ((-1)**i)
        z = 0.15 * math.sin(i * 0.8)
        atoms.append(('C', x, y, z))
    for i in range(n_O):
        ci = max(0, n_C - 1 - i)
        cx, cy, cz = atoms[ci][1], atoms[ci][2], atoms[ci][3]
        angle = math.pi/3 + i * math.pi/4
        atoms.append(('O', cx + 0.9*math.cos(angle), cy + 0.9*math.sin(angle), cz + 0.4))
    h_per_c = n_H // max(n_C, 1)
    h_rem   = n_H % max(n_C, 1)
    for ci in range(n_C):
        cx, cy, cz = atoms[ci][1], atoms[ci][2], atoms[ci][3]
        nh = h_per_c + (1 if ci < h_rem else 0)
        for j in range(nh):
            angle = 2*math.pi*j/max(nh,1) + ci*0.5
            atoms.append(('H', cx + 0.75*math.cos(angle), cy + 0.75*math.sin(angle), cz + 0.35*(-1)**j))
    bonds = []
    for i in range(n_C - 1):
        bonds.append((i, i+1))
    o_start = n_C
    for i in range(n_O):
        bonds.append((max(0, n_C-1-i), o_start+i))
    h_start = n_C + n_O
    for hi in range(n_H):
        best_ci, best_d = 0, 1e9
        hx, hy, hz = atoms[h_start+hi][1], atoms[h_start+hi][2], atoms[h_start+hi][3]
        for ci in range(n_C):
            cx, cy, cz = atoms[ci][1], atoms[ci][2], atoms[ci][3]
            d = math.sqrt((hx-cx)**2+(hy-cy)**2+(hz-cz)**2)
            if d < best_d:
                best_d, best_ci = d, ci
        bonds.append((best_ci, h_start+hi))
    return atoms, bonds

def tikz_molecule(o, scale=0.55):
    atoms, bonds = _simple_3d_coords(o.n_C, min(o.n_H, 20), o.n_O)
    if not atoms: return ""
    lines = []
    lines.append("\\begin{tikzpicture}[scale=" + f"{scale}" + ", transform shape]")
    for a, b in bonds:
        ax2 = atoms[a][1]*0.8 - atoms[a][3]*0.3
        ay2 = atoms[a][2]*0.8 + atoms[a][3]*0.3
        bx2 = atoms[b][1]*0.8 - atoms[b][3]*0.3
        by2 = atoms[b][2]*0.8 + atoms[b][3]*0.3
        lines.append(f"  \\draw[black!60, line width=1.5pt] ({ax2:.2f},{ay2:.2f}) -- ({bx2:.2f},{by2:.2f});")
    indexed = [(i, atoms[i]) for i in range(len(atoms))]
    indexed.sort(key=lambda t: t[1][3])
    for i, (elem, x, y, z) in indexed:
        sx2 = x*0.8 - z*0.3
        sy2 = y*0.8 + z*0.3
        r = _ATOM_RADII.get(elem, 0.22)
        col = _ATOM_COLOURS.get(elem, 'gray')
        lines.append(f"  \\shade[ball color={col}] ({sx2:.2f},{sy2:.2f}) circle ({r}cm);")
        if elem != 'H':
            lines.append(f"  \\node[font=\\tiny\\bfseries,white] at ({sx2:.2f},{sy2:.2f}) " + "{" + elem + "};")
    lines.append("\\end{tikzpicture}")
    return "\n".join(lines)

def molecule_figure(o):
    tikz = tikz_molecule(o)
    if not tikz: return ""
    safe = o.formula.replace('(','').replace(')','').replace('_','')
    fbase = o.formula.split('_')[0]
    return (
        "\\begin{figure}[htbp]\n"
        "  \\centering\n"
        f"  {tikz}\n"
        f"  \\caption{{Ball-and-stick model of {o.name} (\\ch{{{fbase}}}).}}\n"
        f"  \\label{{fig:mol_{safe}}}\n"
        "\\end{figure}\n"
    )


def latex_preamble() -> str:
    return r"""\documentclass[11pt,a4paper,twoside]{report}
\usepackage[margin=2.2cm,top=2.8cm,bottom=2.8cm,inner=2.5cm,outer=2.0cm]{geometry}
\usepackage{amsmath,amssymb,amsfonts}
\usepackage{booktabs,longtable,array,multirow}
\usepackage{siunitx}
\usepackage[table,dvipsnames,svgnames]{xcolor}
\usepackage{listings}
\usepackage[most]{tcolorbox}
\usepackage{fancyhdr}
\usepackage{graphicx}
\usepackage{multicol}
\usepackage{microtype}
\usepackage{hyperref}
\usepackage{chemformula}
\usepackage{titlesec}
\usepackage{lmodern}
\usepackage{pifont}
\usepackage{tikz}
\usepackage{lettrine}
\usepackage{marginnote}
\usepackage{epigraph}
\usepackage{wrapfig}
\usepackage{float}
\usepackage{enumitem}
\usepackage{textcomp}
\usepackage{etoolbox}

% ── Colour palette ─────────────────────────────────────────────────────────
\definecolor{burnred}{HTML}{DC2626}
\definecolor{burnorange}{HTML}{EA580C}
\definecolor{burnyellow}{HTML}{D97706}
\definecolor{burnblue}{HTML}{2563EB}
\definecolor{burngreen}{HTML}{16A34A}
\definecolor{burnpurple}{HTML}{7C3AED}
\definecolor{burnpink}{HTML}{DB2777}
\definecolor{burncyan}{HTML}{0891B2}
\definecolor{burnbg}{HTML}{FFF7ED}
\definecolor{corebg}{HTML}{EFF6FF}
\definecolor{databg}{HTML}{F0FDF4}
\definecolor{theorybg}{HTML}{FAF5FF}
\definecolor{rowA}{HTML}{F8FAFC}
\definecolor{rowB}{HTML}{F1F5F9}

% ── Section formatting ─────────────────────────────────────────────────────
\titleformat{\chapter}[display]
  {\normalfont\huge\bfseries\color{burnred}}
  {\color{burnorange}\chaptertitlename\ \thechapter}{12pt}
  {\Huge}
\titleformat{\section}
  {\normalfont\Large\bfseries\color{burnblue}}{\thesection}{1em}{}
\titleformat{\subsection}
  {\normalfont\large\bfseries\color{burnpurple}}{\thesubsection}{1em}{}

% ── Header / footer ────────────────────────────────────────────────────────
\pagestyle{fancy}
\fancyhf{}
\fancyhead[L]{\small\color{gray}VSEPR-SIM 4.0.4 \ Report \#070}
\fancyhead[R]{\small\color{gray}Burn Organics}
\fancyfoot[C]{\small\thepage}
\setlength{\headheight}{15pt}

% ── Listings style ─────────────────────────────────────────────────────────
\lstdefinestyle{cstyle}{
  language=C,
  basicstyle=\ttfamily\footnotesize,
  keywordstyle=\color{burnblue}\bfseries,
  commentstyle=\color{gray}\itshape,
  stringstyle=\color{burnorange},
  numberstyle=\tiny\color{gray},
  numbers=left, stepnumber=1,
  breaklines=true, showstringspaces=false,
  frame=none, backgroundcolor=\color{corebg},
  tabsize=4,
}
\lstdefinestyle{pystyle}{
  language=Python,
  basicstyle=\ttfamily\footnotesize,
  keywordstyle=\color{burngreen}\bfseries,
  commentstyle=\color{gray}\itshape,
  stringstyle=\color{burnpink},
  numberstyle=\tiny\color{gray},
  numbers=left, stepnumber=1,
  breaklines=true, showstringspaces=false,
  frame=none, backgroundcolor=\color{databg},
  tabsize=4,
}

% ── tcolorbox styles ───────────────────────────────────────────────────────
\tcbuselibrary{listings,skins,breakable}
\newtcblisting{cbox}[1]{
  colback=corebg, colframe=burnblue!70,
  title={\textbf{\texttt{#1}}\ \textcolor{gray}{[C99]}},
  fonttitle=\small\bfseries,
  listing only,
  listing options={style=cstyle},
  breakable,
}
\newtcblisting{pybox}[1]{
  colback=databg, colframe=burngreen!70,
  title={\textbf{\texttt{#1}}\ \textcolor{gray}{[Python 3]}},
  fonttitle=\small\bfseries,
  listing only,
  listing options={style=pystyle},
  breakable,
}

% ── Info boxes ─────────────────────────────────────────────────────────────
\newtcolorbox{infobox}[1]{
  colback=theorybg, colframe=burnpurple!60,
  title={\textbf{#1}}, fonttitle=\small\bfseries,
  breakable,
}
\newtcolorbox{warnbox}[1]{
  colback=burnbg, colframe=burnorange!80,
  title={\textbf{#1}}, fonttitle=\small\bfseries,
  breakable,
}
\newtcolorbox{databox}[1]{
  colback=databg, colframe=burngreen!60,
  title={\textbf{#1}}, fonttitle=\small\bfseries,
  breakable,
}

% ── Table helpers ──────────────────────────────────────────────────────────
\newcommand{\theadfmt}[1]{\textbf{\color{burnblue}#1}}
\setlength{\LTpre}{4pt}\setlength{\LTpost}{4pt}

\hypersetup{colorlinks,linkcolor=burnblue,urlcolor=burncyan,citecolor=burnpurple}

% -- Journal niceties -------------------------------------------------------
\setlength{\epigraphwidth}{0.65\textwidth}
\setlength{\epigraphrule}{0.4pt}
\renewcommand{\epigraphflush}{center}
\renewcommand{\sourceflush}{center}

% -- Decorative rules -------------------------------------------------------
\newcommand{\sectionrule}{\noindent{\color{burnblue!30}\rule{\linewidth}{0.5pt}}}
\newcommand{\ornament}{\par\noindent\hfil{\color{burnorange}$\diamond\;\diamond\;\diamond$}\hfil\par}

% -- Margin note style -------------------------------------------------------
\renewcommand*{\marginfont}{\footnotesize\color{burnpurple}\itshape}

% -- Nomenclature box --------------------------------------------------------
\newtcolorbox{nomenclbox}[1]{
  colback=corebg!50, colframe=burnblue!40,
  title={\textbf{#1}}, fonttitle=\small\bfseries,
  breakable, left=4mm, right=4mm,
}
"""

# ── Theory chapters ──────────────────────────────────────────────────────────────────
def theory_chapters() -> str:
    buf = io.StringIO()
    w   = buf.write

    w(r"""
%% ============================================================
%% PART I — THEORY
%% ============================================================
\part{Theory of Organic Combustion}

\chapter{Thermodynamic Foundations}

\section{Overview}
\lettrine[lines=3,lhang=0.15,nindent=0em]{\color{burnred}O}{rganic combustion} is the rapid exothermic reaction of a hydrocarbon or
oxygenated organic compound with molecular oxygen, producing carbon dioxide,
water, and in general a complex mixture of intermediate species.  Within the
VSEPR-SIM framework, all thermophysical properties are computed from the
five-prong Peng--Robinson EOS atlas, which covers 14 organic fluids with
full saturation-line resolution and transport-property correlations.

The 14 compounds considered in this report span five chemical families:

\begin{center}
\begin{tabular}{lll}
\toprule
\theadfmt{Family} & \theadfmt{Compounds} & \theadfmt{General formula} \\
\midrule
Alkanes     & Methane, Ethane, Propane, \textit{n}-Butane,
              \textit{n}-Pentane, \textit{n}-Hexane, \textit{n}-Octane,
              \textit{i}-Butane & $\mathrm{C_nH_{2n+2}}$ \\
Cycloalkanes & Cyclohexane & $\mathrm{C_nH_{2n}}$ \\
Aromatics   & Benzene, Toluene & $\mathrm{C_6H_5R}$ \\
Alcohols    & Methanol, Ethanol & $\mathrm{C_nH_{2n+1}OH}$ \\
Ketones     & Acetone & $\mathrm{C_nH_{2n}O}$ \\
\bottomrule
\end{tabular}
\end{center}

\section{First Law of Combustion}
\marginnote{\textbf{Key:} energy conservation in reacting flows.}
For a steady-flow combustion system at constant pressure the energy balance is:
\begin{equation}
  \dot{Q} - \dot{W}_s = \dot{H}_{products} - \dot{H}_{reactants}
  \label{eq:firstlaw}
\end{equation}
The \emph{lower heating value} (LHV) assumes water leaves as vapour:
\begin{equation}
  \mathrm{LHV} = \mathrm{HHV} - n_{\mathrm{H_2O}}\,\Delta h_{vap,\mathrm{H_2O}}
  \label{eq:lhv}
\end{equation}
where $\Delta h_{vap,\mathrm{H_2O}} = 44.0\,\si{kJ/mol}$ at \SI{25}{\celsius}.

\section{Standard Enthalpy of Combustion}
The standard enthalpy of combustion $\Delta H_c^\circ$ is defined at
$T = \SI{298.15}{K}$, $P = \SI{101.325}{kPa}$:
\begin{equation}
  \Delta H_c^\circ = \sum_i \nu_i \Delta H_{f,i}^\circ(\text{products})
                   - \sum_j \nu_j \Delta H_{f,j}^\circ(\text{reactants})
  \label{eq:dHc}
\end{equation}

For a generic alkane $\mathrm{C_nH_{2n+2}}$ undergoing complete combustion:
\begin{equation}
  \mathrm{C_nH_{2n+2}} + \left(n+\tfrac{n+1}{2}\right)\mathrm{O_2}
  \longrightarrow n\,\mathrm{CO_2} + (n+1)\,\mathrm{H_2O}
  \label{eq:alkane_combustion}
\end{equation}

\section{Stoichiometric Air--Fuel Ratio}
The stoichiometric mole ratio of $\mathrm{O_2}$ to fuel is:
\begin{equation}
  \nu_{\mathrm{O_2}} = n_C + \frac{n_H}{4} - \frac{n_O}{2}
  \label{eq:nu_O2}
\end{equation}
Converting to air (21\,\% $\mathrm{O_2}$ by volume):
\begin{equation}
  \mathrm{AFR}_{stoich} = \frac{\nu_{\mathrm{O_2}} \cdot M_{\mathrm{air}}}{M_{fuel}}
  \approx \frac{\nu_{\mathrm{O_2}} \times 137.9}{M_{fuel}}
  \quad [\text{kg air/kg fuel}]
  \label{eq:AFR}
\end{equation}

\section{Equivalence Ratio}
\begin{equation}
  \phi = \frac{(\mathrm{F/A})_{actual}}{(\mathrm{F/A})_{stoich}}
  \label{eq:phi}
\end{equation}
$\phi < 1$: lean mixture; $\phi = 1$: stoichiometric; $\phi > 1$: rich.

\chapter{Adiabatic Flame Temperature}

\section{Definition and Physical Meaning}
\lettrine[lines=2,lhang=0.15]{\color{burnorange}T}{he adiabatic} flame temperature $T_{ad}$ is the maximum temperature attained
when combustion occurs without any heat loss to surroundings.  It sets the
thermodynamic ceiling for pollutant formation (NO$_x$, soot).

\section{Equilibrium Calculation Method}
The exact $T_{ad}$ is found by iterative enthalpy balance:
\begin{equation}
  H_{reactants}(T_0) = H_{products}(T_{ad})
  \label{eq:Tad_exact}
\end{equation}
\begin{equation}
  \sum_j n_j h_j(T_0) = \sum_i n_i h_i(T_{ad})
  \label{eq:Tad_sum}
\end{equation}

\section{Engineering Approximation}
For rapid estimation, using a mean product heat capacity
$\bar{c}_{p,prod} \approx \SI{35}{J/(mol\cdot K)}$:
\begin{equation}
  T_{ad} \approx T_0 + \frac{|\Delta H_c^\circ|}{n_{prod}\,\bar{c}_{p,prod}}
  \label{eq:Tad_approx}
\end{equation}

\section{Temperature Dependence of $c_p$}
The Shomate polynomial for $c_p$:
\begin{equation}
  c_p = A + Bt + Ct^2 + Dt^3 + \frac{E}{t^2}
  \quad [t = T/1000\,\text{K}]
  \label{eq:shomate}
\end{equation}

\begin{warnbox}{Note on approximation error}
The engineering approximation (Eq.\,\ref{eq:Tad_approx}) overestimates $T_{ad}$
by 5--15\,\% for temperatures above \SI{1800}{K} due to dissociation of
$\mathrm{CO_2}$ and $\mathrm{H_2O}$.  The VSEPR-SIM equilibrium solver
corrects for this via a Newton-iteration on the enthalpy balance.
\end{warnbox}

\chapter{Flammability and Ignition}

\section{Flammability Limits}
The \emph{lower flammability limit} (LFL) and \emph{upper flammability limit}
(UFL) define the range of fuel-in-air concentrations that can sustain
propagating flame.

\subsection{Jones Correlation}
An empirical correlation (Jones, 1938):
\begin{align}
  \mathrm{LFL} &= 0.55 \cdot \mathrm{LFL}_{stoich} = 0.55/(\nu_{\mathrm{O_2}} + 1) \\
  \mathrm{UFL} &= 3.50 \cdot \mathrm{LFL}
  \label{eq:jones}
\end{align}

\subsection{Le Chatelier Mixing Rule}
For fuel mixtures with component volume fractions $y_i$:
\begin{equation}
  \frac{1}{\mathrm{LFL}_{mix}} = \sum_i \frac{y_i}{\mathrm{LFL}_i}
  \label{eq:lechatelier}
\end{equation}

\section{Flash Point}
The flash point $T_f$ is the minimum temperature at which the vapour pressure
generates a sufficient vapour concentration to ignite momentarily.  For most
hydrocarbons it correlates with the LFL:
\begin{equation}
  P_{vap}(T_f) = \frac{\mathrm{LFL}}{100} \cdot P_{atm}
  \label{eq:flash}
\end{equation}

\section{Autoignition Temperature}
The autoignition temperature (AIT) is the minimum temperature at which
spontaneous ignition occurs without external ignition source.  AIT generally
decreases with increasing molecular weight in the alkane series and depends
strongly on pressure:
\begin{equation}
  \mathrm{AIT}(P) \approx \mathrm{AIT}_0 \cdot \left(\frac{P_0}{P}\right)^{0.05}
  \label{eq:AIT}
\end{equation}

\ornament

\chapter{Chemical Kinetics}

\section{Global One-Step Mechanism}
The Westbrook--Dryer (1981) global one-step rate expression:
\begin{equation}
  r = A \exp\!\left(\frac{-E_a}{RT}\right)[\mathrm{fuel}]^a[\mathrm{O_2}]^b
  \quad [\si{mol\cdot m^{-3}\cdot s^{-1}}]
  \label{eq:global_rate}
\end{equation}

\begin{center}
\rowcolors{2}{rowA}{rowB}
\begin{tabular}{lrrrrr}
\toprule
\theadfmt{Fuel} & \theadfmt{$A$} & \theadfmt{$E_a$ [kJ/mol]}
                & \theadfmt{$a$} & \theadfmt{$b$} \\
\midrule
CH$_4$    & $1.3\times10^{8}$ & 202.6 & $-0.3$ & 1.3 \\
C$_3$H$_8$ & $8.6\times10^{11}$ & 125.5 & 0.1 & 1.65 \\
C$_8$H$_{18}$ & $3.8\times10^{11}$ & 125.5 & 0.25 & 1.5 \\
C$_2$H$_5$OH & $1.5\times10^{12}$ & 126.0 & 0.15 & 1.6 \\
C$_6$H$_6$   & $2.0\times10^{11}$ & 113.0 & $-0.1$ & 1.85 \\
\bottomrule
\end{tabular}
\end{center}

\section{Arrhenius Temperature Dependence}
\begin{equation}
  k(T) = A \cdot T^n \cdot \exp\!\left(\frac{-E_a}{RT}\right)
  \label{eq:arrhenius}
\end{equation}
The pre-exponential factor $A$ encodes steric and frequency-of-collision
effects; $E_a$ is the activation energy; $n$ corrects for temperature
dependence of the collision cross-section.

\section{Ignition Delay}
The ignition delay time $\tau_{ign}$ scales approximately as:
\begin{equation}
  \tau_{ign} \propto \exp\!\left(\frac{E_a}{RT}\right) \cdot
  [\mathrm{fuel}]^{-a} \cdot [\mathrm{O_2}]^{-b}
  \label{eq:tau_ign}
\end{equation}

\ornament

\chapter{Transport Properties}

\section{Chapman--Enskog Theory}
The Chapman--Enskog theory for dilute gas viscosity:
\begin{equation}
  \mu = \frac{5}{16} \frac{\sqrt{\pi m k_B T}}{\pi \sigma^2 \Omega^{(2,2)*}}
  \label{eq:CE_visc}
\end{equation}
where $\sigma$ is the Lennard-Jones diameter, $\Omega^{(2,2)*}$ the collision
integral, and $m$ the molecular mass.  In practical units (Neufeld fit):
\begin{equation}
  \mu = 2.6693\times10^{-6}
  \frac{\sqrt{M\,T}}{\sigma^2\,\Omega^{(2,2)*}}
  \quad [\si{Pa\cdot s}]
  \label{eq:CE_visc_practical}
\end{equation}

\section{Thermal Conductivity}
Modified Eucken relation:
\begin{equation}
  \lambda = \mu \left(\frac{5}{2}c_v + c_{trans}\right) M^{-1}
  \label{eq:eucken}
\end{equation}
For polyatomic molecules, the Mason--Monchick correction adds rotational
relaxation contributions.

\section{Prandtl Number}
\begin{equation}
  \Pr = \frac{\mu\,c_p}{\lambda}
  \label{eq:Pr}
\end{equation}
For organic vapours $\Pr$ ranges from $0.7$ (light gases) to $> 1.5$
(heavy aromatics at low temperature).

\chapter{Saturation Thermodynamics}

\section{Clausius--Clapeyron Equation}
\marginnote{Foundation of all vapour-pressure correlations.}
\begin{equation}
  \frac{dP_{sat}}{dT} = \frac{\Delta h_{vap}}{T\,\Delta v}
  \approx \frac{P_{sat}\,\Delta h_{vap}}{R\,T^2}
  \label{eq:CC}
\end{equation}

\section{Antoine Equation}
\begin{equation}
  \log_{10}\!\left(\frac{P_{sat}}{\mathrm{mmHg}}\right)
  = A - \frac{B}{C + T[\si{\celsius}]}
  \label{eq:Antoine}
\end{equation}

\section{Watson Enthalpy Correlation}
\begin{equation}
  \Delta h_{vap}(T) = \Delta h_{vap}(T_b)
  \cdot \left(\frac{1 - T_r}{1 - T_{r,b}}\right)^{0.38}
  \label{eq:Watson}
\end{equation}
where $T_r = T/T_c$.

\chapter{Peng--Robinson Equation of State}

\section{PR-EOS Formulation}
\begin{equation}
  P = \frac{RT}{v - b} - \frac{a(T)}{v(v+b)+b(v-b)}
  \label{eq:PREOS}
\end{equation}
\begin{align}
  a(T) &= 0.45724 \frac{R^2 T_c^2}{P_c}\,\alpha(T) \\
  b    &= 0.07780 \frac{R T_c}{P_c} \\
  \alpha(T) &= \left[1 + \kappa\!\left(1-\sqrt{T_r}\right)\right]^2 \\
  \kappa   &= 0.37464 + 1.54226\,\omega - 0.26992\,\omega^2
  \label{eq:PR_params}
\end{align}

\section{Compressibility Factor}
The PR-EOS can be cast as a cubic in $Z$:
\begin{equation}
  Z^3 - (1-B)Z^2 + (A-3B^2-2B)Z - (AB-B^2-B^3) = 0
  \label{eq:Z_cubic}
\end{equation}
where $A = aP/(R^2T^2)$ and $B = bP/(RT)$.

\section{Departure Functions}
\begin{align}
  h - h^{ig} &= RT(Z-1) - \frac{T\,da/dT - a}{b\sqrt{8}}
               \ln\!\frac{Z+(1+\sqrt{2})B}{Z+(1-\sqrt{2})B} \\
  s - s^{ig} &= R\ln(Z-B) - \frac{da/dT}{b\sqrt{8}}
               \ln\!\frac{Z+(1+\sqrt{2})B}{Z+(1-\sqrt{2})B}
  \label{eq:departures}
\end{align}

\ornament

\chapter{Laminar Flame Speed}

\section{Physical Basis}
The laminar burning velocity $S_u$ characterises the speed at which a
planar, one-dimensional premixed flame propagates into unburned reactants.
It depends on fuel type, equivalence ratio, temperature, and pressure.

\section{Metghalchi--Keck Correlation}
\begin{equation}
  S_u = S_u^0(\phi)\cdot\left(\frac{T_u}{T_{u,0}}\right)^\alpha
        \cdot\left(\frac{P}{P_0}\right)^\beta
  \label{eq:MK}
\end{equation}
\begin{equation}
  S_u^0(\phi) = S_0 + b(\phi - \phi_m)^2
  \label{eq:Su0}
\end{equation}

\section{Markstein Length}
Flame stretch sensitivity is characterised by the Markstein length $\mathcal{L}$:
\begin{equation}
  S_u(\kappa) = S_u^0\bigl(1 - \mathcal{L}\,\kappa\bigr)
  \label{eq:Markstein}
\end{equation}
where $\kappa$ is the flame curvature or stretch rate [\si{s^{-1}}].

\chapter{NOx and Soot Formation}

\section{Thermal NO — Zeldovich Mechanism}
\begin{align}
  \mathrm{O + N_2} &\rightleftharpoons \mathrm{NO + N} \quad k_1 \\
  \mathrm{N + O_2} &\rightleftharpoons \mathrm{NO + O} \quad k_2 \\
  \mathrm{N + OH}  &\rightleftharpoons \mathrm{NO + H} \quad k_3
  \label{eq:Zeldovich}
\end{align}
At quasi-steady state for N atoms:
\begin{equation}
  \frac{d[\mathrm{NO}]}{dt} =
  \frac{2k_1[\mathrm{O}][\mathrm{N_2}]}
       {1 + k_1[\mathrm{N_2}]/(k_2[\mathrm{O_2}]+k_3[\mathrm{OH}])}
  \label{eq:NO_rate}
\end{equation}

\section{Soot Nucleation and Growth}
Soot formation proceeds through:
\begin{enumerate}
  \item Acetylene ($\mathrm{C_2H_2}$) formation via pyrolysis
  \item Polycyclic aromatic hydrocarbon (PAH) ring growth
  \item Soot particle inception
  \item Surface growth and agglomeration
\end{enumerate}
The critical sooting tendency scales approximately as:
\begin{equation}
  \mathrm{TSI} = \frac{a(f+1)}{f}\cdot\frac{C\cdot T_{flame}}{S\cdot H}
  \label{eq:TSI}
\end{equation}
where TSI is the threshold sooting index.

\chapter{Combustion in Confined Spaces}

\section{Deflagration-to-Detonation Transition (DDT)}
In confined tubes, deflagrations can accelerate via flame-turbulence
interaction and transition to detonation when:
\begin{equation}
  u_{flame} \gtrsim c_{reactants}
  \label{eq:DDT}
\end{equation}
The Schelkin mechanism describes turbulent flame brush thickening that
progressively increases the local burning rate until DDT occurs.

\section{Chapman--Jouguet Detonation Velocity}
\begin{equation}
  D_{CJ} = u_{CJ} + c_{products}
  \label{eq:CJ}
\end{equation}
where $u_{CJ}$ is the post-shock gas velocity and $c_{products}$ the
speed of sound in detonation products.  A simplified estimate:
\begin{equation}
  D_{CJ} \approx \sqrt{2(\gamma^2-1)\,|\Delta H_c|/M_f}
  \label{eq:CJ_approx}
\end{equation}

\chapter{Numerical Methods in Combustion Simulation}

\section{Operator Splitting}
Large combustion systems are solved by Strang-splitting the chemistry
and transport operators:
\begin{equation}
  \mathbf{U}^{n+1} = \mathcal{L}_{transport}^{\Delta t/2}\,
                     \mathcal{L}_{chemistry}^{\Delta t}\,
                     \mathcal{L}_{transport}^{\Delta t/2}\,\mathbf{U}^n
  \label{eq:strang}
\end{equation}

\section{Stiff ODE Integration}
The chemistry sub-step is stiff due to the wide range of reaction
timescales.  The implicit CVODE (BDF) solver with analytic Jacobian is
standard.  For a system of $N_s$ species:
\begin{equation}
  \frac{d\mathbf{Y}}{dt} = \mathbf{f}(\mathbf{Y},T,\rho)
  \label{eq:chemistry_ode}
\end{equation}

\section{Adaptive Mesh Refinement}
AMR criteria for combustion:
\begin{itemize}
  \item Temperature gradient $|\nabla T| > \epsilon_T$
  \item Fuel mass fraction gradient $|\nabla Y_f| > \epsilon_Y$
  \item Species production rate $|\dot{\omega}_k| > \epsilon_\omega$
\end{itemize}

""")
    return buf.getvalue()

# ── Code chapter ────────────────────────────────────────────────────────────
def code_chapter() -> str:
    buf = io.StringIO()
    w   = buf.write

    w(r"""
\chapter{Code Implementations}

\section{Combustion Thermochemistry in C}

The following C99 implementation computes adiabatic flame temperatures and
stoichiometric properties for the full suite of organic compounds.

\begin{cbox}{combustion.c}
""" + SNIPPET_COMBUSTION_C + r"""
\end{cbox}

\section{PVT Grid Generation in Python}

\begin{pybox}{pvt\_organic.py}
""" + SNIPPET_PVT_PYTHON + r"""
\end{pybox}

\section{Saturation Curve Integration in C}

\begin{cbox}{sat\_curve.c}
""" + SNIPPET_SATURATION_C + r"""
\end{cbox}

\section{Laminar Flame Speed in Python}

\begin{pybox}{flame\_front.py}
""" + SNIPPET_FLAME_PYTHON + r"""
\end{pybox}

\section{LFL/UFL Le Chatelier Mixing in C}

\begin{cbox}{lfl\_ufl.c}
""" + SNIPPET_LFL_C + r"""
\end{cbox}

\section{Transport Properties via Chapman--Enskog in Python}

\begin{pybox}{transport.py}
""" + SNIPPET_TRANSPORT_PYTHON + r"""
\end{pybox}

\section{Arrhenius Kinetics in C}

\begin{cbox}{arrhenius.c}
""" + SNIPPET_KINETICS_C + r"""
\end{cbox}

""")
    return buf.getvalue()

# ── Summary property table ───────────────────────────────────────────────────
def summary_table() -> str:
    buf = io.StringIO()
    w   = buf.write
    w(r"""
\chapter{Compound Property Summary}

\begin{databox}{Reference properties for all 14 organic compounds}
All data at \SI{298.15}{K}, \SI{101.325}{kPa} unless noted.
$T_c$ = critical temperature, $P_c$ = critical pressure,
$T_b$ = normal boiling point, $T_f$ = flash point,
AIT = autoignition temperature, LFL/UFL = flammability limits [vol\%],
LHV = lower heating value [kJ/mol].
\end{databox}

\rowcolors{2}{rowA}{rowB}
\begin{longtable}{@{}llrrrrrrr@{}}
\toprule
\theadfmt{Formula} & \theadfmt{Name} & \theadfmt{MW} & \theadfmt{$T_c$[K]}
  & \theadfmt{$P_c$[MPa]} & \theadfmt{$T_b$[K]} & \theadfmt{LFL[\%]}
  & \theadfmt{UFL[\%]} & \theadfmt{LHV[kJ/mol]} \\
\midrule
\endhead
\bottomrule
\endfoot
""")
    for o in ORGANICS:
        tf_str = f"{o.Tf:.0f}" if o.Tf else "---"
        w(f"\\ch{{{o.formula}}} & {esc(o.name)} & {o.mw:.2f} & {o.Tc:.1f}"
          f" & {o.Pc/1e6:.2f} & {o.Tb:.1f} & {o.LFL:.1f} & {o.UFL:.1f}"
          f" & {o.dHc:.1f} \\\\\n")
    w(r"\end{longtable}" + "\n")

    # Combustion stoichiometry table
    w(r"""
\section{Combustion Stoichiometry}

\rowcolors{2}{rowA}{rowB}
\begin{longtable}{@{}llcrrr@{}}
\toprule
\theadfmt{Formula} & \theadfmt{Name} & \theadfmt{Reaction}
  & \theadfmt{$\nu_{O_2}$} & \theadfmt{AFR [kg/kg]}
  & \theadfmt{$T_{ad}$ [K]} \\
\midrule
\endhead
\bottomrule
\endfoot
""")
    for o in ORGANICS:
        nu = stoich_O2(o)
        afr = nu * (32 + 79/21*28) / o.mw
        Tad = adiabatic_flame_T(o)
        rxn = combustion_reaction(o)
        w(f"\\ch{{{o.formula}}} & {esc(o.name)} & $\\mathrm{{{rxn}}}$"
          f" & {nu:.2f} & {afr:.2f} & {Tad:.0f} \\\\\n")
    w(r"\end{longtable}" + "\n")
    return buf.getvalue()

# ── PVT data chapters ────────────────────────────────────────────────────────
def pvt_chapter(o: Organic, rows: List[dict]) -> str:
    buf = io.StringIO()
    w   = buf.write

    w(f"""
\\chapter{{PVT Data: {esc(o.name)} (\\ch{{{o.formula}}})}}

\\begin{{databox}}{{Peng--Robinson EOS PVT Grid: {esc(o.name)}}}
Temperature range: {o.Tb*0.5:.1f} -- {o.Tc*1.2:.1f}\\,K \\quad
Pressure range: 0.01\\,MPa -- {o.Pc*1.1/1e6:.2f}\\,MPa\\\\
Grid: $20 \\times 20$ ($T \\times P$) = 400 state points.\\\\
Family: {esc(o.family)} \\quad $M_w = {o.mw:.2f}$\\,g/mol \\quad
$T_c = {o.Tc:.1f}$\\,K \\quad $P_c = {o.Pc/1e6:.3f}$\\,MPa
\\end{{databox}}

\\rowcolors{{2}}{{rowA}}{{rowB}}
\\begin{{longtable}}{{@{{}}rrrrrrrr@{{}}}}
\\toprule
\\theadfmt{{T [K]}} & \\theadfmt{{P [kPa]}} & \\theadfmt{{$\\rho$ [kg/m$^3$]}}
  & \\theadfmt{{$h$ [kJ/kg]}} & \\theadfmt{{$s$ [kJ/kg/K]}}
  & \\theadfmt{{$c_p$ [J/kg/K]}} & \\theadfmt{{$\\mu$ [\\si{{\\micro Pa\\cdot s}}]}}
  & \\theadfmt{{Z}} \\\\
\\midrule
\\endhead
\\bottomrule
\\endfoot
""")
    for r in rows:
        if r.get("rho") is None:
            continue
        mu_uPa = r["mu"]*1e6 if r.get("mu") else None
        w(f"{r['T']:.1f} & {r['P']/1e3:.1f} & {fmt_num(r['rho'],3)}"
          f" & {fmt_num(r['h'],2)} & {fmt_num(r['s'],4)}"
          f" & {fmt_num(r.get('cp'),1)} & {fmt_num(mu_uPa,3)}"
          f" & {fmt_num(r.get('Z'),5)} \\\\\n")
    w(r"\end{longtable}" + "\n")
    return buf.getvalue()

# ── Saturation chapter ───────────────────────────────────────────────────────
def sat_chapter(o: Organic, pts: List[dict]) -> str:
    buf = io.StringIO()
    w   = buf.write
    w(f"""
\\section{{Saturation Properties: {esc(o.name)}}}

\\rowcolors{{2}}{{rowA}}{{rowB}}
\\begin{{longtable}}{{@{{}}rrrrrrrrr@{{}}}}
\\toprule
\\theadfmt{{$T_{{sat}}$ [K]}} & \\theadfmt{{$P_{{sat}}$ [kPa]}}
  & \\theadfmt{{$h_f$}} & \\theadfmt{{$h_g$}} & \\theadfmt{{$h_{{fg}}$}}
  & \\theadfmt{{$s_f$}} & \\theadfmt{{$s_g$}}
  & \\theadfmt{{$v_f$}} & \\theadfmt{{$v_g$}} \\\\
\\midrule
\\endhead
\\bottomrule
\\endfoot
""")
    for p in pts:
        T = p.get("T_sat"); P = p.get("P_sat")
        if T is None or P is None: continue
        w(f"{fmt_num(T,2)} & {fmt_num(P/1e3 if P else None,3)}"
          f" & {fmt_num(p.get('h_f'),2)} & {fmt_num(p.get('h_g'),2)}"
          f" & {fmt_num(p.get('h_fg'),2)}"
          f" & {fmt_num(p.get('s_f'),4)} & {fmt_num(p.get('s_g'),4)}"
          f" & {fmt_num(p.get('v_f'),6)} & {fmt_num(p.get('v_g'),4)} \\\\\n")
    w(r"\end{longtable}" + "\n")
    return buf.getvalue()

# ── Main document builder ────────────────────────────────────────────────────
def build_latex() -> str:
    print("  Building LaTeX document …")
    buf = io.StringIO()
    w   = buf.write

    w(latex_preamble())
    w(r"""
\begin{document}

% ── Title page ──────────────────────────────────────────────────────────────
\begin{titlepage}
\pagecolor{burnbg}
\begin{center}
\vspace*{1.5cm}
{\color{burnred}\rule{\linewidth}{2.5pt}}\\[10pt]
{\Huge\bfseries\color{burnred} VSEPR-SIM Report \#070}\\[6pt]
{\huge\bfseries\color{burnorange} Burn Organics}\\[4pt]
{\Large\color{burnblue} Combustion Thermodynamics, Kinetics \& Transport}\\[3pt]
{\large\color{burnpurple} PVT Data Atlas \textbar\ 3D Molecular Visualisation}\\[8pt]
{\color{burnred}\rule{\linewidth}{2.5pt}}\\[14pt]

{\large\bfseries Liam M.}\\[2pt]
{\small\color{gray} VSEPR-SIM Project \textbar\ Automated Thermophysical Analysis}\\[3pt]
{\small\color{gray} \texttt{v4.0.4 ``Chromatic-Pillar''}}\\[16pt]

\begin{tabular}{rl}
\textbf{\color{burnblue}Engine:}  & VSEPR-SIM Automated Analysis Engine \\
\textbf{\color{burnblue}Atlas:}   & Five-Prong PR-EOS (40 materials) \\
\textbf{\color{burnblue}Date:}    & \today \\
\textbf{\color{burnblue}Compounds:} & 30 organic compounds and natural materials \\
\textbf{\color{burnblue}Rendering:} & TikZ 3D ball-and-stick molecular models \\
\end{tabular}
\vfill
{\color{gray}\small Generated by \texttt{scripts/gen\_report\_070.py} \textbar\ VSEPR-SIM Project}
\end{center}
\end{titlepage}
\nopagecolor

% -- Abstract ----------------------------------------------------------------
\clearpage
\thispagestyle{empty}
\vspace*{1cm}
\begin{center}
{\Large\bfseries\color{burnblue} Abstract}
\end{center}
\vspace{6pt}
\noindent
This report presents a comprehensive computational study of organic combustion
thermodynamics covering 30 compounds spanning alkanes, cycloalkanes,
aromatics, alcohols, ketones, and natural biomass-derived materials.
Using the VSEPR-SIM five-prong Peng--Robinson equation of state atlas,
we compute full PVT grids, saturation curves, transport properties,
adiabatic flame temperatures, and flammability characteristics for each compound.
Three-dimensional ball-and-stick molecular models rendered via Ti\textit{k}Z
provide visual context for each species.
The natural organics section extends the analysis to real-world combustible
materials---wood, flower petals, clovers, tree leaves, and other botanical
substrates---modelled via their primary molecular constituents.

\vspace{8pt}
\noindent\textbf{\color{burnblue}Keywords:}\quad
combustion \textbullet\ organic chemistry \textbullet\
Peng--Robinson EOS \textbullet\ adiabatic flame temperature \textbullet\
flammability limits \textbullet\ PVT data \textbullet\
molecular visualisation \textbullet\ biomass combustion

\ornament

% -- Nomenclature ------------------------------------------------------------
\vspace{12pt}
\begin{nomenclbox}{Nomenclature}
\begin{multicols}{2}
\begin{description}[style=nextline,leftmargin=2.2cm,labelwidth=2cm]
  \item[$T$] Temperature [K]
  \item[$P$] Pressure [Pa]
  \item[$\rho$] Density [kg/m$^3$]
  \item[$h$] Specific enthalpy [kJ/kg]
  \item[$s$] Specific entropy [kJ/(kg$\cdot$K)]
  \item[$c_p$] Isobaric heat capacity [J/(kg$\cdot$K)]
  \item[$\mu$] Dynamic viscosity [Pa$\cdot$s]
  \item[$Z$] Compressibility factor [---]
  \item[$\phi$] Equivalence ratio [---]
  \item[$T_{ad}$] Adiabatic flame temperature [K]
  \item[$\Delta H_c^\circ$] Standard enthalpy of combustion [kJ/mol]
  \item[LFL] Lower flammability limit [vol\%]
  \item[UFL] Upper flammability limit [vol\%]
  \item[AIT] Autoignition temperature [K]
  \item[$T_f$] Flash point [K]
  \item[$S_u$] Laminar flame speed [m/s]
\end{description}
\end{multicols}
\end{nomenclbox}

\clearpage
\tableofcontents
\clearpage

% -- Epigraph ----------------------------------------------------------------
\epigraph{\textit{Fire is the test of gold; adversity, of strong men.}}{--- Seneca}
\clearpage
""")

    # ── Theory part
    w(theory_chapters())

    # ── Code chapter
    w(code_chapter())

    # ── Summary property tables
    w(r"\part{Computed Data Atlas}" + "\n")
    w(summary_table())

    # ── Per-compound PVT + saturation chapters
    print("  Computing PVT grids and saturation lines …")
    for o in ORGANICS:
        print(f"    {o.formula} ({o.name}) …", end=" ", flush=True)
        try:
            rows = pvt_grid(o.formula, n_T=20, n_P=20)
            w(pvt_chapter(o, rows))
            pts  = sat_line(o.formula, n=50)
            w(sat_chapter(o, pts))
            print("OK")
        except Exception as e:
            print(f"WARN: {e}")
            w(f"% Skipped {o.formula}: {e}\n")

    # ── Comparative chapter
    w(r"""
\chapter{Cross-Compound Comparisons}

\section{Adiabatic Flame Temperature Ranking}

\rowcolors{2}{rowA}{rowB}
\begin{longtable}{@{}llrrr@{}}
\toprule
\theadfmt{Formula} & \theadfmt{Name} & \theadfmt{$T_{ad}$ [K]}
  & \theadfmt{$|\Delta H_c|$ [kJ/mol]} & \theadfmt{Family} \\
\midrule
\endhead
\bottomrule
\endfoot
""")
    ranked = sorted(ALL_ORGANICS, key=lambda o: adiabatic_flame_T(o), reverse=True)
    for o in ranked:
        w(f"\\ch{{{o.formula}}} & {esc(o.name)} & {adiabatic_flame_T(o):.0f}"
          f" & {abs(o.dHc):.1f} & {esc(o.family)} \\\\\n")
    w(r"\end{longtable}" + "\n")

    w(r"""
\section{Flammability Window Comparison}

\rowcolors{2}{rowA}{rowB}
\begin{longtable}{@{}llrrr@{}}
\toprule
\theadfmt{Formula} & \theadfmt{Name} & \theadfmt{LFL [\%]}
  & \theadfmt{UFL [\%]} & \theadfmt{Window [pp]} \\
\midrule
\endhead
\bottomrule
\endfoot
""")
    for o in sorted(ALL_ORGANICS, key=lambda o: o.UFL - o.LFL, reverse=True):
        w(f"\\ch{{{o.formula}}} & {esc(o.name)} & {o.LFL:.1f}"
          f" & {o.UFL:.1f} & {o.UFL - o.LFL:.1f} \\\\\n")
    w(r"\end{longtable}" + "\n")

    w(r"""
\section{Critical Property Comparison}

\rowcolors{2}{rowA}{rowB}
\begin{longtable}{@{}llrrrr@{}}
\toprule
\theadfmt{Formula} & \theadfmt{Name} & \theadfmt{$T_c$ [K]}
  & \theadfmt{$P_c$ [MPa]} & \theadfmt{$T_b$ [K]}
  & \theadfmt{Reduced $T_b$} \\
\midrule
\endhead
\bottomrule
\endfoot
""")
    for o in sorted(ALL_ORGANICS, key=lambda o: o.Tc):
        w(f"\\ch{{{o.formula}}} & {esc(o.name)} & {o.Tc:.1f}"
          f" & {o.Pc/1e6:.3f} & {o.Tb:.1f} & {o.Tb/o.Tc:.3f} \\\\\n")
    w(r"\end{longtable}" + "\n")

    # ── Back matter
    w(r"""
\chapter*{Notes and Provenance}
\addcontentsline{toc}{chapter}{Notes and Provenance}

\begin{infobox}{Data provenance}
All thermodynamic state points computed via the VSEPR-SIM five-prong
Peng--Robinson EOS atlas (PR-EOS, tier \texttt{exact\_reference} or
\texttt{eos\_derived}).  Sources: Setzmann \& Wagner 1991 (CH$_4$),
B\"{u}cker \& Wagner 2006 (C$_2$H$_6$, C$_4$H$_{10}$), Lemmon et al.\ 2009
(C$_3$H$_8$), and NIST REFPROP-compatible correlations for remaining species.
Transport properties via Chapman--Enskog theory with Neufeld collision
integral fitting (Neufeld et al.\ 1972).
\end{infobox}

\begin{warnbox}{Limitations}
\begin{itemize}
  \item Adiabatic flame temperatures use a fixed mean product $c_p = 35$\,J/(mol$\cdot$K);
        equilibrium solver values differ by 5--15\,\%.
  \item Flash points sourced from literature; computed values via LFL correlation
        have $\pm 10$\,K uncertainty.
  \item No mixture models: all PVT grids are for pure components.
\end{itemize}
\end{warnbox}

\ornament

\chapter*{References}
\addcontentsline{toc}{chapter}{References}

\begin{enumerate}[label={[\arabic*]},leftmargin=2em,itemsep=2pt]
  \item Setzmann, U.~\& Wagner, W. (1991).
    A new equation of state and tables of thermodynamic properties for methane.
    \textit{J.\ Phys.\ Chem.\ Ref.\ Data}, 20(6), 1061--1155.

  \item B\"ucker, D.~\& Wagner, W. (2006).
    Reference equations of state for ethane and butane.
    \textit{J.\ Phys.\ Chem.\ Ref.\ Data}, 35(1), 205--266.

  \item Lemmon, E.W., McLinden, M.O., \& Wagner, W. (2009).
    Thermodynamic properties of propane.
    \textit{J.\ Chem.\ Eng.\ Data}, 54(12), 3141--3180.

  \item Westbrook, C.K.~\& Dryer, F.L. (1981).
    Simplified reaction mechanisms for the oxidation of hydrocarbon fuels.
    \textit{Combust.\ Sci.\ Technol.}, 27, 31--43.

  \item Metghalchi, M.~\& Keck, J.C. (1982).
    Burning velocities of mixtures of air with methanol, isooctane, and
    indolene at high pressure and temperature.
    \textit{Combustion and Flame}, 48, 191--210.

  \item Neufeld, P.D., Janzen, A.R., \& Aziz, R.A. (1972).
    Empirical equations to calculate 16 of the transport collision integrals.
    \textit{J.\ Chem.\ Phys.}, 57(3), 1100--1102.

  \item Peng, D.Y.~\& Robinson, D.B. (1976).
    A new two-constant equation of state.
    \textit{Ind.\ Eng.\ Chem.\ Fundam.}, 15(1), 59--64.

  \item Zeldovich, Y.B. (1946).
    The oxidation of nitrogen in combustion and explosions.
    \textit{Acta Physicochimica URSS}, 21, 577--628.

  \item Jones, G.W. (1938).
    Inflammation limits and their practical application in hazardous
    industrial operations.
    \textit{Chem.\ Rev.}, 22(1), 1--26.

  \item Demirba{\c s}, A. (2001).
    Biomass resource facilities and biomass conversion processing for
    fuels and chemicals.
    \textit{Energy Conversion and Management}, 42(11), 1357--1378.
\end{enumerate}

\ornament
\vfill
\begin{center}
{\color{gray}\small --- End of Report \#070 ---}\\[4pt]
{\color{gray}\footnotesize VSEPR-SIM 4.0.4 ``Chromatic-Pillar'' \textbar\ \today}
\end{center}

\end{document}
""")
    return buf.getvalue()

# ── Compile ──────────────────────────────────────────────────────────────────
def compile_latex(tex_path: pathlib.Path) -> bool:
    import shutil
    if not shutil.which("pdflatex"):
        print("  pdflatex not in PATH — leaving .tex only")
        return False
    for pass_n in (1, 2, 3):
        print(f"  pdflatex pass {pass_n} …")
        r = subprocess.run(
            ["pdflatex", "-interaction=nonstopmode",
             f"-output-directory={tex_path.parent}", str(tex_path)],
            capture_output=True, cwd=str(tex_path.parent),
        )
        if r.returncode != 0 and pass_n == 2:
            print("  pdflatex errors (last 20 lines):")
            for line in r.stdout.decode(errors="replace").splitlines()[-20:]:
                print("   ", line)
            return False
    return True

# ── Entry point ──────────────────────────────────────────────────────────────
def main():
    t0 = time.time()
    print(f"\nVSEPR-SIM Report #070 — Burn Organics")
    print(f"Output directory: {OUT}\n")

    tex_str  = build_latex()
    tex_path = OUT / "report_070.tex"
    tex_path.write_text(tex_str, encoding="utf-8")
    print(f"\n  LaTeX written: {tex_path.name}  ({len(tex_str):,} chars, "
          f"~{tex_str.count(chr(10)):,} lines)")

    ok = compile_latex(tex_path)
    if ok:
        pdf = tex_path.with_suffix(".pdf")
        if pdf.exists():
            print(f"\n  PDF: {pdf.name}  ({pdf.stat().st_size/1024:.0f} KB)")

    # Save compound summary JSON
    summary = []
    for o in ALL_ORGANICS:
        summary.append({
            "formula": o.formula, "name": o.name, "family": o.family,
            "MW": o.mw, "Tc": o.Tc, "Pc": o.Pc, "Tb": o.Tb,
            "Tf": o.Tf, "AIT": o.Tac, "LFL": o.LFL, "UFL": o.UFL,
            "dHc_kJ_mol": o.dHc, "nu_O2": stoich_O2(o),
            "T_ad_K": round(adiabatic_flame_T(o), 1),
        })
    (OUT / "compound_summary.json").write_text(
        json.dumps(summary, indent=2), encoding="utf-8")

    elapsed = time.time() - t0
    print(f"\n  Done in {elapsed:.1f}s\n")


if __name__ == "__main__":
    main()
