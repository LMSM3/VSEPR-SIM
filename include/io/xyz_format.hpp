/**
 * XYZ File Format Library
 * 
 * Comprehensive I/O for molecular coordinate files:
 * - .xyz  : Standard XYZ format (positions only)
 * - .xyzA : Extended XYZ with Analysis data (bonds, charges, velocities)
 * - .xyzC : Thermal pathways format (see thermal/xyzc_format.hpp)
 * 
 * Format Specifications:
 * 
 * Standard XYZ (.xyz):
 *   Line 1: <number of atoms>
 *   Line 2: <comment>
 *   Line 3+: <element> <x> <y> <z>
 * 
 * Extended XYZ (.xyzA):
 *   Line 1: <number of atoms>
 *   Line 2: <comment> [properties="<property_list>"]
 *   Line 3+: <element> <x> <y> <z> [<charge> <vx> <vy> <vz> ...]
 * 
 * Properties can include: charge, velocity, force, energy, bonds, etc.
 */

#pragma once

#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <map>
#include <optional>
#include <array>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace vsepr {
namespace io {

// ============================================================================
// Forward Declarations
// ============================================================================

struct XYZAtom;
struct XYZBond;
struct XYZMolecule;
class XYZReader;
class XYZWriter;
class XYZAReader;
class XYZAWriter;

// ============================================================================
// Data Structures
// ============================================================================

/**
 * Single atom in XYZ format
 */
struct XYZAtom {
    std::string element;           // Element symbol (H, C, N, O, etc.)
    std::array<double, 3> position;  // Cartesian coordinates (Ångströms)
    
    // Extended properties (optional, for .xyzA)
    double charge = 0.0;             // Partial charge (e)
    std::array<double, 3> velocity = {0.0, 0.0, 0.0};  // Velocity (Å/fs)
    std::array<double, 3> force = {0.0, 0.0, 0.0};     // Force (eV/Å)
    double energy = 0.0;             // Per-atom energy (eV)
    int atom_type = 0;               // Force field atom type
    
    XYZAtom() : position{0.0, 0.0, 0.0} {}
    XYZAtom(const std::string& elem, double x, double y, double z)
        : element(elem), position{x, y, z} {}
    
    // GLM conversion helpers
    glm::vec3 get_position_glm() const {
        return glm::vec3(position[0], position[1], position[2]);
    }
    
    void set_position_glm(const glm::vec3& pos) {
        position[0] = pos.x;
        position[1] = pos.y;
        position[2] = pos.z;
    }
    
    glm::vec3 get_velocity_glm() const {
        return glm::vec3(velocity[0], velocity[1], velocity[2]);
    }
    
    void set_velocity_glm(const glm::vec3& vel) {
        velocity[0] = vel.x;
        velocity[1] = vel.y;
        velocity[2] = vel.z;
    }
    
    glm::vec3 get_force_glm() const {
        return glm::vec3(force[0], force[1], force[2]);
    }
    
    void set_force_glm(const glm::vec3& f) {
        force[0] = f.x;
        force[1] = f.y;
        force[2] = f.z;
    }
};

/**
 * Bond connectivity information
 */
struct XYZBond {
    int atom_i;          // First atom index (0-based)
    int atom_j;          // Second atom index (0-based)
    double bond_order;   // Bond order (1.0=single, 2.0=double, etc.)
    
    XYZBond(int i, int j, double order = 1.0)
        : atom_i(i), atom_j(j), bond_order(order) {}
};

/**
 * Complete molecular structure
 */
struct XYZMolecule {
    std::vector<XYZAtom> atoms;
    std::vector<XYZBond> bonds;
    std::string comment;
    
    // Metadata
    double total_energy = 0.0;       // Total molecular energy (eV)
    double total_charge = 0.0;       // Net charge (e)
    std::string formula;             // Chemical formula
    
    // Bounding box
    std::array<double, 3> box_min = {0.0, 0.0, 0.0};
    std::array<double, 3> box_max = {0.0, 0.0, 0.0};
    
    XYZMolecule() = default;
    
    // Compute bounding box
    void compute_bounds();
    
    // Get center of geometry
    std::array<double, 3> get_center() const;
    
    // Get number of atoms
    size_t num_atoms() const { return atoms.size(); }
    size_t num_bonds() const { return bonds.size(); }
    
    // Translate all atoms
    void translate(double dx, double dy, double dz);
    void translate(const glm::vec3& delta);
    
    // Rotate all atoms (axis-angle, radians)
    void rotate(const std::array<double, 3>& axis, double angle);
    void rotate(const glm::vec3& axis, double angle);
    void rotate(const glm::mat4& rotation_matrix);
    
    // Scale coordinates
    void scale(double factor);
    
    // Transform all atoms by matrix
    void transform(const glm::mat4& matrix);
};

// ============================================================================
// Standard XYZ Reader (.xyz)
// ============================================================================

class XYZReader {
public:
    XYZReader() = default;
    
    /**
     * Read XYZ file from path
     */
    bool read(const std::string& filename, XYZMolecule& mol);
    
    /**
     * Read XYZ from stream
     */
    bool read_stream(std::istream& input, XYZMolecule& mol);
    
    /**
     * Read XYZ from string
     */
    bool read_string(const std::string& xyz_string, XYZMolecule& mol);
    
    /**
     * Auto-detect bonds based on covalent radii
     */
    void detect_bonds(XYZMolecule& mol, double scale_factor = 1.2) const;
    
    /**
     * Get last error message
     */
    const std::string& get_error() const { return error_message_; }
    
private:
    std::string error_message_;
    
    // Helper: Get covalent radius for element
    double get_covalent_radius(const std::string& element) const;
};

// ============================================================================
// Standard XYZ Writer (.xyz)
// ============================================================================

class XYZWriter {
public:
    XYZWriter() = default;
    
    /**
     * Write XYZ file
     */
    bool write(const std::string& filename, const XYZMolecule& mol);
    
    /**
     * Write XYZ to stream
     */
    bool write_stream(std::ostream& output, const XYZMolecule& mol);
    
    /**
     * Get XYZ as string
     */
    std::string to_string(const XYZMolecule& mol);
    
    /**
     * Set coordinate precision (decimal places)
     */
    void set_precision(int digits) { precision_ = digits; }
    
    /**
     * Get last error message
     */
    const std::string& get_error() const { return error_message_; }
    
private:
    int precision_ = 6;  // Default: 6 decimal places
    std::string error_message_;
};

// ============================================================================
// Extended XYZ Reader (.xyzA)
// ============================================================================

class XYZAReader {
public:
    XYZAReader() = default;
    
    /**
     * Read extended XYZ file
     * Format: element x y z [charge vx vy vz fx fy fz energy]
     */
    bool read(const std::string& filename, XYZMolecule& mol);
    
    /**
     * Read from stream
     */
    bool read_stream(std::istream& input, XYZMolecule& mol);
    
    /**
     * Parse property specification from comment line
     * Example: "properties=species:S:1:pos:R:3:charge:R:1:vel:R:3"
     */
    void parse_properties(const std::string& comment);
    
    /**
     * Get property list
     */
    const std::vector<std::string>& get_properties() const { return properties_; }
    
    const std::string& get_error() const { return error_message_; }
    
private:
    std::vector<std::string> properties_;
    std::string error_message_;
};

// ============================================================================
// Extended XYZ Writer (.xyzA)
// ============================================================================

class XYZAWriter {
public:
    XYZAWriter() = default;
    
    /**
     * Write extended XYZ file with all available data
     */
    bool write(const std::string& filename, const XYZMolecule& mol);
    
    /**
     * Write to stream
     */
    bool write_stream(std::ostream& output, const XYZMolecule& mol);
    
    /**
     * Enable/disable specific properties
     */
    void enable_charge(bool enable) { write_charge_ = enable; }
    void enable_velocity(bool enable) { write_velocity_ = enable; }
    void enable_force(bool enable) { write_force_ = enable; }
    void enable_energy(bool enable) { write_energy_ = enable; }
    void enable_bonds(bool enable) { write_bonds_ = enable; }
    
    /**
     * Set coordinate precision
     */
    void set_precision(int digits) { precision_ = digits; }
    
    const std::string& get_error() const { return error_message_; }
    
private:
    int precision_ = 6;
    bool write_charge_ = false;
    bool write_velocity_ = false;
    bool write_force_ = false;
    bool write_energy_ = false;
    bool write_bonds_ = false;
    std::string error_message_;
    
    // Generate properties string for comment line
    std::string generate_properties_string() const;
};

// ============================================================================
// XYZ Format Utilities
// ============================================================================

namespace xyz_utils {

/**
 * Detect file format from extension or content
 */
enum class XYZFormat {
    STANDARD_XYZ,   // .xyz
    EXTENDED_XYZA,  // .xyzA
    THERMAL_XYZC,   // .xyzC (binary)
    UNKNOWN
};

XYZFormat detect_format(const std::string& filename);

/**
 * Convert between formats
 */
bool convert_xyz_to_xyza(const std::string& input, const std::string& output);
bool convert_xyza_to_xyz(const std::string& input, const std::string& output);

/**
 * Element data helpers
 */
int get_atomic_number(const std::string& element);
std::string get_element_symbol(int atomic_number);
double get_atomic_mass(const std::string& element);
double get_covalent_radius(const std::string& element);
double get_vdw_radius(const std::string& element);

/**
 * Compute molecular properties from XYZ
 */
double compute_molecular_mass(const XYZMolecule& mol);
std::string compute_formula(const XYZMolecule& mol);
std::array<double, 3> compute_center_of_mass(const XYZMolecule& mol);
std::array<double, 3> compute_dipole_moment(const XYZMolecule& mol);

/**
 * Geometry operations
 */
double compute_distance(const XYZAtom& a, const XYZAtom& b);
double compute_angle(const XYZAtom& a, const XYZAtom& b, const XYZAtom& c);  // Returns degrees
double compute_dihedral(const XYZAtom& a, const XYZAtom& b, 
                        const XYZAtom& c, const XYZAtom& d);  // Returns degrees

/**
 * Validation
 */
bool validate_xyz_molecule(const XYZMolecule& mol, std::string& error);
bool check_bonds_valid(const XYZMolecule& mol);
bool check_geometry_reasonable(const XYZMolecule& mol, double min_distance = 0.5);

} // namespace xyz_utils

// ============================================================================
// Multi-Frame XYZ Trajectory
// ============================================================================

/**
 * Multiple XYZ frames for molecular dynamics trajectories
 */
class XYZTrajectory {
public:
    XYZTrajectory() = default;
    
    /**
     * Add frame to trajectory
     */
    void add_frame(const XYZMolecule& mol, double time = 0.0);
    
    /**
     * Get frame at index
     */
    const XYZMolecule& get_frame(size_t index) const;
    XYZMolecule& get_frame(size_t index);
    
    /**
     * Get frame time
     */
    double get_time(size_t index) const;
    
    /**
     * Number of frames
     */
    size_t num_frames() const { return frames_.size(); }
    
    /**
     * Write entire trajectory to multi-frame XYZ file
     */
    bool write(const std::string& filename) const;
    
    /**
     * Read multi-frame XYZ file
     */
    bool read(const std::string& filename);
    
    /**
     * Clear all frames
     */
    void clear();
    
private:
    std::vector<XYZMolecule> frames_;
    std::vector<double> times_;
};

}} // namespace vsepr::io
