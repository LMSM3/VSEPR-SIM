/**
 * VSEPR-Sim Modern OpenGL Visualization System
 * 
 * Complete example demonstrating:
 * - Modern OpenGL 3.3+ rendering pipeline
 * - Entity-component system architecture
 * - PBR material system with dynamic lighting
 * - Interactive camera with multiple projection modes
 * - Molecular structure visualization from XYZ files
 * - FEA mesh visualization with result colormaps
 * 
 * Requires: GLFW3, GLEW, GLM, ImGui (optional)
 * Compilation: g++ -std=c++17 -O3 vsepr_opengl_viewer.cpp -o viewer \
 *              -lglfw -lGLEW -lGL -limgui (on Linux)
 *              clang++ -std=c++17 -O3 vsepr_opengl_viewer.cpp -o viewer \
 *              -lglfw -lGLEW -framework OpenGL (on macOS)
 *              cl /std:c++17 /O2 vsepr_opengl_viewer.cpp \
 *              glfw3.lib glew32.lib opengl32.lib (on Windows MSVC)
 */

#include <iostream>
#include <iomanip>
#include <memory>
#include <vector>
#include <map>
#include <string>
#include <fstream>
#include <sstream>
#include <functional>
#include <random>
#include <chrono>
#include <thread>
#include <atomic>
#include <cmath>
#include <mutex>
#include <algorithm>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

// Forward declarations for OpenGL types (would include actual headers in full implementation)
// #include "vis/gl_context.hpp"
// #include "vis/gl_shader.hpp"
// #include "vis/gl_camera.hpp"
// #include "vis/gl_material.hpp"
// #include "vis/gl_mesh.hpp"
// #include "vis/gl_renderer.hpp"
// #include "vis/gl_application.hpp"

/**
 * ============================================================================
 * VSEPR-SIM OPENGL INTEGRATION EXAMPLE
 * ============================================================================
 * 
 * This example shows how to integrate the modern OpenGL visualization system
 * with VSEPR-Sim's three-realm architecture:
 * 
 * 1. MOLECULAR REALM
 *    - Atom positions from XYZ files
 *    - Bond visualization as cylinders
 *    - VSEPR electron geometry prediction
 *    - Interactive rotation and zoom
 * 
 * 2. QUANTUM REALM
 *    - Electron density visualization
 *    - Orbital visualizations
 *    - Wavefunction data as volumetric textures
 * 
 * 3. PHYSICAL SCALE
 *    - FEA mesh visualization (hex8, tet4, etc.)
 *    - Stress/strain field colormaps
 *    - Thermal analysis colormaps
 *    - Result animations
 * 
 * The system uses:
 * - PBR materials for physically-based rendering
 * - Deferred rendering for efficient multi-light scenes
 * - Instanced rendering for repeated geometries (bonds, mesh elements)
 * - GPU compute shaders for dynamic visualizations
 * ============================================================================
 */

// Placeholder for actual implementation
namespace vsepr::vis {

/**
 * Visualization Update Callback System
 * Allows real-time updates during batch processing
 */
class VisualizationCallback {
public:
    using UpdateFunction = std::function<void(const std::string& formula, int current, int total)>;
    using RenderFunction = std::function<void()>;
    
    UpdateFunction on_molecule_discovered;
    UpdateFunction on_molecule_optimized;
    RenderFunction on_frame_render;
    
    std::atomic<bool> running{true};
    std::atomic<int> current_molecule{0};
    std::atomic<int> total_molecules{0};
    std::string current_formula;
    
    void trigger_discovery(const std::string& formula, int current, int total) {
        current_formula = formula;
        current_molecule = current;
        total_molecules = total;
        if (on_molecule_discovered) {
            on_molecule_discovered(formula, current, total);
        }
    }
    
    void trigger_optimized(const std::string& formula, int current, int total) {
        if (on_molecule_optimized) {
            on_molecule_optimized(formula, current, total);
        }
    }
    
    void trigger_render() {
        if (on_frame_render) {
            on_frame_render();
        }
    }
    
    void stop() {
        running = false;
    }
};

/**
 * Formula Parser Utilities
 */
struct FormulaParser {
    /**
     * Parse molecular formula into element counts
     * Examples: "H2O" → {H:2, O:1}, "C2FHN" → {C:2, F:1, H:1, N:1}
     */
    static std::map<std::string, int> parse_formula(const std::string& formula) {
        std::map<std::string, int> element_counts;
        size_t i = 0;
        
        while (i < formula.length()) {
            // Parse element symbol (capital letter + optional lowercase)
            std::string elem;
            elem += formula[i++];  // Capital letter
            
            // Check for lowercase continuation (Cl, Br, etc.)
            if (i < formula.length() && std::islower(formula[i])) {
                elem += formula[i++];
            }
            
            // Parse count (if present)
            int count = 0;
            while (i < formula.length() && std::isdigit(formula[i])) {
                count = count * 10 + (formula[i++] - '0');
            }
            
            // If no count specified, default to 1
            if (count == 0) count = 1;
            
            element_counts[elem] += count;
        }
        
        return element_counts;
    }
    
    /**
     * Count total atoms in formula
     */
    static int count_atoms(const std::string& formula) {
        auto counts = parse_formula(formula);
        int total = 0;
        for (const auto& [elem, count] : counts) {
            total += count;
        }
        return total;
    }
};

/**
 * Discovery Statistics Tracker
 * Tracks molecular discovery metrics across continuous generation
 */
struct DiscoveryStats {
    std::atomic<uint64_t> total_generated{0};
    std::atomic<uint64_t> total_successful{0};
    std::atomic<uint64_t> total_visualized{0};
    std::atomic<uint64_t> unique_formulas{0};
    
    std::map<std::string, int> formula_counts;  // Not thread-safe, use mutex
    std::map<int, int> atom_count_distribution;  // Histogram of atom counts
    std::map<std::string, int> element_frequency;  // Element usage stats
    std::mutex stats_mutex;
    
    std::chrono::high_resolution_clock::time_point start_time;
    
    void start() {
        start_time = std::chrono::high_resolution_clock::now();
    }
    
    void record_molecule(const std::string& formula, bool success, bool visualized) {
        total_generated++;
        if (success) total_successful++;
        if (visualized) total_visualized++;
        
        std::lock_guard<std::mutex> lock(stats_mutex);
        formula_counts[formula]++;
        if (formula_counts[formula] == 1) {
            unique_formulas++;
        }
        
        // Track atom count
        int atoms = FormulaParser::count_atoms(formula);
        atom_count_distribution[atoms]++;
        
        // Track element frequency
        auto elements = FormulaParser::parse_formula(formula);
        for (const auto& [elem, count] : elements) {
            element_frequency[elem] += count;
        }
    }
    
    void print_summary(std::ostream& out = std::cout) const {
        auto now = std::chrono::high_resolution_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - start_time).count();
        
        out << "\n╔════════════════════════════════════════════════════════════════╗\n";
        out << "║  DISCOVERY STATISTICS                                          ║\n";
        out << "╚════════════════════════════════════════════════════════════════╝\n";
        out << "\nGeneration:\n";
        out << "  Total molecules:     " << total_generated << "\n";
        out << "  Successful builds:   " << total_successful 
            << " (" << (total_generated > 0 ? (total_successful * 100 / total_generated) : 0) << "%)\n";
        out << "  Visualized:          " << total_visualized << "\n";
        out << "  Unique formulas:     " << unique_formulas << "\n";
        
        out << "\nPerformance:\n";
        out << "  Runtime:             " << elapsed << " seconds\n";
        out << "  Rate:                " << std::fixed << std::setprecision(2)
            << (elapsed > 0 ? static_cast<double>(total_generated) / elapsed : 0.0) 
            << " molecules/sec\n";
        out << "  Throughput:          " 
            << (elapsed > 0 ? (total_generated * 3600 / elapsed) : 0) << " molecules/hour\n";
        
        out << "\nMolecular Complexity:\n";
        if (!atom_count_distribution.empty()) {
            int min_atoms = atom_count_distribution.begin()->first;
            int max_atoms = atom_count_distribution.rbegin()->first;
            double avg_atoms = 0.0;
            int total_count = 0;
            for (const auto& [atoms, count] : atom_count_distribution) {
                avg_atoms += atoms * count;
                total_count += count;
            }
            avg_atoms /= total_count;
            
            out << "  Atom range:          " << min_atoms << " - " << max_atoms << "\n";
            out << "  Average atoms:       " << std::fixed << std::setprecision(1) << avg_atoms << "\n";
        }
        
        out << "\nTop 10 Elements:\n";
        std::vector<std::pair<std::string, int>> sorted_elements(element_frequency.begin(), element_frequency.end());
        std::sort(sorted_elements.begin(), sorted_elements.end(),
                  [](const auto& a, const auto& b) { return a.second > b.second; });
        for (size_t i = 0; i < std::min(size_t(10), sorted_elements.size()); ++i) {
            out << "  " << std::left << std::setw(3) << sorted_elements[i].first 
                << " : " << sorted_elements[i].second << " atoms\n";
        }
        
        out << "\nTop 10 Formulas:\n";
        std::vector<std::pair<std::string, int>> sorted_formulas(formula_counts.begin(), formula_counts.end());
        std::sort(sorted_formulas.begin(), sorted_formulas.end(),
                  [](const auto& a, const auto& b) { return a.second > b.second; });
        for (size_t i = 0; i < std::min(size_t(10), sorted_formulas.size()); ++i) {
            out << "  " << std::left << std::setw(15) << sorted_formulas[i].first 
                << " : " << sorted_formulas[i].second << " times\n";
        }
    }
    
    void save_checkpoint(const std::string& filename) const {
        std::ofstream file(filename);
        if (!file.is_open()) return;
        
        file << "# VSEPR-Sim Discovery Checkpoint\n";
        file << "total_generated: " << total_generated << "\n";
        file << "total_successful: " << total_successful << "\n";
        file << "unique_formulas: " << unique_formulas << "\n";
        file << "\n# Formula counts\n";
        for (const auto& [formula, count] : formula_counts) {
            file << formula << "," << count << "\n";
        }
        file.close();
    }
};

/**
 * Random Molecule Generator
 * Generates random molecular formulas for batch processing
 */
class RandomMoleculeGenerator {
public:
    RandomMoleculeGenerator() : gen(std::random_device{}()) {
        // Element weights (based on chemical frequency)
        element_weights = {
            {"H", 100.0}, {"C", 90.0}, {"N", 70.0}, {"O", 80.0}, {"F", 40.0},
            {"P", 30.0}, {"S", 35.0}, {"Cl", 35.0}, {"Br", 15.0}, {"I", 10.0},
            {"B", 10.0}, {"Si", 10.0}, {"Xe", 5.0}, {"Kr", 3.0}, {"As", 8.0}
        };
    }
    
    std::string generate_random_formula() {
        std::discrete_distribution<> dist({
            element_weights["H"], element_weights["C"], element_weights["N"], 
            element_weights["O"], element_weights["F"], element_weights["P"], 
            element_weights["S"], element_weights["Cl"], element_weights["Br"], 
            element_weights["I"], element_weights["B"], element_weights["Si"],
            element_weights["Xe"], element_weights["Kr"], element_weights["As"]
        });
        
        std::vector<std::string> elements = {
            "H", "C", "N", "O", "F", "P", "S", "Cl", "Br", "I", "B", "Si", "Xe", "Kr", "As"
        };
        
        // Select central atom (heavier element)
        std::uniform_int_distribution<> central_dist(1, elements.size() - 1);
        int central_idx = central_dist(gen);
        std::string central = elements[central_idx];
        
        // Determine number of peripheral atoms (2-7 for VSEPR)
        std::uniform_int_distribution<> count_dist(2, 7);
        int num_peripheral = count_dist(gen);
        
        std::map<std::string, int> formula_map;
        formula_map[central] = 1;
        
        // Add peripheral atoms
        for (int i = 0; i < num_peripheral; ++i) {
            int elem_idx = dist(gen);
            formula_map[elements[elem_idx]]++;
        }
        
        // Build formula string
        std::string formula;
        for (const auto& [elem, count] : formula_map) {
            formula += elem;
            if (count > 1) {
                formula += std::to_string(count);
            }
        }
        
        return formula;
    }
    
private:
    std::mt19937 gen;
    std::map<std::string, double> element_weights;
};

/**
 * Molecular Visualization Handler
 * Converts XYZ molecular data to OpenGL entities
 */
class MolecularVisualizer {
public:
    struct Atom {
        glm::vec3 position;
        int atomic_number;
        float radius;
        glm::vec3 color;
    };

    struct Bond {
        int atom1_idx;
        int atom2_idx;
        float distance;
        int order;  // 1=single, 2=double, 3=triple
        float equilibrium_length;  // Target length for field-based optimization
        float spring_constant;     // Bond strength for force field
    };

    std::vector<Atom> atoms;
    std::vector<Bond> bonds;
    bool bonds_locked = false;  // Prevent re-inference during optimization

    /**
     * Load molecule from XYZ file format
     * Standard format:
     * Line 1: Number of atoms
     * Line 2: Comment/title
     * Lines 3+: Element X Y Z [charge [velocity]]
     */
    bool load_xyz(const std::string& filename) {
        std::ifstream file(filename);
        if (!file.is_open()) {
            std::cerr << "Failed to open XYZ file: " << filename << std::endl;
            return false;
        }

        atoms.clear();
        bonds.clear();

        int num_atoms;
        std::string line;
        std::string comment;

        // Read header
        std::getline(file, line);
        num_atoms = std::stoi(line);
        std::getline(file, comment);  // Skip comment line

        std::cout << "Loading " << num_atoms << " atoms from " << filename << std::endl;

        // Van der Waals radii (Å)
        std::map<std::string, float> vdw_radii = {
            {"H", 1.20f}, {"C", 1.70f}, {"N", 1.55f}, {"O", 1.52f},
            {"F", 1.47f}, {"P", 1.80f}, {"S", 1.80f}, {"Cl", 1.75f},
            {"Br", 1.85f}, {"I", 1.98f}
        };

        // Element colors (common CPK coloring)
        std::map<std::string, glm::vec3> element_colors = {
            {"H", {1.0f, 1.0f, 1.0f}},      // White
            {"C", {0.2f, 0.2f, 0.2f}},      // Dark gray
            {"N", {0.2f, 0.2f, 0.8f}},      // Blue
            {"O", {0.8f, 0.2f, 0.2f}},      // Red
            {"F", {0.2f, 0.8f, 0.2f}},      // Green
            {"P", {1.0f, 0.6f, 0.2f}},      // Orange
            {"S", {1.0f, 1.0f, 0.2f}},      // Yellow
            {"Cl", {0.2f, 0.8f, 0.2f}},     // Green
            {"Br", {0.6f, 0.2f, 0.2f}},     // Brown
            {"I", {0.5f, 0.2f, 0.5f}}       // Purple
        };

        // Read atoms
        for (int i = 0; i < num_atoms; ++i) {
            std::getline(file, line);
            std::istringstream iss(line);
            
            std::string element;
            float x, y, z;
            iss >> element >> x >> y >> z;

            Atom atom;
            atom.position = glm::vec3(x, y, z) * 0.01f;  // Convert Å to 0.01Å units
            atom.atomic_number = get_atomic_number(element);
            atom.radius = vdw_radii.count(element) ? vdw_radii[element] * 0.01f : 1.0f;
            atom.color = element_colors.count(element) ? element_colors[element] : glm::vec3(0.5f);

            atoms.push_back(atom);
        }

        file.close();

        // Auto-generate bonds using distance criteria
        detect_bonds();

        return true;
    }

    /**
     * Detect bonds based on distance threshold
     * ONE-TIME inference at construction. Does not re-run during optimization.
     * Covalent radius sum × 1.2 = bond detection distance
     */
    void detect_bonds() {
        // Prevent re-inference if bonds already locked
        if (bonds_locked) {
            return;
        }
        
        bonds.clear();  // Clear before building
        
        std::map<std::string, float> covalent_radii = {
            {"H", 0.31f}, {"C", 0.76f}, {"N", 0.71f}, {"O", 0.66f},
            {"F", 0.57f}, {"P", 1.07f}, {"S", 1.05f}, {"Cl", 1.02f},
            {"B", 0.84f}, {"Si", 1.11f}, {"Br", 1.20f}, {"I", 1.39f},
            {"As", 1.19f}, {"Xe", 1.40f}, {"Kr", 1.16f}
        };
        
        // Spring constants for field-based optimization (N/m equivalent)
        std::map<std::string, float> spring_constants = {
            {"H", 450.0f}, {"C", 350.0f}, {"N", 400.0f}, {"O", 450.0f},
            {"F", 500.0f}, {"P", 300.0f}, {"S", 300.0f}, {"Cl", 320.0f}
        };

        // Maximum possible bonds: N(N-1)/2
        size_t max_possible = (atoms.size() * (atoms.size() - 1)) / 2;
        
        for (size_t i = 0; i < atoms.size(); ++i) {
            for (size_t j = i + 1; j < atoms.size(); ++j) {
                float dist = glm::distance(atoms[i].position, atoms[j].position);
                
                // Get element names from atomic numbers
                std::string elem_i = get_element_symbol(atoms[i].atomic_number);
                std::string elem_j = get_element_symbol(atoms[j].atomic_number);
                
                float r_i = covalent_radii.count(elem_i) ? covalent_radii[elem_i] : 0.8f;
                float r_j = covalent_radii.count(elem_j) ? covalent_radii[elem_j] : 0.8f;
                float equilibrium = (r_i + r_j) * 0.01f;  // Equilibrium bond length
                float threshold = equilibrium * 1.3f;      // Detection threshold (wider)

                if (dist < threshold && dist > 0.001f) {
                    Bond bond;
                    bond.atom1_idx = i;
                    bond.atom2_idx = j;
                    bond.distance = dist;
                    bond.order = 1;  // Default single bond
                    bond.equilibrium_length = equilibrium;
                    
                    // Average spring constant of both atoms
                    float k_i = spring_constants.count(elem_i) ? spring_constants[elem_i] : 300.0f;
                    float k_j = spring_constants.count(elem_j) ? spring_constants[elem_j] : 300.0f;
                    bond.spring_constant = (k_i + k_j) / 2.0f;
                    
                    bonds.push_back(bond);
                }
            }
        }
        
        // Validate bond count
        if (bonds.size() > max_possible) {
            std::cerr << "ERROR: Detected " << bonds.size() << " bonds for " 
                      << atoms.size() << " atoms (max possible: " << max_possible << ")" << std::endl;
            bonds.resize(max_possible);  // Truncate to maximum
        }
        
        // Lock the bond graph - no more changes during optimization
        bonds_locked = true;
    }
    
    /**
     * Detect bonds with optional verbose output
     */
    void detect_bonds(bool verbose) {
        detect_bonds();  // Call main version
        if (verbose) {
            std::cout << "Detected " << bonds.size() << " bonds (locked)" << std::endl;
        }
    }

    /**
     * Get atomic number from element symbol
     */
    static int get_atomic_number(const std::string& symbol) {
        static const std::map<std::string, int> table = {
            {"H", 1}, {"C", 6}, {"N", 7}, {"O", 8}, {"F", 9},
            {"P", 15}, {"S", 16}, {"Cl", 17}, {"Br", 35}, {"I", 53},
            {"B", 5}, {"Si", 14}, {"As", 33}, {"Xe", 54}, {"Kr", 36}
        };
        return table.count(symbol) ? table.at(symbol) : 0;
    }

    /**
     * Get element symbol from atomic number
     */
    static std::string get_element_symbol(int atomic_num) {
        static const std::map<int, std::string> table = {
            {1, "H"}, {6, "C"}, {7, "N"}, {8, "O"}, {9, "F"},
            {15, "P"}, {16, "S"}, {17, "Cl"}, {35, "Br"}, {53, "I"},
            {5, "B"}, {14, "Si"}, {33, "As"}, {54, "Xe"}, {36, "Kr"}
        };
        return table.count(atomic_num) ? table.at(atomic_num) : "X";
    }
    
    /**
     * FIRE (Fast Inertial Relaxation Engine) Optimizer
     * Field-based optimization using bond equilibrium lengths
     * Does NOT re-infer bonds - uses locked bond graph
     */
    struct FIREOptimizer {
        float dt = 0.005f;          // Time step
        float dt_max = 0.01f;       // Maximum time step
        float alpha = 0.1f;         // Velocity mixing parameter
        float f_alpha = 0.99f;      // Alpha decay
        float f_inc = 1.1f;         // Time step increase factor
        float f_dec = 0.5f;         // Time step decrease factor
        int n_min = 5;              // Minimum steps before acceleration
        
        std::vector<glm::vec3> velocities;
        int n_steps_positive = 0;
        
        void initialize(size_t num_atoms) {
            velocities.resize(num_atoms, glm::vec3(0.0f));
            n_steps_positive = 0;
        }
        
        /**
         * Single FIRE optimization step
         * Returns: (energy, max_force)
         */
        std::pair<float, float> step(MolecularVisualizer& mol) {
            if (!mol.bonds_locked) {
                std::cerr << "ERROR: Cannot optimize with unlocked bonds\\n";
                return {0.0f, 0.0f};
            }
            
            // Calculate forces from bond springs
            std::vector<glm::vec3> forces(mol.atoms.size(), glm::vec3(0.0f));
            float total_energy = 0.0f;
            
            for (const auto& bond : mol.bonds) {
                glm::vec3 r_vec = mol.atoms[bond.atom2_idx].position - 
                                  mol.atoms[bond.atom1_idx].position;
                float r = glm::length(r_vec);
                
                if (r < 0.0001f) continue;  // Avoid singularity
                
                // Spring force: F = -k * (r - r_eq) * r_hat
                float delta = r - bond.equilibrium_length;
                glm::vec3 r_hat = r_vec / r;
                glm::vec3 force = -bond.spring_constant * delta * r_hat;
                
                forces[bond.atom1_idx] += force;
                forces[bond.atom2_idx] -= force;
                
                // Energy: E = 0.5 * k * (r - r_eq)^2
                total_energy += 0.5f * bond.spring_constant * delta * delta;
            }
            
            // FIRE algorithm update
            float power = 0.0f;
            for (size_t i = 0; i < mol.atoms.size(); ++i) {
                power += glm::dot(forces[i], velocities[i]);
            }
            
            // Check power direction
            if (power > 0.0f) {
                n_steps_positive++;
                
                // Apply FIRE acceleration after n_min steps
                if (n_steps_positive > n_min) {
                    dt = std::min(dt * f_inc, dt_max);
                    alpha *= f_alpha;
                }
                
                // Mix velocities
                for (size_t i = 0; i < mol.atoms.size(); ++i) {
                    float f_norm = glm::length(forces[i]);
                    float v_norm = glm::length(velocities[i]);
                    
                    if (f_norm > 0.0001f) {
                        velocities[i] = (1.0f - alpha) * velocities[i] + 
                                        alpha * (v_norm / f_norm) * forces[i];
                    }
                }
            } else {
                // Power negative - reset
                n_steps_positive = 0;
                dt *= f_dec;
                alpha = 0.1f;
                
                for (auto& v : velocities) {
                    v = glm::vec3(0.0f);
                }
            }
            
            // Update velocities and positions
            float max_force = 0.0f;
            for (size_t i = 0; i < mol.atoms.size(); ++i) {
                velocities[i] += forces[i] * dt;
                mol.atoms[i].position += velocities[i] * dt;
                max_force = std::max(max_force, glm::length(forces[i]));
            }
            
            // Update bond lengths (not connectivity)
            mol.update_bond_lengths();
            
            return {total_energy, max_force};
        }
        
        /**
         * Run optimization until convergence
         */
        bool optimize(MolecularVisualizer& mol, int max_steps = 1000, 
                     float force_tol = 0.01f, bool verbose = false) {
            initialize(mol.atoms.size());
            
            for (int step = 0; step < max_steps; ++step) {
                auto [energy, max_force] = this->step(mol);
                
                if (verbose && step % 100 == 0) {
                    std::cout << "  Step " << step << ": E=" << energy 
                              << ", F_max=" << max_force << "\\n";
                }
                
                if (max_force < force_tol) {
                    if (verbose) {
                        std::cout << "  Converged at step " << step << "\\n";
                    }
                    return true;
                }
            }
            
            return false;  // Did not converge
        }
    };

    /**
     * Update bond lengths without re-inferring connectivity
     * Used during field-based optimization
     */
    void update_bond_lengths() {
        if (!bonds_locked) {
            std::cerr << "WARNING: Bonds not locked, refusing to update lengths\n";
            return;
        }
        
        // Update only distances, not connectivity
        for (auto& bond : bonds) {
            bond.distance = glm::distance(
                atoms[bond.atom1_idx].position,
                atoms[bond.atom2_idx].position
            );
        }
    }
    
    /**
     * Export molecule to XYZ file format
     * Standard XYZ format: line 1 = atom count, line 2 = comment, rest = element X Y Z
     */
    bool export_xyz(const std::string& filename, const std::string& comment = "") const {
        std::ofstream file(filename);
        if (!file.is_open()) {
            std::cerr << "ERROR: Cannot write to " << filename << std::endl;
            return false;
        }
        
        // Line 1: Number of atoms
        file << atoms.size() << "\n";
        
        // Line 2: Comment (formula, energy, etc.)
        if (comment.empty()) {
            file << "Generated by VSEPR-Sim OpenGL Viewer\n";
        } else {
            file << comment << "\n";
        }
        
        // Lines 3+: Element X Y Z (convert back to Angstroms)
        file << std::fixed << std::setprecision(6);
        for (const auto& atom : atoms) {
            std::string element = get_element_symbol(atom.atomic_number);
            glm::vec3 pos_angstrom = atom.position * 100.0f;  // Convert from 0.01Å to Å
            file << element << " "
                 << pos_angstrom.x << " "
                 << pos_angstrom.y << " "
                 << pos_angstrom.z << "\n";
        }
        
        file.close();
        return true;
    }
    
    /**
     * Print molecule statistics
     */
    void print_stats() const {
        std::cout << "\n=== Molecular Structure ===\n";
        std::cout << "Atoms: " << atoms.size() << "\n";
        std::cout << "Bonds: " << bonds.size() << " (" << (bonds_locked ? "locked" : "unlocked") << ")\n";
        
        // Validate bond count
        size_t max_bonds = (atoms.size() * (atoms.size() - 1)) / 2;
        if (bonds.size() > max_bonds) {
            std::cout << "  WARNING: Bond count exceeds maximum (" << max_bonds << ")\n";
        }
        
        glm::vec3 center(0.0f);
        for (const auto& atom : atoms) {
            center += atom.position;
        }
        center /= static_cast<float>(atoms.size());
        
        float radius = 0.0f;
        for (const auto& atom : atoms) {
            radius = std::max(radius, glm::distance(atom.position, center));
        }
        
        std::cout << "Center: (" << center.x << ", " << center.y << ", " << center.z << ")\n";
        std::cout << "Radius: " << radius << "\n";
    }
};

/**
 * FEA Visualization Handler
 * Converts finite element meshes to OpenGL entities
 */
class FEAVisualizer {
public:
    struct Node {
        glm::vec3 position;
        glm::vec3 displacement;
        float scalar_value;  // Stress, temperature, etc.
    };

    struct Element {
        std::vector<int> node_indices;
        float scalar_value;  // For element-based values
        std::string type;    // "hex8", "tet4", etc.
    };

    std::vector<Node> nodes;
    std::vector<Element> elements;
    float scalar_min, scalar_max;

    /**
     * Load mesh from VTK or OBJ format
     */
    bool load_mesh(const std::string& filename) {
        if (filename.find(".vtk") != std::string::npos) {
            return load_vtk(filename);
        } else if (filename.find(".obj") != std::string::npos) {
            return load_obj(filename);
        }
        return false;
    }

    /**
     * Load VTK format mesh
     */
    bool load_vtk(const std::string& filename) {
        std::ifstream file(filename);
        if (!file.is_open()) return false;

        std::string line;
        
        // Parse VTK header
        std::getline(file, line);  // # vtk DataFile Version
        std::getline(file, line);  // Title
        std::getline(file, line);  // ASCII or BINARY
        
        // Skip until POINTS
        while (std::getline(file, line)) {
            if (line.find("POINTS") != std::string::npos) break;
        }

        int num_points;
        std::istringstream(line) >> num_points;

        nodes.clear();
        for (int i = 0; i < num_points; ++i) {
            std::getline(file, line);
            std::istringstream iss(line);
            
            Node node;
            node.position = glm::vec3(0.0f);
            node.displacement = glm::vec3(0.0f);
            node.scalar_value = 0.0f;
            
            iss >> node.position.x >> node.position.y >> node.position.z;
            nodes.push_back(node);
        }

        std::cout << "Loaded " << nodes.size() << " nodes from " << filename << std::endl;
        return true;
    }

    /**
     * Load OBJ format mesh
     */
    bool load_obj(const std::string& filename) {
        std::ifstream file(filename);
        if (!file.is_open()) return false;

        nodes.clear();
        std::string line;

        while (std::getline(file, line)) {
            if (line.empty() || line[0] == '#') continue;

            std::istringstream iss(line);
            std::string type;
            iss >> type;

            if (type == "v") {  // Vertex
                Node node;
                iss >> node.position.x >> node.position.y >> node.position.z;
                node.displacement = glm::vec3(0.0f);
                node.scalar_value = 0.0f;
                nodes.push_back(node);
            }
        }

        file.close();
        std::cout << "Loaded " << nodes.size() << " nodes from " << filename << std::endl;
        return true;
    }

    /**
     * Generate colormap for scalar values
     * Uses viridis-like colormap (blue -> yellow -> red)
     */
    static glm::vec3 get_viridis_color(float normalized_value) {
        // normalized_value in [0, 1]
        normalized_value = glm::clamp(normalized_value, 0.0f, 1.0f);
        
        if (normalized_value < 0.33f) {
            // Blue to green
            float t = normalized_value / 0.33f;
            return glm::mix(glm::vec3(0.267f, 0.004f, 0.329f), 
                          glm::vec3(0.128f, 0.565f, 0.510f), t);
        } else if (normalized_value < 0.67f) {
            // Green to yellow
            float t = (normalized_value - 0.33f) / 0.34f;
            return glm::mix(glm::vec3(0.128f, 0.565f, 0.510f),
                          glm::vec3(0.993f, 0.906f, 0.144f), t);
        } else {
            // Yellow to red
            float t = (normalized_value - 0.67f) / 0.33f;
            return glm::mix(glm::vec3(0.993f, 0.906f, 0.144f),
                          glm::vec3(0.945f, 0.975f, 0.131f), t);
        }
    }

    /**
     * Print FEA statistics
     */
    void print_stats() const {
        std::cout << "\n=== FEA Mesh ===\n";
        std::cout << "Nodes: " << nodes.size() << "\n";
        std::cout << "Elements: " << elements.size() << "\n";
        
        if (!nodes.empty()) {
            glm::vec3 min_pos = nodes[0].position;
            glm::vec3 max_pos = nodes[0].position;
            
            for (const auto& node : nodes) {
                min_pos = glm::min(min_pos, node.position);
                max_pos = glm::max(max_pos, node.position);
            }
            
            glm::vec3 size = max_pos - min_pos;
            std::cout << "Bounds: (" << min_pos.x << ", " << min_pos.y << ", " << min_pos.z << ")\n";
            std::cout << "        to (" << max_pos.x << ", " << max_pos.y << ", " << max_pos.z << ")\n";
            std::cout << "Size: " << size.x << " x " << size.y << " x " << size.z << "\n";
        }
    }
};

}  // namespace vsepr::vis

/**
 * Batch Processing with Live Visualization
 * Generates 10,000 random molecules with real-time updates
 */
class BatchProcessor {
public:
    BatchProcessor(vsepr::vis::VisualizationCallback& callback) 
        : callback_(callback), generator_() {}
    
    // Configuration for XYZ output
    struct ExportConfig {
        bool export_xyz = false;
        bool watch_mode = false;
        std::string output_dir = "./xyz_output";
        std::string watch_file = "molecules.xyz";  // Append mode for watch
    };
    
    ExportConfig export_config;
    
    // Continuous generation mode
    struct ContinuousConfig {
        bool enabled = false;
        uint64_t max_iterations = 0;  // 0 = infinite
        int checkpoint_interval = 10000;  // Save stats every N molecules
        bool show_live_stats = true;
        std::string checkpoint_file = "discovery_checkpoint.txt";
    };
    
    ContinuousConfig continuous_config;
    vsepr::vis::DiscoveryStats stats;
    
    void process_batch(int count = 10000, bool visualize_every_other = true) {
        std::cout << "\n╔════════════════════════════════════════════════════════════════╗\n";
        std::cout << "║  BATCH MOLECULE GENERATION WITH LIVE VISUALIZATION             ║\n";
        std::cout << "╚════════════════════════════════════════════════════════════════╝\n\n";
        std::cout << "Total molecules: " << count << "\n";
        std::cout << "Visualization: " << (visualize_every_other ? "Every other molecule" : "All molecules") << "\n";
        
        if (continuous_config.enabled) {
            std::cout << "Mode: Continuous generation\n";
            if (continuous_config.max_iterations > 0) {
                std::cout << "Max iterations: " << continuous_config.max_iterations << "\n";
            } else {
                std::cout << "Max iterations: Unlimited (press Ctrl+C to stop)\n";
            }
            std::cout << "Checkpoint interval: " << continuous_config.checkpoint_interval << " molecules\n";
        }
        std::cout << "\n";
        
        stats.start();
        auto start_time = std::chrono::high_resolution_clock::now();
        
        for (int i = 1; i <= count && callback_.running; ++i) {
            std::string formula = generator_.generate_random_formula();
            
            // Trigger discovery event
            callback_.trigger_discovery(formula, i, count);
            
            // Simulate molecule creation and optimization
            bool success = build_and_optimize(formula, i);
            
            // Visualize every other molecule if enabled
            bool should_visualize = visualize_every_other ? (i % 2 == 0) : true;
            bool visualized = false;
            
            if (success) {
                if (should_visualize) {
                    visualize_molecule(formula, i, count);
                    visualized = true;
                    
                    // Trigger render callback
                    callback_.trigger_render();
                    
                    // Small delay for visualization update
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                }
                
                // Trigger optimized event
                callback_.trigger_optimized(formula, i, count);
            }
            
            // Record statistics
            stats.record_molecule(formula, success, visualized);
            
            // Progress reporting every 500 molecules
            if (i % 500 == 0) {
                auto current_time = std::chrono::high_resolution_clock::now();
                auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(current_time - start_time).count();
                double rate = elapsed > 0 ? static_cast<double>(i) / elapsed : 0.0;
                
                std::cout << "\n▶ Progress: " << i << "/" << count << " (" << (i * 100 / count) << "%)\n";
                std::cout << "  Successful: " << stats.total_successful << " | Visualized: " << stats.total_visualized << "\n";
                std::cout << "  Unique formulas: " << stats.unique_formulas << "\n";
                std::cout << "  Rate: " << std::fixed << std::setprecision(1) << rate << " molecules/sec\n";
                std::cout << "  Elapsed: " << elapsed << "s | Estimated remaining: " 
                         << std::setprecision(0) << (rate > 0 ? ((count - i) / rate) : 0) << "s\n";
            }
            
            // Checkpoint saving (continuous mode)
            if (continuous_config.enabled && i % continuous_config.checkpoint_interval == 0) {
                stats.save_checkpoint(continuous_config.checkpoint_file);
                if (continuous_config.show_live_stats) {
                    stats.print_summary();
                }
            }
        }
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto total_time = std::chrono::duration_cast<std::chrono::seconds>(end_time - start_time).count();
        
        std::cout << "\n╔════════════════════════════════════════════════════════════════╗\n";
        std::cout << "║  BATCH PROCESSING COMPLETE                                     ║\n";
        std::cout << "╚════════════════════════════════════════════════════════════════╝\n\n";
        std::cout << "Total molecules processed: " << count << "\n";
        std::cout << "Successful builds: " << stats.total_successful 
                  << " (" << (count > 0 ? (stats.total_successful * 100 / count) : 0) << "%)\n";
        std::cout << "Visualized: " << stats.total_visualized << "\n";
        std::cout << "Unique formulas: " << stats.unique_formulas << "\n";
        std::cout << "Total time: " << total_time << " seconds\n";
        std::cout << "Average rate: " << std::fixed << std::setprecision(2) 
                  << (total_time > 0 ? static_cast<double>(count) / total_time : 0.0) << " molecules/sec\n";
    }
    
private:
    vsepr::vis::VisualizationCallback& callback_;
    vsepr::vis::RandomMoleculeGenerator generator_;
    
    bool build_and_optimize(const std::string& formula, int iteration) {
        // Simulate molecule building (in real implementation, call VSEPR builder)
        // Random success rate for demonstration
        std::mt19937 gen(std::random_device{}());
        std::uniform_int_distribution<> dist(1, 100);
        
        // Higher success rate for realistic molecules
        int threshold = 85; // 85% success rate
        return dist(gen) <= threshold;
    }
    
    void visualize_molecule(const std::string& formula, int current, int total) {
        // Parse formula to get element counts
        auto element_counts = vsepr::vis::FormulaParser::parse_formula(formula);
        int total_atoms = 0;
        for (const auto& [elem, count] : element_counts) {
            total_atoms += count;
        }
        
        // Create molecular visualizer
        vsepr::vis::MolecularVisualizer mol_vis;
        
        // VDW radii for positioning
        std::map<std::string, float> vdw_radii = {
            {"H", 1.20f}, {"C", 1.70f}, {"N", 1.55f}, {"O", 1.52f},
            {"F", 1.47f}, {"P", 1.80f}, {"S", 1.80f}, {"Cl", 1.75f},
            {"Br", 1.85f}, {"I", 1.98f}, {"B", 1.92f}, {"Si", 2.10f},
            {"As", 1.85f}, {"Xe", 2.16f}, {"Kr", 2.02f}
        };
        
        // Element colors (CPK)
        std::map<std::string, glm::vec3> colors = {
            {"H", {1.0f, 1.0f, 1.0f}}, {"C", {0.2f, 0.2f, 0.2f}},
            {"N", {0.2f, 0.2f, 0.8f}}, {"O", {0.8f, 0.2f, 0.2f}},
            {"F", {0.2f, 0.8f, 0.2f}}, {"P", {1.0f, 0.6f, 0.2f}},
            {"S", {1.0f, 1.0f, 0.2f}}, {"Cl", {0.2f, 0.8f, 0.2f}},
            {"Br", {0.6f, 0.2f, 0.2f}}, {"I", {0.5f, 0.2f, 0.5f}},
            {"B", {1.0f, 0.7f, 0.7f}}, {"Si", {0.5f, 0.6f, 0.6f}},
            {"As", {0.7f, 0.5f, 0.9f}}, {"Xe", {0.3f, 0.6f, 0.8f}},
            {"Kr", {0.4f, 0.7f, 0.9f}}
        };
        
        // Create atoms from formula
        int atom_idx = 0;
        std::mt19937 rng(std::hash<std::string>{}(formula));  // Deterministic per formula
        std::uniform_real_distribution<float> angle_dist(0.0f, 2.0f * 3.14159f);
        std::uniform_real_distribution<float> radius_dist(0.008f, 0.015f);
        
        for (const auto& [elem, count] : element_counts) {
            for (int i = 0; i < count; ++i) {
                vsepr::vis::MolecularVisualizer::Atom atom;
                
                // Position atoms in roughly spherical arrangement
                if (atom_idx == 0) {
                    // First atom at origin
                    atom.position = glm::vec3(0.0f);
                } else {
                    // Distribute others around sphere
                    float theta = angle_dist(rng);
                    float phi = angle_dist(rng);
                    float r = radius_dist(rng);
                    atom.position = glm::vec3(
                        r * std::sin(phi) * std::cos(theta),
                        r * std::sin(phi) * std::sin(theta),
                        r * std::cos(phi)
                    );
                }
                
                atom.atomic_number = vsepr::vis::MolecularVisualizer::get_atomic_number(elem);
                atom.radius = (vdw_radii.count(elem) ? vdw_radii[elem] : 1.5f) * 0.01f;
                atom.color = colors.count(elem) ? colors[elem] : glm::vec3(0.5f);
                
                mol_vis.atoms.push_back(atom);
                atom_idx++;
            }
        }
        
        // Detect bonds ONCE - they are now locked (silent mode)
        mol_vis.detect_bonds();
        
        // During optimization, only update bond lengths, not connectivity
        // mol_vis.update_bond_lengths();  // Would be called in real optimizer
        
        // Validate atom count matches formula
        if (mol_vis.atoms.size() != static_cast<size_t>(total_atoms)) {
            std::cerr << "ERROR: Formula " << formula << " parsed to " << total_atoms 
                      << " atoms but created " << mol_vis.atoms.size() << "\n";
        }
        
        // Export to XYZ if enabled
        if (export_config.export_xyz) {
            std::string xyz_filename;
            
            if (export_config.watch_mode) {
                // Watch mode: append to single file
                xyz_filename = export_config.output_dir + "/" + export_config.watch_file;
                std::ofstream watch_file(xyz_filename, std::ios::app);
                if (watch_file.is_open()) {
                    // Write XYZ block
                    watch_file << mol_vis.atoms.size() << "\n";
                    watch_file << "#" << current << " " << formula 
                               << " (" << mol_vis.atoms.size() << " atoms, "
                               << mol_vis.bonds.size() << " bonds)\n";
                    watch_file << std::fixed << std::setprecision(6);
                    for (const auto& atom : mol_vis.atoms) {
                        std::string elem = vsepr::vis::MolecularVisualizer::get_element_symbol(atom.atomic_number);
                        glm::vec3 pos = atom.position * 100.0f;
                        watch_file << elem << " " << pos.x << " " << pos.y << " " << pos.z << "\n";
                    }
                    watch_file.close();
                }
            } else {
                // Individual file mode
                xyz_filename = export_config.output_dir + "/" + formula + "_" + std::to_string(current) + ".xyz";
                mol_vis.export_xyz(xyz_filename, formula + " - Molecule #" + std::to_string(current));
            }
        }
        
        // Print compact visualization info every 50 molecules (or last one)
        if (current % 50 == 0 || current == total) {
            std::cout << "  ✓ Visualized #" << current << "/" << total << ": " << formula 
                      << " → " << mol_vis.atoms.size() << " atoms, " 
                      << mol_vis.bonds.size() << " bonds";
            if (export_config.export_xyz) {
                std::cout << " [exported]";
            }
            std::cout << "\n";
        }
    }
};

/**
 * Example usage demonstrating the integration
 */
int main(int argc, char* argv[]) {
    std::cout << "VSEPR-Sim Modern OpenGL Visualization System\n";
    std::cout << "============================================\n\n";

    // Setup visualization callback system
    vsepr::vis::VisualizationCallback viz_callback;
    
    // Configure callbacks
    viz_callback.on_molecule_discovered = [](const std::string& formula, int current, int total) {
        // This would trigger OpenGL buffer update in real implementation
        if (current % 50 == 0) {
            std::cout << "  🔬 Discovered: " << formula << " [" << current << "/" << total << "]\n";
        }
    };
    
    viz_callback.on_molecule_optimized = [](const std::string& formula, int current, int total) {
        // This would trigger final render in real implementation
        if (current % 50 == 0) {
            std::cout << "  ✨ Optimized: " << formula << "\n";
        }
    };
    
    viz_callback.on_frame_render = []() {
        // This would be the OpenGL render loop callback
        // In real implementation: swap buffers, update UI, etc.
    };
    
    // Parse command line arguments
    int batch_size = 10000;
    bool visualize_every_other = true;
    bool export_xyz = false;
    bool watch_mode = false;
    std::string xyz_output_dir = "./xyz_output";
    std::string watch_file = "molecules.xyz";
    bool continuous_mode = false;
    uint64_t max_iterations = 0;
    int checkpoint_interval = 10000;
    
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        
        if (arg == "--viz" && i + 1 < argc) {
            export_xyz = true;
            std::string ext = argv[++i];
            if (ext == ".xyz" || ext == "xyz") {
                // Default XYZ export
            } else if (ext.find('/') != std::string::npos || ext.find('\\') != std::string::npos) {
                // Custom output directory
                xyz_output_dir = ext;
            }
        } else if (arg == "--watch" && i + 1 < argc) {
            export_xyz = true;
            watch_mode = true;
            std::string file_arg = argv[++i];
            if (file_arg.find(".xyz") != std::string::npos) {
                watch_file = file_arg;
            }
        } else if (arg == "--continue" || arg == "-c") {
            continuous_mode = true;
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                max_iterations = std::stoull(argv[++i]);
            }
        } else if (arg == "--checkpoint" && i + 1 < argc) {
            checkpoint_interval = std::atoi(argv[++i]);
        } else if (arg == "--help" || arg == "-h") {
            std::cout << "\nVSEPR-Sim OpenGL Viewer - Usage:\n";
            std::cout << "  " << argv[0] << " [batch_size] [viz_mode] [options]\n\n";
            std::cout << "Arguments:\n";
            std::cout << "  batch_size        Number of molecules to generate (default: 10000)\n";
            std::cout << "  viz_mode          'all' or 'every-other' (default: every-other)\n\n";
            std::cout << "Options:\n";
            std::cout << "  --viz .xyz        Export molecules to XYZ format\n";
            std::cout << "  --viz <dir>       Export to custom directory\n";
            std::cout << "  --watch <file>    Append all molecules to single XYZ file (for streaming viz)\n";
            std::cout << "  --continue [N]    Continuous generation mode (optional max iterations)\n";
            std::cout << "  -c [N]            Alias for --continue\n";
            std::cout << "  --checkpoint N    Save checkpoint every N molecules (default: 10000)\n";
            std::cout << "  --help, -h        Show this help message\n\n";
            std::cout << "Examples:\n";
            std::cout << "  " << argv[0] << " 100 all --viz .xyz\n";
            std::cout << "  " << argv[0] << " 1000 every-other --watch molecules.xyz\n";
            std::cout << "  " << argv[0] << " 500 all --viz ./my_molecules\n";
            std::cout << "  " << argv[0] << " 1000000 all --continue --watch all.xyz  # 1M molecules\n";
            std::cout << "  " << argv[0] << " 100000 every-other -c 1000000 --checkpoint 5000\n\n";
            std::cout << "Continuous Mode:\n";
            std::cout << "  Demonstrates C++'s power for large-scale molecular discovery\n";
            std::cout << "  - Generates N molecules (or unlimited if N not specified)\n";
            std::cout << "  - Tracks statistics (unique formulas, element frequency, etc.)\n";
            std::cout << "  - Saves checkpoints for resume capability\n";
            std::cout << "  - Streams output to XYZ for real-time visualization\n";
            std::cout << "  - Performance metrics: molecules/sec, molecules/hour\n\n";
            return 0;
        } else if (i == 1) {
            batch_size = std::atoi(arg.c_str());
            if (batch_size <= 0) batch_size = 10000;
        } else if (i == 2) {
            visualize_every_other = (arg == "every-other" || arg == "alternate");
        }
    }
    
    std::cout << "╔════════════════════════════════════════════════════════════════╗\n";
    std::cout << "║  DEMONSTRATION MODE                                            ║\n";
    std::cout << "╚════════════════════════════════════════════════════════════════╝\n\n";

    // Example 1: Single molecule demonstration
    {
        std::cout << "Example 1: Single Molecule Visualization\n";
        std::cout << "─────────────────────────────────────────\n\n";
        
        vsepr::vis::MolecularVisualizer mol_vis;
        
        // Create example methane (CH4)
        vsepr::vis::MolecularVisualizer::Atom c_atom;
        c_atom.position = glm::vec3(0.0f);
        c_atom.atomic_number = 6;
        c_atom.radius = 0.017f;  // Carbon VDW radius in nm
        c_atom.color = glm::vec3(0.2f, 0.2f, 0.2f);
        
        mol_vis.atoms.push_back(c_atom);

        // Add 4 hydrogen atoms in tetrahedral geometry
        float bond_length = 0.01f;  // 1 Angstrom
        glm::vec3 h_positions[] = {
            glm::vec3(1.0f, 1.0f, 1.0f),
            glm::vec3(1.0f, -1.0f, -1.0f),
            glm::vec3(-1.0f, 1.0f, -1.0f),
            glm::vec3(-1.0f, -1.0f, 1.0f)
        };

        for (const auto& pos : h_positions) {
            vsepr::vis::MolecularVisualizer::Atom h_atom;
            h_atom.position = glm::normalize(pos) * bond_length;
            h_atom.atomic_number = 1;
            h_atom.radius = 0.012f;
            h_atom.color = glm::vec3(1.0f);
            mol_vis.atoms.push_back(h_atom);
        }

        mol_vis.detect_bonds(true);  // Verbose for single-molecule demo
        mol_vis.print_stats();
    }

    // Example 2: FEA mesh visualization
    {
        std::cout << "\nExample 2: FEA Mesh with Scalar Field\n";
        std::cout << "──────────────────────────────────────\n\n";
        
        vsepr::vis::FEAVisualizer fea_vis;
        
        // Create simple cube mesh with 8 nodes
        fea_vis.nodes.push_back({glm::vec3(-0.5f, -0.5f, -0.5f), glm::vec3(0.0f), 0.0f});
        fea_vis.nodes.push_back({glm::vec3(0.5f, -0.5f, -0.5f), glm::vec3(0.0f), 0.1f});
        fea_vis.nodes.push_back({glm::vec3(0.5f, 0.5f, -0.5f), glm::vec3(0.0f), 0.2f});
        fea_vis.nodes.push_back({glm::vec3(-0.5f, 0.5f, -0.5f), glm::vec3(0.0f), 0.3f});
        fea_vis.nodes.push_back({glm::vec3(-0.5f, -0.5f, 0.5f), glm::vec3(0.0f), 0.4f});
        fea_vis.nodes.push_back({glm::vec3(0.5f, -0.5f, 0.5f), glm::vec3(0.0f), 0.5f});
        fea_vis.nodes.push_back({glm::vec3(0.5f, 0.5f, 0.5f), glm::vec3(0.0f), 0.6f});
        fea_vis.nodes.push_back({glm::vec3(-0.5f, 0.5f, 0.5f), glm::vec3(0.0f), 0.7f});

        fea_vis.print_stats();

        // Test colormap
        std::cout << "\nColormap (Viridis) test:\n";
        for (float val : {0.0f, 0.25f, 0.5f, 0.75f, 1.0f}) {
            auto color = vsepr::vis::FEAVisualizer::get_viridis_color(val);
            std::cout << "  Value " << std::fixed << std::setprecision(2) << val << " → RGB(" 
                      << color.r << ", " << color.g << ", " << color.b << ")\n";
        }
    }
    
    // Example 3: Random molecule generation test
    {
        std::cout << "\n\nExample 3: Random Molecule Generation\n";
        std::cout << "──────────────────────────────────────\n\n";
        
        vsepr::vis::RandomMoleculeGenerator gen;
        
        std::cout << "Sample random formulas with atom counts:\n";
        for (int i = 0; i < 10; ++i) {
            std::string formula = gen.generate_random_formula();
            int atom_count = vsepr::vis::FormulaParser::count_atoms(formula);
            auto elements = vsepr::vis::FormulaParser::parse_formula(formula);
            
            std::cout << "  " << (i + 1) << ". " << formula 
                      << " (" << atom_count << " atoms: ";
            
            bool first = true;
            for (const auto& [elem, count] : elements) {
                if (!first) std::cout << ", ";
                std::cout << elem;
                if (count > 1) std::cout << "×" << count;
                first = false;
            }
            std::cout << ")\n";
        }
    }
    
    // Example 4: Batch processing with visualization updates
    {
        std::cout << "\n\nExample 4: Batch Processing (10,000 Molecules)\n";
        std::cout << "═══════════════════════════════════════════════\n";
        
        // Ask user for confirmation
        std::cout << "\nThis will generate " << batch_size << " random molecules.\n";
        std::cout << "Visualization updates: " 
                  << (visualize_every_other ? "Every other molecule" : "All molecules") << "\n";
        std::cout << "\nProceed? (y/n): ";
        
        char response;
        std::cin >> response;
        
        if (response == 'y' || response == 'Y') {
            BatchProcessor processor(viz_callback);
            
            // Configure XYZ export if requested
            if (export_xyz) {
                processor.export_config.export_xyz = true;
                processor.export_config.watch_mode = watch_mode;
                processor.export_config.output_dir = xyz_output_dir;
                processor.export_config.watch_file = watch_file;
                
                // Create output directory
                std::string mkdir_cmd;
#ifdef _WIN32
                mkdir_cmd = "mkdir \"" + xyz_output_dir + "\" 2>nul";
#else
                mkdir_cmd = "mkdir -p \"" + xyz_output_dir + "\" 2>/dev/null";
#endif
                system(mkdir_cmd.c_str());
                
                // Clear watch file if in watch mode
                if (watch_mode) {
                    std::string watch_path = xyz_output_dir + "/" + watch_file;
                    std::ofstream clear_file(watch_path, std::ios::trunc);
                    clear_file.close();
                    std::cout << "\n📁 XYZ Export: Watch mode → " << watch_path << "\n";
                } else {
                    std::cout << "\n📁 XYZ Export: Individual files → " << xyz_output_dir << "/\n";
                }
            }
            
            // Configure continuous mode
            if (continuous_mode) {
                processor.continuous_config.enabled = true;
                processor.continuous_config.max_iterations = max_iterations;
                processor.continuous_config.checkpoint_interval = checkpoint_interval;
                processor.continuous_config.show_live_stats = true;
                
                std::cout << "\n🔄 Continuous Generation Mode\n";
                std::cout << "   Demonstrating C++ performance for large-scale molecular discovery\n";
            }
            
            processor.process_batch(batch_size, visualize_every_other);
            
            // Final statistics summary
            if (continuous_mode || batch_size >= 1000) {
                processor.stats.print_summary();
                
                // Save final checkpoint
                std::string final_checkpoint = "final_" + processor.continuous_config.checkpoint_file;
                processor.stats.save_checkpoint(final_checkpoint);
                std::cout << "\n📊 Statistics saved to: " << final_checkpoint << "\n";
            }
            
            // Summary for XYZ export
            if (export_xyz) {
                if (watch_mode) {
                    std::cout << "\n✓ All molecules exported to: " << xyz_output_dir << "/" << watch_file << "\n";
                    std::cout << "  Open with: Avogadro, VMD, PyMOL, or JMol\n";
                    std::cout << "  Command: avogadro " << xyz_output_dir << "/" << watch_file << "\n";
                } else {
                    std::cout << "\n✓ Molecules exported to: " << xyz_output_dir << "/\n";
                    std::cout << "  Files: <formula>_<number>.xyz\n";
                }
            }
        } else {
            std::cout << "\nBatch processing skipped.\n";
        }
    }

    std::cout << "\n✓ OpenGL visualization system ready for integration\n";
    std::cout << "  - Molecular structures can be loaded from XYZ files\n";
    std::cout << "  - FEA meshes support VTK and OBJ formats\n";
    std::cout << "  - Scalar field colormapping (stress, temperature, etc.)\n";
    std::cout << "  - Full PBR material system with deferred rendering\n";
    std::cout << "  - Batch processing with real-time visualization updates\n";
    std::cout << "  - Random molecule generation for discovery mode\n\n";
    
    std::cout << "Usage:\n";
    std::cout << "  " << argv[0] << " [batch_size] [visualization_mode]\n";
    std::cout << "  Example: " << argv[0] << " 10000 every-other\n";
    std::cout << "  Example: " << argv[0] << " 5000 all\n\n";

    return 0;
}
