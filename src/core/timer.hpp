#pragma once
/**
 * timer.hpp - Simple performance timing utilities
 */

#include <chrono>
#include <string>
#include <unordered_map>
#include <iostream>

namespace vsepr {

class Timer {
public:
    void start() {
        start_time_ = std::chrono::high_resolution_clock::now();
    }
    
    double elapsed() const {
        auto now = std::chrono::high_resolution_clock::now();
        return std::chrono::duration<double>(now - start_time_).count();
    }
    
private:
    std::chrono::time_point<std::chrono::high_resolution_clock> start_time_;
};

// Global timer registry for profiling
class TimerRegistry {
public:
    static TimerRegistry& instance() {
        static TimerRegistry reg;
        return reg;
    }
    
    void start(const std::string& name) {
        timers_[name].start();
    }
    
    void stop(const std::string& name) {
        double elapsed = timers_[name].elapsed();
        totals_[name] += elapsed;
        counts_[name]++;
    }
    
    void report() const {
        std::cout << "\n=== Timer Report ===\n";
        for (const auto& [name, total] : totals_) {
            int count = counts_.at(name);
            std::cout << name << ": " << total << "s (" 
                      << count << " calls, avg " 
                      << (total / count) << "s)\n";
        }
    }
    
private:
    std::unordered_map<std::string, Timer> timers_;
    std::unordered_map<std::string, double> totals_;
    std::unordered_map<std::string, int> counts_;
};

// RAII timer
class ScopedTimer {
public:
    ScopedTimer(const std::string& name) : name_(name) {
        TimerRegistry::instance().start(name_);
    }
    
    ~ScopedTimer() {
        TimerRegistry::instance().stop(name_);
    }
    
private:
    std::string name_;
};

#define VSEPR_TIME(name) ScopedTimer _timer(name)

} // namespace vsepr
