#pragma once
/**
 * PropertiesPanel.h — Right dock: properties inspector
 *
 * Sections:
 *   Identity     — formula, atom count, bond count
 *   Energy       — total + breakdown (LJ, Coulomb, bonded)
 *   Geometry     — selected atom info, bond angles
 *   Lattice      — a, b, c, alpha, beta, gamma (if PBC)
 *   Simulation   — steps, temperature, dt, Frms
 *   Frame        — frame index / total, step, time
 */

#include <QWidget>
#include <QScrollArea>
#include <QFormLayout>
#include <QLabel>
#include <QGroupBox>
#include <QVBoxLayout>
#include <QTableWidget>
#include <QHeaderView>
#include <QString>

class PropertiesPanel : public QScrollArea
{
    Q_OBJECT

public:
    explicit PropertiesPanel(QWidget* parent = nullptr);

    // --- Identity ---
    void setFormula(const QString& formula);
    void setAtomCount(int n);
    void setBondCount(int n);

    // --- Energy ---
    void setEnergy(double total_kcal);
    void setEnergyBreakdown(double lj, double coul, double bond);
    void setForceRMS(double rms);

    // --- Lattice ---
    void setLattice(double a, double b, double c,
                    double alpha=90, double beta=90, double gamma=90);
    void clearLattice();

    // --- Simulation ---
    void setSimParams(int steps, double temp_K, double dt_fs);
    void setFrameInfo(int idx, int total, double time_ps);

    // --- Selected atom ---
    void setSelectedAtom(int atomIdx, int Z, const QString& symbol,
                         double x, double y, double z);
    void clearSelectedAtom();

    // --- Bulk ---
    void clearAll();

private:
    QWidget* content_;

    // Identity group
    QGroupBox* identityGroup_;
    QLabel*    formulaLabel_;
    QLabel*    atomCountLabel_;
    QLabel*    bondCountLabel_;

    // Energy group
    QGroupBox* energyGroup_;
    QLabel*    energyLabel_;
    QLabel*    ljLabel_;
    QLabel*    coulLabel_;
    QLabel*    bondedLabel_;
    QLabel*    frmsLabel_;

    // Lattice group
    QGroupBox* latticeGroup_;
    QLabel*    latticeALabel_;
    QLabel*    latticeBLabel_;
    QLabel*    latticeCLabel_;
    QLabel*    latticeAngLabel_;

    // Simulation group
    QGroupBox* simGroup_;
    QLabel*    stepsLabel_;
    QLabel*    tempLabel_;
    QLabel*    dtLabel_;
    QLabel*    frameLabel_;

    // Selection group
    QGroupBox* selGroup_;
    QLabel*    selIdxLabel_;
    QLabel*    selSymLabel_;
    QLabel*    selPosLabel_;

    static QLabel* makeValueLabel();
};
