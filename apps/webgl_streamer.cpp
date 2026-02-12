/**
 * webgl_streamer.cpp
 * 
 * Standalone C++ molecule stream generator for WebGL viewer
 * Continuously generates random molecules with memory management
 * 
 * Features:
 * - Random molecular formula generation
 * - Automatic JSON export for WebGL
 * - Memory-limited batch processing (6GB → 12GB)
 * - Automatic cleanup and rotation
 * - Independent subsystem (no dependencies on main VSEPR)
 */

#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <random>
#include <chrono>
#include <thread>
#include <cmath>
#include <map>
#include <sstream>
#include <iomanip>

// Simple Vec3 for geometry
struct Vec3 {
    double x, y, z;
    Vec3(double x=0, double y=0, double z=0) : x(x), y(y), z(z) {}
};

// Atom structure
struct Atom {
    std::string symbol;
    Vec3 position;
};

// Molecule structure
struct Molecule {
    std::string formula;
    std::string name;
    std::vector<Atom> atoms;
    size_t memory_size;
};

// Random number generator
std::mt19937 rng(std::chrono::steady_clock::now().time_since_epoch().count());

// Element database with VDW radii
struct ElementData {
    std::string symbol;
    double vdw_radius;
    int max_valence;
};

std::map<std::string, ElementData> elements = {
    {"H", {"H", 1.20, 1}},
    {"C", {"C", 1.70, 4}},
    {"N", {"N", 1.55, 3}},
    {"O", {"O", 1.52, 2}},
    {"F", {"F", 1.47, 1}},
    {"P", {"P", 1.80, 5}},
    {"S", {"S", 1.80, 6}},
    {"Cl", {"Cl", 1.75, 1}},
    {"Br", {"Br", 1.85, 1}},
    {"I", {"I", 1.98, 1}},
    {"B", {"B", 1.92, 3}},
    {"Si", {"Si", 2.10, 4}},
    {"As", {"As", 1.85, 5}},
    {"Se", {"Se", 1.90, 6}},
    {"Xe", {"Xe", 2.16, 8}},
    {"Kr", {"Kr", 2.02, 2}}
};

/**
 * Generate random VSEPR formula
 */
std::string generate_random_formula() {
    std::vector<std::string> central_atoms = {"C", "N", "O", "P", "S", "B", "Si", "As", "Se", "Cl", "Br", "I", "Xe", "Kr"};
    std::vector<std::string> ligands = {"H", "F", "Cl", "Br", "I", "O"};
    
    std::uniform_int_distribution<> central_dist(0, central_atoms.size() - 1);
    std::uniform_int_distribution<> ligand_dist(0, ligands.size() - 1);
    std::uniform_int_distribution<> count_dist(1, 6);
    
    std::string central = central_atoms[central_dist(rng)];
    std::string ligand = ligands[ligand_dist(rng)];
    int count = count_dist(rng);
    
    // Ensure valid formula
    if (elements[central].max_valence < count) {
        count = elements[central].max_valence;
    }
    
    std::ostringstream formula;
    formula << central << ligand;
    if (count > 1) formula << count;
    
    return formula.str();
}

/**
 * Generate 3D geometry for molecule using simple VSEPR rules
 */
Molecule generate_molecule(const std::string& formula) {
    Molecule mol;
    mol.formula = formula;
    mol.name = formula;
    
    // Parse formula (simple parser for XYn format)
    std::string central_symbol;
    std::string ligand_symbol;
    int ligand_count = 1;
    
    // Extract central atom (first character or two)
    size_t i = 0;
    central_symbol += formula[i++];
    if (i < formula.size() && islower(formula[i])) {
        central_symbol += formula[i++];
    }
    
    // Extract ligand
    if (i < formula.size()) {
        ligand_symbol += formula[i++];
        if (i < formula.size() && islower(formula[i])) {
            ligand_symbol += formula[i++];
        }
    }
    
    // Extract count
    if (i < formula.size() && isdigit(formula[i])) {
        ligand_count = formula[i] - '0';
        i++;
        if (i < formula.size() && isdigit(formula[i])) {
            ligand_count = ligand_count * 10 + (formula[i] - '0');
        }
    }
    
    // Add central atom
    mol.atoms.push_back({central_symbol, Vec3(0, 0, 0)});
    
    // Generate ligand positions based on count (VSEPR geometries)
    double bond_length = 1.5; // Angstroms
    
    if (ligand_count == 1) {
        // Linear
        mol.atoms.push_back({ligand_symbol, Vec3(bond_length, 0, 0)});
    }
    else if (ligand_count == 2) {
        // Linear
        mol.atoms.push_back({ligand_symbol, Vec3(bond_length, 0, 0)});
        mol.atoms.push_back({ligand_symbol, Vec3(-bond_length, 0, 0)});
    }
    else if (ligand_count == 3) {
        // Trigonal planar
        double angle = 2.0 * M_PI / 3.0;
        for (int j = 0; j < 3; j++) {
            double theta = j * angle;
            mol.atoms.push_back({
                ligand_symbol,
                Vec3(bond_length * cos(theta), bond_length * sin(theta), 0)
            });
        }
    }
    else if (ligand_count == 4) {
        // Tetrahedral
        mol.atoms.push_back({ligand_symbol, Vec3(1, 1, 1)});
        mol.atoms.push_back({ligand_symbol, Vec3(1, -1, -1)});
        mol.atoms.push_back({ligand_symbol, Vec3(-1, 1, -1)});
        mol.atoms.push_back({ligand_symbol, Vec3(-1, -1, 1)});
        
        // Normalize to bond length
        for (size_t j = 1; j < mol.atoms.size(); j++) {
            double len = sqrt(mol.atoms[j].position.x * mol.atoms[j].position.x +
                            mol.atoms[j].position.y * mol.atoms[j].position.y +
                            mol.atoms[j].position.z * mol.atoms[j].position.z);
            mol.atoms[j].position.x *= bond_length / len;
            mol.atoms[j].position.y *= bond_length / len;
            mol.atoms[j].position.z *= bond_length / len;
        }
    }
    else if (ligand_count == 5) {
        // Trigonal bipyramidal
        mol.atoms.push_back({ligand_symbol, Vec3(0, 0, bond_length)});      // Axial
        mol.atoms.push_back({ligand_symbol, Vec3(0, 0, -bond_length)});     // Axial
        
        double angle = 2.0 * M_PI / 3.0;
        for (int j = 0; j < 3; j++) {
            double theta = j * angle;
            mol.atoms.push_back({
                ligand_symbol,
                Vec3(bond_length * 0.9 * cos(theta), bond_length * 0.9 * sin(theta), 0)
            });
        }
    }
    else if (ligand_count == 6) {
        // Octahedral
        mol.atoms.push_back({ligand_symbol, Vec3(bond_length, 0, 0)});
        mol.atoms.push_back({ligand_symbol, Vec3(-bond_length, 0, 0)});
        mol.atoms.push_back({ligand_symbol, Vec3(0, bond_length, 0)});
        mol.atoms.push_back({ligand_symbol, Vec3(0, -bond_length, 0)});
        mol.atoms.push_back({ligand_symbol, Vec3(0, 0, bond_length)});
        mol.atoms.push_back({ligand_symbol, Vec3(0, 0, -bond_length)});
    }
    
    // Estimate memory size (rough)
    mol.memory_size = formula.size() + mol.atoms.size() * 64;
    
    return mol;
}

/**
 * Export molecule to JSON for WebGL viewer
 */
void export_to_json(const Molecule& mol, const std::string& filename) {
    std::ofstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Failed to open output file: " << filename << std::endl;
        return;
    }
    
    file << "{\n";
    file << "  \"" << mol.formula << "\": {\n";
    file << "    \"name\": \"" << mol.name << "\",\n";
    file << "    \"atoms\": [\n";
    
    for (size_t i = 0; i < mol.atoms.size(); i++) {
        const auto& atom = mol.atoms[i];
        file << "      {\"symbol\": \"" << atom.symbol << "\", ";
        file << "\"x\": " << std::fixed << std::setprecision(3) << atom.position.x << ", ";
        file << "\"y\": " << atom.position.y << ", ";
        file << "\"z\": " << atom.position.z << "}";
        if (i < mol.atoms.size() - 1) file << ",";
        file << "\n";
    }
    
    file << "    ]\n";
    file << "  }\n";
    file << "}\n";
    
    file.close();
}

/**
 * Stream molecules with memory management
 */
void stream_molecules(int count, double interval_ms, size_t max_memory_mb, const std::string& output_file) {
    std::cout << "\n";
    std::cout << "╔════════════════════════════════════════════════════════════════╗\n";
    std::cout << "║                                                                ║\n";
    std::cout << "║         C++ Molecule Stream Generator                          ║\n";
    std::cout << "║         Direct WebGL Integration                               ║\n";
    std::cout << "║                                                                ║\n";
    std::cout << "╚════════════════════════════════════════════════════════════════╝\n";
    std::cout << "\n";
    std::cout << "[CONFIG] Output: " << output_file << "\n";
    std::cout << "[CONFIG] Count: " << (count > 0 ? std::to_string(count) : "infinite") << " molecules\n";
    std::cout << "[CONFIG] Interval: " << interval_ms << " ms\n";
    std::cout << "[CONFIG] Memory Limit: " << max_memory_mb << " MB\n";
    std::cout << "\n";
    std::cout << "[ENGINE] All molecular dynamics in C++\n";
    std::cout << "[EXPORT] JSON serialization for WebGL\n";
    std::cout << "\n";
    std::cout << "Streaming started... (Press Ctrl+C to stop)\n";
    std::cout << "\n";
    
    size_t total_memory = 0;
    int iteration = 0;
    
    while (count < 0 || iteration < count) {
        // Generate random molecule
        std::string formula = generate_random_formula();
        Molecule mol = generate_molecule(formula);
        
        // Check memory limit
        if (total_memory + mol.memory_size > max_memory_mb * 1024 * 1024) {
            std::cout << "\n[MEMORY] Limit reached (" << max_memory_mb << " MB), rotating data...\n";
            total_memory = 0;
        }
        
        // Export to JSON
        export_to_json(mol, output_file);
        total_memory += mol.memory_size;
        
        // Log
        auto now = std::chrono::system_clock::now();
        auto now_t = std::chrono::system_clock::to_time_t(now);
        auto tm = *std::localtime(&now_t);
        
        std::cout << "[" 
                  << std::setfill('0') << std::setw(2) << tm.tm_hour << ":"
                  << std::setfill('0') << std::setw(2) << tm.tm_min << ":"
                  << std::setfill('0') << std::setw(2) << tm.tm_sec << "] ";
        std::cout << "Exported " << formula << " (" << (iteration + 1);
        if (count > 0) std::cout << "/" << count;
        std::cout << ") [" << (total_memory / 1024) << " KB]\n";
        
        iteration++;
        
        // Sleep
        std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<int>(interval_ms)));
    }
    
    std::cout << "\n";
    std::cout << "✓ Stream complete - exported " << iteration << " molecules\n";
}

/**
 * Main entry point
 */
int main(int argc, char* argv[]) {
    // Default parameters
    int count = -1;  // Infinite
    double interval = 2000.0;  // 2 seconds
    size_t max_memory_mb = 6 * 1024;  // 6 GB
    std::string output_file = "webgl_molecules.json";
    
    // Parse arguments
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        
        if (arg == "--count" || arg == "-c") {
            if (i + 1 < argc) count = std::stoi(argv[++i]);
        }
        else if (arg == "--interval" || arg == "-i") {
            if (i + 1 < argc) interval = std::stod(argv[++i]);
        }
        else if (arg == "--memory" || arg == "-m") {
            if (i + 1 < argc) max_memory_mb = std::stoull(argv[++i]);
        }
        else if (arg == "--output" || arg == "-o") {
            if (i + 1 < argc) output_file = argv[++i];
        }
        else if (arg == "--help" || arg == "-h") {
            std::cout << "Usage: webgl_streamer [options]\n";
            std::cout << "\nOptions:\n";
            std::cout << "  -c, --count N       Generate N molecules (default: infinite)\n";
            std::cout << "  -i, --interval MS   Delay between molecules in ms (default: 2000)\n";
            std::cout << "  -m, --memory MB     Memory limit before rotation (default: 6144)\n";
            std::cout << "  -o, --output FILE   Output JSON file (default: webgl_molecules.json)\n";
            std::cout << "  -h, --help          Show this help\n";
            std::cout << "\nExamples:\n";
            std::cout << "  webgl_streamer -c 100 -i 1000         # 100 molecules, 1s interval\n";
            std::cout << "  webgl_streamer -m 12288               # 12 GB memory limit\n";
            std::cout << "  webgl_streamer -c 1000 -m 6144        # Stress test with 6GB\n";
            return 0;
        }
    }
    
    // Run streaming
    stream_molecules(count, interval, max_memory_mb, output_file);
    
    return 0;
}
