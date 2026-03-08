#include "PropertiesPanel.h"

PropertiesPanel::PropertiesPanel(QWidget* parent)
    : QScrollArea(parent)
{
    setWidgetResizable(true);
    setFrameShape(QFrame::NoFrame);

    content_ = new QWidget;
    auto* vbox = new QVBoxLayout(content_);
    vbox->setContentsMargins(6,6,6,6);
    vbox->setSpacing(8);

    // ---- Identity ----
    identityGroup_ = new QGroupBox(tr("Identity"));
    auto* ig = new QFormLayout(identityGroup_);
    ig->addRow(tr("Formula:"),    formulaLabel_    = makeValueLabel());
    ig->addRow(tr("Atoms:"),      atomCountLabel_  = makeValueLabel());
    ig->addRow(tr("Bonds:"),      bondCountLabel_  = makeValueLabel());
    vbox->addWidget(identityGroup_);

    // ---- Energy ----
    energyGroup_ = new QGroupBox(tr("Energy"));
    auto* eg = new QFormLayout(energyGroup_);
    eg->addRow(tr("Total:"),       energyLabel_ = makeValueLabel());
    eg->addRow(tr("  LJ:"),        ljLabel_     = makeValueLabel());
    eg->addRow(tr("  Coulomb:"),   coulLabel_   = makeValueLabel());
    eg->addRow(tr("  Bonded:"),    bondedLabel_ = makeValueLabel());
    eg->addRow(tr("Frms:"),        frmsLabel_   = makeValueLabel());
    vbox->addWidget(energyGroup_);

    // ---- Lattice ----
    latticeGroup_ = new QGroupBox(tr("Lattice"));
    auto* lg = new QFormLayout(latticeGroup_);
    lg->addRow(tr("a, b, c:"),    latticeALabel_   = makeValueLabel());
    lg->addRow(tr(""),            latticeBLabel_   = makeValueLabel());
    lg->addRow(tr(""),            latticeCLabel_   = makeValueLabel());
    lg->addRow(tr("α, β, γ:"),   latticeAngLabel_ = makeValueLabel());
    latticeGroup_->setVisible(false);
    vbox->addWidget(latticeGroup_);

    // ---- Simulation ----
    simGroup_ = new QGroupBox(tr("Simulation"));
    auto* sg = new QFormLayout(simGroup_);
    sg->addRow(tr("Steps:"),       stepsLabel_ = makeValueLabel());
    sg->addRow(tr("T (K):"),       tempLabel_  = makeValueLabel());
    sg->addRow(tr("dt (fs):"),     dtLabel_    = makeValueLabel());
    sg->addRow(tr("Frame:"),       frameLabel_ = makeValueLabel());
    vbox->addWidget(simGroup_);

    // ---- Selection ----
    selGroup_ = new QGroupBox(tr("Selected Atom"));
    auto* sl = new QFormLayout(selGroup_);
    sl->addRow(tr("Index:"),       selIdxLabel_ = makeValueLabel());
    sl->addRow(tr("Element:"),     selSymLabel_ = makeValueLabel());
    sl->addRow(tr("Position:"),    selPosLabel_ = makeValueLabel());
    selGroup_->setVisible(false);
    vbox->addWidget(selGroup_);

    vbox->addStretch();
    setWidget(content_);
    clearAll();
}

QLabel* PropertiesPanel::makeValueLabel()
{
    auto* l = new QLabel("—");
    l->setTextInteractionFlags(Qt::TextSelectableByMouse);
    return l;
}

void PropertiesPanel::setFormula(const QString& f)     { formulaLabel_->setText(f); }
void PropertiesPanel::setAtomCount(int n)              { atomCountLabel_->setText(QString::number(n)); }
void PropertiesPanel::setBondCount(int n)              { bondCountLabel_->setText(QString::number(n)); }

void PropertiesPanel::setEnergy(double e)
{
    energyLabel_->setText(QString("%1 kcal/mol").arg(e, 0, 'f', 4));
}

void PropertiesPanel::setEnergyBreakdown(double lj, double coul, double bond)
{
    ljLabel_    ->setText(QString("%1").arg(lj,   0, 'f', 4));
    coulLabel_  ->setText(QString("%1").arg(coul, 0, 'f', 4));
    bondedLabel_->setText(QString("%1").arg(bond, 0, 'f', 4));
}

void PropertiesPanel::setForceRMS(double rms)
{
    frmsLabel_->setText(QString("%1 kcal/mol/Å").arg(rms, 0, 'e', 3));
}

void PropertiesPanel::setLattice(double a, double b, double c,
                                  double alpha, double beta, double gamma)
{
    latticeALabel_  ->setText(QString("a = %1 Å").arg(a, 0, 'f', 4));
    latticeBLabel_  ->setText(QString("b = %1 Å").arg(b, 0, 'f', 4));
    latticeCLabel_  ->setText(QString("c = %1 Å").arg(c, 0, 'f', 4));
    latticeAngLabel_->setText(QString("%1°  %2°  %3°")
        .arg(alpha,0,'f',2).arg(beta,0,'f',2).arg(gamma,0,'f',2));
    latticeGroup_->setVisible(true);
}

void PropertiesPanel::clearLattice()        { latticeGroup_->setVisible(false); }

void PropertiesPanel::setSimParams(int steps, double temp, double dt)
{
    stepsLabel_->setText(QString::number(steps));
    tempLabel_ ->setText(QString("%1 K").arg(temp, 0, 'f', 1));
    dtLabel_   ->setText(QString("%1 fs").arg(dt,  0, 'f', 2));
}

void PropertiesPanel::setFrameInfo(int idx, int total, double time_ps)
{
    frameLabel_->setText(QString("%1 / %2  (t = %3 ps)")
        .arg(idx+1).arg(total).arg(time_ps, 0, 'f', 3));
}

void PropertiesPanel::setSelectedAtom(int atomIdx, int Z, const QString& symbol,
                                       double x, double y, double z)
{
    selIdxLabel_->setText(QString("%1  (Z=%2)").arg(atomIdx).arg(Z));
    selSymLabel_->setText(symbol);
    selPosLabel_->setText(QString("(%1, %2, %3) Å")
        .arg(x,0,'f',3).arg(y,0,'f',3).arg(z,0,'f',3));
    selGroup_->setVisible(true);
}

void PropertiesPanel::clearSelectedAtom() { selGroup_->setVisible(false); }

void PropertiesPanel::clearAll()
{
    formulaLabel_  ->setText("—");
    atomCountLabel_->setText("—");
    bondCountLabel_->setText("—");
    energyLabel_   ->setText("—");
    ljLabel_       ->setText("—");
    coulLabel_     ->setText("—");
    bondedLabel_   ->setText("—");
    frmsLabel_     ->setText("—");
    stepsLabel_    ->setText("—");
    tempLabel_     ->setText("—");
    dtLabel_       ->setText("—");
    frameLabel_    ->setText("—");
    clearLattice();
    clearSelectedAtom();
}
