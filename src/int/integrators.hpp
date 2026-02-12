#pragma once
/**
 * integrators.hpp - MD Integrators
 * 
 * Implementations:
 * - Velocity Verlet (NVE)
 * - Langevin (NVT)
 * - Berendsen (NVT, NPT)
 * - Nosé-Hoover (NVT)
 */

#include <vector>
#include <cmath>
#include <random>

namespace vsepr {

// ============================================================================
// Velocity Verlet (NVE - microcanonical)
// ============================================================================
// Standard symplectic integrator for Hamiltonian dynamics
// Good for: Energy-conserving dynamics, benchmarking, NVE ensemble
// Reference: Swope et al., J. Chem. Phys. 76, 637 (1982)

class VelocityVerlet {
public:
    void step(std::vector<double>& coords,
              std::vector<double>& velocities,
              const std::vector<double>& forces,
              double dt,
              const std::vector<double>& masses) {
        const size_t N = coords.size();
        
        // v(t + dt/2) = v(t) + F(t)/m * dt/2
        for (size_t i = 0; i < N; ++i) {
            velocities[i] += (forces[i] / masses[i]) * dt * 0.5;
        }
        
        // x(t + dt) = x(t) + v(t + dt/2) * dt
        for (size_t i = 0; i < N; ++i) {
            coords[i] += velocities[i] * dt;
        }
        
        // Note: Second half-step needs new forces (computed externally)
    }
    
    void step_second_half(std::vector<double>& velocities,
                         const std::vector<double>& forces,
                         double dt,
                         const std::vector<double>& masses) {
        // v(t + dt) = v(t + dt/2) + F(t + dt)/m * dt/2
        for (size_t i = 0; i < velocities.size(); ++i) {
            velocities[i] += (forces[i] / masses[i]) * dt * 0.5;
        }
    }
};

// ============================================================================
// Langevin Thermostat (NVT - canonical)
// ============================================================================
// Uses stochastic dynamics with friction and random forces
// Good for: Efficient sampling, implicit solvent, variable friction
// Reference: Bussi & Parrinello, Phys. Rev. E 75, 056707 (2007)

class LangevinIntegrator {
public:
    LangevinIntegrator(double temperature, double friction, int seed = 42)
        : T_(temperature), gamma_(friction), rng_(seed) {}
    
    // BAOAB splitting (more accurate than naive Langevin)
    void step(std::vector<double>& coords,
              std::vector<double>& velocities,
              const std::vector<double>& forces,
              double dt,
              const std::vector<double>& masses) {
        const size_t N = coords.size();
        const double kb = 0.001987;  // kcal/mol/K
        const double c1 = std::exp(-gamma_ * dt);
        const double c2 = std::sqrt((1.0 - c1*c1) * kb * T_);
        
        std::normal_distribution<double> normal(0.0, 1.0);
        
        // B: v += F * dt/2 / m
        for (size_t i = 0; i < N; ++i) {
            velocities[i] += forces[i] * (dt * 0.5) / masses[i];
        }
        
        // A: x += v * dt/2
        for (size_t i = 0; i < N; ++i) {
            coords[i] += velocities[i] * dt * 0.5;
        }
        
        // O: Ornstein-Uhlenbeck (stochastic thermostat)
        for (size_t i = 0; i < N; ++i) {
            double sigma = c2 / std::sqrt(masses[i]);
            velocities[i] = c1 * velocities[i] + sigma * normal(rng_);
        }
        
        // A: x += v * dt/2
        for (size_t i = 0; i < N; ++i) {
            coords[i] += velocities[i] * dt * 0.5;
        }
        
        // B step needs new forces (done externally)
    }
    
    void step_final(std::vector<double>& velocities,
                   const std::vector<double>& forces,
                   double dt,
                   const std::vector<double>& masses) {
        // Final B: v += F * dt/2 / m
        for (size_t i = 0; i < velocities.size(); ++i) {
            velocities[i] += forces[i] * (dt * 0.5) / masses[i];
        }
    }
    
    void set_temperature(double T) { T_ = T; }
    void set_friction(double gamma) { gamma_ = gamma; }
    
private:
    double T_;       // Target temperature (K)
    double gamma_;   // Friction coefficient (1/ps)
    std::mt19937 rng_;
};

// ============================================================================
// Berendsen Thermostat (NVT - weak coupling)
// ============================================================================
// Exponentially relaxes temperature to target value
// Good for: Equilibration, gentle temperature control
// Note: Does NOT sample canonical ensemble (use for equilibration only)
// Reference: Berendsen et al., J. Chem. Phys. 81, 3684 (1984)

class BerendsenThermostat {
public:
    BerendsenThermostat(double temperature, double tau)
        : T_target_(temperature), tau_(tau) {}
    
    void apply(std::vector<double>& velocities,
              double dt,
              const std::vector<double>& masses) {
        // Compute current kinetic energy and temperature
        double KE = 0.0;
        double total_mass = 0.0;
        
        for (size_t i = 0; i < velocities.size(); ++i) {
            KE += 0.5 * masses[i] * velocities[i] * velocities[i];
            if (i % 3 == 0) total_mass += masses[i];
        }
        
        const size_t N_atoms = velocities.size() / 3;
        const double kb = 0.001987;  // kcal/mol/K
        const double T_current = (2.0 * KE) / (3.0 * N_atoms * kb);
        
        if (T_current < 1e-6) return;
        
        // Berendsen scaling factor
        double lambda = std::sqrt(1.0 + (dt / tau_) * (T_target_ / T_current - 1.0));
        
        for (auto& v : velocities) {
            v *= lambda;
        }
    }
    
    void set_temperature(double T) { T_target_ = T; }
    void set_coupling_time(double tau) { tau_ = tau; }
    
private:
    double T_target_;  // Target temperature (K)
    double tau_;       // Coupling time constant (ps)
};

// ============================================================================
// Nosé-Hoover Thermostat (NVT - canonical ensemble)
// ============================================================================
// Extended system thermostat that samples canonical ensemble exactly
// Good for: Production runs, proper statistical sampling, NVT ensemble
// Reference: Nosé, J. Chem. Phys. 81, 511 (1984); Hoover, Phys. Rev. A 31, 1695 (1985)

class NoseHooverThermostat {
public:
    NoseHooverThermostat(double temperature, double tau)
        : T_target_(temperature), Q_(0.0), xi_(0.0), v_xi_(0.0) {
        // Q = N_f * kb * T * tau^2 (thermal inertia)
        // Will be set properly when N_dof is known
        tau_ = tau;
    }
    
    void initialize(size_t N_dof) {
        const double kb = 0.001987;  // kcal/mol/K
        Q_ = N_dof * kb * T_target_ * tau_ * tau_;
        xi_ = 0.0;
        v_xi_ = 0.0;
    }
    
    void step(std::vector<double>& velocities,
             double dt,
             const std::vector<double>& masses) {
        if (Q_ < 1e-12) return;  // Not initialized
        
        // Compute current kinetic energy
        double KE = 0.0;
        for (size_t i = 0; i < velocities.size(); ++i) {
            KE += 0.5 * masses[i] * velocities[i] * velocities[i];
        }
        
        const size_t N_dof = velocities.size();
        const double kb = 0.001987;  // kcal/mol/K
        
        // Update thermostat variable (velocity Verlet style)
        // dv_xi/dt = (2*KE - N_f*kb*T) / Q
        double force_xi = (2.0 * KE - N_dof * kb * T_target_) / Q_;
        
        v_xi_ += force_xi * dt * 0.5;
        xi_ += v_xi_ * dt;
        
        // Scale velocities
        double alpha = std::exp(-v_xi_ * dt);
        for (auto& v : velocities) {
            v *= alpha;
        }
        
        // Recompute KE
        KE = 0.0;
        for (size_t i = 0; i < velocities.size(); ++i) {
            KE += 0.5 * masses[i] * velocities[i] * velocities[i];
        }
        
        // Second half-step for v_xi
        force_xi = (2.0 * KE - N_dof * kb * T_target_) / Q_;
        v_xi_ += force_xi * dt * 0.5;
    }
    
    void set_temperature(double T) {
        T_target_ = T;
        // Rescale Q if needed
        if (Q_ > 1e-12) {
            const double kb = 0.001987;
            size_t N_dof = static_cast<size_t>(Q_ / (kb * T_target_ * tau_ * tau_) + 0.5);
            Q_ = N_dof * kb * T * tau_ * tau_;
        }
    }
    
    double get_xi() const { return xi_; }
    double get_conserved_quantity(double KE_system) const {
        // H_extended = H_system + (1/2)*Q*v_xi^2 + N_f*kb*T*xi
        const double kb = 0.001987;
        size_t N_dof = static_cast<size_t>(Q_ / (kb * T_target_ * tau_ * tau_) + 0.5);
        return 0.5 * Q_ * v_xi_ * v_xi_ + N_dof * kb * T_target_ * xi_;
    }
    
private:
    double T_target_;  // Target temperature (K)
    double tau_;       // Coupling time (ps)
    double Q_;         // Thermal inertia
    double xi_;        // Thermostat position
    double v_xi_;      // Thermostat velocity
};

// ============================================================================
// Velocity Rescaling Thermostat (Canonical sampling)
// ============================================================================
// Stochastic velocity rescaling that preserves canonical distribution
// Good for: Fast equilibration with correct ensemble
// Reference: Bussi et al., J. Chem. Phys. 126, 014101 (2007)

class VelocityRescalingThermostat {
public:
    VelocityRescalingThermostat(double temperature, double tau, int seed = 42)
        : T_target_(temperature), tau_(tau), rng_(seed) {}
    
    void apply(std::vector<double>& velocities,
              double dt,
              const std::vector<double>& masses) {
        // Compute current kinetic energy
        double KE = 0.0;
        for (size_t i = 0; i < velocities.size(); ++i) {
            KE += 0.5 * masses[i] * velocities[i] * velocities[i];
        }
        
        const size_t N_dof = velocities.size();
        const double kb = 0.001987;  // kcal/mol/K
        const double KE_target = 0.5 * N_dof * kb * T_target_;
        
        if (KE < 1e-12) return;
        
        // Stochastic scaling factor (Bussi algorithm)
        std::normal_distribution<double> normal(0.0, 1.0);
        std::gamma_distribution<double> gamma((N_dof - 1.0) / 2.0, 1.0);
        
        double R1 = normal(rng_);
        double R2 = 2.0 * gamma(rng_);
        
        double c = std::exp(-dt / tau_);
        double KE_new = KE * c + KE_target * (1.0 - c) * (R2 + R1 * R1) / N_dof
                       + 2.0 * std::sqrt(KE * KE_target * (1.0 - c) * c / N_dof) * R1;
        
        double alpha = std::sqrt(KE_new / KE);
        
        for (auto& v : velocities) {
            v *= alpha;
        }
    }
    
    void set_temperature(double T) { T_target_ = T; }
    
private:
    double T_target_;  // Target temperature (K)
    double tau_;       // Coupling time (ps)
    std::mt19937 rng_;
};

} // namespace vsepr
