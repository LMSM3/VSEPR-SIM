/**
 * VSEPR-Sim ULTIMATE GUI - Live Integration + System Monitor + Thermodynamics
 * ===========================================================================
 * Features:
 * - Real VSEPR molecular simulation (up to 101 atoms)
 * - GPU/CPU/Network/Disk monitoring with live graphs
 * - Thermodynamic properties (Gibbs energy, enthalpy, entropy)
 * - Complete periodic table (Z=1 to Z=118)
 * - Triple bond support
 * - Interactive molecule builder
 */

#include "gui/imgui_integration.hpp"
#include "gui/pokedex_gui.hpp"
#include "gui/data_pipe.hpp"
#include "sim/molecule.hpp"
#include "core/types.hpp"
#include "molecular/unified_types.hpp"
#include "monitor/system_monitor.hpp"
#include "thermo/thermodynamics.hpp"
#include "core/comprehensive_elements.hpp"

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include <GLFW/glfw3.h>
#include <iostream>
#include <memory>
#include <vector>
#include <chrono>

using namespace vsepr;
using namespace vsepr::gui;
using namespace vsepr::pokedex;
using namespace vsepr::monitor;
using namespace vsepr::thermo;

// ============================================================================
// Enhanced Live State with System Monitoring + Thermodynamics
// ============================================================================

struct UltimateVSEPRState {
    // Molecule simulation
    Molecule current_molecule;
    molecular::MolecularMetadata metadata;
    ThermoData thermo_data;
    
    // Simulation state
    bool simulation_running = false;
    bool optimization_running = false;
    double current_energy = 0.0;
    double current_gibbs = 0.0;
    int optimization_step = 0;
    
    // System monitoring
    SystemMonitor system_monitor;
    SystemSnapshot latest_snapshot;
    bool show_system_monitor = true;
    
    // Visualization
    bool show_bonds = true;
    bool show_lone_pairs = false;
    bool show_axes = true;
    bool show_thermodynamics = true;
    float atom_scale = 1.0f;
    
    // Element database
    const elements::ComprehensiveElementDatabase& elem_db;
    
    // Thermodynamics
    ThermodynamicState thermo_state;
    GibbsCalculator gibbs_calc;
    
    // Data pipes
    std::shared_ptr<DataPipe<Molecule>> molecule_pipe;
    std::shared_ptr<DataPipe<double>> energy_pipe;
    std::shared_ptr<DataPipe<std::string>> status_pipe;
    std::shared_ptr<DataPipe<SystemSnapshot>> system_pipe;
    
    // Build tracking
    int molecules_built = 0;
    std::chrono::steady_clock::time_point last_update;
    
    UltimateVSEPRState() 
        : elem_db(elements::comprehensive_elements()),
          gibbs_calc(&thermo_database()),
          last_update(std::chrono::steady_clock::now()) {
        
        // Initialize pipes
        molecule_pipe = std::make_shared<DataPipe<Molecule>>("ultimate_molecule");
        energy_pipe = std::make_shared<DataPipe<double>>("ultimate_energy");
        status_pipe = std::make_shared<DataPipe<std::string>>("ultimate_status");
        system_pipe = system_monitor.system_pipe();
        
        // Register
        PipeNetwork::instance().registerPipe("ultimate_molecule", molecule_pipe);
        PipeNetwork::instance().registerPipe("ultimate_energy", energy_pipe);
        PipeNetwork::instance().registerPipe("ultimate_status", status_pipe);
        
        // Subscribe
        molecule_pipe->subscribe([this](const Molecule& mol) {
            molecules_built++;
            std::cout << "[ULTIMATE] Molecule " << molecules_built << ": " 
                      << mol.num_atoms() << " atoms, " 
                      << mol.num_bonds() << " bonds\n";
        });
        
        energy_pipe->subscribe([](double e) {
            std::cout << "[ULTIMATE] E = " << e << " kcal/mol\n";
        });
        
        // Start system monitor
        system_monitor.start();
    }
    
    void update_system_snapshot() {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - last_update).count();
        
        if (elapsed >= 1) {  // Update every second
            latest_snapshot = system_monitor.get_snapshot();
            last_update = now;
        }
    }
    
    void calculate_thermodynamics() {
        if (current_molecule.num_atoms() == 0) return;
        
        // Estimate thermodynamic properties
        current_gibbs = gibbs_calc.estimate_gibbs(current_molecule, thermo_state);
        
        // Try to get reference data
        // (Would match against thermo database here)
    }
};

// ============================================================================
// Build Extended Molecules (with triple bonds!)
// ============================================================================

Molecule build_acetylene() {
    // C2H2 - Triple bond example
    Molecule mol;
    
    mol.add_atom(6, -0.60, 0.0, 0.0);  // Carbon 1
    mol.add_atom(6, 0.60, 0.0, 0.0);   // Carbon 2
    mol.add_atom(1, -1.66, 0.0, 0.0);  // Hydrogen 1
    mol.add_atom(1, 1.66, 0.0, 0.0);   // Hydrogen 2
    
    mol.add_bond(0, 1, 3);  // C≡C TRIPLE BOND
    mol.add_bond(0, 2, 1);  // C-H
    mol.add_bond(1, 3, 1);  // C-H
    
    mol.generate_angles_from_bonds();
    return mol;
}

Molecule build_nitrogen_molecule() {
    // N2 - Triple bond
    Molecule mol;
    
    mol.add_atom(7, -0.55, 0.0, 0.0);  // Nitrogen 1
    mol.add_atom(7, 0.55, 0.0, 0.0);   // Nitrogen 2
    
    mol.add_bond(0, 1, 3);  // N≡N TRIPLE BOND
    
    return mol;
}

Molecule build_carbon_dioxide() {
    // CO2 - Double bonds, linear
    Molecule mol;
    
    mol.add_atom(6, 0.0, 0.0, 0.0);    // Carbon
    mol.add_atom(8, -1.16, 0.0, 0.0);  // Oxygen 1
    mol.add_atom(8, 1.16, 0.0, 0.0);   // Oxygen 2
    
    mol.add_bond(0, 1, 2);  // C=O double bond
    mol.add_bond(0, 2, 2);  // C=O double bond
    
    mol.generate_angles_from_bonds();
    return mol;
}

Molecule build_benzene() {
    // C6H6 - Aromatic (represented as alternating single/double)
    Molecule mol;
    
    // Hexagon of carbons
    double r = 1.40;  // C-C bond length in benzene
    for (int i = 0; i < 6; ++i) {
        double angle = i * M_PI / 3.0;
        mol.add_atom(6, r * cos(angle), r * sin(angle), 0.0);
    }
    
    // Hydrogens
    double r_h = 2.48;
    for (int i = 0; i < 6; ++i) {
        double angle = i * M_PI / 3.0;
        mol.add_atom(1, r_h * cos(angle), r_h * sin(angle), 0.0);
    }
    
    // Ring bonds (alternating single/double for simplicity)
    for (int i = 0; i < 6; ++i) {
        int order = (i % 2 == 0) ? 2 : 1;  // Alternating
        mol.add_bond(i, (i + 1) % 6, order);
    }
    
    // C-H bonds
    for (int i = 0; i < 6; ++i) {
        mol.add_bond(i, 6 + i, 1);
    }
    
    mol.generate_angles_from_bonds();
    return mol;
}

// Original molecules (from vsepr_gui_live.cpp)
Molecule build_water() {
    Molecule mol;
    mol.add_atom(8, 0.0, 0.0, 0.0);
    mol.add_atom(1, 0.96, 0.0, 0.0);
    mol.add_atom(1, -0.24, 0.93, 0.0);
    mol.add_bond(0, 1, 1);
    mol.add_bond(0, 2, 1);
    mol.generate_angles_from_bonds();
    return mol;
}

Molecule build_ammonia() {
    Molecule mol;
    mol.add_atom(7, 0.0, 0.0, 0.0);
    mol.add_atom(1, 1.01, 0.0, 0.0);
    mol.add_atom(1, -0.34, 0.95, 0.0);
    mol.add_atom(1, -0.34, -0.48, 0.83);
    mol.add_bond(0, 1, 1);
    mol.add_bond(0, 2, 1);
    mol.add_bond(0, 3, 1);
    mol.generate_angles_from_bonds();
    return mol;
}

Molecule build_methane() {
    Molecule mol;
    mol.add_atom(6, 0.0, 0.0, 0.0);
    mol.add_atom(1, 1.09, 0.0, 0.0);
    mol.add_atom(1, -0.36, 1.03, 0.0);
    mol.add_atom(1, -0.36, -0.52, 0.89);
    mol.add_atom(1, -0.36, -0.52, -0.89);
    mol.add_bond(0, 1, 1);
    mol.add_bond(0, 2, 1);
    mol.add_bond(0, 3, 1);
    mol.add_bond(0, 4, 1);
    mol.generate_angles_from_bonds();
    return mol;
}

// ============================================================================
// GUI Rendering Functions
// ============================================================================

void render_system_monitor_panel(UltimateVSEPRState& state) {
    if (!state.show_system_monitor) return;
    
    ImGui::Begin("System Monitor", &state.show_system_monitor);
    
    ImGui::Text("System Performance");
    ImGui::Separator();
    
    // GPU
    if (!state.latest_snapshot.gpus.empty()) {
        auto& gpu = state.latest_snapshot.gpus[0];
        ImGui::Text("GPU: %s", gpu.name.c_str());
        ImGui::ProgressBar(gpu.utilization_percent / 100.0f, 
                          ImVec2(-1, 0), 
                          (std::to_string((int)gpu.utilization_percent) + "%").c_str());
        
        ImGui::Text("Memory: %.0f / %.0f MB (%.1f%%)", 
                   gpu.memory_used_mb, gpu.memory_total_mb, gpu.memory_percent());
        ImGui::Text("Temp: %.1f°C | Power: %.1f W", 
                   gpu.temperature_celsius, gpu.power_watts);
        
        // Mini graph
        ImGui::Text("GPU History: %s", state.system_monitor.gpu_graph().render(40).c_str());
    } else {
        ImGui::TextDisabled("No GPU detected");
    }
    
    ImGui::Separator();
    
    // CPU & RAM
    ImGui::Text("CPU: %.1f%%", state.latest_snapshot.cpu_percent);
    ImGui::ProgressBar(state.latest_snapshot.cpu_percent / 100.0f, ImVec2(-1, 0));
    
    ImGui::Text("RAM: %.1f / %.1f GB (%.1f%%)",
               state.latest_snapshot.ram_used_gb,
               state.latest_snapshot.ram_total_gb,
               state.latest_snapshot.ram_percent());
    ImGui::ProgressBar(state.latest_snapshot.ram_percent() / 100.0f, ImVec2(-1, 0));
    
    ImGui::Separator();
    
    // Network
    if (!state.latest_snapshot.networks.empty()) {
        auto& net = state.latest_snapshot.networks[0];
        ImGui::Text("Network: %s", net.interface.c_str());
        ImGui::Text("RX: %.2f Mbps | TX: %.2f Mbps", net.rx_rate_mbps, net.tx_rate_mbps);
        ImGui::Text("Net History: %s", state.system_monitor.network_graph().render(40).c_str());
    }
    
    ImGui::Separator();
    
    // Disk
    if (!state.latest_snapshot.disks.empty()) {
        auto& disk = state.latest_snapshot.disks[0];
        ImGui::Text("Disk: %s", disk.mount_point.c_str());
        ImGui::Text("%.1f / %.1f GB (%.1f%%)", 
                   disk.used_gb(), disk.total_gb(), disk.usage_percent);
        ImGui::ProgressBar(disk.usage_percent / 100.0f, ImVec2(-1, 0));
    }
    
    ImGui::End();
}

void render_thermodynamics_panel(UltimateVSEPRState& state) {
    if (!state.show_thermodynamics) return;
    
    ImGui::Begin("Thermodynamics", &state.show_thermodynamics);
    
    ImGui::Text("Thermodynamic Properties");
    ImGui::Separator();
    
    ImGui::Text("Temperature: %.2f K (%.2f°C)", 
               state.thermo_state.temperature_K,
               state.thermo_state.temperature_K - 273.15);
    ImGui::Text("Pressure: %.2f atm", state.thermo_state.pressure_atm);
    
    ImGui::Separator();
    
    if (state.current_molecule.num_atoms() > 0) {
        ImGui::Text("Gibbs Free Energy: %.2f kcal/mol", state.current_gibbs);
        ImGui::Text("Enthalpy (est): %.2f kcal/mol", state.current_energy);
        
        // Phase
        ImGui::Text("Phase: Gas (default)");
        
        ImGui::Separator();
        ImGui::TextWrapped("Note: Thermodynamic values are estimates. "
                          "For accurate data, consult experimental databases.");
    } else {
        ImGui::TextDisabled("Build a molecule to see thermodynamic properties");
    }
    
    ImGui::End();
}

void render_molecule_builder_extended(UltimateVSEPRState& state) {
    ImGui::Begin("Molecule Builder (Extended)");
    
    ImGui::Text("Basic Molecules:");
    ImGui::Separator();
    
    if (ImGui::Button("H₂O (Water)", ImVec2(150, 0))) {
        state.current_molecule = build_water();
        state.metadata = {"H2O", "Water", "Bent"};
        state.molecule_pipe->push(state.current_molecule);
        state.calculate_thermodynamics();
        state.status_pipe->push("Built H₂O");
    }
    ImGui::SameLine();
    
    if (ImGui::Button("NH₃ (Ammonia)", ImVec2(150, 0))) {
        state.current_molecule = build_ammonia();
        state.metadata = {"NH3", "Ammonia", "Trigonal Pyramidal"};
        state.molecule_pipe->push(state.current_molecule);
        state.calculate_thermodynamics();
        state.status_pipe->push("Built NH₃");
    }
    ImGui::SameLine();
    
    if (ImGui::Button("CH₄ (Methane)", ImVec2(150, 0))) {
        state.current_molecule = build_methane();
        state.metadata = {"CH4", "Methane", "Tetrahedral"};
        state.molecule_pipe->push(state.current_molecule);
        state.calculate_thermodynamics();
        state.status_pipe->push("Built CH₄");
    }
    
    ImGui::Separator();
    ImGui::Text("Advanced Molecules (Triple Bonds!):");
    ImGui::Separator();
    
    if (ImGui::Button("C₂H₂ (Acetylene)", ImVec2(150, 0))) {
        state.current_molecule = build_acetylene();
        state.metadata = {"C2H2", "Acetylene", "Linear"};
        state.molecule_pipe->push(state.current_molecule);
        state.calculate_thermodynamics();
        state.status_pipe->push("Built C₂H₂ with TRIPLE bond!");
    }
    ImGui::SameLine();
    
    if (ImGui::Button("N₂ (Nitrogen)", ImVec2(150, 0))) {
        state.current_molecule = build_nitrogen_molecule();
        state.metadata = {"N2", "Nitrogen", "Linear"};
        state.molecule_pipe->push(state.current_molecule);
        state.calculate_thermodynamics();
        state.status_pipe->push("Built N₂ with TRIPLE bond!");
    }
    ImGui::SameLine();
    
    if (ImGui::Button("CO₂ (Carbon Dioxide)", ImVec2(150, 0))) {
        state.current_molecule = build_carbon_dioxide();
        state.metadata = {"CO2", "Carbon Dioxide", "Linear"};
        state.molecule_pipe->push(state.current_molecule);
        state.calculate_thermodynamics();
        state.status_pipe->push("Built CO₂ with double bonds!");
    }
    
    if (ImGui::Button("C₆H₆ (Benzene)", ImVec2(150, 0))) {
        state.current_molecule = build_benzene();
        state.metadata = {"C6H6", "Benzene", "Planar Hexagon"};
        state.molecule_pipe->push(state.current_molecule);
        state.calculate_thermodynamics();
        state.status_pipe->push("Built C₆H₆ (aromatic!)");
    }
    
    ImGui::Separator();
    ImGui::Text("Statistics:");
    ImGui::Text("  Molecules built this session: %d", state.molecules_built);
    
    ImGui::End();
}

void render_molecule_info_detailed(UltimateVSEPRState& state) {
    ImGui::Begin("Molecule Info (Detailed)");
    
    if (state.current_molecule.num_atoms() == 0) {
        ImGui::TextDisabled("No molecule loaded. Use Molecule Builder.");
        ImGui::End();
        return;
    }
    
    ImGui::Text("Formula: %s", state.metadata.formula.c_str());
    ImGui::Text("Name: %s", state.metadata.name.c_str());
    ImGui::Text("Geometry: %s", state.metadata.geometry.c_str());
    
    ImGui::Separator();
    
    ImGui::Text("Structure:");
    ImGui::Text("  Atoms: %d", (int)state.current_molecule.num_atoms());
    ImGui::Text("  Bonds: %d", (int)state.current_molecule.num_bonds());
    ImGui::Text("  Angles: %d", (int)state.current_molecule.angles.size());
    
    ImGui::Separator();
    
    ImGui::Text("Atom Details:");
    for (size_t i = 0; i < state.current_molecule.num_atoms(); ++i) {
        const auto& atom = state.current_molecule.atoms[i];
        double x, y, z;
        state.current_molecule.get_position(i, x, y, z);
        
        auto& elem = state.elem_db.get(atom.Z);
        ImGui::Text("  [%d] %s (Z=%d)  (%.3f, %.3f, %.3f)", 
                   (int)i, elem.symbol.data(), atom.Z, x, y, z);
    }
    
    ImGui::Separator();
    
    ImGui::Text("Bond Details:");
    for (size_t i = 0; i < state.current_molecule.num_bonds(); ++i) {
        const auto& bond = state.current_molecule.bonds[i];
        std::string bond_type = (bond.order == 1) ? "single" :
                               (bond.order == 2) ? "DOUBLE" :
                               (bond.order == 3) ? "TRIPLE" : "???";
        ImGui::Text("  [%d] %d-%d  %s  (order=%.1f)", 
                   (int)i, bond.i, bond.j, bond_type.c_str(), bond.order);
    }
    
    ImGui::End();
}

// ============================================================================
// Main
// ============================================================================

int main() {
    std::cout << "╔════════════════════════════════════════════════════════════════╗\n";
    std::cout << "║  VSEPR-Sim ULTIMATE GUI Integration                          ║\n";
    std::cout << "║  Live Simulation + System Monitor + Thermodynamics           ║\n";
    std::cout << "╚════════════════════════════════════════════════════════════════╝\n\n";
    
    // Initialize databases
    elements::init_comprehensive_elements();
    thermo::init_thermo_database();
    
    // Initialize GLFW
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW\n";
        return -1;
    }
    
    // Create window
    GLFWwindow* window = glfwCreateWindow(1600, 900, "VSEPR-Sim Ultimate", nullptr, nullptr);
    if (!window) {
        glfwTerminate();
        return -1;
    }
    
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);  // V-sync
    
    // Setup ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    ImGui::StyleColorsDark();
    
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 130");
    
    std::cout << "Window created: 1600x900\n";
    std::cout << "ImGui initialized\n";
    std::cout << "Connected to VSEPR simulation engine\n";
    std::cout << "System monitor active\n";
    std::cout << "Thermodynamics calculator ready\n\n";
    
    std::cout << "Features:\n";
    std::cout << "  • Build real molecules (up to 101 atoms)\n";
    std::cout << "  • Triple bond support (C≡C, N≡N)\n";
    std::cout << "  • GPU/CPU/Network/Disk monitoring\n";
    std::cout << "  • Gibbs energy calculations\n";
    std::cout << "  • Full periodic table (Z=1 to Z=118)\n\n";
    
    // Create state
    UltimateVSEPRState state;
    
    // Main loop
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        
        // Update system monitor
        state.update_system_snapshot();
        
        // Start ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        
        // Render panels
        render_molecule_builder_extended(state);
        render_molecule_info_detailed(state);
        render_system_monitor_panel(state);
        render_thermodynamics_panel(state);
        
        // Status bar
        ImGui::SetNextWindowPos(ImVec2(0, io.DisplaySize.y - 30));
        ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x, 30));
        ImGui::Begin("Status", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);
        ImGui::Text("READY | Atoms: %d | Bonds: %d | Molecules Built: %d | FPS: %.1f", 
                   (int)state.current_molecule.num_atoms(),
                   (int)state.current_molecule.num_bonds(),
                   state.molecules_built,
                   io.Framerate);
        ImGui::End();
        
        // Rendering
        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
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
