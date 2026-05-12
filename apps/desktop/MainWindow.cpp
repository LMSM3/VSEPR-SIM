#include "MainWindow.h"
#include <QApplication>
#include <QFileInfo>
#include <limits>
#include <cmath>

// ============================================================================
// Construction
// ============================================================================

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setWindowTitle("VSEPR — Molecular Workstation");
    resize(1600, 1000);
    setMinimumSize(960, 600);

    // Central viewport (OpenGL)
    viewport_ = new ViewportWidget(this);
    setCentralWidget(viewport_);

    createActions();
    createMenus();
    createToolBars();
    createDockWidgets();
    createStatusBar();
    applyTheme();
    loadSettings();
}

MainWindow::~MainWindow() = default;

// ============================================================================
// Actions
// ============================================================================

void MainWindow::createActions()
{
    // --- File ---
    openAct_ = new QAction(tr("&Open XYZ…"), this);
    openAct_->setShortcut(QKeySequence::Open);
    openAct_->setStatusTip(tr("Open an XYZ / XYZA / XYZC file"));
    connect(openAct_, &QAction::triggered, this, &MainWindow::onFileOpen);

    saveAct_ = new QAction(tr("&Save XYZ…"), this);
    saveAct_->setShortcut(QKeySequence::Save);
    connect(saveAct_, &QAction::triggered, this, &MainWindow::onFileSave);

    exportImageAct_ = new QAction(tr("Export &Image…"), this);
    exportImageAct_->setShortcut(tr("Ctrl+Shift+E"));
    connect(exportImageAct_, &QAction::triggered, this, &MainWindow::onFileExportImage);

    exitAct_ = new QAction(tr("E&xit"), this);
    exitAct_->setShortcut(QKeySequence::Quit);
    connect(exitAct_, &QAction::triggered, this, &QWidget::close);

    // --- Simulation ---
    singlePointAct_ = new QAction(tr("Single &Point"), this);
    singlePointAct_->setShortcut(tr("F4"));
    singlePointAct_->setStatusTip(tr("Evaluate energy & forces (no motion)"));
    connect(singlePointAct_, &QAction::triggered, this, &MainWindow::onSinglePoint);

    relaxAct_ = new QAction(tr("FIRE &Relax"), this);
    relaxAct_->setShortcut(tr("F5"));
    relaxAct_->setStatusTip(tr("Energy minimization (FIRE algorithm)"));
    connect(relaxAct_, &QAction::triggered, this, &MainWindow::onRunRelax);

    mdAct_ = new QAction(tr("Run &MD"), this);
    mdAct_->setShortcut(tr("F6"));
    mdAct_->setStatusTip(tr("Molecular dynamics (NVT Langevin)"));
    connect(mdAct_, &QAction::triggered, this, &MainWindow::onRunMD);

    // --- View ---
    resetCameraAct_ = new QAction(tr("&Reset Camera"), this);
    resetCameraAct_->setShortcut(tr("R"));
    resetCameraAct_->setStatusTip(tr("Reset camera to default orbit position"));
    connect(resetCameraAct_, &QAction::triggered, this, &MainWindow::onResetCamera);

    fitCameraAct_ = new QAction(tr("&Fit Camera"), this);
    fitCameraAct_->setShortcut(tr("F"));
    fitCameraAct_->setStatusTip(tr("Fit camera to molecule bounding sphere"));
    connect(fitCameraAct_, &QAction::triggered, this, &MainWindow::onFitCamera);

    wireframeAct_ = new QAction(tr("&Wireframe"), this);
    wireframeAct_->setCheckable(true);
    wireframeAct_->setShortcut(tr("W"));
    connect(wireframeAct_, &QAction::triggered, this, &MainWindow::onToggleWireframe);

    // --- Screenshot (quick capture) ---
    screenshotAct_ = new QAction(tr("&Screenshot"), this);
    screenshotAct_->setShortcut(tr("Ctrl+Shift+S"));
    screenshotAct_->setStatusTip(tr("Save viewport screenshot to file"));
    connect(screenshotAct_, &QAction::triggered, this, &MainWindow::onScreenshot);

    // --- Console ---
    clearConsoleAct_ = new QAction(tr("&Clear Console"), this);
    clearConsoleAct_->setShortcut(tr("Ctrl+L"));
    connect(clearConsoleAct_, &QAction::triggered, this, &MainWindow::onClearConsole);
}

// ============================================================================
// Menus
// ============================================================================

void MainWindow::createMenus()
{
    QMenu* fileMenu = menuBar()->addMenu(tr("&File"));
    fileMenu->addAction(openAct_);
    fileMenu->addAction(saveAct_);
    fileMenu->addSeparator();
    fileMenu->addAction(exportImageAct_);
    fileMenu->addAction(screenshotAct_);
    fileMenu->addSeparator();
    fileMenu->addAction(exitAct_);

    QMenu* simMenu = menuBar()->addMenu(tr("&Simulation"));
    simMenu->addAction(singlePointAct_);
    simMenu->addAction(relaxAct_);
    simMenu->addAction(mdAct_);

    QMenu* viewMenu = menuBar()->addMenu(tr("&View"));
    viewMenu->addAction(resetCameraAct_);
    viewMenu->addAction(fitCameraAct_);
    viewMenu->addAction(wireframeAct_);
    viewMenu->addSeparator();
    // Dock visibility toggles are added below after docks are created

    QMenu* consoleMenu = menuBar()->addMenu(tr("&Console"));
    consoleMenu->addAction(clearConsoleAct_);
}

// ============================================================================
// Toolbars
// ============================================================================

void MainWindow::createToolBars()
{
    QToolBar* mainBar = addToolBar(tr("Main"));
    mainBar->setObjectName("MainToolBar");
    mainBar->setMovable(false);
    mainBar->addAction(openAct_);
    mainBar->addAction(saveAct_);
    mainBar->addSeparator();
    mainBar->addAction(singlePointAct_);
    mainBar->addAction(relaxAct_);
    mainBar->addAction(mdAct_);
    mainBar->addSeparator();
    mainBar->addAction(resetCameraAct_);
    mainBar->addAction(fitCameraAct_);
    mainBar->addAction(wireframeAct_);
    mainBar->addSeparator();
    mainBar->addAction(screenshotAct_);
}

// ============================================================================
// Dock Widgets
// ============================================================================

void MainWindow::createDockWidgets()
{
    // --- Object tree (left) ---
    objectTree_ = new ObjectTree(this);
    objectDock_ = new QDockWidget(tr("Objects"), this);
    objectDock_->setObjectName("ObjectDock");
    objectDock_->setWidget(objectTree_);
    objectDock_->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    addDockWidget(Qt::LeftDockWidgetArea, objectDock_);

    // --- Properties inspector (right) ---
    propertiesPanel_ = new PropertiesPanel(this);
    propertiesDock_ = new QDockWidget(tr("Properties"), this);
    propertiesDock_->setObjectName("PropertiesDock");
    propertiesDock_->setWidget(propertiesPanel_);
    propertiesDock_->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    addDockWidget(Qt::RightDockWidgetArea, propertiesDock_);

    // --- Console / log (bottom) ---
    consolePanel_ = new ConsolePanel(this);
    consoleDock_ = new QDockWidget(tr("Console"), this);
    consoleDock_->setObjectName("ConsoleDock");
    consoleDock_->setWidget(consolePanel_);
    consoleDock_->setAllowedAreas(Qt::BottomDockWidgetArea | Qt::TopDockWidgetArea);
    addDockWidget(Qt::BottomDockWidgetArea, consoleDock_);

    connect(consolePanel_, &ConsolePanel::commandSubmitted,
            this, &MainWindow::onCommand);

    // Add dock toggle actions to View menu
    QMenu* viewMenu = nullptr;
    for (auto* m : menuBar()->findChildren<QMenu*>()) {
        if (m->title() == tr("&View")) { viewMenu = m; break; }
    }
    if (viewMenu) {
        viewMenu->addAction(objectDock_->toggleViewAction());
        viewMenu->addAction(propertiesDock_->toggleViewAction());
        viewMenu->addAction(consoleDock_->toggleViewAction());
    }
}

// ============================================================================
// Status Bar
// ============================================================================

void MainWindow::createStatusBar()
{
    statusBar()->showMessage(tr("Ready"));
}

// ============================================================================
// Theme — neutral, workspace-first
// ============================================================================

void MainWindow::applyTheme()
{
    // Neutral dark-gray palette suitable for CAD / scientific workstation.
    // Viewport background is controlled separately in ViewportWidget.
    qApp->setStyleSheet(R"(
        QMainWindow {
            background: #2b2b2b;
        }
        QMenuBar {
            background: #323232;
            color: #d4d4d4;
        }
        QMenuBar::item:selected {
            background: #3d6fa5;
        }
        QMenu {
            background: #2b2b2b;
            color: #d4d4d4;
            border: 1px solid #3a3a3a;
        }
        QMenu::item:selected {
            background: #3d6fa5;
        }
        QToolBar {
            background: #323232;
            border: none;
            spacing: 4px;
            padding: 2px;
        }
        QToolBar QToolButton {
            color: #d4d4d4;
            padding: 4px 8px;
            border-radius: 3px;
        }
        QToolBar QToolButton:hover {
            background: #3a3a3a;
        }
        QToolBar QToolButton:pressed {
            background: #3d6fa5;
        }
        QDockWidget {
            color: #d4d4d4;
            titlebar-close-icon: none;
        }
        QDockWidget::title {
            background: #323232;
            padding: 6px;
            text-align: left;
        }
        QTreeWidget, QTextEdit, QLineEdit, QComboBox {
            background: #1e1e1e;
            color: #d4d4d4;
            border: 1px solid #3a3a3a;
            selection-background-color: #3d6fa5;
        }
        QGroupBox {
            color: #9a9a9a;
            border: 1px solid #3a3a3a;
            margin-top: 12px;
            padding-top: 8px;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            left: 10px;
        }
        QLabel {
            color: #b0b0b0;
        }
        QPushButton {
            background: #3a3a3a;
            color: #d4d4d4;
            border: 1px solid #4a4a4a;
            border-radius: 3px;
            padding: 4px 14px;
        }
        QPushButton:hover {
            background: #454545;
        }
        QPushButton:pressed {
            background: #3d6fa5;
        }
        QStatusBar {
            background: #252525;
            color: #808080;
        }
        QSplitter::handle {
            background: #3a3a3a;
        }
    )");
}

// ============================================================================
// Persistence
// ============================================================================

void MainWindow::saveSettings()
{
    QSettings s("VSEPR-Sim", "Desktop");
    s.setValue("geometry", saveGeometry());
    s.setValue("windowState", saveState());
}

void MainWindow::loadSettings()
{
    QSettings s("VSEPR-Sim", "Desktop");
    restoreGeometry(s.value("geometry").toByteArray());
    restoreState(s.value("windowState").toByteArray());
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    saveSettings();
    event->accept();
}

// ============================================================================
// Slot implementations — all go through EngineAdapter
// ============================================================================

void MainWindow::openVsimPath(const QString& path)
{
    if (path.isEmpty()) return;
    setWindowTitle(tr("VSEPR — %1").arg(QFileInfo(path).fileName()));
    consolePanel_->log(tr("Opening .vsim script: %1").arg(path));
    statusBar()->showMessage(tr("Loaded: %1").arg(path));
    // Run as a LoadXYZ pass for now so the viewport opens with the script name.
    // Full .vsim interpreter integration is wired in a future sprint.
    consolePanel_->log(tr("Script ready. Use Simulation menu to run."));
}

void MainWindow::onFileOpen()
{
    QString path = QFileDialog::getOpenFileName(
        this, tr("Open Structure"),
        QString(),
        tr("XYZ Files (*.xyz *.xyza *.xyzc);;All Files (*)"));

    if (path.isEmpty()) return;

    consolePanel_->log("Loading " + path + " …");

    bridge::KernelRequest req;
    req.op = bridge::KernelOp::LoadXYZ;
    req.file_path = path.toStdString();

    auto result = engine_.run(req);

    if (result.success && result.output) {
        doc_ = result.output;
        viewport_->setDocument(doc_);
        syncPanels();
        consolePanel_->log(QString::fromStdString(result.message));
        statusBar()->showMessage(tr("Loaded: %1").arg(path));
    } else {
        consolePanel_->logError(QString::fromStdString(result.message));
    }
}

void MainWindow::onFileSave()
{
    if (!doc_ || doc_->empty()) {
        consolePanel_->logError("Nothing to save");
        return;
    }

    QString path = QFileDialog::getSaveFileName(
        this, tr("Save Structure"),
        QString(),
        tr("XYZ Files (*.xyz);;All Files (*)"));

    if (path.isEmpty()) return;

    bridge::KernelRequest req;
    req.op = bridge::KernelOp::SaveXYZ;
    req.file_path = path.toStdString();
    req.input = doc_;

    auto result = engine_.run(req);
    if (result.success) {
        consolePanel_->log(QString::fromStdString(result.message));
        statusBar()->showMessage(tr("Saved: %1").arg(path));
    } else {
        consolePanel_->logError(QString::fromStdString(result.message));
    }
}

void MainWindow::onFileExportImage()
{
    QString path = QFileDialog::getSaveFileName(
        this, tr("Export Image"),
        "screenshot.png",
        tr("PNG (*.png);;JPEG (*.jpg *.jpeg);;All Files (*)"));

    if (path.isEmpty()) return;

    if (viewport_->grabScreenshot(path)) {
        consolePanel_->logResult(tr("Image exported: %1").arg(path));
        statusBar()->showMessage(tr("Exported: %1").arg(path));
    } else {
        consolePanel_->logError(tr("Failed to write image: %1").arg(path));
    }
}

void MainWindow::onScreenshot()
{
    QString path = QFileDialog::getSaveFileName(
        this, tr("Save Screenshot"),
        "screenshot.png",
        tr("PNG (*.png);;All Files (*)"));

    if (path.isEmpty()) return;

    if (viewport_->grabScreenshot(path)) {
        consolePanel_->logResult(tr("Screenshot saved: %1").arg(path));
        statusBar()->showMessage(tr("Screenshot: %1").arg(path));
    } else {
        consolePanel_->logError(tr("Screenshot failed: %1").arg(path));
    }
}

void MainWindow::onSinglePoint()
{
    if (!doc_ || doc_->empty()) {
        consolePanel_->logError("No structure loaded");
        return;
    }

    consolePanel_->log("Running single-point energy evaluation\xe2\x80\xa6");
    statusBar()->showMessage(tr("Evaluating\xe2\x80\xa6"));

    bridge::KernelRequest req;
    req.op = bridge::KernelOp::SinglePoint;
    req.input = doc_;

    auto result = engine_.run(req);
    if (result.success && result.output) {
        doc_ = result.output;
        viewport_->setDocument(doc_);
        syncPanels();
        consolePanel_->logResult(QString::fromStdString(result.message));
        statusBar()->showMessage(tr("Single-point complete"));
    } else {
        consolePanel_->logError(QString::fromStdString(result.message));
        statusBar()->showMessage(tr("Single-point failed"));
    }
}

void MainWindow::onRunRelax()
{
    if (!doc_ || doc_->empty()) {
        consolePanel_->logError("No structure loaded");
        return;
    }

    consolePanel_->log("Starting FIRE minimization…");
    statusBar()->showMessage(tr("Relaxing…"));

    bridge::KernelRequest req;
    req.op = bridge::KernelOp::Relax;
    req.input = doc_;
    req.max_steps = 2000;
    req.force_tol = 1e-4;

    auto result = engine_.run(req);
    if (result.success && result.output) {
        doc_ = result.output;
        viewport_->setDocument(doc_);
        syncPanels();
        consolePanel_->logResult(QString::fromStdString(result.message));
        statusBar()->showMessage(tr("Relaxation complete"));
    } else {
        consolePanel_->logError(QString::fromStdString(result.message));
        statusBar()->showMessage(tr("Relaxation failed"));
    }
}

void MainWindow::onRunMD()
{
    if (!doc_ || doc_->empty()) {
        consolePanel_->logError("No structure loaded");
        return;
    }

    consolePanel_->log("Starting Langevin MD (NVT, 300 K)…");
    statusBar()->showMessage(tr("Running MD…"));

    bridge::KernelRequest req;
    req.op = bridge::KernelOp::MD_NVT;
    req.input = doc_;
    req.max_steps = 1000;
    req.dt = 1.0;
    req.temperature = 300.0;

    auto result = engine_.run(req);
    if (result.success && result.output) {
        doc_ = result.output;
        viewport_->setDocument(doc_);
        syncPanels();
        consolePanel_->logResult(QString::fromStdString(result.message));
        statusBar()->showMessage(tr("MD complete"));
    } else {
        consolePanel_->logError(QString::fromStdString(result.message));
        statusBar()->showMessage(tr("MD failed"));
    }
}

void MainWindow::onResetCamera()
{
    viewport_->resetCamera();
}

void MainWindow::onFitCamera()
{
    viewport_->fitCamera();
}

void MainWindow::onToggleWireframe()
{
    bool wf = wireframeAct_->isChecked();
    viewport_->setWireframe(wf);
    statusBar()->showMessage(wf ? tr("Wireframe on") : tr("Wireframe off"), 2000);
}

void MainWindow::onClearConsole()
{
    consolePanel_->clearLog();
}

void MainWindow::onCommand(const QString& cmd)
{
    QString c = cmd.trimmed().toLower();

    if (c == "relax" || c == "fire") {
        onRunRelax();
    } else if (c == "md" || c == "nvt") {
        onRunMD();
    } else if (c == "sp" || c == "energy" || c == "single") {
        onSinglePoint();
    } else if (c == "reset") {
        onResetCamera();
    } else if (c == "fit" || c == "zoom") {
        onFitCamera();
    } else if (c == "wireframe" || c == "wire") {
        wireframeAct_->setChecked(!wireframeAct_->isChecked());
        onToggleWireframe();
    } else if (c == "screenshot" || c == "ss") {
        onScreenshot();
    } else if (c == "clear" || c == "cls") {
        onClearConsole();
    } else if (c == "help") {
        consolePanel_->log("Commands:");
        consolePanel_->log("  relax / fire   — FIRE energy minimization");
        consolePanel_->log("  md / nvt       — Langevin MD (NVT, 300 K)");
        consolePanel_->log("  sp / energy    — single-point evaluation");
        consolePanel_->log("  reset          — reset camera to default");
        consolePanel_->log("  fit / zoom     — fit camera to molecule");
        consolePanel_->log("  wireframe/wire — toggle wireframe mode");
        consolePanel_->log("  screenshot/ss  — save viewport image");
        consolePanel_->log("  clear / cls    — clear this console");
    } else {
        consolePanel_->logError("Unknown command: " + cmd);
    }
}

// ============================================================================
// Panel synchronization
// ============================================================================

void MainWindow::syncPanels()
{
    if (!doc_ || doc_->empty()) {
        propertiesPanel_->clearAll();
        return;
    }

    const int totalFrames = static_cast<int>(doc_->frames.size());
    const int frameIdx = totalFrames - 1;
    const auto& f = doc_->current_frame();

    // --- Identity ---
    if (!doc_->provenance.formula.empty())
        propertiesPanel_->setFormula(
            QString::fromStdString(doc_->provenance.formula));
    propertiesPanel_->setAtomCount(f.atom_count());
    propertiesPanel_->setBondCount(f.bond_count());

    // --- Frame provenance ---
    propertiesPanel_->setFrameIndex(frameIdx, totalFrames);
    propertiesPanel_->setStep(f.step);
    propertiesPanel_->setTime(f.time);

    // --- Energy decomposition ---
    auto getE = [&](const std::string& key) -> double {
        auto it = f.properties.find(key);
        return (it != f.properties.end()) ? it->second
                                          : std::numeric_limits<double>::quiet_NaN();
    };
    propertiesPanel_->setEnergyTotal(getE("energy_total"));
    propertiesPanel_->setEnergyBond(getE("energy_bond"));
    propertiesPanel_->setEnergyAngle(getE("energy_angle"));
    propertiesPanel_->setEnergyVdW(getE("energy_vdw"));
    propertiesPanel_->setEnergyCoulomb(getE("energy_coul"));
    propertiesPanel_->setEnergyPol(getE("energy_pol"));

    // --- Forces ---
    propertiesPanel_->setForceRMS(getE("force_rms"));

    // --- Status bar: live energy ---
    double eTot = getE("energy_total");
    if (!std::isnan(eTot)) {
        statusBar()->showMessage(
            tr("E = %1 kcal/mol").arg(eTot, 0, 'f', 4));
    }

    // --- Object tree ---
    objectTree_->clear();
    QString name = QString::fromStdString(
        doc_->provenance.source_file.empty()
            ? doc_->provenance.mode
            : doc_->provenance.source_file);
    objectTree_->addMolecule(name, f.atom_count(), f.bond_count());
}
