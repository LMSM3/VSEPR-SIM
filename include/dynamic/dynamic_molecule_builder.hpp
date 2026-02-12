/**
 * dynamic_molecule_builder.hpp
 * =============================
 * Dynamic complex molecule generator with live .xyz export
 * Maps FEA::Element concepts to chemical elements
 * Creates compounds with up to 101 atoms
 */

#pragma once

#include "sim/molecule.hpp"
#include "core/types.hpp"
#include "core/element_data.hpp"
#include <string>
#include <vector>
#include <fstream>
#include <random>
#include <functional>
#include <map>

namespace vsepr {
namespace dynamic {

// ============================================================================
// Element Letter Mapping (periodic table symbols)
// ============================================================================

struct ElementSpec {
    uint8_t Z;              // Atomic number
    std::string symbol;     // H, C, N, O, etc.
    int valence;            // Typical bonding capacity
    double electronegativity;
};

// Letter-based element selection (A-Z mapping)
class ElementMapper {
    std::map<char, ElementSpec> letter_map_;
    
public:
    ElementMapper();
    
    // Get element by letter (A=Aluminum, C=Carbon, H=Hydrogen, etc.)
    ElementSpec get_by_letter(char letter) const;
    
    // Get random element from period 2-4 (common elements)
    ElementSpec get_random_common() const;
    
    // Map FEA element ID to chemical element
    ElementSpec from_fea_id(int fea_element_id) const;
};

// ============================================================================
// Complex Molecule Templates
// ============================================================================

enum class MoleculeComplexity {
    SIMPLE,          // 3-10 atoms
    MEDIUM,          // 11-30 atoms
    COMPLEX,         // 31-60 atoms
    VERY_COMPLEX,    // 61-101 atoms
};

struct MoleculeTemplate {
    std::string name;
    std::string formula;
    MoleculeComplexity complexity;
    std::vector<uint8_t> atom_types;  // Z values
    std::function<Molecule()> builder;
};

// ============================================================================
// Dynamic Molecule Generator
// ============================================================================

class DynamicMoleculeGenerator {
    std::mt19937 rng_;
    ElementMapper mapper_;
    const ElementDatabase& elem_db_;
    
    std::string current_xyz_path_;
    std::ofstream xyz_stream_;
    bool live_export_enabled_ = true;
    
public:
    DynamicMoleculeGenerator();
    
    // Generate molecules of varying complexity
    Molecule generate_simple();
    Molecule generate_medium();
    Molecule generate_complex();
    Molecule generate_very_complex();
    
    // Generate specific compound types
    Molecule generate_alkane(int n_carbons);           // CₙH₂ₙ₊₂
    Molecule generate_alkene(int n_carbons);           // CₙH₂ₙ
    Molecule generate_alkyne(int n_carbons);           // CₙH₂ₙ₋₂
    Molecule generate_alcohol(int n_carbons);          // CₙH₂ₙ₊₁OH
    Molecule generate_amine(int n_carbons);            // CₙH₂ₙ₊₁NH₂
    Molecule generate_carboxylic_acid(int n_carbons);  // CₙH₂ₙ₊₁COOH
    
    // Generate aromatic compounds
    Molecule generate_benzene_derivative(const std::vector<uint8_t>& substituents);
    Molecule generate_naphthalene();
    Molecule generate_anthracene();
    
    // Generate heterocycles
    Molecule generate_pyridine();
    Molecule generate_furan();
    Molecule generate_thiophene();
    Molecule generate_imidazole();
    
    // Generate coordination complexes
    Molecule generate_metal_complex(uint8_t metal_Z, int coordination_number);
    
    // Generate biomolecules
    Molecule generate_amino_acid(const std::string& type);
    Molecule generate_sugar(int n_carbons);
    Molecule generate_peptide(int n_residues);
    
    // Generate by element letters (custom)
    Molecule generate_from_letters(const std::string& element_letters);
    
    // Generate crystal structures (up to 101 atoms)
    Molecule generate_monazite_unit_cell();             // CePO₄ primitive (6 atoms)
    Molecule generate_monazite_supercell(int nx=2, int ny=2, int nz=4);  // 96 atoms
    
    Molecule generate_rocksalt_unit_cell();             // NaCl primitive (8 atoms)
    Molecule generate_rocksalt_supercell(int nx=5, int ny=5, int nz=4);  // 100 atoms
    
    // Live .xyz export
    void enable_live_export(const std::string& xyz_path);
    void disable_live_export();
    void export_current(const Molecule& mol, const std::string& comment = "");
    
    // Analyze current bunch
    struct AtomAnalysis {
        std::map<uint8_t, int> atom_counts;      // Z → count
        std::map<uint8_t, std::string> atom_symbols;
        int total_atoms;
        int total_bonds;
        double avg_bond_length;
        std::string molecular_formula;
    };
    
    AtomAnalysis analyze_molecule(const Molecule& mol) const;
    void print_analysis(const AtomAnalysis& analysis) const;
    
private:
    // Helper methods
    void add_carbon_chain(Molecule& mol, int n_carbons, bool linear = true);
    void add_hydrogens_to_carbons(Molecule& mol);
    void add_functional_group(Molecule& mol, const std::string& group);
    
    // Geometry helpers
    void place_tetrahedral(Molecule& mol, size_t center, const std::vector<size_t>& neighbors);
    void place_trigonal_planar(Molecule& mol, size_t center, const std::vector<size_t>& neighbors);
    void place_linear(Molecule& mol, size_t center, const std::vector<size_t>& neighbors);
    
    // Live export
    void write_xyz_header(const Molecule& mol, const std::string& comment);
    void write_xyz_atoms(const Molecule& mol);
};

// ============================================================================
// Live XYZ Monitor
// ============================================================================

class LiveXYZMonitor {
    std::string watched_file_;
    std::function<void(const Molecule&)> on_update_;
    bool watching_ = false;
    
public:
    void start_watching(const std::string& xyz_path,
                       std::function<void(const Molecule&)> callback);
    
    void stop_watching();
    
    // Parse .xyz file and create Molecule
    static Molecule parse_xyz(const std::string& filepath);
    
private:
    void watch_thread();
};

// ============================================================================
// Batch Generator (for testing)
// ============================================================================

class BatchMoleculeGenerator {
public:
    // Generate N molecules of each type
    std::vector<Molecule> generate_alkane_series(int min_C, int max_C);
    std::vector<Molecule> generate_alkene_series(int min_C, int max_C);
    std::vector<Molecule> generate_alcohol_series(int min_C, int max_C);
    
    // Generate library of heterocycles
    std::vector<Molecule> generate_heterocycle_library();
    
    // Generate all amino acids
    std::vector<Molecule> generate_amino_acid_library();
    
    // Export batch to directory
    void export_batch(const std::vector<Molecule>& molecules, const std::string& dir);
};

} // namespace dynamic
} // namespace vsepr
