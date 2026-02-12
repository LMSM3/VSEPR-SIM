/**
 * XYZC Format Specification v1.0
 * 
 * High-precision molecular and thermal dynamics tracking format
 * Frame-of-reference: 10,000 x ~20,000 state vector tracking
 * 
 * Purpose:
 * - Track molecular positions over extended trajectories
 * - Record thermal pathway activation states per timestep
 * - Store energy node distributions (6 pathway classes)
 * - Enable deterministic replay and analysis
 * 
 * File structure:
 * - Header: Global metadata, pathway graph topology
 * - Frames: Timestep snapshots with full state vectors
 * - Footer: Summary statistics, emergent observables
 */

#pragma once

#include <vector>
#include <string>
#include <fstream>
#include <array>
#include <cstdint>

namespace vsepr {
namespace thermal {

// ============================================================================
// Thermal Pathway Classes (The 6 Mandatory Pathways)
// ============================================================================
enum class PathwayClass : uint8_t {
    PHONON_LATTICE = 0,      // Bond-mediated vibrational (solids backbone)
    ELECTRONIC = 1,          // Free electron transport (metals)
    MOLECULAR_ROTATIONAL = 2,// Asymmetric rotation (polymers, soft materials)
    TRANSLATIONAL_KINETIC = 3,// Collision-based (gases, fluids)
    RADIATIVE_MICRO = 4,     // Surface emission (temperature-gated)
    GATED_STRUCTURAL = 5     // Phase change, bond rupture (activation-gated)
};

// Convert pathway enum to string
inline const char* pathway_class_name(PathwayClass cls) {
    static const char* names[] = {
        "Phonon_Lattice",
        "Electronic",
        "Molecular_Rotational",
        "Translational_Kinetic",
        "Radiative_Micro",
        "Gated_Structural"
    };
    return names[static_cast<uint8_t>(cls)];
}

// ============================================================================
// Energy Node (The building block)
// ============================================================================
struct EnergyNode {
    double capacity;           // Heat capacity (J/K)
    double current_energy;     // Current energy content (J)
    PathwayClass pathway_type; // Which pathway class this node belongs to
    uint32_t atom_index;       // Which atom owns this node (0xFFFFFFFF = global)
    
    EnergyNode() 
        : capacity(0.0), current_energy(0.0), 
          pathway_type(PathwayClass::PHONON_LATTICE), atom_index(0) {}
    
    double temperature() const {
        return (capacity > 1e-12) ? (current_energy / capacity) : 0.0;
    }
};

// ============================================================================
// Thermal Edge (Permission to move energy)
// ============================================================================
struct ThermalEdge {
    uint32_t node_i;           // Source node index
    uint32_t node_j;           // Target node index
    double coupling_strength;  // Conductance (W/K or dimensionless)
    
    std::array<double, 3> directionality;  // Unit vector (0,0,0) = isotropic
    double damping;            // Energy loss coefficient [0,1]
    
    // Activation gate
    bool is_gated;             // Is this edge activation-controlled?
    double activation_energy;  // Energy barrier (J)
    double gate_state;         // Current activation [0,1]
    
    ThermalEdge() 
        : node_i(0), node_j(0), coupling_strength(0.0),
          directionality{0.0, 0.0, 0.0}, damping(0.0),
          is_gated(false), activation_energy(0.0), gate_state(1.0) {}
};

// ============================================================================
// Frame State Vector (10,000 x 20,000 reference)
// ============================================================================
struct FrameStateVector {
    uint64_t frame_number;     // Timestep index
    double time;               // Simulation time (fs, ps, or s)
    
    // Molecular positions (N atoms × 3 coordinates)
    std::vector<std::array<double, 3>> positions;  // Ångströms
    std::vector<std::array<double, 3>> velocities; // Å/fs
    
    // Energy node states (6 pathway classes × N atoms)
    std::vector<EnergyNode> energy_nodes;
    
    // Edge activation states (dynamic)
    std::vector<ThermalEdge> active_edges;
    
    // Global observables
    double total_energy;       // System total energy (J)
    double kinetic_energy;     // Kinetic contribution (J)
    double potential_energy;   // Potential contribution (J)
    double thermal_energy;     // Thermal reservoir (J)
    
    double global_temperature; // Volume-averaged T (K)
    double max_temperature;    // Hotspot T (K)
    double min_temperature;    // Coldspot T (K)
    
    FrameStateVector() 
        : frame_number(0), time(0.0),
          total_energy(0.0), kinetic_energy(0.0), potential_energy(0.0),
          thermal_energy(0.0), global_temperature(0.0),
          max_temperature(0.0), min_temperature(0.0) {}
};

// ============================================================================
// XYZC File Header
// ============================================================================
struct XYZCHeader {
    // Magic number for format validation
    uint32_t magic = 0x4358595A;  // "XYZC" in hex
    uint16_t version_major = 1;
    uint16_t version_minor = 0;
    
    // Simulation metadata
    uint32_t num_atoms;
    uint32_t num_frames;
    uint32_t num_energy_nodes;
    uint32_t num_thermal_edges;
    
    // Bounding box for spatial reference (10,000 Å scale)
    std::array<double, 3> box_min = {0.0, 0.0, 0.0};
    std::array<double, 3> box_max = {10000.0, 10000.0, 10000.0};
    
    // Timestep information
    double dt;                 // Timestep size (fs)
    double total_time;         // Total simulation time (ps)
    
    // Pathway topology (static graph structure)
    std::vector<PathwayClass> node_pathway_classes;
    std::vector<std::pair<uint32_t, uint32_t>> edge_topology;
    
    // Element symbols
    std::vector<std::string> element_symbols;
    
    XYZCHeader() 
        : num_atoms(0), num_frames(0), num_energy_nodes(0),
          num_thermal_edges(0), dt(0.0), total_time(0.0) {}
};

// ============================================================================
// XYZC Writer
// ============================================================================
class XYZCWriter {
public:
    XYZCWriter(const std::string& filename);
    ~XYZCWriter();
    
    // Write header (must be called first)
    void write_header(const XYZCHeader& header);
    
    // Write a single frame
    void write_frame(const FrameStateVector& frame);
    
    // Finalize file (write footer with summary stats)
    void finalize();
    
    // Get write status
    bool is_open() const { return file_.is_open(); }
    uint64_t frames_written() const { return frames_written_; }
    
private:
    std::ofstream file_;
    bool header_written_;
    uint64_t frames_written_;
    
    // Write binary data
    template<typename T>
    void write_binary(const T& data);
    
    void write_string(const std::string& str);
    void write_vector_3d(const std::array<double, 3>& vec);
};

// ============================================================================
// XYZC Reader
// ============================================================================
class XYZCReader {
public:
    XYZCReader(const std::string& filename);
    ~XYZCReader();
    
    // Read header
    bool read_header(XYZCHeader& header);
    
    // Read next frame (sequential access)
    bool read_frame(FrameStateVector& frame);
    
    // Seek to specific frame (random access)
    bool seek_frame(uint64_t frame_number);
    
    // Get read status
    bool is_open() const { return file_.is_open(); }
    uint64_t current_frame() const { return current_frame_; }
    
private:
    std::ifstream file_;
    XYZCHeader header_;
    uint64_t current_frame_;
    
    // Read binary data
    template<typename T>
    bool read_binary(T& data);
    
    bool read_string(std::string& str);
    bool read_vector_3d(std::array<double, 3>& vec);
};

// ============================================================================
// Pathway Graph Builder
// ============================================================================
class ThermalPathwayGraph {
public:
    ThermalPathwayGraph(uint32_t num_atoms);
    
    // Build pathway graph from molecular topology
    void build_from_bonds(
        const std::vector<std::pair<uint32_t, uint32_t>>& bonds,
        const std::vector<double>& bond_orders,
        const std::vector<uint8_t>& atomic_numbers
    );
    
    // Enable/disable pathway classes
    void enable_phonon_pathways(bool enable);
    void enable_electronic_pathways(bool enable);
    void enable_rotational_pathways(bool enable);
    void enable_translational_pathways(bool enable);
    void enable_radiative_pathways(bool enable);
    void enable_gated_pathways(bool enable);
    
    // Get pathway graph for export
    std::vector<EnergyNode>& get_energy_nodes() { return energy_nodes_; }
    std::vector<ThermalEdge>& get_thermal_edges() { return thermal_edges_; }
    
    const std::vector<EnergyNode>& get_energy_nodes() const { return energy_nodes_; }
    const std::vector<ThermalEdge>& get_thermal_edges() const { return thermal_edges_; }
    
    // Simulation step (THE 6-STEP MANDATE)
    void simulation_step(double dt);
    
    // Get emergent observables (measured, not input)
    double measure_thermal_conductivity() const;
    double measure_heat_capacity() const;
    double measure_thermal_expansion() const;
    
    // Print pathway status
    void print_pathway_status() const;
    
private:
    uint32_t num_atoms_;
    std::vector<EnergyNode> energy_nodes_;
    std::vector<ThermalEdge> thermal_edges_;
    
    // Pathway enable flags
    bool phonon_enabled_;
    bool electronic_enabled_;
    bool rotational_enabled_;
    bool translational_enabled_;
    bool radiative_enabled_;
    bool gated_enabled_;
    
    // The 6 non-negotiable simulation steps
    void step_1_accumulate_incoming_energy();
    void step_2_evaluate_activation_gates();
    void step_3_transfer_energy_along_edges(double dt);
    void step_4_apply_damping_and_losses();
    void step_5_promote_coherent_energy();
    void step_6_record_observables();
    
    // Helper: compute coupling strength for edge
    double compute_coupling(PathwayClass cls, uint32_t i, uint32_t j, 
                           double bond_order, double distance) const;
};

// ============================================================================
// Demonstration Function
// ============================================================================

// Create a demo XYZC file with 50 frames of water molecule thermal dynamics
void create_demo_xyzc_file(const std::string& filename);

}} // namespace vsepr::thermal
