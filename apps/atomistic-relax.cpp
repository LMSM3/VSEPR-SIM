#include "atomistic/core/state.hpp"
#include "atomistic/parsers/xyz_parser.hpp"
#include "atomistic/compilers/xyz_compiler.hpp"
#include "atomistic/models/model.hpp"
#include "atomistic/integrators/fire.hpp"
#include "atomistic/report/report_md.hpp"
#include "atomistic/core/statistics.hpp"
#include "io/xyz_format.hpp"

#include <iostream>
#include <fstream>
#include <memory>

using namespace atomistic;

void print_usage(const char* prog) {
    std::cout << "Usage: " << prog << " <input.xyz> [options]\n\n";
    std::cout << "Options:\n";
    std::cout << "  --model <name>       Force model (lj, lj_coulomb) [default: lj_coulomb]\n";
    std::cout << "  --epsilon <val>      LJ epsilon [default: 0.1]\n";
    std::cout << "  --sigma <val>        LJ sigma [default: 3.0]\n";
    std::cout << "  --max-iter <n>       Max FIRE iterations [default: 1000]\n";
    std::cout << "  --force-tol <f>      Force convergence [default: 0.01]\n";
    std::cout << "  --output <file>      Output XYZA file [default: relaxed.xyza]\n";
    std::cout << "  --report <file>      Markdown report [default: report.md]\n";
    std::cout << "  --help               Show this help\n";
}

int main(int argc, char** argv) {
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }
    
    std::string input_file = argv[1];
    std::string model_name = "lj_coulomb";
    std::string output_file = "relaxed.xyza";
    std::string report_file = "report.md";
    
    ModelParams mp;
    mp.eps = 0.1;
    mp.sigma = 3.0;
    mp.k_coul = 332.0; // kcal/mol·Å·e²
    mp.rc = 10.0;
    
    FIREParams fp;
    fp.max_steps = 1000;
    fp.epsF = 0.01;
    fp.dt_max = 0.1;
    fp.dt = 0.01;
    
    // Parse arguments
    for (int i = 2; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--help") {
            print_usage(argv[0]);
            return 0;
        } else if (arg == "--model" && i+1 < argc) {
            model_name = argv[++i];
        } else if (arg == "--epsilon" && i+1 < argc) {
            mp.eps = std::stod(argv[++i]);
        } else if (arg == "--sigma" && i+1 < argc) {
            mp.sigma = std::stod(argv[++i]);
        } else if (arg == "--max-iter" && i+1 < argc) {
            fp.max_steps = std::stoi(argv[++i]);
        } else if (arg == "--force-tol" && i+1 < argc) {
            fp.epsF = std::stod(argv[++i]);
        } else if (arg == "--output" && i+1 < argc) {
            output_file = argv[++i];
        } else if (arg == "--report" && i+1 < argc) {
            report_file = argv[++i];
        }
    }
    
    // Load XYZ file
    std::cout << "Loading " << input_file << "...\n";
    vsepr::io::XYZReader reader;
    vsepr::io::XYZMolecule mol;
    if (!reader.read(input_file, mol)) {
        std::cerr << "Error: " << reader.get_error() << "\n";
        return 1;
    }
    
    // Convert to CoreState
    State state = parsers::from_xyz(mol);
    std::cout << "Converted to CoreState: N=" << state.N << " atoms\n";
    
    if (!sane(state)) {
        std::cerr << "Error: Invalid state after conversion\n";
        return 1;
    }
    
    // Select model
    std::unique_ptr<IModel> model;
    if (model_name == "lj" || model_name == "lj_coulomb") {
        model = create_lj_coulomb_model();
        std::cout << "Using LJ+Coulomb model (ε=" << mp.eps 
                  << ", σ=" << mp.sigma << ")\n";
    } else {
        std::cerr << "Unknown model: " << model_name << "\n";
        return 1;
    }
    
    // Initial energy
    model->eval(state, mp);
    std::cout << "Initial energy: " << state.E.total() << " kcal/mol\n";
    std::cout << "  Bond: " << state.E.Ubond << "\n";
    std::cout << "  vdW: " << state.E.UvdW << "\n";
    std::cout << "  Coulomb: " << state.E.UCoul << "\n\n";
    
    // Run FIRE minimization
    std::cout << "Running FIRE minimization (max " << fp.max_steps << " steps)...\n";
    FIRE fire(*model, mp);
    auto stats = fire.minimize(state, fp);
    
    std::cout << "\nOptimization complete!\n";
    std::cout << "  Steps: " << stats.step << "\n";
    std::cout << "  Final energy: " << stats.U << " kcal/mol\n";
    std::cout << "  Final force RMS: " << stats.Frms << "\n";
    std::cout << "  Converged: " << (stats.Frms < fp.epsF ? "yes" : "no") << "\n";
    
    // Generate report
    std::string report_md = fire_report_md(state, stats);
    std::ofstream rf(report_file);
    if (rf.is_open()) {
        rf << report_md;
        std::cout << "Report written to " << report_file << "\n";
    }
    
    // Build element names from original molecule
    std::vector<std::string> elem_names;
    for (const auto& atom : mol.atoms) {
        elem_names.push_back(atom.element);
    }
    
    // Save relaxed structure
    if (compilers::save_xyza(output_file, state, elem_names)) {
        std::cout << "Relaxed structure written to " << output_file << "\n";
    } else {
        std::cerr << "Error writing output file\n";
        return 1;
    }
    
    return 0;
}
