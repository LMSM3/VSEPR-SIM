#pragma once
/**
 * experiment_match.hpp
 * --------------------
 * Experimental data matching for the near-atomistic nanoparticle regime (4–6 nm).
 *
 * Compares internal simulation state against published experimental
 * observables for nanoparticle systems:
 *
 *   1. Geometric energy decomposition
 *      - Surface / bulk / edge / vertex energy partitioning
 *      - Cohesive energy per atom (eV or kcal/mol)
 *      - Surface energy per unit area (J/m² → kcal/mol/Å²)
 *
 *   2. Structural metrics
 *      - Coordination number distribution
 *      - Radial distribution function (RDF) fingerprint
 *      - Bond angle distribution (BAD) fingerprint
 *
 *   3. Nanoparticle dynamics
 *      - Mean square displacement (MSD) → diffusion coefficient D
 *      - Lindemann index (melting criterion)
 *      - Velocity autocorrelation → vibrational density of states
 *
 * References matched against:
 *   - CRC Handbook of Nanoparticle Properties
 *   - Experimental surface energies (Tyson & Miller, 1977)
 *   - Nanoparticle diffusion (Stokes–Einstein + corrections)
 *
 * Regime: 4–6 nm diameter ≈ 200–6000 atoms depending on element.
 * This sits at the upper boundary of fully atomistic treatment,
 * where every atom is still individually resolved.
 */

#include "atomistic/core/state.hpp"
#include <vector>
#include <string>
#include <map>
#include <cmath>
#include <cstdint>

namespace atomistic {
namespace validation {

// ============================================================================
// Unit conversion helpers (internal: kcal/mol & Å; experimental: eV, J/m²)
// ============================================================================

constexpr double KCAL_PER_EV      = 23.0605;       // 1 eV = 23.0605 kcal/mol
constexpr double EV_PER_KCAL      = 1.0 / 23.0605;
constexpr double J_PER_M2_TO_KCAL_PER_ANG2 = 1.4393e-2; // 1 J/m² ≈ 0.014393 kcal/mol/Å²

// ============================================================================
// 1. Experimental Reference Record
// ============================================================================

/**
 * A single experimental data point for a nanoparticle system.
 * All energies in kcal/mol; lengths in Å; time in fs; D in Å²/fs.
 */
struct ExperimentalRecord {
    std::string label;                  // e.g. "Au_5nm_icosahedral"
    std::string source;                 // bibliographic reference

    // --- Identity ---
    uint32_t Z{};                       // dominant element (atomic number)
    double diameter_ang{};              // nominal diameter (Å)
    uint32_t approx_N{};               // approximate atom count

    // --- Geometric energies (kcal/mol per atom) ---
    double cohesive_energy{};           // bulk cohesive energy per atom
    double surface_energy_per_area{};   // σ (kcal/mol/Å²)
    double edge_energy_correction{};    // excess per edge atom (kcal/mol)

    // --- Structural ---
    double bulk_CN{};                   // expected bulk coordination number
    double surface_CN{};               // expected average surface CN
    double lattice_constant{};          // a₀ (Å), 0 if amorphous/unknown

    // --- Dynamics ---
    double diffusion_coeff{};           // D (Å²/fs), 0 if unknown
    double lindemann_threshold{};       // δ_L for melting, typ. 0.1–0.15
    double melting_point_K{};           // size-corrected T_m (K), 0 if unknown

    // --- Tolerances for scoring ---
    double tol_energy_frac{0.10};       // 10% default
    double tol_CN{1.0};                // ±1 neighbour
    double tol_D_frac{0.25};           // 25% (dynamics harder to pin down)
};

// ============================================================================
// 2. Computed Nanoparticle Observables (from State)
// ============================================================================

/**
 * Classify each atom as bulk, surface, edge, or vertex
 * based on coordination number relative to ideal bulk CN.
 */
enum class SiteClass : uint8_t {
    Bulk    = 0,   // CN ≥ bulk_CN_threshold
    Surface = 1,   // CN ∈ [surface_min, bulk_CN_threshold)
    Edge    = 2,   // CN ∈ [edge_min, surface_min)
    Vertex  = 3    // CN < edge_min
};

struct GeometricEnergyDecomposition {
    // Per-class atom counts
    uint32_t n_bulk{};
    uint32_t n_surface{};
    uint32_t n_edge{};
    uint32_t n_vertex{};

    // Per-class average potential energy (kcal/mol per atom)
    double E_bulk{};
    double E_surface{};
    double E_edge{};
    double E_vertex{};

    // Aggregate
    double E_cohesive_per_atom{};       // total PE / N
    double surface_area_est{};          // estimated from convex hull or sphere fit (Å²)
    double surface_energy_per_area{};   // (E_surface_total - n_surface * E_bulk) / A
};

struct DynamicsObservables {
    double diffusion_coeff{};           // D from MSD slope (Å²/fs)
    double lindemann_index{};           // δ_L = <√(<Δr²>)> / <r_nn>
    std::vector<double> msd_times;      // time points (fs)
    std::vector<double> msd_values;     // <|r(t)-r(0)|²> (Å²)
};

struct StructuralFingerprint {
    // Coordination number histogram [CN] → count
    std::map<int, uint32_t> cn_histogram;
    double mean_CN{};

    // RDF fingerprint (first few peak positions in Å)
    std::vector<double> rdf_peak_positions;
    std::vector<double> rdf_peak_heights;

    // Bond angle distribution peaks (degrees)
    std::vector<double> bad_peak_angles;
};

// ============================================================================
// 3. Match Scorecard
// ============================================================================

struct MatchScore {
    std::string label;                  // record label

    // Individual dimensionless scores ∈ [0, 1] where 1 = perfect match
    double score_cohesive{};
    double score_surface_energy{};
    double score_CN{};
    double score_diffusion{};
    double score_lindemann{};

    // Composite
    double composite{};                 // weighted geometric mean

    // Raw deltas for diagnostics
    double delta_cohesive{};            // sim − exp (kcal/mol)
    double delta_surface_energy{};
    double delta_CN{};
    double delta_D{};

    bool pass(double threshold = 0.70) const { return composite >= threshold; }
};

// ============================================================================
// 4. Computation API
// ============================================================================

/**
 * Classify atoms in a nanoparticle state into site classes.
 *
 * @param s               Simulation state (positions, types, forces)
 * @param cutoff          Neighbor cutoff (Å), typically 1.2 × nearest-neighbour distance
 * @param bulk_CN_thresh  Minimum CN to be considered "bulk" (e.g. 12 for FCC)
 * @return                Vector of SiteClass per atom (size N)
 */
std::vector<SiteClass> classify_sites(
    const State& s,
    double cutoff,
    int bulk_CN_thresh);

/**
 * Compute geometric energy decomposition for a nanoparticle.
 *
 * Requires that s.F and s.E have already been evaluated (call IModel::eval first).
 * Uses per-atom energies estimated from pair-wise force dot-position (virial route).
 *
 * @param s           State with evaluated forces
 * @param classes     Per-atom site classification
 * @param cutoff      Neighbor cutoff (Å)
 * @return            Decomposed energy metrics
 */
GeometricEnergyDecomposition compute_geometric_energy(
    const State& s,
    const std::vector<SiteClass>& classes,
    double cutoff);

/**
 * Compute structural fingerprint (CN histogram, RDF peaks, BAD peaks).
 *
 * @param s        State
 * @param cutoff   Neighbor cutoff (Å)
 * @param rdf_dr   RDF bin width (Å), default 0.05
 * @param rdf_max  RDF max distance (Å), default 10.0
 * @return         Structural fingerprint
 */
StructuralFingerprint compute_structural_fingerprint(
    const State& s,
    double cutoff,
    double rdf_dr  = 0.05,
    double rdf_max = 10.0);

/**
 * Compute dynamics observables from a trajectory (sequence of states).
 *
 * @param trajectory  Ordered snapshots; must share same N and ordering
 * @param dt          Time step between snapshots (fs)
 * @param max_lag     Maximum lag for MSD (number of frames)
 * @return            Dynamics metrics (D, Lindemann, MSD curve)
 */
DynamicsObservables compute_dynamics(
    const std::vector<State>& trajectory,
    double dt,
    uint32_t max_lag = 0);

/**
 * Score simulation results against an experimental record.
 *
 * @param geo     Geometric energy decomposition (from compute_geometric_energy)
 * @param struc   Structural fingerprint (from compute_structural_fingerprint)
 * @param dyn     Dynamics observables (from compute_dynamics)
 * @param ref     Experimental reference record
 * @return        Scored comparison
 */
MatchScore score_against_experiment(
    const GeometricEnergyDecomposition& geo,
    const StructuralFingerprint& struc,
    const DynamicsObservables& dyn,
    const ExperimentalRecord& ref);

} // namespace validation
} // namespace atomistic
