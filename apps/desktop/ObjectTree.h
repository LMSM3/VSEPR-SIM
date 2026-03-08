#pragma once
/**
 * ObjectTree.h — Left dock: scene hierarchy
 *
 * Shows loaded molecules, crystal structures, simulation outputs.
 * Selecting an item updates the Properties panel and centers the viewport.
 */

#include <QTreeWidget>

class ObjectTree : public QTreeWidget
{
    Q_OBJECT

public:
    explicit ObjectTree(QWidget* parent = nullptr);

    void addMolecule(const QString& name, int atomCount, int bondCount);
    void clear();

private:
    QTreeWidgetItem* moleculesRoot_;
    QTreeWidgetItem* crystalsRoot_;
    QTreeWidgetItem* resultsRoot_;
};
