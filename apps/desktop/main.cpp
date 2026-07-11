/**
 * VSEPR Desktop  -  Qt-based molecular workstation
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
#include <QTimer>
#include "MainWindow.h"
#include "infra/bootstrap_probe.hpp"
#include "infra/motd.hpp"
#include "infra/nvidia_tui.hpp"

int main(int argc, char* argv[])
{
    using namespace vsepr::infra;

    const std::string mode    = "Desktop";
    const std::string version = "v5.0.14";

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

    // -- Parse command-line open-file arguments --------------------------------
    // Supported forms:
    //   --vsim  <path>       open .vsim script in editor
    //   --open-dynx <path>   open .dynx replay archive
    //   --open-bundle <path> open .x bundle (alias --bundle)
    //   --import-xyz <path>  import .xyz / .xyzFull (alias --xyz)
    //   --open-panel <name>  raise a named dock panel
    //   --showroom           enter showroom / admin debug mode
    //   bare positional args: auto-detected by extension
    QString vsimPath, dynxPath, xyzPath, openPanel;
    bool showroom = false;

    for (int i = 1; i < argc; ++i) {
        QString a = QString::fromLocal8Bit(argv[i]);
        auto nextArg = [&]() -> QString {
            return (i + 1 < argc) ? QString::fromLocal8Bit(argv[++i]) : QString{};
        };

        if ((a == "--vsim" || a == "-vsim")) {
            vsimPath = nextArg();
        } else if ((a == "--open-dynx" || a == "-open-dynx")) {
            dynxPath = nextArg();
        } else if ((a == "--open-bundle" || a == "--bundle")) {
            vsimPath = nextArg();   // bundles open via the same vsim path handler for now
        } else if ((a == "--import-xyz" || a == "--xyz")) {
            xyzPath = nextArg();
        } else if (a == "--open-panel") {
            openPanel = nextArg();
        } else if (a == "--showroom" || a == "-showroom") {
            showroom = true;
        } else if (!a.startsWith(QLatin1Char('-'))) {
            // Positional: auto-detect by extension
            if (a.endsWith(QStringLiteral(".vsim"), Qt::CaseInsensitive))
                vsimPath = a;
            else if (a.endsWith(QStringLiteral(".dynx"), Qt::CaseInsensitive))
                dynxPath = a;
            else if (a.endsWith(QStringLiteral(".x"), Qt::CaseInsensitive))
                vsimPath = a;   // bundle handled as vsim path
            else if (a.endsWith(QStringLiteral(".xyz"), Qt::CaseInsensitive)  ||
                     a.endsWith(QStringLiteral(".xyza"), Qt::CaseInsensitive) ||
                     a.endsWith(QStringLiteral(".xyzfull"), Qt::CaseInsensitive))
                xyzPath = a;
        }
    }

    if (!vsimPath.isEmpty()) {
        QTimer::singleShot(0, [&w, vsimPath]() { w.openVsimPath(vsimPath); });
    }
    if (!dynxPath.isEmpty()) {
        QTimer::singleShot(10, [&w, dynxPath]() { w.openDynxPath(dynxPath); });
    }
    if (!xyzPath.isEmpty()) {
        QTimer::singleShot(20, [&w, xyzPath]() { w.importXyzPath(xyzPath); });
    }

    // Always start the IPC server so the CLI can reach this instance
    quint16 ipcPort = w.startIpcServer();
    Q_UNUSED(ipcPort)

    if (showroom) {
        QTimer::singleShot(50, [&w]() { w.enterShowroomMode(); });
    }
    if (!openPanel.isEmpty()) {
        QTimer::singleShot(60, [&w, openPanel]() { w.openPanelByName(openPanel); });
    }

    return app.exec();
}
