#pragma once
#include <string>
#include <vector>

// Shared configuration structure for all simulation modes
struct SimConfig {
    std::string mode;
    std::string input_file;
    std::string output_dir = "meso_output";
    
    // Model parameters
    bool use_bonded = true;
    bool use_nonbonded = true;
    double cutoff = 10.0;
    double epsilon = 0.086;
    double sigma = 3.4;
    
    // Optimization
    int max_steps = 1000;
    double force_tol = 0.01;
    
    // MD parameters
    double temperature = 300.0;  // K
    double timestep = 1.0;       // fs
    int md_steps = 10000;
    int save_interval = 100;
    
    // Conformer search
    int n_conformers = 100;
    double rmsd_threshold = 0.5;  // Å
    
    // Adaptive sampling
    double convergence_tol = 1e-4;
    int convergence_window = 50;
    int max_samples = 1000;
    
    // Merge mode
    std::vector<std::string> merge_files;
};

// Utility functions
void create_output_directory(const std::string& dir);
std::string timestamp();
