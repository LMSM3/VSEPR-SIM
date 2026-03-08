#pragma once
/**
 * PropertiesPanel.h — Right dock: inspector for selected object
 *
 * Shows formula, energy, forces, lattice parameters, etc.
 * Read-only for now; editable fields come in a later milestone.
 */

#include <QWidget>
#include <QFormLayout>
#include <QLabel>
#include <QGroupBox>
#include <QVBoxLayout>
#include <QScrollArea>

class PropertiesPanel : public QScrollArea
{
    Q_OBJECT

public:
    explicit PropertiesPanel(QWidget* parent = nullptr);

    void setFormula(const QString& formula);
    void setAtomCount(int n);
    void setEnergy(double kcal);
    void setForceRMS(double rms);
    void clearAll();

private:
    QLabel* formulaLabel_;
    QLabel* atomCountLabel_;
    QLabel* energyLabel_;
    QLabel* forceLabel_;
};
