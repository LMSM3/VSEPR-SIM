#pragma once
/**
 * emergence_dataset.hpp — Emergence Dataset #1:
 *     Anisotropy, Isomer Stability, and Fall Transitions
 * ====================================================================
 *
 * A structured benchmark for directional-response and state-transition
 * phenomena across molecular, cluster, and coarse-grained systems.
 *
 * Records:
 *   - Initial and final structural states
 *   - Anisotropy descriptors (geometric, mechanical, transport, field)
 *   - Driving perturbations (thermal, field, flow, stress, radiation)
 *   - Transition barriers and persistence times
 *   - Fall-event classifications with severity scores
 *
 * Purpose:
 *   Support deterministic simulation studies of symmetry breaking,
 *   metastability, conformational switching, field-induced transitions,
 *   and anisotropy-driven collapse events within the VSEPR-SIM
 *   architecture.
 *
 * Dataset organisation:
 *   ED1A  Molecular         (torsions, ring flips, cis/trans)
 *   ED1B  Coordination      (ligand distortions, metal-center isomers)
 *   ED1C  Bead/Structure    (directional collapse, branch reorientation)
 *
 * Isomer Fall (project definition):
 *   := a discrete transition from one local structural state to
 *      another under perturbation.
 *
 * Fall detection criteria (any ONE triggers):
 *   1. ΔE_barrier < E_perturb
 *   2. RMSD(X_t, X_ref) > τ_geom
 *   3. q_state(t + Δt) ≠ q_state(t)
 *
 * Anti-black-box: every field, every threshold, every label is
 * explicitly inspectable. No hidden state.
 *
 * Terminology: atomistic throughout (no meso).
 */

#include "atomistic/core/state.hpp"
#include "atomistic/core/state_c.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <numeric>
#include <sstream>
#include <string>
#include <vector>

namespace atomistic {
namespace emergence {

// ============================================================================
// Dataset Family Tags
// ============================================================================

enum class DatasetFamily : uint8_t {
    ED1A_Molecular    = 0,   // Small molecules, torsions, ring flips, cis/trans
    ED1B_Coordination = 1,   // Metal centers, ligand distortions, actinide assemblies
    ED1C_BeadStructure = 2   // Directional collapse, branch reorientation
};

inline const char* family_name(DatasetFamily f) {
    switch (f) {
        case DatasetFamily::ED1A_Molecular:     return "ED1A-Molecular";
        case DatasetFamily::ED1B_Coordination:  return "ED1B-Coordination";
        case DatasetFamily::ED1C_BeadStructure: return "ED1C-Bead/Structure";
        default:                                return "unknown";
    }
}

// ============================================================================
// System Classification
// ============================================================================

enum class SystemClass : uint8_t {
    SmallMolecule       = 0,
    CoordinationComplex = 1,
    Cluster             = 2,
    Organometallic      = 3,
    CrystalFragment     = 4,
    BeadAssembly        = 5,
    Lattice             = 6
};

inline const char* system_class_name(SystemClass c) {
    switch (c) {
        case SystemClass::SmallMolecule:       return "small_molecule";
        case SystemClass::CoordinationComplex: return "coordination_complex";
        case SystemClass::Cluster:             return "cluster";
        case SystemClass::Organometallic:      return "organometallic";
        case SystemClass::CrystalFragment:     return "crystal_fragment";
        case SystemClass::BeadAssembly:        return "bead_assembly";
        case SystemClass::Lattice:             return "lattice";
        default:                               return "unknown";
    }
}

// ============================================================================
// Perturbation Driver — what caused the transition
// ============================================================================

enum class DriverType : uint8_t {
    Thermal        = 0,
    ElectricField  = 1,
    MagneticField  = 2,
    Flow           = 3,
    Stress         = 4,
    Collision      = 5,
    Radiation      = 6,
    Concentration  = 7,
    Composite      = 8    // Multiple simultaneous drivers
};

inline const char* driver_type_name(DriverType d) {
    switch (d) {
        case DriverType::Thermal:       return "thermal";
        case DriverType::ElectricField: return "electric_field";
        case DriverType::MagneticField: return "magnetic_field";
        case DriverType::Flow:          return "flow";
        case DriverType::Stress:        return "stress";
        case DriverType::Collision:     return "collision";
        case DriverType::Radiation:     return "radiation";
        case DriverType::Concentration: return "concentration";
        case DriverType::Composite:     return "composite";
        default:                        return "unknown";
    }
}

/**
 * PerturbationDriver — full description of the driving force.
 */
struct PerturbationDriver {
    DriverType type{DriverType::Thermal};
    double     magnitude{0.0};       // Energy or field strength (native units)
    Vec3       direction{};          // Direction vector (if applicable)
    double     duration{0.0};        // Duration of perturbation (ps)
    std::string unit;                // Unit string for magnitude
};

// ============================================================================
// Anisotropy Type and Descriptors
// ============================================================================

enum class AnisotropyType : uint8_t {
    Geometric   = 0,   // Shape tensor eigenvalue ratio
    Mechanical  = 1,   // Directional modulus ratio
    Transport   = 2,   // Diffusion or conductivity ratio
    Field       = 3,   // Field-response directional ratio
    Optical     = 4    // Refractive index ratio (if relevant)
};

inline const char* anisotropy_type_name(AnisotropyType a) {
    switch (a) {
        case AnisotropyType::Geometric:  return "geometric";
        case AnisotropyType::Mechanical: return "mechanical";
        case AnisotropyType::Transport:  return "transport";
        case AnisotropyType::Field:      return "field";
        case AnisotropyType::Optical:    return "optical";
        default:                         return "unknown";
    }
}

/**
 * AnisotropyDescriptor — quantified directional property variation.
 *
 * Anisotropy is NOT a vibe. It is:
 *
 *   A_geom  = λ₁/λ₃          (eigenvalue ratio of shape tensor)
 *   A_mech  = P_max/P_min     (modulus or compliance)
 *   A_trans = D_∥/D_⊥         (parallel/perpendicular diffusion)
 *   A_field = E_max∥/E_max⊥   (field response)
 *   A_opt   = n_∥/n_⊥         (refractive index)
 *
 * All ratios are ≥ 1.0 for anisotropic systems. A ratio of 1.0
 * means perfectly isotropic.
 */
struct AnisotropyDescriptor {
    AnisotropyType type{AnisotropyType::Geometric};
    double ratio{1.0};               // A ≥ 1.0 (1.0 = isotropic)

    // Principal axes (eigenvectors of the relevant tensor)
    Vec3   axis_1{};                 // Primary axis (largest eigenvalue)
    Vec3   axis_2{};                 // Secondary axis
    Vec3   axis_3{};                 // Tertiary axis (smallest eigenvalue)

    // Eigenvalues of the relevant tensor
    double eigenvalues[3]{};         // λ₁ ≥ λ₂ ≥ λ₃

    // Derived
    double asphericity{0.0};         // κ (same convention as inertia_frame.hpp)

    bool is_isotropic(double tol = 1.05) const { return ratio < tol; }
};

/**
 * Compute geometric anisotropy from a set of positions.
 *
 * Uses the gyration tensor (mass-weighted if masses provided):
 *   G_ab = (1/M) Σ m_i (r_ia - com_a)(r_ib - com_b)
 *
 * Returns descriptor with eigenvalue ratio λ₁/λ₃.
 */
inline AnisotropyDescriptor compute_geometric_anisotropy(
    const std::vector<Vec3>& positions,
    const std::vector<double>& masses)
{
    AnisotropyDescriptor desc;
    desc.type = AnisotropyType::Geometric;

    const size_t N = positions.size();
    if (N < 2 || masses.size() != N) return desc;

    // Center of mass
    double M = 0.0;
    Vec3 com{};
    for (size_t i = 0; i < N; ++i) {
        M += masses[i];
        com.x += masses[i] * positions[i].x;
        com.y += masses[i] * positions[i].y;
        com.z += masses[i] * positions[i].z;
    }
    if (M < 1e-30) return desc;
    com = com * (1.0 / M);

    // Gyration tensor (3×3 symmetric)
    double G[3][3]{};
    for (size_t i = 0; i < N; ++i) {
        double dx = positions[i].x - com.x;
        double dy = positions[i].y - com.y;
        double dz = positions[i].z - com.z;
        double w = masses[i] / M;
        G[0][0] += w * dx * dx;
        G[0][1] += w * dx * dy;
        G[0][2] += w * dx * dz;
        G[1][1] += w * dy * dy;
        G[1][2] += w * dy * dz;
        G[2][2] += w * dz * dz;
    }
    G[1][0] = G[0][1];
    G[2][0] = G[0][2];
    G[2][1] = G[1][2];

    // Jacobi eigenvalue iteration (same as inertia_frame.hpp)
    double A[3][3], V[3][3]{};
    for (int r = 0; r < 3; ++r)
        for (int c = 0; c < 3; ++c)
            A[r][c] = G[r][c];
    V[0][0] = V[1][1] = V[2][2] = 1.0;

    for (int iter = 0; iter < 50; ++iter) {
        double max_off = 0.0;
        int p = 0, q = 1;
        for (int i = 0; i < 3; ++i)
            for (int j = i + 1; j < 3; ++j)
                if (std::abs(A[i][j]) > max_off) {
                    max_off = std::abs(A[i][j]);
                    p = i; q = j;
                }
        if (max_off < 1e-14) break;

        double app = A[p][p], aqq = A[q][q], apq = A[p][q];
        if (std::abs(apq) < 1e-15) continue;
        double tau = (aqq - app) / (2.0 * apq);
        double t = (tau >= 0)
            ?  1.0 / ( tau + std::sqrt(1.0 + tau * tau))
            : -1.0 / (-tau + std::sqrt(1.0 + tau * tau));
        double c_val = 1.0 / std::sqrt(1.0 + t * t);
        double s_val = t * c_val;

        A[p][p] = app - t * apq;
        A[q][q] = aqq + t * apq;
        A[p][q] = A[q][p] = 0.0;
        for (int r = 0; r < 3; ++r) {
            if (r == p || r == q) continue;
            double arp = A[r][p], arq = A[r][q];
            A[r][p] = A[p][r] = c_val * arp - s_val * arq;
            A[r][q] = A[q][r] = s_val * arp + c_val * arq;
        }
        for (int r = 0; r < 3; ++r) {
            double vrp = V[r][p], vrq = V[r][q];
            V[r][p] = c_val * vrp - s_val * vrq;
            V[r][q] = s_val * vrp + c_val * vrq;
        }
    }

    // Extract and sort eigenvalues descending (λ₁ ≥ λ₂ ≥ λ₃)
    double evals[3] = {A[0][0], A[1][1], A[2][2]};
    int order[3] = {0, 1, 2};
    if (evals[order[0]] < evals[order[1]]) std::swap(order[0], order[1]);
    if (evals[order[1]] < evals[order[2]]) std::swap(order[1], order[2]);
    if (evals[order[0]] < evals[order[1]]) std::swap(order[0], order[1]);

    desc.eigenvalues[0] = evals[order[0]];
    desc.eigenvalues[1] = evals[order[1]];
    desc.eigenvalues[2] = evals[order[2]];

    desc.axis_1 = {V[0][order[0]], V[1][order[0]], V[2][order[0]]};
    desc.axis_2 = {V[0][order[1]], V[1][order[1]], V[2][order[1]]};
    desc.axis_3 = {V[0][order[2]], V[1][order[2]], V[2][order[2]]};

    // Anisotropy ratio: λ₁/λ₃
    if (desc.eigenvalues[2] > 1e-30)
        desc.ratio = desc.eigenvalues[0] / desc.eigenvalues[2];
    else
        desc.ratio = 1e12;  // Effectively degenerate (planar or linear)

    // Asphericity: κ = 1 - 3(λ₁λ₂ + λ₂λ₃ + λ₃λ₁)/(λ₁+λ₂+λ₃)²
    double l1 = desc.eigenvalues[0];
    double l2 = desc.eigenvalues[1];
    double l3 = desc.eigenvalues[2];
    double trace = l1 + l2 + l3;
    if (trace > 1e-30) {
        double cross = l1*l2 + l2*l3 + l3*l1;
        desc.asphericity = 1.0 - 3.0 * cross / (trace * trace);
    }

    return desc;
}

// ============================================================================
// Fall Mode — categorical label for the transition type
// ============================================================================

enum class FallMode : uint8_t {
    RotationFall             = 0,
    TorsionFall              = 1,
    CoordinationFall         = 2,
    BucklingFall             = 3,
    CollapseFall             = 4,
    LigandSwapFall           = 5,
    ElectrostaticReorientation = 6,
    ThermalIsomerization     = 7,
    FieldInducedIsomerization = 8,
    SurfaceBindingFall       = 9,
    ConformerFall            = 10,
    StructuralFall           = 11,
    OrientationFall          = 12,
    None                     = 255
};

inline const char* fall_mode_name(FallMode m) {
    switch (m) {
        case FallMode::RotationFall:              return "rotation_fall";
        case FallMode::TorsionFall:               return "torsion_fall";
        case FallMode::CoordinationFall:          return "coordination_fall";
        case FallMode::BucklingFall:              return "buckling_fall";
        case FallMode::CollapseFall:              return "collapse_fall";
        case FallMode::LigandSwapFall:            return "ligand_swap_fall";
        case FallMode::ElectrostaticReorientation: return "electrostatic_reorientation";
        case FallMode::ThermalIsomerization:      return "thermal_isomerization";
        case FallMode::FieldInducedIsomerization: return "field_induced_isomerization";
        case FallMode::SurfaceBindingFall:        return "surface_binding_fall";
        case FallMode::ConformerFall:             return "conformer_fall";
        case FallMode::StructuralFall:            return "structural_fall";
        case FallMode::OrientationFall:           return "orientation_fall";
        case FallMode::None:                      return "none";
        default:                                  return "unknown";
    }
}

// ============================================================================
// Fall Severity Score
// ============================================================================

/**
 * FallSeverityWeights — configurable weights for the severity score.
 *
 *   S_f = w1 * RMSD/RMSD_max + w2 * |ΔE|/|ΔE|_max + w3 * τ_persist/τ_max
 *
 * Result in [0, 1].
 */
struct FallSeverityWeights {
    double w1{0.4};         // RMSD weight
    double w2{0.4};         // Energy weight
    double w3{0.2};         // Persistence time weight

    double rmsd_max{5.0};   // Normalisation: max expected RMSD (Å)
    double dE_max{50.0};    // Normalisation: max expected |ΔE| (kcal/mol)
    double tau_max{1000.0}; // Normalisation: max expected persistence (ps)
};

inline double compute_fall_severity(
    double rmsd, double delta_E, double persistence_time,
    const FallSeverityWeights& w = {})
{
    double s = w.w1 * std::min(rmsd / w.rmsd_max, 1.0)
             + w.w2 * std::min(std::abs(delta_E) / w.dE_max, 1.0)
             + w.w3 * std::min(persistence_time / w.tau_max, 1.0);
    return std::clamp(s, 0.0, 1.0);
}

// ============================================================================
// Isomer-Fall Detection Thresholds
// ============================================================================

/**
 * FallDetectionConfig — strict event definition thresholds.
 *
 * A sample gets tagged as an isomer-fall event if ANY of:
 *   1. ΔE_barrier < E_perturb
 *   2. RMSD(X_t, X_ref) > τ_geom
 *   3. q_state(t + Δt) ≠ q_state(t)
 *
 * These are not tuning knobs. They define what counts as a fall.
 */
struct FallDetectionConfig {
    double rmsd_threshold{0.5};       // τ_geom (Å): structural displacement threshold
    double barrier_ratio_min{1.0};    // If ΔE_barrier / E_perturb < this, fall detected
    bool   check_state_label{true};   // Check q_state change?
};

// ============================================================================
// State Label — discrete structural state identifier
// ============================================================================

/**
 * StateLabel — identifies the isomer/conformer/structural state.
 *
 * The label is the q_state in the fall detection criterion:
 *   q_state(t + Δt) ≠ q_state(t)  =>  isomer fall
 */
struct StateLabel {
    std::string name;          // "anti", "gauche", "chair", "boat", "fac", "mer", etc.
    std::string geometry;      // VSEPR class: "tetrahedral", "octahedral", etc.
    uint32_t    coordination{0}; // Coordination number
    std::string bond_order_sig;  // Bond order signature string

    bool operator==(const StateLabel& o) const {
        return name == o.name && geometry == o.geometry
            && coordination == o.coordination;
    }
    bool operator!=(const StateLabel& o) const { return !(*this == o); }
};

// ============================================================================
// Emergence Sample — one row in the dataset
// ============================================================================

/**
 * EmergenceSample — the fundamental record in ED1.
 *
 * Minimal schema:
 *   X_i = (id, system_class, composition, state_0, state_1,
 *          anisotropy_vector, driver, driver_magnitude, timescale,
 *          ΔE, ΔG, RMSD, fall_flag, fall_mode)
 *
 * Expanded schema adds: charge_state, mass, shape_descriptor,
 * local_coordination, residence_times, recovery, hysteresis, notes.
 */
struct EmergenceSample {
    // --- Identity ---
    uint64_t       sample_id{0};
    DatasetFamily  family{DatasetFamily::ED1A_Molecular};
    SystemClass    system_class{SystemClass::SmallMolecule};
    std::string    composition;          // Chemical formula or bead formula
    int            charge_state{0};      // Net charge
    double         mass{0.0};            // Total mass (amu)

    // --- States ---
    StateLabel     initial_state;
    StateLabel     final_state;
    std::string    transition_class;     // "conformer", "isomer", "coordination", etc.

    // --- Anisotropy ---
    AnisotropyDescriptor anisotropy;

    // --- Driver ---
    PerturbationDriver   driver;

    // --- Thermodynamic conditions ---
    double temperature{300.0};           // K
    double pressure{1.0};               // atm
    std::string medium;                  // "vacuum", "water", "ethanol", etc.

    // --- Energetics ---
    double barrier_energy{0.0};          // ΔE_barrier (kcal/mol)
    double delta_E{0.0};                 // E_final - E_initial (kcal/mol)
    double delta_G{0.0};                 // ΔG (kcal/mol), 0 = not computed
    double rmsd{0.0};                    // RMSD between initial and final (Å)

    // --- Timing ---
    double residence_time_initial{0.0};  // Time in initial state before fall (ps)
    double residence_time_final{0.0};    // Time in final state after fall (ps)

    // --- Fall classification ---
    bool     fall_flag{false};
    FallMode fall_mode{FallMode::None};
    double   fall_severity{0.0};         // S_f ∈ [0, 1]
    bool     recoverable{false};         // Can the system return to initial?
    bool     hysteresis{false};          // Different forward/reverse path?
    bool     metastable{false};          // Was the initial state metastable?

    // --- Provenance ---
    SourceTag provenance;
    std::string notes;

    // --- Fall detection (apply criteria) ---
    bool detect_fall(const FallDetectionConfig& cfg) const {
        // Criterion 1: barrier < perturbation energy
        if (barrier_energy > 0.0 && driver.magnitude > 0.0) {
            if (barrier_energy < driver.magnitude * cfg.barrier_ratio_min)
                return true;
        }
        // Criterion 2: RMSD exceeds geometric threshold
        if (rmsd > cfg.rmsd_threshold)
            return true;
        // Criterion 3: state label changed
        if (cfg.check_state_label && initial_state != final_state)
            return true;
        return false;
    }

    // --- Compute severity ---
    void compute_severity(const FallSeverityWeights& w = {}) {
        fall_severity = compute_fall_severity(rmsd, delta_E,
                                              residence_time_final, w);
    }
};

// ============================================================================
// Emergence Dataset — the full collection
// ============================================================================

/**
 * EmergenceDataset — container for ED1 samples with query/filter/export.
 */
struct EmergenceDataset {
    std::string name{"Emergence Dataset #1: Anisotropy and Isomer-Fall Transitions"};
    std::string version{"1.0.0"};

    std::vector<EmergenceSample> samples;

    // --- Add sample ---
    void add(EmergenceSample s) {
        s.sample_id = next_id_++;
        samples.push_back(std::move(s));
    }

    // --- Query by family ---
    std::vector<const EmergenceSample*> by_family(DatasetFamily f) const {
        std::vector<const EmergenceSample*> result;
        for (const auto& s : samples)
            if (s.family == f) result.push_back(&s);
        return result;
    }

    // --- Query: only fall events ---
    std::vector<const EmergenceSample*> falls_only() const {
        std::vector<const EmergenceSample*> result;
        for (const auto& s : samples)
            if (s.fall_flag) result.push_back(&s);
        return result;
    }

    // --- Query by fall mode ---
    std::vector<const EmergenceSample*> by_fall_mode(FallMode m) const {
        std::vector<const EmergenceSample*> result;
        for (const auto& s : samples)
            if (s.fall_mode == m) result.push_back(&s);
        return result;
    }

    // --- Query by system class ---
    std::vector<const EmergenceSample*> by_system_class(SystemClass c) const {
        std::vector<const EmergenceSample*> result;
        for (const auto& s : samples)
            if (s.system_class == c) result.push_back(&s);
        return result;
    }

    // --- Statistics ---
    int total() const { return static_cast<int>(samples.size()); }
    int fall_count() const {
        int n = 0;
        for (const auto& s : samples) if (s.fall_flag) ++n;
        return n;
    }
    int recoverable_count() const {
        int n = 0;
        for (const auto& s : samples) if (s.recoverable) ++n;
        return n;
    }
    double mean_severity() const {
        if (samples.empty()) return 0.0;
        double sum = 0.0;
        int count = 0;
        for (const auto& s : samples)
            if (s.fall_flag) { sum += s.fall_severity; ++count; }
        return (count > 0) ? sum / count : 0.0;
    }

    // --- Run fall detection on all samples ---
    void detect_all_falls(const FallDetectionConfig& cfg = {},
                          const FallSeverityWeights& w = {}) {
        for (auto& s : samples) {
            s.fall_flag = s.detect_fall(cfg);
            if (s.fall_flag)
                s.compute_severity(w);
        }
    }

    // --- Export to CSV (state.csv format) ---
    std::string to_csv() const {
        std::ostringstream out;
        out << "sample_id,family,system_class,composition,charge_state,mass,"
               "initial_state,final_state,transition_class,"
               "anisotropy_type,anisotropy_ratio,"
               "axis_1_x,axis_1_y,axis_1_z,axis_2_x,axis_2_y,axis_2_z,"
               "driver_type,driver_magnitude,temperature_K,pressure_atm,medium,"
               "barrier_energy_kcal,deltaE_kcal,deltaG_kcal,rmsd_A,"
               "residence_time_init_ps,residence_time_final_ps,"
               "fall_flag,fall_mode,fall_severity,"
               "recoverable,hysteresis,metastable,notes\n";

        for (const auto& s : samples) {
            out << s.sample_id << ','
                << family_name(s.family) << ','
                << system_class_name(s.system_class) << ','
                << s.composition << ','
                << s.charge_state << ','
                << s.mass << ','
                << s.initial_state.name << ','
                << s.final_state.name << ','
                << s.transition_class << ','
                << anisotropy_type_name(s.anisotropy.type) << ','
                << s.anisotropy.ratio << ','
                << s.anisotropy.axis_1.x << ','
                << s.anisotropy.axis_1.y << ','
                << s.anisotropy.axis_1.z << ','
                << s.anisotropy.axis_2.x << ','
                << s.anisotropy.axis_2.y << ','
                << s.anisotropy.axis_2.z << ','
                << driver_type_name(s.driver.type) << ','
                << s.driver.magnitude << ','
                << s.temperature << ','
                << s.pressure << ','
                << s.medium << ','
                << s.barrier_energy << ','
                << s.delta_E << ','
                << s.delta_G << ','
                << s.rmsd << ','
                << s.residence_time_initial << ','
                << s.residence_time_final << ','
                << (s.fall_flag ? 1 : 0) << ','
                << fall_mode_name(s.fall_mode) << ','
                << s.fall_severity << ','
                << (s.recoverable ? 1 : 0) << ','
                << (s.hysteresis ? 1 : 0) << ','
                << (s.metastable ? 1 : 0) << ','
                << s.notes << '\n';
        }
        return out.str();
    }

    // --- Summary report ---
    std::string summary() const {
        std::ostringstream out;
        out << "=== " << name << " (v" << version << ") ===\n";
        out << "Total samples:     " << total() << '\n';
        out << "Fall events:       " << fall_count() << '\n';
        out << "Recoverable:       " << recoverable_count() << '\n';
        out << "Mean severity:     " << mean_severity() << '\n';
        out << '\n';

        // Per-family breakdown
        for (int fi = 0; fi <= 2; ++fi) {
            auto f = static_cast<DatasetFamily>(fi);
            auto sub = by_family(f);
            if (sub.empty()) continue;
            int falls = 0;
            for (const auto* p : sub) if (p->fall_flag) ++falls;
            out << "  " << family_name(f) << ": "
                << sub.size() << " samples, " << falls << " falls\n";
        }

        // Per-fall-mode breakdown
        out << "\nFall mode distribution:\n";
        for (int mi = 0; mi <= 12; ++mi) {
            auto m = static_cast<FallMode>(mi);
            auto sub = by_fall_mode(m);
            if (sub.empty()) continue;
            out << "  " << fall_mode_name(m) << ": " << sub.size() << '\n';
        }

        return out.str();
    }

private:
    uint64_t next_id_{1};
};

// ============================================================================
// Benchmark Exemplars — controlled reference samples
// ============================================================================

/**
 * Build the five benchmark exemplars described in the ED1 specification.
 *
 * These are controlled reference cases for validating fall detection,
 * severity scoring, and anisotropy computation.
 */
inline EmergenceDataset build_benchmark_exemplars()
{
    EmergenceDataset ds;
    ds.name = "ED1 Benchmark Exemplars";

    // --- Example 1: Butane (anti → gauche, torsion fall) ---
    {
        EmergenceSample s;
        s.family       = DatasetFamily::ED1A_Molecular;
        s.system_class = SystemClass::SmallMolecule;
        s.composition  = "C4H10";
        s.mass         = 58.12;

        s.initial_state.name     = "anti";
        s.initial_state.geometry = "linear_backbone";
        s.final_state.name       = "gauche";
        s.final_state.geometry   = "linear_backbone";
        s.transition_class       = "conformer";

        s.anisotropy.type        = AnisotropyType::Geometric;
        s.anisotropy.ratio       = 3.2;
        s.anisotropy.eigenvalues[0] = 12.4;
        s.anisotropy.eigenvalues[1] = 5.1;
        s.anisotropy.eigenvalues[2] = 3.9;
        s.anisotropy.axis_1      = {1, 0, 0};

        s.driver.type      = DriverType::Thermal;
        s.driver.magnitude = 0.6;  // kcal/mol (kT at 300K)
        s.driver.unit      = "kcal/mol";

        s.temperature    = 300.0;
        s.medium         = "vacuum";
        s.barrier_energy = 3.4;    // kcal/mol
        s.delta_E        = 0.9;    // gauche is ~0.9 kcal/mol higher
        s.rmsd           = 0.8;
        s.residence_time_initial = 50.0;
        s.residence_time_final   = 30.0;

        s.fall_flag = true;
        s.fall_mode = FallMode::TorsionFall;
        s.recoverable = true;
        s.metastable  = false;
        s.notes = "Textbook torsional isomerization; anti is global minimum";

        ds.add(std::move(s));
    }

    // --- Example 2: Cyclohexane (chair → boat, conformer fall) ---
    {
        EmergenceSample s;
        s.family       = DatasetFamily::ED1A_Molecular;
        s.system_class = SystemClass::SmallMolecule;
        s.composition  = "C6H12";
        s.mass         = 84.16;

        s.initial_state.name     = "chair";
        s.initial_state.geometry = "ring_puckered";
        s.final_state.name       = "boat";
        s.final_state.geometry   = "ring_puckered";
        s.transition_class       = "conformer";

        s.anisotropy.type        = AnisotropyType::Geometric;
        s.anisotropy.ratio       = 1.8;
        s.anisotropy.eigenvalues[0] = 8.2;
        s.anisotropy.eigenvalues[1] = 6.5;
        s.anisotropy.eigenvalues[2] = 4.6;
        s.anisotropy.axis_1      = {0, 0, 1};

        s.driver.type      = DriverType::Thermal;
        s.driver.magnitude = 0.6;
        s.driver.unit      = "kcal/mol";

        s.temperature    = 300.0;
        s.medium         = "vacuum";
        s.barrier_energy = 10.8;
        s.delta_E        = 5.5;
        s.rmsd           = 1.2;
        s.residence_time_initial = 500.0;
        s.residence_time_final   = 5.0;

        s.fall_flag = true;
        s.fall_mode = FallMode::ConformerFall;
        s.recoverable = true;
        s.metastable  = true;
        s.notes = "Chair-to-boat interconversion; boat is metastable";

        ds.add(std::move(s));
    }

    // --- Example 3: Organometallic coordination shell ---
    {
        EmergenceSample s;
        s.family       = DatasetFamily::ED1B_Coordination;
        s.system_class = SystemClass::CoordinationComplex;
        s.composition  = "PtCl2(NH3)2";
        s.mass         = 300.1;
        s.charge_state = 0;

        s.initial_state.name          = "trans";
        s.initial_state.geometry      = "square_planar";
        s.initial_state.coordination  = 4;
        s.final_state.name            = "cis";
        s.final_state.geometry        = "square_planar";
        s.final_state.coordination    = 4;
        s.transition_class            = "isomer";

        s.anisotropy.type        = AnisotropyType::Geometric;
        s.anisotropy.ratio       = 2.1;
        s.anisotropy.eigenvalues[0] = 45.0;
        s.anisotropy.eigenvalues[1] = 30.0;
        s.anisotropy.eigenvalues[2] = 21.4;

        s.driver.type      = DriverType::ElectricField;
        s.driver.magnitude = 0.01;  // V/Å
        s.driver.direction = {1, 0, 0};
        s.driver.unit      = "V/A";

        s.temperature    = 298.0;
        s.medium         = "water";
        s.barrier_energy = 25.0;
        s.delta_E        = -2.0;    // cis slightly lower in solution
        s.rmsd           = 1.8;
        s.residence_time_initial = 1e6;
        s.residence_time_final   = 1e6;

        s.fall_flag = true;
        s.fall_mode = FallMode::CoordinationFall;
        s.recoverable = false;
        s.hysteresis  = true;
        s.notes = "Transplatin to cisplatin isomerization; clinically relevant";

        ds.add(std::move(s));
    }

    // --- Example 4: Anisotropic crystal platelet ---
    {
        EmergenceSample s;
        s.family       = DatasetFamily::ED1C_BeadStructure;
        s.system_class = SystemClass::CrystalFragment;
        s.composition  = "MoS2_fragment";
        s.mass         = 960.0;

        s.initial_state.name     = "aligned";
        s.initial_state.geometry = "layered_hexagonal";
        s.final_state.name       = "reoriented";
        s.final_state.geometry   = "layered_hexagonal";
        s.transition_class       = "orientation";

        s.anisotropy.type        = AnisotropyType::Mechanical;
        s.anisotropy.ratio       = 8.5;
        s.anisotropy.eigenvalues[0] = 240.0;
        s.anisotropy.eigenvalues[1] = 120.0;
        s.anisotropy.eigenvalues[2] = 28.2;
        s.anisotropy.axis_1      = {0, 0, 1};

        s.driver.type      = DriverType::Stress;
        s.driver.magnitude = 5.0;  // GPa
        s.driver.direction = {1, 0, 0};
        s.driver.unit      = "GPa";

        s.temperature    = 300.0;
        s.medium         = "vacuum";
        s.barrier_energy = 15.0;
        s.delta_E        = 0.5;
        s.rmsd           = 3.2;
        s.residence_time_initial = 1e4;
        s.residence_time_final   = 1e3;

        s.fall_flag = true;
        s.fall_mode = FallMode::OrientationFall;
        s.recoverable = true;
        s.hysteresis  = true;
        s.notes = "Facet-dependent reorientation under shear stress";

        ds.add(std::move(s));
    }

    // --- Example 5: Bead-assembled structure ---
    {
        EmergenceSample s;
        s.family       = DatasetFamily::ED1C_BeadStructure;
        s.system_class = SystemClass::BeadAssembly;
        s.composition  = "CG_branch_12bead";
        s.mass         = 5400.0;

        s.initial_state.name     = "branch_aligned";
        s.initial_state.geometry = "branched_extended";
        s.final_state.name       = "collapsed";
        s.final_state.geometry   = "branched_compact";
        s.transition_class       = "structural";

        s.anisotropy.type        = AnisotropyType::Geometric;
        s.anisotropy.ratio       = 4.2;
        s.anisotropy.eigenvalues[0] = 180.0;
        s.anisotropy.eigenvalues[1] = 60.0;
        s.anisotropy.eigenvalues[2] = 42.9;

        s.driver.type      = DriverType::Thermal;
        s.driver.magnitude = 2.5;
        s.driver.unit      = "kcal/mol";

        s.temperature    = 350.0;
        s.medium         = "implicit_solvent";
        s.barrier_energy = 8.0;
        s.delta_E        = -12.0;
        s.rmsd           = 6.5;
        s.residence_time_initial = 200.0;
        s.residence_time_final   = 5000.0;

        s.fall_flag = true;
        s.fall_mode = FallMode::CollapseFall;
        s.recoverable = false;
        s.metastable  = true;
        s.notes = "Branch collapse under thermal perturbation; irreversible compaction";

        ds.add(std::move(s));
    }

    // Run detection and severity on all
    ds.detect_all_falls();

    return ds;
}

} // namespace emergence
} // namespace atomistic
