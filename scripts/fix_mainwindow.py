#!/usr/bin/env python3
"""Fix two remaining issues in MainWindow.cpp."""
import os

path = "/mnt/c/Users/Liam/Desktop/vsepr-sim/apps/desktop/MainWindow.cpp"
src = open(path, 'r', encoding='utf-8').read()

# ── Fix 1: remove leftover old-createDockWidgets fragment ──────────────────
old1 = '''}    for (auto* m : menuBar()->findChildren<QMenu*>()) {
        if (m->title() == tr("&View")) { viewMenu = m; break; }
    }
    if (viewMenu) {
        viewMenu->addAction(objectDock_->toggleViewAction());
        viewMenu->addAction(propertiesDock_->toggleViewAction());
        viewMenu->addAction(consoleDock_->toggleViewAction());
    }
}'''
new1 = '}'
assert old1 in src, "Fragment 1 not found"
src = src.replace(old1, new1, 1)

# ── Fix 2: replace old syncPanels with full new version ─────────────────────
# Find from the syncPanels comment through the end of the function
import re
old2_pattern = (
    r'// =+\n// Panel synchronization\n// =+\n\n'
    r'void MainWindow::syncPanels\(\).*?^}'
)
new2 = r'''// ============================================================================
// Panel synchronization
// ============================================================================

void MainWindow::syncPanels()
{
    if (!doc_ || doc_->empty()) {
        propertiesPanel_->clearAll();
        objectTree_->clear();
        statusAtoms_ ->setText(tr("Atoms: \u2014"));
        statusEnergy_->setText(tr("Energy: \u2014"));
        statusFrame_ ->setText(tr("Frame: \u2014"));
        return;
    }

    const auto& f = doc_->current_frame();

    auto prop = [&](const std::string& k) -> double {
        auto it = f.properties.find(k);
        return it != f.properties.end() ? it->second : 0.0;
    };

    // Identity
    propertiesPanel_->setAtomCount(f.atom_count());
    propertiesPanel_->setBondCount(f.bond_count());
    if (!doc_->provenance.formula.empty())
        propertiesPanel_->setFormula(
            QString::fromStdString(doc_->provenance.formula));

    // Energy
    if (f.properties.count("energy_total")) {
        propertiesPanel_->setEnergy(prop("energy_total"));
        propertiesPanel_->setEnergyBreakdown(
            prop("energy_lj"), prop("energy_coul"), prop("energy_bonded"));
        statusEnergy_->setText(
            QString("E: %1 kcal/mol").arg(prop("energy_total"), 0,'f',4));
    }
    if (f.properties.count("force_rms"))
        propertiesPanel_->setForceRMS(prop("force_rms"));

    // Lattice
    if (f.box.enabled) {
        propertiesPanel_->setLattice(
            scene::distance({0,0,0}, f.box.a),
            scene::distance({0,0,0}, f.box.b),
            scene::distance({0,0,0}, f.box.c));
    } else {
        propertiesPanel_->clearLattice();
    }

    // Sim params + frame info
    propertiesPanel_->setSimParams(
        (int)prop("steps"), prop("temperature"), prop("dt"));
    propertiesPanel_->setFrameInfo(
        viewport_->frameIndex(), doc_->frame_count(), prop("time_ps"));

    // Status bar
    statusAtoms_->setText(QString("Atoms: %1").arg(f.atom_count()));
    statusFrame_->setText(QString("Frame: %1/%2")
        .arg(viewport_->frameIndex()+1).arg(doc_->frame_count()));

    // Object tree
    objectTree_->clear();
    QString name = QString::fromStdString(
        doc_->provenance.source_file.empty()
            ? doc_->provenance.mode
            : doc_->provenance.source_file);

    if (doc_->frame_count() > 1)
        objectTree_->addTrajectory(name, doc_->frame_count(), prop("dt"));
    else if (f.box.enabled)
        objectTree_->addCrystal(name,
            QString::fromStdString(doc_->provenance.mode),
            f.atoms.size() >= 2
                ? scene::distance(f.atoms[0].pos, f.atoms[1].pos) : 0.0,
            f.atom_count());
    else
        objectTree_->addMolecule(name, f.atom_count(), f.bond_count());
}'''

match = re.search(old2_pattern, src, re.MULTILINE | re.DOTALL)
assert match, "syncPanels pattern not found"
src = src[:match.start()] + new2 + src[match.end():]

with open(path, 'w', newline='\n', encoding='utf-8') as f_out:
    f_out.write(src)

print(f"Fixed MainWindow.cpp — {len(src.splitlines())} lines")
