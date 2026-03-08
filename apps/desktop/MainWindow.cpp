#include "MainWindow.h"
#include <QApplication>

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
    connect(resetCameraAct_, &QAction::triggered, this, &MainWindow::onResetCamera);

    fitCameraAct_ = new QAction(tr("&Fit to Structure"), this);
    fitCameraAct_->setShortcut(tr("F"));
    connect(fitCameraAct_, &QAction::triggered, this, &MainWindow::onFitCamera);

    wireframeAct_ = new QAction(tr("&Wireframe"), this);
    wireframeAct_->setCheckable(true);
    wireframeAct_->setShortcut(tr("W"));
    connect(wireframeAct_, &QAction::triggered, this, &MainWindow::onToggleWireframe);

    // --- Render mode (exclusive) ---
    renderModeGroup_ = new QActionGroup(this);
    renderBallStickAct_ = new QAction(tr("Ball && Stick"), this);
    renderBallStickAct_->setCheckable(true); renderBallStickAct_->setChecked(true);
    renderSpaceFillAct_ = new QAction(tr("Space Fill (CPK)"), this);
    renderSpaceFillAct_->setCheckable(true);
    renderSticksAct_    = new QAction(tr("Sticks"), this);
    renderSticksAct_->setCheckable(true);
    renderWireframeAct_ = new QAction(tr("Wireframe"), this);
    renderWireframeAct_->setCheckable(true);
    for (auto* a : {renderBallStickAct_, renderSpaceFillAct_,
                    renderSticksAct_, renderWireframeAct_}) {
        renderModeGroup_->addAction(a);
        connect(a, &QAction::triggered, this, &MainWindow::onRenderModeChanged);
    }

    // --- Overlays ---
    showLabelsAct_ = new QAction(tr("Element &Labels"), this);
    showLabelsAct_->setCheckable(true); showLabelsAct_->setShortcut(tr("L"));
    connect(showLabelsAct_, &QAction::toggled, this, &MainWindow::onShowLabels);

    showBoxAct_ = new QAction(tr("Unit Cell &Box"), this);
    showBoxAct_->setCheckable(true); showBoxAct_->setChecked(true); showBoxAct_->setShortcut(tr("B"));
    connect(showBoxAct_, &QAction::toggled, this, &MainWindow::onShowBox);

    showAxesAct_ = new QAction(tr("&Axes Indicator"), this);
    showAxesAct_->setCheckable(true); showAxesAct_->setChecked(true); showAxesAct_->setShortcut(tr("A"));
    connect(showAxesAct_, &QAction::toggled, this, &MainWindow::onShowAxes);

    // --- Crystal preset ---
    crystalPresetAct_ = new QAction(tr("&Crystal Preset…"), this);
    crystalPresetAct_->setShortcut(tr("Ctrl+K"));
    connect(crystalPresetAct_, &QAction::triggered, this, &MainWindow::onCrystalPreset);

    // --- Trajectory ---
    stepBackAct_ = new QAction(tr("◀ Prev Frame"), this);
    stepBackAct_->setShortcut(Qt::Key_Left);
    connect(stepBackAct_, &QAction::triggered, this, &MainWindow::onStepBack);

    stepFwdAct_  = new QAction(tr("Next Frame ▶"), this);
    stepFwdAct_->setShortcut(Qt::Key_Right);
    connect(stepFwdAct_, &QAction::triggered, this, &MainWindow::onStepForward);

    playAct_ = new QAction(tr("▶ Play"), this);
    playAct_->setShortcut(Qt::Key_Space);
    connect(playAct_, &QAction::triggered, this, &MainWindow::onPlayPause);
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
    fileMenu->addSeparator();
    fileMenu->addAction(exitAct_);

    QMenu* simMenu = menuBar()->addMenu(tr("&Simulation"));
    simMenu->addAction(singlePointAct_);
    simMenu->addAction(relaxAct_);
    simMenu->addAction(mdAct_);
    simMenu->addSeparator();
    simMenu->addAction(crystalPresetAct_);

    QMenu* viewMenu = menuBar()->addMenu(tr("&View"));
    viewMenu->addAction(resetCameraAct_);
    viewMenu->addAction(fitCameraAct_);
    viewMenu->addSeparator();
    QMenu* renderSubMenu = viewMenu->addMenu(tr("Render &Mode"));
    renderSubMenu->addAction(renderBallStickAct_);
    renderSubMenu->addAction(renderSpaceFillAct_);
    renderSubMenu->addAction(renderSticksAct_);
    renderSubMenu->addAction(renderWireframeAct_);
    viewMenu->addSeparator();
    viewMenu->addAction(showLabelsAct_);
    viewMenu->addAction(showBoxAct_);
    viewMenu->addAction(showAxesAct_);
    // Dock toggles added after docks are created
}

// ============================================================================
// Toolbars
// ============================================================================

void MainWindow::createToolBars()
{
    // --- Main toolbar ---
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
    mainBar->addSeparator();
    mainBar->addAction(renderBallStickAct_);
    mainBar->addAction(renderSpaceFillAct_);
    mainBar->addAction(renderSticksAct_);
    mainBar->addAction(renderWireframeAct_);
    mainBar->addSeparator();
    mainBar->addAction(showLabelsAct_);
    mainBar->addAction(showAxesAct_);

    // --- Trajectory toolbar ---
    trajectoryBar_ = addToolBar(tr("Trajectory"));
    trajectoryBar_->setObjectName("TrajectoryToolBar");
    trajectoryBar_->setMovable(false);
    trajectoryBar_->addAction(stepBackAct_);
    trajectoryBar_->addAction(playAct_);
    trajectoryBar_->addAction(stepFwdAct_);
    trajectoryBar_->addSeparator();

    frameSlider_ = new QSlider(Qt::Horizontal);
    frameSlider_->setMinimum(0);
    frameSlider_->setMaximum(0);
    frameSlider_->setFixedWidth(200);
    frameSlider_->setEnabled(false);
    connect(frameSlider_, &QSlider::valueChanged, this, &MainWindow::onFrameSliderMoved);
    trajectoryBar_->addWidget(frameSlider_);

    frameLabel_ = new QLabel(tr("  Frame — / —  "));
    frameLabel_->setMinimumWidth(130);
    trajectoryBar_->addWidget(frameLabel_);
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

    // Viewport signals
    connect(viewport_, &ViewportWidget::atomSelected,
            this, &MainWindow::onAtomSelected);
    connect(viewport_, &ViewportWidget::frameIndexChanged,
            this, &MainWindow::onFrameIndexChanged);

    // Object tree signals
    connect(objectTree_, &ObjectTree::itemActivated,
            [this](TreeItemType, const QString&, QTreeWidgetItem*) { /* future: switch viewport frame */ });

    // Add dock toggle actions to View menu
    for (auto* m : menuBar()->findChildren<QMenu*>()) {
        if (m->title() == tr("&View")) {
            m->addSeparator();
            m->addAction(objectDock_->toggleViewAction());
            m->addAction(propertiesDock_->toggleViewAction());
            m->addAction(consoleDock_->toggleViewAction());
            break;
        }
    }
}

// ============================================================================
// Status Bar
// ============================================================================

void MainWindow::createStatusBar()
{
    statusAtoms_  = new QLabel(tr("Atoms: —"));
    statusEnergy_ = new QLabel(tr("Energy: —"));
    statusFrame_  = new QLabel(tr("Frame: —"));
    statusAtoms_ ->setMinimumWidth(100);
    statusEnergy_->setMinimumWidth(160);
    statusFrame_ ->setMinimumWidth(100);
    statusBar()->addPermanentWidget(statusAtoms_);
    statusBar()->addPermanentWidget(statusEnergy_);
    statusBar()->addPermanentWidget(statusFrame_);
    statusBar()->showMessage(tr("Ready — Open an XYZ file to begin"));
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
        tr("PNG (*.png);;JPEG (*.jpg);;All Files (*)"));

    if (path.isEmpty()) return;

    QImage img = viewport_->grabFramebuffer();
    img.save(path);
    consolePanel_->log("Image exported: " + path);
    statusBar()->showMessage(tr("Image exported: %1").arg(path));
}

void MainWindow::onSinglePoint()
{
    if (!doc_ || doc_->empty()) {
        consolePanel_->logError("No structure loaded");
        return;
    }

    consolePanel_->log("Running single-point energy evaluation…");

    bridge::KernelRequest req;
    req.op = bridge::KernelOp::SinglePoint;
    req.input = doc_;

    auto result = engine_.run(req);
    if (result.success && result.output) {
        doc_ = result.output;
        viewport_->setDocument(doc_);
        syncPanels();
        consolePanel_->log(QString::fromStdString(result.message));
    } else {
        consolePanel_->logError(QString::fromStdString(result.message));
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
        consolePanel_->log(QString::fromStdString(result.message));
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
        consolePanel_->log(QString::fromStdString(result.message));
        statusBar()->showMessage(tr("MD complete"));
    } else {
        consolePanel_->logError(QString::fromStdString(result.message));
        statusBar()->showMessage(tr("MD failed"));
    }
}

void MainWindow::onResetCamera() { viewport_->resetCamera(); }
void MainWindow::onFitCamera()   { viewport_->fitCamera(); update(); }

void MainWindow::onToggleWireframe()
{
    viewport_->setRenderStyle(wireframeAct_->isChecked()
        ? RenderStyle::Wireframe : RenderStyle::BallAndStick);
}

void MainWindow::onRenderModeChanged()
{
    RenderStyle s = RenderStyle::BallAndStick;
    if      (renderSpaceFillAct_->isChecked()) s = RenderStyle::SpaceFill;
    else if (renderSticksAct_->isChecked())    s = RenderStyle::Sticks;
    else if (renderWireframeAct_->isChecked()) s = RenderStyle::Wireframe;
    viewport_->setRenderStyle(s);
}

void MainWindow::onShowLabels(bool on) { viewport_->setShowLabels(on); }
void MainWindow::onShowBox(bool on)    { viewport_->setShowBox(on);    }
void MainWindow::onShowAxes(bool on)   { viewport_->setShowAxes(on);   }

void MainWindow::onCrystalPreset()
{
    QDialog dlg(this);
    dlg.setWindowTitle(tr("Build Crystal Preset"));
    auto* vbox = new QVBoxLayout(&dlg);

    auto* form = new QFormLayout;
    auto* presetCombo = new QComboBox;
    presetCombo->addItems({"Cu (FCC)","Fe (BCC)","NaCl (Rocksalt)",
                           "Si (Diamond)","Al (FCC)","MgO (Rocksalt)","CsCl"});
    form->addRow(tr("Preset:"), presetCombo);

    auto* naBox = new QSpinBox; naBox->setRange(1,6); naBox->setValue(2);
    auto* nbBox = new QSpinBox; nbBox->setRange(1,6); nbBox->setValue(2);
    auto* ncBox = new QSpinBox; ncBox->setRange(1,6); ncBox->setValue(2);
    form->addRow(tr("Supercell na:"), naBox);
    form->addRow(tr("Supercell nb:"), nbBox);
    form->addRow(tr("Supercell nc:"), ncBox);
    vbox->addLayout(form);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    vbox->addWidget(buttons);

    if (dlg.exec() != QDialog::Accepted) return;

    static const QStringList presetKeys = {"Cu","Fe","NaCl","Si","Al","MgO","CsCl"};
    QString key = presetKeys.value(presetCombo->currentIndex(), "Cu");

    consolePanel_->log(QString("Building %1 %2×%3×%4 supercell…")
        .arg(key).arg(naBox->value()).arg(nbBox->value()).arg(ncBox->value()));

    bridge::KernelRequest req;
    req.op = bridge::KernelOp::EmitCrystal;
    req.preset        = key.toStdString();
    req.supercell[0]  = naBox->value();
    req.supercell[1]  = nbBox->value();
    req.supercell[2]  = ncBox->value();

    auto result = engine_.run(req);
    if (result.success && result.output) {
        doc_ = result.output;
        viewport_->setDocument(doc_);
        syncPanels();
        consolePanel_->logSuccess(QString::fromStdString(result.message));
    } else {
        consolePanel_->logError(QString::fromStdString(result.message));
    }
}

void MainWindow::onAtomSelected(int atomIdx)
{
    const scene::FrameData* f = viewport_->currentFrame();
    if (!f || atomIdx < 0 || atomIdx >= f->atom_count()) {
        propertiesPanel_->clearSelectedAtom();
        return;
    }
    const auto& a = f->atoms[atomIdx];
    propertiesPanel_->setSelectedAtom(
        atomIdx, a.Z,
        QString::fromStdString(a.symbol.empty() ? std::to_string(a.Z) : a.symbol),
        a.pos.x, a.pos.y, a.pos.z);
    statusBar()->showMessage(
        QString("Selected: atom %1 (Z=%2) at (%.3f, %.3f, %.3f) Å")
            .arg(atomIdx).arg(a.Z).arg(a.pos.x).arg(a.pos.y).arg(a.pos.z));
}

void MainWindow::onFrameIndexChanged(int idx, int total)
{
    frameSlider_->blockSignals(true);
    frameSlider_->setMaximum(std::max(0, total-1));
    frameSlider_->setValue(idx);
    frameSlider_->setEnabled(total > 1);
    frameSlider_->blockSignals(false);
    frameLabel_->setText(QString("  Frame %1 / %2  ").arg(idx+1).arg(total));
    statusFrame_->setText(QString("Frame %1/%2").arg(idx+1).arg(total));

    const scene::FrameData* f = viewport_->currentFrame();
    if (f) {
        auto it = f->properties.find("energy_total");
        if (it != f->properties.end())
            statusEnergy_->setText(QString("E: %1 kcal/mol").arg(it->second, 0,'f',4));
    }
}

void MainWindow::onFrameSliderMoved(int val) { viewport_->setFrameIndex(val); }

void MainWindow::onPlayPause()
{
    if (viewport_->isPlaying()) {
        viewport_->pause();
        playAct_->setText(tr("▶ Play"));
    } else {
        viewport_->play();
        playAct_->setText(tr("⏸ Pause"));
    }
}

void MainWindow::onStepBack()    { viewport_->stepBack(); }
void MainWindow::onStepForward() { viewport_->stepForward(); }

void MainWindow::onCommand(const QString& cmd)
{
    QString c = cmd.trimmed().toLower();

    if (c == "relax" || c == "fire")       { onRunRelax(); }
    else if (c == "md" || c == "nvt")       { onRunMD(); }
    else if (c == "sp" || c == "energy" || c == "single") { onSinglePoint(); }
    else if (c == "reset")                  { viewport_->resetCamera(); }
    else if (c == "fit")                    { viewport_->fitCamera(); }
    else if (c == "labels")                 { showLabelsAct_->toggle(); }
    else if (c == "axes")                   { showAxesAct_->toggle(); }
    else if (c == "box")                    { showBoxAct_->toggle(); }
    else if (c == "bs" || c == "balls")     { renderBallStickAct_->trigger(); }
    else if (c == "cpk" || c == "spacefill"){ renderSpaceFillAct_->trigger(); }
    else if (c == "sticks")                 { renderSticksAct_->trigger(); }
    else if (c == "wire" || c == "wireframe"){ renderWireframeAct_->trigger(); }
    else if (c == "play")                   { onPlayPause(); }
    else if (c == "stop")                   { viewport_->pause(); playAct_->setText(tr("▶ Play")); }
    else if (c == "next" || c == ">")        { viewport_->stepForward(); }
    else if (c == "prev" || c == "<")        { viewport_->stepBack(); }
    else if (c == "crystal" || c == "preset") { onCrystalPreset(); }
    else if (c == "clear")                  { consolePanel_->clear(); }
    else if (c == "help") {
        consolePanel_->log("─── Commands ───────────────────────────────────");
        consolePanel_->log("<b>File:</b>      (use menu) File → Open / Save");
        consolePanel_->log("<b>Simulation:</b> relax | md | sp");
        consolePanel_->log("<b>Camera:</b>     reset | fit");
        consolePanel_->log("<b>Render:</b>     bs | cpk | sticks | wire");
        consolePanel_->log("<b>Overlays:</b>   labels | axes | box");
        consolePanel_->log("<b>Trajectory:</b> play | stop | next | prev");
        consolePanel_->log("<b>Crystal:</b>    crystal (opens preset dialog)");
        consolePanel_->log("<b>Console:</b>    clear | help");
    } else {
        consolePanel_->logError("Unknown command: " + cmd + " — type 'help'");
    }
}

// ============================================================================
// Panel synchronization
// ============================================================================

void MainWindow::syncPanels()
{
    if (!doc_ || doc_->empty()) {
        propertiesPanel_->clearAll();
        objectTree_->clear();
        statusAtoms_ ->setText(tr("Atoms: \u2014"));
        statusEnergy_->setText(tr("Energy: \u2014"));
        statusFrame_ ->setText(tr("Frame: \u2014"));
        return;
    }

    const auto& f = doc_->current_frame();

    auto prop = [&](const std::string& k) -> double {
        auto it = f.properties.find(k);
        return it != f.properties.end() ? it->second : 0.0;
    };

    // Identity
    propertiesPanel_->setAtomCount(f.atom_count());
    propertiesPanel_->setBondCount(f.bond_count());
    if (!doc_->provenance.formula.empty())
        propertiesPanel_->setFormula(
            QString::fromStdString(doc_->provenance.formula));

    // Energy
    if (f.properties.count("energy_total")) {
        propertiesPanel_->setEnergy(prop("energy_total"));
        propertiesPanel_->setEnergyBreakdown(
            prop("energy_lj"), prop("energy_coul"), prop("energy_bonded"));
        statusEnergy_->setText(
            QString("E: %1 kcal/mol").arg(prop("energy_total"), 0,'f',4));
    }
    if (f.properties.count("force_rms"))
        propertiesPanel_->setForceRMS(prop("force_rms"));

    // Lattice
    if (f.box.enabled) {
        propertiesPanel_->setLattice(
            scene::distance({0,0,0}, f.box.a),
            scene::distance({0,0,0}, f.box.b),
            scene::distance({0,0,0}, f.box.c));
    } else {
        propertiesPanel_->clearLattice();
    }

    // Sim params + frame info
    propertiesPanel_->setSimParams(
        (int)prop("steps"), prop("temperature"), prop("dt"));
    propertiesPanel_->setFrameInfo(
        viewport_->frameIndex(), doc_->frame_count(), prop("time_ps"));

    // Status bar
    statusAtoms_->setText(QString("Atoms: %1").arg(f.atom_count()));
    statusFrame_->setText(QString("Frame: %1/%2")
        .arg(viewport_->frameIndex()+1).arg(doc_->frame_count()));

    // Object tree
    objectTree_->clear();
    QString name = QString::fromStdString(
        doc_->provenance.source_file.empty()
            ? doc_->provenance.mode
            : doc_->provenance.source_file);

    if (doc_->frame_count() > 1)
        objectTree_->addTrajectory(name, doc_->frame_count(), prop("dt"));
    else if (f.box.enabled)
        objectTree_->addCrystal(name,
            QString::fromStdString(doc_->provenance.mode),
            f.atoms.size() >= 2
                ? scene::distance(f.atoms[0].pos, f.atoms[1].pos) : 0.0,
            f.atom_count());
    else
        objectTree_->addMolecule(name, f.atom_count(), f.bond_count());
}
