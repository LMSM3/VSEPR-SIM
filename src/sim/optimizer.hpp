#pragma once
/*
optimizer.hpp
-------------
Geometry optimization using FIRE algorithm.

FIRE (Fast Inertial Relaxation Engine):
- Velocity Verlet-like dynamics with adaptive damping
- Increases timestep when energy decreases (power > 0)
- Resets velocity when going uphill (power < 0)
- Deterministic, stable, no line search required

References:
Bitzek et al., PRL 97, 170201 (2006)
*/

#include "pot/energy_model.hpp"
#include "core/geom_ops.hpp"
#include <vector>
#include <string>
#include <cmath>
#include <limits>
#include <algorithm>
#include <iostream>

namespace vsepr {

// ============================================================================
// Optimization Result
// ============================================================================
struct OptimizeResult {
    std::vector<double> coords;     // Final coordinates
    double energy = 0.0;            // Final energy
    double rms_force = 0.0;         // RMS force magnitude
    double max_force = 0.0;         // Maximum force component
    int iterations = 0;             // Number of iterations performed
    std::string termination_reason; // Why optimization stopped
    
    // Component breakdown (optional)
    EnergyResult energy_breakdown;
    
    bool converged = false;         // Did we meet convergence criteria?
};

// ============================================================================
// Optimizer Settings
// ============================================================================
struct OptimizerSettings {
    // Convergence criteria
    double tol_rms_force = 1e-4;    // RMS force tolerance (kcal/mol/Å) - tighter
    double tol_max_force = 1e-3;    // Max force component tolerance
    int max_iterations = 5000;      // Maximum optimization steps - increased
    
    // FIRE parameters (softer for complex molecules)
    double dt_init = 0.05;          // Initial timestep (fs-like units) - softer
    double dt_max = 0.5;            // Maximum timestep - reduced for stability
    double dt_min = 1e-6;           // Minimum timestep (termination criterion)
    double alpha_init = 0.1;        // Initial damping coefficient
    double f_alpha = 0.99;          // Alpha decay factor
    double f_inc = 1.1;             // Timestep increase factor
    double f_dec = 0.5;             // Timestep decrease factor
    int N_min = 5;                  // Min steps before increasing dt
    
    // Safety limits
    double max_step = 0.2;          // Max displacement per atom per step (Å)
    bool clamp_forces = false;      // Enable force clamping (use if unstable)
    double max_force_clamp = 100.0; // Force clamp value (kcal/mol/Å)
    
    // Gradient checking (dev mode)
    bool check_gradients = false;   // Verify analytical gradients
    double grad_check_tol = 1e-5;   // Gradient check tolerance
    double grad_check_h = 1e-6;     // Finite difference step
    
    // Verbosity
    int print_every = 0;            // Print status every N steps (0 = silent)
};

// ============================================================================
// FIRE Optimizer
// ============================================================================
class FIREOptimizer {
public:
    explicit FIREOptimizer(const OptimizerSettings& settings = OptimizerSettings())
        : settings_(settings) {}
    
    // Main optimization entry point
    OptimizeResult minimize(const std::vector<double>& initial_coords,
                           const EnergyModel& model);

private:
    // Compute RMS and max force
    void compute_force_metrics(const std::vector<double>& forces,
                               double& rms, double& max_val) const;
    
    // Check for NaN/Inf in arrays
    bool has_invalid_values(const std::vector<double>& arr) const;
    
    // Clamp displacement to max_step
    void clamp_displacement(std::vector<double>& displacement) const;
    
    // Gradient check using finite differences
    bool verify_gradients(const std::vector<double>& coords,
                         const std::vector<double>& grad_analytic,
                         const EnergyModel& model) const;
    
    OptimizerSettings settings_;
};

// ============================================================================
// Implementation
// ============================================================================

inline OptimizeResult FIREOptimizer::minimize(
    const std::vector<double>& initial_coords,
    const EnergyModel& model)
{
    OptimizeResult result;
    result.coords = initial_coords;
    
    const size_t N = result.coords.size();
    
    // Validate input
    if (!model.validate_coords(result.coords)) {
        result.termination_reason = "Invalid coordinate array size";
        return result;
    }
    
    // Initialize FIRE state
    std::vector<double> velocity(N, 0.0);
    std::vector<double> gradient(N);
    std::vector<double> forces(N);
    
    double dt = settings_.dt_init;
    double alpha = settings_.alpha_init;
    int N_positive = 0;  // Steps with positive power
    
    // Initial energy and gradient
    result.energy = model.evaluate_energy_gradient(result.coords, gradient);
    
    // Convert gradient to forces (F = -grad)
    for (size_t i = 0; i < N; ++i) {
        forces[i] = -gradient[i];
    }
    
    // Optional: gradient check on first iteration
    if (settings_.check_gradients) {
        if (!verify_gradients(result.coords, gradient, model)) {
            result.termination_reason = "Gradient check failed";
            return result;
        }
    }
    
    compute_force_metrics(forces, result.rms_force, result.max_force);
    
    if (settings_.print_every > 0) {
        std::cout << "FIRE: Initial E=" << result.energy 
                  << " rmsF=" << result.rms_force 
                  << " maxF=" << result.max_force << "\n";
    }
    
    // Main optimization loop
    for (int iter = 0; iter < settings_.max_iterations; ++iter) {
        result.iterations = iter + 1;
        
        // Check convergence
        if (result.rms_force < settings_.tol_rms_force && 
            result.max_force < settings_.tol_max_force) {
            result.converged = true;
            result.termination_reason = "Converged: force tolerances met";
            break;
        }
        
        // Safety: check for NaN/Inf
        if (has_invalid_values(result.coords) || 
            has_invalid_values(forces) ||
            !std::isfinite(result.energy)) {
            result.termination_reason = "NaN/Inf detected";
            break;
        }
        
        // Safety: timestep too small
        if (dt < settings_.dt_min) {
            result.termination_reason = "Timestep below minimum";
            break;
        }
        
        // Optional force clamping
        if (settings_.clamp_forces) {
            for (size_t i = 0; i < N; ++i) {
                forces[i] = std::clamp(forces[i], 
                                      -settings_.max_force_clamp,
                                       settings_.max_force_clamp);
            }
        }
        
        // Compute power: P = F · v
        double power = 0.0;
        for (size_t i = 0; i < N; ++i) {
            power += forces[i] * velocity[i];
        }
        
        // FIRE velocity mixing: v = (1 - alpha)*v + alpha*|v|*F/|F|
        double v_norm = 0.0, f_norm = 0.0;
        for (size_t i = 0; i < N; ++i) {
            v_norm += velocity[i] * velocity[i];
            f_norm += forces[i] * forces[i];
        }
        v_norm = std::sqrt(v_norm);
        f_norm = std::sqrt(f_norm);
        
        if (f_norm > 1e-12) {
            double scale = alpha * v_norm / f_norm;
            for (size_t i = 0; i < N; ++i) {
                velocity[i] = (1.0 - alpha) * velocity[i] + scale * forces[i];
            }
        }
        
        // FIRE adaptive timestep and damping
        if (power > 0.0) {
            N_positive++;
            if (N_positive > settings_.N_min) {
                dt = std::min(dt * settings_.f_inc, settings_.dt_max);
                alpha *= settings_.f_alpha;
            }
        } else {
            // Going uphill: reset velocity, reduce timestep
            N_positive = 0;
            std::fill(velocity.begin(), velocity.end(), 0.0);
            dt *= settings_.f_dec;
            alpha = settings_.alpha_init;
        }
        
        // Velocity Verlet step 1: v(t + dt/2) = v(t) + F(t) * dt/2
        // (Note: using unit mass, so a = F)
        for (size_t i = 0; i < N; ++i) {
            velocity[i] += forces[i] * dt * 0.5;
        }
        
        // Update positions: x(t + dt) = x(t) + v(t + dt/2) * dt
        std::vector<double> displacement(N);
        for (size_t i = 0; i < N; ++i) {
            displacement[i] = velocity[i] * dt;
        }
        
        // Clamp displacement for safety
        clamp_displacement(displacement);
        
        for (size_t i = 0; i < N; ++i) {
            result.coords[i] += displacement[i];
        }
        
        // Evaluate new energy and forces
        result.energy = model.evaluate_energy_gradient(result.coords, gradient);
        for (size_t i = 0; i < N; ++i) {
            forces[i] = -gradient[i];
        }
        
        // Velocity Verlet step 2: v(t + dt) = v(t + dt/2) + F(t + dt) * dt/2
        for (size_t i = 0; i < N; ++i) {
            velocity[i] += forces[i] * dt * 0.5;
        }
        
        compute_force_metrics(forces, result.rms_force, result.max_force);
        
        // Print progress
        if (settings_.print_every > 0 && (iter + 1) % settings_.print_every == 0) {
            std::cout << "FIRE iter " << (iter + 1) 
                      << ": E=" << result.energy
                      << " rmsF=" << result.rms_force
                      << " maxF=" << result.max_force
                      << " dt=" << dt << "\n";
        }
    }
    
    // Final energy breakdown
    result.energy_breakdown = model.evaluate_detailed(result.coords);
    
    if (result.termination_reason.empty()) {
        result.termination_reason = "Maximum iterations reached";
    }
    
    return result;
}

inline void FIREOptimizer::compute_force_metrics(
    const std::vector<double>& forces,
    double& rms, double& max_val) const
{
    rms = 0.0;
    max_val = 0.0;
    
    for (double f : forces) {
        double f_abs = std::abs(f);
        rms += f * f;
        max_val = std::max(max_val, f_abs);
    }
    
    rms = std::sqrt(rms / forces.size());
}

inline bool FIREOptimizer::has_invalid_values(const std::vector<double>& arr) const {
    for (double v : arr) {
        if (!std::isfinite(v)) return true;
    }
    return false;
}

inline void FIREOptimizer::clamp_displacement(std::vector<double>& displacement) const {
    const size_t N_atoms = displacement.size() / 3;
    
    for (size_t i = 0; i < N_atoms; ++i) {
        double dx = displacement[3*i + 0];
        double dy = displacement[3*i + 1];
        double dz = displacement[3*i + 2];
        
        double d = std::sqrt(dx*dx + dy*dy + dz*dz);
        
        if (d > settings_.max_step) {
            double scale = settings_.max_step / d;
            displacement[3*i + 0] *= scale;
            displacement[3*i + 1] *= scale;
            displacement[3*i + 2] *= scale;
        }
    }
}

inline bool FIREOptimizer::verify_gradients(
    const std::vector<double>& coords,
    const std::vector<double>& grad_analytic,
    const EnergyModel& model) const
{
    const double h = settings_.grad_check_h;
    std::vector<double> coords_perturbed = coords;
    
    double max_error = 0.0;
    size_t max_error_idx = 0;
    
    for (size_t i = 0; i < coords.size(); ++i) {
        // Forward
        coords_perturbed[i] += h;
        double E_plus = model.evaluate_energy(coords_perturbed);
        
        // Backward
        coords_perturbed[i] = coords[i] - h;
        double E_minus = model.evaluate_energy(coords_perturbed);
        
        // Restore
        coords_perturbed[i] = coords[i];
        
        // Central difference
        double grad_numeric = (E_plus - E_minus) / (2.0 * h);
        double error = std::abs(grad_analytic[i] - grad_numeric);
        
        if (error > max_error) {
            max_error = error;
            max_error_idx = i;
        }
    }
    
    std::cout << "Gradient check: max error = " << max_error 
              << " at index " << max_error_idx << "\n";
    std::cout << "  Analytic: " << grad_analytic[max_error_idx] << "\n";
    std::cout << "  Numeric:  " << (model.evaluate_energy(coords) /* recompute numeric */) << "\n";
    
    return max_error < settings_.grad_check_tol;
}

} // namespace vsepr
