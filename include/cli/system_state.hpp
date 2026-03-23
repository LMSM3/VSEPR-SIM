#pragma once
/**
 * system_state.hpp — Universal Interpretation Layer
 *
 * The central data hub between CLI and kernel/engine.
 *
 * Architecture position:
 *   CLI → command layer → [SystemState] → kernel/environment engine
 *
 * Holds both atomistic and coarse-grained state in a single inspectable
 * container. Commands construct/modify the state; kernel functions
 * operate on it. No physics formulas live here — only data routing.
 *
 * Design rules:
 *   - Anti-black-box: every field is inspectable
 *   - Deterministic: same input → same state
 *   - Modular: atomistic and CG layers are independent
 *   - No rendering, no I/O — pure state
 *
 * Reference: Layer B1 (Structure and System Services)
 */

#include "coarse_grain/core/bead.hpp"
#include "coarse_grain/core/environment_state.hpp"
#include "coarse_grain/models/interaction_engine.hpp"
#include "coarse_grain/models/environment_coupling.hpp"
#include "atomistic/core/state.hpp"
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace vsepr {
namespace cli {

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
// CGSystemState — Coarse-Grained System State
// ============================================================================

/**
 * CGSystemState — the interpretation layer for CG workflows.
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
        step_count = 0;
        scene_name.clear();
    }

    // --- Build from preset ---
    void build_preset(ScenePreset p, int n_beads, double spacing, uint32_t seed);

    // --- Environment update for all beads ---
    void update_environment(int n_steps);

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

}} // namespace vsepr::cli
