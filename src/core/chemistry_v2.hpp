#pragma once
/**
 * chemistry_v2.hpp
 * ================
 * Universal chemistry model for both organic and coordination compounds.
 * 
 * Design Principles:
 * - Data-driven bonding manifolds (no "if organic then..." code paths)
 * - Lightweight Atom (just Z + charge/spin, everything else is lookup)
 * - Tiered validation (reject/penalize/exotic)
 * - Computed annotations cached once (hybridization, aromaticity, rings)
 * - Universal API that works for CH4 and [Fe(CN)6]⁴⁻
 */

#include "types.hpp"
#include "element_data.hpp"
#include <cmath>
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <algorithm>
#include <limits>

namespace vsepr {

//=============================================================================
// Global Thermodynamic Configuration
//=============================================================================

struct ThermalConfig {
    static constexpr double kB = 1.987204259e-3;  // kcal/(mol·K)
    
    double T_K = 0.0;  // Temperature in Kelvin (0 = pure energy mode)
    
    ThermalConfig() : T_K(0.0) {}
    explicit ThermalConfig(double temperature_K) : T_K(temperature_K) {}
    
    double beta() const {
        if (T_K <= 0.0) return std::numeric_limits<double>::infinity();
        return 1.0 / (kB * T_K);
    }
    
    bool is_zero_kelvin() const { return T_K <= 0.0; }
    
    double boltzmann_factor(double energy) const {
        if (is_zero_kelvin()) return (energy <= 0.0) ? 1.0 : 0.0;
        return std::exp(-beta() * energy);
    }
    
    double free_energy_from_energies(const std::vector<double>& energies) const {
        if (is_zero_kelvin()) {
            return *std::min_element(energies.begin(), energies.end());
        }
        if (energies.empty()) return 0.0;
        
        double E_min = *std::min_element(energies.begin(), energies.end());
        double Z = 0.0;
        for (double E : energies) {
            Z += std::exp(-beta() * (E - E_min));
        }
        return E_min - (1.0 / beta()) * std::log(Z);
    }
};

//=============================================================================
// Hybridization (computed annotation for main-group elements)
//=============================================================================

enum class Hybridization : uint8_t {
    UNKNOWN = 0,
    SP3     = 1,  // Tetrahedral (109.5°)
    SP2     = 2,  // Trigonal planar (120°)
    SP      = 3,  // Linear (180°)
    SP3D    = 4,  // Trigonal bipyramidal
    SP3D2   = 5   // Octahedral
};

inline double ideal_angle_for_hybridization(Hybridization hyb) {
    constexpr double pi = 3.14159265358979323846;
    switch (hyb) {
        case Hybridization::SP:     return pi;           // 180°
        case Hybridization::SP2:    return 2.0*pi/3.0;   // 120°
        case Hybridization::SP3:    return 1.910633236;  // 109.471°
        case Hybridization::SP3D:   return pi/2.0;       // 90° (approx)
        case Hybridization::SP3D2:  return pi/2.0;       // 90°
        default:                    return 2.0*pi/3.0;
    }
}

inline double angle_force_constant_from_hybridization(Hybridization hyb) {
    switch (hyb) {
        case Hybridization::SP:     return 100.0;
        case Hybridization::SP2:    return 80.0;
        case Hybridization::SP3:    return 60.0;
        case Hybridization::SP3D:   return 40.0;
        case Hybridization::SP3D2:  return 40.0;
        default:                    return 50.0;
    }
}

//=============================================================================
// Validation Tiers
//=============================================================================

enum class ValidationTier {
    PASS,            // Valid structure
    EXOTIC,          // Allowed but uncommon (hypervalency, radicals)
    IMPLAUSIBLE,     // Chemically unlikely (large penalty)
    REJECT           // Impossible (negative coords, invalid graph)
};

struct ValidationResult {
    ValidationTier tier;
    double penalty_kcal_mol;  // Energy penalty for implausible structures
    std::string message;
    
    ValidationResult(ValidationTier t = ValidationTier::PASS, 
                    double penalty = 0.0, 
                    const std::string& msg = "")
        : tier(t), penalty_kcal_mol(penalty), message(msg) {}
    
    bool is_valid() const { return tier != ValidationTier::REJECT; }
    bool needs_penalty() const { return tier == ValidationTier::IMPLAUSIBLE || tier == ValidationTier::EXOTIC; }
};

//=============================================================================
// Chemistry Graph (topology + computed annotations)
//=============================================================================

class ChemistryGraph {
public:
    // Topology
    std::vector<Atom> atoms;
    std::vector<Bond> bonds;
    
    // Cached topology
    std::vector<std::vector<uint32_t>> neighbors_;       // neighbors[i] = {j, k, ...}
    std::vector<std::vector<uint8_t>> bond_orders_;      // bond_orders_[i][idx] = order to neighbor idx
    std::unordered_map<uint64_t, uint8_t> bond_order_map_;  // (i,j) → order
    
    // Computed annotations (filled by perception pipeline)
    std::vector<Hybridization> hybridization_;
    std::vector<bool> is_aromatic_atom_;
    std::vector<bool> is_aromatic_bond_;
    std::vector<bool> is_ring_atom_;
    
    // Property maps (extensible without struct changes)
    std::unordered_map<uint32_t, int> atom_type_;       // Atom → FF type
    std::unordered_map<uint64_t, int> bond_type_;       // Bond → FF type
    
    ChemistryGraph() = default;
    
    // Build from raw data
    void build(const std::vector<Atom>& atoms_in, const std::vector<Bond>& bonds_in);
    
    // Perception pipeline (compute all annotations)
    void perceive();
    
    //=========================================================================
    // Universal API Functions
    //=========================================================================
    
    // Element database lookups (atoms are lightweight)
    uint8_t Z(uint32_t i) const { return atoms[i].Z; }
    BondingManifold manifold(uint32_t i) const { return chemistry_db().get_manifold(Z(i)); }
    bool is_main_group(uint32_t i) const { return manifold(i) == BondingManifold::COVALENT; }
    bool is_metal(uint32_t i) const { return manifold(i) == BondingManifold::COORDINATION; }
    
    // Topology queries
    const std::vector<uint32_t>& neighbors(uint32_t i) const { return neighbors_[i]; }
    int degree(uint32_t i) const { return neighbors_[i].size(); }
    
    uint8_t bond_order(uint32_t i, uint32_t j) const {
        uint64_t key = bond_key(i, j);
        auto it = bond_order_map_.find(key);
        return (it != bond_order_map_.end()) ? it->second : 0;
    }
    
    int bond_order_sum(uint32_t i) const {
        int sum = 0;
        for (uint8_t order : bond_orders_[i]) sum += order;
        return sum;
    }
    
    int coordination_number(uint32_t i) const { return degree(i); }
    
    // Topological distance (for exclusions, 1-4 scaling)
    int topological_distance(uint32_t i, uint32_t j) const;
    
    // Ring/aromaticity queries
    bool is_ring_atom(uint32_t i) const { 
        return i < is_ring_atom_.size() ? is_ring_atom_[i] : false; 
    }
    
    bool is_aromatic_atom(uint32_t i) const { 
        return i < is_aromatic_atom_.size() ? is_aromatic_atom_[i] : false; 
    }
    
    bool is_aromatic_bond(uint32_t i, uint32_t j) const {
        for (size_t b = 0; b < bonds.size(); ++b) {
            if ((bonds[b].i == i && bonds[b].j == j) || (bonds[b].i == j && bonds[b].j == i)) {
                return b < is_aromatic_bond_.size() ? is_aromatic_bond_[b] : false;
            }
        }
        return false;
    }
    
    // Hybridization (cached, main-group only)
    Hybridization hybridization(uint32_t i) const {
        return i < hybridization_.size() ? hybridization_[i] : Hybridization::UNKNOWN;
    }
    
    //=========================================================================
    // Validation (tiered: reject / penalize / exotic)
    //=========================================================================
    
    ValidationResult validate(bool allow_exotic = false) const;
    
private:
    static uint64_t bond_key(uint32_t i, uint32_t j) {
        if (i > j) std::swap(i, j);
        return (uint64_t(i) << 32) | j;
    }
    
    void build_neighbor_cache();
    void detect_rings();
    void detect_aromaticity();
    void infer_hybridization();
    
    ValidationResult validate_atom(uint32_t i, bool allow_exotic) const;
};

//=============================================================================
// Implementation
//=============================================================================

inline void ChemistryGraph::build(const std::vector<Atom>& atoms_in, const std::vector<Bond>& bonds_in) {
    atoms = atoms_in;
    bonds = bonds_in;
    
    build_neighbor_cache();
}

inline void ChemistryGraph::build_neighbor_cache() {
    neighbors_.clear();
    bond_orders_.clear();
    bond_order_map_.clear();
    
    neighbors_.resize(atoms.size());
    bond_orders_.resize(atoms.size());
    
    for (const auto& bond : bonds) {
        neighbors_[bond.i].push_back(bond.j);
        neighbors_[bond.j].push_back(bond.i);
        
        bond_orders_[bond.i].push_back(bond.order);
        bond_orders_[bond.j].push_back(bond.order);
        
        bond_order_map_[bond_key(bond.i, bond.j)] = bond.order;
    }
}

inline void ChemistryGraph::perceive() {
    detect_rings();
    detect_aromaticity();
    infer_hybridization();
}

inline void ChemistryGraph::detect_rings() {
    // Simple cycle detection (basic implementation)
    is_ring_atom_.resize(atoms.size(), false);
    // TODO: Implement proper ring perception (SSSR, Figueras, etc.)
}

inline void ChemistryGraph::detect_aromaticity() {
    // Placeholder: Hückel rule + planarity check
    is_aromatic_atom_.resize(atoms.size(), false);
    is_aromatic_bond_.resize(bonds.size(), false);
    // TODO: Implement aromaticity perception
}

inline void ChemistryGraph::infer_hybridization() {
    hybridization_.resize(atoms.size(), Hybridization::UNKNOWN);
    
    for (size_t i = 0; i < atoms.size(); ++i) {
        if (!is_main_group(i)) continue;  // Only for covalent manifold
        
        int deg = degree(i);
        int max_order = 0;
        for (uint8_t order : bond_orders_[i]) {
            max_order = std::max(max_order, (int)order);
        }
        
        uint8_t Z = atoms[i].Z;
        int total_domains = deg + atoms[i].lone_pairs;
        
        // Carbon-specific (most common)
        if (Z == 6) {
            if (max_order >= 3) hybridization_[i] = Hybridization::SP;
            else if (max_order == 2) hybridization_[i] = Hybridization::SP2;
            else if (total_domains == 4) hybridization_[i] = Hybridization::SP3;
            else if (total_domains == 3) hybridization_[i] = Hybridization::SP2;
            else if (total_domains == 2) hybridization_[i] = Hybridization::SP;
        }
        // General heuristic
        else {
            switch (total_domains) {
                case 2: hybridization_[i] = Hybridization::SP; break;
                case 3: hybridization_[i] = Hybridization::SP2; break;
                case 4: hybridization_[i] = Hybridization::SP3; break;
                case 5: hybridization_[i] = Hybridization::SP3D; break;
                case 6: hybridization_[i] = Hybridization::SP3D2; break;
            }
        }
    }
}

inline int ChemistryGraph::topological_distance(uint32_t i, uint32_t j) const {
    if (i == j) return 0;
    
    // BFS to find shortest path
    std::vector<int> dist(atoms.size(), -1);
    std::vector<uint32_t> queue;
    
    dist[i] = 0;
    queue.push_back(i);
    
    for (size_t qi = 0; qi < queue.size(); ++qi) {
        uint32_t u = queue[qi];
        if (u == j) return dist[j];
        
        for (uint32_t v : neighbors_[u]) {
            if (dist[v] == -1) {
                dist[v] = dist[u] + 1;
                queue.push_back(v);
            }
        }
    }
    
    return -1;  // Not connected
}

inline ValidationResult ChemistryGraph::validate(bool allow_exotic) const {
    // Tier A: Hard sanity checks (reject)
    if (atoms.empty()) {
        return ValidationResult(ValidationTier::REJECT, 0, "Empty molecule");
    }
    
    for (uint32_t i = 0; i < atoms.size(); ++i) {
        auto result = validate_atom(i, allow_exotic);
        if (result.tier == ValidationTier::REJECT) return result;
    }
    
    return ValidationResult(ValidationTier::PASS);
}

inline ValidationResult ChemistryGraph::validate_atom(uint32_t i, bool allow_exotic) const {
    const uint8_t elem_Z = atoms[i].Z;
    const auto& chem = chemistry_db().get_chem_data(elem_Z);
    const std::string elem_symbol = chemistry_db().get_symbol(elem_Z);
    
    // Tier A: Nonsensical values
    if (degree(i) > 12) {
        return ValidationResult(ValidationTier::REJECT, 0, 
            "Atom " + std::to_string(i) + " has impossible coordination " + std::to_string(degree(i)));
    }
    
    // Main-group covalent validation
    if (is_main_group(i)) {
        int total_bonds = bond_order_sum(i);
        int coord = degree(i);
        int charge = 0;  // TODO: Use atoms[i].formal_charge when available
        
        // Check against allowed valence patterns
        bool found_pattern = false;
        bool is_exotic_pattern = false;
        
        for (const auto& pattern : chem.allowed_valences) {
            if (pattern.formal_charge != charge) continue;
            if (total_bonds == pattern.total_bonds && coord == pattern.coordination_number) {
                found_pattern = true;
                is_exotic_pattern = !pattern.common;
                break;
            }
        }
        
        if (!found_pattern) {
            // Tier C: Not in dataset
            if (!allow_exotic) {
                return ValidationResult(ValidationTier::REJECT, 0,
                    elem_symbol + " with " + std::to_string(total_bonds) + 
                    " bonds not in allowed patterns");
            }
            return ValidationResult(ValidationTier::EXOTIC, 50.0,
                "Exotic bonding for " + elem_symbol);
        }
        
        if (is_exotic_pattern && !allow_exotic) {
            // Tier B: Uncommon but known
            return ValidationResult(ValidationTier::IMPLAUSIBLE, 10.0,
                "Uncommon bonding for " + elem_symbol);
        }
    }
    
    // Coordination manifold validation (broader ranges)
    else if (chem.manifold == BondingManifold::COORDINATION) {
        int coord = degree(i);
        
        // Check if coordination number is in allowed patterns
        bool found = false;
        for (const auto& pattern : chem.allowed_valences) {
            if (coord == pattern.coordination_number) {
                found = true;
                break;
            }
        }
        
        if (!found && !allow_exotic) {
            return ValidationResult(ValidationTier::IMPLAUSIBLE, 5.0,
                "Unusual coordination for " + elem_symbol);
        }
    }
    
    return ValidationResult(ValidationTier::PASS);
}

//=============================================================================
// Backwards Compatibility Helpers
//=============================================================================

// Old API: infer_hybridization(Z, bond_orders, lone_pairs)
inline Hybridization infer_hybridization(uint8_t Z, const std::vector<uint8_t>& bond_orders, uint8_t lone_pairs = 0) {
    int deg = bond_orders.size();
    int max_order = 0;
    for (uint8_t order : bond_orders) {
        max_order = std::max(max_order, (int)order);
    }
    
    int total_domains = deg + lone_pairs;
    
    if (Z == 6) {
        if (max_order >= 3) return Hybridization::SP;
        if (max_order == 2) return Hybridization::SP2;
        if (total_domains == 4) return Hybridization::SP3;
        if (total_domains == 3) return Hybridization::SP2;
        if (total_domains == 2) return Hybridization::SP;
    }
    
    switch (total_domains) {
        case 2: return Hybridization::SP;
        case 3: return Hybridization::SP2;
        case 4: return Hybridization::SP3;
        case 5: return Hybridization::SP3D;
        case 6: return Hybridization::SP3D2;
        default: return Hybridization::UNKNOWN;
    }
}

// Old API: check_valence(Z, bond_orders)
inline bool check_valence(uint8_t Z, const std::vector<uint8_t>& bond_orders) {
    const auto& elem = chemistry_db().get_chem_data(Z);
    int total = 0;
    for (uint8_t order : bond_orders) total += order;
    int coord = bond_orders.size();
    
    for (const auto& pattern : elem.allowed_valences) {
        if (total == pattern.total_bonds && coord == pattern.coordination_number) {
            return true;
        }
    }
    return false;
}

} // namespace vsepr
