#pragma once
// mcf_cai_integrator.hpp - MCF-CAI Kernel Fork - Integrator API
// v1.0 | VSPER-SIM v5.0.0-main | Status: BLUE

#include "mcf_cai_object.hpp"
#include "mcf_cai_world.hpp"
#include <cmath>
#include <functional>
#include <memory>
#include <string>

namespace vsim::kernel_mcf {

// ForceEvaluator: reads positions, writes forces, returns potential energy [eV]
using ForceEvaluator = std::function<double(McfCaiWorld&)>;

// ---- Integrator interface -------------------------------------------------
struct IMcfCaiIntegrator {
    virtual ~IMcfCaiIntegrator() = default;
    virtual void step(McfCaiWorld& world, const ForceEvaluator& eval) = 0;
    virtual std::string name() const = 0;
};

// ---- Velocity-Verlet NVE/NVT integrator -----------------------------------
class VelocityVerletIntegrator : public IMcfCaiIntegrator {
public:
    std::string name() const override { return "velocity_verlet"; }

    void step(McfCaiWorld& world, const ForceEvaluator& eval) override {
        const double dt      = world.params().dt_ps;
        const double dt_half = 0.5 * dt;
        const std::size_t N  = world.size();

        // half-kick: v += F/m * dt/2
        for (std::size_t i = 0; i < N; ++i) {
            auto& o = world.at(i);
            const double inv_m = (o.mass() > 0.0) ? 1.0 / o.mass() : 0.0;
            for (int d = 0; d < 3; ++d)
                o.velocity[d] += o.force[d] * inv_m * dt_half;
        }
        // drift: x += v * dt
        for (std::size_t i = 0; i < N; ++i) {
            auto& o = world.at(i);
            for (int d = 0; d < 3; ++d)
                o.pos()[d] += o.velocity[d] * dt;
        }
        if (world.box().any_pbc()) world.wrap_positions();
        // force eval
        pe_ = eval(world);
        // half-kick: v += F_new/m * dt/2
        for (std::size_t i = 0; i < N; ++i) {
            auto& o = world.at(i);
            const double inv_m = (o.mass() > 0.0) ? 1.0 / o.mass() : 0.0;
            for (int d = 0; d < 3; ++d)
                o.velocity[d] += o.force[d] * inv_m * dt_half;
        }
        // Berendsen thermostat
        const auto& p = world.params();
        const double T = world.stats().temperature;
        if (p.thermostat == "berendsen" && T > 1e-12) {
            const double lam = std::sqrt(1.0 + (p.dt_ps / p.tau_thermo_ps) * (p.temperature_K / T - 1.0));
            for (auto& o : world)
                for (int d = 0; d < 3; ++d) o.velocity[d] *= lam;
        }
        world.recompute_stats();
    }
    double last_pe() const noexcept { return pe_; }
private:
    double pe_ = 0.0;
};

// ---- FIRE geometry optimizer ----------------------------------------------
class FireIntegrator : public IMcfCaiIntegrator {
public:
    double dt_init = 0.1; double dt_max = 1.0; double alpha_init = 0.1;
    double f_inc = 1.1;   double f_dec = 0.5;  double f_alpha = 0.99; int n_min = 5;

    std::string name() const override { return "fire"; }

    void step(McfCaiWorld& world, const ForceEvaluator& eval) override {
        if (first_) { dt_ = dt_init; alpha_ = alpha_init; n_ = 0; pe_ = eval(world); first_ = false; }
        const std::size_t N = world.size();
        double P = 0.0, nF = 0.0, nv = 0.0;
        for (std::size_t i = 0; i < N; ++i) {
            const auto& o = world.at(i);
            for (int d = 0; d < 3; ++d) {
                P  += o.force[d] * o.velocity[d];
                nF += o.force[d] * o.force[d];
                nv += o.velocity[d] * o.velocity[d];
            }
        }
        nF = std::sqrt(nF); nv = std::sqrt(nv);
        const double snF = (nF > 1e-30) ? nF : 1e-30;
        const double snv = (nv > 1e-30) ? nv : 1e-30;
        for (std::size_t i = 0; i < N; ++i) {
            auto& o = world.at(i);
            for (int d = 0; d < 3; ++d)
                o.velocity[d] = (1.0-alpha_)*o.velocity[d] + alpha_*(o.force[d]/snF)*snv;
        }
        if (P > 0.0) { if (++n_ > n_min) { dt_ = std::min(dt_*f_inc, dt_max); alpha_ *= f_alpha; } }
        else { n_ = 0; dt_ *= f_dec; alpha_ = alpha_init;
               for (auto& o : world) o.velocity = kZeroVec3d; }
        const double dth = 0.5 * dt_;
        for (std::size_t i = 0; i < N; ++i) {
            auto& o = world.at(i);
            const double im = (o.mass() > 0.0) ? 1.0/o.mass() : 0.0;
            for (int d = 0; d < 3; ++d) o.velocity[d] += o.force[d]*im*dth;
        }
        for (std::size_t i = 0; i < N; ++i) {
            auto& o = world.at(i);
            for (int d = 0; d < 3; ++d) o.pos()[d] += o.velocity[d]*dt_;
        }
        pe_ = eval(world);
        for (std::size_t i = 0; i < N; ++i) {
            auto& o = world.at(i);
            const double im = (o.mass() > 0.0) ? 1.0/o.mass() : 0.0;
            for (int d = 0; d < 3; ++d) o.velocity[d] += o.force[d]*im*dth;
        }
        world.recompute_stats();
    }
    double last_pe() const noexcept { return pe_; }
private:
    bool first_ = true; double dt_ = 0.1; double alpha_ = 0.1; int n_ = 0; double pe_ = 0.0;
};

// ---- Factory --------------------------------------------------------------
inline std::unique_ptr<IMcfCaiIntegrator> make_integrator(const std::string& n) {
    if (n == "velocity_verlet" || n == "md")       return std::make_unique<VelocityVerletIntegrator>();
    if (n == "fire"            || n == "optimize")  return std::make_unique<FireIntegrator>();
    return nullptr;
}

} // namespace vsim::kernel_mcf
