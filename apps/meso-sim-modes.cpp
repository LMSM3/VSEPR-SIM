/**
 * meso-sim: Additional simulation modes (MD, adaptive, prediction)
 * Part 2 of main implementation
 */

#include "meso-sim-config.hpp"
#include "io/xyz_format.hpp"
#include "atomistic/core/state.hpp"
#include "atomistic/parsers/xyz_parser.hpp"
#include "atomistic/compilers/xyz_compiler.hpp"
#include "atomistic/models/model.hpp"
#include "atomistic/models/bonded.hpp"
#include "atomistic/integrators/fire.hpp"
#include "atomistic/core/thermodynamics.hpp"
#include "atomistic/core/statistics.hpp"
#include "atomistic/predict/properties.hpp"

#include <iostream>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <vector>
#include <random>
#include <filesystem>

using namespace atomistic;

// ============================================================================
// MODE 4: MOLECULAR DYNAMICS (NVE)
// ============================================================================

void mode_md_nve(const SimConfig& config) {
    std::cout << "═══ MODE: Molecular Dynamics (NVE) ═══\n\n";
    
    vsepr::io::XYZReader reader;
    vsepr::io::XYZMolecule mol;
    if (!reader.read(config.input_file, mol)) {
        std::cerr << "Error: " << reader.get_error() << "\n";
        return;
    }
    
    State s = parsers::from_xyz(mol);
    std::cout << "System: " << s.N << " atoms\n";
    std::cout << "Timestep: " << config.timestep << " fs\n";
    std::cout << "Total steps: " << config.md_steps << "\n\n";
    
    // Initialize velocities
    std::mt19937 rng(12345);
    thermo::initialize_velocities_mb(s, config.temperature, rng);
    
    // Build model
    ModelParams p{.rc=config.cutoff, .eps=config.epsilon, .sigma=config.sigma};
    auto model = create_lj_coulomb_model();
    
    // Storage for trajectory analysis
    std::vector<double> E_traj, T_traj, Rg_traj;
    
    create_output_directory(config.output_dir);
    std::ofstream traj_file(config.output_dir + "/trajectory.csv");
    traj_file << "step,time_fs,T_K,E_kin,E_pot,E_tot,Rg\n";
    
    std::cout << "Running NVE dynamics...\n";
    
    for (int step = 0; step < config.md_steps; ++step) {
        // Velocity Verlet integration
        // v(t+dt/2) = v(t) + (dt/2) * F(t)/m
        for (uint32_t i = 0; i < s.N; ++i) {
            if (s.M[i] > 0) {
                s.V[i] = s.V[i] + s.F[i] * (0.5 * config.timestep / s.M[i]);
            }
        }
        
        // x(t+dt) = x(t) + dt * v(t+dt/2)
        for (uint32_t i = 0; i < s.N; ++i) {
            s.X[i] = s.X[i] + s.V[i] * config.timestep;
        }
        
        // F(t+dt) from new positions
        model->eval(s, p);
        
        // v(t+dt) = v(t+dt/2) + (dt/2) * F(t+dt)/m
        for (uint32_t i = 0; i < s.N; ++i) {
            if (s.M[i] > 0) {
                s.V[i] = s.V[i] + s.F[i] * (0.5 * config.timestep / s.M[i]);
            }
        }
        
        // Monitor properties
        if (step % config.save_interval == 0) {
            double T = thermo::temperature(s);
            double K = thermo::kinetic_energy(s);
            double U = s.E.total();
            double Rg = thermo::radius_of_gyration(s);
            
            E_traj.push_back(K + U);
            T_traj.push_back(T);
            Rg_traj.push_back(Rg);
            
            traj_file << step << "," << step * config.timestep << ","
                     << T << "," << K << "," << U << "," << (K+U) << "," << Rg << "\n";
            
            std::cout << "  Step " << step << ": T=" << T << " K, E=" << (K+U) << " kcal/mol\r" << std::flush;
        }
    }
    
    std::cout << "\n\nSimulation complete!\n";
    std::cout << "Energy drift: " << (E_traj.back() - E_traj.front()) / E_traj.front() * 100 << "%\n";
    
    // Save final frame
    std::vector<std::string> elem_names;
    for (const auto& atom : mol.atoms) elem_names.push_back(atom.element);
    compilers::save_xyza(config.output_dir + "/final_frame.xyza", s, elem_names);
    
    std::cout << "\nOutput saved to: " << config.output_dir << "/\n";
}

// ============================================================================
// MODE 5: MOLECULAR DYNAMICS (NVT)
// ============================================================================

void mode_md_nvt(const SimConfig& config) {
    std::cout << "═══ MODE: Molecular Dynamics (NVT) ═══\n\n";
    std::cout << "Temperature: " << config.temperature << " K\n";
    
    vsepr::io::XYZReader reader;
    vsepr::io::XYZMolecule mol;
    if (!reader.read(config.input_file, mol)) {
        std::cerr << "Error: " << reader.get_error() << "\n";
        return;
    }
    
    State s = parsers::from_xyz(mol);
    
    // Initialize velocities
    std::mt19937 rng(12345);
    thermo::initialize_velocities_mb(s, config.temperature, rng);
    
    ModelParams p{.rc=config.cutoff, .eps=config.epsilon, .sigma=config.sigma};
    auto model = create_lj_coulomb_model();
    
    create_output_directory(config.output_dir);
    std::ofstream traj_file(config.output_dir + "/trajectory.csv");
    traj_file << "step,time_fs,T_K,E_kin,E_pot,E_tot\n";
    
    std::cout << "Running NVT dynamics...\n";
    
    for (int step = 0; step < config.md_steps; ++step) {
        // Velocity Verlet
        for (uint32_t i = 0; i < s.N; ++i) {
            if (s.M[i] > 0) {
                s.V[i] = s.V[i] + s.F[i] * (0.5 * config.timestep / s.M[i]);
            }
        }
        
        for (uint32_t i = 0; i < s.N; ++i) {
            s.X[i] = s.X[i] + s.V[i] * config.timestep;
        }
        
        model->eval(s, p);
        
        for (uint32_t i = 0; i < s.N; ++i) {
            if (s.M[i] > 0) {
                s.V[i] = s.V[i] + s.F[i] * (0.5 * config.timestep / s.M[i]);
            }
        }
        
        // Thermostat every 10 steps
        if (step % 10 == 0) {
            thermo::rescale_velocities(s, config.temperature, 100.0, config.timestep);
        }
        
        // Save trajectory
        if (step % config.save_interval == 0) {
            double T = thermo::temperature(s);
            double K = thermo::kinetic_energy(s);
            double U = s.E.total();
            
            traj_file << step << "," << step * config.timestep << ","
                     << T << "," << K << "," << U << "," << (K+U) << "\n";
            
            std::cout << "  Step " << step << ": T=" << T << " K\r" << std::flush;
        }
    }
    
    std::cout << "\n\nOutput saved to: " << config.output_dir << "/\n";
}

// ============================================================================
// MODE 6: ADAPTIVE SAMPLING
// ============================================================================

void mode_adaptive(const SimConfig& config) {
    std::cout << "═══ MODE: Adaptive Sampling ═══\n\n";
    
    vsepr::io::XYZReader reader;
    vsepr::io::XYZMolecule mol;
    if (!reader.read(config.input_file, mol)) {
        std::cerr << "Error: " << reader.get_error() << "\n";
        return;
    }
    
    State initial = parsers::from_xyz(mol);
    
    ModelParams p{.rc=config.cutoff, .eps=config.epsilon, .sigma=config.sigma};
    auto model = create_generic_bonded_model(initial);
    
    OnlineStats energy_stats;
    StationarityGate gate(config.convergence_tol, config.convergence_window, 10);
    
    std::mt19937 rng(12345);
    std::normal_distribution<double> noise(0.0, 0.2);
    
    std::cout << "Sampling until convergence (max " << config.max_samples << " samples)...\n\n";
    
    for (int run = 0; run < config.max_samples; ++run) {
        State s = initial;
        
        // Perturb structure
        for (auto& x : s.X) {
            x.x += noise(rng);
            x.y += noise(rng);
            x.z += noise(rng);
        }
        
        // Minimize
        FIREParams fp;
        fp.max_steps = 500;
        FIRE fire(*model, p);
        fire.minimize(s, fp);
        
        
        // Track energy
        double current_energy = s.E.total();
        energy_stats.add_sample(current_energy);
        
        // Check convergence
        if (run > config.convergence_window) {
            if (gate.test(energy_stats, current_energy)) {
                std::cout << "✓ Converged after " << run << " samples\n\n";
                break;
            }
        }
        
        if ((run+1) % 50 == 0) {
            std::cout << "  Sample " << (run+1) << ": E_mean=" 
                     << energy_stats.get_mean() << " kcal/mol\r" << std::flush;
        }
    }
    
    std::cout << "\nFinal statistics:\n";
    std::cout << "  Mean energy: " << energy_stats.get_mean() << " ± " 
              << std::sqrt(energy_stats.get_variance()) << " kcal/mol\n";
    std::cout << "  Samples: " << energy_stats.count() << "\n\n";
}

// ============================================================================
// MODE 7: PROPERTY PREDICTION
// ============================================================================

void mode_predict(const SimConfig& config) {
    std::cout << "═══ MODE: Property Prediction ═══\n\n";
    
    vsepr::io::XYZReader reader;
    vsepr::io::XYZMolecule mol;
    if (!reader.read(config.input_file, mol)) {
        std::cerr << "Error: " << reader.get_error() << "\n";
        return;
    }
    
    State s = parsers::from_xyz(mol);
    std::cout << "Analyzing: " << s.N << " atoms, " << s.B.size() << " bonds\n\n";
    
    // Predict electronic properties
    auto elec_props = predict::predict_electronic_properties(s);
    
    std::cout << "Electronic Properties:\n";
    std::cout << "  Dipole moment:       " << elec_props.dipole_moment << " Debye\n";
    std::cout << "  Polarizability:      " << elec_props.polarizability << " Å³\n";
    std::cout << "  Ionization potential:" << elec_props.ionization_potential << " eV\n";
    std::cout << "  Electron affinity:   " << elec_props.electron_affinity << " eV\n";
    std::cout << "  Electronegativity:   " << elec_props.electronegativity << " eV\n";
    std::cout << "  Hardness:            " << elec_props.hardness << " eV\n";
    std::cout << "  Electrophilicity:    " << elec_props.electrophilicity << " eV\n\n";
    
    std::cout << "Partial Charges:\n";
    std::vector<std::string> elem_names;
    for (const auto& atom : mol.atoms) elem_names.push_back(atom.element);
    
    for (uint32_t i = 0; i < s.N; ++i) {
        std::cout << "  " << elem_names[i] << i+1 << ": " 
                 << std::fixed << std::setprecision(3) 
                 << elec_props.partial_charges[i] << " e\n";
    }
    
    // Reactivity indices
    auto reactivity = predict::predict_reactivity(s, elec_props);
    
    std::cout << "\nMost Reactive Sites:\n";
    
    // Find most electrophilic site (highest f-)
    uint32_t max_elec = 0;
    for (uint32_t i = 1; i < s.N; ++i) {
        if (reactivity.fukui_minus[i] > reactivity.fukui_minus[max_elec]) {
            max_elec = i;
        }
    }
    std::cout << "  Electrophilic attack: " << elem_names[max_elec] << (max_elec+1) 
             << " (f- = " << reactivity.fukui_minus[max_elec] << ")\n";
    
    // Find most nucleophilic site (highest f+)
    uint32_t max_nuc = 0;
    for (uint32_t i = 1; i < s.N; ++i) {
        if (reactivity.fukui_plus[i] > reactivity.fukui_plus[max_nuc]) {
            max_nuc = i;
        }
    }
    std::cout << "  Nucleophilic attack:  " << elem_names[max_nuc] << (max_nuc+1) 
             << " (f+ = " << reactivity.fukui_plus[max_nuc] << ")\n\n";
    
    // Save results
    create_output_directory(config.output_dir);
    std::ofstream report(config.output_dir + "/prediction_report.txt");
    report << "Property Prediction Report\n";
    report << "==========================\n\n";
    report << "Dipole moment: " << elec_props.dipole_moment << " Debye\n";
    report << "Ionization potential: " << elec_props.ionization_potential << " eV\n";
    // ... (full report)
    
    std::cout << "Report saved to: " << config.output_dir << "/prediction_report.txt\n";
}

// ============================================================================
// MODE 8: REACTION PREDICTION
// ============================================================================

void mode_reaction(const SimConfig& config) {
    std::cout << "═══ MODE: Reaction Energy & Barrier ═══\n\n";
    
    if (config.merge_files.size() < 2) {
        std::cerr << "Error: Need at least 2 files (reactant and product)\n";
        std::cerr << "Usage: meso-sim reaction reactant.xyz product.xyz\n";
        return;
    }
    
    vsepr::io::XYZReader reader;
    vsepr::io::XYZMolecule mol_r, mol_p;
    
    if (!reader.read(config.merge_files[0], mol_r)) {
        std::cerr << "Error reading reactant: " << reader.get_error() << "\n";
        return;
    }
    
    if (!reader.read(config.merge_files[1], mol_p)) {
        std::cerr << "Error reading product: " << reader.get_error() << "\n";
        return;
    }
    
    State reactant = parsers::from_xyz(mol_r);
    State product = parsers::from_xyz(mol_p);
    
    std::cout << "Reactant: " << reactant.N << " atoms\n";
    std::cout << "Product:  " << product.N << " atoms\n\n";
    
    // Predict reaction energy
    double delta_E = predict::predict_reaction_energy(reactant, State{}, product, State{});
    
    std::cout << "Predicted ΔE: " << delta_E << " kcal/mol\n";
    
    if (delta_E < 0) {
        std::cout << "  → Exothermic reaction\n";
    } else {
        std::cout << "  → Endothermic reaction\n";
    }
    
    // Predict activation barrier
    double Ea = predict::predict_activation_barrier(reactant, product, 15.0);
    
    std::cout << "\nPredicted Ea: " << Ea << " kcal/mol\n";
    std::cout << "  (using Bell-Evans-Polanyi principle)\n\n";
    
    // Estimate rate constant (Arrhenius)
    double T = 298.15;  // K
    double k_B = 0.001987;  // kcal/(mol·K)
    double A = 1e13;  // Pre-exponential factor (s⁻¹)
    double k = A * std::exp(-Ea / (k_B * T));
    
    std::cout << "Estimated rate at " << T << " K:\n";
    std::cout << "  k ≈ " << k << " s⁻¹\n\n";
}

// ============================================================================
// MODE 9: DATA MERGING
// ============================================================================

void mode_merge(const SimConfig& config) {
    std::cout << "═══ MODE: Data Merging & Analysis ═══\n\n";
    
    if (config.merge_files.empty()) {
        std::cerr << "Error: No input directories specified\n";
        return;
    }
    
    std::cout << "Merging data from " << config.merge_files.size() << " sources...\n\n";
    
    std::vector<State> all_structures;
    std::vector<double> all_energies;
    
    // Load all structures
    for (const auto& dir : config.merge_files) {
        // Try to load trajectory.csv or energy.xyza
        std::string csv_file = dir + "/trajectory.csv";
        std::string xyza_file = dir + "/energy.xyza";
        
        if (std::filesystem::exists(csv_file)) {
            std::ifstream f(csv_file);
            std::string line;
            std::getline(f, line);  // Skip header
            
            while (std::getline(f, line)) {
                std::istringstream iss(line);
                std::string token;
                int col = 0;
                double E_tot = 0;
                
                while (std::getline(iss, token, ',')) {
                    if (col == 5) {  // E_tot column
                        E_tot = std::stod(token);
                    }
                    col++;
                }
                
                all_energies.push_back(E_tot);
            }
            
            std::cout << "  Loaded " << (all_energies.size()) << " frames from " << dir << "\n";
        }
    }
    
    if (all_energies.empty()) {
        std::cout << "No trajectory data found.\n";
        return;
    }
    
    // Compute statistics
    double E_mean = 0.0;
    for (double E : all_energies) E_mean += E;
    E_mean /= all_energies.size();
    
    double E_var = 0.0;
    for (double E : all_energies) E_var += (E - E_mean) * (E - E_mean);
    E_var /= all_energies.size();
    
    std::cout << "\nCombined Statistics:\n";
    std::cout << "  Total frames: " << all_energies.size() << "\n";
    std::cout << "  Mean energy:  " << E_mean << " ± " << std::sqrt(E_var) << " kcal/mol\n";
    std::cout << "  Min energy:   " << *std::min_element(all_energies.begin(), all_energies.end()) << " kcal/mol\n";
    std::cout << "  Max energy:   " << *std::max_element(all_energies.begin(), all_energies.end()) << " kcal/mol\n\n";
    
    // Save merged data
    create_output_directory(config.output_dir);
    std::ofstream merged(config.output_dir + "/merged_trajectory.csv");
    merged << "frame,energy\n";
    for (size_t i = 0; i < all_energies.size(); ++i) {
        merged << i << "," << all_energies[i] << "\n";
    }
    
    std::cout << "Merged data saved to: " << config.output_dir << "/merged_trajectory.csv\n";
}
