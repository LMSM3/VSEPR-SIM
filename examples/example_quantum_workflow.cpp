/*
example_quantum_workflow.cpp
-----------------------------
Complete example demonstrating quantum module integration.

Demonstrates:
1. Building molecules
2. Computing excitation spectra
3. Generating UV-Vis absorption
4. Exporting to HTML/JSON/CSV
5. Integration with existing VSEPR-Sim code

Compile:
  g++ -std=c++17 -I src -I include example_quantum_workflow.cpp -o quantum_demo

Usage:
  ./quantum_demo
*/

#include "QunatumModel/qm_output_bridge.hpp"
#include "sim/molecule.hpp"
#include "core/types.hpp"
#include <iostream>
#include <iomanip>

using namespace vsepr;
using namespace vsepr::quantum;

// ============================================================================
// Example 1: Benzene - Chromophore Library
// ============================================================================
void example_benzene() {
    std::cout << "\n╔══════════════════════════════════════════════════════════╗\n";
    std::cout << "║   Example 1: Benzene UV-Vis Spectrum                    ║\n";
    std::cout << "╚══════════════════════════════════════════════════════════╝\n\n";
    
    // Get benzene excitation data from library
    auto excitation = ChromophoreLibrary::benzene();
    
    std::cout << "Electronic States:\n";
    for (size_t i = 0; i < excitation.states.size(); ++i) {
        const auto& state = excitation.states[i];
        std::cout << "  S" << i << ": " 
                  << std::fixed << std::setprecision(2) << state.energy_ev 
                  << " eV - " << state.character << "\n";
    }
    
    std::cout << "\nTransitions from Ground State:\n";
    for (const auto& trans : excitation.transitions) {
        std::cout << "  " << trans.wavelength_nm << " nm ("
                  << trans.energy_ev << " eV) - f = " 
                  << std::setprecision(3) << trans.oscillator_strength
                  << " - " << trans.type << "\n";
    }
    
    // Generate absorption spectrum
    auto absorption = AbsorptionSpectrum::from_excitation(excitation);
    
    std::cout << "\nPredicted Color: " << absorption.estimate_color() << "\n";
    
    // Export
    absorption.export_csv("benzene_absorption.csv");
    std::cout << "✓ Exported: benzene_absorption.csv\n";
    
    // Create combined spectrum with fluorescence
    CombinedSpectrum combined;
    combined.absorption = absorption;
    combined.emission = EmissionSpectrum::from_absorption(absorption);
    
    combined.export_html("benzene_spectrum.html", "Benzene");
    std::cout << "✓ Exported: benzene_spectrum.html\n";
}

// ============================================================================
// Example 2: Conjugated System - Simple Hückel Theory
// ============================================================================
void example_huckel() {
    std::cout << "\n╔══════════════════════════════════════════════════════════╗\n";
    std::cout << "║   Example 2: Hückel Theory - Butadiene                  ║\n";
    std::cout << "╚══════════════════════════════════════════════════════════╝\n\n";
    
    // Compute π-system for butadiene (4 conjugated carbons)
    auto excitation = SimpleHuckel::compute_pi_system(4, false);
    
    std::cout << "Butadiene (CH2=CH-CH=CH2):\n";
    std::cout << "  HOMO-LUMO gap: " << std::fixed << std::setprecision(2)
              << excitation.get_homo_lumo_gap_ev() << " eV\n";
    
    if (!excitation.transitions.empty()) {
        const auto& first = excitation.transitions[0];
        std::cout << "  First transition: " << first.wavelength_nm 
                  << " nm (" << first.classify_by_wavelength() << ")\n";
    }
    
    // Compare cyclic vs linear
    auto cyclic = SimpleHuckel::compute_pi_system(6, true);   // Benzene
    auto linear = SimpleHuckel::compute_pi_system(6, false);  // Hexatriene
    
    std::cout << "\nHOMO-LUMO Gap Comparison (6 π-electrons):\n";
    std::cout << "  Benzene (cyclic):     " << cyclic.get_homo_lumo_gap_ev() << " eV\n";
    std::cout << "  Hexatriene (linear):  " << linear.get_homo_lumo_gap_ev() << " eV\n";
    std::cout << "  → Cyclic systems are more stable (larger gap)\n";
}

// ============================================================================
// Example 3: Carbonyl Chromophore
// ============================================================================
void example_carbonyl() {
    std::cout << "\n╔══════════════════════════════════════════════════════════╗\n";
    std::cout << "║   Example 3: Carbonyl Group (n→π* transition)           ║\n";
    std::cout << "╚══════════════════════════════════════════════════════════╝\n\n";
    
    auto excitation = ChromophoreLibrary::carbonyl();
    
    std::cout << "Carbonyl (C=O) Transitions:\n";
    for (const auto& trans : excitation.transitions) {
        std::cout << "  " << trans.type << ": "
                  << trans.wavelength_nm << " nm - "
                  << trans.classify_by_wavelength() << "\n";
    }
    
    auto absorption = AbsorptionSpectrum::from_excitation(excitation);
    
    // Export JSON
    auto json = absorption.to_json();
    std::ofstream json_file("carbonyl_spectrum.json");
    json_file << json;
    json_file.close();
    
    std::cout << "✓ Exported: carbonyl_spectrum.json\n";
}

// ============================================================================
// Example 4: Complete Workflow with Mock Molecule
// ============================================================================
void example_complete_workflow() {
    std::cout << "\n╔══════════════════════════════════════════════════════════╗\n";
    std::cout << "║   Example 4: Complete Workflow (Mock Molecule)          ║\n";
    std::cout << "╚══════════════════════════════════════════════════════════╝\n\n";
    
    // Create a mock benzene molecule (in real code, use builder)
    Molecule mol;
    
    // 12 atoms (6 C + 6 H)
    for (int i = 0; i < 6; ++i) {
        Atom c;
        c.Z = 6;  // Carbon
        c.mass = 12.011;
        mol.atoms.push_back(c);
        
        Atom h;
        h.Z = 1;  // Hydrogen
        h.mass = 1.008;
        mol.atoms.push_back(h);
    }
    
    // Hexagonal geometry (simplified)
    double r = 1.4;  // C-C bond length
    mol.coords.resize(36);  // 12 atoms × 3 coords
    
    for (int i = 0; i < 6; ++i) {
        double angle = i * 2.0 * M_PI / 6.0;
        // Carbon
        mol.coords[6*i + 0] = r * std::cos(angle);
        mol.coords[6*i + 1] = r * std::sin(angle);
        mol.coords[6*i + 2] = 0.0;
        // Hydrogen (outside)
        mol.coords[6*i + 3] = 2.0 * r * std::cos(angle);
        mol.coords[6*i + 4] = 2.0 * r * std::sin(angle);
        mol.coords[6*i + 5] = 0.0;
    }
    
    // Add bonds (simplified)
    for (int i = 0; i < 6; ++i) {
        mol.bonds.push_back({static_cast<uint32_t>(2*i), 
                            static_cast<uint32_t>(2*((i+1)%6))});  // C-C ring
        mol.bonds.push_back({static_cast<uint32_t>(2*i), 
                            static_cast<uint32_t>(2*i+1)});        // C-H
    }
    
    std::cout << "Created mock benzene molecule:\n";
    std::cout << "  Atoms: " << mol.num_atoms() << "\n";
    std::cout << "  Bonds: " << mol.bonds.size() << "\n\n";
    
    // Analyze with quantum module
    QuantumMoleculeData qm_data;
    qm_data.attach(mol);
    
    // Use library data
    qm_data.excitation = ChromophoreLibrary::benzene();
    qm_data.spectrum.absorption = AbsorptionSpectrum::from_excitation(qm_data.excitation);
    qm_data.spectrum.emission = EmissionSpectrum::from_absorption(qm_data.spectrum.absorption);
    qm_data.has_quantum_data = true;
    
    std::cout << "Quantum Analysis:\n";
    std::cout << "  Electronic states: " << qm_data.excitation.states.size() << "\n";
    std::cout << "  Transitions: " << qm_data.excitation.transitions.size() << "\n";
    std::cout << "  HOMO-LUMO gap: " << qm_data.excitation.get_homo_lumo_gap_ev() 
              << " eV\n";
    
    // Export all formats
    std::cout << "\nExporting...\n";
    
    // Note: This would fail without proper PeriodicTable, but shows the API
    // In real code: QuantumWebExport::export_with_spectrum(mol, qm_data, "benzene_full.html");
    
    QuantumDataExport::save_json(qm_data, "benzene_quantum.json");
    std::cout << "✓ Exported: benzene_quantum.json\n";
    
    qm_data.spectrum.absorption.export_csv("benzene_abs.csv");
    std::cout << "✓ Exported: benzene_abs.csv\n";
    
    qm_data.spectrum.emission.export_csv("benzene_em.csv");
    std::cout << "✓ Exported: benzene_em.csv\n";
}

// ============================================================================
// Example 5: Spectral Line Shapes
// ============================================================================
void example_line_shapes() {
    std::cout << "\n╔══════════════════════════════════════════════════════════╗\n";
    std::cout << "║   Example 5: Spectral Line Broadening                   ║\n";
    std::cout << "╚══════════════════════════════════════════════════════════╝\n\n";
    
    auto excitation = ChromophoreLibrary::benzene();
    
    // Test different line shapes
    std::vector<std::pair<LineShape, std::string>> shapes = {
        {LineShape::Gaussian, "Gaussian"},
        {LineShape::Lorentzian, "Lorentzian"},
        {LineShape::Voigt, "Voigt"}
    };
    
    for (const auto& [shape, name] : shapes) {
        AbsorptionSpectrum spec = AbsorptionSpectrum::from_excitation(excitation);
        spec.shape = shape;
        spec.fwhm_nm = 20.0;
        spec.generate_spectrum();
        
        // Find peak intensity
        double max_intensity = 0.0;
        for (double I : spec.intensities) {
            max_intensity = std::max(max_intensity, I);
        }
        
        std::cout << name << " broadening:\n";
        std::cout << "  Peak intensity: " << std::setprecision(3) << max_intensity << "\n";
        std::cout << "  FWHM: " << spec.fwhm_nm << " nm\n";
        
        spec.export_csv("benzene_" + name + ".csv");
        std::cout << "  ✓ Exported: benzene_" << name << ".csv\n\n";
    }
}

// ============================================================================
// Main
// ============================================================================
int main() {
    std::cout << R"(
╔════════════════════════════════════════════════════════════════╗
║                                                                ║
║   VSEPR-Sim Quantum Module - Complete Workflow Example        ║
║   Version 1.0 - January 17, 2026                              ║
║                                                                ║
╚════════════════════════════════════════════════════════════════╝
)";
    
    try {
        example_benzene();
        example_huckel();
        example_carbonyl();
        example_line_shapes();
        example_complete_workflow();
        
        std::cout << R"(
╔════════════════════════════════════════════════════════════════╗
║                                                                ║
║   ✓ All Examples Completed Successfully                       ║
║                                                                ║
║   Output Files:                                                ║
║   - benzene_absorption.csv                                     ║
║   - benzene_spectrum.html                                      ║
║   - carbonyl_spectrum.json                                     ║
║   - benzene_Gaussian.csv                                       ║
║   - benzene_Lorentzian.csv                                     ║
║   - benzene_Voigt.csv                                          ║
║   - benzene_quantum.json                                       ║
║   - benzene_abs.csv                                            ║
║   - benzene_em.csv                                             ║
║                                                                ║
║   Open benzene_spectrum.html in a browser to view!            ║
║                                                                ║
╚════════════════════════════════════════════════════════════════╝
)";
        
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "\n✗ ERROR: " << e.what() << "\n";
        return 1;
    }
}
