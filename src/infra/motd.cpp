/**
 * motd.cpp
 * --------
 * VSEPR-MOTD printing implementation.
 * Outputs the banner, [HW], [INFRA], and [BOOT] sections.
 */

#include "infra/motd.hpp"

#include <iostream>
#include <iomanip>

namespace vsepr {
namespace infra {

namespace {

// ANSI helpers (match cli/display.hpp conventions)
const char* COL_RESET  = "\033[0m";
const char* COL_CYAN   = "\033[0;36m";
const char* COL_GREEN  = "\033[0;32m";
const char* COL_YELLOW = "\033[1;33m";
const char* COL_RED    = "\033[0;31m";
const char* COL_BOLD   = "\033[1m";

const char* status_color(StatusLevel s) {
    switch (s) {
        case StatusLevel::Pass: return COL_GREEN;
        case StatusLevel::Warn: return COL_YELLOW;
        case StatusLevel::Fail: return COL_RED;
    }
    return COL_RESET;
}

void print_entries(const char* tag,
                   const std::vector<DiagEntry>& entries) {
    for (auto& e : entries) {
        const char* col = status_color(e.level);
        std::cout << col << "[" << tag << "] "
                  << std::left << std::setw(20) << e.key
                  << ": " << e.value
                  << COL_RESET;
        if (!e.reason.empty()) {
            std::cout << "\n" << col << "[" << tag << "] "
                      << std::setw(20) << "Reason"
                      << ": " << e.reason << COL_RESET;
        }
        std::cout << "\n";
    }
}

} // anonymous namespace

// ============================================================================
// print_motd_banner
// ============================================================================

void print_motd_banner(const std::string& mode, const std::string& version) {
    std::cout
        << COL_CYAN << COL_BOLD
        << "========================================================\n"
        << " VSEPR-SIM :: MOTD\n"
        << " Deterministic formation engine bootstrap\n"
        << " Mode: " << mode << "\n"
        << " Version: " << version << "\n"
        << "========================================================\n"
        << COL_RESET << "\n";
}

// ============================================================================
// print_motd_report
// ============================================================================

void print_motd_report(const MotdReport& report) {
    print_entries("HW",    report.hardware.entries);
    std::cout << "\n";
    print_entries("INFRA", report.bootstrap.entries);
    std::cout << "\n";

    // Overall
    const char* col = status_color(report.overall);
    std::cout << col
              << "[BOOT] " << std::left << std::setw(20) << "Overall status"
              << ": " << status_label(report.overall)
              << COL_RESET << "\n";
}

// ============================================================================
// print_boot_exit
// ============================================================================

void print_boot_exit(const MotdReport& report, const std::string& mode) {
    if (report.overall == StatusLevel::Fail) {
        std::cout << COL_RED
                  << "[BOOT] Startup failed. Cannot enter " << mode
                  << " environment.\n"
                  << COL_RESET;
    } else {
        std::cout << COL_GREEN
                  << "[BOOT] Entering " << mode << " environment...\n"
                  << COL_RESET;
    }
    std::cout << "\n";
}

} // namespace infra
} // namespace vsepr
