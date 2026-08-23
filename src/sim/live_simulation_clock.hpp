#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace vsepr {

// Wall-clock scheduler for the live worker. Solver-specific timesteps remain
// owned by SimulationState; this clock controls how often a solver step runs.
class LiveSimulationClock {
public:
    static constexpr double kFixedTickSeconds = 1.0 / 120.0;
    static constexpr double kMaximumFrameSeconds = 0.1;

    static constexpr std::array<double, 5> kSpeedPresets{
        0.1, 0.5, 1.0, 2.0, 10.0
    };

    bool set_speed_multiplier(double speed) {
        if (!is_supported_speed(speed)) {
            return false;
        }
        speed_multiplier_ = speed;
        return true;
    }

    static bool is_supported_speed(double speed) {
        for (double preset : kSpeedPresets) {
            if (std::abs(speed - preset) < 1e-9) {
                return true;
            }
        }
        return false;
    }

    int consume_elapsed(double wall_seconds) {
        const double clamped = std::clamp(wall_seconds, 0.0, kMaximumFrameSeconds);
        accumulator_seconds_ += clamped * speed_multiplier_;

        const int ticks = static_cast<int>(accumulator_seconds_ / kFixedTickSeconds);
        accumulator_seconds_ -= static_cast<double>(ticks) * kFixedTickSeconds;
        return ticks;
    }

    void complete_tick() {
        ++tick_count_;
        simulation_time_seconds_ += kFixedTickSeconds;
    }

    void reset() {
        accumulator_seconds_ = 0.0;
        simulation_time_seconds_ = 0.0;
        tick_count_ = 0;
    }

    double speed_multiplier() const { return speed_multiplier_; }
    double simulation_time_seconds() const { return simulation_time_seconds_; }
    double interpolation_alpha() const {
        return std::clamp(accumulator_seconds_ / kFixedTickSeconds, 0.0, 1.0);
    }
    std::uint64_t tick_count() const { return tick_count_; }

private:
    double speed_multiplier_ = 1.0;
    double accumulator_seconds_ = 0.0;
    double simulation_time_seconds_ = 0.0;
    std::uint64_t tick_count_ = 0;
};

} // namespace vsepr
