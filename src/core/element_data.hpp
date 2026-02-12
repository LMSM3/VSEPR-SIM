#pragma once
/**
 * element_data.hpp
 * ================
 * Chemistry-specific element database that INTEGRATES with periodic_db.hpp.
 * 
 * NO DUPLICATION: 
 * - Atomic masses, symbols, electronegativity → FROM periodic_db.hpp
 * - Chemistry metadata (bonding manifolds, valence patterns) → ADDED HERE
 */

#include <cstdint>
#include <string>
#include <vector>
#include <array>
#include <stdexcept>
#include "pot/periodic_db.hpp"

namespace vsepr {

enum class BondingManifold : uint8_t {
    COVALENT, COORDINATION, NOBLE_GAS, UNKNOWN
};

struct ValencePattern {
    int total_bonds, coordination_number, formal_charge;
    bool common;
    ValencePattern(int b, int c, int ch = 0, bool cm = true)
        : total_bonds(b), coordination_number(c), formal_charge(ch), common(cm) {}
};

struct ChemistryMetadata {
    uint8_t Z;
    BondingManifold manifold;
    std::vector<ValencePattern> allowed_valences;
    double lj_epsilon, lj_sigma;
    double covalent_radius_single, covalent_radius_double, covalent_radius_triple;
};

class ElementDatabase {
    const PeriodicTable* pt_;
    std::array<ChemistryMetadata, 119> chem_;
    void init_main_group();
    void init_metals();
    void init_noble();
public:
    explicit ElementDatabase(const PeriodicTable* pt);
    
    double get_mass(uint8_t Z) const {
        auto* p = pt_->by_Z(Z);
        return p ? p->atomic_mass : 0.0;
    }
    double get_electronegativity(uint8_t Z) const {
        auto* p = pt_->by_Z(Z);
        return p && p->en_pauling ? *p->en_pauling : 0.0;
    }
    std::string get_symbol(uint8_t Z) const {
        auto* p = pt_->by_Z(Z);
        return p ? p->symbol : "??";
    }
    std::string get_name(uint8_t Z) const {
        auto* p = pt_->by_Z(Z);
        return p ? p->name : "Unknown";
    }
    double get_vdw_radius(uint8_t Z) const {
        // VDW radius not in current Element struct, use default
        return 2.0;
    }
    uint8_t Z_from_symbol(const std::string& s) const {
        auto* p = pt_->by_symbol(s);
        return p ? p->Z : 0;
    }
    
    const ChemistryMetadata& get_chem_data(uint8_t Z) const {
        return (Z && Z <= 118) ? chem_[Z] : chem_[0];
    }
    BondingManifold get_manifold(uint8_t Z) const { return get_chem_data(Z).manifold; }
    bool is_main_group(uint8_t Z) const { return get_manifold(Z) == BondingManifold::COVALENT; }
    const std::vector<ValencePattern>& get_allowed_valences(uint8_t Z) const {
        return get_chem_data(Z).allowed_valences;
    }
    double get_covalent_radius(uint8_t Z, uint8_t bo = 1) const {
        auto& c = get_chem_data(Z);
        return bo == 1 ? c.covalent_radius_single :
               bo == 2 ? (c.covalent_radius_double > 0 ? c.covalent_radius_double : c.covalent_radius_single * 0.87) :
               bo == 3 ? (c.covalent_radius_triple > 0 ? c.covalent_radius_triple : c.covalent_radius_single * 0.78) :
               c.covalent_radius_single;
    }
    double get_lj_epsilon(uint8_t Z) const { return get_chem_data(Z).lj_epsilon; }
    double get_lj_sigma(uint8_t Z) const { return get_chem_data(Z).lj_sigma; }
};

inline ElementDatabase::ElementDatabase(const PeriodicTable* pt) : pt_(pt) {
    if (!pt) throw std::invalid_argument("ElementDatabase needs PeriodicTable");
    for (auto& c : chem_) {
        c.Z = 0; c.manifold = BondingManifold::UNKNOWN;
        c.lj_epsilon = 0.1; c.lj_sigma = 3.4;
        c.covalent_radius_single = 1.5; c.covalent_radius_double = 0; c.covalent_radius_triple = 0;
    }
    init_main_group(); init_metals(); init_noble();
}

inline void ElementDatabase::init_main_group() {
    chem_[1] = {1, BondingManifold::COVALENT, {{1,1,0,true}}, 0.015, 2.65, 0.31, 0, 0};
    chem_[6] = {6, BondingManifold::COVALENT, {{4,4,0,true},{4,3,0,true},{4,2,0,true},{3,3,1,false},{3,3,-1,false}}, 0.105, 3.40, 0.76, 0.67, 0.60};
    chem_[7] = {7, BondingManifold::COVALENT, {{3,3,0,true},{3,2,0,true},{3,1,0,true},{4,4,1,true},{2,2,-1,false}}, 0.069, 3.25, 0.71, 0.60, 0.54};
    chem_[8] = {8, BondingManifold::COVALENT, {{2,2,0,true},{2,1,0,true},{3,3,1,false},{1,1,-1,true}}, 0.060, 3.12, 0.66, 0.57, 0};
    chem_[9] = {9, BondingManifold::COVALENT, {{1,1,0,true},{1,1,-1,true}}, 0.050, 2.94, 0.57, 0, 0};
    chem_[15] = {15, BondingManifold::COVALENT, {{3,3,0,true},{5,4,0,true}}, 0.200, 3.74, 1.07, 1.00, 0.94};
    chem_[16] = {16, BondingManifold::COVALENT, {{2,2,0,true},{4,3,0,false},{6,4,0,false}}, 0.250, 3.56, 1.05, 0.94, 0};
    chem_[17] = {17, BondingManifold::COVALENT, {{1,1,0,true},{1,1,-1,true}}, 0.265, 3.52, 1.02, 0.89, 0};
    chem_[35] = {35, BondingManifold::COVALENT, {{1,1,0,true},{1,1,-1,true}}, 0.320, 3.73, 1.20, 1.04, 0};
    chem_[53] = {53, BondingManifold::COVALENT, {{1,1,0,true},{1,1,-1,true}}, 0.360, 4.01, 1.39, 1.23, 0};
}

inline void ElementDatabase::init_metals() {
    chem_[26] = {26, BondingManifold::COORDINATION, {{6,6,2,true},{6,6,3,true},{4,4,2,false},{5,5,2,false}}, 0.280, 3.80, 1.32, 0, 0};
    chem_[29] = {29, BondingManifold::COORDINATION, {{4,4,2,true},{4,4,1,false},{6,6,2,false}}, 0.260, 3.76, 1.32, 0, 0};
    chem_[30] = {30, BondingManifold::COORDINATION, {{4,4,2,true},{6,6,2,false}}, 0.240, 3.72, 1.22, 0, 0};
}

inline void ElementDatabase::init_noble() {
    chem_[2] = {2, BondingManifold::NOBLE_GAS, {}, 0.020, 2.55, 0.28, 0, 0};
    chem_[10] = {10, BondingManifold::NOBLE_GAS, {}, 0.042, 2.75, 0.58, 0, 0};
    chem_[18] = {18, BondingManifold::NOBLE_GAS, {}, 0.120, 3.40, 1.06, 0, 0};
}

inline ElementDatabase* g_chem_db = nullptr;
inline void init_chemistry_db(const PeriodicTable* pt) {
    static ElementDatabase db(pt);
    g_chem_db = &db;
}
inline const ElementDatabase& chemistry_db() {
    if (!g_chem_db) throw std::runtime_error("Call init_chemistry_db first");
    return *g_chem_db;
}
inline const ElementDatabase& elements() { return chemistry_db(); }
inline const ChemistryMetadata& getElement(uint8_t Z) { return chemistry_db().get_chem_data(Z); }

} // namespace vsepr
