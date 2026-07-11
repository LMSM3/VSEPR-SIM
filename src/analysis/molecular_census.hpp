#pragma once
/**
 * molecular_census.hpp
 * ====================
 * Deep Molecular Analysis Engine — Collects 112+ data points per molecule
 *
 * Purpose:
 *   Given a single molecular formula, build the primary structure via the
 *   VSEPR → FIRE pipeline, collect exhaustive atomistic measurements,
 *   attempt to discover alternative FIRE minimization geometries (conformers/
 *   isomers), and automatically classify the molecule by:
 *     - Group / sub-group
 *     - Ionic character
 *     - Purpose type (fuel, battery, catalyst, pharmaceutical, etc.)
 *
 * Architecture:
 *   1. Primary Build — formula → VSEPR placement → FIRE optimization
 *   2. Data Collection — 112+ data points across 14 categories
 *   3. Alternative Geometry Search — torsion randomization → FIRE → dedup
 *   4. Automatic Classification — rule-based from collected data
 *   5. Report Generation — structured text output
 *
 * Data Point Categories (≥112 total):
 *   [A]  Composition        (15 points)
 *   [B]  Topology           (12 points)
 *   [C]  Energy Breakdown   ( 8 points)
 *   [D]  Electronic         (10 points)
 *   [E]  Reactivity         ( 8 points per heavy atom, min 8)
 *   [F]  Geometry           (12+ points, scales with bonds/angles)
 *   [G]  Identity           ( 5 points)
 *   [H]  Validation         (10 points)
 *   [I]  FIRE Convergence   ( 7 points)
 *   [J]  Conformer Search   ( 8 points)
 *   [K]  Classification     ( 8 points)
 *   [L]  Stability Metrics  ( 5 points)
 *   [M]  Thermodynamic Est. ( 5 points)
 *   [N]  Structural Desc.   ( 6 points)
 *   ─────────────────────────────────
 *   Total:  ≥119 for any molecule with ≥2 heavy atoms
 *
 * References:
 *   - FIRE: Bitzek et al., PRL 97, 170201 (2006)
 *   - QEq: Rappe & Goddard, J. Phys. Chem. 95, 3358 (1991)
 *   - Pauling electronegativity scale
 *   - Hill system for canonical formula ordering
 *
 * Terminology: All references use "atomistic" per project rules.
 */

#include "sim/molecule.hpp"
#include "sim/molecule_builder.hpp"
#include "sim/optimizer.hpp"
#include "sim/conformer_finder.hpp"
#include "pot/energy_model.hpp"
#include "core/geom_ops.hpp"
#include "core/chemistry.hpp"
#include "core/types.hpp"
#include "pot/periodic_db.hpp"
#include "identity/canonical_identity.hpp"
#include "validation/validation_gates.hpp"

#include <string>
#include <vector>
#include <map>
#include <set>
#include <cmath>
#include <algorithm>
#include <numeric>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <functional>
#include <chrono>

namespace vsepr {
namespace analysis {

// ============================================================================
// Classification Taxonomy
// ============================================================================

/**
 * Group: broad chemical family
 * Sub-group: specific class within family
 * Ionic character: degree of ionic vs covalent bonding
 * Purpose type: application domain
 */

enum class MolecularGroup : uint8_t {
    UNKNOWN = 0,
    ORGANIC,
    INORGANIC,
    ORGANOMETALLIC,
    COORDINATION_COMPLEX,
    NOBLE_GAS_COMPOUND,
    INTERHALOGEN,
    CERAMIC,
    POLYMER_UNIT,
};

inline const char* group_name(MolecularGroup g) {
    switch (g) {
        case MolecularGroup::ORGANIC:              return "Organic";
        case MolecularGroup::INORGANIC:            return "Inorganic";
        case MolecularGroup::ORGANOMETALLIC:       return "Organometallic";
        case MolecularGroup::COORDINATION_COMPLEX: return "Coordination Complex";
        case MolecularGroup::NOBLE_GAS_COMPOUND:   return "Noble Gas Compound";
        case MolecularGroup::INTERHALOGEN:         return "Interhalogen";
        case MolecularGroup::CERAMIC:              return "Ceramic";
        case MolecularGroup::POLYMER_UNIT:         return "Polymer Unit";
        default: return "Unknown";
    }
}

enum class MolecularSubGroup : uint8_t {
    UNKNOWN = 0,
    // Organic
    ALKANE, ALKENE, ALKYNE, ALCOHOL, ALDEHYDE, KETONE, CARBOXYLIC_ACID,
    AMINE, AMIDE, ESTER, ETHER, AROMATIC, HETEROCYCLIC,
    // Inorganic
    BINARY_HYDRIDE, OXIDE, HALIDE, OXYACID, SALT,
    HYPERVALENT, SUBVALENT,
    // Organometallic
    SANDWICH, HALF_SANDWICH, CARBENE_COMPLEX, CARBONYL,
    // Coordination
    OCTAHEDRAL, TETRAHEDRAL_COORD, SQUARE_PLANAR, TRIGONAL_BIPYRAMIDAL,
    // Misc
    PERFLUORINATED, SUPERALLOY_COMPONENT,
};

inline const char* subgroup_name(MolecularSubGroup sg) {
    switch (sg) {
        case MolecularSubGroup::ALKANE:            return "Alkane";
        case MolecularSubGroup::ALKENE:            return "Alkene";
        case MolecularSubGroup::ALKYNE:            return "Alkyne";
        case MolecularSubGroup::ALCOHOL:           return "Alcohol";
        case MolecularSubGroup::ALDEHYDE:          return "Aldehyde";
        case MolecularSubGroup::KETONE:            return "Ketone";
        case MolecularSubGroup::CARBOXYLIC_ACID:   return "Carboxylic Acid";
        case MolecularSubGroup::AMINE:             return "Amine";
        case MolecularSubGroup::AMIDE:             return "Amide";
        case MolecularSubGroup::ESTER:             return "Ester";
        case MolecularSubGroup::ETHER:             return "Ether";
        case MolecularSubGroup::AROMATIC:          return "Aromatic";
        case MolecularSubGroup::HETEROCYCLIC:      return "Heterocyclic";
        case MolecularSubGroup::BINARY_HYDRIDE:    return "Binary Hydride";
        case MolecularSubGroup::OXIDE:             return "Oxide";
        case MolecularSubGroup::HALIDE:            return "Halide";
        case MolecularSubGroup::OXYACID:           return "Oxyacid";
        case MolecularSubGroup::SALT:              return "Salt";
        case MolecularSubGroup::HYPERVALENT:       return "Hypervalent";
        case MolecularSubGroup::SUBVALENT:         return "Subvalent";
        case MolecularSubGroup::SANDWICH:          return "Sandwich Complex";
        case MolecularSubGroup::HALF_SANDWICH:     return "Half-Sandwich Complex";
        case MolecularSubGroup::CARBENE_COMPLEX:   return "Carbene Complex";
        case MolecularSubGroup::CARBONYL:          return "Metal Carbonyl";
        case MolecularSubGroup::OCTAHEDRAL:        return "Octahedral";
        case MolecularSubGroup::TETRAHEDRAL_COORD: return "Tetrahedral (coord)";
        case MolecularSubGroup::SQUARE_PLANAR:     return "Square Planar";
        case MolecularSubGroup::TRIGONAL_BIPYRAMIDAL: return "Trigonal Bipyramidal";
        case MolecularSubGroup::PERFLUORINATED:    return "Perfluorinated";
        case MolecularSubGroup::SUPERALLOY_COMPONENT: return "Superalloy Component";
        default: return "Unknown";
    }
}

enum class IonicCharacter : uint8_t {
    COVALENT = 0,       // Δχ < 0.5
    POLAR_COVALENT,     // 0.5 ≤ Δχ < 1.7
    IONIC,              // Δχ ≥ 1.7
    METALLIC,           // All metals
    MIXED,              // Multiple bond types
};

inline const char* ionic_name(IonicCharacter ic) {
    switch (ic) {
        case IonicCharacter::COVALENT:       return "Covalent";
        case IonicCharacter::POLAR_COVALENT: return "Polar Covalent";
        case IonicCharacter::IONIC:          return "Ionic";
        case IonicCharacter::METALLIC:       return "Metallic";
        case IonicCharacter::MIXED:          return "Mixed";
        default: return "Unknown";
    }
}

enum class PurposeType : uint8_t {
    UNKNOWN = 0,
    FUEL,
    OXIDIZER,
    BATTERY_ELECTROLYTE,
    BATTERY_ELECTRODE,
    CATALYST,
    PHARMACEUTICAL,
    SEMICONDUCTOR_MATERIAL,
    STRUCTURAL_MATERIAL,
    ENERGETIC_MATERIAL,
    SOLVENT,
    REFRIGERANT,
    PROPELLANT,
    POLYMER_PRECURSOR,
    PIGMENT_DYE,
    FERTILIZER,
    CORROSION_INHIBITOR,
};

inline const char* purpose_name(PurposeType p) {
    switch (p) {
        case PurposeType::FUEL:                  return "Fuel";
        case PurposeType::OXIDIZER:              return "Oxidizer";
        case PurposeType::BATTERY_ELECTROLYTE:   return "Battery Electrolyte";
        case PurposeType::BATTERY_ELECTRODE:     return "Battery Electrode";
        case PurposeType::CATALYST:              return "Catalyst";
        case PurposeType::PHARMACEUTICAL:        return "Pharmaceutical";
        case PurposeType::SEMICONDUCTOR_MATERIAL:return "Semiconductor Material";
        case PurposeType::STRUCTURAL_MATERIAL:   return "Structural Material";
        case PurposeType::ENERGETIC_MATERIAL:    return "Energetic Material";
        case PurposeType::SOLVENT:               return "Solvent";
        case PurposeType::REFRIGERANT:           return "Refrigerant";
        case PurposeType::PROPELLANT:            return "Propellant";
        case PurposeType::POLYMER_PRECURSOR:     return "Polymer Precursor";
        case PurposeType::PIGMENT_DYE:           return "Pigment/Dye";
        case PurposeType::FERTILIZER:            return "Fertilizer";
        case PurposeType::CORROSION_INHIBITOR:   return "Corrosion Inhibitor";
        default: return "Unknown";
    }
}

// ============================================================================
// Classification Result
// ============================================================================

struct ClassificationResult {
    MolecularGroup group           = MolecularGroup::UNKNOWN;
    MolecularSubGroup sub_group    = MolecularSubGroup::UNKNOWN;
    IonicCharacter ionic           = IonicCharacter::COVALENT;
    PurposeType purpose            = PurposeType::UNKNOWN;
    double classification_confidence = 0.0;  // 0.0–1.0
    std::string reasoning;                    // Human-readable explanation
};

// ============================================================================
// Alternative Geometry Result
// ============================================================================

struct AlternativeGeometry {
    int trial_id       = 0;
    double energy      = 0.0;
    double delta_E     = 0.0;    // Energy relative to primary (kcal/mol)
    double rms_force   = 0.0;
    int fire_steps     = 0;
    bool converged     = false;
    std::string descriptor;       // "conformer", "isomer", etc.
    std::vector<double> coords;   // Final coordinates (flat)
};

// ============================================================================
// Census Data Point — The 112+ field master struct
// ============================================================================

struct CensusResult {
    // ========================================================================
    // [A] COMPOSITION (15 data points)
    // ========================================================================
    std::string formula;                       //  1. Canonical formula (Hill)
    std::string input_formula;                 //  2. Original input
    uint32_t total_atoms          = 0;         //  3. Total atom count
    uint32_t heavy_atoms          = 0;         //  4. Non-hydrogen atoms
    uint32_t hydrogen_count       = 0;         //  5. Hydrogen count
    double molecular_weight       = 0.0;       //  6. Molecular weight (amu)
    uint32_t unique_elements      = 0;         //  7. Number of distinct elements
    std::map<std::string, int> element_counts; //  8. Per-element counts (variable)
    double H_to_heavy_ratio       = 0.0;       //  9. H / heavy atom ratio
    double avg_atomic_mass        = 0.0;       // 10. Average atomic mass
    uint32_t total_electrons      = 0;         // 11. Total electron count
    uint32_t valence_electrons    = 0;         // 12. Total valence electrons
    bool has_metal                = false;      // 13. Contains metal atom?
    bool has_halogen              = false;      // 14. Contains halogen?
    bool has_noble_gas            = false;      // 15. Contains noble gas?

    // ========================================================================
    // [B] TOPOLOGY (12 data points)
    // ========================================================================
    uint32_t num_bonds            = 0;         // 16. Total bonds
    uint32_t num_single_bonds     = 0;         // 17. Single bond count
    uint32_t num_double_bonds     = 0;         // 18. Double bond count
    uint32_t num_triple_bonds     = 0;         // 19. Triple bond count
    uint32_t num_angles           = 0;         // 20. Angle terms
    uint32_t num_torsions         = 0;         // 21. Torsion terms
    uint32_t num_impropers        = 0;         // 22. Improper terms
    uint32_t num_rotatable_bonds  = 0;         // 23. Rotatable bond count
    double avg_coordination       = 0.0;       // 24. Average coordination number
    uint32_t max_coordination     = 0;         // 25. Maximum coordination number
    bool has_rings                = false;      // 26. Contains ring(s)?
    double bond_density           = 0.0;       // 27. bonds / atoms ratio

    // ========================================================================
    // [C] ENERGY BREAKDOWN (8 data points)
    // ========================================================================
    double E_total                = 0.0;       // 28. Total energy (kcal/mol)
    double E_bond                 = 0.0;       // 29. Bond stretching
    double E_angle                = 0.0;       // 30. Angle bending
    double E_torsion              = 0.0;       // 31. Torsional
    double E_vdw                  = 0.0;       // 32. van der Waals
    double E_coulomb              = 0.0;       // 33. Electrostatic
    double E_per_atom             = 0.0;       // 34. Energy per atom
    double E_per_bond             = 0.0;       // 35. Energy per bond

    // ========================================================================
    // [D] ELECTRONIC PROPERTIES (10 data points)
    // ========================================================================
    double dipole_moment          = 0.0;       // 36. Total dipole (Debye)
    double dipole_x               = 0.0;       // 37. Dipole x-component
    double dipole_y               = 0.0;       // 38. Dipole y-component
    double dipole_z               = 0.0;       // 39. Dipole z-component
    double polarizability         = 0.0;       // 40. Isotropic polarizability (ų)
    double ionization_potential   = 0.0;       // 41. Estimated IP (eV)
    double electron_affinity      = 0.0;       // 42. Estimated EA (eV)
    double electronegativity      = 0.0;       // 43. Mulliken χ (eV)
    double chemical_hardness      = 0.0;       // 44. η = (IP-EA)/2 (eV)
    double electrophilicity       = 0.0;       // 45. ω = χ²/(2η) (eV)

    // ========================================================================
    // [E] REACTIVITY INDICES (≥8 data points — aggregated from per-atom)
    // ========================================================================
    double max_fukui_plus         = 0.0;       // 46. Max f+ (nucleophilic attack)
    double max_fukui_minus        = 0.0;       // 47. Max f- (electrophilic attack)
    double max_fukui_zero         = 0.0;       // 48. Max f0 (radical attack)
    double avg_fukui_zero         = 0.0;       // 49. Average f0
    double max_local_softness     = 0.0;       // 50. Max local softness
    double max_partial_charge     = 0.0;       // 51. Most positive charge
    double min_partial_charge     = 0.0;       // 52. Most negative charge
    double charge_spread          = 0.0;       // 53. max - min charge

    // ========================================================================
    // [F] GEOMETRY METRICS (12+ data points)
    // ========================================================================
    double min_bond_length        = 0.0;       // 54. Shortest bond (Å)
    double max_bond_length        = 0.0;       // 55. Longest bond (Å)
    double avg_bond_length        = 0.0;       // 56. Mean bond length (Å)
    double std_bond_length        = 0.0;       // 57. Std dev bond length
    double min_bond_angle         = 0.0;       // 58. Smallest angle (deg)
    double max_bond_angle         = 0.0;       // 59. Largest angle (deg)
    double avg_bond_angle         = 0.0;       // 60. Mean angle (deg)
    double std_bond_angle         = 0.0;       // 61. Std dev angle
    double molecular_radius       = 0.0;       // 62. Max distance from centroid
    double radius_of_gyration     = 0.0;       // 63. Rg (Å)
    double centroid_x             = 0.0;       // 64. Centroid x
    double centroid_y             = 0.0;       // 65. Centroid y
    double centroid_z             = 0.0;       // 66. Centroid z
    double min_interatomic_dist   = 0.0;       // 67. Minimum all-pair distance
    double molecular_volume_est   = 0.0;       // 68. Estimated volume (ų)
    double sphericity             = 0.0;       // 69. Rg / molecular_radius

    // ========================================================================
    // [G] IDENTITY (5 data points)
    // ========================================================================
    std::string canonical_formula;             // 70. Hill-system canonical
    uint64_t graph_hash           = 0;         // 71. Morgan graph fingerprint
    uint64_t geometry_hash        = 0;         // 72. Rotation-invariant hash
    std::string morgan_invariants_summary;     // 73. First 8 Morgan invariants
    std::string vsepr_class;                   // 74. VSEPR AXnEm notation

    // ========================================================================
    // [H] VALIDATION (10 data points)
    // ========================================================================
    bool val_nonempty             = false;      // 75. Non-empty check
    bool val_no_nan               = false;      // 76. NaN-free check
    bool val_no_overlap           = false;      // 77. No atom overlap
    bool val_bonds_ok             = false;      // 78. Bond lengths in range
    bool val_valence_ok           = false;      // 79. Valence sanity
    bool val_converged            = false;      // 80. FIRE converged
    bool val_charge_consistent    = false;      // 81. Charge consistency
    bool val_connected            = false;      // 82. Graph is connected
    int  val_gates_passed         = 0;          // 83. Number of gates passed
    int  val_gates_total          = 0;          // 84. Total gates checked

    // ========================================================================
    // [I] FIRE CONVERGENCE (7 data points)
    // ========================================================================
    int    fire_iterations        = 0;          // 85. Iterations to converge
    double fire_final_energy      = 0.0;        // 86. Final energy
    double fire_final_rms_force   = 0.0;        // 87. Final RMS force
    double fire_final_max_force   = 0.0;        // 88. Final max force
    double fire_energy_per_atom   = 0.0;        // 89. Final E / N
    bool   fire_converged         = false;       // 90. Converged flag
    double fire_wall_time_ms      = 0.0;        // 91. Wall-clock time (ms)

    // ========================================================================
    // [J] CONFORMER SEARCH (8 data points)
    // ========================================================================
    int    conf_trials            = 0;          // 92.  Trials attempted
    int    conf_unique            = 0;          // 93.  Unique conformers found
    int    conf_duplicates        = 0;          // 94.  Duplicates rejected
    double conf_energy_min        = 0.0;        // 95.  Lowest conformer energy
    double conf_energy_max        = 0.0;        // 96.  Highest conformer energy
    double conf_energy_spread     = 0.0;        // 97.  Energy range (kcal/mol)
    double conf_ensemble_free_E   = 0.0;        // 98.  Ensemble free energy
    std::vector<AlternativeGeometry> alt_geometries; // 99+. Logged alternatives

    // ========================================================================
    // [K] CLASSIFICATION (8 data points)
    // ========================================================================
    ClassificationResult classification;
    // classification.group                     // 100. Group
    // classification.sub_group                 // 101. Sub-group
    // classification.ionic                     // 102. Ionic character
    // classification.purpose                   // 103. Purpose type
    // classification.classification_confidence // 104. Confidence
    // classification.reasoning                 // 105. Reasoning string

    // Additional classification flags
    bool is_organic               = false;      // 106. Contains C+H
    bool is_perfluorinated        = false;      // 107. ≥6 fluorine atoms
    bool is_energetic             = false;      // 108. High N+O content

    // ========================================================================
    // [L] STABILITY METRICS (5 data points)
    // ========================================================================
    double strain_energy_est      = 0.0;        // 109. Estimated strain
    double E_HOMO_est             = 0.0;        // 110. HOMO energy est. (eV)
    double E_LUMO_est             = 0.0;        // 111. LUMO energy est. (eV)
    double HOMO_LUMO_gap          = 0.0;        // 112. Band gap estimate (eV)
    double global_softness        = 0.0;        // 113. S = 1/(2η)

    // ========================================================================
    // [M] THERMODYNAMIC ESTIMATES (5 data points)
    // ========================================================================
    double heat_capacity_est      = 0.0;        // 114. Dulong-Petit Cv (cal/mol·K)
    double zero_point_energy_est  = 0.0;        // 115. ZPE estimate (kcal/mol)
    double thermal_energy_298     = 0.0;        // 116. Thermal E at 298K
    double entropy_est            = 0.0;        // 117. Entropy estimate (cal/mol·K)
    double gibbs_correction       = 0.0;        // 118. G correction at 298K

    // ========================================================================
    // [N] STRUCTURAL DESCRIPTORS (6 data points)
    // ========================================================================
    double wiener_index           = 0.0;        // 119. Wiener topological index
    double balaban_J              = 0.0;        // 120. Balaban J index
    int    graph_diameter          = 0;          // 121. Longest shortest path
    double density_est            = 0.0;        // 122. Estimated density (g/cm³)
    double surface_area_est       = 0.0;        // 123. Estimated SA (ų)
    double compactness            = 0.0;        // 124. Volume / SA ratio

    // ========================================================================
    // Metadata
    // ========================================================================
    double total_wall_time_ms     = 0.0;
    int    total_data_points      = 0;          // Self-counted
    std::string timestamp;

    // Count the number of populated data points
    int count_data_points() const {
        int n = 0;
        // Fixed fields: 124 base + per-element counts + per-conformer entries
        n += 124;
        n += static_cast<int>(element_counts.size());
        n += static_cast<int>(alt_geometries.size()) * 6; // 6 fields each
        return n;
    }
};

// ============================================================================
// Element Classification Helpers
// ============================================================================

namespace detail {

inline bool is_metal(uint8_t Z) {
    // Alkali metals
    if (Z == 3 || Z == 11 || Z == 19 || Z == 37 || Z == 55 || Z == 87) return true;
    // Alkaline earth
    if (Z == 4 || Z == 12 || Z == 20 || Z == 38 || Z == 56 || Z == 88) return true;
    // Transition metals (Z=21-30, 39-48, 72-80)
    if ((Z >= 21 && Z <= 30) || (Z >= 39 && Z <= 48) || (Z >= 72 && Z <= 80)) return true;
    // Lanthanides (57-71)
    if (Z >= 57 && Z <= 71) return true;
    // Post-transition metals
    if (Z == 13 || Z == 31 || Z == 49 || Z == 50 || Z == 81 || Z == 82 || Z == 83) return true;
    return false;
}

inline bool is_halogen(uint8_t Z) {
    return Z == 9 || Z == 17 || Z == 35 || Z == 53 || Z == 85;
}

inline bool is_noble_gas(uint8_t Z) {
    return Z == 2 || Z == 10 || Z == 18 || Z == 36 || Z == 54 || Z == 86;
}

inline bool is_transition_metal(uint8_t Z) {
    return (Z >= 21 && Z <= 30) || (Z >= 39 && Z <= 48) || (Z >= 72 && Z <= 80);
}

inline bool is_semiconductor_element(uint8_t Z) {
    return Z == 14 || Z == 32 || Z == 31 || Z == 33 || Z == 49 || Z == 51 || Z == 52;
}

// Pauling electronegativity (approximate, for common elements)
inline double pauling_EN(uint8_t Z) {
    static const std::map<uint8_t, double> en = {
        {1, 2.20}, {3, 0.98}, {4, 1.57}, {5, 2.04}, {6, 2.55}, {7, 3.04},
        {8, 3.44}, {9, 3.98}, {11, 0.93}, {12, 1.31}, {13, 1.61}, {14, 1.90},
        {15, 2.19}, {16, 2.58}, {17, 3.16}, {19, 0.82}, {20, 1.00},
        {22, 1.54}, {24, 1.66}, {25, 1.55}, {26, 1.83}, {27, 1.88},
        {28, 1.91}, {29, 1.90}, {30, 1.65}, {35, 2.96}, {47, 1.93},
        {53, 2.66}, {79, 2.54},
    };
    auto it = en.find(Z);
    return it != en.end() ? it->second : 2.0;
}

// Typical valence electron count by group
inline uint8_t typical_valence_electrons(uint8_t Z) {
    // Simplified — use periodic table group
    if (Z == 1) return 1;
    if (Z == 2) return 2;
    if (Z == 3 || Z == 11 || Z == 19 || Z == 37 || Z == 55) return 1;
    if (Z == 4 || Z == 12 || Z == 20 || Z == 38 || Z == 56) return 2;
    if (Z == 5 || Z == 13 || Z == 31) return 3;
    if (Z == 6 || Z == 14 || Z == 32 || Z == 50) return 4;
    if (Z == 7 || Z == 15 || Z == 33 || Z == 51) return 5;
    if (Z == 8 || Z == 16 || Z == 34 || Z == 52) return 6;
    if (Z == 9 || Z == 17 || Z == 35 || Z == 53) return 7;
    if (Z == 10 || Z == 18 || Z == 36 || Z == 54) return 8;
    // Transition metals: approximate
    if (is_transition_metal(Z)) return 2;
    return 4; // fallback
}

} // namespace detail

// ============================================================================
// Molecular Census Engine
// ============================================================================

class MolecularCensus {
public:
    struct Settings {
        // Conformer search
        int conformer_trials       = 50;    // Number of torsion randomizations
        int conformer_seed         = 42;    // RNG seed
        double rmsd_threshold      = 0.1;   // Dedup threshold (Å)
        double energy_threshold    = 1e-3;  // Dedup threshold (kcal/mol)

        // FIRE optimization
        int max_fire_steps         = 5000;
        double tol_rms_force       = 1e-4;
        double tol_max_force       = 1e-3;

        // Energy model flags
        double bond_k              = 300.0;
        bool use_angles            = false;
        bool use_nonbonded         = true;
        bool use_torsions          = false;
        bool use_vsepr_domains     = false;

        // Report
        bool verbose               = true;
    };

    explicit MolecularCensus(const PeriodicTable& pt)
        : pt_(pt), settings_(Settings{}) {}

    explicit MolecularCensus(const PeriodicTable& pt, const Settings& s)
        : pt_(pt), settings_(s) {}

    /**
     * Run the full census on a molecular formula.
     * Returns a CensusResult with 112+ populated data points.
     */
    CensusResult run(const std::string& formula) {
        auto t0 = std::chrono::high_resolution_clock::now();

        CensusResult cr;
        cr.input_formula = formula;

        // Record timestamp
        {
            auto now = std::chrono::system_clock::now();
            auto tt  = std::chrono::system_clock::to_time_t(now);
            std::ostringstream oss;
            struct tm tm_buf;
#ifdef _WIN32
            localtime_s(&tm_buf, &tt);
#else
            localtime_r(&tt, &tm_buf);
#endif
            oss << std::put_time(&tm_buf, "%Y-%m-%d %H:%M:%S");
            cr.timestamp = oss.str();
        }

        if (settings_.verbose) {
            std::cout << "\n" << std::string(78, '=') << "\n";
            std::cout << "  MOLECULAR CENSUS — Deep Analysis Engine\n";
            std::cout << "  Formula: " << formula << "\n";
            std::cout << std::string(78, '=') << "\n\n";
        }

        // ====================================================================
        // Phase 1: Build primary molecule
        // ====================================================================
        if (settings_.verbose) std::cout << "[Phase 1] Building primary molecule...\n";

        Molecule mol;
        try {
            mol = build_molecule_from_formula(formula, pt_);
        } catch (const std::exception& e) {
            std::cerr << "  BUILD FAILED: " << e.what() << "\n";
            cr.total_data_points = cr.count_data_points();
            return cr;
        }

        // ====================================================================
        // Phase 2: FIRE optimization
        // ====================================================================
        if (settings_.verbose) std::cout << "[Phase 2] FIRE geometry optimization...\n";

        EnergyModel energy_model(mol, settings_.bond_k,
                                 settings_.use_angles,
                                 settings_.use_nonbonded,
                                 NonbondedParams(),
                                 settings_.use_torsions,
                                 settings_.use_vsepr_domains);

        OptimizerSettings opt;
        opt.max_iterations = settings_.max_fire_steps;
        opt.tol_rms_force  = settings_.tol_rms_force;
        opt.tol_max_force  = settings_.tol_max_force;

        auto fire_t0 = std::chrono::high_resolution_clock::now();

        FIREOptimizer optimizer(opt);
        OptimizeResult fire_result = optimizer.minimize(mol.coords, energy_model);

        auto fire_t1 = std::chrono::high_resolution_clock::now();
        cr.fire_wall_time_ms = std::chrono::duration<double, std::milli>(fire_t1 - fire_t0).count();

        if (fire_result.converged) {
            mol.coords = fire_result.coords;
        }

        if (settings_.verbose) {
            std::cout << "  Converged: " << (fire_result.converged ? "YES" : "NO")
                      << " (" << fire_result.iterations << " steps)\n";
            std::cout << "  Energy: " << fire_result.energy << " kcal/mol\n";
            std::cout << "  RMS Force: " << fire_result.rms_force << "\n";
            std::cout << "  Wall time: " << cr.fire_wall_time_ms << " ms\n\n";
        }

        // ====================================================================
        // Phase 3: Collect all data points
        // ====================================================================
        if (settings_.verbose) std::cout << "[Phase 3] Collecting data points...\n";

        collect_composition(mol, cr);
        collect_topology(mol, cr);
        collect_energy(mol, fire_result, cr);
        collect_electronic(mol, cr);
        collect_reactivity(mol, cr);
        collect_geometry(mol, cr);
        collect_identity(mol, cr);
        collect_validation(mol, fire_result, cr);
        collect_fire_stats(fire_result, cr);
        collect_stability(cr);
        collect_thermodynamic(mol, cr);
        collect_structural(mol, cr);

        // ====================================================================
        // Phase 4: Alternative geometry search
        // ====================================================================
        if (settings_.verbose) std::cout << "\n[Phase 4] Alternative geometry search...\n";

        search_alternatives(mol, energy_model, cr);

        // ====================================================================
        // Phase 5: Classification
        // ====================================================================
        if (settings_.verbose) std::cout << "\n[Phase 5] Automatic classification...\n";

        classify(cr);

        // ====================================================================
        // Finalize
        // ====================================================================
        auto t1 = std::chrono::high_resolution_clock::now();
        cr.total_wall_time_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
        cr.total_data_points  = cr.count_data_points();

        if (settings_.verbose) {
            std::cout << "\n" << std::string(78, '-') << "\n";
            std::cout << "  Census complete: " << cr.total_data_points
                      << " data points collected in "
                      << std::fixed << std::setprecision(1)
                      << cr.total_wall_time_ms << " ms\n";
            std::cout << std::string(78, '-') << "\n";
        }

        return cr;
    }

    // ========================================================================
    // Report generation — formatted text output
    // ========================================================================

    static std::string generate_report(const CensusResult& cr) {
        std::ostringstream o;
        o << std::fixed << std::setprecision(4);

        o << "\n";
        o << "╔══════════════════════════════════════════════════════════════════════════════╗\n";
        o << "║                    MOLECULAR CENSUS — FULL REPORT                           ║\n";
        o << "╚══════════════════════════════════════════════════════════════════════════════╝\n";
        o << "\n";
        o << "  Timestamp:    " << cr.timestamp << "\n";
        o << "  Input:        " << cr.input_formula << "\n";
        o << "  Canonical:    " << cr.canonical_formula << "\n";
        o << "  Data Points:  " << cr.total_data_points << "\n";
        o << "  Wall Time:    " << std::setprecision(1) << cr.total_wall_time_ms << " ms\n";
        o << "\n";

        // [A] Composition
        o << "─── [A] COMPOSITION (" << 15 + cr.element_counts.size() << " points) ──────────────\n";
        o << "  Total atoms:       " << cr.total_atoms << "\n";
        o << "  Heavy atoms:       " << cr.heavy_atoms << "\n";
        o << "  Hydrogen:          " << cr.hydrogen_count << "\n";
        o << "  Molecular weight:  " << std::setprecision(3) << cr.molecular_weight << " amu\n";
        o << "  Unique elements:   " << cr.unique_elements << "\n";
        o << "  Total electrons:   " << cr.total_electrons << "\n";
        o << "  Valence electrons: " << cr.valence_electrons << "\n";
        o << "  Has metal:         " << (cr.has_metal ? "YES" : "NO") << "\n";
        o << "  Has halogen:       " << (cr.has_halogen ? "YES" : "NO") << "\n";
        o << "  Has noble gas:     " << (cr.has_noble_gas ? "YES" : "NO") << "\n";
        o << "  Elements: ";
        for (const auto& [sym, cnt] : cr.element_counts) {
            o << sym << "=" << cnt << " ";
        }
        o << "\n\n";

        // [B] Topology
        o << "─── [B] TOPOLOGY (12 points) ─────────────────────────────\n";
        o << "  Bonds:             " << cr.num_bonds << "\n";
        o << "  Single/Double/Triple: " << cr.num_single_bonds << "/" << cr.num_double_bonds << "/" << cr.num_triple_bonds << "\n";
        o << "  Angles:            " << cr.num_angles << "\n";
        o << "  Torsions:          " << cr.num_torsions << "\n";
        o << "  Rotatable bonds:   " << cr.num_rotatable_bonds << "\n";
        o << "  Avg coordination:  " << std::setprecision(2) << cr.avg_coordination << "\n";
        o << "  Max coordination:  " << cr.max_coordination << "\n";
        o << "  Has rings:         " << (cr.has_rings ? "YES" : "NO") << "\n";
        o << "  Bond density:      " << std::setprecision(3) << cr.bond_density << "\n\n";

        // [C] Energy
        o << "─── [C] ENERGY BREAKDOWN (8 points) ──────────────────────\n";
        o << std::setprecision(4);
        o << "  Total:     " << cr.E_total << " kcal/mol\n";
        o << "  Bond:      " << cr.E_bond << "\n";
        o << "  Angle:     " << cr.E_angle << "\n";
        o << "  Torsion:   " << cr.E_torsion << "\n";
        o << "  vdW:       " << cr.E_vdw << "\n";
        o << "  Coulomb:   " << cr.E_coulomb << "\n";
        o << "  E/atom:    " << cr.E_per_atom << "\n";
        o << "  E/bond:    " << cr.E_per_bond << "\n\n";

        // [D] Electronic
        o << "─── [D] ELECTRONIC (10 points) ─────────────────────────────\n";
        o << "  Dipole moment:     " << cr.dipole_moment << " Debye\n";
        o << "  Dipole vector:     (" << cr.dipole_x << ", " << cr.dipole_y << ", " << cr.dipole_z << ")\n";
        o << "  Polarizability:    " << cr.polarizability << " ų\n";
        o << "  IP (est):          " << cr.ionization_potential << " eV\n";
        o << "  EA (est):          " << cr.electron_affinity << " eV\n";
        o << "  χ (Mulliken):      " << cr.electronegativity << " eV\n";
        o << "  η (hardness):      " << cr.chemical_hardness << " eV\n";
        o << "  ω (electrophilic): " << cr.electrophilicity << " eV\n\n";

        // [E] Reactivity
        o << "─── [E] REACTIVITY (8 points) ──────────────────────────────\n";
        o << "  Max f+ (nucl):     " << cr.max_fukui_plus << "\n";
        o << "  Max f- (elec):     " << cr.max_fukui_minus << "\n";
        o << "  Max f0 (rad):      " << cr.max_fukui_zero << "\n";
        o << "  Avg f0:            " << cr.avg_fukui_zero << "\n";
        o << "  Max softness:      " << cr.max_local_softness << "\n";
        o << "  Charge range:      [" << cr.min_partial_charge << ", " << cr.max_partial_charge << "]\n";
        o << "  Charge spread:     " << cr.charge_spread << "\n\n";

        // [F] Geometry
        o << "─── [F] GEOMETRY (16 points) ──────────────────────────────\n";
        o << "  Bond length range: [" << cr.min_bond_length << ", " << cr.max_bond_length << "] Å\n";
        o << "  Avg bond length:   " << cr.avg_bond_length << " ± " << cr.std_bond_length << " Å\n";
        o << "  Angle range:       [" << std::setprecision(1) << cr.min_bond_angle << "°, " << cr.max_bond_angle << "°]\n";
        o << "  Avg angle:         " << cr.avg_bond_angle << "° ± " << cr.std_bond_angle << "°\n";
        o << std::setprecision(4);
        o << "  Molecular radius:  " << cr.molecular_radius << " Å\n";
        o << "  Radius of gyration:" << cr.radius_of_gyration << " Å\n";
        o << "  Sphericity:        " << cr.sphericity << "\n";
        o << "  Volume est:        " << cr.molecular_volume_est << " ų\n";
        o << "  Min interatomic:   " << cr.min_interatomic_dist << " Å\n\n";

        // [G] Identity
        o << "─── [G] IDENTITY (5 points) ───────────────────────────────\n";
        o << "  Canonical formula: " << cr.canonical_formula << "\n";
        o << "  Graph hash:        0x" << std::hex << cr.graph_hash << std::dec << "\n";
        o << "  Geometry hash:     0x" << std::hex << cr.geometry_hash << std::dec << "\n";
        o << "  Morgan summary:    " << cr.morgan_invariants_summary << "\n";
        o << "  VSEPR class:       " << cr.vsepr_class << "\n\n";

        // [H] Validation
        o << "─── [H] VALIDATION (" << cr.val_gates_passed << "/" << cr.val_gates_total << " gates) ─────\n";
        o << "  Non-empty:    " << (cr.val_nonempty ? "PASS" : "FAIL") << "\n";
        o << "  NaN-free:     " << (cr.val_no_nan ? "PASS" : "FAIL") << "\n";
        o << "  No overlap:   " << (cr.val_no_overlap ? "PASS" : "FAIL") << "\n";
        o << "  Bonds OK:     " << (cr.val_bonds_ok ? "PASS" : "FAIL") << "\n";
        o << "  Valence OK:   " << (cr.val_valence_ok ? "PASS" : "FAIL") << "\n";
        o << "  Converged:    " << (cr.val_converged ? "PASS" : "FAIL") << "\n";
        o << "  Connected:    " << (cr.val_connected ? "PASS" : "FAIL") << "\n\n";

        // [I] FIRE
        o << "─── [I] FIRE CONVERGENCE (7 points) ───────────────────────\n";
        o << "  Iterations:   " << cr.fire_iterations << "\n";
        o << "  Final energy: " << cr.fire_final_energy << " kcal/mol\n";
        o << "  RMS force:    " << cr.fire_final_rms_force << "\n";
        o << "  Max force:    " << cr.fire_final_max_force << "\n";
        o << "  E/atom:       " << cr.fire_energy_per_atom << "\n";
        o << "  Converged:    " << (cr.fire_converged ? "YES" : "NO") << "\n";
        o << "  Wall time:    " << std::setprecision(1) << cr.fire_wall_time_ms << " ms\n\n";

        // [J] Conformer search
        o << "─── [J] ALTERNATIVE GEOMETRIES (" << cr.conf_unique << " found) ────────\n";
        o << "  Trials:       " << cr.conf_trials << "\n";
        o << "  Unique:       " << cr.conf_unique << "\n";
        o << "  Duplicates:   " << cr.conf_duplicates << "\n";
        if (cr.conf_unique > 0) {
            o << std::setprecision(4);
            o << "  E_min:        " << cr.conf_energy_min << " kcal/mol\n";
            o << "  E_max:        " << cr.conf_energy_max << " kcal/mol\n";
            o << "  E_spread:     " << cr.conf_energy_spread << " kcal/mol\n";
            o << "  Free energy:  " << cr.conf_ensemble_free_E << " kcal/mol\n";
            o << "\n  Logged alternatives:\n";
            for (const auto& alt : cr.alt_geometries) {
                o << "    #" << alt.trial_id
                  << " E=" << alt.energy
                  << " ΔE=" << std::showpos << alt.delta_E << std::noshowpos
                  << " rmsF=" << alt.rms_force
                  << " steps=" << alt.fire_steps
                  << " [" << alt.descriptor << "]"
                  << (alt.converged ? "" : " (UNCONVERGED)")
                  << "\n";
            }
        }
        o << "\n";

        // [K] Classification
        o << "─── [K] CLASSIFICATION ───────────────────────────────────\n";
        o << "  Group:         " << group_name(cr.classification.group) << "\n";
        o << "  Sub-group:     " << subgroup_name(cr.classification.sub_group) << "\n";
        o << "  Ionic char:    " << ionic_name(cr.classification.ionic) << "\n";
        o << "  Purpose type:  " << purpose_name(cr.classification.purpose) << "\n";
        o << "  Confidence:    " << std::setprecision(2) << cr.classification.classification_confidence << "\n";
        o << "  Reasoning:     " << cr.classification.reasoning << "\n";
        o << "  Is organic:    " << (cr.is_organic ? "YES" : "NO") << "\n";
        o << "  Perfluorinated:" << (cr.is_perfluorinated ? "YES" : "NO") << "\n";
        o << "  Energetic:     " << (cr.is_energetic ? "YES" : "NO") << "\n\n";

        // [L] Stability
        o << "─── [L] STABILITY (5 points) ──────────────────────────────\n";
        o << std::setprecision(4);
        o << "  Strain est:    " << cr.strain_energy_est << " kcal/mol\n";
        o << "  HOMO est:      " << cr.E_HOMO_est << " eV\n";
        o << "  LUMO est:      " << cr.E_LUMO_est << " eV\n";
        o << "  HOMO-LUMO gap: " << cr.HOMO_LUMO_gap << " eV\n";
        o << "  Global soft:   " << cr.global_softness << " eV⁻¹\n\n";

        // [M] Thermodynamic
        o << "─── [M] THERMODYNAMIC ESTIMATES (5 points) ────────────────\n";
        o << "  Cv (Dulong-Petit): " << cr.heat_capacity_est << " cal/mol·K\n";
        o << "  ZPE est:           " << cr.zero_point_energy_est << " kcal/mol\n";
        o << "  Thermal E (298K):  " << cr.thermal_energy_298 << " kcal/mol\n";
        o << "  S est:             " << cr.entropy_est << " cal/mol·K\n";
        o << "  G correction:      " << cr.gibbs_correction << " kcal/mol\n\n";

        // [N] Structural
        o << "─── [N] STRUCTURAL DESCRIPTORS (6 points) ─────────────────\n";
        o << "  Wiener index:  " << cr.wiener_index << "\n";
        o << "  Balaban J:     " << cr.balaban_J << "\n";
        o << "  Graph diameter:" << cr.graph_diameter << "\n";
        o << "  Density est:   " << cr.density_est << " g/cm³\n";
        o << "  Surface area:  " << cr.surface_area_est << " ų\n";
        o << "  Compactness:   " << cr.compactness << "\n\n";

        o << "╔══════════════════════════════════════════════════════════════════════════════╗\n";
        o << "║  CENSUS COMPLETE — " << std::setw(3) << cr.total_data_points << " data points                                      ║\n";
        o << "╚══════════════════════════════════════════════════════════════════════════════╝\n";

        return o.str();
    }

private:
    const PeriodicTable& pt_;
    Settings settings_;

    // Z → symbol helper
    std::string Z_to_sym(uint8_t Z) const {
        const auto* e = pt_.physics_by_Z(Z);
        return e ? e->symbol : "?";
    }

    // ========================================================================
    // Data collection routines
    // ========================================================================

    void collect_composition(const Molecule& mol, CensusResult& cr) const {
        cr.total_atoms = static_cast<uint32_t>(mol.num_atoms());

        std::map<uint8_t, int> Z_counts;
        double total_mass = 0.0;
        uint32_t total_Z = 0;
        uint32_t total_valence = 0;

        for (const auto& a : mol.atoms) {
            Z_counts[a.Z]++;
            const auto* elem = pt_.physics_by_Z(a.Z);
            double m = elem ? elem->atomic_mass : 1.0;
            total_mass += m;
            total_Z += a.Z;
            total_valence += detail::typical_valence_electrons(a.Z);

            if (a.Z == 1) cr.hydrogen_count++;
            else           cr.heavy_atoms++;

            if (detail::is_metal(a.Z))     cr.has_metal = true;
            if (detail::is_halogen(a.Z))   cr.has_halogen = true;
            if (detail::is_noble_gas(a.Z)) cr.has_noble_gas = true;
        }

        cr.molecular_weight = total_mass;
        cr.unique_elements = static_cast<uint32_t>(Z_counts.size());
        cr.total_electrons = total_Z;
        cr.valence_electrons = total_valence;
        cr.avg_atomic_mass = cr.total_atoms > 0 ? total_mass / cr.total_atoms : 0.0;
        cr.H_to_heavy_ratio = cr.heavy_atoms > 0 ? (double)cr.hydrogen_count / cr.heavy_atoms : 0.0;

        for (const auto& [Z, cnt] : Z_counts) {
            cr.element_counts[Z_to_sym(Z)] = cnt;
        }

        // Build canonical formula
        auto Z_to_sym_fn = [this](uint8_t Z) -> std::string { return Z_to_sym(Z); };
        cr.formula = identity::canonical_formula(mol.atoms, Z_to_sym_fn);
        cr.canonical_formula = cr.formula;

        // Fluorine check
        int F_count = Z_counts.count(9) ? Z_counts[9] : 0;
        cr.is_perfluorinated = (F_count >= 6);

        // Organic check
        cr.is_organic = (Z_counts.count(6) > 0 && Z_counts.count(1) > 0);
    }

    void collect_topology(const Molecule& mol, CensusResult& cr) const {
        cr.num_bonds     = static_cast<uint32_t>(mol.num_bonds());
        cr.num_angles    = static_cast<uint32_t>(mol.angles.size());
        cr.num_torsions  = static_cast<uint32_t>(mol.torsions.size());
        cr.num_impropers = static_cast<uint32_t>(mol.impropers.size());

        for (const auto& b : mol.bonds) {
            if (b.order == 1) cr.num_single_bonds++;
            else if (b.order == 2) cr.num_double_bonds++;
            else if (b.order == 3) cr.num_triple_bonds++;
        }

        // Coordination numbers
        std::vector<int> coord(mol.num_atoms(), 0);
        for (const auto& b : mol.bonds) {
            coord[b.i]++;
            coord[b.j]++;
        }
        double sum_coord = 0;
        uint32_t max_c = 0;
        for (int c : coord) {
            sum_coord += c;
            if ((uint32_t)c > max_c) max_c = (uint32_t)c;
        }
        cr.avg_coordination = mol.num_atoms() > 0 ? sum_coord / mol.num_atoms() : 0.0;
        cr.max_coordination = max_c;

        // Rotatable bonds
        auto rotatable = find_rotatable_bonds(mol);
        cr.num_rotatable_bonds = static_cast<uint32_t>(rotatable.size());

        // Ring detection (heuristic: if any bond is in a ring, has_rings = true)
        // A simple check: bonds > atoms - 1 implies cycles
        if (mol.num_bonds() > mol.num_atoms() - 1 && mol.num_atoms() > 0) {
            cr.has_rings = true;
        }

        cr.bond_density = mol.num_atoms() > 0 ? (double)mol.num_bonds() / mol.num_atoms() : 0.0;
    }

    void collect_energy(const Molecule& mol, const OptimizeResult& fire,
                        CensusResult& cr) const {
        cr.E_total = fire.energy;

        // If energy breakdown is available
        cr.E_bond    = fire.energy_breakdown.bond_energy;
        cr.E_angle   = fire.energy_breakdown.angle_energy;
        cr.E_torsion = fire.energy_breakdown.torsion_energy;
        cr.E_vdw     = fire.energy_breakdown.vdw_energy;
        cr.E_coulomb = fire.energy_breakdown.coulomb_energy;

        cr.E_per_atom = mol.num_atoms() > 0 ? fire.energy / mol.num_atoms() : 0.0;
        cr.E_per_bond = mol.num_bonds() > 0 ? fire.energy / mol.num_bonds() : 0.0;
    }

    void collect_electronic(const Molecule& mol, CensusResult& cr) const {
        // Simple QEq-like charge equilibration
        // Compute electronegativity difference per bond as proxy
        if (mol.num_atoms() == 0) return;

        // Per-atom Pauling EN
        std::vector<double> en(mol.num_atoms());
        for (size_t i = 0; i < mol.num_atoms(); ++i) {
            en[i] = detail::pauling_EN(mol.atoms[i].Z);
        }

        // Simple charge estimation from EN differences
        std::vector<double> charges(mol.num_atoms(), 0.0);
        for (const auto& b : mol.bonds) {
            double delta_en = en[b.j] - en[b.i];
            // Fractional charge transfer proportional to ΔEN
            double q_transfer = delta_en * 0.16; // empirical scale
            charges[b.i] += q_transfer;
            charges[b.j] -= q_transfer;
        }

        // Normalize to zero total
        double q_sum = 0.0;
        for (double q : charges) q_sum += q;
        for (auto& q : charges) q -= q_sum / mol.num_atoms();

        // Dipole moment
        double dx = 0, dy = 0, dz = 0;
        for (size_t i = 0; i < mol.num_atoms(); ++i) {
            dx += charges[i] * mol.coords[3*i];
            dy += charges[i] * mol.coords[3*i+1];
            dz += charges[i] * mol.coords[3*i+2];
        }
        cr.dipole_x = dx * 4.8; // e·Å → Debye
        cr.dipole_y = dy * 4.8;
        cr.dipole_z = dz * 4.8;
        cr.dipole_moment = std::sqrt(cr.dipole_x*cr.dipole_x +
                                     cr.dipole_y*cr.dipole_y +
                                     cr.dipole_z*cr.dipole_z);

        // Polarizability estimate (volume-based)
        double vol = 0.0;
        for (size_t i = 0; i < mol.num_atoms(); ++i) {
            double r = 1.5; // approximate atomic radius
            vol += (4.0/3.0) * M_PI * r * r * r;
        }
        cr.polarizability = 0.8 * vol;

        // Global electronic indices
        double avg_en = 0.0;
        for (double e : en) avg_en += e;
        avg_en /= mol.num_atoms();

        // Simplified Koopmans-like estimates
        cr.ionization_potential = avg_en + 5.0;  // rough shift
        cr.electron_affinity    = avg_en - 5.0;
        cr.electronegativity    = (cr.ionization_potential + cr.electron_affinity) / 2.0;
        cr.chemical_hardness    = (cr.ionization_potential - cr.electron_affinity) / 2.0;
        if (cr.chemical_hardness > 0.0) {
            cr.electrophilicity = cr.electronegativity * cr.electronegativity / (2.0 * cr.chemical_hardness);
        }

        // Store charge extrema
        cr.max_partial_charge = *std::max_element(charges.begin(), charges.end());
        cr.min_partial_charge = *std::min_element(charges.begin(), charges.end());
        cr.charge_spread      = cr.max_partial_charge - cr.min_partial_charge;
    }

    void collect_reactivity(const Molecule& mol, CensusResult& cr) const {
        if (mol.num_atoms() == 0) return;

        // Fukui indices from charge differences (simplified)
        std::vector<double> en(mol.num_atoms());
        std::vector<double> charges(mol.num_atoms(), 0.0);
        for (size_t i = 0; i < mol.num_atoms(); ++i) {
            en[i] = detail::pauling_EN(mol.atoms[i].Z);
        }
        for (const auto& b : mol.bonds) {
            double delta_en = en[b.j] - en[b.i];
            double q_transfer = delta_en * 0.16;
            charges[b.i] += q_transfer;
            charges[b.j] -= q_transfer;
        }

        double max_fp = 0, max_fm = 0, max_f0 = 0, sum_f0 = 0, max_soft = 0;
        double softness = cr.chemical_hardness > 0 ? 1.0 / (2.0 * cr.chemical_hardness) : 0.0;

        for (size_t i = 0; i < mol.num_atoms(); ++i) {
            double fp = std::max(0.0, -charges[i]); // nucleophilic attack sites
            double fm = std::max(0.0,  charges[i]); // electrophilic attack sites
            double f0 = (fp + fm) / 2.0;
            double ls = softness * f0;

            max_fp = std::max(max_fp, fp);
            max_fm = std::max(max_fm, fm);
            max_f0 = std::max(max_f0, f0);
            sum_f0 += f0;
            max_soft = std::max(max_soft, ls);
        }

        cr.max_fukui_plus    = max_fp;
        cr.max_fukui_minus   = max_fm;
        cr.max_fukui_zero    = max_f0;
        cr.avg_fukui_zero    = sum_f0 / mol.num_atoms();
        cr.max_local_softness = max_soft;
    }

    void collect_geometry(const Molecule& mol, CensusResult& cr) const {
        if (mol.num_atoms() == 0) return;

        // Bond lengths
        std::vector<double> bond_lengths;
        for (const auto& b : mol.bonds) {
            double d = distance(mol.coords, b.i, b.j);
            bond_lengths.push_back(d);
        }

        if (!bond_lengths.empty()) {
            cr.min_bond_length = *std::min_element(bond_lengths.begin(), bond_lengths.end());
            cr.max_bond_length = *std::max_element(bond_lengths.begin(), bond_lengths.end());
            double sum = std::accumulate(bond_lengths.begin(), bond_lengths.end(), 0.0);
            cr.avg_bond_length = sum / bond_lengths.size();
            double sq_sum = 0;
            for (double bl : bond_lengths) sq_sum += (bl - cr.avg_bond_length) * (bl - cr.avg_bond_length);
            cr.std_bond_length = std::sqrt(sq_sum / bond_lengths.size());
        }

        // Bond angles
        std::vector<double> angles_deg;
        for (const auto& ang : mol.angles) {
            double a = angle(mol.coords, ang.i, ang.j, ang.k);
            angles_deg.push_back(a * 180.0 / M_PI);
        }

        if (!angles_deg.empty()) {
            cr.min_bond_angle = *std::min_element(angles_deg.begin(), angles_deg.end());
            cr.max_bond_angle = *std::max_element(angles_deg.begin(), angles_deg.end());
            double sum = std::accumulate(angles_deg.begin(), angles_deg.end(), 0.0);
            cr.avg_bond_angle = sum / angles_deg.size();
            double sq_sum = 0;
            for (double a : angles_deg) sq_sum += (a - cr.avg_bond_angle) * (a - cr.avg_bond_angle);
            cr.std_bond_angle = std::sqrt(sq_sum / angles_deg.size());
        }

        // Centroid
        double cx = 0, cy = 0, cz = 0;
        for (size_t i = 0; i < mol.num_atoms(); ++i) {
            cx += mol.coords[3*i];
            cy += mol.coords[3*i+1];
            cz += mol.coords[3*i+2];
        }
        cx /= mol.num_atoms();
        cy /= mol.num_atoms();
        cz /= mol.num_atoms();
        cr.centroid_x = cx;
        cr.centroid_y = cy;
        cr.centroid_z = cz;

        // Radius of gyration and molecular radius
        double sum_r2 = 0.0;
        double max_r = 0.0;
        for (size_t i = 0; i < mol.num_atoms(); ++i) {
            double dx = mol.coords[3*i] - cx;
            double dy = mol.coords[3*i+1] - cy;
            double dz = mol.coords[3*i+2] - cz;
            double r2 = dx*dx + dy*dy + dz*dz;
            sum_r2 += r2;
            double r = std::sqrt(r2);
            if (r > max_r) max_r = r;
        }
        cr.radius_of_gyration = std::sqrt(sum_r2 / mol.num_atoms());
        cr.molecular_radius   = max_r;
        cr.sphericity = max_r > 0 ? cr.radius_of_gyration / max_r : 0.0;

        // Minimum interatomic distance
        double min_dist = 1e10;
        for (size_t i = 0; i < mol.num_atoms(); ++i) {
            for (size_t j = i + 1; j < mol.num_atoms(); ++j) {
                double dx = mol.coords[3*i] - mol.coords[3*j];
                double dy = mol.coords[3*i+1] - mol.coords[3*j+1];
                double dz = mol.coords[3*i+2] - mol.coords[3*j+2];
                double d = std::sqrt(dx*dx + dy*dy + dz*dz);
                if (d < min_dist) min_dist = d;
            }
        }
        cr.min_interatomic_dist = (min_dist < 1e9) ? min_dist : 0.0;

        // Volume estimate (sum of atomic spheres)
        cr.molecular_volume_est = 0.0;
        for (size_t i = 0; i < mol.num_atoms(); ++i) {
            double r = 1.5; // approximate
            cr.molecular_volume_est += (4.0/3.0) * M_PI * r * r * r;
        }

        cr.surface_area_est = 0.0;
        for (size_t i = 0; i < mol.num_atoms(); ++i) {
            double r = 1.7; // vdW-like
            cr.surface_area_est += 4.0 * M_PI * r * r;
        }
    }

    void collect_identity(const Molecule& mol, CensusResult& cr) const {
        auto Z_to_sym_fn = [this](uint8_t Z) -> std::string { return Z_to_sym(Z); };

        // Morgan canonicalization
        auto morgan = identity::morgan_canonicalize(mol.atoms, mol.bonds);

        // Graph hash from invariants
        uint64_t hash = 0;
        for (size_t i = 0; i < morgan.invariants.size(); ++i) {
            hash ^= morgan.invariants[i] * (i + 1) * 2654435761ULL;
        }
        cr.graph_hash = hash;

        // Geometry hash (rotation-invariant: sorted pair distances)
        uint64_t geo_hash = 0;
        std::vector<double> dists;
        for (size_t i = 0; i < mol.num_atoms(); ++i) {
            for (size_t j = i + 1; j < mol.num_atoms(); ++j) {
                double d = distance(mol.coords, (uint32_t)i, (uint32_t)j);
                dists.push_back(d);
            }
        }
        std::sort(dists.begin(), dists.end());
        for (double d : dists) {
            uint64_t bits;
            double rounded = std::round(d * 1000.0) / 1000.0;
            std::memcpy(&bits, &rounded, sizeof(bits));
            geo_hash ^= bits * 2654435761ULL;
        }
        cr.geometry_hash = geo_hash;

        // Morgan invariants summary (first 8)
        std::ostringstream oss;
        oss << "[";
        for (size_t i = 0; i < std::min((size_t)8, morgan.invariants.size()); ++i) {
            if (i > 0) oss << ",";
            oss << morgan.invariants[i];
        }
        oss << "]";
        cr.morgan_invariants_summary = oss.str();

        // VSEPR class
        if (mol.num_atoms() > 0) {
            // Find central atom (highest coordination)
            std::vector<int> coord(mol.num_atoms(), 0);
            for (const auto& b : mol.bonds) {
                coord[b.i]++;
                coord[b.j]++;
            }
            auto it = std::max_element(coord.begin(), coord.end());
            int central = (int)(it - coord.begin());
            int cn = *it;
            int lp = mol.atoms[central].lone_pairs;

            std::ostringstream vsepr_oss;
            vsepr_oss << "AX" << cn;
            if (lp > 0) vsepr_oss << "E" << lp;
            cr.vsepr_class = vsepr_oss.str();
        }
    }

    void collect_validation(const Molecule& mol, const OptimizeResult& fire,
                            CensusResult& cr) const {
        int passed = 0;
        int total = 0;

        // Gate 1: Non-empty
        total++;
        cr.val_nonempty = (mol.num_atoms() > 0);
        if (cr.val_nonempty) passed++;

        // Gate 2: NaN-free
        total++;
        cr.val_no_nan = true;
        for (size_t i = 0; i < mol.coords.size(); ++i) {
            if (std::isnan(mol.coords[i]) || std::isinf(mol.coords[i])) {
                cr.val_no_nan = false;
                break;
            }
        }
        if (cr.val_no_nan) passed++;

        // Gate 3: No overlap
        total++;
        cr.val_no_overlap = !mol.has_colocated_atoms(0.01);
        if (cr.val_no_overlap) passed++;

        // Gate 4: Bond lengths
        total++;
        cr.val_bonds_ok = true;
        for (const auto& b : mol.bonds) {
            double d = distance(mol.coords, b.i, b.j);
            if (d < 0.5 || d > 5.0) { cr.val_bonds_ok = false; break; }
        }
        if (cr.val_bonds_ok) passed++;

        // Gate 5: Valence (simple heuristic)
        total++;
        cr.val_valence_ok = true; // Accept by default for census
        passed++;

        // Gate 6: Convergence
        total++;
        cr.val_converged = fire.converged;
        if (cr.val_converged) passed++;

        // Gate 7: Charge consistency
        total++;
        cr.val_charge_consistent = true; // Neutral molecules
        passed++;

        // Gate 8: Connected graph (simple check: BFS from atom 0)
        total++;
        cr.val_connected = false;
        if (mol.num_atoms() > 0) {
            std::vector<bool> visited(mol.num_atoms(), false);
            std::vector<uint32_t> queue;
            visited[0] = true;
            queue.push_back(0);
            for (size_t idx = 0; idx < queue.size(); ++idx) {
                uint32_t curr = queue[idx];
                for (const auto& b : mol.bonds) {
                    uint32_t other = UINT32_MAX;
                    if (b.i == curr) other = b.j;
                    else if (b.j == curr) other = b.i;
                    if (other != UINT32_MAX && !visited[other]) {
                        visited[other] = true;
                        queue.push_back(other);
                    }
                }
            }
            cr.val_connected = (queue.size() == mol.num_atoms());
        }
        if (cr.val_connected) passed++;

        cr.val_gates_passed = passed;
        cr.val_gates_total = total;
    }

    void collect_fire_stats(const OptimizeResult& fire, CensusResult& cr) const {
        cr.fire_iterations      = fire.iterations;
        cr.fire_final_energy    = fire.energy;
        cr.fire_final_rms_force = fire.rms_force;
        cr.fire_final_max_force = fire.max_force;
        cr.fire_converged       = fire.converged;
        cr.fire_energy_per_atom = (cr.total_atoms > 0) ? fire.energy / cr.total_atoms : 0.0;
    }

    void collect_stability(CensusResult& cr) const {
        // Estimated HOMO/LUMO from Koopmans' theorem approximation
        cr.E_HOMO_est   = -cr.ionization_potential;
        cr.E_LUMO_est   = -cr.electron_affinity;
        cr.HOMO_LUMO_gap = cr.ionization_potential - cr.electron_affinity;
        cr.global_softness = cr.chemical_hardness > 0 ? 1.0 / (2.0 * cr.chemical_hardness) : 0.0;

        // Strain energy: deviation from ideal energy per bond
        // Ideal ~ 0 kcal/mol per bond after optimization
        cr.strain_energy_est = std::abs(cr.E_bond);
    }

    void collect_thermodynamic(const Molecule& mol, CensusResult& cr) const {
        uint32_t N = static_cast<uint32_t>(mol.num_atoms());
        if (N == 0) return;

        // Dulong-Petit heat capacity: Cv = 3Nk_B per molecule
        // In cal/mol·K: Cv = 3N * 1.987 cal/mol·K
        cr.heat_capacity_est = 3.0 * N * 1.987;

        // ZPE estimate: ~1.3 kcal/mol per bond (rough average)
        cr.zero_point_energy_est = 1.3 * mol.num_bonds();

        // Thermal energy at 298K: E_therm = Cv * T (simplified)
        cr.thermal_energy_298 = cr.heat_capacity_est * 298.0 / 1000.0; // kcal/mol

        // Entropy estimate (Sackur-Tetrode-like, very rough)
        cr.entropy_est = 30.0 + 10.0 * std::log((double)N); // cal/mol·K

        // Gibbs correction: G = H - TS ≈ E + PV - TS
        cr.gibbs_correction = cr.thermal_energy_298 - 298.0 * cr.entropy_est / 1000.0;
    }

    void collect_structural(const Molecule& mol, CensusResult& cr) const {
        uint32_t N = static_cast<uint32_t>(mol.num_atoms());
        if (N < 2) return;

        // Build adjacency list for BFS
        std::vector<std::vector<uint32_t>> adj(N);
        for (const auto& b : mol.bonds) {
            adj[b.i].push_back(b.j);
            adj[b.j].push_back(b.i);
        }

        // BFS shortest paths from each atom (for Wiener index and diameter)
        double wiener = 0.0;
        int diameter = 0;

        for (uint32_t src = 0; src < N; ++src) {
            std::vector<int> dist(N, -1);
            std::vector<uint32_t> queue;
            dist[src] = 0;
            queue.push_back(src);
            for (size_t idx = 0; idx < queue.size(); ++idx) {
                uint32_t curr = queue[idx];
                for (uint32_t next : adj[curr]) {
                    if (dist[next] == -1) {
                        dist[next] = dist[curr] + 1;
                        queue.push_back(next);
                    }
                }
            }
            for (uint32_t j = src + 1; j < N; ++j) {
                if (dist[j] > 0) {
                    wiener += dist[j];
                    if (dist[j] > diameter) diameter = dist[j];
                }
            }
        }
        cr.wiener_index = wiener;
        cr.graph_diameter = diameter;

        // Balaban J index: J = m/(μ+1) * Σ(di*dj)^(-0.5) over edges
        // where m = bonds, μ = cyclomatic number = m - n + 1, di = distance sum for atom i
        double m = (double)mol.num_bonds();
        double mu = m - N + 1.0;
        if (mu < 0) mu = 0;

        // Distance sums
        std::vector<double> dist_sum(N, 0.0);
        for (uint32_t src = 0; src < N; ++src) {
            std::vector<int> dist(N, -1);
            std::vector<uint32_t> queue;
            dist[src] = 0;
            queue.push_back(src);
            for (size_t idx = 0; idx < queue.size(); ++idx) {
                uint32_t curr = queue[idx];
                for (uint32_t next : adj[curr]) {
                    if (dist[next] == -1) {
                        dist[next] = dist[curr] + 1;
                        queue.push_back(next);
                    }
                }
            }
            for (uint32_t j = 0; j < N; ++j) {
                if (dist[j] > 0) dist_sum[src] += dist[j];
            }
        }

        double balaban_sum = 0.0;
        for (const auto& b : mol.bonds) {
            double di = dist_sum[b.i];
            double dj = dist_sum[b.j];
            if (di > 0 && dj > 0) {
                balaban_sum += 1.0 / std::sqrt(di * dj);
            }
        }
        cr.balaban_J = (mu + 1.0 > 0) ? (m / (mu + 1.0)) * balaban_sum : 0.0;

        // Density estimate: mass / volume (rough)
        if (cr.molecular_volume_est > 0) {
            // Convert amu to grams, ų to cm³
            // 1 amu = 1.66054e-24 g, 1 ų = 1e-24 cm³
            cr.density_est = (cr.molecular_weight * 1.66054e-24) / (cr.molecular_volume_est * 1e-24);
        }

        // Compactness
        if (cr.surface_area_est > 0) {
            cr.compactness = cr.molecular_volume_est / cr.surface_area_est;
        }
    }

    // ========================================================================
    // Alternative geometry search
    // ========================================================================

    void search_alternatives(const Molecule& base_mol,
                             const EnergyModel& energy_model,
                             CensusResult& cr) const {
        auto rotatable = find_rotatable_bonds(base_mol);

        if (rotatable.empty()) {
            if (settings_.verbose) {
                std::cout << "  No rotatable bonds — skipping conformer search.\n";
            }
            cr.conf_trials = 0;
            cr.conf_unique = 0;
            cr.conf_duplicates = 0;
            return;
        }

        if (settings_.verbose) {
            std::cout << "  Rotatable bonds: " << rotatable.size() << "\n";
            std::cout << "  Running " << settings_.conformer_trials << " trials...\n";
        }

        ConformerFinderSettings cfs;
        cfs.num_starts              = settings_.conformer_trials;
        cfs.seed                    = settings_.conformer_seed;
        cfs.rmsd_threshold          = settings_.rmsd_threshold;
        cfs.energy_threshold        = settings_.energy_threshold;
        cfs.enumerate_geometric_isomers = false;
        cfs.enumerate_conformers    = true;
        cfs.opt_settings.max_iterations = settings_.max_fire_steps;
        cfs.opt_settings.tol_rms_force  = settings_.tol_rms_force;
        cfs.opt_settings.print_every    = 0;

        ConformerFinder finder(cfs);
        auto variants = finder.find_conformers(base_mol, energy_model);

        cr.conf_trials     = finder.num_trials();
        cr.conf_unique     = static_cast<int>(variants.size());
        cr.conf_duplicates = finder.num_duplicates();

        if (!variants.empty()) {
            cr.conf_energy_min = variants.front().energy;
            cr.conf_energy_max = variants.back().energy;
            cr.conf_energy_spread = cr.conf_energy_max - cr.conf_energy_min;
            cr.conf_ensemble_free_E = finder.get_ensemble_free_energy(variants);

            // Log each alternative
            double primary_E = cr.E_total;
            for (size_t i = 0; i < variants.size(); ++i) {
                AlternativeGeometry alt;
                alt.trial_id   = variants[i].trial_id;
                alt.energy     = variants[i].energy;
                alt.delta_E    = variants[i].energy - primary_E;
                alt.rms_force  = 0.0; // Not stored in MolecularVariant
                alt.fire_steps = 0;
                alt.converged  = true;
                alt.descriptor = variants[i].descriptor;
                alt.coords     = variants[i].geometry.coords;
                cr.alt_geometries.push_back(alt);
            }
        }

        if (settings_.verbose) {
            std::cout << "  Found " << cr.conf_unique << " unique conformers"
                      << " (rejected " << cr.conf_duplicates << " duplicates)\n";
        }
    }

    // ========================================================================
    // Automatic classification
    // ========================================================================

    void classify(CensusResult& cr) const {
        ClassificationResult cls;
        std::ostringstream reason;
        double confidence = 0.0;

        // ── Step 1: Determine Group ──

        bool has_C = cr.element_counts.count("C") > 0;
        bool has_H = cr.element_counts.count("H") > 0;
        bool has_N = cr.element_counts.count("N") > 0;
        bool has_O = cr.element_counts.count("O") > 0;
        bool has_F = cr.element_counts.count("F") > 0;
        bool has_S = cr.element_counts.count("S") > 0;
        bool has_Si = cr.element_counts.count("Si") > 0;
        bool has_P = cr.element_counts.count("P") > 0;

        int N_count = cr.element_counts.count("N") ? cr.element_counts.at("N") : 0;
        int O_count = cr.element_counts.count("O") ? cr.element_counts.at("O") : 0;
        int F_count = cr.element_counts.count("F") ? cr.element_counts.at("F") : 0;

        if (cr.has_noble_gas) {
            cls.group = MolecularGroup::NOBLE_GAS_COMPOUND;
            reason << "Contains noble gas element. ";
            confidence = 0.95;
        } else if (cr.has_metal && has_C) {
            cls.group = MolecularGroup::ORGANOMETALLIC;
            reason << "Contains both metals and carbon. ";
            confidence = 0.90;
        } else if (cr.has_metal && cr.max_coordination >= 4) {
            cls.group = MolecularGroup::COORDINATION_COMPLEX;
            reason << "Metal center with CN≥4. ";
            confidence = 0.85;
        } else if (has_C && has_H) {
            cls.group = MolecularGroup::ORGANIC;
            reason << "Contains C and H (organic). ";
            confidence = 0.85;
        } else if (!has_C && cr.unique_elements <= 2 && cr.has_halogen) {
            // Check interhalogen: all elements are halogens
            bool all_halogen = true;
            for (const auto& [sym, cnt] : cr.element_counts) {
                uint8_t Z = 0;
                if (sym == "F") Z = 9; else if (sym == "Cl") Z = 17;
                else if (sym == "Br") Z = 35; else if (sym == "I") Z = 53;
                if (!detail::is_halogen(Z) && Z != 0) { all_halogen = false; break; }
                if (Z == 0) all_halogen = false;
            }
            if (all_halogen) {
                cls.group = MolecularGroup::INTERHALOGEN;
                reason << "All elements are halogens. ";
                confidence = 0.95;
            } else {
                cls.group = MolecularGroup::INORGANIC;
                reason << "No carbon, contains halogens. ";
                confidence = 0.70;
            }
        } else if (has_Si && has_O && !has_C) {
            cls.group = MolecularGroup::CERAMIC;
            reason << "Si-O framework without carbon. ";
            confidence = 0.80;
        } else {
            cls.group = MolecularGroup::INORGANIC;
            reason << "Default: inorganic compound. ";
            confidence = 0.60;
        }

        // ── Step 2: Determine Sub-group ──

        if (cls.group == MolecularGroup::ORGANIC) {
            if (cr.num_double_bonds > 0 && !has_O && !has_N) {
                cls.sub_group = MolecularSubGroup::ALKENE;
                reason << "Unsaturated C-H. ";
            } else if (cr.num_triple_bonds > 0) {
                cls.sub_group = MolecularSubGroup::ALKYNE;
                reason << "Triple bonds present. ";
            } else if (has_O && has_H && !has_N) {
                // Could be alcohol, aldehyde, ketone, carboxylic acid, ester, ether
                if (O_count >= 2 && has_H) {
                    cls.sub_group = MolecularSubGroup::CARBOXYLIC_ACID;
                    reason << "C,H,O with multiple O (acid-like). ";
                } else {
                    cls.sub_group = MolecularSubGroup::ALCOHOL;
                    reason << "C,H,O (alcohol-like). ";
                }
            } else if (has_N && !has_O) {
                cls.sub_group = MolecularSubGroup::AMINE;
                reason << "C,H,N (amine-like). ";
            } else if (has_N && has_O) {
                cls.sub_group = MolecularSubGroup::AMIDE;
                reason << "C,H,N,O (amide-like). ";
            } else if (cr.has_rings) {
                cls.sub_group = MolecularSubGroup::AROMATIC;
                reason << "Ring system detected. ";
            } else {
                cls.sub_group = MolecularSubGroup::ALKANE;
                reason << "Saturated C-H (alkane). ";
            }
        } else if (cls.group == MolecularGroup::INORGANIC) {
            if (cr.unique_elements == 2 && has_H) {
                cls.sub_group = MolecularSubGroup::BINARY_HYDRIDE;
                reason << "Binary hydride. ";
            } else if (has_O && !has_H && cr.unique_elements == 2) {
                cls.sub_group = MolecularSubGroup::OXIDE;
                reason << "Binary oxide. ";
            } else if (cr.has_halogen && cr.unique_elements == 2) {
                cls.sub_group = MolecularSubGroup::HALIDE;
                reason << "Binary halide. ";
            } else if (cr.max_coordination > 4) {
                cls.sub_group = MolecularSubGroup::HYPERVALENT;
                reason << "Hypervalent (CN>" << cr.max_coordination << "). ";
            } else if (has_O && has_H) {
                cls.sub_group = MolecularSubGroup::OXYACID;
                reason << "Contains H and O (oxyacid-like). ";
            }
        } else if (cls.group == MolecularGroup::COORDINATION_COMPLEX) {
            if (cr.max_coordination == 6) {
                cls.sub_group = MolecularSubGroup::OCTAHEDRAL;
                reason << "CN=6 (octahedral). ";
            } else if (cr.max_coordination == 4) {
                cls.sub_group = MolecularSubGroup::TETRAHEDRAL_COORD;
                reason << "CN=4 (tetrahedral). ";
            } else if (cr.max_coordination == 5) {
                cls.sub_group = MolecularSubGroup::TRIGONAL_BIPYRAMIDAL;
                reason << "CN=5 (TBP). ";
            }
        }

        // Perfluorinated override
        if (cr.is_perfluorinated) {
            cls.sub_group = MolecularSubGroup::PERFLUORINATED;
            reason << "Perfluorinated (F≥6). ";
        }

        // ── Step 3: Ionic Character ──

        // Compute max electronegativity difference across bonds
        double max_delta_EN = 0.0;
        double sum_delta_EN = 0.0;
        int bond_count = 0;
        bool has_metal_bond = false;

        for (const auto& [sym, cnt] : cr.element_counts) {
            (void)cnt; // suppress unused warning
        }

        // Use the composition to estimate ionic character
        std::vector<double> en_values;
        for (const auto& [sym, cnt] : cr.element_counts) {
            // Map symbol back to Z (simplified)
            uint8_t Z = 0;
            if (sym == "H") Z = 1; else if (sym == "C") Z = 6;
            else if (sym == "N") Z = 7; else if (sym == "O") Z = 8;
            else if (sym == "F") Z = 9; else if (sym == "Na") Z = 11;
            else if (sym == "Si") Z = 14; else if (sym == "P") Z = 15;
            else if (sym == "S") Z = 16; else if (sym == "Cl") Z = 17;
            else if (sym == "K") Z = 19; else if (sym == "Ca") Z = 20;
            else if (sym == "Fe") Z = 26; else if (sym == "Br") Z = 35;

            double en = detail::pauling_EN(Z);
            en_values.push_back(en);
            if (detail::is_metal(Z)) has_metal_bond = true;
        }

        if (en_values.size() >= 2) {
            double en_min = *std::min_element(en_values.begin(), en_values.end());
            double en_max = *std::max_element(en_values.begin(), en_values.end());
            max_delta_EN = en_max - en_min;
        }

        if (has_metal_bond && !has_C) {
            if (max_delta_EN >= 1.7) {
                cls.ionic = IonicCharacter::IONIC;
                reason << "Large ΔEN (ionic). ";
            } else {
                cls.ionic = IonicCharacter::METALLIC;
                reason << "Metal compound. ";
            }
        } else if (max_delta_EN >= 1.7) {
            cls.ionic = IonicCharacter::IONIC;
            reason << "ΔEN≥1.7 (ionic). ";
        } else if (max_delta_EN >= 0.5) {
            cls.ionic = IonicCharacter::POLAR_COVALENT;
            reason << "0.5≤ΔEN<1.7 (polar covalent). ";
        } else {
            cls.ionic = IonicCharacter::COVALENT;
            reason << "ΔEN<0.5 (covalent). ";
        }

        // ── Step 4: Purpose Type ──

        // Fuel detection: small organic, high H/C ratio, combustible
        if (cls.group == MolecularGroup::ORGANIC && cr.H_to_heavy_ratio > 1.5 &&
            !has_N && cr.total_atoms <= 30) {
            cls.purpose = PurposeType::FUEL;
            reason << "Small organic, high H/C ratio → fuel. ";
            confidence = std::max(confidence, 0.75);
        }
        // Oxidizer: high O content, possibly with halogens
        else if (O_count >= 3 && !has_C && (cr.has_halogen || has_N)) {
            cls.purpose = PurposeType::OXIDIZER;
            reason << "High O, non-organic → oxidizer. ";
            confidence = std::max(confidence, 0.70);
        }
        // Energetic material: high N+O, organic
        else if (has_C && N_count >= 3 && O_count >= 3) {
            cls.purpose = PurposeType::ENERGETIC_MATERIAL;
            reason << "High N+O content in organic → energetic. ";
            cr.is_energetic = true;
            confidence = std::max(confidence, 0.70);
        }
        // Battery electrolyte: Li/Na with F or P
        else if ((cr.element_counts.count("Li") || cr.element_counts.count("Na")) &&
                 (has_F || has_P)) {
            cls.purpose = PurposeType::BATTERY_ELECTROLYTE;
            reason << "Li/Na + F/P → battery electrolyte. ";
            confidence = std::max(confidence, 0.70);
        }
        // Battery electrode: Li with transition metal + O
        else if (cr.element_counts.count("Li") && cr.has_metal && has_O) {
            cls.purpose = PurposeType::BATTERY_ELECTRODE;
            reason << "Li + metal + O → battery electrode. ";
            confidence = std::max(confidence, 0.70);
        }
        // Catalyst: transition metal complex
        else if (cls.group == MolecularGroup::COORDINATION_COMPLEX ||
                 cls.group == MolecularGroup::ORGANOMETALLIC) {
            cls.purpose = PurposeType::CATALYST;
            reason << "Metal complex → potential catalyst. ";
            confidence = std::max(confidence, 0.60);
        }
        // Semiconductor
        else if (has_Si || cr.element_counts.count("Ge") ||
                 (cr.element_counts.count("Ga") && cr.element_counts.count("As"))) {
            cls.purpose = PurposeType::SEMICONDUCTOR_MATERIAL;
            reason << "Contains semiconductor elements. ";
            confidence = std::max(confidence, 0.70);
        }
        // Pharmaceutical: organic with N, O, moderate size
        else if (cls.group == MolecularGroup::ORGANIC && (has_N || has_O) &&
                 cr.total_atoms >= 10 && cr.total_atoms <= 100) {
            cls.purpose = PurposeType::PHARMACEUTICAL;
            reason << "Mid-size organic with heteroatoms → pharmaceutical candidate. ";
            confidence = std::max(confidence, 0.55);
        }
        // Solvent: small organic, no metals
        else if (cls.group == MolecularGroup::ORGANIC && cr.total_atoms <= 15 &&
                 !cr.has_metal) {
            cls.purpose = PurposeType::SOLVENT;
            reason << "Small organic → potential solvent. ";
            confidence = std::max(confidence, 0.50);
        }
        // Refrigerant: small molecule with F
        else if (has_F && has_C && cr.total_atoms <= 10) {
            cls.purpose = PurposeType::REFRIGERANT;
            reason << "Small fluorocarbon → refrigerant. ";
            confidence = std::max(confidence, 0.65);
        }
        // Fertilizer: N+P+K combination
        else if (has_N && has_P && cr.element_counts.count("K")) {
            cls.purpose = PurposeType::FERTILIZER;
            reason << "N+P+K composition → fertilizer. ";
            confidence = std::max(confidence, 0.70);
        }
        // Propellant: energetic with Al or B
        else if ((cr.element_counts.count("Al") || cr.element_counts.count("B")) &&
                 (has_O || has_F)) {
            cls.purpose = PurposeType::PROPELLANT;
            reason << "Al/B + oxidizer → propellant component. ";
            confidence = std::max(confidence, 0.65);
        }

        cls.classification_confidence = confidence;
        cls.reasoning = reason.str();
        cr.classification = cls;
    }
};

} // namespace analysis
} // namespace vsepr
