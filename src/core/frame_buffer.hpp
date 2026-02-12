#pragma once

#include "core/frame_snapshot.hpp"
#include <atomic>
#include <array>

namespace vsepr {

/**
 * Lock-free double-buffered communication between simulation and renderer.
 * 
 * The simulation thread writes to one buffer while the render thread reads from another.
 * An atomic index tracks which buffer contains the latest complete frame.
 * 
 * Thread Safety:
 * - Only the simulation thread calls write()
 * - Only the render thread calls read()
 * - No mutexes needed due to double buffering
 */
class FrameBuffer {
public:
    FrameBuffer() : latest_index_(0) {}
    
    /**
     * Write a new frame snapshot (simulation thread only).
     * Writes to the non-current buffer, then publishes atomically.
     */
    void write(const FrameSnapshot& snapshot) {
        // Determine which buffer to write to
        int current = latest_index_.load(std::memory_order_relaxed);
        int write_idx = 1 - current;  // Write to the other buffer
        
        // Copy data to write buffer
        buffers_[write_idx] = snapshot;
        
        // Publish the new buffer atomically
        latest_index_.store(write_idx, std::memory_order_release);
    }
    
    /**
     * Read the latest frame snapshot (render thread only).
     * Returns a copy of the most recent complete frame.
     */
    FrameSnapshot read() const {
        int read_idx = latest_index_.load(std::memory_order_acquire);
        return buffers_[read_idx];
    }
    
    /**
     * Check if any valid frame has been written.
     */
    bool has_data() const {
        int idx = latest_index_.load(std::memory_order_acquire);
        return buffers_[idx].is_valid();
    }
    
private:
    std::array<FrameSnapshot, 2> buffers_;
    std::atomic<int> latest_index_;
};

} // namespace vsepr
