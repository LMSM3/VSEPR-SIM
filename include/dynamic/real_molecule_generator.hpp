// ============================================================================
// real_molecule_generator.hpp
// Part of VSEPR-Sim: Molecular Geometry Simulation System
// 
// Description:
//   Real molecule database and generation system. Provides parametric
//   generators for chemically valid molecules across multiple categories
//   (inorganics, hydrocarbons, functional groups, aromatics, biomolecules).
//   Includes continuous generation for batch processing and ML datasets.
//
// Features:
//   - 50+ molecule template database with formation energies
//   - Parametric generators (alkanes, alkenes, cycloalkanes, alcohols)
//   - Category-based generation (8 categories)
//   - Continuous generation with threading and statistics
//   - Ring buffer for recent molecules (50-molecule window)
//
// Version: 2.3.1
// Author: VSEPR-Sim Development Team
// ============================================================================

#pragma once

#include "sim/molecule.hpp"
#include <vector>
#include <string>
#include <random>
#include <atomic>
#include <thread>
#include <deque>
#include <set>
#include <map>
#include <chrono>
#include <functional>
#include <fstream>

namespace vsepr {
namespace dynamic {

// Real molecule database
struct RealMoleculeTemplate {
    std::string name;
    std::string formula;
    int num_atoms;
    std::vector<std::pair<uint8_t, int>> composition; // (Z, count)
    double typical_energy;  // kcal/mol
};

// Molecule categories
enum class MoleculeCategory {
    SmallInorganic,      // H2O, NH3, CO2, SO2
    Hydrocarbons,        // CH4, C2H6, C6H6
    Alcohols,            // CH3OH, C2H5OH
    OrganicAcids,        // HCOOH, CH3COOH
    Aromatics,           // C6H6, C6H5OH
    Biomolecules,        // Amino acids, sugars
    Drugs,               // Aspirin, caffeine
    All
};

// Real molecule generator with chemistry knowledge
class RealMoleculeGenerator {
public:
    RealMoleculeGenerator();
    
    // Generate single real molecule
    Molecule generate_random_real_molecule();
    Molecule generate_from_category(MoleculeCategory cat);
    Molecule generate_from_formula(const std::string& formula);
    
    // Generate specific molecule types
    Molecule generate_alkane(int n_carbons);        // CnH2n+2
    Molecule generate_alkene(int n_carbons);        // CnH2n
    Molecule generate_cycloalkane(int n_carbons);   // CnH2n
    Molecule generate_alcohol(int n_carbons);       // CnH2n+1OH
    Molecule generate_carboxylic_acid(int n_carbons); // CnH2n+1COOH
    Molecule generate_aromatic(const std::string& type);
    
    // Database queries
    size_t get_template_count() const { return templates_.size(); }
    const RealMoleculeTemplate& get_template(size_t idx) const;
    std::vector<std::string> get_all_formulas() const;
    
private:
    void initialize_database();
    void add_small_inorganics();
    void add_hydrocarbons();
    void add_functional_groups();
    void add_aromatics();
    void add_biomolecules();
    void add_common_drugs();
    
    std::vector<RealMoleculeTemplate> templates_;
    std::mt19937 rng_;
};

// Statistics for continuous generation
struct GenerationStatistics {
    size_t total_generated{0};
    size_t unique_formulas{0};
    double rate_mol_per_sec{0.0};
    double avg_atoms_per_molecule{0.0};
    std::chrono::steady_clock::time_point start_time;
    std::map<std::string, size_t> formula_counts;
    std::map<MoleculeCategory, size_t> category_counts;
};

// Continuous generator with GPU acceleration
class ContinuousRealMoleculeGenerator {
public:
    ContinuousRealMoleculeGenerator();
    ~ContinuousRealMoleculeGenerator();
    
    // Control
    void start(int target_count = 0,  // 0 = infinite
               int checkpoint_every = 1000,
               bool use_gpu = true,
               MoleculeCategory category = MoleculeCategory::All);
    void stop();
    void pause();
    void resume();
    
    // Status
    bool is_running() const { return running_.load(); }
    bool is_paused() const { return paused_.load(); }
    size_t count() const { return stats_.total_generated; }
    size_t unique_formulas() const { return stats_.unique_formulas; }
    double rate() const { return stats_.rate_mol_per_sec; }
    
    // Data access
    std::vector<Molecule> recent_molecules(size_t count) const;
    Molecule get_molecule(size_t index) const;
    const GenerationStatistics& statistics() const { return stats_; }
    
    // Output
    void set_output_stream(const std::string& xyz_path);
    void set_checkpoint_callback(std::function<void(const GenerationStatistics&)> callback);
    
    // GPU control
    bool is_gpu_available() const;
    void enable_gpu(bool enable);
    
private:
    void generation_thread();
    void gpu_batch_generate(size_t batch_size);
    void cpu_batch_generate(size_t batch_size);
    void save_checkpoint();
    void update_statistics();
    
    RealMoleculeGenerator generator_;
    
    std::thread worker_thread_;
    std::atomic<bool> running_{false};
    std::atomic<bool> paused_{false};
    std::atomic<bool> use_gpu_{true};
    
    int target_count_;
    int checkpoint_every_;
    MoleculeCategory category_;
    
    std::deque<Molecule> recent_buffer_;  // Ring buffer (last 100)
    mutable std::mutex buffer_mutex_;
    
    GenerationStatistics stats_;
    std::mutex stats_mutex_;
    
    std::ofstream xyz_stream_;
    std::string output_path_;
    std::function<void(const GenerationStatistics&)> checkpoint_callback_;
};

}} // namespace vsepr::dynamic
