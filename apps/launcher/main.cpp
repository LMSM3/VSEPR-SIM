// VSEPR-SIM vsepr-launcher — entry point
//
// Usage:
//   vsepr-launcher                    (open empty launcher)
//   vsepr-launcher path/to/file.vsim  (open with file pre-loaded)
//   vsepr-launcher path/to/file.x     (open with file pre-loaded)
//
// The launcher is the Qt Lite double-click handler for .vsim and .x files.
// It has NO OpenGL dependency and does NOT replace the CLI or vsepr-desktop.

#include "LauncherWindow.h"

#include <QApplication>
#include <QFileInfo>
#include <QMessageBox>
#include <QString>

int main(int argc, char* argv[])
{
	QApplication app(argc, argv);
	app.setApplicationName("VSEPR Launcher");
	app.setApplicationVersion("5.0.14");
	app.setOrganizationName("VSEPR-SIM");

	// Validate any provided file argument.
	QString filePath;
	if (argc >= 2) {
		filePath = QString::fromLocal8Bit(argv[1]);
		QFileInfo fi(filePath);

		if (!fi.exists()) {
			QMessageBox::critical(nullptr, "VSEPR Launcher",
				QString("File not found:\n%1").arg(filePath));
			return 1;
		}

		QString ext = fi.suffix().toLower();
		if (ext != "vsim" && ext != "x") {
			// Warn but continue — user may have unusual extension.
			QMessageBox::warning(nullptr, "VSEPR Launcher",
				QString("Unrecognised file type: .%1\n"
						"Expected .vsim or .x — opening anyway.").arg(ext));
		}
	}

	LauncherWindow window(filePath);
	window.show();

	return app.exec();
}
