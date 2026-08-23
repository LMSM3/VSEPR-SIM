#pragma once
#include "../core/state.hpp"
#include "../models/model.hpp"

namespace atomistic {

struct FIREParams {
    double dt = 1e-3;
    double alpha = 0.1;
    double finc = 1.1;
    double fdec = 0.5;
    double falpha = 0.99;
    int nmin = 5;
    double dt_max = 1e-1;

    double epsF = 1e-6;  // RMS force threshold
    double epsU = 1e-10; // per-particle energy delta threshold
    int max_steps = 5000;
};

struct FIREStats {
    int step{};
    double U{};
    double dU_per_atom{};
    double Frms{};
    double alpha{};
    double dt{};
};

struct FIRE {
    const IModel& model;
    ModelParams mp;

    explicit FIRE(const IModel& m, ModelParams p) : model(m), mp(p) {}

    FIREStats minimize(State& s, const FIREParams& fp) const;
};

} // namespace atomistic
