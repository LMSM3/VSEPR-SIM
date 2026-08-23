#pragma once

#include "core/frame_snapshot.hpp"
#include <atomic>
#include <cstdint>
#include <mutex>

namespace vsepr {

/**
 * Sequenced communication between the simulation and renderer.
 *
 * Snapshot copies are deliberately protected by a small mutex.  At 120 Hz this
 * keeps the data hand-off correct even when a renderer misses several ticks;
 * the sequence is then used to avoid treating the same physics state as new.
 */
struct FramePacket {
    FrameSnapshot snapshot;
    std::uint64_t sequence = 0;
};

class FrameBuffer {
public:
    FrameBuffer() = default;
    
    /**
     * Write a new frame snapshot (simulation thread only).
     */
    void write(const FrameSnapshot& snapshot) {
        std::lock_guard<std::mutex> lock(mutex_);
        latest_ = snapshot;
        ++sequence_;
    }
    
    /**
     * Read the latest frame snapshot (render thread only).
     * Returns a copy of the most recent complete frame.
     */
    FramePacket read_packet() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return {latest_, sequence_};
    }

    FrameSnapshot read() const {
        return read_packet().snapshot;
    }
    
    /**
     * Check if any valid frame has been written.
     */
    bool has_data() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return latest_.is_valid();
    }
    
private:
    mutable std::mutex mutex_;
    FrameSnapshot latest_;
    std::uint64_t sequence_ = 0;
};

} // namespace vsepr
