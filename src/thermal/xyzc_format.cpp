/**
 * XYZC Format Implementation
 * 
 * Binary format for high-precision molecular dynamics trajectories
 * with thermal pathway tracking across 10,000+ frame sequences.
 */

#include "thermal/xyzc_format.hpp"
#include <cstring>
#include <iostream>
#include <cmath>
#include <filesystem>

namespace vsepr {
namespace thermal {

// ============================================================================
// XYZCWriter Implementation
// ============================================================================

XYZCWriter::XYZCWriter(const std::string& filename)
    : header_written_(false), frames_written_(0) 
{
    file_.open(filename, std::ios::binary | std::ios::out);
    if (!file_.is_open()) {
        std::cerr << "ERROR: Failed to open " << filename << " for writing\n";
    }
}

XYZCWriter::~XYZCWriter() {
    if (file_.is_open()) {
        file_.close();
    }
}

void XYZCWriter::write_header(const XYZCHeader& header) {
    if (!file_.is_open()) return;
    
    // Write magic number
    write_binary(header.magic);
    write_binary(header.version_major);
    write_binary(header.version_minor);
    
    // Write counts
    write_binary(header.num_atoms);
    write_binary(header.num_frames);
    write_binary(header.num_energy_nodes);
    write_binary(header.num_thermal_edges);
    
    // Write bounding box
    write_vector_3d(header.box_min);
    write_vector_3d(header.box_max);
    
    // Write timestep info
    write_binary(header.dt);
    write_binary(header.total_time);
    
    // Write pathway topology
    uint32_t num_nodes = static_cast<uint32_t>(header.node_pathway_classes.size());
    write_binary(num_nodes);
    for (const auto& cls : header.node_pathway_classes) {
        write_binary(static_cast<uint8_t>(cls));
    }
    
    uint32_t num_edges = static_cast<uint32_t>(header.edge_topology.size());
    write_binary(num_edges);
    for (const auto& edge : header.edge_topology) {
        write_binary(edge.first);
        write_binary(edge.second);
    }
    
    // Write element symbols
    uint32_t num_elems = static_cast<uint32_t>(header.element_symbols.size());
    write_binary(num_elems);
    for (const auto& elem : header.element_symbols) {
        write_string(elem);
    }
    
    header_written_ = true;
}

void XYZCWriter::write_frame(const FrameStateVector& frame) {
    if (!file_.is_open() || !header_written_) return;
    
    // Write frame metadata
    write_binary(frame.frame_number);
    write_binary(frame.time);
    
    // Write positions
    uint32_t num_pos = static_cast<uint32_t>(frame.positions.size());
    write_binary(num_pos);
    for (const auto& pos : frame.positions) {
        write_vector_3d(pos);
    }
    
    // Write velocities
    uint32_t num_vel = static_cast<uint32_t>(frame.velocities.size());
    write_binary(num_vel);
    for (const auto& vel : frame.velocities) {
        write_vector_3d(vel);
    }
    
    // Write energy nodes
    uint32_t num_nodes = static_cast<uint32_t>(frame.energy_nodes.size());
    write_binary(num_nodes);
    for (const auto& node : frame.energy_nodes) {
        write_binary(node.capacity);
        write_binary(node.current_energy);
        write_binary(static_cast<uint8_t>(node.pathway_type));
        write_binary(node.atom_index);
    }
    
    // Write active edges
    uint32_t num_edges = static_cast<uint32_t>(frame.active_edges.size());
    write_binary(num_edges);
    for (const auto& edge : frame.active_edges) {
        write_binary(edge.node_i);
        write_binary(edge.node_j);
        write_binary(edge.coupling_strength);
        write_vector_3d(edge.directionality);
        write_binary(edge.damping);
        write_binary(edge.is_gated);
        write_binary(edge.activation_energy);
        write_binary(edge.gate_state);
    }
    
    // Write global observables
    write_binary(frame.total_energy);
    write_binary(frame.kinetic_energy);
    write_binary(frame.potential_energy);
    write_binary(frame.thermal_energy);
    write_binary(frame.global_temperature);
    write_binary(frame.max_temperature);
    write_binary(frame.min_temperature);
    
    frames_written_++;
}

void XYZCWriter::finalize() {
    if (!file_.is_open()) return;
    
    // Write footer marker
    uint32_t footer_magic = 0x464F4F54;  // "FOOT"
    write_binary(footer_magic);
    write_binary(frames_written_);
    
    file_.close();
}

template<typename T>
void XYZCWriter::write_binary(const T& data) {
    file_.write(reinterpret_cast<const char*>(&data), sizeof(T));
}

void XYZCWriter::write_string(const std::string& str) {
    uint32_t len = static_cast<uint32_t>(str.size());
    write_binary(len);
    file_.write(str.c_str(), len);
}

void XYZCWriter::write_vector_3d(const std::array<double, 3>& vec) {
    for (int i = 0; i < 3; i++) {
        write_binary(vec[i]);
    }
}

// ============================================================================
// XYZCReader Implementation
// ============================================================================

XYZCReader::XYZCReader(const std::string& filename)
    : current_frame_(0)
{
    file_.open(filename, std::ios::binary | std::ios::in);
    if (!file_.is_open()) {
        std::cerr << "ERROR: Failed to open " << filename << " for reading\n";
    }
}

XYZCReader::~XYZCReader() {
    if (file_.is_open()) {
        file_.close();
    }
}

bool XYZCReader::read_header(XYZCHeader& header) {
    if (!file_.is_open()) return false;
    
    // Read and validate magic number
    uint32_t magic;
    if (!read_binary(magic) || magic != 0x4358595A) {
        std::cerr << "ERROR: Invalid XYZC file (bad magic number)\n";
        return false;
    }
    
    header.magic = magic;
    read_binary(header.version_major);
    read_binary(header.version_minor);
    
    // Read counts
    read_binary(header.num_atoms);
    read_binary(header.num_frames);
    read_binary(header.num_energy_nodes);
    read_binary(header.num_thermal_edges);
    
    // Read bounding box
    read_vector_3d(header.box_min);
    read_vector_3d(header.box_max);
    
    // Read timestep info
    read_binary(header.dt);
    read_binary(header.total_time);
    
    // Read pathway topology
    uint32_t num_nodes;
    read_binary(num_nodes);
    header.node_pathway_classes.resize(num_nodes);
    for (uint32_t i = 0; i < num_nodes; i++) {
        uint8_t cls;
        read_binary(cls);
        header.node_pathway_classes[i] = static_cast<PathwayClass>(cls);
    }
    
    uint32_t num_edges;
    read_binary(num_edges);
    header.edge_topology.resize(num_edges);
    for (uint32_t i = 0; i < num_edges; i++) {
        read_binary(header.edge_topology[i].first);
        read_binary(header.edge_topology[i].second);
    }
    
    // Read element symbols
    uint32_t num_elems;
    read_binary(num_elems);
    header.element_symbols.resize(num_elems);
    for (uint32_t i = 0; i < num_elems; i++) {
        read_string(header.element_symbols[i]);
    }
    
    header_ = header;
    return true;
}

bool XYZCReader::read_frame(FrameStateVector& frame) {
    if (!file_.is_open()) return false;
    
    // Read frame metadata
    if (!read_binary(frame.frame_number)) return false;
    read_binary(frame.time);
    
    // Read positions
    uint32_t num_pos;
    read_binary(num_pos);
    frame.positions.resize(num_pos);
    for (uint32_t i = 0; i < num_pos; i++) {
        read_vector_3d(frame.positions[i]);
    }
    
    // Read velocities
    uint32_t num_vel;
    read_binary(num_vel);
    frame.velocities.resize(num_vel);
    for (uint32_t i = 0; i < num_vel; i++) {
        read_vector_3d(frame.velocities[i]);
    }
    
    // Read energy nodes
    uint32_t num_nodes;
    read_binary(num_nodes);
    frame.energy_nodes.resize(num_nodes);
    for (uint32_t i = 0; i < num_nodes; i++) {
        read_binary(frame.energy_nodes[i].capacity);
        read_binary(frame.energy_nodes[i].current_energy);
        uint8_t cls;
        read_binary(cls);
        frame.energy_nodes[i].pathway_type = static_cast<PathwayClass>(cls);
        read_binary(frame.energy_nodes[i].atom_index);
    }
    
    // Read active edges
    uint32_t num_edges;
    read_binary(num_edges);
    frame.active_edges.resize(num_edges);
    for (uint32_t i = 0; i < num_edges; i++) {
        read_binary(frame.active_edges[i].node_i);
        read_binary(frame.active_edges[i].node_j);
        read_binary(frame.active_edges[i].coupling_strength);
        read_vector_3d(frame.active_edges[i].directionality);
        read_binary(frame.active_edges[i].damping);
        read_binary(frame.active_edges[i].is_gated);
        read_binary(frame.active_edges[i].activation_energy);
        read_binary(frame.active_edges[i].gate_state);
    }
    
    // Read global observables
    read_binary(frame.total_energy);
    read_binary(frame.kinetic_energy);
    read_binary(frame.potential_energy);
    read_binary(frame.thermal_energy);
    read_binary(frame.global_temperature);
    read_binary(frame.max_temperature);
    read_binary(frame.min_temperature);
    
    current_frame_++;
    return true;
}

bool XYZCReader::seek_frame(uint64_t frame_number) {
    // Not implemented for now (requires frame index)
    return false;
}

template<typename T>
bool XYZCReader::read_binary(T& data) {
    file_.read(reinterpret_cast<char*>(&data), sizeof(T));
    return file_.good();
}

bool XYZCReader::read_string(std::string& str) {
    uint32_t len;
    if (!read_binary(len)) return false;
    str.resize(len);
    file_.read(&str[0], len);
    return file_.good();
}

bool XYZCReader::read_vector_3d(std::array<double, 3>& vec) {
    for (int i = 0; i < 3; i++) {
        if (!read_binary(vec[i])) return false;
    }
    return true;
}

// ============================================================================
// ThermalPathwayGraph Implementation
// ============================================================================

ThermalPathwayGraph::ThermalPathwayGraph(uint32_t num_atoms)
    : num_atoms_(num_atoms),
      phonon_enabled_(true),
      electronic_enabled_(false),
      rotational_enabled_(true),
      translational_enabled_(true),
      radiative_enabled_(false),
      gated_enabled_(true)
{
    // Initialize 6 energy nodes per atom (one for each pathway class)
    energy_nodes_.resize(num_atoms * 6);
    
    for (uint32_t i = 0; i < num_atoms; i++) {
        for (uint8_t cls = 0; cls < 6; cls++) {
            uint32_t node_id = i * 6 + cls;
            energy_nodes_[node_id].atom_index = i;
            energy_nodes_[node_id].pathway_type = static_cast<PathwayClass>(cls);
            energy_nodes_[node_id].capacity = 1.0;  // Will be updated based on bonding
            energy_nodes_[node_id].current_energy = 0.0;
        }
    }
}

void ThermalPathwayGraph::build_from_bonds(
    const std::vector<std::pair<uint32_t, uint32_t>>& bonds,
    const std::vector<double>& bond_orders,
    const std::vector<uint8_t>& atomic_numbers)
{
    thermal_edges_.clear();
    
    // Build phonon pathways (bond-mediated)
    if (phonon_enabled_) {
        for (size_t b = 0; b < bonds.size(); b++) {
            uint32_t i = bonds[b].first;
            uint32_t j = bonds[b].second;
            double order = (b < bond_orders.size()) ? bond_orders[b] : 1.0;
            
            ThermalEdge edge;
            edge.node_i = i * 6 + 0;  // Phonon node of atom i
            edge.node_j = j * 6 + 0;  // Phonon node of atom j
            edge.coupling_strength = compute_coupling(PathwayClass::PHONON_LATTICE, i, j, order, 1.5);
            edge.directionality = {0.0, 0.0, 0.0};  // Isotropic
            edge.damping = 0.01;  // 1% loss per step
            edge.is_gated = false;
            thermal_edges_.push_back(edge);
        }
    }
    
    // Build translational pathways (all-to-all for gases)
    if (translational_enabled_) {
        for (uint32_t i = 0; i < num_atoms_; i++) {
            for (uint32_t j = i + 1; j < num_atoms_; j++) {
                ThermalEdge edge;
                edge.node_i = i * 6 + 3;  // Translational node
                edge.node_j = j * 6 + 3;
                edge.coupling_strength = 0.1;  // Weak non-bonded
                edge.directionality = {0.0, 0.0, 0.0};
                edge.damping = 0.05;
                edge.is_gated = false;
                thermal_edges_.push_back(edge);
            }
        }
    }
    
    // Build rotational pathways (molecular-level)
    if (rotational_enabled_) {
        for (uint32_t i = 0; i < num_atoms_; i++) {
            ThermalEdge edge;
            edge.node_i = i * 6 + 2;  // Rotational self-coupling
            edge.node_j = i * 6 + 3;  // To translational
            edge.coupling_strength = 0.5;
            edge.damping = 0.02;
            edge.is_gated = false;
            thermal_edges_.push_back(edge);
        }
    }
    
    // Build gated pathways (phase change at 373K for water)
    if (gated_enabled_) {
        for (size_t b = 0; b < bonds.size(); b++) {
            uint32_t i = bonds[b].first;
            uint32_t j = bonds[b].second;
            
            ThermalEdge edge;
            edge.node_i = i * 6 + 5;  // Gated structural
            edge.node_j = j * 6 + 5;
            edge.coupling_strength = 10.0;  // High when active
            edge.damping = 0.0;
            edge.is_gated = true;
            edge.activation_energy = 40.66e3;  // kJ/mol for water vaporization
            edge.gate_state = 0.0;  // Closed initially
            thermal_edges_.push_back(edge);
        }
    }
}

void ThermalPathwayGraph::simulation_step(double dt) {
    step_1_accumulate_incoming_energy();
    step_2_evaluate_activation_gates();
    step_3_transfer_energy_along_edges(dt);
    step_4_apply_damping_and_losses();
    step_5_promote_coherent_energy();
    step_6_record_observables();
}

void ThermalPathwayGraph::step_1_accumulate_incoming_energy() {
    // External energy sources (radiation, boundaries, etc.)
    // For demo: no external sources
}

void ThermalPathwayGraph::step_2_evaluate_activation_gates() {
    for (auto& edge : thermal_edges_) {
        if (edge.is_gated) {
            // Check temperature at source node
            const auto& node = energy_nodes_[edge.node_i];
            double T = node.temperature();
            
            // Boltzmann activation: P = exp(-Ea / RT)
            // Ea in J/mol, R = 8.314462618 J/(mol·K)
            const double R = 8.314462618;  // J/(mol·K) - Gas constant
            double exponent = -edge.activation_energy / (R * T);
            edge.gate_state = (T > 1.0) ? std::exp(std::max(-50.0, exponent)) : 0.0;
        }
    }
}

void ThermalPathwayGraph::step_3_transfer_energy_along_edges(double dt) {
    std::vector<double> energy_deltas(energy_nodes_.size(), 0.0);
    
    for (const auto& edge : thermal_edges_) {
        // Get temperatures
        double Ti = energy_nodes_[edge.node_i].temperature();
        double Tj = energy_nodes_[edge.node_j].temperature();
        
        // Heat flux: Q = g_ij * (Ti - Tj) * gate_state
        double flux = edge.coupling_strength * (Ti - Tj) * edge.gate_state;
        double energy_transfer = flux * dt;
        
        energy_deltas[edge.node_i] -= energy_transfer;
        energy_deltas[edge.node_j] += energy_transfer;
    }
    
    // Apply deltas
    for (size_t i = 0; i < energy_nodes_.size(); i++) {
        energy_nodes_[i].current_energy += energy_deltas[i];
    }
}

void ThermalPathwayGraph::step_4_apply_damping_and_losses() {
    for (const auto& edge : thermal_edges_) {
        energy_nodes_[edge.node_i].current_energy *= (1.0 - edge.damping);
        energy_nodes_[edge.node_j].current_energy *= (1.0 - edge.damping);
    }
}

void ThermalPathwayGraph::step_5_promote_coherent_energy() {
    // Convert thermal energy to macroscopic motion when coherent
    // (Not implemented in demo)
}

void ThermalPathwayGraph::step_6_record_observables() {
    // Observables are extracted on-demand via measure_* functions
}

double ThermalPathwayGraph::measure_thermal_conductivity() const {
    // k = (heat flux) / (temperature gradient)
    // Placeholder: average coupling strength
    double total_coupling = 0.0;
    for (const auto& edge : thermal_edges_) {
        total_coupling += edge.coupling_strength;
    }
    return total_coupling / std::max(1.0, static_cast<double>(thermal_edges_.size()));
}

double ThermalPathwayGraph::measure_heat_capacity() const {
    double total_capacity = 0.0;
    for (const auto& node : energy_nodes_) {
        total_capacity += node.capacity;
    }
    return total_capacity;
}

double ThermalPathwayGraph::measure_thermal_expansion() const {
    // α = (1/V) * (dV/dT)
    // Placeholder: not implemented
    return 0.0;
}

double ThermalPathwayGraph::compute_coupling(PathwayClass cls, uint32_t i, uint32_t j,
                                             double bond_order, double distance) const {
    switch (cls) {
        case PathwayClass::PHONON_LATTICE:
            return bond_order * 10.0 / (distance * distance);
        case PathwayClass::ELECTRONIC:
            return 50.0;  // Strong for metals
        case PathwayClass::MOLECULAR_ROTATIONAL:
            return 1.0;
        case PathwayClass::TRANSLATIONAL_KINETIC:
            return 0.1 / (distance * distance);
        case PathwayClass::RADIATIVE_MICRO:
            return 0.01;  // Weak
        case PathwayClass::GATED_STRUCTURAL:
            return 20.0;  // Very strong when activated
        default:
            return 1.0;
    }
}

void ThermalPathwayGraph::enable_phonon_pathways(bool enable) { phonon_enabled_ = enable; }
void ThermalPathwayGraph::enable_electronic_pathways(bool enable) { electronic_enabled_ = enable; }
void ThermalPathwayGraph::enable_rotational_pathways(bool enable) { rotational_enabled_ = enable; }
void ThermalPathwayGraph::enable_translational_pathways(bool enable) { translational_enabled_ = enable; }
void ThermalPathwayGraph::enable_radiative_pathways(bool enable) { radiative_enabled_ = enable; }
void ThermalPathwayGraph::enable_gated_pathways(bool enable) { gated_enabled_ = enable; }

void ThermalPathwayGraph::print_pathway_status() const {
    std::cout << "\n=== Thermal Pathway Graph Status ===\n";
    std::cout << "Total Energy Nodes: " << energy_nodes_.size() << "\n";
    std::cout << "Total Thermal Edges: " << thermal_edges_.size() << "\n";
    std::cout << "\nPathway Classes Enabled:\n";
    std::cout << "  Phonon_Lattice: " << (phonon_enabled_ ? "ON" : "OFF") << "\n";
    std::cout << "  Electronic: " << (electronic_enabled_ ? "ON" : "OFF") << "\n";
    std::cout << "  Molecular_Rotational: " << (rotational_enabled_ ? "ON" : "OFF") << "\n";
    std::cout << "  Translational_Kinetic: " << (translational_enabled_ ? "ON" : "OFF") << "\n";
    std::cout << "  Radiative_Micro: " << (radiative_enabled_ ? "ON" : "OFF") << "\n";
    std::cout << "  Gated_Structural: " << (gated_enabled_ ? "ON" : "OFF") << "\n";
    
    double total_energy = 0.0;
    for (const auto& node : energy_nodes_) {
        total_energy += node.current_energy;
    }
    std::cout << "\nTotal System Energy: " << total_energy << " J\n";
    std::cout << "Measured k: " << measure_thermal_conductivity() << " W/(m·K)\n";
    std::cout << "Measured Cp: " << measure_heat_capacity() << " J/K\n";
}

// ============================================================================
// Demo Function
// ============================================================================

void create_demo_xyzc_file(const std::string& filename) {
    std::cout << "\n=== Creating Demo XYZC File: " << filename << " ===\n";
    
    // Create a simple water molecule (H2O)
    const uint32_t num_atoms = 3;
    const uint32_t num_frames = 100;  // 100 timesteps
    const double dt = 1.0;  // 1 fs
    
    // Build pathway graph
    ThermalPathwayGraph graph(num_atoms);
    
    std::vector<std::pair<uint32_t, uint32_t>> bonds = {{0, 1}, {0, 2}};  // O-H bonds
    std::vector<double> bond_orders = {1.0, 1.0};
    std::vector<uint8_t> atomic_numbers = {8, 1, 1};  // O, H, H
    
    graph.build_from_bonds(bonds, bond_orders, atomic_numbers);
    graph.print_pathway_status();
    
    // Create XYZC writer
    XYZCWriter writer(filename);
    
    // Write header
    XYZCHeader header;
    header.num_atoms = num_atoms;
    header.num_frames = num_frames;
    header.num_energy_nodes = static_cast<uint32_t>(graph.get_energy_nodes().size());
    header.num_thermal_edges = static_cast<uint32_t>(graph.get_thermal_edges().size());
    header.dt = dt;
    header.total_time = num_frames * dt;
    header.element_symbols = {"O", "H", "H"};
    
    // Extract pathway topology
    for (const auto& node : graph.get_energy_nodes()) {
        header.node_pathway_classes.push_back(node.pathway_type);
    }
    for (const auto& edge : graph.get_thermal_edges()) {
        header.edge_topology.push_back({edge.node_i, edge.node_j});
    }
    
    writer.write_header(header);
    
    // Simulate and write frames
    for (uint32_t frame = 0; frame < num_frames; frame++) {
        FrameStateVector state;
        state.frame_number = frame;
        state.time = frame * dt;
        
        // Water molecule positions (Ångströms)
        state.positions = {
            {0.0, 0.0, 0.0},     // O
            {0.96, 0.0, 0.0},    // H1
            {-0.24, 0.93, 0.0}   // H2
        };
        
        // Velocities (thermal motion)
        state.velocities = {
            {0.01 * std::sin(frame * 0.1), 0.01 * std::cos(frame * 0.1), 0.0},
            {-0.02 * std::sin(frame * 0.15), 0.01, 0.0},
            {0.01, -0.02 * std::cos(frame * 0.12), 0.0}
        };
        
        // Add some thermal energy
        auto& nodes = graph.get_energy_nodes();
        for (auto& node : nodes) {
            node.current_energy = 300.0 * node.capacity + 10.0 * std::sin(frame * 0.05);
        }
        
        // Run thermal simulation step
        graph.simulation_step(dt);
        
        // Copy energy state
        state.energy_nodes = graph.get_energy_nodes();
        state.active_edges = graph.get_thermal_edges();
        
        // Compute observables
        state.total_energy = 0.0;
        state.thermal_energy = 0.0;
        for (const auto& node : state.energy_nodes) {
            state.thermal_energy += node.current_energy;
        }
        state.total_energy = state.thermal_energy;
        state.global_temperature = state.thermal_energy / graph.measure_heat_capacity();
        state.max_temperature = state.global_temperature * 1.1;
        state.min_temperature = state.global_temperature * 0.9;
        
        writer.write_frame(state);
        
        if (frame % 10 == 0) {
            std::cout << "Frame " << frame << "/" << num_frames 
                     << " | T = " << state.global_temperature << " K\n";
        }
    }
    
    writer.finalize();
    std::cout << "\nDemo complete! Wrote " << writer.frames_written() << " frames.\n";
    std::cout << "File size: " << std::filesystem::file_size(filename) / 1024.0 << " KB\n";
}

}} // namespace vsepr::thermal
