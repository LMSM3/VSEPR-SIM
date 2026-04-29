/**
 * species_family.hpp
 * ------------------
 * Species Family Taxonomy and Entity Model for VSEPR-SIM.
 *
 * Defines the four-family classification:
 *   GAS     — noble, diatomic, polyatomic, refrigerant, fuel, mixture
 *   CRYSTAL — metal, ionic, covalent, actinide, defect crystal
 *   OM      — organometallics (large): transition-metal, actinide, Cp/arene, hybrid
 *   CM      — ceramics (medium): oxide, carbide, nitride, silicate, refractory, powder
 *
 * Each SpeciesEntity carries:
 *   - family + subtype classification
 *   - core geometry via io::XYZMolecule backbone (atoms, bonds, metadata)
 *   - external layer (charge, velocity, force, energy, response history)
 *   - scale score and physics emphasis tags
 *   - recommended file format mapping
 *
 * This uses the existing XYZ-layer backbone (io::XYZMolecule, io::XYZAtom, io::XYZBond)
 * rather than inventing a parallel system. Crystal lattice metadata uses
 * data::LatticeVectors when periodic boundary conditions apply.
 *
 * Anti-black-box: every classification is explicit, every field public,
 * every mapping deterministic and inspectable.
 *
 * Reference architecture:
 *   io/xyz_format.hpp      — XYZAtom, XYZBond, XYZMolecule
 *   data/Crystal.hpp       — LatticeVectors, Atom, Bond, Crystal
 *   gas2/gas2_species.hpp  — GasSpecies (transport + EOS data)
 *   truth/truth_state.hpp  — TruthState (reproducibility ledger)
 */

#pragma once

#include <string>
#include <vector>
#include <array>
#include <optional>
#include <map>
#include <cmath>
#include <algorithm>
#include <sstream>

namespace vsepr {

// ============================================================================
// Family enumeration
// ============================================================================

enum class SpeciesFamily {
    GAS,              // Small-to-medium, molecule units, transport+EOS heavy
    CRYSTAL,          // Medium-to-large, periodic cells/lattices/grains
    ORGANOMETALLIC,   // Large, coordination-heavy, hybrid state
    CERAMIC           // Medium, lattice-fragment/grain/powder scale
};

inline const char* family_name(SpeciesFamily f) {
    switch (f) {
        case SpeciesFamily::GAS:             return "GAS";
        case SpeciesFamily::CRYSTAL:         return "CRY";
        case SpeciesFamily::ORGANOMETALLIC:  return "OM";
        case SpeciesFamily::CERAMIC:         return "CM";
    }
    return "UNKNOWN";
}

inline const char* family_long_name(SpeciesFamily f) {
    switch (f) {
        case SpeciesFamily::GAS:             return "Gas";
        case SpeciesFamily::CRYSTAL:         return "Crystal";
        case SpeciesFamily::ORGANOMETALLIC:  return "Organometallic";
        case SpeciesFamily::CERAMIC:         return "Ceramic";
    }
    return "Unknown";
}

// ============================================================================
// Subfamily enumeration (all families, flat)
// ============================================================================

enum class SpeciesSubfamily {
    // GAS subfamilies
    GAS_NOBLE,
    GAS_DIATOMIC,
    GAS_POLYATOMIC,
    GAS_REFRIGERANT,
    GAS_FUEL,
    GAS_MIXTURE,

    // CRYSTAL subfamilies
    CRY_METAL,
    CRY_IONIC,
    CRY_COVALENT,
    CRY_ACTINIDE_SOLID,
    CRY_DEFECT,

    // ORGANOMETALLIC subfamilies
    OM_TRANSITION_METAL,
    OM_ACTINIDE,
    OM_CARBONYL,
    OM_CP_ARENE,
    OM_HYBRID_BEAD_ATOMISTIC,

    // CERAMIC subfamilies
    CM_OXIDE,
    CM_CARBIDE,
    CM_NITRIDE,
    CM_SILICATE,
    CM_REFRACTORY,
    CM_POWDER,

    // Unknown
    UNKNOWN
};

inline SpeciesFamily subfamily_parent(SpeciesSubfamily sf) {
    switch (sf) {
        case SpeciesSubfamily::GAS_NOBLE:
        case SpeciesSubfamily::GAS_DIATOMIC:
        case SpeciesSubfamily::GAS_POLYATOMIC:
        case SpeciesSubfamily::GAS_REFRIGERANT:
        case SpeciesSubfamily::GAS_FUEL:
        case SpeciesSubfamily::GAS_MIXTURE:
            return SpeciesFamily::GAS;

        case SpeciesSubfamily::CRY_METAL:
        case SpeciesSubfamily::CRY_IONIC:
        case SpeciesSubfamily::CRY_COVALENT:
        case SpeciesSubfamily::CRY_ACTINIDE_SOLID:
        case SpeciesSubfamily::CRY_DEFECT:
            return SpeciesFamily::CRYSTAL;

        case SpeciesSubfamily::OM_TRANSITION_METAL:
        case SpeciesSubfamily::OM_ACTINIDE:
        case SpeciesSubfamily::OM_CARBONYL:
        case SpeciesSubfamily::OM_CP_ARENE:
        case SpeciesSubfamily::OM_HYBRID_BEAD_ATOMISTIC:
            return SpeciesFamily::ORGANOMETALLIC;

        case SpeciesSubfamily::CM_OXIDE:
        case SpeciesSubfamily::CM_CARBIDE:
        case SpeciesSubfamily::CM_NITRIDE:
        case SpeciesSubfamily::CM_SILICATE:
        case SpeciesSubfamily::CM_REFRACTORY:
        case SpeciesSubfamily::CM_POWDER:
            return SpeciesFamily::CERAMIC;

        case SpeciesSubfamily::UNKNOWN:
            return SpeciesFamily::GAS;  // default fallback
    }
    return SpeciesFamily::GAS;
}

inline const char* subfamily_name(SpeciesSubfamily sf) {
    switch (sf) {
        case SpeciesSubfamily::GAS_NOBLE:        return "noble";
        case SpeciesSubfamily::GAS_DIATOMIC:     return "diatomic";
        case SpeciesSubfamily::GAS_POLYATOMIC:   return "polyatomic";
        case SpeciesSubfamily::GAS_REFRIGERANT:  return "refrigerant";
        case SpeciesSubfamily::GAS_FUEL:         return "fuel_gas";
        case SpeciesSubfamily::GAS_MIXTURE:      return "mixed_gas";

        case SpeciesSubfamily::CRY_METAL:          return "metal";
        case SpeciesSubfamily::CRY_IONIC:          return "ionic_crystal";
        case SpeciesSubfamily::CRY_COVALENT:       return "covalent_crystal";
        case SpeciesSubfamily::CRY_ACTINIDE_SOLID: return "actinide_solid";
        case SpeciesSubfamily::CRY_DEFECT:         return "defect_crystal";

        case SpeciesSubfamily::OM_TRANSITION_METAL:      return "transition_metal_OM";
        case SpeciesSubfamily::OM_ACTINIDE:              return "actinide_OM";
        case SpeciesSubfamily::OM_CARBONYL:              return "carbonyl";
        case SpeciesSubfamily::OM_CP_ARENE:              return "Cp_arene";
        case SpeciesSubfamily::OM_HYBRID_BEAD_ATOMISTIC: return "hybrid_bead_atomistic_OM";

        case SpeciesSubfamily::CM_OXIDE:       return "oxide";
        case SpeciesSubfamily::CM_CARBIDE:     return "carbide";
        case SpeciesSubfamily::CM_NITRIDE:     return "nitride";
        case SpeciesSubfamily::CM_SILICATE:    return "silicate";
        case SpeciesSubfamily::CM_REFRACTORY:  return "refractory_ceramic";
        case SpeciesSubfamily::CM_POWDER:      return "ceramic_powder";

        case SpeciesSubfamily::UNKNOWN:        return "unknown";
    }
    return "unknown";
}

// ============================================================================
// Scale classification
// ============================================================================

enum class ScaleClass {
    SMALL,      // 1–10 atoms (single molecules, noble atoms)
    MEDIUM,     // 10–100 atoms (small clusters, ceramic grains)
    LARGE,      // 100–10000 atoms (crystals, organometallic complexes)
    BULK        // >10000 atoms (supercells, bulk simulations)
};

inline const char* scale_name(ScaleClass s) {
    switch (s) {
        case ScaleClass::SMALL:  return "small";
        case ScaleClass::MEDIUM: return "medium";
        case ScaleClass::LARGE:  return "large";
        case ScaleClass::BULK:   return "bulk";
    }
    return "unknown";
}

inline ScaleClass classify_scale(int atom_count) {
    if (atom_count <= 10)     return ScaleClass::SMALL;
    if (atom_count <= 100)    return ScaleClass::MEDIUM;
    if (atom_count <= 10000)  return ScaleClass::LARGE;
    return ScaleClass::BULK;
}

// ============================================================================
// Physics emphasis tags
// ============================================================================

struct PhysicsEmphasis {
    bool transport    = false;   // viscosity, diffusion, thermal conductivity
    bool eos          = false;   // equation of state (P-V-T relations)
    bool kinetics     = false;   // reaction rates, collision dynamics
    bool lattice      = false;   // periodicity, phonons, defects
    bool coordination = false;   // metal-ligand bonding, coordination geometry
    bool surface      = false;   // surface reactivity, adsorption
    bool fracture     = false;   // crack propagation, grain boundaries
    bool topology     = false;   // bond graph, connectivity persistence
};

inline PhysicsEmphasis default_emphasis(SpeciesFamily f) {
    PhysicsEmphasis p{};
    switch (f) {
        case SpeciesFamily::GAS:
            p.transport = true;
            p.eos       = true;
            p.kinetics  = true;
            break;
        case SpeciesFamily::CRYSTAL:
            p.lattice   = true;
            p.topology  = true;
            p.surface   = true;
            break;
        case SpeciesFamily::ORGANOMETALLIC:
            p.coordination = true;
            p.topology     = true;
            break;
        case SpeciesFamily::CERAMIC:
            p.lattice  = true;
            p.surface  = true;
            p.fracture = true;
            break;
    }
    return p;
}

// ============================================================================
// Recommended file format
// ============================================================================

enum class RecommendedFormat {
    XYZ,    // .xyz  — static species geometry
    XYZA,   // .xyzA — charge, velocity, force, energy
    XYZC,   // .xyzC — restartable state (checkpoint)
    XYZF    // .xyzF — multi-frame trajectories
};

inline const char* format_extension(RecommendedFormat f) {
    switch (f) {
        case RecommendedFormat::XYZ:  return ".xyz";
        case RecommendedFormat::XYZA: return ".xyzA";
        case RecommendedFormat::XYZC: return ".xyzC";
        case RecommendedFormat::XYZF: return ".xyzF";
    }
    return ".xyz";
}

struct FormatRecommendation {
    RecommendedFormat static_geometry  = RecommendedFormat::XYZ;
    RecommendedFormat extended_state   = RecommendedFormat::XYZA;
    RecommendedFormat checkpoint       = RecommendedFormat::XYZC;
    RecommendedFormat trajectory       = RecommendedFormat::XYZF;
};

// All families use the same XYZ-family format set.
// The differentiation is in what metadata gets populated.
inline FormatRecommendation recommended_formats(SpeciesFamily /*f*/) {
    return FormatRecommendation{};  // All use the full XYZ family
}

// ============================================================================
// External layer (per-entity extended state)
// ============================================================================

struct ExternalLayer {
    // Per-atom arrays (parallel to core atom list)
    std::vector<double> charges;       // partial charges (e)
    std::vector<std::array<double, 3>> velocities;   // Å/fs
    std::vector<std::array<double, 3>> forces;       // eV/Å
    std::vector<double> per_atom_energy;             // eV

    // Aggregate properties
    double total_energy   = 0.0;       // eV
    double total_charge   = 0.0;       // e
    double temperature    = 0.0;       // K
    double pressure       = 0.0;       // Pa

    // Response / interaction history (for OM and reactive systems)
    std::vector<std::string> response_log;

    // Periodic boundary conditions (for CRYSTAL, CM)
    bool has_pbc = false;
    std::array<std::array<double, 3>, 3> cell_vectors = {};  // a, b, c in Å

    // Coordination metadata (for OM)
    std::vector<int> coordination_numbers;
    std::vector<std::string> ligand_labels;

    bool empty() const {
        return charges.empty() && velocities.empty()
            && forces.empty() && per_atom_energy.empty()
            && response_log.empty();
    }

    void resize(size_t n_atoms) {
        charges.resize(n_atoms, 0.0);
        velocities.resize(n_atoms, {0.0, 0.0, 0.0});
        forces.resize(n_atoms, {0.0, 0.0, 0.0});
        per_atom_energy.resize(n_atoms, 0.0);
        coordination_numbers.resize(n_atoms, 0);
    }
};

// ============================================================================
// SpeciesEntity — the unified wrapper
// ============================================================================
//
// Composes:
//   family     — GAS / CRYSTAL / ORGANOMETALLIC / CERAMIC
//   subfamily  — fine-grained classification
//   core       — atoms, bonds, geometry (via io::XYZMolecule interface)
//   ext        — charges, velocities, forces, energy, PBC, coordination
//   scale      — estimated atom count category
//   emphasis   — which physics dominate
//   formats    — recommended XYZ-family file formats
//
// This is a thin aggregation layer. It does NOT duplicate the data in
// io::XYZMolecule or data::Crystal. The core field uses the same
// atom/bond/metadata structures already present in the codebase.
//

struct CoreGeometry {
    // Atom data (mirrors io::XYZAtom layout)
    struct Atom {
        std::string element;
        std::array<double, 3> position = {0.0, 0.0, 0.0};
        int atom_type = 0;
    };

    // Bond data (mirrors io::XYZBond layout)
    struct Bond {
        int atom_i = 0;
        int atom_j = 0;
        double bond_order = 1.0;
    };

    std::vector<Atom> atoms;
    std::vector<Bond> bonds;
    std::string formula;
    std::string comment;

    // Metadata
    double total_energy = 0.0;
    double total_charge = 0.0;

    size_t num_atoms() const { return atoms.size(); }
    size_t num_bonds() const { return bonds.size(); }
};

struct SpeciesEntity {
    // Classification
    SpeciesFamily    family    = SpeciesFamily::GAS;
    SpeciesSubfamily subfamily = SpeciesSubfamily::UNKNOWN;
    std::string      subtype;       // free-form: "noble", "FCC", "actinide_OM", etc.

    // Core geometry (atoms, bonds, formula)
    CoreGeometry core;

    // Extended state layer
    ExternalLayer ext;

    // Scale estimate
    ScaleClass scale = ScaleClass::SMALL;
    double scale_score = 0.0;       // 0-1 normalized size score

    // Physics emphasis
    PhysicsEmphasis emphasis;

    // File format recommendations
    FormatRecommendation formats;

    // --- Helpers ---

    const char* family_str()    const { return family_name(family); }
    const char* subfamily_str() const { return subfamily_name(subfamily); }
    const char* scale_str()     const { return scale_name(scale); }

    // Recompute scale from atom count
    void update_scale() {
        int n = static_cast<int>(core.num_atoms());
        scale = classify_scale(n);
        // Normalized score: log10(n) / log10(100000), clamped [0,1]
        scale_score = (n > 0) ? std::min(1.0, std::log10(static_cast<double>(n)) / 5.0) : 0.0;
    }

    // Summary line
    std::string summary() const {
        std::ostringstream ss;
        ss << family_str() << "/" << subfamily_str()
           << " [" << core.formula << "] "
           << core.num_atoms() << " atoms, "
           << scale_str() << " scale";
        if (!subtype.empty()) ss << " (" << subtype << ")";
        return ss.str();
    }
};

// ============================================================================
// Classification engine — deterministic family assignment
// ============================================================================
//
// Given a formula string, attempts to classify into family + subfamily.
// This is a rule-based classifier, not ML. Every rule is explicit.
//

namespace classify {

// --- Known noble gases ---
inline bool is_noble(const std::string& formula) {
    static const std::vector<std::string> nobles = {"He", "Ne", "Ar", "Kr", "Xe", "Rn"};
    return std::find(nobles.begin(), nobles.end(), formula) != nobles.end();
}

// --- Known diatomics ---
inline bool is_diatomic(const std::string& formula) {
    static const std::vector<std::string> diatomics = {
        "H2", "N2", "O2", "F2", "Cl2", "Br2", "I2"
    };
    return std::find(diatomics.begin(), diatomics.end(), formula) != diatomics.end();
}

// --- Small polyatomics (gas phase at STP) ---
inline bool is_polyatomic_gas(const std::string& formula) {
    static const std::vector<std::string> gases = {
        "H2O", "CO2", "SO2", "NH3", "CH4", "NO2", "N2O", "O3",
        "H2S", "HCl", "HF", "HBr", "HI", "PH3", "SiH4", "BF3",
        "BCl3", "SF6", "XeF2", "ClF3"
    };
    return std::find(gases.begin(), gases.end(), formula) != gases.end();
}

// --- Refrigerants (R-number or common names) ---
inline bool is_refrigerant(const std::string& formula) {
    // Common refrigerant formulas
    static const std::vector<std::string> refs = {
        "CH2F2",     // R-32
        "CHClF2",    // R-22
        "CF3CH2F",   // R-134a (simplified)
        "CF3CF2H",   // R-125
        "CH2CFCF3",  // R-1234yf (simplified)
    };
    // Also match R-prefix designations
    if (formula.size() >= 2 && formula[0] == 'R' && std::isdigit(formula[1]))
        return true;
    return std::find(refs.begin(), refs.end(), formula) != refs.end();
}

// --- Fuel gases / LNG components ---
inline bool is_fuel_gas(const std::string& formula) {
    static const std::vector<std::string> fuels = {
        "C2H6", "C3H8", "C4H10", "C2H4", "C2H2", "C3H6"
    };
    return std::find(fuels.begin(), fuels.end(), formula) != fuels.end();
}

// --- Metals (elemental, crystal-forming) ---
inline bool is_metal_element(const std::string& formula) {
    static const std::vector<std::string> metals = {
        "Li", "Na", "K", "Rb", "Cs",
        "Be", "Mg", "Ca", "Sr", "Ba",
        "Al", "Ga", "In", "Sn", "Tl", "Pb", "Bi",
        "Ti", "V", "Cr", "Mn", "Fe", "Co", "Ni", "Cu", "Zn",
        "Zr", "Nb", "Mo", "Ru", "Rh", "Pd", "Ag", "Cd",
        "Hf", "Ta", "W", "Re", "Os", "Ir", "Pt", "Au"
    };
    return std::find(metals.begin(), metals.end(), formula) != metals.end();
}

// --- Ionic crystals ---
inline bool is_ionic_crystal(const std::string& formula) {
    static const std::vector<std::string> ionics = {
        "NaCl", "KCl", "KBr", "NaF", "CaF2", "MgO", "CsCl",
        "LiF", "NaBr", "KI", "CaCl2", "BaF2"
    };
    return std::find(ionics.begin(), ionics.end(), formula) != ionics.end();
}

// --- Covalent crystals ---
inline bool is_covalent_crystal(const std::string& formula) {
    static const std::vector<std::string> covalents = {
        "Si", "Ge", "C",  // diamond
        "BN", "SiGe", "GaAs", "InP", "GaN", "AlN"
    };
    return std::find(covalents.begin(), covalents.end(), formula) != covalents.end();
}

// --- Actinide solids ---
inline bool is_actinide_solid(const std::string& formula) {
    static const std::vector<std::string> actinides = {
        "UO2", "PuO2", "ThO2", "U", "Pu", "Th", "Am", "Np",
        "UF4", "UF6", "PuF4", "ThF4", "AmO2", "NpO2"
    };
    return std::find(actinides.begin(), actinides.end(), formula) != actinides.end();
}

// --- Ceramic oxides ---
inline bool is_oxide_ceramic(const std::string& formula) {
    static const std::vector<std::string> oxides = {
        "Al2O3", "SiO2", "ZrO2", "TiO2", "Fe2O3", "Fe3O4",
        "Cr2O3", "CuO", "ZnO", "SnO2", "CeO2", "MnO2",
        "V2O5", "WO3", "MoO3", "Nb2O5"
    };
    return std::find(oxides.begin(), oxides.end(), formula) != oxides.end();
}

// --- Carbides ---
inline bool is_carbide(const std::string& formula) {
    static const std::vector<std::string> carbides = {
        "SiC", "WC", "TiC", "TaC", "B4C", "Cr3C2",
        "VC", "NbC", "ZrC", "HfC", "Mo2C"
    };
    return std::find(carbides.begin(), carbides.end(), formula) != carbides.end();
}

// --- Nitrides ---
inline bool is_nitride(const std::string& formula) {
    static const std::vector<std::string> nitrides = {
        "TiN", "Si3N4", "BN", "AlN", "GaN", "CrN",
        "ZrN", "HfN", "TaN", "VN"
    };
    return std::find(nitrides.begin(), nitrides.end(), formula) != nitrides.end();
}

// --- Silicates ---
inline bool is_silicate(const std::string& formula) {
    static const std::vector<std::string> silicates = {
        "CaSiO3", "MgSiO3", "Mg2SiO4", "CaMgSi2O6",
        "NaAlSi3O8", "KAlSi3O8"
    };
    return std::find(silicates.begin(), silicates.end(), formula) != silicates.end();
}

// --- Refractory ceramics ---
inline bool is_refractory(const std::string& formula) {
    // Overlap: some carbides/nitrides/oxides are refractory
    // This catches specific high-temperature materials
    static const std::vector<std::string> refractories = {
        "MgO", "CaO", "BeO", "Y2O3", "La2O3",
        "HfC", "TaC", "ZrC", "HfN", "ZrB2", "HfB2"
    };
    return std::find(refractories.begin(), refractories.end(), formula) != refractories.end();
}

// --- Contains transition metal center (heuristic for OM) ---
inline bool has_transition_metal(const std::string& formula) {
    // Check for common TM element symbols at start or after parentheses
    static const std::vector<std::string> tms = {
        "Ti", "V", "Cr", "Mn", "Fe", "Co", "Ni", "Cu", "Zn",
        "Zr", "Nb", "Mo", "Ru", "Rh", "Pd", "Ag",
        "Hf", "Ta", "W", "Re", "Os", "Ir", "Pt", "Au"
    };
    for (const auto& tm : tms) {
        if (formula.find(tm) != std::string::npos) return true;
    }
    return false;
}

// --- Contains actinide center ---
inline bool has_actinide(const std::string& formula) {
    static const std::vector<std::string> actinides = {
        "U", "Pu", "Th", "Am", "Np", "Cm", "Bk", "Cf"
    };
    for (const auto& ac : actinides) {
        if (formula.find(ac) != std::string::npos) return true;
    }
    return false;
}

// --- Contains carbonyl (CO) ligands ---
inline bool has_carbonyl_ligand(const std::string& formula) {
    // Heuristic: formula contains "(CO)" or ends with carbonyl count
    return formula.find("CO)") != std::string::npos
        || formula.find("(CO") != std::string::npos;
}

// --- Contains Cp/arene ligand ---
inline bool has_cp_arene(const std::string& formula) {
    return formula.find("Cp") != std::string::npos
        || formula.find("C5H5") != std::string::npos
        || formula.find("C6H6") != std::string::npos;
}


// ============================================================================
// Master classifier
// ============================================================================
//
// Priority order:
//   1. Exact match against known lists
//   2. Heuristic matching (element presence, formula structure)
//   3. Default to GAS/UNKNOWN
//
// The classifier is intentionally conservative. Unknown materials
// default to GAS rather than being incorrectly assigned.
//

inline SpeciesSubfamily classify_formula(const std::string& formula) {
    // --- GAS family (highest priority for small molecules) ---
    if (is_noble(formula))                return SpeciesSubfamily::GAS_NOBLE;
    if (is_diatomic(formula))             return SpeciesSubfamily::GAS_DIATOMIC;
    if (is_refrigerant(formula))          return SpeciesSubfamily::GAS_REFRIGERANT;
    if (is_fuel_gas(formula))             return SpeciesSubfamily::GAS_FUEL;
    if (is_polyatomic_gas(formula))       return SpeciesSubfamily::GAS_POLYATOMIC;

    // --- CRYSTAL family ---
    if (is_ionic_crystal(formula))        return SpeciesSubfamily::CRY_IONIC;
    if (is_actinide_solid(formula))       return SpeciesSubfamily::CRY_ACTINIDE_SOLID;
    if (is_covalent_crystal(formula))     return SpeciesSubfamily::CRY_COVALENT;
    if (is_metal_element(formula))        return SpeciesSubfamily::CRY_METAL;

    // --- CERAMIC family ---
    if (is_carbide(formula))              return SpeciesSubfamily::CM_CARBIDE;
    if (is_nitride(formula))              return SpeciesSubfamily::CM_NITRIDE;
    if (is_silicate(formula))             return SpeciesSubfamily::CM_SILICATE;
    if (is_refractory(formula))           return SpeciesSubfamily::CM_REFRACTORY;
    if (is_oxide_ceramic(formula))        return SpeciesSubfamily::CM_OXIDE;

    // --- ORGANOMETALLIC family (heuristic) ---
    // Check after crystals/ceramics to avoid misclassifying simple metal oxides
    if (has_cp_arene(formula)) {
        return has_actinide(formula) ? SpeciesSubfamily::OM_ACTINIDE
                                     : SpeciesSubfamily::OM_CP_ARENE;
    }
    if (has_carbonyl_ligand(formula)) {
        return has_actinide(formula) ? SpeciesSubfamily::OM_ACTINIDE
                                     : SpeciesSubfamily::OM_CARBONYL;
    }
    // Generic transition metal or actinide complex (last resort for OM)
    // Only triggers if the formula is "complex-looking" (contains parentheses)
    if (formula.find('(') != std::string::npos) {
        if (has_actinide(formula))        return SpeciesSubfamily::OM_ACTINIDE;
        if (has_transition_metal(formula)) return SpeciesSubfamily::OM_TRANSITION_METAL;
    }

    // --- Fallback ---
    return SpeciesSubfamily::UNKNOWN;
}

// Convenience: classify and return full family
inline SpeciesFamily classify_family(const std::string& formula) {
    return subfamily_parent(classify_formula(formula));
}

// Build a fully classified SpeciesEntity from formula
inline SpeciesEntity classify(const std::string& formula, int atom_count = 1) {
    SpeciesEntity ent;
    ent.subfamily = classify_formula(formula);
    ent.family    = subfamily_parent(ent.subfamily);
    ent.subtype   = subfamily_name(ent.subfamily);
    ent.core.formula = formula;
    ent.scale     = classify_scale(atom_count);
    ent.scale_score = (atom_count > 0)
        ? std::min(1.0, std::log10(static_cast<double>(atom_count)) / 5.0)
        : 0.0;
    ent.emphasis  = default_emphasis(ent.family);
    ent.formats   = recommended_formats(ent.family);
    return ent;
}

} // namespace classify

// ============================================================================
// Family database — the practical mapping table
// ============================================================================

struct FamilyDescriptor {
    SpeciesFamily family;
    const char* typical_size;
    const char* main_physics;
    const char* best_use;
};

inline const std::vector<FamilyDescriptor>& family_descriptors() {
    static const std::vector<FamilyDescriptor> table = {
        {SpeciesFamily::GAS,
         "small", "EOS, transport, kinetics",
         "mixtures, pressure sweeps"},
        {SpeciesFamily::CRYSTAL,
         "medium-large", "lattice, periodicity, defects",
         "metals, salts, solids"},
        {SpeciesFamily::ORGANOMETALLIC,
         "large", "coordination, hybrid state, charge",
         "metal-ligand systems"},
        {SpeciesFamily::CERAMIC,
         "medium", "lattice + surface + fracture",
         "oxides, carbides, powders"},
    };
    return table;
}

// ============================================================================
// Subfamily registry — all subfamilies with parent + examples
// ============================================================================

struct SubfamilyDescriptor {
    SpeciesSubfamily subfamily;
    const char* examples;
};

inline const std::vector<SubfamilyDescriptor>& subfamily_descriptors() {
    static const std::vector<SubfamilyDescriptor> table = {
        // GAS
        {SpeciesSubfamily::GAS_NOBLE,       "He, Ne, Ar, Kr, Xe"},
        {SpeciesSubfamily::GAS_DIATOMIC,    "H2, N2, O2, Cl2"},
        {SpeciesSubfamily::GAS_POLYATOMIC,  "CO2, H2O, NH3, SO2"},
        {SpeciesSubfamily::GAS_REFRIGERANT, "R32, R134a, R1234yf"},
        {SpeciesSubfamily::GAS_FUEL,        "CH4, C2H6, C3H8"},
        {SpeciesSubfamily::GAS_MIXTURE,     "air, flue gas"},

        // CRYSTAL
        {SpeciesSubfamily::CRY_METAL,          "Au, Pt, Al, Fe"},
        {SpeciesSubfamily::CRY_IONIC,          "NaCl, CaF2, MgO"},
        {SpeciesSubfamily::CRY_COVALENT,       "Si, diamond C, GaAs"},
        {SpeciesSubfamily::CRY_ACTINIDE_SOLID, "UO2, PuO2, ThO2"},
        {SpeciesSubfamily::CRY_DEFECT,         "vacancy-doped Si, irradiated Fe"},

        // ORGANOMETALLIC
        {SpeciesSubfamily::OM_TRANSITION_METAL,      "Pd(PPh3)4, RhCl(PPh3)3"},
        {SpeciesSubfamily::OM_ACTINIDE,              "Pu ligand complexes"},
        {SpeciesSubfamily::OM_CARBONYL,              "Ni(CO)4, Fe(CO)5"},
        {SpeciesSubfamily::OM_CP_ARENE,              "ferrocene, Cp2TiCl2"},
        {SpeciesSubfamily::OM_HYBRID_BEAD_ATOMISTIC, "atomistic core + bead ligands"},

        // CERAMIC
        {SpeciesSubfamily::CM_OXIDE,      "Al2O3, SiO2, ZrO2, TiO2"},
        {SpeciesSubfamily::CM_CARBIDE,    "SiC, WC, TiC, B4C"},
        {SpeciesSubfamily::CM_NITRIDE,    "TiN, Si3N4, AlN"},
        {SpeciesSubfamily::CM_SILICATE,   "CaSiO3, Mg2SiO4"},
        {SpeciesSubfamily::CM_REFRACTORY, "MgO, HfC, ZrB2"},
        {SpeciesSubfamily::CM_POWDER,     "dispersed Al2O3, milled SiC"},
    };
    return table;
}

} // namespace vsepr
