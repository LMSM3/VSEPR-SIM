#pragma once
/**
 * crystal_metrics.hpp
 * -------------------
 * Crystal verification scorecard: deterministic metrics computed every time
 * a crystal is generated or loaded. No presets, no hardcoded outcomes —
 * all checks are computed from the raw lattice + basis data.
 *
 * Six metric categories:
 *   1. Identity      — stoichiometry, charge, volume, density
 *   2. Symmetry      — space group recovery, Wyckoff multiplicities
 *   3. Local geometry — CN, bond lengths, angles, polyhedra signatures
 *   4. Topology      — neighbor graph connectivity, ring statistics
 *   5. Reciprocal    — d-spacings, predicted XRD peak positions
 *   6. Relaxation    — phase preservation, lattice drift, stress
 *
 * Plus a verification scorecard for empirical comparison.
 */

#include "atomistic/crystal/lattice.hpp"
#include "atomistic/crystal/unit_cell.hpp"
#include "atomistic/core/state.hpp"
#include <vector>
#include <map>
#include <string>
#include <array>
#include <cstdint>

namespace atomistic {
namespace crystal {

// ============================================================================
// Physical Constants
// ============================================================================

constexpr double AVOGADRO = 6.02214076e23;  // mol⁻¹
constexpr double ANG3_TO_CM3 = 1.0e-24;    // Å³ → cm³

// ============================================================================
// 1. Identity Metrics
// ============================================================================

struct IdentityMetrics {
    // Stoichiometry
    std::map<uint32_t, int> element_counts;   // Z → count in cell
    std::string reduced_formula;              // e.g., "NaCl"
    int formula_units_Z;                      // Z = formula units per cell
    
    // Charge
    double net_charge;                        // Sum of formal charges (should ≈ 0)
    bool charge_neutral;                      // |net_charge| < tolerance
    
    // Volume
    double cell_volume;                       // V = |det(A)| in Å³
    
    // Density
    double molar_mass;                        // M = Σ(mass_i) for one formula unit (g/mol)
    double total_mass;                        // Σ(mass_i) for entire cell (g/mol)
    double density_gcc;                       // ρ = Z·M / (N_A · V) in g/cm³
};

IdentityMetrics compute_identity_metrics(const UnitCell& uc);
IdentityMetrics compute_identity_metrics(const State& s, const Lattice& lat);

// ============================================================================
// 2. Symmetry Metrics (lightweight, no full space-group finder)
// ============================================================================

struct SymmetryMetrics {
    // Site class analysis (simplified Wyckoff-like)
    int num_unique_sites;                     // Distinct coordination environments
    std::vector<int> site_multiplicities;     // How many atoms per site class
    
    // Lattice system detection
    std::string lattice_system;               // "cubic", "tetragonal", etc.
    bool is_cubic;
    bool is_tetragonal;
    bool is_orthorhombic;
    bool is_hexagonal;
    
    // Space group metadata (from UnitCell, not recomputed)
    int space_group_number;
    std::string space_group_symbol;
};

SymmetryMetrics compute_symmetry_metrics(const UnitCell& uc);

// ============================================================================
// 3. Local Geometry Metrics
// ============================================================================

struct BondStats {
    uint32_t type_i, type_j;      // Element pair (Z_i, Z_j)
    double mean_length;            // Mean bond length (Å)
    double min_length;             // Min bond length (Å)
    double max_length;             // Max bond length (Å)
    double std_dev;                // Standard deviation (Å)
    int count;                     // Number of bonds of this type
};

struct SiteGeometry {
    uint32_t atom_type;            // Z of this atom
    int coordination_number;       // CN from cutoff rule
    double mean_bond_length;       // Mean distance to neighbors
    double distortion_index;       // Polyhedron distortion (0 = ideal)
};

struct LocalGeometryMetrics {
    // Per-atom coordination
    std::vector<SiteGeometry> site_geometries;
    
    // CN distribution
    std::map<uint32_t, double> mean_CN_by_type;    // Z → mean CN
    std::map<uint32_t, int>    expected_CN_by_type; // Z → expected CN (if known)
    
    // Bond statistics per pair
    std::vector<BondStats> bond_statistics;
    
    // Aggregate
    double max_distortion;         // Max polyhedron distortion across all sites
    double rms_bond_error;         // RMS deviation from expected bond lengths (if ref given)
};

LocalGeometryMetrics compute_local_geometry(const UnitCell& uc, double cutoff = 3.5);
LocalGeometryMetrics compute_local_geometry(const State& s, const Lattice& lat, double cutoff = 3.5);

// ============================================================================
// 4. Topology Metrics
// ============================================================================

struct TopologyMetrics {
    // Neighbor graph
    int total_bonds;                              // Edge count in neighbor graph
    std::map<uint32_t, int> bonds_per_type;       // Z → total bonds for atoms of this type
    
    // Connectivity
    int num_connected_components;                  // Should be 1 for a crystal
    bool fully_connected;                          // All atoms reachable
    
    // Sublattice connectivity (per element type)
    std::map<uint32_t, bool> sublattice_connected; // Z → is sublattice connected?
    
    // Network fingerprint
    uint64_t topology_hash;                        // Weisfeiler-Lehman hash
};

TopologyMetrics compute_topology_metrics(const UnitCell& uc, double cutoff = 3.5);
TopologyMetrics compute_topology_metrics(const State& s, const Lattice& lat, double cutoff = 3.5);

// ============================================================================
// 5. Reciprocal Space Metrics
// ============================================================================

struct DSpacing {
    int h, k, l;           // Miller indices
    double d;              // d-spacing (Å)
    double two_theta;      // 2θ position (degrees, Cu Kα λ=1.5406 Å)
};

struct ReciprocalMetrics {
    std::vector<DSpacing> d_spacings;   // Sorted by d descending
    int num_peaks;                       // Number of unique (hkl) within range
    
    // Lattice parameter summary
    double a_star, b_star, c_star;       // Reciprocal lattice lengths (Å⁻¹)
};

ReciprocalMetrics compute_reciprocal_metrics(const Lattice& lat, 
                                              double two_theta_max = 90.0,
                                              double wavelength = 1.5406);

// ============================================================================
// 6. Relaxation Stability Metrics
// ============================================================================

struct RelaxationMetrics {
    bool topology_preserved;            // Same neighbor graph before/after
    double volume_drift;                // ΔV/V (fractional)
    double a_drift, b_drift, c_drift;   // Δa/a, Δb/b, Δc/c (fractional)
    double max_displacement;            // Max atom displacement (Å)
    double rms_displacement;            // RMS atom displacement (Å)
    double energy_per_atom;             // Final E/N
    bool converged;                     // FIRE converged within tolerance
};

RelaxationMetrics compute_relaxation_metrics(const UnitCell& uc_before,
                                              const State& s_after,
                                              const Lattice& lat_after);

// ============================================================================
// Verification Scorecard (empirical comparison)
// ============================================================================

struct ReferenceData {
    std::string formula;
    double a, b, c;                     // Expected lattice parameters (Å)
    double alpha, beta, gamma;          // Expected angles (degrees)
    int space_group;                    // Expected space group number
    int Z;                              // Formula units per cell
    double density_gcc;                 // Expected density (g/cm³)
    
    // Per-site CN expectations (Z → expected CN)
    std::map<uint32_t, int> expected_CN;
    
    // Expected bond lengths per pair (Z_i, Z_j) → (min, max) in Å
    std::map<std::pair<uint32_t,uint32_t>, std::pair<double,double>> expected_bonds;
    
    // Expected XRD peak positions (2θ, Cu Kα)
    std::vector<double> expected_peaks;
};

struct ScorecardEntry {
    std::string check_name;
    bool passed;
    double value;
    double expected;
    double error_pct;
    std::string note;
};

struct VerificationScorecard {
    std::string crystal_name;
    std::vector<ScorecardEntry> entries;
    int total_checks;
    int passed;
    int failed;
    double pass_rate;
    
    void add(const std::string& name, bool pass, double val, double expected,
             double err_pct = 0.0, const std::string& note = "");
    
    void print() const;
};

VerificationScorecard verify_against_reference(const UnitCell& uc,
                                                const ReferenceData& ref,
                                                double cutoff = 3.5);

// ============================================================================
// Full Metrics Bundle (computed automatically on crystal generation)
// ============================================================================

struct CrystalMetrics {
    IdentityMetrics identity;
    SymmetryMetrics symmetry;
    LocalGeometryMetrics geometry;
    TopologyMetrics topology;
    ReciprocalMetrics reciprocal;
    
    void print_summary() const;
};

CrystalMetrics compute_all_metrics(const UnitCell& uc, double cutoff = 3.5);

} // namespace crystal
} // namespace atomistic
