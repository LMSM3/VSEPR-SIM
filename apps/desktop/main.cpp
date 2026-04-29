/**
 * VSEPR Desktop — Qt-based molecular workstation
 *
 * Architecture:
 *   Qt Widgets shell (menus, toolbars, dock panels, status bar)
 *   QOpenGLWidget viewport (reuses existing renderer pipeline)
 *   atomistic core engine (unchanged)
 *
 * The GLFW/ImGui stack is NOT used here. It remains available for
 * the lightweight --viz post-computation viewer.
 *
 * Boot sequence:
 *   1. print MOTD banner
 *   2. run hardware check
 *   3. initialize convenience infrastructure
 *   4. summarize status
 *   5. if FAIL, stop or fall back
 *   6. continue into desktop environment
 */

#include <QApplication>
#include "MainWindow.h"
#include "infra/bootstrap_probe.hpp"
#include "infra/motd.hpp"
#include "infra/nvidia_tui.hpp"

int main(int argc, char* argv[])
{
    using namespace vsepr::infra;

    const std::string mode    = "Desktop";
    const std::string version = "v3.0.1";

    // --- VSEPR-MOTD bootstrap probe ---
    print_motd_banner(mode, version);

    HardwareReport   hw   = run_hardware_check(/*need_gui=*/true, /*need_cuda=*/false);
    BootstrapReport  boot = initialize_infrastructure();
    MotdReport       report = combine_reports(std::move(hw), std::move(boot));

    print_motd_report(report);
    print_boot_exit(report, mode);

    // --- Secret key window (2s, no visible prompt) ---
    char secret = check_secret_keys(2000);
    if (secret != '\0') {
        dispatch_secret_key(secret);
    }

    if (report.overall == StatusLevel::Fail) {
        return 1;
    }

    // --- Enter Qt desktop environment ---
    QApplication app(argc, argv);
    app.setApplicationName("VSEPR");
    app.setOrganizationName("VSEPR-Sim");
    app.setApplicationVersion("3.0.1");

    // Default surface format for the whole application
    QSurfaceFormat fmt;
    fmt.setVersion(3, 3);
    fmt.setProfile(QSurfaceFormat::CoreProfile);
    fmt.setSamples(4);
    fmt.setDepthBufferSize(24);
    QSurfaceFormat::setDefaultFormat(fmt);

    MainWindow w;
    w.show();

    return app.exec();
}
