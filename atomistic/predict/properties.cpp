#include "properties.hpp"
#include "../core/state.hpp"
#include "pot/periodic_db.hpp"
#include <cmath>
#include <algorithm>

namespace atomistic {
namespace predict {

/**
 * Electronegativity Equilibration (QEq) for partial charges
 * 
 * Based on: Rappe & Goddard (1991). J. Phys. Chem. 95(8), 3358.
 * 
 * Model: E = Σ(χ_i·q_i + J_i·q_i²) + Σ k·q_i·q_j/r_ij
 * Minimize E subject to Σq_i = Q_total
 * 
 * Leads to: χ_i + 2J_i·q_i + Σk·q_j/r_ij = λ (Lagrange multiplier)
 */
ElectronicProperties predict_electronic_properties(const State& s) {
    ElectronicProperties props;
    props.partial_charges.resize(s.N, 0.0);
    
    if (s.N == 0) return props;
    
    // Electronegativity values (Pauling scale, approximate)
    // Would normally load from periodic table
    std::vector<double> chi(s.N);
    std::vector<double> J(s.N);  // Hardness
    
    for (uint32_t i = 0; i < s.N; ++i) {
        // Simplified: assume carbon-like atoms
        // TODO: Use actual periodic table data
        chi[i] = 5.0;   // eV (electronegativity)
        J[i] = 10.0;    // eV (self-energy)
        
        // Adjust based on coordination
        int coordination = 0;
        for (const auto& edge : s.B) {
            if (edge.i == i || edge.j == i) coordination++;
        }
        
        // Higher coordination → more electropositive
        chi[i] -= 0.1 * coordination;
    }
    
    // Iterative charge equilibration
    const int max_iter = 100;
    const double tol = 1e-6;
    
    for (int iter = 0; iter < max_iter; ++iter) {
        std::vector<double> q_new(s.N);
        
        // Compute Lagrange multiplier λ
        double lambda = 0.0;
        for (uint32_t i = 0; i < s.N; ++i) {
            double coulomb_sum = 0.0;
            for (uint32_t j = 0; j < s.N; ++j) {
                if (i == j) continue;
                Vec3 rij = s.X[i] - s.X[j];
                double r = norm(rij);
                if (r > 0.1) {
                    coulomb_sum += 14.4 * props.partial_charges[j] / r;  // k = 14.4 eV·Å
                }
            }
            lambda += (chi[i] + coulomb_sum) / s.N;
        }
        
        // Update charges
        for (uint32_t i = 0; i < s.N; ++i) {
            double coulomb_sum = 0.0;
            for (uint32_t j = 0; j < s.N; ++j) {
                if (i == j) continue;
                Vec3 rij = s.X[i] - s.X[j];
                double r = norm(rij);
                if (r > 0.1) {
                    coulomb_sum += 14.4 * props.partial_charges[j] / r;
                }
            }
            q_new[i] = (lambda - chi[i] - coulomb_sum) / (2.0 * J[i]);
        }
        
        // Normalize to zero total charge
        double q_sum = 0.0;
        for (double q : q_new) q_sum += q;
        for (auto& q : q_new) q -= q_sum / s.N;
        
        // Check convergence
        double max_dq = 0.0;
        for (uint32_t i = 0; i < s.N; ++i) {
            max_dq = std::max(max_dq, std::abs(q_new[i] - props.partial_charges[i]));
        }
        
        props.partial_charges = q_new;
        
        if (max_dq < tol) break;
    }
    
    // Compute dipole moment: μ = Σq_i·r_i
    props.dipole_vector = {0, 0, 0};
    for (uint32_t i = 0; i < s.N; ++i) {
        props.dipole_vector = props.dipole_vector + s.X[i] * props.partial_charges[i];
    }
    
    // Convert to Debye (1 e·Å = 4.8 Debye)
    props.dipole_moment = norm(props.dipole_vector) * 4.8;
    
    // Estimate polarizability (Clausius-Mossotti-like)
    double volume = 0.0;
    for (uint32_t i = 0; i < s.N; ++i) {
        volume += 4.0/3.0 * M_PI * 1.5*1.5*1.5;  // Assume 1.5 Å radius
    }
    props.polarizability = 0.8 * volume;  // Rough estimate
    
    // Estimate IP and EA (Koopmans' theorem approximation)
    double avg_chi = 0.0;
    double avg_J = 0.0;
    for (uint32_t i = 0; i < s.N; ++i) {
        avg_chi += chi[i];
        avg_J += J[i];
    }
    avg_chi /= s.N;
    avg_J /= s.N;
    
    props.ionization_potential = avg_chi + avg_J;
    props.electron_affinity = avg_chi - avg_J;
    props.electronegativity = (props.ionization_potential + props.electron_affinity) / 2.0;
    props.hardness = (props.ionization_potential - props.electron_affinity) / 2.0;
    props.electrophilicity = props.electronegativity * props.electronegativity / (2.0 * props.hardness);
    
    return props;
}

ReactivityIndices predict_reactivity(const State& s, 
                                     const ElectronicProperties& props) {
    ReactivityIndices indices;
    indices.fukui_plus.resize(s.N, 0.0);
    indices.fukui_minus.resize(s.N, 0.0);
    indices.fukui_zero.resize(s.N, 0.0);
    indices.local_softness.resize(s.N, 0.0);
    
    // Simplified: use partial charges as proxy for Fukui functions
    // Real implementation would compute frontier orbital densities
    for (uint32_t i = 0; i < s.N; ++i) {
        // f+ ~ negative charge (sites for nucleophilic attack)
        indices.fukui_plus[i] = std::max(0.0, -props.partial_charges[i]);
        
        // f- ~ positive charge (sites for electrophilic attack)
        indices.fukui_minus[i] = std::max(0.0, props.partial_charges[i]);
        
        // f0 = average
        indices.fukui_zero[i] = (indices.fukui_plus[i] + indices.fukui_minus[i]) / 2.0;
        
        // Local softness s = S·f where S = 1/(2η)
        double global_softness = 1.0 / (2.0 * props.hardness);
        indices.local_softness[i] = global_softness * indices.fukui_zero[i];
    }
    
    return indices;
}

GeometryPrediction predict_geometry_from_vsepr(const State& s) {
    GeometryPrediction pred;
    pred.positions = s.X;  // Start with current geometry
    pred.bond_orders.resize(s.B.size(), 1.0);
    pred.strain_energy = 0.0;
    pred.vsepr_class = "Unknown";
    pred.predicted_barrier = 0.0;
    
    // TODO: Integrate with existing VSEPR engine
    // Would call into src/sim/vsepr_engine.hpp to predict ideal geometry
    
    return pred;
}

double predict_reaction_energy(const State& reactants_A,
                               const State& reactants_B,
                               const State& products_C,
                               const State& products_D) {
    // Bond energy method: ΔE = Σ(bonds broken) - Σ(bonds formed)
    // Typical C-C: 83 kcal/mol, C-H: 99 kcal/mol, C=C: 146 kcal/mol
    
    auto count_bonds = [](const State& s) -> double {
        double E_bonds = 0.0;
        for ([[maybe_unused]] const auto& edge : s.B) {
            // Simplified: all bonds = 85 kcal/mol
            E_bonds += 85.0;
        }
        return E_bonds;
    };
    
    double E_reactants = count_bonds(reactants_A) + count_bonds(reactants_B);
    double E_products = count_bonds(products_C) + count_bonds(products_D);
    
    return E_products - E_reactants;
}

double predict_activation_barrier(const State& reactant,
                                  const State& product,
                                  double intrinsic_barrier) {
    // Bell-Evans-Polanyi: Ea = Ea0 + α·ΔH
    // α ≈ 0.25-0.5 for typical reactions
    
    double delta_E = predict_reaction_energy(reactant, State{}, product, State{});
    double alpha = 0.4;  // Empirical parameter
    
    // Exothermic: lower barrier
    // Endothermic: higher barrier
    double Ea = intrinsic_barrier + alpha * std::max(0.0, delta_E);
    
    return std::max(0.0, Ea);
}

} // namespace predict
} // namespace atomistic
