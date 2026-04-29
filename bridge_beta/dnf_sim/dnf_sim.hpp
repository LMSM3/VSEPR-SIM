#pragma once
// ============================================================================
// dnf_sim.hpp - Bridge Beta: Mock Dependency Resolver
// ============================================================================
// Simulates a package manager (dnf-style) for internal VSEPR-SIM modules.
// Checks that all bridge architecture prerequisites are present and
// version-compatible before kernel operations proceed.
//
// Usage:
//   DnfSim dnf;
//   dnf.register_available("atomistic_core", {2,1,0});
//   auto report = dnf.resolve_all();
//   if (!report.all_satisfied) { /* handle missing deps */ }
// ============================================================================

#include <string>
#include <vector>
#include <cstdint>
#include <functional>
#include <map>

namespace vsepr {
namespace bridge_beta {

// -----------------------------------------------------------------------
// SemVer - minimal semantic version triple
// -----------------------------------------------------------------------
struct SemVer {
    uint16_t major = 0;
    uint16_t minor = 0;
    uint16_t patch = 0;

    bool satisfies(const SemVer& minimum) const {
        if (major != minimum.major) return major > minimum.major;
        if (minor != minimum.minor) return minor > minimum.minor;
        return patch >= minimum.patch;
    }

    std::string to_string() const {
        return std::to_string(major) + "." +
               std::to_string(minor) + "." +
               std::to_string(patch);
    }
};

// -----------------------------------------------------------------------
// PackageStatus - result of resolving one dependency
// -----------------------------------------------------------------------
enum class PackageStatus : uint8_t {
    Satisfied,     // present and version >= minimum
    VersionMismatch, // present but version < minimum
    Missing,        // not found at all
    Downloading,    // fake download in progress
    Installed       // just acquired (simulated)
};

struct PackageResult {
    std::string    name;
    SemVer         required;
    SemVer         found;
    PackageStatus  status = PackageStatus::Missing;
    double         download_progress = 0.0; // 0.0 to 1.0
    std::string    provides;  // human-readable description
};

// -----------------------------------------------------------------------
// ResolveReport - aggregate result of resolving all prerequisites
// -----------------------------------------------------------------------
struct ResolveReport {
    std::vector<PackageResult> packages;
    bool all_satisfied = false;
    int  satisfied_count = 0;
    int  missing_count = 0;
    int  mismatch_count = 0;

    std::string summary() const;
};

// -----------------------------------------------------------------------
// ProgressCallback - called during fake download simulation
// -----------------------------------------------------------------------
using ProgressCallback = std::function<void(const std::string& pkg, double progress)>;

// -----------------------------------------------------------------------
// DnfSim - the mock dependency resolver
// -----------------------------------------------------------------------
class DnfSim {
public:
    DnfSim();

    // Register a module as available in the current build
    void register_available(const std::string& name, SemVer version,
                           const std::string& provides = "");

    // Auto-detect available modules by probing known headers/symbols
    void auto_detect();

    // Resolve all prerequisites against registered modules
    ResolveReport resolve_all() const;

    // Simulate downloading missing/outdated packages (fake, with progress)
    ResolveReport install_missing(ProgressCallback cb = nullptr);

    // Print a formatted dependency table to stdout
    void print_status() const;

private:
    struct Prerequisite {
        std::string name;
        SemVer      minimum;
        std::string provides;
    };

    std::vector<Prerequisite> prerequisites_;
    std::map<std::string, SemVer> available_;
    std::map<std::string, std::string> available_provides_;
};

} // namespace bridge_beta
} // namespace vsepr
