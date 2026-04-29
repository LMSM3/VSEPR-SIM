/**
 * reported_quantity.cpp  —  Physical quantity reporting implementation
 * =====================================================================
 * VSEPR-SIM 3.0.1
 *
 * Contains:
 *   - Standard atomic masses Z=1-118 (IUPAC 2021)
 *   - Element symbol table Z=1-118
 *   - ReportedQuantity construction, formatting, and rendering
 */

#include "core/reported_quantity.hpp"

#include <sstream>
#include <iomanip>
#include <numeric>

namespace vsepr {

// ============================================================================
// Standard atomic masses (Z=1-118, IUPAC 2021)
// Index 0 = H (Z=1), index 117 = Og (Z=118)
// ============================================================================

const std::vector<double>& standard_atomic_masses_amu() {
    static const std::vector<double> table = {
        /*   1 H  */   1.008,
        /*   2 He */   4.002602,
        /*   3 Li */   6.941,
        /*   4 Be */   9.0121831,
        /*   5 B  */  10.81,
        /*   6 C  */  12.011,
        /*   7 N  */  14.007,
        /*   8 O  */  15.999,
        /*   9 F  */  18.998403163,
        /*  10 Ne */  20.1797,
        /*  11 Na */  22.98976928,
        /*  12 Mg */  24.305,
        /*  13 Al */  26.9815384,
        /*  14 Si */  28.085,
        /*  15 P  */  30.973761998,
        /*  16 S  */  32.06,
        /*  17 Cl */  35.45,
        /*  18 Ar */  39.948,
        /*  19 K  */  39.0983,
        /*  20 Ca */  40.078,
        /*  21 Sc */  44.955908,
        /*  22 Ti */  47.867,
        /*  23 V  */  50.9415,
        /*  24 Cr */  51.9961,
        /*  25 Mn */  54.938043,
        /*  26 Fe */  55.845,
        /*  27 Co */  58.933194,
        /*  28 Ni */  58.6934,
        /*  29 Cu */  63.546,
        /*  30 Zn */  65.38,
        /*  31 Ga */  69.723,
        /*  32 Ge */  72.630,
        /*  33 As */  74.921595,
        /*  34 Se */  78.971,
        /*  35 Br */  79.904,
        /*  36 Kr */  83.798,
        /*  37 Rb */  85.4678,
        /*  38 Sr */  87.62,
        /*  39 Y  */  88.90584,
        /*  40 Zr */  91.224,
        /*  41 Nb */  92.90637,
        /*  42 Mo */  95.95,
        /*  43 Tc */  98.0,
        /*  44 Ru */ 101.07,
        /*  45 Rh */ 102.90549,
        /*  46 Pd */ 106.42,
        /*  47 Ag */ 107.8682,
        /*  48 Cd */ 112.414,
        /*  49 In */ 114.818,
        /*  50 Sn */ 118.710,
        /*  51 Sb */ 121.760,
        /*  52 Te */ 127.60,
        /*  53 I  */ 126.90447,
        /*  54 Xe */ 131.293,
        /*  55 Cs */ 132.90545196,
        /*  56 Ba */ 137.327,
        /*  57 La */ 138.90547,
        /*  58 Ce */ 140.116,
        /*  59 Pr */ 140.90766,
        /*  60 Nd */ 144.242,
        /*  61 Pm */ 145.0,
        /*  62 Sm */ 150.36,
        /*  63 Eu */ 151.964,
        /*  64 Gd */ 157.25,
        /*  65 Tb */ 158.925354,
        /*  66 Dy */ 162.500,
        /*  67 Ho */ 164.930328,
        /*  68 Er */ 167.259,
        /*  69 Tm */ 168.934218,
        /*  70 Yb */ 173.045,
        /*  71 Lu */ 174.9668,
        /*  72 Hf */ 178.486,
        /*  73 Ta */ 180.94788,
        /*  74 W  */ 183.84,
        /*  75 Re */ 186.207,
        /*  76 Os */ 190.23,
        /*  77 Ir */ 192.217,
        /*  78 Pt */ 195.084,
        /*  79 Au */ 196.966570,
        /*  80 Hg */ 200.592,
        /*  81 Tl */ 204.38,
        /*  82 Pb */ 207.2,
        /*  83 Bi */ 208.98040,
        /*  84 Po */ 209.0,
        /*  85 At */ 210.0,
        /*  86 Rn */ 222.0,
        /*  87 Fr */ 223.0,
        /*  88 Ra */ 226.0,
        /*  89 Ac */ 227.0,
        /*  90 Th */ 232.0377,
        /*  91 Pa */ 231.03588,
        /*  92 U  */ 238.02891,
        /*  93 Np */ 237.0,
        /*  94 Pu */ 244.0,
        /*  95 Am */ 243.0,
        /*  96 Cm */ 247.0,
        /*  97 Bk */ 247.0,
        /*  98 Cf */ 251.0,
        /*  99 Es */ 252.0,
        /* 100 Fm */ 257.0,
        /* 101 Md */ 258.0,
        /* 102 No */ 259.0,
        /* 103 Lr */ 266.0,
        /* 104 Rf */ 267.0,
        /* 105 Db */ 268.0,
        /* 106 Sg */ 269.0,
        /* 107 Bh */ 270.0,
        /* 108 Hs */ 269.0,
        /* 109 Mt */ 278.0,
        /* 110 Ds */ 281.0,
        /* 111 Rg */ 282.0,
        /* 112 Cn */ 285.0,
        /* 113 Nh */ 286.0,
        /* 114 Fl */ 289.0,
        /* 115 Mc */ 290.0,
        /* 116 Lv */ 293.0,
        /* 117 Ts */ 294.0,
        /* 118 Og */ 294.0
    };
    return table;
}

// ============================================================================
// Element symbol table (Z=1-118)
// ============================================================================

static const char* g_symbols[] = {
    "H",  "He", "Li", "Be", "B",  "C",  "N",  "O",  "F",  "Ne",
    "Na", "Mg", "Al", "Si", "P",  "S",  "Cl", "Ar", "K",  "Ca",
    "Sc", "Ti", "V",  "Cr", "Mn", "Fe", "Co", "Ni", "Cu", "Zn",
    "Ga", "Ge", "As", "Se", "Br", "Kr", "Rb", "Sr", "Y",  "Zr",
    "Nb", "Mo", "Tc", "Ru", "Rh", "Pd", "Ag", "Cd", "In", "Sn",
    "Sb", "Te", "I",  "Xe", "Cs", "Ba", "La", "Ce", "Pr", "Nd",
    "Pm", "Sm", "Eu", "Gd", "Tb", "Dy", "Ho", "Er", "Tm", "Yb",
    "Lu", "Hf", "Ta", "W",  "Re", "Os", "Ir", "Pt", "Au", "Hg",
    "Tl", "Pb", "Bi", "Po", "At", "Rn", "Fr", "Ra", "Ac", "Th",
    "Pa", "U",  "Np", "Pu", "Am", "Cm", "Bk", "Cf", "Es", "Fm",
    "Md", "No", "Lr", "Rf", "Db", "Sg", "Bh", "Hs", "Mt", "Ds",
    "Rg", "Cn", "Nh", "Fl", "Mc", "Lv", "Ts", "Og"
};

const char* element_symbol(int Z) {
    if (Z < 1 || Z > 118) return "??";
    return g_symbols[Z - 1];
}

// ============================================================================
// ReportedQuantity::from_composition()
// ============================================================================

ReportedQuantity ReportedQuantity::from_composition(
    const std::vector<int>& atomic_numbers,
    const std::vector<double>& atomic_masses_amu,
    const Energy& total_energy)
{
    ReportedQuantity rq;
    rq.energy = total_energy;

    // Build Z-count and accumulate mass
    double total_mass_amu = 0.0;
    for (int z : atomic_numbers) {
        rq.z_count[z]++;
        if (z > 0 && z <= static_cast<int>(atomic_masses_amu.size())) {
            total_mass_amu += atomic_masses_amu[z - 1];
        }
    }

    // Convert to grams
    rq.mass_g = total_mass_amu * energy_const::AMU_TO_GRAMS;

    // Compute molar mass (sum of atomic masses for one formula unit)
    rq.molar_mass_g_per_mol = 0.0;
    for (const auto& [z, count] : rq.z_count) {
        if (z > 0 && z <= static_cast<int>(atomic_masses_amu.size())) {
            rq.molar_mass_g_per_mol += static_cast<double>(count) * atomic_masses_amu[z - 1];
        }
    }

    // Compute moles
    rq.amount_mol = (rq.molar_mass_g_per_mol > 0.0)
        ? rq.mass_g / rq.molar_mass_g_per_mol
        : 0.0;

    return rq;
}

// ============================================================================
// ReportedQuantity::from_z_count()
// ============================================================================

ReportedQuantity ReportedQuantity::from_z_count(
    const ZCountMap& composition,
    const std::vector<double>& mass_table_amu,
    const Energy& total_energy)
{
    ReportedQuantity rq;
    rq.energy  = total_energy;
    rq.z_count = composition;

    // Compute molar mass and total mass
    double total_mass_amu = 0.0;
    for (const auto& [z, count] : composition) {
        if (z > 0 && z <= static_cast<int>(mass_table_amu.size())) {
            total_mass_amu += static_cast<double>(count) * mass_table_amu[z - 1];
        }
    }

    rq.mass_g = total_mass_amu * energy_const::AMU_TO_GRAMS;
    rq.molar_mass_g_per_mol = total_mass_amu;   // same numerical value in amu = g/mol
    rq.amount_mol = (rq.molar_mass_g_per_mol > 0.0)
        ? rq.mass_g / rq.molar_mass_g_per_mol
        : 0.0;

    return rq;
}

// ============================================================================
// Formatting
// ============================================================================

std::string ReportedQuantity::format_z_count() const {
    if (z_count.empty()) return "(empty)";

    std::ostringstream oss;
    bool first = true;
    for (const auto& [z, count] : z_count) {
        if (!first) oss << ", ";
        first = false;
        oss << count << "\xc3\x97" << element_symbol(z);  // UTF-8 ×
    }
    return oss.str();
}

std::string ReportedQuantity::format_energy_all(int precision) const {
    return energy.format_all(precision);
}

std::string ReportedQuantity::format_summary() const {
    std::ostringstream oss;
    oss << std::setprecision(6);

    if (!label.empty()) oss << label << ": ";
    if (!formula.empty()) oss << formula << " | ";

    oss << "m=" << mass_g << " g, "
        << "E=" << energy.as_hartree() << " Ha, "
        << "n=" << amount_mol << " mol, "
        << "atoms=" << total_atoms();

    return oss.str();
}

std::string ReportedQuantity::format_report(int precision) const {
    std::ostringstream oss;
    oss << std::setprecision(precision);

    oss << "ReportedQuantity\n";
    oss << "────────────────────────────────────────\n";

    if (!label.empty())
        oss << "  Label:          " << label << "\n";
    if (!formula.empty())
        oss << "  Formula:        " << formula << "\n";

    oss << "  Composition:    " << format_z_count() << "\n";
    oss << "  Total atoms:    " << total_atoms() << "\n";
    oss << "  Elements:       " << element_count() << "\n";
    oss << "  ────────────────────────────────────\n";
    oss << "  Mass:           " << mass_g << " g\n";
    oss << "  Molar mass:     " << molar_mass_g_per_mol << " g/mol\n";
    oss << "  Amount:         " << amount_mol << " mol\n";
    oss << "  ────────────────────────────────────\n";
    oss << "  Energy (Ha):    " << energy.as_hartree() << " Hartree\n";
    oss << "  Energy (eV):    " << energy.as_ev() << " eV\n";
    oss << "  Energy (kcal):  " << energy.as_kcalmol() << " kcal/mol\n";
    oss << "  Energy (kJ):    " << energy.as_kjmol() << " kJ/mol\n";

    double ratio = energy.thermal_ratio();
    oss << "  kBT ratio:      " << ratio << " (at 298.15 K)\n";
    oss << "  Thermally acc:  "
        << (energy.thermally_accessible() ? "YES" : "NO")
        << " (within 1 kBT)\n";

    return oss.str();
}

} // namespace vsepr
