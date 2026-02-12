#pragma once
/**
 * fragment_library.hpp
 * --------------------
 * Molecular Fragment Recognition and Assembly System
 * 
 * Provides infrastructure for:
 * - Fragment template definitions (topology only, no hardcoded coordinates)
 * - Automatic fragment detection from formula composition
 * - Modular assembly of complex molecules
 * 
 * IMPORTANT: No hardcoded geometries. Fragment coordinates are generated
 * at runtime via the formation pipeline (formula -> VSEPR -> FIRE).
 * Only topology (atom types, bonds, attachment sites) is stored here.
 */

#include "core/types.hpp"
#include "pot/periodic_db.hpp"
#include <vector>
#include <string>
#include <map>
#include <memory>
#include <algorithm>

namespace vsepr {
namespace fragment {

// ============================================================================
// Attachment Site Types
// ============================================================================
enum class SiteType {
    DONOR,      // Electron donor (e.g., O in -OH, N in -NH2)
    ACCEPTOR,   // Electron acceptor (e.g., C in carbonyl)
    BIDENTATE,  // Two binding sites (e.g., oxalate)
    BRIDGING,   // Can bridge multiple centers
    TERMINAL    // End of chain, no further attachment
};

// ============================================================================
// Assembly Strategy
// ============================================================================
enum class AssemblyStrategy {
    STAR_VSEPR,     // Place ligands via VSEPR directions around center
    LINEAR_CHAIN,   // Connect fragments in a linear chain
    RING,           // Connect fragments in a ring
    CUSTOM          // User-defined placement
};

// ============================================================================
// Fragment Template Definition
// ============================================================================
struct FragmentTemplate {
    std::string name;                    // "hydroxyl", "methyl", "oxalate", etc.
    std::string formula;                 // Chemical formula (e.g., "OH", "CH3")
    std::vector<uint8_t> atom_types;     // Atomic numbers (topology only)
    std::vector<std::pair<int,int>> bonds; // Bond connectivity (0-indexed)
    
    // Attachment information
    int attachment_point = -1;           // Primary atom that connects to parent
    SiteType site_type = SiteType::DONOR;
    
    // Metadata
    int charge = 0;                      // Fragment charge
    bool is_ligand = false;              // Can bind to metal center
    int denticity = 1;                   // Number of binding sites
    std::vector<int> binding_atoms;      // Which atoms bind to metal
};

// ============================================================================
// Fragment Topology Definitions (no coordinates)
// ============================================================================

inline FragmentTemplate hydroxyl_topology() {
    FragmentTemplate frag;
    frag.name = "hydroxyl";
    frag.formula = "OH";
    frag.atom_types = {8, 1};  // O, H
    frag.bonds = {{0, 1}};
    frag.attachment_point = 0;
    frag.site_type = SiteType::DONOR;
    return frag;
}

inline FragmentTemplate methyl_topology() {
    FragmentTemplate frag;
    frag.name = "methyl";
    frag.formula = "CH3";
    frag.atom_types = {6, 1, 1, 1};  // C, H, H, H
    frag.bonds = {{0, 1}, {0, 2}, {0, 3}};
    frag.attachment_point = 0;
    frag.site_type = SiteType::DONOR;
    return frag;
}

inline FragmentTemplate amino_topology() {
    FragmentTemplate frag;
    frag.name = "amino";
    frag.formula = "NH2";
    frag.atom_types = {7, 1, 1};  // N, H, H
    frag.bonds = {{0, 1}, {0, 2}};
    frag.attachment_point = 0;
    frag.site_type = SiteType::DONOR;
    return frag;
}

inline FragmentTemplate carboxyl_topology() {
    FragmentTemplate frag;
    frag.name = "carboxyl";
    frag.formula = "COOH";
    frag.atom_types = {6, 8, 8, 1};  // C, O(=), O(-), H
    frag.bonds = {{0, 1}, {0, 2}, {2, 3}};
    frag.attachment_point = 0;
    frag.site_type = SiteType::DONOR;
    return frag;
}

inline FragmentTemplate oxalate_topology() {
    FragmentTemplate frag;
    frag.name = "oxalate";
    frag.formula = "C2O4";
    frag.atom_types = {6, 6, 8, 8, 8, 8};  // C, C, O, O, O, O
    frag.bonds = {{0, 1}, {0, 2}, {0, 3}, {1, 4}, {1, 5}};
    frag.attachment_point = 2;
    frag.site_type = SiteType::BIDENTATE;
    frag.is_ligand = true;
    frag.denticity = 2;
    frag.binding_atoms = {2, 4};
    frag.charge = -2;
    return frag;
}

inline FragmentTemplate carbonate_topology() {
    FragmentTemplate frag;
    frag.name = "carbonate";
    frag.formula = "CO3";
    frag.atom_types = {6, 8, 8, 8};  // C, O, O, O
    frag.bonds = {{0, 1}, {0, 2}, {0, 3}};
    frag.attachment_point = 1;
    frag.site_type = SiteType::BIDENTATE;
    frag.is_ligand = true;
    frag.denticity = 2;
    frag.binding_atoms = {1, 2};
    frag.charge = -2;
    return frag;
}

inline FragmentTemplate sulfate_topology() {
    FragmentTemplate frag;
    frag.name = "sulfate";
    frag.formula = "SO4";
    frag.atom_types = {16, 8, 8, 8, 8};  // S, O, O, O, O
    frag.bonds = {{0, 1}, {0, 2}, {0, 3}, {0, 4}};
    frag.attachment_point = 1;
    frag.site_type = SiteType::BIDENTATE;
    frag.is_ligand = true;
    frag.denticity = 2;
    frag.binding_atoms = {1, 2};
    frag.charge = -2;
    return frag;
}

inline FragmentTemplate phosphate_topology() {
    FragmentTemplate frag;
    frag.name = "phosphate";
    frag.formula = "PO4";
    frag.atom_types = {15, 8, 8, 8, 8};  // P, O, O, O, O
    frag.bonds = {{0, 1}, {0, 2}, {0, 3}, {0, 4}};
    frag.attachment_point = 1;
    frag.site_type = SiteType::BIDENTATE;
    frag.is_ligand = true;
    frag.denticity = 2;
    frag.binding_atoms = {1, 2};
    frag.charge = -3;
    return frag;
}

inline FragmentTemplate nitrate_topology() {
    FragmentTemplate frag;
    frag.name = "nitrate";
    frag.formula = "NO3";
    frag.atom_types = {7, 8, 8, 8};  // N, O, O, O
    frag.bonds = {{0, 1}, {0, 2}, {0, 3}};
    frag.attachment_point = 1;
    frag.site_type = SiteType::BIDENTATE;
    frag.is_ligand = true;
    frag.denticity = 2;
    frag.binding_atoms = {1, 2};
    frag.charge = -1;
    return frag;
}

// ============================================================================
// Fragment Library Registry
// ============================================================================
class FragmentLibrary {
public:
    FragmentLibrary() {
        register_fragment(hydroxyl_topology());
        register_fragment(methyl_topology());
        register_fragment(amino_topology());
        register_fragment(carboxyl_topology());
        register_fragment(oxalate_topology());
        register_fragment(carbonate_topology());
        register_fragment(sulfate_topology());
        register_fragment(phosphate_topology());
        register_fragment(nitrate_topology());
    }
    
    void register_fragment(const FragmentTemplate& frag) {
        fragments_[frag.name] = frag;
        formula_map_[frag.formula] = frag.name;
    }
    
    const FragmentTemplate* get_by_name(const std::string& name) const {
        auto it = fragments_.find(name);
        return (it != fragments_.end()) ? &it->second : nullptr;
    }
    
    const FragmentTemplate* get_by_formula(const std::string& formula) const {
        auto it = formula_map_.find(formula);
        if (it != formula_map_.end()) {
            return get_by_name(it->second);
        }
        return nullptr;
    }
    
    std::vector<std::string> detect_fragments(
        const std::map<std::string, int>& elem_counts
    ) const {
        std::vector<std::string> detected;
        
        auto has = [&](const std::string& symbol, int min_count) -> bool {
            auto it = elem_counts.find(symbol);
            return (it != elem_counts.end() && it->second >= min_count);
        };
        
        if (has("C", 2) && has("O", 4)) detected.push_back("oxalate");
        if (has("C", 1) && has("O", 3))  detected.push_back("carbonate");
        if (has("S", 1) && has("O", 4))  detected.push_back("sulfate");
        if (has("P", 1) && has("O", 4))  detected.push_back("phosphate");
        if (has("N", 1) && has("O", 3))  detected.push_back("nitrate");
        if (has("O", 1) && has("H", 1))  detected.push_back("hydroxyl");
        if (has("C", 1) && has("H", 3))  detected.push_back("methyl");
        
        return detected;
    }
    
    std::vector<std::string> list_all() const {
        std::vector<std::string> names;
        for (const auto& pair : fragments_) {
            names.push_back(pair.first);
        }
        return names;
    }
    
private:
    std::map<std::string, FragmentTemplate> fragments_;
    std::map<std::string, std::string> formula_map_;
};

inline FragmentLibrary& get_fragment_library() {
    static FragmentLibrary library;
    return library;
}

} // namespace fragment
} // namespace vsepr
