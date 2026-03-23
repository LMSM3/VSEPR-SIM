/**
 * cg_commands.cpp — Coarse-Grained CLI Command Implementations
 *
 * Scientific operator console for the coarse-grained engine.
 *
 * Five operations:
 *   scene   — Build or configure a bead scene from presets
 *   inspect — Display bead positions, environment state, descriptors
 *   env     — Run the environment update pipeline (eta relaxation)
 *   interact — Evaluate pairwise interactions, report energy decomposition
 *   viz     — Lightweight bead visualization viewer
 *
 * All operations route through CGSystemState (the interpretation layer).
 *
 * Reference: Layer C1 (CLI Frontend), Layer B1 (System Services)
 */

#include "cli/cg_commands.hpp"
#include "cli/system_state.hpp"
#include "cli/display.hpp"
#include "coarse_grain/core/channel_kernels.hpp"
#include "coarse_grain/core/unified_descriptor.hpp"
#include "coarse_grain/models/environment_coupling.hpp"
#include "coarse_grain/models/interaction_engine.hpp"
#ifdef BUILD_VISUALIZATION
#include "coarse_grain/vis/cg_viz_viewer.hpp"
#endif
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace vsepr {
namespace cli {

// ============================================================================
// Argument Helpers
// ============================================================================

static bool has_flag(const std::vector<std::string>& args, const std::string& flag) {
    return std::find(args.begin(), args.end(), flag) != args.end();
}

static std::string get_option(const std::vector<std::string>& args,
                               const std::string& key,
                               const std::string& fallback = "") {
    for (size_t i = 0; i + 1 < args.size(); ++i) {
        if (args[i] == key) return args[i + 1];
    }
    return fallback;
}

static int get_int_option(const std::vector<std::string>& args,
                           const std::string& key, int fallback) {
    std::string v = get_option(args, key);
    if (v.empty()) return fallback;
    return std::atoi(v.c_str());
}

static double get_double_option(const std::vector<std::string>& args,
                                 const std::string& key, double fallback) {
    std::string v = get_option(args, key);
    if (v.empty()) return fallback;
    return std::atof(v.c_str());
}

// ============================================================================
// Banner
// ============================================================================

static void cg_banner() {
    Display::Banner("VSEPR-SIM  Coarse-Grained Console",
                    "Atomistic -> Reduced -> Analysis");
    Display::BlankLine();
}

// ============================================================================
// Help
// ============================================================================

static int cg_help() {
    cg_banner();
    Display::Subheader("Available commands");
    Display::BlankLine();

    Display::Table tbl({"Command", "Description"}, {14, 50});
    tbl.PrintHeader();
    tbl.PrintRow({"scene",    "Build or load a bead scene from presets"});
    tbl.PrintRow({"inspect",  "Inspect bead positions, environment, descriptors"});
    tbl.PrintRow({"env",      "Run environment update pipeline (eta relaxation)"});
    tbl.PrintRow({"interact", "Evaluate pairwise interactions, energy decomposition"});
    tbl.PrintRow({"viz",      "Launch lightweight bead visualization"});
    Display::BlankLine();

    Display::Subheader("Usage");
    std::cout << "  vsepr cg <command> [options]\n\n";

    Display::Subheader("Examples");
    std::cout << "  vsepr cg scene --preset pair --spacing 4.0\n"
              << "  vsepr cg scene --preset stack --beads 8 --spacing 3.5\n"
              << "  vsepr cg scene --preset cloud --beads 20 --spacing 15.0 --seed 42\n"
              << "  vsepr cg inspect --all\n"
              << "  vsepr cg env --steps 200 --dt 1.0 --tau 100.0\n"
              << "  vsepr cg interact --all\n"
              << "  vsepr cg viz\n";
    Display::BlankLine();
    return 0;
}

// ============================================================================
// 1. Scene Command
// ============================================================================

static int cmd_scene(const std::vector<std::string>& args) {
    if (has_flag(args, "--help") || has_flag(args, "-h")) {
        Display::Header("cg scene — Build a bead scene");
        Display::BlankLine();
        std::cout << R"(USAGE:
  vsepr cg scene --preset <name> [options]

PRESETS:
  isolated   Single bead at origin
  pair       Two beads along x-axis
  stack      N beads along z-axis (linear chain)
  tshape     3-bead T-shape configuration
  square     4 beads in xy-plane
  shell      Central bead + N neighbours on a sphere
  cloud      N beads in a random box

OPTIONS:
  --preset <name>   Scene preset (required)
  --beads <N>       Number of beads (for stack/shell/cloud, default: 5)
  --spacing <D>     Spacing or radius in Angstroms (default: 4.0)
  --seed <S>        RNG seed for cloud preset (default: 42)

EXAMPLES:
  vsepr cg scene --preset pair --spacing 4.0
  vsepr cg scene --preset stack --beads 8 --spacing 3.5
  vsepr cg scene --preset shell --beads 12 --spacing 5.0
  vsepr cg scene --preset cloud --beads 20 --spacing 15.0 --seed 99
)";
        return 0;
    }

    std::string preset_str = get_option(args, "--preset", "pair");
    int n_beads   = get_int_option(args, "--beads", 5);
    double spacing = get_double_option(args, "--spacing", 4.0);
    uint32_t seed  = static_cast<uint32_t>(get_int_option(args, "--seed", 42));

    ScenePreset preset = parse_scene_preset(preset_str);

    // Build the scene
    CGSystemState state;
    state.build_preset(preset, n_beads, spacing, seed);

    // Display results
    cg_banner();
    Display::Subheader("Scene: " + state.scene_name);
    Display::BlankLine();

    Display::KeyValue("Preset", preset_str);
    Display::KeyValue("Beads", std::to_string(state.num_beads()));
    Display::KeyValue("Spacing", spacing, "A");
    if (preset == ScenePreset::RandomCluster) {
        Display::KeyValue("Seed", std::to_string(seed));
    }
    Display::BlankLine();

    // Bead table
    Display::Subheader("Bead Positions");
    Display::Table tbl({"ID", "X (A)", "Y (A)", "Z (A)", "n_hat"}, {6, 12, 12, 12, 20});
    tbl.PrintHeader();

    for (int i = 0; i < state.num_beads(); ++i) {
        const auto& b = state.beads[i];
        const auto& n = state.orientations[i];
        std::ostringstream nx, ny, nz, ox;
        nx << std::fixed << std::setprecision(3) << b.position.x;
        ny << std::fixed << std::setprecision(3) << b.position.y;
        nz << std::fixed << std::setprecision(3) << b.position.z;
        ox << "(" << std::fixed << std::setprecision(2)
           << n.x << "," << n.y << "," << n.z << ")";
        tbl.PrintRow({std::to_string(i), nx.str(), ny.str(), nz.str(), ox.str()});
    }
    Display::BlankLine();

    // Neighbour summary
    Display::Subheader("Neighbour Summary (r_cutoff = "
                       + std::to_string(state.env_params.r_cutoff) + " A)");
    for (int i = 0; i < state.num_beads(); ++i) {
        auto nbs = state.build_neighbours(i);
        int n_within = 0;
        double min_d = 1e30, max_d = 0;
        for (const auto& nb : nbs) {
            if (nb.distance < state.env_params.r_cutoff) {
                ++n_within;
                if (nb.distance < min_d) min_d = nb.distance;
                if (nb.distance > max_d) max_d = nb.distance;
            }
        }
        std::ostringstream line;
        line << "  Bead " << i << ": " << n_within << " neighbours";
        if (n_within > 0) {
            line << std::fixed << std::setprecision(2)
                 << "  (r_min=" << min_d << ", r_max=" << max_d << " A)";
        }
        std::cout << line.str() << "\n";
    }
    Display::BlankLine();

    Display::Success("Scene built: " + std::to_string(state.num_beads()) + " beads");
    Display::BlankLine();
    return 0;
}

// ============================================================================
// 2. Inspect Command
// ============================================================================

static int cmd_inspect(const std::vector<std::string>& args) {
    if (has_flag(args, "--help") || has_flag(args, "-h")) {
        Display::Header("cg inspect — Inspect bead/system state");
        Display::BlankLine();
        std::cout << R"(USAGE:
  vsepr cg inspect [options]

Builds a scene (via --preset) and runs optional environment steps,
then displays the full inspectable state of every bead.

OPTIONS:
  --preset <name>   Scene preset (default: pair)
  --beads <N>       Number of beads (default: 5)
  --spacing <D>     Spacing in Angstroms (default: 4.0)
  --seed <S>        RNG seed (default: 42)
  --env-steps <N>   Run N environment update steps before inspection (default: 0)
  --dt <T>          Timestep for env updates (default: 1.0 fs)
  --bead <ID>       Inspect only bead with this ID
  --all             Show all fields including descriptors

EXAMPLES:
  vsepr cg inspect --preset stack --beads 5 --spacing 3.5
  vsepr cg inspect --preset pair --spacing 4.0 --env-steps 100
  vsepr cg inspect --preset cloud --beads 10 --spacing 12.0 --bead 0
)";
        return 0;
    }

    // Build scene
    std::string preset_str = get_option(args, "--preset", "pair");
    int n_beads    = get_int_option(args, "--beads", 5);
    double spacing = get_double_option(args, "--spacing", 4.0);
    uint32_t seed  = static_cast<uint32_t>(get_int_option(args, "--seed", 42));
    int env_steps  = get_int_option(args, "--env-steps", 0);
    double dt_val  = get_double_option(args, "--dt", 1.0);
    int single_id  = get_int_option(args, "--bead", -1);
    bool show_all  = has_flag(args, "--all");

    CGSystemState state;
    state.dt = dt_val;
    state.build_preset(parse_scene_preset(preset_str), n_beads, spacing, seed);

    // Run environment steps if requested
    if (env_steps > 0) {
        state.update_environment(env_steps);
    }

    // Display
    cg_banner();
    Display::Subheader("System Inspection: " + state.scene_name);
    Display::BlankLine();
    Display::KeyValue("Total beads", std::to_string(state.num_beads()));
    Display::KeyValue("Env steps run", std::to_string(state.step_count));
    Display::KeyValue("dt", state.dt, "fs");
    Display::KeyValue("tau", state.env_params.tau, "fs");
    Display::KeyValue("r_cutoff", state.env_params.r_cutoff, "A");
    Display::BlankLine();

    // Iterate beads
    int start = 0, end = state.num_beads();
    if (single_id >= 0 && single_id < state.num_beads()) {
        start = single_id;
        end = single_id + 1;
    }

    for (int i = start; i < end; ++i) {
        Display::Separator();
        Display::Subheader("Bead " + std::to_string(i));
        const auto& b = state.beads[i];
        const auto& n = state.orientations[i];
        const auto& e = state.env_states[i];

        // Position
        {
            std::ostringstream oss;
            oss << std::fixed << std::setprecision(4)
                << "(" << b.position.x << ", " << b.position.y
                << ", " << b.position.z << ")";
            Display::KeyValue("Position", oss.str(), "A", 20);
        }
        Display::KeyValue("Mass", b.mass, "amu");

        // Orientation
        {
            std::ostringstream oss;
            oss << std::fixed << std::setprecision(4)
                << "(" << n.x << ", " << n.y << ", " << n.z << ")";
            Display::KeyValue("n_hat", oss.str(), "", 20);
        }

        // Neighbours
        auto nbs = state.build_neighbours(i);
        int n_within = 0;
        for (const auto& nb : nbs) {
            if (nb.distance < state.env_params.r_cutoff) ++n_within;
        }
        Display::KeyValue("Neighbours", std::to_string(n_within)
                          + " within cutoff");

        // Environment state
        if (state.step_count > 0 || show_all) {
            Display::BlankLine();
            Display::Subheader("  Environment State (X_B)");
            Display::KeyValue("  rho", e.rho);
            Display::KeyValue("  rho_hat", e.rho_hat);
            Display::KeyValue("  C", e.C);
            Display::KeyValue("  P2", e.P2);
            Display::KeyValue("  P2_hat", e.P2_hat);
            Display::KeyValue("  eta", e.eta);
            Display::KeyValue("  target_f", e.target_f);
        }

        // Unified descriptor (if present)
        if (show_all && b.has_unified_data()) {
            Display::BlankLine();
            Display::Subheader("  Unified Descriptor");
            const auto& ud = *b.unified;
            Display::KeyValue("  l_max", std::to_string(ud.max_l_max()));
            Display::KeyValue("  Resolution",
                              coarse_grain::resolution_level_name(ud.resolution_level()));
            Display::KeyValue("  Steric active",
                              ud.steric.active ? "yes" : "no");
            Display::KeyValue("  Electrostatic active",
                              ud.electrostatic.active ? "yes" : "no");
            Display::KeyValue("  Dispersion active",
                              ud.dispersion.active ? "yes" : "no");
        }
        Display::BlankLine();
    }

    Display::Success("Inspection complete");
    Display::BlankLine();
    return 0;
}

// ============================================================================
// 3. Env Command
// ============================================================================

static int cmd_env(const std::vector<std::string>& args) {
    if (has_flag(args, "--help") || has_flag(args, "-h")) {
        Display::Header("cg env — Run environment update pipeline");
        Display::BlankLine();
        std::cout << R"(USAGE:
  vsepr cg env [options]

Builds a scene, then runs the environment update pipeline (eta relaxation)
for a specified number of steps.  Reports per-bead trajectories.

OPTIONS:
  --preset <name>   Scene preset (default: stack)
  --beads <N>       Number of beads (default: 5)
  --spacing <D>     Spacing in Angstroms (default: 4.0)
  --seed <S>        RNG seed (default: 42)
  --steps <N>       Number of env update steps (default: 100)
  --dt <T>          Timestep (default: 1.0 fs)
  --tau <T>         Relaxation timescale (default: 100.0 fs)
  --report <N>      Report interval in steps (default: 10)

EXAMPLES:
  vsepr cg env --preset stack --beads 5 --steps 200
  vsepr cg env --preset shell --beads 12 --spacing 5.0 --steps 500 --tau 50
  vsepr cg env --preset cloud --beads 20 --spacing 12.0 --steps 1000
)";
        return 0;
    }

    // Build scene
    std::string preset_str = get_option(args, "--preset", "stack");
    int n_beads    = get_int_option(args, "--beads", 5);
    double spacing = get_double_option(args, "--spacing", 4.0);
    uint32_t seed  = static_cast<uint32_t>(get_int_option(args, "--seed", 42));
    int steps      = get_int_option(args, "--steps", 100);
    double dt_val  = get_double_option(args, "--dt", 1.0);
    double tau_val = get_double_option(args, "--tau", 100.0);
    int report_n   = get_int_option(args, "--report", 10);

    CGSystemState state;
    state.dt = dt_val;
    state.env_params.tau = tau_val;
    state.build_preset(parse_scene_preset(preset_str), n_beads, spacing, seed);

    cg_banner();
    Display::Subheader("Environment Update Pipeline");
    Display::BlankLine();
    Display::KeyValue("Scene", state.scene_name);
    Display::KeyValue("Beads", std::to_string(state.num_beads()));
    Display::KeyValue("Steps", std::to_string(steps));
    Display::KeyValue("dt", state.dt, "fs");
    Display::KeyValue("tau", state.env_params.tau, "fs");
    Display::KeyValue("alpha", state.env_params.alpha);
    Display::KeyValue("beta", state.env_params.beta);
    Display::BlankLine();

    // Run with reporting
    Display::Subheader("Trajectory (reporting every " + std::to_string(report_n) + " steps)");
    Display::BlankLine();

    // Column header
    {
        std::ostringstream hdr;
        hdr << std::setw(8) << "Step";
        for (int i = 0; i < state.num_beads(); ++i) {
            hdr << std::setw(12) << ("eta[" + std::to_string(i) + "]");
        }
        hdr << std::setw(12) << "mean_eta";
        std::cout << Color::CYAN << hdr.str() << Color::RESET << "\n";
    }

    // Initial state
    auto print_row = [&](int step) {
        std::ostringstream row;
        row << std::setw(8) << step;
        double sum_eta = 0;
        for (int i = 0; i < state.num_beads(); ++i) {
            row << std::setw(12) << std::fixed << std::setprecision(6)
                << state.env_states[i].eta;
            sum_eta += state.env_states[i].eta;
        }
        double mean = state.num_beads() > 0 ? sum_eta / state.num_beads() : 0;
        row << std::setw(12) << std::fixed << std::setprecision(6) << mean;
        std::cout << row.str() << "\n";
    };

    print_row(0);

    for (int s = 1; s <= steps; ++s) {
        state.update_environment(1);
        if (s % report_n == 0 || s == steps) {
            print_row(s);
        }
    }

    Display::BlankLine();

    // Final state summary
    Display::Subheader("Final Environment State");
    Display::Table tbl({"Bead", "eta", "rho", "C", "P2", "target_f"}, {6, 10, 10, 10, 10, 10});
    tbl.PrintHeader();
    for (int i = 0; i < state.num_beads(); ++i) {
        const auto& e = state.env_states[i];
        auto fmt = [](double v) {
            std::ostringstream o;
            o << std::fixed << std::setprecision(4) << v;
            return o.str();
        };
        tbl.PrintRow({std::to_string(i), fmt(e.eta), fmt(e.rho),
                      fmt(e.C), fmt(e.P2), fmt(e.target_f)});
    }
    Display::BlankLine();

    // Modulation factors at final state
    Display::Subheader("Kernel Modulation Factors (from final eta)");
    if (state.num_beads() >= 2) {
        double eta_0 = state.env_states[0].eta;
        double eta_1 = state.env_states[1].eta;
        double g_steric = coarse_grain::kernel_modulation_factor(
            coarse_grain::Channel::Steric, eta_0, eta_1, state.env_params);
        double g_elec = coarse_grain::kernel_modulation_factor(
            coarse_grain::Channel::Electrostatic, eta_0, eta_1, state.env_params);
        double g_disp = coarse_grain::kernel_modulation_factor(
            coarse_grain::Channel::Dispersion, eta_0, eta_1, state.env_params);
        Display::KeyValue("g_steric (0,1)", g_steric);
        Display::KeyValue("g_elec (0,1)", g_elec);
        Display::KeyValue("g_disp (0,1)", g_disp);
    } else {
        Display::Info("Need >= 2 beads for modulation factor report");
    }
    Display::BlankLine();

    Display::Success("Environment pipeline complete: "
                     + std::to_string(state.step_count) + " total steps");
    Display::BlankLine();
    return 0;
}

// ============================================================================
// 4. Interact Command
// ============================================================================

static int cmd_interact(const std::vector<std::string>& args) {
    if (has_flag(args, "--help") || has_flag(args, "-h")) {
        Display::Header("cg interact — Evaluate pairwise interactions");
        Display::BlankLine();
        std::cout << R"(USAGE:
  vsepr cg interact [options]

Builds a scene, optionally runs environment updates, then evaluates
pairwise interaction energies with full per-channel decomposition.

OPTIONS:
  --preset <name>   Scene preset (default: pair)
  --beads <N>       Number of beads (default: 2)
  --spacing <D>     Spacing in Angstroms (default: 4.0)
  --seed <S>        RNG seed (default: 42)
  --env-steps <N>   Run N env steps before evaluation (default: 0)
  --dt <T>          Timestep for env updates (default: 1.0 fs)
  --pair <A,B>      Evaluate specific pair (default: 0,1)
  --all             Evaluate all pairs
  --l-max <L>       Descriptor angular resolution (default: 2)

EXAMPLES:
  vsepr cg interact --preset pair --spacing 4.0
  vsepr cg interact --preset stack --beads 4 --spacing 3.5 --all
  vsepr cg interact --preset pair --spacing 4.0 --env-steps 100
)";
        return 0;
    }

    // Build scene
    std::string preset_str = get_option(args, "--preset", "pair");
    int n_beads    = get_int_option(args, "--beads", 2);
    double spacing = get_double_option(args, "--spacing", 4.0);
    uint32_t seed  = static_cast<uint32_t>(get_int_option(args, "--seed", 42));
    int env_steps  = get_int_option(args, "--env-steps", 0);
    double dt_val  = get_double_option(args, "--dt", 1.0);
    bool all_pairs = has_flag(args, "--all");
    int l_max      = get_int_option(args, "--l-max", 2);

    // Parse --pair A,B
    int pair_a = 0, pair_b = 1;
    {
        std::string pair_str = get_option(args, "--pair", "0,1");
        auto comma = pair_str.find(',');
        if (comma != std::string::npos) {
            pair_a = std::atoi(pair_str.substr(0, comma).c_str());
            pair_b = std::atoi(pair_str.substr(comma + 1).c_str());
        }
    }

    CGSystemState state;
    state.dt = dt_val;
    state.build_preset(parse_scene_preset(preset_str), n_beads, spacing, seed);

    // Assign simple unified descriptors to all beads for interaction evaluation
    for (int i = 0; i < state.num_beads(); ++i) {
        coarse_grain::UnifiedDescriptor desc;
        // Initialize all channels at given l_max
        desc.init(l_max);
        // Set isotropic coefficient c_00 = 1 for steric and dispersion
        if (!desc.steric.coeffs.empty()) desc.steric.coeffs[0] = 1.0;
        if (!desc.dispersion.coeffs.empty()) desc.dispersion.coeffs[0] = 1.0;

        state.beads[i].unified = desc;
    }

    // Run environment steps if requested
    if (env_steps > 0) {
        state.update_environment(env_steps);
    }

    cg_banner();
    Display::Subheader("Interaction Evaluation");
    Display::BlankLine();
    Display::KeyValue("Scene", state.scene_name);
    Display::KeyValue("Beads", std::to_string(state.num_beads()));
    Display::KeyValue("l_max", std::to_string(l_max));
    Display::KeyValue("Env steps", std::to_string(env_steps));
    Display::BlankLine();

    // Evaluation function
    auto evaluate_pair = [&](int a, int b) {
        if (a < 0 || a >= state.num_beads() || b < 0 || b >= state.num_beads() || a == b) {
            Display::Error("Invalid pair (" + std::to_string(a) + "," + std::to_string(b) + ")");
            return;
        }

        const auto& ba = state.beads[a];
        const auto& bb = state.beads[b];

        if (!ba.has_unified_data() || !bb.has_unified_data()) {
            Display::Warning("Bead " + std::to_string(a) + " or " + std::to_string(b)
                            + " missing unified descriptor");
            return;
        }

        atomistic::Vec3 r_vec = bb.position - ba.position;
        auto result = coarse_grain::interaction_energy(
            *ba.unified, *bb.unified, r_vec, state.interaction_params);

        Display::Separator();
        Display::Subheader("Pair (" + std::to_string(a) + ", " + std::to_string(b) + ")");
        Display::KeyValue("Separation", result.separation, "A");
        Display::KeyValue("E_total", result.E_total, "kcal/mol");
        Display::KeyValue("E_steric", result.steric.energy, "kcal/mol");
        Display::KeyValue("E_electrostatic", result.electrostatic.energy, "kcal/mol");
        Display::KeyValue("E_dispersion", result.dispersion.energy, "kcal/mol");
        Display::KeyValue("Frames valid", result.frames_valid ? "yes" : "no");

        // Per-l decomposition for steric
        if (result.steric.l_max_used > 0 &&
            !result.steric.per_l_energy.empty()) {
            Display::BlankLine();
            Display::Subheader("  Steric per-l decomposition");
            for (int l = 0; l <= result.steric.l_max_used; ++l) {
                Display::KeyValue("    l=" + std::to_string(l),
                                  result.steric.per_l_energy[l], "kcal/mol");
            }
        }

        // Environment modulation (if env was run)
        if (env_steps > 0) {
            Display::BlankLine();
            Display::Subheader("  Environment modulation");
            double eta_a = state.env_states[a].eta;
            double eta_b = state.env_states[b].eta;
            Display::KeyValue("    eta_A", eta_a);
            Display::KeyValue("    eta_B", eta_b);
            Display::KeyValue("    g_steric",
                coarse_grain::kernel_modulation_factor(
                    coarse_grain::Channel::Steric, eta_a, eta_b, state.env_params));
            Display::KeyValue("    g_elec",
                coarse_grain::kernel_modulation_factor(
                    coarse_grain::Channel::Electrostatic, eta_a, eta_b, state.env_params));
            Display::KeyValue("    g_disp",
                coarse_grain::kernel_modulation_factor(
                    coarse_grain::Channel::Dispersion, eta_a, eta_b, state.env_params));
        }
        Display::BlankLine();
    };

    if (all_pairs) {
        for (int a = 0; a < state.num_beads(); ++a) {
            for (int b = a + 1; b < state.num_beads(); ++b) {
                evaluate_pair(a, b);
            }
        }
    } else {
        evaluate_pair(pair_a, pair_b);
    }

    Display::Success("Interaction evaluation complete");
    Display::BlankLine();
    return 0;
}

// ============================================================================
// 5. Viz Command — Lightweight Bead Viewer
// ============================================================================

static int cmd_viz(const std::vector<std::string>& args) {
    if (has_flag(args, "--help") || has_flag(args, "-h")) {
        Display::Header("cg viz — Lightweight bead visualization");
        Display::BlankLine();
        std::cout << R"(USAGE:
  vsepr cg viz [options]

Builds a bead scene and opens a lightweight GLFW/ImGui viewer for
immediate visual inspection of bead positions, orientation axes,
environment state overlays, and neighbour shells.

OPTIONS:
  --preset <name>   Scene preset (default: pair)
  --beads <N>       Number of beads (default: 5)
  --spacing <D>     Spacing in Angstroms (default: 4.0)
  --seed <S>        RNG seed (default: 42)
  --env-steps <N>   Run N env steps before viewing (default: 0)
  --dt <T>          Timestep for env updates (default: 1.0 fs)
  --tau <T>         Relaxation timescale (default: 100.0 fs)
  --overlay <MODE>  Initial overlay: none|rho|C|P2|eta (default: none)
  --no-axes         Hide orientation axes on startup

CONTROLS (in viewer):
  Right-drag     Orbit camera
  Middle-drag    Pan camera
  +/-            Zoom in/out
  Left-click     Select bead
  O              Cycle overlay (none -> rho -> C -> P2 -> eta)
  A              Toggle orientation axes
  N              Toggle neighbour shell edges
  ESC            Close viewer

EXAMPLES:
  vsepr cg viz --preset pair --spacing 4.0
  vsepr cg viz --preset shell --beads 12 --spacing 5.0 --env-steps 200
  vsepr cg viz --preset cloud --beads 20 --spacing 12.0 --overlay eta
  vsepr cg viz --preset stack --beads 8 --no-axes
)";
        return 0;
    }

#ifdef BUILD_VISUALIZATION
    // Build scene
    std::string preset_str = get_option(args, "--preset", "pair");
    int n_beads    = get_int_option(args, "--beads", 5);
    double spacing = get_double_option(args, "--spacing", 4.0);
    uint32_t seed  = static_cast<uint32_t>(get_int_option(args, "--seed", 42));
    int env_steps  = get_int_option(args, "--env-steps", 0);
    double dt_val  = get_double_option(args, "--dt", 1.0);
    double tau_val = get_double_option(args, "--tau", 100.0);

    CGSystemState state;
    state.dt = dt_val;
    state.env_params.tau = tau_val;
    state.build_preset(parse_scene_preset(preset_str), n_beads, spacing, seed);

    // Run environment steps if requested
    if (env_steps > 0) {
        cg_banner();
        Display::Info("Running " + std::to_string(env_steps) + " environment steps...");
        state.update_environment(env_steps);
        Display::Success("Environment pipeline complete (" + std::to_string(env_steps) + " steps)");
        Display::BlankLine();
    }

    // Configure viewer
    coarse_grain::vis::VizConfig config;
    config.overlay = coarse_grain::vis::parse_overlay_mode(get_option(args, "--overlay", "none"));
    config.show_axes = !has_flag(args, "--no-axes");
    config.show_neighbours = false;

    cg_banner();
    Display::Info("Launching lightweight CG viewer...");
    Display::KeyValue("Scene", state.scene_name);
    Display::KeyValue("Beads", std::to_string(state.num_beads()));
    Display::KeyValue("Overlay", coarse_grain::vis::overlay_name(config.overlay));
    Display::BlankLine();

    return coarse_grain::vis::CGVizViewer::run(state, config);
#else
    cg_banner();
    Display::Subheader("Visualization");
    Display::BlankLine();
    Display::Warning("Visualization is not available in this build.");
    Display::Info("Rebuild with -DBUILD_VIS=ON to enable the CG viewer.");
    Display::BlankLine();
    Display::Info("Use 'vsepr cg inspect' for text-based state inspection.");
    Display::BlankLine();
    return 0;
#endif
}

// ============================================================================
// Main CG Dispatcher
// ============================================================================

int cg_dispatch(int argc, char** argv) {
    // argv: [0]=vsepr [1]=cg [2]=action [3..]=args
    if (argc < 3) {
        return cg_help();
    }

    std::string action = argv[2];

    // Collect remaining arguments
    std::vector<std::string> args;
    for (int i = 3; i < argc; ++i) {
        args.push_back(argv[i]);
    }

    // Check for help on the cg sub-command itself
    if (action == "--help" || action == "-h" || action == "help") {
        return cg_help();
    }

    // Dispatch
    if (action == "scene")    return cmd_scene(args);
    if (action == "inspect")  return cmd_inspect(args);
    if (action == "env")      return cmd_env(args);
    if (action == "interact") return cmd_interact(args);
    if (action == "viz")      return cmd_viz(args);

    Display::Error("Unknown CG command: " + action);
    Display::Info("Run 'vsepr cg help' for available commands");
    return 1;
}

}} // namespace vsepr::cli
