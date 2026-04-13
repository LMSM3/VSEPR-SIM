/**
 * demo_version_lineage.cpp
 * ------------------------
 * VSEPR-SIM 4.0 Legacy-Beta — Version Lineage & Multi-Scale Registry Demo
 *
 * Displays:
 *   1. Full kernel lineage (v0.1 → 4.0-LB) with commit hashes
 *   2. Scale registry (1-5) with units, status, and bridge methods
 *   3. Multi-scale fidelity chain (cumulative information preservation)
 *   4. Property search domain map (what can be computed at each scale)
 *   5. Nuclear simulation property targets
 *
 * This is the "birth certificate" of the 4.0-legacy-beta branch.
 *
 * Build:
 *   cmake --build build --target demo-version-lineage
 */

#include "version/version_manifest.hpp"
#include "multiscale/scale_bridge.hpp"

#include <iostream>
#include <iomanip>
#include <string>
#include <cmath>

namespace ansi {
    const char* RESET   = "\033[0m";
    const char* BOLD    = "\033[1m";
    const char* DIM     = "\033[2m";
    const char* RED     = "\033[91m";
    const char* GREEN   = "\033[92m";
    const char* YELLOW  = "\033[93m";
    const char* BLUE    = "\033[94m";
    const char* MAGENTA = "\033[95m";
    const char* CYAN    = "\033[96m";
    const char* WHITE   = "\033[97m";
    const char* GRAY    = "\033[90m";
}

// ============================================================================
// ASCII progress bar
// ============================================================================

std::string progress_bar(double fraction, int width = 20) {
    int filled = static_cast<int>(fraction * width);
    std::string bar;
    for (int i = 0; i < width; ++i) {
        if (i < filled) bar += "█";
        else            bar += "░";
    }
    return bar;
}

// ============================================================================
// Era color
// ============================================================================

const char* era_color(vsepr::version::KernelEra era) {
    using vsepr::version::KernelEra;
    switch (era) {
        case KernelEra::Genesis:       return ansi::RED;
        case KernelEra::Foundation:    return ansi::YELLOW;
        case KernelEra::Formation:     return ansi::GREEN;
        case KernelEra::Methodology:   return ansi::CYAN;
        case KernelEra::Verification:  return ansi::BLUE;
        case KernelEra::Integration:   return ansi::MAGENTA;
        case KernelEra::Audit:         return ansi::WHITE;
        case KernelEra::MultiScale:    return ansi::GREEN;
        default:                       return ansi::GRAY;
    }
}

const char* era_name(vsepr::version::KernelEra era) {
    using vsepr::version::KernelEra;
    switch (era) {
        case KernelEra::Genesis:       return "Genesis";
        case KernelEra::Foundation:    return "Foundation";
        case KernelEra::Formation:     return "Formation";
        case KernelEra::Methodology:   return "Methodology";
        case KernelEra::Verification:  return "Verification";
        case KernelEra::Integration:   return "Integration";
        case KernelEra::Audit:         return "Audit";
        case KernelEra::MultiScale:    return "MultiScale";
        default:                       return "Unknown";
    }
}

// ============================================================================
// MAIN
// ============================================================================

int main() {
    using namespace vsepr::version;
    using namespace vsepr::multiscale;

    std::cout << "\n";
    std::cout << ansi::BOLD;
    std::cout << "  ╔══════════════════════════════════════════════════════════════════╗\n";
    std::cout << "  ║       VSEPR-SIM 4.0 Legacy-Beta — Version Lineage Demo         ║\n";
    std::cout << "  ║       Multi-Scale Property Search Registry                     ║\n";
    std::cout << "  ╠══════════════════════════════════════════════════════════════════╣\n";
    std::cout << "  ║  Branch:  4.0-legacy-beta                                      ║\n";
    std::cout << "  ║  Kernel:  v0.1 → v0.3 → 2.7 → 2.9 → 3.0 → 4.0-LB            ║\n";
    std::cout << "  ║  Scales:  Atomistic → CG → Grain → Component → Macro          ║\n";
    std::cout << "  ╚══════════════════════════════════════════════════════════════════╝\n";
    std::cout << ansi::RESET << "\n";

    // ========================================================================
    // Phase 1: Kernel Lineage
    // ========================================================================

    std::cout << ansi::BOLD << "  ═══ Phase 1: Kernel Lineage (v0.1 → 4.0-LB) ═══\n" << ansi::RESET << "\n";

    std::cout << "  ┌──────────┬──────────────┬─────────┬───────┬─────────────────────────────────────────┐\n";
    std::cout << "  │ Tag      │ Era          │ Commit  │ Tests │ Description                             │\n";
    std::cout << "  ├──────────┼──────────────┼─────────┼───────┼─────────────────────────────────────────┤\n";

    for (size_t i = 0; i < LINEAGE_COUNT; ++i) {
        const auto& cp = KERNEL_LINEAGE[i];
        std::string tag_str = cp.tag;
        std::string commit_str = cp.commit_prefix;
        if (commit_str.empty()) commit_str = "-------";

        std::string desc = cp.description;
        if (desc.size() > 39) desc = desc.substr(0, 36) + "...";

        std::cout << "  │ " << era_color(cp.era) << std::setw(8) << std::left << tag_str << ansi::RESET
                  << " │ " << era_color(cp.era) << std::setw(12) << std::left << era_name(cp.era) << ansi::RESET
                  << " │ " << ansi::DIM << std::setw(7) << commit_str << ansi::RESET
                  << " │ " << std::setw(5) << std::right << cp.test_count
                  << " │ " << std::setw(39) << std::left << desc
                  << " │\n";
    }

    std::cout << "  └──────────┴──────────────┴─────────┴───────┴─────────────────────────────────────────┘\n\n";

    // Test count growth visualization
    std::cout << "  Test count growth:\n";
    uint32_t max_tests = 0;
    for (size_t i = 0; i < LINEAGE_COUNT; ++i) {
        if (KERNEL_LINEAGE[i].test_count > max_tests)
            max_tests = KERNEL_LINEAGE[i].test_count;
    }

    for (size_t i = 0; i < LINEAGE_COUNT; ++i) {
        const auto& cp = KERNEL_LINEAGE[i];
        double frac = (max_tests > 0) ? static_cast<double>(cp.test_count) / max_tests : 0.0;

        std::cout << "    " << era_color(cp.era) << std::setw(8) << std::left << cp.tag << ansi::RESET
                  << " " << progress_bar(frac, 30) << " " << cp.test_count << "\n";
    }

    // ========================================================================
    // Phase 2: Scale Registry
    // ========================================================================

    std::cout << "\n" << ansi::BOLD << "  ═══ Phase 2: Simulation Scale Registry (1-5) ═══\n" << ansi::RESET << "\n";

    std::cout << "  ┌───┬───────────────────────┬──────┬──────┬──────────────┬─────────┐\n";
    std::cout << "  │ # │ Scale                 │ Len  │ Time │ Energy       │ Status  │\n";
    std::cout << "  ├───┼───────────────────────┼──────┼──────┼──────────────┼─────────┤\n";

    for (size_t i = 0; i < SCALE_COUNT; ++i) {
        const auto& s = SCALE_REGISTRY[i];
        const char* status_color = ansi::GRAY;
        if (std::string(s.status) == "active")  status_color = ansi::GREEN;
        if (std::string(s.status) == "partial") status_color = ansi::YELLOW;
        if (std::string(s.status) == "planned") status_color = ansi::RED;

        std::cout << "  │ " << static_cast<int>(s.scale) << " │ "
                  << std::setw(21) << std::left << s.name << " │ "
                  << std::setw(4) << s.length_unit << " │ "
                  << std::setw(4) << s.time_unit << " │ "
                  << std::setw(12) << s.energy_unit << " │ "
                  << status_color << std::setw(7) << s.status << ansi::RESET
                  << " │\n";
    }

    std::cout << "  └───┴───────────────────────┴──────┴──────┴──────────────┴─────────┘\n\n";

    // Scale bridge diagram
    std::cout << "  Scale transitions:\n\n";
    for (size_t i = 0; i < SCALE_COUNT; ++i) {
        const auto& s = SCALE_REGISTRY[i];
        const char* color = ansi::GRAY;
        if (std::string(s.status) == "active")  color = ansi::GREEN;
        if (std::string(s.status) == "partial") color = ansi::YELLOW;

        std::cout << "    " << color << ansi::BOLD << "[" << static_cast<int>(s.scale) << "] "
                  << s.name << ansi::RESET;

        if (i < SCALE_COUNT - 1) {
            std::cout << "\n      │\n"
                      << "      ├── ↑ " << ansi::DIM << SCALE_REGISTRY[i].bridge_up << ansi::RESET << "\n"
                      << "      │\n"
                      << "      ▼\n";
        } else {
            std::cout << "  ◄── " << ansi::BOLD << "DIGITAL TWIN OUTPUT" << ansi::RESET << "\n";
        }
    }

    // ========================================================================
    // Phase 3: Multi-Scale Fidelity Chain
    // ========================================================================

    std::cout << "\n" << ansi::BOLD << "  ═══ Phase 3: Cumulative Fidelity Chain ═══\n" << ansi::RESET << "\n";

    PropertySearchEngine engine;
    engine.register_default_transitions();

    std::cout << "  Information preservation across scale transitions:\n\n";

    SimulationScale scales[] = {
        SimulationScale::Atomistic,
        SimulationScale::CoarseGrained,
        SimulationScale::Grain,
        SimulationScale::Component,
        SimulationScale::Macroscopic,
    };

    for (auto s : scales) {
        double f = engine.cumulative_fidelity(s);
        const char* color = ansi::GREEN;
        if (f < 0.6) color = ansi::YELLOW;
        if (f < 0.4) color = ansi::RED;

        std::cout << "    " << std::setw(15) << std::left << PropertySearchEngine::scale_name(s)
                  << " " << color << progress_bar(f, 25) << " "
                  << std::fixed << std::setprecision(0) << (f * 100.0) << "%" << ansi::RESET << "\n";
    }

    std::cout << "\n  Transition details:\n";
    for (const auto& t : engine.transitions()) {
        std::cout << "    " << PropertySearchEngine::scale_name(t.from)
                  << " → " << PropertySearchEngine::scale_name(t.to)
                  << ": " << ansi::DIM << t.method << ansi::RESET
                  << " (" << std::fixed << std::setprecision(0) << (t.fidelity * 100) << "% fidelity)\n";
    }

    // ========================================================================
    // Phase 4: Property Search Domain Map
    // ========================================================================

    std::cout << "\n" << ansi::BOLD << "  ═══ Phase 4: Property Search Targets (" << SEARCH_TARGET_COUNT << " properties) ═══\n" << ansi::RESET << "\n";

    PropertyDomain domains[] = {
        PropertyDomain::Structural,
        PropertyDomain::Mechanical,
        PropertyDomain::Thermal,
        PropertyDomain::Electronic,
        PropertyDomain::Stability,
        PropertyDomain::Nuclear,
    };

    for (auto domain : domains) {
        std::cout << "  " << ansi::BOLD << PropertySearchEngine::domain_name(domain) << ":" << ansi::RESET << "\n";

        for (size_t i = 0; i < SEARCH_TARGET_COUNT; ++i) {
            const auto& t = SEARCH_TARGETS[i];
            if (t.domain != domain) continue;

            // Check what scale it needs and if domain survives
            bool reachable = engine.domain_survives_to(domain, t.source_scale) ||
                             t.source_scale == SimulationScale::Atomistic;

            std::cout << "    " << (reachable ? ansi::GREEN : ansi::RED)
                      << (reachable ? "●" : "○") << ansi::RESET
                      << " " << std::setw(30) << std::left << t.name
                      << " " << std::setw(10) << t.unit
                      << " [Scale " << static_cast<int>(t.source_scale) << ": "
                      << PropertySearchEngine::scale_name(t.source_scale) << "]"
                      << "\n";
        }
        std::cout << "\n";
    }

    // ========================================================================
    // Phase 5: Nuclear Targets — Why This Branch Exists
    // ========================================================================

    std::cout << ansi::BOLD << "  ═══ Phase 5: Nuclear Simulation Targets ═══\n" << ansi::RESET << "\n";

    std::cout << "  The 4.0-legacy-beta branch exists because the mature kernel\n";
    std::cout << "  (v0.1→2.9, 1013 tests, deterministic provenance) now provides\n";
    std::cout << "  a trustworthy foundation for multi-scale property extraction.\n\n";

    std::cout << "  Nuclear-relevant property search targets:\n\n";

    for (size_t i = 0; i < SEARCH_TARGET_COUNT; ++i) {
        const auto& t = SEARCH_TARGETS[i];
        if (t.domain != PropertyDomain::Nuclear) continue;

        double f = engine.cumulative_fidelity(t.source_scale);

        std::cout << "    " << ansi::BOLD << t.name << ansi::RESET
                  << " (" << t.unit << ")\n";
        std::cout << "      Source: Scale " << static_cast<int>(t.source_scale)
                  << " (" << PropertySearchEngine::scale_name(t.source_scale) << ")\n";
        std::cout << "      Scraper: " << t.scraper << "\n";
        std::cout << "      Cumulative fidelity: " << progress_bar(f, 15)
                  << " " << std::fixed << std::setprecision(0) << (f * 100.0) << "%\n\n";
    }

    std::cout << "  Why multi-scale matters for nuclear:\n\n";
    std::cout << "    Scale 1 (Atomistic):  Displacement cascades, Frenkel pairs,\n";
    std::cout << "                          interatomic potentials, point defects\n\n";
    std::cout << "    Scale 2 (CG):         Defect cluster evolution, dislocation\n";
    std::cout << "                          loops, void nucleation kinetics\n\n";
    std::cout << "    Scale 3 (Grain):      Grain boundary segregation, fission gas\n";
    std::cout << "                          bubble growth, radiation swelling\n\n";
    std::cout << "    The exotic materials search (random compositions, HGST filter,\n";
    std::cout << "    robust formation physics) is how we find radiation-tolerant\n";
    std::cout << "    candidates that conventional screening misses.\n\n";

    // ========================================================================
    // Phase 6: Nuclear Core Registry — Z=94 active
    // ========================================================================

    using vsepr::multiscale::NUCLEAR_CORES;
    using vsepr::multiscale::NUCLEAR_CORE_COUNT;

    std::cout << ansi::BOLD << "  ═══ Phase 6: Nuclear Core Registry ═══\n" << ansi::RESET << "\n";

    for (size_t i = 0; i < NUCLEAR_CORE_COUNT; ++i) {
        const auto& c = NUCLEAR_CORES[i];
        std::cout << "    "
                  << (c.active ? ansi::GREEN : ansi::GRAY)
                  << (c.active ? "▶ ACTIVE  " : "  inactive")
                  << ansi::RESET << "  Z=" << std::setw(3) << static_cast<int>(c.Z)
                  << "  " << std::setw(2) << c.symbol
                  << "  " << std::setw(12) << std::left << c.isotope
                  << "  Ed=" << std::setw(5) << c.Ed_eV << " eV"
                  << "  fissility=" << std::fixed << std::setprecision(2) << c.fissility
                  << "  [" << c.crystal_phase << "]\n";
        std::cout << "              " << ansi::DIM << c.notes << ansi::RESET << "\n\n";
    }

    const auto* active = vsepr::multiscale::get_active_core();
    if (active) {
        std::cout << "  " << ansi::BOLD << ansi::GREEN
                  << "core = " << static_cast<int>(active->Z)
                  << " (" << active->isotope << ") — NUCLEAR DOMAIN ACTIVE THROUGH ALL SCALES"
                  << ansi::RESET << "\n\n";
    }

    // ========================================================================
    // Summary
    // ========================================================================

    std::cout << ansi::BOLD;
    std::cout << "  ════════════════════════════════════════════════════════════════\n";
    std::cout << "   4.0-legacy-beta — Branch Created\n";
    std::cout << "   Kernel lineage:   " << LINEAGE_COUNT << " checkpoints (v0.1 → 4.0-LB)\n";
    std::cout << "   Scale registry:   " << SCALE_COUNT << " scales (Å → m)\n";
    std::cout << "   Search targets:   " << SEARCH_TARGET_COUNT << " properties across "
              << sizeof(domains) / sizeof(domains[0]) << " domains\n";
    std::cout << "   Nuclear targets:  displacement energy, Frenkel pairs,\n";
    std::cout << "                     radiation tolerance, fission gas diffusion\n";
    if (active) {
        std::cout << "   Active core:      Z=" << static_cast<int>(active->Z)
                  << " (" << active->isotope << ", " << active->crystal_phase
                  << ", Ed=" << active->Ed_eV << " eV)\n";
    }
    std::cout << "  ════════════════════════════════════════════════════════════════\n";
    std::cout << ansi::RESET << "\n";

    return 0;
}
