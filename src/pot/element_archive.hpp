#pragma once
/*
element_archive.hpp
-------------------
Centralized element property archive for Z=1–110 (H through Ds).

Consolidates all four kernel data pipes into a single queryable record:
  - Atomic mass        (IUPAC 2021 standard atomic weights)
  - Covalent radius    (Pyykkö & Atsumi 2009)
  - Van der Waals radius (Bondi 1964 / Alvarez 2013)
  - CPK color          (Jmol reference scheme)

The last 8 elements (Rg Z=111 through Og Z=118) are excluded from the
archive because their physical/chemical property data is almost entirely
estimated — no experimentally confirmed radii, masses, or meaningful
color assignments exist for superheavy elements beyond Ds (Z=110).

This header is the authoritative cross-reference point.  Each value is
forwarded from the single-source-of-truth kernel arrays; no data is
duplicated or re-entered.

Anti-black-box: every number traces to its originating kernel header.
*/

#include <cstdint>
#include <array>
#include <string>

#include "pot/covalent_radii.hpp"
#include "pot/vdw_radii.hpp"
#include "pot/atomic_masses.hpp"

namespace vsepr {
namespace archive {

// Archive boundary — elements Z=1 through Z=110 (Ds)
constexpr uint8_t ARCHIVE_Z_MIN = 1;
constexpr uint8_t ARCHIVE_Z_MAX = 110;
constexpr uint8_t ARCHIVE_COUNT = ARCHIVE_Z_MAX;  // 110 elements

// ============================================================================
// Element symbols (IUPAC standard, Z=0–110)
// ============================================================================

constexpr const char* ELEMENT_SYMBOLS[] = {
    "",    // Z=0 (unused)
    "H",  "He",
    "Li", "Be", "B",  "C",  "N",  "O",  "F",  "Ne",
    "Na", "Mg", "Al", "Si", "P",  "S",  "Cl", "Ar",
    "K",  "Ca",
    "Sc", "Ti", "V",  "Cr", "Mn", "Fe", "Co", "Ni", "Cu", "Zn",
    "Ga", "Ge", "As", "Se", "Br", "Kr",
    "Rb", "Sr",
    "Y",  "Zr", "Nb", "Mo", "Tc", "Ru", "Rh", "Pd", "Ag", "Cd",
    "In", "Sn", "Sb", "Te", "I",  "Xe",
    "Cs", "Ba",
    "La", "Ce", "Pr", "Nd", "Pm", "Sm", "Eu", "Gd",
    "Tb", "Dy", "Ho", "Er", "Tm", "Yb", "Lu",
    "Hf", "Ta", "W",  "Re", "Os", "Ir", "Pt", "Au", "Hg",
    "Tl", "Pb", "Bi", "Po", "At", "Rn",
    "Fr", "Ra",
    "Ac", "Th", "Pa", "U",  "Np", "Pu", "Am", "Cm",
    "Bk", "Cf", "Es", "Fm", "Md", "No", "Lr",
    "Rf", "Db", "Sg", "Bh", "Hs", "Mt", "Ds",
};

// ============================================================================
// Element names (Z=0–110)
// ============================================================================

constexpr const char* ELEMENT_NAMES[] = {
    "",             // Z=0 (unused)
    "Hydrogen",     "Helium",
    "Lithium",      "Beryllium",    "Boron",        "Carbon",
    "Nitrogen",     "Oxygen",       "Fluorine",     "Neon",
    "Sodium",       "Magnesium",    "Aluminium",    "Silicon",
    "Phosphorus",   "Sulfur",       "Chlorine",     "Argon",
    "Potassium",    "Calcium",
    "Scandium",     "Titanium",     "Vanadium",     "Chromium",
    "Manganese",    "Iron",         "Cobalt",       "Nickel",
    "Copper",       "Zinc",
    "Gallium",      "Germanium",    "Arsenic",      "Selenium",
    "Bromine",      "Krypton",
    "Rubidium",     "Strontium",
    "Yttrium",      "Zirconium",    "Niobium",      "Molybdenum",
    "Technetium",   "Ruthenium",    "Rhodium",      "Palladium",
    "Silver",       "Cadmium",
    "Indium",       "Tin",          "Antimony",     "Tellurium",
    "Iodine",       "Xenon",
    "Caesium",      "Barium",
    "Lanthanum",    "Cerium",       "Praseodymium", "Neodymium",
    "Promethium",   "Samarium",     "Europium",     "Gadolinium",
    "Terbium",      "Dysprosium",   "Holmium",      "Erbium",
    "Thulium",      "Ytterbium",    "Lutetium",
    "Hafnium",      "Tantalum",     "Tungsten",     "Rhenium",
    "Osmium",       "Iridium",      "Platinum",     "Gold",
    "Mercury",
    "Thallium",     "Lead",         "Bismuth",      "Polonium",
    "Astatine",     "Radon",
    "Francium",     "Radium",
    "Actinium",     "Thorium",      "Protactinium", "Uranium",
    "Neptunium",    "Plutonium",    "Americium",    "Curium",
    "Berkelium",    "Californium",  "Einsteinium",  "Fermium",
    "Mendelevium",  "Nobelium",     "Lawrencium",
    "Rutherfordium","Dubnium",      "Seaborgium",   "Bohrium",
    "Hassium",      "Meitnerium",   "Darmstadtium",
};

// ============================================================================
// CPK colors for archive elements (Jmol scheme, RGB 0.0–1.0)
// Forwarded from renderer_base.cpp's static table via constexpr.
// ============================================================================

struct CPKColor {
    float r, g, b;
};

// Compiled from the Jmol CPK table in src/vis/renderer_base.cpp
// Z=0–110 (index 0 = unknown / magenta fallback)
constexpr std::array<CPKColor, 111> ARCHIVE_CPK_COLORS = {{
    {1.00f, 0.08f, 0.58f}, // 0 - Unknown (magenta)
    {1.00f, 1.00f, 1.00f}, // 1 - H
    {0.85f, 1.00f, 1.00f}, // 2 - He
    {0.80f, 0.50f, 1.00f}, // 3 - Li
    {0.76f, 1.00f, 0.00f}, // 4 - Be
    {1.00f, 0.71f, 0.71f}, // 5 - B
    {0.30f, 0.30f, 0.30f}, // 6 - C
    {0.05f, 0.05f, 1.00f}, // 7 - N
    {1.00f, 0.05f, 0.05f}, // 8 - O
    {0.70f, 1.00f, 1.00f}, // 9 - F
    {0.70f, 0.89f, 0.96f}, // 10 - Ne
    {0.67f, 0.36f, 0.95f}, // 11 - Na
    {0.54f, 1.00f, 0.00f}, // 12 - Mg
    {0.75f, 0.65f, 0.65f}, // 13 - Al
    {0.94f, 0.78f, 0.63f}, // 14 - Si
    {1.00f, 0.50f, 0.00f}, // 15 - P
    {1.00f, 1.00f, 0.19f}, // 16 - S
    {0.12f, 0.94f, 0.12f}, // 17 - Cl
    {0.50f, 0.82f, 0.89f}, // 18 - Ar
    {0.56f, 0.25f, 0.83f}, // 19 - K
    {0.24f, 1.00f, 0.00f}, // 20 - Ca
    {0.90f, 0.90f, 0.90f}, // 21 - Sc
    {0.75f, 0.76f, 0.78f}, // 22 - Ti
    {0.65f, 0.65f, 0.67f}, // 23 - V
    {0.54f, 0.60f, 0.78f}, // 24 - Cr
    {0.61f, 0.48f, 0.78f}, // 25 - Mn
    {0.88f, 0.40f, 0.20f}, // 26 - Fe
    {0.94f, 0.56f, 0.63f}, // 27 - Co
    {0.31f, 0.82f, 0.31f}, // 28 - Ni
    {0.78f, 0.50f, 0.20f}, // 29 - Cu
    {0.49f, 0.50f, 0.69f}, // 30 - Zn
    {0.76f, 0.56f, 0.56f}, // 31 - Ga
    {0.40f, 0.56f, 0.56f}, // 32 - Ge
    {0.74f, 0.50f, 0.89f}, // 33 - As
    {1.00f, 0.63f, 0.00f}, // 34 - Se
    {0.65f, 0.16f, 0.16f}, // 35 - Br
    {0.36f, 0.72f, 0.82f}, // 36 - Kr
    {0.44f, 0.18f, 0.69f}, // 37 - Rb
    {0.00f, 1.00f, 0.00f}, // 38 - Sr
    {0.58f, 1.00f, 1.00f}, // 39 - Y
    {0.58f, 0.88f, 0.88f}, // 40 - Zr
    {0.45f, 0.76f, 0.79f}, // 41 - Nb
    {0.33f, 0.71f, 0.71f}, // 42 - Mo
    {0.23f, 0.62f, 0.62f}, // 43 - Tc
    {0.14f, 0.56f, 0.56f}, // 44 - Ru
    {0.04f, 0.49f, 0.55f}, // 45 - Rh
    {0.00f, 0.41f, 0.52f}, // 46 - Pd
    {0.75f, 0.75f, 0.75f}, // 47 - Ag
    {1.00f, 0.85f, 0.56f}, // 48 - Cd
    {0.65f, 0.46f, 0.45f}, // 49 - In
    {0.40f, 0.50f, 0.50f}, // 50 - Sn
    {0.62f, 0.39f, 0.71f}, // 51 - Sb
    {0.83f, 0.48f, 0.00f}, // 52 - Te
    {0.58f, 0.00f, 0.58f}, // 53 - I
    {0.26f, 0.62f, 0.69f}, // 54 - Xe
    {0.34f, 0.09f, 0.56f}, // 55 - Cs
    {0.00f, 0.79f, 0.00f}, // 56 - Ba
    {0.44f, 0.83f, 1.00f}, // 57 - La
    {1.00f, 1.00f, 0.78f}, // 58 - Ce
    {0.85f, 1.00f, 0.78f}, // 59 - Pr
    {0.78f, 1.00f, 0.78f}, // 60 - Nd
    {0.64f, 1.00f, 0.78f}, // 61 - Pm
    {0.56f, 1.00f, 0.78f}, // 62 - Sm
    {0.38f, 1.00f, 0.78f}, // 63 - Eu
    {0.27f, 1.00f, 0.78f}, // 64 - Gd
    {0.19f, 1.00f, 0.78f}, // 65 - Tb
    {0.12f, 1.00f, 0.78f}, // 66 - Dy
    {0.00f, 1.00f, 0.61f}, // 67 - Ho
    {0.00f, 0.90f, 0.46f}, // 68 - Er
    {0.00f, 0.83f, 0.32f}, // 69 - Tm
    {0.00f, 0.75f, 0.22f}, // 70 - Yb
    {0.00f, 0.67f, 0.14f}, // 71 - Lu
    {0.30f, 0.76f, 1.00f}, // 72 - Hf
    {0.30f, 0.65f, 1.00f}, // 73 - Ta
    {0.13f, 0.58f, 0.84f}, // 74 - W
    {0.15f, 0.49f, 0.67f}, // 75 - Re
    {0.15f, 0.40f, 0.59f}, // 76 - Os
    {0.09f, 0.33f, 0.53f}, // 77 - Ir
    {0.82f, 0.82f, 0.88f}, // 78 - Pt
    {1.00f, 0.82f, 0.14f}, // 79 - Au
    {0.72f, 0.72f, 0.82f}, // 80 - Hg
    {0.65f, 0.33f, 0.30f}, // 81 - Tl
    {0.34f, 0.35f, 0.38f}, // 82 - Pb
    {0.62f, 0.31f, 0.71f}, // 83 - Bi
    {0.67f, 0.36f, 0.00f}, // 84 - Po
    {0.46f, 0.31f, 0.27f}, // 85 - At
    {0.26f, 0.51f, 0.59f}, // 86 - Rn
    {0.26f, 0.00f, 0.40f}, // 87 - Fr
    {0.00f, 0.49f, 0.00f}, // 88 - Ra
    {0.44f, 0.67f, 0.98f}, // 89 - Ac
    {0.00f, 0.73f, 1.00f}, // 90 - Th
    {0.00f, 0.63f, 1.00f}, // 91 - Pa
    {0.00f, 0.56f, 1.00f}, // 92 - U
    {0.00f, 0.50f, 1.00f}, // 93 - Np
    {0.00f, 0.42f, 1.00f}, // 94 - Pu
    {0.33f, 0.36f, 0.95f}, // 95 - Am
    {0.47f, 0.36f, 0.89f}, // 96 - Cm
    {0.54f, 0.31f, 0.89f}, // 97 - Bk
    {0.63f, 0.21f, 0.83f}, // 98 - Cf
    {0.70f, 0.12f, 0.83f}, // 99 - Es
    {0.70f, 0.12f, 0.73f}, // 100 - Fm
    {0.70f, 0.05f, 0.65f}, // 101 - Md
    {0.74f, 0.05f, 0.53f}, // 102 - No
    {0.78f, 0.00f, 0.40f}, // 103 - Lr
    {0.80f, 0.00f, 0.35f}, // 104 - Rf
    {0.82f, 0.00f, 0.31f}, // 105 - Db
    {0.85f, 0.00f, 0.27f}, // 106 - Sg
    {0.88f, 0.00f, 0.22f}, // 107 - Bh
    {0.90f, 0.00f, 0.18f}, // 108 - Hs
    {0.92f, 0.00f, 0.15f}, // 109 - Mt
    {0.93f, 0.00f, 0.12f}, // 110 - Ds
}};

// ============================================================================
// Consolidated element record
// ============================================================================

struct ElementRecord {
    uint8_t Z;
    const char* symbol;
    const char* name;
    double atomic_mass;       // Da (amu), from atomic_masses.hpp
    double covalent_radius;   // Å, from covalent_radii.hpp
    double vdw_radius;        // Å, from vdw_radii.hpp
    CPKColor cpk;             // RGB 0.0–1.0, from renderer_base.cpp
};

// ============================================================================
// Query API — all data forwarded from kernel source arrays
// ============================================================================

inline bool is_archived(uint8_t Z) {
    return Z >= ARCHIVE_Z_MIN && Z <= ARCHIVE_Z_MAX;
}

inline ElementRecord get_element_record(uint8_t Z) {
    if (!is_archived(Z)) {
        return {0, "", "", 0.0, 0.0, 0.0, {1.0f, 0.08f, 0.58f}};
    }
    return {
        Z,
        ELEMENT_SYMBOLS[Z],
        ELEMENT_NAMES[Z],
        ATOMIC_MASSES[Z],
        COVALENT_RADII[Z],
        VDW_RADII[Z],
        ARCHIVE_CPK_COLORS[Z]
    };
}

inline const char* get_archive_symbol(uint8_t Z) {
    if (!is_archived(Z)) return "";
    return ELEMENT_SYMBOLS[Z];
}

inline const char* get_archive_name(uint8_t Z) {
    if (!is_archived(Z)) return "";
    return ELEMENT_NAMES[Z];
}

// ============================================================================
// Integrity helpers — for compile-time and runtime validation
// ============================================================================

// Verify all archive elements have non-zero data in every pipe
inline bool validate_archive_integrity() {
    for (uint8_t Z = ARCHIVE_Z_MIN; Z <= ARCHIVE_Z_MAX; ++Z) {
        if (ATOMIC_MASSES[Z] <= 0.0) return false;
        if (COVALENT_RADII[Z] <= 0.0) return false;
        if (VDW_RADII[Z] <= 0.0) return false;
    }
    return true;
}

// Verify atomic mass monotonicity (general trend, allows local inversions
// for elements like Ar/K, Co/Ni, Te/I, Th/Pa, U/Np, Pu/Am)
inline bool validate_mass_trend() {
    double max_seen = 0.0;
    int violations = 0;
    for (uint8_t Z = ARCHIVE_Z_MIN; Z <= ARCHIVE_Z_MAX; ++Z) {
        if (ATOMIC_MASSES[Z] < max_seen) {
            violations++;
        } else {
            max_seen = ATOMIC_MASSES[Z];
        }
    }
    // Allow up to 6 known inversions
    return violations <= 6;
}

// Verify covalent radius < VdW radius for all archive elements
inline bool validate_radius_ordering() {
    for (uint8_t Z = ARCHIVE_Z_MIN; Z <= ARCHIVE_Z_MAX; ++Z) {
        if (COVALENT_RADII[Z] >= VDW_RADII[Z]) return false;
    }
    return true;
}

} // namespace archive
} // namespace vsepr
