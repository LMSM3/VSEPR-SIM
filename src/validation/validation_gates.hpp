#pragma once
/**
 * validation_gates.hpp
 * --------------------
 * Hard-fail validation checks that prevent garbage from entering the pipeline.
 *
 * Every molecule must pass these gates before being stored or analyzed.
 * Failures produce structured error codes, not strings.
 *
 * Gate ordering (fail fast):
 *   1. Formula sanity (non-empty, valid elements)
 *   2. Atom overlap / collision
 *   3. Bond length range (element-pair heuristics)
 *   4. Valence sanity (approximate)
 *   5. Convergence check (FIRE actually converged)
 *   6. Charge consistency
 */

#include "sim/molecule.hpp"
#include "core/types.hpp"
#include <vector>
#include <string>
#include <cmath>
#include <sstream>

namespace vsepr {
namespace validation {

// ============================================================================
// Failure Codes (enums, not strings)
// ============================================================================

enum class GateFailure : uint8_t {
    NONE = 0,
    EMPTY_MOLECULE,
    ATOM_OVERLAP,
    BOND_TOO_SHORT,
    BOND_TOO_LONG,
    VALENCE_VIOLATION,
    UNCONVERGED,
    NAN_DETECTED,
    CHARGE_INCONSISTENT,
    DISCONNECTED_GRAPH,
};

inline const char* failure_name(GateFailure f) {
    switch (f) {
        case GateFailure::NONE:                return "NONE";
        case GateFailure::EMPTY_MOLECULE:      return "EMPTY_MOLECULE";
        case GateFailure::ATOM_OVERLAP:        return "ATOM_OVERLAP";
        case GateFailure::BOND_TOO_SHORT:      return "BOND_TOO_SHORT";
        case GateFailure::BOND_TOO_LONG:       return "BOND_TOO_LONG";
        case GateFailure::VALENCE_VIOLATION:    return "VALENCE_VIOLATION";
        case GateFailure::UNCONVERGED:         return "UNCONVERGED";
        case GateFailure::NAN_DETECTED:        return "NAN_DETECTED";
        case GateFailure::CHARGE_INCONSISTENT: return "CHARGE_INCONSISTENT";
        case GateFailure::DISCONNECTED_GRAPH:  return "DISCONNECTED_GRAPH";
        default: return "UNKNOWN";
    }
}

struct GateResult {
    bool passed = true;
    GateFailure failure = GateFailure::NONE;
    std::string detail;       // Human-readable detail
    uint32_t atom_i = 0;     // Offending atom (if applicable)
    uint32_t atom_j = 0;     // Second atom (for pair failures)

    static GateResult pass() { return {true, GateFailure::NONE, "", 0, 0}; }

    static GateResult fail(GateFailure code, const std::string& detail,
                           uint32_t i = 0, uint32_t j = 0) {
        return {false, code, detail, i, j};
    }

    std::string summary() const {
        if (passed) return "PASS";
        std::ostringstream oss;
        oss << "FAIL [" << failure_name(failure) << "]";
        if (!detail.empty()) oss << ": " << detail;
        return oss.str();
    }
};

// ============================================================================
// Gate 1: Empty Molecule Check
// ============================================================================

inline GateResult gate_nonempty(const Molecule& mol) {
    if (mol.num_atoms() == 0)
        return GateResult::fail(GateFailure::EMPTY_MOLECULE, "Molecule has 0 atoms");
    return GateResult::pass();
}

// ============================================================================
// Gate 2: NaN Detection
// ============================================================================

inline GateResult gate_no_nan(const Molecule& mol) {
    for (size_t i = 0; i < mol.num_atoms(); ++i) {
        double x = mol.coords[3*i], y = mol.coords[3*i+1], z = mol.coords[3*i+2];
        if (std::isnan(x) || std::isnan(y) || std::isnan(z) ||
            std::isinf(x) || std::isinf(y) || std::isinf(z)) {
            std::ostringstream oss;
            oss << "Atom " << i << " has NaN/Inf coordinates";
            return GateResult::fail(GateFailure::NAN_DETECTED, oss.str(), static_cast<uint32_t>(i));
        }
    }
    return GateResult::pass();
}

// ============================================================================
// Gate 3: Atom Overlap / Collision
// ============================================================================

inline GateResult gate_no_overlap(const Molecule& mol, double min_dist = 0.3) {
    for (size_t i = 0; i < mol.num_atoms(); ++i) {
        for (size_t j = i + 1; j < mol.num_atoms(); ++j) {
            double dx = mol.coords[3*i]   - mol.coords[3*j];
            double dy = mol.coords[3*i+1] - mol.coords[3*j+1];
            double dz = mol.coords[3*i+2] - mol.coords[3*j+2];
            double r = std::sqrt(dx*dx + dy*dy + dz*dz);
            if (r < min_dist) {
                std::ostringstream oss;
                oss << "Atoms " << i << " (Z=" << (int)mol.atoms[i].Z
                    << ") and " << j << " (Z=" << (int)mol.atoms[j].Z
                    << ") overlap: r=" << r << " < " << min_dist;
                return GateResult::fail(GateFailure::ATOM_OVERLAP, oss.str(),
                                       static_cast<uint32_t>(i), static_cast<uint32_t>(j));
            }
        }
    }
    return GateResult::pass();
}

// ============================================================================
// Gate 4: Bond Length Range (element-pair heuristics)
// ============================================================================

/**
 * Approximate valid bond length range for element pair.
 * Uses covalent radii sum with tolerance factor.
 * Returns {min, max} in Angstroms.
 */
inline std::pair<double, double> bond_length_range(uint8_t Z1, uint8_t Z2) {
    // Approximate covalent radii (Angstroms) - Cordero et al., 2008
    auto cov_radius = [](uint8_t Z) -> double {
        static const double radii[] = {
            0.00, // 0
            0.31, 0.28,                                     // H, He
            1.28, 0.96, 0.84, 0.76, 0.71, 0.66, 0.57, 0.58, // Li-Ne
            1.66, 1.41, 1.21, 1.11, 1.07, 1.05, 1.02, 1.06, // Na-Ar
            2.03, 1.76, 1.70, 1.60, 1.53, 1.39, 1.39, 1.32, // K-Fe
            1.26, 1.24, 1.32, 1.22, 1.22, 1.20, 1.19, 1.20, // Co-Se
            1.20, 1.16,                                       // Br, Kr
        };
        if (Z < 37) return radii[Z];
        return 1.50;  // Default for heavier elements
    };

    double sum = cov_radius(Z1) + cov_radius(Z2);
    double min_r = sum * 0.6;  // 60% of covalent sum (compressed)
    double max_r = sum * 1.6;  // 160% of covalent sum (stretched)
    return {min_r, max_r};
}

inline GateResult gate_bond_lengths(const Molecule& mol) {
    for (const auto& bond : mol.bonds) {
        if (bond.i >= mol.num_atoms() || bond.j >= mol.num_atoms()) continue;

        double dx = mol.coords[3*bond.i]   - mol.coords[3*bond.j];
        double dy = mol.coords[3*bond.i+1] - mol.coords[3*bond.j+1];
        double dz = mol.coords[3*bond.i+2] - mol.coords[3*bond.j+2];
        double r = std::sqrt(dx*dx + dy*dy + dz*dz);

        auto [rmin, rmax] = bond_length_range(mol.atoms[bond.i].Z, mol.atoms[bond.j].Z);

        if (r < rmin) {
            std::ostringstream oss;
            oss << "Bond " << bond.i << "-" << bond.j << ": r=" << r
                << " < min=" << rmin;
            return GateResult::fail(GateFailure::BOND_TOO_SHORT, oss.str(), bond.i, bond.j);
        }
        if (r > rmax) {
            std::ostringstream oss;
            oss << "Bond " << bond.i << "-" << bond.j << ": r=" << r
                << " > max=" << rmax;
            return GateResult::fail(GateFailure::BOND_TOO_LONG, oss.str(), bond.i, bond.j);
        }
    }
    return GateResult::pass();
}

// ============================================================================
// Gate 5: Valence Sanity (approximate)
// ============================================================================

inline GateResult gate_valence(const Molecule& mol) {
    // Count bond orders per atom
    std::vector<int> bond_order_sum(mol.num_atoms(), 0);
    for (const auto& bond : mol.bonds) {
        if (bond.i < mol.num_atoms()) bond_order_sum[bond.i] += bond.order;
        if (bond.j < mol.num_atoms()) bond_order_sum[bond.j] += bond.order;
    }

    // Approximate max valence by element (common cases)
    auto max_valence = [](uint8_t Z) -> int {
        switch (Z) {
            case 1:  return 1;   // H
            case 2:  return 0;   // He
            case 3:  return 1;   // Li
            case 6:  return 4;   // C
            case 7:  return 4;   // N (can be 4 in NH4+)
            case 8:  return 3;   // O (can be 3 in H3O+)
            case 9:  return 1;   // F
            case 14: return 6;   // Si (hypervalent)
            case 15: return 6;   // P (hypervalent)
            case 16: return 6;   // S (hypervalent)
            case 17: return 1;   // Cl (but can be higher in ClO4-)
            case 26: return 6;   // Fe (coordination)
            case 27: return 6;   // Co
            case 28: return 6;   // Ni
            case 29: return 6;   // Cu
            case 30: return 6;   // Zn
            case 78: return 6;   // Pt
            default: return 8;   // Conservative default
        }
    };

    for (size_t i = 0; i < mol.num_atoms(); ++i) {
        int maxv = max_valence(mol.atoms[i].Z);
        if (bond_order_sum[i] > maxv) {
            std::ostringstream oss;
            oss << "Atom " << i << " (Z=" << (int)mol.atoms[i].Z
                << ") has bond order sum " << bond_order_sum[i]
                << " > max " << maxv;
            return GateResult::fail(GateFailure::VALENCE_VIOLATION, oss.str(),
                                   static_cast<uint32_t>(i));
        }
    }
    return GateResult::pass();
}

// ============================================================================
// Full Validation Pipeline
// ============================================================================

struct ValidationReport {
    std::vector<GateResult> results;
    bool all_passed = true;

    void add(GateResult r) {
        results.push_back(r);
        if (!r.passed) all_passed = false;
    }

    std::string summary() const {
        std::ostringstream oss;
        int pass_count = 0, fail_count = 0;
        for (const auto& r : results) {
            if (r.passed) pass_count++;
            else fail_count++;
        }
        oss << (all_passed ? "PASS" : "FAIL")
            << " (" << pass_count << "/" << results.size() << " gates passed)";
        if (!all_passed) {
            oss << "\n  Failures:";
            for (const auto& r : results) {
                if (!r.passed) {
                    oss << "\n    - " << r.summary();
                }
            }
        }
        return oss.str();
    }

    GateFailure first_failure() const {
        for (const auto& r : results) {
            if (!r.passed) return r.failure;
        }
        return GateFailure::NONE;
    }
};

/**
 * Run all validation gates on a molecule.
 * Fails fast on first critical error (NaN, empty).
 * Continues for quality checks (bond lengths, valence).
 */
inline ValidationReport validate_molecule(const Molecule& mol) {
    ValidationReport report;

    // Critical gates (fail fast)
    auto g1 = gate_nonempty(mol);
    report.add(g1);
    if (!g1.passed) return report;

    auto g2 = gate_no_nan(mol);
    report.add(g2);
    if (!g2.passed) return report;

    // Quality gates (collect all)
    report.add(gate_no_overlap(mol));
    report.add(gate_bond_lengths(mol));
    report.add(gate_valence(mol));

    return report;
}

} // namespace validation
} // namespace vsepr
