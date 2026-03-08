#include "ObjectTree.h"

ObjectTree::ObjectTree(QWidget* parent)
    : QTreeWidget(parent)
{
    setHeaderLabels({tr("Name"), tr("Details")});
    setColumnCount(2);

    moleculesRoot_ = new QTreeWidgetItem(this, {tr("Molecules")});
    crystalsRoot_  = new QTreeWidgetItem(this, {tr("Crystals")});
    resultsRoot_   = new QTreeWidgetItem(this, {tr("Results")});

    moleculesRoot_->setExpanded(true);
    crystalsRoot_->setExpanded(true);
    resultsRoot_->setExpanded(true);
}

void ObjectTree::addMolecule(const QString& name, int atomCount, int bondCount)
{
    auto* item = new QTreeWidgetItem(moleculesRoot_);
    item->setText(0, name);
    item->setText(1, QString("%1 atoms, %2 bonds").arg(atomCount).arg(bondCount));
}

void ObjectTree::clear()
{
    while (moleculesRoot_->childCount() > 0) delete moleculesRoot_->takeChild(0);
    while (crystalsRoot_->childCount() > 0)  delete crystalsRoot_->takeChild(0);
    while (resultsRoot_->childCount() > 0)   delete resultsRoot_->takeChild(0);
}
