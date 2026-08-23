/**
 * comprehensive_elements.hpp
 * ==========================
 * Complete periodic table coverage (Z=1 to Z=118)
 * Includes: Alkalis, Alkaline earths, Transition metals, Lanthanides, Actinides, Noble gases
 * Supports: Single, double, triple bonds
 * Triple-recursive bonding up to 101 atoms per composite
 */

#pragma once

#include "core/element_data.hpp"
#include <array>
#include <string_view>

namespace vsepr {
namespace elements {

// ============================================================================
// Element Categories
// ============================================================================

enum class ElementCategory : uint8_t {
    ALKALI_METAL,           // Li, Na, K, Rb, Cs, Fr
    ALKALINE_EARTH,         // Be, Mg, Ca, Sr, Ba, Ra
    TRANSITION_METAL,       // Sc-Zn, Y-Cd, La-Hg, Ac-Cn
    LANTHANIDE,             // La-Lu (f-block)
    ACTINIDE,               // Ac-Lr (f-block)
    POST_TRANSITION,        // Al, Ga, In, Sn, Tl, Pb, Bi
    METALLOID,              // B, Si, Ge, As, Sb, Te, Po
    NONMETAL,               // C, N, O, P, S, Se
    HALOGEN,                // F, Cl, Br, I, At, Ts
    NOBLE_GAS,              // He, Ne, Ar, Kr, Xe, Rn, Og
    UNKNOWN
};

struct ComprehensiveElementData {
    uint8_t Z;
    std::string_view symbol;
    std::string_view name;
    ElementCategory category;
    
    // Oxidation states (common)
    std::vector<int> oxidation_states;
    
    // Bond order support
    bool supports_single;
    bool supports_double;
    bool supports_triple;
    
    // Coordination numbers
    std::vector<int> coordination_numbers;
    
    // Covalent radii (Å)
    double r_single;
    double r_double;
    double r_triple;
    
    // Van der Waals radius (Å)
    double r_vdw;
    
    // Electronegativity (Pauling)
    double electronegativity;
    
    // Typical bond energies (kcal/mol) with H, C, O
    double bond_energy_H;
    double bond_energy_C;
    double bond_energy_O;
};

// ============================================================================
// Complete Periodic Table Database (Z=1 to Z=118)
// ============================================================================

class ComprehensiveElementDatabase {
    std::array<ComprehensiveElementData, 119> data_;
    
    void init_hydrogen_helium();
    void init_main_group();
    void init_transition_metals();
    void init_lanthanides();
    void init_actinides();
    void init_halogens();
    void init_noble_gases();
    void init_post_transition();
    void init_metalloids();
    
public:
    ComprehensiveElementDatabase();
    
    const ComprehensiveElementData& get(uint8_t Z) const {
        return (Z && Z <= 118) ? data_[Z] : data_[0];
    }
    
    uint8_t Z_from_symbol(std::string_view sym) const;
    ElementCategory category(uint8_t Z) const { return get(Z).category; }
    
    bool is_alkali(uint8_t Z) const { return category(Z) == ElementCategory::ALKALI_METAL; }
    bool is_alkaline_earth(uint8_t Z) const { return category(Z) == ElementCategory::ALKALINE_EARTH; }
    bool is_transition_metal(uint8_t Z) const { return category(Z) == ElementCategory::TRANSITION_METAL; }
    bool is_lanthanide(uint8_t Z) const { return category(Z) == ElementCategory::LANTHANIDE; }
    bool is_actinide(uint8_t Z) const { return category(Z) == ElementCategory::ACTINIDE; }
    bool is_halogen(uint8_t Z) const { return category(Z) == ElementCategory::HALOGEN; }
    bool is_noble_gas(uint8_t Z) const { return category(Z) == ElementCategory::NOBLE_GAS; }
    bool is_metal(uint8_t Z) const {
        auto cat = category(Z);
        return cat == ElementCategory::ALKALI_METAL || 
               cat == ElementCategory::ALKALINE_EARTH ||
               cat == ElementCategory::TRANSITION_METAL ||
               cat == ElementCategory::POST_TRANSITION;
    }
    bool is_nonmetal(uint8_t Z) const {
        auto cat = category(Z);
        return cat == ElementCategory::NONMETAL || cat == ElementCategory::HALOGEN;
    }
    
    // Bond support
    bool supports_triple_bonds(uint8_t Z) const { return get(Z).supports_triple; }
    bool supports_double_bonds(uint8_t Z) const { return get(Z).supports_double; }
    bool supports_single_bonds(uint8_t Z) const { return get(Z).supports_single; }
    
    // Coordination
    int max_coordination(uint8_t Z) const {
        auto& coords = get(Z).coordination_numbers;
        return coords.empty() ? 4 : *std::max_element(coords.begin(), coords.end());
    }
    
    // Radii
    double covalent_radius(uint8_t Z, int bond_order = 1) const {
        auto& elem = get(Z);
        return bond_order == 1 ? elem.r_single :
               bond_order == 2 ? elem.r_double :
               bond_order == 3 ? elem.r_triple : elem.r_single;
    }
    
    double vdw_radius(uint8_t Z) const { return get(Z).r_vdw; }
    
    // Properties
    double electroneg(uint8_t Z) const { return get(Z).electronegativity; }
    const std::vector<int>& oxidation_states(uint8_t Z) const {
        return get(Z).oxidation_states;
    }
};

// ============================================================================
// Large Molecule Support (up to 101 atoms)
// ============================================================================

struct LargeMoleculeConfig {
    int max_atoms = 101;
    int max_bonds_per_atom = 6;     // For coordination complexes
    bool allow_triple_bonds = true;
    bool allow_quadruple_bonds = false;  // For transition metals (Mo-Mo, etc.)
    bool allow_recursive_bonding = true; // Triple-recursive patterns
    
    // Performance limits
    int max_angles = 5000;
    int max_torsions = 10000;
    int max_nonbonded_pairs = 10000;
};

// ============================================================================
// Bond Type Classification
// ============================================================================

enum class BondType {
    SINGLE,
    DOUBLE,
    TRIPLE,
    AROMATIC,
    COORDINATION,
    METALLIC,
    HYDROGEN,
    IONIC
};

struct BondClassifier {
    static BondType classify(uint8_t Z1, uint8_t Z2, int bond_order, 
                            const ComprehensiveElementDatabase& db);
    
    static bool is_polar(uint8_t Z1, uint8_t Z2, const ComprehensiveElementDatabase& db);
    
    static double estimate_bond_length(uint8_t Z1, uint8_t Z2, int bond_order,
                                       const ComprehensiveElementDatabase& db);
};

// ============================================================================
// Global Access
// ============================================================================

// Singleton accessor
const ComprehensiveElementDatabase& comprehensive_elements();

// Initialize database
void init_comprehensive_elements();

} // namespace elements
} // namespace vsepr
