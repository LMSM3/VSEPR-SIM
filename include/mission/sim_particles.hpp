#pragma once
/**
 * sim_particles.hpp
 * =================
 * V1 — Particles N
 * Scale Mission: Particles, Clouds, Lattice, and Pipe Gas 3
 *
 * Discrete particle engine for small-to-large N systems.
 *
 * Physical scope:
 *   - Mass, mobility, collision, local force/state updates
 *   - Optional reaction / decay-response layers
 *   - Bead and atomistic object coexistence
 *   - Powder seeds and precursor objects
 *
 * State:
 *   X_i = {id, kind, r_i, v_i, a_i, m_i, q_i, σ_i, η_i, D_i, T_i, S_i}
 *
 * Stress bands (from mission work order):
 *   Instant    : N =   8–32      single-step, static check
 *   Short_50ms : N =  64–256     fixed dt, 1–10 steps
 *   Medium_5s  : N = 1e3–1e4     short trajectory, local statistics
 *   Long_5min  : N = 1e4–1e5     simplified kernels or N=1e3–1e4 rich history
 *
 * Deliverables:
 *   - deterministic benchmark set
 *   - mobility histogram
 *   - collision matrix
 *   - exportable .xyza / .xyzf path data
 *
 * Integrates with:
 *   include/mission/mission_profile.hpp  — shared profile + entity layer
 *   include/physics/particle_id.hpp      — species code namespace
 *   include/core/species_family.hpp      — family classification
 */

#include "mission/mission_profile.hpp"
#include "core/element_descriptor.hpp"

#include <vector>
#include <cstddef>
#include <cstdint>
#include <cmath>
#include <string>
#include <functional>
#include <algorithm>
#include <numeric>
#include <sstream>
#include <iomanip>
#include <chrono>

namespace vsepr {
namespace mission {
namespace particles {

// ============================================================================
// RuntimeProfile factory for V1 Particles N
// ============================================================================

inline RuntimeProfile profile_for(MissionScale s) {
    RuntimeProfile p{};
    p.scale = s;
    p.pairwise_interactions = true;
    p.export_csv = true;

    switch (s) {
        case MissionScale::Instant:
            p.max_entities       = 16;
            p.max_steps          = 1;
            p.dt                 = 1e-15;
            p.high_accuracy      = false;
            p.neighbor_list      = false;
            p.export_full_history = false;
            p.export_markdown    = false;
            p.export_xyz         = false;
            p.approximation_level = 0;
            p.wall_budget_s      = 0.0;
            break;

        case MissionScale::Short_50ms:
            p.max_entities       = 128;
            p.max_steps          = 10;
            p.dt                 = 1e-15;
            p.high_accuracy      = false;
            p.neighbor_list      = false;
            p.export_full_history = false;
            p.export_markdown    = false;
            p.export_xyz         = true;
            p.approximation_level = 0;
            p.wall_budget_s      = 0.05;
            break;

        case MissionScale::Medium_5s:
            p.max_entities       = 4096;
            p.max_steps          = 500;
            p.dt                 = 1e-15;
            p.high_accuracy      = true;
            p.neighbor_list      = true;
            p.export_full_history = false;
            p.export_markdown    = true;
            p.export_xyz         = true;
            p.approximation_level = 0;
            p.wall_budget_s      = 5.0;
            break;

        case MissionScale::Long_5min:
            p.max_entities       = 50000;
            p.max_steps          = 2000;
            p.dt                 = 1e-15;
            p.high_accuracy      = false;
            p.neighbor_list      = true;
            p.export_full_history = true;
            p.export_markdown    = true;
            p.export_xyz         = true;
            p.approximation_level = 1;
            p.wall_budget_s      = 300.0;
            break;
    }
    return p;
}

// ============================================================================
// Particle record — one discrete particle in the engine
// ============================================================================

struct Particle {
    EntityIdentity identity;
    ExternalLayer  ext;

    // Collision counter (per particle)
    std::size_t collision_count {0};

    // Cluster membership (-1 = free)
    int cluster_id {-1};
};

// ============================================================================
// Collision event record
// ============================================================================

struct CollisionEvent {
    std::size_t i;
    std::size_t j;
    double      relative_speed;
    double      impact_parameter;
};

// ============================================================================
// Particle system state
// ============================================================================

struct ParticleSystem {
    std::vector<Particle>        particles;
    std::vector<CollisionEvent>  collision_log;
    RuntimeProfile               profile;
    std::size_t                  step       {0};
    double                       sim_time   {0.0};
    double                       energy_kin {0.0};
    double                       energy_pot {0.0};

    std::size_t size() const { return particles.size(); }
};

// ============================================================================
// Initialisation helpers
// ============================================================================

// Seed a uniform-temperature particle gas in a cubic box of side L (Å)
inline ParticleSystem make_particle_system(
    std::size_t N,
    double      box_L,
    double      mass_amu,
    double      sigma_A,
    double      T_K,
    MissionScale scale,
    uint64_t    seed = 42)
{
    ParticleSystem sys;
    sys.profile = profile_for(scale);
    sys.particles.reserve(N);

    // Simple LCG for reproducible positions
    uint64_t rng = seed ^ 0xdeadbeefcafe1234ull;
    auto lcg = [&]() -> double {
        rng = rng * 6364136223846793005ull + 1442695040888963407ull;
        return static_cast<double>(rng >> 11) / static_cast<double>(1ull << 53);
    };

    // Maxwell-Boltzmann σ_v = sqrt(kT/m)  [Å/ps units: kB=8.314e-3 kJ/(mol·K), m in amu]
    // 1 amu·(Å/ps)² = 1.66054e-4 kJ/mol → kB = 8.314e-3 / 1.66054e-4 ≈ 50.07 amu·Å²/ps²/K/atom
    const double kB_reduced = 0.008314462; // kJ/(mol·K)  (using kJ/mol convention throughout)
    const double sigma_v    = std::sqrt(kB_reduced * T_K / mass_amu);

    for (std::size_t i = 0; i < N; ++i) {
        Particle p{};
        p.identity.id       = i;
        p.identity.family   = SpeciesFamily::GAS;
        p.identity.subfamily = SpeciesSubfamily::GAS_POLYATOMIC;
        p.identity.name     = "pseudo-particle";
        p.identity.capabilities = Capability::Kinematic | Capability::Steric;
        p.identity.enabled_channels = {"kinematic", "steric", "drag"};

        auto& st = p.identity.state;
        st.position    = { lcg()*box_L, lcg()*box_L, lcg()*box_L };
        // Box-Muller for normal velocity distribution
        double u1 = lcg(), u2 = lcg();
        double z0 = std::sqrt(-2.0*std::log(u1+1e-30)) * std::cos(6.2831853*u2);
        double z1 = std::sqrt(-2.0*std::log(u1+1e-30)) * std::sin(6.2831853*u2);
        double u3 = lcg(), u4 = lcg();
        double z2 = std::sqrt(-2.0*std::log(u3+1e-30)) * std::cos(6.2831853*u4);
        st.velocity    = { sigma_v*z0, sigma_v*z1, sigma_v*z2 };
        st.mass        = mass_amu;
        st.sigma       = sigma_A;
        st.temperature = T_K;
        st.species_code = 0; // placeholder

        sys.particles.push_back(std::move(p));
    }
    return sys;
}

// ============================================================================
// Steric exclusion channel — dyn_steric
// Applies a truncated WCA repulsion between all pairs within 2σ_ij cutoff.
// Contract:
//   Required state: position, sigma, mass
//   Writes:         force accumulator in ExternalLayer
//   Pairwise:       yes  O(N²) or O(N) with neighbor list
//   Conservation:   momentum (equal + opposite forces)
//   dt_max:         0.1 * σ / v_max
// ============================================================================

inline void apply_steric(ParticleSystem& sys) {
    const std::size_t N = sys.size();
    for (auto& p : sys.particles) p.ext.zero_force();

    for (std::size_t i = 0; i < N; ++i) {
        for (std::size_t j = i + 1; j < N; ++j) {
            auto& pi = sys.particles[i];
            auto& pj = sys.particles[j];

            const double sig_ij = 0.5 * (pi.identity.state.sigma +
                                          pj.identity.state.sigma);
            const double r_cut  = sig_ij * 1.122462; // 2^(1/6) * sig

            const auto&  ri = pi.identity.state.position;
            const auto&  rj = pj.identity.state.position;
            double dx = ri[0]-rj[0], dy = ri[1]-rj[1], dz = ri[2]-rj[2];
            double r2 = dx*dx + dy*dy + dz*dz;

            if (r2 < r_cut * r_cut && r2 > 1e-12) {
                // LJ derivative (WCA: only repulsive part)
                double eps = std::min(pi.identity.state.epsilon,
                                     pj.identity.state.epsilon);
                double s2 = sig_ij*sig_ij / r2;
                double s6 = s2*s2*s2;
                double f_over_r = 48.0 * eps * s6 * (s6 - 0.5) / r2;

                pi.ext.force[0] += f_over_r * dx;
                pi.ext.force[1] += f_over_r * dy;
                pi.ext.force[2] += f_over_r * dz;
                pj.ext.force[0] -= f_over_r * dx;
                pj.ext.force[1] -= f_over_r * dy;
                pj.ext.force[2] -= f_over_r * dz;

                // Log collision if contact
                if (r2 < sig_ij * sig_ij) {
                    double vrel = 0.0;
                    for (int k = 0; k < 3; ++k) {
                        double dv = pi.identity.state.velocity[k] -
                                    pj.identity.state.velocity[k];
                        vrel += dv*dv;
                    }
                    vrel = std::sqrt(vrel);
                    sys.collision_log.push_back({i, j, vrel, std::sqrt(r2)});
                    ++sys.particles[i].collision_count;
                    ++sys.particles[j].collision_count;
                }
            }
        }
    }
}

// ============================================================================
// Kinematic update channel — dyn_kinematic
// Velocity-Verlet first half + second half.
// Contract:
//   Required state: position, velocity, accel, mass, force
//   Writes:         position, velocity, accel
//   Conservation:   energy (Hamiltonian)
//   dt_max:         profile.dt (validated externally)
// ============================================================================

inline void integrate_kinematic(ParticleSystem& sys, double dt) {
    for (auto& p : sys.particles) {
        auto& st = p.identity.state;
        const double inv_m = 1.0 / st.mass;
        // Velocity Verlet: x += v*dt + 0.5*a*dt²
        for (int k = 0; k < 3; ++k) {
            st.position[k] += st.velocity[k]*dt + 0.5*st.accel[k]*dt*dt;
        }
        // Store old accel, compute new from force
        std::array<double,3> a_new;
        for (int k = 0; k < 3; ++k)
            a_new[k] = p.ext.force[k] * inv_m;
        // v += 0.5*(a_old + a_new)*dt
        for (int k = 0; k < 3; ++k)
            st.velocity[k] += 0.5*(st.accel[k] + a_new[k])*dt;
        st.accel = a_new;
    }
}

// ============================================================================
// Kinetic energy sum
// ============================================================================

inline double kinetic_energy(const ParticleSystem& sys) {
    double ke = 0.0;
    for (const auto& p : sys.particles) {
        const auto& v = p.identity.state.velocity;
        ke += 0.5 * p.identity.state.mass * (v[0]*v[0]+v[1]*v[1]+v[2]*v[2]);
    }
    return ke;
}

// ============================================================================
// Mobility histogram
//   Bins |v| into n_bins bins from 0 to v_max.
//   Returns (bin_edges, counts) pair.
// ============================================================================

struct MobilityHistogram {
    std::vector<double> bin_edges;   // n_bins+1 values
    std::vector<std::size_t> counts; // n_bins values
    double v_mean {0.0};
    double v_rms  {0.0};
};

inline MobilityHistogram mobility_histogram(
    const ParticleSystem& sys,
    int n_bins = 20)
{
    std::vector<double> speeds;
    speeds.reserve(sys.size());
    for (const auto& p : sys.particles) {
        const auto& v = p.identity.state.velocity;
        speeds.push_back(std::sqrt(v[0]*v[0]+v[1]*v[1]+v[2]*v[2]));
    }
    if (speeds.empty()) return {};

    const double v_max = *std::max_element(speeds.begin(), speeds.end()) * 1.05;
    const double dv    = v_max / n_bins;

    MobilityHistogram h;
    h.bin_edges.resize(n_bins + 1);
    h.counts.resize(n_bins, 0);
    for (int i = 0; i <= n_bins; ++i) h.bin_edges[i] = i * dv;
    for (double s : speeds) {
        int bin = std::min(static_cast<int>(s / dv), n_bins - 1);
        ++h.counts[bin];
    }
    double sum = 0.0, sum2 = 0.0;
    for (double s : speeds) { sum += s; sum2 += s*s; }
    h.v_mean = sum  / speeds.size();
    h.v_rms  = std::sqrt(sum2 / speeds.size());
    return h;
}

// ============================================================================
// Main run function — deterministic scheduler
// ============================================================================

inline MissionDeliverable run(ParticleSystem& sys) {
    MissionDeliverable d{};
    d.sim_version   = "V1_Particles";
    d.scale         = sys.profile.scale;
    d.entity_count  = sys.size();

    const double dt = sys.profile.dt;
    const std::size_t max_steps = sys.profile.max_steps;

    auto t_start = std::chrono::steady_clock::now();

    for (std::size_t step = 0; step < max_steps; ++step) {
        // 1. Accumulate forces (steric is the only channel installed at V1 baseline)
        apply_steric(sys);
        // 2. Integrate
        integrate_kinematic(sys, dt);

        sys.step     = step + 1;
        sys.sim_time = (step + 1) * dt;

        // Wall budget check
        if (sys.profile.wall_budget_s > 0.0) {
            auto elapsed = std::chrono::duration<double>(
                std::chrono::steady_clock::now() - t_start).count();
            if (elapsed > sys.profile.wall_budget_s) break;
        }
    }

    sys.energy_kin = kinetic_energy(sys);

    auto elapsed = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - t_start).count();

    d.steps_run     = sys.step;
    d.wall_time_s   = elapsed;
    d.energy_total  = sys.energy_kin;
    d.converged     = true;

    auto h = mobility_histogram(sys);
    d.mobility_mean = h.v_mean;
    d.collision_rate = sys.collision_log.size() > 0
        ? static_cast<double>(sys.collision_log.size()) / static_cast<double>(std::max(std::size_t{1}, sys.step))
        : 0.0;

    return d;
}

// ============================================================================
// Console report
// ============================================================================

inline std::string report(const ParticleSystem& sys, const MissionDeliverable& d) {
    std::ostringstream o;
    o << "\n  V1 Particles N — " << mission_scale_name(sys.profile.scale) << "\n";
    o << "  " << std::string(60, '-') << "\n";
    o << "  Entities  : " << d.entity_count << "\n";
    o << "  Steps run : " << d.steps_run << "\n";
    o << "  Wall time : " << std::fixed << std::setprecision(4) << d.wall_time_s << " s\n";
    o << "  E_kin     : " << sys.energy_kin << " (reduced units)\n";
    o << "  Collisions: " << sys.collision_log.size() << "\n";
    if (d.mobility_mean) o << "  v_mean    : " << *d.mobility_mean << "\n";

    auto h = mobility_histogram(sys, 10);
    o << "  Mobility histogram (|v|):\n";
    for (std::size_t i = 0; i < h.counts.size(); ++i) {
        o << "    [" << std::setw(6) << std::fixed << std::setprecision(2)
          << h.bin_edges[i] << " – " << h.bin_edges[i+1] << "]  ";
        std::size_t bar = h.counts[i] * 30 / std::max(std::size_t{1},
            *std::max_element(h.counts.begin(), h.counts.end()));
        o << std::string(bar, '*') << "  (" << h.counts[i] << ")\n";
    }
    return o.str();
}

} // namespace particles
} // namespace mission
} // namespace vsepr
