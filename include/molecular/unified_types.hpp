/**
 * VSEPR-Sim Unified Molecular Data Types
 * Consolidates all molecular data structures across the codebase
 * Eliminates duplicates and provides single source of truth
 * Version: 2.0.0
 */

#pragma once

#include "core/types.hpp"
#include "subsystem/metallic_sim.hpp"
#include <string>
#include <vector>

namespace vsepr {

// Forward declaration
class Molecule;

namespace molecular {

// ============================================================================
// Core Molecular Data (from original VSEPR code)
// ============================================================================

// NOTE: The primary Molecule class is defined in src/sim/molecule.hpp
// This file provides lightweight data transfer objects (DTOs) for UI/integration

// ============================================================================
// Molecular Metadata (for GUI/Display)
// ============================================================================

/**
 * Lightweight molecular metadata for GUI display and data transfer
 * Does NOT replace the full Molecule class - use for UI components only
 */
struct MolecularMetadata {
    std::string id;              // Unique identifier
    std::string formula;         // Chemical formula (H₂O, NH₃, etc.)
    std::string name;            // Common name (Water, Ammonia, etc.)
    std::string geometry;        // VSEPR geometry (tetrahedral, bent, etc.)
    
    // Structural data
    int atom_count = 0;
    int bond_count = 0;
    int angle_count = 0;
    int torsion_count = 0;
    
    // Computed properties
    double energy = 0.0;                 // Total energy (kJ/mol)
    double binding_energy = 0.0;         // Binding energy (kJ/mol)
    double strain_energy = 0.0;          // Strain energy (kJ/mol)
    double molecular_mass = 0.0;         // Molecular mass (amu)
    
    // Classification
    std::string category;                // Category (hydride, halogen, etc.)
    int phase = 0;                       // Development phase (1, 2, 3, etc.)
    
    // Testing status
    bool tested = false;
    bool success = false;
    std::string test_date;
    std::string test_status;
    std::string error_message;
    
    // Default constructor
    MolecularMetadata() = default;
    
    // Convenience constructor for simple molecules
    MolecularMetadata(const std::string& id_, const std::string& formula_, 
                     double energy_, int atoms, int bonds)
        : id(id_), formula(formula_), atom_count(atoms), bond_count(bonds), energy(energy_) {}
};

// ============================================================================
// Materials Integration Data
// ============================================================================

/**
 * Links molecular data to materials properties
 * Used for organometallic complexes, catalysts, reactor design
 */
struct MolecularMaterialProperties {
    // Molecular data
    MolecularMetadata molecule;
    
    // Corresponding material (for containers, reactors, etc.)
    std::string material_name;
    subsystem::MechanicalProperties material_props;
    
    // Operating conditions
    double operating_temperature_K = 298.15;
    double operating_pressure_MPa = 0.101325;
    
    // Safety analysis
    double safety_factor = 0.0;
    bool safe_for_use = false;
    std::string failure_mode;
    std::string recommendation;
    
    // Compatibility flags
    bool corrosion_resistant = false;
    bool high_temperature_stable = false;
    bool pressure_rated = false;
};

// ============================================================================
// Pokedex Entry (Database Record)
// ============================================================================

/**
 * Complete database entry for molecular Pokedex
 * BACKWARD COMPATIBLE: Maintains flat structure for easy initialization
 */
struct PokedexEntry {
    // Direct fields (for backward compatibility with existing code)
    std::string id;
    std::string formula;
    std::string name;
    std::string category;
    int phase = 0;
    bool tested = false;
    bool success = false;
    double energy = 0.0;
    int atom_count = 0;
    int bond_count = 0;
    std::string geometry;
    std::string test_date;
    std::string test_status;
    
    // Additional Pokedex-specific fields
    bool favorite = false;
    int view_count = 0;
    std::string notes;
    std::vector<std::string> tags;
    
    // Convert to/from MolecularMetadata
    MolecularMetadata to_metadata() const {
        MolecularMetadata meta;
        meta.id = id;
        meta.formula = formula;
        meta.name = name;
        meta.category = category;
        meta.phase = phase;
        meta.tested = tested;
        meta.success = success;
        meta.energy = energy;
        meta.atom_count = atom_count;
        meta.bond_count = bond_count;
        meta.geometry = geometry;
        meta.test_date = test_date;
        meta.test_status = test_status;
        return meta;
    }
    
    static PokedexEntry from_metadata(const MolecularMetadata& meta) {
        PokedexEntry entry;
        entry.id = meta.id;
        entry.formula = meta.formula;
        entry.name = meta.name;
        entry.category = meta.category;
        entry.phase = meta.phase;
        entry.tested = meta.tested;
        entry.success = meta.success;
        entry.energy = meta.energy;
        entry.atom_count = meta.atom_count;
        entry.bond_count = meta.bond_count;
        entry.geometry = meta.geometry;
        entry.test_date = meta.test_date;
        entry.test_status = meta.test_status;
        return entry;
    }
};

// ============================================================================
// Conversion Utilities
// ============================================================================

/**
 * Convert full Molecule to lightweight metadata
 * Extracts display-relevant data from complete molecule object
 * 
 * NOTE: Implemented in source file to avoid circular dependency
 */
MolecularMetadata to_metadata(const Molecule& mol, 
                              const std::string& id = "",
                              const std::string& formula = "");

/**
 * Convert metadata to Pokedex entry
 */
inline PokedexEntry to_pokedex_entry(const MolecularMetadata& meta) {
    return PokedexEntry::from_metadata(meta);
}

// ============================================================================
// Type Aliases (for backward compatibility)
// ============================================================================

// Legacy name support (deprecated - use MolecularMetadata)
using MoleculeData [[deprecated("Use MolecularMetadata instead")]] = MolecularMetadata;

// Pokedex legacy support (use PokedexEntry)
using MoleculeEntry [[deprecated("Use PokedexEntry instead")]] = PokedexEntry;

} // namespace molecular
} // namespace vsepr
