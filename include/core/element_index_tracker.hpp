#pragma once
/**
 * element_index_tracker.hpp
 * =========================
 * Persistent particle identity and index tracking subsystem.
 *
 * Problem solved:
 *   Raw array indices are mutable - they change every time the solver
 *   reorders, compacts, or deletes particles. A persistent ID does not.
 *   Without a tracker, logs, exports, bonding histories, and diagnostics
 *   become chemically meaningless the moment a sort occurs.
 *
 * Architecture:
 *   ParticleRecord - the tracked simulation entity (persistent ID + mutable index)
 *   ElementIndexTracker - manager with bidirectional ID to index maps
 *
 * Design rules:
 *   - persistent_id NEVER changes after creation
 *   - current_index may change on every reorder/compaction
 *   - All state transitions are logged
 *   - Deterministic: same seed, same IDs, same sequence
 *   - Anti-black-box: every field inspectable, every event traceable
 *
 * Error integration:
 *   Uses vsepr::ErrorContext::code for structured error returns.
 */

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>
#include <array>
#include <unordered_map>
#include <functional>

#include "core/element_descriptor.hpp"
#include "core/error.hpp"

namespace vsepr {

// ============================================================================
// Particle Record - the tracked simulation entity
// ============================================================================

struct ParticleRecord {
    uint64_t    persistent_id       = 0;
    size_t      current_index       = 0;
    int         atomic_number       = 0;
    std::string symbol;

    uint32_t    creation_step       = 0;
    uint32_t    last_update_step    = 0;

    bool        active              = true;
    bool        selected_randomly   = false;

    uint64_t    parent_id           = 0;
    uint64_t    generation          = 0;

    double      charge              = 0.0;
    double      mass                = 0.0;

    std::array<double, 3> position  {{0, 0, 0}};
    std::array<double, 3> velocity  {{0, 0, 0}};

    std::string state_tag;

    int         mass_number         = 0;
    int         neutron_number      = 0;
    double      decay_clock         = 0.0;
    std::string isotope_label;
};

// ============================================================================
// Event log entry
// ============================================================================

struct TrackerEvent {
    uint64_t    particle_id     = 0;
    uint32_t    step            = 0;
    std::string event_type;
    std::string detail;
};

using ElementFilter = std::function<bool(const ElementDescriptor&)>;

// ============================================================================
// ElementIndexTracker
// ============================================================================

class ElementIndexTracker {
public:
    ElementIndexTracker() = default;

    uint64_t register_particle(
        const ElementDescriptor& elem,
        size_t index,
        uint32_t step,
        bool random_flag = false
    );

    uint64_t register_child(
        const ElementDescriptor& elem,
        size_t index,
        uint32_t step,
        uint64_t parent_id
    );

    Status update_index(uint64_t persistent_id, size_t new_index);
    Status remap_after_reorder(const std::vector<size_t>& old_to_new, uint32_t step);
    Status remove_particle(uint64_t persistent_id, uint32_t step);

    ParticleRecord*       find_by_id(uint64_t persistent_id);
    const ParticleRecord* find_by_id(uint64_t persistent_id) const;
    ParticleRecord*       find_by_index(size_t index);
    const ParticleRecord* find_by_index(size_t index) const;

    Status log_transition(uint64_t persistent_id, const std::string& new_state, uint32_t step);

    size_t active_count() const;
    size_t total_count() const { return records_.size(); }
    uint64_t next_id() const { return next_id_; }

    const std::vector<ParticleRecord>& records() const { return records_; }
    const std::vector<TrackerEvent>&   events()  const { return events_; }

    struct TrackerMetrics {
        size_t  total_registered         = 0;
        size_t  active_particles         = 0;
        size_t  inactive_particles       = 0;
        size_t  total_events             = 0;
        size_t  creation_events          = 0;
        size_t  reindex_events           = 0;
        size_t  state_events             = 0;
        size_t  removal_events           = 0;
        size_t  unique_elements          = 0;
        size_t  random_selections        = 0;
        size_t  child_particles          = 0;
        size_t  max_generation           = 0;
        uint64_t highest_id             = 0;
        std::unordered_map<int, size_t> element_frequency;
        std::string summary;
    };

    TrackerMetrics compute_metrics() const;
    std::unordered_map<int, size_t> element_frequency() const;

    std::string to_csv() const;
    std::string events_to_string() const;

    struct VerificationResult {
        bool    ok                          = true;
        size_t  unique_ids                  = 0;
        size_t  active_particles            = 0;
        size_t  index_collisions            = 0;
        size_t  duplicate_id_violations     = 0;
        size_t  orphan_map_entries          = 0;
        std::string summary;
    };

    VerificationResult verify() const;

private:
    uint64_t next_id_ = 1;
    std::vector<ParticleRecord> records_;
    std::vector<TrackerEvent>   events_;
    std::unordered_map<uint64_t, size_t> id_to_record_;
    std::unordered_map<size_t, uint64_t> index_to_id_;
};

} // namespace vsepr
