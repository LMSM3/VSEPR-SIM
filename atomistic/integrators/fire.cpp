#include "fire.hpp"
#include "atomistic/core/maxwell_boltzmann.hpp"
#include <algorithm>
#include <stdexcept>
#include <iostream>

namespace atomistic {

static double rms_force(const State& s) {
    long double acc = 0;
    for (auto& f : s.F) acc += (long double)dot(f, f);
    return std::sqrt((double)(acc / (long double)s.N));
}

FIREStats FIRE::minimize(State& s, const FIREParams& fp) const {
    if (!sane(s)) throw std::runtime_error("State not sane()");
    s.F.resize(s.N);

    // Evaluate initial forces
    model.eval(s, mp);

    // Initialize velocities along force direction (deterministic descent)
    // This prevents P=0 deadlock on first iteration
    initialize_velocities_along_force(s, fp.dt);

    double dt = fp.dt;
    double alpha = fp.alpha;
    int npos = 0;

    double Uprev = std::numeric_limits<double>::infinity();

    for (int t = 0; t < fp.max_steps; ++t) {
        // Evaluate forces + energies at current X
        model.eval(s, mp);
        double U = s.E.total();
        double Frms = rms_force(s);

        // Stop conditions (skip first 2 iterations to allow velocity build-up)
        if (t > 1) {
            double dU = std::abs(U - Uprev);
            double dU_per_atom = dU / (double)s.N;
            if (Frms < fp.epsF || dU_per_atom < fp.epsU) {
                return {t, U, dU_per_atom, Frms, alpha, dt};
            }
        }

        // Update Uprev for next iteration
        Uprev = U;
        // Power P = v·f
        long double P = 0;
        long double vnorm2 = 0;
        long double fnorm2 = 0;
        for (uint32_t i = 0; i < s.N; i++) {
            P += (long double)dot(s.V[i], s.F[i]);
            vnorm2 += (long double)dot(s.V[i], s.V[i]);
            fnorm2 += (long double)dot(s.F[i], s.F[i]);
        }
        double vnorm = std::sqrt((double)vnorm2);
        double fnorm = std::sqrt((double)fnorm2);

        // v <- (1-α)v + α |v| f/|f|
        if (fnorm > 0 && vnorm > 0) {
            for (uint32_t i = 0; i < s.N; i++) {
                Vec3 fhat = s.F[i] * (1.0 / fnorm);
                s.V[i] = s.V[i] * (1.0 - alpha) + fhat * (alpha * vnorm);
            }
        }

        if (P > 0) {
            npos++;
            if (npos > fp.nmin) {
                dt = std::min(dt * fp.finc, fp.dt_max);
                alpha *= fp.falpha;
            }
        } else {
            npos = 0;
            dt *= fp.fdec;
            alpha = fp.alpha;
            for (auto& v : s.V) v = {0, 0, 0};
        }

        // X <- X + dt V (explicit Euler)
        for (uint32_t i = 0; i < s.N; i++) {
            s.X[i] = s.X[i] + s.V[i] * dt;
        }

        Uprev = U;
    }

    // Final eval for telemetry
    model.eval(s, mp);
    return {fp.max_steps, s.E.total(), std::abs(s.E.total() - Uprev) / s.N, rms_force(s), alpha, dt};
}

} // namespace atomistic
