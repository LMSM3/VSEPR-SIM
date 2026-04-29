#pragma once
/**
 * PropertiesPanel.h — Right dock: inspector for selected frame/document
 *
 * Groups:
 *   Identity   — formula, atom count, bond count
 *   Frame      — frame index, MD step, simulation time
 *   Energy     — total, bond, angle, vdW, Coulomb, polarisation (kcal/mol)
 *   Forces     — force RMS (kcal/mol·Å)
 *
 * All fields are read-only labels updated via the set* API.
 * clearAll() resets every field to "—".
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

    // Identity
    void setFormula(const QString& formula);
    void setAtomCount(int n);
    void setBondCount(int n);

    // Frame provenance
    void setFrameIndex(int idx, int total);   // "3 / 12"
    void setStep(int step);
    void setTime(double fs);                  // femtoseconds

    // Energy decomposition (kcal/mol) — pass NaN to hide / show "—"
    void setEnergyTotal(double e);
    void setEnergyBond(double e);
    void setEnergyAngle(double e);
    void setEnergyVdW(double e);
    void setEnergyCoulomb(double e);
    void setEnergyPol(double e);

    // Forces
    void setForceRMS(double rms);

    void clearAll();

private:
    static QString fmtEnergy(double e);  // NaN → "—", else f4
    static QString fmtForce(double f);   // NaN → "—", else e3

    // Identity
    QLabel* formulaLabel_;
    QLabel* atomCountLabel_;
    QLabel* bondCountLabel_;

    // Frame
    QLabel* frameLabel_;
    QLabel* stepLabel_;
    QLabel* timeLabel_;

    // Energy
    QLabel* eTotalLabel_;
    QLabel* eBondLabel_;
    QLabel* eAngleLabel_;
    QLabel* eVdWLabel_;
    QLabel* eCoulLabel_;
    QLabel* ePolLabel_;

    // Forces
    QLabel* forceRMSLabel_;
};
