#pragma once
/**
 * crystal_visualizer.hpp
 * ----------------------
 * Bridge between CLI crystal emission and native OpenGL renderer.
 * 
 * Converts CLI atom format (Cartesian) → render::CrystalStructure (fractional)
 * Launches CrystalGridRenderer with proper lattice and supercell parameters.
 */

#include "cli/emit_output.hpp"
#include "atomistic/crystal/lattice.hpp"
#include "vis/crystal_grid.hpp"
#include <vector>
#include <string>

namespace vsepr {
namespace cli {

/**
 * Convert CLI atoms (Cartesian) to render::CrystalStructure (fractional).
 * 
 * @param atoms         CLI atoms in Cartesian coordinates
 * @param lattice       Lattice used to generate atoms
 * @param name          Structure name (e.g., "NaCl 2x2x2")
 * @param space_group   Space group number (0 if unknown)
 * @param space_symbol  Space group symbol (empty if unknown)
 * @return              CrystalStructure ready for rendering
 */
render::CrystalStructure atoms_to_crystal_structure(
    const std::vector<Atom>& atoms,
    const atomistic::crystal::Lattice& lattice,
    const std::string& name,
    int space_group = 0,
    const std::string& space_symbol = ""
);

/**
 * Launch native visualizer for crystal structure.
 * 
 * @param atoms         Generated atoms
 * @param lattice       Lattice vectors
 * @param supercell     Supercell replication (na, nb, nc), empty if unit cell
 * @param name          Structure name
 * @return              0 on success, non-zero on error
 */
int launch_crystal_visualizer(
    const std::vector<Atom>& atoms,
    const atomistic::crystal::Lattice& lattice,
    const std::vector<int>& supercell,
    const std::string& name
);

/**
 * CPK color scheme for elements (RGB 0-255).
 * Returns default gray {128,128,128} for unknown elements.
 */
std::array<uint8_t, 3> cpk_color(const std::string& element);

/**
 * Covalent radius for element (Angstroms).
 * Returns 1.5 Å for unknown elements.
 */
float covalent_radius(const std::string& element);

} // namespace cli
} // namespace vsepr
