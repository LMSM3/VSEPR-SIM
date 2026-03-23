#pragma once
/**
 * surface_mapper.hpp — Probe-Based Surface Descriptor Generator
 *
 * Computes anisotropic surface descriptors for coarse-grained beads
 * by sampling a probe potential on a spherical shell around the bead
 * center and projecting the result onto spherical harmonics.
 *
 * Pipeline:
 *   1. Compute COM and inertia frame for atom group.
 *   2. Generate uniform sample directions (Fibonacci spiral).
 *   3. At each direction, evaluate probe interaction with parent atoms.
 *   4. Project sampled values onto SH basis via least-squares.
 *   5. Store coefficients in SurfaceDescriptor.
 *
 * Anti-black-box: every step is explicit, every intermediate is stored.
 * The probe potential, sampling strategy, and projection method are
 * all visible and deterministic.
 *
 * Reference: Equations (5)-(7) of section_anisotropic_beads.tex
 */

#include "coarse_grain/core/surface_descriptor.hpp"
#include "coarse_grain/core/spherical_harmonics.hpp"
#include "coarse_grain/core/inertia_frame.hpp"
#include "atomistic/core/state.hpp"
#include <cmath>
#include <cstdint>
#include <vector>

namespace coarse_grain {

/**
 * SurfaceMapperConfig — controls the surface sampling process.
 */
struct SurfaceMapperConfig {
    int    n_samples    = 100;    // Number of probe directions on sphere
    double probe_radius = 3.0;   // Distance from COM to probe point (Å)
    double probe_sigma  = 1.0;   // Probe LJ sigma (Å)
};

/**
 * SurfaceMapper — stateless, deterministic surface descriptor generator.
 *
 * Usage:
 *   SurfaceMapper mapper;
 *   auto desc = mapper.compute(state, atom_indices, com, config);
 */
class SurfaceMapper {
public:
    /**
     * Compute the surface descriptor for an atom group.
     *
     * @param state    Atomistic state (positions, masses)
     * @param indices  Parent atom indices for this bead
     * @param com      Center of mass of the bead
     * @param config   Sampling configuration
     * @return Fully populated SurfaceDescriptor
     */
    SurfaceDescriptor compute(const atomistic::State& state,
                              const std::vector<uint32_t>& indices,
                              const atomistic::Vec3& com,
                              const SurfaceMapperConfig& config = {}) const
    {
        SurfaceDescriptor desc;
        desc.probe_radius = config.probe_radius;
        desc.n_samples = config.n_samples;

        // Step 1: Compute inertia frame
        desc.frame = compute_inertia_frame(state.X, state.M, indices, com);

        if (indices.size() < 2) {
            // Single atom → isotropic descriptor
            desc.coeffs[0] = std::sqrt(4.0 * M_PI);  // Uniform unit sphere
            return desc;
        }

        // Step 2: Generate sample directions via Fibonacci spiral
        auto directions = fibonacci_sphere(config.n_samples);

        // Step 3: Evaluate probe potential at each sample point
        std::vector<double> samples(config.n_samples);
        for (int s = 0; s < config.n_samples; ++s) {
            // Probe point in world space
            atomistic::Vec3 probe_world = {
                com.x + config.probe_radius * directions[s].x,
                com.y + config.probe_radius * directions[s].y,
                com.z + config.probe_radius * directions[s].z
            };

            // Sum repulsive probe interaction with all parent atoms
            samples[s] = probe_potential(probe_world, state, indices, config.probe_sigma);
        }

        // Step 4: Project onto SH basis in local frame
        // Using pseudo-inverse via orthogonality of SH on uniform samples:
        //   c_ℓm ≈ (4π / N) Σ_s f(s) · Y_ℓm(θ_s, φ_s)
        constexpr double four_pi = 4.0 * 3.14159265358979323846;
        double weight = four_pi / static_cast<double>(config.n_samples);

        for (int s = 0; s < config.n_samples; ++s) {
            // Get direction in local frame
            atomistic::Vec3 local_dir = desc.frame.to_local(directions[s]);

            // Convert to spherical angles
            double r = std::sqrt(local_dir.x * local_dir.x +
                                 local_dir.y * local_dir.y +
                                 local_dir.z * local_dir.z);
            if (r < 1e-30) continue;

            double theta = std::acos(std::clamp(local_dir.z / r, -1.0, 1.0));
            double phi   = std::atan2(local_dir.y, local_dir.x);
            if (phi < 0.0) phi += 2.0 * 3.14159265358979323846;

            // Evaluate all harmonics at this direction
            auto Y = evaluate_all_harmonics(theta, phi);

            // Accumulate projection
            for (int i = 0; i < SH_NUM_COEFFS; ++i) {
                desc.coeffs[i] += weight * samples[s] * Y[i];
            }
        }

        return desc;
    }

private:
    /**
     * Generate approximately uniform sample directions on S² using
     * the Fibonacci spiral method.
     *
     * Deterministic: same n always produces the same directions.
     */
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

    /**
     * Probe potential: sum of repulsive (σ/d)^6 over parent atoms.
     *
     * This purely repulsive potential maps the effective shape of the
     * atom group as seen from direction (θ, φ) at distance probe_radius.
     * High values indicate the probe overlaps with atoms (bead extends
     * in that direction); low values indicate empty space.
     */
    static double probe_potential(const atomistic::Vec3& probe_pos,
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

            // Avoid division by zero
            if (r2 < 1e-10) r2 = 1e-10;

            double r6 = r2 * r2 * r2;
            total += s6 / r6;   // (σ/r)^6 repulsive
        }
        return total;
    }
};

} // namespace coarse_grain
