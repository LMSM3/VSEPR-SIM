// smoke_tui.cpp — verify the NVIDIA TUI draws correctly
#include "infra/nvidia_tui.hpp"
#include "infra/bootstrap_probe.hpp"
#include "infra/motd.hpp"
#include <iostream>

int main() {
    using namespace vsepr::infra;

    print_motd_banner("TUI Smoke Test", "v3.0.1");

    HardwareReport   hw   = run_hardware_check(false, false);
    BootstrapReport  boot = initialize_infrastructure();
    MotdReport       report = combine_reports(std::move(hw), std::move(boot));

    print_motd_report(report);
    print_boot_exit(report, "TUI Smoke Test");

    // Jump straight into the TUI (skip the secret key wait)
    std::cout << "Launching NVIDIA TUI in 1 second...\n" << std::flush;
    enter_nvidia_tui();

    std::cout << "TUI exited. Boot continues normally.\n";
    return 0;
}
