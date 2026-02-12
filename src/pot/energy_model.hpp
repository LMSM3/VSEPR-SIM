#pragma once
/*
energy_model.hpp
----------------
Molecular energy model - aggregates all energy terms.

Evaluates energy and gradients for a complete molecular system.
Terms are evaluated in fixed order for deterministic results.
*/

#include "pot/energy.hpp"
#include "pot/energy_bond.hpp"
#include "pot/energy_angle.hpp"
#include "pot/energy_nonbonded.hpp"
#include "pot/energy_torsion.hpp"
#include "pot/energy_vsepr.hpp"
#include "sim/molecule.hpp"
#include <memory>
#include <vector>

namespace vsepr {

class EnergyModel {
public:
    // Construct from molecule topology
    // V0.3 policy: bonds + nonbonded + domains, angles weak/off
    explicit EnergyModel(const Molecule& mol, 
                        double bond_k = 300.0,
                        bool use_angles = false,           // Changed: OFF by default
                        bool use_nonbonded = true,         // Changed: ON by default (with exclusions)
                        const NonbondedParams& nb_params = NonbondedParams(),
                        bool use_torsions = false,
                        bool use_vsepr_domains = false,    // New: explicit domain repulsion
                        double angle_scale = 0.1)          // New: weak angles (k *= 0.1)
        : molecule_(mol), nb_params_(nb_params), angle_scale_(angle_scale) {
        
        // Assign bond parameters
        bond_params_ = assign_bond_parameters(
            mol.bonds, mol.atoms, bond_k);
        
        // Create bond energy evaluator
        if (!mol.bonds.empty()) {
            bond_energy_ = std::make_unique<BondEnergy>(
                mol.bonds, bond_params_);
        }
        
        // Assign angle parameters (if requested)
        if (use_angles && !mol.angles.empty()) {
            angle_params_ = assign_angle_parameters(
                mol.angles, mol.atoms, mol.bonds, mol.coords);
            
            // Scale down angle force constants (avoid term fighting with domains)
            for (auto& p : angle_params_) {
                p.k *= angle_scale_;  // Weak stabilizer only
            }
            
            angle_energy_ = std::make_unique<AngleEnergy>(
                mol.angles, angle_params_);
        }
        
        // VSEPR domain repulsion (if requested)
        if (use_vsepr_domains) {
            vsepr_energy_ = std::make_unique<VSEPREnergy>(
                mol.atoms, mol.bonds, VSEPRParams());
        }
        
        // Build nonbonded pairs (if requested)
        if (use_nonbonded) {
            nonbonded_pairs_ = build_nonbonded_pairs(
                mol.atoms.size(),
                mol.bonds,
                nb_params_.scale_13,
                nb_params_.scale_14
            );
            
            nonbonded_energy_ = std::make_unique<NonbondedEnergy>(
                nonbonded_pairs_, mol.atoms, nb_params_);
        }
        
        // Assign torsion parameters (if requested)
        if (use_torsions && !mol.torsions.empty()) {
            torsion_params_ = assign_torsion_parameters(
                mol.torsions, mol.atoms, mol.bonds);
            
            torsion_energy_ = std::make_unique<TorsionEnergy>(
                mol.torsions, torsion_params_);
        }
    }

    // Evaluate total energy
    double evaluate_energy(const std::vector<double>& coords) const {
        EnergyContext ctx;
        ctx.coords = &coords;
        ctx.gradient = nullptr;
        
        EnergyResult result = evaluate_impl(ctx);
        return result.total_energy;
    }

    // Evaluate energy and gradient
    double evaluate_energy_gradient(const std::vector<double>& coords,
                                     std::vector<double>& gradient) const {
        // Initialize gradient to zero
        gradient.assign(coords.size(), 0.0);
        
        EnergyContext ctx;
        ctx.coords = &coords;
        ctx.gradient = &gradient;
        
        EnergyResult result = evaluate_impl(ctx);
        return result.total_energy;
    }

    // Evaluate with component breakdown
    EnergyResult evaluate_detailed(const std::vector<double>& coords) const {
        EnergyContext ctx;
        ctx.coords = &coords;
        ctx.gradient = nullptr;
        
        return evaluate_impl(ctx);
    }

    // Validate coordinate array size
    bool validate_coords(const std::vector<double>& coords) const {
        return coords.size() == 3 * molecule_.num_atoms();
    }

private:
    EnergyResult evaluate_impl(EnergyContext& ctx) const {
        EnergyResult result;

        // Bond stretching
        if (bond_energy_) {
            result.bond_energy = bond_energy_->evaluate(ctx);
            result.total_energy += result.bond_energy;
        }

        // Angle bending
        if (angle_energy_) {
            result.angle_energy = angle_energy_->evaluate(ctx);
            result.total_energy += result.angle_energy;
        }

        // Nonbonded (van der Waals) - with exclusions for 1-2, 1-3
        if (nonbonded_energy_) {
            result.nonbonded_energy = nonbonded_energy_->evaluate(ctx);
            result.total_energy += result.nonbonded_energy;
        }

        // VSEPR domain repulsion - geometry driver
        if (vsepr_energy_) {
            // Note: VSEPR uses extended coordinates (atoms + LP directions)
            // For now, only works if coords match atom count * 3
            // TODO: Handle extended coords properly
            std::vector<double> grad_dummy;
            if (ctx.coords->size() == 3 * molecule_.num_atoms()) {
                // Standard atom-only coordinates - skip VSEPR for now
                // (requires extended coordinate integration)
            }
        }

        // Torsions
        if (torsion_energy_) {
            result.torsion_energy = torsion_energy_->evaluate(ctx);
            result.total_energy += result.torsion_energy;
        }

        return result;
    }

    const Molecule& molecule_;
    std::vector<BondParams> bond_params_;
    std::vector<AngleParams> angle_params_;
    std::vector<NonbondedPair> nonbonded_pairs_;
    std::vector<TorsionParams> torsion_params_;
    NonbondedParams nb_params_;
    double angle_scale_;  // Scaling factor for angle force constants
    std::unique_ptr<BondEnergy> bond_energy_;
    std::unique_ptr<AngleEnergy> angle_energy_;
    std::unique_ptr<NonbondedEnergy> nonbonded_energy_;
    std::unique_ptr<TorsionEnergy> torsion_energy_;
    std::unique_ptr<VSEPREnergy> vsepr_energy_;  // Explicit domain repulsion
};

} // namespace vsepr
