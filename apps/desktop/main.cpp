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
 */

#include <QApplication>
#include "MainWindow.h"

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("VSEPR");
    app.setOrganizationName("VSEPR-Sim");
    app.setApplicationVersion("3.0.0-dev");

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
