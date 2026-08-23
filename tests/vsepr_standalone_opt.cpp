// Standalone VSEPR-only optimizer
// Tests virtual site representation with normalization constraint

#include "sim/molecule.hpp"
#include "pot/energy_vsepr.hpp"
#include "sim/optimizer.hpp"
#include <iostream>
#include <iomanip>
#include <vector>
#include <cmath>

using namespace vsepr;

// Helper: compute angle from coordinates
double angle_from_coords(const std::vector<double>& c, size_t a, size_t b, size_t cen) {
    double ax = c[3*a], ay = c[3*a+1], az = c[3*a+2];
    double bx = c[3*b], by = c[3*b+1], bz = c[3*b+2];
    double cx = c[3*cen], cy = c[3*cen+1], cz = c[3*cen+2];
    
    double vax = ax - cx, vay = ay - cy, vaz = az - cz;
    double vbx = bx - cx, vby = by - cy, vbz = bz - cz;
    
    double dot = vax*vbx + vay*vby + vaz*vbz;
    double na = std::sqrt(vax*vax + vay*vay + vaz*vaz);
    double nb = std::sqrt(vbx*vbx + vby*vby + vbz*vbz);
    
    double cos_theta = dot / (na * nb);
    cos_theta = std::clamp(cos_theta, -1.0, 1.0);
    return std::acos(cos_theta) * 180.0 / M_PI;
}

// VSEPR-only energy evaluator (no bonds, angles, torsions)
class VSEPROnlyEnergy {
public:
    VSEPROnlyEnergy(const VSEPREnergy& vsepr) : vsepr_(vsepr) {}
    
    double evaluate(const std::vector<double>& coords, std::vector<double>& gradient) {
        return vsepr_.evaluate(coords, gradient);
    }
    
    void normalize_constraints(std::vector<double>& coords) {
        vsepr_.normalize_lone_pairs(coords);
    }
    
private:
    const VSEPREnergy& vsepr_;
};

// Custom FIRE optimizer with post-step normalization
struct FIREOptimizerWithConstraints {
    static OptimizeResult optimize(VSEPROnlyEnergy& energy,
                                   std::vector<double>& coords,
                                   int max_iter = 500,
                                   double f_tol = 0.01,
                                   double dt_start = 0.1,
                                   double dt_max = 1.0,
                                   double alpha_start = 0.1,
                                   double f_dec = 0.5,
                                   double f_inc = 1.1,
                                   double alpha_decay = 0.99)
    {
        int N = coords.size();
        std::vector<double> velocity(N, 0.0);
        std::vector<double> grad(N);
        
        double dt = dt_start;
        double alpha = alpha_start;
        int N_pos = 0;
        
        for (int iter = 0; iter < max_iter; ++iter) {
            double E = energy.evaluate(coords, grad);
            
            // Compute forces and metrics
            double f_rms = 0.0, f_max = 0.0;
            for (int i = 0; i < N; ++i) {
                double f = -grad[i];
                f_rms += f * f;
                f_max = std::max(f_max, std::abs(f));
            }
            f_rms = std::sqrt(f_rms / N);
            
            // Check convergence
            bool converged = (f_rms < f_tol && f_max < 10.0 * f_tol);
            if (iter % 10 == 0 || converged) {
                std::cout << "Iter " << std::setw(4) << iter
                          << "  E = " << std::setw(12) << std::fixed << std::setprecision(4) << E
                          << "  F_rms = " << std::setw(10) << std::scientific << std::setprecision(3) << f_rms
                          << "  F_max = " << std::setw(10) << f_max << "\n";
            }
            
            if (converged) {
                OptimizeResult res;
                res.converged = true;
                res.iterations = iter;
                res.energy = E;
                res.rms_force = f_rms;
                res.max_force = f_max;
                return res;
            }
            
            // Compute power P = F · v
            double P = 0.0;
            for (int i = 0; i < N; ++i) {
                P += (-grad[i]) * velocity[i];
            }
            
            if (P > 0) {
                // Acceleration phase
                N_pos++;
                if (N_pos > 5) {
                    dt = std::min(dt * f_inc, dt_max);
                    alpha *= alpha_decay;
                }
                
                // Mix velocity
                double v_norm = 0.0, f_norm = 0.0;
                for (int i = 0; i < N; ++i) {
                    v_norm += velocity[i] * velocity[i];
                    f_norm += grad[i] * grad[i];
                }
                v_norm = std::sqrt(v_norm);
                f_norm = std::sqrt(f_norm);
                
                if (f_norm > 1e-12) {
                    for (int i = 0; i < N; ++i) {
                        velocity[i] = (1.0 - alpha) * velocity[i] - alpha * v_norm * grad[i] / f_norm;
                    }
                }
            } else {
                // Deceleration: reset
                N_pos = 0;
                dt *= f_dec;
                alpha = alpha_start;
                std::fill(velocity.begin(), velocity.end(), 0.0);
            }
            
            // Update velocity and position
            for (int i = 0; i < N; ++i) {
                velocity[i] += -grad[i] * dt;
                coords[i] += velocity[i] * dt;
            }
            
            // **CRITICAL**: Normalize lone pair directions after each step
            energy.normalize_constraints(coords);
        }
        
        // Final energy evaluation
        double E_final = energy.evaluate(coords, grad);
        double f_rms = 0.0, f_max = 0.0;
        for (int i = 0; i < N; ++i) {
            double f = -grad[i];
            f_rms += f * f;
            f_max = std::max(f_max, std::abs(f));
        }
        f_rms = std::sqrt(f_rms / N);
        
        OptimizeResult res;
        res.converged = false;
        res.iterations = max_iter;
        res.energy = E_final;
        res.rms_force = f_rms;
        res.max_force = f_max;
        return res;
    }
};

void test_water_opt() {
    std::cout << "\n=== H2O Standalone VSEPR Optimization ===\n";
    
    Molecule mol;
    mol.add_atom(8, 0.0, 0.0, 0.0);      // O
    mol.add_atom(1, 1.0, 0.0, 0.0);      // H1
    mol.add_atom(1, 0.0, 1.0, 0.0);      // H2
    mol.atoms[0].lone_pairs = 2;         // O has 2 lone pairs
    
    mol.add_bond(0, 1, 1);
    mol.add_bond(0, 2, 1);
    
    // Create VSEPR energy
    VSEPREnergy vsepr_energy(mol.atoms, mol.bonds);
    
    // Initialize extended coordinates
    std::vector<double> coords = mol.coords;
    vsepr_energy.initialize_lone_pair_directions(coords);
    
    std::cout << "Extended coords: " << coords.size() << " (atoms: 9, LPs: 6)\n";
    
    double angle_init = angle_from_coords(coords, 1, 2, 0);
    std::cout << "Initial H-O-H angle: " << angle_init << "°\n\n";
    
    // Optimize
    VSEPROnlyEnergy energy_wrapper(vsepr_energy);
    auto result = FIREOptimizerWithConstraints::optimize(energy_wrapper, coords, 200);
    
    std::cout << "\nConverged: " << (result.converged ? "YES" : "NO") << "\n";
    std::cout << "Iterations: " << result.iterations << "\n";
    std::cout << "Final energy: " << result.energy << " kcal/mol\n";
    
    // Extract final geometry
    double angle_final = angle_from_coords(coords, 1, 2, 0);
    std::cout << "Final H-O-H angle: " << angle_final << "°\n";
    std::cout << "Expected: ~104° (experimental)\n";
}

void test_ammonia_opt() {
    std::cout << "\n=== NH3 Standalone VSEPR Optimization ===\n";
    
    Molecule mol;
    mol.add_atom(7, 0.0, 0.0, 0.0);      // N
    mol.add_atom(1, 1.0, 0.0, 0.0);      // H1
    mol.add_atom(1, -0.5, 0.866, 0.0);   // H2
    mol.add_atom(1, -0.5, -0.866, 0.0);  // H3
    mol.atoms[0].lone_pairs = 1;         // N has 1 lone pair
    
    mol.add_bond(0, 1, 1);
    mol.add_bond(0, 2, 1);
    mol.add_bond(0, 3, 1);
    
    VSEPREnergy vsepr_energy(mol.atoms, mol.bonds);
    
    std::vector<double> coords = mol.coords;
    vsepr_energy.initialize_lone_pair_directions(coords);
    
    std::cout << "Extended coords: " << coords.size() << " (atoms: 12, LPs: 3)\n";
    
    double angle_init = angle_from_coords(coords, 1, 2, 0);
    std::cout << "Initial H-N-H angle: " << angle_init << "°\n\n";
    
    VSEPROnlyEnergy energy_wrapper(vsepr_energy);
    auto result = FIREOptimizerWithConstraints::optimize(energy_wrapper, coords, 200);
    
    std::cout << "\nConverged: " << (result.converged ? "YES" : "NO") << "\n";
    std::cout << "Iterations: " << result.iterations << "\n";
    std::cout << "Final energy: " << result.energy << " kcal/mol\n";
    
    double angle_final = angle_from_coords(coords, 1, 2, 0);
    std::cout << "Final H-N-H angle: " << angle_final << "°\n";
    std::cout << "Expected: ~107° (experimental)\n";
}

void test_methane_opt() {
    std::cout << "\n=== CH4 Standalone VSEPR Optimization ===\n";
    
    Molecule mol;
    mol.add_atom(6, 0.0, 0.0, 0.0);      // C
    mol.add_atom(1, 1.0, 0.0, 0.0);      // H1
    mol.add_atom(1, 0.0, 1.0, 0.0);      // H2
    mol.add_atom(1, 0.0, 0.0, 1.0);      // H3
    mol.add_atom(1, -1.0, 0.0, 0.0);     // H4
    mol.atoms[0].lone_pairs = 0;         // C has no lone pairs
    
    mol.add_bond(0, 1, 1);
    mol.add_bond(0, 2, 1);
    mol.add_bond(0, 3, 1);
    mol.add_bond(0, 4, 1);
    
    VSEPREnergy vsepr_energy(mol.atoms, mol.bonds);
    
    std::vector<double> coords = mol.coords;
    vsepr_energy.initialize_lone_pair_directions(coords);
    
    std::cout << "Extended coords: " << coords.size() << " (atoms: 15, LPs: 0)\n";
    
    double angle_init = angle_from_coords(coords, 1, 2, 0);
    std::cout << "Initial H-C-H angle: " << angle_init << "°\n\n";
    
    VSEPROnlyEnergy energy_wrapper(vsepr_energy);
    auto result = FIREOptimizerWithConstraints::optimize(energy_wrapper, coords, 200);
    
    std::cout << "\nConverged: " << (result.converged ? "YES" : "NO") << "\n";
    std::cout << "Iterations: " << result.iterations << "\n";
    std::cout << "Final energy: " << result.energy << " kcal/mol\n";
    
    double angle_final = angle_from_coords(coords, 1, 2, 0);
    std::cout << "Final H-C-H angle: " << angle_final << "°\n";
    std::cout << "Expected: ~109.5° (tetrahedral)\n";
}

int main() {
    std::cout << "===================================================\n";
    std::cout << "Standalone VSEPR-Only Optimizer\n";
    std::cout << "Virtual sites + normalization constraint\n";
    std::cout << "===================================================\n";
    
    test_water_opt();
    test_ammonia_opt();
    test_methane_opt();
    
    std::cout << "\n===================================================\n";
    std::cout << "All standalone VSEPR optimizations complete!\n";
    std::cout << "===================================================\n";
    
    return 0;
}
