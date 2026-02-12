#pragma once
/**
 * gpu_resource_manager.hpp
 * 
 * GPU Resource Manager for Multiscale Simulations
 * Ensures only one scale (Molecular or Physical/FEA) is active on GPU at a time
 * 
 * CRITICAL FEATURE: Prevents GPU resource conflicts between scales
 * 
 * Date: January 18, 2026
 */

#include <memory>
#include <string>
#include <atomic>
#include <mutex>
#include <chrono>
#include <iostream>

namespace vsepr {
namespace multiscale {

// ============================================================================
// GPU Scale Types
// ============================================================================

enum class GPUScaleType {
    NONE,               // No scale active on GPU
    MOLECULAR,          // Molecular dynamics scale (VSEPR-Sim)
    QUANTUM,            // Quantum mechanics scale (future)
    PHYSICAL_FEA        // Physical/continuum scale (FEA)
};

// ============================================================================
// GPU Resource State
// ============================================================================

struct GPUResourceState {
    GPUScaleType active_scale = GPUScaleType::NONE;
    std::string scale_name;
    void* context_handle = nullptr;  // OpenGL/CUDA context
    size_t gpu_memory_bytes = 0;
    std::chrono::steady_clock::time_point activation_time;
    bool is_confirmed = false;
    
    std::string to_string() const {
        switch (active_scale) {
            case GPUScaleType::NONE: return "NONE";
            case GPUScaleType::MOLECULAR: return "MOLECULAR";
            case GPUScaleType::QUANTUM: return "QUANTUM";
            case GPUScaleType::PHYSICAL_FEA: return "PHYSICAL_FEA";
            default: return "UNKNOWN";
        }
    }
};

// ============================================================================
// GPU Resource Manager (Singleton)
// ============================================================================

class GPUResourceManager {
private:
    static std::unique_ptr<GPUResourceManager> instance_;
    static std::mutex instance_mutex_;
    
    GPUResourceState state_;
    std::mutex state_mutex_;
    std::atomic<bool> transition_in_progress_{false};
    
    // Private constructor (singleton)
    GPUResourceManager() = default;
    
public:
    // No copy/move
    GPUResourceManager(const GPUResourceManager&) = delete;
    GPUResourceManager& operator=(const GPUResourceManager&) = delete;
    
    /**
     * Get singleton instance
     */
    static GPUResourceManager& instance() {
        std::lock_guard<std::mutex> lock(instance_mutex_);
        if (!instance_) {
            instance_.reset(new GPUResourceManager());
        }
        return *instance_;
    }
    
    /**
     * Get current GPU resource state
     */
    GPUResourceState get_state() const {
        std::lock_guard<std::mutex> lock(state_mutex_);
        return state_;
    }
    
    /**
     * Check if GPU is available (no scale active)
     */
    bool is_gpu_available() const {
        std::lock_guard<std::mutex> lock(state_mutex_);
        return state_.active_scale == GPUScaleType::NONE;
    }
    
    /**
     * Check if a specific scale is active
     */
    bool is_scale_active(GPUScaleType scale) const {
        std::lock_guard<std::mutex> lock(state_mutex_);
        return state_.active_scale == scale;
    }
    
    /**
     * Request GPU activation for a scale
     * Returns true if granted, false if another scale is active
     */
    bool request_activation(GPUScaleType scale, const std::string& name, void* context = nullptr) {
        std::lock_guard<std::mutex> lock(state_mutex_);
        
        // Check if already active
        if (state_.active_scale == scale) {
            std::cout << "[GPU] Scale " << state_.to_string() << " already active\n";
            return true;
        }
        
        // Check if another scale is active
        if (state_.active_scale != GPUScaleType::NONE) {
            std::cerr << "[GPU ERROR] Cannot activate " << name << "\n";
            std::cerr << "[GPU ERROR] Scale " << state_.to_string() 
                      << " is currently active on GPU\n";
            std::cerr << "[GPU ERROR] Please deactivate " << state_.scale_name 
                      << " first using deactivate_scale()\n";
            return false;
        }
        
        // Grant activation
        state_.active_scale = scale;
        state_.scale_name = name;
        state_.context_handle = context;
        state_.activation_time = std::chrono::steady_clock::now();
        state_.is_confirmed = false;
        
        std::cout << "[GPU] Activation requested: " << name << " (" << state_.to_string() << ")\n";
        std::cout << "[GPU] Waiting for user confirmation...\n";
        
        return true;
    }
    
    /**
     * Confirm GPU activation (user must explicitly confirm)
     * CRITICAL: Prevents accidental dual activation
     */
    bool confirm_activation(GPUScaleType scale) {
        std::lock_guard<std::mutex> lock(state_mutex_);
        
        if (state_.active_scale != scale) {
            std::cerr << "[GPU ERROR] Cannot confirm activation for " 
                      << static_cast<int>(scale) << "\n";
            std::cerr << "[GPU ERROR] Current scale: " << state_.to_string() << "\n";
            return false;
        }
        
        if (state_.is_confirmed) {
            std::cout << "[GPU] Scale " << state_.scale_name << " already confirmed\n";
            return true;
        }
        
        state_.is_confirmed = true;
        
        std::cout << "\n";
        std::cout << "╔═══════════════════════════════════════════════════════════╗\n";
        std::cout << "║  GPU RESOURCE ACTIVATION CONFIRMED                        ║\n";
        std::cout << "╠═══════════════════════════════════════════════════════════╣\n";
        std::cout << "║  Scale:   " << std::left << std::setw(48) << state_.scale_name << "║\n";
        std::cout << "║  Type:    " << std::left << std::setw(48) << state_.to_string() << "║\n";
        std::cout << "║  Status:  ACTIVE ON GPU                                   ║\n";
        std::cout << "╚═══════════════════════════════════════════════════════════╝\n";
        std::cout << "\n";
        
        return true;
    }
    
    /**
     * Deactivate current GPU scale
     */
    void deactivate_scale() {
        std::lock_guard<std::mutex> lock(state_mutex_);
        
        if (state_.active_scale == GPUScaleType::NONE) {
            std::cout << "[GPU] No scale to deactivate\n";
            return;
        }
        
        auto duration = std::chrono::steady_clock::now() - state_.activation_time;
        auto seconds = std::chrono::duration<double>(duration).count();
        
        std::cout << "\n";
        std::cout << "╔═══════════════════════════════════════════════════════════╗\n";
        std::cout << "║  GPU RESOURCE DEACTIVATION                                ║\n";
        std::cout << "╠═══════════════════════════════════════════════════════════╣\n";
        std::cout << "║  Scale:   " << std::left << std::setw(48) << state_.scale_name << "║\n";
        std::cout << "║  Type:    " << std::left << std::setw(48) << state_.to_string() << "║\n";
        std::cout << "║  Active:  " << std::left << std::setw(48) 
                  << (std::to_string((int)seconds) + " seconds") << "║\n";
        std::cout << "║  Status:  GPU NOW AVAILABLE                               ║\n";
        std::cout << "╚═══════════════════════════════════════════════════════════╝\n";
        std::cout << "\n";
        
        // Reset state
        state_.active_scale = GPUScaleType::NONE;
        state_.scale_name.clear();
        state_.context_handle = nullptr;
        state_.gpu_memory_bytes = 0;
        state_.is_confirmed = false;
    }
    
    /**
     * Request scale transition (requires confirmation)
     */
    bool request_transition(GPUScaleType from_scale, GPUScaleType to_scale, 
                           const std::string& to_name) {
        std::lock_guard<std::mutex> lock(state_mutex_);
        
        if (state_.active_scale != from_scale) {
            std::cerr << "[GPU ERROR] Cannot transition from " << static_cast<int>(from_scale) 
                      << " (current: " << state_.to_string() << ")\n";
            return false;
        }
        
        if (transition_in_progress_) {
            std::cerr << "[GPU ERROR] Transition already in progress\n";
            return false;
        }
        
        std::cout << "\n";
        std::cout << "╔═══════════════════════════════════════════════════════════╗\n";
        std::cout << "║  GPU SCALE TRANSITION REQUESTED                           ║\n";
        std::cout << "╠═══════════════════════════════════════════════════════════╣\n";
        std::cout << "║  FROM:    " << std::left << std::setw(48) << state_.scale_name << "║\n";
        std::cout << "║  TO:      " << std::left << std::setw(48) << to_name << "║\n";
        std::cout << "╠═══════════════════════════════════════════════════════════╣\n";
        std::cout << "║  ACTION REQUIRED:                                         ║\n";
        std::cout << "║  1. Call deactivate_scale() to release current resources ║\n";
        std::cout << "║  2. Call request_activation() for new scale              ║\n";
        std::cout << "║  3. Call confirm_activation() to confirm                 ║\n";
        std::cout << "╚═══════════════════════════════════════════════════════════╝\n";
        std::cout << "\n";
        
        transition_in_progress_ = true;
        return true;
    }
    
    /**
     * Complete transition
     */
    void complete_transition() {
        transition_in_progress_ = false;
    }
    
    /**
     * Update GPU memory usage
     */
    void update_memory(size_t bytes) {
        std::lock_guard<std::mutex> lock(state_mutex_);
        state_.gpu_memory_bytes = bytes;
    }
    
    /**
     * Print current GPU status
     */
    void print_status() const {
        std::lock_guard<std::mutex> lock(state_mutex_);
        
        std::cout << "\n";
        std::cout << "╔═══════════════════════════════════════════════════════════╗\n";
        std::cout << "║  GPU RESOURCE STATUS                                      ║\n";
        std::cout << "╠═══════════════════════════════════════════════════════════╣\n";
        
        if (state_.active_scale == GPUScaleType::NONE) {
            std::cout << "║  Status:  GPU AVAILABLE                                   ║\n";
            std::cout << "║  Scale:   None                                            ║\n";
        } else {
            std::cout << "║  Status:  GPU IN USE                                      ║\n";
            std::cout << "║  Scale:   " << std::left << std::setw(48) << state_.scale_name << "║\n";
            std::cout << "║  Type:    " << std::left << std::setw(48) << state_.to_string() << "║\n";
            std::cout << "║  Confirm: " << std::left << std::setw(48) 
                      << (state_.is_confirmed ? "YES" : "PENDING") << "║\n";
            
            if (state_.gpu_memory_bytes > 0) {
                double mb = state_.gpu_memory_bytes / (1024.0 * 1024.0);
                std::cout << "║  Memory:  " << std::left << std::setw(48) 
                          << (std::to_string((int)mb) + " MB") << "║\n";
            }
        }
        
        std::cout << "╚═══════════════════════════════════════════════════════════╝\n";
        std::cout << "\n";
    }
};

// Static member initialization
std::unique_ptr<GPUResourceManager> GPUResourceManager::instance_ = nullptr;
std::mutex GPUResourceManager::instance_mutex_;

} // namespace multiscale
} // namespace vsepr
