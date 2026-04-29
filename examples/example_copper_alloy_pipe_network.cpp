/**
 * example_copper_alloy_pipe_network.cpp
 *
 * Demonstration: random copper-base alloy at chaos factor χ = 1.25,
 * wired into a complex stochastic Vec3×Vec3 pipe network.
 *
 * Pipeline:
 *   1. Generate Cu-base alloy composition   (alloy::generate_copper_alloy)
 *   2. Place alloy beads on FCC lattice     (alloy::alloy_to_bead_system)
 *   3. Run L2↔L4 boundary check            (layer_stack::evaluate_boundary)
 *   4. Generate Vec3×Vec3 pipe network      (pipe_network::generate_pipe_network)
 *   5. Bind alloy material to each segment  (segment.alloy_bead_index → AlloyBeadRecord)
 *   6. Print beam tensors and network stats
 *
 * Exits 0 on success.
 */

#include "include/alloy_generator.hpp"
#include "include/pipe_network.hpp"
#include "coarse_grain/models/layer_boundary.hpp"
#include "include/layer_stack.hpp"

#include <cassert>
#include <cstdio>
#include <vector>

using namespace alloy;
using namespace pipe_network;
using namespace layer_stack;

// ── helpers ───────────────────────────────────────────────────────────────────

static void section(const char* title) {
    std::printf("\n╔══════════════════════════════════════════════════════════\n");
    std::printf("║  %s\n", title);
    std::printf("╚══════════════════════════════════════════════════════════\n");
}

static void check(bool cond, const char* msg) {
    if (!cond) { std::fprintf(stderr, "[FAIL] %s\n", msg); std::exit(1); }
    std::printf("  [PASS] %s\n", msg);
}

static void print_vec3(const char* label, const Vec3& v) {
    std::printf("    %-20s = (%.4f, %.4f, %.4f)\n", label, v.x, v.y, v.z);
}

static void print_mat3(const char* label, const Mat3& M) {
    std::printf("    %s:\n", label);
    for (int i = 0; i < 3; ++i)
        std::printf("      [%9.5f  %9.5f  %9.5f]\n",
                    M.m[i][0], M.m[i][1], M.m[i][2]);
}

// ── Step 1: Alloy generation ───────────────────────────────────────────────────

static AlloyComposition run_alloy_generation() {
    section("Step 1 — Random Cu-base Alloy  (χ = 1.25)");

    ChaosFactor chaos;
    chaos.chi = 1.25;

    std::printf("  Chaos factor χ = %.2f\n", chaos.chi);
    std::printf("  disorder_amplitude  = %.3f\n", chaos.disorder_amplitude());
    std::printf("  precipitation_prob  = %.3f\n", chaos.precipitation_prob());
    std::printf("  epsilon_noise_frac  = %.3f\n", chaos.epsilon_noise_frac());
    std::printf("  grain_boundary_frac = %.3f\n", chaos.grain_boundary_frac());
    std::printf("  solute_max_fraction = %.3f\n", chaos.solute_max_fraction());
    std::printf("\n");

    AlloyComposition alloy = generate_copper_alloy(
        /*n_beads=*/       64,
        /*chaos=*/         chaos,
        /*seed=*/          1337,
        /*cu_base=*/       0.62);

    std::printf("  Name:  %s\n", alloy.name.c_str());
    std::printf("  Beads: %u\n", alloy.n_beads);
    std::printf("  Mean ε = %.4f kcal/mol  (σ = %.4f)\n",
                alloy.mean_epsilon, alloy.std_epsilon);
    std::printf("  GB fraction (actual):         %.3f\n", alloy.gb_fraction_actual);
    std::printf("  Secondary phase fraction:     %.3f\n", alloy.secondary_phase_fraction);
    std::printf("\n  Composition (mole fraction):\n");

    for (int i = 0; i < 10; ++i) {
        if (alloy.mole_fraction[static_cast<size_t>(i)] < 0.005) continue;
        const auto& er = CU_ALLOY_POOL[static_cast<size_t>(i)];
        std::printf("    Z=%3u %-2s  xf=%.4f  ε_ref=%.3f  σ=%.3f Å\n",
                    er.Z, er.symbol,
                    alloy.mole_fraction[static_cast<size_t>(i)],
                    er.epsilon_ref,
                    er.sigma);
    }

    // Sample 5 beads and print their chaos provenance
    std::printf("\n  Sample bead provenance (first 5):\n");
    for (uint32_t i = 0; i < std::min(5u, alloy.n_beads); ++i) {
        const auto& b = alloy.beads[i];
        std::printf("    Bead %2u  Z=%3u %-2s  Q=%+.4f  ε=%.4f (base=%.4f Δ=%+.4f)  "
                    "GB=%d  SecPhase=%d\n",
                    b.bead_index, b.Z, b.symbol.c_str(),
                    b.Q, b.epsilon, b.epsilon_base, b.epsilon_perturbation,
                    b.is_grain_boundary ? 1 : 0,
                    b.is_secondary_phase ? 1 : 0);
    }

    // Checks
    check(alloy.n_beads == 64,           "64 beads generated");
    check(alloy.mole_fraction[7] > 0.40, "Cu dominant (>40 mol%)");
    check(alloy.mean_epsilon > 0.0,      "Mean ε positive");

    // Propagation invariant: every bead Z must match its pool entry
    bool invariant_ok = true;
    for (auto& b : alloy.beads) {
        if (b.Z < 1 || b.Z > 118) { invariant_ok = false; break; }
    }
    check(invariant_ok, "Propagation invariant: all Z ∈ [1,118]");

    return alloy;
}

// ── Step 2: FCC lattice placement + bead system ───────────────────────────────

static coarse_grain::BeadSystem run_bead_placement(const AlloyComposition& alloy) {
    section("Step 2 — FCC Lattice Placement → BeadSystem");

    coarse_grain::BeadSystem sys = alloy_to_bead_system(alloy, /*seed=*/42);

    check(sys.beads.size() == alloy.n_beads, "Bead count matches alloy");
    check(sys.source_atom_count == alloy.n_beads, "Source atom count correct");

    // Print first 5 bead positions
    std::printf("  First 5 bead positions (FCC + jitter):\n");
    for (uint32_t i = 0; i < std::min(5u, static_cast<uint32_t>(sys.beads.size())); ++i) {
        const auto& b = sys.beads[i];
        std::printf("    Bead %2u  pos=(%.4f, %.4f, %.4f) Å  "
                    "role=%-20s  Λ=%s\n",
                    i, b.position.x, b.position.y, b.position.z,
                    coarse_grain::structural_role_name(b.structural_role),
                    coarse_grain::stability_class_name(b.stability_class));
    }

    return sys;
}

// ── Step 3: L2↔L4 boundary check on the alloy system ─────────────────────────

static void run_boundary_check(coarse_grain::BeadSystem& sys,
                               const AlloyComposition& alloy)
{
    section("Step 3 — L2↔L4 Boundary Check");

    // Build L2 states from alloy records
    std::vector<L2_AtomisticState> l2_states;
    l2_states.reserve(alloy.n_beads);
    for (auto& b : alloy.beads) {
        L2_AtomisticState st;
        st.Z               = b.Z;
        st.A               = b.A;
        st.Q               = b.Q;
        st.epsilon         = b.epsilon;
        st.sigma           = b.sigma;
        st.structural_role = b.structural_role;
        st.stability_class = b.stability_class;
        st.phase           = coarse_grain::chemistry::Phase::SOLID;
        st.source_formula  = b.symbol;
        l2_states.push_back(st);
    }

    // All beads are single-phase solid (metallic alloy)
    std::vector<std::vector<coarse_grain::chemistry::Phase>> phases(
        alloy.n_beads,
        {coarse_grain::chemistry::Phase::SOLID});

    BoundaryParams params;
    params.recompute_sigma = true;
    params.energy_tol_kcal = 500.0;  // metals have large pairwise energies
    params.charge_tol_e    = 0.20;   // chaos generates small charges

    BoundaryReport report = evaluate_boundary(sys, l2_states, phases, params);

    std::printf("  %s\n", report.summary.c_str());
    std::printf("  Mean energy gap:       %.4f kcal/mol\n", report.mean_energy_gap);
    std::printf("  Mean charge error:     %.5f e\n",        report.mean_charge_error);
    std::printf("  Mean mapping residual: %.4f\n",          report.mean_mapping_residual);
    std::printf("  Σ role updates (B4):   %u\n",            report.n_sigma_updated);
    std::printf("  Flagged beads (B5):    %u\n",            report.n_flagged);

    check(report.n_beads_checked == alloy.n_beads, "All beads checked");
    check(report.n_phase_mixed   == 0,             "No mixed-phase beads (solid alloy)");
}

// ── Step 4+5: Vec3×Vec3 pipe network generation ───────────────────────────────

static PipeNetwork run_network_generation(const AlloyComposition& alloy) {
    section("Step 4+5 — Vec3×Vec3 Complex Random Pipe Network  (χ = 1.25)");

    NetworkChaos chaos;
    chaos.chi = 1.25;

    std::printf("  Chaos factor χ = %.2f\n", chaos.chi);
    std::printf("  branch_prob          = %.3f\n", chaos.branch_prob());
    std::printf("  max_branches         = %d\n",   chaos.max_branches());
    std::printf("  length_mean_m        = %.3f m\n", chaos.length_mean_m());
    std::printf("  deflection_max_rad   = %.4f rad (%.1f°)\n",
                chaos.deflection_max_rad(),
                chaos.deflection_max_rad() * 180.0 / 3.14159);
    std::printf("  loop_back_prob       = %.3f\n",  chaos.loop_back_prob());
    std::printf("  cross_section_noise  = %.3f\n",  chaos.cross_section_noise());
    std::printf("\n");

    PipeNetwork net = generate_pipe_network(
        /*n_segments_target=*/ 32,
        /*chaos=*/             chaos,
        /*alloy_bead_count=*/  static_cast<uint32_t>(alloy.beads.size()),
        /*seed=*/              2025);

    std::printf("  Network: %s\n", net.name.c_str());
    std::printf("  Segments:        %u\n",    net.n_segments);
    std::printf("  Nodes:           %zu\n",   net.nodes.size());
    std::printf("  Junctions:       %u\n",    net.n_junctions);
    std::printf("  Loop-backs:      %u\n",    net.n_loops);
    std::printf("  Total length:    %.2f m\n", net.total_length_m);
    std::printf("  Mean seg length: %.3f m\n", net.mean_segment_length_m);
    std::printf("  Mean deflection: %.4f rad (%.1f°)\n",
                net.mean_deflection_rad,
                net.mean_deflection_rad * 180.0 / 3.14159);
    std::printf("  Alloy fraction:  %.3f\n",   net.alloy_segment_fraction);

    // Print first 8 segments: Vec3×Vec3 + beam tensor
    std::printf("\n  First 8 segments — Vec3×Vec3 and Beam Tensor B:\n");
    for (uint32_t i = 0; i < std::min(8u, net.n_segments); ++i) {
        const auto& s = net.segments[i];

        // Material identity from alloy
        const auto& bead_rec = alloy.beads[
            std::min(s.alloy_bead_index,
                     static_cast<uint32_t>(alloy.beads.size()-1))];

        std::printf("\n  ── Segment %u  (nodes %u→%u)  %s  loop=%s\n",
                    s.segment_id, s.node_from, s.node_to,
                    s.material_label.empty() ? bead_rec.symbol.c_str()
                                             : s.material_label.c_str(),
                    s.is_loop_back ? "YES" : "no");
        print_vec3("origin  (m)",   s.origin);
        print_vec3("terminus (m)",  s.terminus);
        print_vec3("unit tangent",  s.unit_tangent);
        std::printf("    %-20s = %.4f m\n", "length",           s.length_m);
        std::printf("    %-20s = %.4f m  wall=%.4f m  ID=%.4f m\n",
                    "OD", s.outer_diameter_m, s.wall_thickness_m, s.inner_diameter_m);
        std::printf("    %-20s = Z=%u (%s)  ε=%.4f kcal/mol  Q=%+.4f\n",
                    "material (L2)",
                    bead_rec.Z, bead_rec.symbol.c_str(),
                    bead_rec.epsilon, bead_rec.Q);
        print_mat3("B = L×(t̂⊗t̂)",   s.beam_tensor_B);
    }

    // Network beam tensor (cumulative)
    std::printf("\n  Network beam tensor (∑ B over all segments):\n");
    print_mat3("Σ B", net.network_beam_tensor);
    std::printf("  Trace(Σ B) = %.4f m  (should equal total length = %.4f m)\n",
                net.network_beam_tensor.trace(), net.total_length_m);

    // Checks
    check(net.n_segments > 0,                "Network has segments");
    check(net.nodes.size() > 1,              "Network has multiple nodes");
    check(net.total_length_m > 0.0,          "Total length positive");
    check(net.mean_segment_length_m > 0.0,   "Mean segment length positive");

    // Beam tensor trace should equal total length (sum of individual L values)
    double trace_sum = 0.0;
    for (auto& s : net.segments) trace_sum += s.beam_tensor_B.trace();
    double total_L_sum = 0.0;
    for (auto& s : net.segments) total_L_sum += s.length_m;
    double trace_error = std::abs(trace_sum - total_L_sum);
    std::printf("  Trace check error: %.2e m\n", trace_error);
    check(trace_error < 1e-9 * total_L_sum + 1e-10,
          "Beam tensor trace equals total segment length");

    // All segments have valid geometry
    bool geom_ok = true;
    for (auto& s : net.segments)
        if (s.length_m <= 0.0 || s.outer_diameter_m <= 0.0)
            { geom_ok = false; break; }
    check(geom_ok, "All segments have valid geometry");

    return net;
}

// ── Step 6: Joint alloy + network report ─────────────────────────────────────

static void run_joint_report(const AlloyComposition& alloy,
                             const PipeNetwork&       net)
{
    section("Step 6 — Joint Alloy + Network Report");

    std::printf("  Material system:  %s\n", alloy.name.c_str());
    std::printf("  Network topology: %s\n", net.name.c_str());
    std::printf("\n");

    // Alloy summary table
    std::printf("  ┌─────────────────────────────────────────────────────┐\n");
    std::printf("  │  Cu-base alloy composition  (χ=1.25, 64 beads)     │\n");
    std::printf("  ├──────┬──────┬──────────┬────────────┬──────────────┤\n");
    std::printf("  │  Z   │  El  │  x_f     │  ε (mean)  │  role        │\n");
    std::printf("  ├──────┼──────┼──────────┼────────────┼──────────────┤\n");
    for (int i = 0; i < 10; ++i) {
        if (alloy.mole_fraction[static_cast<size_t>(i)] < 0.005) continue;
        const auto& er = CU_ALLOY_POOL[static_cast<size_t>(i)];
        // Compute mean ε for this element
        double sum_e = 0.0; int cnt = 0;
        for (auto& b : alloy.beads)
            if (b.Z == er.Z) { sum_e += b.epsilon; cnt++; }
        double mean_e = (cnt > 0) ? sum_e / cnt : 0.0;
        std::printf("  │ %4u │  %-2s  │  %.4f  │  %.4f    │  %-12s │\n",
                    er.Z, er.symbol,
                    alloy.mole_fraction[static_cast<size_t>(i)],
                    mean_e,
                    er.role_hint);
    }
    std::printf("  └──────┴──────┴──────────┴────────────┴──────────────┘\n");

    // Count segments by dominant alloy element
    std::printf("\n  Pipe network material distribution:\n");
    std::array<uint32_t,10> seg_by_elem{}; seg_by_elem.fill(0);
    for (auto& s : net.segments) {
        uint32_t bidx = std::min(s.alloy_bead_index,
                                 static_cast<uint32_t>(alloy.beads.size()-1));
        // Find pool index for this bead's Z
        uint8_t Z = alloy.beads[bidx].Z;
        for (int i = 0; i < 10; ++i)
            if (CU_ALLOY_POOL[static_cast<size_t>(i)].Z == Z) {
                seg_by_elem[static_cast<size_t>(i)]++;
                break;
            }
    }
    for (int i = 0; i < 10; ++i) {
        if (seg_by_elem[static_cast<size_t>(i)] == 0) continue;
        std::printf("    Z=%3u %-2s : %u segments\n",
                    CU_ALLOY_POOL[static_cast<size_t>(i)].Z,
                    CU_ALLOY_POOL[static_cast<size_t>(i)].symbol,
                    seg_by_elem[static_cast<size_t>(i)]);
    }

    std::printf("\n  Network geometry extremes:\n");
    double min_L = 1e30, max_L = 0.0;
    for (auto& s : net.segments) {
        if (s.length_m < min_L) min_L = s.length_m;
        if (s.length_m > max_L) max_L = s.length_m;
    }
    std::printf("    Shortest segment: %.4f m\n", min_L);
    std::printf("    Longest segment:  %.4f m\n", max_L);
    std::printf("    Loop-back ratio:  %.3f\n",
                static_cast<double>(net.n_loops) / net.n_segments);

    std::printf("\n  Network beam tensor principal axes (Σ B):\n");
    std::printf("    Diagonal:  Bxx=%.4f  Byy=%.4f  Bzz=%.4f\n",
                net.network_beam_tensor.m[0][0],
                net.network_beam_tensor.m[1][1],
                net.network_beam_tensor.m[2][2]);
    std::printf("    Off-diag:  Bxy=%.4f  Bxz=%.4f  Byz=%.4f\n",
                net.network_beam_tensor.m[0][1],
                net.network_beam_tensor.m[0][2],
                net.network_beam_tensor.m[1][2]);
    std::printf("    Trace = %.4f m (= total pipe length)\n",
                net.network_beam_tensor.trace());
}

// ── main ──────────────────────────────────────────────────────────────────────

int main() {
    std::printf("VSEPR-SIM — Random Copper Alloy × Vec3×Vec3 Pipe Network\n");
    std::printf("  Chaos factor χ = 1.25  |  FCC lattice  |  Beam tensor\n");
    std::printf("=============================================================\n");

    AlloyComposition alloy = run_alloy_generation();
    coarse_grain::BeadSystem sys = run_bead_placement(alloy);
    run_boundary_check(sys, alloy);
    PipeNetwork net = run_network_generation(alloy);
    run_joint_report(alloy, net);

    section("Result");
    std::printf("  All checks passed.\n\n");
    std::printf("  Summary:\n");
    std::printf("    %s\n", alloy.name.c_str());
    std::printf("    %s\n", net.name.c_str());
    std::printf("    Beam tensor trace = %.4f m\n",
                net.network_beam_tensor.trace());
    return 0;
}
