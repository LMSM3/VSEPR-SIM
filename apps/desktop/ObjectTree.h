#pragma once
/**
 * ObjectTree.h — Left dock: hierarchical structure browser
 *
 * Three root categories:
 *   Molecules   — single-frame molecular structures
 *   Crystals    — crystal emitter results
 *   Results     — energy/trajectory outputs
 *
 * Each item stores type metadata in UserRole data.
 * Double-clicking an item activates it in the viewport.
 */

#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QMenu>
#include <QAction>
#include <QString>

enum class TreeItemType { Molecule = 1, Crystal = 2, Result = 3, Trajectory = 4 };

class ObjectTree : public QTreeWidget
{
    Q_OBJECT

public:
    explicit ObjectTree(QWidget* parent = nullptr);

    // --- Add items ---
    QTreeWidgetItem* addMolecule(const QString& name, int atomCount, int bondCount);
    QTreeWidgetItem* addCrystal(const QString& name, const QString& preset,
                                double nn_dist, int atomCount);
    QTreeWidgetItem* addResult(const QString& name, double energy,
                               int steps, double frms);
    QTreeWidgetItem* addTrajectory(const QString& name, int frameCount,
                                   double dt_fs);

    // --- Bulk operations ---
    void clear();
    void clearCategory(TreeItemType type);

    // --- Selection ---
    void setActiveItem(QTreeWidgetItem* item);

signals:
    void itemActivated(TreeItemType type, const QString& name,
                       QTreeWidgetItem* item);
    void itemRemoveRequested(QTreeWidgetItem* item);

private slots:
    void onDoubleClicked(QTreeWidgetItem* item, int col);
    void onContextMenu(const QPoint& pos);
    void onRemoveSelected();

private:
    QTreeWidgetItem* moleculesRoot_;
    QTreeWidgetItem* crystalsRoot_;
    QTreeWidgetItem* resultsRoot_;

    QAction* removeAct_;
};
