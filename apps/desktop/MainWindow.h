#pragma once
/**
 * MainWindow.h — Top-level application window
 *
 * Layout (SolidWorks / FreeCAD grammar):
 *
 *   ┌──────────────────────────────────────────────────┐
 *   │ Menu bar                                          │
 *   ├──────────────────────────────────────────────────┤
 *   │ Toolbar                                           │
 *   ├───────────┬────────────────────────┬─────────────┤
 *   │ Object    │                        │ Properties  │
 *   │ Tree      │   GL Viewport          │ Inspector   │
 *   │ (dock)    │   (central widget)     │ (dock)      │
 *   │           │                        │             │
 *   ├───────────┴────────────────────────┴─────────────┤
 *   │ Console / Log  (dock, bottom)                     │
 *   ├──────────────────────────────────────────────────┤
 *   │ Status bar                                        │
 *   └──────────────────────────────────────────────────┘
 */

#include <QMainWindow>
#include <QDockWidget>
#include <QTreeWidget>
#include <QTextEdit>
#include <QLabel>
#include <QStatusBar>
#include <QMenuBar>
#include <QToolBar>
#include <QAction>
#include <QActionGroup>
#include <QFileDialog>
#include <QMessageBox>
#include <QSettings>
#include <QCloseEvent>
#include <QSplitter>
#include <QVBoxLayout>
#include <QGroupBox>
#include <QFormLayout>
#include <QLineEdit>
#include <QComboBox>
#include <QPushButton>
#include <QSlider>
#include <QDialog>
#include <QDialogButtonBox>
#include <QSpinBox>

#include "ViewportWidget.h"
#include "ObjectTree.h"
#include "PropertiesPanel.h"
#include "ConsolePanel.h"
#include "bridge/EngineAdapter.h"

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

protected:
    void closeEvent(QCloseEvent* event) override;

private slots:
    // File
    void onFileOpen();
    void onFileSave();
    void onFileExportImage();

    // Simulation
    void onRunRelax();
    void onRunMD();
    void onSinglePoint();

    // View
    void onResetCamera();
    void onFitCamera();
    void onToggleWireframe();
    void onRenderModeChanged();
    void onShowLabels(bool on);
    void onShowBox(bool on);
    void onShowAxes(bool on);
    void onCrystalPreset();
    void onAtomSelected(int atomIdx);
    void onFrameIndexChanged(int idx, int total);
    void onFrameSliderMoved(int val);
    void onPlayPause();
    void onStepBack();
    void onStepForward();

    // Console
    void onCommand(const QString& cmd);

private:
    void createActions();
    void createMenus();
    void createToolBars();
    void createDockWidgets();
    void createStatusBar();
    void applyTheme();
    void saveSettings();
    void loadSettings();

    // Update panels from current document
    void syncPanels();

    // Central viewport
    ViewportWidget* viewport_;

    // Bridge to engine
    bridge::EngineAdapter engine_;
    std::shared_ptr<scene::SceneDocument> doc_;

    // Dock panels
    ObjectTree*      objectTree_;
    PropertiesPanel* propertiesPanel_;
    ConsolePanel*    consolePanel_;

    QDockWidget* objectDock_;
    QDockWidget* propertiesDock_;
    QDockWidget* consoleDock_;

    // Actions
    QAction* openAct_;
    QAction* saveAct_;
    QAction* exportImageAct_;
    QAction* exitAct_;

    QAction* relaxAct_;
    QAction* mdAct_;
    QAction* singlePointAct_;

    QAction* resetCameraAct_;
    QAction* fitCameraAct_;
    QAction* wireframeAct_;

    // Render mode (exclusive action group)
    QActionGroup* renderModeGroup_;
    QAction* renderBallStickAct_;
    QAction* renderSpaceFillAct_;
    QAction* renderSticksAct_;
    QAction* renderWireframeAct_;

    // Overlay toggles
    QAction* showLabelsAct_;
    QAction* showBoxAct_;
    QAction* showAxesAct_;

    // Crystal preset
    QAction* crystalPresetAct_;

    // Trajectory toolbar widgets
    QToolBar*  trajectoryBar_;
    QSlider*   frameSlider_;
    QLabel*    frameLabel_;
    QAction*   playAct_;
    QAction*   stepBackAct_;
    QAction*   stepFwdAct_;

    // Status bar labels
    QLabel* statusAtoms_;
    QLabel* statusEnergy_;
    QLabel* statusFrame_;
};
