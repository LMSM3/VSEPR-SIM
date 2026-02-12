/**
 * VSEPR-Sim Live GUI Integration
 * Connects ImGui interface to actual VSEPR molecular simulation engine
 * NOW WITH 3D VISUALIZATION! (OpenGL spheres + cylinders)
 * NOW WITH DYNAMIC MOLECULE GENERATION! (Complex compounds up to 101 atoms)
 * LIVE .XYZ EXPORT! (Updates in real-time)
 * 
 * This bridges the GUI (elevated_gui_app.cpp) with the core simulation
 * (src/sim/molecule.hpp, energy models, optimization)
 */

#include "gui/imgui_integration.hpp"
#include "gui/pokedex_gui.hpp"
#include "gui/data_pipe.hpp"
#include "sim/molecule.hpp"
#include "core/types.hpp"
#include "molecular/unified_types.hpp"
#include "render/molecular_renderer.hpp"
#include "dynamic/dynamic_molecule_builder.hpp"

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include <GLFW/glfw3.h>
#include <iostream>
#include <memory>
#include <vector>

using namespace vsepr;
using namespace vsepr::gui;
using namespace vsepr::pokedex;
using namespace vsepr::render;
using namespace vsepr::dynamic;

// ============================================================================
// Live VSEPR Integration State
// ============================================================================

struct VSEPRLiveState {
// Current molecule being simulated
Molecule current_molecule;
molecular::MolecularMetadata metadata;
    
// 3D Renderer
MolecularRenderer renderer;
InteractionHandler interaction;
    
// Dynamic Molecule Generator
DynamicMoleculeGenerator generator;
std::string xyz_export_path = "generated_molecule.xyz";
char element_letters_input[256] = "CCCCCHHHHHHHHHHH";  // Default: pentane-like
int carbon_count_alkane = 5;
int carbon_count_alkene = 5;
int carbon_count_alkyne = 5;
DynamicMoleculeGenerator::AtomAnalysis last_analysis;
    
// Simulation state
bool simulation_running = false;
bool optimization_running = false;
double current_energy = 0.0;
int optimization_step = 0;
    
// Visualization
bool show_bonds = true;
bool show_lone_pairs = false;
bool show_axes = true;
float atom_scale = 1.0f;
    
// Data pipes (reactive updates)
std::shared_ptr<DataPipe<Molecule>> molecule_pipe;
std::shared_ptr<DataPipe<double>> energy_pipe;
std::shared_ptr<DataPipe<std::string>> status_pipe;
    
VSEPRLiveState() {
        // Initialize data pipes
        molecule_pipe = std::make_shared<DataPipe<Molecule>>("live_molecule");
        energy_pipe = std::make_shared<DataPipe<double>>("live_energy");
        status_pipe = std::make_shared<DataPipe<std::string>>("live_status");
        
        // Register pipes
        PipeNetwork::instance().registerPipe("live_molecule", molecule_pipe);
        PipeNetwork::instance().registerPipe("live_energy", energy_pipe);
        PipeNetwork::instance().registerPipe("live_status", status_pipe);
        
        // Subscribe to updates
        molecule_pipe->subscribe([this](const Molecule& mol) {
            std::cout << "[LIVE] Molecule updated: " << mol.num_atoms() << " atoms\n";
        });
        
        energy_pipe->subscribe([this](double energy) {
            std::cout << "[LIVE] Energy: " << energy << " kcal/mol\n";
        });
        
        // Initialize renderer options
        renderer.options().show_atoms = true;
        renderer.options().show_bonds = true;
        renderer.options().show_axes = true;
        renderer.options().atom_scale = 0.5f;
        
        // Enable live .xyz export for dynamic generator
        generator.enable_live_export(xyz_export_path);
    }
};

// ============================================================================
// Build Test Molecules (Real VSEPR Code!)
// ============================================================================

Molecule build_water() {
    Molecule mol;
    
    // Add atoms (O at center, 2 H around it)
    mol.add_atom(8, 0.0, 0.0, 0.0);      // Oxygen (Z=8)
    mol.add_atom(1, 0.96, 0.0, 0.0);     // Hydrogen 1
    mol.add_atom(1, -0.24, 0.93, 0.0);   // Hydrogen 2
    
    // Add bonds
    mol.add_bond(0, 1, 1);  // O-H single bond
    mol.add_bond(0, 2, 1);  // O-H single bond
    
    // Generate angles automatically
    mol.generate_angles_from_bonds();
    
    return mol;
}

Molecule build_ammonia() {
    Molecule mol;
    
    // Add atoms (N at center, 3 H around it)
    mol.add_atom(7, 0.0, 0.0, 0.0);      // Nitrogen (Z=7)
    mol.add_atom(1, 1.01, 0.0, 0.0);     // Hydrogen 1
    mol.add_atom(1, -0.34, 0.95, 0.0);   // Hydrogen 2
    mol.add_atom(1, -0.34, -0.48, 0.83); // Hydrogen 3
    
    // Add bonds
    mol.add_bond(0, 1, 1);
    mol.add_bond(0, 2, 1);
    mol.add_bond(0, 3, 1);
    
    mol.generate_angles_from_bonds();
    
    return mol;
}

Molecule build_methane() {
    Molecule mol;
    
    // Add atoms (C at center, 4 H in tetrahedral arrangement)
    mol.add_atom(6, 0.0, 0.0, 0.0);       // Carbon (Z=6)
    mol.add_atom(1, 1.09, 0.0, 0.0);      // H1
    mol.add_atom(1, -0.36, 1.03, 0.0);    // H2
    mol.add_atom(1, -0.36, -0.52, 0.89);  // H3
    mol.add_atom(1, -0.36, -0.52, -0.89); // H4
    
    // Add bonds
    mol.add_bond(0, 1, 1);
    mol.add_bond(0, 2, 1);
    mol.add_bond(0, 3, 1);
    mol.add_bond(0, 4, 1);
    
    mol.generate_angles_from_bonds();
    
    return mol;
}

// ============================================================================
// GUI Rendering
// ============================================================================

void render_molecule_selector(VSEPRLiveState& state) {
    ImGui::Begin("Molecule Builder");
    
    ImGui::Text("Build Test Molecules:");
    ImGui::Separator();
    
    if (ImGui::Button("H₂O (Water)", ImVec2(200, 0))) {
        state.current_molecule = build_water();
        state.metadata.formula = "H2O";
        state.metadata.name = "Water";
        state.metadata.atom_count = 3;
        state.metadata.bond_count = 2;
        state.metadata.geometry = "Bent (V-shaped)";
        
        state.molecule_pipe->push(state.current_molecule);
        state.status_pipe->push("Built H₂O molecule");
    }
    
    if (ImGui::Button("NH₃ (Ammonia)", ImVec2(200, 0))) {
        state.current_molecule = build_ammonia();
        state.metadata.formula = "NH3";
        state.metadata.name = "Ammonia";
        state.metadata.atom_count = 4;
        state.metadata.bond_count = 3;
        state.metadata.geometry = "Trigonal Pyramidal";
        
        state.molecule_pipe->push(state.current_molecule);
        state.status_pipe->push("Built NH₃ molecule");
    }
    
    if (ImGui::Button("CH₄ (Methane)", ImVec2(200, 0))) {
        state.current_molecule = build_methane();
        state.metadata.formula = "CH4";
        state.metadata.name = "Methane";
        state.metadata.atom_count = 5;
        state.metadata.bond_count = 4;
        state.metadata.geometry = "Tetrahedral";
        
        state.molecule_pipe->push(state.current_molecule);
        state.status_pipe->push("Built CH₄ molecule");
    }
    
    ImGui::End();
}

void render_molecule_info(VSEPRLiveState& state) {
    ImGui::Begin("Molecule Info");
    
    if (state.current_molecule.num_atoms() > 0) {
        ImGui::Text("Formula: %s", state.metadata.formula.c_str());
        ImGui::Text("Name: %s", state.metadata.name.c_str());
        ImGui::Text("Geometry: %s", state.metadata.geometry.c_str());
        ImGui::Separator();
        
        ImGui::Text("Atoms: %lu", state.current_molecule.num_atoms());
        ImGui::Text("Bonds: %lu", state.current_molecule.num_bonds());
        ImGui::Text("Angles: %lu", state.current_molecule.angles.size());
        
        ImGui::Separator();
        
        ImGui::Text("Atom Details:");
        for (size_t i = 0; i < state.current_molecule.num_atoms(); i++) {
            double x, y, z;
            state.current_molecule.get_position(i, x, y, z);
            ImGui::Text("  [%lu] Z=%d  (%.3f, %.3f, %.3f)", 
                       i, state.current_molecule.atoms[i].Z, x, y, z);
        }
    } else {
        ImGui::Text("No molecule loaded");
        ImGui::Text("Select a molecule from Builder");
    }
    
    ImGui::End();
}

// ============================================================================
// 3D Molecular Viewer (OpenGL Rendering)
// ============================================================================

void render_3d_viewer(VSEPRLiveState& state, int /*width*/, int height) {
    ImGui::SetNextWindowSize(ImVec2(600, 600), ImGuiCond_FirstUseEver);
    ImGui::Begin("3D Molecular Viewer");
    
    ImVec2 canvas_pos = ImGui::GetCursorScreenPos();
    ImVec2 canvas_size = ImGui::GetContentRegionAvail();
    
    if (canvas_size.x > 50 && canvas_size.y > 50) {
        // Render molecule with OpenGL
        if (state.current_molecule.num_atoms() > 0) {
            // Save ImGui state
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
            
            // Get mouse interaction
            ImGuiIO& io = ImGui::GetIO();
            bool is_hovered = ImGui::IsWindowHovered();
            
            if (is_hovered) {
                // Mouse drag for rotation
                if (ImGui::IsMouseDragging(0)) {
                    ImVec2 delta = ImGui::GetMouseDragDelta(0);
                    state.renderer.camera().rotation_y += delta.x * 0.5f;
                    state.renderer.camera().rotation_x += delta.y * 0.5f;
                    ImGui::ResetMouseDragDelta(0);
                }
                
                // Mouse wheel for zoom
                if (io.MouseWheel != 0.0f) {
                    state.renderer.camera().zoom -= io.MouseWheel;
                    state.renderer.camera().zoom = std::max(1.0f, std::min(50.0f, state.renderer.camera().zoom));
                }
            }
            
            // Render with OpenGL (outside ImGui context)
            glPushAttrib(GL_ALL_ATTRIB_BITS);
            glViewport(canvas_pos.x, height - canvas_pos.y - canvas_size.y, canvas_size.x, canvas_size.y);
            
            // Clear this region
            glEnable(GL_SCISSOR_TEST);
            glScissor(canvas_pos.x, height - canvas_pos.y - canvas_size.y, canvas_size.x, canvas_size.y);
            glClearColor(0.15f, 0.15f, 0.15f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            
            // Render molecule
            state.renderer.render(state.current_molecule, canvas_size.x, canvas_size.y);
            
            glDisable(GL_SCISSOR_TEST);
            glPopAttrib();
            
            ImGui::PopStyleVar();
            
            // Draw info overlay
            ImGui::SetCursorScreenPos(ImVec2(canvas_pos.x + 10, canvas_pos.y + 10));
            ImGui::BeginChild("ViewerOverlay", ImVec2(200, 100), false, 
                             ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
            ImGui::TextColored(ImVec4(1,1,1,1), "Controls:");
            ImGui::TextColored(ImVec4(0.7f,0.7f,0.7f,1), "• Drag to rotate");
            ImGui::TextColored(ImVec4(0.7f,0.7f,0.7f,1), "• Scroll to zoom");
            ImGui::TextColored(ImVec4(0.7f,0.7f,0.7f,1), "• R to reset view");
            ImGui::EndChild();
            
        } else {
            // No molecule loaded
            ImVec2 text_pos = ImVec2(canvas_pos.x + canvas_size.x * 0.5f - 100,
                                    canvas_pos.y + canvas_size.y * 0.5f);
            ImGui::SetCursorScreenPos(text_pos);
            ImGui::TextDisabled("No molecule loaded");
            ImGui::SetCursorScreenPos(ImVec2(text_pos.x - 50, text_pos.y + 20));
            ImGui::TextDisabled("Build a molecule to see 3D view");
        }
    }
    
    ImGui::End();
}

void render_visualization_controls(VSEPRLiveState& state) {
    ImGui::Begin("Visualization");
    
    ImGui::Text("Display Options:");
    ImGui::Checkbox("Show Bonds", &state.renderer.options().show_bonds);
    ImGui::Checkbox("Show Axes", &state.renderer.options().show_axes);
    ImGui::Checkbox("CPK Colors", &state.renderer.options().use_cpk_colors);
    
    ImGui::Separator();
    
    ImGui::Text("Atom Scale:");
    ImGui::SliderFloat("##atom_scale", &state.renderer.options().atom_scale, 0.1f, 1.0f);
    
    ImGui::Text("Bond Radius:");
    ImGui::SliderFloat("##bond_radius", &state.renderer.options().bond_radius, 0.05f, 0.3f);
    
    ImGui::Separator();
    
    ImGui::Text("Camera:");
    ImGui::Text("  Zoom: %.1f", state.renderer.camera().zoom);
    ImGui::Text("  Rotation: (%.0f°, %.0f°)", 
               state.renderer.camera().rotation_x,
               state.renderer.camera().rotation_y);
    
    if (ImGui::Button("Reset Camera", ImVec2(-1, 0))) {
        state.renderer.camera().zoom = 10.0f;
        state.renderer.camera().rotation_x = 0.0f;
        state.renderer.camera().rotation_y = 0.0f;
        state.renderer.camera().pan_x = 0.0f;
        state.renderer.camera().pan_y = 0.0f;
        state.status_pipe->push("Camera reset");
    }
    
    ImGui::End();
}

// ============================================================================
// Dynamic Molecule Builder Panel
// ============================================================================

void render_dynamic_builder_panel(VSEPRLiveState& state) {
    ImGui::SetNextWindowSize(ImVec2(400, 600), ImGuiCond_FirstUseEver);
    ImGui::Begin("Dynamic Molecule Generator");
    
    ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.5f, 1.0f), "COMPLEX COMPOUND GENERATOR");
    ImGui::Text("Create molecules up to 101 atoms");
    ImGui::Separator();
    
    // ========== ALKANE GENERATOR ==========
    ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "Alkanes (CₙH₂ₙ₊₂)");
    ImGui::SliderInt("Carbon Count##alkane", &state.carbon_count_alkane, 1, 30);
    if (ImGui::Button("Generate Alkane", ImVec2(-1, 0))) {
        state.current_molecule = state.generator.generate_alkane(state.carbon_count_alkane);
        state.last_analysis = state.generator.analyze_molecule(state.current_molecule);
        
        state.metadata.formula = state.last_analysis.molecular_formula;
        state.metadata.name = "Alkane C" + std::to_string(state.carbon_count_alkane);
        state.metadata.atom_count = state.last_analysis.total_atoms;
        state.metadata.bond_count = state.last_analysis.total_bonds;
        state.metadata.geometry = "Linear Chain";
        
        state.molecule_pipe->push(state.current_molecule);
        state.status_pipe->push("Generated alkane: " + state.last_analysis.molecular_formula);
        
        std::cout << "[DYNAMIC] Generated alkane: " << state.last_analysis.molecular_formula 
                  << " (" << state.last_analysis.total_atoms << " atoms)\n";
    }
    
    ImGui::Separator();
    
    // ========== ALKENE GENERATOR ==========
    ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.6f, 1.0f), "Alkenes (CₙH₂ₙ) - Double Bond");
    ImGui::SliderInt("Carbon Count##alkene", &state.carbon_count_alkene, 2, 30);
    if (ImGui::Button("Generate Alkene (C=C)", ImVec2(-1, 0))) {
        state.current_molecule = state.generator.generate_alkene(state.carbon_count_alkene);
        state.last_analysis = state.generator.analyze_molecule(state.current_molecule);
        
        state.metadata.formula = state.last_analysis.molecular_formula;
        state.metadata.name = "Alkene C" + std::to_string(state.carbon_count_alkene);
        state.metadata.atom_count = state.last_analysis.total_atoms;
        state.metadata.bond_count = state.last_analysis.total_bonds;
        state.metadata.geometry = "Double Bond";
        
        state.molecule_pipe->push(state.current_molecule);
        state.status_pipe->push("Generated alkene: " + state.last_analysis.molecular_formula);
        
        std::cout << "[DYNAMIC] Generated alkene: " << state.last_analysis.molecular_formula 
                  << " with C=C double bond\n";
    }
    
    ImGui::Separator();
    
    // ========== ALKYNE GENERATOR ==========
    ImGui::TextColored(ImVec4(0.5f, 0.8f, 1.0f, 1.0f), "Alkynes (CₙH₂ₙ₋₂) - TRIPLE BOND");
    ImGui::SliderInt("Carbon Count##alkyne", &state.carbon_count_alkyne, 2, 30);
    if (ImGui::Button("Generate Alkyne (C≡C)", ImVec2(-1, 0))) {
        state.current_molecule = state.generator.generate_alkyne(state.carbon_count_alkyne);
        state.last_analysis = state.generator.analyze_molecule(state.current_molecule);
        
        state.metadata.formula = state.last_analysis.molecular_formula;
        state.metadata.name = "Alkyne C" + std::to_string(state.carbon_count_alkyne);
        state.metadata.atom_count = state.last_analysis.total_atoms;
        state.metadata.bond_count = state.last_analysis.total_bonds;
        state.metadata.geometry = "TRIPLE BOND (C≡C)";
        
        state.molecule_pipe->push(state.current_molecule);
        state.status_pipe->push("Generated alkyne: " + state.last_analysis.molecular_formula);
        
        std::cout << "[DYNAMIC] Generated alkyne: " << state.last_analysis.molecular_formula 
                  << " with C≡C TRIPLE BOND\n";
    }
    
    ImGui::Separator();
    
    // ========== CUSTOM ELEMENT LETTERS ==========
    ImGui::TextColored(ImVec4(0.9f, 0.5f, 1.0f, 1.0f), "Custom Element Letters");
    ImGui::Text("Enter element symbols (e.g., CCCHHHHH)");
    ImGui::Text("Supported: H, C, N, O, F, P, S, K, V, I, W, U");
    ImGui::InputText("Elements##letters", state.element_letters_input, 
                     sizeof(state.element_letters_input));
    
    if (ImGui::Button("Generate from Letters", ImVec2(-1, 0))) {
        std::string letters(state.element_letters_input);
        if (!letters.empty()) {
            state.current_molecule = state.generator.generate_from_letters(letters);
            state.last_analysis = state.generator.analyze_molecule(state.current_molecule);
            
            state.metadata.formula = state.last_analysis.molecular_formula;
            state.metadata.name = "Custom Molecule";
            state.metadata.atom_count = state.last_analysis.total_atoms;
            state.metadata.bond_count = state.last_analysis.total_bonds;
            state.metadata.geometry = "Custom";
            
            state.molecule_pipe->push(state.current_molecule);
            state.status_pipe->push("Generated custom: " + state.last_analysis.molecular_formula);
            
            std::cout << "[DYNAMIC] Generated custom molecule from letters: " 
                      << state.last_analysis.molecular_formula << "\n";
        }
    }
    
    ImGui::Separator();
    
    // ========== CRYSTAL STRUCTURES ==========
    ImGui::TextColored(ImVec4(0.5f, 1.0f, 1.0f, 1.0f), "Crystal Structures (96-100 atoms)");
    ImGui::Text("Monazite-Ce: Rare-earth phosphate mineral");
    ImGui::Text("Formula: CePO₄, Supercell: 2×2×4");
    
    if (ImGui::Button("Generate Monazite-Ce (96 atoms)", ImVec2(-1, 0))) {
        state.current_molecule = state.generator.generate_monazite_supercell(2, 2, 4);
        state.last_analysis = state.generator.analyze_molecule(state.current_molecule);
        
        state.metadata.formula = "Ce16P16O64";
        state.metadata.name = "Monazite-Ce (2×2×4 supercell)";
        state.metadata.atom_count = state.last_analysis.total_atoms;
        state.metadata.bond_count = state.last_analysis.total_bonds;
        state.metadata.geometry = "Monoclinic P2₁/n";
        
        state.molecule_pipe->push(state.current_molecule);
        state.status_pipe->push("Generated Monazite-Ce: " + state.last_analysis.molecular_formula);
        
        std::cout << "[CRYSTAL] Generated Monazite-Ce supercell: " 
                  << state.last_analysis.molecular_formula << "\n";
        std::cout << "[CRYSTAL] " << state.last_analysis.total_atoms << " atoms, "
                  << state.last_analysis.total_bonds << " bonds\n";
    }
    
    ImGui::Text("Rock Salt: Cubic ionic crystal (table salt)");
    ImGui::Text("Formula: NaCl, Supercell: 5×5×4");
    
    if (ImGui::Button("Generate Rock Salt (100 atoms)", ImVec2(-1, 0))) {
        state.current_molecule = state.generator.generate_rocksalt_supercell(5, 5, 4);
        state.last_analysis = state.generator.analyze_molecule(state.current_molecule);
        
        state.metadata.formula = "Na50Cl50";
        state.metadata.name = "Rock Salt (5×5×4 supercell)";
        state.metadata.atom_count = state.last_analysis.total_atoms;
        state.metadata.bond_count = state.last_analysis.total_bonds;
        state.metadata.geometry = "Cubic Fm3̄m";
        
        state.molecule_pipe->push(state.current_molecule);
        state.status_pipe->push("Generated Rock Salt: " + state.last_analysis.molecular_formula);
        
        std::cout << "[CRYSTAL] Generated Rock Salt supercell: " 
                  << state.last_analysis.molecular_formula << "\n";
        std::cout << "[CRYSTAL] " << state.last_analysis.total_atoms << " atoms, "
                  << state.last_analysis.total_bonds << " bonds\n";
    }
    
    ImGui::Separator();
    
    // ========== MOLECULE ANALYSIS ==========
    if (state.last_analysis.total_atoms > 0) {
        ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.8f, 1.0f), "Last Generated Molecule:");
        ImGui::Text("Formula: %s", state.last_analysis.molecular_formula.c_str());
        ImGui::Text("Total Atoms: %d", state.last_analysis.total_atoms);
        ImGui::Text("Total Bonds: %d", state.last_analysis.total_bonds);
        ImGui::Text("Avg Bond Length: %.3f Å", state.last_analysis.avg_bond_length);
        
        ImGui::Separator();
        ImGui::Text("Atom Composition:");
        for (const auto& [Z, count] : state.last_analysis.atom_counts) {
            const auto& symbol = state.last_analysis.atom_symbols.at(Z);
            ImGui::BulletText("%s: %d atoms", symbol.c_str(), count);
        }
    } else {
        ImGui::TextDisabled("No molecule generated yet");
        ImGui::TextDisabled("Click a button above to generate");
    }
    
    ImGui::Separator();
    
    // ========== .XYZ EXPORT CONTROLS ==========
    ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.5f, 1.0f), "Live .XYZ Export");
    
    char xyz_path_buffer[512];
    strncpy(xyz_path_buffer, state.xyz_export_path.c_str(), sizeof(xyz_path_buffer) - 1);
    
    if (ImGui::InputText("File Path", xyz_path_buffer, sizeof(xyz_path_buffer))) {
        state.xyz_export_path = std::string(xyz_path_buffer);
        state.generator.enable_live_export(state.xyz_export_path);
    }
    
    if (ImGui::Button("Export Current Molecule", ImVec2(-1, 0))) {
        if (state.current_molecule.num_atoms() > 0) {
            state.generator.export_current(state.current_molecule, 
                                          "Exported: " + state.metadata.formula);
            state.status_pipe->push("Exported to " + state.xyz_export_path);
            std::cout << "[EXPORT] Wrote molecule to " << state.xyz_export_path << "\n";
        }
    }
    
    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), 
                      "Auto-exports when generating molecules");
    
    ImGui::End();
}

void render_status_bar(VSEPRLiveState& state) {
    ImGui::Begin("Status", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize);
    
    ImGui::Text("VSEPR-Sim Live Integration v2.3.1");
    ImGui::SameLine(300);
    
    if (state.current_molecule.num_atoms() > 0) {
        ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "● READY");
    } else {
        ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "○ No molecule");
    }
    
    ImGui::SameLine(450);
    ImGui::Text("Atoms: %lu | Bonds: %lu", 
                state.current_molecule.num_atoms(),
                state.current_molecule.num_bonds());
    
    ImGui::End();
}

// ============================================================================
// Main Application
// ============================================================================

int main() {
    std::cout << "╔════════════════════════════════════════════════════════════════╗\n";
    std::cout << "║  VSEPR-Sim Live GUI Integration                               ║\n";
    std::cout << "║  Connected to Real Simulation Engine                          ║\n";
    std::cout << "╚════════════════════════════════════════════════════════════════╝\n\n";
    
    // Initialize GLFW
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW\n";
        return -1;
    }
    
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    
    // Create window
    GLFWwindow* window = glfwCreateWindow(1280, 720, 
                                          "VSEPR-Sim Live Integration", 
                                          nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create GLFW window\n";
        glfwTerminate();
        return -1;
    }
    
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // VSync
    
    // Initialize ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    
    ImGui::StyleColorsDark();
    
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");
    
    std::cout << "Window created: 1280x720\n";
    std::cout << "ImGui initialized\n";
    std::cout << "Connected to VSEPR simulation engine\n\n";
    
    // Initialize state
    VSEPRLiveState state;
    
    std::cout << "Features:\n";
    std::cout << "  • Build real molecules (H₂O, NH₃, CH₄)\n";
    std::cout << "  • View molecular structure (atoms, bonds, angles)\n";
    std::cout << "  • 3D visualization with OpenGL (spheres + cylinders)\n";
    std::cout << "  • Interactive camera (drag to rotate, scroll to zoom)\n";
    std::cout << "  • Connected to original VSEPR code\n";
    std::cout << "  • Reactive data pipes for updates\n\n";
    
    // Main loop
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        
        // Start ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        
        // Get window size for OpenGL viewport
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        
        // Render GUI panels
        render_molecule_selector(state);
        render_molecule_info(state);
        render_dynamic_builder_panel(state);  // <-- DYNAMIC GENERATOR!
        render_3d_viewer(state, display_w, display_h);  // <-- 3D VIEWER!
        render_visualization_controls(state);
        render_status_bar(state);
        
        // Render ImGui
        ImGui::Render();
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        
        glfwSwapBuffers(window);
    }
    
    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    
    glfwDestroyWindow(window);
    glfwTerminate();
    
    std::cout << "\nApplication closed\n";
    
    return 0;
}
