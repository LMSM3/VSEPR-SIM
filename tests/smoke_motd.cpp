/**
 * smoke_motd.cpp — quick standalone test of the bootstrap probe
 * Build: linked against vsepr_infra
 * Purpose: verify shell-out GPU detection + full MOTD output on real hardware
 */
#include "infra/bootstrap_probe.hpp"
#include "infra/motd.hpp"

int main() {
    using namespace vsepr::infra;

    print_motd_banner("Smoke Test", "v3.0.1");

    HardwareReport   hw   = run_hardware_check(/*need_gui=*/false, /*need_cuda=*/false);
    BootstrapReport  boot = initialize_infrastructure();
    MotdReport       report = combine_reports(std::move(hw), std::move(boot));

    print_motd_report(report);
    print_boot_exit(report, "Smoke Test");

    return (report.overall == StatusLevel::Fail) ? 1 : 0;
}
