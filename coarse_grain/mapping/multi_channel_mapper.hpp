#pragma once
/**
 * multi_channel_mapper.hpp — Multi-Channel Surface Descriptor Generator
 *
 * Extends the single-channel SurfaceMapper to compute multi-channel
 * descriptors with separate probe potentials for each physical channel:
 *
 *   - Steric:        (σ/r)^6 repulsive probe (geometric shape)
 *   - Electrostatic: q_i / r Coulombic probe (charge landscape)
 *   - Dispersion:    ε_i · (σ_i/r)^6 weighted probe (interaction strength)
 *
 * Each channel is independently sampled on the same Fibonacci spiral
 * directions and projected onto its own SH basis at its own ℓ_max.
 *
 * Anti-black-box: every probe potential, sampling point, and projection
 * step is explicit and deterministic.
 *
 * Reference: "Descriptor Enrichment for Complex Anisotropic Structures"
 *            subsection of section_anisotropic_beads.tex
 */

#include "coarse_grain/core/multi_channel_descriptor.hpp"
#include "coarse_grain/core/spherical_harmonics.hpp"
#include "coarse_grain/core/inertia_frame.hpp"
#include "atomistic/core/state.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace coarse_grain {

/**
 * MultiChannelMapperConfig — per-channel probe configuration.
 */
struct MultiChannelMapperConfig {
    int    n_samples       = 100;    // Number of probe directions on sphere
    double probe_radius    = 3.0;    // Distance from COM to probe point (Å)

    // Per-channel ℓ_max (default all to 4 for backward compatibility)
    int l_max_steric       = 4;
    int l_max_electrostatic = 4;
    int l_max_dispersion   = 4;

    // Steric probe
    double steric_sigma    = 1.0;    // σ for (σ/r)^6 steric probe (Å)

    // Dispersion probe defaults (per-atom ε_i used from state if available)
    double disp_sigma      = 1.0;    // Default dispersion σ (Å)
    double disp_epsilon    = 1.0;    // Default dispersion ε weight

    /**
     * Set all channels to the same ℓ_max.
     */
    void set_uniform_l_max(int l_max) {
        l_max_steric = l_max;
        l_max_electrostatic = l_max;
        l_max_dispersion = l_max;
    }
};

/**
 * MultiChannelMapper — stateless, deterministic multi-channel generator.
 *
 * Usage:
 *   MultiChannelMapper mapper;
 *   auto desc = mapper.compute(state, atom_indices, com, config);
 */
class MultiChannelMapper {
public:
    /**
     * Compute multi-channel surface descriptor for an atom group.
     *
     * @param state    Atomistic state (positions, masses, charges)
     * @param indices  Parent atom indices for this bead
     * @param com      Center of mass of the bead
     * @param config   Sampling and per-channel configuration
     * @return Fully populated MultiChannelDescriptor
     */
    MultiChannelDescriptor compute(const atomistic::State& state,
                                   const std::vector<uint32_t>& indices,
                                   const atomistic::Vec3& com,
                                   const MultiChannelMapperConfig& config = {}) const
    {
        MultiChannelDescriptor desc;
        desc.probe_radius = config.probe_radius;
        desc.n_samples = config.n_samples;

        // Initialize channels with their respective ℓ_max
        desc.init(config.l_max_steric, config.l_max_electrostatic, config.l_max_dispersion);

        // Step 1: Compute inertia frame (shared by all channels)
        desc.frame = compute_inertia_frame(state.X, state.M, indices, com);

        if (indices.size() < 2) {
            // Single atom → isotropic descriptor on all channels
            double iso = std::sqrt(4.0 * 3.14159265358979323846);
            if (!desc.steric.coeffs.empty())        desc.steric.coeffs[0] = iso;
            if (!desc.electrostatic.coeffs.empty())  desc.electrostatic.coeffs[0] = iso;
            if (!desc.dispersion.coeffs.empty())     desc.dispersion.coeffs[0] = iso;
            return desc;
        }

        // Step 2: Generate sample directions via Fibonacci spiral
        auto directions = fibonacci_sphere(config.n_samples);

        // Step 3: Evaluate per-channel probe potential at each sample point
        std::vector<double> steric_samples(config.n_samples);
        std::vector<double> elec_samples(config.n_samples);
        std::vector<double> disp_samples(config.n_samples);

        for (int s = 0; s < config.n_samples; ++s) {
            atomistic::Vec3 probe_world = {
                com.x + config.probe_radius * directions[s].x,
                com.y + config.probe_radius * directions[s].y,
                com.z + config.probe_radius * directions[s].z
            };

            steric_samples[s] = probe_steric(probe_world, state, indices, config.steric_sigma);
            elec_samples[s]   = probe_electrostatic(probe_world, state, indices);
            disp_samples[s]   = probe_dispersion(probe_world, state, indices,
                                                  config.disp_sigma, config.disp_epsilon);
        }

        // Step 4: Project each channel onto its SH basis in local frame
        project_channel(desc.steric, directions, steric_samples, desc.frame, config.n_samples);
        project_channel(desc.electrostatic, directions, elec_samples, desc.frame, config.n_samples);
        project_channel(desc.dispersion, directions, disp_samples, desc.frame, config.n_samples);

        return desc;
    }

private:
    // ====================================================================
    // Fibonacci sphere sampling (deterministic)
    // ====================================================================
    static std::vector<atomistic::Vec3> fibonacci_sphere(int n) {
        std::vector<atomistic::Vec3> dirs(n);
        constexpr double golden_ratio = 1.6180339887498948482;
        constexpr double two_pi = 2.0 * 3.14159265358979323846;

        for (int i = 0; i < n; ++i) {
            double theta = std::acos(1.0 - 2.0 * (i + 0.5) / n);
            double phi   = two_pi * i / golden_ratio;
            dirs[i] = {
                std::sin(theta) * std::cos(phi),
                std::sin(theta) * std::sin(phi),
                std::cos(theta)
            };
        }
        return dirs;
    }

    // ====================================================================
    // SH projection for one channel
    // ====================================================================
    static void project_channel(ChannelDescriptor& ch,
                                const std::vector<atomistic::Vec3>& directions,
                                const std::vector<double>& samples,
                                const InertiaFrame& frame,
                                int n_samples)
    {
        constexpr double four_pi = 4.0 * 3.14159265358979323846;
        double weight = four_pi / static_cast<double>(n_samples);

        for (int s = 0; s < n_samples; ++s) {
            atomistic::Vec3 local_dir = frame.to_local(directions[s]);

            double r = std::sqrt(local_dir.x * local_dir.x +
                                 local_dir.y * local_dir.y +
                                 local_dir.z * local_dir.z);
            if (r < 1e-30) continue;

            double theta = std::acos(std::clamp(local_dir.z / r, -1.0, 1.0));
            double phi   = std::atan2(local_dir.y, local_dir.x);
            if (phi < 0.0) phi += 2.0 * 3.14159265358979323846;

            auto Y = evaluate_all_harmonics_dynamic(theta, phi, ch.l_max);

            int n_coeffs = static_cast<int>(std::min(ch.coeffs.size(), Y.size()));
            for (int i = 0; i < n_coeffs; ++i) {
                ch.coeffs[i] += weight * samples[s] * Y[i];
            }
        }
    }

    // ====================================================================
    // Per-channel probe potentials
    // ====================================================================

    /**
     * Steric probe: Σ (σ/d)^6 — purely repulsive, maps excluded volume.
     */
    static double probe_steric(const atomistic::Vec3& probe_pos,
                               const atomistic::State& state,
                               const std::vector<uint32_t>& indices,
                               double sigma)
    {
        double total = 0.0;
        double s6 = sigma * sigma * sigma * sigma * sigma * sigma;
        for (uint32_t idx : indices) {
            double dx = probe_pos.x - state.X[idx].x;
            double dy = probe_pos.y - state.X[idx].y;
            double dz = probe_pos.z - state.X[idx].z;
            double r2 = dx * dx + dy * dy + dz * dz;
            if (r2 < 1e-10) r2 = 1e-10;
            double r6 = r2 * r2 * r2;
            total += s6 / r6;
        }
        return total;
    }

    /**
     * Electrostatic probe: Σ q_i / d — Coulombic potential from charges.
     */
    static double probe_electrostatic(const atomistic::Vec3& probe_pos,
                                       const atomistic::State& state,
                                       const std::vector<uint32_t>& indices)
    {
        double total = 0.0;
        for (uint32_t idx : indices) {
            double dx = probe_pos.x - state.X[idx].x;
            double dy = probe_pos.y - state.X[idx].y;
            double dz = probe_pos.z - state.X[idx].z;
            double r = std::sqrt(dx * dx + dy * dy + dz * dz);
            if (r < 1e-10) r = 1e-10;
            total += state.Q[idx] / r;
        }
        return total;
    }

    /**
     * Dispersion probe: Σ ε_i · (σ/d)^6 — weighted repulsive, maps
     * interaction strength landscape.
     */
    static double probe_dispersion(const atomistic::Vec3& probe_pos,
                                    const atomistic::State& state,
                                    const std::vector<uint32_t>& indices,
                                    double sigma,
                                    double epsilon)
    {
        double total = 0.0;
        double s6 = sigma * sigma * sigma * sigma * sigma * sigma;
        for (uint32_t idx : indices) {
            double dx = probe_pos.x - state.X[idx].x;
            double dy = probe_pos.y - state.X[idx].y;
            double dz = probe_pos.z - state.X[idx].z;
            double r2 = dx * dx + dy * dy + dz * dz;
            if (r2 < 1e-10) r2 = 1e-10;
            double r6 = r2 * r2 * r2;
            // Use per-atom mass as a proxy for dispersion strength scaling
            double eps_eff = epsilon * state.M[idx] / 12.0;  // Normalize by carbon mass
            total += eps_eff * s6 / r6;
        }
        return total;
    }
};

} // namespace coarse_grain
