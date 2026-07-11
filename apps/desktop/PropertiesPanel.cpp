#include "PropertiesPanel.h"
#include <cmath>

// ============================================================================
// Helpers
// ============================================================================

static QLabel* makeValueLabel()
{
    auto* l = new QLabel("\xe2\x80\x94");
    l->setTextInteractionFlags(Qt::TextSelectableByMouse);
    return l;
}

QString PropertiesPanel::fmtEnergy(double e)
{
    return std::isnan(e) ? "\xe2\x80\x94" : QString::number(e, 'f', 4);
}

QString PropertiesPanel::fmtForce(double f)
{
    return std::isnan(f) ? "\xe2\x80\x94" : QString::number(f, 'e', 3);
}

// ============================================================================
// Construction
// ============================================================================

PropertiesPanel::PropertiesPanel(QWidget* parent)
    : QScrollArea(parent)
{
    setWidgetResizable(true);

    auto* content = new QWidget;
    auto* layout  = new QVBoxLayout(content);
    layout->setSpacing(4);

    // --- Identity ---
    auto* idGroup  = new QGroupBox(tr("Identity"));
    auto* idForm   = new QFormLayout(idGroup);
    formulaLabel_   = makeValueLabel();
    atomCountLabel_ = makeValueLabel();
    bondCountLabel_ = makeValueLabel();
    idForm->addRow(tr("Formula:"),    formulaLabel_);
    idForm->addRow(tr("Atoms:"),      atomCountLabel_);
    idForm->addRow(tr("Bonds:"),      bondCountLabel_);
    layout->addWidget(idGroup);

    // --- Frame ---
    auto* frGroup = new QGroupBox(tr("Frame"));
    auto* frForm  = new QFormLayout(frGroup);
    frameLabel_ = makeValueLabel();
    stepLabel_  = makeValueLabel();
    timeLabel_  = makeValueLabel();
    frForm->addRow(tr("Frame:"), frameLabel_);
    frForm->addRow(tr("Step:"),  stepLabel_);
    frForm->addRow(tr("Time:"),  timeLabel_);
    layout->addWidget(frGroup);

    // --- Energy ---
    auto* enGroup = new QGroupBox(tr("Energy (kcal/mol)"));
    auto* enForm  = new QFormLayout(enGroup);
    eTotalLabel_ = makeValueLabel();
    eBondLabel_  = makeValueLabel();
    eAngleLabel_ = makeValueLabel();
    eVdWLabel_   = makeValueLabel();
    eCoulLabel_  = makeValueLabel();
    ePolLabel_   = makeValueLabel();
    enForm->addRow(tr("Total:"),       eTotalLabel_);
    enForm->addRow(tr("Bond:"),        eBondLabel_);
    enForm->addRow(tr("Angle:"),       eAngleLabel_);
    enForm->addRow(tr("vdW:"),         eVdWLabel_);
    enForm->addRow(tr("Coulomb:"),     eCoulLabel_);
    enForm->addRow(tr("Polar.:"),      ePolLabel_);
    layout->addWidget(enGroup);

    // --- Forces ---
    auto* foGroup = new QGroupBox(tr("Forces"));
    auto* foForm  = new QFormLayout(foGroup);
    forceRMSLabel_ = makeValueLabel();
    foForm->addRow(tr("RMS (kcal/mol\xc2\xb7\xc3\x85):"), forceRMSLabel_);
    layout->addWidget(foGroup);

    layout->addStretch();
    setWidget(content);
}

// ============================================================================
// Identity setters
// ============================================================================

void PropertiesPanel::setFormula(const QString& f)
{
    formulaLabel_->setText(f.isEmpty() ? "\xe2\x80\x94" : f);
}

void PropertiesPanel::setAtomCount(int n)
{
    atomCountLabel_->setText(n >= 0 ? QString::number(n) : "\xe2\x80\x94");
}

void PropertiesPanel::setBondCount(int n)
{
    bondCountLabel_->setText(n >= 0 ? QString::number(n) : "\xe2\x80\x94");
}

// ============================================================================
// Frame setters
// ============================================================================

void PropertiesPanel::setFrameIndex(int idx, int total)
{
    frameLabel_->setText(QString("%1 / %2").arg(idx + 1).arg(total));
}

void PropertiesPanel::setStep(int step)
{
    stepLabel_->setText(step >= 0 ? QString::number(step) : "\xe2\x80\x94");
}

void PropertiesPanel::setTime(double fs)
{
    timeLabel_->setText(std::isnan(fs) || fs < 0.0
                            ? "\xe2\x80\x94"
                            : QString::number(fs, 'f', 2) + " fs");
}

// ============================================================================
// Energy setters
// ============================================================================

void PropertiesPanel::setEnergyTotal(double e)   { eTotalLabel_->setText(fmtEnergy(e)); }
void PropertiesPanel::setEnergyBond(double e)    { eBondLabel_->setText(fmtEnergy(e));  }
void PropertiesPanel::setEnergyAngle(double e)   { eAngleLabel_->setText(fmtEnergy(e)); }
void PropertiesPanel::setEnergyVdW(double e)     { eVdWLabel_->setText(fmtEnergy(e));   }
void PropertiesPanel::setEnergyCoulomb(double e) { eCoulLabel_->setText(fmtEnergy(e));  }
void PropertiesPanel::setEnergyPol(double e)     { ePolLabel_->setText(fmtEnergy(e));   }

// ============================================================================
// Force setter
// ============================================================================

void PropertiesPanel::setForceRMS(double rms)
{
    forceRMSLabel_->setText(fmtForce(rms));
}

// ============================================================================
// clearAll
// ============================================================================

void PropertiesPanel::clearAll()
{
    const QString dash = "\xe2\x80\x94";
    formulaLabel_->setText(dash);
    atomCountLabel_->setText(dash);
    bondCountLabel_->setText(dash);
    frameLabel_->setText(dash);
    stepLabel_->setText(dash);
    timeLabel_->setText(dash);
    eTotalLabel_->setText(dash);
    eBondLabel_->setText(dash);
    eAngleLabel_->setText(dash);
    eVdWLabel_->setText(dash);
    eCoulLabel_->setText(dash);
    ePolLabel_->setText(dash);
    forceRMSLabel_->setText(dash);
}
