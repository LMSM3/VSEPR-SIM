// compute_forces.cpp - CLI tool to compute and save force fields
// Usage: compute_forces --input foo.xyz --model LJ+Coulomb --output foo.xyzF
// NOTE: Stub implementation - full Forces class needs implementation

#include <iostream>
#include <string>
#include <cstdlib>
#include <fstream>

void print_usage() {
    std::cout << "Usage: compute_forces [options]\n\n";
    std::cout << "Options:\n";
    std::cout << "  --input FILE       Input geometry (.xyz, .xyzA, .xyzC)\n";
    std::cout << "  --model MODEL      Force model: LJ, LJ+Coulomb (default), Bonded\n";
    std::cout << "  --cutoff DIST      Cutoff distance in Angstroms (default: 12.0)\n";
    std::cout << "  --output FILE      Output force field (.xyzF)\n";
    std::cout << "  --units UNITS      Force units: kcal_mol_A (default), eV_A, pN\n";
    std::cout << "  --verbose          Print statistics\n";
    std::cout << "\nExample:\n";
    std::cout << "  compute_forces --input nacl.xyz --model LJ+Coulomb --output nacl.xyzF\n";
    std::cout << "\nNOTE: This is a stub - full implementation requires Forces class\n";
}

struct Options {
    std::string input_file;
    std::string output_file;
    std::string model = "LJ+Coulomb";
    std::string units = "kcal_mol_A";
    float cutoff = 12.0f;
    bool verbose = false;
};

Options parse_args(int argc, char** argv) {
    Options opts;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "--help" || arg == "-h") {
            print_usage();
            std::exit(0);
        } else if (arg == "--input" && i+1 < argc) {
            opts.input_file = argv[++i];
        } else if (arg == "--output" && i+1 < argc) {
            opts.output_file = argv[++i];
        } else if (arg == "--model" && i+1 < argc) {
            opts.model = argv[++i];
        } else if (arg == "--cutoff" && i+1 < argc) {
            opts.cutoff = std::stof(argv[++i]);
        } else if (arg == "--units" && i+1 < argc) {
            opts.units = argv[++i];
        } else if (arg == "--verbose") {
            opts.verbose = true;
        } else {
            std::cerr << "Unknown option: " << arg << "\n";
            print_usage();
            std::exit(1);
        }
    }

    if (opts.input_file.empty()) {
        std::cerr << "Error: --input required\n";
        print_usage();
        std::exit(1);
    }

    if (opts.output_file.empty()) {
        // Auto-generate output filename
        opts.output_file = opts.input_file;
        size_t pos = opts.output_file.rfind('.');
        if (pos != std::string::npos) {
            opts.output_file = opts.output_file.substr(0, pos);
        }
        opts.output_file += ".xyzF";
    }

    return opts;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        print_usage();
        return 1;
    }

    Options opts = parse_args(argc, argv);

    try {
        std::cout << "Loading geometry: " << opts.input_file << "\n";

        // TODO: Load geometry using Crystal class
        std::cout << "  [STUB] Would load from " << opts.input_file << "\n";

        std::cout << "\nComputing forces:\n";
        std::cout << "  Model: " << opts.model << "\n";
        std::cout << "  Cutoff: " << opts.cutoff << " Å\n";
        std::cout << "  Units: " << opts.units << "\n";

        // TODO: Compute forces using ForceComputer
        std::cout << "  [STUB] Would compute using " << opts.model << "\n";

        // Write stub xyzF file
        std::cout << "\nSaving force field: " << opts.output_file << "\n";

        std::ofstream out(opts.output_file);
        out << "2\n";
        out << "# xyzF v1  source=\"" << opts.input_file << "\"  units=\"" << opts.units << "\"  model=\"" << opts.model << "\"\n";
        out << "# [STUB] Full implementation requires Forces class\n";
        out << "# computation:\n";
        out << "#   method: \"stub\"\n";
        out << "#   cutoff: " << opts.cutoff << "\n";
        out << "# statistics:\n";
        out << "#   max_force: 0.0\n";
        out << "#   mean_force: 0.0\n";
        out << "# forces:\n";
        out << "#   - atom: \"a1\"\n";
        out << "#     net: [0.0, 0.0, 0.0]\n";
        out << "#     magnitude: 0.0\n";
        out << "Na  0.0  0.0  0.0\n";
        out << "Cl  2.8  0.0  0.0\n";
        out.close();

        std::cout << "✓ Stub file created\n";
        std::cout << "\nNOTE: This is a placeholder. Implement:\n";
        std::cout << "  1. src/data/crystal.cpp (Crystal class)\n";
        std::cout << "  2. src/data/forces.cpp (Forces + ForceComputer)\n";
        std::cout << "  3. src/io/xyzF_format.cpp (xyzF parser/writer)\n";

        return 0;

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}
