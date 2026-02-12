#pragma once
#include "state.hpp"
#include <vector>
#include <cmath>

namespace atomistic {

// Welford online variance (stable, numerically safe)
class OnlineStats {
public:
    void add_sample(double x) {
        n++;
        double delta = x - mean;
        mean += delta / n;
        double delta2 = x - mean;
        M2 += delta * delta2;
    }

    double get_mean() const { return mean; }
    double get_variance() const { return (n > 1) ? M2 / (n - 1) : 0.0; }
    double get_stddev() const { return std::sqrt(get_variance()); }
    size_t count() const { return n; }

private:
    size_t n = 0;
    double mean = 0.0;
    double M2 = 0.0;
};

// Online covariance for Vec3 positions
class OnlineVec3Stats {
public:
    void add_sample(const Vec3& v) {
        n++;
        Vec3 delta = v - mean;
        mean = mean + delta * (1.0 / n);
        Vec3 delta2 = v - mean;
        M2.x += delta.x * delta2.x;
        M2.y += delta.y * delta2.y;
        M2.z += delta.z * delta2.z;
    }

    Vec3 get_mean() const { return mean; }
    Vec3 get_variance() const {
        if (n > 1) return M2 * (1.0 / (n - 1));
        return {0, 0, 0};
    }
    double get_total_variance() const {
        auto v = get_variance();
        return v.x + v.y + v.z;
    }

private:
    size_t n = 0;
    Vec3 mean = {0, 0, 0};
    Vec3 M2 = {0, 0, 0};
};

// Stationarity gate for energy/observable convergence
struct StationarityGate {
    double eps_js = 0.01;      // Jensen-Shannon threshold
    double eps_mean = 1e-6;    // Mean drift threshold
    double eps_var = 1e-6;     // Variance drift threshold
    int consecutive_k = 10;    // Require K consecutive passes

    int consecutive_passes = 0;

    bool test(const OnlineStats& current, double new_sample) {
        // Simple test: check if new sample is within σ of mean
        double mean = current.get_mean();
        double sigma = current.get_stddev();
        
        double deviation = std::abs(new_sample - mean);
        bool pass = (deviation < 3.0 * sigma + eps_mean);
        
        if (pass) {
            consecutive_passes++;
        } else {
            consecutive_passes = 0;
        }

        return consecutive_passes >= consecutive_k;
    }

    void reset() {
        consecutive_passes = 0;
    }
};

// Simple energy/observable tracker for templates
struct ObservableTracker {
    OnlineStats energy_total;
    OnlineStats energy_bond;
    OnlineStats energy_vdw;
    OnlineStats energy_coul;
    
    StationarityGate gate;

    void add_state(const State& s) {
        energy_total.add_sample(s.E.total());
        energy_bond.add_sample(s.E.Ubond);
        energy_vdw.add_sample(s.E.UvdW);
        energy_coul.add_sample(s.E.UCoul);
    }

    bool is_stationary(const State& s) {
        return gate.test(energy_total, s.E.total());
    }
};

} // namespace atomistic
