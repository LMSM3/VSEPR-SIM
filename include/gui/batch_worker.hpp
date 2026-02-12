// ============================================================================
// batch_worker.hpp
// Part of VSEPR-Sim: Molecular Geometry Simulation System
// 
// Description:
//   Background batch processing system for generating multiple molecules
//   from build lists. Supports pause/resume, progress tracking, and
//   multiple output formats (XYZ, JSON, CSV).
//
// Features:
//   - Threaded background processing (non-blocking GUI)
//   - Progress callbacks for live UI updates
//   - Pause/resume support
//   - Build list parsing (TXT format: formula per line)
//   - Multi-format export (XYZ, JSON, CSV)
//   - Per-molecule timing and success tracking
//
// Version: 2.3.1
// Author: VSEPR-Sim Development Team
// ============================================================================

#pragma once

#include "sim/molecule.hpp"
#include "dynamic/real_molecule_generator.hpp"
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <mutex>
#include <functional>

namespace vsepr {
namespace gui {

// Single batch build item
struct BatchBuildItem {
    std::string formula;
    std::string output_path;
    bool optimize;
    bool calculate_energy;
    std::string name;  // Optional display name
};

// Batch processing result
struct BatchResult {
    std::string formula;
    std::string output_path;
    int num_atoms;
    double energy;  // kcal/mol
    bool success;
    std::string error_message;
    double time_seconds;
    
    BatchResult() 
        : num_atoms(0), energy(0.0), success(false), time_seconds(0.0) {}
};

// Batch worker for background processing
class BatchWorker {
public:
    BatchWorker();
    ~BatchWorker();
    
    // Control
    void start(const std::vector<BatchBuildItem>& items);
    void start_from_file(const std::string& build_list_path);
    void stop();
    void pause();
    void resume();
    
    // Status
    bool is_running() const { return running_.load(); }
    bool is_paused() const { return paused_.load(); }
    size_t total_count() const { return total_count_; }
    size_t completed_count() const { return completed_.load(); }
    float progress() const;
    
    // Results access
    std::vector<BatchResult> get_results() const;
    BatchResult get_current_result() const;
    Molecule get_current_molecule() const;
    
    // Configuration
    void set_use_gpu(bool use_gpu) { use_gpu_ = use_gpu; }
    void set_num_threads(int num_threads) { num_threads_ = num_threads; }
    void set_output_format(const std::string& format) { output_format_ = format; }
    
    // Callbacks
    using ProgressCallback = std::function<void(size_t completed, size_t total, const BatchResult&)>;
    void set_progress_callback(ProgressCallback callback) { progress_callback_ = callback; }
    
    using CompletionCallback = std::function<void(const std::vector<BatchResult>&)>;
    void set_completion_callback(CompletionCallback callback) { completion_callback_ = callback; }
    
private:
    void worker_thread();
    BatchResult process_single_item(const BatchBuildItem& item);
    std::vector<BatchBuildItem> parse_build_list(const std::string& path);
    void save_molecule(const Molecule& mol, const std::string& path, const std::string& format);
    
    std::thread worker_thread_;
    std::atomic<bool> running_;
    std::atomic<bool> paused_;
    std::atomic<size_t> completed_;
    
    std::vector<BatchBuildItem> build_queue_;
    std::vector<BatchResult> results_;
    mutable std::mutex results_mutex_;
    
    Molecule current_molecule_;
    mutable std::mutex molecule_mutex_;
    
    size_t total_count_;
    bool use_gpu_;
    int num_threads_;
    std::string output_format_;  // "xyz", "json", "csv"
    
    ProgressCallback progress_callback_;
    CompletionCallback completion_callback_;
};

}  // namespace gui
}  // namespace vsepr
