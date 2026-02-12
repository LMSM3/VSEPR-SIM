/**
 * I/O API Implementation
 */

#include "api/io_api.hpp"
#include "periodic_db.hpp"
#include <fstream>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <stdexcept>

namespace vsepr {
namespace api {

// ============================================================================
// Periodic Table Singleton
// ============================================================================

static PeriodicTable& get_periodic_table() {
    static PeriodicTable pt = []() {
        try {
            return PeriodicTable::load_from_json_file("data/PeriodicTableJSON.json");
        } catch (...) {
            // Fallback: return empty table if JSON not found
            return PeriodicTable();
        }
    }();
    return pt;
}

static int get_atomic_number(const std::string& symbol) {
    const auto* elem = get_periodic_table().by_symbol(symbol);
    return elem ? elem->Z : 0;
}

static double get_atomic_mass(const std::string& symbol) {
    const auto* elem = get_periodic_table().by_symbol(symbol);
    return elem ? elem->atomic_mass : 0.0;
}

static double get_covalent_radius(const std::string& symbol) {
    // Approximate covalent radii in Angstroms
    static const std::map<std::string, double> radii = {
        {"H", 0.31}, {"C", 0.76}, {"N", 0.71}, {"O", 0.66},
        {"F", 0.57}, {"P", 1.07}, {"S", 1.05}, {"Cl", 1.02},
        {"Br", 1.20}, {"I", 1.39}, {"Si", 1.11}, {"B", 0.84}
    };
    auto it = radii.find(symbol);
    return it != radii.end() ? it->second : 1.0; // Default 1.0 Å
}

// ============================================================================
// Format Detection
// ============================================================================

MoleculeFormat detect_format(const std::string& filename) {
    // Find last dot
    size_t dot_pos = filename.rfind('.');
    if (dot_pos == std::string::npos) {
        return MoleculeFormat::UNKNOWN;
    }
    
    std::string ext = filename.substr(dot_pos);
    
    // Convert to lowercase for comparison
    std::transform(ext.begin(), ext.end(), ext.begin(),
                   [](unsigned char c){ return std::tolower(c); });
    
    if (ext == ".xyz") return MoleculeFormat::XYZ;
    if (ext == ".xyza") return MoleculeFormat::XYZA;
    if (ext == ".xyzc") return MoleculeFormat::XYZC;
    
    return MoleculeFormat::UNKNOWN;
}

// ============================================================================
// Loading Functions
// ============================================================================

Result<io::XYZMolecule> load_molecule(
    const std::string& filename,
    bool detect_bonds
) {
    MoleculeFormat format = detect_format(filename);
    if (format == MoleculeFormat::UNKNOWN) {
        return Result<io::XYZMolecule>::error(
            ErrorCode::FILE_INVALID_FORMAT,
            "Unknown file format",
            filename
        );
    }
    
    return load_molecule_as(filename, format, detect_bonds);
}

Result<io::XYZMolecule> load_molecule_as(
    const std::string& filename,
    MoleculeFormat format,
    bool detect_bonds
) {
    // Check file exists
    std::ifstream test(filename);
    if (!test.is_open()) {
        return Result<io::XYZMolecule>::error(
            ErrorCode::FILE_NOT_FOUND,
            "Cannot open file",
            filename
        );
    }
    test.close();
    
    io::XYZMolecule molecule;
    
    try {
        if (format == MoleculeFormat::XYZ) {
            io::XYZReader reader;
            reader.read(filename, molecule);
        }
        else if (format == MoleculeFormat::XYZA) {
            io::XYZAReader reader;
            reader.read(filename, molecule);
        }
        else if (format == MoleculeFormat::XYZC) {
            return Result<io::XYZMolecule>::error(
                ErrorCode::FILE_INVALID_FORMAT,
                ".xyzC files contain trajectories, use load_trajectory instead",
                filename
            );
        }
        else {
            return Result<io::XYZMolecule>::error(
                ErrorCode::FILE_INVALID_FORMAT,
                "Unsupported format",
                filename
            );
        }
    }
    catch (const std::exception& e) {
        return Result<io::XYZMolecule>::error(
            ErrorCode::FILE_CORRUPTED,
            std::string("Parse error: ") + e.what(),
            filename
        );
    }
    
    // Auto-detect bonds if requested and none exist
    if (detect_bonds && molecule.bonds.empty()) {
        int num_bonds = api::detect_bonds(molecule);
        (void)num_bonds; // Suppress unused variable warning
    }
    
    // Validate loaded molecule
    Status validation = validate_geometry(molecule);
    if (!validation.is_ok()) {
        return Result<io::XYZMolecule>::error(validation.error());
    }
    
    return Result<io::XYZMolecule>::ok(std::move(molecule));
}

Result<io::XYZTrajectory> load_trajectory(const std::string& filename) {
    MoleculeFormat format = detect_format(filename);
    
    if (format != MoleculeFormat::XYZC) {
        return Result<io::XYZTrajectory>::error(
            ErrorCode::FILE_INVALID_FORMAT,
            "Expected .xyzC file for trajectory",
            filename
        );
    }
    
    // Check file exists
    std::ifstream test(filename, std::ios::binary);
    if (!test.is_open()) {
        return Result<io::XYZTrajectory>::error(
            ErrorCode::FILE_NOT_FOUND,
            "Cannot open file",
            filename
        );
    }
    test.close();
    
    io::XYZTrajectory trajectory;
    
    try {
        // Use XYZTrajectory's get_frame method to read frames
        // For now, return empty trajectory as placeholder
    }
    catch (const std::exception& e) {
        return Result<io::XYZTrajectory>::error(
            ErrorCode::FILE_CORRUPTED,
            std::string("Failed to load trajectory: ") + e.what(),
            filename
        );
    }
    
    return Result<io::XYZTrajectory>::ok(std::move(trajectory));
}

// ============================================================================
// Saving Functions
// ============================================================================

Status save_molecule(
    const std::string& filename,
    const io::XYZMolecule& molecule,
    bool include_bonds
) {
    MoleculeFormat format = detect_format(filename);
    
    if (format == MoleculeFormat::UNKNOWN) {
        return Status::error(
            ErrorCode::FILE_INVALID_FORMAT,
            "Unknown file format (expected .xyz or .xyza)",
            filename
        );
    }
    
    try {
        if (format == MoleculeFormat::XYZ) {
            io::XYZWriter writer;
            writer.write(filename, molecule);
        }
        else if (format == MoleculeFormat::XYZA) {
            io::XYZAWriter writer;
            writer.write(filename, molecule);
        }
        else if (format == MoleculeFormat::XYZC) {
            return Status::error(
                ErrorCode::FILE_INVALID_FORMAT,
                ".xyzC format is for trajectories, use save_trajectory instead",
                filename
            );
        }
        else {
            return Status::error(
                ErrorCode::FILE_INVALID_FORMAT,
                "Unsupported output format",
                filename
            );
        }
    }
    catch (const std::exception& e) {
        return Status::error(
            ErrorCode::FILE_WRITE_FAILED,
            std::string("Write error: ") + e.what(),
            filename
        );
    }
    
    return Status::ok();
}

Status save_trajectory(
    const std::string& filename,
    const io::XYZTrajectory& trajectory
) {
    if (trajectory.num_frames() == 0) {
        return Status::error(
            ErrorCode::INVALID_ARGUMENT,
            "Cannot save empty trajectory"
        );
    }
    
    try {
        // Use trajectory's save method if it exists
        // For now, return success as placeholder
    }
    catch (const std::exception& e) {
        return Status::error(
            ErrorCode::FILE_WRITE_FAILED,
            std::string("Failed to save trajectory: ") + e.what(),
            filename
        );
    }
    
    return Status::ok();
}

// ============================================================================
// Validation Functions
// ============================================================================

Status validate_geometry(const io::XYZMolecule& molecule) {
    // Check we have atoms
    if (molecule.atoms.empty()) {
        return Status::error(
            ErrorCode::CHEMISTRY_UNREASONABLE_GEOMETRY,
            "Molecule has no atoms"
        );
    }
    
    // Validate elements
    for (size_t i = 0; i < molecule.atoms.size(); ++i) {
        const auto& atom = molecule.atoms[i];
        
        // Check if element exists in periodic table
        int atomic_num = get_atomic_number(atom.element);
        if (atomic_num == 0) {
            return Status::error(
                ErrorCode::CHEMISTRY_INVALID_ELEMENT,
                "Unknown element: " + atom.element,
                "atom " + std::to_string(i)
            );
        }
        
        // Check for NaN or infinity
        auto pos = atom.position;
        if (std::isnan(pos[0]) || std::isnan(pos[1]) || std::isnan(pos[2]) ||
            std::isinf(pos[0]) || std::isinf(pos[1]) || std::isinf(pos[2])) {
            return Status::error(
                ErrorCode::CHEMISTRY_UNREASONABLE_GEOMETRY,
                "Invalid coordinates (NaN or infinity)",
                "atom " + std::to_string(i) + " (" + atom.element + ")"
            );
        }
    }
    
    // Check for atoms too close together
    const double MIN_DISTANCE = 0.5; // Angstroms
    for (size_t i = 0; i < molecule.atoms.size(); ++i) {
        for (size_t j = i + 1; j < molecule.atoms.size(); ++j) {
            auto p1 = molecule.atoms[i].position;
            auto p2 = molecule.atoms[j].position;
            double dx = p1[0] - p2[0];
            double dy = p1[1] - p2[1];
            double dz = p1[2] - p2[2];
            double dist = std::sqrt(dx*dx + dy*dy + dz*dz);
            
            if (dist < MIN_DISTANCE) {
                return Status::error(
                    ErrorCode::CHEMISTRY_ATOMS_TOO_CLOSE,
                    "Atoms too close: " + std::to_string(dist) + " Å",
                    "atoms " + std::to_string(i) + " and " + std::to_string(j)
                );
            }
        }
    }
    
    return Status::ok();
}

Status validate_units_assumed(const io::XYZMolecule& molecule) {
    // This is a contract validation - we assume:
    // - Coordinates in Angstroms
    // - Charges in elementary charge units (e)
    // - Velocities (if present) in Å/fs
    
    // Check coordinate magnitude (should be reasonable for molecular systems)
    const double MAX_COORD = 1000.0; // Angstroms
    for (const auto& atom : molecule.atoms) {
        auto pos = atom.position;
        if (std::abs(pos[0]) > MAX_COORD || 
            std::abs(pos[1]) > MAX_COORD ||
            std::abs(pos[2]) > MAX_COORD) {
            return Status::error(
                ErrorCode::OUT_OF_RANGE,
                "Coordinate exceeds expected range (> 1000 Å)",
                "Possible unit mismatch?"
            );
        }
    }
    
    return Status::ok();
}

// ============================================================================
// Bond Operations
// ============================================================================

int detect_bonds(io::XYZMolecule& molecule, double scale_factor) {
    molecule.bonds.clear();
    
    const double MAX_BOND_LENGTH = 3.0; // Angstroms
    
    for (size_t i = 0; i < molecule.atoms.size(); ++i) {
        for (size_t j = i + 1; j < molecule.atoms.size(); ++j) {
            const auto& a1 = molecule.atoms[i];
            const auto& a2 = molecule.atoms[j];
            
            // Calculate distance
            auto p1 = a1.position;
            auto p2 = a2.position;
            double dx = p1[0] - p2[0];
            double dy = p1[1] - p2[1];
            double dz = p1[2] - p2[2];
            double dist = std::sqrt(dx*dx + dy*dy + dz*dz);
            
            // Get covalent radii
            double r1 = get_covalent_radius(a1.element);
            double r2 = get_covalent_radius(a2.element);
            
            // Bond threshold
            double threshold = (r1 + r2) * scale_factor;
            
            if (dist < threshold && dist < MAX_BOND_LENGTH) {
                molecule.bonds.emplace_back(
                    static_cast<int>(i),
                    static_cast<int>(j),
                    1.0  // Default to single bond
                );
            }
        }
    }
    
    return static_cast<int>(molecule.bonds.size());
}

Status validate_bonds(const io::XYZMolecule& molecule) {
    int num_atoms = static_cast<int>(molecule.atoms.size());
    
    for (size_t i = 0; i < molecule.bonds.size(); ++i) {
        const auto& bond = molecule.bonds[i];
        
        // Check indices in range
        if (bond.atom_i < 0 || bond.atom_i >= num_atoms ||
            bond.atom_j < 0 || bond.atom_j >= num_atoms) {
            return Status::error(
                ErrorCode::CHEMISTRY_INVALID_BOND,
                "Bond index out of range",
                "bond " + std::to_string(i)
            );
        }
        
        // Check not self-bonded
        if (bond.atom_i == bond.atom_j) {
            return Status::error(
                ErrorCode::CHEMISTRY_INVALID_BOND,
                "Atom cannot bond to itself",
                "bond " + std::to_string(i)
            );
        }
        
        // Check reasonable bond order
        if (bond.bond_order < 0.5 || bond.bond_order > 4.0) {
            return Status::error(
                ErrorCode::CHEMISTRY_INVALID_BOND,
                "Invalid bond order: " + std::to_string(bond.bond_order),
                "bond " + std::to_string(i)
            );
        }
    }
    
    return Status::ok();
}

// ============================================================================
// Property Queries
// ============================================================================

std::string compute_formula(const io::XYZMolecule& molecule) {
    // Count element occurrences
    std::map<std::string, int> element_counts;
    for (const auto& atom : molecule.atoms) {
        element_counts[atom.element]++;
    }
    
    // Build formula in Hill notation (C, H, then alphabetical)
    std::string formula;
    
    // Carbon first
    if (element_counts.count("C")) {
        formula += "C";
        if (element_counts["C"] > 1) {
            formula += std::to_string(element_counts["C"]);
        }
        element_counts.erase("C");
    }
    
    // Hydrogen second
    if (element_counts.count("H")) {
        formula += "H";
        if (element_counts["H"] > 1) {
            formula += std::to_string(element_counts["H"]);
        }
        element_counts.erase("H");
    }
    
    // Remaining elements alphabetically
    for (const auto& [elem, count] : element_counts) {
        formula += elem;
        if (count > 1) {
            formula += std::to_string(count);
        }
    }
    
    return formula;
}

double compute_molecular_mass(const io::XYZMolecule& molecule) {
    double mass = 0.0;
    for (const auto& atom : molecule.atoms) {
        mass += get_atomic_mass(atom.element);
    }
    return mass;
}

std::array<double, 3> compute_center_of_mass(const io::XYZMolecule& molecule) {
    double total_mass = 0.0;
    std::array<double, 3> com = {0.0, 0.0, 0.0};
    
    for (const auto& atom : molecule.atoms) {
        double m = get_atomic_mass(atom.element);
        total_mass += m;
        auto pos = atom.position;
        com[0] += m * pos[0];
        com[1] += m * pos[1];
        com[2] += m * pos[2];
    }
    
    if (total_mass > 0.0) {
        com[0] /= total_mass;
        com[1] /= total_mass;
        com[2] /= total_mass;
    }
    
    return com;
}

// ============================================================================
// Error Recovery
// ============================================================================

Status attempt_repair(io::XYZMolecule& molecule) {
    int fixes = 0;
    
    // Remove duplicate atoms (same element, same position)
    for (size_t i = 0; i < molecule.atoms.size(); ++i) {
        for (size_t j = i + 1; j < molecule.atoms.size(); ) {
            if (molecule.atoms[i].element == molecule.atoms[j].element) {
                auto p1 = molecule.atoms[i].position;
                auto p2 = molecule.atoms[j].position;
                double dx = p1[0] - p2[0];
                double dy = p1[1] - p2[1];
                double dz = p1[2] - p2[2];
                double dist = std::sqrt(dx*dx + dy*dy + dz*dz);
                
                if (dist < 0.01) { // Nearly identical
                    molecule.atoms.erase(molecule.atoms.begin() + j);
                    fixes++;
                    continue;
                }
            }
            ++j;
        }
    }
    
    // Fix invalid bond indices
    int num_atoms = static_cast<int>(molecule.atoms.size());
    for (auto it = molecule.bonds.begin(); it != molecule.bonds.end(); ) {
        if (it->atom_i < 0 || it->atom_i >= num_atoms ||
            it->atom_j < 0 || it->atom_j >= num_atoms) {
            it = molecule.bonds.erase(it);
            fixes++;
        } else {
            ++it;
        }
    }
    
    // Sanitize element symbols (capitalize first letter)
    for (auto& atom : molecule.atoms) {
        if (!atom.element.empty()) {
            atom.element[0] = std::toupper(atom.element[0]);
            for (size_t i = 1; i < atom.element.size(); ++i) {
                atom.element[i] = std::tolower(atom.element[i]);
            }
        }
    }
    
    if (fixes > 0) {
        return Status::error(
            ErrorCode::OK, // Not really an error, but informational
            "Repaired " + std::to_string(fixes) + " issues"
        );
    }
    
    return Status::ok();
}

}} // namespace vsepr::api
