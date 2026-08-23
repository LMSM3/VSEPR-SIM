#pragma once
#include "../core/state.hpp"
#include "../core/environment.hpp"
#include <map>
#include <memory>

namespace atomistic {

struct ModelParams {
    double rc = 10.0;        // cutoff radius (Å)
    double k_coul = 332.0636; // Coulomb constant (kcal·Å/(mol·e²))  -  AMBER standard

    // Physical context  -  controls dielectric screening, solvation, etc.
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

// ---- Built-in model factories ----

/// LJ + Coulomb (default MD model)
std::unique_ptr<IModel> create_lj_coulomb_model();

/// Morse pair potential (Girifalco-Weizer parameters for metals + UFF fallback).
/// @param z_map  Map from State::type[i] to atomic number Z.
std::unique_ptr<IModel> create_morse_model(std::map<uint32_t, int> z_map = {});

/// Sutton-Chen EAM for FCC metals (two-pass: density accumulation then forces).
/// @param z_map  Map from State::type[i] to atomic number Z.
std::unique_ptr<IModel> create_eam_model(std::map<uint32_t, int> z_map = {});

} // namespace atomistic
