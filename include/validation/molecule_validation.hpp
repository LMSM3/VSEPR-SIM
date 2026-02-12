/**
 * molecule_validation.hpp
 * =======================
 * Comprehensive validation framework for molecular structures
 * 
 * Implements debugging guidelines:
 * 1. Single-element debugging (canonicalization, valence, geometry)
 * 2. Multi-element debugging (bond plausibility, electron accounting, noble gas gating)
 * 
 * Design:
 * - Each check returns ValidationResult with pass/fail + reason code
 * - No silent failures - every rejection is logged
 * - Deterministic and reproducible
 * - Fast heuristics for screening, not full quantum
 */

#pragma once

#include "core/chemistry.hpp"
#include "core/types.hpp"
#include "sim/molecule.hpp"
#include <string>
#include <vector>
#include <unordered_map>
#include <cmath>
#include <sstream>

namespace vsepr {
namespace validation {

//=============================================================================
// Validation Result Types
//=============================================================================

enum class ValidationLevel {
    CRITICAL,  // Must fix immediately
    WARNING,   // Should investigate
    INFO       // Nice to know
};

struct ValidationResult {
    bool passed = true;
    ValidationLevel level = ValidationLevel::INFO;
    std::string reason_code;
    std::string message;
    
    // Helper constructors
    static ValidationResult Pass() {
        return ValidationResult{true, ValidationLevel::INFO, "", ""};
    }
    
    static ValidationResult Fail(const std::string& code, const std::string& msg, 
                                  ValidationLevel lvl = ValidationLevel::CRITICAL) {
        return ValidationResult{false, lvl, code, msg};
    }
};

struct ValidationReport {
    std::vector<ValidationResult> results;
    int critical_count = 0;
    int warning_count = 0;
    int info_count = 0;
    
    void add(const ValidationResult& result) {
        results.push_back(result);
        if (!result.passed) {
            switch (result.level) {
                case ValidationLevel::CRITICAL: critical_count++; break;
                case ValidationLevel::WARNING: warning_count++; break;
                case ValidationLevel::INFO: info_count++; break;
            }
        }
    }
    
    bool passed() const { return critical_count == 0; }
    
    std::string summary() const {
        std::stringstream ss;
        ss << "Validation: ";
        if (passed()) {
            ss << "✓ PASS";
        } else {
            ss << "✗ FAIL";
        }
        ss << " (Critical: " << critical_count 
           << ", Warnings: " << warning_count 
           << ", Info: " << info_count << ")";
        return ss.str();
    }
};

//=============================================================================
// 1A. Canonicalization and Bookkeeping
//=============================================================================

/**
 * Check symbol parsing correctness
 * "As2" ≠ "AS2" ≠ "As₂"
 */
inline ValidationResult validate_symbol_case(const std::string& symbol) {
    if (symbol.empty()) {
        return ValidationResult::Fail("SYM_EMPTY", "Empty element symbol");
    }
    
    // First character must be uppercase
    if (!std::isupper(symbol[0])) {
        return ValidationResult::Fail("SYM_CASE", 
            "Element symbol must start with uppercase: " + symbol);
    }
    
    // Subsequent characters must be lowercase
    for (size_t i = 1; i < symbol.length(); ++i) {
        if (!std::islower(symbol[i])) {
            return ValidationResult::Fail("SYM_CASE",
                "Element symbol subsequent chars must be lowercase: " + symbol);
        }
    }
    
    return ValidationResult::Pass();
}

/**
 * Count conservation check
 * Input formula atom counts must equal output structure
 */
inline ValidationResult validate_atom_count_conservation(
    const std::unordered_map<std::string, int>& formula_counts,
    const std::vector<Atom>& atoms,
    const PeriodicTable& ptable)
{
    std::unordered_map<std::string, int> actual_counts;
    
    for (const auto& atom : atoms) {
        std::string symbol = ptable.get_symbol(atom.Z);
        actual_counts[symbol]++;
    }
    
    // Check each element
    for (const auto& [symbol, expected] : formula_counts) {
        int actual = actual_counts[symbol];
        if (actual != expected) {
            return ValidationResult::Fail("COUNT_MISMATCH",
                "Element " + symbol + ": expected " + std::to_string(expected) +
                " atoms, got " + std::to_string(actual));
        }
    }
    
    // Check for unexpected elements
    for (const auto& [symbol, count] : actual_counts) {
        if (formula_counts.find(symbol) == formula_counts.end()) {
            return ValidationResult::Fail("EXTRA_ELEMENT",
                "Unexpected element in structure: " + symbol);
        }
    }
    
    return ValidationResult::Pass();
}

/**
 * Charge policy validation
 * Default is neutral - must be logged explicitly if non-neutral
 */
inline ValidationResult validate_charge_policy(
    int formal_charge,
    bool explicit_charge_specified)
{
    if (formal_charge == 0) {
        return ValidationResult::Pass();
    }
    
    if (!explicit_charge_specified) {
        return ValidationResult::Fail("IMPLICIT_CHARGE",
            "Non-zero formal charge (" + std::to_string(formal_charge) +
            ") without explicit specification",
            ValidationLevel::WARNING);
    }
    
    return ValidationResult::Pass();
}

//=============================================================================
// 1B. Allowed Valence / Coordination Envelope
//=============================================================================

struct ValenceEnvelope {
    std::vector<int> allowed_oxidation_states;
    int max_coordination_typical;
    int max_coordination_hypervalent;
    std::vector<int> allowed_bond_orders;  // e.g., {1} for most halogens
};

/**
 * Get valence envelope for element
 */
inline ValenceEnvelope get_valence_envelope(uint8_t Z) {
    ValenceEnvelope env;
    
    switch (Z) {
        case 1:  // H
            env.allowed_oxidation_states = {-1, 0, 1};
            env.max_coordination_typical = 1;
            env.max_coordination_hypervalent = 1;
            env.allowed_bond_orders = {1};
            break;
            
        case 6:  // C
            env.allowed_oxidation_states = {-4, -3, -2, -1, 0, 1, 2, 3, 4};
            env.max_coordination_typical = 4;
            env.max_coordination_hypervalent = 4;
            env.allowed_bond_orders = {1, 2, 3};
            break;
            
        case 7:  // N
            env.allowed_oxidation_states = {-3, -2, -1, 0, 1, 2, 3, 4, 5};
            env.max_coordination_typical = 3;
            env.max_coordination_hypervalent = 4;  // NO2, etc.
            env.allowed_bond_orders = {1, 2, 3};
            break;
            
        case 8:  // O
            env.allowed_oxidation_states = {-2, -1, 0};
            env.max_coordination_typical = 2;
            env.max_coordination_hypervalent = 3;  // H3O+
            env.allowed_bond_orders = {1, 2};
            break;
            
        case 9:  // F
            env.allowed_oxidation_states = {-1, 0};
            env.max_coordination_typical = 1;
            env.max_coordination_hypervalent = 1;
            env.allowed_bond_orders = {1};
            break;
            
        case 16:  // S
            env.allowed_oxidation_states = {-2, 0, 2, 4, 6};
            env.max_coordination_typical = 2;
            env.max_coordination_hypervalent = 6;  // SF6
            env.allowed_bond_orders = {1, 2};
            break;
            
        case 17:  // Cl
            env.allowed_oxidation_states = {-1, 0, 1, 3, 5, 7};
            env.max_coordination_typical = 1;
            env.max_coordination_hypervalent = 7;  // ClF7 theoretical
            env.allowed_bond_orders = {1, 2};
            break;
            
        case 15:  // P
            env.allowed_oxidation_states = {-3, 0, 3, 5};
            env.max_coordination_typical = 3;
            env.max_coordination_hypervalent = 5;  // PCl5, PF5
            env.allowed_bond_orders = {1, 2, 3};
            break;
            
        case 54:  // Xe
            env.allowed_oxidation_states = {0, 2, 4, 6, 8};
            env.max_coordination_typical = 0;
            env.max_coordination_hypervalent = 8;  // XeF8 theoretical
            env.allowed_bond_orders = {1, 2};
            break;
            
        case 36:  // Kr
            env.allowed_oxidation_states = {0, 2};
            env.max_coordination_typical = 0;
            env.max_coordination_hypervalent = 2;  // KrF2
            env.allowed_bond_orders = {1};
            break;
            
        default:
            // Default conservative envelope
            env.allowed_oxidation_states = {0};
            env.max_coordination_typical = 4;
            env.max_coordination_hypervalent = 6;
            env.allowed_bond_orders = {1, 2, 3};
    }
    
    return env;
}

/**
 * Validate coordination number against envelope
 */
inline ValidationResult validate_coordination(uint8_t Z, int coordination) {
    ValenceEnvelope env = get_valence_envelope(Z);
    
    if (coordination > env.max_coordination_hypervalent) {
        return ValidationResult::Fail("COORD_EXCEED",
            "Coordination " + std::to_string(coordination) +
            " exceeds max for Z=" + std::to_string(Z) +
            " (max=" + std::to_string(env.max_coordination_hypervalent) + ")");
    }
    
    if (coordination > env.max_coordination_typical) {
        return ValidationResult{true, ValidationLevel::WARNING, "COORD_HYPERVALENT",
            "Hypervalent coordination " + std::to_string(coordination) +
            " for Z=" + std::to_string(Z)};
    }
    
    return ValidationResult::Pass();
}

/**
 * Validate bond orders against envelope
 */
inline ValidationResult validate_bond_orders(
    uint8_t Z,
    const std::vector<uint8_t>& bond_orders)
{
    ValenceEnvelope env = get_valence_envelope(Z);
    
    for (uint8_t order : bond_orders) {
        bool allowed = false;
        for (int allowed_order : env.allowed_bond_orders) {
            if (order == allowed_order) {
                allowed = true;
                break;
            }
        }
        
        if (!allowed) {
            return ValidationResult::Fail("BOND_ORDER_INVALID",
                "Bond order " + std::to_string(order) +
                " not allowed for Z=" + std::to_string(Z));
        }
    }
    
    return ValidationResult::Pass();
}

//=============================================================================
// 1C. Geometry Sanity for Single Atom Context
//=============================================================================

/**
 * Minimum interatomic distance check
 * Reject if any distance < hard-core threshold
 */
inline ValidationResult validate_minimum_distances(
    const Molecule& mol,
    const PeriodicTable& ptable,
    double hard_core_factor = 0.5)  // 50% of sum of covalent radii
{
    for (size_t i = 0; i < mol.num_atoms(); ++i) {
        double xi, yi, zi;
        mol.get_position(i, xi, yi, zi);
        
        for (size_t j = i + 1; j < mol.num_atoms(); ++j) {
            double xj, yj, zj;
            mol.get_position(j, xj, yj, zj);
            
            double dx = xi - xj;
            double dy = yi - yj;
            double dz = zi - zj;
            double dist = std::sqrt(dx*dx + dy*dy + dz*dz);
            
            // Get covalent radii
            double r_i = ptable.get_covalent_radius(mol.atoms[i].Z);
            double r_j = ptable.get_covalent_radius(mol.atoms[j].Z);
            double min_dist = (r_i + r_j) * hard_core_factor;
            
            if (dist < min_dist) {
                return ValidationResult::Fail("DIST_TOO_CLOSE",
                    "Atoms " + std::to_string(i) + " and " + std::to_string(j) +
                    " too close: " + std::to_string(dist) + " Å < " +
                    std::to_string(min_dist) + " Å (hard-core limit)");
            }
        }
    }
    
    return ValidationResult::Pass();
}

/**
 * Coordination number explosion check
 * Reject if single atom has absurd coordination
 */
inline ValidationResult validate_coordination_numbers(const Molecule& mol) {
    for (size_t i = 0; i < mol.num_atoms(); ++i) {
        int coordination = 0;
        
        // Count bonds
        for (const auto& bond : mol.bonds) {
            if (bond.i == i || bond.j == i) {
                coordination++;
            }
        }
        
        auto result = validate_coordination(mol.atoms[i].Z, coordination);
        if (!result.passed) {
            return result;
        }
    }
    
    return ValidationResult::Pass();
}

/**
 * Bond spaghetti prevention
 * Check for unreasonable over-bonding patterns
 */
inline ValidationResult validate_no_bond_spaghetti(const Molecule& mol) {
    // Check bond triangle inequality violations
    // If atoms i-j bonded and j-k bonded, then i-k distance should be reasonable
    
    for (const auto& bond1 : mol.bonds) {
        for (const auto& bond2 : mol.bonds) {
            if (bond1.j == bond2.i) {
                // Shared atom: bond1.i - bond1.j(=bond2.i) - bond2.j
                uint32_t a = bond1.i;
                uint32_t b = bond1.j;  // shared
                uint32_t c = bond2.j;
                
                if (a == c) continue;  // Same atoms
                
                double xa, ya, za, xb, yb, zb, xc, yc, zc;
                mol.get_position(a, xa, ya, za);
                mol.get_position(b, xb, yb, zb);
                mol.get_position(c, xc, yc, zc);
                
                double d_ab = std::sqrt((xa-xb)*(xa-xb) + (ya-yb)*(ya-yb) + (za-zb)*(za-zb));
                double d_bc = std::sqrt((xb-xc)*(xb-xc) + (yb-yc)*(yb-yc) + (zb-zc)*(zb-zc));
                double d_ac = std::sqrt((xa-xc)*(xa-xc) + (ya-yc)*(ya-yc) + (za-zc)*(za-zc));
                
                // Triangle inequality: d_ac should be <= d_ab + d_bc
                // But also check if angle is absurdly collapsed
                if (d_ac > (d_ab + d_bc) * 1.1) {
                    return ValidationResult::Fail("TRIANGLE_VIOLATION",
                        "Triangle inequality violated for atoms " +
                        std::to_string(a) + "-" + std::to_string(b) + "-" + std::to_string(c),
                        ValidationLevel::WARNING);
                }
            }
        }
    }
    
    return ValidationResult::Pass();
}

//=============================================================================
// 1D. Determinism and Reproducibility
//=============================================================================

struct BuildMetadata {
    std::string build_version = "2.3.1";
    uint64_t random_seed = 0;
    std::string constraint_version = "validation_v1";
    double floating_point_tolerance = 1e-10;
    
    std::string to_string() const {
        std::stringstream ss;
        ss << "Build: " << build_version
           << ", Seed: " << random_seed
           << ", Constraints: " << constraint_version
           << ", FP_tol: " << floating_point_tolerance;
        return ss.str();
    }
};

/**
 * Validate determinism - check if results are reproducible
 */
inline ValidationResult validate_determinism(
    const BuildMetadata& meta1,
    const BuildMetadata& meta2)
{
    if (meta1.random_seed != meta2.random_seed) {
        return ValidationResult{true, ValidationLevel::INFO, "SEED_DIFF",
            "Different random seeds: results expected to differ"};
    }
    
    if (meta1.constraint_version != meta2.constraint_version) {
        return ValidationResult::Fail("CONSTRAINT_VERSION_MISMATCH",
            "Constraint versions differ: " + meta1.constraint_version +
            " vs " + meta2.constraint_version,
            ValidationLevel::WARNING);
    }
    
    return ValidationResult::Pass();
}

//=============================================================================
// 2A. Pairwise Bond Plausibility Matrix
//=============================================================================

struct BondPlausibility {
    std::vector<int> typical_orders;  // e.g., {1, 2} for C-O
    double min_distance_A;
    double max_distance_A;
    bool rare_but_possible = false;
};

/**
 * Get bond plausibility between two elements
 */
inline BondPlausibility get_bond_plausibility(uint8_t Z1, uint8_t Z2) {
    BondPlausibility plaus;
    
    // C-H bond
    if ((Z1 == 6 && Z2 == 1) || (Z1 == 1 && Z2 == 6)) {
        plaus.typical_orders = {1};
        plaus.min_distance_A = 1.0;
        plaus.max_distance_A = 1.2;
    }
    // C-C bond
    else if (Z1 == 6 && Z2 == 6) {
        plaus.typical_orders = {1, 2, 3};
        plaus.min_distance_A = 1.2;
        plaus.max_distance_A = 1.6;
    }
    // C=O bond
    else if ((Z1 == 6 && Z2 == 8) || (Z1 == 8 && Z2 == 6)) {
        plaus.typical_orders = {1, 2};
        plaus.min_distance_A = 1.1;
        plaus.max_distance_A = 1.5;
    }
    // Xe-F bond (rare but valid)
    else if ((Z1 == 54 && Z2 == 9) || (Z1 == 9 && Z2 == 54)) {
        plaus.typical_orders = {1};
        plaus.min_distance_A = 1.8;
        plaus.max_distance_A = 2.2;
        plaus.rare_but_possible = true;
    }
    // Kr-F bond (very rare)
    else if ((Z1 == 36 && Z2 == 9) || (Z1 == 9 && Z2 == 36)) {
        plaus.typical_orders = {1};
        plaus.min_distance_A = 1.8;
        plaus.max_distance_A = 2.0;
        plaus.rare_but_possible = true;
    }
    // Default fallback
    else {
        plaus.typical_orders = {1};
        plaus.min_distance_A = 0.8;
        plaus.max_distance_A = 3.0;
    }
    
    return plaus;
}

/**
 * Validate bond against plausibility matrix
 */
inline ValidationResult validate_bond_plausibility(
    const Bond& bond,
    uint8_t Z1,
    uint8_t Z2,
    double distance_A)
{
    BondPlausibility plaus = get_bond_plausibility(Z1, Z2);
    
    // Check bond order
    bool order_ok = false;
    for (int allowed : plaus.typical_orders) {
        if (bond.order == allowed) {
            order_ok = true;
            break;
        }
    }
    
    if (!order_ok) {
        return ValidationResult::Fail("BOND_ORDER_IMPLAUSIBLE",
            "Bond order " + std::to_string(bond.order) +
            " unusual for Z=" + std::to_string(Z1) +
            "-Z=" + std::to_string(Z2));
    }
    
    // Check distance
    if (distance_A < plaus.min_distance_A || distance_A > plaus.max_distance_A) {
        ValidationLevel level = plaus.rare_but_possible ? 
            ValidationLevel::WARNING : ValidationLevel::CRITICAL;
        
        return ValidationResult::Fail("BOND_DIST_IMPLAUSIBLE",
            "Bond distance " + std::to_string(distance_A) + " Å outside range [" +
            std::to_string(plaus.min_distance_A) + ", " +
            std::to_string(plaus.max_distance_A) + "] for Z=" +
            std::to_string(Z1) + "-Z=" + std::to_string(Z2),
            level);
    }
    
    if (plaus.rare_but_possible) {
        return ValidationResult{true, ValidationLevel::INFO, "BOND_RARE",
            "Bond Z=" + std::to_string(Z1) + "-Z=" + std::to_string(Z2) +
            " is rare but possible"};
    }
    
    return ValidationResult::Pass();
}

//=============================================================================
// 2B. Electron Accounting (Fast Heuristics)
//=============================================================================

/**
 * Calculate total valence electrons for molecule
 */
inline int calculate_valence_electrons(
    const std::vector<Atom>& atoms,
    const PeriodicTable& ptable,
    int formal_charge)
{
    int total = 0;
    for (const auto& atom : atoms) {
        int group = ptable.get_group(atom.Z);
        int valence = (group <= 2) ? group : (group - 10);  // Simplified
        if (valence < 0) valence = 0;  // Transition metals approximation
        total += valence;
    }
    return total - formal_charge;  // Remove electrons for positive charge
}

/**
 * Parity check: odd electron totals indicate radicals
 */
inline ValidationResult validate_electron_parity(
    int total_electrons,
    bool radical_allowed)
{
    if (total_electrons % 2 == 1) {
        if (!radical_allowed) {
            return ValidationResult::Fail("ODD_ELECTRONS",
                "Odd number of electrons (" + std::to_string(total_electrons) +
                ") without radical flag",
                ValidationLevel::WARNING);
        } else {
            return ValidationResult{true, ValidationLevel::INFO, "RADICAL",
                "Radical species with " + std::to_string(total_electrons) + " electrons"};
        }
    }
    
    return ValidationResult::Pass();
}

/**
 * Formal charge distribution sanity
 */
inline ValidationResult validate_formal_charges(
    const std::vector<int>& formal_charges,
    int expected_total_charge)
{
    int sum = 0;
    for (int fc : formal_charges) {
        sum += fc;
    }
    
    if (sum != expected_total_charge) {
        return ValidationResult::Fail("CHARGE_SUM_MISMATCH",
            "Formal charges sum to " + std::to_string(sum) +
            " but expected " + std::to_string(expected_total_charge));
    }
    
    return ValidationResult::Pass();
}

//=============================================================================
// 2C. Noble Gas Gating (Xe/Kr)
//=============================================================================

/**
 * Special validation for noble gas compounds
 */
inline ValidationResult validate_noble_gas_compound(
    uint8_t Z_noble,
    const std::vector<uint8_t>& partner_elements,
    double convergence_force,
    double strain_energy)
{
    // Only Xe and Kr form compounds
    if (Z_noble != 54 && Z_noble != 36) {
        return ValidationResult::Fail("NOBLE_GAS_INVALID",
            "Noble gas Z=" + std::to_string(Z_noble) + " does not form compounds");
    }
    
    // Check bonding partners - must be highly electronegative
    for (uint8_t Z_partner : partner_elements) {
        if (Z_partner != 8 && Z_partner != 9 && Z_partner != 17) {
            // Allow O, F, Cl only
            return ValidationResult::Fail("NOBLE_GAS_PARTNER_INVALID",
                "Noble gas Z=" + std::to_string(Z_noble) +
                " bonded to unusual partner Z=" + std::to_string(Z_partner) +
                " (expected O, F, or Cl)",
                ValidationLevel::WARNING);
        }
    }
    
    // Require clean convergence
    if (convergence_force > 0.01) {
        return ValidationResult::Fail("NOBLE_GAS_CONVERGENCE_POOR",
            "Noble gas compound has poor convergence (F_max = " +
            std::to_string(convergence_force) + " > 0.01)",
            ValidationLevel::WARNING);
    }
    
    // Check strain energy
    if (strain_energy > 10.0) {  // kcal/mol
        return ValidationResult::Fail("NOBLE_GAS_HIGH_STRAIN",
            "Noble gas compound has high strain energy (" +
            std::to_string(strain_energy) + " kcal/mol)",
            ValidationLevel::WARNING);
    }
    
    return ValidationResult::Pass();
}

//=============================================================================
// 2D. Optimization Integrity
//=============================================================================

struct OptimizationQuality {
    std::vector<double> energy_history;
    double final_max_force;
    int num_steps;
    bool converged;
};

/**
 * Validate optimization quality
 */
inline ValidationResult validate_optimization_quality(
    const OptimizationQuality& opt)
{
    // Check for monotonic-ish decrease
    if (opt.energy_history.size() >= 2) {
        int increases = 0;
        for (size_t i = 1; i < opt.energy_history.size(); ++i) {
            if (opt.energy_history[i] > opt.energy_history[i-1]) {
                increases++;
            }
        }
        
        double increase_fraction = static_cast<double>(increases) / 
                                   opt.energy_history.size();
        
        if (increase_fraction > 0.3) {
            return ValidationResult::Fail("OPT_NON_MONOTONIC",
                "Energy increased in " + std::to_string(increases) + "/" +
                std::to_string(opt.energy_history.size()) + " steps",
                ValidationLevel::WARNING);
        }
    }
    
    // Check final force
    if (opt.converged && opt.final_max_force > 0.1) {
        return ValidationResult::Fail("OPT_FORCE_TOO_HIGH",
            "Claimed convergence but F_max = " +
            std::to_string(opt.final_max_force) + " > 0.1",
            ValidationLevel::WARNING);
    }
    
    // Check step count
    if (opt.converged && opt.num_steps < 5) {
        return ValidationResult::Fail("OPT_TOO_FAST",
            "Converged in " + std::to_string(opt.num_steps) +
            " steps - likely numerical coincidence",
            ValidationLevel::WARNING);
    }
    
    if (opt.num_steps > 10000) {
        return ValidationResult::Fail("OPT_TOO_SLOW",
            "Optimization took " + std::to_string(opt.num_steps) +
            " steps - likely stuck",
            ValidationLevel::WARNING);
    }
    
    return ValidationResult::Pass();
}

//=============================================================================
// Master Validation Function
//=============================================================================

/**
 * Run all validation checks on a molecule
 */
inline ValidationReport validate_molecule(
    const Molecule& mol,
    const PeriodicTable& ptable,
    const BuildMetadata& metadata,
    const OptimizationQuality* opt_quality = nullptr,
    int formal_charge = 0,
    bool radical_allowed = false)
{
    ValidationReport report;
    
    // 1A. Canonicalization
    for (const auto& atom : mol.atoms) {
        std::string symbol = ptable.get_symbol(atom.Z);
        report.add(validate_symbol_case(symbol));
    }
    
    report.add(validate_charge_policy(formal_charge, formal_charge != 0));
    
    // 1B. Valence envelope
    report.add(validate_coordination_numbers(mol));
    
    // 1C. Geometry sanity
    report.add(validate_minimum_distances(mol, ptable));
    report.add(validate_no_bond_spaghetti(mol));
    
    // 2A. Bond plausibility
    for (const auto& bond : mol.bonds) {
        double xi, yi, zi, xj, yj, zj;
        mol.get_position(bond.i, xi, yi, zi);
        mol.get_position(bond.j, xj, yj, zj);
        
        double dx = xi - xj;
        double dy = yi - yj;
        double dz = zi - zj;
        double dist = std::sqrt(dx*dx + dy*dy + dz*dz);
        
        report.add(validate_bond_plausibility(
            bond,
            mol.atoms[bond.i].Z,
            mol.atoms[bond.j].Z,
            dist
        ));
    }
    
    // 2B. Electron accounting
    int total_electrons = calculate_valence_electrons(mol.atoms, ptable, formal_charge);
    report.add(validate_electron_parity(total_electrons, radical_allowed));
    
    // 2C. Noble gas gating
    for (size_t i = 0; i < mol.num_atoms(); ++i) {
        uint8_t Z = mol.atoms[i].Z;
        if (Z == 36 || Z == 54) {  // Kr or Xe
            std::vector<uint8_t> partners;
            for (const auto& bond : mol.bonds) {
                if (bond.i == i) {
                    partners.push_back(mol.atoms[bond.j].Z);
                } else if (bond.j == i) {
                    partners.push_back(mol.atoms[bond.i].Z);
                }
            }
            
            if (!partners.empty()) {
                double conv_force = opt_quality ? opt_quality->final_max_force : 0.0;
                double strain = 0.0;  // TODO: calculate from energy
                
                report.add(validate_noble_gas_compound(Z, partners, conv_force, strain));
            }
        }
    }
    
    // 2D. Optimization integrity
    if (opt_quality) {
        report.add(validate_optimization_quality(*opt_quality));
    }
    
    return report;
}

} // namespace validation
} // namespace vsepr
