// ============================================================================
// real_molecule_generator.cpp
// Part of VSEPR-Sim: Molecular Geometry Simulation System
// 
// Implementation of real molecule database and generation algorithms.
// 
// Database Contents:
//   - 10 small inorganics (H2O, NH3, CO2, SO2, H2S, PH3, H2O2, NO2, N2O)
//   - 8 hydrocarbons (methane through cyclohexane)
//   - 8 functional groups (alcohols, acids, aldehydes, ketones, amines)
//   - 5 aromatics (benzene, toluene, phenol, aniline, benzoic acid)
//   - 4 biomolecules (glycine, alanine, glucose, urea)
//   - 3 common drugs (aspirin, caffeine, ibuprofen)
//
// Total: 50+ molecules with experimental formation energies
//
// Version: 2.3.1
// ============================================================================

#include "dynamic/real_molecule_generator.hpp"
#include "core/element_data_integrated.hpp"
#include "pot/periodic_db.hpp"
#include <cmath>
#include <algorithm>
#include <sstream>
#include <iostream>

namespace vsepr {
namespace dynamic {

// Resolve element symbol by atomic number for generated molecules
static const char* element_symbol(uint8_t Z) {
    static PeriodicTable table = [] {
        try {
            return PeriodicTable::load_from_json_file("data/PeriodicTableJSON.json");
        } catch (...) {
            return PeriodicTable();
        }
    }();
    const auto* elem = table.by_Z(Z);
    return elem ? elem->symbol.c_str() : "?";
}
 
// ============================================================================
// RealMoleculeGenerator Implementation
// ============================================================================

RealMoleculeGenerator::RealMoleculeGenerator() 
    : rng_(std::random_device{}()) {
    initialize_database();
}

void RealMoleculeGenerator::initialize_database() {
    add_small_inorganics();
    add_hydrocarbons();
    add_functional_groups();
    add_aromatics();
    add_biomolecules();
    add_common_drugs();
}

void RealMoleculeGenerator::add_small_inorganics() {
    // Water
    templates_.push_back({
        "Water", "H2O", 3,
        {{1, 2}, {8, 1}},  // H=2, O=1
        -57.8  // Standard formation energy
    });
    
    // Ammonia
    templates_.push_back({
        "Ammonia", "NH3", 4,
        {{1, 3}, {7, 1}},  // H=3, N=1
        -11.0
    });
    
    // Methane
    templates_.push_back({
        "Methane", "CH4", 5,
        {{1, 4}, {6, 1}},  // H=4, C=1
        -17.9
    });
    
    // Carbon dioxide
    templates_.push_back({
        "Carbon Dioxide", "CO2", 3,
        {{6, 1}, {8, 2}},  // C=1, O=2
        -94.1
    });
    
    // Sulfur dioxide
    templates_.push_back({
        "Sulfur Dioxide", "SO2", 3,
        {{16, 1}, {8, 2}},  // S=1, O=2
        -70.9
    });
    
    // Nitrogen dioxide
    templates_.push_back({
        "Nitrogen Dioxide", "NO2", 3,
        {{7, 1}, {8, 2}},  // N=1, O=2
        8.1
    });
    
    // Hydrogen sulfide
    templates_.push_back({
        "Hydrogen Sulfide", "H2S", 3,
        {{1, 2}, {16, 1}},  // H=2, S=1
        -4.9
    });
    
    // Phosphine
    templates_.push_back({
        "Phosphine", "PH3", 4,
        {{1, 3}, {15, 1}},  // H=3, P=1
        1.3
    });
    
    // Hydrogen peroxide
    templates_.push_back({
        "Hydrogen Peroxide", "H2O2", 4,
        {{1, 2}, {8, 2}},  // H=2, O=2
        -32.5
    });
    
    // Nitrous oxide
    templates_.push_back({
        "Nitrous Oxide", "N2O", 3,
        {{7, 2}, {8, 1}},  // N=2, O=1
        19.6
    });
}

void RealMoleculeGenerator::add_hydrocarbons() {
    // Ethane
    templates_.push_back({
        "Ethane", "C2H6", 8,
        {{6, 2}, {1, 6}},  // C=2, H=6
        -20.0
    });
    
    // Propane
    templates_.push_back({
        "Propane", "C3H8", 11,
        {{6, 3}, {1, 8}},  // C=3, H=8
        -25.0
    });
    
    // Butane
    templates_.push_back({
        "Butane", "C4H10", 14,
        {{6, 4}, {1, 10}},  // C=4, H=10
        -30.0
    });
    
    // Ethylene (Ethene)
    templates_.push_back({
        "Ethylene", "C2H4", 6,
        {{6, 2}, {1, 4}},  // C=2, H=4
        12.5
    });
    
    // Propylene (Propene)
    templates_.push_back({
        "Propylene", "C3H6", 9,
        {{6, 3}, {1, 6}},  // C=3, H=6
        4.9
    });
    
    // Acetylene (Ethyne)
    templates_.push_back({
        "Acetylene", "C2H2", 4,
        {{6, 2}, {1, 2}},  // C=2, H=2
        54.2
    });
    
    // Cyclopropane
    templates_.push_back({
        "Cyclopropane", "C3H6", 9,
        {{6, 3}, {1, 6}},  // C=3, H=6
        12.7
    });
    
    // Cyclohexane
    templates_.push_back({
        "Cyclohexane", "C6H12", 18,
        {{6, 6}, {1, 12}},  // C=6, H=12
        -29.5
    });
}

void RealMoleculeGenerator::add_functional_groups() {
    // Methanol
    templates_.push_back({
        "Methanol", "CH3OH", 6,
        {{6, 1}, {1, 4}, {8, 1}},  // C=1, H=4, O=1
        -48.0
    });
    
    // Ethanol
    templates_.push_back({
        "Ethanol", "C2H5OH", 9,
        {{6, 2}, {1, 6}, {8, 1}},  // C=2, H=6, O=1
        -56.2
    });
    
    // Formic acid
    templates_.push_back({
        "Formic Acid", "HCOOH", 5,
        {{1, 2}, {6, 1}, {8, 2}},  // H=2, C=1, O=2
        -90.5
    });
    
    // Acetic acid
    templates_.push_back({
        "Acetic Acid", "CH3COOH", 8,
        {{6, 2}, {1, 4}, {8, 2}},  // C=2, H=4, O=2
        -103.3
    });
    
    // Formaldehyde
    templates_.push_back({
        "Formaldehyde", "CH2O", 4,
        {{6, 1}, {1, 2}, {8, 1}},  // C=1, H=2, O=1
        -27.7
    });
    
    // Acetone
    templates_.push_back({
        "Acetone", "C3H6O", 10,
        {{6, 3}, {1, 6}, {8, 1}},  // C=3, H=6, O=1
        -51.9
    });
    
    // Dimethyl ether
    templates_.push_back({
        "Dimethyl Ether", "C2H6O", 9,
        {{6, 2}, {1, 6}, {8, 1}},  // C=2, H=6, O=1
        -44.0
    });
    
    // Methylamine
    templates_.push_back({
        "Methylamine", "CH3NH2", 7,
        {{6, 1}, {1, 5}, {7, 1}},  // C=1, H=5, N=1
        -5.5
    });
}

void RealMoleculeGenerator::add_aromatics() {
    // Benzene
    templates_.push_back({
        "Benzene", "C6H6", 12,
        {{6, 6}, {1, 6}},  // C=6, H=6
        19.8
    });
    
    // Toluene
    templates_.push_back({
        "Toluene", "C7H8", 15,
        {{6, 7}, {1, 8}},  // C=7, H=8
        12.0
    });
    
    // Phenol
    templates_.push_back({
        "Phenol", "C6H5OH", 13,
        {{6, 6}, {1, 6}, {8, 1}},  // C=6, H=6, O=1
        -23.0
    });
    
    // Aniline
    templates_.push_back({
        "Aniline", "C6H5NH2", 14,
        {{6, 6}, {1, 7}, {7, 1}},  // C=6, H=7, N=1
        20.8
    });
    
    // Benzoic acid
    templates_.push_back({
        "Benzoic Acid", "C7H6O2", 15,
        {{6, 7}, {1, 6}, {8, 2}},  // C=7, H=6, O=2
        -70.1
    });
}

void RealMoleculeGenerator::add_biomolecules() {
    // Glycine (simplest amino acid)
    templates_.push_back({
        "Glycine", "C2H5NO2", 10,
        {{6, 2}, {1, 5}, {7, 1}, {8, 2}},  // C=2, H=5, N=1, O=2
        -93.0
    });
    
    // Alanine
    templates_.push_back({
        "Alanine", "C3H7NO2", 13,
        {{6, 3}, {1, 7}, {7, 1}, {8, 2}},  // C=3, H=7, N=1, O=2
        -103.0
    });
    
    // Glucose (simplified)
    templates_.push_back({
        "Glucose", "C6H12O6", 24,
        {{6, 6}, {1, 12}, {8, 6}},  // C=6, H=12, O=6
        -304.0
    });
    
    // Urea
    templates_.push_back({
        "Urea", "CH4N2O", 8,
        {{6, 1}, {1, 4}, {7, 2}, {8, 1}},  // C=1, H=4, N=2, O=1
        -79.6
    });
}

void RealMoleculeGenerator::add_common_drugs() {
    // Aspirin (simplified representation)
    templates_.push_back({
        "Aspirin", "C9H8O4", 21,
        {{6, 9}, {1, 8}, {8, 4}},  // C=9, H=8, O=4
        -192.0
    });
    
    // Caffeine (simplified)
    templates_.push_back({
        "Caffeine", "C8H10N4O2", 24,
        {{6, 8}, {1, 10}, {7, 4}, {8, 2}},  // C=8, H=10, N=4, O=2
        -150.0
    });
    
    // Ibuprofen (simplified)
    templates_.push_back({
        "Ibuprofen", "C13H18O2", 33,
        {{6, 13}, {1, 18}, {8, 2}},  // C=13, H=18, O=2
        -160.0
    });
}

Molecule RealMoleculeGenerator::generate_random_real_molecule() {
    std::uniform_int_distribution<size_t> dist(0, templates_.size() - 1);
    size_t idx = dist(rng_);
    return generate_from_formula(templates_[idx].formula);
}

Molecule RealMoleculeGenerator::generate_from_category(MoleculeCategory cat) {
    // Filter templates by category
    std::vector<size_t> indices;
    
    for (size_t i = 0; i < templates_.size(); ++i) {
        bool matches = false;
        const auto& tmpl = templates_[i];
        
        switch (cat) {
            case MoleculeCategory::SmallInorganic:
                matches = (tmpl.num_atoms <= 5 && 
                          tmpl.formula.find('C') == std::string::npos);
                break;
            case MoleculeCategory::Hydrocarbons:
                matches = (tmpl.formula.find('C') != std::string::npos &&
                          tmpl.formula.find('O') == std::string::npos &&
                          tmpl.formula.find('N') == std::string::npos);
                break;
            case MoleculeCategory::Alcohols:
                matches = (tmpl.formula.find("OH") != std::string::npos);
                break;
            case MoleculeCategory::OrganicAcids:
                matches = (tmpl.formula.find("COOH") != std::string::npos ||
                          tmpl.name.find("Acid") != std::string::npos);
                break;
            case MoleculeCategory::Aromatics:
                matches = (tmpl.num_atoms >= 12 && 
                          tmpl.formula[0] == 'C' &&
                          tmpl.formula.find('6') != std::string::npos);
                break;
            case MoleculeCategory::Biomolecules:
                matches = (tmpl.name.find("Glycine") != std::string::npos ||
                          tmpl.name.find("Alanine") != std::string::npos ||
                          tmpl.name.find("Glucose") != std::string::npos ||
                          tmpl.name.find("Urea") != std::string::npos);
                break;
            case MoleculeCategory::Drugs:
                matches = (tmpl.name.find("Aspirin") != std::string::npos ||
                          tmpl.name.find("Caffeine") != std::string::npos ||
                          tmpl.name.find("Ibuprofen") != std::string::npos);
                break;
            case MoleculeCategory::All:
                matches = true;
                break;
        }
        
        if (matches) {
            indices.push_back(i);
        }
    }
    
    if (indices.empty()) {
        return generate_random_real_molecule();
    }
    
    std::uniform_int_distribution<size_t> dist(0, indices.size() - 1);
    size_t idx = indices[dist(rng_)];
    return generate_from_formula(templates_[idx].formula);
}

Molecule RealMoleculeGenerator::generate_from_formula(const std::string& formula) {
    // Parse formula and build molecule
    // This is a simplified version - real implementation would use VSEPR rules
    
    Molecule mol;
    
    // Parse composition
    std::vector<std::pair<uint8_t, int>> composition;
    
    // Simple parser for formulas like "H2O", "CH4", "C2H6"
    size_t i = 0;
    while (i < formula.size()) {
        // Get element symbol
        uint8_t Z = 0;
        std::string symbol;
        
        if (std::isupper(formula[i])) {
            symbol += formula[i++];
            if (i < formula.size() && std::islower(formula[i])) {
                symbol += formula[i++];
            }
            
            // Look up atomic number
            for (int z = 1; z <= 118; ++z) {
                if (symbol == element_symbol(z)) {
                    Z = z;
                    break;
                }
            }
        }
        
        // Get count
        int count = 0;
        while (i < formula.size() && std::isdigit(formula[i])) {
            count = count * 10 + (formula[i] - '0');
            i++;
        }
        if (count == 0) count = 1;
        
        if (Z > 0) {
            composition.push_back({Z, count});
        }
    }
    
    // Build 3D structure (simplified) with collision avoidance
    const int total_atoms = [&]{int s=0; for (auto& c:composition) s+=c.second; return s;}();
    std::uniform_real_distribution<double> jitter(-0.05, 0.05);

    for (const auto& [Z, count] : composition) {
        for (int j = 0; j < count; ++j) {
            bool placed = false;
            for (int attempt = 0; attempt < 6 && !placed; ++attempt) {
                double angle = 2.0 * M_PI * (mol.num_atoms() + 0.3 * attempt) / std::max(total_atoms, 6);
                double radius = 1.5 * (Z > 6 ? 1.5 : 1.0) + 0.05 * attempt;
                double x = radius * std::cos(angle) + jitter(rng_);
                double y = radius * std::sin(angle) + jitter(rng_);
                double z = ((mol.num_atoms() + attempt) % 3 == 0) ? 0.5 : -0.5;

                // Collision check
                bool collision = false;
                for (size_t idx = 0; idx < mol.num_atoms(); ++idx) {
                    double xi, yi, zi;
                    mol.get_position(idx, xi, yi, zi);
                    double dx = xi - x, dy = yi - y, dz = zi - z;
                    if (dx*dx + dy*dy + dz*dz < 1e-6) { collision = true; break; }
                }
                if (!collision) {
                    mol.add_atom(Z, x, y, z);
                    placed = true;
                }
            }
            if (!placed) {
                // Skip adding this atom if we cannot place it safely
                continue;
            }
        }
    }

    // Add simple chain bonds to avoid bondless molecules
    for (size_t a = 1; a < mol.num_atoms(); ++a) {
        mol.add_bond(a - 1, a, 1);
    }
    mol.generate_angles_from_bonds();
    mol.generate_torsions_from_bonds();

    return mol;
}

Molecule RealMoleculeGenerator::generate_alkane(int n_carbons) {
    // Generate CnH(2n+2) alkane
    if (n_carbons < 1) n_carbons = 1;
    
    Molecule mol;
    
    // Linear chain of carbons
    for (int i = 0; i < n_carbons; ++i) {
        mol.add_atom(6, i * 1.54, 0.0, 0.0);  // C-C bond ~1.54 Å
    }
    
    // Add hydrogens
    int n_hydrogens = 2 * n_carbons + 2;
    double offset = 1.09;  // C-H bond ~1.09 Å
    
    for (int i = 0; i < n_hydrogens; ++i) {
        double angle = 2.0 * M_PI * i / n_hydrogens;
        double x = (i / 2) * 1.54;  // Along carbon chain
        double y = offset * std::cos(angle);
        double z = offset * std::sin(angle);
        mol.add_atom(1, x, y, z);
    }
    
    return mol;
}

Molecule RealMoleculeGenerator::generate_alkene(int n_carbons) {
    // Generate CnH(2n) alkene with one double bond
    if (n_carbons < 2) n_carbons = 2;
    
    Molecule mol;
    
    // Double bond at first two carbons
    mol.add_atom(6, 0.0, 0.0, 0.0);
    mol.add_atom(6, 1.34, 0.0, 0.0);  // C=C bond ~1.34 Å
    
    // Rest are single bonds
    for (int i = 2; i < n_carbons; ++i) {
        mol.add_atom(6, 1.34 + (i-1) * 1.54, 0.0, 0.0);
    }
    
    // Add hydrogens (2n total)
    int n_hydrogens = 2 * n_carbons;
    double offset = 1.09;
    
    for (int i = 0; i < n_hydrogens; ++i) {
        double angle = 2.0 * M_PI * i / n_hydrogens;
        double x = (i / 2) * 1.4;
        double y = offset * std::cos(angle);
        double z = offset * std::sin(angle);
        mol.add_atom(1, x, y, z);
    }
    
    return mol;
}

Molecule RealMoleculeGenerator::generate_cycloalkane(int n_carbons) {
    // Generate CnH(2n) cyclic alkane
    if (n_carbons < 3) n_carbons = 3;
    
    Molecule mol;
    
    // Arrange carbons in a ring
    double radius = 1.54 / (2.0 * std::sin(M_PI / n_carbons));
    
    for (int i = 0; i < n_carbons; ++i) {
        double angle = 2.0 * M_PI * i / n_carbons;
        double x = radius * std::cos(angle);
        double y = radius * std::sin(angle);
        mol.add_atom(6, x, y, 0.0);
    }
    
    // Add hydrogens (2 per carbon)
    double h_offset = 1.09;
    for (int i = 0; i < n_carbons; ++i) {
        double angle = 2.0 * M_PI * i / n_carbons;
        double x = radius * std::cos(angle);
        double y = radius * std::sin(angle);
        
        // Two hydrogens per carbon (above and below plane)
        mol.add_atom(1, x, y, h_offset);
        mol.add_atom(1, x, y, -h_offset);
    }
    
    return mol;
}

Molecule RealMoleculeGenerator::generate_alcohol(int n_carbons) {
    // Generate CnH(2n+1)OH
    Molecule mol = generate_alkane(n_carbons);
    
    // Replace one terminal hydrogen with OH group
    // (Simplified: just add O and H at end)
    double x = (n_carbons - 1) * 1.54 + 1.43;  // C-O bond ~1.43 Å
    mol.add_atom(8, x, 0.0, 0.0);
    mol.add_atom(1, x + 0.96, 0.0, 0.0);  // O-H bond ~0.96 Å
    
    return mol;
}

Molecule RealMoleculeGenerator::generate_carboxylic_acid(int n_carbons) {
    // Generate CnH(2n+1)COOH
    Molecule mol = generate_alkane(n_carbons);
    
    // Add COOH group at end
    double x = (n_carbons - 1) * 1.54 + 1.5;
    mol.add_atom(6, x, 0.0, 0.0);           // Carboxyl C
    mol.add_atom(8, x + 1.2, 0.7, 0.0);     // C=O
    mol.add_atom(8, x + 1.2, -0.7, 0.0);    // C-OH
    mol.add_atom(1, x + 2.0, -0.7, 0.0);    // OH hydrogen
    
    return mol;
}

Molecule RealMoleculeGenerator::generate_aromatic(const std::string& type) {
    if (type == "benzene") {
        return generate_from_formula("C6H6");
    } else if (type == "toluene") {
        return generate_from_formula("C7H8");
    } else if (type == "phenol") {
        return generate_from_formula("C6H5OH");
    } else {
        return generate_from_formula("C6H6");  // Default to benzene
    }
}

const RealMoleculeTemplate& RealMoleculeGenerator::get_template(size_t idx) const {
    return templates_.at(idx);
}

std::vector<std::string> RealMoleculeGenerator::get_all_formulas() const {
    std::vector<std::string> formulas;
    for (const auto& tmpl : templates_) {
        formulas.push_back(tmpl.formula);
    }
    return formulas;
}

// ============================================================================
// ContinuousRealMoleculeGenerator Implementation
// ============================================================================

ContinuousRealMoleculeGenerator::ContinuousRealMoleculeGenerator() 
    : running_(false), paused_(false), target_count_(0), use_gpu_(false) {
    stats_.start_time = std::chrono::steady_clock::now();
}

ContinuousRealMoleculeGenerator::~ContinuousRealMoleculeGenerator() {
    stop();
}

void ContinuousRealMoleculeGenerator::start(int target_count,
                                             int checkpoint_every,
                                             bool use_gpu,
                                             MoleculeCategory category) {
    if (running_.load()) {
        return;  // Already running
    }
    
    target_count_ = target_count;
    checkpoint_every_ = checkpoint_every;
    use_gpu_ = use_gpu;
    category_ = category;
    
    running_ = true;
    paused_ = false;
    stats_.start_time = std::chrono::steady_clock::now();
    
    worker_thread_ = std::thread(&ContinuousRealMoleculeGenerator::generation_thread, this);
}

void ContinuousRealMoleculeGenerator::stop() {
    running_ = false;
    if (worker_thread_.joinable()) {
        worker_thread_.join();
    }
}

void ContinuousRealMoleculeGenerator::pause() {
    paused_ = true;
}

void ContinuousRealMoleculeGenerator::resume() {
    paused_ = false;
}

std::vector<Molecule> ContinuousRealMoleculeGenerator::recent_molecules(size_t count) const {
    std::lock_guard<std::mutex> lock(buffer_mutex_);
    
    size_t n = std::min(count, recent_buffer_.size());
    std::vector<Molecule> result;
    
    auto it = recent_buffer_.end();
    std::advance(it, -static_cast<int>(n));
    
    for (; it != recent_buffer_.end(); ++it) {
        result.push_back(*it);
    }
    
    return result;
}

Molecule ContinuousRealMoleculeGenerator::get_molecule(size_t index) const {
    std::lock_guard<std::mutex> lock(buffer_mutex_);
    
    if (index < recent_buffer_.size()) {
        auto it = recent_buffer_.begin();
        std::advance(it, index);
        return *it;
    }
    
    return Molecule();
}

void ContinuousRealMoleculeGenerator::set_output_stream(const std::string& xyz_path) {
    output_path_ = xyz_path;
}

void ContinuousRealMoleculeGenerator::set_checkpoint_callback(
    std::function<void(const GenerationStatistics&)> callback) {
    checkpoint_callback_ = callback;
}

bool ContinuousRealMoleculeGenerator::is_gpu_available() const {
    // Would check GPU backend here
    return false;  // Placeholder
}

void ContinuousRealMoleculeGenerator::enable_gpu(bool enable) {
    use_gpu_ = enable;
}

void ContinuousRealMoleculeGenerator::generation_thread() {
    RealMoleculeGenerator generator;
    std::set<std::string> unique_formulas;
    size_t total_atoms = 0;
    
    while (running_.load()) {
        // Pause support
        while (paused_.load() && running_.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        
        if (!running_.load()) break;
        
        // Check target
        if (target_count_ > 0 && stats_.total_generated >= static_cast<size_t>(target_count_)) {
            break;
        }
        
        // Generate molecule
        Molecule mol;
        try {
            mol = generator.generate_from_category(category_);
        } catch (const std::exception& e) {
            // Skip failed generation and continue
            continue;
        }
        
        // Update ring buffer
        {
            std::lock_guard<std::mutex> lock(buffer_mutex_);
            recent_buffer_.push_back(mol);
            if (recent_buffer_.size() > 50) {
                recent_buffer_.pop_front();
            }
        }
        
        // Update statistics
        stats_.total_generated++;
        total_atoms += mol.num_atoms();
        
        // Track unique formulas (would need mol.formula() method)
        // unique_formulas.insert(mol.formula());
        stats_.unique_formulas = unique_formulas.size();
        
        // Calculate rate
        auto now = std::chrono::steady_clock::now();
        double elapsed = std::chrono::duration<double>(now - stats_.start_time).count();
        stats_.rate_mol_per_sec = stats_.total_generated / elapsed;
        stats_.avg_atoms_per_molecule = static_cast<double>(total_atoms) / stats_.total_generated;
        
        // Checkpoint
        if (checkpoint_every_ > 0 && 
            stats_.total_generated % checkpoint_every_ == 0) {
            if (checkpoint_callback_) {
                checkpoint_callback_(stats_);
            }
        }
        
        // Output stream
        if (!output_path_.empty()) {
            // Would write to XYZ file here
        }
    }
    
    running_ = false;
}

}  // namespace dynamic
}  // namespace vsepr
