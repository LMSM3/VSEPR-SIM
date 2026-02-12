/**
 * crystal-viewer.cpp
 * ------------------
 * Crystallographic grid visualization demo
 * 
 * Showcases:
 * - Al FCC with 12-fold coordination polyhedra
 * - Fe BCC with 8-fold coordination
 * - NaCl rocksalt
 * - Si diamond
 * 
 * Interactive controls:
 * - Arrow keys: Switch between crystal structures
 * - P: Toggle coordination polyhedra
 * - C: Toggle unit cell edges
 * - +/-: Adjust polyhedron opacity
 * - 1-5: Change supercell size (1×1×1 to 5×5×5)
 */

#include "vis/crystal_grid.hpp"
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <iostream>

using namespace vsepr::render;

// Global state
CrystalGridRenderer renderer;
std::vector<CrystalStructure> structures;
size_t current_structure_idx = 0;

void init_structures() {
    structures.push_back(crystals::aluminum_fcc());
    structures.push_back(crystals::iron_bcc());
    structures.push_back(crystals::sodium_chloride());
    structures.push_back(crystals::silicon_diamond());
    
    std::cout << "Loaded " << structures.size() << " crystal structures:\n";
    for (size_t i = 0; i < structures.size(); ++i) {
        std::cout << "  " << i << ": " << structures[i].name << "\n";
    }
}

void load_structure(size_t idx) {
    if (idx >= structures.size()) return;
    
    current_structure_idx = idx;
    const auto& structure = structures[idx];
    
    std::cout << "\n=== " << structure.name << " ===\n";
    std::cout << "Space group: " << structure.space_group_symbol 
              << " (#" << structure.space_group_number << ")\n";
    
    double a, b, c, alpha, beta, gamma;
    structure.lattice.get_parameters(a, b, c, alpha, beta, gamma);
    std::cout << "Lattice: a=" << a << " b=" << b << " c=" << c << " Å\n";
    std::cout << "         α=" << alpha << "° β=" << beta << "° γ=" << gamma << "°\n";
    std::cout << "Atoms in unit cell: " << structure.atoms.size() << "\n";
    
    // Set coordination cutoff based on structure
    double cutoff = 3.5;  // Default
    if (structure.name == "Al FCC") {
        cutoff = 3.0;  // Al-Al nearest neighbor ~ 2.86 Å
    } else if (structure.name == "Fe BCC") {
        cutoff = 2.8;  // Fe-Fe nearest neighbor ~ 2.48 Å
    } else if (structure.name == "NaCl") {
        cutoff = 3.2;  // Na-Cl ~ 2.82 Å
    } else if (structure.name == "Si") {
        cutoff = 2.6;  // Si-Si ~ 2.35 Å
    }
    
    renderer.set_coordination_cutoff(cutoff);
    renderer.set_structure(structure);
    
    auto polyhedra = structure.find_coordination_polyhedra(cutoff);
    std::cout << "Coordination polyhedra: " << polyhedra.size() << "\n";
    if (!polyhedra.empty()) {
        std::cout << "  Central atom has " << polyhedra[0].neighbor_indices.size() 
                  << " neighbors\n";
    }
}

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (action != GLFW_PRESS) return;
    
    switch (key) {
        case GLFW_KEY_ESCAPE:
            glfwSetWindowShouldClose(window, GLFW_TRUE);
            break;
            
        case GLFW_KEY_RIGHT:
            // Next structure
            load_structure((current_structure_idx + 1) % structures.size());
            break;
            
        case GLFW_KEY_LEFT:
            // Previous structure
            load_structure((current_structure_idx + structures.size() - 1) % structures.size());
            break;
            
        case GLFW_KEY_P:
            // Toggle polyhedra
            {
                static bool show = true;
                show = !show;
                renderer.show_polyhedra(show);
                std::cout << "Polyhedra: " << (show ? "ON" : "OFF") << "\n";
            }
            break;
            
        case GLFW_KEY_C:
            // Toggle cell edges
            {
                static bool show = true;
                show = !show;
                renderer.show_cell_edges(show);
                std::cout << "Cell edges: " << (show ? "ON" : "OFF") << "\n";
            }
            break;
            
        case GLFW_KEY_EQUAL:  // + key
        case GLFW_KEY_KP_ADD:
            // Increase opacity
            {
                static float opacity = 0.5f;
                opacity = std::min(1.0f, opacity + 0.1f);
                renderer.set_polyhedron_opacity(opacity);
                std::cout << "Polyhedron opacity: " << opacity << "\n";
            }
            break;
            
        case GLFW_KEY_MINUS:
        case GLFW_KEY_KP_SUBTRACT:
            // Decrease opacity
            {
                static float opacity = 0.5f;
                opacity = std::max(0.0f, opacity - 0.1f);
                renderer.set_polyhedron_opacity(opacity);
                std::cout << "Polyhedron opacity: " << opacity << "\n";
            }
            break;
            
        case GLFW_KEY_1:
            renderer.set_replication(1, 1, 1);
            load_structure(current_structure_idx);
            std::cout << "Supercell: 1×1×1\n";
            break;
            
        case GLFW_KEY_2:
            renderer.set_replication(2, 2, 2);
            load_structure(current_structure_idx);
            std::cout << "Supercell: 2×2×2\n";
            break;
            
        case GLFW_KEY_3:
            renderer.set_replication(3, 3, 3);
            load_structure(current_structure_idx);
            std::cout << "Supercell: 3×3×3\n";
            break;
            
        case GLFW_KEY_4:
            renderer.set_replication(4, 4, 4);
            load_structure(current_structure_idx);
            std::cout << "Supercell: 4×4×4\n";
            break;
            
        case GLFW_KEY_5:
            renderer.set_replication(5, 5, 5);
            load_structure(current_structure_idx);
            std::cout << "Supercell: 5×5×5\n";
            break;
    }
}

int main() {
    std::cout << "╔══════════════════════════════════════════════════════════╗\n";
    std::cout << "║  Crystallographic Grid Visualization                    ║\n";
    std::cout << "╠══════════════════════════════════════════════════════════╣\n";
    std::cout << "║  Mathematical crystal rendering with coordination       ║\n";
    std::cout << "║  polyhedra and inverted-RGB coloring.                   ║\n";
    std::cout << "╚══════════════════════════════════════════════════════════╝\n\n";
    
    // Initialize GLFW
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW\n";
        return 1;
    }
    
    // Create window
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_SAMPLES, 4);  // 4x MSAA
    
    GLFWwindow* window = glfwCreateWindow(1280, 720, "Crystal Grid Viewer", nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create window\n";
        glfwTerminate();
        return 1;
    }
    
    glfwMakeContextCurrent(window);
    glfwSetKeyCallback(window, key_callback);
    
    // Initialize GLEW
    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) {
        std::cerr << "Failed to initialize GLEW\n";
        return 1;
    }
    
    // OpenGL settings
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_MULTISAMPLE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    // Initialize structures
    init_structures();
    
    // Set default (3×3×3 supercell like reference image)
    renderer.set_replication(3, 3, 3);
    load_structure(0);  // Start with Al FCC
    
    // Print controls
    std::cout << "\n=== Controls ===\n";
    std::cout << "  ←/→     : Switch crystal structure\n";
    std::cout << "  P       : Toggle coordination polyhedra\n";
    std::cout << "  C       : Toggle unit cell edges\n";
    std::cout << "  +/-     : Adjust polyhedron opacity\n";
    std::cout << "  1-5     : Change supercell size (1×1×1 to 5×5×5)\n";
    std::cout << "  ESC     : Quit\n\n";
    
    // Main loop
    while (!glfwWindowShouldClose(window)) {
        // Clear
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        
        // Render crystal
        renderer.render();
        
        // Swap and poll
        glfwSwapBuffers(window);
        glfwPollEvents();
    }
    
    // Cleanup
    glfwDestroyWindow(window);
    glfwTerminate();
    
    return 0;
}
