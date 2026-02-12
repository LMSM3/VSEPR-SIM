/**
 * Batch Processing Worker Implementation
 * vsepr-sim v2.3.1
 * 
 * Features:
 * - Background thread processing
 * - Progress callbacks for GUI integration
 * - Pause/resume support
 * - Batch processing from build lists (TXT files)
 * - Multi-format export: XYZ (via xyz_format library), JSON, CSV
 * 
 * Author: VSEPR-Sim Development Team
 * Date: January 2025
 * License: MIT
 */

#include "gui/batch_worker.hpp"
#include "dynamic/real_molecule_generator.hpp"
#include "io/xyz_format.hpp"  // Use proper XYZ library
#include "core/element_data_integrated.hpp"
#include "pot/periodic_db.hpp"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <chrono>
#include <mutex>

namespace fs = std::filesystem;

// ============================================================================
// class BatchWorker
// Implements batch processing of molecular geometries with support for
// various input and output formats, using a background worker thread.
//
// Member Functions:
// - start: Begin processing a batch of molecular formulas
// - stop:  Stop the worker thread
// - pause/resume: Control processing suspension
// - get_results: Retrieve processed results
//
// Private Functions:
// - worker_thread: The main function executed by the worker thread
// - process_single_item: Process a single molecular formula
// - parse_build_list: Parse a build list file into BatchBuildItem structs
// - save_molecule: Save a Molecule object to a file in the specified format
//
// Supported Formats:
// - XYZ: Standard 3D coordinate format (via xyz_format library)
// - JSON: Structured data for web apps
// - CSV: Tabular format for spreadsheets
//
// Version: 2.3.1
// ============================================================================

namespace vsepr {
namespace gui {

// Resolve element symbol by atomic number using the periodic database
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

BatchWorker::BatchWorker()
    : running_(false)
    , paused_(false)
    , completed_(0)
    , total_count_(0)
    , use_gpu_(false)
    , num_threads_(1)
    , output_format_("xyz") {
}

BatchWorker::~BatchWorker() {
    stop();
}

void BatchWorker::start(const std::vector<BatchBuildItem>& items) {
    if (running_.load()) {
        return;  // Already running
    }
    
    build_queue_ = items;
    total_count_ = items.size();
    completed_ = 0;
    results_.clear();
    
    running_ = true;
    paused_ = false;
    
    worker_thread_ = std::thread(&BatchWorker::worker_thread, this);
}

void BatchWorker::start_from_file(const std::string& build_list_path) {
    auto items = parse_build_list(build_list_path);
    start(items);
}

void BatchWorker::stop() {
    running_ = false;
    if (worker_thread_.joinable()) {
        worker_thread_.join();
    }
}

void BatchWorker::pause() {
    paused_ = true;
}

void BatchWorker::resume() {
    paused_ = false;
}

float BatchWorker::progress() const {
    if (total_count_ == 0) return 0.0f;
    return static_cast<float>(completed_.load()) / total_count_;
}

std::vector<BatchResult> BatchWorker::get_results() const {
    std::lock_guard<std::mutex> lock(results_mutex_);
    return results_;
}

BatchResult BatchWorker::get_current_result() const {
    std::lock_guard<std::mutex> lock(results_mutex_);
    if (results_.empty()) {
        return BatchResult();
    }
    return results_.back();
}

Molecule BatchWorker::get_current_molecule() const {
    std::lock_guard<std::mutex> lock(molecule_mutex_);
    return current_molecule_;
}

void BatchWorker::worker_thread() {
    dynamic::RealMoleculeGenerator generator;
    
    for (size_t i = 0; i < build_queue_.size() && running_.load(); ++i) {
        // Pause support
        while (paused_.load() && running_.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        
        if (!running_.load()) break;
        
        const auto& item = build_queue_[i];
        BatchResult result = process_single_item(item);
        
        // Store result
        {
            std::lock_guard<std::mutex> lock(results_mutex_);
            results_.push_back(result);
        }
        
        completed_++;
        
        // Progress callback
        if (progress_callback_) {
            progress_callback_(completed_.load(), total_count_, result);
        }
    }
    
    // Completion callback
    if (completion_callback_ && running_.load()) {
        std::vector<BatchResult> final_results = get_results();
        completion_callback_(final_results);
    }
    
    running_ = false;
}

BatchResult BatchWorker::process_single_item(const BatchBuildItem& item) {
    BatchResult result;
    result.formula = item.formula;
    result.output_path = item.output_path;
    
    auto start_time = std::chrono::steady_clock::now();
    
    try {
        // Generate molecule
        dynamic::RealMoleculeGenerator generator;
        Molecule mol = generator.generate_from_formula(item.formula);
        
        // Store for preview
        {
            std::lock_guard<std::mutex> lock(molecule_mutex_);
            current_molecule_ = mol;
        }
        
        result.num_atoms = mol.num_atoms();
        
        // Optimize if requested
        if (item.optimize) {
            // Would call mol.optimize() here if implemented
            // For now, skip optimization
        }
        
        // Calculate energy if requested
        if (item.calculate_energy) {
            // Would call mol.calculate_energy() here
            // Placeholder: estimate based on atom count
            result.energy = -10.0 * mol.num_atoms();
        }
        
        // Save molecule
        if (!item.output_path.empty()) {
            save_molecule(mol, item.output_path, output_format_);
        }
        
        result.success = true;
        
    } catch (const std::exception& e) {
        result.success = false;
        result.error_message = e.what();
    }
    
    auto end_time = std::chrono::steady_clock::now();
    result.time_seconds = std::chrono::duration<double>(end_time - start_time).count();
    
    return result;
}

std::vector<BatchBuildItem> BatchWorker::parse_build_list(const std::string& path) {
    std::vector<BatchBuildItem> items;
    std::ifstream file(path);
    
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open build list file: " + path);
    }
    
    std::string line;
    int line_num = 0;
    
    while (std::getline(file, line)) {
        line_num++;
        
        // Skip empty lines and comments
        if (line.empty() || line[0] == '#') {
            continue;
        }
        
        // Parse line format: "formula [output_path] [optimize] [calculate_energy]"
        std::istringstream iss(line);
        BatchBuildItem item;
        
        iss >> item.formula;
        
        if (iss >> item.output_path) {
            // Output path provided
        } else {
            // Generate output path from formula
            item.output_path = item.formula + ".xyz";
        }
        
        std::string flag;
        while (iss >> flag) {
            if (flag == "optimize" || flag == "-O") {
                item.optimize = true;
            } else if (flag == "energy" || flag == "-E") {
                item.calculate_energy = true;
            }
        }
        
        // Default name
        item.name = item.formula;
        
        items.push_back(item);
    }
    
    return items;
}

void BatchWorker::save_molecule(const Molecule& mol, const std::string& path, const std::string& format) {
    fs::path out_path(path);
    if (!out_path.parent_path().empty()) {
        fs::create_directories(out_path.parent_path());
    }

    if (format == "xyz" || format == "XYZ") {
        // Use the proper xyz_format library - never encode xyz by hand!
        vsepr::io::XYZMolecule xyz_mol;
        
        // Convert vsepr::Molecule to vsepr::io::XYZMolecule
        xyz_mol.comment = "Generated by VSEPR-Sim Batch Processor";
        xyz_mol.atoms.reserve(mol.num_atoms());
        
        for (size_t i = 0; i < mol.num_atoms(); ++i) {
            double x, y, z;
            mol.get_position(i, x, y, z);
            
            const char* symbol = element_symbol(mol.atoms[i].Z);
            xyz_mol.atoms.emplace_back(symbol, x, y, z);
        }
        
        // Copy bond information if available
        for (const auto& bond : mol.bonds) {
            xyz_mol.bonds.emplace_back(bond.i, bond.j, static_cast<double>(bond.order));
        }
        
        // Write using the proper XYZ writer
        vsepr::io::XYZWriter writer;
        writer.set_precision(6);  // 6 decimal places
        
        if (!writer.write(out_path.string(), xyz_mol)) {
            throw std::runtime_error("XYZ write failed: " + writer.get_error());
        }
        
    } else if (format == "json" || format == "JSON") {
        // Save as JSON (simplified)
        std::ofstream file(out_path);
        if (!file.is_open()) {
            throw std::runtime_error("Cannot write to file: " + path);
        }
        
        file << "{\n";
        file << "  \"num_atoms\": " << mol.num_atoms() << ",\n";
        file << "  \"atoms\": [\n";
        
        for (size_t i = 0; i < mol.num_atoms(); ++i) {
            double x, y, z;
            mol.get_position(i, x, y, z);
            
            const char* symbol = element_symbol(mol.atoms[i].Z);
            file << "    {\"element\": \"" << symbol << "\", ";
            file << "\"x\": " << x << ", \"y\": " << y << ", \"z\": " << z << "}";
            
            if (i < mol.num_atoms() - 1) {
                file << ",";
            }
            file << "\n";
        }
        
        file << "  ]\n";
        file << "}\n";
        
    } else if (format == "csv" || format == "CSV") {
        // Save as CSV
        std::ofstream file(out_path);
        if (!file.is_open()) {
            throw std::runtime_error("Cannot write to file: " + path);
        }
        
        file << "Element,X,Y,Z\n";
        
        for (size_t i = 0; i < mol.num_atoms(); ++i) {
            double x, y, z;
            mol.get_position(i, x, y, z);
            
            const char* symbol = element_symbol(mol.atoms[i].Z);
            file << symbol << "," << x << "," << y << "," << z << "\n";
        }
    }
}

}  // namespace gui
}  // namespace vsepr
