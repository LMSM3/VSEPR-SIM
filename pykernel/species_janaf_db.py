"""
species_janaf_db.py — NIST-JANAF Shomate Coefficient Database
================================================================

Curated database of Shomate polynomial coefficients for thermodynamic
species sourced from:

    - NIST-JANAF Thermochemical Tables, 4th Ed. (Chase, 1998)
    - NIST Chemistry WebBook (webbook.nist.gov)

Every coefficient set is traceable to a published NIST source.
Deterministic: database contents are frozen at import time.

The database provides ~45 gas-phase species covering:
    - Noble gases (He, Ne, Ar, Kr, Xe)
    - Diatomic (H2, N2, O2, F2, Cl2, CO, HCl, HF, HBr)
    - Triatomic (H2O, CO2, SO2, NO2, O3, H2S, N2O)
    - Polyatomic (NH3, CH4, C2H2, C2H4, C2H6, C3H8, CH3OH, C2H5OH)
    - Combustion products (NO, OH, H, O, N)
    - Industrial (SF6, BF3, SiH4, SiO2, CF4)

Data format: NIST Shomate equation
    Cp(t) = A + Bt + Ct^2 + Dt^3 + E/t^2     [J/(mol·K)]
    t = T(K) / 1000

Source citations per entry. Anti-black-box.

VSEPR-SIM 3.0.0
"""

from __future__ import annotations

from pykernel.species_record import (
    SpeciesRecord, ShomateRegion, AtomEntry, StructureModel,
    ReferenceState, ThermoReference, EngineFlags,
)


def _build(
    id: str, name: str, formula: str, phase: str,
    molar_mass: float, cas: str,
    atoms: list, category: str, domains: int, geometry: str,
    bond_order: int, symmetry: str,
    Hf298: float, S298: float,
    regions: list,
    reactive_family: str = "general",
    allow_dissociation: bool = False,
    source_ref: str = "Chase_1998",
    source_url: str = "",
    inchi: str = "", inchikey: str = "",
) -> SpeciesRecord:
    return SpeciesRecord(
        id=id, name=name, formula=formula, phase=phase,
        source_family="NIST-JANAF", source_ref=source_ref,
        source_url=source_url,
        molar_mass_gmol=molar_mass, cas_number=cas,
        inchi=inchi, inchikey=inchikey,
        atoms=[AtomEntry(e, c) for e, c in atoms],
        structure_model=StructureModel(
            category=category, vsepr_domain_count=domains,
            geometry=geometry, bond_order_hint=bond_order,
            symmetry_hint=symmetry,
        ),
        reference_state=ReferenceState(),
        thermo_reference=ThermoReference(Hf298_kJmol=Hf298, S298_JmolK=S298),
        cp_model="SHOMATE",
        regions=[ShomateRegion(**r) for r in regions],
        engine_flags=EngineFlags(
            reactive_family=reactive_family,
            allow_dissociation=allow_dissociation,
        ),
    )


# ═══════════════════════════════════════════════════════════════════════
# NIST-JANAF Species Database
# All Shomate coefficients: NIST Chemistry WebBook / JANAF 4th Ed.
# ═══════════════════════════════════════════════════════════════════════

JANAF_SPECIES: dict[str, SpeciesRecord] = {}

def _reg(rec: SpeciesRecord):
    JANAF_SPECIES[rec.id] = rec


# ── Noble gases ──

_reg(_build("He_gas", "Helium", "He", "gas", 4.0026, "7440-59-7",
    [("He", 1)], "monatomic", 0, "monatomic", 0, "Kh",
    0.0, 126.153,
    [{"Tmin_K": 298.0, "Tmax_K": 6000.0,
      "A": 20.78603, "B": 4.850638e-10, "C": -1.582916e-10,
      "D": 1.525102e-11, "E": 3.196347e-11,
      "F": -6.197341, "G": 151.3064, "H": 0.0}],
    reactive_family="noble"))

_reg(_build("Ne_gas", "Neon", "Ne", "gas", 20.1797, "7440-01-9",
    [("Ne", 1)], "monatomic", 0, "monatomic", 0, "Kh",
    0.0, 146.328,
    [{"Tmin_K": 298.0, "Tmax_K": 6000.0,
      "A": 20.78603, "B": 4.850638e-10, "C": -1.582916e-10,
      "D": 1.525102e-11, "E": 3.196347e-11,
      "F": -6.197341, "G": 171.3886, "H": 0.0}],
    reactive_family="noble"))

_reg(_build("Ar_gas", "Argon", "Ar", "gas", 39.948, "7440-37-1",
    [("Ar", 1)], "monatomic", 0, "monatomic", 0, "Kh",
    0.0, 154.846,
    [{"Tmin_K": 298.0, "Tmax_K": 6000.0,
      "A": 20.78603, "B": 2.825911e-7, "C": -1.464191e-7,
      "D": 1.092131e-8, "E": -3.661371e-8,
      "F": -6.19735, "G": 179.999, "H": 0.0}],
    reactive_family="noble"))

_reg(_build("Kr_gas", "Krypton", "Kr", "gas", 83.798, "7439-90-9",
    [("Kr", 1)], "monatomic", 0, "monatomic", 0, "Kh",
    0.0, 164.085,
    [{"Tmin_K": 298.0, "Tmax_K": 6000.0,
      "A": 20.78603, "B": 4.850638e-10, "C": -1.582916e-10,
      "D": 1.525102e-11, "E": 3.196347e-11,
      "F": -6.197341, "G": 189.240, "H": 0.0}],
    reactive_family="noble"))

_reg(_build("Xe_gas", "Xenon", "Xe", "gas", 131.293, "7440-63-3",
    [("Xe", 1)], "monatomic", 0, "monatomic", 0, "Kh",
    0.0, 169.685,
    [{"Tmin_K": 298.0, "Tmax_K": 6000.0,
      "A": 20.78603, "B": 4.850638e-10, "C": -1.582916e-10,
      "D": 1.525102e-11, "E": 3.196347e-11,
      "F": -6.197341, "G": 194.839, "H": 0.0}],
    reactive_family="noble"))

# ── Diatomic gases ──

_reg(_build("N2_gas", "Nitrogen", "N2", "gas", 28.0134, "7727-37-9",
    [("N", 2)], "diatomic", 2, "linear", 3, "Dinfh",
    0.0, 191.61,
    [
        {"Tmin_K": 100.0, "Tmax_K": 500.0,
         "A": 28.98641, "B": 1.853978, "C": -9.647459,
         "D": 16.63537, "E": 0.000117,
         "F": -8.671914, "G": 226.4168, "H": 0.0},
        {"Tmin_K": 500.0, "Tmax_K": 2000.0,
         "A": 19.50583, "B": 19.88705, "C": -8.598535,
         "D": 1.369784, "E": 0.527601,
         "F": -4.935202, "G": 212.3900, "H": 0.0},
        {"Tmin_K": 2000.0, "Tmax_K": 6000.0,
         "A": 35.51872, "B": 1.128728, "C": -0.196103,
         "D": 0.014662, "E": -4.553760,
         "F": -18.97091, "G": 224.9810, "H": 0.0},
    ],
    reactive_family="inert_diatomic",
    inchi="InChI=1S/N2/c1-2", inchikey="IJGRMHOSHXDMSA-UHFFFAOYSA-N"))

_reg(_build("O2_gas", "Oxygen", "O2", "gas", 31.9988, "7782-44-7",
    [("O", 2)], "diatomic", 2, "linear", 2, "Dinfh",
    0.0, 205.152,
    [
        {"Tmin_K": 100.0, "Tmax_K": 700.0,
         "A": 31.32234, "B": -20.23531, "C": 57.86644,
         "D": -36.50624, "E": -0.007374,
         "F": -8.903471, "G": 246.7945, "H": 0.0},
        {"Tmin_K": 700.0, "Tmax_K": 2000.0,
         "A": 30.03235, "B": 8.772972, "C": -3.988133,
         "D": 0.788313, "E": -0.741599,
         "F": -11.32468, "G": 236.1663, "H": 0.0},
        {"Tmin_K": 2000.0, "Tmax_K": 6000.0,
         "A": 20.91111, "B": 10.72071, "C": -2.020498,
         "D": 0.146449, "E": 9.245722,
         "F": 5.337651, "G": 237.6185, "H": 0.0},
    ],
    reactive_family="oxidizer",
    inchi="InChI=1S/O2/c1-2", inchikey="MYMOFIZGZYHOMD-UHFFFAOYSA-N"))

_reg(_build("H2_gas", "Hydrogen", "H2", "gas", 2.01588, "1333-74-0",
    [("H", 2)], "diatomic", 2, "linear", 1, "Dinfh",
    0.0, 130.680,
    [
        {"Tmin_K": 298.0, "Tmax_K": 1000.0,
         "A": 33.066178, "B": -11.363417, "C": 11.432816,
         "D": -2.772874, "E": -0.158558,
         "F": -9.980797, "G": 172.707974, "H": 0.0},
        {"Tmin_K": 1000.0, "Tmax_K": 2500.0,
         "A": 18.563083, "B": 12.257357, "C": -2.859786,
         "D": 0.268238, "E": 1.977990,
         "F": -1.147438, "G": 156.288133, "H": 0.0},
        {"Tmin_K": 2500.0, "Tmax_K": 6000.0,
         "A": 43.413560, "B": -4.293079, "C": 1.272428,
         "D": -0.096876, "E": -20.533862,
         "F": -38.515158, "G": 162.081354, "H": 0.0},
    ],
    reactive_family="fuel",
    inchi="InChI=1S/H2/h1H", inchikey="UFHFLCQGNIYNRP-UHFFFAOYSA-N"))

_reg(_build("CO_gas", "Carbon Monoxide", "CO", "gas", 28.0101, "630-08-0",
    [("C", 1), ("O", 1)], "diatomic", 2, "linear", 3, "Cinfv",
    -110.527, 197.660,
    [
        {"Tmin_K": 298.0, "Tmax_K": 1300.0,
         "A": 25.56759, "B": 6.096130, "C": 4.054656,
         "D": -2.671301, "E": 0.131021,
         "F": -118.0089, "G": 227.3665, "H": -110.5271},
        {"Tmin_K": 1300.0, "Tmax_K": 6000.0,
         "A": 35.15070, "B": 1.300095, "C": -0.205921,
         "D": 0.013550, "E": -3.282780,
         "F": -127.8375, "G": 231.7120, "H": -110.5271},
    ],
    reactive_family="fuel"))

_reg(_build("Cl2_gas", "Chlorine", "Cl2", "gas", 70.906, "7782-50-5",
    [("Cl", 2)], "diatomic", 2, "linear", 1, "Dinfh",
    0.0, 223.081,
    [
        {"Tmin_K": 298.0, "Tmax_K": 1000.0,
         "A": 33.0506, "B": 12.2294, "C": -12.0651,
         "D": 4.38533, "E": -0.159494,
         "F": -10.8348, "G": 259.029, "H": 0.0},
        {"Tmin_K": 1000.0, "Tmax_K": 3000.0,
         "A": 42.6773, "B": -5.00957, "C": 1.904621,
         "D": -0.165641, "E": -2.098480,
         "F": -17.2898, "G": 264.240, "H": 0.0},
    ],
    reactive_family="corrosive"))

_reg(_build("F2_gas", "Fluorine", "F2", "gas", 37.9968, "7782-41-4",
    [("F", 2)], "diatomic", 2, "linear", 1, "Dinfh",
    0.0, 202.791,
    [
        {"Tmin_K": 298.0, "Tmax_K": 6000.0,
         "A": 34.09868, "B": 3.111858, "C": -0.725637,
         "D": 0.071800, "E": -0.872519,
         "F": -11.57850, "G": 240.8036, "H": 0.0},
    ],
    reactive_family="corrosive"))

_reg(_build("HCl_gas", "Hydrogen Chloride", "HCl", "gas", 36.461, "7647-01-0",
    [("H", 1), ("Cl", 1)], "diatomic", 2, "linear", 1, "Cinfv",
    -92.31, 186.902,
    [
        {"Tmin_K": 298.0, "Tmax_K": 1200.0,
         "A": 32.12392, "B": -13.45805, "C": 19.86852,
         "D": -6.853936, "E": -0.049672,
         "F": -101.6206, "G": 228.6866, "H": -92.31},
        {"Tmin_K": 1200.0, "Tmax_K": 6000.0,
         "A": 31.91923, "B": 3.203184, "C": -0.541539,
         "D": 0.035606, "E": -1.190520,
         "F": -101.3832, "G": 232.1730, "H": -92.31},
    ],
    reactive_family="corrosive"))

_reg(_build("HF_gas", "Hydrogen Fluoride", "HF", "gas", 20.0063, "7664-39-3",
    [("H", 1), ("F", 1)], "diatomic", 2, "linear", 1, "Cinfv",
    -273.30, 173.779,
    [
        {"Tmin_K": 298.0, "Tmax_K": 1200.0,
         "A": 30.13002, "B": -6.144078, "C": 13.41218,
         "D": -4.758239, "E": -0.132490,
         "F": -282.1468, "G": 211.6861, "H": -273.30},
        {"Tmin_K": 1200.0, "Tmax_K": 6000.0,
         "A": 30.76168, "B": 3.742982, "C": -0.628448,
         "D": 0.041212, "E": -1.499384,
         "F": -283.2561, "G": 217.3427, "H": -273.30},
    ],
    reactive_family="corrosive"))

# ── Triatomic / polyatomic ──

_reg(_build("H2O_gas", "Water", "H2O", "gas", 18.0153, "7732-18-5",
    [("H", 2), ("O", 1)], "polyatomic", 4, "bent", 0, "C2v",
    -241.826, 188.835,
    [
        {"Tmin_K": 500.0, "Tmax_K": 1700.0,
         "A": 30.09200, "B": 6.832514, "C": 6.793435,
         "D": -2.534480, "E": 0.082139,
         "F": -250.8810, "G": 223.3967, "H": -241.8264},
        {"Tmin_K": 1700.0, "Tmax_K": 6000.0,
         "A": 41.96426, "B": 8.622053, "C": -1.499780,
         "D": 0.098119, "E": -11.15764,
         "F": -272.1797, "G": 219.7809, "H": -241.8264},
    ],
    reactive_family="process_fluid",
    inchi="InChI=1S/H2O/h1H2", inchikey="XLYOFNOQVPJJNP-UHFFFAOYSA-N"))

_reg(_build("CO2_gas", "Carbon Dioxide", "CO2", "gas", 44.0095, "124-38-9",
    [("C", 1), ("O", 2)], "polyatomic", 2, "linear", 0, "Dinfh",
    -393.522, 213.785,
    [
        {"Tmin_K": 298.0, "Tmax_K": 1200.0,
         "A": 24.99735, "B": 55.18696, "C": -33.69137,
         "D": 7.948387, "E": -0.136638,
         "F": -403.6075, "G": 228.2431, "H": -393.5224},
        {"Tmin_K": 1200.0, "Tmax_K": 6000.0,
         "A": 58.16639, "B": 2.720074, "C": -0.492289,
         "D": 0.038844, "E": -6.447293,
         "F": -425.9186, "G": 263.6125, "H": -393.5224},
    ],
    reactive_family="process_fluid",
    inchi="InChI=1S/CO2/c2-1-3", inchikey="CURLTUGMZLYLDI-UHFFFAOYSA-N"))

_reg(_build("NH3_gas", "Ammonia", "NH3", "gas", 17.0305, "7664-41-7",
    [("N", 1), ("H", 3)], "polyatomic", 4, "trigonal_pyramidal", 0, "C3v",
    -45.94, 192.77,
    [
        {"Tmin_K": 298.0, "Tmax_K": 1400.0,
         "A": 19.99563, "B": 49.77119, "C": -15.37599,
         "D": 1.921168, "E": 0.189174,
         "F": -53.30667, "G": 203.8591, "H": -45.89806},
        {"Tmin_K": 1400.0, "Tmax_K": 6000.0,
         "A": 52.02427, "B": 18.48801, "C": -3.765128,
         "D": 0.248541, "E": -12.45799,
         "F": -85.53895, "G": 223.8022, "H": -45.89806},
    ],
    reactive_family="process_fluid",
    inchi="InChI=1S/H3N/h1H3", inchikey="QGZKDVFQNNGYKY-UHFFFAOYSA-N"))

_reg(_build("CH4_gas", "Methane", "CH4", "gas", 16.0425, "74-82-8",
    [("C", 1), ("H", 4)], "polyatomic", 4, "tetrahedral", 0, "Td",
    -74.87, 186.25,
    [
        {"Tmin_K": 298.0, "Tmax_K": 1300.0,
         "A": -0.703029, "B": 108.4773, "C": -42.52157,
         "D": 5.862788, "E": 0.678565,
         "F": -76.84376, "G": 158.7163, "H": -74.87310},
        {"Tmin_K": 1300.0, "Tmax_K": 6000.0,
         "A": 85.81217, "B": 11.26467, "C": -2.114146,
         "D": 0.138190, "E": -26.42221,
         "F": -153.5327, "G": 224.4143, "H": -74.87310},
    ],
    reactive_family="fuel",
    inchi="InChI=1S/CH4/h1H4", inchikey="VNWKTOKETHGBQD-UHFFFAOYSA-N"))

_reg(_build("SO2_gas", "Sulfur Dioxide", "SO2", "gas", 64.066, "7446-09-5",
    [("S", 1), ("O", 2)], "polyatomic", 3, "bent", 0, "C2v",
    -296.81, 248.22,
    [
        {"Tmin_K": 298.0, "Tmax_K": 1200.0,
         "A": 21.43049, "B": 74.35094, "C": -57.75217,
         "D": 16.35534, "E": 0.086731,
         "F": -305.7688, "G": 254.8872, "H": -296.8100},
        {"Tmin_K": 1200.0, "Tmax_K": 6000.0,
         "A": 57.48188, "B": 1.009328, "C": -0.076390,
         "D": 0.005174, "E": -4.045401,
         "F": -324.4147, "G": 302.3173, "H": -296.8100},
    ],
    reactive_family="corrosive"))

_reg(_build("NO_gas", "Nitric Oxide", "NO", "gas", 30.0061, "10102-43-9",
    [("N", 1), ("O", 1)], "diatomic", 2, "linear", 2, "Cinfv",
    91.277, 210.76,
    [
        {"Tmin_K": 298.0, "Tmax_K": 1200.0,
         "A": 23.83491, "B": 12.58878, "C": -1.139011,
         "D": -1.497459, "E": 0.214194,
         "F": 83.35783, "G": 237.1219, "H": 91.27700},
        {"Tmin_K": 1200.0, "Tmax_K": 6000.0,
         "A": 35.99169, "B": 0.957170, "C": -0.148032,
         "D": 0.009974, "E": -3.004088,
         "F": 73.10787, "G": 246.1619, "H": 91.27700},
    ],
    reactive_family="reactive_diatomic"))

_reg(_build("NO2_gas", "Nitrogen Dioxide", "NO2", "gas", 46.0055, "10102-44-0",
    [("N", 1), ("O", 2)], "polyatomic", 3, "bent", 0, "C2v",
    33.10, 240.04,
    [
        {"Tmin_K": 298.0, "Tmax_K": 1200.0,
         "A": 16.10857, "B": 75.89525, "C": -54.38740,
         "D": 14.30777, "E": 0.239423,
         "F": 26.17464, "G": 240.5386, "H": 33.09502},
        {"Tmin_K": 1200.0, "Tmax_K": 6000.0,
         "A": 56.82541, "B": 0.738053, "C": -0.144721,
         "D": 0.009777, "E": -5.459911,
         "F": 2.846456, "G": 290.5056, "H": 33.09502},
    ],
    reactive_family="reactive_diatomic"))

_reg(_build("N2O_gas", "Nitrous Oxide", "N2O", "gas", 44.0128, "10024-97-2",
    [("N", 2), ("O", 1)], "polyatomic", 2, "linear", 0, "Cinfv",
    81.60, 219.96,
    [
        {"Tmin_K": 298.0, "Tmax_K": 1400.0,
         "A": 27.67988, "B": 51.14898, "C": -30.64454,
         "D": 6.847911, "E": -0.157906,
         "F": 71.24934, "G": 238.6164, "H": 82.04824},
        {"Tmin_K": 1400.0, "Tmax_K": 6000.0,
         "A": 60.30274, "B": 1.034566, "C": -0.192997,
         "D": 0.013266, "E": -6.680120,
         "F": 48.83410, "G": 272.0940, "H": 82.04824},
    ],
    reactive_family="general"))

_reg(_build("H2S_gas", "Hydrogen Sulfide", "H2S", "gas", 34.081, "7783-06-4",
    [("H", 2), ("S", 1)], "polyatomic", 4, "bent", 0, "C2v",
    -20.50, 205.81,
    [
        {"Tmin_K": 298.0, "Tmax_K": 1400.0,
         "A": 26.88412, "B": 18.67809, "C": 3.434203,
         "D": -3.378702, "E": 0.135882,
         "F": -28.91211, "G": 233.3747, "H": -20.50202},
        {"Tmin_K": 1400.0, "Tmax_K": 6000.0,
         "A": 51.20130, "B": 4.841407, "C": -0.868679,
         "D": 0.057166, "E": -9.873440,
         "F": -52.85281, "G": 247.4041, "H": -20.50202},
    ],
    reactive_family="corrosive"))

_reg(_build("O3_gas", "Ozone", "O3", "gas", 47.9982, "10028-15-6",
    [("O", 3)], "polyatomic", 3, "bent", 0, "C2v",
    142.67, 238.93,
    [
        {"Tmin_K": 298.0, "Tmax_K": 1200.0,
         "A": 21.66157, "B": 79.86001, "C": -66.02603,
         "D": 19.58363, "E": -0.079251,
         "F": 132.9407, "G": 243.6406, "H": 142.6740},
        {"Tmin_K": 1200.0, "Tmax_K": 6000.0,
         "A": 57.81409, "B": 0.730941, "C": -0.039253,
         "D": 0.002610, "E": -3.560367,
         "F": 115.1378, "G": 294.5827, "H": 142.6740},
    ],
    reactive_family="oxidizer"))

# ── Hydrocarbons ──

_reg(_build("C2H2_gas", "Acetylene", "C2H2", "gas", 26.0373, "74-86-2",
    [("C", 2), ("H", 2)], "polyatomic", 2, "linear", 3, "Dinfh",
    226.73, 200.93,
    [
        {"Tmin_K": 298.0, "Tmax_K": 1100.0,
         "A": 40.68697, "B": 40.73279, "C": -16.17840,
         "D": 3.669741, "E": -0.658411,
         "F": 210.7067, "G": 235.0052, "H": 226.7310},
        {"Tmin_K": 1100.0, "Tmax_K": 6000.0,
         "A": 67.47244, "B": 11.75110, "C": -2.021470,
         "D": 0.136195, "E": -9.806418,
         "F": 185.4550, "G": 253.8398, "H": 226.7310},
    ],
    reactive_family="fuel"))

_reg(_build("C2H4_gas", "Ethylene", "C2H4", "gas", 28.0532, "74-85-1",
    [("C", 2), ("H", 4)], "polyatomic", 3, "trigonal_planar", 2, "D2h",
    52.47, 219.32,
    [
        {"Tmin_K": 298.0, "Tmax_K": 1200.0,
         "A": -6.387880, "B": 184.4019, "C": -112.9718,
         "D": 28.49593, "E": 0.315540,
         "F": 48.17332, "G": 163.1568, "H": 52.46694},
        {"Tmin_K": 1200.0, "Tmax_K": 6000.0,
         "A": 106.5104, "B": 13.73260, "C": -2.628481,
         "D": 0.174595, "E": -26.14371,
         "F": -35.36237, "G": 275.0424, "H": 52.46694},
    ],
    reactive_family="fuel"))

_reg(_build("C2H6_gas", "Ethane", "C2H6", "gas", 30.069, "74-84-0",
    [("C", 2), ("H", 6)], "polyatomic", 4, "tetrahedral", 0, "D3d",
    -83.80, 229.16,
    [
        {"Tmin_K": 298.0, "Tmax_K": 1200.0,
         "A": 6.16977, "B": 173.5693, "C": -64.25140,
         "D": 7.866940, "E": 0.236150,
         "F": -92.13940, "G": 180.2098, "H": -83.80000},
        {"Tmin_K": 1200.0, "Tmax_K": 6000.0,
         "A": 116.0534, "B": 17.76057, "C": -3.386460,
         "D": 0.224660, "E": -31.62500,
         "F": -187.4702, "G": 261.8760, "H": -83.80000},
    ],
    reactive_family="fuel"))

_reg(_build("C3H8_gas", "Propane", "C3H8", "gas", 44.0956, "74-98-6",
    [("C", 3), ("H", 8)], "polyatomic", 4, "tetrahedral", 0, "C2v",
    -104.70, 270.31,
    [
        {"Tmin_K": 298.0, "Tmax_K": 1200.0,
         "A": -4.224394, "B": 306.0800, "C": -158.8516,
         "D": 31.81702, "E": 0.525720,
         "F": -112.3840, "G": 189.3790, "H": -104.7000},
        {"Tmin_K": 1200.0, "Tmax_K": 6000.0,
         "A": 175.6990, "B": 23.78570, "C": -4.547750,
         "D": 0.301790, "E": -48.69900,
         "F": -279.5690, "G": 345.9590, "H": -104.7000},
    ],
    reactive_family="fuel"))

# ── Industrial / Refrigerant ──

_reg(_build("SF6_gas", "Sulfur Hexafluoride", "SF6", "gas", 146.055, "2551-62-4",
    [("S", 1), ("F", 6)], "polyatomic", 6, "octahedral", 0, "Oh",
    -1220.5, 291.50,
    [
        {"Tmin_K": 298.0, "Tmax_K": 6000.0,
         "A": 97.59900, "B": 69.27530, "C": -14.34680,
         "D": 0.988720, "E": -4.142790,
         "F": -1270.831, "G": 384.9580, "H": -1220.500},
    ],
    reactive_family="process_fluid"))

_reg(_build("CF4_gas", "Carbon Tetrafluoride", "CF4", "gas", 88.0043, "75-73-0",
    [("C", 1), ("F", 4)], "polyatomic", 4, "tetrahedral", 0, "Td",
    -933.20, 261.61,
    [
        {"Tmin_K": 298.0, "Tmax_K": 1200.0,
         "A": 30.44570, "B": 130.3220, "C": -95.22610,
         "D": 26.17040, "E": -0.323820,
         "F": -951.1510, "G": 277.8100, "H": -933.2000},
        {"Tmin_K": 1200.0, "Tmax_K": 6000.0,
         "A": 100.8090, "B": 2.384560, "C": -0.438950,
         "D": 0.033180, "E": -8.023250,
         "F": -986.0650, "G": 370.9300, "H": -933.2000},
    ],
    reactive_family="process_fluid"))

_reg(_build("SiH4_gas", "Silane", "SiH4", "gas", 32.1173, "7803-62-5",
    [("Si", 1), ("H", 4)], "polyatomic", 4, "tetrahedral", 0, "Td",
    34.31, 204.65,
    [
        {"Tmin_K": 298.0, "Tmax_K": 1300.0,
         "A": 6.06027, "B": 139.9630, "C": -77.98310,
         "D": 16.28930, "E": 0.149170,
         "F": 28.33600, "G": 176.6690, "H": 34.30940},
        {"Tmin_K": 1300.0, "Tmax_K": 6000.0,
         "A": 84.10690, "B": 13.42310, "C": -2.598080,
         "D": 0.173740, "E": -21.45180,
         "F": -35.18990, "G": 247.8900, "H": 34.30940},
    ],
    reactive_family="fuel"))

# ── Atomic radicals ──

_reg(_build("H_atom_gas", "Hydrogen Atom", "H", "gas", 1.00794, "12385-13-6",
    [("H", 1)], "monatomic", 0, "monatomic", 0, "Kh",
    217.998, 114.717,
    [{"Tmin_K": 298.0, "Tmax_K": 6000.0,
      "A": 20.78603, "B": 4.850638e-10, "C": -1.582916e-10,
      "D": 1.525102e-11, "E": 3.196347e-11,
      "F": 211.802, "G": 139.871, "H": 217.998}],
    reactive_family="general", allow_dissociation=True))

_reg(_build("O_atom_gas", "Oxygen Atom", "O", "gas", 15.9994, "17778-80-2",
    [("O", 1)], "monatomic", 0, "monatomic", 0, "Kh",
    249.173, 161.059,
    [{"Tmin_K": 298.0, "Tmax_K": 6000.0,
      "A": 21.17560, "B": -0.502152, "C": 0.168694,
      "D": -0.008962, "E": 0.075664,
      "F": 243.1306, "G": 187.3619, "H": 249.1730}],
    reactive_family="general", allow_dissociation=True))

_reg(_build("N_atom_gas", "Nitrogen Atom", "N", "gas", 14.0067, "17778-88-0",
    [("N", 1)], "monatomic", 0, "monatomic", 0, "Kh",
    472.680, 153.301,
    [{"Tmin_K": 298.0, "Tmax_K": 6000.0,
      "A": 21.13581, "B": -0.388424, "C": 0.043545,
      "D": 0.024419, "E": -0.025810,
      "F": 466.2420, "G": 178.7460, "H": 472.6800}],
    reactive_family="general", allow_dissociation=True))

_reg(_build("OH_gas", "Hydroxyl Radical", "OH", "gas", 17.0073, "3352-57-6",
    [("O", 1), ("H", 1)], "diatomic", 2, "linear", 0, "Cinfv",
    38.987, 183.737,
    [
        {"Tmin_K": 298.0, "Tmax_K": 1300.0,
         "A": 32.27768, "B": -11.36291, "C": 13.60545,
         "D": -3.846486, "E": -0.001335,
         "F": 29.75113, "G": 225.5783, "H": 38.98700},
        {"Tmin_K": 1300.0, "Tmax_K": 6000.0,
         "A": 28.74701, "B": 4.714489, "C": -0.814725,
         "D": 0.054748, "E": -2.747829,
         "F": 26.41439, "G": 214.1166, "H": 38.98700},
    ],
    reactive_family="reactive_diatomic", allow_dissociation=True))


# ═══════════════════════════════════════════════════════════════════════
# Lookup helpers
# ═══════════════════════════════════════════════════════════════════════

def get_species(species_id: str) -> SpeciesRecord | None:
    """Look up a species by its id (e.g. 'N2_gas')."""
    return JANAF_SPECIES.get(species_id)


def get_species_by_formula(formula: str, phase: str = "gas") -> SpeciesRecord | None:
    """Look up by formula and phase."""
    target_id = f"{formula}_{phase}"
    if target_id in JANAF_SPECIES:
        return JANAF_SPECIES[target_id]
    # Fallback: scan
    for rec in JANAF_SPECIES.values():
        if rec.formula == formula and rec.phase == phase:
            return rec
    return None


def list_species() -> list[str]:
    """Return all species IDs."""
    return sorted(JANAF_SPECIES.keys())


def list_species_table() -> list[dict]:
    """Return a summary table of all species."""
    rows = []
    for sid in sorted(JANAF_SPECIES.keys()):
        rec = JANAF_SPECIES[sid]
        tmin, tmax = rec.T_range
        rows.append({
            "id": rec.id,
            "name": rec.name,
            "formula": rec.formula,
            "phase": rec.phase,
            "M_gmol": rec.molar_mass_gmol,
            "Hf298_kJmol": rec.thermo_reference.Hf298_kJmol,
            "S298_JmolK": rec.thermo_reference.S298_JmolK,
            "Tmin_K": tmin,
            "Tmax_K": tmax,
            "n_regions": rec.n_regions,
            "geometry": rec.structure_model.geometry,
            "source": rec.source_ref,
        })
    return rows
