#include "PropertiesPanel.h"

PropertiesPanel::PropertiesPanel(QWidget* parent)
    : QScrollArea(parent)
{
    setWidgetResizable(true);

    auto* content = new QWidget;
    auto* layout  = new QVBoxLayout(content);

    // Identity group
    auto* idGroup  = new QGroupBox(tr("Identity"));
    auto* idForm   = new QFormLayout(idGroup);
    formulaLabel_  = new QLabel("—");
    atomCountLabel_= new QLabel("—");
    idForm->addRow(tr("Formula:"), formulaLabel_);
    idForm->addRow(tr("Atoms:"),   atomCountLabel_);
    layout->addWidget(idGroup);

    // Energy group
    auto* enGroup = new QGroupBox(tr("Energy"));
    auto* enForm  = new QFormLayout(enGroup);
    energyLabel_  = new QLabel("—");
    forceLabel_   = new QLabel("—");
    enForm->addRow(tr("Total (kcal/mol):"), energyLabel_);
    enForm->addRow(tr("Force RMS:"),        forceLabel_);
    layout->addWidget(enGroup);

    layout->addStretch();
    setWidget(content);
}

void PropertiesPanel::setFormula(const QString& f)   { formulaLabel_->setText(f); }
void PropertiesPanel::setAtomCount(int n)            { atomCountLabel_->setText(QString::number(n)); }
void PropertiesPanel::setEnergy(double e)            { energyLabel_->setText(QString::number(e, 'f', 4)); }
void PropertiesPanel::setForceRMS(double f)          { forceLabel_->setText(QString::number(f, 'e', 3)); }
void PropertiesPanel::clearAll()
{
    formulaLabel_->setText("—");
    atomCountLabel_->setText("—");
    energyLabel_->setText("—");
    forceLabel_->setText("—");
}
