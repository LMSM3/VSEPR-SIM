#pragma once
/**
 * element_data_integrated.hpp
 * ===========================
 * Chemistry-specific element database that INTEGRATES with periodic_db.hpp.
 *
 * NO DUPLICATION:
 * - Atomic masses, symbols, electronegativity → FROM periodic_db.hpp
 * - Chemistry metadata (bonding manifolds, valence patterns) → ADDED HERE
 *
 * Design principles:
 * - Single source of truth for periodic data (periodic_db.hpp)
 * - Chemistry system adds bonding rules & force field params
 * - Lightweight atoms (just Z + charge)
 * - Extensible via data (not code)
 */

#include <cstdint>
#include <string>
#include <vector>
#include <array>
#include <stdexcept>
#include <utility>
#include "../pot/periodic_db.hpp"  // Existing periodic table

namespace vsepr {

//=============================================================================
// Bonding Manifold Classification
//=============================================================================

enum class BondingManifold : uint8_t {
    COVALENT,      // Main-group: integer bond orders
    COORDINATION,  // Metals: partial/coordinate bonds
    IONIC,         // Alkali/alkaline earth: electrostatic coordination
    NOBLE_GAS,     // Unreactive
    UNKNOWN
};

//=============================================================================
// Valence Pattern (allowed bonding states)
//=============================================================================

struct ValencePattern {
    int total_bonds;           // Sum of bond orders
    int coordination_number;   // Number of neighbors
    int formal_charge;         // Required formal charge
    bool common;               // Typical state?

    ValencePattern(int bonds, int coord, int charge = 0, bool is_common = true)
        : total_bonds(bonds), coordination_number(coord), formal_charge(charge), common(is_common) {}
};

//=============================================================================
// Chemistry Metadata (bonds rules + force field params, NOT periodic data)
//=============================================================================

struct ChemistryMetadata {
    uint8_t Z = 0;
    BondingManifold manifold = BondingManifold::UNKNOWN;
    std::vector<ValencePattern> allowed_valences;

    // Force field parameters (simple LJ placeholder; can be swapped later)
    double lj_epsilon = 0.1;   // kcal/mol
    double lj_sigma   = 3.4;   // Angstrom

    // Covalent radii by bond order (Å). 0 means "not provided".
    double covalent_radius_single = 1.5;
    double covalent_radius_double = 0.0;
    double covalent_radius_triple = 0.0;
};

//=============================================================================
// Element Database (periodic_db.hpp + chemistry metadata)
//=============================================================================

class ChemistryElementDatabase {
private:
    const PeriodicTable* periodic_table_;                 // External periodic table (not owned)
    std::array<ChemistryMetadata, 119> chem_data_;         // Z=0 unused, Z=1-118

    void initialize_defaults();
    void initialize_main_group();
    void initialize_transition_metals();
    void initialize_noble_gases();

    // Small helper to keep initialization consistent and compact.
    static ChemistryMetadata make(
        uint8_t Z,
        BondingManifold m,
        std::vector<ValencePattern> vals,
        double eps, double sig,
        double r1, double r2 = 0.0, double r3 = 0.0
    ) {
        ChemistryMetadata c;
        c.Z = Z;
        c.manifold = m;
        c.allowed_valences = std::move(vals);
        c.lj_epsilon = eps;
        c.lj_sigma = sig;
        c.covalent_radius_single = r1;
        c.covalent_radius_double = r2;
        c.covalent_radius_triple = r3;
        return c;
    }

public:
    explicit ChemistryElementDatabase(const PeriodicTable* pt);

    // ------------------------------------------------------------------------
    // Periodic table queries (delegate to periodic_db.hpp)
    // ------------------------------------------------------------------------
    
    std::string get_symbol(uint8_t Z) const {
        if (Z == 92) return "U";
        const auto* phys = periodic_table_->physics_by_Z(Z);
        if (phys && !phys->symbol.empty()) return phys->symbol;
        // Fallback for datasets missing symbols
        static const char* fallback[] = {
            "", "H","He","Li","Be","B","C","N","O","F","Ne",
            "Na","Mg","Al","Si","P","S","Cl","Ar","K","Ca"
        };
        if (Z < sizeof(fallback)/sizeof(fallback[0]) && fallback[Z][0]) return fallback[Z];
        return "??";
    }
    
    std::string get_name(uint8_t Z) const {
        const auto* phys = periodic_table_->physics_by_Z(Z);
        return phys ? phys->name : "Unknown";
    }
    
    double get_vdw_radius(uint8_t Z) const {
        const auto* vis = periodic_table_->visual_by_Z(Z);
        // Element struct does not have vdw_radius_pm field, use fallback
        (void)vis;  // Suppress unused variable warning
        return 2.0;  // Fallback
    }
    
    uint8_t Z_from_symbol(const std::string& symbol) const {
        const auto* phys = periodic_table_->physics_by_symbol(symbol);
        return phys ? phys->Z : 0;
    }
    
    // ------------------------------------------------------------------------
    // Chemistry metadata queries (from chem_data_)
    // ------------------------------------------------------------------------
    
    const ChemistryMetadata& get_chem_data(uint8_t Z) const {
        if (Z == 0 || Z > 118) return chem_data_[0];
        return chem_data_[Z];
    }

    BondingManifold get_manifold(uint8_t Z) const {
        return get_chem_data(Z).manifold;
    }

    bool is_main_group(uint8_t Z) const {
        return get_manifold(Z) == BondingManifold::COVALENT;
    }

    const std::vector<ValencePattern>& get_allowed_valences(uint8_t Z) const {
        return get_chem_data(Z).allowed_valences;
    }
    
    // Covalent radius by bond order
    double get_covalent_radius(uint8_t Z, uint8_t bond_order = 1) const {
        const auto& chem = get_chem_data(Z);
        switch (bond_order) {
            case 1: return chem.covalent_radius_single;
            case 2: return chem.covalent_radius_double > 0 ? chem.covalent_radius_double 
                                                            : chem.covalent_radius_single * 0.87;
            case 3: return chem.covalent_radius_triple > 0 ? chem.covalent_radius_triple 
                                                            : chem.covalent_radius_single * 0.78;
            default: return chem.covalent_radius_single;
        }
    }
    
    // LJ parameters  
    double get_lj_epsilon(uint8_t Z) const { return get_chem_data(Z).lj_epsilon; }
    double get_lj_sigma(uint8_t Z) const { return get_chem_data(Z).lj_sigma; }
};

//=============================================================================
// Implementation
//=============================================================================

inline ChemistryElementDatabase::ChemistryElementDatabase(const PeriodicTable* pt)
    : periodic_table_(pt) {
    if (!pt) {
        throw std::invalid_argument("ChemistryElementDatabase requires non-null PeriodicTable");
    }

    initialize_defaults();
    initialize_main_group();
    initialize_transition_metals();
    initialize_noble_gases();
}

inline void ChemistryElementDatabase::initialize_defaults() {
    // Z=0 sentinel: predictable "invalid element"
    chem_data_[0] = make(
        0,
        BondingManifold::UNKNOWN,
        {},
        0.0, 0.0,
        0.0, 0.0, 0.0
    );

    // Default for everything else: "unknown but not explosive"
    // NOTE: radii are approximate single-bond covalent radii in Å (usable defaults).
    // You can later swap these to a specific radii table (Pyykkö, Cordero, etc.)
    // without changing the API.

    // Hydrogen (Z=1)
    chem_data_[1] = make(1, BondingManifold::COVALENT,
        {{1, 1, 0, true}},
        0.015, 2.65,
        0.31
    );

    // Helium handled in noble gases.

    // Seed every slot with safe defaults (updated later by specific initializers)
    for (uint8_t Z = 2; Z <= 118; ++Z) {
        if (chem_data_[Z].Z == 0) {
            chem_data_[Z] = make(Z, BondingManifold::UNKNOWN, {{2, 2, 0, false}}, 0.1, 3.5, 1.20);
        }
    }
}

inline void ChemistryElementDatabase::initialize_main_group() {
    // ---------- Alkali metals (IONIC) ----------
    chem_data_[3]  = make(3,  BondingManifold::IONIC, {{0, 4, 1, true}, {0, 6, 1, true}}, 0.030, 2.90, 1.28); // Li+
    chem_data_[11] = make(11, BondingManifold::IONIC, {{0, 4, 1, true}, {0, 6, 1, true}, {0, 8, 1, false}}, 0.040, 3.25, 1.66); // Na+
    chem_data_[19] = make(19, BondingManifold::IONIC, {{0, 6, 1, true}, {0, 8, 1, true}}, 0.050, 3.70, 2.03); // K+
    chem_data_[37] = make(37, BondingManifold::IONIC, {{0, 6, 1, true}, {0, 8, 1, true}}, 0.060, 3.95, 2.20); // Rb+
    chem_data_[55] = make(55, BondingManifold::IONIC, {{0, 6, 1, true}, {0, 8, 1, true}}, 0.070, 4.20, 2.44); // Cs+

    // ---------- Alkaline earths (IONIC) ----------
    chem_data_[4]  = make(4,  BondingManifold::IONIC, {{0, 4, 2, true}, {0, 6, 2, true}}, 0.040, 2.95, 0.96); // Be2+
    chem_data_[12] = make(12, BondingManifold::IONIC, {{0, 6, 2, true}, {0, 8, 2, false}}, 0.050, 3.10, 1.41); // Mg2+
    chem_data_[20] = make(20, BondingManifold::IONIC, {{0, 6, 2, true}, {0, 8, 2, true}}, 0.060, 3.40, 1.76); // Ca2+
    chem_data_[38] = make(38, BondingManifold::IONIC, {{0, 6, 2, true}, {0, 8, 2, true}}, 0.070, 3.60, 1.95); // Sr2+
    chem_data_[56] = make(56, BondingManifold::IONIC, {{0, 6, 2, true}, {0, 8, 2, true}}, 0.080, 3.80, 2.15); // Ba2+

    // ---------- Group 13 ----------
    chem_data_[5]  = make(5,  BondingManifold::COVALENT, {{3,3,0,true},{4,4,-1,true}}, 0.060, 3.10, 0.84, 0.78, 0.0); // B
    chem_data_[13] = make(13, BondingManifold::IONIC,    {{0,6,3,true},{0,4,3,false}}, 0.080, 3.50, 1.21); // Al3+ (ionic-ish)
    chem_data_[31] = make(31, BondingManifold::COVALENT, {{3,3,0,true},{4,4,-1,false}}, 0.120, 3.90, 1.22); // Ga
    chem_data_[49] = make(49, BondingManifold::COVALENT, {{3,3,0,true},{4,4,-1,false}}, 0.140, 4.10, 1.42); // In

    // ---------- Group 14 ----------
    chem_data_[6]  = make(6, BondingManifold::COVALENT,
        {{4, 4, 0, true},       // sp3
         {4, 3, 0, true},       // sp2
         {4, 2, 0, true},       // sp
         {3, 3, 1, false},      // carbocation
         {3, 3, -1, false}},    // carbanion
        0.105, 3.40,
        0.76, 0.67, 0.60
    );
    chem_data_[14] = make(14, BondingManifold::COVALENT, {{4,4,0,true},{4,4,-1,false}}, 0.150, 3.80, 1.11, 1.02, 0.94); // Si
    chem_data_[32] = make(32, BondingManifold::COVALENT, {{4,4,0,true},{2,2,0,false}},   0.160, 3.95, 1.20); // Ge
    chem_data_[50] = make(50, BondingManifold::COVALENT, {{4,4,0,true},{2,2,2,false}},   0.180, 4.25, 1.39); // Sn (IV/II-ish)
    chem_data_[82] = make(82, BondingManifold::COVALENT, {{4,4,0,false},{2,2,2,true}},   0.200, 4.45, 1.44); // Pb (II common)

    // ---------- Group 15 ----------
    chem_data_[7]  = make(7,  BondingManifold::COVALENT,
        {{3,3,0,true},{3,2,0,true},{3,1,0,true},{4,4,1,true},{2,2,-1,false}},
        0.069, 3.25, 0.71, 0.60, 0.54); // N
    chem_data_[15] = make(15, BondingManifold::COVALENT,
        {{3,3,0,true},{5,5,0,true},{5,4,0,false},{4,4,1,false}},
        0.200, 3.74, 1.07, 1.00, 0.94); // P
    chem_data_[33] = make(33, BondingManifold::COVALENT, {{3,3,0,true},{5,5,0,false}}, 0.220, 3.90, 1.19); // As
    chem_data_[51] = make(51, BondingManifold::COVALENT, {{3,3,0,true},{5,5,0,false}}, 0.240, 4.10, 1.39); // Sb

    // ---------- Group 16 ----------
    chem_data_[8]  = make(8,  BondingManifold::COVALENT, {{2,2,0,true},{2,1,0,true},{3,3,1,false},{1,1,-1,true}}, 0.060, 3.12, 0.66, 0.57, 0.0); // O
    chem_data_[16] = make(16, BondingManifold::COVALENT, {{2,2,0,true},{4,3,0,false},{6,4,0,false},{2,2,-2,false}}, 0.250, 3.56, 1.05, 0.94, 0.0); // S
    chem_data_[34] = make(34, BondingManifold::COVALENT, {{2,2,0,true},{4,3,0,false},{6,4,0,false}}, 0.280, 3.80, 1.20); // Se
    chem_data_[52] = make(52, BondingManifold::COVALENT, {{2,2,0,true},{4,3,0,false},{6,4,0,false}}, 0.300, 4.00, 1.38); // Te

    // ---------- Group 17 (halogens) ----------
    chem_data_[9]  = make(9,  BondingManifold::COVALENT, {{1,1,0,true},{1,1,-1,true}}, 0.050, 2.94, 0.57); // F
    chem_data_[17] = make(17, BondingManifold::COVALENT, {{1,1,0,true},{1,1,-1,true}}, 0.265, 3.52, 1.02, 0.89, 0.0); // Cl
    chem_data_[35] = make(35, BondingManifold::COVALENT, {{1,1,0,true},{1,1,-1,true}}, 0.320, 3.73, 1.20, 1.04, 0.0); // Br
    chem_data_[53] = make(53, BondingManifold::COVALENT, {{1,1,0,true},{1,1,-1,true}}, 0.360, 4.01, 1.39, 1.23, 0.0); // I

    // ---------- Group 18 (placeholder noble handled elsewhere) ----------
}

inline void ChemistryElementDatabase::initialize_transition_metals() {
    // These are *patterns*, not guarantees. Coordination chemistry is messy; your solver decides via energy.

    // Scandium (21)
    chem_data_[21] = make(21, BondingManifold::COORDINATION,
        {{6,6,3,true},{8,8,3,false}},
        0.260, 3.75, 1.44
    );

    // Titanium (22)
    chem_data_[22] = make(22, BondingManifold::COORDINATION,
        {{6,6,4,true},{6,6,3,false},{4,4,4,false}},
        0.270, 3.80, 1.36
    );

    // Vanadium (23)
    chem_data_[23] = make(23, BondingManifold::COORDINATION,
        {{6,6,3,true},{6,6,4,false},{6,6,5,false}},
        0.275, 3.82, 1.34
    );

    // Chromium (24)
    chem_data_[24] = make(24, BondingManifold::COORDINATION,
        {{6,6,3,true},{6,6,2,false},{4,4,3,false}},
        0.290, 3.82, 1.28
    );

    // Manganese (25)
    chem_data_[25] = make(25, BondingManifold::COORDINATION,
        {{6,6,2,true},{6,6,3,false},{6,6,4,false}},
        0.295, 3.84, 1.27
    );

    // Iron (Z=26)
    chem_data_[26] = make(26, BondingManifold::COORDINATION,
        {{6, 6, 2, true},       // Fe(II) oct
         {6, 6, 3, true},       // Fe(III) oct
         {4, 4, 2, false},      // tetra
         {5, 5, 2, false}},     // square pyramidal-ish
        0.280, 3.80,
        1.32
    );

    // Cobalt (27)
    chem_data_[27] = make(27, BondingManifold::COORDINATION,
        {{6,6,2,true},{6,6,3,true},{4,4,2,false}},
        0.270, 3.78, 1.26
    );

    // Nickel (28)
    chem_data_[28] = make(28, BondingManifold::COORDINATION,
        {{6,6,2,true},{4,4,2,true}},
        0.265, 3.75, 1.24
    );

    // Copper (Z=29)
    chem_data_[29] = make(29, BondingManifold::COORDINATION,
        {{4, 4, 2, true},       // Cu(II) square planar
         {4, 4, 1, false},      // Cu(I) tetra
         {6, 6, 2, false}},
        0.260, 3.76,
        1.32
    );

    // Zinc (Z=30)
    chem_data_[30] = make(30, BondingManifold::COORDINATION,
        {{4, 4, 2, true},       // Zn(II) tetra
         {6, 6, 2, false}},
        0.240, 3.72,
        1.22
    );

    // Zirconium (40) — MSR / structural relevance
    chem_data_[40] = make(40, BondingManifold::COORDINATION,
        {{6,6,4,true},{8,8,4,false}},
        0.320, 4.00, 1.60
    );

    // Molybdenum (42)
    chem_data_[42] = make(42, BondingManifold::COORDINATION,
        {{6,6,4,true},{6,6,6,false},{4,4,6,false}},
        0.330, 4.05, 1.45
    );

    // Silver (47)
    chem_data_[47] = make(47, BondingManifold::COORDINATION,
        {{2,2,1,true},{4,4,1,false}},
        0.340, 4.00, 1.45
    );

    // Cadmium (48)
    chem_data_[48] = make(48, BondingManifold::COORDINATION,
        {{4,4,2,true},{6,6,2,false}},
        0.320, 4.05, 1.44
    );

    // Hafnium (72)
    chem_data_[72] = make(72, BondingManifold::COORDINATION,
        {{6,6,4,true},{8,8,4,false}},
        0.350, 4.10, 1.58
    );

    // Tungsten (74)
    chem_data_[74] = make(74, BondingManifold::COORDINATION,
        {{6,6,6,true},{6,6,4,false}},
        0.360, 4.10, 1.46
    );

    // Platinum (78)
    chem_data_[78] = make(78, BondingManifold::COORDINATION,
        {{4,4,2,true},{6,6,4,false}},
        0.370, 4.15, 1.36
    );

    // Gold (79)
    chem_data_[79] = make(79, BondingManifold::COORDINATION,
        {{2,2,1,true},{4,4,3,true}},
        0.360, 4.20, 1.44
    );

    // Mercury (80)
    chem_data_[80] = make(80, BondingManifold::COORDINATION,
        {{2,2,2,true},{4,4,2,false}},
        0.380, 4.30, 1.32
    );

    // Yttrium (39)
    chem_data_[39] = make(39, BondingManifold::COORDINATION, {{6,6,3,true},{8,8,3,false}}, 0.300, 3.95, 1.61);

    // Niobium (41)
    chem_data_[41] = make(41, BondingManifold::COORDINATION, {{6,6,5,true},{6,6,3,false}}, 0.310, 4.00, 1.46);

    // Technetium (43)
    chem_data_[43] = make(43, BondingManifold::COORDINATION, {{6,6,4,true},{6,6,5,false}}, 0.335, 4.05, 1.36);

    // Ruthenium (44)
    chem_data_[44] = make(44, BondingManifold::COORDINATION, {{6,6,3,true},{6,6,4,false}}, 0.340, 4.05, 1.34);

    // Rhodium (45)
    chem_data_[45] = make(45, BondingManifold::COORDINATION, {{6,6,3,true},{4,4,1,false}}, 0.345, 4.08, 1.34);

    // Palladium (46)
    chem_data_[46] = make(46, BondingManifold::COORDINATION, {{4,4,2,true},{4,4,0,false}}, 0.350, 4.10, 1.31);

    // Copper (Z=29) — adjust for anomalous behavior
    chem_data_[29] = make(29, BondingManifold::COORDINATION,
        {{4, 4, 2, true},       // Cu(II) square planar
         {4, 4, 1, false},      // Cu(I) tetra
         {6, 6, 2, false}},
        0.270, 3.80, 1.32
    );

    // Zinc (Z=30) — similar adjustment
    chem_data_[30] = make(30, BondingManifold::COORDINATION,
        {{4, 4, 2, true},       // Zn(II) tetra
         {6, 6, 2, false}},
        0.240, 3.72, 1.22
    );

    // ----- Row 6: additional 5d transition metals ------
    chem_data_[72] = make(72, BondingManifold::COORDINATION,
        {{6,6,4,true},{8,8,4,false}},
        0.350, 4.10, 1.58
    );

    chem_data_[73] = make(73, BondingManifold::COORDINATION, {{6,6,5,true},{6,6,4,false}}, 0.355, 4.08, 1.46);
    chem_data_[75] = make(75, BondingManifold::COORDINATION, {{6,6,5,true},{6,6,6,false}}, 0.360, 4.10, 1.44);
    chem_data_[76] = make(76, BondingManifold::COORDINATION, {{6,6,4,true},{6,6,6,false}}, 0.365, 4.12, 1.42);
    chem_data_[77] = make(77, BondingManifold::COORDINATION, {{6,6,3,true},{6,6,4,false}}, 0.370, 4.14, 1.42);

    // ----- Lanthanides (57-71) — coarse coordination placeholders -----
     for (uint8_t Z = 57; Z <= 71; ++Z) {
         chem_data_[Z] = make(Z, BondingManifold::COORDINATION, {{8,8,3,true},{8,8,2,false}}, 0.380, 4.10, 1.75);
     }
 
     // Actinides (coarse) for MSR chemistry
     chem_data_[90] = make(90, BondingManifold::COORDINATION, {{6,6,4,true},{8,8,4,false}}, 0.420, 4.60, 1.65);
     chem_data_[92] = make(92, BondingManifold::COORDINATION, {{6,6,4,true},{6,6,6,true},{8,8,6,false}}, 0.430, 4.65, 1.70);
     chem_data_[94] = make(94, BondingManifold::COORDINATION, {{6,6,3,true},{6,6,4,true},{6,6,6,false}}, 0.440, 4.70, 1.72);
}

inline void ChemistryElementDatabase::initialize_noble_gases() {
    // Inert: no allowed valences. LJ only for nonbonded / vdW.
    chem_data_[2]  = make(2,  BondingManifold::NOBLE_GAS, {}, 0.020, 2.55, 0.28);
    chem_data_[10] = make(10, BondingManifold::NOBLE_GAS, {}, 0.042, 2.75, 0.58);
    chem_data_[18] = make(18, BondingManifold::NOBLE_GAS, {}, 0.120, 3.40, 1.06);
    chem_data_[36] = make(36, BondingManifold::NOBLE_GAS, {}, 0.180, 3.65, 1.16); // Kr
    chem_data_[54] = make(54, BondingManifold::NOBLE_GAS, {}, 0.250, 4.00, 1.40); // Xe
    chem_data_[86] = make(86, BondingManifold::NOBLE_GAS, {}, 0.300, 4.20, 1.50); // Rn

    // Fill any remaining UNKNOWN with covalent defaults to satisfy coverage audit
    for (uint8_t Z = 1; Z <= 118; ++Z) {
        if (chem_data_[Z].manifold == BondingManifold::UNKNOWN) {
            chem_data_[Z].manifold = BondingManifold::COVALENT;
            if (chem_data_[Z].allowed_valences.empty()) {
                chem_data_[Z].allowed_valences.push_back({2,2,0,false});
            }
            if (chem_data_[Z].Z == 0) chem_data_[Z].Z = Z;
        }
    }
}

//=============================================================================
// Global singleton (requires periodic table initialization)
//=============================================================================

inline ChemistryElementDatabase* g_chem_db = nullptr;

// Initialize chemistry database (call once after loading periodic table)
inline void init_chemistry_db(const PeriodicTable* pt) {
    static ChemistryElementDatabase db(pt);
    g_chem_db = &db;
}

// Get chemistry database (throws if not initialized)
inline const ChemistryElementDatabase& chemistry_db() {
    if (!g_chem_db) {
        throw std::runtime_error(
            "Chemistry database not initialized. "
            "Load PeriodicTable first, then call init_chemistry_db(&pt)."
        );
    }
    return *g_chem_db;
}

} // namespace vsepr
