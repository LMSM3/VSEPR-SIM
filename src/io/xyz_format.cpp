/**
 * XYZ File Format Implementation
 */

#include "io/xyz_format.hpp"
#include <cmath>
#include <algorithm>
#include <set>

namespace vsepr {
namespace io {

// ============================================================================
// XYZ Molecule Methods
// ============================================================================

void XYZMolecule::compute_bounds() {
    if (atoms.empty()) {
        box_min = {0.0, 0.0, 0.0};
        box_max = {0.0, 0.0, 0.0};
        return;
    }
    
    box_min = atoms[0].position;
    box_max = atoms[0].position;
    
    for (const auto& atom : atoms) {
        for (int i = 0; i < 3; i++) {
            box_min[i] = std::min(box_min[i], atom.position[i]);
            box_max[i] = std::max(box_max[i], atom.position[i]);
        }
    }
}

std::array<double, 3> XYZMolecule::get_center() const {
    if (atoms.empty()) return {0.0, 0.0, 0.0};
    
    std::array<double, 3> center = {0.0, 0.0, 0.0};
    for (const auto& atom : atoms) {
        for (int i = 0; i < 3; i++) {
            center[i] += atom.position[i];
        }
    }
    
    for (int i = 0; i < 3; i++) {
        center[i] /= atoms.size();
    }
    
    return center;
}

void XYZMolecule::translate(double dx, double dy, double dz) {
    for (auto& atom : atoms) {
        atom.position[0] += dx;
        atom.position[1] += dy;
        atom.position[2] += dz;
    }
    compute_bounds();
}

void XYZMolecule::translate(const glm::vec3& delta) {
    translate(delta.x, delta.y, delta.z);
}

void XYZMolecule::rotate(const std::array<double, 3>& axis, double angle) {
    glm::vec3 glm_axis(axis[0], axis[1], axis[2]);
    rotate(glm_axis, angle);
}

void XYZMolecule::rotate(const glm::vec3& axis, double angle) {
    glm::mat4 rotation = glm::rotate(glm::mat4(1.0f), static_cast<float>(angle), 
                                     glm::vec3(axis.x, axis.y, axis.z));
    transform(rotation);
}

void XYZMolecule::rotate(const glm::mat4& rotation_matrix) {
    transform(rotation_matrix);
}

void XYZMolecule::scale(double factor) {
    for (auto& atom : atoms) {
        atom.position[0] *= factor;
        atom.position[1] *= factor;
        atom.position[2] *= factor;
    }
    compute_bounds();
}

void XYZMolecule::transform(const glm::mat4& matrix) {
    for (auto& atom : atoms) {
        glm::vec4 pos(atom.position[0], atom.position[1], atom.position[2], 1.0);
        glm::vec4 transformed = matrix * pos;
        
        atom.position[0] = transformed.x;
        atom.position[1] = transformed.y;
        atom.position[2] = transformed.z;
    }
    compute_bounds();
}

// ============================================================================
// XYZReader Implementation
// ============================================================================

bool XYZReader::read(const std::string& filename, XYZMolecule& mol) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        error_message_ = "Cannot open file: " + filename;
        return false;
    }
    return read_stream(file, mol);
}

bool XYZReader::read_stream(std::istream& input, XYZMolecule& mol) {
    mol = XYZMolecule();  // Reset
    
    std::string line;
    
    // Line 1: Number of atoms
    if (!std::getline(input, line)) {
        error_message_ = "Failed to read atom count";
        return false;
    }
    
    int num_atoms = std::stoi(line);
    if (num_atoms <= 0) {
        error_message_ = "Invalid atom count: " + std::to_string(num_atoms);
        return false;
    }
    
    // Line 2: Comment
    if (!std::getline(input, line)) {
        error_message_ = "Failed to read comment line";
        return false;
    }
    mol.comment = line;
    
    // Lines 3+: Atom data
    mol.atoms.reserve(num_atoms);
    
    for (int i = 0; i < num_atoms; i++) {
        if (!std::getline(input, line)) {
            error_message_ = "Unexpected end of file at atom " + std::to_string(i);
            return false;
        }
        
        std::istringstream iss(line);
        std::string element;
        double x, y, z;
        
        if (!(iss >> element >> x >> y >> z)) {
            error_message_ = "Invalid atom data at line " + std::to_string(i + 3);
            return false;
        }
        
        mol.atoms.emplace_back(element, x, y, z);
    }
    
    mol.compute_bounds();
    return true;
}

bool XYZReader::read_string(const std::string& xyz_string, XYZMolecule& mol) {
    std::istringstream stream(xyz_string);
    return read_stream(stream, mol);
}

void XYZReader::detect_bonds(XYZMolecule& mol, double scale_factor) const {
    mol.bonds.clear();
    
    for (size_t i = 0; i < mol.atoms.size(); i++) {
        for (size_t j = i + 1; j < mol.atoms.size(); j++) {
            const auto& atom_i = mol.atoms[i];
            const auto& atom_j = mol.atoms[j];
            
            // Compute distance
            double dx = atom_i.position[0] - atom_j.position[0];
            double dy = atom_i.position[1] - atom_j.position[1];
            double dz = atom_i.position[2] - atom_j.position[2];
            double dist = std::sqrt(dx*dx + dy*dy + dz*dz);
            
            // Get covalent radii
            double r_i = get_covalent_radius(atom_i.element);
            double r_j = get_covalent_radius(atom_j.element);
            double bond_threshold = (r_i + r_j) * scale_factor;
            
            if (dist < bond_threshold && dist > 0.4) {  // Min 0.4 Å
                mol.bonds.emplace_back(i, j, 1.0);
            }
        }
    }
}

double XYZReader::get_covalent_radius(const std::string& element) const {
    return xyz_utils::get_covalent_radius(element);
}

// ============================================================================
// XYZWriter Implementation
// ============================================================================

bool XYZWriter::write(const std::string& filename, const XYZMolecule& mol) {
    std::ofstream file(filename);
    if (!file.is_open()) {
        error_message_ = "Cannot write to file: " + filename;
        return false;
    }
    return write_stream(file, mol);
}

bool XYZWriter::write_stream(std::ostream& output, const XYZMolecule& mol) {
    // Line 1: Number of atoms
    output << mol.atoms.size() << "\n";
    
    // Line 2: Comment
    if (mol.comment.empty()) {
        output << "Generated by VSEPR-Sim XYZ Writer\n";
    } else {
        output << mol.comment << "\n";
    }
    
    // Lines 3+: Atom data
    output << std::fixed << std::setprecision(precision_);
    for (const auto& atom : mol.atoms) {
        output << atom.element << " "
               << atom.position[0] << " "
               << atom.position[1] << " "
               << atom.position[2] << "\n";
    }
    
    return true;
}

std::string XYZWriter::to_string(const XYZMolecule& mol) {
    std::ostringstream oss;
    write_stream(oss, mol);
    return oss.str();
}

// ============================================================================
// XYZAReader Implementation
// ============================================================================

bool XYZAReader::read(const std::string& filename, XYZMolecule& mol) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        error_message_ = "Cannot open file: " + filename;
        return false;
    }
    return read_stream(file, mol);
}

bool XYZAReader::read_stream(std::istream& input, XYZMolecule& mol) {
    mol = XYZMolecule();
    
    std::string line;
    
    // Line 1: Number of atoms
    if (!std::getline(input, line)) {
        error_message_ = "Failed to read atom count";
        return false;
    }
    int num_atoms = std::stoi(line);
    
    // Line 2: Comment (may contain properties)
    if (!std::getline(input, line)) {
        error_message_ = "Failed to read comment";
        return false;
    }
    mol.comment = line;
    parse_properties(line);
    
    // Lines 3+: Extended atom data
    mol.atoms.reserve(num_atoms);
    
    for (int i = 0; i < num_atoms; i++) {
        if (!std::getline(input, line)) {
            error_message_ = "Unexpected end at atom " + std::to_string(i);
            return false;
        }
        
        std::istringstream iss(line);
        XYZAtom atom;
        
        // Required: element x y z
        if (!(iss >> atom.element >> atom.position[0] >> atom.position[1] >> atom.position[2])) {
            error_message_ = "Invalid atom data at line " + std::to_string(i + 3);
            return false;
        }
        
        // Optional: charge vx vy vz fx fy fz energy
        iss >> atom.charge;
        iss >> atom.velocity[0] >> atom.velocity[1] >> atom.velocity[2];
        iss >> atom.force[0] >> atom.force[1] >> atom.force[2];
        iss >> atom.energy;
        
        mol.atoms.push_back(atom);
    }
    
    // Read bonds if present
    while (std::getline(input, line)) {
        if (line.find("BOND") == 0 || line.find("bond") == 0) {
            std::istringstream iss(line);
            std::string keyword;
            int i, j;
            double order = 1.0;
            
            iss >> keyword >> i >> j >> order;
            mol.bonds.emplace_back(i, j, order);
        }
    }
    
    mol.compute_bounds();
    return true;
}

void XYZAReader::parse_properties(const std::string& comment) {
    properties_.clear();
    
    // Look for "properties=" keyword
    size_t pos = comment.find("properties=");
    if (pos == std::string::npos) return;
    
    pos += 11;  // Skip "properties="
    size_t end = comment.find_first_of(" \t\n", pos);
    std::string props_str = comment.substr(pos, end - pos);
    
    // Split by colon
    std::istringstream iss(props_str);
    std::string prop;
    while (std::getline(iss, prop, ':')) {
        if (!prop.empty()) {
            properties_.push_back(prop);
        }
    }
}

// ============================================================================
// XYZAWriter Implementation
// ============================================================================

bool XYZAWriter::write(const std::string& filename, const XYZMolecule& mol) {
    std::ofstream file(filename);
    if (!file.is_open()) {
        error_message_ = "Cannot write to file: " + filename;
        return false;
    }
    return write_stream(file, mol);
}

bool XYZAWriter::write_stream(std::ostream& output, const XYZMolecule& mol) {
    // Line 1: Number of atoms
    output << mol.atoms.size() << "\n";
    
    // Line 2: Comment with properties
    std::string props = generate_properties_string();
    if (!props.empty()) {
        output << mol.comment << " properties=" << props << "\n";
    } else {
        output << mol.comment << "\n";
    }
    
    // Lines 3+: Extended atom data
    output << std::fixed << std::setprecision(precision_);
    
    for (const auto& atom : mol.atoms) {
        output << atom.element << " "
               << atom.position[0] << " "
               << atom.position[1] << " "
               << atom.position[2];
        
        if (write_charge_) {
            output << " " << atom.charge;
        }
        
        if (write_velocity_) {
            output << " " << atom.velocity[0] 
                   << " " << atom.velocity[1]
                   << " " << atom.velocity[2];
        }
        
        if (write_force_) {
            output << " " << atom.force[0]
                   << " " << atom.force[1]
                   << " " << atom.force[2];
        }
        
        if (write_energy_) {
            output << " " << atom.energy;
        }
        
        output << "\n";
    }
    
    // Write bonds if enabled
    if (write_bonds_ && !mol.bonds.empty()) {
        output << "\n# Bond connectivity\n";
        for (const auto& bond : mol.bonds) {
            output << "BOND " << bond.atom_i << " " << bond.atom_j 
                   << " " << bond.bond_order << "\n";
        }
    }
    
    return true;
}

std::string XYZAWriter::generate_properties_string() const {
    std::string props = "species:S:1:pos:R:3";
    
    if (write_charge_) props += ":charge:R:1";
    if (write_velocity_) props += ":vel:R:3";
    if (write_force_) props += ":force:R:3";
    if (write_energy_) props += ":energy:R:1";
    
    return props;
}

// ============================================================================
// XYZ Utilities Implementation
// ============================================================================

namespace xyz_utils {

XYZFormat detect_format(const std::string& filename) {
    // Check extension (C++17 compatible)
    if (filename.length() >= 4 && filename.substr(filename.length() - 4) == ".xyz") {
        return XYZFormat::STANDARD_XYZ;
    }
    if (filename.length() >= 5 && filename.substr(filename.length() - 5) == ".xyzA") {
        return XYZFormat::EXTENDED_XYZA;
    }
    if (filename.length() >= 5 && filename.substr(filename.length() - 5) == ".xyzC") {
        return XYZFormat::THERMAL_XYZC;
    }
    
    // Check content
    std::ifstream file(filename);
    if (!file.is_open()) return XYZFormat::UNKNOWN;
    
    std::string line;
    std::getline(file, line);  // Skip atom count
    std::getline(file, line);  // Comment line
    
    if (line.find("properties=") != std::string::npos) {
        return XYZFormat::EXTENDED_XYZA;
    }
    
    return XYZFormat::STANDARD_XYZ;
}

bool convert_xyz_to_xyza(const std::string& input, const std::string& output) {
    XYZReader reader;
    XYZMolecule mol;
    
    if (!reader.read(input, mol)) return false;
    
    XYZAWriter writer;
    writer.enable_charge(false);
    writer.enable_velocity(false);
    
    return writer.write(output, mol);
}

bool convert_xyza_to_xyz(const std::string& input, const std::string& output) {
    XYZAReader reader;
    XYZMolecule mol;
    
    if (!reader.read(input, mol)) return false;
    
    XYZWriter writer;
    return writer.write(output, mol);
}

int get_atomic_number(const std::string& element) {
    static const std::map<std::string, int> atomic_numbers = {
        {"H", 1}, {"He", 2}, {"Li", 3}, {"Be", 4}, {"B", 5}, {"C", 6},
        {"N", 7}, {"O", 8}, {"F", 9}, {"Ne", 10}, {"Na", 11}, {"Mg", 12},
        {"Al", 13}, {"Si", 14}, {"P", 15}, {"S", 16}, {"Cl", 17}, {"Ar", 18},
        {"K", 19}, {"Ca", 20}, {"Sc", 21}, {"Ti", 22}, {"V", 23}, {"Cr", 24},
        {"Mn", 25}, {"Fe", 26}, {"Co", 27}, {"Ni", 28}, {"Cu", 29}, {"Zn", 30},
        {"Ga", 31}, {"Ge", 32}, {"As", 33}, {"Se", 34}, {"Br", 35}, {"Kr", 36}
    };
    
    auto it = atomic_numbers.find(element);
    return (it != atomic_numbers.end()) ? it->second : 0;
}

std::string get_element_symbol(int atomic_number) {
    static const std::string elements[] = {
        "", "H", "He", "Li", "Be", "B", "C", "N", "O", "F", "Ne",
        "Na", "Mg", "Al", "Si", "P", "S", "Cl", "Ar", "K", "Ca",
        "Sc", "Ti", "V", "Cr", "Mn", "Fe", "Co", "Ni", "Cu", "Zn",
        "Ga", "Ge", "As", "Se", "Br", "Kr"
    };
    
    return (atomic_number > 0 && atomic_number <= 36) ? elements[atomic_number] : "";
}

double get_atomic_mass(const std::string& element) {
    static const std::map<std::string, double> masses = {
        {"H", 1.008}, {"C", 12.011}, {"N", 14.007}, {"O", 15.999},
        {"F", 18.998}, {"P", 30.974}, {"S", 32.06}, {"Cl", 35.45},
        {"Br", 79.904}, {"I", 126.90}
    };
    
    auto it = masses.find(element);
    return (it != masses.end()) ? it->second : 0.0;
}

double get_covalent_radius(const std::string& element) {
    static const std::map<std::string, double> radii = {
        {"H", 0.31}, {"C", 0.76}, {"N", 0.71}, {"O", 0.66},
        {"F", 0.57}, {"P", 1.07}, {"S", 1.05}, {"Cl", 1.02},
        {"Br", 1.20}, {"I", 1.39}, {"Si", 1.11}, {"Al", 1.21}
    };
    
    auto it = radii.find(element);
    return (it != radii.end()) ? it->second : 1.0;
}

double get_vdw_radius(const std::string& element) {
    static const std::map<std::string, double> radii = {
        {"H", 1.20}, {"C", 1.70}, {"N", 1.55}, {"O", 1.52},
        {"F", 1.47}, {"P", 1.80}, {"S", 1.80}, {"Cl", 1.75},
        {"Br", 1.85}, {"I", 1.98}
    };
    
    auto it = radii.find(element);
    return (it != radii.end()) ? it->second : 2.0;
}

double compute_molecular_mass(const XYZMolecule& mol) {
    double total_mass = 0.0;
    for (const auto& atom : mol.atoms) {
        total_mass += get_atomic_mass(atom.element);
    }
    return total_mass;
}

std::string compute_formula(const XYZMolecule& mol) {
    std::map<std::string, int> counts;
    for (const auto& atom : mol.atoms) {
        counts[atom.element]++;
    }
    
    std::string formula;
    // Order: C, H, then alphabetical
    if (counts.count("C")) {
        formula += "C";
        if (counts["C"] > 1) formula += std::to_string(counts["C"]);
        counts.erase("C");
    }
    if (counts.count("H")) {
        formula += "H";
        if (counts["H"] > 1) formula += std::to_string(counts["H"]);
        counts.erase("H");
    }
    
    for (const auto& [element, count] : counts) {
        formula += element;
        if (count > 1) formula += std::to_string(count);
    }
    
    return formula;
}

std::array<double, 3> compute_center_of_mass(const XYZMolecule& mol) {
    std::array<double, 3> com = {0.0, 0.0, 0.0};
    double total_mass = 0.0;
    
    for (const auto& atom : mol.atoms) {
        double mass = get_atomic_mass(atom.element);
        for (int i = 0; i < 3; i++) {
            com[i] += mass * atom.position[i];
        }
        total_mass += mass;
    }
    
    if (total_mass > 0.0) {
        for (int i = 0; i < 3; i++) {
            com[i] /= total_mass;
        }
    }
    
    return com;
}

std::array<double, 3> compute_dipole_moment(const XYZMolecule& mol) {
    std::array<double, 3> dipole = {0.0, 0.0, 0.0};
    
    for (const auto& atom : mol.atoms) {
        for (int i = 0; i < 3; i++) {
            dipole[i] += atom.charge * atom.position[i];
        }
    }
    
    return dipole;
}

double compute_distance(const XYZAtom& a, const XYZAtom& b) {
    double dx = a.position[0] - b.position[0];
    double dy = a.position[1] - b.position[1];
    double dz = a.position[2] - b.position[2];
    return std::sqrt(dx*dx + dy*dy + dz*dz);
}

double compute_angle(const XYZAtom& a, const XYZAtom& b, const XYZAtom& c) {
    // Vectors b->a and b->c
    double ba[3] = {
        a.position[0] - b.position[0],
        a.position[1] - b.position[1],
        a.position[2] - b.position[2]
    };
    double bc[3] = {
        c.position[0] - b.position[0],
        c.position[1] - b.position[1],
        c.position[2] - b.position[2]
    };
    
    // Dot product and magnitudes
    double dot = ba[0]*bc[0] + ba[1]*bc[1] + ba[2]*bc[2];
    double len_ba = std::sqrt(ba[0]*ba[0] + ba[1]*ba[1] + ba[2]*ba[2]);
    double len_bc = std::sqrt(bc[0]*bc[0] + bc[1]*bc[1] + bc[2]*bc[2]);
    
    double cos_angle = dot / (len_ba * len_bc);
    cos_angle = std::max(-1.0, std::min(1.0, cos_angle));  // Clamp
    
    return std::acos(cos_angle) * 180.0 / M_PI;  // Convert to degrees
}

double compute_dihedral(const XYZAtom& a, const XYZAtom& b,
                        const XYZAtom& c, const XYZAtom& d) {
    // Vectors
    double b1[3], b2[3], b3[3];
    for (int i = 0; i < 3; i++) {
        b1[i] = b.position[i] - a.position[i];
        b2[i] = c.position[i] - b.position[i];
        b3[i] = d.position[i] - c.position[i];
    }
    
    // Normal vectors
    double n1[3], n2[3];
    n1[0] = b1[1]*b2[2] - b1[2]*b2[1];
    n1[1] = b1[2]*b2[0] - b1[0]*b2[2];
    n1[2] = b1[0]*b2[1] - b1[1]*b2[0];
    
    n2[0] = b2[1]*b3[2] - b2[2]*b3[1];
    n2[1] = b2[2]*b3[0] - b2[0]*b3[2];
    n2[2] = b2[0]*b3[1] - b2[1]*b3[0];
    
    // Angle between normals
    double dot = n1[0]*n2[0] + n1[1]*n2[1] + n1[2]*n2[2];
    double len1 = std::sqrt(n1[0]*n1[0] + n1[1]*n1[1] + n1[2]*n1[2]);
    double len2 = std::sqrt(n2[0]*n2[0] + n2[1]*n2[1] + n2[2]*n2[2]);
    
    double cos_angle = dot / (len1 * len2);
    cos_angle = std::max(-1.0, std::min(1.0, cos_angle));
    
    return std::acos(cos_angle) * 180.0 / M_PI;
}

bool validate_xyz_molecule(const XYZMolecule& mol, std::string& error) {
    if (mol.atoms.empty()) {
        error = "No atoms in molecule";
        return false;
    }
    
    // Check for valid elements
    for (size_t i = 0; i < mol.atoms.size(); i++) {
        if (get_atomic_number(mol.atoms[i].element) == 0) {
            error = "Invalid element '" + mol.atoms[i].element + "' at atom " + std::to_string(i);
            return false;
        }
    }
    
    // Check bonds
    for (const auto& bond : mol.bonds) {
        if (bond.atom_i >= static_cast<int>(mol.atoms.size()) ||
            bond.atom_j >= static_cast<int>(mol.atoms.size()) ||
            bond.atom_i < 0 || bond.atom_j < 0) {
            error = "Invalid bond indices: " + std::to_string(bond.atom_i) + 
                    " - " + std::to_string(bond.atom_j);
            return false;
        }
    }
    
    return true;
}

bool check_bonds_valid(const XYZMolecule& mol) {
    for (const auto& bond : mol.bonds) {
        if (bond.atom_i >= static_cast<int>(mol.atoms.size()) ||
            bond.atom_j >= static_cast<int>(mol.atoms.size())) {
            return false;
        }
        
        // Check reasonable distance
        double dist = compute_distance(mol.atoms[bond.atom_i], mol.atoms[bond.atom_j]);
        if (dist < 0.4 || dist > 3.0) {  // Reasonable bond length range
            return false;
        }
    }
    return true;
}

bool check_geometry_reasonable(const XYZMolecule& mol, double min_distance) {
    for (size_t i = 0; i < mol.atoms.size(); i++) {
        for (size_t j = i + 1; j < mol.atoms.size(); j++) {
            double dist = compute_distance(mol.atoms[i], mol.atoms[j]);
            if (dist < min_distance) {
                return false;  // Atoms too close
            }
        }
    }
    return true;
}

} // namespace xyz_utils

// ============================================================================
// XYZTrajectory Implementation
// ============================================================================

void XYZTrajectory::add_frame(const XYZMolecule& mol, double time) {
    frames_.push_back(mol);
    times_.push_back(time);
}

const XYZMolecule& XYZTrajectory::get_frame(size_t index) const {
    return frames_.at(index);
}

XYZMolecule& XYZTrajectory::get_frame(size_t index) {
    return frames_.at(index);
}

double XYZTrajectory::get_time(size_t index) const {
    return times_.at(index);
}

bool XYZTrajectory::write(const std::string& filename) const {
    std::ofstream file(filename);
    if (!file.is_open()) return false;
    
    XYZWriter writer;
    
    for (size_t i = 0; i < frames_.size(); i++) {
        XYZMolecule frame_copy = frames_[i];
        frame_copy.comment += " t=" + std::to_string(times_[i]);
        writer.write_stream(file, frame_copy);
    }
    
    return true;
}

bool XYZTrajectory::read(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) return false;
    
    clear();
    
    XYZReader reader;
    double time = 0.0;
    
    while (file.peek() != EOF) {
        XYZMolecule mol;
        if (!reader.read_stream(file, mol)) break;
        
        // Try to parse time from comment
        size_t pos = mol.comment.find("t=");
        if (pos != std::string::npos) {
            time = std::stod(mol.comment.substr(pos + 2));
        }
        
        add_frame(mol, time);
        time += 1.0;  // Default increment
    }
    
    return !frames_.empty();
}

void XYZTrajectory::clear() {
    frames_.clear();
    times_.clear();
}

}} // namespace vsepr::io
