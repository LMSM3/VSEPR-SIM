#pragma once
/**
 * coupled_solver.hpp
 *
 * Electrohydrodynamic Simulation — Stage 3: Physics
 *
 * Coupled multiphysics solver orchestrating the three sub-models:
 *
 *   1. Flow (Navier-Stokes + electric body force)
 *   2. Electrostatics (Poisson with space-charge from ions)
 *   3. Ion transport (Nernst-Planck: diffusion + migration + convection)
 *
 * Coupling order (segregated, outer-iteration):
 *   (a) Solve flow  →  u, p
 *   (b) Solve electric  →  φ, E   (with ρ_e from previous species solution)
 *   (c) Solve ion transport  →  c_i, N_i   (with u from flow, E from electric)
 *   (d) Update ρ_e = F Σ z_i c_i
 *   (e) Repeat until convergence
 *
 * This module defines the run-card, the solver state, and the outer loop.
 */

#include "sim/ehd/ehd_types.hpp"
#include "sim/ehd/physics/flow_model.hpp"
#include "sim/ehd/physics/electrostatic_model.hpp"
#include "sim/ehd/physics/ion_transport_model.hpp"
#include <string>
#include <vector>
#include <cmath>
#include <iostream>
#include <functional>

namespace vsepr {
namespace ehd {
namespace physics {

// ============================================================================
// Run Card
// ============================================================================

struct RunCard {
    std::string case_id = "EHD-001";

    // Grid resolution
    int nr = 40;
    int nz = 200;

    // Outer coupling iterations
    int    max_outer_iterations = 50;
    double convergence_tol      = 1e-6;

    // Time stepping (for transient, 0 = steady)
    double dt = 0.0;
    int    max_time_steps = 0;

    // Under-relaxation factors
    double relax_flow     = 0.7;
    double relax_electric = 0.9;
    double relax_species  = 0.5;
};

// ============================================================================
// Solver Output Metrics
// ============================================================================

struct SolverMetrics {
    double delta_P       = 0.0;   // pressure drop (Pa)
    double E_max         = 0.0;   // max electric field (V/m)
    double E_avg         = 0.0;   // volume-averaged field (V/m)
    double Re            = 0.0;   // Reynolds number
    double u_max         = 0.0;   // max velocity magnitude (m/s)

    std::vector<double> outlet_flux;        // molar flux per species (mol/s)
    std::vector<double> accumulation_idx;   // accumulation index per species

    int    iterations_used = 0;
    double residual_final  = 0.0;
    bool   converged       = false;

    /**
     * Compute enhancement factor η between this case and a baseline.
     * η = metric_modulated / metric_baseline
     */
    static double enhancement(double metric_mod, double metric_base) {
        if (std::abs(metric_base) < 1e-30) return 0.0;
        return metric_mod / metric_base;
    }
};

// ============================================================================
// CoupledSolver
// ============================================================================

class CoupledSolver {
public:
    CoupledSolver(const EHDParameters& params, const RunCard& card)
        : params_(params), card_(card),
          flow_(params), electro_(params), transport_(params) {}

    /**
     * Initialize all fields to analytical baselines.
     */
    void initialize() {
        flow_field_     = flow_.initialize_poiseuille(card_.nr, card_.nz);
        electric_field_ = electro_.initialize_coaxial(card_.nr, card_.nz);
        species_fields_ = transport_.initialize_uniform(card_.nr, card_.nz);

        std::cout << "[EHD] Initialized — Re = " << flow_.reynolds_number()
                  << " | E_max(coaxial) = " << electro_.coaxial_E_max()
                  << " V/m\n";
    }

    /**
     * Run the coupled outer iteration loop.
     *
     * In a production solver the inner solves would be iterative PDE solvers
     * (SIMPLE for flow, CG/GMRES for Poisson, upwind for transport).
     * Here we demonstrate the coupling structure and metric extraction.
     */
    SolverMetrics solve() {
        SolverMetrics metrics;
        metrics.Re = flow_.reynolds_number();

        double prev_residual = 1e30;

        for (int iter = 0; iter < card_.max_outer_iterations; ++iter) {
            // (a) Flow sub-solve stub
            //     In production: SIMPLE or projection method
            //     Here: keep Poiseuille baseline (no body force update yet)

            // (b) Electrostatic sub-solve stub
            //     In production: assemble charge → solve Poisson → compute E
            //     Here: update E from potential via finite-difference
            ElectrostaticModel::compute_field_from_potential(electric_field_);

            // (c) Ion transport sub-solve stub
            //     In production: assemble N_i for each species → advect/diffuse
            //     Here: compute flux at each cell using current fields
            update_species_fluxes();

            // (d) Update ρ_e from species
            update_charge_density();

            // Compute residual (change in charge density as proxy)
            double residual = compute_residual();

            if (iter > 0 && residual < card_.convergence_tol) {
                metrics.converged = true;
                metrics.iterations_used = iter + 1;
                metrics.residual_final = residual;
                break;
            }

            prev_residual = residual;
            metrics.iterations_used = iter + 1;
            metrics.residual_final = residual;
        }

        // Extract output metrics
        metrics.delta_P = FlowModel::compute_pressure_drop(flow_field_);
        metrics.E_max   = electric_field_.E_max();

        // Approximate total volume (axisymmetric: ∫ 2πr dr dz over Ω_f)
        double Omega_f = constants::PI * params_.tube_radius_m
                       * params_.tube_radius_m
                       * (params_.tube_length_m + params_.inlet_length_m
                        + params_.outlet_length_m);
        metrics.E_avg = electric_field_.E_avg(Omega_f);
        metrics.u_max = flow_field_.max_velocity_magnitude();

        for (size_t i = 0; i < species_fields_.size(); ++i) {
            metrics.outlet_flux.push_back(
                IonTransportModel::outlet_molar_flux(
                    species_fields_[i], params_.tube_radius_m));
            metrics.accumulation_idx.push_back(
                IonTransportModel::accumulation_index(species_fields_[i]));
        }

        return metrics;
    }

    // Accessors
    const FlowField&                flow_field()     const { return flow_field_; }
    const ElectricField&            electric_field()  const { return electric_field_; }
    const std::vector<SpeciesField>& species_fields() const { return species_fields_; }
    const EHDParameters&            params()          const { return params_; }

private:
    EHDParameters   params_;
    RunCard         card_;

    FlowModel          flow_;
    ElectrostaticModel electro_;
    IonTransportModel  transport_;

    FlowField                  flow_field_;
    ElectricField              electric_field_;
    std::vector<SpeciesField>  species_fields_;

    /**
     * Update species flux vectors using current flow and electric fields.
     */
    void update_species_fluxes() {
        for (size_t si = 0; si < species_fields_.size(); ++si) {
            auto& sf = species_fields_[si];
            const auto& sp = params_.species[si];

            for (int ir = 1; ir < sf.nr - 1; ++ir) {
                for (int iz = 1; iz < sf.nz - 1; ++iz) {
                    // Concentration gradient (central difference)
                    double dcdr = (sf.at(ir + 1, iz).concentration
                                 - sf.at(ir - 1, iz).concentration) / (2.0 * sf.dr);
                    double dcdz = (sf.at(ir, iz + 1).concentration
                                 - sf.at(ir, iz - 1).concentration) / (2.0 * sf.dz);
                    Vec3 grad_c{dcdr, 0.0, dcdz};

                    // Electric potential gradient
                    Vec3 grad_phi = electric_field_.at(ir, iz).field * (-1.0);

                    // Velocity
                    Vec3 u = flow_field_.at(ir, iz).velocity;

                    // Concentration
                    double c = sf.at(ir, iz).concentration;

                    sf.at(ir, iz).flux = IonTransportModel::compute_flux(
                        sp, c, grad_c, grad_phi, u, params_.fluid.temperature_K);
                }
            }
        }
    }

    /**
     * Update charge density in electric field from species concentrations.
     */
    void update_charge_density() {
        for (int ir = 0; ir < electric_field_.nr; ++ir) {
            for (int iz = 0; iz < electric_field_.nz; ++iz) {
                std::vector<double> concs;
                for (const auto& sf : species_fields_) {
                    concs.push_back(sf.at(ir, iz).concentration);
                }
                electric_field_.at(ir, iz).charge_density =
                    IonTransportModel::assemble_charge_density(params_.species, concs);
            }
        }
    }

    /**
     * Compute convergence residual (L2 norm of charge density change).
     */
    double compute_residual() const {
        double sum = 0.0;
        for (const auto& cell : electric_field_.cells) {
            sum += cell.charge_density * cell.charge_density;
        }
        return std::sqrt(sum / std::max(size_t{1}, electric_field_.cells.size()));
    }
};

} // namespace physics
} // namespace ehd
} // namespace vsepr
