/**
 * atomistic-sim: Unified Molecular Simulation & Prediction Tool
 * 
 * Replaces old batch system with integrated workflow:
 * - Multiple simulation modes (optimization, MD, conformers, etc.)
 * - Property prediction from VSEPR topology
 * - Data aggregation and analysis
 * - Reaction energy/barrier prediction
 * 
 * Usage:
 *   atomistic-sim <mode> [options] input.xyz
 * 
 * Modes:
 *   energy      - Single-point energy calculation
 *   optimize    - Geometry optimization (FIRE)
 *   conformers  - Generate & analyze conformer ensemble
 *   md-nve      - Molecular dynamics (constant energy)
 *   md-nvt      - Molecular dynamics (constant temperature)
 *   adaptive    - Adaptive sampling with convergence
 *   predict     - Property prediction from topology
 *   reaction    - Reaction energy/barrier estimation
 *   merge       - Merge multiple simulation outputs
 */

#include "atomistic-sim-config.hpp"
#include "atomistic/core/state.hpp"
#include "atomistic/parsers/xyz_parser.hpp"
#include "atomistic/compilers/xyz_compiler.hpp"
#include "atomistic/models/model.hpp"
#include "atomistic/models/bonded.hpp"
#include "atomistic/integrators/fire.hpp"
#include "atomistic/core/alignment.hpp"
#include "atomistic/core/statistics.hpp"
#include "atomistic/core/thermodynamics.hpp"
#include "atomistic/predict/properties.hpp"
#include "atomistic/report/report_md.hpp"
#include "io/xyz_format.hpp"

#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <vector>
#include <string>
#include <memory>
#include <random>
#include <filesystem>
#include <ctime>

using namespace atomistic;
namespace fs = std::filesystem;

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

void print_header() {
    std::cout << "\n";
    std::cout << "╔══════════════════════════════════════════════════════════════╗\n";
    std::cout << "║         atomistic-sim: Molecular Simulation & Prediction         ║\n";
    std::cout << "║              Integrated VSEPR + Force Field Engine           ║\n";
    std::cout << "╚══════════════════════════════════════════════════════════════╝\n";
    std::cout << "\n";
}

void print_usage() {
    std::cout << "Usage: atomistic-sim <mode> [options] input.xyz\n\n";
    std::cout << "Modes:\n";
    std::cout << "  energy       Single-point energy evaluation\n";
    std::cout << "  optimize     Geometry optimization (FIRE minimizer)\n";
    std::cout << "  conformers   Generate & cluster conformer ensemble\n";
    std::cout << "  md-nve       Molecular dynamics (NVE, constant energy)\n";
    std::cout << "  md-nvt       Molecular dynamics (NVT, constant temperature)\n";
    std::cout << "  adaptive     Adaptive sampling with convergence detection\n";
    std::cout << "  predict      Predict properties from VSEPR topology\n";
    std::cout << "  reaction     Estimate reaction energy & barrier\n";
    std::cout << "  merge        Merge & analyze multiple outputs\n\n";
    std::cout << "Options:\n";
    std::cout << "  --output DIR         Output directory (default: atomistic_output)\n";
    std::cout << "  --cutoff VAL         Nonbonded cutoff in Å (default: 10.0)\n";
    std::cout << "  --temp VAL           Temperature in K (default: 300)\n";
    std::cout << "  --steps N            Number of steps (default: mode-dependent)\n";
    std::cout << "  --no-bonded          Disable bonded interactions\n";
    std::cout << "  --no-nonbonded       Disable nonbonded interactions\n";
    std::cout << "\nExamples:\n";
    std::cout << "  atomistic-sim optimize water.xyz\n";
    std::cout << "  atomistic-sim md-nvt --temp 350 --steps 50000 protein.xyz\n";
    std::cout << "  atomistic-sim conformers --output ethane_confs ethane.xyz\n";
    std::cout << "  atomistic-sim predict molecule.xyz\n";
    std::cout << "  atomistic-sim merge output1/ output2/ output3/\n";
}

SimConfig parse_args(int argc, char** argv) {
    SimConfig config;
    
    if (argc < 2) {
        print_usage();
        exit(1);
    }
    
    config.mode = argv[1];
    
    for (int i = 2; i < argc; ++i) {
        std::string arg = argv[i];
        
        if (arg == "--output" && i+1 < argc) {
            config.output_dir = argv[++i];
        } else if (arg == "--cutoff" && i+1 < argc) {
            config.cutoff = std::stod(argv[++i]);
        } else if (arg == "--temp" && i+1 < argc) {
            config.temperature = std::stod(argv[++i]);
        } else if (arg == "--steps" && i+1 < argc) {
            config.max_steps = std::stoi(argv[++i]);
            config.md_steps = config.max_steps;
        } else if (arg == "--no-bonded") {
            config.use_bonded = false;
        } else if (arg == "--no-nonbonded") {
            config.use_nonbonded = false;
        } else if (arg[0] != '-') {
            if (config.mode == "merge") {
                config.merge_files.push_back(arg);
            } else {
                config.input_file = arg;
            }
        }
    }
    
    return config;
}

void create_output_directory(const std::string& dir) {
    if (!fs::exists(dir)) {
        fs::create_directories(dir);
    }
}

std::string timestamp() {
    auto t = std::time(nullptr);
    auto tm = *std::localtime(&t);
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y%m%d_%H%M%S");
    return oss.str();
}

// ============================================================================
// MODE 1: SINGLE-POINT ENERGY
// ============================================================================

void mode_energy(const SimConfig& config) {
    std::cout << "═══ MODE: Single-Point Energy ═══\n\n";
    
    // Load structure
    vsepr::io::XYZReader reader;
    vsepr::io::XYZMolecule mol;
    if (!reader.read(config.input_file, mol)) {
        std::cerr << "Error: " << reader.get_error() << "\n";
        return;
    }
    
    State s = parsers::from_xyz(mol);
    std::cout << "Loaded: " << s.N << " atoms\n";
    
    // Build models
    ModelParams p{.rc=config.cutoff, .eps=config.epsilon, .sigma=config.sigma};
    
    double E_total = 0.0;
    
    if (config.use_bonded && !s.B.empty()) {
        auto bonded = create_generic_bonded_model(s);
        bonded->eval(s, p);
        std::cout << "Bonded energy:    " << s.E.Ubond + s.E.Uangle + s.E.Utors << " kcal/mol\n";
        E_total += s.E.total();
    }
    
    if (config.use_nonbonded) {
        State s_nb = s;
        auto nonbonded = create_lj_coulomb_model();
        nonbonded->eval(s_nb, p);
        std::cout << "vdW energy:       " << s_nb.E.UvdW << " kcal/mol\n";
        std::cout << "Coulomb energy:   " << s_nb.E.UCoul << " kcal/mol\n";
        E_total += s_nb.E.UvdW + s_nb.E.UCoul;
    }
    
    std::cout << "─────────────────────────────────\n";
    std::cout << "Total energy:     " << E_total << " kcal/mol\n\n";
    
    // Save output
    create_output_directory(config.output_dir);
    std::vector<std::string> elem_names;
    for (const auto& atom : mol.atoms) elem_names.push_back(atom.element);
    
    compilers::save_xyza(config.output_dir + "/energy.xyza", s, elem_names);
    std::cout << "Output saved to: " << config.output_dir << "/energy.xyza\n";
}

// ============================================================================
// MODE 2: GEOMETRY OPTIMIZATION
// ============================================================================

void mode_optimize(const SimConfig& config) {
    std::cout << "═══ MODE: Geometry Optimization ═══\n\n";
    
    vsepr::io::XYZReader reader;
    vsepr::io::XYZMolecule mol;
    if (!reader.read(config.input_file, mol)) {
        std::cerr << "Error: " << reader.get_error() << "\n";
        return;
    }
    
    State s = parsers::from_xyz(mol);
    std::cout << "Initial structure: " << s.N << " atoms\n";
    
    // Build combined model
    ModelParams p{.rc=config.cutoff, .eps=config.epsilon, .sigma=config.sigma};
    
    // Wrapper to evaluate both bonded and nonbonded
    struct CombinedModel : public IModel {
        std::unique_ptr<IModel> bonded;
        std::unique_ptr<IModel> nonbonded;
        bool use_bonded, use_nonbonded;
        
        void eval(State& s, const ModelParams& p) const override {
            std::fill(s.F.begin(), s.F.end(), Vec3{0,0,0});
            s.E = {};
            
            if (use_bonded && bonded) {
                bonded->eval(s, p);
            }
            
            if (use_nonbonded && nonbonded) {
                State s_nb = s;
                nonbonded->eval(s_nb, p);
                for (uint32_t i = 0; i < s.N; ++i) {
                    s.F[i] = s.F[i] + s_nb.F[i];
                }
                s.E.UvdW = s_nb.E.UvdW;
                s.E.UCoul = s_nb.E.UCoul;
            }
        }
    };
    
    auto combined = std::make_unique<CombinedModel>();
    if (config.use_bonded && !s.B.empty()) {
        combined->bonded = create_generic_bonded_model(s);
    }
    if (config.use_nonbonded) {
        combined->nonbonded = create_lj_coulomb_model();
    }
    combined->use_bonded = config.use_bonded;
    combined->use_nonbonded = config.use_nonbonded;
    
    // Initial energy
    combined->eval(s, p);
    double E_initial = s.E.total();
    std::cout << "Initial energy: " << E_initial << " kcal/mol\n\n";
    
    // Optimize
    FIREParams fp;
    fp.max_steps = config.max_steps;
    fp.epsF = config.force_tol;
    
    FIRE fire(*combined, p);
    auto stats = fire.minimize(s, fp);
    
    std::cout << "Optimization complete:\n";
    std::cout << "  Steps:        " << stats.step << "\n";
    std::cout << "  Final energy: " << stats.U << " kcal/mol\n";
    std::cout << "  ΔE:           " << stats.U - E_initial << " kcal/mol\n";
    std::cout << "  RMS force:    " << stats.Frms << "\n\n";
    
    // Save results
    create_output_directory(config.output_dir);
    std::vector<std::string> elem_names;
    for (const auto& atom : mol.atoms) elem_names.push_back(atom.element);

    compilers::save_xyza(config.output_dir + "/optimized.xyza", s, elem_names);

    std::string report_str = atomistic::fire_report_md(s, stats);
    std::ofstream rf(config.output_dir + "/optimization_report.md");
    rf << report_str;

    std::cout << "Output saved to: " << config.output_dir << "/\n";
}

// ============================================================================
// MODE 3: CONFORMER ENSEMBLE
// ============================================================================

void mode_conformers(const SimConfig& config) {
    std::cout << "═══ MODE: Conformer Ensemble ═══\n\n";
    
    vsepr::io::XYZReader reader;
    vsepr::io::XYZMolecule mol;
    if (!reader.read(config.input_file, mol)) {
        std::cerr << "Error: " << reader.get_error() << "\n";
        return;
    }
    
    State initial = parsers::from_xyz(mol);
    std::cout << "Generating " << config.n_conformers << " conformers...\n\n";
    
    // Build model
    ModelParams p{.rc=config.cutoff, .eps=config.epsilon, .sigma=config.sigma};
    auto model = create_generic_bonded_model(initial);
    
    std::vector<State> conformers;
    std::vector<double> energies;
    std::mt19937 rng(12345);
    std::normal_distribution<double> noise(0.0, 0.3);
    
    for (int i = 0; i < config.n_conformers; ++i) {
        State s = initial;
        
        // Perturb torsions randomly
        for (auto& x : s.X) {
            x.x += noise(rng);
            x.y += noise(rng);
            x.z += noise(rng);
        }
        
        // Optimize
        FIREParams fp;
        fp.max_steps = 500;
        fp.epsF = 0.05;
        FIRE fire(*model, p);
        fire.minimize(s, fp);
        
        conformers.push_back(s);
        energies.push_back(s.E.total());
        
        if ((i+1) % 10 == 0) {
            std::cout << "  Generated " << (i+1) << "/" << config.n_conformers << "\r" << std::flush;
        }
    }
    std::cout << "\n\n";
    
    // Cluster by RMSD
    std::vector<int> cluster_id(conformers.size(), -1);
    int n_clusters = 0;
    
    for (size_t i = 0; i < conformers.size(); ++i) {
        if (cluster_id[i] >= 0) continue;
        
        cluster_id[i] = n_clusters;
        
        for (size_t j = i+1; j < conformers.size(); ++j) {
            if (cluster_id[j] >= 0) continue;
            
            State s_j = conformers[j];
            kabsch_align(s_j, conformers[i]);
            double rmsd = compute_rmsd(s_j, conformers[i]);
            
            if (rmsd < config.rmsd_threshold) {
                cluster_id[j] = n_clusters;
            }
        }
        
        n_clusters++;
    }
    
    std::cout << "Found " << n_clusters << " unique conformers (RMSD > " 
              << config.rmsd_threshold << " Å)\n\n";
    
    // Save cluster representatives
    create_output_directory(config.output_dir);
    std::vector<std::string> elem_names;
    for (const auto& atom : mol.atoms) elem_names.push_back(atom.element);
    
    for (int c = 0; c < n_clusters; ++c) {
        // Find lowest energy in cluster
        int best_idx = -1;
        double best_E = 1e100;
        for (size_t i = 0; i < conformers.size(); ++i) {
            if (cluster_id[i] == c && energies[i] < best_E) {
                best_E = energies[i];
                best_idx = i;
            }
        }
        
        std::string filename = config.output_dir + "/conformer_" + std::to_string(c+1) + ".xyza";
        compilers::save_xyza(filename, conformers[best_idx], elem_names);
    }
    
    std::cout << "Saved " << n_clusters << " conformers to: " << config.output_dir << "/\n";
}

// Forward declarations for remaining modes
void mode_md_nve(const SimConfig& config);
void mode_md_nvt(const SimConfig& config);
void mode_adaptive(const SimConfig& config);
void mode_predict(const SimConfig& config);
void mode_reaction(const SimConfig& config);
void mode_merge(const SimConfig& config);

// ============================================================================
// MAIN
// ============================================================================

int main(int argc, char** argv) {
    print_header();
    
    SimConfig config = parse_args(argc, argv);
    
    try {
        if (config.mode == "energy") {
            mode_energy(config);
        } else if (config.mode == "optimize") {
            mode_optimize(config);
        } else if (config.mode == "conformers") {
            mode_conformers(config);
        } else if (config.mode == "md-nve") {
            mode_md_nve(config);
        } else if (config.mode == "md-nvt") {
            mode_md_nvt(config);
        } else if (config.mode == "adaptive") {
            mode_adaptive(config);
        } else if (config.mode == "predict") {
            mode_predict(config);
        } else if (config.mode == "reaction") {
            mode_reaction(config);
        } else if (config.mode == "merge") {
            mode_merge(config);
        } else {
            std::cerr << "Error: Unknown mode '" << config.mode << "'\n";
            print_usage();
            return 1;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
    
    std::cout << "\n✓ Simulation complete!\n\n";
    return 0;
}
