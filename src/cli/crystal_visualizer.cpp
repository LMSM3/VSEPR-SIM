#include "crystal_visualizer.hpp"
#include <iostream>
#include <map>
#include <cmath>

#ifdef ENABLE_CRYSTAL_VIZ
#include "vis/crystal_grid.hpp"
#endif

namespace vsepr {
namespace cli {

// ============================================================================
// CPK Color Scheme (Corey-Pauling-Koltun)
// ============================================================================

std::array<uint8_t, 3> cpk_color(const std::string& element) {
    static const std::map<std::string, std::array<uint8_t, 3>> colors = {
        // Nonmetals
        {"H",  {255, 255, 255}},  // White
        {"C",  {144, 144, 144}},  // Gray
        {"N",  { 48,  80, 248}},  // Blue
        {"O",  {255,  13,  13}},  // Red
        {"F",  {144, 224,  80}},  // Green
        {"P",  {255, 128,   0}},  // Orange
        {"S",  {255, 255,  48}},  // Yellow
        {"Cl", { 31, 240,  31}},  // Green
        {"Br", {166,  41,  41}},  // Brown
        {"I",  {148,   0, 148}},  // Purple
        
        // Alkali/Alkaline earth
        {"Li", {204, 128, 255}},  // Violet
        {"Na", {171,  92, 242}},  // Purple
        {"Mg", {138, 255,   0}},  // Light green
        {"K",  {143,  64, 212}},  // Purple
        {"Ca", { 61, 255,   0}},  // Green
        
        // Transition metals
        {"Ti", {191, 194, 199}},  // Gray
        {"Fe", {224, 102,  51}},  // Orange-brown
        {"Cu", {200, 128,  51}},  // Copper
        {"Zn", {125, 128, 176}},  // Blue-gray
        {"Au", {255, 209,  35}},  // Gold
        
        // Post-transition metals
        {"Al", {191, 166, 166}},  // Silver-gray
        {"Si", { 61, 123, 196}},  // Blue-gray
        {"Cs", { 87,  23, 143}},  // Purple
    };
    
    auto it = colors.find(element);
    if (it != colors.end()) {
        return it->second;
    }
    
    // Default: light gray
    return {192, 192, 192};
}

// ============================================================================
// Covalent Radii (Cordero et al. 2008)
// ============================================================================

float covalent_radius(const std::string& element) {
    static const std::map<std::string, float> radii = {
        {"H",  0.31f}, {"He", 0.28f}, {"Li", 1.28f}, {"Be", 0.96f}, {"B",  0.84f},
        {"C",  0.76f}, {"N",  0.71f}, {"O",  0.66f}, {"F",  0.57f}, {"Ne", 0.58f},
        {"Na", 1.66f}, {"Mg", 1.41f}, {"Al", 1.21f}, {"Si", 1.11f}, {"P",  1.07f},
        {"S",  1.05f}, {"Cl", 1.02f}, {"Ar", 1.06f}, {"K",  2.03f}, {"Ca", 1.76f},
        {"Ti", 1.60f}, {"Fe", 1.32f}, {"Cu", 1.32f}, {"Zn", 1.22f}, {"Br", 1.20f},
        {"Au", 1.36f}, {"I",  1.39f}, {"Cs", 2.44f},
    };
    
    auto it = radii.find(element);
    if (it != radii.end()) {
        return it->second;
    }
    
    // Default: 1.5 Å
    return 1.5f;
}

// ============================================================================
// Conversion: CLI Atoms → render::CrystalStructure
// ============================================================================

render::CrystalStructure atoms_to_crystal_structure(
    const std::vector<Atom>& atoms,
    const atomistic::crystal::Lattice& lattice,
    const std::string& name,
    int space_group,
    const std::string& space_symbol)
{
    render::CrystalStructure structure;
    structure.name = name;
    structure.space_group_number = space_group;
    structure.space_group_symbol = space_symbol;
    
    // Convert atomistic::crystal::Lattice → render::LatticeVectors
    atomistic::Vec3 a_vec = lattice.A.col(0);
    atomistic::Vec3 b_vec = lattice.A.col(1);
    atomistic::Vec3 c_vec = lattice.A.col(2);
    
    structure.lattice.a = {a_vec.x, a_vec.y, a_vec.z};
    structure.lattice.b = {b_vec.x, b_vec.y, b_vec.z};
    structure.lattice.c = {c_vec.x, c_vec.y, c_vec.z};
    
    // Convert atoms: Cartesian → fractional
    structure.atoms.reserve(atoms.size());
    
    for (const auto& atom : atoms) {
        render::CrystalAtom crystal_atom;
        
        // Convert Cartesian to fractional
        atomistic::Vec3 cart = {atom.x, atom.y, atom.z};
        atomistic::Vec3 frac = lattice.to_fractional(cart);
        
        // Wrap to [0, 1)
        frac.x = frac.x - std::floor(frac.x);
        frac.y = frac.y - std::floor(frac.y);
        frac.z = frac.z - std::floor(frac.z);
        
        crystal_atom.fractional = {frac.x, frac.y, frac.z};
        crystal_atom.cartesian = {cart.x, cart.y, cart.z};
        
        // Map element symbol to atomic number
        static const std::map<std::string, uint8_t> elem_to_z = {
            {"H", 1}, {"He", 2}, {"Li", 3}, {"Be", 4}, {"B", 5}, {"C", 6},
            {"N", 7}, {"O", 8}, {"F", 9}, {"Ne", 10}, {"Na", 11}, {"Mg", 12},
            {"Al", 13}, {"Si", 14}, {"P", 15}, {"S", 16}, {"Cl", 17}, {"Ar", 18},
            {"K", 19}, {"Ca", 20}, {"Ti", 22}, {"Fe", 26}, {"Cu", 29}, {"Zn", 30},
            {"Br", 35}, {"Cs", 55}, {"Au", 79}, {"I", 53}
        };
        
        auto it = elem_to_z.find(atom.element);
        crystal_atom.atomic_number = (it != elem_to_z.end()) ? it->second : 0;
        
        // Visualization properties
        crystal_atom.color_rgb = cpk_color(atom.element);
        crystal_atom.radius = covalent_radius(atom.element);
        
        structure.atoms.push_back(crystal_atom);
    }
    
    return structure;
}

// ============================================================================
// Visualizer Launch
// ============================================================================

int launch_crystal_visualizer(
    const std::vector<Atom>& atoms,
    const atomistic::crystal::Lattice& lattice,
    const std::vector<int>& supercell,
    const std::string& name)
{
#ifndef ENABLE_CRYSTAL_VIZ
    std::cerr << "\n━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n";
    std::cerr << "  ERROR: Visualization Not Enabled\n";
    std::cerr << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n\n";
    std::cerr << "The --viz flag requires the project to be built with:\n";
    std::cerr << "  cmake -DBUILD_VIS=ON ...\n\n";
    std::cerr << "Falling back to XYZ output instead.\n\n";
    return 1;
#else
    std::cout << "\n━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n";
    std::cout << "  Launching Native Crystal Visualizer\n";
    std::cout << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n\n";

    // Convert to render format
    int na = supercell.empty() ? 1 : supercell[0];
    int nb = supercell.empty() ? 1 : supercell[1];
    int nc = supercell.empty() ? 1 : supercell[2];

    std::string full_name = name;
    if (!supercell.empty()) {
        full_name += " [" + std::to_string(na) + "×" + 
                     std::to_string(nb) + "×" + std::to_string(nc) + "]";
    }

    auto structure = atoms_to_crystal_structure(atoms, lattice, full_name);

    std::cout << "Structure: " << full_name << "\n";
    std::cout << "  Atoms: " << atoms.size() << "\n";
    std::cout << "  Lattice: " << lattice.a_len() << " × " 
              << lattice.b_len() << " × " << lattice.c_len() << " Å\n";
    std::cout << "  Angles: α=" << lattice.alpha_deg() << "°, "
              << "β=" << lattice.beta_deg() << "°, "
              << "γ=" << lattice.gamma_deg() << "°\n";
    std::cout << "  Volume: " << lattice.V << " Å³\n\n";

    // Create renderer
    render::CrystalGridRenderer renderer;

    // For supercells, the atoms are already replicated
    // So we pass the full structure and set replication to 1×1×1
    renderer.set_structure(structure);
    renderer.set_replication(1, 1, 1);  // Already expanded

    // Enable features
    renderer.show_polyhedra(true);
    renderer.show_cell_edges(true);
    renderer.set_polyhedron_opacity(0.3f);
    renderer.set_coordination_cutoff(3.5);

    std::cout << "Rendering features:\n";
    std::cout << "  ✓ Coordination polyhedra (translucent)\n";
    std::cout << "  ✓ Unit cell edges (cyan wireframe)\n";
    std::cout << "  ✓ CPK element coloring\n\n";

    std::cout << "Controls:\n";
    std::cout << "  Mouse drag: Rotate view\n";
    std::cout << "  Scroll: Zoom in/out\n";
    std::cout << "  ESC: Close window\n\n";

    std::cout << "Starting render loop...\n";
    std::cout << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n\n";

    try {
        // Launch renderer (blocks until window closed)
        renderer.render();

        std::cout << "\n✓ Visualizer closed successfully\n";
        return 0;

    } catch (const std::exception& e) {
        std::cerr << "\nERROR: Visualizer failed: " << e.what() << "\n";
        std::cerr << "Note: Native visualization requires OpenGL 3.3+ and GLFW\n";
        return 1;
    }
#endif
}

} // namespace cli
} // namespace vsepr
