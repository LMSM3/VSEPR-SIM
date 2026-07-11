#pragma once
#include "../core/state.hpp"
#include "../core/environment.hpp"
#include <memory>

namespace atomistic {

struct ModelParams {
    double rc = 10.0;        // cutoff radius (Å)
    double k_coul = 332.0636; // Coulomb constant (kcal·Å/(mol·e²)) — AMBER standard

    // Physical context — controls dielectric screening, solvation, etc.
    // Defaults to NearVacuum (no screening, eps_r=1).
    EnvironmentContext env;

    // DEPRECATED: Global LJ parameters (per-type params used instead)
    double eps   = 0.0;
    double sigma = 0.0;
};

struct IModel {
    virtual ~IModel() = default;
    // Must fill s.F and s.E for current X, Q, etc.
    virtual void eval(State& s, const ModelParams& p) const = 0;
};

// Factory for built-in models
std::unique_ptr<IModel> create_lj_coulomb_model();

} // namespace atomistic
