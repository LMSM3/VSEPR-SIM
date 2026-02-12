#pragma once
#include "state.hpp"
#include <cmath>
#include <vector>
#include <random>

namespace atomistic {
namespace thermo {

/**
 * Statistical Mechanics utilities for molecular simulations
 * 
 * Physics:
 * --------
 * 
 * 1. Temperature (instantaneous):
 *    T = (2·K_kinetic) / (N_df · k_B)
 *    where K = Σ½m_i|v_i|², N_df = 3N - N_constraints
 *    k_B = 0.001987 kcal/(mol·K) [Boltzmann constant]
 * 
 * 2. Pressure (virial equation):
 *    P = (N·k_B·T)/V + (1/3V)Σr_ij·F_ij  [virial term]
 *    Units: kcal/(mol·Å³) → convert to atm or bar
 *    1 kcal/(mol·Å³) = 68568.415 atm = 6.9479×10⁴ bar
 * 
 * 3. Heat capacity (fluctuation):
 *    C_V = k_B + (⟨E²⟩ - ⟨E⟩²)/(k_B·T²)
 *    Canonical ensemble fluctuation-dissipation theorem
 * 
 * 4. Gyration radius:
 *    R_g² = Σm_i|r_i - r_COM|² / Σm_i
 *    Measures molecular compactness
 * 
 * 5. Virial:
 *    W = -Σr_i·F_i = -(1/2)Σr_ij·F_ij
 *    Related to pressure through P = (N·k_B·T + W/3)/V
 * 
 * References:
 * - Allen, M.P. & Tildesley, D.J. (2017). "Computer Simulation of Liquids." 2nd ed.
 * - Frenkel, D. & Smit, B. (2002). "Understanding Molecular Simulation." 2nd ed.
 * - Lebowitz, J.L. et al. (1967). "Ensemble dependence of fluctuations..." Phys. Rev. 153, 250.
 */

// Physical constants
constexpr double kB = 0.001987204;  // kcal/(mol·K) - Boltzmann constant
constexpr double NA = 6.02214076e23; // Avogadro's number
constexpr double kcal_per_mol_A3_to_atm = 68568.415;  // Pressure conversion
constexpr double kcal_per_mol_A3_to_bar = 69478.97;   // Pressure conversion

/**
 * Compute instantaneous kinetic energy
 * K = Σ ½m_i|v_i|²
 */
inline double kinetic_energy(const State& s) {
    if (s.V.size() != s.N || s.M.size() != s.N) return 0.0;
    
    double K = 0.0;
    for (uint32_t i = 0; i < s.N; ++i) {
        K += 0.5 * s.M[i] * dot(s.V[i], s.V[i]);
    }
    return K;
}

/**
 * Compute instantaneous temperature from kinetic energy
 * T = 2K/(N_df · k_B)
 * 
 * @param s State with velocities
 * @param N_constraints Number of holonomic constraints (default 0)
 * @return Temperature in Kelvin
 */
inline double temperature(const State& s, int N_constraints = 0) {
    double K = kinetic_energy(s);
    int N_df = 3*s.N - N_constraints;  // Degrees of freedom
    if (N_df <= 0) return 0.0;
    return (2.0 * K) / (N_df * kB);
}

/**
 * Compute virial: W = -Σr_i·F_i
 * For pair potentials: W = -(1/2)Σr_ij·F_ij
 * 
 * Note: Forces must be from pair interactions (not constraint forces)
 */
inline double virial(const State& s) {
    double W = 0.0;
    for (uint32_t i = 0; i < s.N; ++i) {
        W -= dot(s.X[i], s.F[i]);
    }
    return W;
}

/**
 * Compute instantaneous pressure (virial equation)
 * P = (N·k_B·T)/V + W/(3V)
 * 
 * @param s State with positions, velocities, forces
 * @param volume System volume (Ų)
 * @param N_constraints Number of constraints
 * @return Pressure in kcal/(mol·Å³)
 */
inline double pressure(const State& s, double volume, int N_constraints = 0) {
    if (volume <= 0) return 0.0;
    
    double T = temperature(s, N_constraints);
    double W = virial(s);
    
    return (s.N * kB * T + W/3.0) / volume;
}

/**
 * Convert pressure from kcal/(mol·Å³) to atm
 */
inline double pressure_to_atm(double P_internal) {
    return P_internal * kcal_per_mol_A3_to_atm;
}

/**
 * Convert pressure from kcal/(mol·Å³) to bar
 */
inline double pressure_to_bar(double P_internal) {
    return P_internal * kcal_per_mol_A3_to_bar;
}

/**
 * Radius of gyration: R_g² = Σm_i|r_i - r_COM|² / M_total
 */
inline double radius_of_gyration(const State& s) {
    if (s.M.size() != s.N) return 0.0;
    
    // Compute COM
    Vec3 com = {0, 0, 0};
    double M_total = 0.0;
    for (uint32_t i = 0; i < s.N; ++i) {
        com = com + s.X[i] * s.M[i];
        M_total += s.M[i];
    }
    if (M_total <= 0) return 0.0;
    com = com * (1.0 / M_total);
    
    // Compute R_g²
    double Rg2 = 0.0;
    for (uint32_t i = 0; i < s.N; ++i) {
        Vec3 dr = s.X[i] - com;
        Rg2 += s.M[i] * dot(dr, dr);
    }
    
    return std::sqrt(Rg2 / M_total);
}

/**
 * Momentum (should be conserved in isolated system)
 */
inline Vec3 linear_momentum(const State& s) {
    if (s.V.size() != s.N || s.M.size() != s.N) return {0,0,0};
    
    Vec3 P = {0, 0, 0};
    for (uint32_t i = 0; i < s.N; ++i) {
        P = P + s.V[i] * s.M[i];
    }
    return P;
}

/**
 * Angular momentum about origin
 * L = Σr_i × (m_i·v_i)
 */
inline Vec3 angular_momentum(const State& s) {
    if (s.V.size() != s.N || s.M.size() != s.N) return {0,0,0};
    
    Vec3 L = {0, 0, 0};
    for (uint32_t i = 0; i < s.N; ++i) {
        Vec3 r = s.X[i];
        Vec3 p = s.V[i] * s.M[i];
        // L += r × p
        L.x += r.y*p.z - r.z*p.y;
        L.y += r.z*p.x - r.x*p.z;
        L.z += r.x*p.y - r.y*p.x;
    }
    return L;
}

/**
 * Heat capacity estimator from energy fluctuations
 * C_V ≈ (⟨E²⟩ - ⟨E⟩²)/(k_B·T²)
 * 
 * Requires trajectory of total energies for averaging
 */
inline double heat_capacity_from_fluctuations(const std::vector<double>& E_traj, double T_avg) {
    if (E_traj.size() < 2 || T_avg <= 0) return 0.0;
    
    // Compute mean and variance
    double E_avg = 0.0;
    for (double E : E_traj) E_avg += E;
    E_avg /= E_traj.size();
    
    double E2_avg = 0.0;
    for (double E : E_traj) E2_avg += E*E;
    E2_avg /= E_traj.size();
    
    double var_E = E2_avg - E_avg*E_avg;
    
    return kB + var_E / (kB * T_avg * T_avg);
}

/**
 * Remove center-of-mass motion (set total momentum to zero)
 * Useful for microcanonical simulations to avoid drift
 */
inline void remove_com_motion(State& s) {
    if (s.V.size() != s.N || s.M.size() != s.N) return;
    
    Vec3 P_com = linear_momentum(s);
    double M_total = 0.0;
    for (uint32_t i = 0; i < s.N; ++i) M_total += s.M[i];
    
    if (M_total > 0) {
        Vec3 v_com = P_com * (1.0 / M_total);
        for (uint32_t i = 0; i < s.N; ++i) {
            s.V[i] = s.V[i] - v_com;
        }
    }
}

/**
 * Velocity rescaling to target temperature (Berendsen-like)
 * λ = √[T_target / T_current]
 * v_new = λ · v_old
 * 
 * @param s State (velocities modified in-place)
 * @param T_target Target temperature (K)
 * @param tau Coupling time constant (fs, larger = weaker coupling)
 * @param dt Time step (fs)
 */
inline void rescale_velocities(State& s, double T_target, double tau = 100.0, double dt = 1.0) {
    double T_current = temperature(s);
    if (T_current <= 0) return;
    
    // Berendsen thermostat: λ² = 1 + (dt/tau)(T_target/T_current - 1)
    double lambda2 = 1.0 + (dt/tau) * (T_target/T_current - 1.0);
    double lambda = std::sqrt(std::max(0.0, lambda2));
    
    for (auto& v : s.V) {
        v = v * lambda;
    }
}

/**
 * Initialize velocities from Maxwell-Boltzmann distribution
 * Requires random number generator (std::mt19937)
 * 
 * v_i ~ Normal(0, √(k_B·T/m_i))
 */
template<typename RNG>
inline void initialize_velocities_mb(State& s, double T, RNG& rng) {
    if (s.V.size() != s.N || s.M.size() != s.N) return;
    
    std::normal_distribution<double> normal(0.0, 1.0);
    
    for (uint32_t i = 0; i < s.N; ++i) {
        if (s.M[i] <= 0) continue;
        
        double sigma = std::sqrt(kB * T / s.M[i]);
        s.V[i].x = normal(rng) * sigma;
        s.V[i].y = normal(rng) * sigma;
        s.V[i].z = normal(rng) * sigma;
    }
    
    // Remove COM drift
    remove_com_motion(s);
}

} // namespace thermo
} // namespace atomistic
