#include "ObjectTree.h"
#include <QHeaderView>

ObjectTree::ObjectTree(QWidget* parent)
    : QTreeWidget(parent)
{
    setHeaderLabels({tr("Name"), tr("Details")});
    setColumnCount(2);
    header()->setSectionResizeMode(0, QHeaderView::Stretch);
    header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    setContextMenuPolicy(Qt::CustomContextMenu);
    setAlternatingRowColors(true);
    setSelectionMode(QAbstractItemView::SingleSelection);

    moleculesRoot_ = new QTreeWidgetItem(this, {tr("Molecules")});
    crystalsRoot_  = new QTreeWidgetItem(this, {tr("Crystals")});
    resultsRoot_   = new QTreeWidgetItem(this, {tr("Results")});

    for (auto* root : {moleculesRoot_, crystalsRoot_, resultsRoot_}) {
        QFont f = root->font(0);
        f.setBold(true);
        root->setFont(0, f);
        root->setExpanded(true);
    }

    removeAct_ = new QAction(tr("Remove"), this);
    connect(removeAct_, &QAction::triggered, this, &ObjectTree::onRemoveSelected);
    connect(this, &QTreeWidget::itemDoubleClicked, this, &ObjectTree::onDoubleClicked);
    connect(this, &QTreeWidget::customContextMenuRequested, this, &ObjectTree::onContextMenu);
}

QTreeWidgetItem* ObjectTree::addMolecule(const QString& name, int atomCount, int bondCount)
{
    auto* item = new QTreeWidgetItem(moleculesRoot_);
    item->setText(0, name);
    item->setText(1, QString("%1 atoms, %2 bonds").arg(atomCount).arg(bondCount));
    item->setData(0, Qt::UserRole, (int)TreeItemType::Molecule);
    moleculesRoot_->setExpanded(true);
    return item;
}

QTreeWidgetItem* ObjectTree::addCrystal(const QString& name, const QString& preset,
                                        double nn_dist, int atomCount)
{
    auto* item = new QTreeWidgetItem(crystalsRoot_);
    item->setText(0, name);
    item->setText(1, QString("%1 | nn=%.3f Å | %2 atoms")
        .arg(preset).arg(nn_dist).arg(atomCount));
    item->setData(0, Qt::UserRole, (int)TreeItemType::Crystal);
    crystalsRoot_->setExpanded(true);
    return item;
}

QTreeWidgetItem* ObjectTree::addResult(const QString& name, double energy,
                                       int steps, double frms)
{
    auto* item = new QTreeWidgetItem(resultsRoot_);
    item->setText(0, name);
    item->setText(1, QString("E=%.4f | steps=%1 | Frms=%.2e")
        .arg(energy).arg(steps).arg(frms));
    item->setData(0, Qt::UserRole, (int)TreeItemType::Result);
    resultsRoot_->setExpanded(true);
    return item;
}

QTreeWidgetItem* ObjectTree::addTrajectory(const QString& name, int frameCount,
                                           double dt_fs)
{
    auto* item = new QTreeWidgetItem(resultsRoot_);
    item->setText(0, name);
    item->setText(1, QString("%1 frames | dt=%.1f fs").arg(frameCount).arg(dt_fs));
    item->setData(0, Qt::UserRole, (int)TreeItemType::Trajectory);
    resultsRoot_->setExpanded(true);
    return item;
}

void ObjectTree::clear()
{
    while (moleculesRoot_->childCount()>0) delete moleculesRoot_->takeChild(0);
    while (crystalsRoot_->childCount()>0)  delete crystalsRoot_->takeChild(0);
    while (resultsRoot_->childCount()>0)   delete resultsRoot_->takeChild(0);
}

void ObjectTree::clearCategory(TreeItemType type)
{
    QTreeWidgetItem* root = nullptr;
    if (type==TreeItemType::Molecule)   root = moleculesRoot_;
    else if (type==TreeItemType::Crystal) root = crystalsRoot_;
    else root = resultsRoot_;
    if (root) while (root->childCount()>0) delete root->takeChild(0);
}

void ObjectTree::setActiveItem(QTreeWidgetItem* item)
{
    if (item) { setCurrentItem(item); scrollToItem(item); }
}

void ObjectTree::onDoubleClicked(QTreeWidgetItem* item, int)
{
    if (!item || !item->parent()) return; // ignore root items
    auto type = (TreeItemType)item->data(0, Qt::UserRole).toInt();
    emit itemActivated(type, item->text(0), item);
}

void ObjectTree::onContextMenu(const QPoint& pos)
{
    QTreeWidgetItem* item = itemAt(pos);
    if (!item || !item->parent()) return;
    QMenu menu(this);
    menu.addAction(removeAct_);
    menu.exec(viewport()->mapToGlobal(pos));
}

void ObjectTree::onRemoveSelected()
{
    QTreeWidgetItem* item = currentItem();
    if (!item || !item->parent()) return;
    emit itemRemoveRequested(item);
    delete item;
}
