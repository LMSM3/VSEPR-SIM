#pragma once
#include "../core/state.hpp"
#include <memory>

namespace atomistic {

struct ModelParams {
    double rc = 10.0;        // cutoff radius (Å)
    double k_coul = 332.0636; // Coulomb constant (kcal·Å/(mol·e²)) - AMBER standard
                              // Old value 138.935 was wrong (SI-based or dielectric scaled)

    // DEPRECATED: Global LJ parameters (per-type params used instead)
    // These are kept for backward compatibility but should be 0.0 to disable
    double eps = 0.0;        // LJ epsilon (DEPRECATED - use per-type)
    double sigma = 0.0;      // LJ sigma (DEPRECATED - use per-type)
};

struct IModel {
    virtual ~IModel() = default;
    // Must fill s.F and s.E for current X, Q, etc.
    virtual void eval(State& s, const ModelParams& p) const = 0;
};

// Factory for built-in models
std::unique_ptr<IModel> create_lj_coulomb_model();

} // namespace atomistic
