#pragma once
/**
 * bootstrap_probe.hpp
 * Public declarations for infra/hardware probes.
 */

#include <string>

namespace vsepr {
namespace infra {

// RAM / disk telemetry (GB).
double total_ram_gb();
double free_ram_gb();
double disk_free_gb();

// GPU name detection (empty if not found).
std::string detect_gpu();

// Poseable CPU-load sampler. First call seeds state and returns 0.0;
// subsequent calls return a [0,1] fraction relative to the previous sample.
struct CpuLoadState {
#ifdef _WIN32
    unsigned long long idle   = 0;
    unsigned long long kernel = 0;
    unsigned long long user   = 0;
#else
    unsigned long long idle   = 0;
    unsigned long long total  = 0;
#endif
    bool seeded = false;
};

double cpu_load_fraction(CpuLoadState& state);

} // namespace infra
} // namespace vsepr