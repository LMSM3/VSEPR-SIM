#pragma once
/**
 * system_state.hpp  -  Universal Interpretation Layer
 *
 * The central data hub between CLI and kernel/engine.
 *
 * Architecture position:
 *   CLI -> command layer -> [SystemState] -> kernel/environment engine
 *
 * Holds both atomistic and coarse-grained state in a single inspectable
 * container. Commands construct/modify the state; kernel functions
 * operate on it. No physics formulas live here  -  only data routing.
 *
 * Design rules:
 *   - Anti-black-box: every field is inspectable
 *   - Deterministic: same input -> same state
 *   - Modular: atomistic and CG layers are independent
 *   - No rendering, no I/O  -  pure state
 *
 * Reference: Layer B1 (Structure and System Services)
 */

#include "coarse_grain/core/bead.hpp"
#include "coarse_grain/core/environment_state.hpp"
#include "coarse_grain/models/interaction_engine.hpp"
#include "coarse_grain/models/environment_coupling.hpp"
#include "coarse_grain/physics/qcd_transient.hpp"
#include "atomistic/core/state.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

namespace vsepr {
namespace cli {

// ============================================================================
// Transient Particle Class Codes (negative assignments)
// ============================================================================

enum class TransientTypeCode : int32_t {
    ElectronLike = -1,
    IonLike      = -2,
    NeutronLike  = -3,
    GammaLike    = -4,
    AlphaLike    = -5,
    ReservedBase = -6
};

inline bool is_transient_type_code(int32_t code) {
    return code < 0;
}

inline bool is_bead_type_code(int32_t code) {
    return code > 0;
}

inline bool transient_is_neutral(int32_t code) {
    return code == static_cast<int32_t>(TransientTypeCode::NeutronLike)
        || code == static_cast<int32_t>(TransientTypeCode::GammaLike);
}

struct TransientParticle {
    uint32_t id{};
    int32_t type_code{static_cast<int32_t>(TransientTypeCode::ElectronLike)};
    atomistic::Vec3 position{};
    atomistic::Vec3 velocity{};
    double mass{0.0};
    double charge{0.0};
    double energy{0.0};
    bool active{true};
};

// ============================================================================
// Scene Preset Identifiers
// ============================================================================

enum class ScenePreset {
    Isolated,       // Single bead at origin
    Pair,           // Two beads along x-axis
    LinearStack,    // N beads along z-axis
    TShape,         // 3-bead T configuration
    Square,         // 4 beads in xy-plane
    DenseShell,     // Central bead + shell of N neighbours
    RandomCluster,  // N beads in a random box
    COUNT
};

inline const char* scene_preset_name(ScenePreset p) {
    switch (p) {
        case ScenePreset::Isolated:      return "isolated";
        case ScenePreset::Pair:          return "pair";
        case ScenePreset::LinearStack:   return "stack";
        case ScenePreset::TShape:        return "tshape";
        case ScenePreset::Square:        return "square";
        case ScenePreset::DenseShell:    return "shell";
        case ScenePreset::RandomCluster: return "cloud";
        default:                         return "unknown";
    }
}

inline ScenePreset parse_scene_preset(const std::string& name) {
    if (name == "isolated")  return ScenePreset::Isolated;
    if (name == "pair")      return ScenePreset::Pair;
    if (name == "stack")     return ScenePreset::LinearStack;
    if (name == "tshape")    return ScenePreset::TShape;
    if (name == "square")    return ScenePreset::Square;
    if (name == "shell")     return ScenePreset::DenseShell;
    if (name == "cloud")     return ScenePreset::RandomCluster;
    return ScenePreset::Isolated;
}

// ============================================================================
// CGSystemState  -  Coarse-Grained System State
// ============================================================================

/**
 * CGSystemState  -  the interpretation layer for CG workflows.
 *
 * Contains all state needed to drive the CG engine from the CLI.
 * Scene construction, environment update, and interaction evaluation
 * all operate through this object.
 */
struct CGSystemState {
    // --- Bead data ---
    std::vector<coarse_grain::Bead> beads;

    // --- Per-bead environment state ---
    std::vector<coarse_grain::EnvironmentState> env_states;

    // --- Per-bead orientation axes (lightweight, for CLI scene beads) ---
    std::vector<atomistic::Vec3> orientations;
    std::vector<bool> orientation_valid;

    // --- Parameters ---
    coarse_grain::EnvironmentParams env_params;
    coarse_grain::InteractionParams interaction_params;

    // --- Simulation state ---
    int step_count{};
    double dt{1.0};  // fs

    // --- Transient optimizer (Day 63-A runtime sidecar) ---
    bool enable_transient_optimizer{true};
    int transient_substeps{100};
    double transient_dt{0.01};  // fs
    std::vector<TransientParticle> transient_particles;
    std::vector<double> pending_charge_delta;
    std::vector<double> pending_mass_delta;

    // --- QCD extreme-environment sidecar (v5.1.3~2, WO-v513-QCD) ---
    // Runs on T_A|B dual-timescale: K=200 QCD substeps per bead T_B step.
    // Seeded automatically when the scene has ≥2 beads and enable_qcd is true.
    bool                     enable_qcd{false};  // opt-in; expensive for large K
    vsepr::qcd::QuarkGluonPlasma qcd_plasma;

    // --- Scene metadata ---
    std::string scene_name;
    ScenePreset preset{ScenePreset::Isolated};

    // --- Accessors ---
    int num_beads() const { return static_cast<int>(beads.size()); }
    bool is_empty() const { return beads.empty(); }
    bool has_env_state() const { return env_states.size() == beads.size(); }

    // --- Clear all state ---
    void clear() {
        beads.clear();
        env_states.clear();
        orientations.clear();
        orientation_valid.clear();
        transient_particles.clear();
        pending_charge_delta.clear();
        pending_mass_delta.clear();
        qcd_plasma.clear();
        step_count = 0;
        scene_name.clear();
    }

    // --- Build from preset ---
    void build_preset(ScenePreset p, int n_beads, double spacing, uint32_t seed);

    // --- Environment update for all beads ---
    void update_environment(int n_steps);

    // --- Transient optimizer sidecar update ---
    void run_transient_optimizer_substeps();

    // --- QCD plasma T_B step (runs K T_A substeps internally) ---
    void run_qcd_tb_step();

    // --- Build neighbour list for a specific bead ---
    std::vector<coarse_grain::NeighbourInfo> build_neighbours(int bead_index) const;
};

// ============================================================================
// Scene Construction
// ============================================================================

inline void CGSystemState::build_preset(ScenePreset p, int n_beads,
                                         double spacing, uint32_t seed) {
    clear();
    preset = p;

    auto add_bead = [&](const atomistic::Vec3& pos,
                        const atomistic::Vec3& n_hat = {0, 0, 1}) {
        coarse_grain::Bead b;
        b.position = pos;
        b.mass = 1.0;
        beads.push_back(b);
        orientations.push_back(n_hat);
        orientation_valid.push_back(true);
    };

    switch (p) {
        case ScenePreset::Isolated:
            scene_name = "isolated";
            add_bead({0, 0, 0});
            break;

        case ScenePreset::Pair:
            scene_name = "pair (d=" + std::to_string(spacing) + " A)";
            add_bead({0, 0, 0});
            add_bead({spacing, 0, 0});
            break;

        case ScenePreset::LinearStack:
            scene_name = "stack (n=" + std::to_string(n_beads)
                       + ", d=" + std::to_string(spacing) + " A)";
            for (int i = 0; i < n_beads; ++i) {
                add_bead({0, 0, i * spacing});
            }
            break;

        case ScenePreset::TShape:
            scene_name = "T-shape (d=" + std::to_string(spacing) + " A)";
            add_bead({0, 0, 0});
            add_bead({0, 0, spacing});
            add_bead({spacing, 0, 0}, {1, 0, 0});
            break;

        case ScenePreset::Square: {
            double h = spacing / 2.0;
            scene_name = "square (side=" + std::to_string(spacing) + " A)";
            add_bead({-h, -h, 0});
            add_bead({ h, -h, 0});
            add_bead({ h,  h, 0});
            add_bead({-h,  h, 0});
            break;
        }

        case ScenePreset::DenseShell: {
            scene_name = "shell (n=" + std::to_string(n_beads)
                       + ", r=" + std::to_string(spacing) + " A)";
            // Central bead
            add_bead({0, 0, 0});
            // Fibonacci spiral distribution
            constexpr double pi = 3.14159265358979323846;
            constexpr double golden = 1.6180339887498949;
            for (int i = 0; i < n_beads; ++i) {
                double theta = std::acos(1.0 - 2.0 * (i + 0.5) / n_beads);
                double phi = 2.0 * pi * i / golden;
                double x = spacing * std::sin(theta) * std::cos(phi);
                double y = spacing * std::sin(theta) * std::sin(phi);
                double z = spacing * std::cos(theta);
                add_bead({x, y, z});
            }
            break;
        }

        case ScenePreset::RandomCluster: {
            scene_name = "cloud (n=" + std::to_string(n_beads)
                       + ", box=" + std::to_string(spacing) + " A)";
            uint32_t s = seed;
            auto next = [&s]() -> double {
                s ^= s << 13; s ^= s >> 17; s ^= s << 5;
                return (s & 0x7FFFFFFF) / static_cast<double>(0x7FFFFFFF);
            };
            constexpr double pi = 3.14159265358979323846;
            for (int i = 0; i < n_beads; ++i) {
                double px = next() * spacing - spacing / 2.0;
                double py = next() * spacing - spacing / 2.0;
                double pz = next() * spacing - spacing / 2.0;
                double theta = std::acos(2.0 * next() - 1.0);
                double phi = 2.0 * pi * next();
                add_bead({px, py, pz},
                         {std::sin(theta) * std::cos(phi),
                          std::sin(theta) * std::sin(phi),
                          std::cos(theta)});
            }
            break;
        }

        default:
            scene_name = "empty";
            break;
    }

    // Initialise environment states to zero
    env_states.resize(beads.size());
    pending_charge_delta.assign(beads.size(), 0.0);
    pending_mass_delta.assign(beads.size(), 0.0);

    // Minimal default transient sidecar population (disabled for isolated)
    transient_particles.clear();
    if (!beads.empty() && p != ScenePreset::Isolated) {
        auto add_transient = [&](int32_t code,
                                 const atomistic::Vec3& pos,
                                 const atomistic::Vec3& vel,
                                 double m, double q) {
            TransientParticle tp;
            tp.id = static_cast<uint32_t>(transient_particles.size());
            tp.type_code = code;
            tp.position = pos;
            tp.velocity = vel;
            tp.mass = m;
            tp.charge = q;
            tp.active = true;
            transient_particles.push_back(tp);
        };

        const atomistic::Vec3 p0 = beads.front().position;
        add_transient(static_cast<int32_t>(TransientTypeCode::ElectronLike),
                      {p0.x + 0.5, p0.y, p0.z}, {0.0, 0.0, 0.0}, 0.00054858, -1.0);
        add_transient(static_cast<int32_t>(TransientTypeCode::NeutronLike),
                      {p0.x - 0.7, p0.y, p0.z}, {0.02, 0.0, 0.0}, 1.0, 0.0);
    }
}

inline void CGSystemState::run_transient_optimizer_substeps() {
    if (!enable_transient_optimizer || beads.empty() || transient_particles.empty()) {
        return;
    }

    if (pending_charge_delta.size() != beads.size()) pending_charge_delta.assign(beads.size(), 0.0);
    if (pending_mass_delta.size() != beads.size()) pending_mass_delta.assign(beads.size(), 0.0);

    const int K = std::max(1, transient_substeps);
    const double dt_sub = std::max(1e-6, transient_dt);
    const double capture_radius = 0.8; // Å, lightweight default for sidecar coupling

    for (int sub = 0; sub < K; ++sub) {
        for (auto& tp : transient_particles) {
            if (!tp.active) continue;

            tp.position.x += tp.velocity.x * dt_sub;
            tp.position.y += tp.velocity.y * dt_sub;
            tp.position.z += tp.velocity.z * dt_sub;

            // Neutral transients stay ballistic and event-driven (no EM force term here)
            // Charged transients may be captured by nearest bead.
            if (std::abs(tp.charge) > 1e-12) {
                double best_d2 = std::numeric_limits<double>::max();
                int best_i = -1;
                for (int i = 0; i < static_cast<int>(beads.size()); ++i) {
                    atomistic::Vec3 dr = tp.position - beads[i].position;
                    double d2 = dr.x * dr.x + dr.y * dr.y + dr.z * dr.z;
                    if (d2 < best_d2) {
                        best_d2 = d2;
                        best_i = i;
                    }
                }

                if (best_i >= 0 && best_d2 < capture_radius * capture_radius) {
                    pending_charge_delta[static_cast<size_t>(best_i)] += tp.charge;
                    pending_mass_delta[static_cast<size_t>(best_i)] += tp.mass;
                    tp.active = false;
                }
            }
        }
    }

    for (size_t i = 0; i < beads.size(); ++i) {
        beads[i].charge += pending_charge_delta[i];
        beads[i].mass = std::max(0.0, beads[i].mass + pending_mass_delta[i]);
        pending_charge_delta[i] = 0.0;
        pending_mass_delta[i] = 0.0;
    }
}

// ============================================================================
// Neighbour List
// ============================================================================

inline std::vector<coarse_grain::NeighbourInfo>
CGSystemState::build_neighbours(int bead_index) const {
    std::vector<coarse_grain::NeighbourInfo> nbs;
    const auto& center = beads[bead_index];
    for (int i = 0; i < static_cast<int>(beads.size()); ++i) {
        if (i == bead_index) continue;
        atomistic::Vec3 dr = beads[i].position - center.position;
        double dist = atomistic::norm(dr);
        coarse_grain::NeighbourInfo ni;
        ni.distance = dist;
        if (i < static_cast<int>(orientations.size())) {
            ni.n_hat = orientations[i];
            ni.has_orientation = orientation_valid[i];
        }
        nbs.push_back(ni);
    }
    return nbs;
}

// ============================================================================
// Environment Update
// ============================================================================

inline void CGSystemState::update_environment(int n_steps) {
    for (int step = 0; step < n_steps; ++step) {
        run_transient_optimizer_substeps();
        run_qcd_tb_step();

        for (int i = 0; i < num_beads(); ++i) {
            auto nbs = build_neighbours(i);

            atomistic::Vec3 n_hat_i = {0, 0, 1};
            bool has_orient_i = false;
            if (i < static_cast<int>(orientations.size())) {
                n_hat_i = orientations[i];
                has_orient_i = orientation_valid[i];
            }

            double eta_prev = env_states[i].eta;
            env_states[i] = coarse_grain::update_environment_state(
                eta_prev, n_hat_i, has_orient_i, nbs, env_params, dt);
        }
        ++step_count;
    }
}

// ============================================================================
// QCD Plasma T_B Step
// ============================================================================

inline void CGSystemState::run_qcd_tb_step() {
    if (!enable_qcd) return;

    // Auto-seed if plasma is empty and scene has beads
    if (qcd_plasma.particles.empty() && !beads.empty()) {
        vsepr::qcd::QCDPopulationConfig cfg;
        cfg.n_quarks    = std::min(12, 2 * num_beads());
        cfg.n_gluons    = std::min(8,  num_beads());
        cfg.box_radius  = 2.5;
        cfg.v_thermal   = 0.08;
        cfg.seed        = static_cast<uint32_t>(0x513'0001u + step_count);
        vsepr::qcd::qcd_seed_plasma(qcd_plasma, cfg);
    }

    vsepr::qcd::qcd_run_tb_step(qcd_plasma);
}

}} // namespace vsepr::cli
