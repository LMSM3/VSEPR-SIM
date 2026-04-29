/**
 * gas2_kinetic.hpp
 * ----------------
 * Kinetic Theory of Gases for gas2 Module.
 *
 * Provides:
 *   - Maxwell-Boltzmann speed distribution (analytic + sampling)
 *   - Transport properties: viscosity, thermal conductivity, diffusion
 *   - Mean free path and collision frequency
 *   - Speed statistics (RMS, mean, most probable)
 *   - Energy partition (translational, rotational, vibrational DOF)
 *
 * Mirrors pattern: thermal_model.hpp     (per-atom thermal state)
 *                  gas_module.hpp        (speed calculations, MB sampling)
 *
 * Theory note: degrees of freedom f for energy partition:
 *   monoatomic:  f = 3  (translation only)
 *   diatomic:    f = 5  (3 trans + 2 rot), f = 7 at high T (+ 2 vib)
 *   polyatomic:  f = 6  (3 trans + 3 rot), higher with vib modes
 *
 * Anti-black-box: all intermediate DOF, sigma, lambda values are stored.
 */

#pragma once

#include "gas2_constants.hpp"
#include "gas2_species.hpp"
#include <cmath>
#include <vector>
#include <random>
#include <cstdint>

namespace vsepr {
namespace gas2 {

// ============================================================================
// Degrees of freedom
// ============================================================================

struct DOFPartition {
    int translational;   // always 3
    int rotational;      // 0 (mono), 2 (linear), 3 (nonlinear)
    int vibrational;     // 2*(3N-5) linear, 2*(3N-6) nonlinear at high T
    int total_classical; // trans + rot (low T, vib frozen)
    int total_full;      // trans + rot + vib (high T limit)
};

inline DOFPartition compute_dof(int n_atoms, bool is_linear = false) {
    DOFPartition d{};
    d.translational = 3;
    if (n_atoms == 1) {
        d.rotational = 0;
        d.vibrational = 0;
    } else if (is_linear || n_atoms == 2) {
        d.rotational = 2;
        d.vibrational = 2 * (3 * n_atoms - 5);
    } else {
        d.rotational = 3;
        d.vibrational = 2 * (3 * n_atoms - 6);
    }
    d.total_classical = d.translational + d.rotational;
    d.total_full = d.translational + d.rotational + d.vibrational;
    return d;
}

// ============================================================================
// Speed statistics
// ============================================================================

// RMS speed: v_rms = sqrt(3RT/M)
inline double rms_speed(double T, double M_kg) {
    return std::sqrt(3.0 * R_gas * T / M_kg);
}

// Mean speed: v_mean = sqrt(8RT/(pi·M))
inline double mean_speed(double T, double M_kg) {
    return std::sqrt(8.0 * R_gas * T / (PI * M_kg));
}

// Most probable speed: v_mp = sqrt(2RT/M)
inline double most_probable_speed(double T, double M_kg) {
    return std::sqrt(2.0 * R_gas * T / M_kg);
}

// Average translational KE per molecule: 3/2 kBT
inline double avg_translational_ke(double T) {
    return 1.5 * kB * T;
}

// Average total KE per molecule using equipartition: f/2 kBT
inline double avg_total_ke(double T, int dof) {
    return 0.5 * static_cast<double>(dof) * kB * T;
}

// Hartree-converted average translational KE: (3/2 kBT) / Eh
// Mass-invariant — depends only on temperature.
inline double avg_translational_ke_Eh(double T) {
    return avg_translational_ke(T) / Hartree_J;
}

// Hartree-converted average total KE: (f/2 kBT) / Eh
inline double avg_total_ke_Eh(double T, int dof) {
    return avg_total_ke(T, dof) / Hartree_J;
}

// ============================================================================
// Mean free path and collision rate
// ============================================================================

// lambda = kBT / (sqrt(2) · pi · d^2 · P)
inline double mean_free_path(double T, double P_Pa, double d_m) {
    return kB * T / (SQRT2 * PI * d_m * d_m * P_Pa);
}

// Collision frequency z = v_mean / lambda
inline double collision_frequency(double T, double P_Pa, double M_kg, double d_m) {
    double v = mean_speed(T, M_kg);
    double lam = mean_free_path(T, P_Pa, d_m);
    return (lam > 0.0) ? v / lam : 0.0;
}

// ============================================================================
// Transport: Chapman-Enskog first approximation
// ============================================================================

// Viscosity (hard sphere, Chapman-Enskog first approximation):
//   eta = (5/16) * sqrt(m * kB * T / pi) / (sigma^2 * Omega_22)
// where Omega_22 = 1.0 for hard spheres.
// Derivation: m is per-molecule mass (kg), sigma in metres.
// Note: the numerator factor is (5/16)*sqrt(mkBT/pi), NOT (5/16sqrt(pi))*sqrt(mkBT)/pi.
inline double viscosity_hard_sphere(double T, double M_kg, double d_m) {
    double m = M_kg / N_A;  // kg/mol -> kg/molecule
    return (5.0 / 16.0)
           * std::sqrt(m * kB * T / PI) / (d_m * d_m);
}

// Thermal conductivity (monoatomic): k = (15/4) · mu · kB / m
inline double thermal_cond_mono(double mu, double M_kg) {
    double m = M_kg / N_A;
    return (15.0 / 4.0) * mu * kB / m;
}

// Self-diffusion coefficient: D = 3/(8·n·d^2) · sqrt(kBT/(pi·m))
inline double self_diffusion(double T, double P_Pa, double M_kg, double d_m) {
    double n_density = P_Pa / (kB * T);  // number density
    double m = M_kg / N_A;
    return (3.0 / (8.0 * n_density * d_m * d_m))
           * std::sqrt(kB * T / (PI * m));
}

// ============================================================================
// Maxwell-Boltzmann velocity sampling
// ============================================================================

struct Velocity3 {
    double vx, vy, vz;
    double speed() const { return std::sqrt(vx*vx + vy*vy + vz*vz); }
    double ke(double m_kg) const { return 0.5 * m_kg * (vx*vx + vy*vy + vz*vz); }
};

inline std::vector<Velocity3> sample_mb(double T, double M_kg, size_t count,
                                        uint64_t seed = 42) {
    double m = M_kg / N_A;
    double sigma = std::sqrt(kB * T / m);
    std::mt19937_64 rng(seed);
    std::normal_distribution<double> gauss(0.0, sigma);

    std::vector<Velocity3> v(count);
    for (size_t i = 0; i < count; ++i) {
        v[i] = {gauss(rng), gauss(rng), gauss(rng)};
    }
    return v;
}

} // namespace gas2
} // namespace vsepr
