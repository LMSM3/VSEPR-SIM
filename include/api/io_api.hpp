/**
 * I/O API - App-Facing Façade
 * 
 * Clean contract for loading/saving molecular structures
 * 
 * Contracts:
 * - Units: Ångströms for positions, e for charge, Å/fs for velocity
 * - Coordinate conventions: Right-handed Cartesian
 * - Indexing: 0-based for all arrays (atoms, bonds)
 * - Bond detection: Covalent radius sum × 1.2 threshold
 */

#pragma once

#include "core/error.hpp"
#include "io/xyz_format.hpp"
#include <string>
#include <vector>

namespace vsepr {
namespace api {

// ============================================================================
// Format Detection
// ============================================================================

enum class MoleculeFormat {
    XYZ,      // Standard XYZ
    XYZA,     // Extended XYZ with analysis
    XYZC,     // Thermal pathway trajectories
    UNKNOWN
};

/**
 * Detect file format from extension or content
 */
MoleculeFormat detect_format(const std::string& filename);

// ============================================================================
// Loading Functions
// ============================================================================

/**
 * Load molecule from file with automatic format detection
 * 
 * @param filename Path to file
 * @param detect_bonds Whether to auto-detect bonds (default: true)
 * @return Result with loaded molecule or error
 */
Result<io::XYZMolecule> load_molecule(
    const std::string& filename,
    bool detect_bonds = true
);

/**
 * Load molecule from specific format
 * 
 * @param filename Path to file
 * @param format Explicit format to use
 * @param detect_bonds Whether to auto-detect bonds
 * @return Result with loaded molecule or error
 */
Result<io::XYZMolecule> load_molecule_as(
    const std::string& filename,
    MoleculeFormat format,
    bool detect_bonds = true
);

/**
 * Load trajectory (multiple frames)
 * 
 * @param filename Path to multi-frame file
 * @return Result with trajectory or error
 */
Result<io::XYZTrajectory> load_trajectory(const std::string& filename);

// ============================================================================
// Saving Functions
// ============================================================================

/**
 * Save molecule to file with format auto-detected from extension
 * 
 * @param filename Output path
 * @param molecule Molecule to save
 * @param include_bonds Whether to include bond connectivity (for .xyzA)
 * @return Status indicating success or error
 */
Status save_molecule(
    const std::string& filename,
    const io::XYZMolecule& molecule,
    bool include_bonds = false
);

/**
 * Save trajectory to file
 * 
 * @param filename Output path
 * @param trajectory Trajectory to save
 * @return Status indicating success or error
 */
Status save_trajectory(
    const std::string& filename,
    const io::XYZTrajectory& trajectory
);

// ============================================================================
// Validation Functions
// ============================================================================

/**
 * Validate molecule geometry
 * Checks:
 * - Valid elements (from canonical periodic table)
 * - Reasonable distances (min 0.5 Å)
 * - Valid bond indices
 * 
 * @param molecule Molecule to validate
 * @return Status with detailed error if validation fails
 */
Status validate_geometry(const io::XYZMolecule& molecule);

/**
 * Validate units and coordinate system
 * Ensures molecule conforms to API contracts
 * 
 * @param molecule Molecule to validate
 * @return Status with error if assumptions violated
 */
Status validate_units_assumed(const io::XYZMolecule& molecule);

// ============================================================================
// Bond Operations
// ============================================================================

/**
 * Detect bonds based on covalent radii
 * 
 * @param molecule Molecule to analyze (modified in-place)
 * @param scale_factor Threshold multiplier (default: 1.2)
 * @return Number of bonds detected
 */
int detect_bonds(io::XYZMolecule& molecule, double scale_factor = 1.2);

/**
 * Validate existing bonds
 * Checks that bond indices are valid and distances reasonable
 * 
 * @param molecule Molecule with bonds
 * @return Status indicating if bonds are valid
 */
Status validate_bonds(const io::XYZMolecule& molecule);

// ============================================================================
// Property Queries
// ============================================================================

/**
 * Compute molecular formula (e.g., "C6H6")
 * 
 * @param molecule Input molecule
 * @return Formula string (Hill notation: C, H, then alphabetical)
 */
std::string compute_formula(const io::XYZMolecule& molecule);

/**
 * Compute molecular mass in amu
 * 
 * @param molecule Input molecule
 * @return Molecular mass (uses canonical periodic table)
 */
double compute_molecular_mass(const io::XYZMolecule& molecule);

/**
 * Compute center of mass
 * 
 * @param molecule Input molecule
 * @return {x, y, z} in Ångströms
 */
std::array<double, 3> compute_center_of_mass(const io::XYZMolecule& molecule);

// ============================================================================
// Error Recovery
// ============================================================================

/**
 * Attempt to fix common issues in loaded molecule
 * - Remove duplicate atoms
 * - Fix invalid bond indices
 * - Sanitize element symbols
 * 
 * @param molecule Molecule to repair (modified in-place)
 * @return Status describing what was fixed
 */
Status attempt_repair(io::XYZMolecule& molecule);

}} // namespace vsepr::api
