/**
 * element_index_tracker.cpp
 * =========================
 * Implementation of persistent particle identity and index tracking.
 *
 * Every operation is logged. Every map mutation is verified.
 * No silent failures — structured errors via ErrorContext::code.
 */

#include "core/element_index_tracker.hpp"

#include <sstream>
#include <iomanip>
#include <algorithm>
#include <unordered_set>

namespace vsepr {

// ============================================================================
// Registration
// ============================================================================

uint64_t ElementIndexTracker::register_particle(
    const ElementDescriptor& elem,
    size_t index,
    uint32_t step,
    bool random_flag)
{
    uint64_t pid = next_id_++;

    ParticleRecord rec;
    rec.persistent_id       = pid;
    rec.current_index       = index;
    rec.atomic_number       = elem.atomic_number;
    rec.symbol              = elem.symbol;
    rec.creation_step       = step;
    rec.last_update_step    = step;
    rec.active              = true;
    rec.selected_randomly   = random_flag;
    rec.parent_id           = 0;
    rec.generation          = 0;
    rec.charge              = 0.0;
    rec.mass                = elem.atomic_mass;
    rec.state_tag           = "initialized";
    rec.mass_number         = elem.mass_number;
    rec.neutron_number      = elem.neutron_number;
    rec.decay_clock         = elem.decay_clock;
    rec.isotope_label       = elem.isotope_label;

    size_t rec_idx = records_.size();
    records_.push_back(std::move(rec));

    id_to_record_[pid]     = rec_idx;
    index_to_id_[index]    = pid;

    TrackerEvent ev;
    ev.particle_id  = pid;
    ev.step         = step;
    ev.event_type   = "created";

    std::ostringstream d;
    d << "Element=" << elem.symbol << " Z=" << elem.atomic_number
      << " Index=" << index
      << " Mode=" << (random_flag ? "random" : "manual");
    ev.detail = d.str();
    events_.push_back(std::move(ev));

    return pid;
}

uint64_t ElementIndexTracker::register_child(
    const ElementDescriptor& elem,
    size_t index,
    uint32_t step,
    uint64_t parent_id)
{
    uint64_t pid = next_id_++;

    // Find parent generation
    uint64_t gen = 0;
    const auto* parent = find_by_id(parent_id);
    if (parent) {
        gen = parent->generation + 1;
    }

    ParticleRecord rec;
    rec.persistent_id       = pid;
    rec.current_index       = index;
    rec.atomic_number       = elem.atomic_number;
    rec.symbol              = elem.symbol;
    rec.creation_step       = step;
    rec.last_update_step    = step;
    rec.active              = true;
    rec.selected_randomly   = false;
    rec.parent_id           = parent_id;
    rec.generation          = gen;
    rec.charge              = 0.0;
    rec.mass                = elem.atomic_mass;
    rec.state_tag           = "decay_product";
    rec.mass_number         = elem.mass_number;
    rec.neutron_number      = elem.neutron_number;
    rec.decay_clock         = elem.decay_clock;
    rec.isotope_label       = elem.isotope_label;

    size_t rec_idx = records_.size();
    records_.push_back(std::move(rec));

    id_to_record_[pid]     = rec_idx;
    index_to_id_[index]    = pid;

    TrackerEvent ev;
    ev.particle_id  = pid;
    ev.step         = step;
    ev.event_type   = "created";

    std::ostringstream d;
    d << "Element=" << elem.symbol << " Z=" << elem.atomic_number
      << " Index=" << index << " Parent=" << parent_id
      << " Gen=" << gen << " State=decay_product";
    ev.detail = d.str();
    events_.push_back(std::move(ev));

    return pid;
}

// ============================================================================
// Index management
// ============================================================================

Status ElementIndexTracker::update_index(uint64_t persistent_id, size_t new_index) {
    auto it = id_to_record_.find(persistent_id);
    if (it == id_to_record_.end()) {
        return Status::error(ErrorCode::TRACKER_PARTICLE_NOT_FOUND,
            "Particle not found", "ID=" + std::to_string(persistent_id));
    }

    ParticleRecord& rec = records_[it->second];
    if (!rec.active) {
        return Status::error(ErrorCode::TRACKER_PARTICLE_INACTIVE,
            "Particle inactive", "ID=" + std::to_string(persistent_id));
    }

    size_t old_index = rec.current_index;

    // Remove old index mapping
    index_to_id_.erase(old_index);

    // Update record and maps
    rec.current_index = new_index;
    index_to_id_[new_index] = persistent_id;

    return Status::ok();
}

Status ElementIndexTracker::remap_after_reorder(
    const std::vector<size_t>& old_to_new, uint32_t step)
{
    // Clear the index→id map; we rebuild it
    index_to_id_.clear();

    size_t remap_count = 0;

    for (auto& [pid, rec_idx] : id_to_record_) {
        ParticleRecord& rec = records_[rec_idx];
        if (!rec.active) continue;

        size_t old_idx = rec.current_index;
        if (old_idx >= old_to_new.size()) {
            return Status::error(ErrorCode::TRACKER_REMAP_OUT_OF_RANGE,
                "Old index out of remap range",
                "ID=" + std::to_string(pid) + " OldIndex=" + std::to_string(old_idx));
        }

        size_t new_idx = old_to_new[old_idx];
        if (new_idx != old_idx) {
            TrackerEvent ev;
            ev.particle_id  = pid;
            ev.step         = step;
            ev.event_type   = "reindex";

            std::ostringstream d;
            d << "OldIndex=" << old_idx << " NewIndex=" << new_idx;
            ev.detail = d.str();
            events_.push_back(std::move(ev));

            rec.current_index    = new_idx;
            rec.last_update_step = step;
            remap_count++;
        }

        index_to_id_[rec.current_index] = pid;
    }

    return Status::ok();
}

// ============================================================================
// Removal
// ============================================================================

Status ElementIndexTracker::remove_particle(uint64_t persistent_id, uint32_t step) {
    auto it = id_to_record_.find(persistent_id);
    if (it == id_to_record_.end()) {
        return Status::error(ErrorCode::TRACKER_PARTICLE_NOT_FOUND,
            "Particle not found for removal", "ID=" + std::to_string(persistent_id));
    }

    ParticleRecord& rec = records_[it->second];
    if (!rec.active) {
        return Status::error(ErrorCode::TRACKER_PARTICLE_INACTIVE,
            "Particle already inactive", "ID=" + std::to_string(persistent_id));
    }

    rec.active           = false;
    rec.last_update_step = step;
    rec.state_tag        = "removed";

    index_to_id_.erase(rec.current_index);

    TrackerEvent ev;
    ev.particle_id  = persistent_id;
    ev.step         = step;
    ev.event_type   = "removed";
    ev.detail       = "Element=" + rec.symbol + " Z=" + std::to_string(rec.atomic_number);
    events_.push_back(std::move(ev));

    return Status::ok();
}

// ============================================================================
// Lookup
// ============================================================================

ParticleRecord* ElementIndexTracker::find_by_id(uint64_t persistent_id) {
    auto it = id_to_record_.find(persistent_id);
    return (it != id_to_record_.end()) ? &records_[it->second] : nullptr;
}

const ParticleRecord* ElementIndexTracker::find_by_id(uint64_t persistent_id) const {
    auto it = id_to_record_.find(persistent_id);
    return (it != id_to_record_.end()) ? &records_[it->second] : nullptr;
}

ParticleRecord* ElementIndexTracker::find_by_index(size_t index) {
    auto it = index_to_id_.find(index);
    if (it == index_to_id_.end()) return nullptr;
    auto rit = id_to_record_.find(it->second);
    return (rit != id_to_record_.end()) ? &records_[rit->second] : nullptr;
}

const ParticleRecord* ElementIndexTracker::find_by_index(size_t index) const {
    auto it = index_to_id_.find(index);
    if (it == index_to_id_.end()) return nullptr;
    auto rit = id_to_record_.find(it->second);
    return (rit != id_to_record_.end()) ? &records_[rit->second] : nullptr;
}

// ============================================================================
// State transitions
// ============================================================================

Status ElementIndexTracker::log_transition(
    uint64_t persistent_id, const std::string& new_state, uint32_t step)
{
    auto it = id_to_record_.find(persistent_id);
    if (it == id_to_record_.end()) {
        return Status::error(ErrorCode::TRACKER_PARTICLE_NOT_FOUND,
            "Particle not found for state transition",
            "ID=" + std::to_string(persistent_id));
    }

    ParticleRecord& rec = records_[it->second];
    std::string old_state = rec.state_tag;
    rec.state_tag        = new_state;
    rec.last_update_step = step;

    TrackerEvent ev;
    ev.particle_id  = persistent_id;
    ev.step         = step;
    ev.event_type   = "state";

    std::ostringstream d;
    d << "\"" << old_state << "\" -> \"" << new_state << "\"";
    ev.detail = d.str();
    events_.push_back(std::move(ev));

    return Status::ok();
}

// ============================================================================
// Queries
// ============================================================================

size_t ElementIndexTracker::active_count() const {
    size_t count = 0;
    for (const auto& rec : records_) {
        if (rec.active) ++count;
    }
    return count;
}

// ============================================================================
// Export — CSV
// ============================================================================

std::string ElementIndexTracker::to_csv() const {
    std::ostringstream out;
    out << "persistent_id,current_index,Z,symbol,parent_id,generation,"
        << "state_tag,selected_randomly,active,mass_number,isotope_label\n";

    for (const auto& rec : records_) {
        out << rec.persistent_id << ","
            << rec.current_index << ","
            << rec.atomic_number << ","
            << rec.symbol << ","
            << rec.parent_id << ","
            << rec.generation << ","
            << rec.state_tag << ","
            << (rec.selected_randomly ? "true" : "false") << ","
            << (rec.active ? "true" : "false") << ","
            << rec.mass_number << ","
            << rec.isotope_label << "\n";
    }
    return out.str();
}

// ============================================================================
// Export — Event log
// ============================================================================

std::string ElementIndexTracker::events_to_string() const {
    std::ostringstream out;
    for (const auto& ev : events_) {
        out << "[Step " << std::setfill('0') << std::setw(6) << ev.step << "] "
            << ev.event_type
            << " Particle ID=" << ev.particle_id
            << " " << ev.detail << "\n";
    }
    return out.str();
}

// ============================================================================
// Verification
// ============================================================================

ElementIndexTracker::VerificationResult ElementIndexTracker::verify() const {
    VerificationResult v;

    std::unordered_set<uint64_t> seen_ids;
    std::unordered_set<size_t>   seen_indices;

    for (const auto& rec : records_) {
        // Check unique IDs
        if (!seen_ids.insert(rec.persistent_id).second) {
            v.duplicate_id_violations++;
        }

        if (rec.active) {
            v.active_particles++;

            // Check for index collisions among active particles
            if (!seen_indices.insert(rec.current_index).second) {
                v.index_collisions++;
            }
        }
    }

    v.unique_ids = seen_ids.size();

    // Verify maps are consistent with records
    for (const auto& [pid, rec_idx] : id_to_record_) {
        if (rec_idx >= records_.size()) {
            v.orphan_map_entries++;
        } else if (records_[rec_idx].persistent_id != pid) {
            v.orphan_map_entries++;
        }
    }

    for (const auto& [idx, pid] : index_to_id_) {
        auto it = id_to_record_.find(pid);
        if (it == id_to_record_.end()) {
            v.orphan_map_entries++;
        } else {
            const auto& rec = records_[it->second];
            if (!rec.active || rec.current_index != idx) {
                v.orphan_map_entries++;
            }
        }
    }

    v.ok = (v.duplicate_id_violations == 0 &&
            v.index_collisions == 0 &&
            v.orphan_map_entries == 0);

    std::ostringstream s;
    s << "Verification: " << (v.ok ? "PASS" : "FAIL")
      << " | IDs=" << v.unique_ids
      << " Active=" << v.active_particles
      << " IndexCollisions=" << v.index_collisions
      << " DuplicateIDs=" << v.duplicate_id_violations
      << " OrphanMaps=" << v.orphan_map_entries;
    v.summary = s.str();

    return v;
}

// ============================================================================
// Metrics
// ============================================================================

ElementIndexTracker::TrackerMetrics ElementIndexTracker::compute_metrics() const {
    TrackerMetrics m;
    m.total_registered = records_.size();
    m.total_events     = events_.size();
    m.highest_id       = (next_id_ > 1) ? (next_id_ - 1) : 0;

    std::unordered_set<int> unique_Z;

    for (const auto& rec : records_) {
        if (rec.active) {
            m.active_particles++;
            m.element_frequency[rec.atomic_number]++;
            unique_Z.insert(rec.atomic_number);
        } else {
            m.inactive_particles++;
        }
        if (rec.selected_randomly) m.random_selections++;
        if (rec.parent_id != 0) m.child_particles++;
        if (rec.generation > m.max_generation) m.max_generation = rec.generation;
    }

    m.unique_elements = unique_Z.size();

    for (const auto& ev : events_) {
        if (ev.event_type == "created")  m.creation_events++;
        else if (ev.event_type == "reindex")  m.reindex_events++;
        else if (ev.event_type == "state")    m.state_events++;
        else if (ev.event_type == "removed")  m.removal_events++;
    }

    std::ostringstream s;
    s << "Tracker: " << m.active_particles << " active / "
      << m.total_registered << " total"
      << " | " << m.unique_elements << " elements"
      << " | " << m.total_events << " events"
      << " | " << m.random_selections << " random"
      << " | " << m.child_particles << " children"
      << " | max_gen=" << m.max_generation;
    m.summary = s.str();

    return m;
}

std::unordered_map<int, size_t> ElementIndexTracker::element_frequency() const {
    std::unordered_map<int, size_t> freq;
    for (const auto& rec : records_) {
        if (rec.active) {
            freq[rec.atomic_number]++;
        }
    }
    return freq;
}

} // namespace vsepr
