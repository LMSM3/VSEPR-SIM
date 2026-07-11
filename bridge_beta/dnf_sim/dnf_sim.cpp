// ============================================================================
// dnf_sim.cpp - Bridge Beta: Mock Dependency Resolver Implementation
// ============================================================================

#include "dnf_sim.hpp"
#include <iostream>
#include <iomanip>
#include <thread>
#include <chrono>
#include <sstream>

namespace vsepr {
namespace bridge_beta {

// -----------------------------------------------------------------------
// Constructor: register the bridge architecture prerequisites
// -----------------------------------------------------------------------
DnfSim::DnfSim() {
    prerequisites_ = {
        {"atomistic_core", {2,0,0}, "State, IModel, FIRE/Verlet integrators"},
        {"scene_document",  {1,3,0}, "SceneDocument, FrameData, Provenance"},
        {"glass_pipeline",  {1,0,0}, "Topology->Layout->PrerenderBuffers->SVG"},
        {"coarse_grain",    {0,8,0}, "BeadSystem, SH descriptors, LL-FIRE"},
        {"bridge_adapter",  {1,1,0}, "EngineAdapter, KernelRequest/Result"},
    };
}

// -----------------------------------------------------------------------
// register_available
// -----------------------------------------------------------------------
void DnfSim::register_available(const std::string& name, SemVer version,
                                const std::string& provides) {
    available_[name] = version;
    if (!provides.empty()) available_provides_[name] = provides;
}

// -----------------------------------------------------------------------
// auto_detect: probe known compile-time symbols
// -----------------------------------------------------------------------
void DnfSim::auto_detect() {
    // Atomistic core - always present in current build
    register_available("atomistic_core", {2,1,0}, "State, IModel, integrators");

    // Scene document
    register_available("scene_document", {1,3,2}, "SceneDocument, FrameData");

    // Glass pipeline (we just built it)
    register_available("glass_pipeline", {1,0,0}, "Topology->Layout->SVG");

    // Coarse grain
    register_available("coarse_grain", {0,9,0}, "BeadSystem, SH, LL-FIRE");

    // Bridge adapter
    register_available("bridge_adapter", {1,1,0}, "EngineAdapter");
}

// -----------------------------------------------------------------------
// resolve_all
// -----------------------------------------------------------------------
ResolveReport DnfSim::resolve_all() const {
    ResolveReport report;
    report.all_satisfied = true;

    for (const auto& prereq : prerequisites_) {
        PackageResult pr;
        pr.name = prereq.name;
        pr.required = prereq.minimum;
        pr.provides = prereq.provides;

        auto it = available_.find(prereq.name);
        if (it == available_.end()) {
            pr.status = PackageStatus::Missing;
            pr.found = {0,0,0};
            report.missing_count++;
            report.all_satisfied = false;
        } else {
            pr.found = it->second;
            if (it->second.satisfies(prereq.minimum)) {
                pr.status = PackageStatus::Satisfied;
                pr.download_progress = 1.0;
                report.satisfied_count++;
            } else {
                pr.status = PackageStatus::VersionMismatch;
                report.mismatch_count++;
                report.all_satisfied = false;
            }
        }
        report.packages.push_back(pr);
    }
    return report;
}

// -----------------------------------------------------------------------
// install_missing: fake download simulation with progress callbacks
// -----------------------------------------------------------------------
ResolveReport DnfSim::install_missing(ProgressCallback cb) {
    auto report = resolve_all();

    for (auto& pkg : report.packages) {
        if (pkg.status == PackageStatus::Missing ||
            pkg.status == PackageStatus::VersionMismatch) {

            pkg.status = PackageStatus::Downloading;

            // Simulate download in 10 steps
            for (int step = 1; step <= 10; ++step) {
                pkg.download_progress = step / 10.0;
                if (cb) cb(pkg.name, pkg.download_progress);
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }

            // Mark as installed and register
            pkg.status = PackageStatus::Installed;
            pkg.found = pkg.required;
            register_available(pkg.name, pkg.required, pkg.provides);
        }
    }

    // Re-evaluate
    report = resolve_all();
    return report;
}

// -----------------------------------------------------------------------
// summary
// -----------------------------------------------------------------------
std::string ResolveReport::summary() const {
    std::ostringstream oss;
    oss << "Bridge Beta Dependency Report\n";
    oss << "============================\n";
    for (const auto& p : packages) {
        const char* tag = "???";
        switch (p.status) {
            case PackageStatus::Satisfied:      tag = " OK "; break;
            case PackageStatus::VersionMismatch: tag = "MISM"; break;
            case PackageStatus::Missing:         tag = "MISS"; break;
            case PackageStatus::Downloading:     tag = "DWNL"; break;
            case PackageStatus::Installed:       tag = "INST"; break;
        }
        oss << "  [" << tag << "] " << p.name
            << " (need " << p.required.to_string()
            << ", have " << p.found.to_string() << ")\n";
    }
    oss << "\n" << satisfied_count << "/" << packages.size()
        << " satisfied";
    if (missing_count > 0) oss << ", " << missing_count << " missing";
    if (mismatch_count > 0) oss << ", " << mismatch_count << " version mismatch";
    oss << "\n";
    return oss.str();
}

// -----------------------------------------------------------------------
// print_status
// -----------------------------------------------------------------------
void DnfSim::print_status() const {
    auto report = resolve_all();
    std::cout << report.summary();
}

} // namespace bridge_beta
} // namespace vsepr
