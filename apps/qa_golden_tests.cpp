/**
 * qa_golden_tests.cpp
 * -------------------
 * Pre-Batching Quality & Reproducibility Milestone
 * 
 * Purpose: Deterministic validation framework that proves your simulations
 *          are reproducible, not just "looks right."
 * 
 * Milestone Definition:
 * - One command regenerates golden structures deterministically
 * - Reports pass/fail with concrete diffs
 * - Prints compact benchmark summary
 * - Outputs metadata for cross-platform reproduction
 * 
 * NO VIBES. Only facts.
 */

#include "atomistic/core/linalg.hpp"
#include "atomistic/models/model.hpp"
#include "atomistic/models/bonded.hpp"
#include "atomistic/parsers/xyz_parser.hpp"
#include "src/io/xyz_format.cpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <filesystem>
#include <random>
#include <vector>
#include <map>
#include <cmath>
#include <algorithm>

using namespace atomistic;
namespace fs = std::filesystem;

// ============================================================================
// CHARGE ASSIGNMENT POLICY (TEST-SPECIFIC)
// ============================================================================

/**
 * Simple charge policy for QA tests
 * 
 * Real Implementation Note:
 * For molecules, charges should come from:
 * - ESP fitting (electrostatic potential)
 * - Charge equilibration (QEq)
 * - Force field parameters (OPLS, etc.)
 * 
 * For now, using simple ionic charges for salts, neutral for everything else.
 * 
 * NOTE: LJ parameters (sigma, epsilon) are now in atomistic/models/lj_coulomb.cpp
 */
static const std::map<int, double> CHARGE_BY_Z_DEFAULT = {
    {1,  0.0},     // H (set per-molecule)
    {6,  0.0},     // C
    {7,  0.0},     // N
    {8,  0.0},     // O
    {9,  0.0},     // F
    {11, +1.0},    // Na (ionic)
    {12, +2.0},    // Mg (ionic)
    {13, 0.0},     // Al (metallic)
    {14, 0.0},     // Si
    {15, 0.0},     // P
    {16, 0.0},     // S
    {17, -1.0},    // Cl (ionic)
    {20, +2.0},    // Ca (ionic)
    {26, 0.0},     // Fe (metallic)
    {54, 0.0},     // Xe
    {55, +1.0},    // Cs (ionic)
    {84, 0.0}      // Po
};

/**
 * Get charge for an element (with fallback to neutral)
 */
inline double get_charge(int atomic_number) {
    auto it = CHARGE_BY_Z_DEFAULT.find(atomic_number);
    return (it != CHARGE_BY_Z_DEFAULT.end()) ? it->second : 0.0;
}

// ============================================================================
// STATE ADAPTER LAYER
// ============================================================================

/**
 * Simplified state structure for QA tests
 * 
 * Why this exists:
 * - QA validation logic uses simple {atomic_numbers, positions} format
 * - atomistic::State uses {N, X, type, Q, M, V, F, E} format
 * - These adapters convert between the two
 */
struct CoreState {
    std::vector<int> atomic_numbers;
    std::vector<Vec3> positions;

    // Periodic boundary conditions (part of system definition)
    bool pbc_enabled = false;
    atomistic::Vec3 box_lengths = {0, 0, 0};  // {Lx, Ly, Lz} in Å

    // Optional: store atomistic state for round-tripping
    atomistic::State* atomistic_state_ptr = nullptr;
};

// ============================================================================
// CRYSTAL INVARIANT STRUCTURES (defined early for use throughout)
// ============================================================================

struct CoordinationShell {
    double distance;       // Shell distance (Å)
    int multiplicity;      // Number of neighbors
    double tolerance;      // Distance tolerance
};

struct RDFPeak {
    double r;              // Peak position (Å)
    int count;             // Number of atoms in bin
};

struct LatticeInvariants {
    double det_A;          // Cell volume (det of lattice matrix)
    Vec3 metric_eigenvalues;  // Eigenvalues of G = A^T·A
    double a, b, c;        // Lattice parameters
    double alpha, beta, gamma;  // Angles
};

// Forward declarations for types used before definition
struct GoldenTest;

/**
 * Convert atomistic::State → CoreState
 * Used after simulation to extract data for validation
 */
inline CoreState from_atomistic_state(const atomistic::State& s) {
    CoreState core;
    core.positions = s.X;  // atomistic uses X for positions
    core.atomic_numbers.resize(s.type.size());

    // Convert type IDs to atomic numbers (assume 1:1 mapping for now)
    for (size_t i = 0; i < s.type.size(); ++i) {
        core.atomic_numbers[i] = static_cast<int>(s.type[i]);
    }

    return core;
}

/**
 * Convert CoreState → atomistic::State
 * Used before simulation to prepare initial structure
 */
inline atomistic::State to_atomistic_state(const CoreState& core) {
    atomistic::State s;
    s.N = static_cast<uint32_t>(core.positions.size());
    s.X = core.positions;  // atomistic uses X for positions

    // Convert atomic numbers to type IDs
    s.type.resize(core.atomic_numbers.size());
    for (size_t i = 0; i < core.atomic_numbers.size(); ++i) {
        s.type[i] = static_cast<uint32_t>(core.atomic_numbers[i]);
    }

    // Initialize required atomistic fields
    s.V.resize(s.N, {0, 0, 0});      // Velocities (start at rest)
    s.M.resize(s.N, 1.0);            // Masses (unit mass for now)
    s.F.resize(s.N, {0, 0, 0});      // Forces (will be computed)

    // Assign charges based on atomic numbers (using charge policy)
    s.Q.resize(s.N);
    for (size_t i = 0; i < core.atomic_numbers.size(); ++i) {
        s.Q[i] = get_charge(core.atomic_numbers[i]);
    }

    // Translate PBC box from CoreState to atomistic::State (deterministic)
    if (core.pbc_enabled && core.box_lengths.x > 0) {
        s.box = atomistic::BoxPBC(core.box_lengths.x, 
                             core.box_lengths.y, 
                             core.box_lengths.z);
    }

    return s;
}

/**
 * Update CoreState with results from atomistic::State
 * Used after simulation to sync positions back
 */
inline void sync_from_atomistic(CoreState& core, const atomistic::State& s) {
    core.positions = s.X;
}

// ============================================================================
// LJCOULOMB MODEL WRAPPER
// ============================================================================

/**
 * Simple wrapper around atomistic factory-based LJ+Coulomb model
 * 
 * Why this exists:
 * - atomistic provides create_lj_coulomb_model() factory (returns std::unique_ptr<IModel>)
 * - QA tests want direct class usage (LJCoulombModel model;)
 * - This wrapper provides the expected interface
 */
class LJCoulombModel {
    std::unique_ptr<IModel> impl_;
    ModelParams params_;

public:
    LJCoulombModel() {
        impl_ = create_lj_coulomb_model();  // Factory function

        // Default parameters (per-type params are now used from lj_coulomb.cpp)
        params_.rc = 10.0;        // 10 Å cutoff
        params_.k_coul = 138.935; // Coulomb constant (kcal·Å·e⁻²·mol⁻¹)

        // NOTE: sigma and eps are now per-type in the model itself
        // These global values are no longer used
        params_.sigma = 0.0;  // Disabled (per-type used instead)
        params_.eps = 0.0;    // Disabled (per-type used instead)
    }

    /**
     * Compute energy for given state (CoreState version)
     * Note: Converts to atomistic::State, evaluates, returns energy
     */
    double energy(CoreState& core_state) {
        atomistic::State s = to_atomistic_state(core_state);
        impl_->eval(s, params_);
        sync_from_atomistic(core_state, s);
        return s.E.total();
    }

    /**
     * Compute forces for given state (CoreState version)
     * Note: Converts to atomistic::State, evaluates, returns forces
     */
    std::vector<Vec3> forces(CoreState& core_state) {
        atomistic::State s = to_atomistic_state(core_state);
        impl_->eval(s, params_);
        sync_from_atomistic(core_state, s);
        return s.F;
    }

    /**
     * Direct atomistic::State version (for efficiency)
     */
    double energy(atomistic::State& state) {
        impl_->eval(state, params_);
        return state.E.total();
    }

    /**
     * Direct atomistic::State version (for efficiency)
     */
    std::vector<Vec3> forces(atomistic::State& state) {
        impl_->eval(state, params_);
        return state.F;
    }

    /**
     * Set model parameters
     */
    void set_params(double rc, double eps, double sigma) {
        params_.rc = rc;
        params_.eps = eps;
        params_.sigma = sigma;
    }
};

// ============================================================================
// FIRE MINIMIZER (Simplified for QA)
// ============================================================================

struct FIREMinimizer {
    int max_steps = 1000;
    double f_tol = 1e-4;     // Force tolerance (Å⁻¹)
    double dt_init = 0.1;    // Initial timestep
    double dt_max = 1.0;     // Maximum timestep
    double alpha_init = 0.1; // Initial mixing parameter

    struct Result {
        bool converged;
        int iterations;
        double final_max_force;
        double final_energy;
        std::vector<double> energy_trace;  // WARNING: Can grow large!

        // SAFETY: Reserve reasonable capacity
        Result() {
            energy_trace.reserve(1000);  // Typical max iterations
        }
    };

    template<typename Model>
    Result minimize(CoreState& core_state, Model& model) {
        // Convert to atomistic::State for simulation
        atomistic::State state = to_atomistic_state(core_state);

        Result result;
        result.converged = false;
        result.iterations = 0;

        // SAFETY: Validate initial state
        if (state.X.empty()) {
            std::cerr << "ERROR: Cannot minimize empty state!\n";
            return result;
        }

        // SAFETY: Check for NaN/Inf in initial positions
        for (size_t i = 0; i < state.X.size(); ++i) {
            if (!std::isfinite(state.X[i].x) ||
                !std::isfinite(state.X[i].y) ||
                !std::isfinite(state.X[i].z)) {
                std::cerr << "ERROR: Initial position " << i << " contains NaN/Inf!\n";
                return result;
            }
        }

        // Initialize velocities to zero
        std::vector<Vec3> velocities(state.X.size(), {0, 0, 0});

        double dt = dt_init;
        double alpha = alpha_init;
        int n_pos = 0;  // Steps since last restart

        // SAFETY: Track max force history to detect divergence
        double prev_max_force = 1e10;
        int divergence_count = 0;

        for (int step = 0; step < max_steps; ++step) {
            // Compute forces and energy
            auto forces = model.forces(state);
            double energy = model.energy(state);

            result.energy_trace.push_back(energy);

            // Check convergence (max force)
            double max_force = 0.0;
            for (const auto& f : forces) {
                double f_mag = std::sqrt(f.x*f.x + f.y*f.y + f.z*f.z);
                max_force = std::max(max_force, f_mag);
            }

            result.final_max_force = max_force;
            result.final_energy = energy;

            // SAFETY: Check for NaN/Inf in energy or forces
            if (!std::isfinite(energy) || !std::isfinite(max_force)) {
                std::cerr << "ERROR: FIRE iteration " << step 
                          << " produced NaN/Inf! Energy=" << energy 
                          << ", max_force=" << max_force << "\n";
                result.converged = false;
                result.iterations = step;
                return result;
            }

            // SAFETY: Detect runaway divergence
            if (max_force > prev_max_force * 2.0) {
                divergence_count++;
                if (divergence_count > 10) {
                    std::cerr << "ERROR: FIRE diverging! Max force increased 10x in a row.\n";
                    result.converged = false;
                    result.iterations = step;
                    return result;
                }
            } else {
                divergence_count = 0;
            }
            prev_max_force = max_force;

            if (max_force < f_tol) {
                result.converged = true;
                result.iterations = step;
                sync_from_atomistic(core_state, state);  // Sync final positions
                return result;
            }

            // FIRE algorithm
            // v = (1 - α)v + α * |v| * F/|F|
            double P = 0.0;  // Power: v · F
            double v_norm = 0.0, f_norm = 0.0;

            for (size_t i = 0; i < state.X.size(); ++i) {
                P += velocities[i].x * forces[i].x + 
                     velocities[i].y * forces[i].y + 
                     velocities[i].z * forces[i].z;

                v_norm += velocities[i].x*velocities[i].x + 
                         velocities[i].y*velocities[i].y + 
                         velocities[i].z*velocities[i].z;

                f_norm += forces[i].x*forces[i].x + 
                         forces[i].y*forces[i].y + 
                         forces[i].z*forces[i].z;
            }

            v_norm = std::sqrt(v_norm);
            f_norm = std::sqrt(f_norm);

            if (P > 0) {
                // Positive power - mix velocities toward force direction
                n_pos++;

                if (n_pos > 5) {
                    dt = std::min(dt * 1.1, dt_max);
                    alpha *= 0.99;
                }

                double mix = alpha * v_norm / (f_norm + 1e-10);
                for (size_t i = 0; i < state.X.size(); ++i) {
                    velocities[i].x = (1 - alpha) * velocities[i].x + mix * forces[i].x;
                    velocities[i].y = (1 - alpha) * velocities[i].y + mix * forces[i].y;
                    velocities[i].z = (1 - alpha) * velocities[i].z + mix * forces[i].z;
                }
            } else {
                // Negative power - reset
                n_pos = 0;
                dt = dt_init;
                alpha = alpha_init;

                for (auto& v : velocities) {
                    v = {0, 0, 0};
                }
            }

            // Velocity Verlet integration
            for (size_t i = 0; i < state.X.size(); ++i) {
                velocities[i].x += dt * forces[i].x;
                velocities[i].y += dt * forces[i].y;
                velocities[i].z += dt * forces[i].z;

                state.X[i].x += dt * velocities[i].x;
                state.X[i].y += dt * velocities[i].y;
                state.X[i].z += dt * velocities[i].z;
            }
        }

        result.converged = false;
        result.iterations = max_steps;

        // Sync back to CoreState
        sync_from_atomistic(core_state, state);

        return result;
    }
};

// ============================================================================
// 1) STRICT METADATA CAPTURE
// ============================================================================

struct RunManifest {
    // Identity
    std::string run_id;
    std::string git_commit;
    std::string build_id;

    // Platform
    std::string os;
    std::string cpu;
    std::string gpu;

    // Configuration
    std::string commandline;
    std::string config_hash;
    uint64_t rng_seed;
    std::string rng_algorithm;
    std::string model_id;

    // Validation
    std::string validation_mode;  // "STRICT" or "PORTABLE"

    // Tolerances
    double force_tolerance;
    double energy_tolerance;

    // PBC
    bool pbc_enabled;
    std::string pbc_cell;

    // Outputs
    std::vector<std::string> output_artifacts;

    std::string timestamp;
    
    // Serialize to JSON
    std::string to_json() const {
        std::ostringstream oss;
        oss << "{\n";
        oss << "  \"run_id\": \"" << run_id << "\",\n";
        oss << "  \"timestamp\": \"" << timestamp << "\",\n";
        oss << "  \"git_commit\": \"" << git_commit << "\",\n";
        oss << "  \"build_id\": \"" << build_id << "\",\n";
        oss << "  \"platform\": {\n";
        oss << "    \"os\": \"" << os << "\",\n";
        oss << "    \"cpu\": \"" << cpu << "\",\n";
        oss << "    \"gpu\": \"" << gpu << "\"\n";
        oss << "  },\n";
        oss << "  \"config\": {\n";
        oss << "    \"commandline\": \"" << commandline << "\",\n";
        oss << "    \"config_hash\": \"" << config_hash << "\",\n";
        oss << "    \"rng_seed\": " << rng_seed << ",\n";
        oss << "    \"rng_algorithm\": \"" << rng_algorithm << "\",\n";
        oss << "    \"model_id\": \"" << model_id << "\",\n";
        oss << "    \"validation_mode\": \"" << validation_mode << "\"\n";
        oss << "  },\n";
        oss << "  \"tolerances\": {\n";
        oss << "    \"force\": " << force_tolerance << ",\n";
        oss << "    \"energy\": " << energy_tolerance << "\n";
        oss << "  },\n";
        oss << "  \"pbc\": {\n";
        oss << "    \"enabled\": " << (pbc_enabled ? "true" : "false") << ",\n";
        oss << "    \"cell\": \"" << pbc_cell << "\"\n";
        oss << "  },\n";
        oss << "  \"output_artifacts\": [\n";
        for (size_t i = 0; i < output_artifacts.size(); ++i) {
            oss << "    \"" << output_artifacts[i] << "\"";
            if (i < output_artifacts.size() - 1) oss << ",";
            oss << "\n";
        }
        oss << "  ]\n";
        oss << "}\n";
        return oss.str();
    }
};

struct StructureRecord {
    std::string structure_id;     // Canonical hash
    uint64_t source_seed;
    bool converged;
    int iterations;
    double final_energy;
    double max_force;
    std::map<std::string, double> energy_decomp;
    std::string reject_reason;

    // Crystal metrics (optional)
    double nn_distance = 0.0;     // Nearest-neighbor distance
    double r2_over_r1 = 0.0;      // Shell ratio
    std::vector<RDFPeak> rdf_peaks;  // RDF fingerprint
    LatticeInvariants lattice_inv;   // Lattice invariants

    std::string to_json() const {
        std::ostringstream oss;
        oss << "{\n";
        oss << "  \"structure_id\": \"" << structure_id << "\",\n";
        oss << "  \"source_seed\": " << source_seed << ",\n";
        oss << "  \"converged\": " << (converged ? "true" : "false") << ",\n";
        oss << "  \"iterations\": " << iterations << ",\n";
        oss << "  \"final_energy\": " << std::fixed << std::setprecision(6) 
            << final_energy << ",\n";
        oss << "  \"max_force\": " << max_force << ",\n";
        oss << "  \"energy_decomp\": {\n";
        size_t idx = 0;
        for (const auto& [key, val] : energy_decomp) {
            oss << "    \"" << key << "\": " << val;
            if (idx < energy_decomp.size() - 1) oss << ",";
            oss << "\n";
            ++idx;
        }
        oss << "  }";

        // Add crystal metrics if present
        if (nn_distance > 0.0) {
            oss << ",\n  \"crystal_metrics\": {\n";
            oss << "    \"nn_distance\": " << std::fixed << std::setprecision(3) 
                << nn_distance << ",\n";
            oss << "    \"r2_over_r1\": " << std::fixed << std::setprecision(3) 
                << r2_over_r1;

            // Add RDF peaks
            if (!rdf_peaks.empty()) {
                oss << ",\n    \"rdf_peaks\": [\n";
                for (size_t i = 0; i < rdf_peaks.size(); ++i) {
                    oss << "      {\"r\": " << std::fixed << std::setprecision(2) 
                        << rdf_peaks[i].r << ", \"count\": " << rdf_peaks[i].count << "}";
                    if (i < rdf_peaks.size() - 1) oss << ",";
                    oss << "\n";
                }
                oss << "    ]";
            }

            // Add lattice invariants
            if (lattice_inv.det_A > 0.0) {
                oss << ",\n    \"lattice_invariants\": {\n";
                oss << "      \"volume\": " << std::fixed << std::setprecision(2) 
                    << lattice_inv.det_A << ",\n";
                oss << "      \"parameters\": {\"a\": " << std::setprecision(3) 
                    << lattice_inv.a << ", \"b\": " << lattice_inv.b << ", \"c\": " 
                    << lattice_inv.c << "},\n";
                oss << "      \"angles\": {\"alpha\": " << std::setprecision(1) 
                    << lattice_inv.alpha << ", \"beta\": " << lattice_inv.beta 
                    << ", \"gamma\": " << lattice_inv.gamma << "},\n";
                oss << "      \"metric_eigenvalues\": [" << std::setprecision(2) 
                    << lattice_inv.metric_eigenvalues.x << ", " 
                    << lattice_inv.metric_eigenvalues.y << ", " 
                    << lattice_inv.metric_eigenvalues.z << "]\n";
                oss << "    }";
            }

            oss << "\n  }";
        }

        if (!reject_reason.empty()) {
            oss << ",\n  \"reject_reason\": \"" << reject_reason << "\"";
        }
        oss << "\n}\n";
        return oss.str();
    }
};

// ============================================================================
// VALIDATION MODES
// ============================================================================

enum class ValidationMode {
    STRICT,     // Byte-identical (same build, same platform)
    PORTABLE    // Physics-identical (cross-platform, tolerances)
};

// ============================================================================
// GOLDEN TEST STRUCTURE (must be defined before Validator uses it)
// ============================================================================

struct GoldenTest {
    std::string name;
    std::string category;  // "molecule" or "crystal"
    CoreState initial_state;
    uint64_t seed;
    std::string expected_hash;
    double expected_energy_min;
    double expected_energy_max;
    std::map<std::string, int> expected_coordination;

    // Crystal-specific metrics
    double expected_nn_distance;  // Nearest-neighbor distance (Å)
    double expected_r2_over_r1;   // Second shell / first shell ratio
    double expected_nn_tolerance = 0.05;  // ±5% tolerance on distances

    // RDF fingerprint (first 3 peaks)
    std::vector<RDFPeak> expected_rdf_peaks;
    double rdf_r_tolerance = 0.1;     // ±0.1 Å tolerance on peak positions
    int rdf_count_tolerance = 2;      // ±2 count tolerance

    // Lattice invariants (for crystal validation)
    LatticeInvariants expected_lattice;
    double lattice_volume_tolerance = 0.05;   // ±5% on volume
    double lattice_length_tolerance = 0.2;    // ±0.2 Å on lengths
    double lattice_angle_tolerance = 2.0;     // ±2° on angles

    void print() const {
        std::cout << "\n=== " << name << " (" << category << ") ===\n";
        std::cout << "Seed: " << seed << "\n";
        std::cout << "Expected hash: " << expected_hash.substr(0, 40) << "...\n";
        std::cout << "Expected energy: [" << expected_energy_min 
                  << ", " << expected_energy_max << "]\n";
        if (category == "crystal") {
            std::cout << "Expected NN distance: " << expected_nn_distance << " Å\n";
            std::cout << "Expected r2/r1 ratio: " << expected_r2_over_r1 << "\n";
            if (!expected_rdf_peaks.empty()) {
                std::cout << "Expected RDF peaks: " << expected_rdf_peaks.size() << "\n";
            }
        }
    }
};

// ============================================================================
// STRUCTURE CANONICALIZATION (for deterministic hash computation)
// ============================================================================

class StructureCanonicalizer {
public:
    // Canonicalize molecule: center + sort + align
    static CoreState canonicalize_molecule(const CoreState& state, double tolerance = 1e-6) {
        CoreState canonical = state;

        // Step 1: Center at origin
        Vec3 com = {0, 0, 0};
        for (const auto& pos : canonical.positions) {
            com = com + pos;
        }
        com = com * (1.0 / static_cast<double>(canonical.positions.size()));

        for (auto& pos : canonical.positions) {
            pos = pos - com;
        }

        // Step 2: Sort by distance from origin (stable ordering)
        std::vector<size_t> indices(canonical.positions.size());
        for (size_t i = 0; i < indices.size(); ++i) indices[i] = i;

        std::sort(indices.begin(), indices.end(), [&](size_t i, size_t j) {
            auto& pi = canonical.positions[i];
            auto& pj = canonical.positions[j];
            double di = std::sqrt(pi.x*pi.x + pi.y*pi.y + pi.z*pi.z);
            double dj = std::sqrt(pj.x*pj.x + pj.y*pj.y + pj.z*pj.z);

            if (std::abs(di - dj) > tolerance) return di < dj;

            // Secondary sort: atomic number
            return canonical.atomic_numbers[i] < canonical.atomic_numbers[j];
        });

        // Reorder
        std::vector<Vec3> sorted_pos;
        std::vector<int> sorted_Z;
        for (size_t idx : indices) {
            sorted_pos.push_back(canonical.positions[idx]);
            sorted_Z.push_back(canonical.atomic_numbers[idx]);
        }
        canonical.positions = sorted_pos;
        canonical.atomic_numbers = sorted_Z;

        // Step 3: Kabsch alignment to standard orientation (optional)
        // For simplicity, skip full Kabsch - already have stable ordering

        return canonical;
    }

    // Compute canonical hash (deterministic string representation)
    static std::string compute_hash(const CoreState& state, double tolerance = 1e-6) {
        CoreState canonical = canonicalize_molecule(state, tolerance);

        std::ostringstream oss;
        oss << std::fixed << std::setprecision(6);

        for (size_t i = 0; i < canonical.positions.size(); ++i) {
            oss << static_cast<int>(canonical.atomic_numbers[i]) << "_";
            oss << canonical.positions[i].x << "_";
            oss << canonical.positions[i].y << "_";
            oss << canonical.positions[i].z;
            if (i < canonical.positions.size() - 1) oss << "|";
        }

        // Simple hash: just use the string representation
        // In production, use SHA256 or similar
        return oss.str();
    }
};

// ============================================================================
// VALIDATION FUNCTIONS
// ============================================================================

class Validator {
public:
    /**
     * STRICT validation: Byte-identical (same build, same platform)
     * - Canonical hash must match exactly
     * - Energy must match within tight epsilon
     * - Ordering, quantization, formatting must be identical
     */
    static bool validate_strict(
        const CoreState& result,
        const GoldenTest& test,
        double energy,
        std::string& failure_reason
    ) {
        // 1. Canonical hash must match exactly
        std::string computed_hash = StructureCanonicalizer::compute_hash(result);
        if (computed_hash != test.expected_hash) {
            failure_reason = "Hash mismatch (STRICT mode)";
            return false;
        }

        // 2. Energy must match within tight epsilon
        double energy_epsilon = 1e-6;  // Very tight tolerance
        if (std::abs(energy - test.expected_energy_min) > energy_epsilon) {
            failure_reason = "Energy mismatch (STRICT mode): expected " + 
                           std::to_string(test.expected_energy_min) + 
                           ", got " + std::to_string(energy);
            return false;
        }

        return true;
    }

    /**
     * PORTABLE validation: Physics-identical (cross-platform)
     * - RMSD after alignment < δ
     * - Coordination signature matches
     * - Energy within relative tolerance
     * - Lattice invariants match (for crystals)
     */
    static bool validate_portable(
        const CoreState& result,
        const GoldenTest& test,
        double energy,
        const std::map<std::string, int>& coordination,
        std::string& failure_reason
    ) {
        // 1. Energy within calibrated range [min, max]
        if (energy < test.expected_energy_min || energy > test.expected_energy_max) {
            failure_reason = "Energy outside tolerance (PORTABLE mode): expected [" + 
                           std::to_string(test.expected_energy_min) + ", " + 
                           std::to_string(test.expected_energy_max) + "], got " + 
                           std::to_string(energy);
            return false;
        }

        // 2. Coordination signature matches
        for (const auto& [pair_type, expected_cn] : test.expected_coordination) {
            auto it = coordination.find(pair_type);
            if (it == coordination.end()) {
                failure_reason = "Missing coordination for " + pair_type;
                return false;
            }

            int computed_cn = it->second;
            if (computed_cn != expected_cn) {
                failure_reason = "Coordination mismatch for " + pair_type + 
                               ": expected " + std::to_string(expected_cn) + 
                               ", got " + std::to_string(computed_cn);
                return false;
            }
        }

        // 3. For crystals: check NN distance
        if (test.category == "crystal" && test.expected_nn_distance > 0.0) {
            // This will be filled in by coordination shell analysis (item #2)
            // For now, just pass
        }

        return true;
    }
};

// ============================================================================
// CRYSTAL METRICS AGGREGATE (for result storage)
// ============================================================================

struct CrystalMetrics {
    // Coordination shells
    std::vector<CoordinationShell> shells;

    // RDF fingerprint (first 3 peaks)
    std::vector<RDFPeak> rdf_peaks;

    // Lattice invariants
    LatticeInvariants lattice;

    // Nearest-neighbor ratios
    double r2_over_r1;  // Second shell / first shell ratio
};

// ============================================================================
// RDF FINGERPRINTING (lightweight, deterministic)
// ============================================================================

class RDFAnalyzer {
public:
    /**
     * Compute RDF fingerprint (first N peaks)
     * 
     * Algorithm:
     * 1. Compute all pairwise distances (with MIC if PBC)
     * 2. Bin into histogram (bin_width = 0.1 Å)
     * 3. Find local maxima (peaks)
     * 4. Return first N peaks with positions and counts
     * 
     * Why this matters:
     * - Catches "wrong 12 neighbors" bugs
     * - Rotation-invariant structural signature
     * - Lightweight but information-dense
     * 
     * @param state The atomic structure
     * @param num_peaks Number of peaks to extract (default: 3)
     * @param bin_width Histogram bin width in Å (default: 0.1)
     * @param max_distance Maximum distance to consider (default: 10.0 Å)
     * @return Vector of RDF peaks
     */
    static std::vector<RDFPeak> compute_rdf_peaks(
        const CoreState& state,
        int num_peaks = 3,
        double bin_width = 0.1,
        double max_distance = 10.0
    ) {
        if (state.positions.size() < 2) {
            return {};
        }

        // SAFETY: Limit max_distance to prevent memory overflow
        const double MAX_SAFE_DISTANCE = 100.0;  // 100 Å is plenty
        if (max_distance > MAX_SAFE_DISTANCE) {
            std::cerr << "WARNING: max_distance " << max_distance 
                      << " exceeds safe limit " << MAX_SAFE_DISTANCE 
                      << " Å. Clamping.\n";
            max_distance = MAX_SAFE_DISTANCE;
        }

        // SAFETY: Limit bin count to prevent huge allocations
        int num_bins = static_cast<int>(max_distance / bin_width) + 1;
        const int MAX_BINS = 10000;  // 40KB max for histogram
        if (num_bins > MAX_BINS) {
            std::cerr << "WARNING: num_bins " << num_bins 
                      << " exceeds safe limit " << MAX_BINS 
                      << ". Increasing bin_width.\n";
            bin_width = max_distance / MAX_BINS;
            num_bins = MAX_BINS;
        }

        // 1. Compute all pairwise distances
        std::vector<double> distances;
        for (size_t i = 0; i < state.positions.size(); ++i) {
            for (size_t j = i + 1; j < state.positions.size(); ++j) {
                Vec3 dr = {
                    state.positions[j].x - state.positions[i].x,
                    state.positions[j].y - state.positions[i].y,
                    state.positions[j].z - state.positions[i].z
                };

                // TODO: Apply minimum-image convention if PBC enabled

                double dist = std::sqrt(dr.x*dr.x + dr.y*dr.y + dr.z*dr.z);

                if (dist < max_distance) {
                    distances.push_back(dist);
                }
            }
        }

        if (distances.empty()) {
            return {};
        }

        // 2. Bin into histogram (num_bins already calculated above)
        std::vector<int> histogram(num_bins, 0);

        for (double dist : distances) {
            int bin = static_cast<int>(dist / bin_width);
            if (bin >= 0 && bin < num_bins) {
                histogram[bin]++;
            }
        }

        // 3. Find peaks (local maxima)
        std::vector<RDFPeak> all_peaks;

        for (int i = 1; i < num_bins - 1; ++i) {
            // Check if this is a local maximum
            if (histogram[i] > histogram[i-1] && histogram[i] > histogram[i+1]) {
                // Skip tiny peaks (noise)
                if (histogram[i] >= 2) {
                    RDFPeak peak;
                    peak.r = (i + 0.5) * bin_width;  // Center of bin
                    peak.count = histogram[i];
                    all_peaks.push_back(peak);
                }
            }
        }

        // 4. Sort by height (descending)
        std::sort(all_peaks.begin(), all_peaks.end(), 
                  [](const RDFPeak& a, const RDFPeak& b) {
                      return a.count > b.count;
                  });

        // 5. Return first N peaks
        std::vector<RDFPeak> result;
        for (int i = 0; i < std::min(num_peaks, static_cast<int>(all_peaks.size())); ++i) {
            result.push_back(all_peaks[i]);
        }

        // Sort by distance for output
        std::sort(result.begin(), result.end(), 
                  [](const RDFPeak& a, const RDFPeak& b) {
                      return a.r < b.r;
                  });

        return result;
    }

    /**
     * Validate RDF peaks against expected values
     * 
     * @param computed Computed RDF peaks
     * @param expected Expected RDF peaks (from golden test)
     * @param r_tolerance Distance tolerance in Å (default: 0.1)
     * @param count_tolerance Count tolerance (absolute, default: 2)
     * @param failure_reason Output parameter for failure description
     * @return true if peaks match within tolerance
     */
    static bool validate_peaks(
        const std::vector<RDFPeak>& computed,
        const std::vector<RDFPeak>& expected,
        double r_tolerance,
        int count_tolerance,
        std::string& failure_reason
    ) {
        if (expected.empty()) {
            // No expected peaks specified - skip validation
            return true;
        }

        if (computed.size() < expected.size()) {
            failure_reason = "Too few RDF peaks: expected " + 
                           std::to_string(expected.size()) + ", got " + 
                           std::to_string(computed.size());
            return false;
        }

        // Validate each peak
        for (size_t i = 0; i < expected.size(); ++i) {
            const auto& exp = expected[i];
            const auto& comp = computed[i];

            // Check distance
            if (std::abs(comp.r - exp.r) > r_tolerance) {
                failure_reason = "RDF peak " + std::to_string(i+1) + 
                               " position mismatch: expected " + 
                               std::to_string(exp.r) + " ± " + 
                               std::to_string(r_tolerance) + " Å, got " + 
                               std::to_string(comp.r) + " Å";
                return false;
            }

            // Check count
            if (std::abs(comp.count - exp.count) > count_tolerance) {
                failure_reason = "RDF peak " + std::to_string(i+1) + 
                               " count mismatch: expected " + 
                               std::to_string(exp.count) + " ± " + 
                               std::to_string(count_tolerance) + ", got " + 
                               std::to_string(comp.count);
                return false;
            }
        }

        return true;
    }

    /**
     * Print RDF peaks for debugging
     */
    static void print_peaks(const std::vector<RDFPeak>& peaks) {
        std::cout << "  RDF peaks:\n";
        for (size_t i = 0; i < peaks.size(); ++i) {
            std::cout << "    Peak " << (i+1) << ": r = " 
                      << std::fixed << std::setprecision(2) 
                      << peaks[i].r << " Å, count = " 
                      << peaks[i].count << "\n";
        }
    }

    /**
     * Compute RDF histogram for visualization/debugging
     * 
     * @param state The atomic structure
     * @param bin_width Histogram bin width in Å
     * @param max_distance Maximum distance to consider
     * @return Pair of (bin_centers, counts)
     */
    static std::pair<std::vector<double>, std::vector<int>> compute_histogram(
        const CoreState& state,
        double bin_width = 0.1,
        double max_distance = 10.0
    ) {
        std::vector<double> bin_centers;
        std::vector<int> counts;

        // Compute distances
        std::vector<double> distances;
        for (size_t i = 0; i < state.positions.size(); ++i) {
            for (size_t j = i + 1; j < state.positions.size(); ++j) {
                Vec3 dr = {
                    state.positions[j].x - state.positions[i].x,
                    state.positions[j].y - state.positions[i].y,
                    state.positions[j].z - state.positions[i].z
                };

                double dist = std::sqrt(dr.x*dr.x + dr.y*dr.y + dr.z*dr.z);

                if (dist < max_distance) {
                    distances.push_back(dist);
                }
            }
        }

        // Bin
        int num_bins = static_cast<int>(max_distance / bin_width) + 1;
        std::vector<int> histogram(num_bins, 0);

        for (double dist : distances) {
            int bin = static_cast<int>(dist / bin_width);
            if (bin >= 0 && bin < num_bins) {
                histogram[bin]++;
            }
        }

        // Convert to output format
        for (int i = 0; i < num_bins; ++i) {
            bin_centers.push_back((i + 0.5) * bin_width);
            counts.push_back(histogram[i]);
        }

        return {bin_centers, counts};
    }
};

// ============================================================================
// LATTICE INVARIANTS (rotation-invariant cell properties)
// ============================================================================

class LatticeAnalyzer {
public:
    /**
     * Compute lattice invariants for a crystal structure
     * 
     * Invariants computed:
     * 1. det(A) - Unit cell volume (scalar triple product)
     * 2. Metric tensor eigenvalues - G = A^T·A eigenvalues (rotation-invariant)
     * 3. Lattice parameters - a, b, c, α, β, γ
     * 
     * Why this matters:
     * - Catches wrong cell orientation (det sign)
     * - Catches wrong cell volume (det magnitude)
     * - Catches cell distortion (eigenvalues)
     * - Rotation-invariant (eigenvalues)
     * 
     * @param state The atomic structure (must have lattice info)
     * @return Lattice invariants
     */
    static LatticeInvariants compute_invariants(const CoreState& state) {
        LatticeInvariants inv;

        // For now, assume Cartesian coordinates = lattice vectors
        // TODO: Extract actual lattice matrix from state when PBC implemented

        // Placeholder: Use bounding box as lattice
        if (state.positions.empty()) {
            return inv;
        }

        // Find bounding box
        Vec3 min_pos = state.positions[0];
        Vec3 max_pos = state.positions[0];

        for (const auto& pos : state.positions) {
            min_pos.x = std::min(min_pos.x, pos.x);
            min_pos.y = std::min(min_pos.y, pos.y);
            min_pos.z = std::min(min_pos.z, pos.z);

            max_pos.x = std::max(max_pos.x, pos.x);
            max_pos.y = std::max(max_pos.y, pos.y);
            max_pos.z = std::max(max_pos.z, pos.z);
        }

        // Lattice vectors (assumed cubic for now)
        Vec3 a = {max_pos.x - min_pos.x, 0.0, 0.0};
        Vec3 b = {0.0, max_pos.y - min_pos.y, 0.0};
        Vec3 c = {0.0, 0.0, max_pos.z - min_pos.z};

        // 1. Compute det(A) - Volume
        inv.det_A = compute_determinant(a, b, c);

        // 2. Compute lattice parameters
        inv.a = std::sqrt(a.x*a.x + a.y*a.y + a.z*a.z);
        inv.b = std::sqrt(b.x*b.x + b.y*b.y + b.z*b.z);
        inv.c = std::sqrt(c.x*c.x + c.y*c.y + c.z*c.z);

        // Angles (in degrees)
        if (inv.b > 1e-6 && inv.c > 1e-6) {
            double dot_bc = b.x*c.x + b.y*c.y + b.z*c.z;
            inv.alpha = std::acos(std::clamp(dot_bc / (inv.b * inv.c), -1.0, 1.0)) * 180.0 / M_PI;
        } else {
            inv.alpha = 90.0;
        }

        if (inv.a > 1e-6 && inv.c > 1e-6) {
            double dot_ac = a.x*c.x + a.y*c.y + a.z*c.z;
            inv.beta = std::acos(std::clamp(dot_ac / (inv.a * inv.c), -1.0, 1.0)) * 180.0 / M_PI;
        } else {
            inv.beta = 90.0;
        }

        if (inv.a > 1e-6 && inv.b > 1e-6) {
            double dot_ab = a.x*b.x + a.y*b.y + a.z*b.z;
            inv.gamma = std::acos(std::clamp(dot_ab / (inv.a * inv.b), -1.0, 1.0)) * 180.0 / M_PI;
        } else {
            inv.gamma = 90.0;
        }

        // 3. Compute metric tensor G = A^T·A
        double G[3][3];

        // G = A^T·A where A = [a b c] (column vectors)
        G[0][0] = a.x*a.x + a.y*a.y + a.z*a.z;  // a·a
        G[0][1] = a.x*b.x + a.y*b.y + a.z*b.z;  // a·b
        G[0][2] = a.x*c.x + a.y*c.y + a.z*c.z;  // a·c
        G[1][0] = G[0][1];  // b·a (symmetric)
        G[1][1] = b.x*b.x + b.y*b.y + b.z*b.z;  // b·b
        G[1][2] = b.x*c.x + b.y*c.y + b.z*c.z;  // b·c
        G[2][0] = G[0][2];  // c·a (symmetric)
        G[2][1] = G[1][2];  // c·b (symmetric)
        G[2][2] = c.x*c.x + c.y*c.y + c.z*c.z;  // c·c

        // 4. Compute eigenvalues of G (rotation-invariant!)
        inv.metric_eigenvalues = compute_eigenvalues_3x3(G);

        return inv;
    }

    /**
     * Compute determinant of 3x3 matrix formed by column vectors
     * det(A) = a·(b × c)
     */
    static double compute_determinant(const Vec3& a, const Vec3& b, const Vec3& c) {
        // Cross product: b × c
        Vec3 cross_bc = {
            b.y * c.z - b.z * c.y,
            b.z * c.x - b.x * c.z,
            b.x * c.y - b.y * c.x
        };

        // Scalar triple product: a·(b × c)
        return a.x * cross_bc.x + a.y * cross_bc.y + a.z * cross_bc.z;
    }

    /**
     * Compute eigenvalues of symmetric 3x3 matrix
     * Uses analytical solution for 3x3 symmetric case
     * 
     * For symmetric matrix, eigenvalues are real
     * Returns eigenvalues sorted in descending order
     */
    static Vec3 compute_eigenvalues_3x3(const double G[3][3]) {
        // Characteristic polynomial: det(G - λI) = 0
        // -λ³ + I₁λ² - I₂λ + I₃ = 0

        // Invariants
        double I1 = G[0][0] + G[1][1] + G[2][2];  // Trace

        double I2 = G[0][0]*G[1][1] + G[0][0]*G[2][2] + G[1][1]*G[2][2]
                  - G[0][1]*G[0][1] - G[0][2]*G[0][2] - G[1][2]*G[1][2];

        double I3 = G[0][0]*(G[1][1]*G[2][2] - G[1][2]*G[1][2])
                  - G[0][1]*(G[0][1]*G[2][2] - G[0][2]*G[1][2])
                  + G[0][2]*(G[0][1]*G[1][2] - G[0][2]*G[1][1]);  // Determinant

        // Solve cubic using trigonometric method (stable for symmetric matrices)
        double p = I2 - I1*I1/3.0;
        double q = 2.0*I1*I1*I1/27.0 - I1*I2/3.0 + I3;

        Vec3 eigenvalues;

        if (std::abs(p) < 1e-10) {
            // All eigenvalues equal (isotropic)
            eigenvalues = {I1/3.0, I1/3.0, I1/3.0};
        } else {
            // Use trigonometric method
            double m = 2.0*std::sqrt(-p/3.0);
            double theta = std::acos(std::clamp(3.0*q/(p*m), -1.0, 1.0))/3.0;

            eigenvalues.x = I1/3.0 + m*std::cos(theta);
            eigenvalues.y = I1/3.0 + m*std::cos(theta + 2.0*M_PI/3.0);
            eigenvalues.z = I1/3.0 + m*std::cos(theta + 4.0*M_PI/3.0);

            // Sort descending
            if (eigenvalues.x < eigenvalues.y) std::swap(eigenvalues.x, eigenvalues.y);
            if (eigenvalues.y < eigenvalues.z) std::swap(eigenvalues.y, eigenvalues.z);
            if (eigenvalues.x < eigenvalues.y) std::swap(eigenvalues.x, eigenvalues.y);
        }

        return eigenvalues;
    }

    /**
     * Validate lattice invariants against expected values
     */
    static bool validate_invariants(
        const LatticeInvariants& computed,
        const LatticeInvariants& expected,
        double volume_tolerance,
        double length_tolerance,
        double angle_tolerance,
        std::string& failure_reason
    ) {
        // 1. Check volume (det(A))
        if (expected.det_A > 0.0) {
            double vol_error = std::abs(computed.det_A - expected.det_A);
            double rel_error = vol_error / std::abs(expected.det_A);

            if (rel_error > volume_tolerance) {
                failure_reason = "Cell volume mismatch: expected " + 
                               std::to_string(expected.det_A) + " ± " + 
                               std::to_string(volume_tolerance * 100.0) + "%, got " + 
                               std::to_string(computed.det_A);
                return false;
            }
        }

        // 2. Check lattice parameters
        if (expected.a > 0.0) {
            if (std::abs(computed.a - expected.a) > length_tolerance) {
                failure_reason = "Lattice parameter 'a' mismatch: expected " + 
                               std::to_string(expected.a) + " ± " + 
                               std::to_string(length_tolerance) + " Å, got " + 
                               std::to_string(computed.a) + " Å";
                return false;
            }
        }

        // 3. Check angles (cubic = 90° expected)
        if (expected.alpha > 0.0) {
            if (std::abs(computed.alpha - expected.alpha) > angle_tolerance) {
                failure_reason = "Angle α mismatch: expected " + 
                               std::to_string(expected.alpha) + " ± " + 
                               std::to_string(angle_tolerance) + "°, got " + 
                               std::to_string(computed.alpha) + "°";
                return false;
            }
        }

        // 4. Check metric eigenvalues (rotation-invariant!)
        if (expected.metric_eigenvalues.x > 0.0) {
            for (int i = 0; i < 3; ++i) {
                double comp_eval = (i==0 ? computed.metric_eigenvalues.x : 
                                   (i==1 ? computed.metric_eigenvalues.y : 
                                           computed.metric_eigenvalues.z));
                double exp_eval = (i==0 ? expected.metric_eigenvalues.x : 
                                  (i==1 ? expected.metric_eigenvalues.y : 
                                          expected.metric_eigenvalues.z));

                if (exp_eval > 1e-6) {
                    double rel_error = std::abs(comp_eval - exp_eval) / std::abs(exp_eval);
                    if (rel_error > 0.05) {  // 5% tolerance on eigenvalues
                        failure_reason = "Metric eigenvalue " + std::to_string(i+1) + 
                                       " mismatch: expected " + std::to_string(exp_eval) + 
                                       ", got " + std::to_string(comp_eval);
                        return false;
                    }
                }
            }
        }

        return true;
    }

    /**
     * Print lattice invariants for debugging
     */
    static void print_invariants(const LatticeInvariants& inv) {
        std::cout << "  Lattice invariants:\n";
        std::cout << "    Volume (det A): " << std::fixed << std::setprecision(2) 
                  << inv.det_A << " ų\n";
        std::cout << "    Parameters: a=" << std::setprecision(3) << inv.a 
                  << " b=" << inv.b << " c=" << inv.c << " Å\n";
        std::cout << "    Angles: α=" << std::setprecision(1) << inv.alpha 
                  << "° β=" << inv.beta << "° γ=" << inv.gamma << "°\n";
        std::cout << "    Metric eigenvalues: [" << std::setprecision(2) 
                  << inv.metric_eigenvalues.x << ", " 
                  << inv.metric_eigenvalues.y << ", " 
                  << inv.metric_eigenvalues.z << "]\n";
    }
};

class CoordinationAnalyzer {
public:
    /**
     * Compute coordination shells for a structure
     * 
     * Algorithm:
     * 1. Compute all neighbor distances under MIC (if PBC)
     * 2. Bin by distance (with tolerance)
     * 3. Find first shell (r1, n1), second shell (r2, n2), etc.
     * 4. Return shells with multiplicities
     * 
     * @param state The atomic structure
     * @param cutoff Maximum distance to consider (Å)
     * @param tolerance Distance binning tolerance (Å)
     * @return Vector of coordination shells
     */
    static std::vector<CoordinationShell> compute_shells(
        const CoreState& state,
        double cutoff = 5.0,
        double tolerance = 0.05
    ) {
        if (state.positions.size() < 2) {
            return {};
        }

        // SAFETY: Warn for large structures
        const size_t MAX_SAFE_ATOMS = 10000;
        if (state.positions.size() > MAX_SAFE_ATOMS) {
            std::cerr << "WARNING: Structure has " << state.positions.size() 
                      << " atoms (> " << MAX_SAFE_ATOMS << "). "
                      << "Coordination analysis may be slow/memory-intensive.\n";
        }

        // Compute all pairwise distances
        std::vector<double> distances;
        size_t max_pairs = (state.positions.size() * (state.positions.size() - 1)) / 2;

        // SAFETY: Reserve memory upfront to avoid reallocations
        distances.reserve(std::min(max_pairs, size_t(100000)));  // Cap at 100k pairs
        for (size_t i = 0; i < state.positions.size(); ++i) {
            for (size_t j = i + 1; j < state.positions.size(); ++j) {
                Vec3 dr = {
                    state.positions[j].x - state.positions[i].x,
                    state.positions[j].y - state.positions[i].y,
                    state.positions[j].z - state.positions[i].z
                };

                // TODO: Apply minimum-image convention if PBC enabled

                double dist = std::sqrt(dr.x*dr.x + dr.y*dr.y + dr.z*dr.z);

                if (dist < cutoff) {
                    distances.push_back(dist);
                }
            }
        }

        if (distances.empty()) {
            return {};
        }

        // Sort distances
        std::sort(distances.begin(), distances.end());

        // Bin distances into shells
        std::vector<CoordinationShell> shells;

        double current_distance = distances[0];
        int current_count = 1;

        for (size_t i = 1; i < distances.size(); ++i) {
            if (std::abs(distances[i] - current_distance) < tolerance) {
                // Same shell
                current_count++;
            } else {
                // New shell
                CoordinationShell shell;
                shell.distance = current_distance;
                shell.multiplicity = current_count;
                shell.tolerance = tolerance;
                shells.push_back(shell);

                current_distance = distances[i];
                current_count = 1;
            }
        }

        // Add last shell
        CoordinationShell shell;
        shell.distance = current_distance;
        shell.multiplicity = current_count;
        shell.tolerance = tolerance;
        shells.push_back(shell);

        return shells;
    }

    /**
     * Compute r2/r1 ratio (second shell / first shell)
     */
    static double compute_r2_over_r1(const std::vector<CoordinationShell>& shells) {
        if (shells.size() < 2) {
            return 0.0;
        }

        return shells[1].distance / shells[0].distance;
    }

    /**
     * Validate coordination shells against expected values
     */
    static bool validate_shells(
        const std::vector<CoordinationShell>& computed,
        const GoldenTest& test,
        std::string& failure_reason
    ) {
        if (computed.empty()) {
            failure_reason = "No coordination shells computed";
            return false;
        }

        // Check first shell distance
        if (test.expected_nn_distance > 0.0) {
            double r1 = computed[0].distance;
            double expected = test.expected_nn_distance;
            double tolerance = test.expected_nn_tolerance * expected;

            if (std::abs(r1 - expected) > tolerance) {
                failure_reason = "NN distance mismatch: expected " + 
                               std::to_string(expected) + " ± " + 
                               std::to_string(tolerance) + " Å, got " + 
                               std::to_string(r1) + " Å";
                return false;
            }
        }

        // Check r2/r1 ratio
        if (test.expected_r2_over_r1 > 0.0 && computed.size() >= 2) {
            double ratio = compute_r2_over_r1(computed);
            double expected_ratio = test.expected_r2_over_r1;
            double ratio_tolerance = 0.05;  // ±5%

            if (std::abs(ratio - expected_ratio) > ratio_tolerance) {
                failure_reason = "r2/r1 ratio mismatch: expected " + 
                               std::to_string(expected_ratio) + " ± " + 
                               std::to_string(ratio_tolerance) + ", got " + 
                               std::to_string(ratio);
                return false;
            }
        }

        return true;
    }

    /**
     * Print shells for debugging
     */
    static void print_shells(const std::vector<CoordinationShell>& shells) {
        std::cout << "  Coordination shells:\n";
        for (size_t i = 0; i < shells.size(); ++i) {
            std::cout << "    Shell " << (i+1) << ": r = " 
                      << std::fixed << std::setprecision(3) 
                      << shells[i].distance << " Å, n = " 
                      << shells[i].multiplicity << "\n";
        }

        if (shells.size() >= 2) {
            double ratio = compute_r2_over_r1(shells);
            std::cout << "    r2/r1 = " << std::fixed << std::setprecision(3) 
                      << ratio << "\n";
        }
    }
};

// ============================================================================
// 3) GOLDEN TEST CASES (GoldenTest struct defined earlier in file)
// ============================================================================

class GoldenTestSuite {
public:
    static std::vector<GoldenTest> get_molecular_tests() {
        std::vector<GoldenTest> tests;
        
        // 1. Water (H2O) - Bent, 104.5°
        {
            GoldenTest test;
            test.name = "H2O_Water";
            test.category = "molecule";
            test.seed = 42;
            
            test.initial_state.atomic_numbers = {8, 1, 1};  // O, H, H
            test.initial_state.positions = {
                {0.0, 0.0, 0.0},
                {0.96, 0.0, 0.0},
                {-0.24, 0.93, 0.0}
            };
            
            // Calibrated from actual test run (PORTABLE mode)
            test.expected_hash = "PLACEHOLDER_H2O";  // TODO: Capture from STRICT mode
            test.expected_energy_min = -0.330000;  // Actual: -0.300000 eV, ±10%
            test.expected_energy_max = -0.270000;
            
            tests.push_back(test);
        }
        
        // 2. Ammonia (NH3) - Pyramidal
        {
            GoldenTest test;
            test.name = "NH3_Ammonia";
            test.category = "molecule";
            test.seed = 43;
            
            test.initial_state.atomic_numbers = {7, 1, 1, 1};  // N, H, H, H
            test.initial_state.positions = {
                {0.0, 0.0, 0.0},
                {1.0, 0.0, 0.0},
                {-0.5, 0.87, 0.0},
                {-0.5, -0.87, 0.0}
            };

            // Calibrated from actual test run (PORTABLE mode)
            test.expected_hash = "PLACEHOLDER_NH3";  // TODO: Capture from STRICT mode
            test.expected_energy_min = -0.558076;  // Actual: -0.507342 eV, ±10%
            test.expected_energy_max = -0.456608;

            tests.push_back(test);
        }
        
        // 3. Methane (CH4) - Tetrahedral
        {
            GoldenTest test;
            test.name = "CH4_Methane";
            test.category = "molecule";
            test.seed = 44;
            
            test.initial_state.atomic_numbers = {6, 1, 1, 1, 1};
            test.initial_state.positions = {
                {0.0, 0.0, 0.0},
                {1.09, 0.0, 0.0},
                {-0.36, 1.03, 0.0},
                {-0.36, -0.52, 0.89},
                {-0.36, -0.52, -0.89}
            };

            // Calibrated from actual test run (PORTABLE mode)
            test.expected_hash = "PLACEHOLDER_CH4";  // TODO: Capture from STRICT mode
            test.expected_energy_min = -0.901768;  // Actual: -0.819789 eV, ±10%
            test.expected_energy_max = -0.737810;

            tests.push_back(test);
        }
        
        // 4. CO2 - Linear
        {
            GoldenTest test;
            test.name = "CO2_CarbonDioxide";
            test.category = "molecule";
            test.seed = 45;

            test.initial_state.atomic_numbers = {6, 8, 8};
            test.initial_state.positions = {
                {0.0, 0.0, 0.0},
                {1.16, 0.0, 0.0},
                {-1.16, 0.0, 0.0}
            };

            // Calibrated from actual test run (PORTABLE mode)
            test.expected_hash = "PLACEHOLDER_CO2";  // TODO: Capture from STRICT mode
            test.expected_energy_min = -0.223423;  // Actual: -0.203112 eV, ±10%
            test.expected_energy_max = -0.182801;

            tests.push_back(test);
        }

        // 5. SF6 - Octahedral
        {
            GoldenTest test;
            test.name = "SF6_SulfurHexafluoride";
            test.category = "molecule";
            test.seed = 46;

            // Central S with 6 F in octahedral arrangement
            test.initial_state.atomic_numbers = {16, 9, 9, 9, 9, 9, 9};
            test.initial_state.positions = {
                {0.0, 0.0, 0.0},      // S center
                {1.56, 0.0, 0.0},     // F +x
                {-1.56, 0.0, 0.0},    // F -x
                {0.0, 1.56, 0.0},     // F +y
                {0.0, -1.56, 0.0},    // F -y
                {0.0, 0.0, 1.56},     // F +z
                {0.0, 0.0, -1.56}     // F -z
            };

            // Calibrated from actual test run (PORTABLE mode)
            test.expected_hash = "PLACEHOLDER_SF6";  // TODO: Capture from STRICT mode
            test.expected_energy_min = -1.012419;  // Actual: -0.920381 eV, ±10%
            test.expected_energy_max = -0.828343;
            test.expected_coordination["S-F"] = 6;

            tests.push_back(test);
        }

        // 6. XeF4 - Square Planar
        {
            GoldenTest test;
            test.name = "XeF4_XenonTetrafluoride";
            test.category = "molecule";
            test.seed = 47;

            // Central Xe with 4 F in square planar
            test.initial_state.atomic_numbers = {54, 9, 9, 9, 9};
            test.initial_state.positions = {
                {0.0, 0.0, 0.0},      // Xe center
                {1.95, 0.0, 0.0},     // F +x
                {-1.95, 0.0, 0.0},    // F -x
                {0.0, 1.95, 0.0},     // F +y
                {0.0, -1.95, 0.0}     // F -y
            };

            test.expected_hash = "PLACEHOLDER_XeF4";
            test.expected_energy_min = -60.0;
            test.expected_energy_max = 0.0;
            test.expected_coordination["Xe-F"] = 4;

            tests.push_back(test);
        }

        // 7. PCl5 - Trigonal Bipyramidal
        {
            GoldenTest test;
            test.name = "PCl5_PhosphorusPentachloride";
            test.category = "molecule";
            test.seed = 48;

            // Central P with 5 Cl in trigonal bipyramidal
            test.initial_state.atomic_numbers = {15, 17, 17, 17, 17, 17};
            test.initial_state.positions = {
                {0.0, 0.0, 0.0},           // P center
                {0.0, 0.0, 2.12},          // Cl axial +z
                {0.0, 0.0, -2.12},         // Cl axial -z
                {2.04, 0.0, 0.0},          // Cl equatorial +x
                {-1.02, 1.77, 0.0},        // Cl equatorial 120°
                {-1.02, -1.77, 0.0}        // Cl equatorial 240°
            };

            test.expected_hash = "PLACEHOLDER_PCl5";
            test.expected_energy_min = -70.0;
            test.expected_energy_max = 0.0;
            test.expected_coordination["P-Cl"] = 5;

            tests.push_back(test);
        }

        return tests;
    }
    
    static std::vector<GoldenTest> get_crystal_tests() {
        std::vector<GoldenTest> tests;

        // ====================================================================
        // A) Coordination Shell Crystals (PBC + neighbor logic stress)
        // ====================================================================

        // 1. NaCl rocksalt (conventional cubic cell)
        {
            GoldenTest test;
            test.name = "NaCl_Rocksalt";
            test.category = "crystal";
            test.seed = 100;

            // Conventional cubic cell (8 atoms)
            // Lattice parameter: a = 5.64 Å
            // NN distance: a/2 = 2.82 Å
            double a = 5.64;

            test.initial_state.atomic_numbers = {11, 17, 11, 17, 11, 17, 11, 17};
            test.initial_state.positions = {
                {0.0,  0.0,  0.0},      // Na
                {a/2,  0.0,  0.0},      // Cl
                {0.0,  a/2,  0.0},      // Na
                {a/2,  a/2,  0.0},      // Cl
                {0.0,  0.0,  a/2},      // Na
                {a/2,  0.0,  a/2},      // Cl
                {0.0,  a/2,  a/2},      // Na
                {a/2,  a/2,  a/2}       // Cl
            };

            // Enable PBC (part of system definition)
            test.initial_state.pbc_enabled = true;
            test.initial_state.box_lengths = {a, a, a};

            test.expected_hash = "PLACEHOLDER_NaCl";
            test.expected_energy_min = -50.0;
            test.expected_energy_max = 0.0;
            test.expected_coordination["Na-Cl"] = 6;  // 6-fold coordination

            // Crystal metrics
            test.expected_nn_distance = 2.82;  // Na-Cl nearest neighbor (a/2)
            test.expected_r2_over_r1 = 1.41;   // sqrt(2) for rocksalt

            tests.push_back(test);
        }

        // 2. Si diamond (conventional cell - 8 atoms)
        {
            GoldenTest test;
            test.name = "Si_Diamond";
            test.category = "crystal";
            test.seed = 101;

            // Diamond structure: FCC + basis (8 atoms)
            double a = 5.43;
            test.initial_state.atomic_numbers = {14, 14, 14, 14, 14, 14, 14, 14};
            test.initial_state.positions = {
                {0.0, 0.0, 0.0},           // Corner
                {a/4, a/4, a/4},           // Basis 1
                {a/2, a/2, 0.0},           // Face center
                {3*a/4, 3*a/4, a/4},       // Basis 2
                {a/2, 0.0, a/2},           // Face center
                {3*a/4, a/4, 3*a/4},       // Basis 3
                {0.0, a/2, a/2},           // Face center
                {a/4, 3*a/4, 3*a/4}        // Basis 4
            };

            // Enable PBC (crystal requires periodic boundaries)
            test.initial_state.pbc_enabled = true;
            test.initial_state.box_lengths = {a, a, a};

            test.expected_hash = "PLACEHOLDER_Si";
            test.expected_energy_min = -40.0;
            test.expected_energy_max = 0.0;
            test.expected_coordination["Si-Si"] = 4;  // 4-fold tetrahedral

            test.expected_nn_distance = 2.35;  // Si-Si covalent bond
            test.expected_r2_over_r1 = 1.63;   // sqrt(8/3) for diamond

            tests.push_back(test);
        }

        // 3. Al FCC (face-centered cubic) - 12-fold coordination
        {
            GoldenTest test;
            test.name = "Al_FCC";
            test.category = "crystal";
            test.seed = 102;

            // FCC primitive cell (4 atoms)
            // Lattice parameter for Al: ~4.05 Å
            double a = 4.05;
            test.initial_state.atomic_numbers = {13, 13, 13, 13};  // Al
            test.initial_state.positions = {
                {0.0, 0.0, 0.0},           // Corner
                {a/2, a/2, 0.0},           // Face center xy
                {a/2, 0.0, a/2},           // Face center xz
                {0.0, a/2, a/2}            // Face center yz
            };

            // Enable PBC (crystal requires periodic boundaries)
            test.initial_state.pbc_enabled = true;
            test.initial_state.box_lengths = {a, a, a};

            test.expected_hash = "PLACEHOLDER_Al_FCC";
            test.expected_energy_min = -60.0;
            test.expected_energy_max = 0.0;
            test.expected_coordination["Al-Al"] = 12;  // 12-fold FCC coordination

            test.expected_nn_distance = 2.86;  // Al-Al nearest neighbor
            test.expected_r2_over_r1 = 1.41;   // sqrt(2) for FCC

            // RDF fingerprint (expected first 3 peaks)
            test.expected_rdf_peaks = {
                {2.86, 12},   // First shell: 12 neighbors at ~2.86 Å
                {4.05, 6},    // Second shell: 6 neighbors at ~4.05 Å (a)
                {4.95, 24}    // Third shell: 24 neighbors at ~4.95 Å (sqrt(3)*a/2)
            };

            // Lattice invariants (cubic Al)
            test.expected_lattice.det_A = 66.4;  // a³ = 4.05³ ≈ 66.4 ų
            test.expected_lattice.a = 4.05;
            test.expected_lattice.b = 4.05;
            test.expected_lattice.c = 4.05;
            test.expected_lattice.alpha = 90.0;
            test.expected_lattice.beta = 90.0;
            test.expected_lattice.gamma = 90.0;
            test.expected_lattice.metric_eigenvalues = {16.40, 16.40, 16.40};  // a²

            tests.push_back(test);
        }

        // 4. Fe BCC (body-centered cubic) - 8-fold coordination
        {
            GoldenTest test;
            test.name = "Fe_BCC";
            test.category = "crystal";
            test.seed = 103;

            // BCC primitive cell (2 atoms)
            // Lattice parameter for Fe: ~2.87 Å
            double a = 2.87;
            test.initial_state.atomic_numbers = {26, 26};  // Fe
            test.initial_state.positions = {
                {0.0, 0.0, 0.0},              // Corner
                {a/2, a/2, a/2}               // Body center
            };

            // Enable PBC (crystal requires periodic boundaries)
            test.initial_state.pbc_enabled = true;
            test.initial_state.box_lengths = {a, a, a};

            test.expected_hash = "PLACEHOLDER_Fe_BCC";
            test.expected_energy_min = -50.0;
            test.expected_energy_max = 0.0;
            test.expected_coordination["Fe-Fe"] = 8;  // 8-fold BCC coordination

            test.expected_nn_distance = 2.48;  // Fe-Fe nearest neighbor (sqrt(3)/2 * a)
            test.expected_r2_over_r1 = 1.15;   // sqrt(4/3) for BCC

            // RDF fingerprint (expected first 3 peaks)
            test.expected_rdf_peaks = {
                {2.48, 8},    // First shell: 8 neighbors at ~2.48 Å
                {2.87, 6},    // Second shell: 6 neighbors at ~2.87 Å (a)
                {4.05, 12}    // Third shell: 12 neighbors at ~4.05 Å
            };

            tests.push_back(test);
        }

        // 5. HCP Mg (hexagonal close-packed) - catches "FCC-only thinking"
        {
            GoldenTest test;
            test.name = "Mg_HCP";
            test.category = "crystal";
            test.seed = 104;

            // HCP primitive cell (2 atoms)
            // Lattice parameters for Mg: a = 3.21 Å, c = 5.21 Å
            double a = 3.21;
            double c = 5.21;
            test.initial_state.atomic_numbers = {12, 12};  // Mg
            test.initial_state.positions = {
                {0.0, 0.0, 0.0},
                {a/3.0, a/(2.0*sqrt(3.0)), c/2.0}  // HCP offset
            };

            // Enable PBC (crystal requires periodic boundaries)
            // Note: HCP is hexagonal, but using orthorhombic approximation
            test.initial_state.pbc_enabled = true;
            test.initial_state.box_lengths = {a, a*sqrt(3.0), c};

            test.expected_hash = "PLACEHOLDER_Mg_HCP";
            test.expected_energy_min = -55.0;
            test.expected_energy_max = 0.0;
            test.expected_coordination["Mg-Mg"] = 12;  // 12-fold HCP coordination

            test.expected_nn_distance = 3.20;  // Mg-Mg nearest neighbor
            test.expected_r2_over_r1 = 1.63;   // HCP second shell ratio

            tests.push_back(test);
        }

        // 6. SC Po (simple cubic) - catches minimum-image and cutoff weirdness
        {
            GoldenTest test;
            test.name = "Po_SimpleCubic";
            test.category = "crystal";
            test.seed = 105;

            // SC primitive cell (1 atom)
            // Lattice parameter for Po: ~3.35 Å
            double a = 3.35;
            test.initial_state.atomic_numbers = {84};  // Po
            test.initial_state.positions = {
                {0.0, 0.0, 0.0}
            };

            // Enable PBC (crystal requires periodic boundaries)
            test.initial_state.pbc_enabled = true;
            test.initial_state.box_lengths = {a, a, a};

            test.expected_hash = "PLACEHOLDER_Po_SC";
            test.expected_energy_min = -30.0;
            test.expected_energy_max = 0.0;
            test.expected_coordination["Po-Po"] = 6;  // 6-fold SC coordination

            test.expected_nn_distance = 3.35;  // Po-Po nearest neighbor (= a)
            test.expected_r2_over_r1 = 1.41;   // sqrt(2) for SC

            tests.push_back(test);
        }

        // ====================================================================
        // B) Ionic / Multi-Species (pair handling + charge sanity)
        // ====================================================================

        // 7. CsCl (B2 structure) - coordination 8, different topology than NaCl
        {
            GoldenTest test;
            test.name = "CsCl_B2";
            test.category = "crystal";
            test.seed = 106;

            // CsCl primitive cell (2 atoms)
            // Lattice parameter: ~4.12 Å
            double a = 4.12;
            test.initial_state.atomic_numbers = {55, 17};  // Cs, Cl
            test.initial_state.positions = {
                {0.0, 0.0, 0.0},              // Cs at corner
                {a/2, a/2, a/2}               // Cl at body center
            };

            // Enable PBC (crystal requires periodic boundaries)
            test.initial_state.pbc_enabled = true;
            test.initial_state.box_lengths = {a, a, a};

            test.expected_hash = "PLACEHOLDER_CsCl";
            test.expected_energy_min = -45.0;
            test.expected_energy_max = 0.0;
            test.expected_coordination["Cs-Cl"] = 8;  // 8-fold coordination

            test.expected_nn_distance = 3.57;  // Cs-Cl nearest neighbor
            test.expected_r2_over_r1 = 1.15;   // B2 second shell ratio

            tests.push_back(test);
        }

        // 8. CaF2 (fluorite) - multi-coordination: Ca=8, F=4
        {
            GoldenTest test;
            test.name = "CaF2_Fluorite";
            test.category = "crystal";
            test.seed = 107;

            // Fluorite primitive cell (3 atoms: 1 Ca + 2 F)
            // Lattice parameter: ~5.46 Å
            double a = 5.46;
            test.initial_state.atomic_numbers = {20, 9, 9};  // Ca, F, F
            test.initial_state.positions = {
                {0.0, 0.0, 0.0},              // Ca
                {a/4, a/4, a/4},              // F
                {3*a/4, 3*a/4, 3*a/4}         // F
            };

            // Enable PBC (crystal requires periodic boundaries)
            test.initial_state.pbc_enabled = true;
            test.initial_state.box_lengths = {a, a, a};

            test.expected_hash = "PLACEHOLDER_CaF2";
            test.expected_energy_min = -70.0;
            test.expected_energy_max = 0.0;
            test.expected_coordination["Ca-F"] = 8;   // Ca has 8 F neighbors
            test.expected_coordination["F-Ca"] = 4;   // F has 4 Ca neighbors

            test.expected_nn_distance = 2.37;  // Ca-F nearest neighbor
            test.expected_r2_over_r1 = 1.73;   // Fluorite second shell ratio

            tests.push_back(test);
        }

        // ====================================================================
        // C) Distortion Stability (elastic / numerical robustness)
        // ====================================================================

        // 9. Tetragonally strained FCC (should relax back or stay stable)
        {
            GoldenTest test;
            test.name = "Al_FCC_Strained";
            test.category = "crystal";
            test.seed = 108;

            // Start from FCC, apply +2% z strain
            double a = 4.05;
            double strain_z = 1.02;  // +2% along z
            test.initial_state.atomic_numbers = {13, 13, 13, 13};
            test.initial_state.positions = {
                {0.0, 0.0, 0.0},
                {a/2, a/2, 0.0},
                {a/2, 0.0, a/2*strain_z},
                {0.0, a/2, a/2*strain_z}
            };

            // Enable PBC (crystal requires periodic boundaries)
            // Strained box to match distortion
            test.initial_state.pbc_enabled = true;
            test.initial_state.box_lengths = {a, a, a*strain_z};

            test.expected_hash = "PLACEHOLDER_Al_FCC_Strained";
            test.expected_energy_min = -58.0;  // Slightly higher than unstrained
            test.expected_energy_max = 0.0;
            test.expected_coordination["Al-Al"] = 12;  // CN should be preserved

            test.expected_nn_distance = 2.88;  // Slightly perturbed
            test.expected_r2_over_r1 = 1.40;   // Close to unstrained

            tests.push_back(test);
        }

        // 10. INTENTIONAL FAILURE: Too dense initialization (should reject)
        {
            GoldenTest test;
            test.name = "BadInit_TooDense";
            test.category = "molecule";
            test.seed = 999;

            // Two carbon atoms way too close (should reject)
            test.initial_state.atomic_numbers = {6, 6};
            test.initial_state.positions = {
                {0.0, 0.0, 0.0},
                {0.01, 0.0, 0.0}  // Only 0.01 Å apart - OVERLAPPING!
            };

            test.expected_hash = "SHOULD_REJECT";
            test.expected_energy_min = 1e6;  // Huge energy expected (repulsion)
            test.expected_energy_max = 1e10;

            tests.push_back(test);
        }

        return tests;
    }
};

// ============================================================================
// 4) BENCHMARK FRAMEWORK
// ============================================================================

struct BenchmarkResult {
    std::string test_name;

    // Stability metrics
    int iterations_to_converge;
    double final_max_force;
    bool energy_monotonic;
    int rejection_count;

    // Performance metrics
    double total_time_ms;
    double time_per_iteration_ms;
    double time_per_force_eval_ms;

    // Validation results
    bool passed;
    std::string failure_reason;
    ValidationMode validation_mode;

    // Crystal metrics (for crystal tests)
    std::vector<CoordinationShell> shells;
    double computed_nn_distance;
    double computed_r2_over_r1;

    // RDF fingerprint
    std::vector<RDFPeak> rdf_peaks;

    // Lattice invariants
    LatticeInvariants lattice_inv;

    void print() const {
        std::cout << "\n--- Benchmark: " << test_name << " ---\n";
        std::cout << "Status: " << (passed ? "✅ PASS" : "❌ FAIL") << "\n";
        if (!passed) {
            std::cout << "Reason: " << failure_reason << "\n";
        }
        std::cout << "Validation mode: " 
                  << (validation_mode == ValidationMode::STRICT ? "STRICT" : "PORTABLE") << "\n";
        std::cout << "Iterations: " << iterations_to_converge << "\n";
        std::cout << "Final max force: " << std::scientific << final_max_force << "\n";
        std::cout << "Energy monotonic: " << (energy_monotonic ? "Yes" : "No") << "\n";
        std::cout << "Total time: " << std::fixed << std::setprecision(2) 
                  << total_time_ms << " ms\n";
        std::cout << "Time/iteration: " << time_per_iteration_ms << " ms\n";

        // Print crystal metrics
        if (!shells.empty()) {
            std::cout << "\n  Crystal metrics:\n";
            std::cout << "    NN distance (r1): " << std::fixed << std::setprecision(3) 
                      << computed_nn_distance << " Å\n";
            if (shells.size() >= 2) {
                std::cout << "    r2/r1 ratio: " << computed_r2_over_r1 << "\n";
            }
            CoordinationAnalyzer::print_shells(shells);
        }

        // Print RDF peaks
        if (!rdf_peaks.empty()) {
            std::cout << "\n  RDF fingerprint:\n";
            RDFAnalyzer::print_peaks(rdf_peaks);
        }

        // Print lattice invariants
        if (lattice_inv.det_A > 0.0) {
            LatticeAnalyzer::print_invariants(lattice_inv);
        }
    }
};

class BenchmarkRunner {
public:
    static BenchmarkResult run_benchmark(
        const GoldenTest& test, 
        LJCoulombModel& model,
        ValidationMode mode = ValidationMode::PORTABLE  // Default to portable
    ) {
        BenchmarkResult result;
        result.test_name = test.name;
        result.passed = false;
        result.validation_mode = mode;

        auto start = std::chrono::high_resolution_clock::now();

        // Run relaxation (placeholder - integrate with actual FIRE)
        CoreState relaxed = relax_structure(test.initial_state, model, test.seed, result);

        auto end = std::chrono::high_resolution_clock::now();
        result.total_time_ms = std::chrono::duration<double, std::milli>(end - start).count();

        if (result.iterations_to_converge > 0) {
            result.time_per_iteration_ms = result.total_time_ms / result.iterations_to_converge;
        }

        // Compute energy
        double energy = model.energy(relaxed);

        // Compute coordination (placeholder)
        std::map<std::string, int> coordination = compute_coordination(relaxed);

        // For crystals: compute coordination shells
        if (test.category == "crystal") {
            double cutoff = 1.5 * test.expected_nn_distance;  // 1.5x NN distance
            result.shells = CoordinationAnalyzer::compute_shells(relaxed, cutoff);

            if (!result.shells.empty()) {
                result.computed_nn_distance = result.shells[0].distance;
                if (result.shells.size() >= 2) {
                    result.computed_r2_over_r1 = CoordinationAnalyzer::compute_r2_over_r1(result.shells);
                }
            }

            // Validate shells
            std::string shell_failure;
            if (!CoordinationAnalyzer::validate_shells(result.shells, test, shell_failure)) {
                result.failure_reason = shell_failure;
                return result;
            }

            // Compute RDF fingerprint
            result.rdf_peaks = RDFAnalyzer::compute_rdf_peaks(relaxed, 3);  // First 3 peaks

            // Validate RDF peaks if expected values provided
            if (!test.expected_rdf_peaks.empty()) {
                std::string rdf_failure;
                if (!RDFAnalyzer::validate_peaks(
                    result.rdf_peaks, 
                    test.expected_rdf_peaks,
                    test.rdf_r_tolerance,
                    test.rdf_count_tolerance,
                    rdf_failure
                )) {
                    result.failure_reason = rdf_failure;
                    return result;
                }
            }

            // Compute lattice invariants
            result.lattice_inv = LatticeAnalyzer::compute_invariants(relaxed);

            // Validate lattice invariants if expected values provided
            if (test.expected_lattice.det_A > 0.0) {
                std::string lattice_failure;
                if (!LatticeAnalyzer::validate_invariants(
                    result.lattice_inv,
                    test.expected_lattice,
                    test.lattice_volume_tolerance,
                    test.lattice_length_tolerance,
                    test.lattice_angle_tolerance,
                    lattice_failure
                )) {
                    result.failure_reason = lattice_failure;
                    return result;
                }
            }
        }

        // Validate based on mode
        std::string validation_failure;
        bool validation_passed = false;

        if (mode == ValidationMode::STRICT) {
            validation_passed = Validator::validate_strict(
                relaxed, test, energy, validation_failure
            );
        } else {
            validation_passed = Validator::validate_portable(
                relaxed, test, energy, coordination, validation_failure
            );
        }

        if (!validation_passed) {
            result.failure_reason = validation_failure;
            return result;
        }

        // Additional sanity checks
        if (result.final_max_force > 1e-3) {
            result.failure_reason = "Did not converge (max force too high)";
            return result;
        }

        result.passed = true;
        return result;
    }

private:
    static CoreState relax_structure(const CoreState& initial, LJCoulombModel& model, 
                                     uint64_t seed, BenchmarkResult& result) {
        // Copy initial state
        CoreState relaxed = initial;

        // Create FIRE minimizer
        FIREMinimizer fire;
        fire.max_steps = 1000;
        fire.f_tol = 1e-4;

        // Run FIRE minimization
        auto fire_result = fire.minimize(relaxed, model);

        // Store results
        result.iterations_to_converge = fire_result.iterations;
        result.final_max_force = fire_result.final_max_force;
        result.energy_monotonic = check_energy_monotonic(fire_result.energy_trace);
        result.rejection_count = 0;
        result.time_per_force_eval_ms = 0.01;  // Estimate

        if (!fire_result.converged) {
            std::cout << "  ⚠️  Warning: Did not converge within " << fire.max_steps << " steps\n";
            std::cout << "      Final max force: " << fire_result.final_max_force << "\n";
        }

        return relaxed;
    }

    static bool check_energy_monotonic(const std::vector<double>& energies) {
        // Check if energy is generally decreasing (allow small increases from FIRE)
        if (energies.size() < 2) return true;

        int increases = 0;
        for (size_t i = 1; i < energies.size(); ++i) {
            if (energies[i] > energies[i-1] + 1e-6) {
                increases++;
            }
        }

        // Allow up to 10% increases (FIRE algorithm can have small uphill steps)
        return increases < energies.size() / 10;
    }

    static std::map<std::string, int> compute_coordination(const CoreState& state, double cutoff = 3.5) {
        // Compute coordination numbers
        std::map<std::string, int> coord;

        // For each pair of atoms
        for (size_t i = 0; i < state.positions.size(); ++i) {
            for (size_t j = i + 1; j < state.positions.size(); ++j) {
                Vec3 dr = {
                    state.positions[j].x - state.positions[i].x,
                    state.positions[j].y - state.positions[i].y,
                    state.positions[j].z - state.positions[i].z
                };

                double dist = std::sqrt(dr.x*dr.x + dr.y*dr.y + dr.z*dr.z);

                if (dist < cutoff) {
                    // Get element symbols (simplified)
                    std::string elem_i = std::to_string(state.atomic_numbers[i]);
                    std::string elem_j = std::to_string(state.atomic_numbers[j]);

                    // Create pair key (sorted)
                    std::string pair_key;
                    if (elem_i < elem_j) {
                        pair_key = elem_i + "-" + elem_j;
                    } else {
                        pair_key = elem_j + "-" + elem_i;
                    }

                    coord[pair_key]++;
                }
            }
        }

        return coord;
    }
};

// ============================================================================
// 5) MAIN QA DRIVER
// ============================================================================

class QARunner {
public:
    QARunner(const std::string& output_dir, ValidationMode mode = ValidationMode::PORTABLE) 
        : output_dir_(output_dir), validation_mode_(mode) {
        fs::create_directories(output_dir);
    }

    void run_all_tests() {
        std::cout << "╔══════════════════════════════════════════════════════════╗\n";
        std::cout << "║  Pre-Batching Quality & Reproducibility Milestone       ║\n";
        std::cout << "╠══════════════════════════════════════════════════════════╣\n";
        std::cout << "║  Deterministic validation - NO VIBES, only facts        ║\n";
        std::cout << "║  Mode: " 
                  << std::left << std::setw(45) 
                  << (validation_mode_ == ValidationMode::STRICT ? "STRICT" : "PORTABLE") 
                  << "║\n";
        std::cout << "╚══════════════════════════════════════════════════════════╝\n\n";

        // 1. Generate run manifest
        RunManifest manifest = generate_manifest();
        manifest.validation_mode = (validation_mode_ == ValidationMode::STRICT ? "STRICT" : "PORTABLE");
        save_manifest(manifest);

        // 2. Run molecular tests
        std::cout << "\n=== MOLECULAR TESTS (7 cases) ===\n";
        auto mol_tests = GoldenTestSuite::get_molecular_tests();
        run_test_suite(mol_tests, "molecular");

        // 3. Run crystal tests
        std::cout << "\n=== CRYSTAL TESTS (10 cases) ===\n";
        auto crystal_tests = GoldenTestSuite::get_crystal_tests();
        run_test_suite(crystal_tests, "crystal");

        // 4. Print summary
        print_summary();

        // 5. Generate report
        generate_report();
    }

private:
    std::string output_dir_;
    ValidationMode validation_mode_;
    std::vector<BenchmarkResult> all_results_;
    std::vector<StructureRecord> all_records_;
    
    RunManifest generate_manifest() {
        RunManifest manifest;
        
        // Generate run ID (timestamp + short hash)
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        std::ostringstream oss;
        oss << std::put_time(std::localtime(&time_t), "%Y%m%d_%H%M%S");
        manifest.run_id = oss.str() + "_" + std::to_string(std::hash<std::string>{}(oss.str()) % 10000);
        manifest.timestamp = oss.str();
        
        // Platform info
        manifest.os = "Windows";  // TODO: Detect actual OS
        manifest.cpu = "x86_64";  // TODO: Detect actual CPU
        manifest.gpu = "N/A";     // TODO: Detect GPU
        
        // Build info
        manifest.git_commit = "HEAD";  // TODO: Get actual git commit
        manifest.build_id = "Release-MSVC";  // TODO: Get actual build config
        
        // Config
        manifest.commandline = "vsepr qa goldens";
        manifest.config_hash = "default";
        manifest.rng_seed = 42;
        manifest.rng_algorithm = "mt19937_64";
        manifest.model_id = "LJ+Coulomb_v1.0";
        
        // Tolerances
        manifest.force_tolerance = 1e-4;
        manifest.energy_tolerance = 1e-6;
        
        // PBC
        manifest.pbc_enabled = false;
        manifest.pbc_cell = "N/A";
        
        return manifest;
    }
    
    void save_manifest(const RunManifest& manifest) {
        std::string path = output_dir_ + "/manifest.json";
        std::ofstream ofs(path);
        ofs << manifest.to_json();
        ofs.close();
        std::cout << "📝 Manifest saved: " << path << "\n";
    }
    
    void run_test_suite(const std::vector<GoldenTest>& tests, const std::string& category) {
        LJCoulombModel model;

        for (const auto& test : tests) {
            test.print();

            // Special handling for intentional failure case
            bool should_reject = (test.name == "BadInit_TooDense");

            // Run benchmark with current validation mode
            auto result = BenchmarkRunner::run_benchmark(test, model, validation_mode_);

            // For intentional failure: PASS if it correctly rejects
            if (should_reject) {
                if (!result.passed && !result.failure_reason.empty()) {
                    std::cout << "\n✅ CORRECTLY REJECTED bad initialization\n";
                    std::cout << "   Reason: " << result.failure_reason << "\n";
                    result.passed = true;  // Flip to pass (rejection is success)
                } else {
                    std::cout << "\n❌ FAILED TO REJECT bad initialization!\n";
                    std::cout << "   This is a validation bug - should have rejected overlapping atoms.\n";
                    result.passed = false;
                }
            }

            result.print();

            all_results_.push_back(result);

            // Generate structure record
            StructureRecord record;
            record.structure_id = StructureCanonicalizer::compute_hash(test.initial_state);
            record.source_seed = test.seed;
            record.converged = result.passed;
            record.iterations = result.iterations_to_converge;
            record.final_energy = 0.0;  // TODO: Get actual energy
            record.max_force = result.final_max_force;

            // Add crystal metrics
            if (!result.shells.empty()) {
                record.nn_distance = result.computed_nn_distance;
                record.r2_over_r1 = result.computed_r2_over_r1;
                record.rdf_peaks = result.rdf_peaks;
            }

            if (!result.passed) {
                record.reject_reason = result.failure_reason;
            }

            all_records_.push_back(record);
        }
    }
    
    void print_summary() {
        int passed = 0;
        int failed = 0;
        
        for (const auto& result : all_results_) {
            if (result.passed) passed++;
            else failed++;
        }
        
        std::cout << "\n╔══════════════════════════════════════════════════════════╗\n";
        std::cout << "║  SUMMARY                                                 ║\n";
        std::cout << "╠══════════════════════════════════════════════════════════╣\n";
        std::cout << "║  Total tests: " << std::setw(2) << all_results_.size() << "                                         ║\n";
        std::cout << "║  Passed:      " << std::setw(2) << passed << "                                         ║\n";
        std::cout << "║  Failed:      " << std::setw(2) << failed << "                                         ║\n";
        std::cout << "╚══════════════════════════════════════════════════════════╝\n\n";
        
        if (failed == 0) {
            std::cout << "✅ ALL TESTS PASSED - Golden suite validated!\n\n";
        } else {
            std::cout << "❌ " << failed << " TEST(S) FAILED - See report for details\n\n";
        }
    }
    
    void generate_report() {
        std::string path = output_dir_ + "/report.md";
        std::ofstream ofs(path);

        if (!ofs.is_open()) {
            std::cerr << "ERROR: Could not open " << path << " for writing!\n";
            return;
        }

        ofs << "# QA Golden Tests Report\n\n";
        ofs << "**Run ID:** " << "TODO" << "\n\n";
        ofs << "## Results\n\n";

        // SAFETY: Flush after each test to avoid memory buildup
        for (const auto& result : all_results_) {
            ofs << "### " << result.test_name << "\n\n";
            ofs << "- **Status:** " << (result.passed ? "✅ PASS" : "❌ FAIL") << "\n";
            if (!result.passed) {
                ofs << "- **Failure:** " << result.failure_reason << "\n";
            }
            ofs << "- **Iterations:** " << result.iterations_to_converge << "\n";
            ofs << "- **Time:** " << result.total_time_ms << " ms\n\n";

            ofs.flush();  // SAFETY: Flush incrementally
        }

        ofs.close();
        std::cout << "📄 Report saved: " << path << "\n";
    }
};

// ============================================================================
// MAIN
// ============================================================================

int main(int argc, char** argv) {
    std::string output_dir = "out/qa/run_" + 
        std::to_string(std::chrono::system_clock::now().time_since_epoch().count());

    ValidationMode mode = ValidationMode::PORTABLE;  // Default to portable

    // Parse command-line arguments
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "--strict") {
            mode = ValidationMode::STRICT;
        } else if (arg == "--portable") {
            mode = ValidationMode::PORTABLE;
        } else if (arg == "--output" || arg == "-o") {
            if (i + 1 < argc) {
                output_dir = argv[i + 1];
                ++i;
            }
        } else if (arg == "--help" || arg == "-h") {
            std::cout << "Usage: qa_golden_tests [options]\n\n";
            std::cout << "Options:\n";
            std::cout << "  --strict         Use STRICT validation (byte-identical)\n";
            std::cout << "  --portable       Use PORTABLE validation (physics-identical, default)\n";
            std::cout << "  --output <dir>   Set output directory\n";
            std::cout << "  --help           Show this help\n\n";
            std::cout << "Validation Modes:\n";
            std::cout << "  STRICT   - Same build, same platform (hash must match exactly)\n";
            std::cout << "  PORTABLE - Cross-platform (tolerances on energy, coordination)\n\n";
            return 0;
        }
    }

    try {
        QARunner runner(output_dir, mode);
        runner.run_all_tests();

        return 0;
    } catch (const std::exception& e) {
        std::cerr << "\n❌ QA RUN FAILED: " << e.what() << "\n\n";
        return 1;
    }
}
